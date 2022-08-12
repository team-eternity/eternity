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

#include "c_io.h"
#include "doomstat.h"
#include "ev_specials.h"
#include "p_mobj.h"
#include "p_portal.h"   // ioanch 20160115: portal aware
#include "p_portalcross.h"
#include "p_saveg.h"
#include "p_scroll.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"
#include "v_misc.h"


struct scrollerlist_t
{
   ScrollThinker    *scroller;
   scrollerlist_t   *next;
   scrollerlist_t  **prev;
};

//
// Data for interpolating scrolled sidedefs
//
struct sidelerpinfo_t
{
   side_t *side;     // the affected sidedef
   v2fixed_t offset; // how much this is scrolling
};

//
// Data for interpolating sector surfaces
//
struct seclerpinfo_t
{
   sector_t *sector;
   bool isceiling;
   v2fixed_t offset;
};

static scrollerlist_t *scrollers;

static PODCollection<sidelerpinfo_t> pScrolledSides;
static PODCollection<seclerpinfo_t> pScrolledSectors;

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
      fixed_t height = sectors[this->control].srf.floor.height +
         sectors[this->control].srf.ceiling.height;
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

   side_t *side;
   sector_t *sec;
   fixed_t height, waterheight;  // killough 4/4/98: add waterheight
   msecnode_t *node;
   Mobj *thing;

   switch(this->type)
   {
   case ScrollThinker::sc_side:          // killough 3/7/98: Scroll wall texture
      side = sides + this->affectee;
      side->textureoffset += dx;
      side->rowoffset += dy;
      P_AddScrolledSide(side, dx, dy);
      break;

   case ScrollThinker::sc_floor:         // killough 3/7/98: Scroll floor texture
      sec = sectors + this->affectee;
      sec->srf.floor.offset.x += dx;
      sec->srf.floor.offset.y += dy;
      {
         seclerpinfo_t &info = pScrolledSectors.addNew();
         info.sector = sec;
         info.isceiling = false;
         info.offset.x = dx;
         info.offset.y = dy;
      }
      break;

   case ScrollThinker::sc_ceiling:       // killough 3/7/98: Scroll ceiling texture
      sec = sectors + this->affectee;
      sec->srf.ceiling.offset.x += dx;
      sec->srf.ceiling.offset.y += dy;
      {
         seclerpinfo_t &info = pScrolledSectors.addNew();
         info.sector = sec;
         info.isceiling = true;
         info.offset.x = dx;
         info.offset.y = dy;
      }
      break;

   case ScrollThinker::sc_carry:
      
      // killough 3/7/98: Carry things on floor
      // killough 3/20/98: use new sector list which reflects true members
      // killough 3/27/98: fix carrier bug
      // killough 4/4/98: Underwater, carry things even w/o gravity

      sec = sectors + this->affectee;
      height = sec->srf.floor.height;
      waterheight = sec->heightsec != -1 &&
         sectors[sec->heightsec].srf.floor.height > height ?
         sectors[sec->heightsec].srf.floor.height : D_MININT;

      // Move objects only if on floor or underwater,
      // non-floating, and clipped.
      
      // haleyjd: added much-needed MF2_NOTHRUST flag to make some
      //   objects unmoveable by sector effects

      for(node = sec->touching_thinglist; node; node = node->m_snext)
      {
         // ioanch 20160115: portal aware
         if(useportalgroups && full_demo_version >= make_full_version(340, 48) &&
            !P_SectorTouchesThingVertically(sec, node->m_thing))
         {
            continue;
         }
         if(!((thing = node->m_thing)->flags & MF_NOCLIP) &&
            !(thing->flags2 & MF2_NOTHRUST) &&
            (!(thing->flags & MF_NOGRAVITY || thing->z > height) ||
             thing->z < waterheight))
         {
            thing->momx += dx;
            thing->momy += dy;
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

   if(arc.isLoading())
      addScroller();
}

//
// Adds a scroll thinker to the list
//
void ScrollThinker::addScroller()
{
   list = estructalloc(scrollerlist_t, 1);
   list->scroller = this;
   if((list->next = scrollers))
      list->next->prev = &list->next;
   list->prev = &scrollers;
   scrollers = list;
}

//
// Removes a scroll thinker to the list
//
void ScrollThinker::removeScroller()
{
   remove();
   if((*list->prev = list->next))
      list->next->prev = list->prev;
   efree(list);
}

//
// Remove all scrollers
//
// Passed nothing, returns nothing
//
void ScrollThinker::RemoveAllScrollers()
{
   while(scrollers)
   {
      scrollerlist_t *next = scrollers->next;
      efree(scrollers);
      scrollers = next;
   }
}

//
// Stop a scroller based on sector number
//
static bool EV_stopFlatScrollerBySecnum(int type, int secnum)
{
   if(!scrollers)
      return false;

   // search the scrolled sectors
   scrollerlist_t *sl = scrollers;
   while(sl)
   {
      ScrollThinker *scroller = sl->scroller;
      sl = sl->next; // MUST do this here or bad things happen
      if(scroller->affectee == secnum && scroller->type == type)
         scroller->removeScroller();
   }

   return true;
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
                  int control, int affectee, int accel, bool overridescroller)
{
   ScrollThinker *s = new ScrollThinker;

   s->type = type;
   s->dx = dx;
   s->dy = dy;
   s->accel = accel;
   s->vdx = s->vdy = 0;

   if((s->control = control) != -1)
      s->last_height =
       sectors[control].srf.floor.height + sectors[control].srf.ceiling.height;

   s->affectee = affectee;

   if(type != ScrollThinker::sc_side)
   {
      if(overridescroller)
         EV_stopFlatScrollerBySecnum(type, affectee);
      s->addScroller();
   }
   else if(dy) // mark it as vertically scrolling for proper sky rendering
      sides[affectee].intflags |= SDI_VERTICALLYSCROLLING;

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
   {
      d = x;
      x = y;
      y = d;
   }
   d = FixedDiv(x,
      finesine[(tantoangle[FixedDiv(y,x)>>DBITS]+ANG90) >> ANGLETOFINESHIFT]);

   x = (int)((dy * -l->dy - dx * l->dx) / d);  // killough 10/98:
   y = (int)((dy *  l->dx - dx * l->dy) / d);  // Use 64-bit arithmetic
   Add_Scroller(ScrollThinker::sc_side, x, y, control, *l->sidenum, accel);
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
static constexpr int SCROLL_SHIFT = 5;

// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
static constexpr fixed_t CARRYFACTOR = fixed_t(FRACUNIT * .09375);

// This makes it so certain values are approx 1 unit per second
static constexpr int ZDSCROLLFACTOR = 10;

//
// Helper to get accel and control values from bits
//
static void P_getAccelControl(const line_t &line, int bits, int &accel, int &control)
{
   accel = 0;
   control = -1;
   if(bits & ev_Scroll_Bit_Accel)
      accel = 1;
   if(bits & (ev_Scroll_Bit_Accel | ev_Scroll_Bit_Displace))
      control = eindex(sides[*line.sidenum].sector - sectors);
}

// killough 3/7/98: Types 245-249 are same as 250-254 except that the
// first side's sector's heights cause scrolling when they change, and
// this linedef controls the direction and speed of the scrolling. The
// most complicated linedef since donuts, but powerful :)
//
// killough 3/15/98: Add acceleration. Types 214-218 are the same but
// are accelerative.

static void P_getScrollParams(const line_t *l, fixed_t &dx, fixed_t &dy,
                              int &control, int &accel, bool acs = false)
{
   int bits = l->args[ev_Scroll_Arg_Bits];
   if(bits & ev_Scroll_Bit_UseLine)
   {
      dx = l->dx >> SCROLL_SHIFT; // direction and speed of scrolling
      dy = l->dy >> SCROLL_SHIFT;
   }
   else if(!acs)
   {
      dx = ((l->args[ev_Scroll_Arg_X] - 128) << FRACBITS) >> SCROLL_SHIFT;
      dy = ((l->args[ev_Scroll_Arg_Y] - 128) << FRACBITS) >> SCROLL_SHIFT;
   }
   else
   {
      // For some reason ACS Scroll_Floor and Scroll_Ceiling have different
      // meaning for dx/dy if they're called from ACS. This is a ZDoom thing.
      dx = ((l->args[ev_Scroll_Arg_X] * ZDSCROLLFACTOR) << FRACBITS) >> SCROLL_SHIFT;
      dy = ((l->args[ev_Scroll_Arg_Y] * ZDSCROLLFACTOR) << FRACBITS) >> SCROLL_SHIFT;
   }
   P_getAccelControl(*l, bits, accel, control);
}

//
// P_spawnCeilingScroller
//
// Spawn a ceiling scroller effect.
// Handles the following static init functions:
// * EV_STATIC_SCROLL_CEILING
// * EV_STATIC_SCROLL_DISPLACE_CEILING
// * EV_STATIC_SCROLL_ACCEL_CEILING
//
static void P_spawnCeilingScroller(int staticFn, const line_t *l)
{
   fixed_t dx, dy;
   int control = -1;
   int accel = 0;

   dx = l->dx >> SCROLL_SHIFT; // direction and speed of scrolling
   dy = l->dy >> SCROLL_SHIFT;

   if(staticFn == EV_STATIC_SCROLL_ACCEL_CEILING)
      accel = 1;
   if(staticFn == EV_STATIC_SCROLL_ACCEL_CEILING ||
      staticFn == EV_STATIC_SCROLL_DISPLACE_CEILING)
      control = eindex(sides[*l->sidenum].sector - sectors);

   for(int s = -1; (s = P_FindSectorFromLineArg0(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_ceiling, -dx, dy, control, s, accel);
}

// Spawns a parameterised ceiling scroller, either by ACS or by static init.
// Handles the following static init function:
// * EV_STATIC_SCROLL_CEILING_PARAM
void P_SpawnCeilingParam(const line_t *l, bool acs)
{
   fixed_t dx, dy;
   int control = -1;
   int accel = 0;

   P_getScrollParams(l, dx, dy, control, accel, acs);

   for(int s = -1; (s = P_FindSectorFromLineArg0(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_ceiling, -dx, dy, control, s, accel, acs);
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
static void P_spawnFloorScroller(int staticFn, const line_t *l, bool acs = false)
{
   fixed_t dx, dy;
   int control = -1;
   int accel = 0;

   if(staticFn == EV_STATIC_SCROLL_FLOOR_PARAM)
      P_getScrollParams(l, dx, dy, control, accel, acs);
   else
   {
      dx = l->dx >> SCROLL_SHIFT;
      dy = l->dy >> SCROLL_SHIFT;

      if(staticFn == EV_STATIC_SCROLL_ACCEL_FLOOR)
         accel = 1;
      if(staticFn == EV_STATIC_SCROLL_ACCEL_FLOOR ||
         staticFn == EV_STATIC_SCROLL_DISPLACE_FLOOR)
         control = eindex(sides[*l->sidenum].sector - sectors);
   }

   for(int s = -1; (s = P_FindSectorFromLineArg0(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_floor, -dx, dy, control, s, accel, acs);
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
static void P_spawnFloorCarrier(int staticFn, const line_t *l, bool acs = false)
{
   fixed_t dx, dy;
   int control = -1;
   int accel   =  0;

   if(staticFn == EV_STATIC_SCROLL_FLOOR_PARAM)
   {
      P_getScrollParams(l, dx, dy, control, accel, acs);
      dx = FixedMul(dx, CARRYFACTOR);
      dy = FixedMul(dy, CARRYFACTOR);
   }
   else
   {
      dx = FixedMul((l->dx >> SCROLL_SHIFT), CARRYFACTOR);
      dy = FixedMul((l->dy >> SCROLL_SHIFT), CARRYFACTOR);

      if(staticFn == EV_STATIC_CARRY_ACCEL_FLOOR)
         accel = 1;
      if(staticFn == EV_STATIC_CARRY_ACCEL_FLOOR ||
         staticFn == EV_STATIC_CARRY_DISPLACE_FLOOR)
         control = eindex(sides[*l->sidenum].sector - sectors);
   }

   for(int s = -1; (s = P_FindSectorFromLineArg0(l, s)) >= 0;)
      Add_Scroller(ScrollThinker::sc_carry, dx, dy, control, s, accel, acs);
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
static void P_spawnFloorScrollAndCarry(int staticFn, const line_t *l, bool acs = false)
{
   fixed_t dx, dy;
   int control = -1;
   int accel = 0;
   int s;

   if(staticFn == EV_STATIC_SCROLL_FLOOR_PARAM)
      P_getScrollParams(l, dx, dy, control, accel, acs);
   else
   {
      dx = l->dx >> SCROLL_SHIFT;
      dy = l->dy >> SCROLL_SHIFT;

      if(staticFn == EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR)
         accel = 1;
      if(staticFn == EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR ||
         staticFn == EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR)
         control = eindex(sides[*l->sidenum].sector - sectors);
   }

   for(s = -1; (s = P_FindSectorFromLineArg0(l, s)) >= 0; )
      Add_Scroller(ScrollThinker::sc_floor, -dx, dy, control, s, accel, acs);

   dx = FixedMul(dx, CARRYFACTOR);
   dy = FixedMul(dy, CARRYFACTOR);

   // NB: don't fold these loops together. Even though it would be more
   // efficient, we must maintain BOOM-compatible thinker spawning order.
   for(s = -1; (s = P_FindSectorFromLineArg0(l, s)) >= 0; )
      Add_Scroller(ScrollThinker::sc_carry, dx, dy, control, s, accel, acs);
}

void P_SpawnFloorParam(const line_t *l, bool acs)
{
   switch(l->args[ev_Scroll_Arg_Type])
   {
      case ev_Scroll_Type_Scroll:
         P_spawnFloorScroller(EV_STATIC_SCROLL_FLOOR_PARAM, l, acs);
         break;
      case ev_Scroll_Type_Carry:
         P_spawnFloorCarrier(EV_STATIC_SCROLL_FLOOR_PARAM, l, acs);
         break;
      case ev_Scroll_Type_ScrollCarry:
         P_spawnFloorScrollAndCarry(EV_STATIC_SCROLL_FLOOR_PARAM, l, acs);
         break;
      default:
         // wrong arg values will just do nothing, but it's undefined anyway
         C_Printf(FC_ERROR "Unknown scroll type %d at line %d\a\n",
                  l->args[ev_Scroll_Arg_Type], (int)(l - lines));
   }
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
// MBF21:
// * EV_STATIC_SCROLL_BY_OFFSETS_TAG
// * EV_STATIC_SCROLL_BY_OFFSETS_TAG_DISPLACE
// * EV_STATIC_SCROLL_BY_OFFSETS_TAG_ACCEL
//
static void P_spawnDynamicWallScroller(int staticFn, line_t *l, int linenum)
{
   fixed_t dx;  // direction and speed of scrolling
   fixed_t dy;
   bool byoffset;
   int control = -1;
   int accel   =  0;

   switch(staticFn)
   {
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG_DISPLACE:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG_ACCEL:
      {
         const side_t &side = sides[l->sidenum[0]];
         dx = -side.textureoffset / 8;
         dy = side.rowoffset / 8;
         byoffset = true;
         break;
      }
      default:
         dx = l->dx >> SCROLL_SHIFT;
         dy = l->dy >> SCROLL_SHIFT;
         byoffset = false;
         break;
   }

   if(staticFn == EV_STATIC_SCROLL_WALL_PARAM)
   {
      int bits = l->args[ev_Scroll_Arg_Bits];
      P_getAccelControl(*l, bits, accel, control);
   }
   else switch(staticFn)
   {
      case EV_STATIC_SCROLL_ACCEL_WALL:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG_ACCEL:
         accel = 1;
         // go on
      case EV_STATIC_SCROLL_DISPLACE_WALL:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG_DISPLACE:
         control = eindex(sides[*l->sidenum].sector - sectors);
   }

   // killough 3/1/98: scroll wall according to linedef
   // (same direction and speed as scrolling floors)
   for(int s = -1; (s = P_FindLineFromLineArg0(l, s)) >= 0;)
   {
      if(s != linenum)
      {
         if(byoffset)
            Add_Scroller(ScrollThinker::sc_side, dx, dy, control, lines[s].sidenum[0], accel);
         else
            Add_WallScroller(dx, dy, lines + s, control, accel);
      }
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
// Handle Scroll_Texture_Offsets special (parameterized)
//
static void P_handleScrollByOffsetsParam(line_t &line)
{
   const side_t &side = sides[line.sidenum[0]];
   v2fixed_t delta = { -side.textureoffset, side.rowoffset };

   // Check args now
   qstring error;
   int flags = line.args[0];
   if(flags < 0 || (flags >= 1 && flags <= 6))
      error.Printf(64, "Scroll_Texture_Offsets: unsupported edge flags %d", flags);
   int tag = line.args[1];
   int accel, control;
   int bits = line.args[2];
   if(bits & ~3)
      error.Printf(64, "Scroll_Texture_Offsets: invalid mode bits %d", bits);
   P_getAccelControl(line, bits, accel, control);
   int divider = line.args[3];
   if(divider <= 0)
      divider = 1;

   if(!error.empty())
   {
      doom_warningf("%s (line=%d)", error.constPtr(), eindex(&line - lines));
      return;
   }

   delta /= divider;

   if(tag)
   {
      int triggernum = eindex(&line - lines);
      for(int linenum = -1; (linenum = P_FindLineFromTag(tag, linenum)) >= 0;)
         if(linenum != triggernum)
         {
            Add_Scroller(ScrollThinker::sc_side, delta.x, delta.y, control,
                         lines[linenum].sidenum[0], accel);
         }
      return;
   }

   // killough 3/2/98: scroll according to sidedef offsets
   Add_Scroller(ScrollThinker::sc_side, delta.x, delta.y, control, line.sidenum[0], accel);
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
      case EV_STATIC_SCROLL_CEILING_PARAM:
         P_SpawnCeilingParam(line, false);
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
      case EV_STATIC_SCROLL_FLOOR_PARAM:
         P_SpawnFloorParam(line, false);
         break;

      case EV_STATIC_SCROLL_ACCEL_WALL:
      case EV_STATIC_SCROLL_DISPLACE_WALL:
      case EV_STATIC_SCROLL_WALL_WITH:
      case EV_STATIC_SCROLL_WALL_PARAM:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG_DISPLACE:
      case EV_STATIC_SCROLL_BY_OFFSETS_TAG_ACCEL:
         // killough 3/1/98: scroll wall according to linedef
         // (same direction and speed as scrolling floors)
         P_spawnDynamicWallScroller(staticFn, line, i);
         break;
   
      case EV_STATIC_SCROLL_LINE_LEFT:
         P_spawnStaticWallScroller(line, FRACUNIT, 0);    // scroll first side
         break;
      case EV_STATIC_SCROLL_LINE_RIGHT:
         P_spawnStaticWallScroller(line, -FRACUNIT, 0);   // jff 1/30/98 2-way scroll
         break;
      case EV_STATIC_SCROLL_LINE_UP:
         P_spawnStaticWallScroller(line, 0, FRACUNIT);    // haleyjd: Strife and PSX type
         break;
      case EV_STATIC_SCROLL_LINE_DOWN:
         P_spawnStaticWallScroller(line, 0, -FRACUNIT);   // haleyjd: Strife and PSX type
         break;
      case EV_STATIC_SCROLL_LINE_DOWN_FAST:
         P_spawnStaticWallScroller(line, 0, -3*FRACUNIT); // haleyjd: Strife type
         break;
      case EV_STATIC_SCROLL_BY_OFFSETS:
         // killough 3/2/98: scroll according to sidedef offsets
         P_spawnStaticWallScroller(line, 
            -sides[*line->sidenum].textureoffset,
             sides[*line->sidenum].rowoffset);
         break;
      case EV_STATIC_SCROLL_BY_OFFSETS_PARAM:
         P_handleScrollByOffsetsParam(*line);
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

//
// Spawn a floor scroller based on UDMF properties
//
void P_SpawnFloorUDMF(int s, int type, double scrollx, double scrolly)
{
   bool texture = false, carry = false;
   
   switch(type)
   {
   case SCROLLTYPE_TEXTURE:
      texture = true;
      break;
   case SCROLLTYPE_CARRY:
      carry = true;
      break;
   case SCROLLTYPE_BOTH:
      texture = carry = true;
      break;
   default:
      return;
   }

   fixed_t dx = M_DoubleToFixed(scrollx * ZDSCROLLFACTOR) >> SCROLL_SHIFT;
   fixed_t dy = M_DoubleToFixed(scrolly * ZDSCROLLFACTOR) >> SCROLL_SHIFT;
   if(texture)
      Add_Scroller(ScrollThinker::sc_floor, -dx, dy, -1, s, false, false);
   if(carry)
   {
      dx = FixedMul(dx, CARRYFACTOR);
      dy = FixedMul(dy, CARRYFACTOR);
      Add_Scroller(ScrollThinker::sc_carry, dx, dy, -1, s, false, false);
   }
}

//
// Spawn a ceiling scroller based on UDMF properties
//
void P_SpawnCeilingUDMF(int s, int type, double scrollx, double scrolly)
{
   bool texture, carry;

   switch(type)
   {
   case SCROLLTYPE_TEXTURE:
      texture = true;
      carry = false;
      break;
   case SCROLLTYPE_CARRY:
      carry = true;
      texture = false;
      break;
   case SCROLLTYPE_BOTH:
      texture = carry = true;
      break;
   default:
      return;
   }

   fixed_t dx = M_DoubleToFixed(scrollx * ZDSCROLLFACTOR) >> SCROLL_SHIFT;
   fixed_t dy = M_DoubleToFixed(scrolly * ZDSCROLLFACTOR) >> SCROLL_SHIFT;
   if(texture)
      Add_Scroller(ScrollThinker::sc_ceiling, -dx, dy, -1, s, false, false);
   if(carry)
   {
      C_Printf(FC_ERROR "Carrying ceiling scrollers will be added later\a\n");
      dx = FixedMul(dx, CARRYFACTOR);
      dy = FixedMul(dy, CARRYFACTOR);
      Add_Scroller(ScrollThinker::sc_carry_ceiling, dx, dy, -1, s, false, false);
   }
}

//
// Resets the scrolled sides list used for interpolation.
// Called at the start of each tic.
//
void P_TicResetLerpScrolledSides()
{
   pScrolledSides.makeEmpty();
   pScrolledSectors.makeEmpty();
}

//
// Add a scrolled side to the list
//
void P_AddScrolledSide(side_t *side, fixed_t dx, fixed_t dy)
{
   sidelerpinfo_t &info = pScrolledSides.addNew();
   info.side = side;
   info.offset.x = dx;
   info.offset.y = dy;
}

//
// Iterates the scroll info list
//
void P_ForEachScrolledSide(void (*func)(side_t *side, v2fixed_t offset))
{
   for(const sidelerpinfo_t &info : pScrolledSides)
      func(info.side, info.offset);
}

void P_ForEachScrolledSector(void (*func)(sector_t *sector, bool isceiling, v2fixed_t offset))
{
   for(const seclerpinfo_t &info : pScrolledSectors)
      func(info.sector, info.isceiling, info.offset);
}

// killough 3/7/98 -- end generalized scroll effects

// EOF

