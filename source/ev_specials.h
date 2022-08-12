// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized line action system
//
//-----------------------------------------------------------------------------

#ifndef EV_SPECIALS_H__
#define EV_SPECIALS_H__

#include "m_dllist.h"

struct ev_action_t;
struct line_t;
class  Mobj;
struct polyobj_t;
struct sector_t;
struct sectoraction_t;

// Action flags
enum EVActionFlags
{
   EV_PREALLOWMONSTERS = 0x00000001, // Preamble should allow non-players
   EV_PREMONSTERSONLY  = 0x00000002, // Preamble should only allow monsters
   EV_PREALLOWZEROTAG  = 0x00000004, // Preamble should allow zero tag
   EV_PREFIRSTSIDEONLY = 0x00000008, // Disallow activation from back side
   
   EV_POSTCLEARSPECIAL = 0x00000010, // Clear special after activation
   EV_POSTCLEARALWAYS  = 0x00000020, // Always clear special
   EV_POSTCHANGESWITCH = 0x00000040, // Changes switch texture if successful
   EV_POSTCHANGEALWAYS = 0x00000080, // Changes switch texture even if fails
   EV_POSTCHANGESIDED  = 0x00000100, // Switch texture changes on proper side of line

   EV_PARAMLINESPEC    = 0x00000200, // Is a parameterized line special

   EV_PARAMLOCKID      = 0x00000400, // Has a parameterized lockdef ID

   EV_ISTELEPORTER     = 0x00000800, // Is a teleporter action, for automap
   EV_ISLIFT           = 0x00001000, // Is a lift action
   EV_ISMBFLIFT        = 0x00002000, // Is a lift for MBF friends during demo_version 203
   EV_ISMAPPEDEXIT     = 0x00004000, // Is an exit line for purposes of automap
};

// Data related to an instance of a special activation.
struct ev_instance_t
{
   Mobj           *actor;        // actor, if any
   line_t         *line;         // line, if any
   sectoraction_t *sectoraction; // sectoraction, if any
   int             special;      // special to activate (may == line->special)
   const int      *args;         // arguments (may point to line->args)
   int             tag;          // tag (may == line->args[0])
   int             side;         // side of activation
   union
   {
      int spac; // special activation type
      int seac; // sector  activation type
   };
   int             gentype;      // generalized type, if is generalized (-1 otherwise)
   int             genspac;      // generalized activation type, if generalized
   polyobj_t      *poly;         // possible polyobject activator
   bool byALineEffect;           // true if activated by A_LineEffect
};

//
// EVPreFunc
//
// Before a special will be activated, it will be checked for validity of the
// activation. This allows common prologue functionality to occur before the
// special is activated. Most arguments are optional and must be checked for
// validity.
//
typedef bool (*EVPreFunc)(ev_action_t *, ev_instance_t *);

//
// EVPostFunc
//
// This function will be executed after the special and can react to its
// execution. The result code of the EVActionFunc is passed as the first
// parameter. Most other parameters are optional and must be checked for
// validity.
//
typedef int (*EVPostFunc)(ev_action_t *, int, ev_instance_t *);

//
// EVActionFunc
//
// All actions must adhere to this call signature. Most members of the 
// specialactivation structure are optional and must be checked for validity.
//
typedef int (*EVActionFunc)(ev_action_t *, ev_instance_t *);

//
// ev_actiontype_t
//
// This structure represents a distinct type of action, for example a DOOM
// cross-type action.
//
struct ev_actiontype_t
{
   int           activation; // activation type of this special, if restricted
   EVPreFunc     pre;        // pre-action callback
   EVPostFunc    post;       // post-action callback
   unsigned int  flags;      // flags; the action may add additional flags.
   const char   *name;       // name for display purposes
};

struct ev_action_t
{
   ev_actiontype_t *type;       // actiontype structure
   EVActionFunc     action;     // action function
   unsigned int     flags;      // action flags
   int              minversion; // minimum demo version
   const char      *name;       // name for display purposes
   int              lockarg;    // if(flags & EV_PARAMLOCKID), use this arg #
};

// Binds a line special action to a specific action number.
struct ev_binding_t
{
   int           actionNumber; // line action number
   ev_action_t  *action;       // the actual action to execute
   const char   *name;         // name, if this binding has one

   ev_binding_t *pEDBinding;   // corresponding ExtraData binding, if one exists

   DLListItem<ev_binding_t> links;     // hash links by number
   DLListItem<ev_binding_t> namelinks; // hash links by name
};

