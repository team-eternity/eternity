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
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef P_DOOMEN_H
#define P_DOOMEN_H

struct line_t;
struct msecnode_t;
struct player_t;
struct sector_t;

class  ClipContext;

class DoomClipEngine : public ClipEngine
{
   public:
      virtual ~DoomClipEngine() {}
      
      // killough 3/15/98: add fourth argument to P_TryMove
      virtual bool tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff) = 0;

      // killough 8/9/98: extra argument for telefragging
      virtual bool teleportMove(Mobj *thing, fixed_t x, fixed_t y, bool boss) = 0;

      // haleyjd 06/06/05: new function that won't stick the thing inside inert objects
      virtual bool teleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss) = 0;

      // SoM: new function that won't telefrag things which the transporting mobj isn't
      // touching on the z axis.
      virtual bool portalTeleportMove(Mobj *thing, fixed_t x, fixed_t y) = 0;

      virtual void slideMove(Mobj *mo) = 0;

      virtual bool checkPosition(Mobj *thing, fixed_t x, fixed_t y) = 0;

      virtual bool checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil) = 0;
      virtual bool checkSides(Mobj *, int, int) = 0;

      virtual void        delSeclist(msecnode_t *) = 0;
      virtual void        freeSecNodeList(void) = 0;
      virtual msecnode_t *createSecNodeList(Mobj *,fixed_t, fixed_t) = 0;
      
      virtual int  getMoveFactor(Mobj *mo, int *friction) = 0;
      virtual int  getFriction(const Mobj *mo, int *factor) = 0;
      virtual void applyTorque(Mobj *mo) = 0;
      
      virtual void radiusAttack(Mobj *spot, Mobj *source, int damage, int mod) = 0;
      
      // Clipping contexts
      virtual ClipContext*  getContext() = 0;
      virtual void          freeContext(ClipContext *) = 0;
      
      private ClipContext   cc;
};
#endif // __P_MAP__

//----------------------------------------------------------------------------
//
// $Log: p_map.h,v $
// Revision 1.2  1998/05/07  00:53:07  killough
// Add more external declarations
//
// Revision 1.1  1998/05/03  22:19:23  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
