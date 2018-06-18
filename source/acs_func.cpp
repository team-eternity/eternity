// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2016 James Haley, David Hill, et al.
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
//----------------------------------------------------------------------------
//
// ACS CALLFUNC functions.
//
// Some of these are used to implement what have historically been discreet ACS
// instructions. They are moved here in the interest of keeping the core
// interpreter loop as clean as possible.
//
// Generally speaking, instructions that call an external function should go
// here instead of acs_intr.cpp.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"
#include "c_runcmd.h"
#include "d_event.h"
#include "d_gi.h"
#include "e_args.h"
#include "e_exdata.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
#include "ev_specials.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_random.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_maputl.h"
#include "p_portal.h"   // ioanch 20160116
#include "p_spec.h"
#include "p_xenemy.h"
#include "r_data.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sndseq.h"
#include "v_misc.h"
#include "doomstat.h"

#include "ACSVM/Scope.hpp"
#include "ACSVM/Thread.hpp"


//=============================================================================
// Local Utilities
//

//
// ACS_ChkThingProp
//
static bool ACS_ChkThingProp(ACSThread *thread, int32_t tid, uint32_t prop, uint32_t val)
{
   Mobj *mo = P_FindMobjFromTID(tid, nullptr, thread->info.mo);
   thread->dataStk.push(mo ? ACS_ChkThingProp(mo, prop, val) : 0);
   return false;
}

//
// ACS_GetThingProp
//
static bool ACS_GetThingProp(ACSThread *thread, int32_t tid, uint32_t prop)
{
   Mobj *mo = P_FindMobjFromTID(tid, nullptr, thread->info.mo);
   thread->dataStk.push(mo ? ACS_GetThingProp(mo, prop) : 0);
   return false;
}

//
// ACS_SetThingProp
//
static bool ACS_SetThingProp(ACSThread *thread, int32_t tid, uint32_t prop, uint32_t val)
{
   Mobj *mo = nullptr;
   while((mo = P_FindMobjFromTID(tid, mo, thread->info.mo)))
      ACS_SetThingProp(mo, prop, val);
   return false;
}

//=============================================================================
// Helpers
//
inline static PointThinker *ACS_getSoundSource(const ACSThreadInfo *info)
{
   // It can be activated both by polyobjects and mobjs. In that case, 
   // prioritize mobj
   return info->mo ? info->mo : info->po ? &info->po->spawnSpot : nullptr;
}

//=============================================================================
// CallFuncs
//

//
// ACS_CF_ActivatorArmor
//
// int ActivatorArmor(void);
//
bool ACS_CF_ActivatorArmor(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), 0, ACS_TP_Armor);
}

//
// ACS_CF_ActivatorFrags
//
// int ActivatorFrags(void);
//
bool ACS_CF_ActivatorFrags(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), 0, ACS_TP_Frags);
}

//
// ACS_CF_ActivatorHealth
//
// int ActivatorHealth(void);
//
bool ACS_CF_ActivatorHealth(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), 0, ACS_TP_Health);
}

//
// ACS_CF_ActivatorSigil
//
// int GetSigilPieces(void);
//
bool ACS_CF_ActivatorSigil(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), 0, ACS_TP_SigilPieces);
}

//
// ACS_CF_ActivatorSound
//
// void ActivatorSound(str sound, int volume);
//
bool ACS_CF_ActivatorSound(ACS_CF_ARGS)
{
   auto        info = &static_cast<ACSThread *>(thread)->info;
   const char *snd  = thread->scopeMap->getString(argV[0])->str;
   int         vol  = argV[1];

   S_StartSoundNameAtVolume(ACS_getSoundSource(info), snd, vol, ATTN_NORMAL, CHAN_AUTO);

   return false;
}

//
// ACS_CF_ActivatorTID
//
// int ActivatorTID(void)
//
bool ACS_CF_ActivatorTID(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), 0, ACS_TP_TID);
}

//
// ACS_CF_AmbientSound
//
// void AmbientSound(str sound, int volume);
//
bool ACS_CF_AmbientSound(ACS_CF_ARGS)
{
   const char *snd = thread->scopeMap->getString(argV[0])->str;
   int         vol = argV[1];

   S_StartSoundNameAtVolume(NULL, snd, vol, ATTN_NORMAL, CHAN_AUTO);

   return false;
}

//
// ACS_CF_AmbientSoundLoc
//
// void LocalAmbientSound(str sound, int volume);
//
bool ACS_CF_AmbientSoundLoc(ACS_CF_ARGS)
{
   auto        info = &static_cast<ACSThread *>(thread)->info;
   const char *snd  = thread->scopeMap->getString(argV[0])->str;
   int         vol  = argV[1];

   if(info->mo == players[displayplayer].mo)
      S_StartSoundNameAtVolume(NULL, snd, vol, ATTN_NORMAL, CHAN_AUTO);

   return false;
}

//
// ACS_CF_ATan2
//
// fixed ATan2(fixed x, fixed y);
//
bool ACS_CF_ATan2(ACS_CF_ARGS)
{
   thread->dataStk.push(P_PointToAngle(0, 0, argV[0], argV[1]));
   return false;
}

//
// ACS_CF_ChangeCeil
//
// void ChangeCeiling(int tag, str tex);
//
bool ACS_CF_ChangeCeil(ACS_CF_ARGS)
{
   P_ChangeCeilingTex(thread->scopeMap->getString(argV[1])->str, argV[0]);
   return false;
}

//
// ACS_CF_ChangeFloor
//
// void ChangeFloor(int tag, str tex);
//
bool ACS_CF_ChangeFloor(ACS_CF_ARGS)
{
   P_ChangeFloorTex(thread->scopeMap->getString(argV[1])->str, argV[0]);
   return false;
}

//
// ACS_CF_CheckSight
//
// int CheckSight(int tid1, int tid2, int flags);
//
bool ACS_CF_CheckSight(ACS_CF_ARGS)
{
   auto    info = &static_cast<ACSThread *>(thread)->info;
   int32_t tid1 = argV[0];
   int32_t tid2 = argV[1];
   Mobj   *mo1  = nullptr;
   Mobj   *mo2  = nullptr;

   while((mo1 = P_FindMobjFromTID(tid1, mo1, info->mo)))
   {
      while((mo2 = P_FindMobjFromTID(tid2, mo2, info->mo)))
      {
         if(P_CheckSight(mo1, mo2))
         {
            thread->dataStk.push(1);
            return false;
         }
      }
   }

   thread->dataStk.push(0);
   return false;
}

//
// ACS_CF_ChkThingCeilTex
//
// int CheckThingCeilingTexture(int tid, str texture)
//
bool ACS_CF_ChkThingCeilTex(ACS_CF_ARGS)
{
   return ACS_ChkThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_CeilTex, argV[1]);
}

//
// ACS_CF_ChkThingFlag
//
// int CheckThingFlag(int tid, str flag);
//
bool ACS_CF_ChkThingFlag(ACS_CF_ARGS)
{
   auto        info = &static_cast<ACSThread *>(thread)->info;
   Mobj       *mo   = P_FindMobjFromTID(argV[0], nullptr, info->mo);
   dehflags_t *flag = deh_ParseFlagCombined(thread->scopeMap->getString(argV[1])->str);
   bool        res = false;

   if(mo && flag) switch(flag->index)
   {
   case 0: res = !!(mo->flags  & flag->value); break;
   case 1: res = !!(mo->flags2 & flag->value); break;
   case 2: res = !!(mo->flags3 & flag->value); break;
   case 3: res = !!(mo->flags4 & flag->value); break;
   }

   thread->dataStk.push(res);
   return false;
}

//
// ACS_CF_ChkThingFloorTex
//
// int CheckThingFloorTexture(int tid, str texture)
//
bool ACS_CF_ChkThingFloorTex(ACS_CF_ARGS)
{
   return ACS_ChkThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_FloorTex, argV[1]);
}

