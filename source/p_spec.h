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
//--------------------------------------------------------------------------
//
// DESCRIPTION:  definitions, declarations and prototypes for specials
//
//-----------------------------------------------------------------------------

#ifndef P_SPEC_H__
#define P_SPEC_H__

// HEADER_FIXME: Needs to be broken up, too much intermixed functionality.

// Required for: fixed_t
#include "m_fixed.h"

// Required for: Thinker
#include "p_tick.h"

struct line_t;
class  Mobj;
struct player_t;
class  SaveArchive;
struct sector_t;
struct side_t;

//      Define values for map objects
#define MO_TELEPORTMAN  14

// p_floor

#define ELEVATORSPEED (FRACUNIT*4)
#define FLOORSPEED     FRACUNIT

// p_ceilng

#define CEILSPEED   FRACUNIT
#define CEILWAIT    150

// p_doors

#define VDOORSPEED  (FRACUNIT*2)
#define VDOORWAIT   150

// p_plats

#define PLATWAIT    3

#define PLATSPEED   FRACUNIT

// p_switch

// 4 players, 4 buttons each at once, max.
// killough 2/14/98: redefine in terms of MAXPLAYERS
// #define MAXBUTTONS    (MAXPLAYERS*4)

// 1 second, in ticks. 
#define BUTTONTIME  TICRATE

// p_lights

#define GLOWSPEED       8
#define GLOWSPEEDSLOW   3*FRACUNIT/2
#define STROBEBRIGHT    5
#define FASTDARK        15
#define SLOWDARK        35

//jff 3/14/98 add bits and shifts for generalized sector types
#define LIGHT_MASK      0x1f
#define DAMAGE_MASK     0x60
#define DAMAGE_SHIFT    5
#define SECRET_MASK     0x80
#define SECRET_SHIFT    7
#define FRICTION_MASK   0x100
#define FRICTION_SHIFT  8
#define PUSH_MASK       0x200
#define PUSH_SHIFT      9
#define KILLSOUND_MASK  0x400
#define KILLSOUND_SHIFT 10
#define MOVESOUND_MASK  0x800
#define MOVESOUND_SHIFT 11

// haleyjd 12/28/08: mask used to get generalized special bits that are now
// part of the sector flags
#define GENSECTOFLAGSMASK \
   (SECRET_MASK|FRICTION_MASK|PUSH_MASK|KILLSOUND_MASK|MOVESOUND_MASK)

//jff 02/04/98 Define masks, shifts, for fields in 
// generalized linedef types

#define GenFloorBase          0x6000
#define GenCeilingBase        0x4000
#define GenDoorBase           0x3c00
#define GenLockedBase         0x3800
#define GenLiftBase           0x3400
#define GenStairsBase         0x3000
#define GenCrusherBase        0x2F80

#define TriggerType           0x0007
#define TriggerTypeShift      0

// haleyjd: Generalized type enumeration
enum
{
   GenTypeFloor,
   GenTypeCeiling,
   GenTypeDoor,
   GenTypeLocked,
   GenTypeLift,
   GenTypeStairs,
   GenTypeCrusher
};

// define masks and shifts for the floor type fields

#define FloorCrush            0x1000
#define FloorChange           0x0c00
#define FloorTarget           0x0380
#define FloorDirection        0x0040
#define FloorModel            0x0020
#define FloorSpeed            0x0018

#define FloorCrushShift           12
#define FloorChangeShift          10
#define FloorTargetShift           7
#define FloorDirectionShift        6
#define FloorModelShift            5
#define FloorSpeedShift            3
                               
// define masks and shifts for the ceiling type fields

#define CeilingCrush          0x1000
#define CeilingChange         0x0c00
#define CeilingTarget         0x0380
#define CeilingDirection      0x0040
#define CeilingModel          0x0020
#define CeilingSpeed          0x0018

#define CeilingCrushShift         12
#define CeilingChangeShift        10
#define CeilingTargetShift         7
#define CeilingDirectionShift      6
#define CeilingModelShift          5
#define CeilingSpeedShift          3

// define masks and shifts for the lift type fields

#define LiftTarget            0x0300
#define LiftDelay             0x00c0
#define LiftMonster           0x0020
#define LiftSpeed             0x0018

#define LiftTargetShift            8
#define LiftDelayShift             6
#define LiftMonsterShift           5
#define LiftSpeedShift             3

// define masks and shifts for the stairs type fields

#define StairIgnore           0x0200
#define StairDirection        0x0100
#define StairStep             0x00c0
#define StairMonster          0x0020
#define StairSpeed            0x0018

#define StairIgnoreShift           9
#define StairDirectionShift        8
#define StairStepShift             6
#define StairMonsterShift          5
#define StairSpeedShift            3

