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
// Clientside voting routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_hash.h"

#include "cs_main.h"
#include "cs_vote.h"
#include "cl_vote.h"

static ClientVote *current_vote = NULL;

bool ClientVote::castBallot(int clientnum, int ballot_value)
{
   int i;

   if(result)
      return false;

   if(ballot_value == Ballot::yea_vote)
      ballots[clientnum]->voteYea();
   else if(ballot_value == Ballot::nay_vote)
      ballots[clientnum]->voteNay();
   else
      return false;

   yeas = nays = 0;

   for(i = 0; i < MAX_CLIENTS; i++)
   {
      if(ballots[i]->votedYea())
         yeas++;
      else if(ballots[i]->votedNay())
         nays++;
   }

   return true;
}

void CL_CloseVote()
{
   if(current_vote)
   {
      delete current_vote;
      current_vote = NULL;
   }
}

ClientVote* CL_GetCurrentVote()
{
   return current_vote;
}

bool CL_CastBallot(int clientnum, int ballot_value)
{
   if(!current_vote)
      return false;

   return current_vote->castBallot(clientnum, ballot_value);
}

void CL_NewVote(const char *command, unsigned int duration, double threshold,
                unsigned int max_votes)
{
   if(!current_vote)
      current_vote = new ClientVote(command, duration, threshold, max_votes);
   else
      doom_printf("CL_NewVote: Vote still in progress.\n");
}

