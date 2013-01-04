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

#include "c_io.h"
#include "c_runcmd.h"
#include "d_player.h"
#include "doomstat.h"
#include "e_hash.h"
#include "e_lib.h"
#include "g_game.h"
#include "hal/i_platform.h"
#include "in_stats.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "v_misc.h"
#include "w_wad.h"
#include "wi_stuff.h"

// Global Singleton Instance
INStatsManager INStatsManager::singleton;

// 
// A hash key with file system case sensitivity properties.
//
class INFSStringHashKey
{
public:
   typedef const char *basic_type;
   typedef const char *param_type;

   static unsigned int HashCode(const char *input)
   {
      return EE_PLATFORM_TEST(EE_PLATF_CSFS) 
               ? D_HashTableKeyCase(input) : D_HashTableKey(input);
   }

   static bool Compare(const char *first, const char *second)
   {
      return EE_PLATFORM_TEST(EE_PLATF_CSFS) 
               ? !strcmp(first, second) : !strcasecmp(first, second);
   }
};


//
// Private Implementation Object
//
class INStatsMgrPimpl
{
public:
   static INStatsMgrPimpl singleton;

   EHashTable<in_stat_t, INFSStringHashKey, 
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

   void parseCSVLine(char *&line, Collection<qstring> &output);
   void parseCSVScores(char *input);
};

// Singleton for the INStatsManager's private implementation
INStatsMgrPimpl INStatsMgrPimpl::singleton;

// Field names for CSV header
static const char *fieldNames[INStatsMgrPimpl::FIELD_NUMFIELDS] =
{
   "levelkey",
   "playername",
   "skill",
   "recordtype",
   "value",
   "maxvalue"
};

// Names for record types
static const char *recordTypeNames[INSTAT_NUMTYPES] =
{
   "kills",
   "items",
   "secrets",
   "time",
   "frags"
};

//
// INStatsMgrPimpl::parseCSVLine
//
// Parse a single line of the CSV input file.
//
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

//
// INStatsMgrPimpl::parseCSVScores
//
// Parse the CSV input file used to save top scores.
//
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
         int recordType = E_StrToNumLinear(recordTypeNames, INSTAT_NUMTYPES, 
                                           fields[FIELD_RECORDTYPE].constPtr());

         if(recordType < INSTAT_NUMTYPES)
         {
            in_stat_t *newStats = estructalloc(in_stat_t, 1);

            newStats->levelkey   = fields[FIELD_LEVELKEY  ].duplicate(PU_STATIC);
            newStats->playername = fields[FIELD_PLAYERNAME].duplicate(PU_STATIC);
            newStats->skill      = fields[FIELD_SKILL     ].toInt();
            newStats->value      = fields[FIELD_VALUE     ].toInt();
            newStats->maxValue   = fields[FIELD_MAXVALUE  ].toInt();

            newStats->recordType = recordType;

            // add to hash tables
            statsByLevelKey.addObject(newStats);
         }
      }
   }
}