// define masks and shifts for the crusher type fields

#define CrusherSilent         0x0040
#define CrusherMonster        0x0020
#define CrusherSpeed          0x0018

#define CrusherSilentShift         6
#define CrusherMonsterShift        5
#define CrusherSpeedShift          3

// define masks and shifts for the door type fields

#define DoorDelay             0x0300
#define DoorMonster           0x0080
#define DoorKind              0x0060
#define DoorSpeed             0x0018

#define DoorDelayShift             8
#define DoorMonsterShift           7
#define DoorKindShift              5
#define DoorSpeedShift             3

// define masks and shifts for the locked door type fields

#define LockedNKeys           0x0200
#define LockedKey             0x01c0
#define LockedKind            0x0020
#define LockedSpeed           0x0018

#define LockedNKeysShift           9
#define LockedKeyShift             6
#define LockedKindShift            5
#define LockedSpeedShift           3

// define names for the TriggerType field of the general linedefs

typedef enum
{
   WalkOnce,
   WalkMany,
   SwitchOnce,
   SwitchMany,
   GunOnce,
   GunMany,
   PushOnce,
   PushMany
} triggertype_e;

// define names for the Speed field of the general linedefs

typedef enum
{
   SpeedSlow,
   SpeedNormal,
   SpeedFast,
   SpeedTurbo,  
   SpeedParam  // haleyjd 05/04/04: parameterized extension 
} motionspeed_e;

// define names for the Target field of the general floor

typedef enum
{
   FtoHnF,
   FtoLnF,
   FtoNnF,
   FtoLnC,
   FtoC,
   FbyST,
   Fby24,
   Fby32,
  
   FbyParam, // haleyjd 05/07/04: parameterized extensions
   FtoAbs,
   FInst
} floortarget_e;

// define names for the Changer Type field of the general floor

typedef enum
{
   FNoChg,
   FChgZero,
   FChgTxt,
   FChgTyp
} floorchange_e;

// define names for the Change Model field of the general floor

typedef enum
{
   FTriggerModel,
   FNumericModel
} floormodel_t;

// define names for the Target field of the general ceiling

typedef enum
{
   CtoHnC,
   CtoLnC,
   CtoNnC,
   CtoHnF,
   CtoF,
   CbyST,
   Cby24,
   Cby32,

   CbyParam, // haleyjd 05/07/04: parameterized extensions
   CtoAbs,
   CInst
} ceilingtarget_e;

// define names for the Changer Type field of the general ceiling

typedef enum
{
   CNoChg,
   CChgZero,
   CChgTxt,
   CChgTyp
} ceilingchange_e;

// define names for the Change Model field of the general ceiling

typedef enum
{
   CTriggerModel,
   CNumericModel
} ceilingmodel_t;

// define names for the Target field of the general lift

typedef enum
{
   F2LnF,
   F2NnF,
   F2LnC,
   LnF2HnF
} lifttarget_e;

// haleyjd 10/06/05: defines for generalized stair step sizes
 
typedef enum
{
   StepSize4,
   StepSize8,
   StepSize16,
   StepSize24,

   StepSizeParam  // haleyjd 10/06/05: parameterized extension
} genstairsize_e;

// define names for the door Kind field of the general ceiling

typedef enum
{
   OdCDoor,
   ODoor,
   CdODoor,
   CDoor,
  
   // haleyjd 03/01/05: new param types with initial delays
   pDOdCDoor,
   pDCDoor
} doorkind_e;

// define names for the locked door Kind field of the general ceiling

typedef enum
{
   AnyKey,
   RCard,
   BCard,
   YCard,
   RSkull,
   BSkull,
   YSkull,
   AllKeys,
   MaxKeyKind
} keykind_e;

//////////////////////////////////////////////////////////////////
//
// enums for classes of linedef triggers
//
//////////////////////////////////////////////////////////////////

//jff 2/23/98 identify the special classes that can share sectors

typedef enum
{
   floor_special,
   ceiling_special,
   lighting_special
} special_e;

//jff 3/15/98 pure texture/type change for better generalized support
typedef enum
{
   trigChangeOnly,
   numChangeOnly
} change_e;

// p_plats

typedef enum
{
   perpetualRaise,
   downWaitUpStay,
   raiseAndChange,
   raiseToNearestAndChange,
   blazeDWUS,
   genLift,       // jff added to support generalized Plat types
   genPerpetual, 
   toggleUpDn,    // jff 3/14/98 added to support instant toggle type
   upWaitDownStay // haleyjd 2/18/13: Hexen and Strife reverse plats

} plattype_e;

// haleyjd 02/18/13: parameterized plat trigger types
typedef enum
{
   paramDownWaitUpStay,
   paramDownByValueWaitUpStay,
   paramUpWaitDownStay,
   paramUpByValueWaitDownStay,
   paramPerpetualRaise
} paramplattype_e;

