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
//   Generalized line action system - Static Init Functions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_gi.h"
#include "e_hash.h"
#include "ev_specials.h"
#include "p_info.h"
#include "p_setup.h"

//=============================================================================
//
// Static Init Lines
//
// Rather than activatable triggers, these line types are used to setup some
// sort of persistent effect in the map at level load time.
//

#define STATICSPEC(line, function) { line, function },

// DOOM Static Init Bindings
static ev_static_t DOOMStaticBindings[] =
{
   STATICSPEC( 48, EV_STATIC_SCROLL_LINE_LEFT)
   STATICSPEC( 85, EV_STATIC_SCROLL_LINE_RIGHT)
   STATICSPEC(213, EV_STATIC_LIGHT_TRANSFER_FLOOR)
   STATICSPEC(214, EV_STATIC_SCROLL_ACCEL_CEILING)
   STATICSPEC(215, EV_STATIC_SCROLL_ACCEL_FLOOR)
   STATICSPEC(216, EV_STATIC_CARRY_ACCEL_FLOOR)
   STATICSPEC(217, EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR)
   STATICSPEC(218, EV_STATIC_SCROLL_ACCEL_WALL)
   STATICSPEC(223, EV_STATIC_FRICTION_TRANSFER)
   STATICSPEC(224, EV_STATIC_WIND_CONTROL)
   STATICSPEC(225, EV_STATIC_CURRENT_CONTROL)
   STATICSPEC(226, EV_STATIC_PUSHPULL_CONTROL)
   STATICSPEC(242, EV_STATIC_TRANSFER_HEIGHTS)
   STATICSPEC(245, EV_STATIC_SCROLL_DISPLACE_CEILING)
   STATICSPEC(246, EV_STATIC_SCROLL_DISPLACE_FLOOR)
   STATICSPEC(247, EV_STATIC_CARRY_DISPLACE_FLOOR)
   STATICSPEC(248, EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR)
   STATICSPEC(249, EV_STATIC_SCROLL_DISPLACE_WALL)
   STATICSPEC(250, EV_STATIC_SCROLL_CEILING)
   STATICSPEC(251, EV_STATIC_SCROLL_FLOOR)
   STATICSPEC(252, EV_STATIC_CARRY_FLOOR)
   STATICSPEC(253, EV_STATIC_SCROLL_CARRY_FLOOR)
   STATICSPEC(254, EV_STATIC_SCROLL_WALL_WITH)
   STATICSPEC(255, EV_STATIC_SCROLL_BY_OFFSETS)
   STATICSPEC(260, EV_STATIC_TRANSLUCENT)
   STATICSPEC(261, EV_STATIC_LIGHT_TRANSFER_CEILING)
   STATICSPEC(270, EV_STATIC_EXTRADATA_LINEDEF)
   STATICSPEC(271, EV_STATIC_SKY_TRANSFER)
   STATICSPEC(272, EV_STATIC_SKY_TRANSFER_FLIPPED)
   STATICSPEC(281, EV_STATIC_3DMIDTEX_ATTACH_FLOOR)
   STATICSPEC(282, EV_STATIC_3DMIDTEX_ATTACH_CEILING)
   STATICSPEC(283, EV_STATIC_PORTAL_PLANE_CEILING)
   STATICSPEC(284, EV_STATIC_PORTAL_PLANE_FLOOR)
   STATICSPEC(285, EV_STATIC_PORTAL_PLANE_CEILING_FLOOR)
   STATICSPEC(286, EV_STATIC_PORTAL_HORIZON_CEILING)
   STATICSPEC(287, EV_STATIC_PORTAL_HORIZON_FLOOR)
   STATICSPEC(288, EV_STATIC_PORTAL_HORIZON_CEILING_FLOOR)
   STATICSPEC(289, EV_STATIC_PORTAL_LINE)
   STATICSPEC(290, EV_STATIC_PORTAL_SKYBOX_CEILING)
   STATICSPEC(291, EV_STATIC_PORTAL_SKYBOX_FLOOR)
   STATICSPEC(292, EV_STATIC_PORTAL_SKYBOX_CEILING_FLOOR)
   STATICSPEC(293, EV_STATIC_HERETIC_WIND)
   STATICSPEC(294, EV_STATIC_HERETIC_CURRENT)
   STATICSPEC(295, EV_STATIC_PORTAL_ANCHORED_CEILING)
   STATICSPEC(296, EV_STATIC_PORTAL_ANCHORED_FLOOR)
   STATICSPEC(297, EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR)
   STATICSPEC(298, EV_STATIC_PORTAL_ANCHOR)
   STATICSPEC(299, EV_STATIC_PORTAL_ANCHOR_FLOOR)
   STATICSPEC(344, EV_STATIC_PORTAL_TWOWAY_CEILING)
   STATICSPEC(345, EV_STATIC_PORTAL_TWOWAY_FLOOR)
   STATICSPEC(346, EV_STATIC_PORTAL_TWOWAY_ANCHOR)
   STATICSPEC(347, EV_STATIC_PORTAL_TWOWAY_ANCHOR_FLOOR)
   STATICSPEC(348, EV_STATIC_POLYOBJ_START_LINE)
   STATICSPEC(349, EV_STATIC_POLYOBJ_EXPLICIT_LINE)
   STATICSPEC(358, EV_STATIC_PORTAL_LINKED_CEILING)
   STATICSPEC(359, EV_STATIC_PORTAL_LINKED_FLOOR)
   STATICSPEC(360, EV_STATIC_PORTAL_LINKED_ANCHOR)
   STATICSPEC(361, EV_STATIC_PORTAL_LINKED_ANCHOR_FLOOR)
   STATICSPEC(376, EV_STATIC_PORTAL_LINKED_LINE2LINE)
   STATICSPEC(377, EV_STATIC_PORTAL_LINKED_L2L_ANCHOR)
   STATICSPEC(378, EV_STATIC_LINE_SET_IDENTIFICATION)
   STATICSPEC(379, EV_STATIC_ATTACH_SET_CEILING_CONTROL)
   STATICSPEC(380, EV_STATIC_ATTACH_SET_FLOOR_CONTROL)
   STATICSPEC(381, EV_STATIC_ATTACH_FLOOR_TO_CONTROL)
   STATICSPEC(382, EV_STATIC_ATTACH_CEILING_TO_CONTROL)
   STATICSPEC(383, EV_STATIC_ATTACH_MIRROR_FLOOR)
   STATICSPEC(384, EV_STATIC_ATTACH_MIRROR_CEILING)
   STATICSPEC(385, EV_STATIC_PORTAL_APPLY_FRONTSECTOR)
   STATICSPEC(386, EV_STATIC_SLOPE_FSEC_FLOOR)
   STATICSPEC(387, EV_STATIC_SLOPE_FSEC_CEILING)
   STATICSPEC(388, EV_STATIC_SLOPE_FSEC_FLOOR_CEILING)
   STATICSPEC(389, EV_STATIC_SLOPE_BSEC_FLOOR)
   STATICSPEC(390, EV_STATIC_SLOPE_BSEC_CEILING)
   STATICSPEC(391, EV_STATIC_SLOPE_BSEC_FLOOR_CEILING)
   STATICSPEC(392, EV_STATIC_SLOPE_BACKFLOOR_FRONTCEILING)
   STATICSPEC(393, EV_STATIC_SLOPE_FRONTFLOOR_BACKCEILING)
   STATICSPEC(394, EV_STATIC_SLOPE_FRONTFLOOR_TAG)
   STATICSPEC(395, EV_STATIC_SLOPE_FRONTCEILING_TAG)
   STATICSPEC(396, EV_STATIC_SLOPE_FRONTFLOORCEILING_TAG)
   STATICSPEC(401, EV_STATIC_EXTRADATA_SECTOR)
   STATICSPEC(406, EV_STATIC_SCROLL_LEFT_PARAM)
   STATICSPEC(407, EV_STATIC_SCROLL_RIGHT_PARAM)
   STATICSPEC(408, EV_STATIC_SCROLL_UP_PARAM)
   STATICSPEC(409, EV_STATIC_SCROLL_DOWN_PARAM)
   STATICSPEC(417, EV_STATIC_SCROLL_LINE_UP)
   STATICSPEC(418, EV_STATIC_SCROLL_LINE_DOWN)
   STATICSPEC(419, EV_STATIC_SCROLL_LINE_DOWN_FAST)
   STATICSPEC(450, EV_STATIC_PORTAL_HORIZON_LINE)
   STATICSPEC(455, EV_STATIC_SLOPE_PARAM)
   STATICSPEC(456, EV_STATIC_PORTAL_SECTOR_PARAM_COMPAT)
   STATICSPEC(457, EV_STATIC_WIND_CONTROL_PARAM)
   STATICSPEC(479, EV_STATIC_CURRENT_CONTROL_PARAM)
   STATICSPEC(480, EV_STATIC_PUSHPULL_CONTROL_PARAM)
   STATICSPEC(481, EV_STATIC_INIT_PARAM)
   STATICSPEC(482, EV_STATIC_3DMIDTEX_ATTACH_PARAM)
   STATICSPEC(483, EV_STATIC_SCROLL_CEILING_PARAM)
   STATICSPEC(484, EV_STATIC_SCROLL_FLOOR_PARAM)
   STATICSPEC(485, EV_STATIC_SCROLL_WALL_PARAM)
   STATICSPEC(486, EV_STATIC_PORTAL_LINE_PARAM_COMPAT)
   STATICSPEC(491, EV_STATIC_PORTAL_LINE_PARAM_QUICK)
   STATICSPEC(492, EV_STATIC_PORTAL_DEFINE)
   STATICSPEC(493, EV_STATIC_SLOPE_PARAM_TAG)
   STATICSPEC(503, EV_STATIC_SCROLL_BY_OFFSETS_PARAM)
   STATICSPEC(1024, EV_STATIC_SCROLL_BY_OFFSETS_TAG)
   STATICSPEC(1025, EV_STATIC_SCROLL_BY_OFFSETS_TAG_DISPLACE)
   STATICSPEC(1026, EV_STATIC_SCROLL_BY_OFFSETS_TAG_ACCEL)
};

