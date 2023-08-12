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
// DESCRIPTION:
//      Map Objects, MObj, definition and handling.
//
//-----------------------------------------------------------------------------

#ifndef P_MOBJ_H__
#define P_MOBJ_H__

// We need the thinker_t stuff.
#include "p_tick.h"
// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"
#include "doomdef.h"
// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"
#include "m_fixed.h"
#include "m_vector.h"   // ioanch 20160109: for portal rendering
#include "p_map.h"
#include "r_interpolate.h"
#include "r_things.h"   // ioanch 20160109: for portal rendering
#include "tables.h"
#include "linkoffs.h"

struct msecnode_t;
struct player_t;
struct skin_t;
struct divline_t;
class  BloodSpawner;

// Defines

// [XA] VIEWHEIGHT is now a playerclass property

// sf: gravity >>> defaultgravity
#define DEFAULTGRAVITY  FRACUNIT
#define MAXMOVE         (30*FRACUNIT)

#define ONFLOORZ        D_MININT
#define ONCEILINGZ      D_MAXINT
// haleyjd 10/13/02: floatrand 
#define FLOATRANDZ     (D_MAXINT - 1)
#define MINFLTRNDZ     (40*FRACUNIT)

// Time interval for item respawning.
#define ITEMQUESIZE     128

#define FLOATSPEED      (FRACUNIT*4)
#define STOPSPEED       (FRACUNIT/16)

// killough 11/98:
// For torque simulation:
#define OVERDRIVE 6
#define MAXGEAR (OVERDRIVE+16)

// haleyjd 11/28/02: default z coord addend for missile spawn
#define DEFAULTMISSILEZ (4*8*FRACUNIT)

#define NUMMOBJCOUNTERS 8

// Mobjs are attached to subsectors by pointer.
struct line_t;
struct subsector_t;

//
// NOTES: Mobj
//
// Mobjs are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the Mobj.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when Mobjs are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The Mobj->flags element has various bit flags
// used by the simulation.
//
// Every Mobj is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any Mobj that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable Mobj that has its origin contained.  
//
// A valid Mobj is a Mobj that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

// Each sector has a degenmobj in its center for sound origin purposes.
// haleyjd 11/22/10: degenmobj, which has become PointThinker, is now the base
// class for all thinkers that want to be located somewhere in the game world.
class PointThinker : public Thinker
{
   DECLARE_THINKER_TYPE(PointThinker, Thinker)

public:
   PointThinker() : Super(), x(0), y(0), z(0), groupid(0) {}

   // Methods
   virtual void serialize(SaveArchive &arc) override;
   virtual void remove() override;
   
   // Data Members
   fixed_t x, y, z;
   // SoM: yes Quasar, this is entirely necessary
   int     groupid; // The group the sound originated in
};

//
// Keep track, for sprite portal copying
//
struct sprojlast_t
{
   v3fixed_t pos;    // holds coordinates
   uint32_t sprite;  // holds both sprite num and frame num
   float yscale;     // if scale changes, sprojheight may also do
   float xscale;
};

//
// Map Object definition.
//
// killough 2/20/98:
//
// WARNING: Special steps must be taken here if C pointers are added to this 
// Mobj class, or else savegames will crash when loaded.
//
// Do not add "Mobj *fooptr" without adding code to serialize and deswizzle to
// convert the pointers to ordinals and back for savegames. This was the whole
// reason behind monsters going to sleep when loading savegames (the "target" 
// pointer was simply nullified after loading, to prevent Doom from crashing),
// and the whole reason behind loadgames crashing on savegames of AV attacks.
//
// killough 9/8/98: changed some fields to shorts,
// for better memory usage (if only for cache).
//
class Mobj : public PointThinker
{
   DECLARE_THINKER_TYPE(Mobj, PointThinker)

protected:
   // Data Members
   struct deswizzle_info
   {
      // Add a corresponding field here, with code to handle it in the
      // serialization routines, any time you add an Mobj * to this class.
      unsigned int target;
      unsigned int tracer;
      unsigned int lastenemy;
   };
   deswizzle_info *dsInfo; // valid only during deserialization

   // Methods
   void Think() override;
   void ThinkFast();