//
// INStatsManager Constructor
//
INStatsManager::INStatsManager()
{
   // NB: just taking the address, order of construction won't matter.
   pImpl = &INStatsMgrPimpl::singleton;
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

static void MakeCSVValue(qstring &qstr, const char *input, bool comma)
{
   qstr.clear() << '"' << input << '"';
   if(comma)
      qstr << ',';
}

//
// INStatsManager::saveStats
//
// Save the high scores table back to file as CSV data.
//
void INStatsManager::saveStats()
{
   in_stat_t *stat = NULL;   
   qstring path;
   qstring value;
   FILE *f;
   int i;
   
   path = usergamepath;
   path.pathConcatenate("stats.csv");

   if(!(f = fopen(path.constPtr(), "w")))
      return; // woops...

   // Write the header row first
   for(i = 0; i < INStatsMgrPimpl::FIELD_NUMFIELDS; i++)
   {
      fputs(fieldNames[i], f);
      fputc(i == INStatsMgrPimpl::FIELD_MAXVALUE ? '\n' : ',', f);
   }

   while((stat = pImpl->statsByLevelKey.tableIterator(stat)))
   {
      // levelkey
      MakeCSVValue(value, stat->levelkey, true);
      fputs(value.constPtr(), f);

      // playername
      MakeCSVValue(value, stat->playername, true);
      fputs(value.constPtr(), f);

      // skill
      value.clear() << stat->skill << ',';
      fputs(value.constPtr(), f);

      // recordtype
      MakeCSVValue(value, recordTypeNames[stat->recordType], true);
      fputs(value.constPtr(), f);

      // value
      value.clear() << stat->value << ',';
      fputs(value.constPtr(), f);

      // maxvalue
      value.clear() << stat->maxValue << '\n';
      fputs(value.constPtr(), f);
   }

   fclose(f);
}

//
// INStatsManager::findScore
//
// Find a particular score for the given level key and score type.
//
in_stat_t *INStatsManager::findScore(const qstring &key, int type)
{
   in_stat_t *itr = NULL;

   while((itr = pImpl->statsByLevelKey.keyIterator(itr, key.constPtr())))
   {
      if(itr->recordType == type)
         return itr;
   }

   return NULL;
}

//
// INStatsManager::addScore
//
// Add a new score to the table.
//
void INStatsManager::addScore(const char *levelkey, int score, int maxscore, 
                              int scoretype, int pnum)
{
   in_stat_t *newStat = estructalloc(in_stat_t, 1);

   newStat->levelkey   = estrdup(levelkey);
   newStat->value      = score;
   newStat->maxValue   = maxscore;
   newStat->recordType = scoretype;
   newStat->skill      = gameskill;
   newStat->playername = estrdup(players[pnum].name);
   
   pImpl->statsByLevelKey.addObject(newStat);
}

//
// INStatsManager::getLevelKey
//
// Create the semi-unique key for the current level.
//
void INStatsManager::getLevelKey(qstring &outstr)
{
   int levelLump  = g_dir->checkNumForName(gamemapname);
   const char *fn = g_dir->getLumpFileName(levelLump);

   // really should not happen.
   if(!fn)
      return;   

   // construct the unique key for this level
   outstr << fn << "::" << qstring(gamemapname).toUpper();
}

//
// INStatsManager::getLevelKey
//
// Create the semi-unique key for a given level. In order for this
// to work, the wad being referenced must have been actually loaded
// via -file or some other loading mechanism.
//
void INStatsManager::getLevelKey(qstring &outstr, const char *mapName)
{
   int levelLump  = wGlobalDir.checkNumForName(mapName);
   const char *fn = wGlobalDir.getLumpFileName(levelLump);

   if(!fn)
      return;

   // build it
   outstr << fn << "::" << qstring(mapName).toUpper();
}

//
// INStatsManager::makeKey
//
// Make a key from a path and a lump name, even if the wad's not loaded.
//
void INStatsManager::getLevelKey(qstring &outstr, 
                                 const char *path, const char *mapName)
{
   outstr = path;
   outstr.normalizeSlashes();
   outstr << "::" << qstring(mapName).toUpper();
}

//
// INStatsManager::recordStats
//
// Add stats for a particular level.
//
void INStatsManager::recordStats(const wbstartstruct_t *wbstats)
{
   qstring levelkey;
   int   scores[INSTAT_NUMTYPES];
   int   maxscores[INSTAT_NUMTYPES];
   const wbplayerstruct_t *playerData = &wbstats->plyr[wbstats->pnum];

   getLevelKey(levelkey);

   // really shouldn't happen
   if(levelkey == "")
      return;

   // not playing??
   if(!playerData->in)
      return;

   // cheated?
   if(players[wbstats->pnum].cheats & CF_CHEATED)
      return;

   // demo?
   if(demoplayback)
      return;

   // copy data from wbstats/playerData to scores array
   scores[INSTAT_KILLS  ] = playerData->skills;
   scores[INSTAT_ITEMS  ] = playerData->sitems;
   scores[INSTAT_SECRETS] = playerData->ssecret;
   scores[INSTAT_TIME   ] = playerData->stime;
   scores[INSTAT_FRAGS  ] = players[wbstats->pnum].totalfrags;

   // Setup the maxscores array too
   maxscores[INSTAT_KILLS  ] = wbstats->maxkills;
   maxscores[INSTAT_ITEMS  ] = wbstats->maxitems;
   maxscores[INSTAT_SECRETS] = wbstats->maxsecret;
   maxscores[INSTAT_TIME   ] = wbstats->partime;   // Actually a minimum, of sorts.
   maxscores[INSTAT_FRAGS  ] = -1;                 // There's no max for frags.


   for(int i = 0; i < INSTAT_NUMTYPES; i++)
   {
      in_stat_t *instat;

      if((instat = findScore(levelkey, i)))
      {
         // If we're not playing on the same or a better skill level, this
         // score can't replace the old one.
         if(gameskill < instat->skill)
            continue;

         // * For most categories, a higher score wins. Time is an exception.
         // * A tie wins if it is achieved on a higher skill level.
         if((gameskill > instat->skill && scores[i] == instat->value) ||
            (i == INSTAT_TIME ? 
             scores[i] < instat->value : scores[i] > instat->value))
         {
            // This player has the new high score!
            instat->skill    = gameskill;
            instat->value    = scores[i];
            instat->maxValue = maxscores[i];
            E_ReplaceString(instat->playername, 
                            estrdup(players[wbstats->pnum].name));
         }
      }
      else
         addScore(levelkey.constPtr(), scores[i], maxscores[i], i, wbstats->pnum);
   }
}

//=============================================================================
//
// Useful score formatting data
//

// Short skill names
static const char *short_skills[] =
{
   "ITYTD",
   "HNTR",
   "HMP",
   "UV",
   "NM"
};

static const char *pretty_names[] =
{
   FC_ERROR "Kills   " FC_NORMAL,
   FC_ERROR "Items   " FC_NORMAL,
   FC_ERROR "Secrets " FC_NORMAL,
   FC_ERROR "Time    " FC_NORMAL,
   FC_ERROR "Frags   " FC_NORMAL
};

#define pretty_skill \
   "\n" FC_HI " Skill  " FC_NORMAL

#define pretty_by \
   "\n" FC_HI " Set By " FC_NORMAL

static const char *IN_formatTics(int tics)
{
   static char buffer[16];
   int h, m, s;

   s =  tics / TICRATE;
   h =  s / 3600;
   s -= h * 3600;
   m =  s / 60;
   s -= m * 60;

   psnprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
   return buffer;
}

//=============================================================================
//
// Console Commands
//

CONSOLE_COMMAND(mapscores, cf_level)
{
   INStatsManager &statsMgr = INStatsManager::Get();
   qstring levelkey, descr;

   if(Console.argc < 1)       // current level
      statsMgr.getLevelKey(levelkey);
   else if(Console.argc == 1) // a named level (must be loaded)
      statsMgr.getLevelKey(levelkey, Console.argv[0]->constPtr());
   else if(Console.argc >= 2) // absolute filepath::map
   {
      statsMgr.getLevelKey(levelkey, 
         Console.argv[0]->constPtr(),
         Console.argv[1]->constPtr());
   }

   if(levelkey == "")
      return;

   for(int i = 0; i < INSTAT_NUMTYPES; i++)
   {
      in_stat_t *stat;

      if((stat = statsMgr.findScore(levelkey, i)))
      {
         descr.clear() << pretty_names[stat->recordType];

         switch(stat->recordType)
         {
         case INSTAT_TIME:
            descr << IN_formatTics(stat->value);
            if(stat->maxValue > 0)
               descr << " (Par: " << IN_formatTics(stat->maxValue) << ")";
            break;
         case INSTAT_FRAGS:
            descr << stat->value;
            break;
         default:
            descr << stat->value << FC_HI "/" FC_NORMAL << stat->maxValue;
            break;
         }

         descr << pretty_skill << short_skills[stat->skill];
         descr << pretty_by    << stat->playername;
         C_Puts(descr.constPtr());
      }
   }
}

void IN_Stats_AddCommands()
{
   C_AddCommand(mapscores);
}

// EOF