//
// ACS_ChkThingProp
//
bool ACS_ChkThingProp(Mobj *mo, uint32_t var, uint32_t val)
{
   if(!mo) return false;

   switch(var)
   {
   case ACS_TP_Health:       return static_cast<uint32_t>(mo->health) == val;
   case ACS_TP_Speed:        return static_cast<uint32_t>(mo->info->speed) == val;
   case ACS_TP_Damage:       return static_cast<uint32_t>(mo->damage) == val;
   case ACS_TP_Alpha:        return static_cast<uint32_t>(mo->translucency) == val;
   case ACS_TP_RenderStyle:  return false;
   case ACS_TP_SeeSound:     return false;
   case ACS_TP_AttackSound:  return false;
   case ACS_TP_PainSound:    return false;
   case ACS_TP_DeathSound:   return false;
   case ACS_TP_ActiveSound:  return false;
   case ACS_TP_Ambush:       return !!(mo->flags & MF_AMBUSH) == !!val;
   case ACS_TP_Invulnerable: return !!(mo->flags2 & MF2_INVULNERABLE) == !!val;
   case ACS_TP_JumpZ:        return false;
   case ACS_TP_ChaseGoal:    return false;
   case ACS_TP_Frightened:   return false;
   case ACS_TP_Friendly:     return !!(mo->flags & MF_FRIEND) == !!val;
   case ACS_TP_SpawnHealth:  return static_cast<uint32_t>(mo->getModifiedSpawnHealth()) == val;
   case ACS_TP_Dropped:      return !!(mo->flags & MF_DROPPED) == !!val;
   case ACS_TP_NoTarget:     return false;
   case ACS_TP_Species:      return false;
   case ACS_TP_NameTag:      return false;
   case ACS_TP_Score:        return false;
   case ACS_TP_NoTrigger:    return false;
   case ACS_TP_DamageFactor: return false;
   case ACS_TP_MasterTID:    return false;
   case ACS_TP_TargetTID:    return mo->target ? mo->target->tid == val : false;
   case ACS_TP_TracerTID:    return mo->tracer ? mo->tracer->tid == val : false;
   case ACS_TP_WaterLevel:   return false;
   case ACS_TP_ScaleX:       return static_cast<uint32_t>(M_FloatToFixed(mo->xscale)) == val;
   case ACS_TP_ScaleY:       return static_cast<uint32_t>(M_FloatToFixed(mo->yscale)) == val;
   case ACS_TP_Dormant:      return !!(mo->flags2 & MF2_DORMANT) == !!val;
   case ACS_TP_Mass:         return static_cast<uint32_t>(mo->info->mass) == val;
   case ACS_TP_Accuracy:     return false;
   case ACS_TP_Stamina:      return false;
   case ACS_TP_Height:       return static_cast<uint32_t>(mo->height) == val;
   case ACS_TP_Radius:       return static_cast<uint32_t>(mo->radius) == val;
   case ACS_TP_ReactionTime: return static_cast<uint32_t>(mo->reactiontime) == val;
   case ACS_TP_MeleeRange:   return MELEERANGE == val;
   case ACS_TP_ViewHeight:   return false;
   case ACS_TP_AttackZOff:   return false;
   case ACS_TP_StencilColor: return false;
   case ACS_TP_Friction:     return false;
   case ACS_TP_DamageMult:   return false;
   case ACS_TP_Counter0:     return static_cast<uint32_t>(mo->counters[0]) == val;
   case ACS_TP_Counter1:     return static_cast<uint32_t>(mo->counters[1]) == val;
   case ACS_TP_Counter2:     return static_cast<uint32_t>(mo->counters[2]) == val;
   case ACS_TP_Counter3:     return static_cast<uint32_t>(mo->counters[3]) == val;
   case ACS_TP_Counter4:     return static_cast<uint32_t>(mo->counters[4]) == val;
   case ACS_TP_Counter5:     return static_cast<uint32_t>(mo->counters[5]) == val;
   case ACS_TP_Counter6:     return static_cast<uint32_t>(mo->counters[6]) == val;
   case ACS_TP_Counter7:     return static_cast<uint32_t>(mo->counters[7]) == val;

   case ACS_TP_Angle:        return mo->angle >> 16 == (uint32_t)val;
   case ACS_TP_Armor:        return mo->player ?
                                    static_cast<uint32_t>(mo->player->armorpoints) == val : false;
   case ACS_TP_CeilTex:      return mo->subsector->sector->ceilingpic == R_FindWall(ACSenv.getString(val)->str);
   case ACS_TP_CeilZ:        return static_cast<uint32_t>(mo->ceilingz) == val;
   case ACS_TP_FloorTex:     return mo->subsector->sector->floorpic == R_FindWall(ACSenv.getString(val)->str);
   case ACS_TP_FloorZ:       return static_cast<uint32_t>(mo->floorz) == val;
   case ACS_TP_Frags:        return mo->player ?
                                    static_cast<uint32_t>(mo->player->totalfrags) == val : false;
   case ACS_TP_LightLevel:   return static_cast<uint32_t>(mo->subsector->sector->lightlevel) == val;
   case ACS_TP_MomX:         return static_cast<uint32_t>(mo->momx) == val;
   case ACS_TP_MomY:         return static_cast<uint32_t>(mo->momy) == val;
   case ACS_TP_MomZ:         return static_cast<uint32_t>(mo->momz) == val;
   case ACS_TP_Pitch:        return mo->player ?
                                    static_cast<uint32_t>(mo->player->pitch >> 16) == val : false;
   case ACS_TP_PlayerNumber: return mo->player ? mo->player - players == val : false;
   case ACS_TP_SigilPieces:  return false;
   case ACS_TP_TID:          return mo->tid == val;
   case ACS_TP_Type:         return mo->type == E_ThingNumForCompatName(ACSenv.getString(val)->str);
   case ACS_TP_X:            return static_cast<uint32_t>(mo->x) == val;
   case ACS_TP_Y:            return static_cast<uint32_t>(mo->y) == val;
   case ACS_TP_Z:            return static_cast<uint32_t>(mo->z) == val;

   default: return false;
   }
}

//
// ACS_CF_ChkThingProp
//
// int CheckThingProperty(int tid, int prop, int val);
//
bool ACS_CF_ChkThingProp(ACS_CF_ARGS)
{
   return ACS_ChkThingProp(static_cast<ACSThread *>(thread), argV[0], argV[1], argV[2]);
}

//
// ACS_CF_ChkThingType
//
// int CheckThingType(int tid, str type)
//
bool ACS_CF_ChkThingType(ACS_CF_ARGS)
{
   return ACS_ChkThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Type, argV[1]);
}

// Flags for ClassifyThing.
enum
{
   THINGCLASS_NONE       = 0x00000000,
   THINGCLASS_WORLD      = 0x00000001,
   THINGCLASS_PLAYER     = 0x00000002,
   THINGCLASS_BOT        = 0x00000004,
   THINGCLASS_VOODOODOLL = 0x00000008,
   THINGCLASS_MONSTER    = 0x00000010,
   THINGCLASS_ALIVE      = 0x00000020,
   THINGCLASS_DEAD       = 0x00000040,
   THINGCLASS_MISSILE    = 0x00000080,
   THINGCLASS_GENERIC    = 0x00000100,
};

//
// ACS_CF_ClassifyThing
//
// int ClassifyThing(int tid);
//
bool ACS_CF_ClassifyThing(ACS_CF_ARGS)
{
   auto     info = &static_cast<ACSThread *>(thread)->info;
   int32_t  tid  = argV[0];
   uint32_t res  = 0;
   Mobj    *mo;

   mo = P_FindMobjFromTID(tid, nullptr, info->mo);

   if(mo)
   {
      if(mo->player)
      {
         res |= THINGCLASS_PLAYER;

         if(mo->player->mo != mo)
            res |= THINGCLASS_VOODOODOLL;
      }

      if(mo->flags & MF_MISSILE)
         res |= THINGCLASS_MISSILE;
      else if(mo->flags3 & MF3_KILLABLE || mo->flags & MF_COUNTKILL)
         res |= THINGCLASS_MONSTER;
      else
         res |= THINGCLASS_GENERIC;

      if(mo->health > 0)
         res |= THINGCLASS_ALIVE;
      else
         res |= THINGCLASS_DEAD;
   }
   else
   {
      if(tid)
         res = THINGCLASS_NONE;
      else
         res = THINGCLASS_WORLD;
   }

   thread->dataStk.push(res);
   return false;
}

//
// ACS_CF_ClrLineSpec
//
// void ClearLineSpecial(void);
//
bool ACS_CF_ClrLineSpec(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;

   if(info->line)
      info->line->special = 0;

   return false;
}

//
// ACS_CF_Cos
//
// fixed Cos(fixed x);
//
bool ACS_CF_Cos(ACS_CF_ARGS)
{
   thread->dataStk.push(finecosine[(angle_t)(argV[0] << FRACBITS) >> ANGLETOFINESHIFT]);
   return false;
}

//
// ACS_CF_EndLog
//
// void EndLog(void);
//
bool ACS_CF_EndLog(ACS_CF_ARGS)
{
   printf("%s\n", thread->printBuf.data());
   doom_printf("%s", thread->printBuf.data());
   thread->printBuf.drop();

   return false;
}

//
// ACS_CF_EndPrint
//
// void EndPrint(void);
//
bool ACS_CF_EndPrint(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;

   if(info->mo && info->mo->player)
      player_printf(info->mo->player, "%s", thread->printBuf.data());
   else
      player_printf(&players[consoleplayer], "%s", thread->printBuf.data());
   thread->printBuf.drop();

   return false;
}

//
// ACS_CF_EndPrintBold
//
// void EndPrintBold(void);
//
bool ACS_CF_EndPrintBold(ACS_CF_ARGS)
{
   HU_CenterMsgTimedColor(thread->printBuf.data(), FC_GOLD, 20*35);
   thread->printBuf.drop();

   return false;
}

//
// ACS_CF_GameSkill
//
// int GameSkill(void);
//
bool ACS_CF_GameSkill(ACS_CF_ARGS)
{
   thread->dataStk.push(gameskill);
   return false;
}

//
// ACS_CF_GameType
//
// int GameType(void);
//
bool ACS_CF_GameType(ACS_CF_ARGS)
{
   thread->dataStk.push(GameType);
   return false;
}

//
// ACS_CF_GetCVar
//
// int GetCVar(str cvar);
//
bool ACS_CF_GetCVar(ACS_CF_ARGS)
{
   char const *name = thread->scopeMap->getString(argV[0])->str;

   command_t  *command;
   variable_t *var;

   if(!(command = C_GetCmdForName(name)) || !(var = command->variable))
   {
      thread->dataStk.push(0);
      return false;
   }

   switch(var->type)
   {
   case vt_int: thread->dataStk.push(*(int *)var->variable); break;
   case vt_float: thread->dataStk.push(M_DoubleToFixed(*(double *)var->variable)); break;
   case vt_string: thread->dataStk.push(0); break;
   case vt_chararray: thread->dataStk.push(0); break;
   case vt_toggle: thread->dataStk.push(*(bool *)var->variable); break;
   default: thread->dataStk.push(0); break;
   }

   return false;
}

//
// ACS_CF_GetCVarStr
//
// str GetCVarString(str cvar);
//
bool ACS_CF_GetCVarStr(ACS_CF_ARGS)
{
   char const *name = thread->scopeMap->getString(argV[0])->str;

   command_t  *command;
   variable_t *var;

   if(!(command = C_GetCmdForName(name)) || !(var = command->variable))
   {
      thread->dataStk.push(0);
      return false;
   }

   thread->dataStk.push(~ACSenv.getString(C_VariableValue(var))->idx);
   return false;
}

//
// int CheckInventory(str itemname);
//
bool ACS_CF_GetInventory(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;
   char const *itemname = thread->scopeMap->getString(argV[0])->str;
   itemeffect_t *item = E_ItemEffectForName(itemname);

   // We could use E_GetItemOwnedAmountName but let's inform the player if stuff's broke
   if(!item)
   {
      doom_printf("ACS_CF_GetInventory: Inventory item '%s' not found\a\n", itemname);
      thread->dataStk.push(0);
      return false;
   }

   if(!info->mo || !info->mo->player)
      thread->dataStk.push(0);
   else
      thread->dataStk.push(E_GetItemOwnedAmount(info->mo->player, item));
   return false;
}

