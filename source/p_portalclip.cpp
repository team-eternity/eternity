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
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

// The First Commandment of EE: Thou shalt include z_zone.h. Thus sayeth the LORD.
#include "z_zone.h"

#include "doomstat.h"
#include "m_bbox.h"
#include "m_collection.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_portalclip.h"
#include "p_portal.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_traceengine.h"
#include "polyobj.h"
#include "r_defs.h"
#include "r_state.h"



// --------------------------------------------------------------------------------------
// PortalClip
static int groupcells;


PortalClipEngine::~PortalClipEngine()
{
   ClipContext *rover, *next;

   rover = unused;
   while(rover)
   {
      next = rover->next;
      free(rover);
      rover = next;
   }
}


bool PortalClipEngine::changeSector(sector_t *sector, int crunch, ClipContext *cc)
{
   return false;
}

bool PortalClipEngine::checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil, ClipContext *cc)
{
   return false;
}

bool PortalClipEngine::checkSides(Mobj *actor, int x, int y, ClipContext *cc)
{
   return false;
}

//
// getFriction()
//
// Returns the friction associated with a particular mobj.
//
int PortalClipEngine::getFriction(const Mobj *mo, int *frictionfactor)
{
   int friction = ORIG_FRICTION;
   int movefactor = ORIG_FRICTION_FACTOR;
   const msecnode_t *m;
   const sector_t *sec;

   // Assign the friction value to objects on the floor, non-floating,
   // and clipped. Normally the object's friction value is kept at
   // ORIG_FRICTION and this thinker changes it for icy or muddy floors.
   //
   // When the object is straddling sectors with the same
   // floorheight that have different frictions, use the lowest
   // friction value (muddy has precedence over icy).

   if(!(mo->flags & (MF_NOCLIP|MF_NOGRAVITY)) 
      && mo->player && variable_friction)
   {
      for (m = mo->touching_sectorlist; m; m = m->m_tnext)
      {
         if((sec = m->m_sector)->flags & SECF_FRICTION &&
            (sec->friction < friction || friction == ORIG_FRICTION) &&
            (mo->z <= sec->floorheight ||
             (sec->heightsec != -1 &&
              mo->z <= sectors[sec->heightsec].floorheight)))
         {
            friction = sec->friction, movefactor = sec->movefactor;
         }
      }
   }
   
   if(frictionfactor)
      *frictionfactor = movefactor;
   
   return friction;
}

// P_GetMoveFactor() returns the value by which the x,y
// movements are multiplied to add to player movement.
//
// killough 8/28/98: rewritten
//
int PortalClipEngine::getMoveFactor(Mobj *mo, int *frictionp)
{
   int movefactor, friction;

   // If the floor is icy or muddy, it's harder to get moving. This is where
   // the different friction factors are applied to 'trying to move'. In
   // p_mobj.c, the friction factors are applied as you coast and slow down.

   if((friction = getFriction(mo, &movefactor)) < ORIG_FRICTION)
   {
      // phares 3/11/98: you start off slowly, then increase as
      // you get better footing
      
      int momentum = P_AproxDistance(mo->momx,mo->momy);
      
      if(momentum > MORE_FRICTION_MOMENTUM<<2)
         movefactor <<= 3;
      else if(momentum > MORE_FRICTION_MOMENTUM<<1)
         movefactor <<= 2;
      else if(momentum > MORE_FRICTION_MOMENTUM)
         movefactor <<= 1;
   }

   if(frictionp)
      *frictionp = friction;
   
   return movefactor;
}

void PortalClipEngine::applyTorque(Mobj *mo, ClipContext *cc)
{
}

void PortalClipEngine::radiusAttack(Mobj *spot, Mobj *source, int damage, int distance, int mod, unsigned int flags, ClipContext *cc)
{
}

fixed_t PortalClipEngine::avoidDropoff(Mobj *actor, ClipContext *cc)
{
   return 0;
}
      
// Utility functions
ClipContext*  PortalClipEngine::getContext()
{
   ClipContext *ret;

   if(unused == NULL) {
      ret = new ClipContext();
      ret->setEngine(this);
   }
   else {  
      ret = unused;
      unused = unused->next;
      ret->next = NULL;
   }

   return ret;
}


void PortalClipEngine::releaseContext(ClipContext *cc)
{
   cc->next = unused;
   unused = cc;
}




bool PortalClipEngine::blockLinesIterator(int x, int y, bool func(line_t *line, MapContext *mc), MapContext *mc)
{
   int        offset;
   const int  *list;     // killough 3/1/98: for removal of blockmap limit
   DLListItem<polymaplink_t> *plink; // haleyjd 02/22/06
   
   if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
      return true;
   offset = y * bmapwidth + x;

   MarkVector *markedLines = mc->getMarkedLines();

   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      polyobj_t *po = (*plink)->po;
      int i;

      for(i = 0; i < po->numLines; ++i)
      {
         if(markedLines->isMarked(po->lines[i] - lines)) // line has been checked
            continue;
         markedLines->mark(po->lines[i] - lines);
         if(!func(po->lines[i], mc))
            return false;
      }

      plink = plink->dllNext;
   }

   // original was reading delimiting 0 as linedef 0 -- phares
   offset = *(blockmap + offset);
   list = blockmaplump + offset;

   list++;
   for( ; *list != -1; list++)
   {
      line_t *ld;
      
      // haleyjd 04/06/10: to avoid some crashes during demo playback due to
      // invalid blockmap lumps
      if(*list >= numlines)
         continue;

      ld = &lines[*list];
      if(markedLines->isMarked(*list))
         continue;
      markedLines->mark(*list);

      if(!func(ld, mc))
         return false;
   }
   return true;  // everything was checked
}



// --------------------------------------------------------------------------------------
// MarkVector

MarkVector::MarkVector(size_t numItems) : ZoneObject()
{
   arraySize = (numItems + 31) >> 5;
   markArray = ecalloc(int *, arraySize, sizeof(int));

   firstDirty = arraySize;
   lastDirty = 0;
}


MarkVector::~MarkVector() 
{
   efree(markArray);
   arraySize = 0;
}


void MarkVector::clearMarks()
{
   if(lastDirty >= firstDirty)
   {
      memset(markArray, 0, sizeof(int) * (lastDirty - firstDirty + 1));
      firstDirty = arraySize;
      lastDirty = 0;
   }
}


bool MarkVector::mark(size_t itemIndex)
{
   size_t index = itemIndex >> 5;
   if(index > lastDirty) 
      lastDirty = index;
   if(index < firstDirty)
      firstDirty = index;

   bool result = isMarked(itemIndex);
   markArray[index] |= 1 << (itemIndex & 31);

   return result;
}