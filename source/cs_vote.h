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
// Client/Server voting module.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#ifndef CS_VOTE_H__
#define CS_VOTE_H__

// [CG] TODO: - Clientside: HUD widget.

class Ballot : public ZoneObject
{
private:
   int value;

public:
   DLListItem<Ballot> links;
   int client_id;

   static const int no_vote  = 0;
   static const int yea_vote = 1;
   static const int nay_vote = 2;

   Ballot(int new_client_id)
      : ZoneObject(), value(no_vote), client_id(new_client_id) {}

   void voteYea() { value = yea_vote; }
   void voteNay() { value = nay_vote; }
   bool votedYea() { if(value == yea_vote) return true; return false; }
   bool votedNay() { if(value == nay_vote) return true; return false; }

};

class Vote : public ZoneObject
{
protected:
   char *command;
   int started_at;
   unsigned int duration;
   double threshold;
   int result;

public:

   static const int no_result     = 0;
   static const int failed_result = 1;
   static const int passed_result = 2;

   Vote(const char *new_command, unsigned int new_duration,
        double new_threshold)
      : ZoneObject(), started_at(gametic), duration(new_duration),
        threshold(new_threshold), result(no_result)
   {
      command = estrdup(new_command);
   }

   ~Vote()
   {
      if(command)
         efree(command);
   }

   const char* getCommand() { return command; }
   unsigned int getDuration() const { return duration; }
   double getThreshold() const { return threshold; }
   bool passed() { if(result == passed_result) return true; return false; }
   bool failed() { if(result == failed_result) return true; return false; }
   unsigned int ticsRemaining()
   {
      unsigned int elapsed = gametic - started_at;

      if(elapsed >= duration)
         return 0;

      return duration - elapsed;
   }
   unsigned int timeRemaining()
   {
      return ticsRemaining() / TICRATE;
   }
   bool hasResult()
   {
      if(result != no_result) // [CG] Got a result from a vote.
         return true;

      if(timeRemaining()) // [CG] No result and still time left.
         return false;

      // [CG] Time ran out and the vote failed.
      result = failed_result;
      return true;
   }

   virtual bool castBallot(int clientnum, int ballot_value) { return false; }

};

#endif