// Hexen Static Init Bindings
static ev_static_t HexenStaticBindings[] =
{
   STATICSPEC(  1, EV_STATIC_POLYOBJ_START_LINE)
   STATICSPEC(  5, EV_STATIC_POLYOBJ_EXPLICIT_LINE)
   STATICSPEC(  9, EV_STATIC_PORTAL_HORIZON_LINE)
   STATICSPEC( 48, EV_STATIC_3DMIDTEX_ATTACH_PARAM)
   STATICSPEC( 57, EV_STATIC_PORTAL_SECTOR_PARAM_COMPAT)
   STATICSPEC(100, EV_STATIC_SCROLL_LEFT_PARAM)
   STATICSPEC(101, EV_STATIC_SCROLL_RIGHT_PARAM)
   STATICSPEC(102, EV_STATIC_SCROLL_UP_PARAM)
   STATICSPEC(103, EV_STATIC_SCROLL_DOWN_PARAM)
   STATICSPEC(118, EV_STATIC_SLOPE_PARAM_TAG)
   STATICSPEC(121, EV_STATIC_LINE_SET_IDENTIFICATION)
   STATICSPEC(156, EV_STATIC_PORTAL_LINE_PARAM_COMPAT)
   STATICSPEC(181, EV_STATIC_SLOPE_PARAM)
   STATICSPEC(190, EV_STATIC_INIT_PARAM)
   STATICSPEC(209, EV_STATIC_TRANSFER_HEIGHTS)
   STATICSPEC(210, EV_STATIC_LIGHT_TRANSFER_FLOOR)
   STATICSPEC(211, EV_STATIC_LIGHT_TRANSFER_CEILING)
   STATICSPEC(218, EV_STATIC_WIND_CONTROL_PARAM)
   STATICSPEC(219, EV_STATIC_FRICTION_TRANSFER)
   STATICSPEC(220, EV_STATIC_CURRENT_CONTROL_PARAM)
   STATICSPEC(222, EV_STATIC_SCROLL_WALL_PARAM)
   STATICSPEC(223, EV_STATIC_SCROLL_FLOOR_PARAM)
   STATICSPEC(224, EV_STATIC_SCROLL_CEILING_PARAM)
   STATICSPEC(225, EV_STATIC_SCROLL_BY_OFFSETS_PARAM)
   STATICSPEC(227, EV_STATIC_PUSHPULL_CONTROL_PARAM)
};

