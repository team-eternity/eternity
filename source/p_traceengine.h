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
//      Abstract tracer engine
//
//-----------------------------------------------------------------------------


#ifndef P_TRACEENGINE_H
#define P_TRACEENGINE_H


#include "p_mapcontext.h"

struct intercept_t;

typedef bool (*pathfunc_t)(intercept_t *, TracerContext *);

class TracerEngine
{
   public:
      virtual fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int mask, TracerContext *tc) = 0;
      virtual fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int mask);
      
      virtual void    lineAttack(Mobj *t1, angle_t angle, fixed_t distance,
                                 fixed_t slope, int damage) = 0;
                                 
      virtual bool    checkSight(Mobj *t1, Mobj *t2) = 0;

      virtual void    useLines(player_t *player) = 0;
      
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int,
                                   pathfunc_t trav, TracerContext *) = 0;
                             
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int, 
                                   pathfunc_t trav) = 0;
                             
      virtual TracerContext   *getContext() = 0;
      virtual void            freeContext(TracerContext *) = 0;
      
      // HAX
      virtual fixed_t         tracerPuffAttackRange() = 0;
};


// SoM: Because these aren't very big, I'm just including them here
class DoomTraceEngine : public TracerEngine
{
   public:
      DoomTraceEngine();
      virtual ~DoomTraceEngine() {}
      
      virtual fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int mask, TracerContext *tc);
      
      virtual void    lineAttack(Mobj *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage);
                                 
      virtual bool    checkSight(Mobj *t1, Mobj *t2);

      virtual void    useLines(player_t *player);
      
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int, pathfunc_t trav, TracerContext *);
                             
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int, pathfunc_t trav);      
      
      virtual TracerContext   *getContext() {return &tracec;}
      virtual void            freeContext(TracerContext *) {}
      
      // HAX
      virtual fixed_t         tracerPuffAttackRange();
      
   private:
      TracerContext tracec;
};




// ----------------------------------------------------------------------------
// Tracer engine selection

// This is actually selected along side the clipping engine in 
// P_SetClippingEngine
extern TracerEngine *trace;


#endif