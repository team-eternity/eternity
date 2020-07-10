// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// Polyobjects
//
// Movable segs like in Hexen, but more flexible due to application of
// dynamic binary space partitioning theory.
//
// haleyjd 02/16/06
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "cam_sight.h"
#include "i_system.h"
#include "doomstat.h"
#include "d_mod.h"
#include "ev_specials.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_compare.h"
#include "m_collection.h"
#include "m_queue.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_portal.h"
#include "p_portalblockmap.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "p_tick.h"
#include "polyobj.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"
#include "r_dynseg.h"
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
DLListItem<polymaplink_t> **polyblocklinks;


//
// Static Data
//

// Polyobject Blockmap
static polymaplink_t *bmap_freelist; // free list of blockmap links


//
// Static Functions
//

inline static void Polyobj_bboxAdd(fixed_t *bbox, vertex_t *add)
{
   bbox[BOXTOP]    += add->y;
   bbox[BOXBOTTOM] += add->y;
   bbox[BOXLEFT]   += add->x;
   bbox[BOXRIGHT]  += add->x;
}

inline static void Polyobj_bboxSub(fixed_t *bbox, vertex_t *sub)
{
   bbox[BOXTOP]    -= sub->y;
   bbox[BOXBOTTOM] -= sub->y;
   bbox[BOXLEFT]   -= sub->x;
   bbox[BOXRIGHT]  -= sub->x;
}

inline static void Polyobj_vecAdd(vertex_t *dst, vertex_t *add)
{
   dst->x += add->x;
   dst->y += add->y;
   dst->fx = M_FixedToFloat(dst->x);
   dst->fy = M_FixedToFloat(dst->y);
}

inline static void Polyobj_vecSub(vertex_t *dst, vertex_t *sub)
{
   dst->x -= sub->x;
   dst->y -= sub->y;
   dst->fx = M_FixedToFloat(dst->x);
   dst->fy = M_FixedToFloat(dst->y);
}

inline static void Polyobj_vecSub2(vertex_t *dst, vertex_t *v1, vertex_t *v2)
{
   dst->x = v1->x - v2->x;
   dst->y = v1->y - v2->y;
   dst->fx = M_FixedToFloat(dst->x);
   dst->fy = M_FixedToFloat(dst->y);
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
      po->vertices  = erealloctag(vertex_t **, po->vertices,  po->numVerticesAlloc * sizeof(vertex_t *), PU_LEVEL, nullptr);
      po->origVerts = erealloctag(vertex_t *,  po->origVerts, po->numVerticesAlloc * sizeof(vertex_t),   PU_LEVEL, nullptr);
      po->tmpVerts  = erealloctag(vertex_t *,  po->tmpVerts, po->numVerticesAlloc * sizeof(vertex_t),    PU_LEVEL, nullptr);
   }
   po->vertices[po->numVertices] = v;
   v->polyindex = po->numVertices; // mark it for reference to the index.
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
      po->lines = erealloctag(line_t **, po->lines, po->numLinesAlloc * sizeof(line_t *), PU_LEVEL, nullptr);
   }
   po->lines[po->numLines++] = l;

   // add vertices
   Polyobj_addVertex(po, l->v1);
   Polyobj_addVertex(po, l->v2);

   // haleyjd 01/22/11: mark linedef to render only through dynasegs
   l->intflags |= MLI_DYNASEGLINE;
}

// Line-finding functions

//
// Polyobj_findLines
//
// This method adds lines to a polyobject by following lines from vertex to
// vertex.  The process stops when the original starting point is reached
// or if a particular search ends unexpectedly (ie, the polyobject is not
// closed).
//
static void Polyobj_findLines(polyobj_t *po, line_t *line)
{
   int startx, starty;
   int i;

   Polyobj_addLine(po, line);

   // on first line, save the initial vertex as the starting position
   startx = line->v1->x;
   starty = line->v1->y;

   // Loop around, starting a search through the lines array from scratch each
   // time to find the next connecting linedef, until one of two things happens:
   // A. We find a line that ends where the first line began.
   // B. We search through the entire lines array and do not find a line to
   //    continue the search process (this is an error condition).
   do
   {
      // terminal case: we have reached a line where v2 is the same as v1 of the
      // initial line
      if(line->v2->x == startx && line->v2->y == starty)
         return;
      
      // search the lines for one whose starting vertex is equal to the current
      // line's ending vertex.
      for(i = 0; i < numlines; ++i)
      {
         if(lines[i].v1->x == line->v2->x && lines[i].v1->y == line->v2->y)
         {
            Polyobj_addLine(po, &lines[i]); // add the new line
            line = &lines[i];               // set new line as current line
            break;                          // restart the search
         }
      }
   }
   while(i < numlines); // if i >= numlines, an error has occured.

   // Error: if we reach here, the line search never found another line to
   // continue the loop, and thus the polyobject is open. This isn't allowed.
   po->flags |= POF_ISBAD;
   doom_printf(FC_ERROR "Polyobject %d is not closed", po->id);
}

// structure used to store linedefs during explicit search process
struct lineitem_t
{
   line_t *line;
   int   num;
};

//
// Polyobj_lineCompare
//
// Callback for qsort that compares two lineitems.
//
static int Polyobj_lineCompare(const void *s1, const void *s2)
{
   lineitem_t *si1 = (lineitem_t *)s1;
   lineitem_t *si2 = (lineitem_t *)s2;

   return si2->num - si1->num;
}

//
// Polyobj_findExplicit
//
// Searches for lines to put into a polyobject in an explicitly provided order.
//
static void Polyobj_findExplicit(polyobj_t *po)
{
   // temporary dynamic seg array
   lineitem_t *lineitems = nullptr;
   int numLineItems = 0;
   int numLineItemsAlloc = 0;

   int i;

   int explicitspec = EV_SpecialForStaticInit(EV_STATIC_POLYOBJ_EXPLICIT_LINE);
   if(!explicitspec) // special is not defined, cannot spawn polyobjects
   {
      po->flags |= POF_ISBAD;
      return;
   }

   // first loop: save off all segs with polyobject's id number
   for(i = 0; i < numlines; ++i)
   {
      if(lines[i].special == explicitspec && 
         lines[i].args[0] == po->id &&
         lines[i].args[1] > 0)
      {
         if(numLineItems >= numLineItemsAlloc)
         {
            numLineItemsAlloc = numLineItemsAlloc ? numLineItemsAlloc*2 : 4;
            lineitems = erealloc(lineitem_t *, lineitems, numLineItemsAlloc*sizeof(lineitem_t));
         }
         lineitems[numLineItems].line = &lines[i];
         lineitems[numLineItems].num  = lines[i].args[1];
         ++numLineItems;
      }
   }

   // make sure array isn't empty
   if(numLineItems == 0)
   {
      po->flags |= POF_ISBAD;
      doom_printf(FC_ERROR "Polyobject %d is empty", po->id);
      return;
   }

   // sort the array if necessary
   if(numLineItems >= 2)
      qsort(lineitems, numLineItems, sizeof(lineitem_t), Polyobj_lineCompare);

   // second loop: put the sorted segs into the polyobject
   for(i = 0; i < numLineItems; ++i)
      Polyobj_addLine(po, lineitems[i].line);

   // free the temporary array
   efree(lineitems);
}

