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
//  Switches, buttons. Two-state animation. Exits.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "ev_specials.h"
#include "g_game.h"
#include "m_swap.h"
#include "p_info.h"
#include "p_skin.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"

// killough 2/8/98: Remove switch limit

static int *switchlist;                           // killough
static int max_numswitches;                       // killough
static int numswitches;                           // killough

button_t *buttonlist     = NULL; // haleyjd 04/16/08: made dynamic
int      numbuttonsalloc = 0;    // haleyjd 04/16/08: number allocated

//
// P_InitSwitchList()
//
// Only called at game initialization in order to list the set of switches
// and buttons known to the engine. This enables their texture to change
// when activated, and in the case of buttons, change back after a timeout.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called SWITCHES rather than a static table in this module to
// allow wad designers to insert or modify switches.
//
// Lump format is an array of byte packed switchlist_t structures, terminated
// by a structure with episode == -0. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// Rewritten by Lee Killough to remove limit 2/8/98
//
void P_InitSwitchList(void)
{
   int i, index = 0;
   int episode; 
   switchlist_t *alphSwitchList;         //jff 3/23/98 pointer to switch table
   int lumpnum;
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();
   lumpinfo_t  *lump;

   episode = GameModeInfo->switchEpisode;

   //jff 3/23/98 read the switch table from a predefined lump

   // haleyjd 08/29/09: run down the hash chain for SWITCHES
   lump = W_GetLumpNameChain("SWITCHES");
   
   for(lumpnum = lump->index; lumpnum >= 0; lumpnum = lump->next)
   {
      lump = lumpinfo[lumpnum];

      // look for a lump which is of a possibly good size
      if(!strcasecmp(lump->name, "SWITCHES") && 
         lump->size % sizeof(switchlist_t) == 0)
         break;
   }

   if(lumpnum < 0)
      I_Error("P_InitSwitchList: missing SWITCHES lump\n");

   alphSwitchList = (switchlist_t *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));

   for(i = 0; ; i++)
   {
      if(index + 1 >= max_numswitches)
      {
         max_numswitches = max_numswitches ? max_numswitches * 2 : 8;
         switchlist = erealloc(int *, switchlist, sizeof(*switchlist) * max_numswitches);
      }
      if(SwapShort(alphSwitchList[i].episode) <= episode) //jff 5/11/98 endianess
      {
         if(!SwapShort(alphSwitchList[i].episode))
            break;
         switchlist[index++] =
            R_FindWall(alphSwitchList[i].name1);
         switchlist[index++] =
            R_FindWall(alphSwitchList[i].name2);
      }
   }

   numswitches = index / 2;
   switchlist[index] = -1;
   Z_ChangeTag(alphSwitchList, PU_CACHE); //jff 3/23/98 allow table to be freed
}

//
// P_FindFreeButton
//
// haleyjd 04/16/08: Made buttons dynamic.
//
button_t *P_FindFreeButton()
{
   int i;
   int oldnumbuttons;

   // look for a free button
   for(i = 0; i < numbuttonsalloc; ++i)
   {
      if(!buttonlist[i].btimer)
         return &buttonlist[i];
   }

   // if we get here, there are no free buttons. Reallocate.
   oldnumbuttons = numbuttonsalloc;
   numbuttonsalloc = numbuttonsalloc ? numbuttonsalloc * 2 : MAXPLAYERS * 4;
   buttonlist = erealloc(button_t *, buttonlist, numbuttonsalloc * sizeof(button_t));

   // 05/04/08: be sure all the new buttons are initialized
   for(i = oldnumbuttons; i < numbuttonsalloc; ++i)
      memset(&buttonlist[i], 0, sizeof(button_t));

   // return the first new button
   return &buttonlist[oldnumbuttons];
}

