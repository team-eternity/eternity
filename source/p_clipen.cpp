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
//      Clipping engine
//
//-----------------------------------------------------------------------------


#include "p_mobj.h"
#include "p_clipen.h"
#include "p_doomen.h"

#include "m_bbox.h"
#include "r_defs.h"



// ----------------------------------------------------------------------------
// ClipContext
ClipContext::ClipContext() : spechit()
{
}




// ----------------------------------------------------------------------------
// ClipEngine

msecnode_t* ClipEngine::headsecnode = NULL;


msecnode_t* ClipEngine::getSecnode()
{
   msecnode_t *node;
   
   return headsecnode ?
      node = headsecnode, headsecnode = node->m_snext, node :
      (msecnode_t *)(Z_Malloc(sizeof *node, PU_LEVEL, NULL)); 
}



void ClipEngine::putSecnode(msecnode_t *node)
{
   node->m_snext = headsecnode;
   headsecnode = node;
}



msecnode_t* ClipEngine::addSecnode(sector_t *s, Mobj *thing, msecnode_t *nextnode)
{
   msecnode_t *node;
   
   for(node = nextnode; node; node = node->m_tnext)
   {
      if(node->m_sector == s)   // Already have a node for this sector?
      {
         node->m_thing = thing; // Yes. Setting m_thing says 'keep it'.
         return nextnode;
      }
   }

   // Couldn't find an existing node for this sector. Add one at the head
   // of the list.
   
   node = getSecnode();
   
   node->visited = 0;  // killough 4/4/98, 4/7/98: mark new nodes unvisited.

   node->m_sector = s;         // sector
   node->m_thing  = thing;     // mobj
   node->m_tprev  = NULL;      // prev node on Thing thread
   node->m_tnext  = nextnode;  // next node on Thing thread
   
   if(nextnode)
      nextnode->m_tprev = node; // set back link on Thing

   // Add new node at head of sector thread starting at s->touching_thinglist
   
   node->m_sprev  = NULL;    // prev node on sector thread
   node->m_snext  = s->touching_thinglist; // next node on sector thread
   
   if(s->touching_thinglist)
      node->m_snext->m_sprev = node;
      
   return s->touching_thinglist = node;
}



msecnode_t* ClipEngine::delSecnode(msecnode_t *node)
{
   if(node)
   {
      msecnode_t *tp = node->m_tprev;  // prev node on thing thread
      msecnode_t *tn = node->m_tnext;  // next node on thing thread
      msecnode_t *sp;  // prev node on sector thread
      msecnode_t *sn;  // next node on sector thread

      // Unlink from the Thing thread. The Thing thread begins at
      // sector_list and not from Mobj->touching_sectorlist.

      if(tp)
         tp->m_tnext = tn;
      
      if(tn)
         tn->m_tprev = tp;

      // Unlink from the sector thread. This thread begins at
      // sector_t->touching_thinglist.
      
      sp = node->m_sprev;
      sn = node->m_snext;
      
      if(sp)
         sp->m_snext = sn;
      else
         node->m_sector->touching_thinglist = sn;
      
      if(sn)
         sn->m_sprev = sp;

      // Return this node to the freelist
      putSecnode(node);
      
      node = tn;
   }
   return node;
}



// ----------------------------------------------------------------------------
// Clipping engine selection

static DoomClipEngine doomen = new DoomClipEngine();


ClipEngine *clip = &doomen;

void P_SetClippingEngine(DoomClipper_e engine)
{
   switch(engine)
   {
      case Doom:
      case Doom3D:
      case Portal:
         clip = &doomen;
         break;
   };
}