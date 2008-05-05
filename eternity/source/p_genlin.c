// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
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
// DESCRIPTION:
//  Generalized linedef type handlers
//  Floors, Ceilings, Doors, Locked Doors, Lifts, Stairs, Crushers
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "r_main.h"
#include "p_info.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_xenemy.h"
#include "m_random.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"
#include "a_small.h"
#include "e_exdata.h"
#include "acs_intr.h"
#include "c_runcmd.h"
#include "c_io.h"
#include "c_net.h"

//////////////////////////////////////////////////////////
//
// Generalized Linedef Type handlers
//
//////////////////////////////////////////////////////////

int EV_DoParamFloor(line_t *line, int tag, floordata_t *fd)
{
   int         secnum;
   int         rtn = 0;
   boolean     manual = false;
   sector_t    *sec;
   floormove_t *floor;

   // check if a manual trigger, if so do just the sector on the backside
   // haleyjd 05/07/04: only line actions can be manual
   if(fd->trigger_type == PushOnce || fd->trigger_type == PushMany)
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = sec - sectors;
      manual = true;
      goto manual_floor;
   }

   secnum = -1;
   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
manual_floor:                
      // Do not start another function if floor already moving
      if(P_SectorActive(floor_special,sec))
      {
         if(manual)
            return rtn;
         continue;
      }

      // new floor thinker
      rtn = 1;
      floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
      P_AddThinker(&floor->thinker);
      sec->floordata = floor;
      
      floor->thinker.function = T_MoveFloor;
      floor->crush = fd->crush;
      floor->direction = fd->direction ? plat_up : plat_down;
      floor->sector = sec;
      floor->texture = sec->floorpic;
      //jff 3/14/98 transfer old special field too
      P_SetupSpecialTransfer(sec, &(floor->special));
      floor->type = genFloor;

      // set the speed of motion
      switch(fd->speed_type)
      {
      case SpeedSlow:
         floor->speed = FLOORSPEED;
         break;
      case SpeedNormal:
         floor->speed = FLOORSPEED*2;
         break;
      case SpeedFast:
         floor->speed = FLOORSPEED*4;
         break;
      case SpeedTurbo:
         floor->speed = FLOORSPEED*8;
         break;
      case SpeedParam: // haleyjd 05/07/04: parameterized extension
         floor->speed = fd->speed_value;
         break;
      default:
         break;
      }

      // set the destination height
      switch(fd->target_type)
      {
      case FtoHnF:
         floor->floordestheight = P_FindHighestFloorSurrounding(sec);
         break;
      case FtoLnF:
         floor->floordestheight = P_FindLowestFloorSurrounding(sec);
         break;
      case FtoNnF:
         floor->floordestheight = fd->direction ?
            P_FindNextHighestFloor(sec,sec->floorz) :
            P_FindNextLowestFloor(sec,sec->floorz);
         break;
      case FtoLnC:
         floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
         break;
      case FtoC:
         floor->floordestheight = sec->ceilingz;
         break;
      case FbyST:
         floor->floordestheight = 
            (floor->sector->floorz>>FRACBITS) + floor->direction * 
            (P_FindShortestTextureAround(secnum)>>FRACBITS);
         if(floor->floordestheight>32000)  //jff 3/13/98 prevent overflow
            floor->floordestheight=32000;    // wraparound in floor height
         if(floor->floordestheight<-32000)
            floor->floordestheight=-32000;
         floor->floordestheight<<=FRACBITS;
         break;
      case Fby24:
         floor->floordestheight = floor->sector->floorz +
            floor->direction * 24*FRACUNIT;
         break;
      case Fby32:
         floor->floordestheight = floor->sector->floorz +
            floor->direction * 32*FRACUNIT;
         break;
      
         // haleyjd 05/07/04: parameterized extensions
         //         05/20/05: added FtoAbs, FInst
      case FbyParam: 
         floor->floordestheight = floor->sector->floorz +
            floor->direction * fd->height_value;
         break;
      case FtoAbs:
         floor->floordestheight = fd->height_value;
         // adjust direction appropriately (instant movement not possible)
         if(floor->floordestheight > floor->sector->floorz)
            floor->direction = plat_up;
         else
            floor->direction = plat_down;
         break;
      case FInst:
         floor->floordestheight = floor->sector->floorz +
            floor->direction * fd->height_value;
         // adjust direction appropriately (always instant)
         if(floor->floordestheight > floor->sector->floorz)
            floor->direction = plat_down;
         else
            floor->direction = plat_up;
         break;
      default:
         break;
      }

      // set texture/type change properties
      if(fd->change_type)   // if a texture change is indicated
      {
         if(fd->change_model) // if a numeric model change
         {
            sector_t *sec;

            //jff 5/23/98 find model with ceiling at target height
            //if target is a ceiling type
            sec = (fd->target_type == FtoLnC || fd->target_type == FtoC)?
               P_FindModelCeilingSector(floor->floordestheight,secnum) :
               P_FindModelFloorSector(floor->floordestheight,secnum);
            
            if(sec)
            {
               floor->texture = sec->floorpic;
               switch(fd->change_type)
               {
               case FChgZero:  // zero type
                  //jff 3/14/98 change old field too
                  P_ZeroSpecialTransfer(&(floor->special));
                  floor->type = genFloorChg0;
                  break;
               case FChgTyp:   // copy type
                  //jff 3/14/98 change old field too
                  P_SetupSpecialTransfer(sec, &(floor->special));
                  floor->type = genFloorChgT;
                  break;
               case FChgTxt:   // leave type be
                  floor->type = genFloorChg;
                  break;
               default:
                  break;
               }
            }
         }
         else     // else if a trigger model change
         {
            if(line) // haleyjd 05/07/04: only line actions can use this
            {
               floor->texture = line->frontsector->floorpic;
               switch(fd->change_type)
               {
               case FChgZero:    // zero type
                  //jff 3/14/98 change old field too
                  P_ZeroSpecialTransfer(&(floor->special));
                  floor->type = genFloorChg0;
                  break;
               case FChgTyp:     // copy type
                  //jff 3/14/98 change old field too
                  P_SetupSpecialTransfer(line->frontsector, &(floor->special));
                  floor->type = genFloorChgT;
                  break;
               case FChgTxt:     // leave type be
                  floor->type = genFloorChg;
               default:
                  break;
               }
            } // end if(line)
         }
      }

      P_FloorSequence(floor->sector);
      if(manual)
         return rtn;
   }
   return rtn;
}

//
// EV_DoGenFloor()
//
// Handle generalized floor types
//
// Passed the line activating the generalized floor function
// Returns true if a thinker is created
//
// jff 02/04/98 Added this routine (and file) to handle generalized
// floor movers using bit fields in the line special type.
//
// haleyjd 05/07/04: rewritten to use EV_DoParamFloor
//
int EV_DoGenFloor(line_t *line)
{
   floordata_t fd;
   unsigned value = (unsigned)line->special - GenFloorBase;

   // parse the bit fields in the line's special type
   
   fd.crush        = ((value & FloorCrush) >> FloorCrushShift) ? 10 : -1;
   fd.change_type  = (value & FloorChange) >> FloorChangeShift;
   fd.target_type  = (value & FloorTarget) >> FloorTargetShift;
   fd.direction    = (value & FloorDirection) >> FloorDirectionShift;
   fd.change_model = (value & FloorModel) >> FloorModelShift;
   fd.speed_type   = (value & FloorSpeed) >> FloorSpeedShift;
   fd.trigger_type = (value & TriggerType) >> TriggerTypeShift;

   return EV_DoParamFloor(line, line->tag, &fd);
}

//
// EV_DoParamCeiling
//
int EV_DoParamCeiling(line_t *line, int tag, ceilingdata_t *cd)
{
   int       secnum;
   int       rtn = 0;
   boolean   manual = false;
   fixed_t   targheight;
   sector_t  *sec;
   ceiling_t *ceiling;

   // check if a manual trigger, if so do just the sector on the backside
   if(cd->trigger_type == PushOnce || cd->trigger_type == PushMany)
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = sec - sectors;
      manual = true;
      goto manual_ceiling;
   }

   secnum = -1;
   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];