// PSX Static Init Bindings
static ev_static_t PSXStaticBindings[] =
{
   STATICSPEC(200, EV_STATIC_SCROLL_LINE_LEFT)
   STATICSPEC(201, EV_STATIC_SCROLL_LINE_RIGHT)
   STATICSPEC(202, EV_STATIC_SCROLL_LINE_UP)
   STATICSPEC(203, EV_STATIC_SCROLL_LINE_DOWN)
};

// UDMF "Eternity" Namespace Static Bindings
static ev_static_t UDMFEternityStaticBindings[] =
{
   STATICSPEC(121, EV_STATIC_NULL) // Line_SetIdentification isn't needed in UDMF
   STATICSPEC(300, EV_STATIC_PORTAL_DEFINE)
   STATICSPEC(301, EV_STATIC_PORTAL_LINE_PARAM_QUICK)   
};

//
// Hash Tables
//

// Resolve a static init function to the special to which it is bound
typedef EHashTable<ev_static_t, EIntHashKey, &ev_static_t::staticFn,
                   &ev_static_t::staticLinks> EV_StaticHash;

// Resolve a line special to the static init function to which it is bound
typedef EHashTable<ev_static_t, EIntHashKey,&ev_static_t::actionNumber,
                   &ev_static_t::actionLinks> EV_StaticSpecHash;