// p_doors

// haleyjd 03/17/02: the names normal, close, and open are too
// common to be used unprefixed in a global enumeration, so they
// have been changed

typedef enum
{
   doorNormal,
   closeThenOpen,
   doorClose,
   doorOpen,
   doorRaiseIn,
   blazeRaise,
   blazeOpen,
   blazeClose,

   // jff 02/05/98 add generalize door types
   // haleyjd 01/22/12: no distinction is necessary any longer

   // haleyjd 03/01/05: exclusively param door types
   paramCloseIn
} vldoor_e;

// haleyjd 05/04/04: door wait types
typedef enum
{
   doorWaitOneSec,
   doorWaitStd,
   doorWaitStd2x,
   doorWaitStd7x,
   doorWaitParam
} doorwait_e;

// p_ceilng

typedef enum
{
   lowerToFloor,
   raiseToHighest,
   lowerToLowest,
   lowerToMaxFloor,
   lowerAndCrush,
   crushAndRaise,
   fastCrushAndRaise,
   silentCrushAndRaise,

   //jff 02/04/98 add types for generalized ceiling mover
   genCeiling,
   genCeilingChg,
   genCeilingChg0,
   genCeilingChgT,

   //jff 02/05/98 add types for generalized ceiling mover
   genCrusher,
   genSilentCrusher
} ceiling_e;

// p_floor

typedef enum
{
   // lower floor to highest surrounding floor
   lowerFloor,
  
   // lower floor to lowest surrounding floor
   lowerFloorToLowest,
  
   // lower floor to highest surrounding floor VERY FAST
   turboLower,
  
   // raise floor to lowest surrounding CEILING
   raiseFloor,
  
   // raise floor to next highest surrounding floor
   raiseFloorToNearest,

   //jff 02/03/98 lower floor to next lowest neighbor
   lowerFloorToNearest,

   //jff 02/03/98 lower floor 24 absolute
   lowerFloor24,

   //jff 02/03/98 lower floor 32 absolute
   lowerFloor32Turbo,

   // raise floor to shortest height texture around it
   raiseToTexture,
  
   // lower floor to lowest surrounding floor
   //  and change floorpic
   lowerAndChange,

   raiseFloor24,

   //jff 02/03/98 raise floor 32 absolute
   raiseFloor32Turbo,

   raiseFloor24AndChange,
   raiseFloorCrush,

   // raise to next highest floor, turbo-speed
   raiseFloorTurbo,       
   donutRaise,
   raiseFloor512,

   //jff 02/04/98  add types for generalized floor mover
   genFloor,
   genFloorChg,
   genFloorChg0,
   genFloorChgT,

   //new types for stair builders
   buildStair,
   genBuildStair,
   genWaitStair,  // haleyjd 10/10/05: stair resetting
   genDelayStair, // haleyjd 10/13/05: delayed stair
   genResetStair, 

   // new types for supporting other idTech games
   turboLowerA    // haleyjd 02/09/13: for Heretic turbo floors
} floor_e;

typedef enum
{
   build8, // slowly build by 8
   turbo16 // quickly build by 16    
} stair_e;

typedef enum
{
   elevateUp,
   elevateDown,
   elevateCurrent
} elevator_e;

// haleyjd 01/09/07: p_lights

typedef enum
{
   setlight_set, // set light to given level
   setlight_add, // add to light level
   setlight_sub  // subtract from light level
} setlight_e;

typedef enum
{
   fade_once, // just a normal fade effect
   fade_glow  // glow effect
} lightfade_e;

// haleyjd 06/29/14: second argument to P_SpawnPSXGlowingLight
typedef enum
{
   psxglow_low, // glow down to lowest neighboring lightlevel
   psxglow_10,  // glow down to 10 from sector->lightlevel
   psxglow_255  // glow up to 255 from sector->lightlevel
} psxglow_e;

//////////////////////////////////////////////////////////////////
//
// general enums
//
//////////////////////////////////////////////////////////////////

// texture type enum
typedef enum
{
   top,
   middle,
   bottom      
} bwhere_e;

//=============================================================================
//
// linedef and sector special data types
//

// haleyjd 10/13/2011: base class for sector action types
class SectorThinker : public Thinker
{
   DECLARE_THINKER_TYPE(SectorThinker, Thinker)

protected:
   // sector attach points
   typedef enum
   {
      ATTACH_NONE, 
      ATTACH_FLOOR,
      ATTACH_CEILING,
      ATTACH_FLOORCEILING,
      ATTACH_LIGHT
   } attachpoint_e;