//
// ACS_GetLevelProp
//
uint32_t ACS_GetLevelProp(uint32_t var)
{
   switch(var)
   {
   case ACS_LP_ParTime:        return LevelInfo.partime;
   case ACS_LP_ClusterNumber:  return 0;
   case ACS_LP_LevelNumber:    return gamemap;
   case ACS_LP_TotalSecrets:   return totalsecret;
   case ACS_LP_FoundSecrets:   return G_TotalFoundSecrets();
   case ACS_LP_TotalItems:     return totalitems;
   case ACS_LP_FoundItems:     return G_TotalFoundItems();
   case ACS_LP_TotalMonsters:  return totalkills;
   case ACS_LP_KilledMonsters: return G_TotalKilledMonsters();
   case ACS_LP_SuckTime:       return 1;

   default: return 0;
   }
}

//
// ACS_CF_GetLevelProp
//
// int GetLevelInfo(int prop);
//
bool ACS_CF_GetLevelProp(ACS_CF_ARGS)
{
   thread->dataStk.push(ACS_GetLevelProp(argV[0]));
   return false;
}

// GetPlayerInput inputs.
enum
{
   INPUT_OLDBUTTONS     =  0,
   INPUT_BUTTONS        =  1,
   INPUT_PITCH          =  2,
   INPUT_YAW            =  3,
   INPUT_ROLL           =  4,
   INPUT_FORWARDMOVE    =  5,
   INPUT_SIDEMOVE       =  6,
   INPUT_UPMOVE         =  7,
   MODINPUT_OLDBUTTONS  =  8,
   MODINPUT_BUTTONS     =  9,
   MODINPUT_PITCH       = 10,
   MODINPUT_YAW         = 11,
   MODINPUT_ROLL        = 12,
   MODINPUT_FORWARDMOVE = 13,
   MODINPUT_SIDEMOVE    = 14,
   MODINPUT_UPMOVE      = 15,
};

// INPUT_BUTTON buttons.
enum
{
   BUTTON_ATTACK     = 0x00000001,
   BUTTON_USE        = 0x00000002,
   BUTTON_JUMP       = 0x00000004,
   BUTTON_CROUCH     = 0x00000008,
   BUTTON_TURN180    = 0x00000010,
   BUTTON_ALTATTACK  = 0x00000020,
   BUTTON_RELOAD     = 0x00000040,
   BUTTON_ZOOM       = 0x00000080,
   BUTTON_SPEED      = 0x00000100,
   BUTTON_STRAFE     = 0x00000200,
   BUTTON_MOVERIGHT  = 0x00000400,
   BUTTON_MOVELEFT   = 0x00000800,
   BUTTON_BACK       = 0x00001000,
   BUTTON_FORWARD    = 0x00002000,
   BUTTON_RIGHT      = 0x00004000,
   BUTTON_LEFT       = 0x00008000,
   BUTTON_LOOKUP     = 0x00010000,
   BUTTON_LOOKDOWN   = 0x00020000,
   BUTTON_MOVEUP     = 0x00040000,
   BUTTON_MOVEDOWN   = 0x00080000,
   BUTTON_SHOWSCORES = 0x00100000,
   BUTTON_USER1      = 0x00200000,
   BUTTON_USER2      = 0x00400000,
   BUTTON_USER3      = 0x00800000,
   BUTTON_USER4      = 0x01000000,
};

//
// ACS_CF_GetLineX
//
// int GetLineX(lineid, lineratio, linedist);
//
// Returns the X coordinate of lineratio across the tagged line,
// and linedist away from it (perpendicular).
//
bool ACS_CF_GetLineX(ACS_CF_ARGS)
{
   int           lineid  = argV[0];
   int           lineratio = argV[1];
   int           linedist = argV[2];

   int           linenum = -1;
   const line_t *line    = P_FindLine(lineid, &linenum);

   if(!line)
   {
      thread->dataStk.push(0);
      return false;
   }
   int32_t result = line->v1->x + FixedMul(line->dx, lineratio);
   if(linedist)
   {
      angle_t angle = P_PointToAngle(line->v1->x, line->v1->y, line->v2->x, line->v2->y);
      angle -= ANG90;
      unsigned fineangle = angle >> ANGLETOFINESHIFT;
      result += FixedMul(finecosine[fineangle], linedist);
   }
   thread->dataStk.push(result);
   return false;
}

//
// ACS_CF_GetLineY
//
// int GetLineY(lineid, lineratio, linedist);
//
// Returns the Y coordinate of lineratio across the tagged line,
// and linedist away from it (perpendicular).
//
bool ACS_CF_GetLineY(ACS_CF_ARGS)
{
   int           lineid = argV[0];
   int           lineratio = argV[1];
   int           linedist = argV[2];

   int           linenum = -1;
   const line_t *line = P_FindLine(lineid, &linenum);

   if(!line)
   {
      thread->dataStk.push(0);
      return false;
   }
   int32_t result = line->v1->y + FixedMul(line->dy, lineratio);
   if(linedist)
   {
      angle_t angle = P_PointToAngle(line->v1->x, line->v1->y, line->v2->x, line->v2->y);
      angle -= ANG90;
      unsigned fineangle = angle >> ANGLETOFINESHIFT;
      result += FixedMul(finesine[fineangle], linedist);
   }
   thread->dataStk.push(result);
   return false;
}

//
// ACS_CF_GetPlayerInput
//
// int GetPlayerInput(int player, int input);
//
bool ACS_CF_GetPlayerInput(ACS_CF_ARGS)
{
   auto      info   = &static_cast<ACSThread *>(thread)->info;
   int32_t   pnum   = argV[0];
   int32_t   input  = argV[1];
   uint32_t  result = 0;
   player_t *player;

   if(pnum == -1 && info->mo)
      player = info->mo->player;
   else if(pnum >= 0 && pnum < MAXPLAYERS)
      player = &players[pnum];
   else
      player = nullptr;

   if(player) switch(input)
   {
   case MODINPUT_BUTTONS:
   case INPUT_BUTTONS:
      result = 0;

      if(player->cmd.buttons & BT_ATTACK)
         result |= BUTTON_ATTACK;

      if(player->cmd.buttons & BT_USE)
         result |= BUTTON_USE;

      if(player->cmd.actions & AC_JUMP)
         result |= BUTTON_JUMP;

      break;

   case MODINPUT_PITCH:
   case INPUT_PITCH:
      result = player->cmd.look;
      break;

   case MODINPUT_YAW:
   case INPUT_YAW:
      result = player->cmd.angleturn;
      break;

   case MODINPUT_FORWARDMOVE:
   case INPUT_FORWARDMOVE:
      result = player->cmd.forwardmove * 2048;
      break;

   case MODINPUT_SIDEMOVE:
   case INPUT_SIDEMOVE:
      result = player->cmd.sidemove * 2048;
      break;

   default:
      result = 0;
      break;
   }
   else
      result = 0;

   thread->dataStk.push(result);
   return false;
}

//
// ACS_CF_GetPolyobjX
//
// fixed GetPolyobjX(int po);
//
bool ACS_CF_GetPolyobjX(ACS_CF_ARGS)
{
   polyobj_t *po = Polyobj_GetForNum(argV[0]);

   if(po)
      thread->dataStk.push(po->centerPt.x);
   else
      thread->dataStk.push(0x7FFF0000);

   return false;
}

//
// ACS_CF_GetPolyobjY
//
// fixed GetPolyobjY(int po);
//
bool ACS_CF_GetPolyobjY(ACS_CF_ARGS)
{
   polyobj_t *po = Polyobj_GetForNum(argV[0]);

   if(po)
      thread->dataStk.push(po->centerPt.y);
   else
      thread->dataStk.push(0x7FFF0000);

   return false;
}

//
// ACS_CF_GetScreenH
//
// int GetScreenHeight(void);
//
bool ACS_CF_GetScreenH(ACS_CF_ARGS)
{
   thread->dataStk.push(video.height);
   return false;
}

//
// ACS_CF_GetScreenW
//
// int GetScreenWidth(void);
//
bool ACS_CF_GetScreenW(ACS_CF_ARGS)
{
   thread->dataStk.push(video.width);
   return false;
}

//
// ACS_CF_GetSectorCeilZ
//
// fixed GetSectorCeilingZ(int tag, int x, int y);
//
bool ACS_CF_GetSectorCeilZ(ACS_CF_ARGS)
{
   int secnum = P_FindSectorFromTag(argV[0], -1);

   if(secnum >= 0)
   {
      // TODO/FIXME: sloped sectors
      thread->dataStk.push(sectors[secnum].ceilingheight);
   }
   else
      thread->dataStk.push(0);

   return false;
}

//
// ACS_CF_GetSectorFloorZ
//
// fixed GetSectorFloorZ(int tag, int x, int y);
//
bool ACS_CF_GetSectorFloorZ(ACS_CF_ARGS)
{
   int secnum = P_FindSectorFromTag(argV[0], -1);

   if(secnum >= 0)
   {
      // TODO/FIXME: sloped sectors
      thread->dataStk.push(sectors[secnum].floorheight);
   }
   else
      thread->dataStk.push(0);

   return false;
}

//
// ACS_CF_GetSectorLight
//
// int GetSectorLightLevel(int tag);
//
bool ACS_CF_GetSectorLight(ACS_CF_ARGS)
{
   int secnum = P_FindSectorFromTag(argV[0], -1);
   thread->dataStk.push(secnum >= 0 ? sectors[secnum].lightlevel : 0);
   return false;
}

//
// ACS_CF_GetThingAngle
//
// fixed GetThingAngle(int tid);
//
bool ACS_CF_GetThingAngle(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Angle);
}

//
// ACS_CF_GetThingCeilZ
//
// fixed GetThingCeilZ(int tid);
//
bool ACS_CF_GetThingCeilZ(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_CeilZ);
}

//
// ACS_CF_GetThingFloorZ
//
// fixed GetThingFloorZ(int tid);
//
bool ACS_CF_GetThingFloorZ(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_FloorZ);
}

//
// ACS_CF_GetThingLight
//
// fixed GetThingLightLevel(int tid);
//
bool ACS_CF_GetThingLight(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_LightLevel);
}

//
// ACS_CF_GetThingMomX
//
// fixed GetThingMomX(int tid);
//
bool ACS_CF_GetThingMomX(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_MomX);
}

//
// ACS_CF_GetThingMomY
//
// fixed GetThingMomY(int tid);
//
bool ACS_CF_GetThingMomY(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_MomY);
}

