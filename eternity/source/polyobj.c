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

#include "z_zone.h"

#include "doomstat.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_queue.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "p_tick.h"
#include "polyobj.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sndseq.h"
#include "v_misc.h"

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
// Defines
//

#define BYTEANGLEMUL     (ANG90/64)

//
// Globals
//

// The Polyobjects
polyobj_t *PolyObjects;
int numPolyObjects;

// Polyobject Blockmap -- initialized in P_LoadBlockMap
polymaplink_t **polyblocklinks;


//
// Static Data
//

// Polyobject Blockmap
static polymaplink_t *bmap_freelist; // free list of blockmap links


//
// Static Functions
//

d_inline static void Polyobj_bboxAdd(int *bbox, vertex_t *add)
{
   bbox[BOXTOP]    += add->y;
   bbox[BOXBOTTOM] += add->y;
   bbox[BOXLEFT]   += add->x;
   bbox[BOXRIGHT]  += add->x;
}

d_inline static void Polyobj_bboxSub(int *bbox, vertex_t *sub)
{
   bbox[BOXTOP]    -= sub->y;
   bbox[BOXBOTTOM] -= sub->y;
   bbox[BOXLEFT]   -= sub->x;
   bbox[BOXRIGHT]  -= sub->x;
}

d_inline static void Polyobj_vecAdd(vertex_t *dst, vertex_t *add)
{
   dst->x += add->x;
   dst->y += add->y;
   dst->fx = (float)dst->x / 65536.0f;
   dst->fy = (float)dst->y / 65536.0f;
}

d_inline static void Polyobj_vecSub(vertex_t *dst, vertex_t *sub)
{
   dst->x -= sub->x;
   dst->y -= sub->y;
   dst->fx = (float)dst->x / 65536.0f;
   dst->fy = (float)dst->y / 65536.0f;
}

d_inline static void Polyobj_vecSub2(vertex_t *dst, vertex_t *v1, vertex_t *v2)
{
   dst->x = v1->x - v2->x;
   dst->y = v1->y - v2->y;
   dst->fx = (float)dst->x / 65536.0f;
   dst->fy = (float)dst->y / 65536.0f;
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

   // add the vertex to all arrays (translation for origVerts is done later)
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

      po->tmpVerts =
         (vertex_t *)Z_Realloc(po->tmpVerts,
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

   // add the line to the array
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
   doom_printf(FC_ERROR "polyobject %d is not closed", po->id);
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

   return si2->num - si1->num;
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
      doom_printf(FC_ERROR "polyobject %d is empty", po->id);
      return;
   }

   // sort the array if necessary
   if(numSegItems >= 2)
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

   // don't spawn a polyobject more than once
   if(po->segCount)
   {
      doom_printf(FC_ERROR "polyobj %d has more than one spawn spot", po->id);
      return;
   }

   po->id = id;

   // TODO: support customized damage somehow?
   if(spawnSpot->info->doomednum == POLYOBJ_SPAWNCRUSH_DOOMEDNUM)
      po->damage = 3;

   // set to default thrust; may be modified by attached thinkers
   // TODO: support customized thrust?
   po->thrust = FRACUNIT;

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
         po->mirror = seg->linedef->args[1];
         if(po->mirror == po->id) // do not allow a self-reference
            po->mirror = -1;
         // sound sequence is in args[2]
         po->seqId = seg->linedef->args[2];
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
   {
      Polyobj_findExplicit(po);
      // if an error occurred above, quit processing this object
      if(po->isBad)
         return;
      po->mirror = po->segs[0]->linedef->args[2];
      if(po->mirror == po->id) // do not allow a self-reference
         po->mirror = -1;
      // sound sequence is in args[3]
      po->seqId = po->segs[0]->linedef->args[3];
   }


   // set the polyobject's spawn spot
   po->spawnSpot.x = spawnSpot->x;
   po->spawnSpot.y = spawnSpot->y;

   // hash the polyobject by its numeric id
   if(Polyobj_GetForNum(po->id))
   {
      // bad polyobject due to id conflict
      po->isBad = true;
      doom_printf(FC_ERROR "polyobject id conflict: %d", id);
   }
   else
   {
      int hashkey = po->id % numPolyObjects;
      po->next = PolyObjects[hashkey].first;
      PolyObjects[hashkey].first = num;
   }
}

static void Polyobj_attachToSubsec(polyobj_t *po);

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
      doom_printf(FC_ERROR "bad polyobject %d for anchor point", anchor->angle);
      return;
   }

   // don't move any bad polyobject that may have gotten through
   if(po->isBad)
      return;

   // don't move any polyobject more than once
   if(po->attached)
   {
      doom_printf(FC_ERROR "polyobj %d has more than one anchor", po->id);
      return;
   }

   sspot.x = po->spawnSpot.x;
   sspot.y = po->spawnSpot.y;

   // calculate distance from anchor to spawn spot
   dist.x = (anchor->x << FRACBITS) - sspot.x;
   dist.y = (anchor->y << FRACBITS) - sspot.y;

   // update linedef bounding boxes
   for(i = 0; i < po->numLines; ++i)
      Polyobj_bboxSub(po->lines[i]->bbox, &dist);

   // translate vertices and record original coordinates relative to spawn spot
   for(i = 0; i < po->numVertices; ++i)
   {
      Polyobj_vecSub(po->vertices[i], &dist);

      Polyobj_vecSub2(&(po->origVerts[i]), po->vertices[i], &sspot);
   }

   // attach to subsector
   Polyobj_attachToSubsec(po);
}

