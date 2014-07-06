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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Scrollers, carriers, and related effects
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "ev_specials.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_scroll.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"


IMPLEMENT_THINKER_TYPE(ScrollThinker)

// killough 2/28/98:
//
// This function, with the help of r_plane.c and r_bsp.c, supports generalized
// scrolling floors and walls, with optional mobj-carrying properties, e.g.
// conveyor belts, rivers, etc. A linedef with a special type affects all
// tagged sectors the same way, by creating scrolling and/or object-carrying
// properties. Multiple linedefs may be used on the same sector and are
// cumulative, although the special case of scrolling a floor and carrying
// things on it, requires only one linedef. The linedef's direction determines
// the scrolling direction, and the linedef's length determines the scrolling
// speed. This was designed so that an edge around the sector could be used to
// control the direction of the sector's scrolling, which is usually what is
// desired.
//
// Process the active scrollers.
//
// This is the main scrolling code
// killough 3/7/98
//
void ScrollThinker::Think()
{
   fixed_t dx = this->dx, dy = this->dy;
   
   if(this->control != -1)
   {   // compute scroll amounts based on a sector's height changes
      fixed_t height = sectors[this->control].floorheight +
         sectors[this->control].ceilingheight;
      fixed_t delta = height - this->last_height;
      this->last_height = height;
      dx = FixedMul(dx, delta);
      dy = FixedMul(dy, delta);
   }

   // killough 3/14/98: Add acceleration
   if(this->accel)
   {
      this->vdx = dx += this->vdx;
      this->vdy = dy += this->vdy;
   }

   if(!(dx | dy))                   // no-op if both (x,y) offsets 0
      return;

   switch(this->type)
   {
      side_t *side;
      sector_t *sec;
      fixed_t height, waterheight;  // killough 4/4/98: add waterheight
      msecnode_t *node;
      Mobj *thing;

   case ScrollThinker::sc_side:          // killough 3/7/98: Scroll wall texture
      side = sides + this->affectee;
      side->textureoffset += dx;
      side->rowoffset += dy;
      break;

   case ScrollThinker::sc_floor:         // killough 3/7/98: Scroll floor texture
      sec = sectors + this->affectee;
      sec->floor_xoffs += dx;
      sec->floor_yoffs += dy;
      break;

   case ScrollThinker::sc_ceiling:       // killough 3/7/98: Scroll ceiling texture
      sec = sectors + this->affectee;
      sec->ceiling_xoffs += dx;
      sec->ceiling_yoffs += dy;
      break;

   case ScrollThinker::sc_carry:
      
      // killough 3/7/98: Carry things on floor
      // killough 3/20/98: use new sector list which reflects true members
      // killough 3/27/98: fix carrier bug
      // killough 4/4/98: Underwater, carry things even w/o gravity

      sec = sectors + this->affectee;
      height = sec->floorheight;
      waterheight = sec->heightsec != -1 &&
         sectors[sec->heightsec].floorheight > height ?
         sectors[sec->heightsec].floorheight : D_MININT;

      // Move objects only if on floor or underwater,
      // non-floating, and clipped.
      
      // haleyjd: added much-needed MF2_NOTHRUST flag to make some
      //   objects unmoveable by sector effects

      for(node = sec->touching_thinglist; node; node = node->m_snext)
      {
         if(!((thing = node->m_thing)->flags & MF_NOCLIP) &&
            !(thing->flags2 & MF2_NOTHRUST) &&
            (!(thing->flags & MF_NOGRAVITY || thing->z > height) ||
             thing->z < waterheight))
         {
            thing->momx += dx, thing->momy += dy;
         }
      }
      break;

   case ScrollThinker::sc_carry_ceiling:   // to be added later
      break;
   }
}

//
// ScrollThinker::serialize
//
// Save/load a ScrollThinker thinker.
//
void ScrollThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << dx << dy << affectee << control << last_height << vdx << vdy
       << accel << type;
}