//
// ACS_CF_GetThingMomZ
//
// fixed GetThingMomZ(int tid);
//
bool ACS_CF_GetThingMomZ(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_MomZ);
}

//
// ACS_CF_GetThingPitch
//
// fixed GetThingPitch(int tid);
//
bool ACS_CF_GetThingPitch(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Pitch);
}

//
// ACS_GetThingProp
//
uint32_t ACS_GetThingProp(Mobj *mo, uint32_t prop)
{
   switch(prop)
   {
   case ACS_TP_Health:       return mo->health;
   case ACS_TP_Speed:        return mo->info->speed;
   case ACS_TP_Damage:       return mo->damage;
   case ACS_TP_Alpha:        return mo->translucency;
   case ACS_TP_RenderStyle:  return 0;
   case ACS_TP_SeeSound:     return 0;
   case ACS_TP_AttackSound:  return 0;
   case ACS_TP_PainSound:    return 0;
   case ACS_TP_DeathSound:   return 0;
   case ACS_TP_ActiveSound:  return 0;
   case ACS_TP_Ambush:       return !!(mo->flags & MF_AMBUSH);
   case ACS_TP_Invulnerable: return !!(mo->flags2 & MF2_INVULNERABLE);
   case ACS_TP_JumpZ:        return 0;
   case ACS_TP_ChaseGoal:    return 0;
   case ACS_TP_Frightened:   return 0;
   case ACS_TP_Friendly:     return !!(mo->flags & MF_FRIEND);
   case ACS_TP_SpawnHealth:  return mo->getModifiedSpawnHealth();
   case ACS_TP_Dropped:      return !!(mo->flags & MF_DROPPED);
   case ACS_TP_NoTarget:     return 0;
   case ACS_TP_Species:      return 0;
   case ACS_TP_NameTag:      return 0;
   case ACS_TP_Score:        return 0;
   case ACS_TP_NoTrigger:    return 0;
   case ACS_TP_DamageFactor: return 0;
   case ACS_TP_MasterTID:    return 0;
   case ACS_TP_TargetTID:    return mo->target ? mo->target->tid : 0;
   case ACS_TP_TracerTID:    return mo->tracer ? mo->tracer->tid : 0;
   case ACS_TP_WaterLevel:   return 0;
   case ACS_TP_ScaleX:       return M_FloatToFixed(mo->xscale);
   case ACS_TP_ScaleY:       return M_FloatToFixed(mo->yscale);
   case ACS_TP_Dormant:      return !!(mo->flags2 & MF2_DORMANT);
   case ACS_TP_Mass:         return mo->info->mass;
   case ACS_TP_Accuracy:     return 0;
   case ACS_TP_Stamina:      return 0;
   case ACS_TP_Height:       return mo->height;
   case ACS_TP_Radius:       return mo->radius;
   case ACS_TP_ReactionTime: return mo->reactiontime;
   case ACS_TP_MeleeRange:   return MELEERANGE;
   case ACS_TP_ViewHeight:   return 0;
   case ACS_TP_AttackZOff:   return 0;
   case ACS_TP_StencilColor: return 0;
   case ACS_TP_Friction:     return 0;
   case ACS_TP_DamageMult:   return 0;
   case ACS_TP_Counter0:     return mo->counters[0];
   case ACS_TP_Counter1:     return mo->counters[1];
   case ACS_TP_Counter2:     return mo->counters[2];
   case ACS_TP_Counter3:     return mo->counters[3];
   case ACS_TP_Counter4:     return mo->counters[4];
   case ACS_TP_Counter5:     return mo->counters[5];
   case ACS_TP_Counter6:     return mo->counters[6];
   case ACS_TP_Counter7:     return mo->counters[7];

   case ACS_TP_Angle:        return mo->angle >> 16;
   case ACS_TP_Armor:        return mo->player ? mo->player->armorpoints : 0;
   case ACS_TP_CeilTex:      return 0;
   case ACS_TP_CeilZ:        return mo->ceilingz;
   case ACS_TP_FloorTex:     return 0;
   case ACS_TP_FloorZ:       return mo->floorz;
   case ACS_TP_Frags:        return mo->player ? mo->player->totalfrags : 0;
   case ACS_TP_LightLevel:   return mo->subsector->sector->lightlevel;
   case ACS_TP_MomX:         return mo->momx;
   case ACS_TP_MomY:         return mo->momy;
   case ACS_TP_MomZ:         return mo->momz;
   case ACS_TP_Pitch:        return mo->player ? mo->player->pitch >> 16 : 0;
   case ACS_TP_PlayerNumber: return mo->player ? eindex(mo->player - players) : -1;
   case ACS_TP_SigilPieces:  return 0;
   case ACS_TP_TID:          return mo->tid;
   case ACS_TP_Type:         return ~ACSenv.getString(mo->info->name)->idx;
   case ACS_TP_X:            return mo->x;
   case ACS_TP_Y:            return mo->y;
   case ACS_TP_Z:            return mo->z;

   default: return 0;
   }
}

//
// ACS_CF_GetThingProp
//
// int GetThingProperty(int tid, int prop);
//
bool ACS_CF_GetThingProp(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], argV[1]);
}

//
// ACS_CF_GetThingX
//
// fixed GetThingX(int tid);
//
bool ACS_CF_GetThingX(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_X);
}

//
// ACS_CF_GetThingY
//
// fixed GetThingY(int tid);
//
bool ACS_CF_GetThingY(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Y);
}

//
// ACS_CF_GetThingZ
//
// fixed GetThingZ(int tid);
//
bool ACS_CF_GetThingZ(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Z);
}

//
// ACS_CF_Hypot
//
// fixed Hypot(fixed x, fixed y);
//
bool ACS_CF_Hypot(ACS_CF_ARGS)
{
   double x = M_FixedToDouble(argV[0]);
   double y = M_FixedToDouble(argV[1]);
   thread->dataStk.push(M_DoubleToFixed(hypot(x, y)));
   return false;
}

//
// ACS_CF_IsTIDUsed
//
// int IsTIDUsed(int tid);
//
bool ACS_CF_IsTIDUsed(ACS_CF_ARGS)
{
   thread->dataStk.push(!!P_FindMobjFromTID(argV[0], nullptr, nullptr));
   return false;
}

//
// ACS_CF_LineOffsetY
//
// int GetLineRowOffset(void);
//
bool ACS_CF_LineOffsetY(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;
   thread->dataStk.push(info->line ? sides[info->line->sidenum[0]].rowoffset >> FRACBITS : 0);
   return false;
}

//
// ACS_CF_LineSide
//
// int LineSide(void);
//
bool ACS_CF_LineSide(ACS_CF_ARGS)
{
   thread->dataStk.push(static_cast<ACSThread *>(thread)->info.side);
   return false;
}

//
// ACS_playSound
//
static bool ACS_playSound(ACS_CF_ARGS, Mobj *mo, sfxinfo_t *sfx)
{
   thread->dataStk.push(0);

   if(!mo || !sfx)
      return false;

   int     chan = argC > 2 ? argV[2] & 7 : CHAN_BODY;
   int     vol  = argC > 3 ? argV[3] >> 9 : 127;
   bool    loop = argC > 4 ? !!(argV[4] & 1) : false;
   fixed_t attn = argC > 5 ? argV[5] : 1 << FRACBITS;

   // We don't have arbitrary attentuation, so try to map to a supported enum.
   int attnEnum;
   if(attn <= 0)
      attnEnum = ATTN_NONE;
   else if(attn <= (1 << FRACBITS))
      attnEnum = ATTN_NORMAL;
   else if(attn <= (2 << FRACBITS))
      attnEnum = ATTN_IDLE;
   else
      attnEnum = ATTN_STATIC;

   soundparams_t param;
   param.setNormalDefaults(mo);
   param.sfx         = sfx;
   param.volumeScale = vol;
   param.attenuation = attnEnum;
   param.loop        = loop;
   param.subchannel  = chan;

   S_StartSfxInfo(param);

   return false;
}

//
// ACS_CF_PlaySound
//
// int PlaySound(int tid, str sound, int channel = CHAN_BODY, fixed volume = 1,
//               int looping = 0, fixed attenuation = 1);
//
bool ACS_CF_PlaySound(ACS_CF_ARGS)
{
   auto       info = &static_cast<ACSThread *>(thread)->info;
   Mobj      *mo   = P_FindMobjFromTID(argV[0], nullptr, info->mo);
   sfxinfo_t *sfx  = S_SfxInfoForName(thread->scopeMap->getString(argV[1])->str);

   return ACS_playSound(thread, argV, argC, mo, sfx);
}

// PlayThingSound sound indexes.
enum
{
   SOUND_See        = 0,
   SOUND_Attack     = 1,
   SOUND_Pain       = 2,
   SOUND_Death      = 3,
   SOUND_Active     = 4,
   SOUND_Use        = 5,
   SOUND_Bounce     = 6,
   SOUND_WallBounce = 7,
   SOUND_CrushPain  = 8,
   SOUND_Howl       = 9,
};

//
// ACS_CF_PlayThingSound
//
// int PlayThingSound(int tid, int sound, int channel = CHAN_BODY,
//                    fixed volume = 1, bool looping = 0, fixed attenuation = 1);
//
bool ACS_CF_PlayThingSound(ACS_CF_ARGS)
{
   auto       info = &static_cast<ACSThread *>(thread)->info;
   Mobj      *mo   = P_FindMobjFromTID(argV[0], nullptr, info->mo);
   int        snd  = argV[1];
   sfxinfo_t *sfx;

   switch(snd)
   {
   case SOUND_See:    sfx = E_SoundForDEHNum(mo->info->seesound);    break;
   case SOUND_Attack: sfx = E_SoundForDEHNum(mo->info->attacksound); break;
   case SOUND_Pain:   sfx = E_SoundForDEHNum(mo->info->painsound);   break;
   case SOUND_Death:  sfx = E_SoundForDEHNum(mo->info->deathsound);  break;
   case SOUND_Active: sfx = E_SoundForDEHNum(mo->info->activesound); break;
   default: sfx = nullptr; break;
   }

   return ACS_playSound(thread, argV, argC, mo, sfx);
}

