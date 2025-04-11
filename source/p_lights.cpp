//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//  Action routines for lighting thinkers
//  Spawn sector based lighting effects.
//  Handle lighting linedef types
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomdef.h"
#include "doomstat.h" //jff 5/18/98
#include "m_random.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"

//////////////////////////////////////////////////////////
//
// Lighting action routines, called once per tick
//
//////////////////////////////////////////////////////////

IMPLEMENT_THINKER_TYPE(FireFlickerThinker)

//
// T_FireFlicker()
//
// Firelight flicker action routine, called once per tick
//
// Passed a FireFlickerThinker structure containing light levels and timing
// Returns nothing
//
void FireFlickerThinker::Think()
{
   int amount;
   
   if(--this->count)
      return;
   
   amount = (P_Random(pr_lights)&3)*16;
   M_RandomLog("FireFlickerThinker\n");
   
   if(this->sector->lightlevel - amount < this->minlight)
      this->sector->lightlevel = this->minlight;
   else
      this->sector->lightlevel = this->maxlight - amount;
   
   this->count = 4;
}

//
// FireFlickerThinker::serialize
//
// Saves/loads a FireFlickerThinker thinker.
//
void FireFlickerThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << count << maxlight << minlight;
}


IMPLEMENT_THINKER_TYPE(LightFlashThinker)

//
// T_LightFlash()
//
// Broken light flashing action routine, called once per tick
//
// Passed a LightFlashThinker structure containing light levels and timing
// Returns nothing
//
void LightFlashThinker::Think()
{
   if(--this->count)
      return;
   
   if(this->sector->lightlevel == this->maxlight)
   {
      this->sector->lightlevel = this->minlight;
      this->count = (P_Random(pr_lights)&this->mintime)+1;
      M_RandomLog("LightFlashThinkerMax %d\n", (int)(sector - sectors));
   }
   else
   {
      this->sector->lightlevel = this->maxlight;
      this->count = (P_Random(pr_lights)&this->maxtime)+1;
      M_RandomLog("LightFlashThinker %d\n", (int)(sector - sectors));
   }
}

//
// LightFlashThinker::serialize
//
// Saves/loads LightFlashThinker thinkers.
//
void LightFlashThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << count << maxlight << minlight << maxtime << mintime;
}

//
// LightFlashThinker::reTriggerVerticalDoor
//
// haleyjd 10/13/2011: emulate vanilla behavior when a LightFlashThiker is
// treated as a VerticalDoorThinker
//
bool LightFlashThinker::reTriggerVerticalDoor(bool player)
{
   if(!demo_compatibility)
      return false;

   if(maxtime == plat_down)
      maxtime = plat_up;
   else
   {
      if(!player)
         return false;

      maxtime = plat_down;
   }

   return true;
}


IMPLEMENT_THINKER_TYPE(StrobeThinker)

//
// T_StrobeFlash()
//
// Strobe light flashing action routine, called once per tick
//
// Passed a StrobeThinker structure containing light levels and timing
// Returns nothing
//
void StrobeThinker::Think()
{
   if(--this->count)
      return;
   
   if(this->sector->lightlevel == this->minlight)
   {
      this->sector->lightlevel = this->maxlight;
      this->count = this->brighttime;
   }
   else
   {
      this->sector->lightlevel = this->minlight;
      this->count = this->darktime;
   }
}

//
// StrobeThinker::serialize
//
// Saves/loads a StrobeThinker.
//
void StrobeThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << count << minlight << maxlight << darktime << brighttime;
}

//
// StrobeThinker::reTriggerVerticalDoor
//
// haleyjd 10/13/2011: emulate vanilla behavior when a strobe thinker is 
// manipulated as a VerticalDoorThinker
//
bool StrobeThinker::reTriggerVerticalDoor(bool player) 
{
   if(!demo_compatibility)
      return false;

   if(darktime == plat_down)
      darktime = plat_up;
   else
   {
      if(!player)
         return false;

      darktime = plat_down;
   }
   
   return true;
}