   // Methods
   virtual attachpoint_e getAttachPoint() const { return ATTACH_NONE; }

public:
   SectorThinker() : Thinker(), sector(NULL) {}

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player) { return false; }

   // Data Members
   sector_t *sector;
};


// p_switch

// switch animation structure type

//jff 3/23/98 pack to read from memory
#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

struct switchlist_s
{
  char    name1[9];
  char    name2[9];
  int16_t episode;
}; 

typedef struct switchlist_s switchlist_t;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

typedef struct button_s
{
   int      line;
   int      side;
   int      where;
   int      btexture;
   int      btimer;
   bool     dopopout;
} button_t;

// haleyjd 04/17/08: made buttonlist/numbuttonsalloc external for savegames
extern button_t *buttonlist;
extern int numbuttonsalloc;

// p_lights

class FireFlickerThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(FireFlickerThinker, SectorThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int count;
   int maxlight;
   int minlight;
};

class LightFlashThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(LightFlashThinker, SectorThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player);
   
   // Data Members
   int count;
   int maxlight;
   int minlight;
   int maxtime;
   int mintime;
};

class StrobeThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(StrobeThinker, SectorThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player);

   // Data Members
   int count;
   int minlight;
   int maxlight;
   int darktime;
   int brighttime;
};

class GlowThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(GlowThinker, SectorThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int     minlight;
   int     maxlight;
   int     direction;
};

// haleyjd: For PSX lighting
class SlowGlowThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(SlowGlowThinker, SectorThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int     minlight;
   int     maxlight;
   int     direction;
   fixed_t accum;
};

// sf 13/10/99
// haleyjd 01/10/06: revised for parameterized line specs

class LightFadeThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(LightFadeThinker, SectorThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   fixed_t lightlevel;
   fixed_t destlevel;
   fixed_t step;
   fixed_t glowmin;
   fixed_t glowmax;
   int     glowspeed;
   int     type;
};

class PhasedLightThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(PhasedLightThinker, SectorThinker)

protected:
   void Think();

   // Data members
   int base;
   int index;

public:
   // Methods
   virtual void serialize(SaveArchive &arc);

   // Statics
   static void Spawn(sector_t *sector, int base, int index);
   static void SpawnSequence(sector_t *sector, int step);
};

// p_plats

class PlatThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(PlatThinker, SectorThinker)

public:
   // Enumerations
   typedef enum
   {
      up,
      down,
      waiting,
      in_stasis
   } plat_e;

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_FLOOR; }

public:
   // Overridden Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player);

   // Methods
   void addActivePlat();
   void removeActivePlat();

   // Static Methods
   static void ActivateInStasis(int tag);
   static void RemoveAllActivePlats();    // killough

   // Data Members
   fixed_t speed;
   fixed_t low;
   fixed_t high;
   int wait;
   int count;
   int status;
   int oldstatus;
   int crush;
   int tag;
   int type;
   struct platlist_t *list;   // killough
};

// p_ceilng

// haleyjd: ceiling noise levels
enum
{
   CNOISE_NORMAL,     // plays plat move sound
   CNOISE_SEMISILENT, // plays plat stop at end of strokes
   CNOISE_SILENT,     // plays silence sequence (not same as silent flag!)
};

class VerticalDoorThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(VerticalDoorThinker, SectorThinker)

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_CEILING; }

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player);

   // Data Members
   int type;
   fixed_t topheight;
   fixed_t speed;

   // 1 = up, 0 = waiting at top, -1 = down
   int direction;

   // tics to wait at the top
   int topwait;
   // (keep in case a door going down is reset)
   // when it reaches 0, start going down
   int topcountdown;

   int lighttag; //killough 10/98: sector tag for gradual lighting effects

   bool turbo;     // haleyjd: behave as a turbo door, independent of speed
};

// door data flags
enum
{
   DDF_HAVETRIGGERTYPE = 0x00000001, // BOOM-style generalized trigger
   DDF_HAVESPAC        = 0x00000002, // Hexen-style parameterized trigger
   DDF_USEALTLIGHTTAG  = 0x00000004, // use the altlighttag field
   DDF_REUSABLE        = 0x00000008, // action can be retriggered
};

// haleyjd 05/04/04: extended data struct for gen/param doors
typedef struct doordata_s
{
   int     flags;         // flags for action; use DDF values above.
   int     spac;          // valid IFF DDF_HAVESPAC is set
   int     trigger_type;  // valid IFF DDF_HAVETRIGGERTYPE is set

   int     kind;          // kind of door action
   fixed_t speed_value;   // speed of door action
   int     delay_value;   // delay between open and close
   int     altlighttag;   // alternate light tag, if DDF_USEALTLIGHTTAG is set
   int     topcountdown;  // delay before initial activation

   Mobj   *thing;         // activating thing, if any
} doordata_t;

