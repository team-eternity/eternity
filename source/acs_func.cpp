// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley, David Hill, et al.
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
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
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
#include "doomstat.h"


//
// ACS_funcNOP
//
static void ACS_funcNOP(ACS_FUNCARG)
{
}

//
// ACS_funcActivatorSound
//
static void ACS_funcActivatorSound(ACS_FUNCARG)
{
   const char *snd = ACSVM::GetString(args[0]);
   int         vol = args[1];

   // If trigger is null, turn into ambient sound as in ZDoom.
   S_StartSoundNameAtVolume(thread->trigger, snd, vol, ATTN_NORMAL, CHAN_AUTO);
}

//
// ACS_funcAmbientSound
//
static void ACS_funcAmbientSound(ACS_FUNCARG)
{
   const char *snd = ACSVM::GetString(args[0]);
   int         vol = args[1];

   S_StartSoundNameAtVolume(NULL, snd, vol, ATTN_NORMAL, CHAN_AUTO);
}

//
// ACS_funcAmbientSoundLocal
//
static void ACS_funcAmbientSoundLocal(ACS_FUNCARG)
{
   const char *snd = ACSVM::GetString(args[0]);
   int         vol = args[1];

   if(thread->trigger == players[displayplayer].mo)
      S_StartSoundNameAtVolume(NULL, snd, vol, ATTN_NORMAL, CHAN_AUTO);
}

//
// ACS_funcChangeCeiling
//
static void ACS_funcChangeCeiling(ACS_FUNCARG)
{
   P_ChangeCeilingTex(ACSVM::GetString(args[1]), args[0]);
}

//
// ACS_funcChangeFloor
//
static void ACS_funcChangeFloor(ACS_FUNCARG)
{
   P_ChangeFloorTex(ACSVM::GetString(args[1]), args[0]);
}

//
// ACS_funcCheckSight
//
static void ACS_funcCheckSight(ACS_FUNCARG)
{
   int32_t tid1 = args[0];
   int32_t tid2 = args[1];
   Mobj   *mo1  = NULL;
   Mobj   *mo2  = NULL;

   while((mo1 = P_FindMobjFromTID(tid1, mo1, thread->trigger)))
   {
      while((mo2 = P_FindMobjFromTID(tid2, mo2, thread->trigger)))
      {
         if(P_CheckSight(mo1, mo2))
         {
            *retn++ = 1;
            return;
         }
      }
   }

   *retn++ = 0;
}

//
// ACS_funcCheckThingFlag
//
static void ACS_funcCheckThingFlag(ACS_FUNCARG)
{
   Mobj       *mo   = P_FindMobjFromTID(args[0], NULL, thread->trigger);
   dehflags_t *flag = deh_ParseFlagCombined(ACSVM::GetString(args[1]));

   if(!mo || !flag) {*retn++ = 0; return;}

   switch(flag->index)
   {
   case 0: *retn++ = !!(mo->flags  & flag->value); break;
   case 1: *retn++ = !!(mo->flags2 & flag->value); break;
   case 2: *retn++ = !!(mo->flags3 & flag->value); break;
   case 3: *retn++ = !!(mo->flags4 & flag->value); break;
   default: *retn++ = 0; break;
   }
}

//
// ACS_funcCheckThingType
//
static void ACS_funcCheckThingType(ACS_FUNCARG)
{
   Mobj *mo = P_FindMobjFromTID(args[0], NULL, thread->trigger);

   if(mo)
      *retn++ = E_ThingNumForName(ACSVM::GetString(args[1])) == mo->type;
   else
      *retn++ = 0;
}

//
// ACS_ChkThingVar
//
bool ACS_ChkThingVar(Mobj *thing, uint32_t var, int32_t val)
{
   if(!thing) return false;

   switch(var)
   {
   case ACS_THINGVAR_Health:       return thing->health == val;
   case ACS_THINGVAR_Speed:        return thing->info->speed == val;
   case ACS_THINGVAR_Damage:       return thing->damage == val;
   case ACS_THINGVAR_Alpha:        return thing->translucency == val;
   case ACS_THINGVAR_RenderStyle:  return false;
   case ACS_THINGVAR_SeeSound:     return false;
   case ACS_THINGVAR_AttackSound:  return false;
   case ACS_THINGVAR_PainSound:    return false;
   case ACS_THINGVAR_DeathSound:   return false;
   case ACS_THINGVAR_ActiveSound:  return false;
   case ACS_THINGVAR_Ambush:       return !!(thing->flags & MF_AMBUSH) == !!val;
   case ACS_THINGVAR_Invulnerable: return !!(thing->flags2 & MF2_INVULNERABLE) == !!val;
   case ACS_THINGVAR_JumpZ:        return false;
   case ACS_THINGVAR_ChaseGoal:    return false;
   case ACS_THINGVAR_Frightened:   return false;
   case ACS_THINGVAR_Friendly:     return !!(thing->flags & MF_FRIEND) == !!val;
   case ACS_THINGVAR_SpawnHealth:  return thing->info->spawnhealth == val;
   case ACS_THINGVAR_Dropped:      return !!(thing->flags & MF_DROPPED) == !!val;
   case ACS_THINGVAR_NoTarget:     return false;
   case ACS_THINGVAR_Species:      return false;
   case ACS_THINGVAR_NameTag:      return false;
   case ACS_THINGVAR_Score:        return false;
   case ACS_THINGVAR_NoTrigger:    return false;
   case ACS_THINGVAR_DamageFactor: return false;
   case ACS_THINGVAR_MasterTID:    return false;
   case ACS_THINGVAR_TargetTID:    return thing->target ? thing->target->tid == val : false;
   case ACS_THINGVAR_TracerTID:    return thing->tracer ? thing->tracer->tid == val : false;
   case ACS_THINGVAR_WaterLevel:   return false;
   case ACS_THINGVAR_ScaleX:       return M_FloatToFixed(thing->xscale) == val;
   case ACS_THINGVAR_ScaleY:       return M_FloatToFixed(thing->yscale) == val;
   case ACS_THINGVAR_Dormant:      return !!(thing->flags2 & MF2_DORMANT) == !!val;
   case ACS_THINGVAR_Mass:         return thing->info->mass == val;
   case ACS_THINGVAR_Accuracy:     return false;
   case ACS_THINGVAR_Stamina:      return false;
   case ACS_THINGVAR_Height:       return thing->height == val;
   case ACS_THINGVAR_Radius:       return thing->radius == val;
   case ACS_THINGVAR_ReactionTime: return thing->reactiontime == val;
   case ACS_THINGVAR_MeleeRange:   return MELEERANGE == val;
   case ACS_THINGVAR_ViewHeight:   return false;
   case ACS_THINGVAR_AttackZOff:   return false;
   case ACS_THINGVAR_StencilColor: return false;
   case ACS_THINGVAR_Friction:     return false;
   case ACS_THINGVAR_DamageMult:   return false;

   case ACS_THINGVAR_Angle:          return thing->angle >> 16 == (uint32_t)val;
   case ACS_THINGVAR_Armor:          return thing->player ? thing->player->armorpoints == val : false;
      // ioanch 20160116: extreme sector (portal aware)
   case ACS_THINGVAR_CeilingTexture: return P_ExtremeSectorAtPoint(thing, true)->ceilingpic == R_FindWall(ACSVM::GetString(val));
   case ACS_THINGVAR_CeilingZ:       return thing->ceilingz == val;
   case ACS_THINGVAR_FloorTexture:   return P_ExtremeSectorAtPoint(thing, false)->floorpic == R_FindWall(ACSVM::GetString(val));
   case ACS_THINGVAR_FloorZ:         return thing->floorz == val;
   case ACS_THINGVAR_Frags:          return thing->player ? thing->player->totalfrags == val : false;
   case ACS_THINGVAR_LightLevel:     return thing->subsector->sector->lightlevel == val;
   case ACS_THINGVAR_MomX:           return thing->momx == val;
   case ACS_THINGVAR_MomY:           return thing->momy == val;
   case ACS_THINGVAR_MomZ:           return thing->momz == val;
   case ACS_THINGVAR_Pitch:          return thing->player ? thing->player->pitch >> 16 == val : false;
   case ACS_THINGVAR_PlayerNumber:   return thing->player ? thing->player - players == val : false;
   case ACS_THINGVAR_SigilPieces:    return false;
   case ACS_THINGVAR_TID:            return thing->tid == val;
   case ACS_THINGVAR_Type:           return thing->type == E_ThingNumForName(ACSVM::GetString(val));
   case ACS_THINGVAR_X:              return thing->x == val;
   case ACS_THINGVAR_Y:              return thing->y == val;
   case ACS_THINGVAR_Z:              return thing->z == val;

   default: return false;
   }
}

