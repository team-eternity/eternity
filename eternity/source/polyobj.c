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

#include "z_zone.h"

#include "doomstat.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_queue.h"
#include "polyobj.h"
#include "r_main.h"
#include "r_state.h"

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

d_inline static void Polyobj_bboxAdd(int *bbox, int x, int y)
{
   bbox[BOXTOP]    += y;
   bbox[BOXBOTTOM] += y;
   bbox[BOXLEFT]   += x;
   bbox[BOXTOP]    += x;
}

d_inline static void Polyobj_bboxSub(int *bbox, int x, int y)
{
   bbox[BOXTOP]    -= y;
   bbox[BOXBOTTOM] -= y;
   bbox[BOXLEFT]   -= x;
   bbox[BOXTOP]    -= x;
}

d_inline static void Polyobj_vecSub(vertex_t *dst, vertex_t *sub)
{
   dst->x -= sub->x;
   dst->y -= sub->y;
}

d_inline static void Polyobj_vecSub2(vertex_t *dst, vertex_t *v1, vertex_t *v2)
{
   dst->x = v1->x - v2->x;
   dst->y = v1->y - v2->y;
}

// Reallocating array maintenance

//
// Polyobj_addVertex
//
// Adds a vertex to a polyobject's reallocating vertex arrays, provided
// that such a vertex isn't already in the array. Each vertex must only
// be translated once during polyobject movement. Keeping track of them
// this way results in much more clear and efficient code than what
// Hexen used.
//
static void Polyobj_addVertex(polyobj_t *po, vertex_t *v)
{
   int i;

   // First: search the existing vertex pointers for a match. If one is found,
   // do not add this vertex again.
   for(i = 0; i < po->numVertices; ++i)
   {
      if(po->vertices[i] == v)
         return;
   }

   // add the vertex to both arrays (translation for origVerts is done later)
   if(po->numVertices >= po->numVerticesAlloc)
   {
      po->numVerticesAlloc = po->numVerticesAlloc ? po->numVerticesAlloc * 2 : 4;
      po->vertices = 
         (vertex_t **)Z_Realloc(po->vertices,
                                po->numVerticesAlloc * sizeof(vertex_t *),
                                PU_LEVEL, NULL);
      po->origVerts =
         (vertex_t *)Z_Realloc(po->origVerts,
                               po->numVerticesAlloc * sizeof(vertex_t),
                               PU_LEVEL, NULL);
   }
   po->vertices[po->numVertices] = v;
   po->origVerts[po->numVertices] = *v;
   po->numVertices++;
}

//
// Polyobj_addLine
//
// Adds a linedef to a polyobject's reallocating linedefs array, provided
// that such a linedef isn't already in the array. Each linedef must only
// be adjusted once during polyobject movement. Keeping track of them
// this way provides the same benefits as for vertices.
//
static void Polyobj_addLine(polyobj_t *po, line_t *l)
{
   int i;

   // First: search the existing line pointers for a match. If one is found,
   // do not add this line again.
   for(i = 0; i < po->numLines; ++i)
   {
      if(po->lines[i] == l)
         return;
   }

   // add the vertex to both arrays (translation for origVerts is done later)
   if(po->numLines >= po->numLinesAlloc)
   {
      po->numLinesAlloc = po->numLinesAlloc ? po->numLinesAlloc * 2 : 4;
      po->lines = (line_t **)Z_Realloc(po->lines, 
                                       po->numLinesAlloc * sizeof(line_t *),
                                       PU_LEVEL, NULL);
   }
   po->lines[po->numLines++] = l;
}

//
// Polyobj_addSeg
//
// Adds a single seg to a polyobject's reallocating seg pointer array.
// Most polyobjects will have between 4 and 16 segs, so the array size
// begins much smaller than usual. Calls Polyobj_addVertex and Polyobj_addLine
// to add those respective structures for this seg, as well.
//
static void Polyobj_addSeg(polyobj_t *po, seg_t *seg)
{
   if(po->segCount >= po->numSegsAlloc)
   {
      po->numSegsAlloc = po->numSegsAlloc ? po->numSegsAlloc * 2 : 4;
      po->segs = (seg_t **)Z_Realloc(po->segs, 
                                     po->numSegsAlloc * sizeof(seg_t *),
                                     PU_LEVEL, NULL);
   }
   po->segs[po->segCount++] = seg;

   // possibly add the lines and vertices for this seg. It may be technically
   // unnecessary to add the v2 vertex of segs, but this makes sure that even
   // erroneously open "explicit" segs will have both vertices added and will
   // reduce problems.
   Polyobj_addVertex(po, seg->v1);
   Polyobj_addVertex(po, seg->v2);
   Polyobj_addLine(po, seg->linedef);
}

// Seg-finding functions

//
// Polyobj_findSegs
//
// This method adds segs to a polyobject by following segs from vertex to
// vertex.  The process stops when the original starting point is reached
// or if a particular search ends unexpectedly (ie, the polyobject is not
// closed).
//
static void Polyobj_findSegs(polyobj_t *po, seg_t *seg)
{
   int startx, starty;
   int i;

   Polyobj_addSeg(po, seg);

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
   doom_printf("polyobject %d is not closed", po->id);
}

