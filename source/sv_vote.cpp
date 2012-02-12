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
// Serverside voting routines.
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
#include "sv_main.h"
#include "sv_vote.h"

#define NUM_VOTE_COMMANDS 16

class VoteCommand : public ZoneObject
{
private:
   unsigned int duration;
   double threshold;

public:
   DLListItem<VoteCommand> links;
   char *command;

   VoteCommand(const char *new_command, unsigned int new_duration,
               double new_threshold)
      : ZoneObject(), duration(new_duration), threshold(new_threshold)
   {
      command = estrdup(new_command);
   }

   ~VoteCommand() { if(command) efree(command); }

   char* getCommand() const { return command; }
   unsigned int getDuration() const { return duration; }
   double getThreshold() const { return threshold; }

};

static ServerVote *current_vote = NULL;

static EHashTable<VoteCommand, ENCStringHashKey, &VoteCommand::command,
                  &VoteCommand::links> vote_commands(NUM_VOTE_COMMANDS);

void ServerVote::checkForResult()
{
   Ballot *ballot = NULL;
   unsigned int yeas = 0;
   unsigned int nays = 0;

   while((ballot = ballots.tableIterator(ballot)))
   {
      if(ballot->votedYea())
         yeas++;
      else if(ballot->votedNay())
         nays++;
   }

   if(yeas > nays)
   {
      if((((double)yeas) / ((double)ballots.getNumItems())) >= threshold)
         result = passed_result;
   }
   else if(nays > yeas)
   {
      if((((double)nays) / ((double)ballots.getNumItems())) >= threshold)
         result = failed_result;
   }
}

bool ServerVote::castBallot(int clientnum, int ballot_value)
{
   server_client_t *sc;
   Ballot *ballot;

   if(result)
      return false;

   sc = &server_clients[clientnum];
   ballot = ballots.objectForKey(sc->connect_id);

   if(!ballot)
      return false;

   if(ballot_value == Ballot::yea_vote)
      ballot->voteYea();
   else if(ballot_value == Ballot::nay_vote)
      ballot->voteNay();

   checkForResult();

   return true;
}

void SV_CloseVote()
{
   if(current_vote)
   {
      delete current_vote;
      current_vote = NULL;
   }
}

ServerVote* SV_GetCurrentVote()
{
   return current_vote;
}

bool SV_CastBallot(int clientnum, int ballot_value)
{
   if(!current_vote)
      return false;

   return current_vote->castBallot(clientnum, ballot_value);
}

bool SV_NewVote(const char *command)
{
   VoteCommand *vc = vote_commands.objectForKey(command);

   if(!vc)
      return false;

   current_vote =
      new ServerVote(vc->getCommand(), vc->getDuration(), vc->getThreshold());

   return true;
}

bool SV_AddVoteCommand(const char *command, unsigned int duration,
                       double threshold)
{
   if(vote_commands.objectForKey(command))
      return false;

   vote_commands.addObject(new VoteCommand(command, duration, threshold));
   return true;
}