// DOOM hashes
static EV_StaticHash     DOOMStaticHash(earrlen(DOOMStaticBindings));
static EV_StaticSpecHash DOOMStaticSpecHash(earrlen(DOOMStaticBindings));

// Hexen hashes
static EV_StaticHash     HexenStaticHash(earrlen(HexenStaticBindings));
static EV_StaticSpecHash HexenStaticSpecHash(earrlen(HexenStaticBindings));

// UDMF hashes
static EV_StaticHash     UDMFEternityStaticHash(earrlen(UDMFEternityStaticBindings));
static EV_StaticSpecHash UDMFEternityStaticSpecHash(earrlen(UDMFEternityStaticBindings));

//
// EV_addStaticSpecialsToHash
//
// Add an array of static specials to a hash table.
//
static void EV_addStaticSpecialsToHash(EV_StaticHash &staticHash, 
                                       EV_StaticSpecHash &actionHash,
                                       ev_static_t *bindings, size_t count)
{
   for(size_t i = 0; i < count; i++)
   {
      staticHash.addObject(bindings[i]);
      actionHash.addObject(bindings[i]);
   }
}

//
// EV_InitDOOMStaticHash
//
// First-time-use initialization for the DOOM static specials hash table.
//
static void EV_initDOOMStaticHash()
{
   static bool firsttime = true;

   if(firsttime)
   {
      firsttime = false;
      
      // add every item in the DOOM static bindings array
      EV_addStaticSpecialsToHash(DOOMStaticHash, DOOMStaticSpecHash, 
                                 DOOMStaticBindings,
                                 earrlen(DOOMStaticBindings));
   }
}

//
// EV_InitHexenStaticHash
//
// First-time-use initialization for the Hexen static specials hash table.
//
static void EV_initHexenStaticHash()
{
   static bool firsttime = true;

   if(firsttime)
   {
      firsttime = false;

      // add every item in the Hexen static bindings array
      EV_addStaticSpecialsToHash(HexenStaticHash, HexenStaticSpecHash,
                                 HexenStaticBindings,
                                 earrlen(HexenStaticBindings));
   }
}

static void EV_initUDMFEternityStaticHash()
{
   static bool firsttime = true;

   if(firsttime)
   {
      firsttime = false;

      // add every item in the Hexen static bindings array
      EV_addStaticSpecialsToHash(UDMFEternityStaticHash, UDMFEternityStaticSpecHash,
         UDMFEternityStaticBindings,
         earrlen(UDMFEternityStaticBindings));
   }
}
//
// EV_DOOMSpecialForStaticInit
//
// Always looks up a special in the DOOM gamemode's static init list, regardless
// of the map format or gamemode in use. Returns 0 if no such special exists.
//
static int EV_DOOMSpecialForStaticInit(int staticFn)
{
   ev_static_t *binding;

   // init the hash if it hasn't been done yet
   EV_initDOOMStaticHash();

   return (binding = DOOMStaticHash.objectForKey(staticFn)) ? binding->actionNumber : 0;
}

//
// EV_DOOMStaticInitForSpecial
//
// Always looks up a static init function in the DOOM gamemode's static init list,
// regardless of the map format or gamemode in use. Returns 0 if the given special
// isn't bound to a static init function.
//
static int EV_DOOMStaticInitForSpecial(int special)
{
   ev_static_t *binding;

   // Early return for special 0
   if(!special)
      return EV_STATIC_NULL;

   // init the hash if it hasn't been done yet
   EV_initDOOMStaticHash();

   return (binding = DOOMStaticSpecHash.objectForKey(special)) ? binding->staticFn : 0;
}