manual_ceiling:                
      // Do not start another function if ceiling already moving
      if(P_SectorActive(ceiling_special, sec)) //jff 2/22/98
      {
         if(manual)
            return rtn;
         continue;
      }

      // new ceiling thinker
      rtn = 1;
      ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
      P_AddThinker(&ceiling->thinker);
      sec->ceilingdata = ceiling; //jff 2/22/98

      ceiling->thinker.function = T_MoveCeiling;
      ceiling->crush = cd->crush;
      ceiling->direction = cd->direction ? plat_up : plat_down;
      ceiling->sector = sec;
      ceiling->texture = sec->ceilingpic;
      //jff 3/14/98 change old field too
      P_SetupSpecialTransfer(sec, &(ceiling->special));
      ceiling->tag = sec->tag;
      ceiling->type = genCeiling;

      // set speed of motion
      switch(cd->speed_type)
      {
      case SpeedSlow:
         ceiling->speed = CEILSPEED;
         break;
      case SpeedNormal:
         ceiling->speed = CEILSPEED*2;
         break;
      case SpeedFast:
         ceiling->speed = CEILSPEED*4;
         break;
      case SpeedTurbo:
         ceiling->speed = CEILSPEED*8;
         break;
      case SpeedParam: // haleyjd 10/06/05: parameterized extension
         ceiling->speed = cd->speed_value;
         break;
      default:
         break;
      }

      // set destination target height
      targheight = sec->ceilingz;
      switch(cd->target_type)
      {
      case CtoHnC:
         targheight = P_FindHighestCeilingSurrounding(sec);
         break;
      case CtoLnC:
         targheight = P_FindLowestCeilingSurrounding(sec);
         break;
      case CtoNnC:
         targheight = cd->direction ?
            P_FindNextHighestCeiling(sec,sec->ceilingz) :
            P_FindNextLowestCeiling(sec,sec->ceilingz);
         break;
      case CtoHnF:
         targheight = P_FindHighestFloorSurrounding(sec);
         break;
      case CtoF:
         targheight = sec->floorz;
         break;
      case CbyST:
         targheight = (ceiling->sector->ceilingz>>FRACBITS) +
            ceiling->direction * (P_FindShortestUpperAround(secnum)>>FRACBITS);
         if(targheight > 32000)  // jff 3/13/98 prevent overflow
            targheight = 32000;  // wraparound in ceiling height
         if(targheight < -32000)
            targheight = -32000;
         targheight <<= FRACBITS;
         break;
      case Cby24:
         targheight = ceiling->sector->ceilingz +
            ceiling->direction * 24*FRACUNIT;
         break;
      case Cby32:
         targheight = ceiling->sector->ceilingz +
            ceiling->direction * 32*FRACUNIT;
         break;

         // haleyjd 10/06/05: parameterized extensions
      case CbyParam:
         targheight = ceiling->sector->ceilingz +
            ceiling->direction * cd->height_value;
         break;
      case CtoAbs:
         targheight = cd->height_value;
         // adjust direction appropriately (instant movement not possible)
         if(targheight > ceiling->sector->ceilingz)
            ceiling->direction = plat_up;
         else
            ceiling->direction = plat_down;
         break;
      case CInst:
         targheight = ceiling->sector->ceilingz +
            ceiling->direction * cd->height_value;
         // adjust direction appropriately (always instant)
         if(targheight > ceiling->sector->ceilingz)
            ceiling->direction = plat_down;
         else
            ceiling->direction = plat_up;
         break;
      default:
         break;
      }
    
      if(cd->direction)
         ceiling->topheight = targheight;
      else
         ceiling->bottomheight = targheight;

      // set texture/type change properties
      if(cd->change_type)     // if a texture change is indicated
      {
         if(cd->change_model)   // if a numeric model change
         {
            sector_t *sec;

            // jff 5/23/98 find model with floor at target height if 
            // target is a floor type
            sec = (cd->target_type == CtoHnF || cd->target_type == CtoF) ?
                     P_FindModelFloorSector(targheight, secnum) :
                     P_FindModelCeilingSector(targheight, secnum);
            if(sec)
            {
               ceiling->texture = sec->ceilingpic;
               switch(cd->change_type)
               {
               case CChgZero:  // type is zeroed
                  //jff 3/14/98 change old field too
                  P_ZeroSpecialTransfer(&(ceiling->special));
                  ceiling->type = genCeilingChg0;
                  break;
               case CChgTyp:   // type is copied
                  //jff 3/14/98 change old field too
                  P_SetupSpecialTransfer(sec, &(ceiling->special));
                  ceiling->type = genCeilingChgT;
                  break;
               case CChgTxt:   // type is left alone
                  ceiling->type = genCeilingChg;
                  break;
               default:
                  break;
               }
            }
         }
         else        // else if a trigger model change
         {
            if(line) // haleyjd 10/05/05: only line actions can use this
            {
               ceiling->texture = line->frontsector->ceilingpic;
               switch(cd->change_type)
               {
               case CChgZero:    // type is zeroed
                  //jff 3/14/98 change old field too
                  P_ZeroSpecialTransfer(&(ceiling->special));
                  ceiling->type = genCeilingChg0;
                  break;
               case CChgTyp:     // type is copied
                  //jff 3/14/98 change old field too
                  P_SetupSpecialTransfer(line->frontsector, &(ceiling->special));
                  ceiling->type = genCeilingChgT;
                  break;
               case CChgTxt:     // type is left alone
                  ceiling->type = genCeilingChg;
                  break;
               default:
                  break;
               }
            }
         }
      }
      P_AddActiveCeiling(ceiling); // add this ceiling to the active list
      P_CeilingSequence(ceiling->sector, CNOISE_NORMAL); // haleyjd 09/29/06
      if(manual)
         return rtn;
   }
   return rtn;
}

//
// EV_DoGenCeiling()
//
// Handle generalized ceiling types
//
// Passed the linedef activating the ceiling function
// Returns true if a thinker created
//
// jff 02/04/98 Added this routine (and file) to handle generalized
// floor movers using bit fields in the line special type.
//
int EV_DoGenCeiling(line_t *line)
{
   ceilingdata_t cd;
   unsigned value = (unsigned)line->special - GenCeilingBase;

   // parse the bit fields in the line's special type
   
   cd.crush        = ((value & CeilingCrush) >> CeilingCrushShift) ? 10 : -1;
   cd.change_type  = (value & CeilingChange) >> CeilingChangeShift;
   cd.target_type  = (value & CeilingTarget) >> CeilingTargetShift;
   cd.direction    = (value & CeilingDirection) >> CeilingDirectionShift;
   cd.change_model = (value & CeilingModel) >> CeilingModelShift;
   cd.speed_type   = (value & CeilingSpeed) >> CeilingSpeedShift;
   cd.trigger_type = (value & TriggerType) >> TriggerTypeShift;

   return EV_DoParamCeiling(line, line->tag, &cd);
}

//
// EV_DoGenLift()
//
// Handle generalized lift types
//
// Passed the linedef activating the lift
// Returns true if a thinker is created
//
int EV_DoGenLift(line_t *line)
{
   plat_t   *plat;
   int      secnum;
   int      rtn;
   boolean  manual;
   sector_t *sec;
   unsigned value = (unsigned)line->special - GenLiftBase;

   // parse the bit fields in the line's special type
   
   int Targ = (value & LiftTarget) >> LiftTargetShift;
   int Dely = (value & LiftDelay) >> LiftDelayShift;
   int Sped = (value & LiftSpeed) >> LiftSpeedShift;
   int Trig = (value & TriggerType) >> TriggerTypeShift;

   secnum = -1;
   rtn = 0;
   
   // Activate all <type> plats that are in_stasis
   
   if(Targ==LnF2HnF)
      P_ActivateInStasis(line->tag);
        
   // check if a manual trigger, if so do just the sector on the backside
   manual = false;
   if(Trig==PushOnce || Trig==PushMany)
   {
      if (!(sec = line->backsector))
         return rtn;
      secnum = sec-sectors;
      manual = true;
      goto manual_lift;
   }

   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
manual_lift:
      // Do not start another function if floor already moving
      if(P_SectorActive(floor_special, sec))
      {
         if(!manual)
            continue;
         else
            return rtn;
      }
      
      // Setup the plat thinker
      rtn = 1;
      plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
      P_AddThinker(&plat->thinker);
      
      plat->sector = sec;
      plat->sector->floordata = plat;
      plat->thinker.function = T_PlatRaise;
      plat->crush = -1;
      plat->tag = line->tag;
      
      plat->type = genLift;
      plat->high = sec->floorz;
      plat->status = down;

      // setup the target destination height
      switch(Targ)
      {
      case F2LnF:
         plat->low = P_FindLowestFloorSurrounding(sec);
         if(plat->low > sec->floorz)
            plat->low = sec->floorz;
         break;
      case F2NnF:
         plat->low = P_FindNextLowestFloor(sec,sec->floorz);
         break;
      case F2LnC:
         plat->low = P_FindLowestCeilingSurrounding(sec);
         if(plat->low > sec->floorz)
            plat->low = sec->floorz;
         break;
      case LnF2HnF:
         plat->type = genPerpetual;
         plat->low = P_FindLowestFloorSurrounding(sec);
         if(plat->low > sec->floorz)
            plat->low = sec->floorz;
         plat->high = P_FindHighestFloorSurrounding(sec);
         if(plat->high < sec->floorz)
            plat->high = sec->floorz;
         plat->status = P_Random(pr_genlift)&1;
         break;
      default:
         break;
      }

      // setup the speed of motion
      switch(Sped)
      {
      case SpeedSlow:
         plat->speed = PLATSPEED * 2;
         break;
      case SpeedNormal:
         plat->speed = PLATSPEED * 4;
         break;
      case SpeedFast:
         plat->speed = PLATSPEED * 8;
         break;
      case SpeedTurbo:
         plat->speed = PLATSPEED * 16;
         break;
      default:
         break;
      }

      // setup the delay time before the floor returns
      switch(Dely)
      {
      case 0:
         plat->wait = 1*35;
         break;
      case 1:
         plat->wait = PLATWAIT*35;
         break;
      case 2:
         plat->wait = 5*35;
         break;
      case 3:
         plat->wait = 10*35;
         break;
      }

      P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
      P_AddActivePlat(plat); // add this plat to the list of active plats
      
      if(manual)
         return rtn;
   }
   return rtn;
}

//
// EV_DoParamStairs()
//
// Handle parameterized stair building
//
// Returns true if a thinker is created
//
int EV_DoParamStairs(line_t *line, int tag, stairdata_t *sd)
{
   int      secnum;
   int      osecnum; //jff 3/4/98 preserve loop index
   int      height;
   int      i;
   int      newsecnum;
   int      texture;
   int      ok;
   int      rtn = 0;
   boolean  manual = false;
    
   sector_t *sec;
   sector_t *tsec;
   
   floormove_t *floor;
   
   fixed_t  stairsize;
   fixed_t  speed;  
   
   // check if a manual trigger, if so do just the sector on the backside
   // haleyjd 10/06/05: only line actions can be manual
   if(sd->trigger_type == PushOnce || sd->trigger_type == PushMany)
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = sec - sectors;
      manual = true;
      goto manual_stair;
   }

   secnum = -1;
   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