// haleyjd 09/06/07: sector special transfer structure

struct spectransfer_t
{
   int newspecial;
   unsigned int flags;
   int damage;
   int damagemask;
   int damagemod;
   unsigned int damageflags;
};

// p_doors

class CeilingThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(CeilingThinker, SectorThinker)

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_CEILING; }

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player);

   // Data Members
   int type;
   fixed_t bottomheight;
   fixed_t topheight;
   fixed_t speed;
   fixed_t oldspeed;
   int crush;

   //jff 02/04/98 add these to support ceiling changers
   //jff 3/14/98 add to fix bug in change transfers
   spectransfer_t special; // haleyjd 09/06/07: spectransfer
   int texture;

   // 1 = up, 0 = waiting, -1 = down
   int direction;

   // haleyjd: stasis
   bool inStasis;

   // ID
   int tag;                   
   int olddirection;
   struct ceilinglist *list;   // jff 2/22/98 copied from killough's plats
};

typedef struct ceilinglist 
{
  CeilingThinker *ceiling; 
  struct ceilinglist *next,**prev;
} ceilinglist_t;

// haleyjd 01/09/12: ceiling data flags
enum
{
   CDF_HAVETRIGGERTYPE = 0x00000001, // has BOOM-style gen action trigger
   CDF_HAVESPAC        = 0x00000002  // has Hexen-style spac
};

// haleyjd 10/05/05: extended data struct for parameterized ceilings
typedef struct ceilingdata_s
{   
   int flags;        // combination of values above
   int trigger_type; // valid IFF (flags & CDF_HAVETRIGGERTYPE)
   int spac;         // valid IFF (flags & CDF_HAVESPAC)
   
   // generalized values   
   int crush;
   int direction;
   int speed_type;
   int change_type;
   int change_model;
   int target_type;

   // parameterized values
   fixed_t height_value;
   fixed_t speed_value;
} ceilingdata_t;


// p_floor

class FloorMoveThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(FloorMoveThinker, SectorThinker)

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_FLOOR; }

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual bool reTriggerVerticalDoor(bool player);

   // Data Members
   int type;
   int crush;
   int direction;

   // jff 3/14/98 add to fix bug in change transfers
   // haleyjd 09/06/07: spectransfer
   spectransfer_t special;

   int16_t texture;
   fixed_t floordestheight;
   fixed_t speed;
   int resetTime;       // haleyjd 10/13/05: resetting stairs
   fixed_t resetHeight;
   int stepRaiseTime;   // haleyjd 10/13/05: delayed stairs
   int delayTime;       
   int delayTimer;
};

// Floor data flags
enum
{
   FDF_HAVESPAC        = 0x00000001, // has Hexen-style SPAC activation
   FDF_HAVETRIGGERTYPE = 0x00000002, // has BOOM-style generalized trigger type
   FDF_HACKFORDESTHNF  = 0x00000004  // adjust and force_adjust are valid
};

// haleyjd 05/07/04: extended data struct for parameterized floors
typedef struct floordata_s
{   
   // generalized values
   int flags;
   int spac;         // valid IFF flags & FDF_HAVESPAC
   int trigger_type; // valid IFF flags & FDF_HAVETRIGGERTYPE
   
   int crush;
   int direction;
   int speed_type;
   int change_type;
   int change_model;
   int target_type;

   // parameterized values
   fixed_t height_value;
   fixed_t speed_value;
   int     adjust;        // valid IFF flags & FDF_HACKFORDESTHNF
   int     force_adjust;  // valid IFF flags & FDF_HACKFORDESTHNF
} floordata_t;

// haleyjd 01/21/13: stairdata flags
enum
{
   SDF_HAVESPAC        = 0x00000001, // Hexen-style activation
   SDF_HAVETRIGGERTYPE = 0x00000002, // BOOM-style activation
   SDF_IGNORETEXTURES  = 0x00000004, // whether or not to ignore floor textures
   SDF_SYNCHRONIZED    = 0x00000008, // if set, build in sync
};

// haleyjd 10/06/05: extended data struct for parameterized stairs
typedef struct stairdata_s
{   
   int flags;
   int spac;
   int trigger_type;

   // generalized values
   int direction;
   int stepsize_type;
   int speed_type;

   // parameterized values
   fixed_t stepsize_value;
   fixed_t speed_value;
   int delay_value;
   int reset_value;
} stairdata_t;

class ElevatorThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(ElevatorThinker, SectorThinker)

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_FLOORCEILING; }

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int type;
   int direction;
   fixed_t floordestheight;
   fixed_t ceilingdestheight;
   fixed_t speed;
};