//
// EV_HereticSpecialForStaticInit
//
// Always looks up a special in the Heretic gamemode's static init list, 
// regardless of the map format or gamemode in use. Returns 0 if no such 
// special exists.
//
static int EV_HereticSpecialForStaticInit(int staticFn)
{
   // There is only one difference between Heretic and DOOM regarding static
   // init specials; line type 99 is equivalent to BOOM extended type 85, 
   // scroll line right.

   if(staticFn == EV_STATIC_SCROLL_LINE_RIGHT)
      return 99;

   return EV_DOOMSpecialForStaticInit(staticFn);
}

//
// EV_HereticStaticInitForSpecial
//
// Always looks up a static init function in the Heretic gamemode's static init
// list, regardless of the map format or gamemode in use. Returns 0 if the given
// special isn't bound to a static init function.
//
static int EV_HereticStaticInitForSpecial(int special)
{
   if(special == 99)
      return EV_STATIC_SCROLL_LINE_RIGHT;

   return EV_DOOMStaticInitForSpecial(special);
}

//
// EV_HexenSpecialForStaticInit
//
// Always looks up a special in the Hexen gamemode's static init list, 
// regardless of the map format or gamemode in use. Returns 0 if no such special
// exists.
//
static int EV_HexenSpecialForStaticInit(int staticFn)
{
   ev_static_t *binding;

   // init the hash if it hasn't been done yet
   EV_initHexenStaticHash();

   return (binding = HexenStaticHash.objectForKey(staticFn)) ? binding->actionNumber : 0;
}

//
// EV_HexenStaticInitForSpecial
//
// Always looks up a static init function in the Hexen gamemode's statc init
// list, regardless of the map format or gamemode in use. Returns 0 if the given
// special isn't bound to a static init function.
//
static int EV_HexenStaticInitForSpecial(int special)
{
   ev_static_t *binding;

   // Early return for special 0
   if(!special)
      return EV_STATIC_NULL;

   // init the hash if it hasn't been done yet
   EV_initHexenStaticHash();

   return (binding = HexenStaticSpecHash.objectForKey(special)) ? binding->staticFn: 0;
}

// 
// EV_StrifeSpecialForStaticInit
//
// Always looks up a special in the Strife gamemode's static init list,
// regardless of the map format or gamemode in use. Returns 0 if no such special
// exists.
//
static int EV_StrifeSpecialForStaticInit(int staticFn)
{
   // TODO
   return 0;
}

//
// EV_StrifeStaticInitForSpecial
//
// TODO
//
static int EV_StrifeStaticInitForSpecial(int special)
{
   return 0;
}

//
// EV_PSXSpecialForStaticInit
//
// Always looks up a special in the PSX mission's static init list, regardless
// of the map format or gamemode in use. Returns 0 if no such special exists.
//
static int EV_PSXSpecialForStaticInit(int staticFn)
{
   // small set, so, linear search
   for(size_t i = 0; i < earrlen(PSXStaticBindings); i++)
   {
      if(PSXStaticBindings[i].staticFn == staticFn)
         return PSXStaticBindings[i].actionNumber;
   }

   // otherwise, check the DOOM lookup
   return EV_DOOMSpecialForStaticInit(staticFn);
}

//
// EV_PSXStaticInitForSpecial
//
// Always looks up a static init function in the PSX mission's static init list,
// regardless of the map format or gamemode in use. Returns 0 if no such special
// exists.
//
static int EV_PSXStaticInitForSpecial(int special)
{
   for(size_t i = 0; i < earrlen(PSXStaticBindings); i++)
   {
      if(PSXStaticBindings[i].actionNumber == special)
         return PSXStaticBindings[i].staticFn;
   }

   // otherwise, check the DOOM lookup
   return EV_DOOMStaticInitForSpecial(special);
}

//
// EV_UDMFEternitySpecialForStaticInit
//
// Always looks up a special in the UDMFEternity gamemode's static init list, 
// regardless of the map format or gamemode in use. Returns 0 if no such 
// special exists.
//
static int EV_UDMFEternitySpecialForStaticInit(int staticFn)
{
   ev_static_t *binding;

   // init the hash if it hasn't been done yet
   EV_initUDMFEternityStaticHash();

   if((binding = UDMFEternityStaticHash.objectForKey(staticFn)))
      return binding->actionNumber;
   else
      return EV_HexenSpecialForStaticInit(staticFn);
}

