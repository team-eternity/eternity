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


// SoM: linetracer_t can be cast to divline_t for the appropriate functions but holds much more
// data which is needed for making tracers correctly travel through portals
/*typedef struct linetracer_s
{
   fixed_t     x;
   fixed_t     y;
   fixed_t     dx;
   fixed_t     dy;

   // Moved crappy globals here
   fixed_t     z; // replaces shootz
   int         la_damage;
   fixed_t     attackrange;
   fixed_t     aimslope;
   fixed_t     topslope, bottomslope;

   // SoM: used by aiming TPT
   fixed_t     originx;
   fixed_t     originy;
   fixed_t     originz;

   fixed_t     sin;
   fixed_t     cos;

   // Accumulated travel along the line. Should be the XY distance between (x,y) 
   // and (originx, originy) 
   fixed_t movefrac;


   bool finished;
} linetracer_t;*/

class TracerContext;
class ClipContext;

typedef bool (*pathfunc_t)(intercept_t *, TracerContext *);

class TracerEngine
{
   public:
      virtual fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int mask, TracerContext *tc) = 0;
      
      virtual void    lineAttack(Mobj *t1, angle_t angle, fixed_t distance,
                                 fixed_t slope, int damage) = 0;
                                 
      virtual bool    checkSight(Mobj *t1, Mobj *t2) = 0;

      virtual void    useLines(player_t *player) = 0;
      
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int,
                                   pathfunc_t trav, TracerContext *, 
                                   ClipContext *cc = NULL) = 0;
                             
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int, 
                                   pathfunc_t trav, ClipContext *cc = NULL) = 0;
                             
      virtual TracerContext   *getContext() = 0;
      virtual void            freeContext(TracerContext *) = 0;
};


// SoM: Because these aren't very big, I'm just including them here
class DoomTraceEngine : public TracerEngine
{
   public:
      virtual fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int mask, TracerContext *tc);
      
      virtual void    lineAttack(Mobj *t1, angle_t angle, fixed_t distance,
                                 fixed_t slope, int damage) = 0;
                                 
      virtual bool    checkSight(Mobj *t1, Mobj *t2) = 0;

      virtual void    useLines(player_t *player);
      
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int,
                                   pathfunc_t trav, TracerContext *);
                             
      virtual bool    pathTraverse(fixed_t, fixed_t, fixed_t, fixed_t, int, 
                                   pathfunc_t trav);      
      
      virtual TracerContext   *getContext() = 0;
      virtual void            freeContext(TracerContext *) = 0;
};




// ----------------------------------------------------------------------------
// Tracer engine selection

// This is actually selected along side the clipping engine in 
// P_SetClippingEngine
extern TracerEngine *trace;


#endif