//
// ACS_CF_PlayerCount
//
// int PlayerCount(void);
//
bool ACS_CF_PlayerCount(ACS_CF_ARGS)
{
   int count = 0;

   for(int i = 0; i < MAXPLAYERS; ++i)
      if(playeringame[i])
         ++count;

   thread->dataStk.push(count);

   return false;
}

//
// ACS_CF_PlayerNumber
//
// int PlayerNumber(void);
//
bool ACS_CF_PlayerNumber(ACS_CF_ARGS)
{
   return ACS_GetThingProp(static_cast<ACSThread *>(thread), 0, ACS_TP_PlayerNumber);
}

//
// ACS_CF_PrintName
//
// void PrintName(int name);
//
bool ACS_CF_PrintName(ACS_CF_ARGS)
{
   ACSVM::SWord name = argV[0];

   switch(name)
   {
   case 0:
      thread->printBuf.reserve(strlen(players[consoleplayer].name));
      thread->printBuf.put(players[consoleplayer].name);
      break;

   default:
      if(name > 0 && name <= MAXPLAYERS)
      {
         thread->printBuf.reserve(strlen(players[consoleplayer].name));
         thread->printBuf.put(players[consoleplayer].name);
      }
      break;
   }

   return false;
}

//
// ACS_CF_RadiusQuake
//
// int Radius_Quake2(int tid, int intensity, int duration, int damrad, int tremrad, str sound);
//
bool ACS_CF_RadiusQuake(ACS_CF_ARGS)
{
   auto        info         = &static_cast<ACSThread *>(thread)->info;
   int32_t     tid          = argV[0];
   int32_t     intensity    = argV[1];
   int32_t     duration     = argV[2];
   int32_t     damageRadius = argV[3];
   int32_t     quakeRadius  = argV[4];
   const char *snd          = thread->scopeMap->getString(argV[5])->str;
   Mobj       *mo           = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, info->mo)))
   {
      QuakeThinker *qt;

      qt = new QuakeThinker;
      qt->addThinker();

      qt->intensity    = intensity;
      qt->duration     = duration;
      qt->damageRadius = damageRadius << FRACBITS;
      qt->quakeRadius  = quakeRadius << FRACBITS;
      qt->soundName    = snd;

      qt->x       = mo->x;
      qt->y       = mo->y;
      qt->z       = mo->z;
      qt->groupid = mo->groupid;
   }

   thread->dataStk.push(0);
   return false;
}

//
// ACS_CF_Random
//
// int Random(int min, int max);
//
bool ACS_CF_Random(ACS_CF_ARGS)
{
   thread->dataStk.push(P_RangeRandomEx(pr_script, argV[0], argV[1]));
   return false;
}

// ReplaceTextures flags
enum
{
   RETEX_NOT_BOTTOM = 0x01,
   RETEX_NOT_MID    = 0x02,
   RETEX_NOT_TOP    = 0x04,
   RETEX_NOT_FLOOR  = 0x08,
   RETEX_NOT_CEIL   = 0x10,

   RETEX_NOT_LINE   = RETEX_NOT_BOTTOM|RETEX_NOT_MID|RETEX_NOT_TOP,
   RETEX_NOT_SECTOR = RETEX_NOT_FLOOR|RETEX_NOT_CEIL,
};

//
// ACS_CF_ReplaceTex
//
// void ReplaceTextures(str oldtex, str newtex, int flags);
//
bool ACS_CF_ReplaceTex(ACS_CF_ARGS)
{
   int      oldtex = R_FindWall(thread->scopeMap->getString(argV[0])->str);
   int      newtex = R_FindWall(thread->scopeMap->getString(argV[1])->str);
   uint32_t flags  = argV[2];

   // If doing anything to lines.
   if((flags & RETEX_NOT_LINE) != RETEX_NOT_LINE)
   {
      for(side_t *side = sides, *end = side + numsides; side != end; ++side)
      {
         if(!(flags & RETEX_NOT_BOTTOM) && side->bottomtexture == oldtex)
            side->bottomtexture = newtex;

         if(!(flags & RETEX_NOT_MID) && side->midtexture == oldtex)
            side->midtexture = newtex;

         if(!(flags & RETEX_NOT_TOP) && side->toptexture == oldtex)
            side->toptexture = newtex;
      }
   }

   // If doing anything to sectors.
   if((flags & RETEX_NOT_SECTOR) != RETEX_NOT_SECTOR)
   {
      for(sector_t *sector = sectors, *end = sector + numsectors; sector != end; ++sector)
      {
         if(!(flags & RETEX_NOT_FLOOR) && sector->floorpic == oldtex)
            sector->floorpic = newtex;

         if(!(flags & RETEX_NOT_CEIL) && sector->ceilingpic == oldtex)
            sector->ceilingpic = newtex;
      }
   }

   return false;
}

// sector damage flags
enum
{
   SECDAM_PLAYERS          = 0x01,
   SECDAM_NONPLAYERS       = 0x02,
   SECDAM_IN_AIR           = 0x04,
   SECDAM_SUBCLASS_PROTECT = 0x08,
};

//
// ACS_CF_SectorDamage
//
// void SectorDamage(int tag, int amount, str type, str protection, int flags);
//
bool ACS_CF_SectorDamage(ACS_CF_ARGS)
{
   int32_t   tag    = argV[0];
   int32_t   damage = argV[1];
   int       mod    = E_DamageTypeNumForName(thread->scopeMap->getString(argV[2])->str);
   uint32_t  flags  = argV[4];
   int       secnum = -1;
   sector_t *sector;

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sector = &sectors[secnum];

      for(Mobj *mo = sector->thinglist; mo; mo = mo->snext)
      {
         if(mo->player && !(flags & SECDAM_PLAYERS))
            continue;

         if(!mo->player && !(flags & SECDAM_NONPLAYERS))
            continue;

         if(mo->z != mo->floorz && !(flags & SECDAM_IN_AIR))
            continue;

         P_DamageMobj(mo, NULL, NULL, damage, mod);
      }
   }

   return false;
}

//
// ACS_CF_SectorSound
//
// void SectorSound(str sound, int volume);
//
bool ACS_CF_SectorSound(ACS_CF_ARGS)
{
   auto         *info = &static_cast<ACSThread *>(thread)->info;
   const char   *snd = thread->scopeMap->getString(argV[0])->str;
   int           vol = argV[1];
   PointThinker *src;

   // if script started from a line, use the frontsector's sound origin
   if(info->line)
      src = &(info->line->frontsector->soundorg);
   else
      src = NULL;

   S_StartSoundNameAtVolume(src, snd, vol, ATTN_NORMAL, CHAN_AUTO);

   return false;
}

//
// ACS_CF_SetActivator
//
// int SetActivator(int tid);
//
bool ACS_CF_SetActivator(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;
   P_SetTarget(&info->mo, P_FindMobjFromTID(argV[0], nullptr, nullptr));

   thread->dataStk.push(!!info->mo);
   return false;
}

//
// ACS_CF_SetActivatorToTarget
//
// int SetActivatorToTarget(int tid);
//
bool ACS_CF_SetActivatorToTarget(ACS_CF_ARGS)
{
   auto  info = &static_cast<ACSThread *>(thread)->info;
   Mobj *mo   = P_FindMobjFromTID(argV[0], nullptr, info->mo);

   if(mo)
   {
      if(mo->target)
         P_SetTarget(&info->mo, mo->target);
      else
         P_SetTarget(&info->mo, mo);

      thread->dataStk.push(1);
   }
   else
      thread->dataStk.push(0);

   return false;
}

//
// void SetAirControl(fixed amount);
//
bool ACS_CF_SetAirControl(ACS_CF_ARGS)
{
   LevelInfo.airControl = argV[0];
   return false;
}

//
// void SetAirFriction(fixed amount);
//
bool ACS_CF_SetAirFriction(ACS_CF_ARGS)
{
   thread->dataStk.push(0);
   LevelInfo.airFriction = argV[0];
   return false;
}

//
// ACS_CF_SetGravity
//
// void SetGravity(fixed gravity);
//
// This function uses Quake-style gravity values where 800 is normal.
//
bool ACS_CF_SetGravity(ACS_CF_ARGS)
{
   LevelInfo.gravity = argV[0] / 800;
   return false;
}

// ZDoom blocking types
enum
{
   BLOCK_NOTHING,
   BLOCK_CREATURES,
   BLOCK_EVERYTHING,
   BLOCK_RAILING,
   BLOCK_PLAYERS,
   BLOCK_MONSTERS_OFF,
   BLOCK_MONSTERS_ON,
};

//
// ACS_setLineBlocking
//
// Toggles the blocking flag on all tagged lines.
//
static void ACS_setLineBlocking(int tag, int block)
{
   line_t *l;
   int linenum = -1;

   while((l = P_FindLine(tag, &linenum)) != NULL)
   {
      switch(block)
      {
      case BLOCK_NOTHING:
         // clear the flags
         l->flags    &= ~ML_BLOCKING;
         l->extflags &= ~EX_ML_BLOCKALL;
         break;
      case BLOCK_CREATURES:
         l->extflags &= ~EX_ML_BLOCKALL;
         l->flags |= ML_BLOCKING;
         break;
      case BLOCK_EVERYTHING: // ZDoom extension - block everything
         l->flags    |= ML_BLOCKING;
         l->extflags |= EX_ML_BLOCKALL;
         break;
      case BLOCK_MONSTERS_OFF:
         l->flags &= ~ML_BLOCKMONSTERS;
         break;
      case BLOCK_MONSTERS_ON:
         l->flags |= ML_BLOCKMONSTERS;
         break;
      default: // Others not implemented yet :P
         break;
      }
   }
}

//
// ACS_CF_SetLineBlock
//
// void SetLineBlocking(int tag, int block);
//
bool ACS_CF_SetLineBlock(ACS_CF_ARGS)
{
   ACS_setLineBlocking(argV[0], argV[1]);
   return false;
}

//
// ACS_CF_SetLineBlockMon
//
// void SetLineBlockMon(int tag, int block);
//
bool ACS_CF_SetLineBlockMon(ACS_CF_ARGS)
{
   ACS_setLineBlocking(argV[0], argV[1] ? BLOCK_MONSTERS_ON : BLOCK_MONSTERS_OFF);
   return false;
}

