// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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

// haleyjd: temporary define
#ifdef POLYOBJECTS

#include "z_malloc.h"

#include "g_game.h"
#include "m_queue.h"
#include "polyobj.h"

/*
   Theory behind Polyobjects:

      "The BSP tree hidden surface removal algorithm can easily be
      extended to allow for dynamic objects. For each frame, start with
      a BSP tree containing all the static objects in the scene, and
      reinsert the dynamic objects. While this is straightforward to
      implement, it can involve substantial computation.
       
      "If a dynamic object is separated from each static object by a
      plane, the dynamic object can be represented as a single point
      regardless of its complexity. This can dramatically reduce the
      computation per frame because only one node per dynamic object is
      inserted into the BSP tree. Compare that to one node for every
      polygon in the object, and the reason for the savings is obvious.
      During tree traversal, each point is expanded into the original
      object...

      "Inserting a point into the BSP tree is very cheap, because there
      is only one front/back test at each node. Points are never split,
      which explains the requirement of separation by a plane. The
      dynamic object will always be drawn completely in front of the
      static objects behind it.

      "...a different front/back test is necessary, because a point 
      doesn't partition three dimesnional (sic) space. The correct 
      front/back test is to simply compare distances to the eye. Once 
      computed, this distance can be cached at the node until the frame
      is drawn."

   From http://www.faqs.org/faqs/graphics/bsptree-faq/ (The BSP FAQ)

   While Hexen had polyobjects, it put severe and artificial limits upon
   them by keeping them attached to one subsector, and allowing only one
   per subsector. Neither is necessary, and removing those limitations
   results in the free-moving polyobjects implemented here. The only
   true - and unavoidable - restriction is that polyobjects should never
   overlap with each other or with static walls.

   The reason that multiple polyobjects per subsector is viable is that
   with the above assumption that the objects will not overlap, if the 
   center point of one polyobject is closer to the viewer than the center
   point of another, then the entire polyobject is closer to the viewer. 
   In this way it is possible to impose an order on polyobjects within a 
   subsector, as well as allowing the BSP tree to impose its natural 
   ordering on polyobjects amongst all subsectors.
*/

//
// Globals
//

// TODO: make anything static that ends up only being used in here.

// The Polyobjects
polyobj_t *PolyObjects;
int numPolyObjects;


//
// Static Data
//


//
// Static Functions
//

static void Polyobj_addSeg(polyobj_t *po, seg_t *seg)
{
}

static void Polyobj_findSegs(polyobj_t *po, seg_t *seg)
{
   int startx, starty;
   int i;

   // on first seg, save the initial vertex
   startx = seg->v1->x;
   starty = seg->v1->y;

   // use goto instead of recursion for maximum efficiency - thanks to lament
newseg:

   // terminal case: we have reached a seg where v2 is the same as v1 of the
   // initial seg
   if(seg->v2->x == startx && seg->v2->y == starty)
      return;
      
   // search the segs for one whose starting vertex is equal to the current
   // seg's ending vertex.
   for(i = 0; i < numsegs; ++i)
   {
      if(segs[i].v1->x == seg->v2->x && segs[i].v1->y == seg->v2->y)
      {
         // add the new seg and recurse
         Polyobj_addSeg(po, &segs[i]);
         seg = &segs[i];
         goto newseg;
      }
   }

   // error: if we reach here, the seg search never found another seg to
   // continue the loop, and thus the polyobject is open. This isn't allowed.
   po->isBad = true;
   doom_printf("polyobject %d is unclosed", po->id);
}

//
// Polyobj_spawnPolyObj
//
// Sets up a Polyobject.
//
static void Polyobj_spawnPolyObj(int num, mobj_t *spawnSpot, int id)
{
   polyobj_t *po = &PolyObjects[num];

/*
  TODO:

  1. Search segs for "line start" special with tag matching this polyobject's
     id number. If found, iterate through segs which share common vertices and
     record them into the polyobject.

  2. If no such line existed in the first step, look for a seg with the
     "explicit" special with tag matching this polyobject's id number. If found,
     continue to search for all such lines, sorting them into a temporary list
     of segs which is then copied into the polyobject in sorted order.

  3. If the polyobject is still empty, it is erroneous and should be marked
     as such (allowing the game to continue if possible).
*/

   // set the polyobject's spawn spot
   po->spawnSpot = spawnSpot;

   // hash the polyobject by its numeric id
   if(Polyobj_GetForNum(id))
   {
      // bad polyobject due to id conflict
      po->isBad = true;
      doom_printf("polyobject id conflict: %d", id);
   }
   else
   {
      int hashkey = id % numPolyObjects;
      po->id = id;      
      po->next = PolyObjects[hashkey].first;
      PolyObjects[hashkey].first = num;
   }
}


//
// Global Functions
//

//
// Polyobj_GetForNum
//
// Retrieves a polyobject by its numeric id using hashing.
// Returns NULL if no such polyobject exists.
//
polyobj_t *Polyobj_GetForNum(int id)
{
   int curidx  = PolyObjects[id % numPolyObjects].first;

   while(curidx != numPolyObjects && PolyObjects[curidx].id != id)
      curidx = PolyObjects[curidx].next;

   return curidx == numPolyObjects ? NULL : &PolyObjects[curidx];
}


// structure used to queue up mobj pointers in Polyobj_InitLevel
typedef struct mo_qitem_s
{
   mqueueitem_t mqitem;
   mobj_t *mo;
} mobjqitem_t;

//
// Polyobj_InitLevel
//
// Called at the beginning of each map after all other line and thing
// processing is finished.
//
void Polyobj_InitLevel(void)
{
   thinker_t   *th;
   mqueue_t    mobjqueue;
   mobjqitem_t *qitem;
   int i;

   M_QueueInit(&mobjqueue);

   // run down the thinker list, count the number of spawn points, and save
   // the mobj_t pointers on a queue for use below.
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function == P_MobjThinker)
      {
         mobj_t *mo = (mobj_t *)th;

         if(mo->info->doomednum == POLYOBJ_SPAWN_DOOMEDNUM ||
            mo->info->doomednum == POLYOBJ_SPAWNCRUSH_DOOMEDNUM)
            ++numPolyObjects;

         qitem = malloc(sizeof(mobjqitem_t));
         memset(qitem, 0, sizeof(mobjqitem_t));
         qitem->mo = mo;
         M_QueueInsert(&(qitem->mqitem), &mobjqueue);
      }
   }

   if(!numPolyObjects) // no polyobjects?
   {
      PolyObjects = NULL;
      return;
   }

   // allocate the PolyObjects array
   PolyObjects = Z_Malloc(numPolyObjects * sizeof(polyobj_t), PU_LEVEL, NULL);
   memset(PolyObjects, 0, numPolyObjects * sizeof(polyobj_t));

   // setup hash fields
   for(i = 0; i < numPolyObjects; ++i)
      PolyObjects[i].first = PolyObjects[i].next = numPolyObjects;

   // setup polyobjects
   for(i = 0; i < numPolyObjects; ++i)
   {
      qitem = (mobjqitem_t *)M_QueueIterator(&mobjqueue);

      Polyobj_spawnPolyObj(i, qitem->mo, qitem->mo->spawnpoint.angle);
   }

   // TODO: move polyobjects to anchor points

   // done with mobjqueue
   M_QueueFree(&mobjqueue);

   // TODO: setup polyobject clipping
}

#endif // ifdef POLYOBJECTS

// EOF

