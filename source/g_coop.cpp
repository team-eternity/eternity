// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
// DESCRIPTION:
//   Cooperative/Singleplayer game type.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "info.h"
#include "m_fixed.h"
#include "g_type.h"
#include "g_coop.h"
#include "p_mobj.h"

#include "cs_main.h"

CoopGameType::CoopGameType(const char *new_name)
   : BaseGameType(new_name, xgt_cooperative) {}

void CoopGameType::handleActorKilled(Mobj *source, Mobj *target, emod_t *mod)
{
   // [CG] If a player killed a monster, increase their score.  We also have to
   //      handle the case where monsters kill each other.  Single player adds
   //      the kill to player 0, so c/s can use the server's player, but p2p
   //      just has to defer (like it always does) to player 0.
   if(target && (!target->player))
   {
      // killough 7/20/98: don't count friends
      if((!(target->flags & MF_FRIEND)) && (target->flags & MF_COUNTKILL))
      {
         int playernum;

         if(source && source->player)
            playernum = source->player - players;
         else
            playernum = 0;

         // [CG] FIXME: Team coop is weird....
         CS_IncrementClientScore(playernum, true);
      }
   }
}