// Setup functions

//
// Polyobj_spawnPolyObj
//
// Sets up a Polyobject.
//
static void Polyobj_spawnPolyObj(int num, Mobj *spawnSpot, int id)
{
   int i;
   polyobj_t *po = &PolyObjects[num];

   // don't spawn a polyobject more than once
   if(po->numLines)
   {
      doom_printf(FC_ERROR "polyobj %d has more than one spawn spot", po->id);
      return;
   }

   po->id = id;

   // TODO: support customized damage somehow?
   if(spawnSpot->info->doomednum == POLYOBJ_SPAWNCRUSH_DOOMEDNUM)
      po->damage = 3;

   // haleyjd 09/11/09: damage-by-touch polyobjects
   if(spawnSpot->info->doomednum == POLYOBJ_SPAWNDAMAGE_DOOMEDNUM)
   {
      po->damage = 3;
      po->flags |= POF_DAMAGING;
   }

   // set to default thrust; may be modified by attached thinkers
   // TODO: support customized thrust?
   po->thrust = FRACUNIT;

   int startspec = EV_SpecialForStaticInit(EV_STATIC_POLYOBJ_START_LINE);
   if(!startspec) // special is not defined, cannot spawn polyobjects
   {
      po->flags |= POF_ISBAD;
      return;
   }

   // 1. Search lines for "line start" special with tag matching this 
   //    polyobject's id number. If found, iterate through lines which
   //    share common vertices and record them into the polyobject.
   for(i = 0; i < numlines; ++i)
   {
      line_t *line = &lines[i];

      // is it a START line with this polyobject's id?
      if(line->special == startspec && 
         line->args[0] == po->id)
      {
         Polyobj_findLines(po, line);
         po->mirror = line->args[1];
         if(po->mirror == po->id) // do not allow a self-reference
            po->mirror = -1;
         // sound sequence is in args[2]
         po->seqId = line->args[2];
         break;
      }
   }

   // if an error occurred above, quit processing this object
   if(po->flags & POF_ISBAD)
      return;

   // 2. If no such line existed in the first step, look for a line with the 
   //    "explicit" special with tag matching this polyobject's id number. If
   //    found, continue to search for all such lines, storing them in a 
   //    temporary list of lines which is then copied into the polyobject in 
   //    sorted order.
   if(po->numLines == 0)
   {
      Polyobj_findExplicit(po);
      // if an error occurred above, quit processing this object
      if(po->flags & POF_ISBAD)
         return;
      po->mirror = po->lines[0]->args[2];
      if(po->mirror == po->id) // do not allow a self-reference
         po->mirror = -1;
      // sound sequence is in args[3]
      po->seqId = po->lines[0]->args[3];
   }

   // set the polyobject's spawn spot
   po->spawnSpot.x = spawnSpot->x;
   po->spawnSpot.y = spawnSpot->y;

   // 05/25/08: keep track of the spawn spot for now.
   po->spawnSpotMobj = spawnSpot;

   // hash the polyobject by its numeric id
   if(Polyobj_GetForNum(po->id))
   {
      // bad polyobject due to id conflict
      po->flags |= POF_ISBAD;
      doom_printf(FC_ERROR "polyobject id conflict: %d", id);
   }
   else
   {
      int hashkey = po->id % numPolyObjects;
      po->next = PolyObjects[hashkey].first;
      PolyObjects[hashkey].first = num;
   }
}

static void Polyobj_setCenterPt(polyobj_t *po);

//
// Polyobj_collectPortals
//
// ioanch 20160228: Inits the portal collection, if any.
// Adds anchored and linked portals
//
static void Polyobj_collectPortals(polyobj_t *po)
{
   // Collect them in an easy collection
   PODCollection<portal_t *> portals;
   bool hasLinked = false;
   for(int i = 0; i < po->numLines; ++i)
   {
      line_t &line = *po->lines[i];
      portal_t *portal = line.portal;
      if(!portal || !R_portalIsAnchored(portal))
         continue;

      line.intflags |= MLI_MOVINGPORTAL;
      if(line.beyondportalline)
         line.beyondportalline->intflags |= MLI_MOVINGPORTAL;

      for(portal_t *prevPortal : portals)
      {
         if(prevPortal == portal)
            goto nextLine;
      }

      if(line.pflags & PS_PASSABLE && useportalgroups)
         hasLinked = true;

      portals.add(portal);

   nextLine:
      ;
   }

   // copy it into the permanent array
   po->numPortals = portals.getLength();
   po->hasLinkedPortals = hasLinked;
   if(po->numPortals)
   {
      po->portals = emalloctag(decltype(po->portals), 
         po->numPortals * sizeof(*po->portals), PU_LEVEL, nullptr);
      memcpy(po->portals, &portals[0], po->numPortals * sizeof(*po->portals));
   }
}

//
// ioanch 20160226: moves the portals from the polyobject
// The 'cancel' argument sets whether to keep or remove the reference
//
static void Polyobj_moveLinkedPortals(const polyobj_t *po, fixed_t dx, fixed_t dy, bool cancel)
{
   if(!useportalgroups)
      return;
   bool *groupvisit = ecalloc(bool *, P_PortalGroupCount(), sizeof(bool));
   for(size_t i = 0; i < po->numPortals; ++i)
   {
      portal_t *portal = po->portals[i];
      if(portal->type == R_LINKED)
      {
         linkdata_t &ldata = portal->data.link;
         ldata.deltax -= dx;
         ldata.deltay -= dy;
         portal_t *partner = ldata.polyportalpartner;
         if(partner)
         {
            partner->data.link.deltax += dx;
            partner->data.link.deltay += dy;
         }
         // mark the group as being moved by the portal or not.
         P_MoveGroupCluster(ldata.fromid, ldata.toid, groupvisit, dx, dy, true,
            cancel ? nullptr : po);
      }
   }
   efree(groupvisit);
}

//
// Rotates the portals from the poly.
//
static void Polyobj_updateAnchoredPortals(const polyobj_t &po)
{
   for(size_t i = 0; i < po.numPortals; ++i)
   {
      portal_t *portal = po.portals[i];
      if(portal->type != R_ANCHORED && portal->type != R_TWOWAY)
         continue;
      anchordata_t &adata = portal->data.anchor;
      adata.transform.updateFromLines(true);

      portal_t *partner = adata.polyportalpartner;
      if(partner)
         partner->data.anchor.transform.updateFromLines(true);
   }
}