IMPLEMENT_THINKER_TYPE(GlowThinker)

//
// T_Glow
//
// Glowing light action routine, called once per tick
//
// Passed a GlowThinker structure containing light levels and timing
// Returns nothing
//
void GlowThinker::Think()
{
   switch(direction)
   {
   case -1:
      // light dims
      sector->lightlevel -= GLOWSPEED;
      if(sector->lightlevel <= minlight)
      {
         sector->lightlevel += GLOWSPEED;
         direction = 1;
      }
      break;
      
   case 1:
      // light brightens
      sector->lightlevel += GLOWSPEED;
      if(sector->lightlevel >= maxlight)
      {
         sector->lightlevel -= GLOWSPEED;
         direction = -1;
      }
      break;
   }
}

//
// GlowThinker::serialize
//
// Saves/loads a GlowThinker.
//
void GlowThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << minlight << maxlight << direction;
}


IMPLEMENT_THINKER_TYPE(SlowGlowThinker)

//
// SlowGlowThinker::Think
//
// haleyjd: Thinker for PSX glow effects, which need to move at non-integral speeds.
//
void SlowGlowThinker::Think()
{
   switch(direction)
   {
   case -1:
      // light dims
      accum -= GLOWSPEEDSLOW;
      sector->lightlevel = accum / FRACUNIT;
      if(sector->lightlevel <= minlight)
      {
         accum += GLOWSPEEDSLOW;
         sector->lightlevel = accum / FRACUNIT;
         direction = 1;
      }
      break;
      
   case 1:
      // light brightens
      accum += GLOWSPEEDSLOW;
      sector->lightlevel = accum / FRACUNIT;
      if(sector->lightlevel >= maxlight)
      {
         accum -= GLOWSPEEDSLOW;
         sector->lightlevel = accum / FRACUNIT;
         direction = -1;
      }
      break;
   }
}

//
// SlowGlowThinker::serialize
//
// Saves/loads a SlowGlowThinker.
//
void SlowGlowThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << minlight << maxlight << direction << accum;
}


IMPLEMENT_THINKER_TYPE(LightFadeThinker)

//
// T_LightFade
//
// sf 13/10/99:
// Just fade the light level in a sector to a new level
//
// haleyjd 01/10/07: changes for param line specs
//
void LightFadeThinker::Think()
{
   bool done = false;

   // fade light by one step
   this->lightlevel += this->step;

   // fading up or down? check if done
   if((this->step > 0) ? this->lightlevel >= this->destlevel 
                       : this->lightlevel <= this->destlevel)
   {
      this->lightlevel = this->destlevel;
      done = true;
   }

   // write light level back to sector
   this->sector->lightlevel = (int16_t)(this->lightlevel / FRACUNIT);

   if(this->sector->lightlevel < 0)
      this->sector->lightlevel = 0;
   else if(this->sector->lightlevel > 255)
      this->sector->lightlevel = 255;

   if(done)
   {
      if(this->type == fade_once)
         this->remove();
      else
      {
         // reverse glow direction
         this->destlevel = (this->lightlevel == this->glowmax) ? this->glowmin : this->glowmax;
         this->step      = (this->destlevel - this->lightlevel) / this->glowspeed;
      }
   }
}

//
// LightFadeThinker::serialize
//
// Saves/loads a LightFadeThinker thinker.
//
void LightFadeThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << lightlevel << destlevel << step << glowmin << glowmax
       << glowspeed << type;
}

IMPLEMENT_THINKER_TYPE(PhasedLightThinker)

//
// phaseTable for PhasedLightThinker
//
// Wrapped around modulus 64; defines a delta to the thinker's base light level
//
static int phaseTable[64] =
{
   128, 112,  96,  80,  64,  48,  32,  32,
    16,  16,  16,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,  16,  16,  16,
    32,  32,  48,  64,  80,  96, 112, 128
};

//
// PhasedLightThinker::Think
//
// Think for Hexen-style phased light effect.
//
void PhasedLightThinker::Think()
{
   index = (index + 1) & 63;
   sector->lightlevel = base + phaseTable[index];
}