manual_stair:          
      //Do not start another function if floor already moving
      //jff 2/26/98 add special lockout condition to wait for entire
      //staircase to build before retriggering
      if(P_SectorActive(floor_special, sec) || sec->stairlock)
      {
         if(manual)
            return rtn;
         continue;
      }
      
      // new floor thinker
      rtn = 1;
      floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, NULL);
      P_AddThinker(&floor->thinker);
      sec->floordata = floor;

      floor->thinker.function = T_MoveFloor;
      floor->direction = sd->direction ? plat_up : plat_down;
      floor->sector = sec;

      // setup speed of stair building
      switch(sd->speed_type)
      {
      default:
      case SpeedSlow:
         floor->speed = FLOORSPEED/4;
         break;
      case SpeedNormal:
         floor->speed = FLOORSPEED/2;
         break;
      case SpeedFast:
         floor->speed = FLOORSPEED*2;
         break;
      case SpeedTurbo:
         floor->speed = FLOORSPEED*4;
         break;
      case SpeedParam:
         // haleyjd 10/06/05: parameterized extension
         floor->speed = sd->speed_value;
         if(floor->speed == 0)
            floor->speed = FLOORSPEED/4; // no zero-speed stairs
         break;
      }

      // setup stepsize for stairs
      switch(sd->stepsize_type)
      {
      default:
      case StepSize4:
         stairsize = 4*FRACUNIT;
         break;
      case StepSize8:
         stairsize = 8*FRACUNIT;
         break;
      case StepSize16:
         stairsize = 16*FRACUNIT;
         break;
      case StepSize24:
         stairsize = 24*FRACUNIT;
         break;
      case StepSizeParam:
         // haleyjd 10/06/05: parameterized extension
         stairsize = sd->stepsize_value;
         if(stairsize <= 0) // no zero-or-less stairs
            stairsize = 4*FRACUNIT;
         break;
      }

      speed = floor->speed;
      height = sec->floorz + floor->direction * stairsize;
      floor->floordestheight = height;
      texture = sec->floorpic;
      floor->crush = -1;
      floor->type = genBuildStair; // jff 3/31/98 do not leave uninited

      // haleyjd 10/13/05: init reset and delay properties
      floor->resetTime     = sd->reset_value;
      floor->resetHeight   = sec->floorz;
      floor->delayTime     = sd->delay_value;      
      floor->stepRaiseTime = FixedDiv(stairsize, speed) >> FRACBITS;
      floor->delayTimer    = floor->delayTime ? floor->stepRaiseTime : 0;

      sec->stairlock = -2;         // jff 2/26/98 set up lock on current sector
      sec->nextsec   = -1;
      sec->prevsec   = -1;

      P_FloorSequence(floor->sector);
      
      osecnum = secnum;            // jff 3/4/98 preserve loop index  
      // Find next sector to raise
      // 1.     Find 2-sided line with same sector side[0]
      // 2.     Other side is the next sector to raise
      do
      {
         ok = 0;
         for(i = 0; i < sec->linecount; ++i)
         {
            if(!((sec->lines[i])->backsector))
               continue;
            
            tsec = (sec->lines[i])->frontsector;
            newsecnum = tsec-sectors;
            
            if(secnum != newsecnum)
               continue;
            
            tsec = (sec->lines[i])->backsector;
            newsecnum = tsec - sectors;
            
            if(!sd->ignore && tsec->floorpic != texture)
               continue;

            // jff 6/19/98 prevent double stepsize
            // killough 10/98: corrected use of demo compatibility flag
            if(demo_version < 202)
               height += floor->direction * stairsize;

            //jff 2/26/98 special lockout condition for retriggering
            if(P_SectorActive(floor_special, tsec) || tsec->stairlock)
               continue;

            // jff 6/19/98 increase height AFTER continue        
            // killough 10/98: corrected use of demo compatibility flag
            if(demo_version >= 202)
               height += floor->direction * stairsize;

            // jff 2/26/98
            // link the stair chain in both directions
            // lock the stair sector until building complete
            sec->nextsec = newsecnum; // link step to next
            tsec->prevsec = secnum;   // link next back
            tsec->nextsec = -1;       // set next forward link as end
            tsec->stairlock = -2;     // lock the step
            
            sec = tsec;
            secnum = newsecnum;
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, NULL);
            
            P_AddThinker(&floor->thinker);
            
            sec->floordata = floor;
            floor->thinker.function = T_MoveFloor;
            floor->direction = sd->direction ? plat_up : plat_down;
            floor->sector = sec;

            // haleyjd 10/06/05: support synchronized stair raising
            if(sd->sync_value)
            {
               floor->speed = 
                  D_abs(FixedMul(speed, 
                        FixedDiv(height - sec->floorz, stairsize)));
            }
            else
               floor->speed = speed;
            
            floor->floordestheight = height;
            floor->crush = -1;
            floor->type = genBuildStair; // jff 3/31/98 do not leave uninited

            // haleyjd 10/13/05: init reset and delay properties
            floor->resetTime     = sd->reset_value;
            floor->resetHeight   = sec->floorz;
            floor->delayTime     = sd->delay_value;      
            floor->stepRaiseTime = FixedDiv(stairsize, speed) >> FRACBITS;            
            floor->delayTimer    = floor->delayTime ? floor->stepRaiseTime : 0;

            P_FloorSequence(floor->sector);
            
            ok = 1;
            break;
         }
      } while(ok);
      if(manual)
         return rtn;
      secnum = osecnum; //jff 3/4/98 restore old loop index
   }

   return rtn;
}

//
// EV_DoGenStairs()
//
// Handle generalized stair building
//
// Passed the linedef activating the stairs
// Returns true if a thinker is created
//
int EV_DoGenStairs(line_t *line)
{
   stairdata_t sd;
   int         rtn;
   unsigned    value = (unsigned)line->special - GenStairsBase;

   // parse the bit fields in the line's special type
   
   sd.ignore        = (value & StairIgnore) >> StairIgnoreShift;
   sd.direction     = (value & StairDirection) >> StairDirectionShift;
   sd.stepsize_type = (value & StairStep) >> StairStepShift;
   sd.speed_type    = (value & StairSpeed) >> StairSpeedShift;
   sd.trigger_type  = (value & TriggerType) >> TriggerTypeShift;
   
   // haleyjd 10/06/05: generalized stairs don't support the following
   sd.sync_value    = 0;
   sd.delay_value   = 0;
   sd.reset_value   = 0;

   rtn = EV_DoParamStairs(line, line->tag, &sd);

   // retriggerable generalized stairs build up or down alternately
   if(rtn)
      line->special ^= StairDirection; // alternate dir on succ activations

   return rtn;
}

//
// EV_DoGenCrusher()
//
// Handle generalized crusher types
//
// Passed the linedef activating the crusher
// Returns true if a thinker created
//
int EV_DoGenCrusher(line_t *line)
{
   int       secnum;
   int       rtn;
   boolean   manual;
   sector_t  *sec;
   ceiling_t *ceiling;
   unsigned  value = (unsigned)line->special - GenCrusherBase;
   
   // parse the bit fields in the line's special type
   
   int Slnt = (value & CrusherSilent) >> CrusherSilentShift;
   int Sped = (value & CrusherSpeed) >> CrusherSpeedShift;
   int Trig = (value & TriggerType) >> TriggerTypeShift;

   //jff 2/22/98  Reactivate in-stasis ceilings...for certain types.
   //jff 4/5/98 return if activated
   rtn = P_ActivateInStasisCeiling(line);

   // check if a manual trigger, if so do just the sector on the backside
   manual = false;
   if(Trig==PushOnce || Trig==PushMany)
   {
      if(!(sec = line->backsector))
         return rtn;
      secnum = sec-sectors;
      manual = true;
      goto manual_crusher;
   }
   
   secnum = -1;
   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
manual_crusher:                
      // Do not start another function if ceiling already moving
      if(P_SectorActive(ceiling_special,sec)) //jff 2/22/98
      {
         if(!manual)
            continue;
         else
            return rtn;
      }

      // new ceiling thinker
      rtn = 1;
      ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVSPEC, 0);
      P_AddThinker (&ceiling->thinker);
      sec->ceilingdata = ceiling; //jff 2/22/98
      ceiling->thinker.function = T_MoveCeiling;
      ceiling->crush = 10;
      ceiling->direction = plat_down;
      ceiling->sector = sec;
      ceiling->texture = sec->ceilingpic;
      // haleyjd: note: transfer isn't actually used by crushers...
      P_SetupSpecialTransfer(sec, &(ceiling->special));
      ceiling->tag = sec->tag;
      ceiling->type = Slnt? genSilentCrusher : genCrusher;
      ceiling->topheight = sec->ceilingz;
      ceiling->bottomheight = sec->floorz + (8*FRACUNIT);

      // setup ceiling motion speed
      switch (Sped)
      {
      case SpeedSlow:
         ceiling->speed = CEILSPEED;
         break;
      case SpeedNormal:
         ceiling->speed = CEILSPEED*2;
         break;
      case SpeedFast:
         ceiling->speed = CEILSPEED*4;
         break;
      case SpeedTurbo:
         ceiling->speed = CEILSPEED*8;
         break;
      default:
         break;
      }
      ceiling->oldspeed=ceiling->speed;

      P_AddActiveCeiling(ceiling); // add to list of active ceilings
      // haleyjd 09/29/06
      P_CeilingSequence(ceiling->sector, Slnt ? CNOISE_SILENT : CNOISE_NORMAL);

      if(manual)
         return rtn;
   }
   return rtn;
}

// haleyjd 02/23/04: yuck, a global -- this is necessary because
// I can't change the prototype of EV_DoGenDoor
mobj_t *genDoorThing;

//
// GenDoorRetrigger
//
// haleyjd 02/23/04: This function handles retriggering of certain
// active generalized door types, a functionality which was neglected 
// in BOOM. To be retriggerable, the door must fit these criteria:
// 1. The thinker on the sector must be a T_VerticalDoor
// 2. The door type must be raise, not open or close
// 3. The activation trigger must be PushMany
//
// ** genDoorThing must be set before the calling routine is
//    executed! If it is NULL, no retrigger can occur.
//
static int GenDoorRetrigger(vldoor_t *door, int trig)
{
   if(genDoorThing && door->thinker.function == T_VerticalDoor &&
      (door->type == genRaise || door->type == genBlazeRaise) &&
      trig == PushMany)
   {
      if(door->direction == plat_down) // door is closing
         door->direction = plat_up;
      else
      {
         // monsters will not close doors
         if(!genDoorThing->player)
            return 0;
         door->direction = plat_down;
      }

      // haleyjd: squash the sector's sound sequence
      S_SquashSectorSequence(door->sector, true);
      
      return 1;
   }

   return 0;
}