//
// ACS_CF_SetLineSpec
//
// void SetLineSpecial(int tag, int spec, int arg0, int arg1, int arg2, int arg3, int arg4);
//
bool ACS_CF_SetLineSpec(ACS_CF_ARGS)
{
   int     tag  = argV[0];
   int     spec = argV[1];
   line_t *l;
   int     linenum = -1;

   while((l = P_FindLine(tag, &linenum)) != NULL)
   {
      l->special = EV_ActionForACSAction(spec);
      for(int i = NUMLINEARGS; i--;)
         l->args[i] = argV[i + 2];
   }

   return false;
}

//
// ACS_CF_SetLineTex
//
// void SetLineTexture(int tag, int side, int pos, str texture);
//
bool ACS_CF_SetLineTex(ACS_CF_ARGS)
{
   P_ChangeLineTex(thread->scopeMap->getString(argV[3])->str, argV[2], argV[1], argV[0], true);
   return false;
}

//
// ACS_CF_SetMusic
//
// void SetMusic(str song, int order = 0);
//
bool ACS_CF_SetMusic(ACS_CF_ARGS)
{
   S_ChangeMusicName(thread->scopeMap->getString(argV[0])->str, 1);
   return false;
}

//
// ACS_CF_SetMusicLoc
//
// void LocalSetMusic(str song, int order = 0);
//
bool ACS_CF_SetMusicLoc(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;
   if(info->mo == players[consoleplayer].mo)
      S_ChangeMusicName(thread->scopeMap->getString(argV[0])->str, 1);
   return false;
}

//
// ACS_CF_SetSkyDelta
//
// int SetSkyScrollSpeed(int sky, fixed speed);
//
bool ACS_CF_SetSkyDelta(ACS_CF_ARGS)
{
   switch(argV[0])
   {
   case 1: LevelInfo.skyDelta  = argV[1] >> FRACBITS; break;
   case 2: LevelInfo.sky2Delta = argV[1] >> FRACBITS; break;
   }

   thread->dataStk.push(0);
   return false;
}

//
// ACS_CF_SetThingAngle
//
// void SetThingAngle(int tid, fixed angle, int interpolate = 0);
//
bool ACS_CF_SetThingAngle(ACS_CF_ARGS)
{
   return ACS_SetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Angle, argV[1]);
}

//
// ACS_CF_SetThingAngleRet
//
// int SetThingAngle(int tid, fixed angle, int interpolate = 0);
//
bool ACS_CF_SetThingAngleRet(ACS_CF_ARGS)
{
   thread->dataStk.push(0);
   return ACS_CF_SetThingAngle(thread, argV, argC);
}

//
// ACS_CF_SetThingMom
//
// int SetThingMomentum(int tid, fixed momx, fixed momy, fixed momz, int add);
//
bool ACS_CF_SetThingMom(ACS_CF_ARGS)
{
   auto    info = &static_cast<ACSThread *>(thread)->info;
   int32_t tid  = argV[0];
   fixed_t momx = argV[1];
   fixed_t momy = argV[2];
   fixed_t momz = argV[3];
   bool    add  = argV[4] ? true : false;
   Mobj   *mo   = nullptr;

   while((mo = P_FindMobjFromTID(tid, mo, info->mo)))
   {
      if(add)
      {
         mo->momx += momx;
         mo->momy += momy;
         mo->momz += momz;
      }
      else
      {
         mo->momx = momx;
         mo->momy = momy;
         mo->momz = momz;
      }
   }

   thread->dataStk.push(0);
   return false;
}

//
// ACS_CF_SetThingPitch
//
// void SetThingPitch(int tid, fixed pitch, int interpolate = 0);
//
bool ACS_CF_SetThingPitch(ACS_CF_ARGS)
{
   return ACS_SetThingProp(static_cast<ACSThread *>(thread), argV[0], ACS_TP_Pitch, argV[1]);
}

//
// ACS_CF_SetThingPitchRet
//
// int SetThingPitch(int tid, fixed pitch, int interpolate = 0);
//
bool ACS_CF_SetThingPitchRet(ACS_CF_ARGS)
{
   thread->dataStk.push(0);
   return ACS_CF_SetThingPitch(thread, argV, argC);
}

//
// ACS_CF_SetThingPos
//
// int SetThingPosition(int tid, fixed x, fixed y, fixed z, int fog);
//
bool ACS_CF_SetThingPos(ACS_CF_ARGS)
{
   auto     info = &static_cast<ACSThread *>(thread)->info;
   int32_t  tid  = argV[0];
   fixed_t  x    = argV[1];
   fixed_t  y    = argV[2];
   fixed_t  z    = argV[3];
   bool     fog  = argV[4] ? true : false;
   uint32_t res  = 0;
   Mobj    *mo;

   if((mo = P_FindMobjFromTID(tid, nullptr, info->mo)))
   {
      fixed_t oldx = mo->x;
      fixed_t oldy = mo->y;
      fixed_t oldz = mo->z;

      mo->z = z;

      if(P_CheckPositionExt(mo, x, y, z))
      {
         subsector_t *newsubsec;

         newsubsec = R_PointInSubsector(x, y);

         // Set new position.
         P_UnsetThingPosition(mo);

         mo->floorz = mo->dropoffz = newsubsec->sector->floorheight;
         mo->ceilingz = newsubsec->sector->ceilingheight;
         mo->passfloorz = mo->secfloorz = mo->floorz;
         mo->passceilz = mo->secceilz = mo->ceilingz;

         mo->x = x;
         mo->y = y;

         mo->backupPosition();
         P_SetThingPosition(mo);


         // Handle fog.
         if(fog)
         {
            // Teleport fog at source...
            Mobj *fogmo = P_SpawnMobj(oldx, oldy, oldz + GameModeInfo->teleFogHeight,
                                E_SafeThingName(GameModeInfo->teleFogType));
            S_StartSound(fogmo, GameModeInfo->teleSound);

            // ... and destination.
            fogmo = P_SpawnMobj(x, y, z + GameModeInfo->teleFogHeight,
                                E_SafeThingName(GameModeInfo->teleFogType));
            S_StartSound(fogmo, GameModeInfo->teleSound);
         }

         ++res;
      }
      else
      {
         mo->z = oldz;
      }
   }

   thread->dataStk.push(res);
   return false;
}

//
// ACS_SetThingProp
//
void ACS_SetThingProp(Mobj *thing, uint32_t var, uint32_t val)
{
   if(!thing) return;

   switch(var)
   {
   case ACS_TP_Health:       thing->health = val; break;
   case ACS_TP_Speed:        break;
   case ACS_TP_Damage:       thing->damage = val; break;
   case ACS_TP_Alpha:        thing->translucency = val; break;
   case ACS_TP_RenderStyle:  break;
   case ACS_TP_SeeSound:     break;
   case ACS_TP_AttackSound:  break;
   case ACS_TP_PainSound:    break;
   case ACS_TP_DeathSound:   break;
   case ACS_TP_ActiveSound:  break;
   case ACS_TP_Ambush:       if(val) thing->flags |=  MF_AMBUSH;
                             else    thing->flags &= ~MF_AMBUSH; break;
   case ACS_TP_Invulnerable: if(val) thing->flags2 |=  MF2_INVULNERABLE;
                             else    thing->flags2 &= ~MF2_INVULNERABLE; break;
   case ACS_TP_JumpZ:        break;
   case ACS_TP_ChaseGoal:    break;
   case ACS_TP_Frightened:   break;
   case ACS_TP_Friendly:     if(val) thing->flags |=  MF_FRIEND;
                             else    thing->flags &= ~MF_FRIEND; break;
   case ACS_TP_SpawnHealth:  break;
   case ACS_TP_Dropped:      if(val) thing->flags |=  MF_DROPPED;
                             else    thing->flags &= ~MF_DROPPED; break;
   case ACS_TP_NoTarget:     break;
   case ACS_TP_Species:      break;
   case ACS_TP_NameTag:      break;
   case ACS_TP_Score:        break;
   case ACS_TP_NoTrigger:    break;
   case ACS_TP_DamageFactor: break;
   case ACS_TP_MasterTID:    break;
   case ACS_TP_TargetTID:    P_SetTarget(&thing->target, P_FindMobjFromTID(val, 0, 0)); break;
   case ACS_TP_TracerTID:    P_SetTarget(&thing->tracer, P_FindMobjFromTID(val, 0, 0)); break;
   case ACS_TP_WaterLevel:   break;
   case ACS_TP_ScaleX:       thing->xscale = M_FixedToFloat(val); break;
   case ACS_TP_ScaleY:       thing->yscale = M_FixedToFloat(val); break;
   case ACS_TP_Dormant:      if(val) thing->flags2 |=  MF2_DORMANT;
                             else    thing->flags2 &= ~MF2_DORMANT; break;
   case ACS_TP_Mass:         break;
   case ACS_TP_Accuracy:     break;
   case ACS_TP_Stamina:      break;
   case ACS_TP_Height:       break;
   case ACS_TP_Radius:       break;
   case ACS_TP_ReactionTime: break;
   case ACS_TP_MeleeRange:   break;
   case ACS_TP_ViewHeight:   break;
   case ACS_TP_AttackZOff:   break;
   case ACS_TP_StencilColor: break;
   case ACS_TP_Friction:     break;
   case ACS_TP_DamageMult:   break;
   case ACS_TP_Counter0:     thing->counters[0] = val; break;
   case ACS_TP_Counter1:     thing->counters[1] = val; break;
   case ACS_TP_Counter2:     thing->counters[2] = val; break;
   case ACS_TP_Counter3:     thing->counters[3] = val; break;
   case ACS_TP_Counter4:     thing->counters[4] = val; break;
   case ACS_TP_Counter5:     thing->counters[5] = val; break;
   case ACS_TP_Counter6:     thing->counters[6] = val; break;
   case ACS_TP_Counter7:     thing->counters[7] = val; break;

   case ACS_TP_Angle:        thing->angle = val << 16; break;
   case ACS_TP_Armor:        break;
   case ACS_TP_CeilTex:      break;
   case ACS_TP_CeilZ:        break;
   case ACS_TP_FloorTex:     break;
   case ACS_TP_FloorZ:       break;
   case ACS_TP_Frags:        break;
   case ACS_TP_LightLevel:   break;
   case ACS_TP_MomX:         thing->momx = val; break;
   case ACS_TP_MomY:         thing->momy = val; break;
   case ACS_TP_MomZ:         thing->momz = val; break;
   case ACS_TP_Pitch:        if(thing->player) thing->player->pitch = val << 16; break;
   case ACS_TP_PlayerNumber: break;
   case ACS_TP_SigilPieces:  break;
   case ACS_TP_TID:          P_RemoveThingTID(thing); P_AddThingTID(thing, val); break;
   case ACS_TP_Type:         break;
   case ACS_TP_X:            thing->x = val; break;
   case ACS_TP_Y:            thing->y = val; break;
   case ACS_TP_Z:            thing->z = val; break;
   }
}

