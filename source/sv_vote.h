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

#ifndef SV_VOTE_H__
#define SV_VOTE_H__

#define DEFAULT_VOTE_DURATION 30
#define DEFAULT_VOTE_THRESHOLD 66

class ServerVote : public Vote
{
protected:

   EHashTable<Ballot, EIntHashKey, &Ballot::client_id,
              &Ballot::links> ballots;

   void checkForResult();

public:

   ServerVote(const char *new_command, unsigned int new_duration,
              double new_threshold)
      : Vote(new_command, new_duration, new_threshold)
   {
      int i;

      ballots.initialize(MAX_CLIENTS);

      for(i = 1; i < MAX_CLIENTS; i++)
      {
         if(playeringame[i] && clients[i].queue_level == ql_playing)
            ballots.addObject(new Ballot(server_clients[i].connect_id));
      }

   }

   ~ServerVote()
   {
      Ballot *ballot = NULL;

      while((ballot = ballots.tableIterator(NULL)))
      {
         ballots.removeObject(ballot);
         delete ballot;
      }
   }

   unsigned int getMaxVotes() { return ballots.getNumItems(); }

   bool castBallot(int clientnum, int ballot_value);
};

void        SV_CloseVote();
ServerVote* SV_GetCurrentVote();
bool        SV_CastBallot(int clientnum, int ballot_value);
bool        SV_NewVote(const char *command);
bool        SV_AddVoteCommand(const char *command, unsigned int duration,
                              double threshold);

#endif

