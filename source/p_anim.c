//=========================================================================
//
// Copyright(C) 2000 James Haley
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
//--------------------------------------------------------------------------
//
// haleyjd 11/14/00
//
// Description:  Handles new frame-linked animation processes
//               Currently performs lightning in sky sectors
//
//               11/14/00: updates scroll offsets for hexen-style
//                         skies
//
//=========================================================================

#include "z_zone.h"
#include "doomstat.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "r_sky.h"
#include "m_random.h"
#include "p_setup.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_anim.h"
#include "p_info.h"
#include "a_small.h"

int NextLightningFlash;
int LightningFlash;
int LevelSky;
int LevelTempSky;

static void P_LightningFlash(void);

//
// P_AnimateSurfaces
//
// Called every tic in P_Ticker
//
void P_AnimateSurfaces(void)
{
   // update sky scroll offsets
   //   haleyjd: stored as regular ints in the mapinfo so we need 
   //   to transform these to fixed point values :)

   Sky1ColumnOffset += ((fixed_t)LevelInfo.skyDelta ) << 8;
   Sky2ColumnOffset += ((fixed_t)LevelInfo.sky2Delta) << 8;
   
   if(LevelInfo.hasLightning)
   {
      if(!NextLightningFlash || LightningFlash)
         P_LightningFlash();
      else
         NextLightningFlash--;
   }
}

static void P_LightningFlash(void)
{
   int i;
   sector_t *tempSec;
   boolean foundSec;
   int flashLight;

   if(LightningFlash)
   {
      LightningFlash--;

      if(LightningFlash)
      {
         for(i = 0; i < numsectors; i++)
         {
            tempSec = &sectors[i];

            if(tempSec->intflags & SIF_SKY &&
               tempSec->oldlightlevel < tempSec->lightlevel - 4)
            {
               tempSec->lightlevel -= 4;
            }
         }
      }
      else
      {
         for(i = 0; i < numsectors; i++)
         {
            tempSec = &sectors[i];

            if(tempSec->intflags & SIF_SKY)
               tempSec->lightlevel = tempSec->oldlightlevel;
         }

         if(LevelSky != -1 && LevelTempSky != -1)
            skytexture = LevelSky;
      }
   }
   else
   {
      LightningFlash = (P_Random(pr_lightning) & 7) + 8;
      flashLight     = 200 + (P_Random(pr_lightning) & 31);
      
      foundSec = false;

      for(i = 0; i < numsectors; i++)
      {
         tempSec = &sectors[i];

         if(tempSec->intflags & SIF_SKY)
         {
            tempSec->oldlightlevel = tempSec->lightlevel;
            tempSec->lightlevel    = flashLight;

            if(tempSec->lightlevel < tempSec->oldlightlevel)
               tempSec->lightlevel = tempSec->oldlightlevel;

            foundSec = true;
         }
      }

      if(foundSec)
      {
         if(LevelSky != -1 && LevelTempSky != -1)
            skytexture = LevelTempSky;

         S_StartSound(NULL, sfx_thundr);
      }

      if(!NextLightningFlash)
      {
         if(M_Random() < 50)
            NextLightningFlash = (M_Random() & 15) + 16;
         else
         {
            if(M_Random() < 128 && !(leveltime & 32))
               NextLightningFlash = ((M_Random() & 7) + 2) * 35;
            else
               NextLightningFlash = ((M_Random() & 15) + 5) * 35;
         }
      }
   }
}

void P_ForceLightning(void)
{
   NextLightningFlash = 0;
}

//
// P_InitLightning(void)
//
// Called from P_SetupLevel
//
void P_InitLightning(void)
{  
   if(!LevelInfo.hasLightning)
      LightningFlash = 0;
   else
   {
      LevelSky = skytexture;
      
      if(LevelInfo.altSkyName)
         LevelTempSky = R_TextureNumForName(LevelInfo.altSkyName);
      else
         LevelTempSky = -1;
      
      LightningFlash = 0;      
      NextLightningFlash = ((M_Random() & 15) + 5) * 35;
   }
}

#ifndef EE_NO_SMALL_SUPPORT
static cell AMX_NATIVE_CALL sm_lightning(AMX *amx, cell *params)
{
   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   P_ForceLightning();
   return 0;
}

AMX_NATIVE_INFO panim_Natives[] =
{
   { "_ForceLightning", sm_lightning },
   { NULL, NULL }
};
#endif

// EOF