//
// ACS_CF_SetThingProp
//
// void SetThingProp(int tid, int prop, int value);
//
bool ACS_CF_SetThingProp(ACS_CF_ARGS)
{
   return ACS_SetThingProp(static_cast<ACSThread *>(thread), argV[0], argV[1], argV[2]);
}

//
// ACS_CF_SetThingSpec
//
// void SetThingSpecial(int tid, int spec, int arg0, int arg1, int arg2, int arg3, int arg4);
//
bool ACS_CF_SetThingSpec(ACS_CF_ARGS)
{
   auto  info = &static_cast<ACSThread *>(thread)->info;
   int   tid  = argV[0];
   //int spec = argV[1]; // HEXEN_TODO
   Mobj *mo   = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, info->mo)))
   {
      //mo->special = spec; // HEXEN_TODO
      for(int i = 0; i != 5; ++i)
         mo->args[i] = argV[i + 2];
   }

   return false;
}

//
// ACS_CF_SetThingState
//
// int SetThingState(int tid, str state, int exact);
//
bool ACS_CF_SetThingState(ACS_CF_ARGS)
{
   auto  info = &static_cast<ACSThread *>(thread)->info;
   int32_t     tid       = argV[0];
   const char *statename = thread->scopeMap->getString(argV[1])->str;
   statenum_t  statenum  = E_StateNumForName(statename);
   state_t    *state;
   uint32_t    count     = 0;
   Mobj       *mo        = nullptr;

   while((mo = P_FindMobjFromTID(tid, mo, info->mo)))
   {
      // Look for the named state for that type.
      if((state = E_GetJumpInfo(mo->info, statename)))
      {
         P_SetMobjState(mo, state->index);
         ++count;
      }
      // Otherwise, fall back to the global name.
      else if(statenum >= 0)
      {
         P_SetMobjState(mo, statenum);
         ++count;
      }
   }

   thread->dataStk.push(count);
   return false;
}

//
// ACS_CF_Sin
//
// fixed Sin(fixed x);
//
bool ACS_CF_Sin(ACS_CF_ARGS)
{
   thread->dataStk.push(finesine[(angle_t)(argV[0] << FRACBITS) >> ANGLETOFINESHIFT]);
   return false;
}

//
// ACS_CF_SinglePlayer
//
// int SinglePlayer();
//
bool ACS_CF_SinglePlayer(ACS_CF_ARGS)
{
   thread->dataStk.push(GameType == gt_single);
   return false;
}

//
// ACS_CF_SoundSeq
//
// void SoundSequence(str sndseq);
//
bool ACS_CF_SoundSeq(ACS_CF_ARGS)
{
   auto        info = &static_cast<ACSThread *>(thread)->info;
   const char *snd  = thread->scopeMap->getString(argV[0])->str;
   sector_t   *sec;

   if(info->line && (sec = info->line->frontsector))
      S_StartSectorSequenceName(sec, snd, SEQ_ORIGIN_SECTOR_F);
   else
      S_StartSequenceName(NULL, snd, SEQ_ORIGIN_OTHER, -1);

   return false;
}

//
// ACS_spawn
//
static Mobj *ACS_spawn(mobjtype_t type, fixed_t x, fixed_t y, fixed_t z,
                       int tid, angle_t angle, bool forced)
{
   Mobj *mo;
   if(type != -1 && (mo = P_SpawnMobj(x, y, z, type)))
   {
      // If not forcing spawn, check that the position is OK.
      if(!forced && !P_CheckPositionExt(mo, mo->x, mo->y, mo->z))
      {
         // And if not, unmake the Mobj.
         mo->state = NULL;
         mo->remove();
         return NULL;
      }

      if(tid) P_AddThingTID(mo, tid);
      mo->angle = angle;
      return mo;
   }
   else
      return NULL;
}

//
// ACS_spawnMissile
//
static Mobj *ACS_spawnMissile(mobjtype_t type, fixed_t x, fixed_t y, fixed_t z,
                              fixed_t momx, fixed_t momy, fixed_t momz, int tid,
                              Mobj *target, angle_t angle, bool gravity)
{
   Mobj *mo;

   if((mo = ACS_spawn(type, x, y, z, tid, angle, true)))
   {
      if(mo->info->seesound)
         S_StartSound(mo, mo->info->seesound);

      P_SetTarget<Mobj>(&mo->target, target);

      mo->momx = momx;
      mo->momy = momy;
      mo->momz = momz;

      if(gravity)
      {
         mo->flags &= ~MF_NOGRAVITY;
         mo->flags2 |= MF2_LOGRAV;
      }
      else
         mo->flags |= MF_NOGRAVITY;
   }

   return mo;
}

//
// ACS_CF_SpawnMissile
//
// void SpawnProjectile(int tid, int type, int angle, int speed, int vspeed,
//                      int gravity, int newtid);
//
bool ACS_CF_SpawnMissile(ACS_CF_ARGS)
{
   auto       info    = &static_cast<ACSThread *>(thread)->info;
   int32_t    spotid  = argV[0];
   mobjtype_t type    = E_ThingNumForCompatName(thread->scopeMap->getString(argV[1])->str);
   angle_t    angle   = argV[2] << 24;
   int32_t    speed   = argV[3] * 8;
   int32_t    vspeed  = argV[4] * 8;
   bool       gravity = argV[5] ? true : false;
   int32_t    tid     = argV[6];
   Mobj      *spot    = nullptr;
   fixed_t    momx    = speed * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t    momy    = speed * finesine[  angle >> ANGLETOFINESHIFT];
   fixed_t    momz    = vspeed << FRACBITS;

   while((spot = P_FindMobjFromTID(spotid, spot, info->mo)))
   {
      ACS_spawnMissile(type, spot->x, spot->y, spot->z, momx, momy, momz, tid,
                       spot, angle, gravity);
   }

   return false;
}

//
// ACS_spawnPoint
//
static void ACS_spawnPoint(ACS_CF_ARGS, bool forced)
{
   mobjtype_t type  = E_ThingNumForCompatName(thread->scopeMap->getString(argV[0])->str);
   fixed_t    x     = argV[1];
   fixed_t    y     = argV[2];
   fixed_t    z     = argV[3];
   int        tid   = argV[4];
   angle_t    angle = argV[5] << 24;

   thread->dataStk.push(!!ACS_spawn(type, x, y, z, tid, angle, forced));
}

//
// ACS_CF_SpawnPoint
//
// int Spawn(str type, fixed x, fixed y, fixed z, int tid, int angle);
//
bool ACS_CF_SpawnPoint(ACS_CF_ARGS)
{
   ACS_spawnPoint(thread, argV, argC, false);
   return false;
}

//
// ACS_CF_SpawnPointF
//
// int SpawnForced(str type, fixed x, fixed y, fixed z, int tid, int angle);
//
bool ACS_CF_SpawnPointF(ACS_CF_ARGS)
{
   ACS_spawnPoint(thread, argV, argC, true);
   return false;
}

//
// ACS_spawnSpot
//
static void ACS_spawnSpot(ACS_CF_ARGS, bool forced)
{
   auto        info   = &static_cast<ACSThread *>(thread)->info;
   mobjtype_t  type   = E_ThingNumForCompatName(thread->scopeMap->getString(argV[0])->str);
   int32_t     spotid = argV[1];
   int32_t     tid    = argV[2];
   angle_t     angle  = argV[3] << 24;
   Mobj       *spot   = nullptr;
   ACSVM::Word res    = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, info->mo)))
      res += !!ACS_spawn(type, spot->x, spot->y, spot->z, tid, angle, forced);

   thread->dataStk.push(res);
}

//
// ACS_CF_SpawnSpot
//
// int SpawnSpot(str type, int spottid, int tid, int angle);
//
bool ACS_CF_SpawnSpot(ACS_CF_ARGS)
{
   ACS_spawnSpot(thread, argV, argC, false);
   return false;
}

//
// ACS_CF_SpawnSpotF
//
// int SpawnSpotForced(str type, int spottid, int tid, int angle);
//
bool ACS_CF_SpawnSpotF(ACS_CF_ARGS)
{
   ACS_spawnSpot(thread, argV, argC, true);
   return false;
}

//
// ACS_spawnSpotAng
//
static void ACS_spawnSpotAng(ACS_CF_ARGS, bool forced)
{
   auto       info   = &static_cast<ACSThread *>(thread)->info;
   mobjtype_t type   = E_ThingNumForCompatName(thread->scopeMap->getString(argV[0])->str);
   int32_t    spotid = argV[1];
   int32_t    tid    = argV[2];
   Mobj      *spot   = nullptr;
   uint32_t   res    = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, info->mo)))
      res += !!ACS_spawn(type, spot->x, spot->y, spot->z, tid, spot->angle, forced);

   thread->dataStk.push(res);
}

//
// ACS_CF_SpawnSpotAng
//
// int SpawnSpotFacing(str type, int spottid, int tid);
//
bool ACS_CF_SpawnSpotAng(ACS_CF_ARGS)
{
   ACS_spawnSpotAng(thread, argV, argC, false);
   return false;
}

//
// ACS_CF_SpawnSpotAngF
//
// int SpawnSpotFacingForced(str type, int spottid, int tid);
//
bool ACS_CF_SpawnSpotAngF(ACS_CF_ARGS)
{
   ACS_spawnSpotAng(thread, argV, argC, true);
   return false;
}

//
// ACS_CF_Sqrt
//
// int Sqrt(int x);
//
bool ACS_CF_Sqrt(ACS_CF_ARGS)
{
   thread->dataStk.push((uint32_t)sqrt((double)argV[0]));
   return false;
}