//
// Polyobj_attachToSubsec
//
// Attaches a polyobject to its appropriate subsector.
//
static void Polyobj_attachToSubsec(polyobj_t *po)
{
   subsector_t  *ss;
   double center_x = 0.0, center_y = 0.0;
   int i;

   // never attach a bad polyobject
   if(po->isBad)
      return;

   for(i = 0; i < po->numVertices; ++i)
   {
      center_x += (double)(po->vertices[i]->x) / FRACUNIT;
      center_y += (double)(po->vertices[i]->y) / FRACUNIT;
   }
   
   center_x /= po->numVertices;
   center_y /= po->numVertices;
   
   po->centerPt.x = (fixed_t)(center_x * FRACUNIT);
   po->centerPt.y = (fixed_t)(center_y * FRACUNIT);

   ss = R_PointInSubsector(po->centerPt.x, po->centerPt.y);

   M_DLListInsert(&po->link, (mdllistitem_t **)(&ss->polyList));

#ifdef R_LINKEDPORTALS
   // set spawnSpot's groupid for correct portal sound behavior
   po->spawnSpot.groupid = ss->sector->groupid;
#endif

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

// Blockmap Functions

//
// Polyobj_getLink
//
// Retrieves a polymaplink object from the free list or creates a new one.
//
static polymaplink_t *Polyobj_getLink(void)
{
   polymaplink_t *l;

   if(bmap_freelist)
   {
      l = bmap_freelist;
      bmap_freelist = l->po_next;
   }
   else
      l = Z_Calloc(1, sizeof(*l), PU_LEVEL, NULL);

   return l;
}

//
// Polyobj_putLink
//
// Puts a polymaplink object into the free list.
//
static void Polyobj_putLink(polymaplink_t *l)
{
   memset(l, 0, sizeof(*l));
   l->po_next = bmap_freelist;
   bmap_freelist = l;
}

//
// Polyobj_linkToBlockmap
//
// Inserts a polyobject into the polyobject blockmap. Unlike mobj_t's,
// polyobjects need to be linked into every blockmap cell which their
// bounding box intersects. This ensures the accurate level of clipping
// which is present with linedefs but absent from most mobj interactions.
//
static void Polyobj_linkToBlockmap(polyobj_t *po)
{
   fixed_t *blockbox = po->blockbox;
   int i, x, y;
   
   // never link a bad polyobject or a polyobject already linked
   if(po->isBad || po->linked)
      return;
   
   // 2/26/06: start line box with values of first vertex, not MININT/MAXINT
   blockbox[BOXLEFT]   = blockbox[BOXRIGHT] = po->vertices[0]->x;
   blockbox[BOXBOTTOM] = blockbox[BOXTOP]   = po->vertices[0]->y;
   
   // add all vertices to the bounding box
   for(i = 1; i < po->numVertices; ++i)
      M_AddToBox(blockbox, po->vertices[i]->x, po->vertices[i]->y);
   
   // adjust bounding box relative to blockmap 
   blockbox[BOXRIGHT]  = (blockbox[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
   blockbox[BOXLEFT]   = (blockbox[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
   blockbox[BOXTOP]    = (blockbox[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;
   blockbox[BOXBOTTOM] = (blockbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   
   // link polyobject to every block its bounding box intersects
   for(y = blockbox[BOXBOTTOM]; y <= blockbox[BOXTOP]; ++y)
   {
      for(x = blockbox[BOXLEFT]; x <= blockbox[BOXRIGHT]; ++x)   
      {
         if(!(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
         {
            polymaplink_t  *l = Polyobj_getLink();
            
            l->po = po;

            // haleyjd 05/18/06: optimization: keep track of links in polyobject
            l->po_next = po->linkhead;
            po->linkhead = l;
            
            M_DLListInsert(&l->link, 
                           (mdllistitem_t **)(&polyblocklinks[y*bmapwidth + x]));
         }
      }
   }

   po->linked = true;
}

//
// Polyobj_removeFromBlockmap
//
// Unlinks a PolyObject from the blockmap and returns its links to the
// freelist.
//
// haleyjd 05/18/06: completely rewritten to optimize unlinking. Links are
// now tracked in a single-linked list starting in the polyobject itself
// and running through all polymaplinks. This should be much faster than
// searching through the blockmap.
//
static void Polyobj_removeFromBlockmap(polyobj_t *po)
{
   polymaplink_t *l = po->linkhead;

   // don't bother trying to unlink one that's not linked
   if(!po->linked)
      return;

   while(l)
   {
      polymaplink_t *next = l->po_next;
      M_DLListRemove(&l->link);
      Polyobj_putLink(l);
      l = next;
   }

   po->linkhead = NULL;

   po->linked = false;
}


// Movement functions

//
// Polyobj_untouched
//
// A version of Lee's routine from p_maputl.c that accepts an mobj pointer
// argument instead of using tmthing. Returns true if the line isn't contacted
// and false otherwise.
//
d_inline static boolean Polyobj_untouched(line_t *ld, mobj_t *mo)
{
   fixed_t x, y, tmbbox[4];

   return
      (tmbbox[BOXRIGHT]  = (x = mo->x) + mo->radius) <= ld->bbox[BOXLEFT]   ||
      (tmbbox[BOXLEFT]   =           x - mo->radius) >= ld->bbox[BOXRIGHT]  ||
      (tmbbox[BOXTOP]    = (y = mo->y) + mo->radius) <= ld->bbox[BOXBOTTOM] ||
      (tmbbox[BOXBOTTOM] =           y - mo->radius) >= ld->bbox[BOXTOP]    ||
      P_BoxOnLineSide(tmbbox, ld) != -1;
}

//
// Polyobj_pushThing
//
// Inflicts thrust and possibly damage on a thing which has been found to be
// blocking the motion of a polyobject. The default thrust amount is only one
// unit, but the motion of the polyobject can be used to change this.
//
static void Polyobj_pushThing(polyobj_t *po, line_t *line, mobj_t *mo)
{
   angle_t lineangle;
   fixed_t momx, momy;
   
   // calculate angle of line and subtract 90 degrees to get normal
   lineangle = R_PointToAngle2(0, 0, line->dx, line->dy) - ANG90;
   lineangle >>= ANGLETOFINESHIFT;
   momx = FixedMul(po->thrust, finecosine[lineangle]);
   momy = FixedMul(po->thrust, finesine[lineangle]);
   mo->momx += momx;
   mo->momy += momy;

   // if object doesn't fit at desired location, possibly hurt it
   if(po->damage && mo->flags & MF_SHOOTABLE)
   {
      if(!P_CheckPosition(mo, mo->x + momx, mo->y + momy))
         P_DamageMobj(mo, NULL, NULL, po->damage, MOD_CRUSH);
   }
}

//
// Polyobj_clipThings
//
// Checks for things that are in the way of a polyobject line move.
// Returns true if something was hit.
//
static boolean Polyobj_clipThings(polyobj_t *po, line_t *line)
{
   boolean hitthing = false;
   fixed_t linebox[4];
   int x, y;

   // adjust linedef bounding box to blockmap, extend by MAXRADIUS
   linebox[BOXLEFT]   = (line->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   linebox[BOXRIGHT]  = (line->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   linebox[BOXBOTTOM] = (line->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   linebox[BOXTOP]    = (line->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   // check all mobj blockmap cells the line contacts
   for(y = linebox[BOXBOTTOM]; y <= linebox[BOXTOP]; ++y)
   {
      for(x = linebox[BOXLEFT]; x <= linebox[BOXRIGHT]; ++x)
      {
         if(!(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
         {
            mobj_t *mo = blocklinks[y * bmapwidth + x];

            for(; mo; mo = mo->bnext)
            {
               // always push players even if not solid
               if(!((mo->flags & MF_SOLID) || mo->player))
                  continue;

               if(Polyobj_untouched(line, mo))
                  continue;

               Polyobj_pushThing(po, line, mo);
               hitthing = true;
            }
         } // end if
      } // end for(x)
   } // end for(y)

   return hitthing;
}

//
// Polyobj_moveXY
//
// Moves a polyobject on the x-y plane.
//
static boolean Polyobj_moveXY(polyobj_t *po, fixed_t x, fixed_t y)
{
   int i;
   vertex_t vec;
   boolean hitthing = false;

   vec.x = x;
   vec.y = y;

   // don't move bad polyobjects
   if(po->isBad)
      return false;

   // translate vertices
   for(i = 0; i < po->numVertices; ++i)
      Polyobj_vecAdd(po->vertices[i], &vec);

   // translate each line
   for(i = 0; i < po->numLines; ++i)
      Polyobj_bboxAdd(po->lines[i]->bbox, &vec);

   // check for blocking things (yes, it needs to be done separately)
   for(i = 0; i < po->numLines; ++i)
      hitthing |= Polyobj_clipThings(po, po->lines[i]);

   if(hitthing)
   {
      // reset vertices
      for(i = 0; i < po->numVertices; ++i)
         Polyobj_vecSub(po->vertices[i], &vec);
      
      // reset lines that have been moved
      for(i = 0; i < po->numLines; ++i)
         Polyobj_bboxSub(po->lines[i]->bbox, &vec);      
   }
   else
   {
      // translate the spawnSpot as well
      po->spawnSpot.x += vec.x;
      po->spawnSpot.y += vec.y;          

      
      Polyobj_removeFromBlockmap(po); // unlink it from the blockmap
      Polyobj_removeFromSubsec(po);   // unlink it from its subsector
      Polyobj_linkToBlockmap(po);     // relink to blockmap
      Polyobj_attachToSubsec(po);     // relink to subsector
   }

   return !hitthing;
}

//
// Polyobj_rotatePoint
//
// Rotates a point and then translates it relative to point c.
// The formula for this can be found here:
// http://www.inversereality.org/tutorials/graphics%20programming/2dtransformations.html
// It is, of course, just a vector-matrix multiplication.
//
d_inline static void Polyobj_rotatePoint(vertex_t *v, const vertex_t *c, int ang)
{
   vertex_t tmp = *v;

   v->x = FixedMul(tmp.x, finecosine[ang]) - FixedMul(tmp.y,   finesine[ang]);
   v->y = FixedMul(tmp.x,   finesine[ang]) + FixedMul(tmp.y, finecosine[ang]);

   v->x += c->x;
   v->y += c->y;

   v->fx = (float)v->x / 65536.0f;
   v->fy = (float)v->y / 65536.0f;
}

//
// Polyobj_rotateLine
//
// Taken from P_LoadLineDefs; simply updates the linedef's dx, dy, slopetype,
// and bounding box to be consistent with its vertices.
//
static void Polyobj_rotateLine(line_t *ld)
{
   vertex_t *v1, *v2;

   v1 = ld->v1;
   v2 = ld->v2;

   // set dx, dy
   ld->dx = v2->x - v1->x;
   ld->dy = v2->y - v1->y;

   // determine slopetype
   ld->slopetype = !ld->dx ? ST_VERTICAL : !ld->dy ? ST_HORIZONTAL :
      FixedDiv(ld->dy, ld->dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

   // update bounding box
   if(v1->x < v2->x)
   {
      ld->bbox[BOXLEFT]  = v1->x;
      ld->bbox[BOXRIGHT] = v2->x;
   }
   else
   {
      ld->bbox[BOXLEFT]  = v2->x;
      ld->bbox[BOXRIGHT] = v1->x;
   }
      
   if(v1->y < v2->y)
   {
      ld->bbox[BOXBOTTOM] = v1->y;
      ld->bbox[BOXTOP]    = v2->y;
   }
   else
   {
      ld->bbox[BOXBOTTOM] = v2->y;
      ld->bbox[BOXTOP]    = v1->y;
   }
}

//
// Polyobj_rotate
//
// Rotates a polyobject around its start point.
//
static boolean Polyobj_rotate(polyobj_t *po, angle_t delta)
{
   int i, angle;
   vertex_t origin;
   boolean hitthing = false;

   // don't move bad polyobjects
   if(po->isBad)
      return false;

   angle = (po->angle + delta) >> ANGLETOFINESHIFT;

   // point about which to rotate is the spawn spot
   origin.x = po->spawnSpot.x;
   origin.y = po->spawnSpot.y;

   // save current positions and rotate all vertices
   for(i = 0; i < po->numVertices; ++i)
   {
      po->tmpVerts[i] = *(po->vertices[i]);

      // use original pts to rotate to new position
      *(po->vertices[i]) = po->origVerts[i];

      Polyobj_rotatePoint(po->vertices[i], &origin, angle);
   }

   // rotate lines
   for(i = 0; i < po->numLines; ++i)
      Polyobj_rotateLine(po->lines[i]);

   // check for blocking things
   for(i = 0; i < po->numLines; ++i)
      hitthing |= Polyobj_clipThings(po, po->lines[i]);

   if(hitthing)
   {
      // reset vertices to previous positions
      for(i = 0; i < po->numVertices; ++i)
         *(po->vertices[i]) = po->tmpVerts[i];

      // reset lines
      for(i = 0; i < po->numLines; ++i)
         Polyobj_rotateLine(po->lines[i]);
   }
   else
   {
      // update seg angles (used only by renderer)
      for(i = 0; i < po->segCount; ++i)
         po->segs[i]->angle += delta;

      // update polyobject's angle
      po->angle += delta;

      Polyobj_removeFromBlockmap(po); // unlink it from the blockmap
      Polyobj_removeFromSubsec(po);   // remove from subsector
      Polyobj_linkToBlockmap(po);     // relink to blockmap
      Polyobj_attachToSubsec(po);     // relink to subsector
   }

   return !hitthing;
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
   int curidx;

   // haleyjd 01/07/07: must check if == 0 first!
   if(numPolyObjects == 0)
      return NULL;
   
   curidx = PolyObjects[id % numPolyObjects].first;

   while(curidx != numPolyObjects && PolyObjects[curidx].id != id)
      curidx = PolyObjects[curidx].next;

   return curidx == numPolyObjects ? NULL : &PolyObjects[curidx];
}

//
// Polyobj_GetMirror
//
// Retrieves the mirroring polyobject if one exists. Returns NULL
// otherwise.
//
polyobj_t *Polyobj_GetMirror(polyobj_t *po)
{
   return (po && po->mirror != -1) ? Polyobj_GetForNum(po->mirror) : NULL;
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
   // note: as with msecnodes, it is very important to clear out the blockmap
   // node freelist, otherwise it may contain dangling pointers to old objects
   PolyObjects    = NULL;
   numPolyObjects = 0;
   bmap_freelist  = NULL;

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
            
            qitem = calloc(1, sizeof(mobjqitem_t));
            qitem->mo = mo;
            M_QueueInsert(&(qitem->mqitem), &spawnqueue);
         }
         else if(mo->info->doomednum == POLYOBJ_ANCHOR_DOOMEDNUM)
         {
            ++numAnchors;

            qitem = calloc(1, sizeof(mobjqitem_t));
            qitem->mo = mo;
            M_QueueInsert(&(qitem->mqitem), &anchorqueue);
         }
      }
   }

   if(numPolyObjects)
   {
      // allocate the PolyObjects array
      PolyObjects = Z_Calloc(numPolyObjects, sizeof(polyobj_t), 
                             PU_LEVEL, NULL);

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

      // setup polyobject clipping
      for(i = 0; i < numPolyObjects; ++i)
         Polyobj_linkToBlockmap(&PolyObjects[i]);
   }

#if 0
   // haleyjd 02/22/06: temporary debug
   printf("DEBUG: numPolyObjects = %d\n", numPolyObjects);
   for(i = 0; i < numPolyObjects; ++i)
   {
      int j;
      polyobj_t *po = &PolyObjects[i];

      printf("polyobj %d:\n", i);
      printf("id = %d, first = %d, next = %d\n", po->id, po->first, po->next);
      printf("segCount = %d, numSegsAlloc = %d\n", po->segCount, po->numSegsAlloc);
      for(j = 0; j < po->segCount; ++j)
         printf("\tseg %d: %p\n", j, po->segs[j]);
      printf("numVertices = %d, numVerticesAlloc = %d\n", po->numVertices, po->numVerticesAlloc);
      for(j = 0; j < po->numVertices; ++j)
      {
         printf("\tvtx %d: (%d, %d) / orig: (%d, %d)\n",
                j, po->vertices[j]->x>>FRACBITS, po->vertices[j]->y>>FRACBITS,
                po->origVerts[j].x>>FRACBITS, po->origVerts[j].y>>FRACBITS);
      }
      printf("numLines = %d, numLinesAlloc = %d\n", po->numLines, po->numLinesAlloc);
      for(j = 0; j < po->numLines; ++j)
         printf("\tline %d: %p\n", j, po->lines[j]);
      printf("spawnSpot = (%d, %d)\n", po->spawnSpot.x >> FRACBITS, po->spawnSpot.y >> FRACBITS);
      printf("centerPt = (%d, %d)\n", po->centerPt.x >> FRACBITS, po->centerPt.y >> FRACBITS);
      printf("attached = %d, linked = %d, validcount = %d, isBad = %d\n",
             po->attached, po->linked, po->validcount, po->isBad);
      printf("blockbox: [%d, %d, %d, %d]\n", 
             po->blockbox[BOXLEFT], po->blockbox[BOXRIGHT], po->blockbox[BOXBOTTOM],
             po->blockbox[BOXTOP]);
   }
#endif

   // done with mobj queues
   M_QueueFree(&spawnqueue);
   M_QueueFree(&anchorqueue);
}

//
// Polyobj_MoveOnLoad
//
// Called when a savegame is being loaded. Rotates and translates an
// existing polyobject to its position when the game was saved.
//
void Polyobj_MoveOnLoad(polyobj_t *po, angle_t angle, fixed_t x, fixed_t y)
{
   fixed_t dx, dy;
   
   // first, rotate to the saved angle
   Polyobj_rotate(po, angle);
   
   // determine component distances to translate
   dx = x - po->spawnSpot.x;
   dy = y - po->spawnSpot.y;

   // translate
   Polyobj_moveXY(po, dx, dy);
}

// Thinker Functions

//
// T_PolyObjRotate
//
// Thinker function for PolyObject rotation.
//
void T_PolyObjRotate(polyrotate_t *th)
{
   polyobj_t *po = Polyobj_GetForNum(th->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyObjRotate: thinker has invalid id %d\n", th->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == NULL)
   {
      po->thinker = &th->thinker;
      
      // reset polyobject's thrust
      po->thrust = D_abs(th->speed) >> 8;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // rotate by 'speed' angle per frame
   // if distance == -1, this polyobject rotates perpetually
   if(Polyobj_rotate(po, th->speed) && th->distance != -1)
   {
      int avel = D_abs(th->speed);

      // decrement distance by the amount it moved
      th->distance -= avel;

      // are we at or past the destination?
      if(th->distance <= 0)
      {
         // remove thinker
         if(po->thinker == &th->thinker)
         {
            po->thinker = NULL;
            po->thrust = FRACUNIT;
         }
         P_RemoveThinker(&th->thinker);

         // TODO: notify scripts
         S_StopPolySequence(po);
      }
      else if(th->distance < avel)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         th->speed = th->speed >= 0 ? th->distance : -th->distance;
      }
   }
}

//
// Polyobj_componentSpeed
//
// Calculates the speed components from the desired resultant velocity.
//
d_inline static void Polyobj_componentSpeed(int resVel, int angle, 
                                            int *xVel, int *yVel)
{
   *xVel = FixedMul(resVel, finecosine[angle]);
   *yVel = FixedMul(resVel,   finesine[angle]);
}

void T_PolyObjMove(polymove_t *th)
{
   polyobj_t *po = Polyobj_GetForNum(th->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyObjRotate: thinker has invalid id %d\n", th->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == NULL)
   {
      po->thinker = &th->thinker;
      
      // reset polyobject's thrust
      po->thrust = D_abs(th->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // move the polyobject one step along its movement angle
   if(Polyobj_moveXY(po, th->momx, th->momy))
   {
      int avel = D_abs(th->speed);

      // decrement distance by the amount it moved
      th->distance -= avel;

      // are we at or past the destination?
      if(th->distance <= 0)
      {
         // remove thinker
         if(po->thinker == &th->thinker)
         {
            po->thinker = NULL;
            po->thrust = FRACUNIT;
         }
         P_RemoveThinker(&th->thinker);

         // TODO: notify scripts
         S_StopPolySequence(po);
      }
      else if(th->distance < avel)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         th->speed = th->speed >= 0 ? th->distance : -th->distance;
         Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);
      }
   }
}

void T_PolyDoorSlide(polyslidedoor_t *th)
{
   polyobj_t *po = Polyobj_GetForNum(th->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyDoorSlide: thinker has invalid id %d\n", th->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == NULL)
   {
      po->thinker = &th->thinker;
      
      // reset polyobject's thrust
      po->thrust = D_abs(th->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // count down wait period
   if(th->delayCount)
   {
      if(--th->delayCount == 0)
         S_StartPolySequence(po);
      return;
   }

      // move the polyobject one step along its movement angle
   if(Polyobj_moveXY(po, th->momx, th->momy))
   {
      int avel = D_abs(th->speed);
      
      // decrement distance by the amount it moved
      th->distance -= avel;
      
      // are we at or past the destination?
      if(th->distance <= 0)
      {
         // does it need to close?
         if(!th->closing)
         {
            th->closing = true;
            
            // reset distance and speed
            th->distance = th->initDistance;
            th->speed    = th->initSpeed;
            
            // start delay
            th->delayCount = th->delay;
            
            // reverse angle
            th->angle = th->revAngle;
            
            // reset component speeds
            Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);
         } 
         else
         {
            // remove thinker
            if(po->thinker == &th->thinker)
            {
               po->thinker = NULL;
               po->thrust = FRACUNIT;
            }
            P_RemoveThinker(&th->thinker);
            // TODO: notify scripts
         }
         S_StopPolySequence(po);
      }
      else if(th->distance < avel)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         th->speed = th->speed >= 0 ? th->distance : -th->distance;
         Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);
      }
   }
   else if(th->closing && th->distance != th->initDistance)
   {
      // move was blocked, special handling required -- make it reopen
      th->distance = th->initDistance - th->distance;
      th->speed    = th->initSpeed;
      th->angle    = th->initAngle;
      Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);
      th->closing  = false;
      S_StartPolySequence(po);
   }
}

void T_PolyDoorSwing(polyswingdoor_t *th)
{
   polyobj_t *po = Polyobj_GetForNum(th->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyDoorSwing: thinker has invalid id %d\n", th->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == NULL)
   {
      po->thinker = &th->thinker;
      
      // reset polyobject's thrust
      po->thrust = D_abs(th->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // count down wait period
   if(th->delayCount)
   {
      if(--th->delayCount == 0)
         S_StartPolySequence(po);
      return;
   }

   // rotate by 'speed' angle per frame
   // if distance == -1, this polyobject rotates perpetually
   if(Polyobj_rotate(po, th->speed) && th->distance != -1)
   {
      int avel = D_abs(th->speed);

      // decrement distance by the amount it moved
      th->distance -= avel;

      // are we at or past the destination?
      if(th->distance <= 0)
      {
         // does it need to close?
         if(!th->closing)
         {
            th->closing = true;
            
            // reset distance and speed
            th->distance =  th->initDistance;
            th->speed    = -th->initSpeed; // reverse speed on close
            
            // start delay
            th->delayCount = th->delay;            
         } 
         else
         {
            // remove thinker
            if(po->thinker == &th->thinker)
            {
               po->thinker = NULL;
               po->thrust = FRACUNIT;
            }
            P_RemoveThinker(&th->thinker);
            // TODO: notify scripts
         }
         S_StopPolySequence(po);
      }
      else if(th->distance < avel)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         th->speed = th->speed >= 0 ? th->distance : -th->distance;
      }
   }
   else if(th->closing && th->distance != th->initDistance)
   {
      // move was blocked, special handling required -- make it reopen

      th->distance = th->initDistance - th->distance;
      th->speed    = th->initSpeed;
      th->closing  = false;

      S_StartPolySequence(po);
   }
}

// Linedef Handlers

int EV_DoPolyObjRotate(polyrotdata_t *prdata)
{
   polyobj_t *po;
   polyrotate_t *th;
   int diracc = -1;

   if(!(po = Polyobj_GetForNum(prdata->polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyObjRotate: bad polyobj %d", 
                  prdata->polyObjNum);
      return 0;
   }

   // don't allow line actions to affect bad polyobjects
   if(po->isBad)
      return 0;

   // check for override if this polyobj already has a thinker
   if(po->thinker && !prdata->overRide)
      return 0;

   // create a new thinker
   th = Z_Malloc(sizeof(polyrotate_t), PU_LEVSPEC, NULL);
   th->thinker.function = T_PolyObjRotate;
   P_AddThinker(&th->thinker);
   po->thinker = &th->thinker;

   // set fields
   th->polyObjNum = prdata->polyObjNum;
   
   // use Hexen-style byte angles for speed and distance
   th->speed = (prdata->speed * prdata->direction * BYTEANGLEMUL) >> 3;

   if(prdata->distance == 255)    // 255 means perpetual
      th->distance = -1;
   else if(prdata->distance == 0) // 0 means 360 degrees
      th->distance = ANG360 - 1;
   else
      th->distance = prdata->distance * BYTEANGLEMUL;

   // set polyobject's thrust
   po->thrust = D_abs(th->speed) >> 8;
   if(po->thrust < FRACUNIT)
      po->thrust = FRACUNIT;
   else if(po->thrust > 4*FRACUNIT)
      po->thrust = 4*FRACUNIT;

   S_StartPolySequence(po);

   // apply action to mirroring polyobjects as well
   while((po = Polyobj_GetMirror(po)))
   {
      if(po->isBad)
         break;

      // check for override if this polyobj already has a thinker
      if(po->thinker && !prdata->overRide)
         break;
      
      // create a new thinker
      th = Z_Malloc(sizeof(polyrotate_t), PU_LEVSPEC, NULL);
      th->thinker.function = T_PolyObjRotate;
      P_AddThinker(&th->thinker);
      po->thinker = &th->thinker;
      
      // set fields
      th->polyObjNum = po->id;
      
      // use opposite direction for mirroring polyobjects
      // alternate the direction for each successive mirror
      th->speed = 
         (prdata->speed * diracc * prdata->direction * BYTEANGLEMUL) >> 3;
      diracc = (diracc > 0) ? -1 : 1;
      
      if(prdata->distance == 255)    // 255 means perpetual
         th->distance = -1;
      else if(prdata->distance == 0) // 0 means approx. 360 degrees
         th->distance = ANG360 - 1;
      else
         th->distance = prdata->distance * BYTEANGLEMUL;

      // set polyobject's thrust
      po->thrust = D_abs(th->speed) >> 8;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;

      S_StartPolySequence(po);
   }

   // action was successful
   return 1;
}

int EV_DoPolyObjMove(polymovedata_t *pmdata)
{
   polyobj_t *po;
   polymove_t *th;
   unsigned int angadd = ANG180;

   if(!(po = Polyobj_GetForNum(pmdata->polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyObjMove: bad polyobj %d", 
                  pmdata->polyObjNum);
      return 0;
   }

   // don't allow line actions to affect bad polyobjects
   if(po->isBad)
      return 0;

   // check for override if this polyobj already has a thinker
   if(po->thinker && !pmdata->overRide)
      return 0;

   // create a new thinker
   th = Z_Malloc(sizeof(polymove_t), PU_LEVSPEC, NULL);
   th->thinker.function = T_PolyObjMove;
   P_AddThinker(&th->thinker);
   po->thinker = &th->thinker;

   // set fields
   th->polyObjNum = pmdata->polyObjNum;
   th->distance   = pmdata->distance;
   th->speed      = pmdata->speed;
   th->angle      = (pmdata->angle * BYTEANGLEMUL) >> ANGLETOFINESHIFT;

   // set component speeds
   Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);

   // set polyobject's thrust
   po->thrust = D_abs(th->speed) >> 3;
   if(po->thrust < FRACUNIT)
      po->thrust = FRACUNIT;
   else if(po->thrust > 4*FRACUNIT)
      po->thrust = 4*FRACUNIT;

   S_StartPolySequence(po);

   // apply action to mirroring polyobjects as well
   while((po = Polyobj_GetMirror(po)))
   {
      if(po->isBad)
         break;

      // check for override if this polyobject already has a thinker
      if(po->thinker && !pmdata->overRide)
         break;

      // create a new thinker
      th = Z_Malloc(sizeof(polymove_t), PU_LEVSPEC, NULL);
      th->thinker.function = T_PolyObjMove;
      P_AddThinker(&th->thinker);
      po->thinker = &th->thinker;
      
      // set fields
      th->polyObjNum = po->id;
      th->distance   = pmdata->distance;
      th->speed      = pmdata->speed;
      
      // use opposite angle for every other mirror
      th->angle = ((pmdata->angle * BYTEANGLEMUL) + angadd) >> ANGLETOFINESHIFT;
      angadd = angadd ? 0 : ANG180;
      
      // set component speeds
      Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);
      
      // set polyobject's thrust
      po->thrust = D_abs(th->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;

      S_StartPolySequence(po);
   }

   // action was successful
   return 1;
}

static void Polyobj_doSlideDoor(polyobj_t *po, polydoordata_t *doordata)
{
   polyslidedoor_t *th;
   unsigned int angtemp, angadd = ANG180;

   // allocate and add a new slide door thinker
   th = Z_Malloc(sizeof(polyslidedoor_t), PU_LEVSPEC, NULL);
   th->thinker.function = T_PolyDoorSlide;
   P_AddThinker(&th->thinker);
   
   // point the polyobject to this thinker
   po->thinker = &th->thinker;

   // setup fields of the thinker
   th->polyObjNum = po->id;
   th->closing    = false;
   th->delay      = doordata->delay;
   th->delayCount = 0;
   th->distance   = th->initDistance = doordata->distance;
   th->speed      = th->initSpeed    = doordata->speed;
   
   // haleyjd: do angle reverse calculation in full precision to avoid
   // drift due to ANGLETOFINESHIFT.
   angtemp       = doordata->angle * BYTEANGLEMUL;
   th->angle     = angtemp >> ANGLETOFINESHIFT;
   th->initAngle = th->angle;
   th->revAngle  = (angtemp + ANG180) >> ANGLETOFINESHIFT;
   
   Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);

   // set polyobject's thrust
   po->thrust = D_abs(th->speed) >> 3;
   if(po->thrust < FRACUNIT)
      po->thrust = FRACUNIT;
   else if(po->thrust > 4*FRACUNIT)
      po->thrust = 4*FRACUNIT;

   S_StartPolySequence(po);

   // start action on mirroring polyobjects as well
   while((po = Polyobj_GetMirror(po)))
   {
      // don't allow line actions to affect bad polyobjects;
      // polyobject doors don't allow action overrides
      if(po->isBad || po->thinker)
         break;

      th = Z_Malloc(sizeof(polyslidedoor_t), PU_LEVSPEC, NULL);
      th->thinker.function = T_PolyDoorSlide;
      P_AddThinker(&th->thinker);

      // point the polyobject to this thinker
      po->thinker = &th->thinker;

      th->polyObjNum = po->id;
      th->closing    = false;
      th->delay      = doordata->delay;
      th->delayCount = 0;
      th->distance   = th->initDistance = doordata->distance;
      th->speed      = th->initSpeed    = doordata->speed;
      
      // alternate angle for each successive mirror
      angtemp       = doordata->angle * BYTEANGLEMUL + angadd;
      th->angle     = angtemp >> ANGLETOFINESHIFT;
      th->initAngle = th->angle;
      th->revAngle  = (angtemp + ANG180) >> ANGLETOFINESHIFT;
      angadd = angadd ? 0 : ANG180;

      Polyobj_componentSpeed(th->speed, th->angle, &th->momx, &th->momy);

      // set polyobject's thrust
      po->thrust = D_abs(th->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
      
      S_StartPolySequence(po);
   }
}

static void Polyobj_doSwingDoor(polyobj_t *po, polydoordata_t *doordata)
{
   polyswingdoor_t *th;
   int diracc = -1;

   // allocate and add a new swing door thinker
   th = Z_Malloc(sizeof(polyswingdoor_t), PU_LEVSPEC, NULL);
   th->thinker.function = T_PolyDoorSwing;
   P_AddThinker(&th->thinker);
   
   // point the polyobject to this thinker
   po->thinker = &th->thinker;

   // setup fields of the thinker
   th->polyObjNum   = po->id;
   th->closing      = false;
   th->delay        = doordata->delay;
   th->delayCount   = 0;
   th->distance     = th->initDistance = doordata->distance * BYTEANGLEMUL;
   th->speed        = (doordata->speed * BYTEANGLEMUL) >> 3;
   th->initSpeed    = th->speed;

   // set polyobject's thrust
   po->thrust = D_abs(th->speed) >> 3;
   if(po->thrust < FRACUNIT)
      po->thrust = FRACUNIT;
   else if(po->thrust > 4*FRACUNIT)
      po->thrust = 4*FRACUNIT;
   
   S_StartPolySequence(po);

   // start action on mirroring polyobjects as well
   while((po = Polyobj_GetMirror(po)))
   {
      // don't allow line actions to affect bad polyobjects;
      // polyobject doors don't allow action overrides
      if(po->isBad || po->thinker)
         break;

      th = Z_Malloc(sizeof(polyswingdoor_t), PU_LEVSPEC, NULL);
      th->thinker.function = T_PolyDoorSwing;
      P_AddThinker(&th->thinker);

      // point the polyobject to this thinker
      po->thinker = &th->thinker;

      // setup fields of the thinker
      th->polyObjNum   = po->id;
      th->closing      = false;
      th->delay        = doordata->delay;
      th->delayCount   = 0;
      th->distance     = th->initDistance = doordata->distance * BYTEANGLEMUL;

      // alternate direction with each mirror
      th->speed = (doordata->speed * BYTEANGLEMUL * diracc) >> 3;
      diracc = (diracc > 0) ? -1 : 1;

      th->initSpeed = th->speed;

      // set polyobject's thrust
      po->thrust = D_abs(th->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;

      S_StartPolySequence(po);
   }
}

int EV_DoPolyDoor(polydoordata_t *doordata)
{
   polyobj_t *po;

   if(!(po = Polyobj_GetForNum(doordata->polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyDoor: bad polyobj %d", 
                  doordata->polyObjNum);
      return 0;
   }

   // don't allow line actions to affect bad polyobjects;
   // polyobject doors don't allow action overrides
   if(po->isBad || po->thinker)
      return 0;

   switch(doordata->doorType)
   {
   case POLY_DOOR_SLIDE:
      Polyobj_doSlideDoor(po, doordata);
      break;
   case POLY_DOOR_SWING:
      Polyobj_doSwingDoor(po, doordata);
      break;
   default:
      doom_printf(FC_ERROR "EV_DoPolyDoor: unknown door type %d", 
                  doordata->doorType);
      return 0;
   }

   return 1;
}

// EOF