// joek: pillars
class PillarThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(PillarThinker, SectorThinker)

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_FLOORCEILING; }

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int ceilingSpeed;
   int floorSpeed;
   int floordest;
   int ceilingdest;
   int direction;
   int crush;
};

// haleyjd 10/21/06: data struct for param pillars
typedef struct pillardata_s
{
   fixed_t speed;  // speed of furthest moving surface
   fixed_t fdist;  // for open, how far to open floor
   fixed_t cdist;  // for open, how far to open ceiling
   fixed_t height; // for close, where to meet
   int     crush;  // amount of crushing damage
   int     tag;    // tag
} pillardata_t;

// haleyjd 06/30/09: waggle floors
class FloorWaggleThinker : public SectorThinker
{
   DECLARE_THINKER_TYPE(FloorWaggleThinker, SectorThinker)

protected:
   void Think();

   virtual attachpoint_e getAttachPoint() const { return ATTACH_FLOOR; }

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   fixed_t originalHeight;
   fixed_t accumulator;
   fixed_t accDelta;
   fixed_t targetScale;
   fixed_t scale;
   fixed_t scaleDelta;
   int ticker;
   int state;
};

// p_spec

// haleyjd 04/11/10: FrictionThinker restored
// phares 3/12/98: added new model of friction for ice/sludge effects