//
// EV_CompositeActionFlags
//
// Returns the composition of the action's flags with the flags imposed by the
// action's type. Always call this rather than directly accessing action->flags,
// because all flags that are implied by an action's type will only be set by
// the type and never by the action. For example, W1ActionType sets 
// EV_POSTCLEARSPECIAL for all DOOM-style W1 actions.
//
inline static unsigned int EV_CompositeActionFlags(const ev_action_t *action)
{
   return (action ? (action->type->flags | action->flags) : 0);
}

//
// Like EV_IsParamLineSpec, but directly on action
//
inline static bool EV_IsParamAction(const ev_action_t &action)
{
   return !!(EV_CompositeActionFlags(&action) & EV_PARAMLINESPEC);
}

// Action Types
extern ev_actiontype_t NullActionType;
extern ev_actiontype_t WRActionType;
extern ev_actiontype_t W1ActionType;
extern ev_actiontype_t SRActionType;
extern ev_actiontype_t S1ActionType;
extern ev_actiontype_t DRActionType;
extern ev_actiontype_t GRActionType;
extern ev_actiontype_t G1ActionType;
extern ev_actiontype_t BoomGenActionType;
extern ev_actiontype_t ParamActionType;

// Interface

// Binding for Special
ev_binding_t *EV_DOOMBindingForSpecial(int special);
ev_binding_t *EV_HereticBindingForSpecial(int special);
ev_binding_t *EV_StrifeBindingForSpecial(int special);
ev_binding_t *EV_HexenBindingForSpecial(int special);

// Binding for Name
ev_binding_t *EV_DOOMBindingForName(const char *name);
ev_binding_t *EV_HexenBindingForName(const char *name);
ev_binding_t *EV_UDMFEternityBindingForName(const char *name);
ev_binding_t *EV_BindingForName(const char *name);

// Action for Special 
ev_action_t  *EV_DOOMActionForSpecial(int special);
ev_action_t  *EV_HereticActionForSpecial(int special);
ev_action_t  *EV_StrifeActionForSpecial(int special);
ev_action_t  *EV_HexenActionForSpecial(int special);
ev_action_t  *EV_UDMFEternityActionForSpecial(int special);
ev_action_t  *EV_ACSActionForSpecial(int special);
ev_action_t  *EV_ActionForSpecial(int special);

// Get map-specific action number for an ACS action
int EV_ActionForACSAction(int acsActionNum);

// Lockdef ID for Special
int EV_LockDefIDForSpecial(int special);

// Lockdef ID for Linedef
int EV_LockDefIDForLine(const line_t *line);

// Testing
bool EV_IsParamLineSpec(int special);
bool EV_CheckGenSpecialSpac(int special, int spac);
bool EV_CheckActionIntrinsicSpac(const ev_action_t &action, int spac);
int EV_GenTypeForSpecial(int special);

// Activation
bool EV_ActivateSpecialLineWithSpac(line_t *line, int side, Mobj *thing, polyobj_t *poly, int spac, bool byALineEffect);
bool EV_ActivateSpecialNum(int special, int *args, Mobj *thing, bool nonParamOnly);
int  EV_ActivateACSSpecial(line_t *line, int special, int *args, int side, Mobj *thing, polyobj_t *poly);
bool EV_ActivateAction(ev_action_t *action, int *args, Mobj *thing);

int EV_ActivateSectorAction(sector_t *sector, Mobj *thing, int seac);

//
// Static Init Line Types
//