   bool shouldApplyTorque();

public:   
   // Virtual methods (overridables)
   // Inherited from Thinker:
   virtual void updateThinker() override;
   virtual void remove() override;
   virtual void serialize(SaveArchive &arc) override;
   virtual void deSwizzle() override;

   // Methods
   void backupPosition();
   // This one only backs up angle against interpolation, while leaving XYZ and portal refs alone
   void backupAngle()
   {
      prevpos.angle = angle;
   }
   void copyPosition(const Mobj *other);
   int getModifiedSpawnHealth() const;
   
   // Data members

   // More list: links in sector (if needed)
   Mobj  *snext;
   Mobj **sprev; // killough 8/10/98: change to ptr-to-ptr

   // ioanch 20160109: sprite projection chains
   DLListItem<spriteprojnode_t> *spriteproj;
   sprojlast_t sprojlast; // coordinates after last check. Initially "invalid"

   //More drawing info: to determine current sprite.
   angle_t     angle;  // orientation
   spritenum_t sprite; // used to find patch_t and flip value
   int         frame;  // might be ORed with FF_FULLBRIGHT

   // Interaction info, by BLOCKMAP.
   // Links in blocks (if needed).
   Mobj  *bnext;
   Mobj **bprev; // killough 8/11/98: change to ptr-to-ptr

   subsector_t *subsector;

   // The closest interval over all contacted Sectors.
   zrefs_t zref;

   // For movement checking.
   fixed_t radius;
   fixed_t height; 

   // Momentums, used to update position.
   fixed_t momx;
   fixed_t momy;
   fixed_t momz;

   // If == validcount, already checked.
   int validcount;

   mobjtype_t  type;
   mobjinfo_t *info;   // mobjinfo[mobj->type]

   int colour; // sf: the sprite colour
   int tranmap;   // the translucency map: MUST BE CACHED IF MODIFIED AT RUNTIME

   // INVENTORY_FIXME: eliminate union
   union
   {
      int            bfgcount;
   } extradata;

   int           tics;   // state tic counter
   state_t      *state;
   unsigned int  flags;
   unsigned int  flags2;    // haleyjd 04/09/99: I know, kill me now
   unsigned int  flags3;    // haleyjd 11/03/02
   unsigned int  flags4;    // haleyjd 09/13/09
   unsigned int  flags5;    // MaxW: 2021/02/14: flags5
   int           intflags;  // killough 9/15/98: internal flags
   int           health;

   // Movement direction, movement generation (zig-zagging).
   int16_t movedir;        // 0-7
   int16_t movecount;      // when 0, select a new dir
   int16_t strafecount;    // killough 9/8/98: monster strafing

   // Thing being chased/attacked (or nullptr),
   // also the originator for missiles.
   Mobj *target;

   // Reaction time: if non 0, don't attack yet.
   // Used by player to freeze a bit after teleporting.
   int16_t reactiontime;   

   // If >0, the current target will be chased no
   // matter what (even if shot by another object)
   int16_t threshold;

   // killough 9/9/98: How long a monster pursues a target.
   int16_t pursuecount;

   int16_t gear; // killough 11/98: used in torque simulation

   // Additional info record for player avatars only.
   // Only valid if thing is a player
   player_t *player;
   skin_t   *skin;   //sf: skin

   // Player number last looked for.
   int16_t lastlook;       

   // For nightmare respawn.
   mapthing_t spawnpoint;     

   // Thing being chased/attacked for tracers.
   Mobj *tracer; 

   // new field: last known enemy -- killough 2/15/98
   Mobj *lastenemy;

   // killough 8/2/98: friction properties part of sectors,
   // not objects -- removed friction properties from here
   // haleyjd 04/11/10: added back for compatibility code segments
   int friction;
   int movefactor;

   // a linked list of sectors where this object appears
   msecnode_t *touching_sectorlist;                 // phares 3/14/98
   msecnode_t *old_sectorlist;                      // haleyjd 04/16/10

   // SEE WARNING ABOVE ABOUT POINTER FIELDS!!!

   // New Fields for Eternity -- haleyjd

   // counters - these were known as special1/2/3 in Heretic and Hexen
   int counters[NUMMOBJCOUNTERS];

   // particle effect flag field
   unsigned int effects; 
   
   int translucency;  // zdoom-style translucency level
   int alphavelocity; // haleyjd 05/23/08: change in translucency
   int floatbob;      // floatbob offset
   int damage;        // haleyjd 08/02/04: copy damage to mobj now
   int dropamount;    // haleyjd 08/05/13: for ammo drops, overrides ammoeffect
   