class FrictionThinker : public Thinker
{
   DECLARE_THINKER_TYPE(FrictionThinker, Thinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int friction;      // friction value (E800 = normal)
   int movefactor;    // inertia factor when adding to momentum
   int affectee;      // Number of affected sector
};

// sf: direction plat moving

enum
{
   plat_stop     =  0,
   plat_up       =  1,
   plat_down     = -1,
   plat_special  =  2,  // haleyjd 02/24/05
};


//////////////////////////////////////////////////////////////////
//
// external data declarations
//
//////////////////////////////////////////////////////////////////

//
// End-level timer (-TIMER option)
// frags limit (-frags)
//

extern int             levelTimeLimit;
extern int             levelFragLimit;

extern ceilinglist_t *activeceilings;  // jff 2/22/98

////////////////////////////////////////////////////////////////
//
// Linedef and sector special utility function prototypes
//
////////////////////////////////////////////////////////////////

int twoSided(int sector, int line);

sector_t *getSector(int currentSector, int line, int side);

side_t *getSide(int currentSector, int line, int side);

fixed_t P_FindLowestFloorSurrounding(const sector_t *sec);

fixed_t P_FindHighestFloorSurrounding(const sector_t *sec);

fixed_t P_FindNextHighestFloor(const sector_t *sec, int currentheight);

fixed_t P_FindNextLowestFloor(const sector_t *sec, int currentheight);

fixed_t P_FindLowestCeilingSurrounding(const sector_t *sec); // jff 2/04/98

fixed_t P_FindHighestCeilingSurrounding(const sector_t *sec); // jff 2/04/98

fixed_t P_FindNextLowestCeiling(const sector_t *sec, int currentheight);
   // jff 2/04/98

fixed_t P_FindNextHighestCeiling(const sector_t *sec, int currentheight);
   // jff 2/04/98

fixed_t P_FindShortestTextureAround(int secnum); // jff 2/04/98

fixed_t P_FindShortestUpperAround(int secnum); // jff 2/04/98

sector_t *P_FindModelFloorSector(fixed_t floordestheight, int secnum); //jff 02/04/98

sector_t *P_FindModelCeilingSector(fixed_t ceildestheight, int secnum); //jff 02/04/98 

int P_FindSectorFromLineTag(const line_t *line, int start); // killough 4/17/98

int P_FindLineFromLineTag(const line_t *line, int start);   // killough 4/17/98

int P_FindSectorFromTag(const int tag, int start);        // sf

int P_FindMinSurroundingLight(const sector_t *sector, int max);

sector_t *getNextSector(const line_t *line, const sector_t *sec);

int P_SectorActive(special_e t, const sector_t *s);

bool P_IsSecret(const sector_t *sec);

bool P_WasSecret(const sector_t *sec);

void P_ChangeSwitchTexture(line_t *line, int useAgain, int side);

////////////////////////////////////////////////////////////////
//
// Linedef and sector special handler prototypes
//
////////////////////////////////////////////////////////////////

// p_telept

int EV_Teleport(const line_t *line, int side, Mobj *thing);

// killough 2/14/98: Add silent teleporter
int EV_SilentTeleport(const line_t *line, int side, Mobj *thing);

// killough 1/31/98: Add silent line teleporter
int EV_SilentLineTeleport(const line_t *line, int side,
			              Mobj *thing, bool reverse);

// p_floor

int EV_DoElevator(const line_t *line, elevator_e type);

int EV_BuildStairs(const line_t *line, stair_e type);

int EV_DoFloor(const line_t *line, floor_e floortype);

// p_ceilng

int EV_DoCeiling(const line_t *line, ceiling_e type);

int EV_CeilingCrushStop(const line_t *line);

void P_ChangeCeilingTex(const char *name, int tag);

// p_doors

int EV_VerticalDoor(line_t *line, const Mobj *thing, int lockID);

int EV_DoDoor(const line_t *line, vldoor_e type);

int EV_DoLockedDoor(const line_t *line, vldoor_e type, int lockID,
                    const Mobj *thing);

void EV_OpenDoor(int sectag, int speed, int wait_time);

void EV_CloseDoor(int sectag, int speed);

// p_lights

int EV_StartLightStrobing(const line_t *line);

int EV_TurnTagLightsOff(const line_t *line);

int EV_LightTurnOn(const line_t *line, int bright);

int EV_LightTurnOnPartway(int tag, fixed_t level);  // killough 10/10/98

int EV_SetLight(const line_t *, int tag, setlight_e type, int lvl);
   // haleyjd 01/09/07

int EV_FadeLight(const line_t *, int tag, int destvalue, int speed);
   // haleyjd 01/10/07

// haleyjd 01/10/07:
int EV_GlowLight(const line_t *, int tag, int maxval, int minval, int speed);

// haleyjd 01/16/07:
int EV_StrobeLight(const line_t *, int tag, int maxval, int minval,
                   int maxtime, int mintime);

int EV_FlickerLight(const line_t *, int tag, int maxval, int minval);

// p_floor

int EV_DoChange(const line_t *line, change_e changetype);

int EV_DoDonut(const line_t *line);

int EV_PillarBuild(const line_t *line, const pillardata_t *pd);
   // joek: pillars

int EV_PillarOpen(const line_t *line, const pillardata_t *pd);

int EV_StartFloorWaggle(const line_t *line, int tag, int height, int speed,
                        int offset, int timer);

void P_ChangeFloorTex(const char *name, int tag);

// p_plats

bool EV_DoPlat(const line_t *line, plattype_e type, int amount);
bool EV_DoParamPlat(const line_t *line, const int *args, paramplattype_e type);
bool EV_StopPlatByTag(int tag);
bool EV_StopPlat(const line_t *line);

// p_genlin

int EV_DoParamFloor(const line_t *line, int tag, const floordata_t *fd);
int EV_DoGenFloor(const line_t *line);

int EV_DoParamCeiling(const line_t *line, int tag, const ceilingdata_t *cd);
int EV_DoGenCeiling(const line_t *line);

int EV_DoGenLift(const line_t *line);

int EV_DoParamStairs(const line_t *line, int tag, const stairdata_t *sd);
int EV_DoGenStairs(line_t *line);

int EV_DoGenCrusher(const line_t *line);

int EV_DoParamDoor(const line_t *line, int tag, const doordata_t *dd);
int EV_DoGenDoor(const line_t *line, Mobj *thing);

int EV_DoGenLockedDoor(const line_t *line, Mobj *thing);

void P_ChangeLineTex(const char *texture, int pos, int side, int tag, bool usetag);

// p_things

int EV_ThingProjectile(const int *args, bool gravity);
int EV_ThingSpawn(const int *args, bool fog);
int EV_ThingActivate(int tid);
int EV_ThingDeactivate(int tid);
int EV_ThrustThing(Mobj *activator, const int *args, int side);
int EV_ThrustThingZ(Mobj *activator, const int *args);
int EV_DamageThing(Mobj *activator, const int *args);


////////////////////////////////////////////////////////////////
//
// Linedef and sector special thinker spawning
//
////////////////////////////////////////////////////////////////

// at game start
void P_InitPicAnims();

void P_InitSwitchList();

// at map load
void P_SpawnSpecials();

// 
// P_SpawnDeferredSpecials
//
// SoM: Specials that copy slopes, ect., need to be collected in a separate 
// pass
void P_SpawnDeferredSpecials();

// every tic
void P_UpdateSpecials();

// when needed
bool P_UseSpecialLine(Mobj *thing, line_t *line, int side);

void P_ShootSpecialLine(Mobj *thing, line_t *line, int side);

void P_CrossSpecialLine(line_t *, int side, Mobj *thing); // killough 11/98

void P_PlayerInSpecialSector(player_t *player);
void P_PlayerOnSpecialFlat(const player_t *player);

// p_switch

// haleyjd 10/16/05: moved all button code into p_switch.c
void P_ClearButtons();
void P_RunButtons();

// p_lights

void P_SpawnFireFlicker(sector_t *sector);

void P_SpawnLightFlash(sector_t *sector);

void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync);

void P_SpawnPSXStrobeFlash(sector_t *sector, int speed, bool inSync);