//
// PhasedLightThinker::serialize
//
// Save or restore a Hexen-style phased light effect.
//
void PhasedLightThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << base << index;
}

//
// PhasedLightThinker::Spawn
//
// Static function. Creates a new PhasedLightThinker attached to the provided
// sector with the given base level and initial phase index. If -1 is passed
// in index, then the initial phase is set by the sector's light level mod 64.
//
void PhasedLightThinker::Spawn(sector_t *sector, int base, int index)
{
   auto phase = new PhasedLightThinker;
   phase->addThinker();

   phase->sector = sector;
   phase->base   = base & 255;
   phase->index  = ((index == -1) ? sector->lightlevel : index) & 63;

   sector->lightlevel = phase->base + phaseTable[phase->index];
}

//
// PhasedLightThinker::SpawnSequence
//
// Called on sectors with SECF_PHASEDLIGHT; walks into neighboring sectors with
// SECF_LIGHTSEQUENCE and SECF_LIGHTSEQALT, in alternating order, to create
// multiple phased light thinkers in a stepped sequence.
//
void PhasedLightThinker::SpawnSequence(sector_t *sector, int step)
{
   sector_t *sec, *nextSec, *tempSec;
   unsigned int flag = SECF_LIGHTSEQUENCE; // look for light sequence first
   int count = 1;

   // find the sectors and mark them all with SECF_PHASEDLIGHT
   // (the initial sector is already marked with that flag via its special)
   sec = sector;
   do
   {
      nextSec = nullptr;
      sec->intflags |= SIF_PHASESCAN; // make sure the search doesn't back up
      for(int i = 0; i < sec->linecount; i++)
      {
         if(!(tempSec = getNextSector(sec->lines[i], sec)))
            continue;
         if(tempSec->intflags & SIF_PHASESCAN) // already processed
            continue;
         if((tempSec->flags & (SECF_LIGHTSEQUENCE|SECF_LIGHTSEQALT)) == flag)
         {
            if(flag == SECF_LIGHTSEQUENCE)
               flag = SECF_LIGHTSEQALT;
            else
               flag = SECF_LIGHTSEQUENCE;
            nextSec = tempSec;
            ++count;
         }
      }
      sec = nextSec;
   }
   while(sec);

   sec = sector;
   count *= step;

   fixed_t index = 0;
   fixed_t indexDelta = 64 * FRACUNIT / count;
   int     base = sec->lightlevel;
   
   // spawn thinkers on all the marked sectors
   do
   {
      nextSec = nullptr;
      if(sec->lightlevel)
         base = sec->lightlevel;
      PhasedLightThinker::Spawn(sec, base, index >> FRACBITS);
      sec->intflags &= ~SIF_PHASESCAN; // clear the marker flag
      index += indexDelta;
      for(int i = 0; i < sec->linecount; i++)
      {
         if(!(tempSec = getNextSector(sec->lines[i], sec)))
            continue;
         if(tempSec->intflags & SIF_PHASESCAN)
            nextSec = tempSec;
      }
      sec = nextSec;
   }
   while(sec);
}

//////////////////////////////////////////////////////////
//
// Sector lighting type spawners
//
// After the map has been loaded, each sector is scanned
// for specials that spawn thinkers
//
//////////////////////////////////////////////////////////

//
// P_SpawnFireFlicker()
//
// Spawns a fire flicker lighting thinker
//
// Passed the sector that spawned the thinker
// Returns nothing
//
void P_SpawnFireFlicker(sector_t *sector)
{
   FireFlickerThinker *flick;
   
   // Note that we are resetting sector attributes.
   // Nothing special about it during gameplay.
   sector->special &= ~LIGHT_MASK; //jff 3/14/98 clear non-generalized sector type
   
   flick = new FireFlickerThinker;
   flick->addThinker();
   
   flick->sector = sector;
   flick->maxlight = sector->lightlevel;
   flick->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel)+16;
   flick->count = 4;
}