   fixed_t floorclip; // haleyjd 08/07/04: floor clip amount

   float xscale;      // haleyjd 11/22/09: x scaling
   float yscale;      // haleyjd 11/22/09: y scaling

   prevpos_t prevpos;   // previous position for interpolation

   // scripting fields
   int special;         // special
   int args[NUMMTARGS]; // arguments
   uint16_t tid;        // thing id used by scripts

   // Note: tid chain pointers are NOT serialized in save games,
   // but are restored on load by rehashing the things as they are
   // spawned.
   Mobj  *tid_next;  // ptr to next thing in tid chain
   Mobj **tid_prevn; // ptr to last thing's next pointer
};

//
// MobjFadeThinker
//
// Takes care of processing for Mobj alphavelocity fade effects.
// At most one of these can exist for a given Mobj. Most Mobj do not use
// this effect so in terms of processing power, this makes sense.
//
class MobjFadeThinker : public Thinker
{
   DECLARE_THINKER_TYPE(MobjFadeThinker, Thinker)

protected:
   Mobj *target;
   unsigned int swizzled_target; // for serialization

   virtual void Think() override;

public:
   MobjFadeThinker() : Super(), target(nullptr), swizzled_target(0) {}
   virtual void remove() override;
   virtual void serialize(SaveArchive &arc) override;
   virtual void deSwizzle() override;

   Mobj *getTarget() const { return target; }
   void setTarget(Mobj *pTarget);
};

void P_StartMobjFade(Mobj *mo, int alphavelocity);

// External declarations (formerly in p_local.h) -- killough 5/2/98

// killough 11/98:
// Whether an object is "sentient" or not. Used for environmental influences.
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate != NullStateNum)

extern int iquehead;
extern int iquetail;

enum bloodaction_e : int
{
   BLOOD_SHOT,   // bullet
   BLOOD_IMPACT, // projectile impact
   BLOOD_RIP,    // ripper projectile
   BLOOD_CRUSH,  // crusher blood
   
   NUMBLOODACTIONS
};

int P_FindDoomedNum(int type);

void  P_RespawnSpecials();
Mobj *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type,
                  bool nolastlook = false);
bool  P_SetMobjState(Mobj *mobj, statenum_t state);
Mobj *P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown,
                  bool ptcl, Mobj *shooter, const MetaTable *pufftype = nullptr,
                  const Mobj *hitmobj = nullptr);
void  P_SpawnUnknownThings();
Mobj *P_SpawnMapThing(mapthing_t *mt);
bool  P_CheckMissileSpawn(Mobj *);  // killough 8/2/98
void  P_ExplodeMissile(Mobj *, const sector_t *topedgesec);     // killough
bool P_CheckPortalTeleport(Mobj *mobj);

enum class seekcenter_e : bool
{
   no = false,
   yes = true,
};
bool P_SeekerMissile(Mobj *actor, const angle_t threshold, const angle_t maxturn, const seekcenter_e seekcenter);

//
// Blood spawning
//
class BloodSpawner
{
public:
   Mobj   *target;    // thing being damaged (always valid)
   Mobj   *inflictor; // inflictor (not always valid)
   fixed_t x;         // x position for spawn
   fixed_t y;         // y position for spawn
   fixed_t z;         // z position for spawn
   angle_t dir;       // angular offset (used by particle fx)
   int     damage;    // damage inflicted

   BloodSpawner(Mobj *ptarget, fixed_t px, fixed_t py, fixed_t pz, int pdamage, 
                angle_t pdir, Mobj *pinflictor = nullptr)
      : target(ptarget), inflictor(pinflictor), x(px), y(py), z(pz), dir(pdir),
        damage(pdamage)
   {
   }

   BloodSpawner(Mobj *ptarget, fixed_t px, fixed_t py, fixed_t pz, int pdamage,
                const divline_t &dv, Mobj *pinflictor = nullptr);

   BloodSpawner(Mobj *ptarget, Mobj *source, int pdamage, Mobj *pinflictor = nullptr);

   BloodSpawner(Mobj *crushtarget, int pdamage);

   void spawn(bloodaction_e action) const;
};