//
// EV_DoParamDoor
//
// haleyjd 05/04/04: Parameterized extension of the generalized
// door types. Absorbs code from the below two functions and adds
// the ability to pass in fully customized data. Values for the
// speed and delay types that are outside the range representable
// in BOOM generalized lines are used to indicate that full values 
// are contained in the doordata structure and should be used 
// instead of the hardcoded generalized options.
//
// Parameters:
// line -- pointer to originating line; may be NULL
// tag  -- tag of sectors to affect (may come from line or elsewhere)
// dd   -- pointer to full parameter info for door
//
int EV_DoParamDoor(line_t *line, int tag, doordata_t *dd)
{
   int secnum, rtn = 0;
   sector_t *sec;
   vldoor_t *door;
   boolean manual = false;
   boolean turbo;

   // check if a manual trigger, if so do just the sector on the backside
   // haleyjd 05/04/04: door actions with no line can't be manual
   if(dd->trigger_type == PushOnce || dd->trigger_type == PushMany)
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = sec - sectors;
      manual = true;
      goto manual_door;
   }

   secnum = -1;
   rtn = 0;

   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
manual_door:
      // Do not start another function if ceiling already moving
      if(P_SectorActive(ceiling_special, sec)) //jff 2/22/98
      {
         if(manual)
         {
            // haleyjd 02/23/04: allow repushing of certain generalized
            // doors
            if(demo_version >= 331)
               rtn = GenDoorRetrigger(sec->ceilingdata, dd->trigger_type);

            return rtn;
         }
         continue;
      }

      // new door thinker
      rtn = 1;
      door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
      P_AddThinker(&door->thinker);
      sec->ceilingdata = door; //jff 2/22/98
      
      door->thinker.function = T_VerticalDoor;
      door->sector = sec;
      
      // setup delay for door remaining open/closed
      switch(dd->delay_type)
      {
      default:
      case doorWaitOneSec:
         door->topwait = 35;
         break;
      case doorWaitStd:
         door->topwait = VDOORWAIT;
         break;
      case doorWaitStd2x:
         door->topwait = 2*VDOORWAIT;
         break;
      case doorWaitStd7x:
         door->topwait = 7*VDOORWAIT;
         break;
      case doorWaitParam: // haleyjd 05/04/04: parameterized wait
         door->topwait = dd->delay_value;
         break;
      }

      // setup speed of door motion
      switch(dd->speed_type)
      {
      default:
      case SpeedSlow:
         door->speed = VDOORSPEED;
         break;
      case SpeedNormal:
         door->speed = VDOORSPEED*2;
         break;
      case SpeedFast:
         door->speed = VDOORSPEED*4;
         break;
      case SpeedTurbo:
         door->speed = VDOORSPEED*8;
         break;
      case SpeedParam: // haleyjd 05/04/04: parameterized speed
         door->speed = dd->speed_value;
         break;
      }
      door->line = line; // jff 1/31/98 remember line that triggered us

      // killough 10/98: implement gradual lighting
      // haleyjd 02/28/05: support light changes from alternate tag
      if(dd->usealtlighttag)
         door->lighttag = dd->altlighttag;
      else
         door->lighttag = !comp[comp_doorlight] && line && 
            (line->special&6) == 6 && 
            line->special > GenLockedBase ? line->tag : 0;
      
      // set kind of door, whether it opens then close, opens, closes etc.
      // assign target heights accordingly
      // haleyjd 05/04/04: fixed sound playing; was totally messed up!
      switch(dd->kind)
      {
      case OdCDoor:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         if(door->speed >= VDOORSPEED*4)
         {
            door->type = genBlazeRaise;
            turbo = true;
         }
         else
         {
            door->type = genRaise;
            turbo = false;
         }
         if(door->topheight != sec->ceilingz)
            P_DoorSequence(true, turbo, false, door->sector); // haleyjd
         break;
      case ODoor:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         if(door->speed >= VDOORSPEED*4)
         {
            door->type = genBlazeOpen;
            turbo = true;
         }
         else
         {
            door->type = genOpen;
            turbo = false;
         }
         if(door->topheight != sec->ceilingz)
            P_DoorSequence(true, turbo, false, door->sector); // haleyjd
         break;
      case CdODoor:
         door->topheight = sec->ceilingz;
         door->direction = plat_down;
         if(door->speed >= VDOORSPEED*4)
         {
            door->type = genBlazeCdO;
            turbo = true;;
         }
         else
         {
            door->type = genCdO;
            turbo = false;
         }
         P_DoorSequence(false, turbo, false, door->sector); // haleyjd
         break;
      case CDoor:
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->direction = plat_down;
         if(door->speed >= VDOORSPEED*4)
         {
            door->type = genBlazeClose;
            turbo = true;
         }
         else
         {
            door->type = genClose;
            turbo = false;
         }
         P_DoorSequence(false, turbo, false, door->sector); // haleyjd
         break;
      
      // haleyjd: The following door types are parameterized only
      case pDOdCDoor:
         // parameterized "raise in" type
         door->direction = plat_special; // door starts in stasis
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->topcountdown = dd->topcountdown; // wait to start
         if(door->speed >= VDOORSPEED*4)
            door->type = paramBlazeRaiseIn;
         else
            door->type = paramRaiseIn;
         break;
      case pDCDoor:
         // parameterized "close in" type
         door->direction    = plat_stop;        // door starts in wait
         door->topcountdown = dd->topcountdown; // wait to start
         if(door->speed >= VDOORSPEED*4)
            door->type = paramBlazeCloseIn;
         else
            door->type = paramCloseIn;
         break;
         break;
      default:
         break;
      }
      if(manual)
         return rtn;
   }
   return rtn;
}

//
// EV_DoGenLockedDoor()
//
// Handle generalized locked door types
//
// Passed the linedef activating the generalized locked door
// Returns true if a thinker created
//
// haleyjd 05/04/04: rewritten to use EV_DoParamDoor
//
int EV_DoGenLockedDoor(line_t *line)
{
   doordata_t dd;
   unsigned value = (unsigned)line->special - GenLockedBase;

   // parse the bit fields in the line's special type
   
   dd.delay_type   = doorWaitStd;
   dd.kind         = (value & LockedKind ) >> LockedKindShift;
   dd.speed_type   = (value & LockedSpeed) >> LockedSpeedShift;
   dd.trigger_type = (value & TriggerType) >> TriggerTypeShift;
   dd.usealtlighttag = false;
   
   return EV_DoParamDoor(line, line->tag, &dd);
}

//
// EV_DoGenDoor()
//
// Handle generalized door types
//
// Passed the linedef activating the generalized door
// Returns true if a thinker created
//
// haleyjd 05/04/04: rewritten to use EV_DoParamDoor
//
int EV_DoGenDoor(line_t* line)
{
   doordata_t dd;
   unsigned value = (unsigned)line->special - GenDoorBase;

   // parse the bit fields in the line's special type
   
   dd.delay_type   = (value & DoorDelay  ) >> DoorDelayShift;
   dd.kind         = (value & DoorKind   ) >> DoorKindShift;
   dd.speed_type   = (value & DoorSpeed  ) >> DoorSpeedShift;
   dd.trigger_type = (value & TriggerType) >> TriggerTypeShift;
   dd.usealtlighttag = false;

   return EV_DoParamDoor(line, line->tag, &dd);
}

//
// haleyjd 02/28/05: Parameterized Line Special System
//
// This is the code that dispatches requests to execute parameterized
// line specials, which work very similar to Hexen's line specials.
// Parameterized specials try to avoid code explosion by absorbing the
// generalized line code as special cases and then allowing fully
// customized data to be passed into the EV_ functions above inside
// new structs that hold all the parameters. This is a lot easier and
// more compatible than converting generalized lines into parameterized
// specials at run-time.
//

//
// pspec_TriggerType
//
// Routine to get a generalized line trigger type for a given
// parameterized special activation.
//
static int pspec_TriggerType(int spac, long tag, boolean reuse)
{
   int trig = 0;

   // zero tags must always be treated as push types
   if(!tag)
      return (reuse ? PushMany : PushOnce);

   switch(spac)
   {
   case SPAC_USE:
      trig = (reuse ? SwitchMany : SwitchOnce);
      break;
   case SPAC_IMPACT:
      trig = (reuse ? GunMany : GunOnce);
      break;
   case SPAC_CROSS:
      trig = (reuse ? WalkMany : WalkOnce);
      break;
   case SPAC_PUSH:
      // TODO
      break;
   }

   return trig;
}

// parameterized door trigger type lookup table

static int param_door_kinds[6] =
{
   OdCDoor, ODoor, CDoor, CdODoor, pDOdCDoor, pDCDoor
};

//
// pspec_Door
//
// Parses arguments for parameterized Door specials.
//
static boolean pspec_Door(line_t *line, mobj_t *thing, long *args, 
                          short special, int trigger_type)
{
   int kind;
   doordata_t dd;

#ifdef RANGECHECK
   if(special < 300 || special > 305)
      I_Error("pspec_Door: parameterized door special out of range\n");
#endif

   kind = param_door_kinds[special - 300];

   // speed is always second parameter
   // value is eighths of a unit per tic
   dd.speed_type  = SpeedParam;
   dd.speed_value = args[1] * FRACUNIT / 8;
  
   // all param doors support alternate light tagging
   dd.usealtlighttag = true;

   // OdC and CdO doors support wait as third param.
   // pDCDoor has topcountdown as third param
   // pDOdCDoor has delay and countdown as third and fourth
   // Other door types have light tag as third param.
   switch(kind)
   {
   case OdCDoor:
   case CdODoor:
      dd.delay_type  = doorWaitParam;
      dd.delay_value = args[2];
      dd.altlighttag = args[3];
      break;
   case pDCDoor:
      dd.delay_type   = doorWaitStd; // not used by this door type
      dd.topcountdown = args[2];
      dd.altlighttag  = args[3];
      break;
   case pDOdCDoor:
      dd.delay_type   = doorWaitParam;
      dd.delay_value  = args[2];
      dd.topcountdown = args[3];
      dd.altlighttag  = args[4];
      break;
   default:
      dd.delay_type  = doorWaitStd; // not used by this door type
      dd.altlighttag = args[2];
      break;
   }

   dd.kind = kind;
   dd.trigger_type = trigger_type;

   // set genDoorThing in case of manual retrigger
   genDoorThing = thing;

   return EV_DoParamDoor(line, args[0], &dd);
}