//
// P_SpawnLightFlash()
//
// Spawns a broken light flash lighting thinker
//
// Passed the sector that spawned the thinker
// Returns nothing
//
void P_SpawnLightFlash(sector_t *sector)
{
   LightFlashThinker *flash;
   
   // nothing special about it during gameplay
   sector->special &= ~LIGHT_MASK; //jff 3/14/98 clear non-generalized sector type
   
   flash = new LightFlashThinker;
   flash->addThinker();
   
   flash->sector = sector;
   flash->maxlight = sector->lightlevel;
   
   flash->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel);
   flash->maxtime = 64;
   flash->mintime = 7;
   flash->count = (P_Random(pr_lights)&flash->maxtime)+1;
   M_RandomLog("SpawnLightFlash\n");
}

//
// P_SpawnStrobeFlash
//
// Spawns a blinking light thinker
//
// Passed the sector that spawned the thinker, speed of blinking
// and whether blinking is to by syncrhonous with other sectors
//
// Returns nothing
//
void P_SpawnStrobeFlash(sector_t *sector, int darkTime, int brightTime,
                        int inSync)
{
   StrobeThinker *flash;
   
   flash = new StrobeThinker;
   flash->addThinker();
   
   flash->sector = sector;
   flash->darktime = darkTime;
   flash->brighttime = brightTime;
   flash->maxlight = sector->lightlevel;
   flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
   
   if(flash->minlight == flash->maxlight)
      flash->minlight = 0;
   
   // nothing special about it during gameplay
   sector->special &= ~LIGHT_MASK; //jff 3/14/98 clear non-generalized sector type
   
   if(!inSync)
   {
      flash->count = (P_Random(pr_lights)&7)+1;
      M_RandomLog("SpawnStrobeFlash\n");
   }
   else
      flash->count = 1;
}

//
// P_SpawnPSXStrobeFlash
//
// Spawn a PSX strobe effect.
//
void P_SpawnPSXStrobeFlash(sector_t *sector, int speed, bool inSync)
{
   auto flash = new StrobeThinker;
   flash->addThinker();

   flash->sector     = sector;
   flash->darktime   = speed;
   flash->brighttime = speed;
   flash->maxlight   = sector->lightlevel;
   flash->minlight   = P_FindMinSurroundingLight(sector, sector->lightlevel);

   if(flash->minlight == flash->maxlight)
      flash->minlight = 0;

   if(!inSync)
   {
      flash->count = (P_Random(pr_lights) & 7) + 1;
      M_RandomLog("SpawnPSXStrobeFlash\n");
   }
   else
      flash->count = 1;
}

//
// P_SpawnGlowingLight()
//
// Spawns a glowing light (smooth oscillation from min to max) thinker
//
// Passed the sector that spawned the thinker
// Returns nothing
//
void P_SpawnGlowingLight(sector_t *sector)
{
   auto g = new GlowThinker;
   g->addThinker();
   
   g->sector    = sector;
   g->minlight  = P_FindMinSurroundingLight(sector, sector->lightlevel);
   g->maxlight  = sector->lightlevel;
   g->direction = -1;
   
   sector->special &= ~LIGHT_MASK; //jff 3/14/98 clear non-generalized sector type
}

//
// P_SpawnPSXGlowingLight
//
// Spawn a SlowGlowThinker setup for PSX sector types 8, 200, and 201.
//
void P_SpawnPSXGlowingLight(sector_t *sector, psxglow_e glowtype)
{
   auto g = new SlowGlowThinker;
   g->addThinker();

   g->sector = sector;
   g->accum  = sector->lightlevel * FRACUNIT;

   switch(glowtype)
   {
   case psxglow_low:
      g->minlight  = P_FindMinSurroundingLight(sector, sector->lightlevel);
      g->maxlight  = sector->lightlevel;
      g->direction = -1;
      break;
   case psxglow_10:
      g->minlight  = 10;
      g->maxlight  = sector->lightlevel;
      g->direction = -1;
      break;
   case psxglow_255:
      g->minlight  = sector->lightlevel;
      g->maxlight  = 255;
      g->direction = 1;
      break;
   }
}