// structure used to store segs during explicit search process
typedef struct segitem_s
{
   seg_t *seg;
   int   num;
} segitem_t;

//
// Polyobj_segCompare
//
// Callback for qsort that compares two segitems.
//
static int Polyobj_segCompare(const void *s1, const void *s2)
{
   segitem_t *si1 = (segitem_t *)s1;
   segitem_t *si2 = (segitem_t *)s2;

   return (si1->num < si2->num) ? -1 : (si1->num > si2->num) ? 1 : 0;
}

//
// Polyobj_findExplicit
//
// Searches for segs to put into a polyobject in an explicitly provided order.
//
static void Polyobj_findExplicit(polyobj_t *po)
{
   // temporary dynamic seg array
   segitem_t *segitems = NULL;
   int numSegItems = 0;
   int numSegItemsAlloc = 0;

   int i;

   // first loop: save off all segs with polyobject's id number
   for(i = 0; i < numsegs; ++i)
   {
      if(segs[i].linedef->special == POLYOBJ_EXPLICIT_LINE  && 
         segs[i].linedef->args[0] == po->id &&
         segs[i].linedef->args[1] > 0)
      {
         if(numSegItems >= numSegItemsAlloc)
         {
            numSegItemsAlloc = numSegItemsAlloc ? numSegItemsAlloc*2 : 4;
            segitems = realloc(segitems, numSegItemsAlloc*sizeof(segitem_t));
         }
         segitems[numSegItems].seg = &segs[i];
         segitems[numSegItems].num = segs[i].linedef->args[1];
         ++numSegItems;
      }
   }

   // make sure array isn't empty
   if(numSegItems == 0)
   {
      po->isBad = true;
      doom_printf("polyobject %d is empty", po->id);
      return;
   }

   // sort the array
   qsort(segitems, numSegItems, sizeof(segitem_t), Polyobj_segCompare);

   // second loop: put the sorted segs into the polyobject
   for(i = 0; i < numSegItems; ++i)
      Polyobj_addSeg(po, segitems[i].seg);

   // free the temporary array
   free(segitems);
}

// Setup functions

//
// Polyobj_spawnPolyObj
//
// Sets up a Polyobject.
//
static void Polyobj_spawnPolyObj(int num, mobj_t *spawnSpot, int id)
{
   int i;
   polyobj_t *po = &PolyObjects[num];

   po->id = id;

   // 1. Search segs for "line start" special with tag matching this 
   //    polyobject's id number. If found, iterate through segs which
   //    share common vertices and record them into the polyobject.
   for(i = 0; i < numsegs; ++i)
   {
      seg_t *seg = &segs[i];

      // is it a START line with this polyobject's id?
      if(seg->linedef->special == POLYOBJ_START_LINE && 
         seg->linedef->args[0] == po->id)
      {
         Polyobj_findSegs(po, seg);
         break;
      }
   }

   // if an error occurred above, quit processing this object
   if(po->isBad)
      return;

   // 2. If no such line existed in the first step, look for a seg with the 
   //    "explicit" special with tag matching this polyobject's id number. If
   //    found, continue to search for all such lines, storing them in a 
   //    temporary list of segs which is then copied into the polyobject in 
   //    sorted order.
   if(po->segCount == 0)
      Polyobj_findExplicit(po);

   // if an error occurred above, quit processing this object
   if(po->isBad)
      return;

   // set the polyobject's spawn spot
   po->spawnSpot.x = spawnSpot->x;
   po->spawnSpot.y = spawnSpot->y;

   // hash the polyobject by its numeric id
   if(Polyobj_GetForNum(po->id))
   {
      // bad polyobject due to id conflict
      po->isBad = true;
      doom_printf("polyobject id conflict: %d", id);
   }
   else
   {
      int hashkey = po->id % numPolyObjects;
      po->next = PolyObjects[hashkey].first;
      PolyObjects[hashkey].first = num;
   }
}

//
// Polyobj_moveToSpawnSpot
//
// Translates the polyobject's vertices with respect to the difference between
// the anchor and spawn spots. Updates linedef bounding boxes as well.
//
static void Polyobj_moveToSpawnSpot(mapthing_t *anchor)
{
   polyobj_t *po;
   vertex_t  dist, sspot;
   int i;

   if(!(po = Polyobj_GetForNum(anchor->angle)))
   {
      doom_printf("bad polyobject %d for anchor point", anchor->angle);
      return;
   }

   // don't move any bad polyobject that may have gotten through
   if(po->isBad)
      return;

   sspot.x = po->spawnSpot.x;
   sspot.y = po->spawnSpot.y;

   // calculate distance from anchor to spawn spot
   dist.x = (anchor->x << FRACBITS) - sspot.x;
   dist.y = (anchor->y << FRACBITS) - sspot.y;

   // update linedef bounding boxes
   for(i = 0; i < po->numLines; ++i)
      Polyobj_bboxSub(po->lines[i]->bbox, dist.x, dist.y);

   // translate vertices and record original coordinates relative to spawn spot
   for(i = 0; i < po->numVertices; ++i)
   {
      Polyobj_vecSub(po->vertices[i], &dist);

      Polyobj_vecSub2(&(po->origVerts[i]), po->vertices[i], &sspot);
   }

   // all polyobjects start flagged as having moved
   po->hasMoved = true;
}

