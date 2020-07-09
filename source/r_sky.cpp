// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//  Sky rendering. The DOOM sky is a texture map like any
//  wall, wrapping around. A 1024 columns equal 360 degrees.
//  The default sky map is 256 columns and repeats 4 times
//  on a 320 screen?
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "r_sky.h"
#include "r_data.h"
#include "p_info.h"
#include "w_wad.h"
#include "d_gi.h"

//
// sky mapping
//

int stretchsky;

static int numskyflats;

//
// R_StartSky
//
// Called when the level starts to load the appropriate sky.
//
void R_StartSky()
{
   // haleyjd 07/18/04: init moved to MapInfo
  
   numskyflats = 0;

   // Set the sky map.
   // First thing, we have a dummy sky texture name,
   //  a flat. The data is in the WAD only because
   //  we look for an actual index, instead of simply
   //  setting one.
   skyflat_t *skies = GameModeInfo->skyFlats;
   skyflat_t *sky   = skies;
   while(sky->flatname)
   {
      // reset scroll indices
      sky->columnoffset = 0;

      // look up flat
      sky->flatnum = R_FindFlat(sky->flatname);

      // look up default texture, if specified
      if(sky->deftexture)
         sky->texture = R_FindWall(sky->deftexture);

      ++sky;
      ++numskyflats;
   }
      
   // allow LevelInfo to override sky textures 1 and 2
   skyflat_t *sky1 = R_SkyFlatForIndex(0);
   skyflat_t *sky2 = R_SkyFlatForIndex(1);

   if(sky1)
      sky1->texture = R_FindWall(LevelInfo.skyName);
   if(sky2)
      sky2->texture = R_FindWall(LevelInfo.sky2Name);
}

//
// Sky texture information hash table stuff
// haleyjd 08/30/02: I need to store information about sky textures
// for use in the renderer, because each sky texture must be
// rendered differently depending on its size.
//

// the sky texture hash table
skytexture_t *skytextures[NUMSKYCHAINS];

#define skytexturekey(a) ((a) % NUMSKYCHAINS)

//
// R_AddSkyTexture
//
// Constructs a skytexture_t and adds it to the hash table
//
static skytexture_t *R_AddSkyTexture(int texturenum)
{
   skytexture_t *newSky;
   int key;

   // SoM: The new texture system handles tall patches in textures.
   newSky = (skytexture_t *)(Z_Malloc(sizeof(skytexture_t), PU_STATIC, nullptr));

   // 02/11/04: only if patch height is greater than texture height
   // should we use it
   newSky->texturenum = texturenum;
   newSky->height = textures[texturenum]->height;
   
   if(newSky->height >= 200)
      newSky->texturemid = 200*FRACUNIT;
   else
      newSky->texturemid = 100*FRACUNIT;

   key = skytexturekey(texturenum);

   // use head insertion
   newSky->next = skytextures[key];
   skytextures[key] = newSky;

   return newSky;
}

//
// R_GetSkyTexture
//
// Looks for the specified skytexture_t with the given texturenum
// in the hash table. If it doesn't exist, it'll be created now.
// 
skytexture_t *R_GetSkyTexture(int texturenum)
{
   int key;
   skytexture_t *target = nullptr;

   key = skytexturekey(texturenum);

   if(skytextures[key])
   {
      // search in chain
      skytexture_t *rover = skytextures[key];

      while(rover)
      {
         if(rover->texturenum == texturenum)
         {
            target = rover;
            break;
         }

         rover = rover->next;
      }
   }

   return target ? target : R_AddSkyTexture(texturenum);
}

//
// R_ClearSkyTextures
//
// Must be called from R_InitData to clear out old texture numbers.
// Otherwise the data will be corrupt and meaningless.
//
void R_ClearSkyTextures()
{
   for(int i = 0; i < NUMSKYCHAINS; i++)
   {
      if(skytextures[i])
      {
         skytexture_t *next;
         skytexture_t *rover = skytextures[i];

         while(rover)
         {
            next = rover->next;

            Z_Free(rover);

            rover = next;
         }
      }

      skytextures[i] = nullptr;
   }
}

//
// R_IsSkyFlat
//
// haleyjd 07/04/14: Scan GameModeInfo's skyFlats array to determine if a flat
// is sky or not.
//
bool R_IsSkyFlat(int picnum)
{
   const skyflat_t *sky = GameModeInfo->skyFlats;

   while(sky->flatname)
   {
      if(picnum == sky->flatnum)
         return true;
      ++sky;
   }

   return false;
}

//
// R_SkyFlatForIndex
//
// Return a skyflat_t structure for a sky number.
// Returns null if the gamemode does not define that many skies.
//
skyflat_t *R_SkyFlatForIndex(int skynum)
{
   if(skynum >= 0 && skynum < numskyflats)
      return &(GameModeInfo->skyFlats[skynum]);
   else
      return nullptr;
}

//
// R_SkyFlatForPicnum
//
// Return a skyflat_t structure for a flat texture directory index.
// Returns null if the picnum is not a sky.
//
skyflat_t *R_SkyFlatForPicnum(int picnum)
{
   skyflat_t *sky = GameModeInfo->skyFlats;
   while(sky->flatname)
   {
      if(sky->flatnum == picnum)
         return sky;
      ++sky;
   }

   return nullptr;
}

//----------------------------------------------------------------------------
//
// $Log: r_sky.c,v $
// Revision 1.6  1998/05/03  23:01:06  killough
// beautification
//
// Revision 1.5  1998/05/01  14:14:24  killough
// beautification
//
// Revision 1.4  1998/02/05  12:14:31  phares
// removed dummy comment
//
// Revision 1.3  1998/01/26  19:24:49  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/19  16:17:59  rand
// Added dummy line to be removed later.
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