//
// Tablified data for parameterized floor types
//

static int param_floor_data[17][2] =
{
//  dir trigger
   { 1, FtoHnF   }, // 306: Floor_RaiseToHighest
   { 0, FtoHnF   }, // 307: Floor_LowerToHighest
   { 1, FtoLnF   }, // 308: Floor_RaiseToLowest
   { 0, FtoLnF   }, // 309: Floor_LowerToLowest
   { 1, FtoNnF   }, // 310: Floor_RaiseToNearest
   { 0, FtoNnF   }, // 311: Floor_LowerToNearest
   { 1, FtoLnC   }, // 312: Floor_RaiseToLowestCeiling
   { 0, FtoLnC   }, // 313: Floor_LowerToLowestCeiling
   { 1, FtoC     }, // 314: Floor_RaiseToCeiling
   { 1, FbyST    }, // 315: Floor_RaiseByTexture
   { 0, FbyST    }, // 316: Floor_LowerByTexture
   { 1, FbyParam }, // 317: Floor_RaiseByValue
   { 0, FbyParam }, // 318: Floor_LowerByValue
   { 1, FtoAbs   }, // 319: Floor_MoveToValue (note: this dir not used)
   { 1, FInst    }, // 320: Floor_RaiseInstant
   { 0, FInst    }, // 321: Floor_LowerInstant
   { 0, FtoC     }, // 322: Floor_ToCeilingInstant
};

static int fchgdata[7][2] =
{
//   model          change type
   { FTriggerModel, FNoChg },
   { FTriggerModel, FChgZero },
   { FNumericModel, FChgZero },
   { FTriggerModel, FChgTxt  },
   { FNumericModel, FChgTxt  },
   { FTriggerModel, FChgTyp  },
   { FNumericModel, FChgTyp  },
};

//
// pspec_Floor
//
// Parses arguments for parameterized Floor specials.
//
static boolean pspec_Floor(line_t *line, long *args, short special, 
                           int trigger_type)
{
   floordata_t fd;
   int normspec;

#ifdef RANGECHECK
   if(special < 306 || special > 322)
      I_Error("pspec_Floor: parameterized floor trigger out of range\n");
#endif

   normspec = special - 306;

   fd.direction   = param_floor_data[normspec][0];
   fd.target_type = param_floor_data[normspec][1];
   fd.trigger_type = trigger_type;
   fd.crush = -1;

   switch(special)
   {
      // (tag, speed, change, [crush])
   case 306: // Floor_RaiseToHighest
   case 310: // Floor_RaiseToNearest
   case 312: // Floor_RaiseToLowestCeiling
   case 314: // Floor_RaiseToCeiling
   case 315: // Floor_RaiseByTexture
      fd.crush = args[3];
      // fall through:
   case 307: // Floor_LowerToHighest
   case 309: // Floor_LowerToLowest
   case 311: // Floor_LowerToNearest
   case 313: // Floor_LowerToLowestCeiling
   case 316: // Floor_LowerByTexture
      fd.speed_type   = SpeedParam;
      fd.speed_value  = args[1] * FRACUNIT / 8;
      if(args[2] >= 0 && args[2] < 7)
      {
         fd.change_model = fchgdata[args[2]][0];
         fd.change_type  = fchgdata[args[2]][1];
      }
      else
      {
         fd.change_model = 0;
         fd.change_type  = 0;
      }
      break;
 
      // (tag, change, crush)
   case 308: // Floor_RaiseToLowest -- special case, always instant
   case 322: // ToCeilingInstant
      fd.speed_type   = SpeedNormal; // not used
      if(args[1] >= 0 && args[1] < 7)
      {
         fd.change_model = fchgdata[args[1]][0];
         fd.change_type  = fchgdata[args[1]][1];
      }
      else
      {
         fd.change_model = 0;
         fd.change_type  = 0;
      }
      fd.crush = args[2];
      break;

      // (tag, speed, height, change, [crush])
   case 317: // Floor_RaiseByValue
   case 319: // Floor_MoveToValue
      fd.crush = args[4];
      // fall through:
   case 318: // Floor_LowerByValue
      fd.speed_type   = SpeedParam;
      fd.speed_value  = args[1] * FRACUNIT / 8;
      fd.height_value = args[2] * FRACUNIT;
      if(args[3] >= 0 && args[3] < 7)
      {
         fd.change_model = fchgdata[args[3]][0];
         fd.change_type  = fchgdata[args[3]][1];
      }
      else
      {
         fd.change_model = 0;
         fd.change_type  = 0;
      }
      break;

      // (tag, height, change, [crush])
   case 320: // Floor_RaiseInstant
      fd.crush = args[3];
      // fall through:
   case 321: // Floor_LowerInstant
      fd.speed_type   = SpeedNormal; // not really used
      fd.height_value = args[1] * FRACUNIT;
      if(args[2] >= 0 && args[2] < 7)
      {
         fd.change_model = fchgdata[args[2]][0];
         fd.change_type  = fchgdata[args[2]][1];
      }
      else
      {
         fd.change_model = 0;
         fd.change_type  = 0;
      }
      break;
   }

   return EV_DoParamFloor(line, args[0], &fd);
}

//
// Tablified data for parameterized ceiling types
//

static int param_ceiling_data[17][2] =
{
//  dir trigger
   { 1, CtoHnC   }, // 323: Ceiling_RaiseToHighest
   { 0, CtoHnC   }, // 324: Ceiling_ToHighestInstant
   { 1, CtoNnC   }, // 325: Ceiling_RaiseToNearest
   { 0, CtoNnC   }, // 326: Ceiling_LowerToNearest
   { 1, CtoLnC   }, // 327: Ceiling_RaiseToLowest
   { 0, CtoLnC   }, // 328: Ceiling_LowerToLowest
   { 1, CtoHnF   }, // 329: Ceiling_RaiseToHighestFloor
   { 0, CtoHnF   }, // 330: Ceiling_LowerToHighestFloor
   { 1, CtoF     }, // 331: Ceiling_ToFloorInstant
   { 0, CtoF     }, // 332: Ceiling_LowerToFloor
   { 1, CbyST    }, // 333: Ceiling_RaiseByTexture
   { 0, CbyST    }, // 334: Ceiling_LowerByTexture
   { 1, CbyParam }, // 335: Ceiling_RaiseByValue
   { 0, CbyParam }, // 336: Ceiling_LowerByValue
   { 1, CtoAbs   }, // 337: Ceiling_MoveToValue (note: this dir not used)
   { 1, CInst    }, // 338: Ceiling_RaiseInstant
   { 0, CInst    }, // 339: Ceiling_LowerInstant
};

static int cchgdata[7][2] =
{
//   model          change type
   { CTriggerModel, CNoChg },
   { CTriggerModel, CChgZero },
   { CNumericModel, CChgZero },
   { CTriggerModel, CChgTxt  },
   { CNumericModel, CChgTxt  },
   { CTriggerModel, CChgTyp  },
   { CNumericModel, CChgTyp  },
};

//
// pspec_Ceiling
//
// Parses arguments for parameterized Ceiling specials.
//
static boolean pspec_Ceiling(line_t *line, long *args, short special, 
                             int trigger_type)
{
   ceilingdata_t cd;
   int normspec;

#ifdef RANGECHECK
   if(special < 323 || special > 339)
      I_Error("pspec_Ceiling: parameterized ceiling trigger out of range\n");
#endif

   normspec = special - 323;

   cd.direction    = param_ceiling_data[normspec][0];
   cd.target_type  = param_ceiling_data[normspec][1];
   cd.trigger_type = trigger_type;
   cd.crush        = -1;

   switch(special)
   {
      // (tag, speed, change, [crush])
   case 326: // Ceiling_LowerToNearest
   case 328: // Ceiling_LowerToLowest
   case 330: // Ceiling_LowerToHighestFloor
   case 332: // Ceiling_LowerToFloor
   case 334: // Ceiling_LowerByTexture
      cd.crush = args[3];
      // fall through:
   case 323: // Ceiling_RaiseToHighest
   case 325: // Ceiling_RaiseToNearest
   case 327: // Ceiling_RaiseToLowest
   case 329: // Ceiling_RaiseToHighestFloor
   case 333: // Ceiling_RaiseByTexture
      cd.speed_type   = SpeedParam;
      cd.speed_value  = args[1] * FRACUNIT / 8;
      if(args[2] >= 0 && args[2] < 7)
      {
         cd.change_model = cchgdata[args[2]][0];
         cd.change_type  = cchgdata[args[2]][1];
      }
      else
      {
         cd.change_model = 0;
         cd.change_type  = 0;
      }
      break;
      
      // (tag, change, crush)
   case 324: // Ceiling_ToHighestInstant -- special case, always instant
   case 331: // Ceiling_ToFloorInstant
      cd.speed_type   = SpeedNormal; // not used
      if(args[1] >= 0 && args[1] < 7)
      {
         cd.change_model = cchgdata[args[1]][0];
         cd.change_type  = cchgdata[args[1]][1];
      }
      else
      {
         cd.change_model = 0;
         cd.change_type  = 0;
      }
      cd.crush = args[2];
      break;

      // (tag, speed, height, change, [crush])
   case 336: // Ceiling_LowerByValue
   case 337: // Ceiling_MoveToValue
      cd.crush = args[4];
      // fall through:
   case 335: // Ceiling_RaiseByValue
      cd.speed_type   = SpeedParam;
      cd.speed_value  = args[1] * FRACUNIT / 8;
      cd.height_value = args[2] * FRACUNIT;
      if(args[3] >= 0 && args[3] < 7)
      {
         cd.change_model = cchgdata[args[3]][0];
         cd.change_type  = cchgdata[args[3]][1];
      }
      else
      {
         cd.change_model = 0;
         cd.change_type  = 0;
      }
      break;

      // (tag, height, change, [crush])
   case 339: // Ceiling_LowerInstant
      cd.crush = args[3];
      // fall through:
   case 338: // Ceiling_RaiseInstant
      cd.speed_type   = SpeedNormal; // not really used
      cd.height_value = args[1] * FRACUNIT;
      if(args[2] >= 0 && args[2] < 7)
      {
         cd.change_model = cchgdata[args[2]][0];
         cd.change_type  = cchgdata[args[2]][1];
      }
      else
      {
         cd.change_model = 0;
         cd.change_type  = 0;
      }
      break;
   }

   return EV_DoParamCeiling(line, args[0], &cd);
}