enum
{
   // Enumerator                               DOOM/BOOM/MBF/EE Line Special
   EV_STATIC_NULL,                          // No-op placeholder
   EV_STATIC_SCROLL_LINE_LEFT,              // 48
   EV_STATIC_SCROLL_LINE_RIGHT,             // 85
   EV_STATIC_LIGHT_TRANSFER_FLOOR,          // 213
   EV_STATIC_SCROLL_ACCEL_CEILING,          // 214
   EV_STATIC_SCROLL_ACCEL_FLOOR,            // 215
   EV_STATIC_CARRY_ACCEL_FLOOR,             // 216
   EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR,      // 217
   EV_STATIC_SCROLL_ACCEL_WALL,             // 218
   EV_STATIC_FRICTION_TRANSFER,             // 223
   EV_STATIC_WIND_CONTROL,                  // 224
   EV_STATIC_CURRENT_CONTROL,               // 225
   EV_STATIC_PUSHPULL_CONTROL,              // 226
   EV_STATIC_TRANSFER_HEIGHTS,              // 242
   EV_STATIC_SCROLL_DISPLACE_CEILING,       // 245
   EV_STATIC_SCROLL_DISPLACE_FLOOR,         // 246
   EV_STATIC_CARRY_DISPLACE_FLOOR,          // 247
   EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR,   // 248
   EV_STATIC_SCROLL_DISPLACE_WALL,          // 249
   EV_STATIC_SCROLL_CEILING,                // 250
   EV_STATIC_SCROLL_FLOOR,                  // 251
   EV_STATIC_CARRY_FLOOR,                   // 252
   EV_STATIC_SCROLL_CARRY_FLOOR,            // 253
   EV_STATIC_SCROLL_WALL_WITH,              // 254
   EV_STATIC_SCROLL_BY_OFFSETS,             // 255
   EV_STATIC_TRANSLUCENT,                   // 260
   EV_STATIC_LIGHT_TRANSFER_CEILING,        // 261
   EV_STATIC_EXTRADATA_LINEDEF,             // 270
   EV_STATIC_SKY_TRANSFER,                  // 271
   EV_STATIC_SKY_TRANSFER_FLIPPED,          // 272
   EV_STATIC_3DMIDTEX_ATTACH_FLOOR,         // 281
   EV_STATIC_3DMIDTEX_ATTACH_CEILING,       // 282
   EV_STATIC_PORTAL_PLANE_CEILING,          // 283
   EV_STATIC_PORTAL_PLANE_FLOOR,            // 284
   EV_STATIC_PORTAL_PLANE_CEILING_FLOOR,    // 285
   EV_STATIC_PORTAL_HORIZON_CEILING,        // 286
   EV_STATIC_PORTAL_HORIZON_FLOOR,          // 287
   EV_STATIC_PORTAL_HORIZON_CEILING_FLOOR,  // 288
   EV_STATIC_PORTAL_LINE,                   // 289
   EV_STATIC_PORTAL_SKYBOX_CEILING,         // 290
   EV_STATIC_PORTAL_SKYBOX_FLOOR,           // 291
   EV_STATIC_PORTAL_SKYBOX_CEILING_FLOOR,   // 292
   EV_STATIC_HERETIC_WIND,                  // 293
   EV_STATIC_HERETIC_CURRENT,               // 294
   EV_STATIC_PORTAL_ANCHORED_CEILING,       // 295
   EV_STATIC_PORTAL_ANCHORED_FLOOR,         // 296
   EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR, // 297
   EV_STATIC_PORTAL_ANCHOR,                 // 298
   EV_STATIC_PORTAL_ANCHOR_FLOOR,           // 299
   EV_STATIC_PORTAL_TWOWAY_CEILING,         // 344
   EV_STATIC_PORTAL_TWOWAY_FLOOR,           // 345
   EV_STATIC_PORTAL_TWOWAY_ANCHOR,          // 346
   EV_STATIC_PORTAL_TWOWAY_ANCHOR_FLOOR,    // 347
   EV_STATIC_POLYOBJ_START_LINE,            // 348
   EV_STATIC_POLYOBJ_EXPLICIT_LINE,         // 349
   EV_STATIC_PORTAL_LINKED_CEILING,         // 358
   EV_STATIC_PORTAL_LINKED_FLOOR,           // 359
   EV_STATIC_PORTAL_LINKED_ANCHOR,          // 360
   EV_STATIC_PORTAL_LINKED_ANCHOR_FLOOR,    // 361
   EV_STATIC_PORTAL_LINKED_LINE2LINE,       // 376
   EV_STATIC_PORTAL_LINKED_L2L_ANCHOR,      // 377
   EV_STATIC_LINE_SET_IDENTIFICATION,       // 378
   EV_STATIC_ATTACH_SET_CEILING_CONTROL,    // 379
   EV_STATIC_ATTACH_SET_FLOOR_CONTROL,      // 380
   EV_STATIC_ATTACH_FLOOR_TO_CONTROL,       // 381
   EV_STATIC_ATTACH_CEILING_TO_CONTROL,     // 382
   EV_STATIC_ATTACH_MIRROR_FLOOR,           // 383
   EV_STATIC_ATTACH_MIRROR_CEILING,         // 384
   EV_STATIC_PORTAL_APPLY_FRONTSECTOR,      // 385
   EV_STATIC_SLOPE_FSEC_FLOOR,              // 386
   EV_STATIC_SLOPE_FSEC_CEILING,            // 387
   EV_STATIC_SLOPE_FSEC_FLOOR_CEILING,      // 388
   EV_STATIC_SLOPE_BSEC_FLOOR,              // 389
   EV_STATIC_SLOPE_BSEC_CEILING,            // 390
   EV_STATIC_SLOPE_BSEC_FLOOR_CEILING,      // 391
   EV_STATIC_SLOPE_BACKFLOOR_FRONTCEILING,  // 392
   EV_STATIC_SLOPE_FRONTFLOOR_BACKCEILING,  // 393
   EV_STATIC_SLOPE_FRONTFLOOR_TAG,          // 394
   EV_STATIC_SLOPE_FRONTCEILING_TAG,        // 395
   EV_STATIC_SLOPE_FRONTFLOORCEILING_TAG,   // 396
   EV_STATIC_EXTRADATA_SECTOR,              // 401
   EV_STATIC_SCROLL_LEFT_PARAM,             // 406
   EV_STATIC_SCROLL_RIGHT_PARAM,            // 407
   EV_STATIC_SCROLL_UP_PARAM,               // 408
   EV_STATIC_SCROLL_DOWN_PARAM,             // 409
   EV_STATIC_SCROLL_LINE_UP,                // 417
   EV_STATIC_SCROLL_LINE_DOWN,              // 418
   EV_STATIC_SCROLL_LINE_DOWN_FAST,         // 419
   EV_STATIC_PORTAL_HORIZON_LINE,           // 450
   EV_STATIC_SLOPE_PARAM,                   // 455
   EV_STATIC_PORTAL_SECTOR_PARAM_COMPAT,    // 456
   EV_STATIC_WIND_CONTROL_PARAM,            // 457
   EV_STATIC_CURRENT_CONTROL_PARAM,         // 479
   EV_STATIC_PUSHPULL_CONTROL_PARAM,        // 480
   EV_STATIC_INIT_PARAM,                    // 481
   EV_STATIC_3DMIDTEX_ATTACH_PARAM,         // 482
   EV_STATIC_SCROLL_CEILING_PARAM,          // 483
   EV_STATIC_SCROLL_FLOOR_PARAM,            // 484
   EV_STATIC_SCROLL_WALL_PARAM,             // 485
   EV_STATIC_PORTAL_LINE_PARAM_COMPAT,      // 486
   EV_STATIC_PORTAL_LINE_PARAM_QUICK,       // 491
   EV_STATIC_PORTAL_DEFINE,                 // 492
   EV_STATIC_SLOPE_PARAM_TAG,               // 493
   EV_STATIC_SCROLL_BY_OFFSETS_PARAM,       // 503
   EV_STATIC_SCROLL_BY_OFFSETS_TAG,         // 1024 (MBF21)
   EV_STATIC_SCROLL_BY_OFFSETS_TAG_DISPLACE,// 1025 (MBF21)
   EV_STATIC_SCROLL_BY_OFFSETS_TAG_ACCEL,   // 1026 (MBF21)