// particles and lines: sf
/*
void P_SpawnParticle(fixed_t x, fixed_t y, fixed_t z);
void P_ParticleLine(Mobj *source, Mobj *dest);
*/

fixed_t P_MissileMomz(fixed_t, fixed_t, fixed_t, int);
fixed_t P_PlayerPitchSlope(player_t *player);

// haleyjd 08/08/11: structure for use with P_SpawnMissileEx
struct missileinfo_t
{
   enum
   {
      USEANGLE = 0x01, // angle and momz are valid
      NOFUZZ   = 0x02, // fuzz will not affect aiming
   };

   Mobj       *source; // Object firing the missile
   Mobj       *dest;   // Destination object, if one exists
   mobjtype_t  type;   // Type of object to fire
   fixed_t     z;      // z height to fire from at source
   angle_t     angle;  // angle to fire missile at
   fixed_t     momz;   // z momentum when firing at an angle
   fixed_t     destx;  // target position x (may come from dest)
   fixed_t     desty;  // target position y (may come from dest)
   fixed_t     destz;  // target position z (may come from dest)
   uint32_t    flags;  // flags to affect firing (use enum values)
};

//
// Feedback from targetting
//
struct playertargetinfo_t
{
   bool isfriend;
   fixed_t slope;
};

//
// SpawnPlayerMissile flags
//
enum
{
   SPM_ADDSLOPETOZ = 1,

   SPMAH_FOLLOWTARGETFRIENDSLOPE = 1,
   SPMAH_AIMFRIENDSTOO = 2,
};

Mobj *P_SpawnMissileEx(const missileinfo_t &missileinfo);

// Convenience routines for missile shooting
Mobj *P_SpawnMissile(Mobj *source, Mobj *dest, mobjtype_t type, fixed_t z);
Mobj *P_SpawnPlayerMissile(Mobj *source, mobjtype_t type, unsigned flags = 0,
                           playertargetinfo_t *targetinfo = nullptr);
Mobj *P_SpawnMissileAngle(Mobj *source, mobjtype_t type, angle_t angle, fixed_t momz, fixed_t z);
Mobj *P_SpawnPlayerMissileAngleHeretic(Mobj *source, mobjtype_t type, angle_t angle,
                                       unsigned flags = 0,
                                       const playertargetinfo_t *targetinfo = nullptr);
Mobj *P_SpawnMissileWithDest(Mobj* source, Mobj* dest, mobjtype_t type, fixed_t srcz, 
                             fixed_t destx, fixed_t desty, fixed_t destz);

// new Eternity mobj function prototypes  haleyjd
void P_Massacre(int friends); // haleyjd 1/22/99:  kills everything
bool P_SetMobjStateNF(Mobj *mobj, statenum_t state); // sets state without calling action function
void P_ThrustMobj(Mobj *mo, angle_t angle, fixed_t move);

// TIDs
void  P_InitTIDHash(void);
void  P_AddThingTID(Mobj *mo, int tid);
void  P_RemoveThingTID(Mobj *mo);
Mobj *P_FindMobjFromTID(int tid, Mobj *rover, Mobj *trigger);

void P_AdjustFloorClip(Mobj *thing);

int P_ThingInfoHeight(const mobjinfo_t *mi);
void P_ChangeThingHeights(void);

// extern data
extern fixed_t FloatBobOffsets[64];

// Made these use getThing* to eliminate the code duplication
#define getTargetX(mo) getThingX((mo), (mo)->target)
#define getTargetY(mo) getThingY((mo), (mo)->target)
#define getTargetZ(mo) getThingZ((mo), (mo)->target)

// haleyjd 05/21/08: Functions like the above, but when we have a specific
// Mobj pointer we want to use, and not mo->target.

inline static fixed_t getThingX(Mobj *mo1, Mobj *mo2)
{
   if(!mo2) return 0;
   if(!mo1) return mo2->x;
   return mo2->x + P_GetLinkOffset(mo2->groupid, mo1->groupid)->x;
}

inline static fixed_t getThingY(Mobj *mo1, Mobj *mo2)
{
   if(!mo2) return 0;
   if(!mo1) return mo2->y;
   return mo2->y + P_GetLinkOffset(mo2->groupid, mo1->groupid)->y;
}