//////////////////////////////////////////////////////////
//
// Linedef lighting function handlers
//
//////////////////////////////////////////////////////////

//
// EV_StartLightStrobing()
//
// Start strobing lights (usually from a trigger)
//
// Passed the line that activated the strobing
// Returns true
//
// jff 2/12/98 added int return value, fixed return
//
int EV_StartLightStrobing(const line_t *line, int tag, int darkTime,
                          int brightTime, bool isParam)
{
   int   secnum = -1;
   sector_t* sec;

   bool manual = false;
   if(isParam && !tag)
   {
      if(!line || !(sec = line->backsector))
         return 0;
      manual = true;
      goto manualLight;
   }
   
   // start lights strobing in all sectors tagged same as line
   while(!manual && (secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
   manualLight:
      // if already doing a lighting function, don't start a second
      if(P_SectorActive(lighting_special,sec)) //jff 2/22/98
         continue;
      
      P_SpawnStrobeFlash(sec, darkTime, brightTime, 0);
   }
   return 1;
}

//
// EV_TurnTagLightsOff()
//
// Turn line's tagged sector's lights to min adjacent neighbor level
//
// Passed the line that activated the lights being turned off
// Returns true
//
// jff 2/12/98 added int return value, fixed return
//
int EV_TurnTagLightsOff(const line_t* line, int tag, bool isParam)
{
   int j = -1;
   // search sectors for those with same tag as activating line
   
   // MaxW: Param tag0 support
   bool manual = false;
   sector_t *sector;
   if(isParam && !tag)
   {
      if(!line || !(sector = line->backsector))
         return 0;
      manual = true;
      goto manualLight;
   }

   // killough 10/98: replaced inefficient search with fast search
   while((j = P_FindSectorFromTag(tag, j)) >= 0)
   {
      sector = sectors + j;

   manualLight:
      ;
      sector_t *tsec;
      int min = sector->lightlevel;
      
      // find min neighbor light level
      for(int i = 0; i < sector->linecount; i++)
      {
         if((tsec = getNextSector(sector->lines[i], sector)) &&
            tsec->lightlevel < min)
            min = tsec->lightlevel;
      }
      
      sector->lightlevel = min;

      if(manual)
         return 1;
   }

   return 1;
}

//
// EV_LightTurnOn()
//
// Turn sectors tagged to line lights on to specified or max neighbor level
//
// Passed the activating line, and a level to set the light to
// If level passed is 0, the maximum neighbor lighting is used
// Returns true
//
// jff 2/12/98 added int return value, fixed return
//
int EV_LightTurnOn(const line_t *line, int tag, int bright, bool isParam)
{
   int i = -1;
   
   // search all sectors for ones with same tag as activating line

   // ioanch: param tag0 support
   bool manual = false;
   sector_t *sector;
   if(isParam && !tag)
   {
      if(!line || !(sector = line->backsector))
         return 0;
      manual = true;
      goto manualLight;
   }

   // killough 10/98: replace inefficient search with fast search
   while((i = P_FindSectorFromTag(tag, i)) >= 0)
   {
      sector = sectors+i;

   manualLight:
      ;
      sector_t *temp;
      int tbright = bright; //jff 5/17/98 search for maximum PER sector
      
      // bright = 0 means to search for highest light level surrounding sector
      
      if(!bright)
      {
         for(int j = 0;j < sector->linecount; j++)
         {
            if((temp = getNextSector(sector->lines[j],sector)) &&
               temp->lightlevel > tbright)
               tbright = temp->lightlevel;
         }
      }

      sector->lightlevel = tbright;
      
      //jff 5/17/98 unless compatibility optioned 
      //then maximum near ANY tagged sector
      
      if(getComp(comp_model))
         bright = tbright;

      if(manual)
         return 1;
   }
   return 1;
}

// killough 10/98:
//
// EV_LightTurnOnPartway()
//
// Turn sectors tagged to line lights on to specified or max neighbor level
//
// Passed the activating line's tag, and a light level fraction between 
// 0 and 1. Sets the light to min on 0, max on 1, and interpolates in-
// between. Used for doors with gradual lighting effects.
//
// haleyjd 02/28/05: changed to take a tag instead of a line.
//
// Returns true
//
int EV_LightTurnOnPartway(int tag, fixed_t level)
{
   int i;
   
   if(level < 0)          // clip at extremes 
      level = 0;
   if(level > FRACUNIT)
      level = FRACUNIT;

   // search all sectors for ones with same tag as activating line
   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
      sector_t *temp, *sector = sectors + i;
      int j, bright = 0, min = sector->lightlevel;

      for(j = 0; j < sector->linecount; ++j)
      {
         if((temp = getNextSector(sector->lines[j], sector)))
         {
            if(temp->lightlevel > bright)
               bright = temp->lightlevel;
            if(temp->lightlevel < min)
               min = temp->lightlevel;
         }
      }

      sector->lightlevel =   // Set level in-between extremes
         (level * bright + (FRACUNIT - level) * min) >> FRACBITS;
   }
   return 1;
}