//
// ACS_CF_SqrtFixed
//
// fixed SqrtFixed(fixed x);
//
bool ACS_CF_SqrtFixed(ACS_CF_ARGS)
{
   thread->dataStk.push(M_DoubleToFixed(sqrt(M_FixedToDouble(argV[0]))));
   return false;
}

//
// ACS_CF_StopSound
//
// int StopSound(int tid, int channel);
//
bool ACS_CF_StopSound(ACS_CF_ARGS)
{
   auto  info = &static_cast<ACSThread *>(thread)->info;
   Mobj *mo   = P_FindMobjFromTID(argV[0], nullptr, info->mo);
   int   chan = argC > 1 ? argV[1] & 7 : CHAN_BODY;

   if(mo)
      S_StopSound(mo, chan);

   thread->dataStk.push(0);
   return false;
}

//
// void TakeInventory(str itemname, int amount);
//
bool ACS_CF_SubInventory(ACS_CF_ARGS)
{
   auto info = &static_cast<ACSThread *>(thread)->info;
   char const *itemname = thread->scopeMap->getString(argV[0])->str;
   int amount = argV[1];
   itemeffect_t *item = E_ItemEffectForName(itemname);

   if(!item)
   {
      doom_printf("ACS_CF_SubInventory: Inventory item '%s' not found\a\n", itemname);
      return false;
   }

   if(info->mo)
   {
      // FIXME: Needs to be adapted for when Mobjs get inventory if they get inventory
      if(info->mo->player)
         E_RemoveInventoryItem(info->mo->player, item, amount);
   }
   else
   {
      for(int pnum = 0; pnum != MAXPLAYERS; ++pnum)
      {
         if(playeringame[pnum])
            E_RemoveInventoryItem(&players[pnum], item, amount);
      }
   }
   return false;
}

//
// ACS_thingCount
//
static uint32_t ACS_thingCount(mobjtype_t type, int32_t tid)
{
   Mobj    *mo    = nullptr;
   uint32_t count = 0;

   if(tid)
   {
      while((mo = P_FindMobjFromTID(tid, mo, NULL)))
      {
         if(type == 0 || mo->type == type)
         {
            // don't count killable things that are dead
            if(((mo->flags & MF_COUNTKILL) || (mo->flags3 & MF3_KILLABLE)) &&
               mo->health <= 0)
               continue;
            ++count;
         }
      }
   }
   else
   {
      while((mo = P_NextThinker(mo)))
      {
         if(type == 0 || mo->type == type)
         {
            // don't count killable things that are dead
            if(((mo->flags & MF_COUNTKILL) || (mo->flags3 & MF3_KILLABLE)) &&
               mo->health <= 0)
               continue;
            ++count;
         }
      }
   }

   return count;
}

//
// ACS_CF_ThingCount
//
// int ThingCount(int type, int tid);
//
bool ACS_CF_ThingCount(ACS_CF_ARGS)
{
   int32_t type = argV[0];
   int32_t tid  = argV[1];

   if(type == 0)
      thread->dataStk.push(ACS_thingCount(0, tid));
   else if(type > 0 && type < ACS_NUM_THINGTYPES)
      thread->dataStk.push(ACS_thingCount(ACS_thingtypes[type], tid));
   else
      thread->dataStk.push(0);

   return false;
}

//
// ACS_CF_ThingCountStr
//
// int ThingCountName(str type, int tid);
//
bool ACS_CF_ThingCountStr(ACS_CF_ARGS)
{
   mobjtype_t type = E_ThingNumForCompatName(thread->scopeMap->getString(argV[0])->str);
   int32_t    tid  = argV[1];

   thread->dataStk.push(ACS_thingCount(type, tid));

   return false;
}

//
// ACS_thingCountSec
//
static uint32_t ACS_thingCountSec(mobjtype_t type, int32_t tid, int32_t tag)
{
   sector_t *sector;
   uint32_t  count  = 0;
   int       secnum = -1;

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sector = &sectors[secnum];

      for(Mobj *mo = sector->thinglist; mo; mo = mo->snext)
      {
         if((type == 0 || mo->type == type) && (tid == 0 || mo->tid == tid))
         {
            // don't count killable things that are dead
            if(((mo->flags & MF_COUNTKILL) || (mo->flags3 & MF3_KILLABLE)) &&
               mo->health <= 0)
               continue;
            ++count;
         }
      }
   }

   return count;
}

//
// ACS_CF_ThingCountSec
//
// int ThingCountSector(int type, int tid, int tag);
//
bool ACS_CF_ThingCountSec(ACS_CF_ARGS)
{
   int32_t type = argV[0];
   int32_t tid  = argV[1];
   int32_t tag  = argV[2];

   if(type == 0)
      thread->dataStk.push(ACS_thingCountSec(0, tid, tag));
   else if(type > 0 && type < ACS_NUM_THINGTYPES)
      thread->dataStk.push(ACS_thingCountSec(ACS_thingtypes[type], tid, tag));
   else
      thread->dataStk.push(0);

   return false;
}

//
// ACS_CF_ThingCountSecStr
//
// int ThingCountNameSector(str type, int tid, int tag);
//
bool ACS_CF_ThingCountSecStr(ACS_CF_ARGS)
{
   mobjtype_t type = E_ThingNumForCompatName(thread->scopeMap->getString(argV[0])->str);
   int32_t    tid  = argV[1];
   int32_t    tag  = argV[2];

   thread->dataStk.push(ACS_thingCountSec(type, tid, tag));

   return false;
}

//
// ACS_CF_ThingDamage
//
// int Thing_Damage2(int tid, int amount, str type);
//
bool ACS_CF_ThingDamage(ACS_CF_ARGS)
{
   auto     info   = &static_cast<ACSThread *>(thread)->info;
   int32_t  tid    = argV[0];
   int32_t  damage = argV[1];
   int      mod    = E_DamageTypeNumForName(thread->scopeMap->getString(argV[2])->str);
   Mobj    *mo     = nullptr;
   uint32_t count  = 0;

   while((mo = P_FindMobjFromTID(tid, mo, info->mo)))
   {
      P_DamageMobj(mo, nullptr, nullptr, damage, mod);
      ++count;
   }

   thread->dataStk.push(count);
   return false;
}

//
// ACS_CF_ThingMissile
//
// void Thing_Projectile2(int tid, int type, int angle, int speed, int vspeed,
//                        int gravity, int newtid);
//
bool ACS_CF_ThingMissile(ACS_CF_ARGS)
{
   auto    info    = &static_cast<ACSThread *>(thread)->info;
   int32_t spotid  = argV[0];
   int32_t type    = argV[1];
   angle_t angle   = argV[2] << 24;
   int32_t speed   = argV[3] * 8;
   int32_t vspeed  = argV[4] * 8;
   bool    gravity = argV[5] ? true : false;
   int32_t tid     = argV[6];
   Mobj   *spot    = nullptr;
   fixed_t momx    = speed * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t momy    = speed * finesine[  angle >> ANGLETOFINESHIFT];
   fixed_t momz    = vspeed << FRACBITS;

   if(type < 0 || type >= ACS_NUM_THINGTYPES)
      return false;

   type = ACS_thingtypes[type];

   while((spot = P_FindMobjFromTID(spotid, spot, info->mo)))
   {
      ACS_spawnMissile(type, spot->x, spot->y, spot->z, momx, momy, momz, tid,
                       spot, angle, gravity);
   }

   return false;
}

//
// ACS_CF_ThingSound
//
// void ThingSound(int tid, str sound, int volume);
//
bool ACS_CF_ThingSound(ACS_CF_ARGS)
{
   auto        info = &static_cast<ACSThread *>(thread)->info;
   int         tid  = argV[0];
   const char *snd  = thread->scopeMap->getString(argV[1])->str;
   int         vol  = argV[2];
   Mobj       *mo   = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, info->mo)))
      S_StartSoundNameAtVolume(mo, snd, vol, ATTN_NORMAL, CHAN_AUTO);

   return false;
}

//
// ACS_CF_ThingSoundSeq
//
// int SoundSequenceOnThing(int tid, str sndseq);
//
bool ACS_CF_ThingSoundSeq(ACS_CF_ARGS)
{
   auto  info = &static_cast<ACSThread *>(thread)->info;
   Mobj *mo   = P_FindMobjFromTID(argV[0], nullptr, info->mo);

   S_StartSequenceName(mo, thread->scopeMap->getString(argV[1])->str, SEQ_ORIGIN_OTHER, -1);

   thread->dataStk.push(0);
   return false;
}

//
// ACS_CF_Timer
//
// int Timer(void);
//
bool ACS_CF_Timer(ACS_CF_ARGS)
{
   thread->dataStk.push(leveltime);
   return false;
}

//
// ACS_CF_UniqueTID
//
// int UniqueTID(int start = 0, int limit = 0);
//
bool ACS_CF_UniqueTID(ACS_CF_ARGS)
{
   int32_t  tid = argC > 0 ? argV[0] : 0;
   uint32_t max = argC > 1 ? argV[1] : 0;

   // Start point of 0 means random.
   // Negative TIDs are reserved for special use.
   if(!tid || tid < 0)
      tid = P_RangeRandomEx(pr_script, 1, 0x7FFF);

   while(P_FindMobjFromTID(tid, nullptr, nullptr))
   {
      // Don't overflow the TID.
      if(tid == 0x7FFF)
         tid = 1;
      else
         ++tid;

      // Avoid infinite loops.
      if(!--max)
      {
         tid = 0;
         break;
      }
   }

   thread->dataStk.push(tid);
   return false;
}

//
// ACS_CF_WaitPolyObj
//
// void PolyWait(int polyid);
//
bool ACS_CF_WaitPolyObj(ACS_CF_ARGS)
{
   thread->state = {ACSVM::ThreadState::WaitTag, argV[0], ACS_TAGTYPE_POLYOBJ};
   return true;
}

//
// ACS_CF_WaitSector
//
// void TagWait(int tag);
//
bool ACS_CF_WaitSector(ACS_CF_ARGS)
{
   thread->state = {ACSVM::ThreadState::WaitTag, argV[0], ACS_TAGTYPE_SECTOR};
   return true;
}

// EOF