//
// Polyobj_attachToSubsec
//
// Attaches a polyobject to its appropriate subsector.
//
static void Polyobj_attachToSubsec(polyobj_t *po)
{
   subsector_t *ss;
   double center_x = 0.0, center_y = 0.0;
   int i;

   // the center point calculation is only necessary if the polyobject has
   // moved since the last time it was attached.
   if(po->hasMoved)
   {
      po->hasMoved = false;

      for(i = 0; i < po->numVertices; ++i)
      {
         center_x += (double)(po->vertices[i]->x) / FRACUNIT;
         center_y += (double)(po->vertices[i]->y) / FRACUNIT;
      }

      center_x /= po->numVertices;
      center_y /= po->numVertices;

      po->centerPt.x = (fixed_t)(center_x * FRACUNIT);
      po->centerPt.y = (fixed_t)(center_y * FRACUNIT);
   }

   ss = R_PointInSubsector(po->centerPt.x, po->centerPt.y);

   M_DLListInsert((mdllistitem_t *)&po->link, 
                  &((mdllistitem_t *)ss->polyList));

   po->attached = true;
}

//
// Polyobj_removeFromSubsec
//
// Removes a polyobject from the subsector to which it is attached.
//
static void Polyobj_removeFromSubsec(polyobj_t *po)
{
   if(po->attached)
   {
      M_DLListRemove(&po->link);
      po->attached = false;
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
typedef struct mobjqitem_s
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
   mqueue_t    spawnqueue;
   mqueue_t    anchorqueue;
   mobjqitem_t *qitem;
   int i, numAnchors = 0;

   M_QueueInit(&spawnqueue);
   M_QueueInit(&anchorqueue);

   // get rid of values from previous level
   PolyObjects    = NULL;
   numPolyObjects = 0;

   // run down the thinker list, count the number of spawn points, and save
   // the mobj_t pointers on a queue for use below.
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function == P_MobjThinker)
      {
         mobj_t *mo = (mobj_t *)th;

         if(mo->info->doomednum == POLYOBJ_SPAWN_DOOMEDNUM ||
            mo->info->doomednum == POLYOBJ_SPAWNCRUSH_DOOMEDNUM)
         {
            ++numPolyObjects;
            
            qitem = malloc(sizeof(mobjqitem_t));
            memset(qitem, 0, sizeof(mobjqitem_t));
            qitem->mo = mo;
            M_QueueInsert(&(qitem->mqitem), &spawnqueue);
         }
         else if(mo->info->doomednum == POLYOBJ_ANCHOR_DOOMEDNUM)
         {
            ++numAnchors;

            qitem = malloc(sizeof(mobjqitem_t));
            memset(qitem, 0, sizeof(mobjqitem_t));
            qitem->mo = mo;
            M_QueueInsert(&(qitem->mqitem), &anchorqueue);
         }
      }
   }

   if(!numPolyObjects) // no polyobjects?
      return;

   // allocate the PolyObjects array
   PolyObjects = Z_Malloc(numPolyObjects * sizeof(polyobj_t), PU_LEVEL, NULL);
   memset(PolyObjects, 0, numPolyObjects * sizeof(polyobj_t));

   // setup hash fields
   for(i = 0; i < numPolyObjects; ++i)
      PolyObjects[i].first = PolyObjects[i].next = numPolyObjects;

   // setup polyobjects
   for(i = 0; i < numPolyObjects; ++i)
   {
      qitem = (mobjqitem_t *)M_QueueIterator(&spawnqueue);

      Polyobj_spawnPolyObj(i, qitem->mo, qitem->mo->spawnpoint.angle);
   }

   // move polyobjects to spawn points
   for(i = 0; i < numAnchors; ++i)
   {
      qitem = (mobjqitem_t *)M_QueueIterator(&anchorqueue);

      Polyobj_moveToSpawnSpot(&(qitem->mo->spawnpoint));
   }

   // done with mobj queues
   M_QueueFree(&spawnqueue);
   M_QueueFree(&anchorqueue);

   // TODO: setup polyobject clipping
}

//
// Polyobj_Ticker
//
// Called from P_Ticker after thinkers run. Removes polyobjects from their
// current subsectors and reattaches them to the appropriate subsectors.
//
void Polyobj_Ticker(void)
{
   int i;
   polyobj_t *po;

   for(i = 0; i < numPolyObjects; ++i)
   {
      po = &PolyObjects[i];

      Polyobj_removeFromSubsec(po);      
      Polyobj_attachToSubsec(po);
   }
}

#endif // ifdef POLYOBJECTS

// EOF

