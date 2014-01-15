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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Sector effects.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "doomstat.h"
#include "e_reverbs.h"
#include "e_things.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"
#include "v_misc.h"

//=============================================================================
//
// SectorThinker class methods
//

IMPLEMENT_THINKER_TYPE(SectorThinker)

//
// SectorThinker::serialize
//
void SectorThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << sector;

   // when reloading, attach to sector
   if(arc.isLoading())
   {
      switch(getAttachPoint())
      {
      case ATTACH_FLOOR:
         sector->floordata = this;
         break;
      case ATTACH_CEILING:
         sector->ceilingdata = this;
         break;
      case ATTACH_FLOORCEILING:
         sector->floordata   = this;
         sector->ceilingdata = this;
         break;
      case ATTACH_LIGHT:
         sector->lightingdata = this;
         break;
      default:
         break;
      }
   }
}

//=============================================================================
//
// Sector Actions
//

//
// P_NewSectorActionFromMobj
//
// Adds the Mobj's special to the sector
//
void P_NewSectorActionFromMobj(Mobj *actor)
{
#if 0
   sectoraction_t *newAction = estructalloc(sectoraction_t, 1);

   if(actor->type == E_ThingNumForName("EESectorActionEnter"))
   {
      // TODO
   }
#endif
}

//=============================================================================
//
// Sector Interpolation
//

//
// P_SaveSectorPositions
//
// Backup current sector floor and ceiling heights to the sector interpolation
// structures at the beginning of a frame.
//
void P_SaveSectorPositions()
{
   for(int i = 0; i < numsectors; i++)
   {
      auto &si  = sectorinterps[i];
      auto &sec = sectors[i];

      si.prevfloorheight    = sec.floorheight;
      si.prevfloorheightf   = sec.floorheightf;
      si.prevceilingheight  = sec.ceilingheight;
      si.prevceilingheightf = sec.ceilingheightf;
   }
}

//=============================================================================
//
// Sound Environment Zones
//

//
// P_SetSectorZoneFromMobj
//
// Change a sound environment zone to the reverb definition indicated in the
// actor's first two arguments. The zone affected will be the one containing 
// the sector that the thing's centerpoint is inside.
//
void P_SetSectorZoneFromMobj(Mobj *actor)
{
   sector_t  *sec    = actor->subsector->sector;
   ereverb_t *reverb = E_ReverbForID(actor->args[0], actor->args[1]);

   if(reverb)
      soundzones[sec->soundzone].reverb = reverb;
}

//
// p_testenvironment
//
// Alter the console command player's sound zone.
//
CONSOLE_COMMAND(p_testenvironment, cf_level)
{
   if(Console.argc < 2)
   {
      C_Printf("Usage: p_testenvironment id1 id2\n");
      return;
   }
   int id1 = Console.argv[0]->toInt();
   int id2 = Console.argv[1]->toInt();

   ereverb_t *reverb = E_ReverbForID(id1, id2);
   if(!reverb)
   {
      C_Printf(FC_ERROR "Reverb (%d, %d) not defined.\n", id1, id2);
      return;
   }
   
   Mobj *mo = players[Console.cmdsrc].mo;
   if(!mo)
   {
      C_Printf(FC_ERROR "Command source has no body!\n");
      return;
   }
   
   sector_t *sec = mo->subsector->sector;
   soundzones[sec->soundzone].reverb = reverb;
}


// EOF