inline static fixed_t getThingZ(Mobj *mo1, Mobj *mo2)
{
   if(!mo2) return 0;
   if(!mo1) return mo2->z;
   return mo2->z + P_GetLinkOffset(mo2->groupid, mo1->groupid)->z;
}

bool P_CheckFloorCeilingForSpawning(const Mobj& mobj);

//=============================================================================
//
// Misc. mobj flags
//

enum mobjflags_e : unsigned int
{
   MF_SPECIAL      = 0x00000001, // Call P_SpecialThing when touched.
   MF_SOLID        = 0x00000002, // Blocks.    
   MF_SHOOTABLE    = 0x00000004, // Can be hit.    
   MF_NOSECTOR     = 0x00000008, // Don't use the sector links (invisible but touchable).
   MF_NOBLOCKMAP   = 0x00000010, // Don't use the blocklinks (inert but displayable)
   MF_AMBUSH       = 0x00000020, // Not to be activated by sound, deaf monster.    
   MF_JUSTHIT      = 0x00000040, // Will try to attack right back.
   MF_JUSTATTACKED = 0x00000080, // Will take at least one step before attacking.
   MF_SPAWNCEILING = 0x00000100, // Hangs from ceiling instead of standing on floor.
   MF_NOGRAVITY    = 0x00000200, // Don't apply gravity.
   MF_DROPOFF      = 0x00000400, // Movement flags. This allows jumps from high places.
   MF_PICKUP       = 0x00000800, // For players, will pick up items.
   MF_NOCLIP       = 0x00001000, // Player cheat.
   MF_SLIDE        = 0x00002000, // Player: keep info about sliding along walls.
   MF_FLOAT        = 0x00004000, // Allow moves to any height. For active floaters.
   MF_TELEPORT     = 0x00008000, // Don't cross lines ??? or look at heights on teleport.
   MF_MISSILE      = 0x00010000, // Don't hit same species, explode on block.
   MF_DROPPED      = 0x00020000, // Dropped by a demon, not level spawned. 
   MF_SHADOW       = 0x00040000, // Use fuzzy draw (shadow demons or spectres)
   MF_NOBLOOD      = 0x00080000, // Don't bleed when shot (use puff).
   MF_CORPSE       = 0x00100000, // Don't stop moving halfway off a step.
   MF_INFLOAT      = 0x00200000, // Floating to a height for a move.
   MF_COUNTKILL    = 0x00400000, // On kill, count this enemy object towards intermission kill total. Happy gathering.
   MF_COUNTITEM    = 0x00800000, // On picking up, count this item object towards intermission item total.
   MF_SKULLFLY     = 0x01000000, // Special handling: skull in flight. Neither a cacodemon nor a
                                 // missile. ALSO CHECK intflag MIF_SKULLFLYSEE.
   MF_NOTDMATCH    = 0x02000000, // Don't spawn this object in deathmatch mode (e.g. key cards).
   MF_TRANSLATION  = 0x0c000000, // Player translation mask for sprite re-indexing.
   MF_TRANSSHIFT   = 26,         // Hmm ???. -- well, what? sf  -- Bernd Kremeier again, probably. haleyjd
   MF_TOUCHY       = 0x10000000, // killough 11/98: dies when solids touch it
   MF_BOUNCES      = 0x20000000, // killough 7/11/98: for beta BFG fireballs
   MF_FRIEND       = 0x40000000, // killough 7/18/98: friendly monsters
   MF_TRANSLUCENT  = 0x80000000  // Translucent sprite - phares
};

