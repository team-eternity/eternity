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
#include "e_exdata.h"
#include "e_things.h"
#include "m_random.h"
#include "p_spec.h"
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
// ACS_funcRandom
//
static void ACS_funcRandom(ACS_FUNCARG)
{
   *retn++ = P_RangeRandom(pr_script, args[0], args[1]);
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
// ACS_funcThingCount
//
static void ACS_funcThingCount(ACS_FUNCARG)
{
   int32_t type = args[0];
   int32_t tid  = args[1];
   Mobj *mo = NULL;
   int count = 0;

   // translate ACS thing type ids to true types
   if(type < 0 || type >= ACS_NUM_THINGTYPES)
      goto counted;

   type = ACS_thingtypes[type];

   while((mo = P_NextThinker(mo)))
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

counted:
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
   Mobj   *spot    = NULL, *mo;
   fixed_t momx    = speed * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t momy    = speed * finesine[  angle >> ANGLETOFINESHIFT];
   fixed_t momz    = vspeed << FRACBITS;

   if(type < 0 || type >= ACS_NUM_THINGTYPES)
      return;

   type = ACS_thingtypes[type];

   while((spot = P_FindMobjFromTID(spotid, spot, thread->trigger)))
   {
      if(!(mo = ACS_spawn(type, spot->x, spot->y, spot->z, tid, angle)))
         continue;

      if(mo->info->seesound)
         S_StartSound(mo, mo->info->seesound);

      P_SetTarget<Mobj>(&mo->target, spot);

      mo->momx = momx;
      mo->momy = momy;
      mo->momz = momz;

      mo->flags &= ~MF_NOGRAVITY;
      if(gravity) mo->flags2 |= MF2_LOGRAV;
   }
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
   ACS_funcGetSectorCeilingZ,
   ACS_funcGetSectorFloorZ,
   ACS_funcRandom,
   ACS_funcSectorSound,
   ACS_funcSetLineBlocking,
   ACS_funcSetLineMonsterBlocking,
   ACS_funcSetLineSpecial,
   ACS_funcSetLineTexture,
   ACS_funcSetMusic,
   ACS_funcSetMusicLocal,
   ACS_funcSetThingSpecial,
   ACS_funcSoundSequence,
   ACS_funcSpawnPoint,
   ACS_funcSpawnSpot,
   ACS_funcThingCount,
   ACS_funcThingProjectile,
   ACS_funcThingSound,
};

// EOF