//
// ACS_funcChkThingVar
//
static void ACS_funcChkThingVar(ACS_FUNCARG)
{
   *retn++ = ACS_ChkThingVar(P_FindMobjFromTID(args[0], NULL, thread->trigger), args[1], args[2]);
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
// ACS_funcClassifyThing
//
static void ACS_funcClassifyThing(ACS_FUNCARG)
{
   int32_t tid = args[0];
   int32_t result;
   Mobj   *mo;

   mo = P_FindMobjFromTID(tid, NULL, thread->trigger);

   if(mo)
   {
      result = 0;

      if(mo->player)
      {
         result |= THINGCLASS_PLAYER;

         if(mo->player->mo != mo)
            result |= THINGCLASS_VOODOODOLL;
      }

      if(mo->flags & MF_MISSILE)
         result |= THINGCLASS_MISSILE;
      else if(mo->flags3 & MF3_KILLABLE || mo->flags & MF_COUNTKILL)
         result |= THINGCLASS_MONSTER;
      else
         result |= THINGCLASS_GENERIC;

      if(mo->health > 0)
         result |= THINGCLASS_ALIVE;
      else
         result |= THINGCLASS_DEAD;
   }
   else
   {
      if(tid)
         result = THINGCLASS_NONE;
      else
         result = THINGCLASS_WORLD;
   }

   *retn++ = result;
}

//
// ACS_funcExecuteScriptAlwaysName
//
static void ACS_funcExecuteScriptAlwaysName(ACS_FUNCARG)
{
   *retn++ = ACS_ExecuteScriptString(args[0], args[1], ACS_EXECUTE_ALWAYS, args+2, argc-2,
                                     thread->trigger, thread->line, thread->lineSide);
}

//
// ACS_funcExecuteScriptName
//
static void ACS_funcExecuteScriptName(ACS_FUNCARG)
{
   *retn++ = ACS_ExecuteScriptString(args[0], args[1], 0, args+2, argc-2,
                                     thread->trigger, thread->line, thread->lineSide);
}

//
// ACS_funcExecuteScriptResultName
//
static void ACS_funcExecuteScriptResultName(ACS_FUNCARG)
{
   ACSThinker *newThread;

   ACS_ExecuteScriptString(args[0], gamemap, ACS_EXECUTE_ALWAYS|ACS_EXECUTE_IMMEDIATE,
                           args+1, argc-1, thread->trigger, thread->line,
                           thread->lineSide, &newThread);

   if(newThread)
      *retn++ = newThread->result;
   else
      *retn++ = 0;
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
// ACS_funcGetCVar
//
static void ACS_funcGetCVar(ACS_FUNCARG)
{
   char const *name = ACSVM::GetString(args[0]);

   command_t *command;
   variable_t *var;

   if(!(command = C_GetCmdForName(name)) || !(var = command->variable))
   {
      *retn++ = 0;
      return;
   }

   switch(var->type)
   {
   case vt_int: *retn++ = *(int *)var->variable; break;
   case vt_float: *retn++ = M_DoubleToFixed(*(double *)var->variable); break;
   case vt_string: *retn++ = 0; break;
   case vt_chararray: *retn++ = 0; break;
   case vt_toggle: *retn++ = *(bool *)var->variable; break;
   default:
      *retn++ = 0;
      break;
   }
}

//
// ACS_funcGetCVarString
//
static void ACS_funcGetCVarString(ACS_FUNCARG)
{
   const char *name = ACSVM::GetString(args[0]);

   command_t *command;
   variable_t *var;

   if(!(command = C_GetCmdForName(name)) || !(var = command->variable))
   {
      *retn++ = 0;
      return;
   }

   const char *val = C_VariableValue(var);
   *retn++ = ACSVM::AddString(val, static_cast<uint32_t>(strlen(val)));
}

//
// Returns the X coordinate of line's centre point
// ACS: GetLineX(lineid, lineratio, linedist)
//
static void ACS_funcGetLineX(ACS_FUNCARG)
{
   int lineid = args[0];
   fixed_t lineratio = args[1];
   fixed_t linedist = args[2];

   int linenum = -1;
   const line_t *line = P_FindLine(lineid, &linenum);
   if (!line)
   {
      *retn++ = 0;
      return;
   }
   int32_t result = line->v1->x + FixedMul(line->dx, lineratio);
   if (linedist)
   {
      angle_t angle = P_PointToAngle(line->v1->x, line->v1->y, line->v2->x, line->v2->y);
      angle -= ANG90;
      unsigned fineangle = angle >> ANGLETOFINESHIFT;
      result += FixedMul(finecosine[fineangle], linedist);
   }
   *retn++ = result;
}

//
// Returns the Y coordinate of line's centre point
//
static void ACS_funcGetLineY(ACS_FUNCARG)
{
   int lineid = args[0];
   fixed_t lineratio = args[1];
   fixed_t linedist = args[2];

   int linenum = -1;
   const line_t *line = P_FindLine(lineid, &linenum);
   if (!line)
   {
      *retn++ = 0;
      return;
   }
   int32_t result = line->v1->y + FixedMul(line->dy, lineratio);
   if (linedist)
   {
      angle_t angle = P_PointToAngle(line->v1->x, line->v1->y, line->v2->x, line->v2->y);
      angle -= ANG90;
      unsigned fineangle = angle >> ANGLETOFINESHIFT;
      result += FixedMul(finesine[fineangle], linedist);
   }
   *retn++ = result;
}

//
// ACS_funcGetPlayerInput
//
static void ACS_funcGetPlayerInput(ACS_FUNCARG)
{
   int32_t   pnum   = args[0];
   int32_t   input  = args[1];
   int32_t   result;
   player_t *player;

   if(pnum == -1 && thread->trigger)
      player = thread->trigger->player;
   else if(pnum >= 0 && pnum < MAXPLAYERS)
      player = &players[pnum];
   else
      player = NULL;

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

   *retn++ = result;
}

//
// ACS_funcGetPolyobjX
//
static void ACS_funcGetPolyobjX(ACS_FUNCARG)
{
   polyobj_t *po = Polyobj_GetForNum(args[0]);

   if(po)
      *retn++ = po->centerPt.x;
   else
      *retn++ = 0x7FFF0000;
}

//
// ACS_funcGetPolyobjY
//
static void ACS_funcGetPolyobjY(ACS_FUNCARG)
{
   polyobj_t *po = Polyobj_GetForNum(args[0]);

   if(po)
      *retn++ = po->centerPt.y;
   else
      *retn++ = 0x7FFF0000;
}

//
// ACS_funcGetSectorCeilingZ
//
static void ACS_funcGetSectorCeilingZ(ACS_FUNCARG)
{
   int secnum = P_FindSectorFromTag(args[0], -1);

   if(secnum >= 0)
   {
      // TODO/FIXME: sloped sectors
      *retn++ = sectors[secnum].ceilingheight;
   }
   else
      *retn++ = 0;
}

//
// ACS_funcGetSectorFloorZ
//
static void ACS_funcGetSectorFloorZ(ACS_FUNCARG)
{
   int secnum = P_FindSectorFromTag(args[0], -1);

   if(secnum >= 0)
   {
      // TODO/FIXME: sloped sectors
      *retn++ = sectors[secnum].floorheight;
   }
   else
      *retn++ = 0;
}

//
// ACS_funcGetSectorLightLevel
//
static void ACS_funcGetSectorLightLevel(ACS_FUNCARG)
{
   int secnum = P_FindSectorFromTag(args[0], -1);

   *retn++ = secnum >= 0 ? sectors[secnum].lightlevel : 0;
}

//
// ACS_GetThingVar
//
int32_t ACS_GetThingVar(Mobj *thing, uint32_t var)
{
   if(!thing) return 0;

   switch(var)
   {
   case ACS_THINGVAR_Health:       return thing->health;
   case ACS_THINGVAR_Speed:        return thing->info->speed;
   case ACS_THINGVAR_Damage:       return thing->damage;
   case ACS_THINGVAR_Alpha:        return thing->translucency;
   case ACS_THINGVAR_RenderStyle:  return 0;
   case ACS_THINGVAR_SeeSound:     return 0;
   case ACS_THINGVAR_AttackSound:  return 0;
   case ACS_THINGVAR_PainSound:    return 0;
   case ACS_THINGVAR_DeathSound:   return 0;
   case ACS_THINGVAR_ActiveSound:  return 0;
   case ACS_THINGVAR_Ambush:       return !!(thing->flags & MF_AMBUSH);
   case ACS_THINGVAR_Invulnerable: return !!(thing->flags2 & MF2_INVULNERABLE);
   case ACS_THINGVAR_JumpZ:        return 0;
   case ACS_THINGVAR_ChaseGoal:    return 0;
   case ACS_THINGVAR_Frightened:   return 0;
   case ACS_THINGVAR_Friendly:     return !!(thing->flags & MF_FRIEND);
   case ACS_THINGVAR_SpawnHealth:  return thing->info->spawnhealth;
   case ACS_THINGVAR_Dropped:      return !!(thing->flags & MF_DROPPED);
   case ACS_THINGVAR_NoTarget:     return 0;
   case ACS_THINGVAR_Species:      return 0;
   case ACS_THINGVAR_NameTag:      return 0;
   case ACS_THINGVAR_Score:        return 0;
   case ACS_THINGVAR_NoTrigger:    return 0;
   case ACS_THINGVAR_DamageFactor: return 0;
   case ACS_THINGVAR_MasterTID:    return 0;
   case ACS_THINGVAR_TargetTID:    return thing->target ? thing->target->tid : 0;
   case ACS_THINGVAR_TracerTID:    return thing->tracer ? thing->tracer->tid : 0;
   case ACS_THINGVAR_WaterLevel:   return 0;
   case ACS_THINGVAR_ScaleX:       return M_FloatToFixed(thing->xscale);
   case ACS_THINGVAR_ScaleY:       return M_FloatToFixed(thing->yscale);
   case ACS_THINGVAR_Dormant:      return !!(thing->flags2 & MF2_DORMANT);
   case ACS_THINGVAR_Mass:         return thing->info->mass;
   case ACS_THINGVAR_Accuracy:     return 0;
   case ACS_THINGVAR_Stamina:      return 0;
   case ACS_THINGVAR_Height:       return thing->height;
   case ACS_THINGVAR_Radius:       return thing->radius;
   case ACS_THINGVAR_ReactionTime: return thing->reactiontime;
   case ACS_THINGVAR_MeleeRange:   return MELEERANGE;
   case ACS_THINGVAR_ViewHeight:   return 0;
   case ACS_THINGVAR_AttackZOff:   return 0;
   case ACS_THINGVAR_StencilColor: return 0;
   case ACS_THINGVAR_Friction:     return 0;
   case ACS_THINGVAR_DamageMult:   return 0;

   case ACS_THINGVAR_Angle:          return thing->angle >> 16;
   case ACS_THINGVAR_Armor:          return thing->player ? thing->player->armorpoints : 0;
   case ACS_THINGVAR_CeilingTexture: return 0;
   case ACS_THINGVAR_CeilingZ:       return thing->ceilingz;
   case ACS_THINGVAR_FloorTexture:   return 0;
   case ACS_THINGVAR_FloorZ:         return thing->floorz;
   case ACS_THINGVAR_Frags:          return thing->player ? thing->player->totalfrags : 0;
   case ACS_THINGVAR_LightLevel:     return thing->subsector->sector->lightlevel;
   case ACS_THINGVAR_MomX:           return thing->momx;
   case ACS_THINGVAR_MomY:           return thing->momy;
   case ACS_THINGVAR_MomZ:           return thing->momz;
   case ACS_THINGVAR_Pitch:          return thing->player ? thing->player->pitch >> 16 : 0;
   case ACS_THINGVAR_PlayerNumber:   return static_cast<int32_t>(thing->player ? thing->player - players : -1);
   case ACS_THINGVAR_SigilPieces:    return 0;
   case ACS_THINGVAR_TID:            return thing->tid;
   case ACS_THINGVAR_Type:           return ACSVM::AddString(thing->info->name);
   case ACS_THINGVAR_X:              return thing->x;
   case ACS_THINGVAR_Y:              return thing->y;
   case ACS_THINGVAR_Z:              return thing->z;

   default: return 0;
   }
}

//
// ACS_funcGetThingVar
//
static void ACS_funcGetThingVar(ACS_FUNCARG)
{
   *retn++ = ACS_GetThingVar(P_FindMobjFromTID(args[0], NULL, thread->trigger), args[1]);
}

//
// ACS_funcIsTIDUsed
//
static void ACS_funcIsTIDUsed(ACS_FUNCARG)
{
   *retn++ = !!P_FindMobjFromTID(args[0], NULL, NULL);
}

//
// ACS_playSound
//
static void ACS_playSound(Mobj *mo, sfxinfo_t *sfx, uint32_t argc, const int32_t *args)
{
   int     chan = argc > 2 ? args[2] & 7 : CHAN_BODY;
   int     vol  = argc > 3 ? args[3] >> 9 : 127;
   bool    loop = argc > 4 ? !!(args[4] & 1) : false;
   fixed_t attn = argc > 5 ? args[5] : 1 << FRACBITS;

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
}

//
// ACS_funcPlaySound
//
static void ACS_funcPlaySound(ACS_FUNCARG)
{
   Mobj      *mo  = P_FindMobjFromTID(args[0], NULL, thread->trigger);
   sfxinfo_t *sfx = S_SfxInfoForName(ACSVM::GetString(args[1]));

   if(!mo || !sfx) return;

   ACS_playSound(mo, sfx, argc, args);
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
// ACS_funcPlayThingSound
//
static void ACS_funcPlayThingSound(ACS_FUNCARG)
{
   Mobj      *mo  = P_FindMobjFromTID(args[0], NULL, thread->trigger);
   int        snd = args[1];
   sfxinfo_t *sfx;

   if(!mo) return;

   switch(snd)
   {
   case SOUND_See:    snd = mo->info->seesound;    break;
   case SOUND_Attack: snd = mo->info->attacksound; break;
   case SOUND_Pain:   snd = mo->info->painsound;   break;
   case SOUND_Death:  snd = mo->info->deathsound;  break;
   case SOUND_Active: snd = mo->info->activesound; break;

   default:
      return;
   }

   if(!snd || !(sfx = E_SoundForDEHNum(snd))) return;

   ACS_playSound(mo, sfx, argc, args);
}

//
// ACS_funcRadiusQuake
//
static void ACS_funcRadiusQuake(ACS_FUNCARG)
{
   int32_t     tid          = args[0];
   int32_t     intensity    = args[1];
   int32_t     duration     = args[2];
   int32_t     damageRadius = args[3];
   int32_t     quakeRadius  = args[4];
   const char *snd          = ACSVM::GetString(args[5]);
   Mobj       *mo           = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
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
}

//
// ACS_funcRandom
//
static void ACS_funcRandom(ACS_FUNCARG)
{
   *retn++ = P_RangeRandomEx(pr_script, args[0], args[1]);
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
// ACS_funcReplaceTextures
//
static void ACS_funcReplaceTextures(ACS_FUNCARG)
{
   int      oldtex = R_FindWall(ACSVM::GetString(args[0]));
   int      newtex = R_FindWall(ACSVM::GetString(args[1]));
   uint32_t flags  = args[2];

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
// ACS_funcSectorDamage
//
static void ACS_funcSectorDamage(ACS_FUNCARG)
{
   int32_t   tag    = args[0];
   int32_t   damage = args[1];
   int       mod    = E_DamageTypeNumForName(ACSVM::GetString(args[2]));
   uint32_t  flags  = args[4];
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
}

//
// ACS_funcSectorSound
//
static void ACS_funcSectorSound(ACS_FUNCARG)
{
   const char   *snd = ACSVM::GetString(args[0]);
   int           vol = args[1];
   PointThinker *src;

   // if script started from a line, use the frontsector's sound origin
   if(thread->line)
      src = &(thread->line->frontsector->soundorg);
   else
      src = NULL;

   S_StartSoundNameAtVolume(src, snd, vol, ATTN_NORMAL, CHAN_AUTO);
}

//
// ACS_funcSetActivator
//
static void ACS_funcSetActivator(ACS_FUNCARG)
{
   P_SetTarget(&thread->trigger, P_FindMobjFromTID(args[0], NULL, NULL));

   *retn++ = !!thread->trigger;
}

//
// ACS_funcSetActivatorToTarget
//
static void ACS_funcSetActivatorToTarget(ACS_FUNCARG)
{
   Mobj *mo = P_FindMobjFromTID(args[0], NULL, thread->trigger);

   if(mo)
   {
      if(mo->target)
         P_SetTarget(&thread->trigger, mo->target);
      else
         P_SetTarget(&thread->trigger, mo);

      *retn++ = 1;
   }
   else
      *retn++ = 0;
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
// ACS_funcSetLineBlocking
//
static void ACS_funcSetLineBlocking(ACS_FUNCARG)
{
   ACS_setLineBlocking(args[0], args[1]);
}

//
// ACS_funcSetLineMonsterBlocking
//
static void ACS_funcSetLineMonsterBlocking(ACS_FUNCARG)
{
   ACS_setLineBlocking(args[0], BLOCK_MONSTERS_OFF + !!args[1]);
}

//
// ACS_funcSetLineSpecial
//
static void ACS_funcSetLineSpecial(ACS_FUNCARG)
{
   int     tag  = args[0];
   int     spec = args[1];
   int     larg[NUMLINEARGS];
   line_t *l;
   int     linenum = -1;

   memcpy(larg, args+2, sizeof(larg));

   while((l = P_FindLine(tag, &linenum)) != NULL)
   {
      l->special = spec;
      memcpy(l->args, larg, sizeof(larg));
   }
}

//
// ACS_funcSetLineTexture
//
static void ACS_funcSetLineTexture(ACS_FUNCARG)
{
   //                               texture   pos      side     tag
   P_ChangeLineTex(ACSVM::GetString(args[3]), args[2], args[1], args[0], false);
}

//
// ACS_funcSetMusic
//
static void ACS_funcSetMusic(ACS_FUNCARG)
{
   S_ChangeMusicName(ACSVM::GetString(args[0]), 1);
}

//
// ACS_funcSetMusicLocal
//
static void ACS_funcSetMusicLocal(ACS_FUNCARG)
{
   if(thread->trigger == players[consoleplayer].mo)
      S_ChangeMusicName(ACSVM::GetString(args[0]), 1);
}

//
// ACS_funcSetSkyDelta
//
static void ACS_funcSetSkyDelta(ACS_FUNCARG)
{
   switch(args[0])
   {
   case 1: LevelInfo.skyDelta  = args[1] >> FRACBITS; break;
   case 2: LevelInfo.sky2Delta = args[1] >> FRACBITS; break;
   }
}

//
// ACS_funcSetThingAngle
//
static void ACS_funcSetThingAngle(ACS_FUNCARG)
{
   int32_t tid   = args[0];
   angle_t angle = (angle_t)args[1] << 16;

   for(Mobj *mo = NULL; (mo = P_FindMobjFromTID(tid, mo, thread->trigger));)
   {
      mo->angle = angle;
   }
}

//
// ACS_funcSetThingMomentum
//
static void ACS_funcSetThingMomentum(ACS_FUNCARG)
{
   int32_t tid  = args[0];
   fixed_t momx = args[1];
   fixed_t momy = args[2];
   fixed_t momz = args[3];
   bool    add  = args[4] ? true : false;
   Mobj   *mo   = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
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
}

//
// ACS_funcSetThingPitch
//
static void ACS_funcSetThingPitch(ACS_FUNCARG)
{
   int32_t tid   = args[0];
   angle_t pitch = (angle_t)args[1] << 16;

   for(Mobj *mo = NULL; (mo = P_FindMobjFromTID(tid, mo, thread->trigger));)
   {
      if(mo->player)
         mo->player->prevpitch = mo->player->pitch = pitch;
   }
}

//
// ACS_funcSetThingPosition
//
static void ACS_funcSetThingPosition(ACS_FUNCARG)
{
   int32_t tid = args[0];
   fixed_t x   = args[1];
   fixed_t y   = args[2];
   fixed_t z   = args[3];
   bool    fog = args[4] ? true : false;
   Mobj   *mo, *fogmo;

   if((mo = P_FindMobjFromTID(tid, NULL, thread->trigger)))
   {
      fixed_t oldx = mo->x;
      fixed_t oldy = mo->y;
      fixed_t oldz = mo->z;

      mo->z = z;

      if(P_CheckPositionExt(mo, x, y))
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
            fogmo = P_SpawnMobj(oldx, oldy, oldz + GameModeInfo->teleFogHeight,
                                E_SafeThingName(GameModeInfo->teleFogType));
            S_StartSound(fogmo, GameModeInfo->teleSound);

            // ... and destination.
            fogmo = P_SpawnMobj(x, y, z + GameModeInfo->teleFogHeight,
                                E_SafeThingName(GameModeInfo->teleFogType));
            S_StartSound(fogmo, GameModeInfo->teleSound);
         }

         *retn++ = 1;
      }
      else
      {
         mo->z = oldz;

         *retn++ = 0;
      }
   }
   else
      *retn++ = 0;
}

//
// ACS_funcSetThingSpecial
//
static void ACS_funcSetThingSpecial(ACS_FUNCARG)
{
   int   tid  = args[0];
   //int   spec = args[1]; HEXEN_TODO
   int   larg[NUMLINEARGS];
   Mobj *mo   = NULL;

   memcpy(larg, args+2, sizeof(larg));

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
   {
      //mo->special = spec; HEXEN_TODO
      memcpy(mo->args, larg, sizeof(larg));
   }
}

//
// ACS_funcSetThingState
//
static void ACS_funcSetThingState(ACS_FUNCARG)
{
   int32_t     tid       = args[0];
   const char *statename = ACSVM::GetString(args[1]);
   statenum_t  statenum  = E_StateNumForName(statename);
   state_t    *state;
   int32_t     count     = 0;
   Mobj       *mo        = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
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

   *retn++ = count;
}

//
// ACS_SetThingVar
//
void ACS_SetThingVar(Mobj *thing, uint32_t var, int32_t val)
{
   if(!thing) return;

   switch(var)
   {
   case ACS_THINGVAR_Health:       thing->health = val; break;
   case ACS_THINGVAR_Speed:        break;
   case ACS_THINGVAR_Damage:       thing->damage = val; break;
   case ACS_THINGVAR_Alpha:        thing->translucency = val; break;
   case ACS_THINGVAR_RenderStyle:  break;
   case ACS_THINGVAR_SeeSound:     break;
   case ACS_THINGVAR_AttackSound:  break;
   case ACS_THINGVAR_PainSound:    break;
   case ACS_THINGVAR_DeathSound:   break;
   case ACS_THINGVAR_ActiveSound:  break;
   case ACS_THINGVAR_Ambush:       if(val) thing->flags |=  MF_AMBUSH;
                                   else    thing->flags &= ~MF_AMBUSH; break;
   case ACS_THINGVAR_Invulnerable: if(val) thing->flags2 |=  MF2_INVULNERABLE;
                                   else    thing->flags2 &= ~MF2_INVULNERABLE; break;
   case ACS_THINGVAR_JumpZ:        break;
   case ACS_THINGVAR_ChaseGoal:    break;
   case ACS_THINGVAR_Frightened:   break;
   case ACS_THINGVAR_Friendly:     if(val) thing->flags |=  MF_FRIEND;
                                   else    thing->flags &= ~MF_FRIEND; break;
   case ACS_THINGVAR_SpawnHealth:  break;
   case ACS_THINGVAR_Dropped:      if(val) thing->flags |=  MF_DROPPED;
                                   else    thing->flags &= ~MF_DROPPED; break;
   case ACS_THINGVAR_NoTarget:     break;
   case ACS_THINGVAR_Species:      break;
   case ACS_THINGVAR_NameTag:      break;
   case ACS_THINGVAR_Score:        break;
   case ACS_THINGVAR_NoTrigger:    break;
   case ACS_THINGVAR_DamageFactor: break;
   case ACS_THINGVAR_MasterTID:    break;
   case ACS_THINGVAR_TargetTID:    P_SetTarget(&thing->target, P_FindMobjFromTID(val, 0, 0)); break;
   case ACS_THINGVAR_TracerTID:    P_SetTarget(&thing->tracer, P_FindMobjFromTID(val, 0, 0)); break;
   case ACS_THINGVAR_WaterLevel:   break;
   case ACS_THINGVAR_ScaleX:       thing->xscale = M_FixedToFloat(val); break;
   case ACS_THINGVAR_ScaleY:       thing->yscale = M_FixedToFloat(val); break;
   case ACS_THINGVAR_Dormant:      if(val) thing->flags2 |=  MF2_DORMANT;
                                   else    thing->flags2 &= ~MF2_DORMANT; break;
   case ACS_THINGVAR_Mass:         break;
   case ACS_THINGVAR_Accuracy:     break;
   case ACS_THINGVAR_Stamina:      break;
   case ACS_THINGVAR_Height:       break;
   case ACS_THINGVAR_Radius:       break;
   case ACS_THINGVAR_ReactionTime: break;
   case ACS_THINGVAR_MeleeRange:   break;
   case ACS_THINGVAR_ViewHeight:   break;
   case ACS_THINGVAR_AttackZOff:   break;
   case ACS_THINGVAR_StencilColor: break;
   case ACS_THINGVAR_Friction:     break;
   case ACS_THINGVAR_DamageMult:   break;

   case ACS_THINGVAR_Angle:          thing->angle = val << 16; break;
   case ACS_THINGVAR_Armor:          break;
   case ACS_THINGVAR_CeilingTexture: break;
   case ACS_THINGVAR_CeilingZ:       break;
   case ACS_THINGVAR_FloorTexture:   break;
   case ACS_THINGVAR_FloorZ:         break;
   case ACS_THINGVAR_Frags:          break;
   case ACS_THINGVAR_LightLevel:     break;
   case ACS_THINGVAR_MomX:           thing->momx = val; break;
   case ACS_THINGVAR_MomY:           thing->momy = val; break;
   case ACS_THINGVAR_MomZ:           thing->momz = val; break;
   case ACS_THINGVAR_Pitch:          if(thing->player) thing->player->prevpitch =
                                                       thing->player->pitch = val << 16; break;
   case ACS_THINGVAR_PlayerNumber:   break;
   case ACS_THINGVAR_SigilPieces:    break;
   case ACS_THINGVAR_TID:            P_RemoveThingTID(thing); P_AddThingTID(thing, val); break;
   case ACS_THINGVAR_Type:           break;
   case ACS_THINGVAR_X:              thing->x = val; break;
   case ACS_THINGVAR_Y:              thing->y = val; break;
   case ACS_THINGVAR_Z:              thing->z = val; break;
   }
}

//
// ACS_funcSetThingVar
//
static void ACS_funcSetThingVar(ACS_FUNCARG)
{
   int32_t  tid = args[0];
   uint32_t var = args[1];
   int32_t  val = args[2];
   Mobj    *mo = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
      ACS_SetThingVar(mo, var, val);
}

//
// ACS_funcSoundSequence
//
static void ACS_funcSoundSequence(ACS_FUNCARG)
{
   const char *snd = ACSVM::GetString(args[0]);
   sector_t   *sec;

   if(thread->line && (sec = thread->line->frontsector))
      S_StartSectorSequenceName(sec, snd, SEQ_ORIGIN_SECTOR_F);
   else
      S_StartSequenceName(NULL, snd, SEQ_ORIGIN_OTHER, -1);
}

//
// ACS_funcSoundSequenceThing
//
static void ACS_funcSoundSequenceThing(ACS_FUNCARG)
{
   Mobj *mo = P_FindMobjFromTID(args[0], NULL, NULL);
   S_StartSequenceName(mo, ACSVM::GetString(args[1]), SEQ_ORIGIN_OTHER, -1);
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
      if(!forced && !P_CheckPositionExt(mo, mo->x, mo->y))
      {
         // And if not, unmake the Mobj.
         mo->state = NULL;
         mo->removeThinker();
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
// ACS_spawnProjectile
//
static Mobj *ACS_spawnProjectile(mobjtype_t type, fixed_t x, fixed_t y, fixed_t z,
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
// ACS_spawnPoint
//
static void ACS_spawnPoint(ACS_FUNCARG, bool forced)
{
   mobjtype_t type  = E_ThingNumForName(ACSVM::GetString(args[0]));
   fixed_t    x     = args[1];
   fixed_t    y     = args[2];
   fixed_t    z     = args[3];
   int        tid   = args[4];
   angle_t    angle = args[5] << 24;

   *retn++ = !!ACS_spawn(type, x, y, z, tid, angle, forced);
}

//
// ACS_funcSpawnPoint
//
static void ACS_funcSpawnPoint(ACS_FUNCARG)
{
   ACS_spawnPoint(thread, argc, args, retn, false);
}

//
// ACS_funcSpawnPointForced
//
static void ACS_funcSpawnPointForced(ACS_FUNCARG)
{
   ACS_spawnPoint(thread, argc, args, retn, true);
}

//
// ACS_funcSpawnProjectile
//
static void ACS_funcSpawnProjectile(ACS_FUNCARG)
{
   int32_t    spotid  = args[0];
   mobjtype_t type    = E_ThingNumForName(ACSVM::GetString(args[1]));
   angle_t    angle   = args[2] << 24;
   int32_t    speed   = args[3] * 8;
   int32_t    vspeed  = args[4] * 8;
   bool       gravity = args[5] ? true : false;
   int32_t    tid     = args[6];
   Mobj      *spot    = NULL;
   fixed_t    momx    = speed * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t    momy    = speed * finesine[  angle >> ANGLETOFINESHIFT];
   fixed_t    momz    = vspeed << FRACBITS;

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
      ACS_spawnProjectile(type, spot->x, spot->y, spot->z, momx, momy, momz,
                          tid, spot, angle, gravity);
}

//
// ACS_spawnSpot
//
static void ACS_spawnSpot(ACS_FUNCARG, bool forced)
{
   mobjtype_t type   = E_ThingNumForName(ACSVM::GetString(args[0]));
   int        spotid = args[1];
   int        tid    = args[2];
   angle_t    angle  = args[3] << 24;
   Mobj      *spot   = NULL;

   *retn = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
      *retn += !!ACS_spawn(type, spot->x, spot->y, spot->z, tid, angle, forced);

   ++retn;
}

//
// ACS_funcSpawnSpot
//
static void ACS_funcSpawnSpot(ACS_FUNCARG)
{
   ACS_spawnSpot(thread, argc, args, retn, false);
}

//
// ACS_funcSpawnSpotForced
//
static void ACS_funcSpawnSpotForced(ACS_FUNCARG)
{
   ACS_spawnSpot(thread, argc, args, retn, true);
}

//
// ACS_spawnSpotAngle
//
static void ACS_spawnSpotAngle(ACS_FUNCARG, bool forced)
{
   mobjtype_t type   = E_ThingNumForName(ACSVM::GetString(args[0]));
   int        spotid = args[1];
   int        tid    = args[2];
   Mobj      *spot   = NULL;

   *retn = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
      *retn += !!ACS_spawn(type, spot->x, spot->y, spot->z, tid, spot->angle, forced);

   ++retn;
}

//
// ACS_funcSpawnSpotAngle
//
static void ACS_funcSpawnSpotAngle(ACS_FUNCARG)
{
   ACS_spawnSpotAngle(thread, argc, args, retn, false);
}

//
// ACS_funcSpawnSpotAngleForced
//
static void ACS_funcSpawnSpotAngleForced(ACS_FUNCARG)
{
   ACS_spawnSpotAngle(thread, argc, args, retn, true);
}

//
// ACS_funcSqrt
//
static void ACS_funcSqrt(ACS_FUNCARG)
{
   *retn++ = (int32_t)sqrt((double)args[0]);
}

//
// ACS_funcSqrtFixed
//
static void ACS_funcSqrtFixed(ACS_FUNCARG)
{
   *retn++ = M_DoubleToFixed(sqrt(M_FixedToDouble(args[0])));
}

//
// ACS_funcStopSound
//
static void ACS_funcStopSound(ACS_FUNCARG)
{
   Mobj *mo   = P_FindMobjFromTID(args[0], NULL, thread->trigger);
   int   chan = argc > 1 ? args[1] & 7 : CHAN_BODY;

   if(!mo) return;

   S_StopSound(mo, chan);
}

//
// ACS_funcStrCaseCmp
//
static void ACS_funcStrCaseCmp(ACS_FUNCARG)
{
   const char *l = ACSVM::GetString(args[0]);
   const char *r = ACSVM::GetString(args[1]);

   if(argc > 2)
      *retn++ = strncasecmp(l, r, args[2]);
   else
      *retn++ = strcasecmp(l, r);
}

//
// ACS_funcStrCmp
//
static void ACS_funcStrCmp(ACS_FUNCARG)
{
   const char *l = ACSVM::GetString(args[0]);
   const char *r = ACSVM::GetString(args[1]);

   if(argc > 2)
      *retn++ = strncmp(l, r, args[2]);
   else
      *retn++ = strcmp(l, r);
}

//
// ACS_funcStrLeft
//
static void ACS_funcStrLeft(ACS_FUNCARG)
{
   ACSString *str = ACSVM::GetStringData(args[0]);
   uint32_t   len = args[1];

   if(!str)
   {
      *retn++ = ACSVM::AddString("", 0);
   }
   else if(len < str->data.l)
   {
      *retn++ = ACSVM::AddString(str->data.s, len);
   }
   else
   {
      *retn++ = args[0];
   }
}

//
// ACS_funcStrMid
//
static void ACS_funcStrMid(ACS_FUNCARG)
{
   ACSString *str   = ACSVM::GetStringData(args[0]);
   uint32_t   start = args[1];
   uint32_t   len   = args[2];

   if(!str || start >= str->data.l)
   {
      *retn++ = ACSVM::AddString("", 0);
   }
   else if(start + len < str->data.l)
   {
      *retn++ = ACSVM::AddString(str->data.s + start, len);
   }
   else
   {
      *retn++ = ACSVM::AddString(str->data.s + start, str->data.l - start);
   }
}

//
// ACS_funcStrRight
//
static void ACS_funcStrRight(ACS_FUNCARG)
{
   ACSString *str = ACSVM::GetStringData(args[0]);
   uint32_t   len = args[1];

   if(!str)
   {
      *retn++ = ACSVM::AddString("", 0);
   }
   else if(len < str->data.l)
   {
      *retn++ = ACSVM::AddString(str->data.s + str->data.l - len, len);
   }
   else
   {
      *retn++ = args[0];
   }
}

//
// ACS_funcSuspendScriptName
//
static void ACS_funcSuspendScriptName(ACS_FUNCARG)
{
   *retn++ = ACS_SuspendScriptString(args[0], args[1]);
}

//
// ACS_funcTerminateScriptName
//
static void ACS_funcTerminateScriptName(ACS_FUNCARG)
{
   *retn++ = ACS_TerminateScriptString(args[0], args[1]);
}

//
// ACS_thingCount
//
static int32_t ACS_thingCount(mobjtype_t type, int32_t tid)
{
   Mobj *mo = NULL;
   int count = 0;

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
// ACS_funcThingCount
//
static void ACS_funcThingCount(ACS_FUNCARG)
{
   int32_t type = args[0];
   int32_t tid  = args[1];

   if(type == 0)
      *retn++ = ACS_thingCount(0, tid);
   else if(type > 0 && type < ACS_NUM_THINGTYPES)
      *retn++ = ACS_thingCount(ACS_thingtypes[type], tid);
   else
      *retn++ = 0;
}

//
// ACS_funcThingCountName
//
static void ACS_funcThingCountName(ACS_FUNCARG)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   int32_t    tid  = args[1];

   *retn++ = ACS_thingCount(type, tid);
}

//
// ACS_thingCountSector
//
static int32_t ACS_thingCountSector(int32_t tag, mobjtype_t type, int32_t tid)
{
   sector_t *sector;
   int count = 0;
   int secnum = -1;

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
// ACS_funcThingCountNameSector
//
static void ACS_funcThingCountNameSector(ACS_FUNCARG)
{
   int32_t    tag  = args[0];
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[1]));
   int32_t    tid  = args[2];

   *retn++ = ACS_thingCountSector(tag, type, tid);
}

//
// ACS_funcThingCountSector
//
static void ACS_funcThingCountSector(ACS_FUNCARG)
{
   int32_t tag  = args[0];
   int32_t type = args[1];
   int32_t tid  = args[2];

   if(type == 0)
      *retn++ = ACS_thingCountSector(tag, 0, tid);
   else if(type > 0 && type < ACS_NUM_THINGTYPES)
      *retn++ = ACS_thingCountSector(tag, ACS_thingtypes[type], tid);
   else
      *retn++ = 0;
}

//
// ACS_funcThingDamage
//
static void ACS_funcThingDamage(ACS_FUNCARG)
{
   int32_t tid    = args[0];
   int32_t damage = args[1];
   int     mod    = E_DamageTypeNumForName(ACSVM::GetString(args[2]));
   Mobj   *mo     = NULL;
   int32_t count  = 0;

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
   {
      P_DamageMobj(mo, NULL, NULL, damage, mod);
      ++count;
   }

   *retn++ = count;
}

//
// ACS_funcThingProjectile
//
static void ACS_funcThingProjectile(ACS_FUNCARG)
{
   int32_t spotid  = args[0];
   int32_t type    = args[1];
   angle_t angle   = args[2] << 24;
   int32_t speed   = args[3] * 8;
   int32_t vspeed  = args[4] * 8;
   bool    gravity = args[5] ? true : false;
   int32_t tid     = args[6];
   Mobj   *spot    = NULL;
   fixed_t momx    = speed * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t momy    = speed * finesine[  angle >> ANGLETOFINESHIFT];
   fixed_t momz    = vspeed << FRACBITS;

   if(type < 0 || type >= ACS_NUM_THINGTYPES)
      return;

   type = ACS_thingtypes[type];

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
      ACS_spawnProjectile(type, spot->x, spot->y, spot->z, momx, momy, momz,
                          tid, spot, angle, gravity);
}

//
// ACS_funcThingSound
//
static void ACS_funcThingSound(ACS_FUNCARG)
{
   int         tid = args[0];
   const char *snd = ACSVM::GetString(args[1]);
   int         vol = args[2];
   Mobj       *mo  = NULL;

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
      S_StartSoundNameAtVolume(mo, snd, vol, ATTN_NORMAL, CHAN_AUTO);
}

//
// ACS_funcTrigHypot
//
static void ACS_funcTrigHypot(ACS_FUNCARG)
{
   double x = M_FixedToDouble(args[0]);
   double y = M_FixedToDouble(args[1]);
   *retn++ = M_DoubleToFixed(sqrt(x * x + y * y));
}

//
// ACS_funcUniqueTID
//
static void ACS_funcUniqueTID(ACS_FUNCARG)
{
   int32_t  tid = argc > 0 ? args[0] : 0;
   uint32_t max = argc > 1 ? args[1] : 0;

   // Start point of 0 means random. How about outside the int16_t range?
   // We also don't trust no negative TIDs 'round these here parts.
   if(!tid || tid < 0)
      tid = P_RangeRandomEx(pr_script, 0x8000, 0xFFFF);

   while(P_FindMobjFromTID(tid, NULL, NULL))
   {
      // Don't overflow the tid. Again, we don't take kindly to negative TIDs.
      if(tid == 0xFFFF)
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

   *retn++ = tid;
}

acs_func_t ACSfunc[ACS_FUNCMAX] =
{
   #define ACS_FUNC(FUNC) ACS_func##FUNC,
   #include "acs_op.h"
   #undef ACS_FUNC
};

// EOF