enum mobjflags2_e : unsigned int
{
   // haleyjd 04/09/99: extended mobj flags
   // More of these will be filled in as I add support.
   MF2_LOGRAV        = 0x00000001,  // subject to low gravity
   MF2_NOSPLASH      = 0x00000002,  // does not splash
   MF2_NOSTRAFE      = 0x00000004,  // never strafes
   MF2_NORESPAWN     = 0x00000008,  // never respawns
   MF2_ALWAYSRESPAWN = 0x00000010,  // respawns in ALL difficulties
   MF2_REMOVEDEAD    = 0x00000020,  // removed shortly after death
   MF2_NOTHRUST      = 0x00000040,  // not affected by push/pull/wind/current
   MF2_NOCROSS       = 0x00000080,  // cannot trigger special lines
   MF2_JUMPDOWN      = 0x00000100,  // if friend, can jump down
   MF2_PUSHABLE      = 0x00000200,  // can be pushed by moving things
   MF2_MAP07BOSS1    = 0x00000400,  // is a MAP07 boss type 1
   MF2_MAP07BOSS2    = 0x00000800,  // is a MAP07 boss type 2
   MF2_E1M8BOSS      = 0x00001000,  // is an E1M8 boss 
   MF2_E2M8BOSS      = 0x00002000,  // is an E2M8 boss
   MF2_E3M8BOSS      = 0x00004000,  // is an E3M8 boss
   MF2_BOSS          = 0x00008000,  // is a boss
   MF2_E4M6BOSS      = 0x00010000,  // is an E4M6 boss
   MF2_E4M8BOSS      = 0x00020000,  // is an E4M8 boss
   MF2_FOOTCLIP      = 0x00040000,  // feet are clipped by liquids
   MF2_FLOATBOB      = 0x00080000,  // uses floatbob z movement
   MF2_DONTDRAW      = 0x00100000,  // doesn't generate vissprite
   MF2_SHORTMRANGE   = 0x00200000,  // has short missile range
   MF2_LONGMELEE     = 0x00400000,  // has long melee range
   MF2_RANGEHALF     = 0x00800000,  // uses half actual distance
   MF2_HIGHERMPROB   = 0x01000000,  // min prob. of miss. att. = 37.5% vs 22%
   MF2_CANTLEAVEFLOORPIC = 0x02000000,  // restricted to current floorpic
   MF2_SPAWNFLOAT    = 0x04000000,  // random initial z coordinate
   MF2_INVULNERABLE  = 0x08000000,  // invulnerable to damage
   MF2_DORMANT       = 0x10000000,  // dormant (internal)
   MF2_SEEKERMISSILE = 0x20000000,  // might use tracer effects
   MF2_DEFLECTIVE    = 0x40000000,  // deflects projectiles
   MF2_REFLECTIVE    = 0x80000000   // reflects projectiles
};

// haleyjd 11/03/02: flags3 -- even more stuff!
enum mobjflags3_e : unsigned int
{
   MF3_GHOST        = 0x00000001,  // heretic ghost effect
   MF3_THRUGHOST    = 0x00000002,  // object passes through ghosts
   MF3_NODMGTHRUST  = 0x00000004,  // don't thrust target on damage
   MF3_ACTSEESOUND  = 0x00000008,  // use seesound as activesound 50% of time
   MF3_LOUDACTIVE   = 0x00000010,  // play activesound full-volume
   MF3_E5M8BOSS     = 0x00000020,  // thing is heretic E5M8 boss
   MF3_DMGIGNORED   = 0x00000040,  // other things ignore its attacks
   MF3_BOSSIGNORE   = 0x00000080,  // attacks ignored if both have flag
   MF3_SLIDE        = 0x00000100,  // slides against walls
   MF3_TELESTOMP    = 0x00000200,  // thing can telefrag other things
   MF3_WINDTHRUST   = 0x00000400,  // affected by heretic wind sectors
   MF3_FIREDAMAGE   = 0x00000800,  // does fire damage
   MF3_KILLABLE     = 0x00001000,  // mobj is killable, but doesn't count
   MF3_DEADFLOAT    = 0x00002000,  // NOGRAVITY isn't removed on death
   MF3_NOTHRESHOLD  = 0x00004000,  // has no target threshold
   MF3_FLOORMISSILE = 0x00008000,  // is a floor missile
   MF3_SUPERITEM    = 0x00010000,  // is a super powerup item
   MF3_NOITEMRESP   = 0x00020000,  // item doesn't respawn
   MF3_SUPERFRIEND  = 0x00040000,  // monster won't attack friends
   MF3_INVULNCHARGE = 0x00080000,  // invincible when skull flying
   MF3_EXPLOCOUNT   = 0x00100000,  // doesn't explode until this expires
   MF3_CANNOTPUSH   = 0x00200000,  // thing can't push pushable things
   MF3_TLSTYLEADD   = 0x00400000,  // uses additive translucency
   MF3_SPACMONSTER  = 0x00800000,  // monster can activate param lines
   MF3_SPACMISSILE  = 0x01000000,  // missile can activate param lines
   MF3_NOFRIENDDMG  = 0x02000000,  // object isn't hurt by friends
   MF3_3DDECORATION = 0x04000000,  // object is a decor. with 3D height info
   MF3_ALWAYSFAST   = 0x08000000,  // object is always in -fast mode
   MF3_PASSMOBJ     = 0x10000000,  // haleyjd: OVER_UNDER
   MF3_DONTOVERLAP  = 0x20000000,  // haleyjd: OVER_UNDER
   MF3_CYCLEALPHA   = 0x40000000,  // alpha cycles from 0 to 65536 perpetually
   MF3_RIP          = 0x80000000   // ripper - goes through everything
};