//
// pspec_Stairs
//
// Parses arguments for parameterized Stair specials.
//
static boolean pspec_Stairs(line_t *line, long *args, short special, 
                            int trigger_type)
{
   stairdata_t sd;

   sd.trigger_type  = trigger_type;
   sd.direction     = 0;
   sd.speed_type    = SpeedParam;
   sd.stepsize_type = StepSizeParam;
   
   // haleyjd: eventually this will depend on the special type; right
   // now, all are set to not ignore texture differences
   sd.ignore        = 0;

   switch(special)
   {
   case 340: // Stairs_BuildUpDoom
      sd.direction = 1;
      // fall through
   case 341: // Stairs_BuildDownDoom
      sd.sync_value  = 0;
      sd.delay_value = args[3];
      sd.reset_value = args[4];
      break;
   case 342: // Stairs_BuildUpDoomSync
      sd.direction = 1;
      // fall through
   case 343: // Stairs_BuildDownDoomSync
      sd.sync_value  = 1;
      sd.delay_value = 0;       // 10/02/06 sync'd stairs can't delay >_<
      sd.reset_value = args[3];
      break;
   }

   // all stair types take the same arguments:
   //    (tag, speed, stepsize, delay, reset)
   sd.speed_value    = args[1] * FRACUNIT / 8;
   sd.stepsize_value = args[2] * FRACUNIT;

   return EV_DoParamStairs(line, args[0], &sd);
}

//
// pspec_PolyDoor
//
// Parses arguments for parameterized polyobject door types
//
static boolean pspec_PolyDoor(long *args, short special)
{
   polydoordata_t pdd;

   pdd.polyObjNum = args[0]; // polyobject id
   
   switch(special)
   {
   case 350: // Polyobj_DoorSlide
      pdd.doorType = POLY_DOOR_SLIDE;
      pdd.speed    = args[1] * FRACUNIT / 8;
      pdd.angle    = args[2]; // angle of motion (byte angle)
      pdd.distance = args[3] * FRACUNIT;
      pdd.delay    = args[4]; // delay in tics
      break;
   case 351: // Polyobj_DoorSwing
      pdd.doorType = POLY_DOOR_SWING;
      pdd.speed    = args[1]; // angular speed (byte angle)
      pdd.distance = args[2]; // angular distance (byte angle)
      pdd.delay    = args[3]; // delay in tics
      break;
   default:
      return 0; // ???
   }

   return EV_DoPolyDoor(&pdd);
}

//
// pspec_PolyMove
//
// Parses arguments for parameterized polyobject move specials
//
static boolean pspec_PolyMove(long *args, short special)
{
   polymovedata_t pmd;

   pmd.polyObjNum = args[0];
   pmd.speed      = args[1] * FRACUNIT / 8;
   pmd.angle      = args[2]; // byteangle
   pmd.distance   = args[3] * FRACUNIT;

   pmd.overRide = (special == 353); // Polyobj_OR_Move

   return EV_DoPolyObjMove(&pmd);
}

//
// pspec_PolyRotate
//
// Parses arguments for parameterized polyobject rotate specials
//
static boolean pspec_PolyRotate(long *args, short special)
{
   polyrotdata_t prd;

   prd.polyObjNum = args[0];
   prd.speed      = args[1]; // angular speed (byteangle)
   prd.distance   = args[2]; // angular distance (byteangle)

   // Polyobj_(OR_)RotateRight have dir == -1
   prd.direction = (special == 354 || special == 355) ? -1 : 1;
   
   // Polyobj_OR types have override set to true
   prd.overRide  = (special == 355 || special == 357);

   return EV_DoPolyObjRotate(&prd);
}

//
// pspec_Pillar
//
// joek
// Parses arguments for hexen style parameterized Pillar specials.
//
// haleyjd: rewritten to use pillardata_t struct
//
static boolean pspec_Pillar(line_t *line, long *args, short special)
{
   pillardata_t pd;
   
   pd.tag   = args[0];
   pd.crush = 0;

   switch(special)
   {
   case 363:
      pd.crush  = args[3];
      // fall through
   case 362:
      pd.speed  = args[1] ? args[1] * FRACUNIT / 8 : FRACUNIT; // no 0 speed!
      pd.height = args[2] * FRACUNIT;
      return EV_PillarBuild(line, &pd);

   case 364:
      pd.speed = args[1] ? args[1] * FRACUNIT / 8 : FRACUNIT;
      pd.fdist = args[2] * FRACUNIT;
      pd.cdist = args[3] * FRACUNIT;
      return EV_PillarOpen(line, &pd);

   default:
      return false;
   }
}

//
// pspec_ACSExecute
//
// haleyjd 01/07/07: Runs an ACS script.
//
static boolean pspec_ACSExecute(line_t *line, long *args, short special,
                                int side, mobj_t *thing)
{
   int snum, mnum;
   long script_args[5] = { 0, 0, 0, 0, 0 };


   snum           = args[0];
   mnum           = args[1];
   script_args[0] = args[2]; // transfer last three line args to script args
   script_args[1] = args[3];
   script_args[2] = args[4];

   return ACS_StartScript(snum, mnum, script_args, thing, line, side, NULL);
}

//
// P_ExecParamLineSpec
//
// Executes a parameterized line special.
//
// line:    Pointer to line being activated. May be NULL in this context.
// thing:   Pointer to thing doing activation. May be NULL in this context.
// special: Special to execute.
// args:    Arguments to special.
// side:    Side of line activated. May be ignored.
// reuse:   if action is repeatable
//
boolean P_ExecParamLineSpec(line_t *line, mobj_t *thing, short special, 
                            long *args, int side, int spac, boolean reuse)
{
   boolean success = false;

   int trigger_type = pspec_TriggerType(spac, args[0], reuse);

   switch(special)
   {
   case 300: // Door_Raise
   case 301: // Door_Open
   case 302: // Door_Close
   case 303: // Door_CloseWaitOpen
   case 304: // Door_WaitRaise
   case 305: // Door_WaitClose
      success = pspec_Door(line, thing, args, special, trigger_type);
      break;
   case 306: // Floor_RaiseToHighest
   case 307: // Floor_LowerToHighest
   case 308: // Floor_RaiseToLowest
   case 309: // Floor_LowerToLowest
   case 310: // Floor_RaiseToNearest
   case 311: // Floor_LowerToNearest
   case 312: // Floor_RaiseToLowestCeiling
   case 313: // Floor_LowerToLowestCeiling
   case 314: // Floor_RaiseToCeiling
   case 315: // Floor_RaiseByTexture
   case 316: // Floor_LowerByTexture
   case 317: // Floor_RaiseByValue
   case 318: // Floor_LowerByValue
   case 319: // Floor_MoveToValue
   case 320: // Floor_RaiseInstant
   case 321: // Floor_LowerInstant
   case 322: // Floor_ToCeilingInstant
      success = pspec_Floor(line, args, special, trigger_type);
      break;
   case 323: // Ceiling_RaiseToHighest
   case 324: // Ceiling_ToHighestInstant
   case 325: // Ceiling_RaiseToNearest
   case 326: // Ceiling_LowerToNearest
   case 327: // Ceiling_RaiseToLowest
   case 328: // Ceiling_LowerToLowest
   case 329: // Ceiling_RaiseToHighestFloor
   case 330: // Ceiling_LowerToHighestFloor
   case 331: // Ceiling_ToFloorInstant
   case 332: // Ceiling_LowerToFloor
   case 333: // Ceiling_RaiseByTexture
   case 334: // Ceiling_LowerByTexture
   case 335: // Ceiling_RaiseByValue
   case 336: // Ceiling_LowerByValue
   case 337: // Ceiling_MoveToValue
   case 338: // Ceiling_RaiseInstant
   case 339: // Ceiling_LowerInstant
      success = pspec_Ceiling(line, args, special, trigger_type);
      break;
   case 340: // Stairs_BuildUpDoom
   case 341: // Stairs_BuildDownDoom
   case 342: // Stairs_BuildUpDoomSync
   case 343: // Stairs_BuildDownDoomSync
      success = pspec_Stairs(line, args, special, trigger_type);
      break;
   case 350: // Polyobj_DoorSlide
   case 351: // Polyobj_DoorSwing
      success = pspec_PolyDoor(args, special);
      break;
   case 352: // Polyobj_Move
   case 353: // Polyobj_OR_Move
      success = pspec_PolyMove(args, special);
      break;
   case 354: // Polyobj_RotateRight
   case 355: // Polyobj_OR_RotateRight
   case 356: // Polyobj_RotateLeft
   case 357: // Polyobj_OR_RotateLeft
      success = pspec_PolyRotate(args, special);
      break;
   case 362: // Pillar_Build
   case 363: // Pillar_BuildAndCrush
   case 364: // Pillar_Open
      success = pspec_Pillar(line, args, special);
      break;
   case 365: // ACS_Execute
      success = pspec_ACSExecute(line, args, special, side, thing);
      break;
   case 366: // ACS_Suspend
      success = ACS_SuspendScript(args[0], args[1]);
      break;
   case 367: // ACS_Terminate
      success = ACS_TerminateScript(args[0], args[1]);
      break;
   case 368: // Light_RaiseByValue
      success = EV_SetLight(line, args[0], setlight_add, args[1]);
      break;
   case 369: // Light_LowerByValue
      success = EV_SetLight(line, args[0], setlight_sub, args[1]);
      break;
   case 370: // Light_ChangeToValue
      success = EV_SetLight(line, args[0], setlight_set, args[1]);
      break;
   case 371: // Light_Fade
      success = EV_FadeLight(line, args[0], args[1], args[2]);
      break;
   case 372: // Light_Glow
      success = EV_GlowLight(line, args[0], args[1], args[2], args[3]);
      break;
   case 373: // Light_Flicker
      success = EV_FlickerLight(line, args[0], args[1], args[2]);
      break;
   case 374: // Light_Strobe
      success = EV_StrobeLight(line, args[0], args[1], args[2], args[3], args[4]);
      break;
   case 375: // Radius_Quake
      success = P_StartQuake(args);
      break;
   default:
      break;
   }

   return success;
}

