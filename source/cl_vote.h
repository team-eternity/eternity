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

#ifndef CL_VOTE_H__
#define CL_VOTE_H__

class ClientVote : public Vote
{
protected:

   Ballot **ballots;
   unsigned int yeas;
   unsigned int nays;
   unsigned int max_votes;

public:

   ClientVote(const char *new_command, unsigned int new_duration,
              double new_threshold, unsigned int new_max_votes)
      : Vote(new_command, new_duration, new_threshold), yeas(0), nays(0),
        max_votes(new_max_votes)
   {
      int i;

      ballots = ecalloc(Ballot **, MAX_CLIENTS, sizeof(Ballot *));
      for(i = 0; i < MAX_CLIENTS; i++)
         ballots[i] = new Ballot(i);
   }

   ~ClientVote()
   {
      int i;

      for(i = 0; i < MAX_CLIENTS; i++)
         delete ballots[i];

      efree(ballots);
   }

   unsigned int getYeaVotes() const { return yeas; }
   unsigned int getNayVotes() const { return nays; }
   unsigned int getMaxVotes() const { return max_votes; }

   bool castBallot(int clientnum, int ballot_value);
};

void        CL_CloseVote();
ClientVote* CL_GetCurrentVote();
bool        CL_CastBallot(int clientnum, int ballot_value);
void        CL_NewVote(const char *command, unsigned int duration,
                       double threshold, unsigned int max_votes);

#endif

