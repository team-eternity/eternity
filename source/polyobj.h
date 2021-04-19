// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// Polyobjects
//
// Movable segs like in Hexen, but more flexible due to application of
// dynamic binary space partitioning theory.
//
// haleyjd 02/16/06
//
//----------------------------------------------------------------------------

#ifndef POLYOBJ_H__
#define POLYOBJ_H__

// Required for: DLListItem, PointThinker (FIXME?), vertex_t
#include "m_dllist.h"
#include "p_mobj.h"
#include "r_defs.h"

//
// Defines
//

// haleyjd: use zdoom-compatible doomednums

#define POLYOBJ_ANCHOR_DOOMEDNUM      9300
#define POLYOBJ_SPAWN_DOOMEDNUM       9301
#define POLYOBJ_SPAWNCRUSH_DOOMEDNUM  9302
#define POLYOBJ_SPAWNDAMAGE_DOOMEDNUM 9303

//
// Polyobject Flags
//
enum
{
   POF_ATTACHED = 0x01, // is attached to the world for rendering (has dynasegs)
   POF_LINKED   = 0x02, // is attached to the world for clipping
   POF_ISBAD    = 0x04, // is bad; should not be attached/linked/moved/rendered
   POF_DAMAGING = 0x08, // does damage just by touching objects
};

struct polymaplink_t;

//
// Polyobject Structure
//
struct polyobj_t
{
   int id;    // numeric id
   int first; // for hashing: index of first polyobject in this hash chain
   int next;  // for hashing: next polyobject in this hash chain

   int mirror; // numeric id of a mirroring polyobject

   subsector_t **dynaSubsecs;  // list of subsectors holding fragments
   int numDSS;                 // number of subsector pointers
   int numDSSAlloc;            // number of subsector pointers allocated

   int numVertices;            // number of vertices (generally == segCount)
   int numVerticesAlloc;       // number of vertices allocated
   vertex_t *origVerts;        // original positions relative to spawn spot
   vertex_t *tmpVerts;         // temporary vertex backups for rotation
   vertex_t **vertices;        // vertices this polyobject must move

   int numLines;               // number of linedefs
   int numLinesAlloc;          // number of linedefs allocated
   line_t **lines;             // linedefs this polyobject must move

   Mobj *spawnSpotMobj;        // for use during init only!
   PointThinker spawnSpot;     // location of spawn spot
   vertex_t centerPt;          // center point
   angle_t angle;              // for rotation

   fixed_t blockbox[4];        // bounding box for clipping
   polymaplink_t *linkhead;    // haleyjd 05/18/06: unlink optimization
   int validcount;             // for clipping: prevents multiple checks
   int damage;                 // damage to inflict on stuck things
   fixed_t thrust;             // amount of thrust to put on blocking objects

   int seqId;                  // 10/17/06: sound sequence id

   Thinker *thinker;           // pointer to a thinker affecting this polyobj

   unsigned int flags;         // 09/11/09: polyobject flags

   size_t numPortals;          // ioanch 20160228: quick cache if it has
   portal_t **portals;         // portals. NO NEED TO SERIALIZE THIS. 
   bool hasLinkedPortals;      // quick check if any portal is linked

};

//
// Polyobject Blockmap Link Structure
//
struct polymaplink_t
{
   DLListItem<polymaplink_t> link; // for blockmap links
   polyobj_t *po;                   // pointer to polyobject
   polymaplink_t *po_next;          // haleyjd 05/18/06: unlink optimization
};

//
// Polyobject Special Thinkers
//

class SaveArchive;

class PolyRotateThinker final : public Thinker
{
   DECLARE_THINKER_TYPE(PolyRotateThinker, Thinker)

protected:
   void Think() override;

public:
   // Methods
   virtual void serialize(SaveArchive &arc) override;
   
   // Data Members
   int polyObjNum;    // numeric id of polyobject (avoid C pointers here)
   int speed;         // speed of movement per frame
   int distance;      // distance to move
   bool hasBeenPositive;  // MaxW: 20160106: flag to differentiate angles of >=180 from < 0
};

class PolyMoveThinker final : public Thinker
{
   DECLARE_THINKER_TYPE(PolyMoveThinker, Thinker)

protected:
   void Think() override;

public:
   // Methods
   virtual void serialize(SaveArchive &arc) override;
   
   // Data Members
   int polyObjNum;     // numeric id of polyobject
   int speed;          // resultant velocity
   int momx;           // x component of speed along angle
   int momy;           // y component of speed along angle
   int distance;       // total distance to move
   unsigned int angle; // angle along which to move
};