//
// If linked portals exist, updats a line's portalmap position
//
static void Polyobj_relinkLine(const line_t &line)
{
   if(line.portal && line.portal->type == R_LINKED && useportalgroups)
   {
      gPortalBlockmap.unlinkLine(line);
      gPortalBlockmap.linkLine(line);
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
      doom_printf(FC_ERROR "bad polyobject %d for anchor point", anchor->angle);
      return;
   }

   // don't move any bad polyobject that may have gotten through
   if(po->flags & POF_ISBAD)
      return;

   // don't move any polyobject more than once
   if(po->flags & POF_ATTACHED)
   {
      doom_printf(FC_ERROR "polyobj %d has more than one anchor", po->id);
      return;
   }

   sspot.x = po->spawnSpot.x;
   sspot.y = po->spawnSpot.y;

   // calculate distance from anchor to spawn spot
   // ioanch 20151218: use 32-bit coordinates
   dist.x = anchor->x - sspot.x;
   dist.y = anchor->y - sspot.y;

   // update linedef bounding boxes
   for(i = 0; i < po->numLines; ++i)
   {
      Mobj *mo;

      Polyobj_bboxSub(po->lines[i]->bbox, &dist);

      // 05/25/08: must change lines' sector groupids for correct portal
      // automap overlay behavior.
      mo = po->spawnSpotMobj;
      po->lines[i]->frontsector->groupid = mo->subsector->sector->groupid;
      po->lines[i]->soundorg.groupid     = mo->subsector->sector->groupid;
   }

   // ioanch 20160226: update portal position
   Polyobj_collectPortals(po);
   Polyobj_moveLinkedPortals(po, -dist.x, -dist.y, false);

   // translate vertices and record original coordinates relative to spawn spot
   for(i = 0; i < po->numVertices; ++i)
   {
      Polyobj_vecSub(po->vertices[i], &dist);
      po->tmpVerts[i] = *po->vertices[i]; // backup position
      Polyobj_vecSub2(&(po->origVerts[i]), po->vertices[i], &sspot);
   }

   // Update sound origins
   for(i = 0; i < po->numLines; ++i)
   {
      line_t &line = *po->lines[i];
      line.soundorg.x = line.v1->x + line.dx / 2;
      line.soundorg.y = line.v1->y + line.dy / 2;

      Polyobj_relinkLine(line);
   }

   Polyobj_setCenterPt(po);
   R_AttachPolyObject(po);

   Polyobj_updateAnchoredPortals(*po); // finally update the anchored portals.
}

static void Polyobj_setCenterPt(polyobj_t *po)
{
   subsector_t  *ss;
   fixed_t center_x = 0, center_y = 0;
   int i;

   // never attach a bad polyobject
   if(po->flags & POF_ISBAD)
      return;

   for(i = 0; i < po->numVertices; ++i)
   {
      center_x += po->vertices[i]->x / po->numVertices;
      center_y += po->vertices[i]->y / po->numVertices;
   }
   
   po->centerPt.x = center_x;
   po->centerPt.y = center_y;

   ss = R_PointInSubsector(po->centerPt.x, po->centerPt.y);

   // set spawnSpot's groupid for correct portal sound behavior
   po->spawnSpot.groupid = ss->sector->groupid;
}

// Blockmap Functions

//
// Polyobj_getLink
//
// Retrieves a polymaplink object from the free list or creates a new one.
//
static polymaplink_t *Polyobj_getLink(void)
{
   polymaplink_t *link;

   if(bmap_freelist)
   {
      link = bmap_freelist;
      bmap_freelist = link->po_next;
   }
   else
      link = estructalloctag(polymaplink_t, 1, PU_LEVEL);

   return link;
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
// Inserts a polyobject into the polyobject blockmap. Unlike Mobj's,
// polyobjects need to be linked into every blockmap cell which their
// bounding box intersects. This ensures the accurate level of clipping
// which is present with linedefs but absent from most mobj interactions.
//
static void Polyobj_linkToBlockmap(polyobj_t *po)
{
   fixed_t *blockbox = po->blockbox;
   int i, x, y;
   
   // never link a bad polyobject or a polyobject already linked
   if(po->flags & (POF_ISBAD | POF_LINKED))
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
            
            l->link.insert(l, &polyblocklinks[y * bmapwidth + x]);
         }
      }
   }

   po->flags |= POF_LINKED;
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
   if(!(po->flags & POF_LINKED))
      return;

   while(l)
   {
      polymaplink_t *next = l->po_next;
      l->link.remove();
      Polyobj_putLink(l);
      l = next;
   }

   po->linkhead = nullptr;

   po->flags &= ~POF_LINKED;
}


// Movement functions

//
// Polyobj_untouched
//
// A version of Lee's routine from p_maputl.c that accepts an mobj pointer
// argument instead of using tmthing. Returns true if the line isn't contacted
// and false otherwise.
//
inline static bool Polyobj_untouched(const line_t *ld, const Mobj *mo)
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
// About things which can be pushed
//
inline static bool Polyobj_canPushThing(const Mobj &mo)
{
   return mo.flags & MF_SOLID || mo.player;
}

//
// Polyobj_pushThing
//
// Inflicts thrust and possibly damage on a thing which has been found to be
// blocking the motion of a polyobject. The default thrust amount is only one
// unit, but the motion of the polyobject can be used to change this.
//
static void Polyobj_pushThing(polyobj_t *po, line_t *line, Mobj *mo)
{
   angle_t lineangle;
   fixed_t momx, momy;
   
   // calculate angle of line and subtract 90 degrees to get normal
   lineangle = P_PointToAngle(0, 0, line->dx, line->dy) - ANG90;
   lineangle >>= ANGLETOFINESHIFT;
   momx = FixedMul(po->thrust, finecosine[lineangle]);
   momy = FixedMul(po->thrust, finesine[lineangle]);
   mo->momx += momx;
   mo->momy += momy;

   // if object doesn't fit at desired location, possibly hurt it
   if(po->damage && mo->flags & MF_SHOOTABLE)
   {
      if(po->flags & POF_DAMAGING)
         P_DamageMobj(mo, nullptr, nullptr, po->damage, MOD_CRUSH);
      else
      {
         // Temporarily remove from blockmap to avoid this poly's lines from
         // counting as collision lines. Only separate walls should block and 
         // damage the mobj.
         Polyobj_removeFromBlockmap(po);
         if(!P_CheckPosition(mo, mo->x + momx, mo->y + momy))
            P_DamageMobj(mo, nullptr, nullptr, po->damage, MOD_CRUSH);
         Polyobj_linkToBlockmap(po);
      }
   }
}