enum mobjflags4_e : unsigned int
{
   MF4_AUTOTRANSLATE  = 0x00000001, // DOOM sprite is automatically translated
   MF4_NORADIUSDMG    = 0x00000002, // Doesn't take damage from blast radii
   MF4_FORCERADIUSDMG = 0x00000004, // Does radius damage to everything, no exceptions
   MF4_LOOKALLAROUND  = 0x00000008, // Looks all around (like an AMBUSH monster)
   MF4_NODAMAGE       = 0x00000010, // Takes no damage but still reacts normally
   MF4_SYNCHRONIZED   = 0x00000020, // Spawn state tics are not randomized
   MF4_NORANDOMIZE    = 0x00000040, // Missiles' spawn/death state tics non-random
   MF4_BRIGHT         = 0x00000080, // Actor is always fullbright
   MF4_FLY            = 0x00000100, // Actor is flying
   MF4_NORADIUSHACK   = 0x00000200, // Bouncing missiles obey normal radius attack flags
   MF4_NOSOUNDCUTOFF  = 0x00000400, // Actor can play any number of sounds
   MF4_RAVENRESPAWN   = 0x00000800, // Special item respawns Raven-style
   MF4_NOTSHAREWARE   = 0x00001000, // Item won't spawn in shareware gamemodes
   MF4_NOTORQUE       = 0x00002000, // Never subject to torque simulation
   MF4_ALWAYSTORQUE   = 0x00004000, // Subject to torque even if comp_falloff is on
   MF4_NOZERODAMAGE   = 0x00008000, // Missile won't inflict damage if damage is 0
   MF4_TLSTYLESUB     = 0x00010000, // Use subtractive blending map
   MF4_TOTALINVISIBLE = 0x00020000, // Thing is invisible to monsters
   MF4_DRAWSBLOOD     = 0x00040000, // For missiles, spawn blood when hitting bleeding things
   MF4_SPACPUSHWALL   = 0x00080000, // thing can activate push walls
   MF4_NOSPECIESINFIGHT   = 0x00100000, // no infighting in this species, but still damage
   MF4_HARMSPECIESMISSILE = 0x00200000, // harmed even by projectiles of same species
   MF4_FRIENDFOEMISSILE   = 0x00400000, // friends and foes of same species hurt each other
   MF4_BLOODLESSIMPACT    = 0x00800000, // doesn't draw blood when it hits or rips a thing
   MF4_HERETICBOUNCES     = 0x01000000, // thing bounces à la Heretic
   MF4_MONSTERPASS        = 0x02000000, // not blocked by blockmonsters
   MF4_LOWAIMPRIO         = 0x04000000, // less likely to be autoaimed
   MF4_STICKYCARRY        = 0x08000000, // can carry other things on top of it
   MF4_SETTARGETONDEATH   = 0x10000000, // target is updated even when one-shot
   MF4_SLIDEOVERTHINGS    = 0x20000000, // thing will keep sliding when on top of things
   MF4_UNSTEPPABLE        = 0x40000000, // thing cannot be stepped on like stairs
   MF4_RANGEEIGHTH        = 0x80000000  // uses eighth actual distance
};

enum mobjflags5_e : unsigned int
{
   MF5_NOTAUTOAIMED       = 0x00000001, // can't be autoaimed (for real)
   MF5_FULLVOLSOUNDS      = 0x00000002, // full-volume see/death sounds
   MF5_ACTLIKEBRIDGE      = 0x00000004, // unmoved by sector actions, and pickups can sit atop
   MF5_FASTMISSILE        = 0x00000008  // [XA] use Hexen's fast-projectile physics
};

// killough 9/15/98: Same, but internal flags, not intended for .deh
// (some degree of opaqueness is good, to avoid compatibility woes)

