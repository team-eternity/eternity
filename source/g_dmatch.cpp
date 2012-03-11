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
#include "g_dmatch.h"
#include "p_mobj.h"

#include "cs_main.h"

DeathmatchGameType::DeathmatchGameType(const char *new_name)
   : BaseGameType(new_name, xgt_deathmatch) {}

int DeathmatchGameType::getStrengthOfVictory(float low_score, float high_score)
{
   float score_ratio = 0.0;

   if(low_score && high_score)
      score_ratio = low_score / high_score;

   if(score_ratio <= 50.0 && ((high_score - low_score) >= 20))
      return sov_blowout;

   if(score_ratio <= 66.6)
      return sov_humiliation;

   if(score_ratio <= 80.0)
      return sov_impressive;

   return sov_normal;
}

bool DeathmatchGameType::shouldExitLevel()
{
   int i;

   for(i = 1; i < MAXPLAYERS; i++)
   {
      if(playeringame[i] && clients[i].stats.score >= levelFragLimit)
         return true;
   }

   return false;
}

void DeathmatchGameType::handleActorKilled(Mobj *source, Mobj *target,
                                           emod_t *mod)
{
   // [CG] If a player killed another player (that isn't themselves), increase
   //      their score.
   if(source && source->player && target && target->player)
   {
      if(source->player != target->player)
         CS_IncrementClientScore(source->player - players);
      else // [CG] Suicide
         CS_DecrementClientScore(source->player - players);
   }
}

