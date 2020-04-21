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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  Generalized linedef type handlers
//  Floors, Ceilings, Doors, Locked Doors, Lifts, Stairs, Crushers
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "a_small.h"
#include "acs_intr.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "ev_specials.h"
#include "g_game.h"
#include "m_compare.h"
#include "m_random.h"
#include "p_info.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_xenemy.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"
#include "r_data.h"
#include "r_main.h"
#include "r_state.h"

//=============================================================================
//
// Generalized Linedef Type handlers
//

int EV_DoParamFloor(const line_t *line, int tag, const floordata_t *fd)
{
   int       secnum;
   int       rtn = 0;
   bool      manual = false;
   sector_t *sec;
   FloorMoveThinker *floor;

   // check if a manual trigger, if so do just the sector on the backside
   // haleyjd 05/07/04: only line actions can be manual
   if(((fd->flags & FDF_HAVETRIGGERTYPE) &&
       (fd->trigger_type == PushOnce || fd->trigger_type == PushMany)) ||
      ((fd->flags & FDF_HAVESPAC) && !tag)
     )
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = eindex(sec - sectors);
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
      floor = new FloorMoveThinker;
      floor->addThinker();
      sec->floordata = floor;
      
      floor->crush     = fd->crush;
      floor->direction = fd->direction ? plat_up : plat_down;
      floor->sector    = sec;
      floor->texture   = sec->floorpic;
      floor->type      = genFloor;

      //jff 3/14/98 transfer old special field too
      P_SetupSpecialTransfer(sec, &(floor->special));
      

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
         if(fd->flags & FDF_HACKFORDESTHNF) // haleyjd 06/20/14: hacks
         {
            fixed_t amt = (fd->adjust - 128) * FRACUNIT;
            if(fd->force_adjust == 1)
               floor->floordestheight += amt;
            else if(floor->floordestheight != sec->floorheight)
               floor->floordestheight += amt;
         }
         break;
      case FtoLnF:
         floor->floordestheight = P_FindLowestFloorSurrounding(sec);
         break;
      case FtoNnF:
         floor->floordestheight = fd->direction ?
            P_FindNextHighestFloor(sec,sec->floorheight) :
            P_FindNextLowestFloor(sec,sec->floorheight);
         break;
      case FtoLnC:
         floor->floordestheight = P_FindLowestCeilingSurrounding(sec) + fd->adjust;
         break;
      case FtoLnCInclusive:
         floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
         if(floor->floordestheight > sec->ceilingheight)
            floor->floordestheight = sec->ceilingheight;
         floor->floordestheight += fd->adjust;
         break;
      case FtoC:
         floor->floordestheight = sec->ceilingheight + fd->adjust;
         break;
      case FbyST:
         floor->floordestheight = 
            (floor->sector->floorheight>>FRACBITS) + floor->direction * 
            (P_FindShortestTextureAround(secnum)>>FRACBITS);
         if(floor->floordestheight>32000)  //jff 3/13/98 prevent overflow
            floor->floordestheight=32000;    // wraparound in floor height
         if(floor->floordestheight<-32000)
            floor->floordestheight=-32000;
         floor->floordestheight<<=FRACBITS;
         break;
      case Fby24:
         floor->floordestheight = floor->sector->floorheight +
            floor->direction * 24*FRACUNIT;
         break;
      case Fby32:
         floor->floordestheight = floor->sector->floorheight +
            floor->direction * 32*FRACUNIT;
         break;
      
         // haleyjd 05/07/04: parameterized extensions
         //         05/20/05: added FtoAbs, FInst
      case FbyParam: 
         floor->floordestheight = floor->sector->floorheight +
            floor->direction * fd->height_value;
         break;
      case FtoAbs:
         floor->floordestheight = fd->height_value;
         // adjust direction appropriately (instant movement not possible)
         if(floor->floordestheight > floor->sector->floorheight)
            floor->direction = plat_up;
         else
            floor->direction = plat_down;
         break;
      case FInst:
         floor->floordestheight = floor->sector->floorheight +
            floor->direction * fd->height_value;
         // adjust direction appropriately (always instant)
         if(floor->floordestheight > floor->sector->floorheight)
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
            sector_t *msec;

            //jff 5/23/98 find model with ceiling at target height
            //if target is a ceiling type
            msec = (fd->target_type == FtoLnC || fd->target_type == FtoC ||
                    fd->target_type == FtoLnCInclusive) ?
               P_FindModelCeilingSector(floor->floordestheight,secnum) :
               P_FindModelFloorSector(floor->floordestheight,secnum);
            
            if(msec)
            {
               if(fd->changeOnStart)
               {
                  sec->floorpic = msec->floorpic;
                  switch(fd->change_type)
                  {
                     case FChgZero:  // zero type
                        //jff 3/14/98 change old field too
                        P_ZeroSpecialTransfer(&(floor->special));
                        P_TransferSectorSpecial(sec, &floor->special);
                        break;
                     case FChgTyp:   // copy type
                        //jff 3/14/98 change old field too
                        P_SetupSpecialTransfer(msec, &(floor->special));
                        P_TransferSectorSpecial(sec, &floor->special);
                        break;
                     default:
                        break;
                  }
               }
               else
               {
                  floor->texture = msec->floorpic;
                  switch(fd->change_type)
                  {
                     case FChgZero:  // zero type
                        //jff 3/14/98 change old field too
                        P_ZeroSpecialTransfer(&(floor->special));
                        floor->type = genFloorChg0;
                        break;
                     case FChgTyp:   // copy type
                        //jff 3/14/98 change old field too
                        P_SetupSpecialTransfer(msec, &(floor->special));
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
         }
         else     // else if a trigger model change
         {
            if(line) // haleyjd 05/07/04: only line actions can use this
            {
               if(fd->changeOnStart)
               {
                  sec->floorpic = line->frontsector->floorpic;
                  switch(fd->change_type)
                  {
                     case FChgZero:    // zero type
                        //jff 3/14/98 change old field too
                        P_ZeroSpecialTransfer(&(floor->special));
                        P_TransferSectorSpecial(sec, &floor->special);
                        break;
                     case FChgTyp:     // copy type
                        //jff 3/14/98 change old field too
                        P_SetupSpecialTransfer(line->frontsector, &(floor->special));
                        P_TransferSectorSpecial(sec, &floor->special);
                        break;
                     default:
                        break;
                  }
               }
               else
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
int EV_DoGenFloor(const line_t *line)
{
   floordata_t fd;
   memset(&fd, 0, sizeof(fd));

   int value = line->special - GenFloorBase;

   // parse the bit fields in the line's special type
   
   fd.crush        = ((value & FloorCrush) >> FloorCrushShift) ? 10 : -1;
   fd.change_type  = (value & FloorChange   ) >> FloorChangeShift;
   fd.target_type  = (value & FloorTarget   ) >> FloorTargetShift;
   fd.direction    = (value & FloorDirection) >> FloorDirectionShift;
   fd.change_model = (value & FloorModel    ) >> FloorModelShift;
   fd.speed_type   = (value & FloorSpeed    ) >> FloorSpeedShift;
   fd.trigger_type = (value & TriggerType   ) >> TriggerTypeShift;
   fd.flags        = FDF_HAVETRIGGERTYPE;

   return EV_DoParamFloor(line, line->args[0], &fd);
}

//
// EV_DoParamCeiling
//
int EV_DoParamCeiling(const line_t *line, int tag, const ceilingdata_t *cd)
{
   int       secnum;
   int       rtn = 0;
   bool      manual = false;
   fixed_t   targheight;
   sector_t *sec;
   CeilingThinker *ceiling;

   // check if a manual trigger, if so do just the sector on the backside
   if(((cd->flags & CDF_HAVETRIGGERTYPE) && 
       (cd->trigger_type == PushOnce || cd->trigger_type == PushMany)) ||
      ((cd->flags & CDF_HAVESPAC) && !tag)
     )
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = eindex(sec - sectors);
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
      ceiling = new CeilingThinker;
      ceiling->addThinker();
      sec->ceilingdata = ceiling; //jff 2/22/98

      ceiling->crush = cd->crush;
      ceiling->crushflags = 0;
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
      targheight = sec->ceilingheight;
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
            P_FindNextHighestCeiling(sec,sec->ceilingheight) :
            P_FindNextLowestCeiling(sec,sec->ceilingheight);
         break;
      case CtoHnF:
         targheight = P_FindHighestFloorSurrounding(sec);
         if(cd->ceiling_gap)
         {
            // target height needs to be adjusted if gap is non-zero, if we want
            // gap to have any effect. But if gap is 0, just emulate the buggy
            // (but compat-fixed) Boom behavior. The only classic specials with
            // this behavior are from Boom anyway.
            if(targheight < sec->floorheight)
               targheight = sec->floorheight;
            targheight += cd->ceiling_gap;

            // Also slow ceiling down if blocked while gap is nonzero
            if(cd->flags & CDF_HACKFORDESTF)
               ceiling->crushflags |= CeilingThinker::crushParamSlow;
         }
         break;
      case CtoF:
         targheight = sec->floorheight + cd->ceiling_gap;
         // ioanch: if hack flag is available, apply the Doom-like behavior if
         // gap is nonzero
         if(cd->ceiling_gap && cd->flags & CDF_HACKFORDESTF)
            ceiling->crushflags |= CeilingThinker::crushParamSlow;
         break;
      case CbyST:
         targheight = (ceiling->sector->ceilingheight>>FRACBITS) +
            ceiling->direction * (P_FindShortestUpperAround(secnum)>>FRACBITS);
         if(targheight > 32000)  // jff 3/13/98 prevent overflow
            targheight = 32000;  // wraparound in ceiling height
         if(targheight < -32000)
            targheight = -32000;
         targheight <<= FRACBITS;
         break;
      case Cby24:
         targheight = ceiling->sector->ceilingheight +
            ceiling->direction * 24*FRACUNIT;
         break;
      case Cby32:
         targheight = ceiling->sector->ceilingheight +
            ceiling->direction * 32*FRACUNIT;
         break;

         // haleyjd 10/06/05: parameterized extensions
      case CbyParam:
         targheight = ceiling->sector->ceilingheight +
            ceiling->direction * cd->height_value;
         break;
      case CtoAbs:
         targheight = cd->height_value;
         // adjust direction appropriately (instant movement not possible)
         if(targheight > ceiling->sector->ceilingheight)
            ceiling->direction = plat_up;
         else
            ceiling->direction = plat_down;
         break;
      case CInst:
         targheight = ceiling->sector->ceilingheight +
            ceiling->direction * cd->height_value;
         // adjust direction appropriately (always instant)
         if(targheight > ceiling->sector->ceilingheight)
            ceiling->direction = plat_down;
         else
            ceiling->direction = plat_up;
         break;
      default:
         break;
      }
    
      if(ceiling->direction == plat_up)
         ceiling->topheight = targheight;
      else
         ceiling->bottomheight = targheight;

      // set texture/type change properties
      if(cd->change_type)     // if a texture change is indicated
      {
         if(cd->change_model)   // if a numeric model change
         {
            sector_t *msec;

            // jff 5/23/98 find model with floor at target height if 
            // target is a floor type
            msec = (cd->target_type == CtoHnF || cd->target_type == CtoF) ?
                     P_FindModelFloorSector(targheight, secnum) :
                     P_FindModelCeilingSector(targheight, secnum);
            if(msec)
            {
               if(cd->flags & CDF_CHANGEONSTART)
               {
                  P_SetSectorCeilingPic(sec, msec->ceilingpic);
                  switch(cd->change_type)
                  {
                     case CChgZero:
                        P_ZeroSpecialTransfer(&(ceiling->special));
                        P_TransferSectorSpecial(sec, &ceiling->special);
                        break;
                     case CChgTyp:
                        P_SetupSpecialTransfer(msec, &ceiling->special);
                        P_TransferSectorSpecial(sec, &ceiling->special);
                        break;
                     default:
                        break;
                  }
               }
               else
               {
                  ceiling->texture = msec->ceilingpic;
                  switch(cd->change_type)
                  {
                     case CChgZero:  // type is zeroed
                        //jff 3/14/98 change old field too
                        P_ZeroSpecialTransfer(&(ceiling->special));
                        ceiling->type = genCeilingChg0;
                        break;
                     case CChgTyp:   // type is copied
                        //jff 3/14/98 change old field too
                        P_SetupSpecialTransfer(msec, &(ceiling->special));
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
         }
         else        // else if a trigger model change
         {
            if(line) // haleyjd 10/05/05: only line actions can use this
            {
               if(cd->flags & CDF_CHANGEONSTART)
               {
                  P_SetSectorCeilingPic(sec, line->frontsector->ceilingpic);
                  switch(cd->change_type)
                  {
                     case CChgZero:
                        P_ZeroSpecialTransfer(&ceiling->special);
                        P_TransferSectorSpecial(sec, &ceiling->special);
                        break;
                     case CChgTyp:
                        P_SetupSpecialTransfer(line->frontsector,
                                               &ceiling->special);
                        P_TransferSectorSpecial(sec, &ceiling->special);
                        break;
                     default:
                        break;
                  }
               }
               else
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
int EV_DoGenCeiling(const line_t *line)
{
   edefstructvar(ceilingdata_t, cd);
   int value = line->special - GenCeilingBase;

   // parse the bit fields in the line's special type
   
   cd.crush        = ((value & CeilingCrush) >> CeilingCrushShift) ? 10 : -1;
   cd.change_type  = (value & CeilingChange   ) >> CeilingChangeShift;
   cd.target_type  = (value & CeilingTarget   ) >> CeilingTargetShift;
   cd.direction    = (value & CeilingDirection) >> CeilingDirectionShift;
   cd.change_model = (value & CeilingModel    ) >> CeilingModelShift;
   cd.speed_type   = (value & CeilingSpeed    ) >> CeilingSpeedShift;
   cd.trigger_type = (value & TriggerType     ) >> TriggerTypeShift;
   cd.flags        = CDF_HAVETRIGGERTYPE; // BOOM-style activation

   // 08/25/09: initialize unused values
   cd.height_value = 0;
   cd.speed_value  = 0;

   return EV_DoParamCeiling(line, line->args[0], &cd);
}

//
// EV_DoGenLift()
//
// Handle generalized lift types
//
// Passed the linedef activating the lift
// Returns true if a thinker is created
//
int EV_DoGenLift(const line_t *line)
{
   int  value = line->special - GenLiftBase;

   // parse the bit fields in the line's special type
   
   int Targ = (value & LiftTarget ) >> LiftTargetShift;
   int Dely = (value & LiftDelay  ) >> LiftDelayShift;
   int Sped = (value & LiftSpeed  ) >> LiftSpeedShift;
   int Trig = (value & TriggerType) >> TriggerTypeShift;

   // setup the delay time before the floor returns
   int delay;
   switch(Dely)
   {
      case 0:
         delay = 1*35;
         break;
      case 1:
         delay = PLATWAIT*35;
         break;
      case 2:
         delay = 5*35;
         break;
      case 3:
         delay = 10*35;
         break;
      default:
         delay = 0;
         break;
   }

   // setup the speed of motion
   fixed_t speed;
   switch(Sped)
   {
      case SpeedSlow:
         speed = PLATSPEED * 2;
         break;
      case SpeedNormal:
         speed = PLATSPEED * 4;
         break;
      case SpeedFast:
         speed = PLATSPEED * 8;
         break;
      case SpeedTurbo:
         speed = PLATSPEED * 16;
         break;
      default:
         speed = 0;
         break;
   }

   return EV_DoGenLiftByParameters(Trig == PushOnce || Trig == PushMany, *line, speed, delay, Targ,
                                   0);
}

//
// Do generic lift using direct parameters. Meant to be called both from Boom generalized actions
// and the parameterized special Generic_Lift.
//
int EV_DoGenLiftByParameters(bool manualtrig, const line_t &line, fixed_t speed, int delay,
                             int target, fixed_t height)
{
   PlatThinker *plat;
   sector_t    *sec;
   int  secnum;
   int  rtn;
   bool manual;

   // parse the bit fields in the line's special type

   secnum = -1;
   rtn = 0;

   // Activate all <type> plats that are in_stasis

   if(target == LnF2HnF)
      PlatThinker::ActivateInStasis(line.args[0]);

   // check if a manual trigger, if so do just the sector on the backside
   manual = false;
   if(manualtrig)
   {
      if (!(sec = line.backsector))
         return rtn;
      secnum = eindex(sec - sectors);
      manual = true;
      goto manual_lift;
   }

   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromLineArg0(&line, secnum)) >= 0)
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
      plat = new PlatThinker;
      plat->addThinker();

      plat->crush  = -1;
      plat->tag    = line.args[0];
      plat->type   = genLift;
      plat->high   = sec->floorheight;
      plat->status = PlatThinker::down;
      plat->sector = sec;
      plat->sector->floordata = plat;

      // setup the target destination height
      switch(target)
      {
         case F2LnF:
            plat->low = P_FindLowestFloorSurrounding(sec);
            if(plat->low > sec->floorheight)
               plat->low = sec->floorheight;
            break;
         case F2NnF:
            plat->low = P_FindNextLowestFloor(sec,sec->floorheight);
            break;
         case F2LnC:
            plat->low = P_FindLowestCeilingSurrounding(sec);
            if(plat->low > sec->floorheight)
               plat->low = sec->floorheight;
            break;
         case LnF2HnF:
            plat->type = genPerpetual;
            plat->low = P_FindLowestFloorSurrounding(sec);
            if(plat->low > sec->floorheight)
               plat->low = sec->floorheight;
            plat->high = P_FindHighestFloorSurrounding(sec);
            if(plat->high < sec->floorheight)
               plat->high = sec->floorheight;
            plat->status = (P_Random(pr_genlift)&1) ? PlatThinker::down : PlatThinker::up;
            break;
         case lifttarget_upValue:
            // just use the parameterized equivalent, this is a ZDoom extension imposed on
            // Generic_Lift anyway.
            plat->type = upWaitDownStay;
            plat->status = PlatThinker::up;
            plat->low = sec->floorheight;
            plat->high = plat->low + height;
            if(plat->high < sec->floorheight)
               plat->high = sec->floorheight;
            break;
         default:
            break;
      }

      // setup the speed of motion
      plat->speed = speed;

      // setup the delay time before the floor returns
      plat->wait = delay;

      P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
      plat->addActivePlat(); // add this plat to the list of active plats

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
int EV_DoParamStairs(const line_t *line, int tag, const stairdata_t *sd)
{
   int  secnum;
   int  osecnum; //jff 3/4/98 preserve loop index
   int  height;
   int  i;
   int  newsecnum;
   int  texture;
   int  ok;
   int  rtn = 0;
   bool manual = false;
    
   sector_t *sec;
   sector_t *tsec;
   
   FloorMoveThinker *floor;
   
   fixed_t stairsize;
   fixed_t speed;  
   
   // check if a manual trigger, if so do just the sector on the backside
   // haleyjd 10/06/05: only line actions can be manual

   if(((sd->flags & SDF_HAVETRIGGERTYPE) &&
       (sd->trigger_type == PushOnce || sd->trigger_type == PushMany)) ||
      ((sd->flags & SDF_HAVESPAC) && !tag)
     )
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = eindex(sec - sectors);
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
      floor = new FloorMoveThinker;
      floor->addThinker();
      sec->floordata = floor;

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

      speed        = floor->speed;
      height       = sec->floorheight + floor->direction * stairsize;
      texture      = sec->floorpic;
      floor->crush = sd->crush ? 10 : -1; // constant crush damage, for now
      floor->type  = genBuildStair; // jff 3/31/98 do not leave uninited
      
      floor->floordestheight = height;
      
      // haleyjd 10/13/05: init reset and delay properties
      floor->resetTime     = sd->reset_value;
      floor->resetHeight   = sec->floorheight;
      floor->delayTime     = sd->delay_value;      
      floor->stepRaiseTime = FixedDiv(stairsize, speed) >> FRACBITS;
      floor->delayTimer    = floor->delayTime ? floor->stepRaiseTime : 0;

      sec->stairlock = -2;         // jff 2/26/98 set up lock on current sector
      sec->nextsec   = -1;
      sec->prevsec   = -1;

      P_StairSequence(floor->sector);
      
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
            newsecnum = eindex(tsec-sectors);
            
            if(secnum != newsecnum)
               continue;
            
            tsec = (sec->lines[i])->backsector;
            newsecnum = eindex(tsec - sectors);
            
            if(!(sd->flags & SDF_IGNORETEXTURES) && tsec->floorpic != texture)
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
            sec->nextsec    = newsecnum; // link step to next
            tsec->prevsec   = secnum;    // link next back
            tsec->nextsec   = -1;        // set next forward link as end
            tsec->stairlock = -2;        // lock the step
            
            sec = tsec;
            secnum = newsecnum;
            floor = new FloorMoveThinker;
            
            floor->addThinker();
            
            sec->floordata = floor;
            floor->direction = sd->direction ? plat_up : plat_down;
            floor->sector = sec;

            // haleyjd 10/06/05: support synchronized stair raising
            if(sd->flags & SDF_SYNCHRONIZED)
            {
               floor->speed = 
                  D_abs(FixedMul(speed, 
                        FixedDiv(height - sec->floorheight, stairsize)));
            }
            else
               floor->speed = speed;
            
            floor->floordestheight = height;
            floor->crush = sd->crush ? 10 : -1;
            floor->type = genBuildStair; // jff 3/31/98 do not leave uninited

            // haleyjd 10/13/05: init reset and delay properties
            floor->resetTime     = sd->reset_value;
            floor->resetHeight   = sec->floorheight;
            floor->delayTime     = sd->delay_value;      
            floor->stepRaiseTime = FixedDiv(stairsize, speed) >> FRACBITS;            
            floor->delayTimer    = floor->delayTime ? floor->stepRaiseTime : 0;

            P_StairSequence(floor->sector);
            
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
   edefstructvar(stairdata_t, sd);
   int         rtn;
   int         value = line->special - GenStairsBase;

   // parse the bit fields in the line's special type
   sd.direction     = (value & StairDirection) >> StairDirectionShift;
   sd.stepsize_type = (value & StairStep) >> StairStepShift;
   sd.speed_type    = (value & StairSpeed) >> StairSpeedShift;
   sd.trigger_type  = (value & TriggerType) >> TriggerTypeShift;
   sd.flags         = SDF_HAVETRIGGERTYPE;

   if((value & StairIgnore) >> StairIgnoreShift)
      sd.flags |= SDF_IGNORETEXTURES;
   
   // haleyjd 10/06/05: generalized stairs don't support the following
   sd.delay_value    = 0;
   sd.reset_value    = 0;
   sd.speed_value    = 0;
   sd.stepsize_value = 0;

   rtn = EV_DoParamStairs(line, line->args[0], &sd);

   // retriggerable generalized stairs build up or down alternately
   if(rtn)
      line->special ^= StairDirection; // alternate dir on succ activations

   return rtn;
}

//
// EV_DoParamCrusher
//
// ioanch 20160305: parameterized crusher. Should support both Hexen and Doom
// kinds.
//
int EV_DoParamCrusher(const line_t *line, int tag, const crusherdata_t *cd)
{
   int       secnum;
   int       rtn;
   bool      manual;
   sector_t *sec;
   CeilingThinker *ceiling;

   //jff 2/22/98  Reactivate in-stasis ceilings...for certain types.
   //jff 4/5/98 return if activated

   // ioanch 20160305: support this for manual parameterized! But generalized
   // ones should still act like in Boom
   bool manualParam = (cd->flags & CDF_HAVESPAC) && !tag;
   rtn = P_ActivateInStasisCeiling(line, tag, manualParam);

   // check if a manual trigger, if so do just the sector on the backside
   manual = false;
   if(((cd->flags & CDF_HAVETRIGGERTYPE) && 
       (cd->trigger_type == PushOnce || cd->trigger_type == PushMany)) ||
      manualParam)
   {
      if(!(sec = line->backsector))
         return rtn;
      secnum = eindex(sec-sectors);
      manual = true;
      goto manual_crusher;
   }
   
   secnum = -1;
   // if not manual do all sectors tagged the same as the line
   while((secnum = P_FindSectorFromTag(tag,secnum)) >= 0)
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
      ceiling = new CeilingThinker;
      ceiling->addThinker();
      sec->ceilingdata = ceiling; //jff 2/22/98
      ceiling->crush = cd->damage;
      ceiling->direction = plat_down;
      ceiling->sector = sec;
      ceiling->texture = sec->ceilingpic;
      // haleyjd: note: transfer isn't actually used by crushers...
      P_SetupSpecialTransfer(sec, &(ceiling->special));
      ceiling->tag = sec->tag;
      // ioanch 20160305: set the type here
      ceiling->type = cd->type;
      
      switch(cd->type)
      {
      case paramHexenCrush:
      case paramHexenCrushRaiseStay:
      case paramHexenLowerCrush:
         switch(cd->crushmode)
         {
         case crushmodeDoom:
            ceiling->crushflags = 0;
            break;
         case crushmodeHexen:
            ceiling->crushflags = CeilingThinker::crushRest;
            break;
         case crushmodeDoomSlow:
            ceiling->crushflags = CeilingThinker::crushParamSlow;
            break;
         default: // compat or anything else
            // like in ZDoom, decide based on current game
            ceiling->crushflags =         LevelInfo.levelType == LI_TYPE_HEXEN ? 
               CeilingThinker::crushRest      : cd->speed_value == CEILSPEED ?
               CeilingThinker::crushParamSlow : 0;
            break;
         }
         break;
      default:
         ceiling->crushflags = 0;
         break;
      }

      ceiling->topheight = sec->ceilingheight;
      ceiling->bottomheight = sec->floorheight + cd->ground_dist;

      // setup ceiling motion speed
      switch (cd->speed_type)
      {
      case SpeedSlow:
         ceiling->upspeed = ceiling->speed = CEILSPEED;
         break;
      case SpeedNormal:
         ceiling->upspeed = ceiling->speed = CEILSPEED*2;
         break;
      case SpeedFast:
         ceiling->upspeed = ceiling->speed = CEILSPEED*4;
         break;
      case SpeedTurbo:
         ceiling->upspeed = ceiling->speed = CEILSPEED*8;
         break;
      case SpeedParam:
         ceiling->speed = cd->speed_value;
         ceiling->upspeed = cd->upspeed;
         break;
      default:
         break;
      }
      ceiling->oldspeed = ceiling->speed;

      P_AddActiveCeiling(ceiling); // add to list of active ceilings
      // haleyjd 09/29/06
      // ioanch 20160314: use silent for Boom generalized silent crushers,
      // semi-silent for parameterized silent specials, and normal otherwise
      if(cd->flags & CDF_PARAMSILENT)
         ceiling->crushflags |= CeilingThinker::crushSilent;
      P_CeilingSequence(ceiling->sector, cd->type == genSilentCrusher ? 
                        CNOISE_SILENT : (cd->flags & CDF_PARAMSILENT) ? 
                    CNOISE_SEMISILENT : CNOISE_NORMAL);

      if(manual)
         return rtn;
   }
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
int EV_DoGenCrusher(const line_t *line)
{
   edefstructvar(crusherdata_t, cd);
   int value = line->special - GenCrusherBase;

   cd.type = ((value & CrusherSilent) >> CrusherSilentShift != 0) ? 
             genSilentCrusher : genCrusher;
   cd.speed_type = (value & CrusherSpeed) >> CrusherSpeedShift;
   cd.trigger_type = (value & TriggerType) >> TriggerTypeShift;
   cd.damage = 10;
   cd.flags = CDF_HAVETRIGGERTYPE;
   cd.ground_dist = 8 * FRACUNIT;

   return EV_DoParamCrusher(line, line->args[0], &cd);
}



//
// GenDoorRetrigger
//
// haleyjd 02/23/04: This function handles retriggering of certain
// active generalized door types, a functionality which was neglected 
// in BOOM. To be retriggerable, the door must fit these criteria:
// 1. The thinker on the sector must be a VerticalDoorThinker
// 2. The door type must be raise, not open or close
// 3. The activation trigger must be PushMany
//
static int GenDoorRetrigger(Thinker *th, const doordata_t *dd, int tag)
{
   VerticalDoorThinker *door;

   if(!(door = thinker_cast<VerticalDoorThinker *>(th)))
      return 0;

   if(!dd->thing)
      return 0;

   if(door->type != doorNormal && door->type != blazeRaise)
      return 0;

   if(dd->flags & DDF_HAVETRIGGERTYPE) // BOOM-style activation
   {
      // PushMany generalized doors only.
      if(dd->trigger_type != PushMany)
         return 0;
   }
   else if(dd->flags & DDF_HAVESPAC) // Hexen-style activation
   {
      // Must be usable, must be capable of multiple activations, and must be a 
      // manual door action (ie., zero tag)
      if(dd->spac != SPAC_USE || !(dd->flags & DDF_REUSABLE) || tag)
         return 0;
   }

   return door->reTriggerVerticalDoor(!!dd->thing->player);
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
int EV_DoParamDoor(const line_t *line, int tag, const doordata_t *dd)
{
   int secnum, rtn = 0;
   sector_t *sec;
   VerticalDoorThinker *door;
   bool manual = false;

   // check if a manual trigger, if so do just the sector on the backside
   // haleyjd 05/04/04: door actions with no line can't be manual
   // haleyjd 01/03/12: BOOM-style, or Hexen-style?
   if(((dd->flags & DDF_HAVETRIGGERTYPE) && 
       (dd->trigger_type == PushOnce || dd->trigger_type == PushMany)) ||
      ((dd->flags & DDF_HAVESPAC) && !tag))
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      secnum = eindex(sec - sectors);
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
               rtn = GenDoorRetrigger(sec->ceilingdata, dd, tag);

            return rtn;
         }
         continue;
      }

      // new door thinker
      rtn = 1;
      door = new VerticalDoorThinker;
      door->addThinker();
      sec->ceilingdata = door; //jff 2/22/98
      
      door->sector = sec;
      door->turbo  = false;

      door->topwait = dd->delay_value;
      door->speed   = dd->speed_value;

      // killough 10/98: implement gradual lighting
      // haleyjd 02/28/05: support light changes from alternate tag
      if(dd->flags & DDF_USEALTLIGHTTAG)
         door->lighttag = dd->altlighttag;
      else
         door->lighttag = !g_opts.comp[comp_doorlight] && line &&
            (line->special&6) == 6 && 
            line->special > GenLockedBase ? line->args[0] : 0;
      
      // set kind of door, whether it opens then close, opens, closes etc.
      // assign target heights accordingly
      // haleyjd 05/04/04: fixed sound playing; was totally messed up!
      switch(dd->kind)
      {
      case OdCDoor:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         if((door->turbo = (door->speed >= VDOORSPEED*4)))
            door->type = blazeRaise;
         else
            door->type = doorNormal;
         if(door->topheight != sec->ceilingheight)
            P_DoorSequence(true, door->turbo, false, door->sector); // haleyjd
         break;
      case ODoor:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         if((door->turbo = (door->speed >= VDOORSPEED*4)))
            door->type = blazeOpen;
         else
            door->type = doorOpen;
         if(door->topheight != sec->ceilingheight)
            P_DoorSequence(true, door->turbo, false, door->sector); // haleyjd
         break;
      case CdODoor:
         door->topheight = sec->ceilingheight;
         door->direction = plat_down;
         door->turbo = (door->speed >= VDOORSPEED*4);
         door->type = closeThenOpen;
         P_DoorSequence(false, door->turbo, false, door->sector); // haleyjd
         break;
      case CDoor:
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->direction = plat_down;
         if((door->turbo = (door->speed >= VDOORSPEED*4)))
            door->type = blazeClose;
         else
            door->type = doorClose;
         P_DoorSequence(false, door->turbo, false, door->sector); // haleyjd
         break;
      
      // haleyjd: The following door types are parameterized only
      case pDOdCDoor:
         // parameterized "raise in" type
         door->direction = plat_special; // door starts in stasis
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->topcountdown = dd->topcountdown; // wait to start
         door->turbo = (door->speed >= VDOORSPEED*4);
         door->type  = doorRaiseIn;
         break;
      case pDCDoor:
         // parameterized "close in" type
         door->direction    = plat_stop;        // door starts in wait
         door->topcountdown = dd->topcountdown; // wait to start
         door->turbo = (door->speed >= VDOORSPEED*4);
         door->type  = paramCloseIn;
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
// EV_DoGenLockedDoor
//
// Handle generalized locked door types
//
// Passed the linedef activating the generalized locked door
// Returns true if a thinker created
//
// haleyjd 05/04/04: rewritten to use EV_DoParamDoor
//
int EV_DoGenLockedDoor(const line_t *line, Mobj *thing)
{
   doordata_t dd;
   int value = line->special - GenLockedBase;
   int speedType;

   memset(&dd, 0, sizeof(doordata_t));

   dd.thing = thing;

   // parse the bit fields in the line's special type
   dd.trigger_type = (value & TriggerType) >> TriggerTypeShift;
   dd.flags = DDF_HAVETRIGGERTYPE;

   switch(dd.trigger_type)
   {
   case PushMany:
   case SwitchMany:
   case WalkMany:
   case GunMany:
      dd.flags |= DDF_REUSABLE;
      break;
   default:
      break;
   }
   
   dd.delay_value = VDOORWAIT;
   
   speedType = (value & LockedSpeed) >> LockedSpeedShift;

   // setup speed of door motion
   dd.speed_value = VDOORSPEED * (1 << speedType);

   dd.kind = (value & LockedKind) >> LockedKindShift;
   
   return EV_DoParamDoor(line, line->args[0], &dd);
}

//
// EV_DoGenDoor
//
// Handle generalized door types
//
// Passed the linedef activating the generalized door
// Returns true if a thinker created
//
// haleyjd 05/04/04: rewritten to use EV_DoParamDoor
//
int EV_DoGenDoor(const line_t *line, Mobj *thing)
{
   doordata_t dd;
   int value = line->special - GenDoorBase;
   int delayType, speedType;

   memset(&dd, 0, sizeof(doordata_t));

   dd.thing = thing;

   // parse the bit fields in the line's special type
   dd.trigger_type = (value & TriggerType) >> TriggerTypeShift;
   dd.flags = DDF_HAVETRIGGERTYPE;

   switch(dd.trigger_type)
   {
   case PushMany:
   case SwitchMany:
   case WalkMany:
   case GunMany:
      dd.flags |= DDF_REUSABLE;
      break;
   default:
      break;
   }

   delayType = (value & DoorDelay) >> DoorDelayShift;
   
   // setup delay for door remaining open/closed
   switch(delayType)
   {
   default:
   case doorWaitOneSec:
      dd.delay_value = 35;
      break;
   case doorWaitStd:
      dd.delay_value = VDOORWAIT;
      break;
   case doorWaitStd2x:
      dd.delay_value = 2*VDOORWAIT;
      break;
   case doorWaitStd7x:
      dd.delay_value = 7*VDOORWAIT;
      break;
   }
   
   speedType = (value & DoorSpeed) >> DoorSpeedShift;

   // setup speed of door motion
   dd.speed_value  = VDOORSPEED * (1 << speedType);

   dd.kind = (value & DoorKind) >> DoorKindShift;
   
   return EV_DoParamDoor(line, line->args[0], &dd);
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
void P_ChangeLineTex(const char *texture, int pos, int side, int tag, bool usetag)
{
   line_t *l = NULL;
   int linenum, texnum;
   
   texnum = R_FindWall(texture);
   linenum = -1;

   while((l = P_FindLine(tag, &linenum)) != NULL)
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

#if 0
//
// Small Natives
//

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

   if((err = SM_GetSmallString(amx, &flat, params[2])) != AMX_ERR_NONE)
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

   if((err = SM_GetSmallString(amx, &flat, params[2])) != AMX_ERR_NONE)
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

   if((err = SM_GetSmallString(amx, &texname, params[4])) != AMX_ERR_NONE)
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

   if((err = SM_GetSmallString(amx, &texname, params[4])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   P_ChangeLineTex(texname, pos, side, tag, true);

   Z_Free(texname);


   return 0;
}

#endif

//
// Console Command to execute line specials
//

CONSOLE_COMMAND(p_linespec, cf_notnet|cf_level)
{
   if(!Console.argc)
   {
      C_Printf("usage: p_linespec name args\n");
      return;
   }

   ev_binding_t *bind = EV_BindingForName(Console.argv[0]->constPtr());
   if(bind)
   {
      if(EV_CompositeActionFlags(bind->action) & EV_PARAMLINESPEC)
      {
         int args[NUMLINEARGS] = { 0, 0, 0, 0, 0 };
         int numargs = emin(Console.argc - 1, NUMLINEARGS);
         for(int i = 0; i < numargs; i++)
            args[i] = Console.argv[i + 1]->toInt();

         EV_ActivateAction(bind->action, args, players[Console.cmdsrc].mo);
      }
   }
}

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