void P_SpawnGlowingLight(sector_t *sector);

void P_SpawnPSXGlowingLight(sector_t *sector, psxglow_e glowtype);

// p_plats

void P_PlatSequence(sector_t *s, const char *seqname);

// p_doors

void P_SpawnDoorCloseIn30(sector_t *sec);

void P_SpawnDoorRaiseIn5Mins(sector_t *sec);

void P_DoorSequence(bool raise, bool turbo, bool bounced, sector_t *s); // haleyjd

// p_floor
void P_FloorSequence(sector_t *s);

// p_ceilng

void P_SetSectorCeilingPic(sector_t *sector, int pic); // haleyjd 08/30/09

void P_RemoveActiveCeiling(CeilingThinker *ceiling);  //jff 2/22/98

void P_RemoveAllActiveCeilings();                //jff 2/22/98

void P_AddActiveCeiling(CeilingThinker *c);

void P_RemoveActiveCeiling(CeilingThinker *c);

int P_ActivateInStasisCeiling(const line_t *line);

void P_CeilingSequence(sector_t *s, int noiseLevel);

// SoM 9/19/02: 3dside movement. :)
void P_AttachLines(const line_t *cline, bool ceiling);
bool P_MoveAttached(const sector_t *sector, bool ceiling, fixed_t delta,
                    int crush);
void P_AttachSectors(const line_t *line, int staticFn);

bool P_Scroll3DSides(const sector_t *sector, bool ceiling, fixed_t delta,
                     int crush);

line_t *P_FindLine(int tag, int *searchPosition);

// haleyjd: sector special transfers
void P_SetupSpecialTransfer(const sector_t *, spectransfer_t *);

void P_ZeroSpecialTransfer(spectransfer_t *);

void P_TransferSectorSpecial(sector_t *, const spectransfer_t *);

void P_DirectTransferSectorSpecial(const sector_t *, sector_t *);

void P_ZeroSectorSpecial(sector_t *);

void P_SetLineID(line_t *line, int id);

// haleyjd: parameterized lines

// param special activation types
enum
{
   SPAC_CROSS,
   SPAC_USE,
   SPAC_IMPACT,
   SPAC_PUSH,
};

extern void P_StartLineScript(line_t *line, Mobj *thing);

#endif

//----------------------------------------------------------------------------
//
// $Log: p_spec.h,v $
// Revision 1.30  1998/05/04  02:22:23  jim
// formatted p_specs, moved a coupla routines to p_floor
//
// Revision 1.28  1998/04/17  10:25:04  killough
// Add P_FindLineFromLineTag()
//
// Revision 1.27  1998/04/14  18:49:50  jim
// Added monster only and reverse teleports
//
// Revision 1.26  1998/04/12  02:05:54  killough
// Add ceiling light setting, start ceiling carriers
//
// Revision 1.25  1998/04/05  13:54:03  jim
// fixed switch change on second activation
//
// Revision 1.24  1998/03/31  16:52:09  jim
// Fixed uninited type field in stair builders
//
// Revision 1.23  1998/03/23  18:38:39  jim
// Switch and animation tables now lumps
//
// Revision 1.22  1998/03/23  15:24:47  phares
// Changed pushers to linedef control
//
// Revision 1.21  1998/03/20  14:24:48  jim
// Gen ceiling target now shortest UPPER texture
//
// Revision 1.20  1998/03/20  00:30:27  phares
// Changed friction to linedef control
//
// Revision 1.19  1998/03/19  16:49:00  jim
// change sector bits to combine ice and sludge
//
// Revision 1.18  1998/03/16  12:39:08  killough
// Add accelerative scrollers
//
// Revision 1.17  1998/03/15  14:39:39  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.16  1998/03/14  17:18:56  jim
// Added instant toggle floor type
//
// Revision 1.15  1998/03/09  07:24:40  killough
// Add ScrollThinker for generalized scrollers
//
// Revision 1.14  1998/03/02  12:11:35  killough
// Add scroll_effect_offset declaration
//
// Revision 1.13  1998/02/27  19:20:42  jim
// Fixed 0 tag trigger activation
//
// Revision 1.12  1998/02/23  23:47:15  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.11  1998/02/23  00:42:12  jim
// Implemented elevators
//
// Revision 1.9  1998/02/17  06:20:32  killough
// Add prototypes, redefine MAXBUTTONS
//
// Revision 1.8  1998/02/13  03:28:17  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.7  1998/02/09  03:09:37  killough
// Remove limit on switches
//
// Revision 1.6  1998/02/08  05:35:48  jim
// Added generalized linedef types
//
// Revision 1.4  1998/02/02  13:43:55  killough
// Add silent teleporter
//
// Revision 1.3  1998/01/30  14:44:03  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:27:29  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
