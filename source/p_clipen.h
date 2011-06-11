// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Clipping engine
//
//-----------------------------------------------------------------------------


#ifndef P_CLIPEN_H__
#define P_CLIPEN_H__

struct line_t;
struct msecnode_t;
struct player_t;
struct sector_t;


#include "m_collection.h"


#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)



// SoM: This functions as a clipping 'context' now.
class ClipContext
{
   public:
      ClipContext();
      virtual ~ClipContext();
      
      
      // SoM 09/07/02: Solution to problem of monsters walking on 3dsides
      // haleyjd: values for tmtouch3dside:
      // 0 == no 3DMidTex involved in clipping
      // 1 == 3DMidTex involved but not responsible for floorz
      // 2 == 3DMidTex responsible for floorz
      int        touch3dside;

      Mobj       *thing;    // current thing being clipped
      fixed_t    x;         // x position, usually where we want to move
      fixed_t    y;         // y position, usually where we want to move

      fixed_t    bbox[4];   // bounding box for thing/line intersection checks
      fixed_t    floorz;    // floor you'd hit if free to fall
      fixed_t    ceilingz;  // ceiling of sector you're in
      fixed_t    dropoffz;  // dropoff on other side of line you're crossing

      fixed_t    secfloorz; // SoM: floorz considering only sector heights
      fixed_t    secceilz;  // SoM: ceilingz considering only sector heights
      
      fixed_t    passfloorz; // SoM 11/6/02: UGHAH
      fixed_t    passceilz;

      int        floorpic;  // haleyjd: for CANTLEAVEFLOORPIC flag

      int        unstuck;   // killough 8/1/98: whether to allow unsticking

      bool       floatok;   // If "floatok" true, move ok if within floorz - ceilingz   
      bool       felldown;  // killough 11/98: if "felldown" true, object was pushed down ledge

      // keep track of the line that lowers the ceiling,
      // so missiles don't explode against sky hack walls
      line_t     *ceilingline;
      line_t     *blockline;   // killough 8/11/98: blocking linedef
      line_t     *floorline;   // killough 8/1/98: Highest touched floor

      // Temporary holder for thing_sectorlist threads
      // haleyjd: this is now *only* used inside P_CreateSecNodeList and callees
      msecnode_t *sector_list;     // phares 3/16/98
      
      Mobj       *BlockingMobj;    // haleyjd 1/17/00: global hit reference

      // Spechit stuff
      PODCollection<line_t *> spechit;
};



class ClipEngine
{
   public:
      virtual ~ClipEngine() {}
      
      virtual bool tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff, ClipContext *cc);

      virtual bool teleportMove(Mobj *thing, fixed_t x, fixed_t y, bool boss) = 0;

      virtual bool teleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss) = 0;

      virtual bool portalTeleportMove(Mobj *thing, fixed_t x, fixed_t y) = 0;

      virtual void slideMove(Mobj *mo) = 0;

      virtual bool checkPosition(Mobj *thing, fixed_t x, fixed_t y, ClipContext *cc) = 0;

      virtual bool checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil, ClipContext *cc) = 0;
      virtual bool checkSides(Mobj *, int, int, ClipContext *) = 0;

      virtual void        delSeclist(msecnode_t *) = 0;
      virtual void        freeSecNodeList(void) = 0;
      virtual msecnode_t *createSecNodeList(Mobj *,fixed_t, fixed_t) = 0;
      
      virtual int  getMoveFactor(Mobj *mo, int *friction) = 0;
      virtual int  getFriction(const Mobj *mo, int *factor) = 0;
      virtual void applyTorque(Mobj *mo, ClipContext *cc) = 0;
      
      virtual void radiusAttack(Mobj *spot, Mobj *source, int damage, int mod, ClipContext *cc) = 0;
      
      // Clipping contexts
      virtual ClipContext*  getContext() = 0;
      virtual void          freeContext(ClipContext *) = 0;
      
      // Secnodes
      
      // Retrieves a node from the freelist. The calling routine should make sure it
      // sets all fields properly.
      //
      // killough 11/98: reformatted
      //
      static msecnode_t*   getSecnode();

      // Returns a node to the freelist.
      //
      static void          putSecnode(msecnode_t *node);
      
      // phares 3/16/98
      // Searches the current list to see if this sector is already there. If 
      // not, it adds a sector node at the head of the list of sectors this 
      // object appears in. This is called when creating a list of nodes that
      // will get linked in later. Returns a pointer to the new node.
      //
      // killough 11/98: reformatted
      //
      static msecnode_t*   addSecnode(sector_t *s, Mobj *thing, msecnode_t *nextnode);
      
      // Deletes a sector node from the list of sectors this object appears in.
      // Returns a pointer to the next node on the linked list, or NULL.
      //
      // killough 11/98: reformatted
      //
      static msecnode_t*   delSecnode(msecnode_t *node);

   private:
      // Secnode stuffs
      static msecnode_t*   headsecnode;
};



class TracerEngine
{
   public:
      virtual fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance,int mask) = 0;
      
      virtual void    lineAttack(Mobj *t1, angle_t angle, fixed_t distance,
                                 fixed_t slope, int damage) = 0;
                                 
      virtual bool    checkSight(Mobj *t1, Mobj *t2) = 0;

      virtual void    useLines(player_t *player) = 0;
   
      // Accessors
      Mobj*      getLinetarget() const {return linetarget;} 
        
   private:
      Mobj       *linetarget;  // who got hit (or NULL)
};



// ----------------------------------------------------------------------------
// Clipping engine selection

enum DoomClipper_e
{
   Doom,
   Doom3D,
   Portal,
};

// Global function to set the clipping engine
void P_SetClippingEngine(DoomClipper_e engine);

// This is the reference to the clipping engine currently being used by EE
extern ClipEngine *clip;


// ----------------------------------------------------------------------------
// Tracer engine selection

// This is actually selected along side the clipping engine in 
// P_SetClippingEngine
extern TracerEngine *trace;

#endif //P_CLIP_H__
