// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
// Copyright(C) 2012 David Hill
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

#include "a_small.h"
#include "acs_intr.h"
#include "d_event.h"
#include "d_gi.h"
#include "e_args.h"
#include "e_exdata.h"
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_maputl.h"
#include "p_spec.h"
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
// ACS_funcRandom
//
static void ACS_funcRandom(ACS_FUNCARG)
{
   *retn++ = P_RangeRandom(pr_script, args[0], args[1]);
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
   int16_t spec = args[1];
   int     larg[NUMLINEARGS];
   line_t *l;
   int     linenum = -1;

   memcpy(larg, args+2, sizeof(larg));

   // do special/args translation for Hexen maps
   P_ConvertHexenLineSpec(&spec, larg);

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
// ACS_funcSetThingPosition
//
static void ACS_funcSetThingPosition(ACS_FUNCARG)
{
   int32_t tid = args[0];
   fixed_t x   = args[1];
   fixed_t y   = args[2];
   fixed_t z   = args[3];
   bool    fog = args[4];
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

         P_SetThingPosition(mo);

         // Handle fog.
         if(fog)
         {
            // Teleport fog at source...
            fogmo = P_SpawnMobj(oldx, oldy, oldz + GameModeInfo->teleFogHeight,
                                E_SafeThingType(GameModeInfo->teleFogType));
            S_StartSound(fogmo, GameModeInfo->teleSound);

            // ... and destination.
            fogmo = P_SpawnMobj(x, y, z + GameModeInfo->teleFogHeight,
                                E_SafeThingType(GameModeInfo->teleFogType));
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
   int     tid  = args[0];
   int16_t spec = args[1];
   int     larg[NUMLINEARGS];
   Mobj   *mo   = NULL;

   memcpy(larg, args+2, sizeof(larg));

   // do special/args translation for Hexen maps
   P_ConvertHexenLineSpec(&spec, larg);

   while((mo = P_FindMobjFromTID(tid, mo, thread->trigger)))
   {
    //mo->special = spec;
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
// ACS_spawn
//
static Mobj *ACS_spawn(mobjtype_t type, fixed_t x, fixed_t y, fixed_t z,
                       int tid, angle_t angle)
{
   Mobj *mo;
   if(type != -1 && (mo = P_SpawnMobj(x, y, z, type)))
   {
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

   if((mo = ACS_spawn(type, x, y, z, tid, angle)))
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
// ACS_funcSpawnPoint
//
static void ACS_funcSpawnPoint(ACS_FUNCARG)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   fixed_t x     = args[1];
   fixed_t y     = args[2];
   fixed_t z     = args[3];
   int     tid   = args[4];
   angle_t angle = args[5] << 24;

   *retn++ = !!ACS_spawn(type, x, y, z, tid, angle);
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
   bool       gravity = args[5];
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
// ACS_funcSpawnSpot
//
static void ACS_funcSpawnSpot(ACS_FUNCARG)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   int     spotid = args[1];
   int     tid    = args[2];
   angle_t angle  = args[3] << 24;
   Mobj   *spot   = NULL;

   *retn = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
      *retn += !!ACS_spawn(type, spot->x, spot->y, spot->z, tid, angle);

   ++retn;
}

//
// ACS_funcSpawnSpotAngle
//
static void ACS_funcSpawnSpotAngle(ACS_FUNCARG)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   int     spotid = args[1];
   int     tid    = args[2];
   Mobj   *spot   = NULL;

   *retn = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
      *retn += !!ACS_spawn(type, spot->x, spot->y, spot->z, tid, spot->angle);

   ++retn;
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

   if(type <= 0 || type >= ACS_NUM_THINGTYPES)
      type = 0;
   else
      type = ACS_thingtypes[type];

   *retn++ = ACS_thingCount(type, tid);
}

//
// ACS_funcThingCountName
//
static void ACS_funcThingCountName(ACS_FUNCARG)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   int32_t    tid  = args[1];

   if(type == -1)
      type = 0;

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

   if(type == -1)
      type = 0;

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

   if(type <= 0 || type >= ACS_NUM_THINGTYPES)
      type = 0;
   else
      type = ACS_thingtypes[type];

   *retn++ = ACS_thingCountSector(tag, type, tid);
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
   bool    gravity = args[5];
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

acs_func_t ACSfunc[ACS_FUNCMAX] =
{
   ACS_funcNOP,
   ACS_funcActivatorSound,
   ACS_funcAmbientSound,
   ACS_funcAmbientSoundLocal,
   ACS_funcChangeCeiling,
   ACS_funcChangeFloor,
   ACS_funcClassifyThing,
   ACS_funcGetPlayerInput,
   ACS_funcGetSectorCeilingZ,
   ACS_funcGetSectorFloorZ,
   ACS_funcGetSectorLightLevel,
   ACS_funcRandom,
   ACS_funcReplaceTextures,
   ACS_funcSectorDamage,
   ACS_funcSectorSound,
   ACS_funcSetLineBlocking,
   ACS_funcSetLineMonsterBlocking,
   ACS_funcSetLineSpecial,
   ACS_funcSetLineTexture,
   ACS_funcSetMusic,
   ACS_funcSetMusicLocal,
   ACS_funcSetThingPosition,
   ACS_funcSetThingSpecial,
   ACS_funcSetThingState,
   ACS_funcSoundSequence,
   ACS_funcSpawnPoint,
   ACS_funcSpawnProjectile,
   ACS_funcSpawnSpot,
   ACS_funcSpawnSpotAngle,
   ACS_funcThingCount,
   ACS_funcThingCountName,
   ACS_funcThingCountNameSector,
   ACS_funcThingCountSector,
   ACS_funcThingDamage,
   ACS_funcThingProjectile,
   ACS_funcThingSound,
};

// EOF

