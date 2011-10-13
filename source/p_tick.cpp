// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
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
//      Thinker, Ticker.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "c_runcmd.h"
#include "c_io.h"
#include "d_main.h"
#include "e_things.h" // [CG] 09/18/11
#include "p_user.h"
#include "p_chase.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_anim.h"  // haleyjd
#include "p_partcl.h"
#include "polyobj.h"
#include "s_sndseq.h"

#include "cs_main.h"     // [CG] 09/18/11
#include "cs_position.h" // [CG] 09/18/11
#include "cl_cmd.h"     // [CG] 09/18/11
#include "cl_main.h"     // [CG] 09/18/11
#include "cl_pred.h"     // [CG] 09/18/11
#include "sv_main.h"     // [CG] 09/18/11

int leveltime;
bool reset_viewz;

//=============================================================================
//
// THINKERS
//
// All thinkers should be allocated by Z_Malloc so they can be operated on 
// uniformly. The actual structures will vary in size, but the first element 
// must be thinker_t.
//

// Both the head and tail of the thinker list.
Thinker thinkercap;

// killough 8/29/98: we maintain several separate threads, each containing
// a special class of thinkers, to allow more efficient searches. 

Thinker thinkerclasscap[NUMTHCLASS];

//
// P_InitThinkers
//
void Thinker::InitThinkers(void)
{
   int i;
   
   for(i = 0; i < NUMTHCLASS; i++)  // killough 8/29/98: initialize threaded lists
      thinkerclasscap[i].cprev = thinkerclasscap[i].cnext = &thinkerclasscap[i];
   
   thinkercap.prev = thinkercap.next  = &thinkercap;
}

//
// Thinker::addToThreadedList
//
// haleyjd 11/22/10:
// Puts the thinker in the indicated threaded list.
//
void Thinker::addToThreadedList(int tclass)
{
   register Thinker *th;
   
   // Remove from current thread, if in one -- haleyjd: from PrBoom
   if((th = this->cnext) != NULL)
      (th->cprev = this->cprev)->cnext = th;

   // Add to appropriate thread
   th = &thinkerclasscap[tclass];
   th->cprev->cnext = this;
   this->cnext = th;
   this->cprev = th->cprev;
   th->cprev = this;
}

//
// P_UpdateThinker
//
// killough 8/29/98:
// 
// We maintain separate threads of friends and enemies, to permit more
// efficient searches.
//
// haleyjd 11/09/06: Added bug fixes from PrBoom, including addition of the
// th_delete thinker class to rectify problems with infinite loops in the AI
// code due to corruption of the th_enemies/th_friends lists when monsters get
// removed at an inopportune moment.
//
void Thinker::updateThinker()
{
   addToThreadedList(this->removed ? th_delete : th_misc);
}

//
// P_AddThinker
//
// Adds a new thinker at the end of the list.
//
void Thinker::addThinker()
{
   thinkercap.prev->next = this;
   next = &thinkercap;
   prev = thinkercap.prev;
   thinkercap.prev = this;
   
   references = 0;    // killough 11/98: init reference counter to 0
   
   // killough 8/29/98: set sentinel pointers, and then add to appropriate list
   cnext = cprev = this;
   updateThinker();
}

//
// killough 11/98:
//
// Make currentthinker external, so that P_RemoveThinkerDelayed
// can adjust currentthinker when thinkers self-remove.

Thinker *Thinker::currentthinker;

//
// P_RemoveThinkerDelayed
//
// Called automatically as part of the thinker loop in P_RunThinkers(),
// on nodes which are pending deletion.
//
// If this thinker has no more pointers referencing it indirectly,
// remove it, and set currentthinker to one node preceeding it, so
// that the next step in P_RunThinkers() will get its successor.
//
void Thinker::removeDelayed()
{
   if(!this->references)
   {
      Thinker *lnext = this->next;
      (lnext->prev = currentthinker = this->prev)->next = lnext;
      
      // haleyjd 11/09/06: remove from threaded list now
      (this->cnext->cprev = this->cprev)->cnext = this->cnext;
      
      delete this;
   }
}

//
// P_RemoveThinker
//
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
// killough 4/25/98:
//
// Instead of marking the function with -1 value cast to a function pointer,
// set the function to P_RemoveThinkerDelayed(), so that later, it will be
// removed automatically as part of the thinker process.
//
void Thinker::removeThinker()
{
   removed = true;
   
   // killough 8/29/98: remove immediately from threaded list
   
   // haleyjd 11/09/06: NO! Doing this here was always suspect to me, and
   // sure enough: if a monster's removed at the wrong time, it gets put
   // back into the list improperly and starts causing an infinite loop in
   // the AI code. We'll follow PrBoom's lead and create a th_delete class
   // for thinkers awaiting deferred removal.

   // Old code:
   //(thinker->cnext->cprev = thinker->cprev)->cnext = thinker->cnext;

   // Move to th_delete class.
   updateThinker();
}