//
// Add_Scroller
//
// Add a generalized scroller to the thinker list.
//
// type: the enumerated type of scrolling: floor, ceiling, floor carrier,
//   wall, floor carrier & scroller
//
// (dx,dy): the direction and speed of the scrolling or its acceleration
//
// control: the sector whose heights control this scroller's effect
//   remotely, or -1 if no control sector
//
// affectee: the index of the affected object (sector or sidedef)
//
// accel: non-zero if this is an accelerative effect
//
void Add_Scroller(int type, fixed_t dx, fixed_t dy,
                  int control, int affectee, int accel)
{
   ScrollThinker *s = new ScrollThinker;

   s->type = type;
   s->dx = dx;
   s->dy = dy;
   s->accel = accel;
   s->vdx = s->vdy = 0;

   if((s->control = control) != -1)
      s->last_height =
       sectors[control].floorheight + sectors[control].ceilingheight;

   s->affectee = affectee;
   s->addThinker();
}

// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff
//
// killough 10/98:
// fix scrolling aliasing problems, caused by long linedefs causing overflowing

static void Add_WallScroller(int64_t dx, int64_t dy, const line_t *l,
                             int control, int accel)
{
   fixed_t x = D_abs(l->dx), y = D_abs(l->dy), d;
   if(y > x)
      d = x, x = y, y = d;
   d = FixedDiv(x,
      finesine[(tantoangle[FixedDiv(y,x)>>DBITS]+ANG90) >> ANGLETOFINESHIFT]);

   x = (int)((dy * -l->dy - dx * l->dx) / d);  // killough 10/98:
   y = (int)((dy *  l->dx - dx * l->dy) / d);  // Use 64-bit arithmetic
   Add_Scroller(ScrollThinker::sc_side, x, y, control, *l->sidenum, accel);
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5

// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
#define CARRYFACTOR ((fixed_t)(FRACUNIT*.09375))

// killough 3/7/98: Types 245-249 are same as 250-254 except that the
// first side's sector's heights cause scrolling when they change, and
// this linedef controls the direction and speed of the scrolling. The
// most complicated linedef since donuts, but powerful :)
//
// killough 3/15/98: Add acceleration. Types 214-218 are the same but
// are accelerative.

//
// P_spawnCeilingScroller
//
// Spawn a ceiling scroller effect.
// Handles the following static init functions:
// * EV_STATIC_SCROLL_CEILING
// * EV_STATIC_SCROLL_DISPLACE_CEILING
// * EV_STATIC_SCROLL_ACCEL_CEILING
//
static void P_spawnCeilingScroller(int staticFn, line_t *l)
{
   fixed_t dx = l->dx >> SCROLL_SHIFT; // direction and speed of scrolling
   fixed_t dy = l->dy >> SCROLL_SHIFT;
   int control = -1;
   int accel   =  0;

   if(staticFn == EV_STATIC_SCROLL_ACCEL_CEILING)
      accel = 1;
   if(staticFn == EV_STATIC_SCROLL_ACCEL_CEILING ||
      staticFn == EV_STATIC_SCROLL_DISPLACE_CEILING)
      control = static_cast<int>(sides[*l->sidenum].sector - sectors);

   for(int s = -1; (s = P_FindSectorFromLineTag(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_ceiling, -dx, dy, control, s, accel);
}

//
// P_spawnFloorScroller
//
// Spawn a floor scroller effect.
// Handles the following static init functions:
// * EV_STATIC_SCROLL_FLOOR
// * EV_STATIC_SCROLL_DISPLACE_FLOOR
// * EV_STATIC_SCROLL_ACCEL_FLOOR
//
static void P_spawnFloorScroller(int staticFn, line_t *l)
{
   fixed_t dx = l->dx >> SCROLL_SHIFT;
   fixed_t dy = l->dy >> SCROLL_SHIFT;
   int control = -1;
   int accel   =  0;

   if(staticFn == EV_STATIC_SCROLL_ACCEL_FLOOR)
      accel = 1;
   if(staticFn == EV_STATIC_SCROLL_ACCEL_FLOOR ||
      staticFn == EV_STATIC_SCROLL_DISPLACE_FLOOR)
      control = static_cast<int>(sides[*l->sidenum].sector - sectors);

   for(int s = -1; (s = P_FindSectorFromLineTag(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_floor, -dx, dy, control, s, accel);
}

//
// P_spawnFloorCarrier
//
// Spawn a floor object carrying effect.
// Handles the following static init functions:
// * EV_STATIC_CARRY_FLOOR
// * EV_STATIC_CARRY_DISPLACE_FLOOR
// * EV_STATIC_CARRY_ACCEL_FLOOR
//
static void P_spawnFloorCarrier(int staticFn, line_t *l)
{
   fixed_t dx = FixedMul((l->dx >> SCROLL_SHIFT), CARRYFACTOR);
   fixed_t dy = FixedMul((l->dy >> SCROLL_SHIFT), CARRYFACTOR);
   int control = -1;
   int accel   =  0;

   if(staticFn == EV_STATIC_CARRY_ACCEL_FLOOR)
      accel = 1;
   if(staticFn == EV_STATIC_CARRY_ACCEL_FLOOR ||
      staticFn == EV_STATIC_CARRY_DISPLACE_FLOOR)
      control = static_cast<int>(sides[*l->sidenum].sector - sectors);

   for(int s = -1; (s = P_FindSectorFromLineTag(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_carry, dx, dy, control, s, accel);
}

//
// P_spawnFloorScrollAndCarry
//
// Spawn scroller and carrying effects which are rate-matched.
// Handles the following static init functions:
// * EV_STATIC_SCROLL_CARRY_FLOOR
// * EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR
// * EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR
//
static void P_spawnFloorScrollAndCarry(int staticFn, line_t *l)
{
   fixed_t dx = l->dx >> SCROLL_SHIFT;
   fixed_t dy = l->dy >> SCROLL_SHIFT;
   int control = -1;
   int accel   =  0;
   int s;

   if(staticFn == EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR)
      accel = 1;
   if(staticFn == EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR ||
      staticFn == EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR)
      control = static_cast<int>(sides[*l->sidenum].sector - sectors);

   for(s = -1; (s = P_FindSectorFromLineTag(l, s)) >= 0; )
      Add_Scroller(ScrollThinker::sc_floor, -dx, dy, control, s, accel);

   dx = FixedMul(dx, CARRYFACTOR);
   dy = FixedMul(dy, CARRYFACTOR);

   // NB: don't fold these loops together. Even though it would be more
   // efficient, we must maintain BOOM-compatible thinker spawning order.
   for(s = -1; (s = P_FindSectorFromLineTag(l, s)) >= 0; )
      Add_Scroller(ScrollThinker::sc_carry, dx, dy, control, s, accel);
}

//
// P_spawnDynamicWallScroller
//
// Spawn a wall scrolling effect that can be synchronized with the movement
// of a sector.
// Handles the following static init functions:
// * EV_STATIC_SCROLL_WALL_WITH
// * EV_STATIC_SCROLL_DISPLACE_WALL
// * EV_STATIC_SCROLL_ACCEL_WALL
//
static void P_spawnDynamicWallScroller(int staticFn, line_t *l, int linenum)
{
   fixed_t dx = l->dx >> SCROLL_SHIFT;  // direction and speed of scrolling
   fixed_t dy = l->dy >> SCROLL_SHIFT;
   int control = -1;
   int accel   =  0;

   if(staticFn == EV_STATIC_SCROLL_ACCEL_WALL)
      accel = 1;
   if(staticFn == EV_STATIC_SCROLL_ACCEL_WALL ||
      staticFn == EV_STATIC_SCROLL_DISPLACE_WALL)
      control = static_cast<int>(sides[*l->sidenum].sector - sectors);

   // killough 3/1/98: scroll wall according to linedef
   // (same direction and speed as scrolling floors)
   for(int s = -1; (s = P_FindLineFromLineTag(l, s)) >= 0;)
   {
      if(s != linenum)
         Add_WallScroller(dx, dy, lines + s, control, accel);
   }
}

//
// P_spawnStaticWallScroller
//
// Spawn a constant wall scrolling effect.
// Handles the following static init functions:
// * EV_STATIC_SCROLL_LINE_LEFT
// * EV_STATIC_SCROLL_LINE_RIGHT
// * EV_STATIC_SCROLL_BY_OFFSETS
// * EV_STATIC_SCROLL_LEFT_PARAM
// * EV_STATIC_SCROLL_RIGHT_PARAM
// * EV_STATIC_SCROLL_UP_PARAM
// * EV_STATIC_SCROLL_DOWN_PARAM
//
static void P_spawnStaticWallScroller(line_t *l, fixed_t dx, fixed_t dy)
{
   Add_Scroller(ScrollThinker::sc_side, dx, dy, -1, l->sidenum[0], 0);
}

//
// P_SpawnScrollers
//
// Initialize the scrollers
//
void P_SpawnScrollers()
{
   line_t *line = lines;

   for(int i = 0; i < numlines; i++, line++)
   {
      int staticFn;
      int special = line->special;

      // haleyjd 02/03/13: look up special in static init table
      if(!(staticFn = EV_StaticInitForSpecial(special)))
         continue;

      switch(staticFn)
      {
      case EV_STATIC_SCROLL_ACCEL_CEILING:
      case EV_STATIC_SCROLL_DISPLACE_CEILING:
      case EV_STATIC_SCROLL_CEILING:     // scroll effect ceiling
         P_spawnCeilingScroller(staticFn, line);
         break;
   
      case EV_STATIC_SCROLL_ACCEL_FLOOR:
      case EV_STATIC_SCROLL_DISPLACE_FLOOR:
      case EV_STATIC_SCROLL_FLOOR:       // scroll effect floor
         P_spawnFloorScroller(staticFn, line);
         break;
   
      case EV_STATIC_CARRY_ACCEL_FLOOR:
      case EV_STATIC_CARRY_DISPLACE_FLOOR:
      case EV_STATIC_CARRY_FLOOR:        // carry objects on floor
         P_spawnFloorCarrier(staticFn, line);
         break;
   
      case EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR:
      case EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR:
      case EV_STATIC_SCROLL_CARRY_FLOOR: // scroll and carry objects on floor
         P_spawnFloorScrollAndCarry(staticFn, line);
         break;

      case EV_STATIC_SCROLL_ACCEL_WALL:
      case EV_STATIC_SCROLL_DISPLACE_WALL:
      case EV_STATIC_SCROLL_WALL_WITH:   
         // killough 3/1/98: scroll wall according to linedef
         // (same direction and speed as scrolling floors)
         P_spawnDynamicWallScroller(staticFn, line, i);
         break;
   
      case EV_STATIC_SCROLL_LINE_LEFT:
         P_spawnStaticWallScroller(line, FRACUNIT, 0);  // scroll first side
         break;
      case EV_STATIC_SCROLL_LINE_RIGHT:
         P_spawnStaticWallScroller(line, -FRACUNIT, 0); // jff 1/30/98 2-way scroll
         break;
      case EV_STATIC_SCROLL_BY_OFFSETS:
         // killough 3/2/98: scroll according to sidedef offsets
         P_spawnStaticWallScroller(line, 
            -sides[*line->sidenum].textureoffset,
             sides[*line->sidenum].rowoffset);
         break;

      case EV_STATIC_SCROLL_LEFT_PARAM:
         // haleyjd 02/17/13: Hexen Scroll_Texture_Left
         P_spawnStaticWallScroller(line, line->args[0] << 10, 0);
         break;

      case EV_STATIC_SCROLL_RIGHT_PARAM:
         // haleyjd 02/17/13: Hexen Scroll_Texture_Right
         P_spawnStaticWallScroller(line, -(line->args[0] << 10), 0);
         break;

      case EV_STATIC_SCROLL_UP_PARAM:
         // haleyjd 02/17/13: Hexen Scroll_Texture_Up
         P_spawnStaticWallScroller(line, 0, line->args[0] << 10);
         break;

      case EV_STATIC_SCROLL_DOWN_PARAM:
         // haleyjd 02/17/13: Hexen Scroll_Texture_Down
         P_spawnStaticWallScroller(line, 0, -(line->args[0] << 10));
         break;

      default:
         break; // not a function handled here
      }
   }
}

// killough 3/7/98 -- end generalized scroll effects

// EOF

