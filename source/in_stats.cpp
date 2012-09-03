// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// 
// Statistics Saving and Loading
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_player.h"
#include "doomstat.h"
#include "e_hash.h"
#include "e_lib.h"
#include "g_game.h"
#include "in_stats.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "w_wad.h"

// Global Singleton Instance
INStatsManager INStatsManager::singleton;

//
// Private Implementation Object
//
class INStatsMgrPimpl : public ZoneObject
{
public:
   EHashTable<in_stat_t, EStringHashKey, 
              &in_stat_t::levelkey, &in_stat_t::links> statsByLevelKey;

   // expected fields
   enum
   {
      FIELD_LEVELKEY,
      FIELD_PLAYERNAME,
      FIELD_SKILL,
      FIELD_RECORDTYPE,   
      FIELD_VALUE,
      FIELD_MAXVALUE,
      FIELD_NUMFIELDS
   };

   // record types
   enum
   {
      RECORD_KILLS,
      RECORD_ITEMS,
      RECORD_SECRETS,
      RECORD_TIME,
      RECORD_FRAGS,
      RECORD_NUMTYPES
   };

   void parseCSVLine(char *&line, Collection<qstring> &output);
   void parseCSVScores(char *input);
};

// Names for record types
static const char *recordTypeNames[INStatsMgrPimpl::RECORD_NUMTYPES] =
{
   "kills",
   "items",
   "secrets",
   "time",
   "frags"
};

void INStatsMgrPimpl::parseCSVLine(char *&line, Collection<qstring> &output)
{
   char *rover  = line;
   bool  quoted = false;
   qstring curtoken;

   output.makeEmpty();

   while(*rover && *rover != '\n')
   {
      char thisChar = *rover;
      char nextChar = *(rover + 1);

      if(quoted)
      {
         if(thisChar == '"' && nextChar == '"')
         {
            curtoken += thisChar;
            ++rover; // step forward one
         }
         else if(thisChar == '"')
         {
            if(nextChar == ',' || nextChar == '\n' || nextChar == '\0')
               quoted = false;
            else
               curtoken += thisChar;
         }
         else
            curtoken += thisChar;
      }
      else
      {
         if(thisChar == '"')
         {
            if(curtoken.length() == 0)
               quoted = true;
            else
               curtoken += thisChar;
         }
         else if(thisChar == ',' || nextChar == '\n' || nextChar == '\0')
         {
            output.add(curtoken);
            curtoken = "";
         }
         else
            curtoken += thisChar;
      }

      ++rover;
   }

   if(*rover)
      line = rover + 1; // step to next line
   else
      line = rover;     // we are at the end.
}

void INStatsMgrPimpl::parseCSVScores(char *input)
{
   Collection<qstring> fields;

   // Go past the first line of input, which consists of column headers
   while(*input && *input != '\n')
      ++input;

   // Parse each remaining line
   while(*input)
   {
      parseCSVLine(input, fields);

      if(fields.getLength() >= FIELD_NUMFIELDS)
      {
         in_stat_t *newStats = estructalloc(in_stat_t, 1);

         newStats->levelkey   = fields[FIELD_LEVELKEY  ].duplicate(PU_STATIC);
         newStats->playername = fields[FIELD_PLAYERNAME].duplicate(PU_STATIC);
         newStats->skill      = fields[FIELD_SKILL     ].toInt();
         newStats->value      = fields[FIELD_VALUE     ].toInt();
         newStats->maxValue   = fields[FIELD_MAXVALUE  ].toInt();
         
         int recordType = E_StrToNumLinear(recordTypeNames, RECORD_NUMTYPES, 
                                           fields[FIELD_RECORDTYPE].constPtr());
         newStats->recordType = recordType;

         // add to hash tables
         statsByLevelKey.addObject(newStats);
      }
   }
}

//
// Constructor
//
INStatsManager::INStatsManager()
{
   pImpl = new INStatsMgrPimpl;
}

//
// INStatsManager::Init
//
// Initialize the intermission statistics manager.
//
void INStatsManager::Init()
{
   singleton.loadStats();
}

//
// INStatsManager::loadStats
//
// Load saved statistics from disk.
//
void INStatsManager::loadStats()
{
   char *statsText = NULL;
   qstring path;

   path = usergamepath;
   path.pathConcatenate("stats.csv");

   if((statsText = M_LoadStringFromFile(path.constPtr())))
   {
      pImpl->parseCSVScores(statsText);
      efree(statsText);
   }
}

/*
struct wbplayerstruct_t
{
  bool        in;     // whether the player is in game
    
  // Player stats, kills, collected items etc.
  int         skills;
  int         sitems;
  int         ssecret;
  int         stime; 
  int         frags[4];
  int         score;  // current score on entry, modified on return
  
};

struct wbstartstruct_t
{
  int         epsd;   // episode # (0-2)

  // if true, splash the secret level
  bool        didsecret;

  // haleyjd: if player is going to secret map
  bool        gotosecret;
    
  // previous and next levels, origin 0
  int         last;
  int         next;   
    
  int         maxkills;
  int         maxitems;
  int         maxsecret;
  int         maxfrags;

  // the par time
  int         partime;
    
  // index of this player in game
  int         pnum;   

  wbplayerstruct_t    plyr[MAXPLAYERS];

};
*/

//
// INStatsManager::recordStats
//
// Add stats for a particular level.
//
void INStatsManager::recordStats(const wbstartstruct_t *stats)
{
   const wbplayerstruct_t *playerData = &stats->plyr[stats->pnum];

   int levelLump    = g_dir->checkNumForName(gamemapname);
   const char *fn   = g_dir->getLumpFileName(levelLump);
   qstring levelkey;

   // really should not happen.
   if(!fn)
      return;   

   // construct the unique key for this level
   levelkey << fn << "::" << gamemapname;

   // iterate over any existing records for this map and update them
   // if appropriate
}

// EOF