//
// P_RunThinkers
//
// killough 4/25/98:
//
// Fix deallocator to stop using "next" pointer after node has been freed
// (a Doom bug).
//
// Process each thinker. For thinkers which are marked deleted, we must
// load the "next" pointer prior to freeing the node. In Doom, the "next"
// pointer was loaded AFTER the thinker was freed, which could have caused
// crashes.
//
// But if we are not deleting the thinker, we should reload the "next"
// pointer after calling the function, in case additional thinkers are
// added at the end of the list.
//
// killough 11/98:
//
// Rewritten to delete nodes implicitly, by making currentthinker
// external and using P_RemoveThinkerDelayed() implicitly.
//
void Thinker::RunThinkers(void)
{
   Mobj *mo;

   for(currentthinker = thinkercap.next; 
       currentthinker != &thinkercap;
       currentthinker = currentthinker->next)
   {
      if(currentthinker->removed)
         currentthinker->removeDelayed();
      else if(!clientserver)
         currentthinker->Think();
      else if(!(mo = thinker_cast<Mobj *>(currentthinker)))
         currentthinker->Think();
      else if(mo->player == NULL)
         currentthinker->Think();
   }
}

//
// Thinker::serialize
//
// Thinker serialization works together with the SaveArchive class and the
// series of ThinkerType-derived classes. Thinker subclasses should always
// call their parent implementation's serialize method. This default 
// implementation takes care of writing the class name when the archive is in
// save mode. If the thinker is being loaded from a save, the save archive
// has already read out the thinker name in order to instantiate an instance
// of the proper class courtesy of ThinkerType.
//
void Thinker::serialize(SaveArchive &arc)
{
   if(arc.isSaving())
      arc.WriteLString(getClassName());
}

//
// P_Ticker
//
void P_Ticker(void)
{
   int i;
   
   // pause if in menu and at least one tic has been run
   //
   // killough 9/29/98: note that this ties in with basetic,
   // since G_Ticker does the pausing during recording or
   // playback, and compensates by incrementing basetic.
   // 
   // All of this complicated mess is used to preserve demo sync.
   if(paused || ((menuactive || consoleactive) && !demoplayback && !netgame &&
                 players[consoleplayer].viewz != 1))
   {
      if(!clientserver) // [CG] Never pause in c/s.
         return;
   }
   
   P_ParticleThinker(); // haleyjd: think for particles

   S_RunSequences(); // haleyjd 06/06/06

   // not if this is an intermission screen
   // haleyjd: players don't think during cinematic pauses
   if(gamestate == GS_LEVEL && !cinema_pause)
   {
      // [CG] Only tick on the server's spectator actor serverside.
      i = 0;
      if(clientserver)
      {
         if(CS_SERVER)
         {
            P_PlayerThink(&players[0]);
            players[0].mo->Think();
         }
         i = 1;
      }

      for(; i < MAXPLAYERS; i++)
      {
         if(!playeringame[i])
            continue;

         if(CS_SERVER)
            SV_LoadClientOptions(i);

         if(!clientserver || CS_SERVER || CS_DEMO)
            P_PlayerThink(&players[i]);
         else if(i != consoleplayer)
            CL_PlayerThink(i);
         else if(cl_enable_prediction || clients[i].spectating)
            CL_PredictPlayerPosition(cl_current_world_index);
         else
            P_PlayerThink(&players[i]);

         if(clientserver && players[i].mo)
            players[i].mo->Think();

         if(CS_SERVER)
            SV_RestoreServerOptions();
      }
   }

   reset_viewz = false;  // sf

   Thinker::RunThinkers();
   P_UpdateSpecials();
   P_RespawnSpecials();
   if(demo_version >= 329)
      P_AnimateSurfaces(); // haleyjd 04/14/99
   
   leveltime++;                       // for par times

   // sf: on original doom, sometimes if you activated a hyperlift
   // while standing on it, your viewz was left behind and appeared
   // to "jump". code in p_floor.c detects if a hyperlift has been
   // activated and viewz is reset appropriately here.
   
   if(demo_version >= 303 && reset_viewz && gamestate == GS_LEVEL)
      P_CalcHeight(&players[displayplayer]); // Determines view height and bobbing
   
   P_RunEffects(); // haleyjd: run particle effects
}

//=============================================================================
//
// Thinker Factory
//
// haleyjd 12/18/10: The functions and methods below here help the savegame
// code create new thinkers.
//

// thinkerTypes - this is a list of all the ThinkerType global objects.
static ThinkerType *thinkerTypes;

//
// ThinkerType::FindType
//
// Static method to find a ThinkerType in the list by name. There are a 
// limited number of thinker types so this is just a linear search. The
// overhead of hashing is not worth it here. Returns NULL if no such type 
// exists (or it hasn't been registered yet).
//
ThinkerType *ThinkerType::FindType(const char *pName)
{
   ThinkerType *curType = thinkerTypes;

   while(curType && strcmp(curType->name, pName))
      curType = curType->next;

   return curType;
}

// 
// ThinkerType Constructor
//
// The object will automatically be added into the thinker factory list.
//
ThinkerType::ThinkerType(const char *pName) : name(pName)
{
   // ThinkerTypes must be singletons with unique names.
   if(FindType(pName))
      I_Error("ThinkerType: duplicate class registered with name %s\n", pName);

   // Add it to the global list; order is unimportant.
   this->next = thinkerTypes;
   thinkerTypes = this;
}

//----------------------------------------------------------------------------
//
// $Log: p_tick.c,v $
// Revision 1.7  1998/05/15  00:37:56  killough
// Remove unnecessary crash hack, fix demo sync
//
// Revision 1.6  1998/05/13  22:57:59  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.5  1998/05/03  22:49:01  killough
// Get minimal includes at top
//
// Revision 1.4  1998/04/29  16:19:16  killough
// Fix typo causing game to not pause correctly
//
// Revision 1.3  1998/04/27  01:59:58  killough
// Fix crashes caused by thinkers being used after freed
//
// Revision 1.2  1998/01/26  19:24:32  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