//
// P_StartButton
//
// Start a button (retriggerable switch) counting down till it turns off.
//
// Passed the linedef the button is on, which texture on the sidedef contains
// the button, the texture number of the button, and the time the button is
// to remain active in gametics.
// No return value.
//
// haleyjd 04/16/08: rewritten to store indices instead of pointers
//
static void P_StartButton(int sidenum, line_t *line, sector_t *sector, 
                          bwhere_e w, int texture, int time, bool dopopout,
                          const char *startsound)
{
   int i;
   button_t *button;
   
   // See if button is already pressed
   for(i = 0; i < numbuttonsalloc; ++i)
   {
      if(buttonlist[i].btimer && buttonlist[i].side == sidenum)
         return;
   }

   button = P_FindFreeButton();
   
   button->line     = line - lines;
   button->side     = sidenum;
   button->where    = w;
   button->btexture = texture;
   button->btimer   = time;
   button->dopopout = dopopout;

   // 04/19/09: rewritten to use linedef sound origin

   // switch activation sound
   S_StartSoundName(&line->soundorg, startsound);
   
   // haleyjd 04/16/08: and thus dies one of the last static limits.
   // I_Error("P_StartButton: no button slots left!");
}

//
// P_ClearButtons
//
// haleyjd 10/16/05: isolated from P_SpawnSpecials
//
void P_ClearButtons()
{
   if(numbuttonsalloc)
      memset(buttonlist, 0, numbuttonsalloc * sizeof(button_t));
}

//
// P_RunButtons
//
// Check buttons and change texture on timeout.
// haleyjd 10/16/05: moved here and turned into its own function.
// haleyjd 04/16/08: rewritten to use indices instead of pointers
//
void P_RunButtons()
{
   int i;
   button_t *button;

   for(i = 0; i < numbuttonsalloc; ++i)
   {
      button = &buttonlist[i];

      if(button->btimer)
      {
         button->btimer--;
         if(!button->btimer)
         {
            // haleyjd 04/16/08: these are even used for one-use switches now,
            // so that every switch press can have its own sound origin. Only
            // change texture back and make pop-out sound if this is a reusable
            // switch.
            if(button->dopopout)
            {
               line_t *line = &lines[button->line]; // 04/19/09: line soundorgs

               switch(button->where)
               {
               case top:
                  sides[button->side].toptexture = button->btexture;
                  break;
                  
               case middle:
                  sides[button->side].midtexture = button->btexture;
                  break;
                  
               case bottom:
                  sides[button->side].bottomtexture = button->btexture;
                  break;
               }
               
               S_StartSoundName(&line->soundorg, "EE_SwitchOn");
            }
            
            // clear out the button
            memset(button, 0, sizeof(button_t));
         }
      }
   }
}

//
// P_ChangeSwitchTexture()
//
// Function that changes switch wall texture on activation.
//
// Passed the line which the switch is on, whether it's retriggerable,
// and the side it was activated on (haleyjd 03/13/05).
// If not retriggerable, this function clears the line special to insure that
//
// No return value
//
// haleyjd 04/16/08: rewritten for button_t changes.
//
void P_ChangeSwitchTexture(line_t *line, int useAgain, int side)
{
   int       texTop;
   int       texMid;
   int       texBot;
   int       i;
   const char *sound;     // haleyjd
   int       sidenum;
   sector_t *sector;
   
   if(!useAgain)
      line->special = 0;

   sidenum = line->sidenum[side];

   // haleyjd: cannot start a button on a non-existent side
   if(sidenum == -1)
      return;

   // haleyjd 04/16/08: get proper sector pointer
   sector = side ? line->backsector : line->frontsector;

   if(!sector) // ???; really should not happen.
      return;

   texTop = sides[sidenum].toptexture;
   texMid = sides[sidenum].midtexture;
   texBot = sides[sidenum].bottomtexture;

   sound = "EE_SwitchOn"; // haleyjd
   
   // EXIT SWITCH?
   if(line->special == 11)
      sound = "EE_SwitchEx"; // haleyjd

   for(i = 0; i < numswitches * 2; ++i)
   {
      // haleyjd 07/02/09: we do not match "-" as a switch texture
      if(switchlist[i] == 0)
         continue;

      if(switchlist[i] == texTop) // if an upper texture
      {
         sides[sidenum].toptexture = switchlist[i^1]; // chg texture
         
         P_StartButton(sidenum, line, sector, top, switchlist[i], BUTTONTIME,
                       !!useAgain, sound); // start timer
         
         return;
      }
      else if(switchlist[i] == texMid) // if a normal texture
      {
         sides[sidenum].midtexture = switchlist[i^1]; // chg texture
         
         P_StartButton(sidenum, line, sector, middle, switchlist[i], BUTTONTIME,
                       !!useAgain, sound); // start timer
         
         return;
      }
      else if(switchlist[i] == texBot) // if a lower texture
      {
         sides[sidenum].bottomtexture = switchlist[i^1]; //chg texture
         
         P_StartButton(sidenum, line, sector, bottom, switchlist[i], BUTTONTIME,
                       !!useAgain, sound); // start timer
         
         return;
      }
   }
}

