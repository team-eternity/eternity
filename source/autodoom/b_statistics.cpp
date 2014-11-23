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
#include "../info.h"

struct DamageStat
{
   int toPlayer;
   int monsterDeaths;
};

static std::map<const mobjinfo_t*, DamageStat> g_stats;

void B_AddMonsterDeath(const mobjinfo_t* mi)
{
   ++g_stats[mi].monsterDeaths;
}

void B_AddToPlayerDamage(const mobjinfo_t* mi, int amount)
{
   g_stats[mi].toPlayer += amount;
}

void B_LoadMonsterStats()
{
   FILE* f = fopen("damagestats.txt", "rt");
   if(!f)
   {
      B_Log("damagestats.txt not written yet.");
      return;
   }
   
   int minum, toPlayer, monsterDeaths;
   double toDiv;
   
   while(!feof(f))
   {
      if(fscanf(f, "%d%d%d%lf", &minum, &toPlayer, &monsterDeaths, &toDiv) < 4)
         break;
      
      g_stats[mobjinfo[0] + minum].toPlayer = toPlayer;
      g_stats[mobjinfo[0] + minum].monsterDeaths = monsterDeaths;
   }
   
   fclose(f);
}

void B_StoreMonsterStats()
{
   FILE* f = fopen("damagestats.txt", "wt");
   for (auto it = g_stats.begin(); it != g_stats.end(); ++it)
   {
      fprintf(f, "%zd\t%d\t%d\t%g\n", it->first - mobjinfo[0], it->second.toPlayer, it->second.monsterDeaths,
              it->second.monsterDeaths > 0 ? (double)it->second.toPlayer / it->second.monsterDeaths : DBL_MAX);
   }
   fclose(f);
}