//
// P_ActivateParamLine
//
// Handles a param line activation and executes the appropriate
// parameterized line special.
//
// line:  The line being activated. Never NULL in this context.
// thing: The thing that wants to activate this line. Never NULL in this context.
// side:  Side of line activated, 0 or 1.
// spac:  Type of activation. This is de-wed from the special with
//        parameterized lines using the ExtraData extflags line field.
//
boolean P_ActivateParamLine(line_t *line, mobj_t *thing, int side, int spac)
{
   boolean success = false, reuse = false;
   long flags = 0;

   // check player / monster / missile enable flags
   if(thing->player)                   // treat as player?
      flags |= EX_ML_PLAYER;
   if(thing->flags3 & MF3_SPACMISSILE) // treat as missile?
      flags |= EX_ML_MISSILE;
   if(thing->flags3 & MF3_SPACMONSTER) // treat as monster?
      flags |= EX_ML_MONSTER;

   if(!(line->extflags & flags))
      return false;

   // check activation flags -- can we activate this line this way?
   switch(spac)
   {
   case SPAC_CROSS:
      flags = EX_ML_CROSS;
      break;
   case SPAC_USE:
      flags = EX_ML_USE;
      break;
   case SPAC_IMPACT:
      flags = EX_ML_IMPACT;
      break;
   case SPAC_PUSH:
      flags = EX_ML_PUSH;
      break;
   }

   if(!(line->extflags & flags))
      return false;

   // check 1S only flag -- if set, must be activated from first side
   if((line->extflags & EX_ML_1SONLY) && side != 0)
      return false;

   // is action reusable?
   if(line->extflags & EX_ML_REPEAT)
      reuse = true;

   // execute the special
   success = P_ExecParamLineSpec(line, thing, line->special, line->args,
                                 side, spac, reuse);

   // actions to take if line activation was successful:
   if(success)
   {
      // clear special if line is not repeatable
      if(!reuse)
         line->special = 0;
      
      // change switch textures where appropriate
      if(spac == SPAC_USE || spac == SPAC_IMPACT)
         P_ChangeSwitchTexture(line, reuse, side);
   }

   return success;
}

// ChangeLineTex texture position numbers
enum
{
   CLT_TEX_UPPER,
   CLT_TEX_MIDDLE,
   CLT_TEX_LOWER,
};

//
// EV_ChangeLineTex
//
// Sets the indicated texture on all lines of lineid tag (if usetag false) or of
// the given tag (if usetag true)
//
void P_ChangeLineTex(const char *texture, int pos, int side, int tag, boolean usetag)
{
   line_t *l = NULL;
   int linenum = -1, texnum;
   
   line_t *(*GetID)(int, int *);

   texnum = R_TextureNumForName(texture);
   linenum = -1;

   if(usetag)
      GetID = P_FindLine;
   else
      GetID = P_FindLineForID;

   while((l = GetID(tag, &linenum)) != NULL)
   {
      if(l->sidenum[side] == -1)
         continue;

      switch(pos)
      {
      case CLT_TEX_UPPER:
         sides[l->sidenum[side]].toptexture = texnum;
         break;
      case CLT_TEX_MIDDLE:
         sides[l->sidenum[side]].midtexture = texnum;
         break;
      case CLT_TEX_LOWER:
         sides[l->sidenum[side]].bottomtexture = texnum;
         break;
      }
   }
}

//
// Small Natives
//

//
// sm_specialmode
//
// Deprecated: does nothing
//
static cell AMX_NATIVE_CALL sm_specialmode(AMX *amx, cell *params)
{
   return 0;
}