//
// Polyobj_clipThings
//
// Checks for things that are in the way of a polyobject line move.
// Argument vec is non-null if the polyobject was moved and is used for linked
// portal walls.
// Returns true if something was hit.
//
static bool Polyobj_clipThings(polyobj_t *po, line_t *line, 
   const vertex_t *vec = nullptr)
{
   bool hitthing = false;
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
            Mobj *mo = blocklinks[y * bmapwidth + x];

            // haleyjd 08/14/10: use modification-safe traversal
            while(mo)
            {
               Mobj *next = mo->bnext;

               // always push players even if not solid
               if(Polyobj_canPushThing(*mo) && !Polyobj_untouched(line, mo))
               {
                  // ioanch 20160226: in case of portal lines, just make sure
                  // the mobj budges a bit just to detect the specline
                  if(line->pflags & PS_PASSABLE)
                  {
                     // HACK
                     v2fixed_t pos = { mo->x, mo->y };
                     if(vec)
                     {
                        mo->x += FixedMul(vec->x, 72090);   // FRACUNIT * 1.1
                        mo->y += FixedMul(vec->y, 72090);
                     }
                     if(!P_TryMove(mo, pos.x, pos.y, true))
                     {
                        mo->x = pos.x;
                        mo->y = pos.y;
                        // FIXME: this one needs checking after i figure out
                        // portalmap
                        Polyobj_pushThing(po, line, mo);
                        hitthing = true;
                     }
                  }
                  else
                  {
                     Polyobj_pushThing(po, line, mo);
                     hitthing = true;
                  }
               }
               mo = next; // demo compatibility is not a factor here
            }
         } // end if
      } // end for(x)
   } // end for(y)

   return hitthing;
}

//
// Keeps track of portal-polyobject touched things. If position and velocity don't change, then it
// means the thing may need to be dropped from a departing polyobject
//
struct portalthing_t
{
   Mobj *thing;   // the touched thing
   v2fixed_t position;  // the position when touched
   v2fixed_t velocity;  // the velocity when touched
};