//==============================================================================
//
// haleyjd 01/09/07: Parameterized Lighting
//

//
// EV_SetLight
//
// haleyjd 01/09/07: Depending on the value of "type", this function
// sets, adds to, or subtracts from all tagged sectors' light levels.
//
int EV_SetLight(const line_t *line, int tag, setlight_e type, int lvl)
{
   int i, rtn = 0;
   sector_t *s;
   bool backside = false;

   if(line && tag == 0)
   {
      if(!line->backsector)
         return rtn;
      i = eindex(line->backsector - sectors);
      backside = true;
      goto dobackside;
   }

   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {      
dobackside:
      s = &sectors[i];

      rtn = 1; // if any sector is changed, we return 1

      switch(type)
      {
      case setlight_set:
         s->lightlevel = lvl;
         break;
      case setlight_add:
         s->lightlevel += lvl;
         break;
      case setlight_sub:
         s->lightlevel -= lvl;
         break;
      }

      if(s->lightlevel < 0)
         s->lightlevel = 0;
      else if(s->lightlevel > 255)
         s->lightlevel = 255;

      if(backside)
         break;
   }

   return rtn;
}

//
// EV_FadeLight
//
// sf 13/10/99:
// Fade all the lights in sectors with a particular tag to a new value
//
// haleyjd 01/10/07: changes for param specs
//
int EV_FadeLight(const line_t *line, int tag, int destvalue, int speed)
{
   int i, rtn = 0;
   LightFadeThinker *lf;
   bool backside = false;

   // speed <= 0? hell no.
   if(speed <= 0)
      return rtn;

   if(line && tag == 0)
   {
      if(!line->backsector)
         return rtn;
      i = eindex(line->backsector - sectors);
      backside = true;
      goto dobackside;
   }
   
   // search all sectors for ones with tag
   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
dobackside:
      rtn = 1;

      lf = new LightFadeThinker;
      lf->addThinker();       // add thinker

      lf->sector = &sectors[i];
      
      lf->destlevel  = destvalue * FRACUNIT;               // dest. light level
      lf->lightlevel = lf->sector->lightlevel * FRACUNIT;  // curr. light level
      lf->step = (lf->destlevel - lf->lightlevel) / speed; // delta per frame

      lf->type = fade_once;

      if(backside)
         break;
   }

   return rtn;
}