//
// EV_UDMFEternityStaticInitForSpecial
//
// Always looks up a static init function in the UDMFEternity gamemode's static init
// list, regardless of the map format or gamemode in use. Returns 0 if the given
// special isn't bound to a static init function.
//
static int EV_UDMFEternityStaticInitForSpecial(int special)
{
   ev_static_t *binding;

   // Early return for special 0 or 121
   if(!special || special == 121)
      return EV_STATIC_NULL;

   // init the hash if it hasn't been done yet
   EV_initUDMFEternityStaticHash();
   if((binding = UDMFEternityStaticSpecHash.objectForKey(special)))
      return binding->staticFn;
   else
      return EV_HexenStaticInitForSpecial(special);
}

//
// EV_SpecialForStaticInit
//
// Pass in the symbolic static function name you want the line special for; it
// will return the line special number currently bound to that function for the
// currently active map type. If zero is returned, that static function has no
// binding for the current map.
//
int EV_SpecialForStaticInit(int staticFn)
{
   switch(LevelInfo.mapFormat)
   {
   case LEVEL_FORMAT_UDMF_ETERNITY:
      return EV_UDMFEternitySpecialForStaticInit(staticFn);
   case LEVEL_FORMAT_HEXEN:
      return EV_HexenSpecialForStaticInit(staticFn);
   case LEVEL_FORMAT_PSX:
      return EV_PSXSpecialForStaticInit(staticFn);
   default:
      switch(LevelInfo.levelType)
      {
      case LI_TYPE_DOOM: 
      default:
         return EV_DOOMSpecialForStaticInit(staticFn);
      case LI_TYPE_HERETIC:
      case LI_TYPE_HEXEN:   // matches ZDoom's default behavior
         return EV_HereticSpecialForStaticInit(staticFn);
      case LI_TYPE_STRIFE:
         return EV_StrifeSpecialForStaticInit(staticFn);
      }
   }      
}

//
// EV_StaticInitForSpecial
//
// Pass in a line special number and the static function ordinal bound to that
// special will be returned. If zero is returned, there is no static function
// bound to that special for the current map.
//
int EV_StaticInitForSpecial(int special)
{
   // Early return for special 0
   if(!special)
      return EV_STATIC_NULL;

   switch(LevelInfo.mapFormat)
   {
   case LEVEL_FORMAT_UDMF_ETERNITY:
      return EV_UDMFEternityStaticInitForSpecial(special);
   case LEVEL_FORMAT_HEXEN:
      return EV_HexenStaticInitForSpecial(special);
   case LEVEL_FORMAT_PSX:
      return EV_PSXStaticInitForSpecial(special);
   default:
      switch(LevelInfo.levelType)
      {
      case LI_TYPE_DOOM:
      default:
         return EV_DOOMStaticInitForSpecial(special);
      case LI_TYPE_HERETIC:
      case LI_TYPE_HEXEN:
         return EV_HereticStaticInitForSpecial(special);
      case LI_TYPE_STRIFE:
         return EV_StrifeStaticInitForSpecial(special);
      }
   }
}