//
// If this is a portal polyobject, collect all things on the edge: they may be dropped after moving.
// Must be called before moving, hence not at the same time as clipThings.
// Line must be PS_PASSABLE.
//
static void Polyobj_collectPortalThings(const polyobj_t &po, const line_t &line,
                                        PODCollection<portalthing_t> &things)
{
   fixed_t linebox[4];
   // adjust linedef bounding box to blockmap, extend by MAXRADIUS
   linebox[BOXLEFT]   = (line.bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   linebox[BOXRIGHT]  = (line.bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   linebox[BOXBOTTOM] = (line.bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   linebox[BOXTOP]    = (line.bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   // check all mobj blockmap cells the line contacts
   for(int y = linebox[BOXBOTTOM]; y <= linebox[BOXTOP]; ++y)
      for(int x = linebox[BOXLEFT]; x <= linebox[BOXRIGHT]; ++x)
      {
         if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
            continue;

         Mobj *next;
         for(Mobj *mo = blocklinks[y * bmapwidth + x]; mo; mo = next)
         {
            next = mo->bnext;
            if(!Polyobj_canPushThing(*mo) || Polyobj_untouched(&line, mo))
               continue;
            portalthing_t &pt = things.addNew();
            pt.thing = mo;
            pt.position = { mo->x, mo->y };
            pt.velocity = { mo->momx, mo->momy };
         }
      }
}

//
// Iterator for the function below
//
static bool PolyobjIT_moveObjectsInside(int groupid, void *context)
{
   int count;
   sector_t **gsectors = P_GetSectorsWithGroupId(groupid, &count);
   auto &moved = *static_cast<PODCollection<Mobj *> *>(context);
   for(int i = 0; i < count; ++i)
   {
      for(Mobj *mo = gsectors[i]->thinglist; mo; mo = mo->snext)
      {
         // NOSECTOR invisible things will be ignored :)
         // Also don't push back standing and hanging things
         if(P_mobjOnSurface(*mo))
            continue;
         moved.add(mo); // don't move them from here, as it may mutate the sector thinglist.
      }
   }
   return true;
}

//
// Moves all airborne objects inside the poly
//
static void Polyobj_moveObjectsInside(const polyobj_t &po, fixed_t dx, fixed_t dy)
{
   if(!useportalgroups)
      return;
   bool *groupvisit = ecalloc(bool *, P_PortalGroupCount(), sizeof(bool));
   PODCollection<Mobj *> moved;
   for(size_t i = 0; i < po.numPortals; ++i)
   {
      const portal_t &portal = *po.portals[i];
      if(portal.type != R_LINKED || groupvisit[portal.data.link.toid])
         continue;
      P_ForEachClusterGroup(portal.data.link.fromid, portal.data.link.toid, groupvisit,
                            PolyobjIT_moveObjectsInside, &moved);
   }
   for(Mobj *mo : moved)
      P_TryMove(mo, mo->x + dx, mo->y + dy, 1);
   efree(groupvisit);
}

//
// Cross any special lines. Uses the centre point as reference, thus allowing
// both moving and rotating polyobjects 
//
static void Polyobj_crossLines(polyobj_t *po, v2fixed_t oldcentre)
{
   if(oldcentre.x == po->centerPt.x && oldcentre.y == po->centerPt.y)
      return;
   CAM_PathTraverse(oldcentre.x, oldcentre.y, po->centerPt.x, po->centerPt.y,
      CAM_ADDLINES, po,
      [](const intercept_t *in, void *data, const divline_t &trace) {

      auto *po = static_cast<polyobj_t *>(data);
      if(in->d.line->special)
      {
         P_CrossSpecialLine(in->d.line,
            P_PointOnLineSidePrecise(trace.x, trace.y, in->d.line), nullptr, po);
      }

      return true;
   });
}

//
// Polyobj_moveXY
//
// Moves a polyobject on the x-y plane.
//
static bool Polyobj_moveXY(polyobj_t *po, fixed_t x, fixed_t y, bool onload = false)
{
   int i;
   vertex_t vec;
   bool hitthing = false;

   vec.x = x;
   vec.y = y;

   // don't move bad polyobjects
   if(po->flags & POF_ISBAD)
      return false;

   PODCollection<portalthing_t> pts;
   if(po->numPortals)
      for(i = 0; i < po->numLines; ++i)
         if(po->lines[i]->pflags & PS_PASSABLE)
            Polyobj_collectPortalThings(*po, *po->lines[i], pts);

   // ioanch 20160226: update portal position
   Polyobj_moveLinkedPortals(po, x, y, false);

   // translate vertices
   for(i = 0; i < po->numVertices; ++i)
   {
      if(!onload)
         po->tmpVerts[i] = *po->vertices[i];
      Polyobj_vecAdd(po->vertices[i], &vec);
      if(onload)
         po->tmpVerts[i] = *po->vertices[i];
   }

   // translate each line
   for(i = 0; i < po->numLines; ++i)
   {
      Polyobj_bboxAdd(po->lines[i]->bbox, &vec);
      Polyobj_relinkLine(*po->lines[i]);
   }

   // check for blocking things (yes, it needs to be done separately)
   // ioanch 20160302: do NOT collide and get back if onload = true.
   if(!onload)
      for(i = 0; i < po->numLines; ++i)
         hitthing |= Polyobj_clipThings(po, po->lines[i], &vec);

   if(hitthing)
   {
      // reset vertices
      for(i = 0; i < po->numVertices; ++i)
         Polyobj_vecSub(po->vertices[i], &vec);
      
      // reset lines that have been moved
      for(i = 0; i < po->numLines; ++i)
      {
         Polyobj_bboxSub(po->lines[i]->bbox, &vec);
         Polyobj_relinkLine(*po->lines[i]);
      }

      // ioanch 20160226: update portal position
      // CAREFUL: do not replace this and the previous call to a single call,
      // because there's a lot of stuff going on in Polyobj_clipThings (e.g. things eaten by
      // portals). Heavy testing needs to be done if you do so.
      Polyobj_moveLinkedPortals(po, -x, -y, true);
   }
   else
   {
      // translate the spawnSpot as well
      po->spawnSpot.x += vec.x;
      po->spawnSpot.y += vec.y;          
      
      // 04/19/09: translate sound origins
      for(i = 0; i < po->numLines; ++i)
      {
         po->lines[i]->soundorg.x += vec.x;
         po->lines[i]->soundorg.y += vec.y;
      }

      Polyobj_removeFromBlockmap(po); // unlink it from the blockmap
      R_DetachPolyObject(po);
      Polyobj_linkToBlockmap(po);     // relink to blockmap
      v2fixed_t oldcentre = { po->centerPt.x, po->centerPt.y };
      Polyobj_setCenterPt(po);
      if(!onload)
         Polyobj_crossLines(po, oldcentre);
      R_AttachPolyObject(po);

      Polyobj_updateAnchoredPortals(*po);

      for(const portalthing_t &pt : pts)
      {
         // Object was neither teleported by portal nor pushed by solid wall
         if(pt.thing->x == pt.position.x && pt.thing->y == pt.position.y &&
            pt.thing->momx == pt.velocity.x && pt.thing->momy == pt.velocity.y)
         {
            // We got one which we may want to move
            if(P_mobjOnSurface(*pt.thing))
            {
               if(!P_TryMove(pt.thing, pt.thing->x + x, pt.thing->y + y, 1))
                  pt.thing->zref = clip.zref;   // If couldn't move, still adjust Z references
            }
            else
            {
               // Floating things still need zref updating
               P_CheckPosition(pt.thing, pt.thing->x, pt.thing->y);
               pt.thing->zref = clip.zref;
            }
         }
      }
      // Now move the airborne things inside the polyobject portal, except for the ceiling hangers
      Polyobj_moveObjectsInside(*po, -x, -y);
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
inline static void Polyobj_rotatePoint(vertex_t &v, v2fixed_t c, int ang)
{
   const v2fixed_t tmp = { v.x, v.y };

   v.x = FixedMul(tmp.x, finecosine[ang]) - FixedMul(tmp.y,   finesine[ang]);
   v.y = FixedMul(tmp.x,   finesine[ang]) + FixedMul(tmp.y, finecosine[ang]);

   v.x += c.x;
   v.y += c.y;

   v.fx = M_FixedToFloat(v.x);
   v.fy = M_FixedToFloat(v.y);
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

   // 04/19/09: reposition sound origin
   ld->soundorg.x = v1->x + ld->dx / 2;
   ld->soundorg.y = v1->y + ld->dy / 2;

   // Also update the normals if necessary
   if(ld->portal && R_portalIsAnchored(ld->portal))
      P_MakeLineNormal(ld);
}

//
// Polyobj_rotate
//
// Rotates a polyobject around its start point.
//
static bool Polyobj_rotate(polyobj_t *po, angle_t delta, bool onload = false)
{
   int i, angle;
   v2fixed_t origin;
   bool hitthing = false;

   // don't move bad polyobjects
   if(po->flags & POF_ISBAD)
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

      Polyobj_rotatePoint(*po->vertices[i], origin, angle);
   }

   // rotate lines
   for(i = 0; i < po->numLines; ++i)
   {
      Polyobj_rotateLine(po->lines[i]);
      Polyobj_relinkLine(*po->lines[i]);
   }

   // check for blocking things
   // ioanch 20160302: do NOT collide if onload = true.
   if(!onload)
      for(i = 0; i < po->numLines; ++i)
         hitthing |= Polyobj_clipThings(po, po->lines[i]);

   if(hitthing)
   {
      // reset vertices to previous positions
      for(i = 0; i < po->numVertices; ++i)
         *(po->vertices[i]) = po->tmpVerts[i];

      // reset lines
      for(i = 0; i < po->numLines; ++i)
      {
         Polyobj_rotateLine(po->lines[i]);
         Polyobj_relinkLine(*po->lines[i]);
      }
   }
   else
   {
      // update polyobject's angle
      po->angle += delta;

      Polyobj_removeFromBlockmap(po); // unlink it from the blockmap
      R_DetachPolyObject(po);
      Polyobj_linkToBlockmap(po);     // relink to blockmap
      v2fixed_t oldcentre = { po->centerPt.x, po->centerPt.y };
      Polyobj_setCenterPt(po);
      if(!onload)
         Polyobj_crossLines(po, oldcentre);
      R_AttachPolyObject(po);

      Polyobj_updateAnchoredPortals(*po);
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
// Returns nullptr if no such polyobject exists.
//
polyobj_t *Polyobj_GetForNum(int id)
{
   int curidx;

   // haleyjd 01/07/07: must check if == 0 first!
   if(numPolyObjects == 0)
      return nullptr;
   
   curidx = PolyObjects[id % numPolyObjects].first;

   while(curidx != numPolyObjects && PolyObjects[curidx].id != id)
      curidx = PolyObjects[curidx].next;

   return curidx == numPolyObjects ? nullptr : &PolyObjects[curidx];
}

//
// Polyobj_GetMirror
//
// Retrieves the mirroring polyobject if one exists. Returns nullptr
// otherwise.
//
static polyobj_t *Polyobj_GetMirror(polyobj_t *po)
{
   return (po && po->mirror != -1) ? Polyobj_GetForNum(po->mirror) : nullptr;
}

// structure used to queue up mobj pointers in Polyobj_InitLevel
struct mobjqitem_t
{
   mqueueitem_t mqitem;
   Mobj *mo;
};

//
// Check if mobj is a polyobject spawn spot
//
bool Polyobj_IsSpawnSpot(const Mobj &mo)
{
   return mo.info->doomednum == POLYOBJ_SPAWN_DOOMEDNUM ||
      mo.info->doomednum == POLYOBJ_SPAWNCRUSH_DOOMEDNUM ||
      mo.info->doomednum == POLYOBJ_SPAWNDAMAGE_DOOMEDNUM;
}

//
// Polyobj_InitLevel
//
// Called at the beginning of each map after all other line and thing
// processing is finished.
//
void Polyobj_InitLevel(void)
{
   Thinker   *th;
   mqueue_t    spawnqueue;
   mqueue_t    anchorqueue;
   mobjqitem_t *qitem;
   int i, numAnchors = 0;

   M_QueueInit(&spawnqueue);
   M_QueueInit(&anchorqueue);

   // get rid of values from previous level
   // note: as with msecnodes, it is very important to clear out the blockmap
   // node freelist, otherwise it may contain dangling pointers to old objects
   PolyObjects    = nullptr;
   numPolyObjects = 0;
   bmap_freelist  = nullptr;

   // run down the thinker list, count the number of spawn points, and save
   // the Mobj pointers on a queue for use below.
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;
      if((mo = thinker_cast<Mobj *>(th)))
      {
         if(Polyobj_IsSpawnSpot(*mo))
         {
            ++numPolyObjects;
            
            qitem = ecalloc(mobjqitem_t *, 1, sizeof(mobjqitem_t));
            qitem->mo = mo;
            M_QueueInsert(&(qitem->mqitem), &spawnqueue);
         }
         else if(mo->info->doomednum == POLYOBJ_ANCHOR_DOOMEDNUM)
         {
            ++numAnchors;

            qitem = ecalloc(mobjqitem_t *, 1, sizeof(mobjqitem_t));
            qitem->mo = mo;
            M_QueueInsert(&(qitem->mqitem), &anchorqueue);
         }
      }
   }

   if(numPolyObjects)
   {
      // allocate the PolyObjects array
      PolyObjects = estructalloctag(polyobj_t, numPolyObjects, PU_LEVEL);

      // CPP_FIXME: temporary in-place construction of origin
      for(i = 0; i < numPolyObjects; ++i)
         ::new (&(PolyObjects[i].spawnSpot)) PointThinker;

      // setup hash fields
      for(i = 0; i < numPolyObjects; ++i)
         PolyObjects[i].first = PolyObjects[i].next = numPolyObjects;

      // setup polyobjects
      for(i = 0; i < numPolyObjects; ++i)
      {
         qitem = (mobjqitem_t *)M_QueueIterator(&spawnqueue);
         
         Polyobj_spawnPolyObj(i, qitem->mo, qitem->mo->spawnpoint.angle);
      }

      // Used to get portal clusters when moving polyobjects.
      P_MarkPortalClusters();

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
   
   // ioanch 20160302: do NOT collide and get back if onload = true.

   // first, rotate to the saved angle
   // ioanch 20160310: loadgame fix: don't budge polyobjects ever so little
   // if they haven't rotated anyway. Angle 0 still means some error.
   if(angle)
      Polyobj_rotate(po, angle, true);
   
   // determine component distances to translate
   dx = x - po->spawnSpot.x;
   dy = y - po->spawnSpot.y;

   // translate
   Polyobj_moveXY(po, dx, dy, true);
}

// Thinker Functions

IMPLEMENT_THINKER_TYPE(PolyRotateThinker)

//
// T_PolyObjRotate
//
// Thinker function for PolyObject rotation.
//
void PolyRotateThinker::Think()
{
   polyobj_t *po = Polyobj_GetForNum(this->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyObjRotate: thinker has invalid id %d\n", this->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == nullptr)
   {
      po->thinker = this;
      
      // reset polyobject's thrust
      po->thrust = D_abs(this->speed) >> 8;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // rotate by 'speed' angle per frame
   // if distance == -1, this polyobject rotates perpetually
   if(Polyobj_rotate(po, this->speed) && this->distance != -1)
   {
      int avel = D_abs(this->speed);

      // decrement distance by the amount it moved
      this->distance -= avel;

      // MaxW: 20160106: set the flag which differentiates angles >= 180 from angles < 0
      hasBeenPositive = hasBeenPositive || (distance > 0);

      // are we at or past the destination?
      if(this->distance <= 0 && this->hasBeenPositive)
      {
         // remove thinker
         if(po->thinker == this)
         {
            po->thinker = nullptr;
            po->thrust = FRACUNIT;
         }
         this->remove();

         // TODO: notify scripts
         S_StopPolySequence(po);
      }
      else if(this->distance < avel && this->distance > 0)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         this->speed = this->speed >= 0 ? this->distance : -this->distance;
      }
   }
}

//
// PolyRotateThinker::serialize
//
// Saves/loads a PolyRotateThinker thinker.
//
void PolyRotateThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << polyObjNum << speed << distance << hasBeenPositive;
   // ioanch 20160310: fix the thinker reference
   if(arc.isLoading())
      Polyobj_GetForNum(polyObjNum)->thinker = this;
}

//
// Polyobj_componentSpeed
//
// Calculates the speed components from the desired resultant velocity.
//
inline static void Polyobj_componentSpeed(int resVel, int angle, 
                                          int *xVel, int *yVel)
{
   *xVel = FixedMul(resVel, finecosine[angle]);
   *yVel = FixedMul(resVel,   finesine[angle]);
}

IMPLEMENT_THINKER_TYPE(PolyMoveThinker)

void PolyMoveThinker::Think()
{
   polyobj_t *po = Polyobj_GetForNum(this->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyObjRotate: thinker has invalid id %d\n", this->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == nullptr)
   {
      po->thinker = this;
      
      // reset polyobject's thrust
      po->thrust = D_abs(this->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // move the polyobject one step along its movement angle
   if(Polyobj_moveXY(po, this->momx, this->momy))
   {
      int avel = D_abs(this->speed);

      // decrement distance by the amount it moved
      this->distance -= avel;

      // are we at or past the destination?
      if(this->distance <= 0)
      {
         // remove thinker
         if(po->thinker == this)
         {
            po->thinker = nullptr;
            po->thrust = FRACUNIT;
         }
         this->remove();

         // TODO: notify scripts
         S_StopPolySequence(po);
      }
      else if(this->distance < avel)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         this->speed = this->speed >= 0 ? this->distance : -this->distance;
         Polyobj_componentSpeed(this->speed, this->angle, &this->momx, &this->momy);
      }
   }
}

//
// PolyMoveThinker::serialize
//
// Saves/loads a PolyMoveThinker thinker.
//
void PolyMoveThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << polyObjNum << speed << momx << momy << distance << angle;

   // ioanch 20160310: fix the thinker reference
   if(arc.isLoading())
      Polyobj_GetForNum(polyObjNum)->thinker = this;
}

IMPLEMENT_THINKER_TYPE(PolyMoveXYThinker)

//
// Thinker stuff
//
void PolyMoveXYThinker::Think()
{
   polyobj_t *po = Polyobj_GetForNum(this->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyObjRotate: thinker has invalid id %d\n", this->polyObjNum);
#endif

   if(!po->thinker)
   {
      po->thinker = this;
      po->thrust = eclamp(D_abs(speed) >> 3, FRACUNIT, 4 * FRACUNIT);
   }

   if(Polyobj_moveXY(po, velocity.x, velocity.y))
   {
      v2fixed_t avel = velocity.abs();
      distance -= avel;
      if(distance.x <= 0 && distance.y <= 0)
      {
         if(po->thinker == this)
         {
            po->thinker = nullptr;
            po->thrust = FRACUNIT;
         }
         remove();

         S_StopPolySequence(po);
      }
      else
      {
         if(distance.x > 0 && distance.x < avel.x)
            velocity.x = velocity.x < 0 ? -distance.x : distance.x;
         else if(distance.x <= 0)
            velocity.x = 0;
         if(distance.y > 0 && distance.y < avel.y)
            velocity.y = velocity.y < 0 ? -distance.y : distance.y;
         else if(distance.y <= 0)
            velocity.y = 0;
      }
   }
}

//
// Saves/loads a polyobject thinker of movement XY
//
void PolyMoveXYThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << polyObjNum << speed << velocity << distance;

   // ioanch 20160310: fix the thinker reference
   if(arc.isLoading())
      Polyobj_GetForNum(polyObjNum)->thinker = this;
}

IMPLEMENT_THINKER_TYPE(PolySlideDoorThinker)

void PolySlideDoorThinker::Think()
{
   polyobj_t *po = Polyobj_GetForNum(this->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyDoorSlide: thinker has invalid id %d\n", this->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == nullptr)
   {
      po->thinker = this;
      
      // reset polyobject's thrust
      po->thrust = D_abs(this->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // count down wait period
   if(this->delayCount)
   {
      if(--this->delayCount == 0)
         S_StartPolySequence(po);
      return;
   }

      // move the polyobject one step along its movement angle
   if(Polyobj_moveXY(po, this->momx, this->momy))
   {
      int avel = D_abs(this->speed);
      
      // decrement distance by the amount it moved
      this->distance -= avel;
      
      // are we at or past the destination?
      if(this->distance <= 0)
      {
         // does it need to close?
         if(!this->closing)
         {
            this->closing = true;
            
            // reset distance and speed
            this->distance = this->initDistance;
            this->speed    = this->initSpeed;
            
            // start delay
            this->delayCount = this->delay;
            
            // reverse angle
            this->angle = this->revAngle;
            
            // reset component speeds
            Polyobj_componentSpeed(this->speed, this->angle, &this->momx, &this->momy);
         } 
         else
         {
            // remove thinker
            if(po->thinker == this)
            {
               po->thinker = nullptr;
               po->thrust = FRACUNIT;
            }
            this->remove();
            // TODO: notify scripts
         }
         S_StopPolySequence(po);
      }
      else if(this->distance < avel)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         this->speed = this->speed >= 0 ? this->distance : -this->distance;
         Polyobj_componentSpeed(this->speed, this->angle, &this->momx, &this->momy);
      }
   }
   else if(this->closing && this->distance != this->initDistance)
   {
      // move was blocked, special handling required -- make it reopen
      this->distance = this->initDistance - this->distance;
      this->speed    = this->initSpeed;
      this->angle    = this->initAngle;
      Polyobj_componentSpeed(this->speed, this->angle, &this->momx, &this->momy);
      this->closing  = false;
      S_StartPolySequence(po);
   }
}

//
// PolySlideDoorThinker::serialize
//
// Saves/loads a PolySlideDoorThinker thinker.
//
void PolySlideDoorThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << polyObjNum << delay << delayCount << initSpeed << speed
       << initDistance << distance << initAngle << angle << revAngle
       << momx << momy << closing;
   // ioanch 20160310: fix the thinker reference
   if(arc.isLoading())
      Polyobj_GetForNum(polyObjNum)->thinker = this;
}


IMPLEMENT_THINKER_TYPE(PolySwingDoorThinker)

void PolySwingDoorThinker::Think()
{
   polyobj_t *po = Polyobj_GetForNum(this->polyObjNum);

#ifdef RANGECHECK
   if(!po)
      I_Error("T_PolyDoorSwing: thinker has invalid id %d\n", this->polyObjNum);
#endif

   // check for displacement due to override and reattach when possible
   if(po->thinker == nullptr)
   {
      po->thinker = this;
      
      // reset polyobject's thrust
      po->thrust = D_abs(this->speed) >> 3;
      if(po->thrust < FRACUNIT)
         po->thrust = FRACUNIT;
      else if(po->thrust > 4*FRACUNIT)
         po->thrust = 4*FRACUNIT;
   }

   // count down wait period
   if(this->delayCount)
   {
      if(--this->delayCount == 0)
         S_StartPolySequence(po);
      return;
   }

   // rotate by 'speed' angle per frame
   // if distance == -1, this polyobject rotates perpetually
   if(Polyobj_rotate(po, this->speed) && this->distance != -1)
   {
      int avel = D_abs(this->speed);

      // decrement distance by the amount it moved
      this->distance -= avel;

      // MaxW: 20160109: set the flag which differentiates angles >= 180 from angles < 0
      hasBeenPositive = hasBeenPositive || (distance > 0);

      // are we at or past the destination?
      if(this->distance <= 0 && this->hasBeenPositive)
      {
         // does it need to close?
         if(!this->closing)
         {
            this->closing = true;
            
            // reset distance and speed
            this->distance =  this->initDistance;
            this->speed    = -this->initSpeed; // reverse speed on close
            
            // start delay
            this->delayCount = this->delay;    

            this->hasBeenPositive = this->distance < 0 ? false : true;
         } 
         else
         {
            // remove thinker
            if(po->thinker == this)
            {
               po->thinker = nullptr;
               po->thrust = FRACUNIT;
            }
            this->remove();
            // TODO: notify scripts
         }
         S_StopPolySequence(po);
      }
      else if (this->distance < avel && this->distance > 0)
      {
         // we have less than one multiple of 'speed' left to go,
         // so change the speed so that it doesn't pass the destination
         this->speed = this->speed >= 0 ? this->distance : -this->distance;
      }
   }
   else if(this->closing && this->distance != this->initDistance)
   {
      // move was blocked, special handling required -- make it reopen

      this->distance = this->initDistance - this->distance;
      this->hasBeenPositive = this->distance < 0 ? false : true;
      this->speed    = this->initSpeed;
      this->closing  = false;

      S_StartPolySequence(po);
   }
}

//
// PolySwingDoorThinker::serialize
//
// Saves/loads a PolySwingDoorThinker thinker.
//
void PolySwingDoorThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << polyObjNum << delay << delayCount << initSpeed << speed
       << initDistance << distance << closing << hasBeenPositive;
   // ioanch 20160310: fix the thinker reference
   if(arc.isLoading())
      Polyobj_GetForNum(polyObjNum)->thinker = this;
}

// Linedef Handlers

int EV_DoPolyObjRotate(const polyrotdata_t *prdata)
{
   polyobj_t *po;
   PolyRotateThinker *th;
   int diracc = -1;

   if(!(po = Polyobj_GetForNum(prdata->polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyObjRotate: bad polyobj %d", 
                  prdata->polyObjNum);
      return 0;
   }

   // don't allow line actions to affect bad polyobjects
   if(po->flags & POF_ISBAD)
      return 0;

   // check for override if this polyobj already has a thinker
   if(po->thinker && !prdata->overRide)
      return 0;

   // create a new thinker
   th = new PolyRotateThinker;
   th->addThinker();
   po->thinker = th;

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

   // MaxW: 20160106: Initialise flag which differentiates angles >= 180 from angles < 0
   th->hasBeenPositive = th->distance < 0 ? false : true;
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
      if(po->flags & POF_ISBAD)
         break;

      // check for override if this polyobj already has a thinker
      if(po->thinker && !prdata->overRide)
         break;
      
      // create a new thinker
      th = new PolyRotateThinker;
      th->addThinker();
      po->thinker = th;
      
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

      // MaxW: 20160106: Initialise flag which differentiates angles >= 180 from angles < 0
      th->hasBeenPositive = th->distance < 0 ? false : true;
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

int EV_DoPolyObjMove(const polymovedata_t *pmdata)
{
   polyobj_t *po;
   PolyMoveThinker *th;
   unsigned int angadd = ANG180;

   if(!(po = Polyobj_GetForNum(pmdata->polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyObjMove: bad polyobj %d", 
                  pmdata->polyObjNum);
      return 0;
   }

   // don't allow line actions to affect bad polyobjects
   if(po->flags & POF_ISBAD)
      return 0;

   // check for override if this polyobj already has a thinker
   if(po->thinker && !pmdata->overRide)
      return 0;

   // create a new thinker
   th = new PolyMoveThinker;
   th->addThinker();
   po->thinker = th;

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
      if(po->flags & POF_ISBAD)
         break;

      // check for override if this polyobject already has a thinker
      if(po->thinker && !pmdata->overRide)
         break;

      // create a new thinker
      th = new PolyMoveThinker;
      th->addThinker();
      po->thinker = th;
      
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

int EV_DoPolyObjStop(int polyObjNum)
{
   polyobj_t *po;
   if(!(po = Polyobj_GetForNum(polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyObjStop: bad polyobj %d", polyObjNum);
      return 0;
   }

   // don't allow line actions to affect bad polyobjects
   if(po->flags & POF_ISBAD)
      return 0;

   // don't remove thinker if there is no thinker, but do successfully activate
   if(po->thinker)
   {
      po->thinker->remove();
      po->thinker = nullptr;
      S_StopPolySequence(po);
   }

   return 1;
}

//
// Polyobj_MoveTo or Polyobj_MoveToSpot.
//
int EV_DoPolyObjMoveToSpot(const polymoveto_t &pmdata)
{
   polyobj_t *po;
   if(!(po = Polyobj_GetForNum(pmdata.polyObjNum)))
   {
      doom_printf(FC_ERROR "EV_DoPolyObjMoveToSpot: bad polyobj %d", pmdata.polyObjNum);
      return 0;
   }
   if(po->flags & POF_ISBAD || (po->thinker && !pmdata.overRide))
      return 0;
   

   v2fixed_t distance;
   if(pmdata.targetMobj)
   {
      const Mobj *mobj = P_FindMobjFromTID(pmdata.tid, nullptr, pmdata.activator);
      if(!mobj)
         return 0;
      distance.x = mobj->x;
      distance.y = mobj->y;
   }
   else
      distance = pmdata.pos;

   auto th = new PolyMoveXYThinker;
   th->addThinker();
   po->thinker = th;

   distance -= po->spawnSpot;

   th->polyObjNum = pmdata.polyObjNum;
   th->distance = distance.abs();
   th->speed = pmdata.speed;

   Polyobj_componentSpeed(th->speed, 
      P_PointToAngle(0, 0, distance.x, distance.y) >> ANGLETOFINESHIFT, &th->velocity.x, 
      &th->velocity.y);

   po->thrust = eclamp(D_abs(th->speed) >> 3, FRACUNIT, 4 * FRACUNIT);

   S_StartPolySequence(po);

   unsigned mirrorfineangle = P_PointToAngle(0, 0, -distance.x, -distance.y) >> ANGLETOFINESHIFT;
   while((po = Polyobj_GetMirror(po)))
   {
      if(po->flags & POF_ISBAD || (po->thinker && !pmdata.overRide))
         break;

      th = new PolyMoveXYThinker;
      th->addThinker();
      po->thinker = th;

      th->polyObjNum = po->id;
      th->distance = distance.abs();  // mirror vector
      th->speed = pmdata.speed;

      Polyobj_componentSpeed(th->speed, mirrorfineangle, &th->velocity.x, &th->velocity.y);

      po->thrust = eclamp(D_abs(th->speed) >> 3, FRACUNIT, 4 * FRACUNIT);

      S_StartPolySequence(po);
   }

   return 1;
}

static void Polyobj_doSlideDoor(polyobj_t *po, const polydoordata_t *doordata)
{
   PolySlideDoorThinker *th;
   unsigned int angtemp, angadd = ANG180;

   // allocate and add a new slide door thinker
   th = new PolySlideDoorThinker;
   th->addThinker();
   
   // point the polyobject to this thinker
   po->thinker = th;

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
      if((po->flags & POF_ISBAD) || po->thinker)
         break;

      th = new PolySlideDoorThinker;
      th->addThinker();

      // point the polyobject to this thinker
      po->thinker = th;

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

static void Polyobj_doSwingDoor(polyobj_t *po, const polydoordata_t *doordata)
{
   PolySwingDoorThinker *th;
   int diracc = -1;

   // allocate and add a new swing door thinker
   th = new PolySwingDoorThinker;
   th->addThinker();
   
   // point the polyobject to this thinker
   po->thinker = th;

   // setup fields of the thinker
   th->polyObjNum   = po->id;
   th->closing      = false;
   th->delay        = doordata->delay;
   th->delayCount   = 0;
   th->distance     = th->initDistance = doordata->distance * BYTEANGLEMUL;
   th->speed        = (doordata->speed * BYTEANGLEMUL) >> 3;
   th->initSpeed    = th->speed;
   // MaxW: 20160109: Initialise flag which differentiates angles >= 180 from angles < 0
   th->hasBeenPositive = th->distance < 0 ? false : true;

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
      if((po->flags & POF_ISBAD) || po->thinker)
         break;

      th = new PolySwingDoorThinker;
      th->addThinker();

      // point the polyobject to this thinker
      po->thinker = th;

      // setup fields of the thinker
      th->polyObjNum   = po->id;
      th->closing      = false;
      th->delay        = doordata->delay;
      th->delayCount   = 0;
      th->distance     = th->initDistance = doordata->distance * BYTEANGLEMUL;
      // MaxW: 20160109: Initialise flag which differentiates angles >= 180 from angles < 0
      th->hasBeenPositive = th->distance < 0 ? false : true;

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

int EV_DoPolyDoor(const polydoordata_t *doordata)
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
   if((po->flags & POF_ISBAD) || po->thinker)
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

