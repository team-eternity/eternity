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

class  ClipContext;

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)


typedef enum
{
   OF_SETSCEILINGZ = 0x0001, // Indicates this line could lower the ceilingz (not an open ceiling portal)
   OF_SETSFLOORZ   = 0x0002, // Indicates this line could raise the floorz (not an open floor portal)
   OF_TOUCH3DSIDE  = 0x0004, // Indicates the mobj 
} openingflags_e;

typedef struct 
{
   fixed_t    top;      // top of line opening
   fixed_t    bottom;   // bottom of line opening
   fixed_t    range;    // height of opening: top - bottom
   fixed_t    lowfloor;     // lowest floorheight involved   
   fixed_t    secfloor; // SoM 11/3/02: considering only sector floor
   fixed_t    secceil;  // SoM 11/3/02: considering only sector ceiling

   // SoM: Flags describing attributes of the line
   int        openflags;

   // moved front and back outside P_LineOpening and changed -- phares 3/7/98
   // them to these so we can pick up the new friction value
   // in PIT_CheckLine()
   sector_t   *frontsector; // made global
   sector_t   *backsector;  // made global
} open_t;


// flags for controlling radius attack behaviors
enum
{
   RAF_NOSELFDAMAGE = 0x00000001, // explosion does not damage originator
   RAF_CLIPHEIGHT   = 0x00000002, // height range is checked, like Hexen
};


class ClipEngine : public ZoneObject
{
   public:
      virtual ~ClipEngine() {}
      
      virtual bool tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff);
      virtual bool tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff, ClipContext *cc) = 0;
      
      virtual bool tryZMove(Mobj *thing, fixed_t z);
      virtual bool tryZMove(Mobj *thing, fixed_t z, ClipContext *cc) = 0;

      virtual bool makeZMove(Mobj *thing, fixed_t z);
      virtual bool makeZMove(Mobj *thing, fixed_t z, ClipContext *cc) = 0;
      
      virtual bool teleportMove(Mobj *thing, fixed_t x, fixed_t y, bool boss) = 0;

      virtual bool teleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss) = 0;

      virtual bool portalTeleportMove(Mobj *thing, fixed_t x, fixed_t y) = 0;

      virtual void slideMove(Mobj *mo) = 0;

      virtual bool checkPosition(Mobj *thing, fixed_t x, fixed_t y);
      virtual bool checkPosition(Mobj *thing, fixed_t x, fixed_t y, ClipContext *cc) = 0;

      virtual bool checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil);
      virtual bool checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil, ClipContext *cc) = 0;
      virtual bool changeSector(sector_t *sector, int crunch);
      virtual bool changeSector(sector_t *sector, int crunch, ClipContext *cc) = 0;
      virtual bool checkSides(Mobj *actor, int x, int y);
      virtual bool checkSides(Mobj *actor, int x, int y, ClipContext *cc) = 0;

      virtual void        delSeclist(msecnode_t *) = 0;
      virtual void        freeSecNodeList(void);
      
      virtual int  getMoveFactor(Mobj *mo, int *friction) = 0;
      virtual int  getFriction(const Mobj *mo, int *factor) = 0;
      
      virtual void applyTorque(Mobj *mo);
      virtual void applyTorque(Mobj *mo, ClipContext *cc) = 0;
      
      virtual void radiusAttack(Mobj *spot, Mobj *source, int damage, int distance, int mod, unsigned int flags);
      virtual void radiusAttack(Mobj *spot, Mobj *source, int damage, int distance, int mod, unsigned int flags, ClipContext *cc) = 0;

      virtual fixed_t avoidDropoff(Mobj *actor, ClipContext *cc) = 0;
      
      // Utility functions
      virtual void lineOpening(line_t *linedef, Mobj *mo, open_t *opening);
      virtual void lineOpening(line_t *linedef, Mobj *mo, open_t *opening, ClipContext *cc) = 0;
      virtual void unsetThingPosition(Mobj *mo) = 0;
      virtual void setThingPosition(Mobj *mo) = 0;
      
      // Clipping contexts
      virtual ClipContext*  getContext() = 0;
      virtual void          releaseContext(ClipContext *) = 0;
      
     
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

      // Links a mobj to a sector.
      static void          linkMobjToSector(Mobj *mobj, sector_t *sector);

      // Removes all sector links for a given mobj.
      static void          unlinkMobjFromSectors(Mobj *mobj);
      
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



// ----------------------------------------------------------------------------
// Clipping engine selection

enum DoomClipper_e
{
   CLIP_DOOM,
   CLIP_PORTAL,
};

// Global function to set the clipping engine
void P_SetClippingEngine(DoomClipper_e engine);

// This is the reference to the clipping engine currently being used by EE
extern ClipEngine *clip;


#endif //P_CLIP_H__
