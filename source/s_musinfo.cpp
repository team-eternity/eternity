//
// The Eternity Engine
// Copyright (C) 2017 James Haley et al.
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
// Purpose: MUSINFO implementation
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "doomstat.h"
#include "g_game.h"
#include "p_info.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "r_defs.h"
#include "s_musinfo.h"
#include "s_sound.h"

enum
{
   COOLDOWN = 30
};

//
// The MUSINFO structure
//
struct musinfo_t
{
   Mobj *mapthing;      // keep track of touched music changers
   Mobj *lastmapthing;
   int tics;            // set a cooldown
   int savedmusic;      // current music must be archived. -1 means no change.
};

static musinfo_t musinfo;

//
// Clears the musinfo data. Called from P_SetupLevel.
//
void S_MusInfoClear()
{
   memset(&musinfo, 0, sizeof(musinfo));
   musinfo.savedmusic = -1;
}

//
// Called by the EEMusicChanger mobj. Replaces any mobj behaviour.
//
void S_MusInfoThink(Mobj &thing)
{
   if(musinfo.mapthing != &thing &&
      thing.subsector->sector == players[consoleplayer].mo->subsector->sector)
   {
      P_SetTarget(&musinfo.lastmapthing, musinfo.mapthing);
      P_SetTarget(&musinfo.mapthing, &thing);
      musinfo.tics = COOLDOWN;
   }
}

//
// Updates the music after the current setting was changed
//
static void S_updateMusic()
{
   if(musinfo.savedmusic < 0)
      return;
   const char *lumpname = P_GetMusInfoMusic(gamemapname, musinfo.savedmusic);
   if(lumpname)
      S_ChangeMusicName(lumpname, 1);
}

//
// Run from Thinker::RunThinkers, it acts upon any changes on musinfo
//
void S_MusInfoUpdate()
{
   if(musinfo.tics < 0 || !musinfo.mapthing)
      return;
   if(musinfo.tics > 0)
   {
      musinfo.tics--;
      return;
   }
   if(!musinfo.tics && musinfo.lastmapthing != musinfo.mapthing)
   {
      musinfo.savedmusic = musinfo.mapthing->args[0];
      S_updateMusic();
   }
}

//
// Save current music. Needed because otherwise the old music gets loaded.
//
void S_MusInfoArchive(SaveArchive &arc)
{
   arc << musinfo.savedmusic;
   if(arc.isLoading())
      S_updateMusic();
}

// EOF

