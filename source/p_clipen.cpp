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
#include "p_doomclip.h"

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


// sf: fix annoying crash on restarting levels
//
//      This crash occurred because the msecnode_t's are allocated as
//      PU_LEVEL. This meant that these were freed whenever a new level
//      was loaded or the level restarted. However, the actual list was
//      not emptied. As a result, msecnode_t's were being used that had
//      been freed back to the zone memory. I really do not understand
//      why boom or MBF never got this bug- or am I missing something?
//
//      additional comment 5/7/99
//      There _is_ code to free the list in g_game.c. But this is called
//      too late: some msecnode_t's are used during the loading of the
//      level. 
//
// haleyjd 05/11/09: This in fact was never a problem. Clear the pointer
// before or after Z_FreeTags; it doesn't matter as long as you clear it
// at some point in or before P_SetupLevel and before we start attaching
// Mobj's to sectors. I don't know what the hubbub above is about, but
// I almost copied this change into WinMBF unnecessarily. I will leave it
// be here, because as I said, changing it has no effect whatsoever.

//
// freeSecNodeList
//
void ClipEngine::freeSecNodeList(void)
{
   headsecnode = NULL; // this is all thats needed to fix the bug
}


// ----------------------------------------------------------------------------
// Tracer engine selection

static DoomTraceEngine doomte = new DoomTraceEngine();

TracerEngine *trace = &doomte;



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
         trace = &doomte;
         break;
   };
}