   EV_STATIC_MAX
};

//
// Parameterized specifics
//

// Line_SetPortal
enum
{
   ev_LinePortal_Maker = 0,
   ev_LinePortal_Anchor = 1,
   ev_LinePortal_Arg_SelfTag = 1,
   ev_LinePortal_Arg_Type = 2,
   ev_LinePortal_Type_Visual = 0,
   ev_LinePortal_Type_Linked = 3,
   ev_LinePortal_Type_EEClassic = 4
};

// Scroll
enum
{
   ev_Scroll_Arg_Bits = 1,
   ev_Scroll_Bit_Displace = 1,
   ev_Scroll_Bit_Accel = 2,
   ev_Scroll_Bit_UseLine = 4,
   ev_Scroll_Arg_Type = 2,
   ev_Scroll_Type_Scroll = 0,
   ev_Scroll_Type_Carry = 1,
   ev_Scroll_Type_ScrollCarry = 2,
   ev_Scroll_Arg_X = 3,
   ev_Scroll_Arg_Y = 4,
};

// Sector_Attach3dMidtex
enum
{
   ev_AttachMidtex_Arg_SectorTag = 1,
   ev_AttachMidtex_Arg_DoCeiling = 2,
};

// Sector_SetWind
enum
{
   ev_SetWind_Arg_Strength = 1,
   ev_SetWind_Arg_Angle = 2,
   ev_SetWind_Arg_Flags = 3,
   ev_SetWind_Flag_UseLine = 1,
   ev_SetWind_Flag_Heretic = 2,
};

// Static_Init
enum
{
   ev_StaticInit_Arg_Prop = 1,
   ev_StaticInit_Prop_SkyTransfer = 255,
   ev_StaticInit_Arg_Flip = 2,
};

// Binds a line special number to a static init function
struct ev_static_t
{
   int actionNumber; // numeric line special
   int staticFn;     // static init function enumeration value

   DLListItem<ev_static_t> staticLinks; // hash links for static->special
   DLListItem<ev_static_t> actionLinks; // hash links for special->static
};

// Interface

int  EV_SpecialForStaticInit(int staticFn);
int  EV_StaticInitForSpecial(int special);
int  EV_SpecialForStaticInitName(const char *name);
bool EV_IsParamStaticInit(int special);

#endif

// EOF

