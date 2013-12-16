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
//      Portal clipping engine
//
//-----------------------------------------------------------------------------

#ifndef P_PORTALCLIP_H
#define P_PORTALCLIP_H

#include "p_clipen.h"
#include "p_mapcontext.h"

class   ClipContext;

class PortalClipEngine : public ClipEngine
{
   public:
      PortalClipEngine() : unused(NULL) {}
      virtual ~PortalClipEngine();
      
      virtual bool tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff, ClipContext *cc);
      virtual bool tryZMove(Mobj *thing, fixed_t z, ClipContext *cc);
      virtual bool makeZMove(Mobj *thing, fixed_t z, ClipContext *cc);

      virtual bool teleportMove(Mobj *thing, fixed_t x, fixed_t y, bool boss);
      virtual bool teleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss);
      virtual bool portalTeleportMove(Mobj *thing, fixed_t x, fixed_t y);

      virtual void slideMove(Mobj *mo);

      virtual bool checkPosition(Mobj *thing, fixed_t x, fixed_t y, ClipContext *cc);

      virtual bool changeSector(sector_t *sector, int crunch, ClipContext *cc);
      virtual bool checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil, ClipContext *cc);
      virtual bool checkSides(Mobj *actor, int x, int y, ClipContext *cc);

      virtual int  getMoveFactor(Mobj *mo, int *friction);
      virtual int  getFriction(const Mobj *mo, int *factor);
      virtual void applyTorque(Mobj *mo, ClipContext *cc);
      
      virtual void radiusAttack(Mobj *spot, Mobj *source, int damage, int distance, int mod, unsigned int flags, ClipContext *cc);

      virtual fixed_t avoidDropoff(Mobj *actor, ClipContext *cc);
      
      // Utility functions
      virtual void lineOpening(line_t *linedef, Mobj *mo, open_t *opening, ClipContext *cc);
      virtual void unsetThingPosition(Mobj *mo);
      virtual void setThingPosition(Mobj *mo);
      virtual void setThingPosition(Mobj *mo, ClipContext *cc, bool findPortals);

      // Clipping contexts
      virtual ClipContext*  getContext();
      virtual void          releaseContext(ClipContext *);

   protected:
      ClipContext    *unused;

      // Internal utilities
      virtual void addMobjSectorLink(Mobj *mobj, sector_t *sector);
      virtual void unlinkMobjFromSectors(Mobj *mobj);

      void gatherSectorLinks(Mobj *thing, ClipContext *cc);
      void addMobjBlockLinks(ClipContext *cc);
      void delSeclist(msecnode_t *);
      void delSectorLinks(msecnode_t *node);
      void delTouchingThingChain(msecnode_t *node);

      // Iterators
      static bool PIT_FindAdjacentPortals(line_t *line, MapContext *context);
      static void findAdjacentPortals(ClipContext *cc);

      // Blockmap iterators
      static bool blockThingsIterator(int x, int y, bool (*func)(Mobj *, MapContext *), MapContext *mc);
      static bool blockLinesIterator(int x, int y, bool (*func)(line_t *, MapContext *), MapContext *mc);
};


// Utility
bool LineIntersectsBBox(ClipContext *cc, line_t *line);
void CalculateBBoxForThing(ClipContext *cc, fixed_t x, fixed_t y, fixed_t radius, linkoffset_t *link);
void GetBlockmapBoundsFromBBox(ClipContext *cc, int &xl, int &yl, int &xh, int &yh);
void HitPortalGroup(int groupid, ClipContext *cc);
void CheckSectorPortals(sector_t *sector, ClipContext *cc);

//
// MarkVector
//
class MarkVector : public ZoneObject
{
public:
   MarkVector(size_t size);
   virtual ~MarkVector();

   // Clears all marks in the vector. This will only memset over the range of words that have been marked
   void clearMarks();

   // Sets the bit at the given index. Returns true if the bit was previously set.
   bool mark(size_t itemIndex);

   // Returns true if the bit at the given index is set.
   bool isMarked(size_t itemIndex) const { return !!(markArray[itemIndex >> 5] & (1 << (itemIndex & 31))); };

protected:
   size_t  arraySize;
   size_t  firstDirty;
   size_t  lastDirty;
   int     *markArray;
};



#endif // P_PORTALCLIP_H