//
// This is like above, but with a movement vector instead of angle. Needed because we don't have
// an accurate fixed-point square root function.
//
class PolyMoveXYThinker final : public Thinker
{
   DECLARE_THINKER_TYPE(PolyMoveXYThinker, Thinker)

protected:
   void Think() override;

public:
   void serialize(SaveArchive &arc) override;

   int polyObjNum;
   fixed_t speed;       // velocity absolute (cached)
   v2fixed_t velocity;  // velocity to move
   v2fixed_t distance;  // distance to travel (absolute X and absolute Y)
};

class PolySlideDoorThinker final : public Thinker
{
   DECLARE_THINKER_TYPE(PolySlideDoorThinker, Thinker)

protected:
   void Think() override;

public:
   // Methods
   virtual void serialize(SaveArchive &arc) override;
   
   // Data Members
   int polyObjNum;         // numeric id of affected polyobject
   int delay;              // delay time
   int delayCount;         // delay counter
   int initSpeed;          // initial speed 
   int speed;              // speed of motion
   int initDistance;       // initial distance to travel
   int distance;           // current distance to travel
   unsigned int initAngle; // intial angle
   unsigned int angle;     // angle of motion
   unsigned int revAngle;  // reversed angle to avoid roundoff error
   int momx;               // x component of speed along angle
   int momy;               // y component of speed along angle
   bool closing;           // if true, is closing
};

class PolySwingDoorThinker final : public Thinker
{
   DECLARE_THINKER_TYPE(PolySwingDoorThinker, Thinker)

protected:
   void Think() override;

public:
   // Methods
   virtual void serialize(SaveArchive &arc) override;
   
   // Data Members
   int polyObjNum;        // numeric id of affected polyobject
   int delay;             // delay time
   int delayCount;        // delay counter
   int initSpeed;         // initial speed
   int speed;             // speed of rotation
   int initDistance;      // initial distance to travel
   int distance;          // current distance to travel
   bool closing;          // if true, is closing
   bool hasBeenPositive;  // MaxW: 20160109: flag to differentiate angles of >=180 from < 0
};

//
// Line Activation Data Structures
//

struct polyrotdata_t
{
   int polyObjNum;   // numeric id of polyobject to affect
   int direction;    // direction of rotation
   int speed;        // angular speed
   int distance;     // distance to move
   bool overRide;    // if true, will override any action on the object
};

struct polymovedata_t
{
   int polyObjNum;     // numeric id of polyobject to affect
   fixed_t distance;   // distance to move
   fixed_t speed;      // linear speed
   unsigned int angle; // angle of movement
   bool overRide;      // if true, will override any action on the object
};

// polyobject door types
typedef enum
{
   POLY_DOOR_SLIDE,
   POLY_DOOR_SWING,
} polydoor_e;

struct polydoordata_t
{
   int polyObjNum;     // numeric id of polyobject to affect
   int doorType;       // polyobj door type
   int speed;          // linear or angular speed
   unsigned int angle; // for slide door only, angle of motion
   int distance;       // distance to move
   int delay;          // delay time after opening
};

//
// Polyobj_[OR]_MoveTo[Spot] data
//
struct polymoveto_t
{
   int polyObjNum;   // poly id
   int speed;        // speed
   bool targetMobj;  // true if target is a tid, false if absolute coordinates
   union
   {
      int tid;       // target tid
      v2fixed_t pos; // or coordinates
   };
   bool overRide;    // whether it can override
   Mobj *activator;  // activator if TID == 0
};

//
// Functions
//

polyobj_t *Polyobj_GetForNum(int id);
bool Polyobj_IsSpawnSpot(const Mobj &mo);
void Polyobj_InitLevel(void);
void Polyobj_MoveOnLoad(polyobj_t *po, angle_t angle, fixed_t x, fixed_t y);

int EV_DoPolyDoor(const polydoordata_t *);
int EV_DoPolyObjMove(const polymovedata_t *);
int EV_DoPolyObjRotate(const polyrotdata_t *);
int EV_DoPolyObjStop(int polyObjNum);
int EV_DoPolyObjMoveToSpot(const polymoveto_t &);

//
// External Variables
//

extern polyobj_t *PolyObjects;
extern int numPolyObjects;
extern DLListItem<polymaplink_t> **polyblocklinks; // polyobject blockmap

#endif

// EOF