//
// EV_SpecialForStaticInitName
//
// Some static init specials have symbolic names. This will
// return the bound special for such a name if one exists.
//
int EV_SpecialForStaticInitName(const char *name)
{
   struct staticname_t
   {
      int staticFn;
      const char *name;
   };
   static staticname_t namedStatics[] =
   {
      { EV_STATIC_3DMIDTEX_ATTACH_PARAM,   "Sector_Attach3dMidtex"  },
      { EV_STATIC_INIT_PARAM,              "Static_Init"            },
      { EV_STATIC_PORTAL_LINE_PARAM_QUICK, "Line_QuickPortal"       },
      { EV_STATIC_PORTAL_LINE_PARAM_COMPAT, "Line_SetPortal"        },
      { EV_STATIC_SLOPE_PARAM,             "Plane_Align"            },
      { EV_STATIC_SLOPE_PARAM_TAG,         "Plane_Copy"             },
      { EV_STATIC_POLYOBJ_START_LINE,      "Polyobj_StartLine"      },
      { EV_STATIC_POLYOBJ_EXPLICIT_LINE,   "Polyobj_ExplicitLine"   },
      { EV_STATIC_PUSHPULL_CONTROL_PARAM,  "PointPush_SetForce"     },
      { EV_STATIC_PORTAL_DEFINE,           "Portal_Define"          },
      { EV_STATIC_SCROLL_BY_OFFSETS_PARAM, "Scroll_Texture_Offsets" },
      { EV_STATIC_SCROLL_CEILING_PARAM,    "Scroll_Ceiling"         },
      { EV_STATIC_SCROLL_FLOOR_PARAM,      "Scroll_Floor"           },
      { EV_STATIC_SCROLL_LEFT_PARAM,       "Scroll_Texture_Left"    },
      { EV_STATIC_SCROLL_WALL_PARAM,       "Scroll_Texture_Model"   },
      { EV_STATIC_SCROLL_RIGHT_PARAM,      "Scroll_Texture_Right"   },
      { EV_STATIC_SCROLL_UP_PARAM,         "Scroll_Texture_Up"      },
      { EV_STATIC_SCROLL_DOWN_PARAM,       "Scroll_Texture_Down"    },
      { EV_STATIC_CURRENT_CONTROL_PARAM,   "Sector_SetCurrent"      },
      { EV_STATIC_FRICTION_TRANSFER,       "Sector_SetFriction"     },
      { EV_STATIC_PORTAL_SECTOR_PARAM_COMPAT, "Sector_SetPortal"    },
      { EV_STATIC_WIND_CONTROL_PARAM,      "Sector_SetWind"         },
      { EV_STATIC_PORTAL_HORIZON_LINE,     "Line_Horizon"           },
      { EV_STATIC_LINE_SET_IDENTIFICATION, "Line_SetIdentification" },
      { EV_STATIC_LIGHT_TRANSFER_FLOOR,    "Transfer_FloorLight"    },
      { EV_STATIC_LIGHT_TRANSFER_CEILING,  "Transfer_CeilingLight"  },
      { EV_STATIC_TRANSFER_HEIGHTS,        "Transfer_Heights"       },
   };

   // There aren't enough of these to warrant a hash table. Yet.
   for(size_t i = 0; i < earrlen(namedStatics); i++)
   {
      if(!strcasecmp(namedStatics[i].name, name))
         return EV_SpecialForStaticInit(namedStatics[i].staticFn);
   }

   return 0;
}

//
// EV_IsParamStaticInit
//
// Test if a given line special is bound to a parameterized static init
// function. There are only a handful of these, so they are hard-coded
// here rather than being tablified.
//
// ioanch WARNING: do not put formerly classic specials here, if they've become
// parameterized in the mean time. This may break maps.
//
bool EV_IsParamStaticInit(int special)
{
   switch(EV_StaticInitForSpecial(special))
   {
   case EV_STATIC_3DMIDTEX_ATTACH_PARAM:
   case EV_STATIC_CURRENT_CONTROL_PARAM:
   case EV_STATIC_INIT_PARAM:
   case EV_STATIC_PORTAL_DEFINE:
   case EV_STATIC_PORTAL_LINE_PARAM_QUICK:
   case EV_STATIC_PORTAL_LINE_PARAM_COMPAT:
   case EV_STATIC_SCROLL_WALL_PARAM:
   case EV_STATIC_SLOPE_PARAM:
   case EV_STATIC_SLOPE_PARAM_TAG:
   case EV_STATIC_POLYOBJ_START_LINE:
   case EV_STATIC_POLYOBJ_EXPLICIT_LINE:
   case EV_STATIC_PUSHPULL_CONTROL_PARAM:
   case EV_STATIC_SCROLL_CEILING_PARAM:
   case EV_STATIC_SCROLL_FLOOR_PARAM:
   case EV_STATIC_SCROLL_LEFT_PARAM:
   case EV_STATIC_SCROLL_RIGHT_PARAM:
   case EV_STATIC_SCROLL_UP_PARAM:
   case EV_STATIC_SCROLL_DOWN_PARAM:
   case EV_STATIC_PORTAL_SECTOR_PARAM_COMPAT:
   case EV_STATIC_LINE_SET_IDENTIFICATION:
   case EV_STATIC_WIND_CONTROL_PARAM:
      return true;
   default:
      return false;
   }
}

// EOF

