// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Various statistics, used for logging
//
//-----------------------------------------------------------------------------

#include <map>
#include "../z_zone.h"
#include "b_statistics.h"
#include "b_util.h"
#include "../doomstat.h"
#include "../info.h"
#include "../m_misc.h"

#define MONSTER_STATS_NAME "damagestats.data"

struct DamageStat
{
   int totalDamageToPlayer;
   int monsterDeaths;
};

static std::map<const mobjinfo_t*, DamageStat> g_stats;

void B_AddMonsterDeath(const mobjinfo_t* mi)
{
   ++g_stats[mi].monsterDeaths;
}

void B_AddToPlayerDamage(const mobjinfo_t* mi, int amount)
{
   g_stats[mi].totalDamageToPlayer += amount;
}

double B_GetMonsterThreatLevel(const mobjinfo_t* mi)
{
   DamageStat& stat = g_stats[mi];
   return stat.monsterDeaths > 0 ? (double)stat.totalDamageToPlayer / stat.monsterDeaths : DBL_MAX;
}

void B_LoadMonsterStats()
{
   const char* fpath = M_SafeFilePath(g_autoDoomPath, MONSTER_STATS_NAME);
   
   FILE* f = fopen(fpath, "rt");
   if(!f)
   {
      B_Log(MONSTER_STATS_NAME " not written yet.");
      return;
   }
   
   int minum, toPlayer, monsterDeaths;
   double toDiv;
   
   while(!feof(f))
   {
      if(fscanf(f, "%d%d%d%lf", &minum, &toPlayer, &monsterDeaths, &toDiv) < 4)
         break;
      
      g_stats[mobjinfo[0] + minum].totalDamageToPlayer = toPlayer;
      g_stats[mobjinfo[0] + minum].monsterDeaths = monsterDeaths;
   }
   
   fclose(f);
}

void B_StoreMonsterStats()
{
   const char* fpath = M_SafeFilePath(g_autoDoomPath, MONSTER_STATS_NAME);
   
   FILE* f = fopen(fpath, "wt");
   if(!f)
   {
      B_Log("Couldn't write " MONSTER_STATS_NAME);
      return;
   }
   for (auto it = g_stats.begin(); it != g_stats.end(); ++it)
   {
      fprintf(f, "%d\t%d\t%d\t%g\n", (int)(it->first - mobjinfo[0]), it->second.totalDamageToPlayer, it->second.monsterDeaths, B_GetMonsterThreatLevel(it->first));
   }
   fclose(f);
}