//
// EV_GlowLight
//
// haleyjd:
// Parameterized glow effect. Uses the fading thinker, but sets it up to
// perpetually fade between two different light levels. The glow always starts
// by fading the sector toward the lower light level, whether that requires
// fading up or down.
//
int EV_GlowLight(const line_t *line, int tag, int maxval, int minval, int speed)
{
   int i, rtn = 0;
   LightFadeThinker *lf;
   bool backside = false;

   // speed <= 0? hell no.
   if(speed <= 0 || maxval == minval)
      return rtn;

   // ensure min and max have proper relationship
   if(maxval < minval)
   {
      int temp = maxval;
      maxval = minval;
      minval = temp;
   }

   if(line && tag == 0)
   {
      if(!line->backsector)
         return rtn;
      i = eindex(line->backsector - sectors);
      backside = true;
      goto dobackside;
   }
   
   // search all sectors for ones with tag
   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
dobackside:
      rtn = 1;

      lf = new LightFadeThinker;
      lf->addThinker();

      lf->sector = &sectors[i];

      lf->glowmin   = minval * FRACUNIT;
      lf->glowmax   = maxval * FRACUNIT;
      lf->glowspeed = speed;

      // start out fading to min level
      lf->destlevel  = lf->glowmin;                        // dest. light level
      lf->lightlevel = lf->sector->lightlevel * FRACUNIT;  // curr. light level
      lf->step = (lf->destlevel - lf->lightlevel) / speed; // delta per frame

      lf->type = fade_glow;

      if(backside)
         return rtn;
   }

   return rtn;
}

//
// EV_StrobeLight
//
// haleyjd 01/16/07: Parameterized light strobe effect.
// Allows strobing the light of a sector between any two light levels with
// independent durations for both levels. Uses the same thinker as the normal
// strobe light effect.
//
int EV_StrobeLight(const line_t *line, int tag,
                   int maxval, int minval, int maxtime, int mintime)
{
   StrobeThinker *flash;
   int i, rtn = 0;
   bool backside = false;

   if(line && tag == 0)
   {
      if(!line->backsector)
         return rtn;
      i = eindex(line->backsector - sectors);
      backside = true;
      goto dobackside;
   }
   
   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
dobackside:
      rtn = 1;
      flash = new StrobeThinker;
      flash->addThinker();
      
      flash->sector     = &sectors[i];
      flash->maxlight   = maxval;
      flash->minlight   = minval;
      flash->brighttime = maxtime;
      flash->darktime   = mintime;
      flash->count      = 1;

      flash->sector->lightlevel = flash->maxlight;

      if(backside)
         return rtn;
   }

   return rtn;
}

//
// EV_FlickerLight
//
// haleyjd 01/16/07:
// Parameterized flickering light effect. Changes between maxval and minval
// at a randomized time period between 0.2 and 1.8 seconds (7 to 64 tics).
// Uses the normal lightflash thinker.
//
int EV_FlickerLight(const line_t *line, int tag, int maxval, int minval)
{
   LightFlashThinker *flash;
   int i, rtn = 0;
   bool backside = false;

   if(line && tag == 0)
   {
      if(!line->backsector)
         return rtn;
      i = eindex(line->backsector - sectors);
      backside = true;
      goto dobackside;
   }
   
   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
dobackside:
      rtn = 1;
      flash = new LightFlashThinker;
      flash->addThinker();
      
      flash->sector   = &sectors[i];
      flash->maxlight = maxval;
      flash->minlight = minval;
      flash->maxtime  = 64;
      flash->mintime  = 7;
      flash->count    = (P_Random(pr_lights) & flash->maxtime) + 1;
      M_RandomLog("EVFlickerLight\n");

      flash->sector->lightlevel = flash->maxlight;

      if(backside)
         return rtn;
   }

   return rtn;
}

//----------------------------------------------------------------------------
//
// $Log: p_lights.c,v $
// Revision 1.11  1998/05/18  09:04:41  jim
// fix compatibility decl
//
// Revision 1.10  1998/05/17  11:31:36  jim
// fixed bug in lights to max neighbor
//
// Revision 1.9  1998/05/09  18:57:50  jim
// formatted/documented p_lights
//
// Revision 1.8  1998/05/03  23:17:23  killough
// Fix #includes at the top, nothing else
//
// Revision 1.7  1998/03/15  14:40:10  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.6  1998/02/23  23:46:56  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.5  1998/02/23  00:41:51  jim
// Implemented elevators
//
// Revision 1.4  1998/02/17  06:07:11  killough
// Change RNG calling sequence
//
// Revision 1.3  1998/02/13  03:28:42  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.2  1998/01/26  19:24:07  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