//
// P_UseSpecialLine
//
// Called when a thing uses (pushes) a special line.
// Only the front sides of lines are usable.
// Dispatches to the appropriate linedef function handler.
//
// Passed the thing using the line, the line being used, and the side used
// Returns true if a thinker was created
//
bool P_UseSpecialLine(Mobj *thing, line_t *line, int side)
{
   // haleyjd: param lines make sidedness decisions on their own
   bool is_param = EV_IsParamLineSpec(line->special);

   if(side && !is_param) //jff 6/1/98 fix inadvertent deletion of side test
      return false;

   // SoM: only allow switch specials on 3d sides to be triggered if 
   // the mobj is within range of the side.
   // haleyjd 05/02/06: ONLY on two-sided lines.
   if(demo_version >= 331 && (line->flags & ML_3DMIDTEX) &&
      line->backsector && line->sidenum[side] != -1 && 
      sides[line->sidenum[side]].midtexture)
   {
      fixed_t opentop, openbottom;
      fixed_t textop, texbot;
      side_t *sidedef = &sides[line->sidenum[side]];

      opentop = line->frontsector->ceilingheight < line->backsector->ceilingheight ?
                line->frontsector->ceilingheight : line->backsector->ceilingheight;
      
      openbottom = line->frontsector->floorheight > line->backsector->floorheight ?
                   line->frontsector->floorheight : line->backsector->floorheight;

      if(line->flags & ML_DONTPEGBOTTOM)
      {
         texbot = sidedef->rowoffset + openbottom;
         textop = texbot + textures[sidedef->midtexture]->heightfrac;
      }
      else
      {
         textop = opentop + sidedef->rowoffset;
         texbot = textop - textures[sidedef->midtexture]->heightfrac;
      }

      if(thing->z > textop || thing->z + thing->height < texbot)
         return false;
   }

   // haleyjd 02/28/05: parameterized specials
   if(is_param)
      return P_ActivateParamLine(line, thing, side, SPAC_USE);

   //jff 02/04/98 add check here for generalized floor/ceil mover
   if(!demo_compatibility)
   {
      // pointer to line function is NULL by default, set non-null if
      // line special is push or switch generalized linedef type
      int (*linefunc)(line_t *line)=NULL;

      // check each range of generalized linedefs
      if((unsigned)line->special >= GenFloorBase)
      {
         if(!thing->player)
         {
            if((line->special & FloorChange) ||
               !(line->special & FloorModel))
               return false; // FloorModel is "Allow Monsters" if FloorChange is 0
         }
         if(!line->tag && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
            return false;                         // generalized types require tag
         linefunc = EV_DoGenFloor;
      }
      else if((unsigned)line->special >= GenCeilingBase)
      {
         if(!thing->player)
         {
            if((line->special & CeilingChange) ||
               !(line->special & CeilingModel))
               return false;   // CeilingModel is "Allow Monsters" if CeilingChange is 0
         }
         if(!line->tag && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
            return false;                         // generalized types require tag
         linefunc = EV_DoGenCeiling;
      }
      else if((unsigned)line->special >= GenDoorBase)
      {
         if(!thing->player)
         {
            if(!(line->special & DoorMonster))
               return false;   // monsters disallowed from this door
            if(line->flags & ML_SECRET) // they can't open secret doors either
               return false;
         }
         if(!line->tag && ((line->special&6)!=6)) //jff 3/2/98 all non-manual
            return false;                         // generalized types require tag
         genDoorThing = thing;
         linefunc = EV_DoGenDoor;
      }
      else if((unsigned)line->special >= GenLockedBase)
      {
         if(!thing->player)
            return false;   // monsters disallowed from unlocking doors
         if(!P_CanUnlockGenDoor(line,thing->player))
            return false;
         if(!line->tag && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
            return false;                         // generalized types require tag

         genDoorThing = thing;
         linefunc = EV_DoGenLockedDoor;
      }
      else if((unsigned)line->special >= GenLiftBase)
      {
         if(!thing->player)
         {
            if(!(line->special & LiftMonster))
               return false; // monsters disallowed
         }
         if(!line->tag && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
            return false;                         // generalized types require tag
         linefunc = EV_DoGenLift;
      }
      else if ((unsigned)line->special >= GenStairsBase)
      {
         if(!thing->player)
         {
            if(!(line->special & StairMonster))
               return false; // monsters disallowed
         }
         if(!line->tag && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
            return false;                         // generalized types require tag
         linefunc = EV_DoGenStairs;
      }
      else if ((unsigned)line->special >= GenCrusherBase)
      {
         if(!thing->player)
         {
            if(!(line->special & CrusherMonster))
               return false; // monsters disallowed
         }
         if(!line->tag && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
            return false;                         // generalized types require tag
         linefunc = EV_DoGenCrusher;
      }

      if(linefunc)
      {
         switch((line->special & TriggerType) >> TriggerTypeShift)
         {
         case PushOnce:
            if(!side)
               if(linefunc(line))
                  line->special = 0;
            return true;
         case PushMany:
            if(!side)
               linefunc(line);
            return true;
         case SwitchOnce:
            if(linefunc(line))
               P_ChangeSwitchTexture(line,0,0);
            return true;
         case SwitchMany:
            if(linefunc(line))
               P_ChangeSwitchTexture(line,1,0);
            return true;
         default:  // if not a switch/push type, do nothing here
            return false;
         }
      }
   }
    
   // Switches that other things can activate.
   if(!thing->player)
   {
      // never open secret doors
      if(line->flags & ML_SECRET)
         return false;
      
      switch(line->special)
      {
      case 1:         // MANUAL DOOR RAISE
      case 32:        // MANUAL BLUE           - haleyjd 01/23/12: !?!!?!??!?!!
      case 33:        // MANUAL RED            - This is why monsters get stuck on key doors...
      case 34:        // MANUAL YELLOW
         //jff 3/5/98 add ability to use teleporters for monsters
      case 195:       // switch teleporters
      case 174:
      case 210:       // silent switch teleporters
      case 209:
         break;
         
      default:
         return false;
      }
   }

   if(!P_CheckTag(line))  //jff 2/27/98 disallow zero tag on some types
      return false;

   // Dispatch to handler according to linedef type
   switch(line->special)
   {
      // Manual doors, push type with no tag
   case 1:             // Vertical Door
   case 26:            // Blue Door/Locked
   case 27:            // Yellow Door /Locked
   case 28:            // Red Door /Locked
      
   case 31:            // Manual door open
   case 32:            // Blue locked door open
   case 33:            // Red locked door open
   case 34:            // Yellow locked door open
      
   case 117:           // Blazing door raise
   case 118:           // Blazing door open
      EV_VerticalDoor(line, thing);          // haleyjd: note ignored return value...
      break;
        
      // Switches (non-retriggerable)
   case 7:
      // Build Stairs
      if(EV_BuildStairs(line,build8))
         P_ChangeSwitchTexture(line,0,0);
      break;

   case 9:
      // Change Donut
      if (EV_DoDonut(line))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 11:
      // Exit level
      // killough 10/98: prevent zombies from exiting levels
      if(thing->player && thing->player->health <= 0 && 
         !comp[comp_zombie])
      {
         S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
         return false;
      }
      P_ChangeSwitchTexture(line,0,0);
      G_ExitLevel ();
      break;
        
   case 14:
      // Raise Floor 32 and change texture
      if (EV_DoPlat(line,raiseAndChange,32))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 15:
      // Raise Floor 24 and change texture
      if (EV_DoPlat(line,raiseAndChange,24))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 18:
      // Raise Floor to next highest floor
      if (EV_DoFloor(line, raiseFloorToNearest))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 20:
      // Raise Plat next highest floor and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 21:
      // PlatDownWaitUpStay
      if (EV_DoPlat(line,downWaitUpStay,0))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 23:
      // Lower Floor to Lowest
      if (EV_DoFloor(line,lowerFloorToLowest))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 29:
      // Raise Door
      if (EV_DoDoor(line,doorNormal))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 41:
      // Lower Ceiling to Floor
      if (EV_DoCeiling(line,lowerToFloor))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 71:
      // Turbo Lower Floor
      if (EV_DoFloor(line,turboLower))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 49:
      // Ceiling Crush And Raise
      if (EV_DoCeiling(line,crushAndRaise))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 50:
      // Close Door
      if (EV_DoDoor(line,doorClose))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 51:
      // Secret EXIT
      // killough 10/98: prevent zombies from exiting levels
      if(thing->player && thing->player->health <= 0 &&
         !comp[comp_zombie])
      {
         S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
         return false;
      }
      P_ChangeSwitchTexture(line,0,0);
      G_SecretExitLevel ();
      break;
        
   case 55:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 101:
      // Raise Floor
      if (EV_DoFloor(line,raiseFloor))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 102:
      // Lower Floor to Surrounding floor height
      if (EV_DoFloor(line,lowerFloor))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 103:
      // Open Door
      if (EV_DoDoor(line,doorOpen))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 111:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor (line,blazeRaise))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 112:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 113:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 122:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 127:
      // Build Stairs Turbo 16
      if (EV_BuildStairs(line,turbo16))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 131:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 133:
      // BlzOpenDoor BLUE
   case 135:
      // BlzOpenDoor RED
   case 137:
      // BlzOpenDoor YELLOW
      if(EV_DoLockedDoor (line,blazeOpen,thing))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 140:
      // Raise Floor 512
      if (EV_DoFloor(line,raiseFloor512))
         P_ChangeSwitchTexture(line,0,0);
      break;

      // killough 1/31/98: factored out compatibility check;
      // added inner switch, relaxed check to demo_compatibility

   default:
      if(!demo_compatibility)
      {
         switch(line->special)
         {
            //jff 1/29/98 added linedef types to fill all functions out so that
            // all possess SR, S1, WR, W1 types

         case 158:
            // Raise Floor to shortest lower texture
            // 158 S1  EV_DoFloor(raiseToTexture), CSW(0)
            if (EV_DoFloor(line,raiseToTexture))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 159:
            // Raise Floor to shortest lower texture
            // 159 S1  EV_DoFloor(lowerAndChange)
            if (EV_DoFloor(line,lowerAndChange))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 160:
            // Raise Floor 24 and change
            // 160 S1  EV_DoFloor(raiseFloor24AndChange)
            if (EV_DoFloor(line,raiseFloor24AndChange))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 161:
            // Raise Floor 24
            // 161 S1  EV_DoFloor(raiseFloor24)
            if (EV_DoFloor(line,raiseFloor24))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 162:
            // Moving floor min n to max n
            // 162 S1  EV_DoPlat(perpetualRaise,0)
            if (EV_DoPlat(line,perpetualRaise,0))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 163:
            // Stop Moving floor
            // 163 S1  EV_DoPlat(perpetualRaise,0)
            EV_StopPlat(line);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 164:
            // Start fast crusher
            // 164 S1  EV_DoCeiling(fastCrushAndRaise)
            if (EV_DoCeiling(line,fastCrushAndRaise))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 165:
            // Start slow silent crusher
            // 165 S1  EV_DoCeiling(silentCrushAndRaise)
            if (EV_DoCeiling(line,silentCrushAndRaise))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 166:
            // Raise ceiling, Lower floor
            // 166 S1 EV_DoCeiling(raiseToHighest), EV_DoFloor(lowerFloortoLowest)
            if(EV_DoCeiling(line, raiseToHighest) ||
               EV_DoFloor(line, lowerFloorToLowest))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 167:
            // Lower floor and Crush
            // 167 S1 EV_DoCeiling(lowerAndCrush)
            if (EV_DoCeiling(line, lowerAndCrush))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 168:
            // Stop crusher
            // 168 S1 EV_CeilingCrushStop()
            if (EV_CeilingCrushStop(line))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 169:
            // Lights to brightest neighbor sector
            // 169 S1  EV_LightTurnOn(0)
            EV_LightTurnOn(line,0);
            P_ChangeSwitchTexture(line,0,0);
            break;

         case 170:
            // Lights to near dark
            // 170 S1  EV_LightTurnOn(35)
            EV_LightTurnOn(line,35);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 171:
            // Lights on full
            // 171 S1  EV_LightTurnOn(255)
            EV_LightTurnOn(line,255);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 172:
            // Start Lights Strobing
            // 172 S1  EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 173:
            // Lights to Dimmest Near
            // 173 S1  EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 174:
            // Teleport
            // 174 S1  EV_Teleport(side,thing)
            if (EV_Teleport(line,side,thing))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 175:
            // Close Door, Open in 30 secs
            // 175 S1  EV_DoDoor(close30ThenOpen)
            if (EV_DoDoor(line, closeThenOpen))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 189: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Trigger)
            // 189 S1 Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 203:
            // Lower ceiling to lowest surrounding ceiling
            // 203 S1 EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 204:
            // Lower ceiling to highest surrounding floor
            // 204 S1 EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 209:
            // killough 1/31/98: silent teleporter
            //jff 209 S1 SilentTeleport 
            if (EV_SilentTeleport(line, side, thing))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 241: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Numeric)
            // 241 S1 Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 221:
            // Lower floor to next lowest floor
            // 221 S1 Lower Floor To Nearest Floor
            if (EV_DoFloor(line,lowerFloorToNearest))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 229:
            // Raise elevator next floor
            // 229 S1 Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 233:
            // Lower elevator next floor
            // 233 S1 Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 237:
            // Elevator to current floor
            // 237 S1 Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
            
            // jff 1/29/98 end of added S1 linedef types
            
            //jff 1/29/98 added linedef types to fill all functions out so that
            // all possess SR, S1, WR, W1 types
            
         case 78: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Numeric)
            // 78 SR Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 176:
            // Raise Floor to shortest lower texture
            // 176 SR  EV_DoFloor(raiseToTexture), CSW(1)
            if (EV_DoFloor(line,raiseToTexture))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 177:
            // Raise Floor to shortest lower texture
            // 177 SR  EV_DoFloor(lowerAndChange)
            if (EV_DoFloor(line,lowerAndChange))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 178:
            // Raise Floor 512
            // 178 SR  EV_DoFloor(raiseFloor512)
            if (EV_DoFloor(line,raiseFloor512))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 179:
            // Raise Floor 24 and change
            // 179 SR  EV_DoFloor(raiseFloor24AndChange)
            if (EV_DoFloor(line,raiseFloor24AndChange))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 180:
            // Raise Floor 24
            // 180 SR  EV_DoFloor(raiseFloor24)
            if (EV_DoFloor(line,raiseFloor24))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 181:
            // Moving floor min n to max n
            // 181 SR  EV_DoPlat(perpetualRaise,0)
            EV_DoPlat(line,perpetualRaise,0);
            P_ChangeSwitchTexture(line,1,0);
            break;

         case 182:
            // Stop Moving floor
            // 182 SR  EV_DoPlat(perpetualRaise,0)
            EV_StopPlat(line);
            P_ChangeSwitchTexture(line,1,0);
            break;

         case 183:
            // Start fast crusher
            // 183 SR  EV_DoCeiling(fastCrushAndRaise)
            if (EV_DoCeiling(line,fastCrushAndRaise))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 184:
            // Start slow crusher
            // 184 SR  EV_DoCeiling(crushAndRaise)
            if(EV_DoCeiling(line,crushAndRaise))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 185:
            // Start slow silent crusher
            // 185 SR  EV_DoCeiling(silentCrushAndRaise)
            if (EV_DoCeiling(line,silentCrushAndRaise))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 186:
            // Raise ceiling, Lower floor
            // 186 SR EV_DoCeiling(raiseToHighest), EV_DoFloor(lowerFloortoLowest)
            if(EV_DoCeiling(line, raiseToHighest) ||
               EV_DoFloor(line, lowerFloorToLowest))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 187:
            // Lower floor and Crush
            // 187 SR EV_DoCeiling(lowerAndCrush)
            if (EV_DoCeiling(line, lowerAndCrush))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 188:
            // Stop crusher
            // 188 SR EV_CeilingCrushStop()
            if (EV_CeilingCrushStop(line))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 190: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Trigger)
            // 190 SR Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 191:
            // Lower Pillar, Raise Donut
            // 191 SR  EV_DoDonut()
            if (EV_DoDonut(line))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 192:
            // Lights to brightest neighbor sector
            // 192 SR  EV_LightTurnOn(0)
            EV_LightTurnOn(line,0);
            P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 193:
            // Start Lights Strobing
            // 193 SR  EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 194:
            // Lights to Dimmest Near
            // 194 SR  EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 195:
            // Teleport
            // 195 SR  EV_Teleport(side,thing)
            if (EV_Teleport(line,side,thing))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 196:
            // Close Door, Open in 30 secs
            // 196 SR  EV_DoDoor(close30ThenOpen)
            if (EV_DoDoor(line, closeThenOpen))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 205:
            // Lower ceiling to lowest surrounding ceiling
            // 205 SR EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 206:
            // Lower ceiling to highest surrounding floor
            // 206 SR EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 210:
            // killough 1/31/98: silent teleporter
            //jff 210 SR SilentTeleport 
            if (EV_SilentTeleport(line, side, thing))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 211: //jff 3/14/98 create instant toggle floor type
            // Toggle Floor Between C and F Instantly
            // 211 SR Toggle Floor Instant
            if (EV_DoPlat(line,toggleUpDn,0))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 222:
            // Lower floor to next lowest floor
            // 222 SR Lower Floor To Nearest Floor
            if (EV_DoFloor(line,lowerFloorToNearest))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 230:
            // Raise elevator next floor
            // 230 SR Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 234:
            // Lower elevator next floor
            // 234 SR Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 238:
            // Elevator to current floor
            // 238 SR Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 258:
            // Build stairs, step 8
            // 258 SR EV_BuildStairs(build8)
            if (EV_BuildStairs(line,build8))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 259:
            // Build stairs, step 16
            // 259 SR EV_BuildStairs(turbo16)
            if (EV_BuildStairs(line,turbo16))
               P_ChangeSwitchTexture(line,1,0);
            break;
            // 1/29/98 jff end of added SR linedef types
            
            // sf: scripting
         case 277: // S1 start script
            line->special = 0;
         case 276: // SR start script
            P_ChangeSwitchTexture(line, (line->special ? 1 : 0), 0);
            P_StartLineScript(line, thing);
            break;            
         }
      }
      break;

      // Buttons (retriggerable switches)
   case 42:
      // Close Door
      if (EV_DoDoor(line,doorClose))
         P_ChangeSwitchTexture(line,1,0);
      break;
        
   case 43:
      // Lower Ceiling to Floor
      if (EV_DoCeiling(line,lowerToFloor))
         P_ChangeSwitchTexture(line,1,0);
      break;
        
   case 45:
      // Lower Floor to Surrounding floor height
      if (EV_DoFloor(line,lowerFloor))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 60:
      // Lower Floor to Lowest
      if (EV_DoFloor(line,lowerFloorToLowest))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 61:
      // Open Door
      if (EV_DoDoor(line,doorOpen))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 62:
      // PlatDownWaitUpStay
      if (EV_DoPlat(line,downWaitUpStay,1))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 63:
      // Raise Door
      if (EV_DoDoor(line,doorNormal))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 64:
      // Raise Floor to ceiling
      if (EV_DoFloor(line,raiseFloor))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 66:
      // Raise Floor 24 and change texture
      if (EV_DoPlat(line,raiseAndChange,24))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 67:
      // Raise Floor 32 and change texture
      if (EV_DoPlat(line,raiseAndChange,32))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 65:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 68:
      // Raise Plat to next highest floor and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 69:
      // Raise Floor to next highest floor
      if (EV_DoFloor(line, raiseFloorToNearest))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 70:
      // Turbo Lower Floor
      if (EV_DoFloor(line,turboLower))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 114:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor (line,blazeRaise))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 115:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 116:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 123:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 132:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 99:
      // BlzOpenDoor BLUE
   case 134:
      // BlzOpenDoor RED
   case 136:
      // BlzOpenDoor YELLOW
      if (EV_DoLockedDoor (line,blazeOpen,thing))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 138:
      // Light Turn On
      EV_LightTurnOn(line,255);
      P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 139:
      // Light Turn Off
      EV_LightTurnOn(line,35);
      P_ChangeSwitchTexture(line,1,0);
      break;
   }
   return true;
}