//
// IMPORTANT: if you want to reuse any of the UNUSED bits, remember to increment WRITE_SAVE_VERSION
//            and clear them in Mobj::serialize when loading older saves.
//
enum
{
   MIF_FALLING     = 0x00000001, // Object is falling
   MIF_ARMED       = 0x00000002, // Object is armed (for MF_TOUCHY objects)
   MIF_LINEDONE    = 0x00000004, // Object has activated W1 or S1 linedef via DEH frame
   MIF_DIEDFALLING = 0x00000008, // haleyjd: object died by falling
   MIF_ONFLOOR     = 0x00000010, // [UNUSED] SoM: object stands on floor
   MIF_ONSECFLOOR  = 0x00000020, // [UNUSED] SoM: Object stands on sector floor *specific*
   MIF_SCREAMED    = 0x00000040, // haleyjd: player has screamed
   MIF_NOFACE      = 0x00000080, // haleyjd: thing won't face its target
   MIF_CRASHED     = 0x00000100, // haleyjd: thing has entered crashstate
   MIF_NOPTCLEVTS  = 0x00000200, // haleyjd: thing can't trigger particle events
   MIF_ISCHILD     = 0x00000400, // haleyjd: thing spawned as a child
   MIF_NOTOUCH     = 0x00000800, // haleyjd: OVER_UNDER: don't blow up touchies
   MIF_ONMOBJ      = 0x00001000, // haleyjd: OVER_UNDER: is on another thing
   MIF_WIMPYDEATH  = 0x00002000, // haleyjd: for player, died wimpy (10 damage or less)
   MIF_CLEARMOMZ   = 0x00004000, // davidph: clear momz (and this flag) in P_MovePlayer
   MIF_PLYRCORPSE  = 0x00008000, // haleyjd: object has been in the player corpse queue

   // Player sprites must be temporarily hidden using DONTDRAW during quakes,
   // because they change view position. This isn't sufficient however for
   // hiding cross-portal sprite projections, so the HIDDENBYQUAKE internal flag
   // was added to keep track.
   MIF_HIDDENBYQUAKE = 0x00010000,

   // A substitute for calling E_SafeThingName every tic for every Mobj
   MIF_MUSICCHANGER = 0x00020000,

   MIF_MAYPLAYPARTICLESOUNDS = 0x00040000,   // Hint that it may be playing particle sounds
   MIF_SKULLFLYSEE = 0x00080000, // ioanch: when MF_SKULLFLY is set, return to seestate, not spawn

   // these should be cleared when a thing is being raised
   MIF_CLEARRAISED = (MIF_DIEDFALLING|MIF_SCREAMED|MIF_CRASHED|MIF_SKULLFLYSEE|MIF_WIMPYDEATH),
};

//=============================================================================
//
// Functions which depend on flags
//

//
// True if thing is on floor or hanging from ceiling
//
inline static bool P_mobjOnSurface(const Mobj &mobj)
{
   return mobj.z <= mobj.zref.floor || (mobj.z + mobj.height >= mobj.zref.ceiling &&
                                        mobj.flags & MF_SPAWNCEILING && mobj.flags & MF_NOGRAVITY);
}

//
// Common procedure to copy a spawner's friendship status to a spawnee
//
inline static void P_transferFriendship(Mobj &target, const Mobj &source)
{
   target.flags = (target.flags & ~MF_FRIEND) | (source.flags & MF_FRIEND);
}

#endif

//----------------------------------------------------------------------------
//
// $Log: p_mobj.h,v $
// Revision 1.10  1998/05/03  23:45:09  killough
// beautification, fix headers, declarations
//
// Revision 1.9  1998/03/23  15:24:33  phares
// Changed pushers to linedef control
//
// Revision 1.8  1998/03/20  00:30:09  phares
// Changed friction to linedef control
//
// Revision 1.7  1998/03/09  18:27:13  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.6  1998/02/24  08:46:24  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.5  1998/02/20  21:56:34  phares
// Preliminarey sprite translucency
//
// Revision 1.4  1998/02/20  09:51:14  killough
// Add savegame warning
//
// Revision 1.3  1998/02/17  05:48:16  killough
// Add new last enemy field to prevent monster sleepiness
//
// Revision 1.2  1998/01/26  19:27:23  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