//
// sm_changecelingtex  -- joek 9/11/07
//
// - small func to change our ceiling texture
//
static cell AMX_NATIVE_CALL sm_changeceilingtex(AMX *amx, cell *params)
{
   char *flat;
   int tag, err;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tag = params[1];

   if((err = A_GetSmallString(amx, &flat, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   P_ChangeCeilingTex(flat, tag);

   Z_Free(flat);

   return 0;
}

//
// sm_changefloortex  -- joek 9/11/07
//
// - small func to change our floor texture
//
static cell AMX_NATIVE_CALL sm_changefloortex(AMX *amx, cell *params)
{
   char *flat;
   int err, tag;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tag = params[1];

   if((err = A_GetSmallString(amx, &flat, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   P_ChangeFloorTex(flat, tag);

   Z_Free(flat);


   return 0;
}

//
// sm_changelinetex  -- joek 9/11/07
//
// - small func to change our line texture by lineid
//
static cell AMX_NATIVE_CALL sm_changelinetex(AMX *amx, cell *params)
{
   char *texname;
   int err, lineid, side, pos;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   lineid = params[1];        // lineid of lines to affect
   side = params[2];       // Front/Back
   pos = params[3];    // Upper/Middle/Lower

   if((err = A_GetSmallString(amx, &texname, params[4])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   P_ChangeLineTex(texname, pos, side, lineid, false);

   Z_Free(texname);


   return 0;
}

//
// sm_changelinetextag  -- joek 9/11/07
//
// - small func to change our line texture by tag
//
static cell AMX_NATIVE_CALL sm_changelinetextag(AMX *amx, cell *params)
{
   char *texname;
   int err, tag, side, pos;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tag = params[1];        // lineid of lines to affect
   side = params[2];       // Front/Back
   pos = params[3];    // Upper/Middle/Lower

   if((err = A_GetSmallString(amx, &texname, params[4])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   P_ChangeLineTex(texname, pos, side, tag, true);

   Z_Free(texname);


   return 0;
}

//
// P_ScriptSpec
//
// haleyjd 05/20/05
//
// Thunks from Small script params to line special args and executes
// the indicated special. All functions using this must take the
// same arguments in the same order as the line special.
//
static boolean P_ScriptSpec(short spec, AMX *amx, cell *params)
{
   long args[5] = { 0, 0, 0, 0, 0 };
   int i, numparams = params[0] / sizeof(cell);
   SmallContext_t *ctx;
   line_t *line  = NULL;
   mobj_t *thing = NULL;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   ctx = A_GetContextForAMX(amx);

   // If invocation type is SC_INVOKE_LINE, we can pass on the line and
   // thing that triggered this script. This results in the action acting
   // exactly like it belongs to the line normally.

   if(ctx->invocationData.invokeType == SC_INVOKE_LINE)
   {
      line  = ctx->invocationData.line;
      thing = ctx->invocationData.trigger;
   }

   for(i = 0; i < numparams; ++i)
      args[i] = params[i + 1];

   // FIXME: spac important?
   return P_ExecParamLineSpec(line, thing, spec, args, 0, SPAC_CROSS, true);
}

//
// Console Command to execute line specials
//

CONSOLE_COMMAND(p_linespec, cf_notnet|cf_level)
{
   short spec;
   long args[5] = { 0, 0, 0, 0, 0 };
   int i, numargs;

   if(!c_argc)
   {
      C_Printf("usage: p_linespec name args\n");
      return;
   }

   spec = E_LineSpecForName(c_argv[0]);

   numargs = c_argc - 1;

   for(i = 0; i < numargs; ++i)
      args[i] = atoi(c_argv[i + 1]);

   P_ExecParamLineSpec(NULL, players[cmdsrc].mo, spec, args, 0, SPAC_CROSS, true);
}

void P_AddGenLineCommands(void)
{
   C_AddCommand(p_linespec);
}

//
// Small Param Line Special Wrappers
//

#define SCRIPT_SPEC(num, name) \
static cell AMX_NATIVE_CALL sm_ ## name(AMX *amx, cell *params) \
{ \
   return P_ScriptSpec((num), amx, params); \
}

SCRIPT_SPEC(300, door_raise)
SCRIPT_SPEC(301, door_open)
SCRIPT_SPEC(302, door_close)
SCRIPT_SPEC(303, door_closewaitopen)
SCRIPT_SPEC(304, door_waitraise)
SCRIPT_SPEC(305, door_waitclose)
SCRIPT_SPEC(306, floor_raisetohighest)
SCRIPT_SPEC(307, floor_lowertohighest)
SCRIPT_SPEC(308, floor_raisetolowest)
SCRIPT_SPEC(309, floor_lowertolowest)
SCRIPT_SPEC(310, floor_raisetonearest)
SCRIPT_SPEC(311, floor_lowertonearest)
SCRIPT_SPEC(312, floor_raisetolowestceiling)
SCRIPT_SPEC(313, floor_lowertolowestceiling)
SCRIPT_SPEC(314, floor_raisetoceiling)
SCRIPT_SPEC(315, floor_raisebytexture)
SCRIPT_SPEC(316, floor_lowerbytexture)
SCRIPT_SPEC(317, floor_raisebyvalue)
SCRIPT_SPEC(318, floor_lowerbyvalue)
SCRIPT_SPEC(319, floor_movetovalue)
SCRIPT_SPEC(320, floor_raiseinstant)
SCRIPT_SPEC(321, floor_lowerinstant)
SCRIPT_SPEC(322, floor_toceilinginstant)
SCRIPT_SPEC(323, ceiling_raisetohighest)
SCRIPT_SPEC(324, ceiling_tohighestinstant)
SCRIPT_SPEC(325, ceiling_raisetonearest)
SCRIPT_SPEC(326, ceiling_lowertonearest)
SCRIPT_SPEC(327, ceiling_raisetolowest)
SCRIPT_SPEC(328, ceiling_lowertolowest)
SCRIPT_SPEC(329, ceiling_raisetohighestfloor)
SCRIPT_SPEC(330, ceiling_lowertohighestfloor)
SCRIPT_SPEC(331, ceiling_tofloorinstant)
SCRIPT_SPEC(332, ceiling_lowertofloor)
SCRIPT_SPEC(333, ceiling_raisebytexture)
SCRIPT_SPEC(334, ceiling_lowerbytexture)
SCRIPT_SPEC(335, ceiling_raisebyvalue)
SCRIPT_SPEC(336, ceiling_lowerbyvalue)
SCRIPT_SPEC(337, ceiling_movetovalue)
SCRIPT_SPEC(338, ceiling_raiseinstant)
SCRIPT_SPEC(339, ceiling_lowerinstant)
SCRIPT_SPEC(340, stairs_buildupdoom)
SCRIPT_SPEC(341, stairs_builddowndoom)
SCRIPT_SPEC(342, stairs_buildupdoomsync)
SCRIPT_SPEC(343, stairs_builddowndoomsync)
SCRIPT_SPEC(350, polyobj_doorslide)
SCRIPT_SPEC(351, polyobj_doorswing)
SCRIPT_SPEC(352, polyobj_move)
SCRIPT_SPEC(353, polyobj_or_move)
SCRIPT_SPEC(354, polyobj_rotateright)
SCRIPT_SPEC(355, polyobj_or_rotateright)
SCRIPT_SPEC(356, polyobj_rotateleft)
SCRIPT_SPEC(357, polyobj_or_rotateleft)
SCRIPT_SPEC(362, pillar_build)
SCRIPT_SPEC(363, pillar_buildandcrush)
SCRIPT_SPEC(364, pillar_open)
SCRIPT_SPEC(365, acs_execute)
SCRIPT_SPEC(366, acs_suspend)
SCRIPT_SPEC(367, acs_terminate)
SCRIPT_SPEC(368, light_raisebyvalue)
SCRIPT_SPEC(369, light_lowerbyvalue)
SCRIPT_SPEC(370, light_changetovalue)
SCRIPT_SPEC(371, light_fade)
SCRIPT_SPEC(372, light_glow)
SCRIPT_SPEC(373, light_flicker)
SCRIPT_SPEC(374, light_strobe)
SCRIPT_SPEC(375, radius_quake)

AMX_NATIVE_INFO genlin_Natives[] =
{
   { "_SpecialMode",                 sm_specialmode                 },
   { "_Door_Raise",                  sm_door_raise                  },
   { "_Door_Open",                   sm_door_open                   },
   { "_Door_Close",                  sm_door_close                  },
   { "_Door_CloseWaitOpen",          sm_door_closewaitopen          },
   { "_Door_WaitRaise",              sm_door_waitraise              },
   { "_Door_WaitClose",              sm_door_waitclose              },
   { "_Floor_RaiseToHighest",        sm_floor_raisetohighest        },
   { "_Floor_LowerToHighest",        sm_floor_lowertohighest        },
   { "_Floor_RaiseToLowest",         sm_floor_raisetolowest         },
   { "_Floor_LowerToLowest",         sm_floor_lowertolowest         },
   { "_Floor_RaiseToNearest",        sm_floor_raisetonearest        },
   { "_Floor_LowerToNearest",        sm_floor_lowertonearest        },
   { "_Floor_RaiseToLowestCeiling",  sm_floor_raisetolowestceiling  },
   { "_Floor_LowerToLowestCeiling",  sm_floor_lowertolowestceiling  },
   { "_Floor_RaiseToCeiling",        sm_floor_raisetoceiling        },
   { "_Floor_RaiseByTexture",        sm_floor_raisebytexture        },
   { "_Floor_LowerByTexture",        sm_floor_lowerbytexture        },
   { "_Floor_RaiseByValue",          sm_floor_raisebyvalue          },
   { "_Floor_LowerByValue",          sm_floor_lowerbyvalue          },
   { "_Floor_MoveToValue",           sm_floor_movetovalue           },
   { "_Floor_RaiseInstant",          sm_floor_raiseinstant          },
   { "_Floor_LowerInstant",          sm_floor_lowerinstant          },
   { "_Floor_ToCeilingInstant",      sm_floor_toceilinginstant      },
   { "_Ceiling_RaiseToHighest",      sm_ceiling_raisetohighest      },
   { "_Ceiling_ToHighestInstant",    sm_ceiling_tohighestinstant    },
   { "_Ceiling_RaiseToNearest",      sm_ceiling_raisetonearest      },
   { "_Ceiling_LowerToNearest",      sm_ceiling_lowertonearest      },
   { "_Ceiling_RaiseToLowest",       sm_ceiling_raisetolowest       },
   { "_Ceiling_LowerToLowest",       sm_ceiling_lowertolowest       },
   { "_Ceiling_RaiseToHighestFloor", sm_ceiling_raisetohighestfloor },
   { "_Ceiling_LowerToHighestFloor", sm_ceiling_lowertohighestfloor },
   { "_Ceiling_ToFloorInstant",      sm_ceiling_tofloorinstant      },
   { "_Ceiling_LowerToFloor",        sm_ceiling_lowertofloor        },
   { "_Ceiling_RaiseByTexture",      sm_ceiling_raisebytexture      },
   { "_Ceiling_LowerByTexture",      sm_ceiling_lowerbytexture      },
   { "_Ceiling_RaiseByValue",        sm_ceiling_raisebyvalue        },
   { "_Ceiling_LowerByValue",        sm_ceiling_lowerbyvalue        },
   { "_Ceiling_MoveToValue",         sm_ceiling_movetovalue         },
   { "_Ceiling_RaiseInstant",        sm_ceiling_raiseinstant        },
   { "_Ceiling_LowerInstant",        sm_ceiling_lowerinstant        },
   { "_Stairs_BuildUpDoom",          sm_stairs_buildupdoom          },
   { "_Stairs_BuildDownDoom",        sm_stairs_builddowndoom        },
   { "_Stairs_BuildUpDoomSync",      sm_stairs_buildupdoomsync      },
   { "_Stairs_BuildDownDoomSync",    sm_stairs_builddowndoomsync    },
   { "_Polyobj_DoorSlide",           sm_polyobj_doorslide           },
   { "_Polyobj_DoorSwing",           sm_polyobj_doorswing           },
   { "_Polyobj_Move",                sm_polyobj_move                },
   { "_Polyobj_OR_Move",             sm_polyobj_or_move             },
   { "_Polyobj_RotateRight",         sm_polyobj_rotateright         },
   { "_Polyobj_OR_RotateRight",      sm_polyobj_or_rotateright      },
   { "_Polyobj_RotateLeft",          sm_polyobj_rotateleft          },
   { "_Polyobj_OR_RotateLeft",       sm_polyobj_or_rotateleft       },
   { "_Pillar_Build",                sm_pillar_build                },
   { "_Pillar_BuildAndCrush",        sm_pillar_buildandcrush        },
   { "_Pillar_Open",                 sm_pillar_open                 },
   { "_ACS_Execute",                 sm_acs_execute                 },
   { "_ACS_Suspend",                 sm_acs_suspend                 },
   { "_ACS_Terminate",               sm_acs_terminate               },
   { "_Light_RaiseByValue",          sm_light_raisebyvalue          },
   { "_Light_LowerByValue",          sm_light_lowerbyvalue          },
   { "_Light_ChangeToValue",         sm_light_changetovalue         },
   { "_Light_Fade",                  sm_light_fade                  },
   { "_Light_Glow",                  sm_light_glow                  },
   { "_Light_Flicker",               sm_light_flicker               },
   { "_Light_Strobe",                sm_light_strobe                },
   { "_Radius_Quake",                sm_radius_quake                },
   { "_ChangeFloor",                 sm_changefloortex              },
   { "_ChangeCeiling",               sm_changeceilingtex            },
   { "_SetLineTexture",              sm_changelinetex               },
   { "_SetLineTextureTag",           sm_changelinetextag            },
   { NULL, NULL }
};

//----------------------------------------------------------------------------
//
// $Log: p_genlin.c,v $
// Revision 1.18  1998/05/23  10:23:23  jim
// Fix numeric changer loop corruption
//
// Revision 1.17  1998/05/08  03:34:56  jim
// formatted/documented p_genlin
//
// Revision 1.16  1998/05/03  23:05:56  killough
// Fix #includes at the top, nothing else
//
// Revision 1.15  1998/04/16  06:25:23  killough
// Fix generalized doors' opening sounds
//
// Revision 1.14  1998/04/05  13:54:10  jim
// fixed switch change on second activation
//
// Revision 1.13  1998/03/31  16:52:15  jim
// Fixed uninited type field in stair builders
//
// Revision 1.12  1998/03/20  14:24:28  jim
// Gen ceiling target now shortest UPPER texture
//
// Revision 1.11  1998/03/15  14:40:14  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.10  1998/03/13  14:05:56  jim
// Fixed arith overflow in some linedef types
//
// Revision 1.9  1998/03/04  11:56:30  jim
// Fix multiple sector stair raise
//
// Revision 1.8  1998/02/27  11:50:59  jim
// Fixes for stairs
//
// Revision 1.7  1998/02/23  23:46:50  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.6  1998/02/23  00:41:46  jim
// Implemented elevators
//
// Revision 1.4  1998/02/17  06:07:56  killough
// Change RNG calling sequence
//
// Revision 1.3  1998/02/13  03:28:36  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
//
// Revision 1.1.1.1  1998/02/04  09:19:00  jim
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------