//----------------------------------------------------------------------------
//
// $Log: p_switch.c,v $
// Revision 1.25  1998/06/01  14:48:19  jim
// Fix switch use from back side
//
// Revision 1.24  1998/05/11  14:04:46  jim
// Fix endianess of speed field in SWITCH predefined lump
//
// Revision 1.23  1998/05/07  21:30:40  jim
// formatted/documented p_switch
//
// Revision 1.22  1998/05/03  23:17:44  killough
// Fix #includes at the top, nothing else
//
// Revision 1.21  1998/03/24  22:10:32  jim
// Fixed switch won't light problem in UDOOM
//
// Revision 1.20  1998/03/23  18:39:45  jim
// Switch and animation tables now lumps
//
// Revision 1.19  1998/03/16  15:47:18  killough
// Merge Jim's linedef changes
//
// Revision 1.18  1998/03/15  14:39:58  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.17  1998/03/14  17:19:15  jim
// Added instant toggle floor type
//
// Revision 1.16  1998/03/12  21:54:23  jim
// Freed up 12 linedefs for use as vectors
//
// Revision 1.15  1998/03/05  16:59:22  jim
// Fixed inability of monsters/barrels to use new teleports
//
// Revision 1.14  1998/03/02  15:33:10  jim
// fixed errors in numeric model sector search and 0 tag trigger defeats
//
// Revision 1.13  1998/02/28  01:25:06  jim
// Fixed error in 0 tag trigger fix
//
// Revision 1.11  1998/02/23  00:42:17  jim
// Implemented elevators
//
// Revision 1.10  1998/02/13  03:28:49  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.9  1998/02/09  03:11:03  killough
// Remove limit on switches
//
// Revision 1.8  1998/02/08  05:35:54  jim
// Added generalized linedef types
//
// Revision 1.6  1998/02/02  13:39:27  killough
// Program beautification
//
// Revision 1.5  1998/01/30  14:44:20  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:24:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

