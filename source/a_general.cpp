// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//      Action Pointer Functions
//      that are associated with states/frames.
//
//      Generalized/Parameterized action functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_common.h"
#include "a_small.h"
#include "acs_intr.h"
#include "c_io.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_string.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "hu_stuff.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//
void A_Mushroom(Mobj *actor)
{
   int i, j, n = actor->damage;
   int ShotType;
   
   // Mushroom parameters are part of code pointer's state
   fixed_t misc1 = 
      actor->state->misc1 ? actor->state->misc1 : FRACUNIT*4;
   fixed_t misc2 = 
      actor->state->misc2 ? actor->state->misc2 : FRACUNIT/2;

   // haleyjd: extended parameter support requested by Mordeth:
   // allow specification of thing type in args[0]

   ShotType = E_ArgAsThingNumG0(actor->state->args, 0);

   if(ShotType < 0 || ShotType == -1)
      ShotType = E_SafeThingType(MT_FATSHOT);
   
   A_Explode(actor);               // make normal explosion

   for(i = -n; i <= n; i += 8)    // launch mushroom cloud
   {
      for(j = -n; j <= n; j += 8)
      {
         // haleyjd 08/07/11: This is bad. Very bad.
         // Rewritten to use P_SpawnMissileWithDest function.
         //Mobj target = *actor, *mo;
         Mobj *mo;
         fixed_t x, y, z;

         x = actor->x + (i << FRACBITS); // Aim in many directions from source
         y = actor->y + (j << FRACBITS);
         z = actor->z + P_AproxDistance(i, j) * misc1; // Aim fairly high
       
         mo = P_SpawnMissileWithDest(actor, actor, 
                                     ShotType,          // Launch fireball
                                     actor->z + DEFAULTMISSILEZ,
                                     x, y, z);
         
         mo->momx = FixedMul(mo->momx, misc2);
         mo->momy = FixedMul(mo->momy, misc2);         // Slow down a bit
         mo->momz = FixedMul(mo->momz, misc2);
         mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
      }
   }
}

//
// killough 11/98
//
// The following were inspired by Len Pitre
//
// A small set of highly-sought-after code pointers
//

void A_Spawn(Mobj *mo)
{
   if(mo->state->misc1)
   {
      Mobj *newmobj;

      // haleyjd 03/06/03 -- added error check
      //         07/05/03 -- adjusted for EDF
      int thingtype = E_SafeThingType((int)(mo->state->misc1));
      
      newmobj = 
         P_SpawnMobj(mo->x, mo->y, 
                     (mo->state->misc2 << FRACBITS) + mo->z,
                     thingtype);
      if(newmobj)
         newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
   }
}

void A_Turn(Mobj *mo)
{
   mo->angle += (angle_t)(((uint64_t) mo->state->misc1 << 32) / 360);
}

void A_Face(Mobj *mo)
{
   mo->angle = (angle_t)(((uint64_t) mo->state->misc1 << 32) / 360);
}

static const char *kwds_A_Scratch[] =
{
   "usemisc1",
   "usedamage",
   "usecounter",
   "useconstant"
};

static argkeywd_t scratchkwds =
{
   kwds_A_Scratch,
   sizeof(kwds_A_Scratch) / sizeof(const char *)
};

//
// A_Scratch
//
// Parameterized melee attack.
// * misc1 == constant damage amount
// * misc2 == optional sound deh num to play
//
// haleyjd 08/02/04: extended parameters:
// * args[0] == special mode select
//              * 0 == compatibility (use misc1 like normal)
//              * 1 == use mo->damage
//              * 2 == use counter specified in args[1]
//              * 3 == use constant value in args[1]
// * args[1] == counter number for mode 2; constant for mode 3
// * args[2] == EDF sound name
//
void A_Scratch(Mobj *mo)
{
   int damage, cnum;
   int mode;

   // haleyjd: demystified
   if(!mo->target)
      return;

   mode = E_ArgAsKwd(mo->state->args, 0, &scratchkwds, 0);

   // haleyjd 08/02/04: extensions to get damage from multiple sources
   switch(mode)
   {
   default:
   case 0: // default, compatibility mode
      damage = mo->state->misc1;
      break;
   case 1: // use mo->damage
      damage = mo->damage;
      break;
   case 2: // use a counter
      cnum = E_ArgAsInt(mo->state->args, 1, 0);

      if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
         return; // invalid

      damage = mo->counters[cnum];
      break;
   case 3: // use constant ("immediate operand" mode)
      damage = E_ArgAsInt(mo->state->args, 1, 0);
      break;
   }

   A_FaceTarget(mo);

   if(P_CheckMeleeRange(mo))
   {
      if(mo->state->misc2)
         S_StartSound(mo, mo->state->misc2);
      else
      {
         // check to see if args[2] is a valid sound
         sfxinfo_t *sfx = E_ArgAsSound(mo->state->args, 2);
         if(sfx)
            S_StartSfxInfo(mo, sfx, 127, ATTN_NORMAL, false, CHAN_AUTO);
      }

      P_DamageMobj(mo->target, mo, mo, damage, MOD_HIT);
   }
}

void A_PlaySound(Mobj *mo)
{
   S_StartSound(mo->state->misc2 ? NULL : mo, mo->state->misc1);
}

void A_RandomJump(Mobj *mo)
{
   // haleyjd 03/06/03: rewrote to be failsafe
   //         07/05/03: adjusted for EDF
   int statenum = mo->state->misc1;

   statenum = E_StateNumForDEHNum(statenum);
   if(statenum < 0)
      return;

   if(P_Random(pr_randomjump) < mo->state->misc2)
      P_SetMobjState(mo, statenum);
}

//
// This allows linedef effects to be activated inside deh frames.
//
void A_LineEffect(Mobj *mo)
{
   // haleyjd 05/02/04: bug fix:
   // This line can end up being referred to long after this
   // function returns, thus it must be made static or memory
   // corruption is possible.
   static line_t junk;

   if(!(mo->intflags & MIF_LINEDONE))                // Unless already used up
   {
      junk = *lines;                                 // Fake linedef set to 1st
      if((junk.special = (int16_t)mo->state->misc1))   // Linedef type
      {
         player_t player, *oldplayer = mo->player;   // Remember player status
         mo->player = &player;                       // Fake player
         player.health = 100;                        // Alive player
         junk.tag = (int16_t)mo->state->misc2;         // Sector tag for linedef
         if(!P_UseSpecialLine(mo, &junk, 0))         // Try using it
            P_CrossSpecialLine(&junk, 0, mo);        // Try crossing it
         if(!junk.special)                           // If type cleared,
            mo->intflags |= MIF_LINEDONE;            // no more for this thing
         mo->player = oldplayer;                     // Restore player status
      }
   }
}

//
// haleyjd: Start Eternity Engine action functions
//

//
// A_SetFlags
//
// A parameterized codepointer that turns on thing flags
//
// args[0] == 0, 1, 2, 3, 4 -- flags field to affect (0 == combined)
// args[1] == flags value to OR with thing flags
//
void A_SetFlags(Mobj *actor)
{
   int flagfield;
   int *flags;

   flagfield = E_ArgAsInt(actor->state->args, 0, 0);

   if(!(flags = E_ArgAsThingFlags(actor->state->args, 1)))
      return;
   
   switch(flagfield)
   {
   case 0:
      actor->flags  |= (unsigned int)flags[0];
      actor->flags2 |= (unsigned int)flags[1];
      actor->flags3 |= (unsigned int)flags[2];
      actor->flags4 |= (unsigned int)flags[3];
      break;
   case 1:
      actor->flags  |= (unsigned int)flags[0];
      break;
   case 2:
      actor->flags2 |= (unsigned int)flags[1];
      break;
   case 3:
      actor->flags3 |= (unsigned int)flags[2];
      break;
   case 4:
      actor->flags4 |= (unsigned int)flags[3];
      break;
   }
}

//
// A_UnSetFlags
//
// A parameterized codepointer that turns off thing flags
//
// args[0] == 0, 1, 2, 3, 4 -- flags field to affect (0 == combined)
// args[1] == flags value to inverse AND with thing flags
//
void A_UnSetFlags(Mobj *actor)
{
   int flagfield;
   int *flags;

   flagfield = E_ArgAsInt(actor->state->args, 0, 0);

   if(!(flags = E_ArgAsThingFlags(actor->state->args, 1)))
      return;

   switch(flagfield)
   {
   case 0:
      actor->flags  &= ~((unsigned int)flags[0]);
      actor->flags2 &= ~((unsigned int)flags[1]);
      actor->flags3 &= ~((unsigned int)flags[2]);
      actor->flags4 &= ~((unsigned int)flags[3]);
      break;
   case 1:
      actor->flags  &= ~((unsigned int)flags[0]);
      break;
   case 2:
      actor->flags2 &= ~((unsigned int)flags[1]);
      break;
   case 3:
      actor->flags3 &= ~((unsigned int)flags[2]);
      break;
   case 4:
      actor->flags4 &= ~((unsigned int)flags[3]);
      break;
   }
}

static const char *kwds_A_StartScript[] =
{
   "gamescript",  // 0
   "levelscript", // 1
   "acs"          // 2
};

static argkeywd_t sscriptkwds =
{
   kwds_A_StartScript,
   sizeof(kwds_A_StartScript) / sizeof(const char *)
};

//
// A_StartScript
//
// Parameterized codepointer for starting Small scripts
//
// args[0] - script number to start
// args[1] - select vm (0 == gamescript, 1 == levelscript, 2 == ACS levelscript)
// args[2-4] - parameters to script (must accept 3 params)
//
void A_StartScript(Mobj *actor)
{
   int scriptnum = E_ArgAsInt(actor->state->args, 0, 0);
   int selectvm  = E_ArgAsKwd(actor->state->args, 1, &sscriptkwds, 0);
   
   if(selectvm < 2)
   {
      /* nothing */ ;
   }
   else
   {
      int args[3] = { 0, 0, 0 };
      args[0] = E_ArgAsInt(actor->state->args, 2, 0);
      args[1] = E_ArgAsInt(actor->state->args, 3, 0);
      args[2] = E_ArgAsInt(actor->state->args, 4, 0);
      ACS_ExecuteScriptNumber(scriptnum, gamemap, 0, args, 3, NULL, NULL, 0);
   }
}

//
// A_FaceMoveDir
//
// Face a walking object in the direction it is moving.
// haleyjd TODO: this is not documented or available in BEX yet.
//
void A_FaceMoveDir(Mobj *actor)
{
   angle_t moveangles[NUMDIRS] = { 0, 32, 64, 96, 128, 160, 192, 224 };

   if(actor->movedir != DI_NODIR)
      actor->angle = moveangles[actor->movedir] << 24;
}

//
// A_GenRefire
//
// Generalized refire check pointer, requested by Kate.
//
// args[0] : state to branch to when ceasing to fire
// args[1] : random chance of still firing if target out of sight
//
void A_GenRefire(Mobj *actor)
{
   int statenum;
   int chance;

   statenum = E_ArgAsStateNum(actor->state->args, 0, actor);
   chance   = E_ArgAsInt(actor->state->args, 1, 0);

   A_FaceTarget(actor);

   // check for friends in the way
   if(actor->flags & MF_FRIEND && P_HitFriend(actor))
   {
      P_SetMobjState(actor, statenum);
      return;
   }

   // random chance of continuing to fire
   if(P_Random(pr_genrefire) < chance)
      return;

   if(!actor->target || actor->target->health <= 0 ||
      (actor->flags & actor->target->flags & MF_FRIEND) ||
      !P_CheckSight(actor, actor->target))
   {
      P_SetMobjState(actor, statenum);
   }
}

#define TRACEANGLE 0xc000000

//
// A_GenTracer
//
// Generic homing missile maintenance
//
void A_GenTracer(Mobj *actor)
{
   angle_t       exact;
   fixed_t       dist;
   fixed_t       slope;
   Mobj        *dest;
  
   // adjust direction
   dest = actor->tracer;
   
   if(!dest || dest->health <= 0)
      return;

   // change angle
   exact = P_PointToAngle(actor->x, actor->y, dest->x, dest->y);

   if(exact != actor->angle)
   {
      if(exact - actor->angle > 0x80000000)
      {
         actor->angle -= TRACEANGLE;
         if(exact - actor->angle < 0x80000000)
            actor->angle = exact;
      }
      else
      {
         actor->angle += TRACEANGLE;
         if(exact - actor->angle > 0x80000000)
            actor->angle = exact;
      }
   }

   exact = actor->angle>>ANGLETOFINESHIFT;
   actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
   actor->momy = FixedMul(actor->info->speed, finesine[exact]);

   // change slope
   dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
   
   dist = dist / actor->info->speed;

   if(dist < 1)
      dist = 1;

   slope = (dest->z + 40*FRACUNIT - actor->z) / dist;
   
   if(slope < actor->momz)
      actor->momz -= FRACUNIT/8;
   else
      actor->momz += FRACUNIT/8;
}

static const char *kwds_A_SetTics[] =
{
   "constant", //          0
   "counter",  //          1
};

static argkeywd_t settickwds =
{
   kwds_A_SetTics,
   sizeof(kwds_A_SetTics)/sizeof(const char *)
};

//
// A_SetTics
//
// Parameterized codepointer to set a thing's tics value.
// * args[0] : base amount
// * args[1] : randomizer modulus value (0 == not randomized)
// * args[2] : counter toggle
//
void A_SetTics(Mobj *actor)
{
   int baseamt = E_ArgAsInt(actor->state->args, 0, 0);
   int rnd     = E_ArgAsInt(actor->state->args, 1, 0);
   int counter = E_ArgAsKwd(actor->state->args, 2, &settickwds, 0);

   // if counter toggle is set, args[0] is a counter number
   if(counter)
   {
      if(baseamt < 0 || baseamt >= NUMMOBJCOUNTERS)
         return; // invalid
      baseamt = actor->counters[baseamt];
   }

   actor->tics = baseamt + (rnd ? P_Random(pr_settics) % rnd : 0);
}

static const char *kwds_A_MissileAttack[] =
{
   "normal",        //  0
   "homing",        //  1
};

static argkeywd_t missileatkkwds =
{
   kwds_A_MissileAttack,
   sizeof(kwds_A_MissileAttack) / sizeof(const char *)
};

//
// A_MissileAttack
//
// Parameterized missile firing for enemies.
// Arguments:
// * args[0] = type to fire
// * args[1] = whether or not to home on target
// * args[2] = amount to add to standard missile z firing height
// * args[3] = amount to add to actor angle
// * args[4] = optional state to enter for melee attack
//
void A_MissileAttack(Mobj *actor)
{
   int type, a;
   fixed_t z, momz;
   bool homing;
   angle_t ang;
   Mobj *mo;
   int statenum;
   bool hastarget = true;

   if(!actor->target || actor->target->health <= 0)
      hastarget = false;

   type     = E_ArgAsThingNum(actor->state->args,      0);   
   homing   = !!E_ArgAsKwd(actor->state->args,  1, &missileatkkwds, 0);   
   z        = (fixed_t)(E_ArgAsInt(actor->state->args, 2, 0) * FRACUNIT);
   a        = E_ArgAsInt(actor->state->args,           3, 0);
   statenum = E_ArgAsStateNumG0(actor->state->args,    4, actor);

   if(hastarget)
   {
      A_FaceTarget(actor);

      if(statenum >= 0 && statenum < NUMSTATES)
      {
         if(P_CheckMeleeRange(actor))
         {
            P_SetMobjState(actor, statenum);
            return;
         }
      }
   }

   // adjust angle -> BAM (must adjust negative angles too)
   while(a >= 360)
      a -= 360;
   while(a < 0)
      a += 360;

   ang = (angle_t)(((uint64_t)a << 32) / 360);

   // adjust z coordinate
   z = actor->z + DEFAULTMISSILEZ + z;

   if(!hastarget)
   {
      P_SpawnMissileAngle(actor, type, actor->angle + ang, 0, z);
      return;
   }
   
   if(!a)
      mo = P_SpawnMissile(actor, actor->target, type, z);
   else
   {
      // calculate z momentum
      Mobj *target = actor->target;

      momz = P_MissileMomz(target->x - actor->x,
                           target->y - actor->y,
                           target->z - actor->z,
                           mobjinfo[type]->speed);

      mo = P_SpawnMissileAngle(actor, type, actor->angle + ang, momz, z);
   }

   if(homing)
      P_SetTarget<Mobj>(&mo->tracer, actor->target);
}

//
// A_MissileSpread
//
// Fires an angular spread of missiles.
// Arguments:
// * args[0] = type to fire
// * args[1] = number of missiles to fire
// * args[2] = amount to add to standard missile z firing height
// * args[3] = total angular sweep
// * args[4] = optional state to enter for melee attack
//
void A_MissileSpread(Mobj *actor)
{
   int type, num, a, i;
   fixed_t z, momz;
   angle_t angsweep, ang, astep;
   int statenum;

   if(!actor->target)
      return;

   type     = E_ArgAsThingNum(actor->state->args,      0);
   num      = E_ArgAsInt(actor->state->args,           1, 0);
   z        = (fixed_t)(E_ArgAsInt(actor->state->args, 2, 0) * FRACUNIT);
   a        = E_ArgAsInt(actor->state->args,           3, 0);
   statenum = E_ArgAsStateNumG0(actor->state->args,    4, actor);

   if(num < 2)
      return;

   A_FaceTarget(actor);

   if(statenum >= 0 && statenum < NUMSTATES)
   {
      if(P_CheckMeleeRange(actor))
      {
         P_SetMobjState(actor, statenum);
         return;
      }
   }

   // adjust angle -> BAM (must adjust negative angles too)
   while(a >= 360)
      a -= 360;
   while(a < 0)
      a += 360;

   angsweep = (angle_t)(((uint64_t)a << 32) / 360);

   // adjust z coordinate
   z = actor->z + DEFAULTMISSILEZ + z;

   ang = actor->angle - angsweep / 2;
   astep = angsweep / (num - 1);

   for(i = 0; i < num; ++i)
   {
      // calculate z momentum
#ifdef R_LINKEDPORTALS
      momz = P_MissileMomz(getTargetX(actor) - actor->x,
                           getTargetY(actor) - actor->y,
                           getTargetZ(actor) - actor->z,
#else
      momz = P_MissileMomz(actor->target->x - actor->x,
                           actor->target->y - actor->y,
                           actor->target->z - actor->z,
#endif
                           mobjinfo[type]->speed);

      P_SpawnMissileAngle(actor, type, ang, momz, z);

      ang += astep;
   }
}

// old keywords - deprecated
static const char *kwds_A_BulletAttack[] =
{
   "{DUMMY}",
   "ba_always",   // 1
   "ba_never",    // 2
   "ba_ssg",      // 3
   "ba_monster",  // 4
};

static argkeywd_t bulletkwdsold =
{
   kwds_A_BulletAttack,
   sizeof(kwds_A_BulletAttack) / sizeof(const char *)
};

// new keywords - preferred
static const char *kwds_A_BulletAttack2[] =
{
   "{DUMMY}",
   "always",   // 1
   "never",    // 2
   "ssg",      // 3
   "monster",  // 4
};

static argkeywd_t bulletkwdsnew =
{
   kwds_A_BulletAttack2,
   sizeof(kwds_A_BulletAttack2) / sizeof(const char *)
};

//
// A_BulletAttack
//
// A parameterized monster bullet code pointer
// Parameters:
// args[0] : sound (dehacked num)
// args[1] : accuracy (always, never, ssg, monster)
// args[2] : number of bullets to fire
// args[3] : damage factor of bullets
// args[4] : damage modulus of bullets
//
void A_BulletAttack(Mobj *actor)
{
   int i, accurate, numbullets, damage, dmgmod, slope;
   sfxinfo_t *sfx;

   if(!actor->target)
      return;

   sfx        = E_ArgAsSound(actor->state->args, 0);
   numbullets = E_ArgAsInt(actor->state->args,   2, 0);
   damage     = E_ArgAsInt(actor->state->args,   3, 0);
   dmgmod     = E_ArgAsInt(actor->state->args,   4, 0);

   // handle accuracy

   // try old keywords first
   accurate = E_ArgAsKwd(actor->state->args, 1, &bulletkwdsold, -1);

   // try new keywords second
   if(accurate == -1)
   {
      E_ResetArgEval(actor->state->args, 1);
      accurate = E_ArgAsKwd(actor->state->args, 1, &bulletkwdsnew, 0);
   }

   if(!accurate)
      accurate = 1;

   if(dmgmod < 1)
      dmgmod = 1;
   else if(dmgmod > 256)
      dmgmod = 256;

   A_FaceTarget(actor);
   S_StartSfxInfo(actor, sfx, 127, ATTN_NORMAL, false, CHAN_AUTO);

   slope = P_AimLineAttack(actor, actor->angle, MISSILERANGE, 0);

   // loop on numbullets
   for(i = 0; i < numbullets; i++)
   {
      int dmg = damage * (P_Random(pr_monbullets)%dmgmod + 1);
      angle_t angle = actor->angle;
      
      if(accurate <= 2 || accurate == 4)
      {
         // if never accurate or monster accurate,
         // add some to the angle
         if(accurate == 2 || accurate == 4)
         {
            int aimshift = ((accurate == 4) ? 20 : 18);
            angle += P_SubRandom(pr_monmisfire) << aimshift;
         }

         P_LineAttack(actor, angle, MISSILERANGE, slope, dmg);
      }
      else if(accurate == 3) // ssg spread
      {
         angle += P_SubRandom(pr_monmisfire) << 19;         
         slope += P_SubRandom(pr_monmisfire) << 5;

         P_LineAttack(actor, angle, MISSILERANGE, slope, dmg);
      }
   }
}

static const char *kwds_A_ThingSummon_KR[] =
{
   "kill",           //  0
   "remove",         //  1
};

static argkeywd_t killremovekwds =
{
   kwds_A_ThingSummon_KR,
   2
};

static const char *kwds_A_ThingSummon_MC[] =
{
   "normal",         //  0
   "makechild",      //  1
};

static argkeywd_t makechildkwds =
{
   kwds_A_ThingSummon_MC,
   2
};

void A_ThingSummon(Mobj *actor)
{
   fixed_t x, y, z;
   Mobj  *newmobj;
   angle_t an;
   int     type, prestep, deltaz, kill_or_remove, make_child;

   type    = E_ArgAsThingNum(actor->state->args, 0);
   prestep = E_ArgAsInt(actor->state->args,      1, 0) << FRACBITS;
   deltaz  = E_ArgAsInt(actor->state->args,      2, 0) << FRACBITS;

   kill_or_remove = !!E_ArgAsKwd(actor->state->args, 3, &killremovekwds, 0);
   make_child     = !!E_ArgAsKwd(actor->state->args, 4, &makechildkwds, 0);
   
   // good old-fashioned pain elemental style spawning
   
   an = actor->angle >> ANGLETOFINESHIFT;
   
   prestep = prestep + 3*(actor->info->radius + mobjinfo[type]->radius)/2;
   
   x = actor->x + FixedMul(prestep, finecosine[an]);
   y = actor->y + FixedMul(prestep, finesine[an]);
   z = actor->z + deltaz;

   // Check whether the thing is being spawned through a 1-sided
   // wall or an impassible line, or a "monsters can't cross" line.
   // If it is, then we don't allow the spawn.
   
   if(Check_Sides(actor, x, y))
      return;

   newmobj = P_SpawnMobj(x, y, z, type);
   
   // Check to see if the new thing's z value is above the
   // ceiling of its new sector, or below the floor. If so, kill it.

   if((newmobj->z >
      (newmobj->subsector->sector->ceilingheight - newmobj->height)) ||
      (newmobj->z < newmobj->subsector->sector->floorheight))
   {
      // kill it immediately
      switch(kill_or_remove)
      {
      case 0:
         A_Die(newmobj);
         break;
      case 1:
         newmobj->removeThinker();
         break;
      }
      return;
   }                                                         
   
   // spawn thing with same friendliness
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

   // killough 8/29/98: add to appropriate thread
   newmobj->updateThinker();
   
   // Check for movements.
   // killough 3/15/98: don't jump over dropoffs:

   if(!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
   {
      // kill it immediately
      switch(kill_or_remove)
      {
      case 0:
         A_Die(newmobj);
         break;
      case 1:
         newmobj->removeThinker();
         break;
      }
      return;
   }

   // give same target
   P_SetTarget<Mobj>(&newmobj->target, actor->target);

   // set child properties
   if(make_child)
   {
      P_SetTarget<Mobj>(&newmobj->tracer, actor);
      newmobj->intflags |= MIF_ISCHILD;
   }
}

void A_KillChildren(Mobj *actor)
{
   Thinker *th;
   int kill_or_remove = !!E_ArgAsKwd(actor->state->args, 0, &killremovekwds, 0);

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;

      if(!(mo = thinker_cast<Mobj *>(th)))
         continue;

      if(mo->intflags & MIF_ISCHILD && mo->tracer == actor)
      {
         switch(kill_or_remove)
         {
         case 0:
            A_Die(mo);
            break;
         case 1:
            mo->removeThinker();
            break;
         }
      }
   }
}

//
// A_AproxDistance
//
// Parameterized pointer, returns the approximate distance between
// a thing and its target in the indicated counter.
// * args[0] == destination counter
//
void A_AproxDistance(Mobj *actor)
{
   int *dest = NULL;
   fixed_t distance;
   int cnum;

   cnum = E_ArgAsInt(actor->state->args, 0, 0);

   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   dest = &(actor->counters[cnum]);

   if(!actor->target)
   {
      *dest = -1;
      return;
   }
   
#ifdef R_LINKEDPORTALS
   distance = P_AproxDistance(actor->x - getTargetX(actor), 
                              actor->y - getTargetY(actor));
#else   
   distance = P_AproxDistance(actor->x - actor->target->x, 
                              actor->y - actor->target->y);
#endif

   *dest = distance >> FRACBITS;
}

static const char *kwds_A_ShowMessage[] =
{
   "msg_console",          //  0
   "msg_normal",           //  1
   "msg_center",           //  2
};

static argkeywd_t messagekwds =
{
   kwds_A_ShowMessage,
   sizeof(kwds_A_ShowMessage) / sizeof(const char *)
};

//
// A_ShowMessage
//
// A codepointer that can display EDF strings to the players as
// messages.
// Arguments:
// args[0] = EDF message number
// args[1] = message type
//
void A_ShowMessage(Mobj *actor)
{
   edf_string_t *msg;
   int type;

   // find the message
   if(!(msg = E_ArgAsEDFString(actor->state->args, 0)))
      return;

   type = E_ArgAsKwd(actor->state->args, 1, &messagekwds, 0);

   switch(type)
   {
   case 0:
      C_Printf("%s", msg->string);
      break;
   case 1:
      doom_printf("%s", msg->string);
      break;
   case 2:
      HU_CenterMessage(msg->string);
      break;
   }
}

//
// A_AmbientThinker
//
// haleyjd 05/31/06: Ambient sound driver function
//
void A_AmbientThinker(Mobj *mo)
{
   EAmbience_t *amb = E_AmbienceForNum(mo->args[0]);
   bool loop = false;

   // nothing to play?
   if(!amb || !amb->sound)
      return;

   // run thinker actions for corresponding ambience type
   switch(amb->type)
   {
   case E_AMBIENCE_CONTINUOUS:
      if(S_CheckSoundPlaying(mo, amb->sound)) // not time yet?
         return;
      loop = true;
      break;
   case E_AMBIENCE_PERIODIC:
      if(mo->counters[0]-- >= 0) // not time yet?
         return;
      mo->counters[0] = amb->period; // reset sound period
      break;
   case E_AMBIENCE_RANDOM:
      if(mo->counters[0]-- >= 0) // not time yet?
         return;
      mo->counters[0] = (int)M_RangeRandomEx(amb->minperiod, amb->maxperiod);
      break;
   default: // ???
      return;
   }

   // time to play the sound
   S_StartSfxInfo(mo, amb->sound, amb->volume, amb->attenuation, loop, CHAN_AUTO);
}

void A_SteamSpawn(Mobj *mo)
{
   Mobj *steamthing;
   int thingtype;
   int vrange, hrange;
   int tvangle, thangle;
   angle_t vangle, hangle;
   fixed_t speed, angularspeed;
   
   // Get the thingtype of the thing we're spewing (a steam cloud for example)
   thingtype = E_ArgAsThingNum(mo->state->args, 0);
   
   // And the speed to fire it out at
   speed = (fixed_t)(E_ArgAsInt(mo->state->args, 4, 0) << FRACBITS);

   // Make our angles byteangles
   hangle = (mo->angle / (ANG90/64));
   thangle = hangle;
   tvangle = (E_ArgAsInt(mo->state->args, 2, 0) * 256) / 360;
   // As well as the spread ranges
   hrange = (E_ArgAsInt(mo->state->args, 1, 0) * 256) / 360;
   vrange = (E_ArgAsInt(mo->state->args, 3, 0) * 256) / 360;
   
   // Get the angles we'll be firing the things in, factoring in 
   // where within the range it will lie
   thangle += (hrange >> 1) - (P_Random(pr_steamspawn) * hrange / 255);
   tvangle += (vrange >> 1) - (P_Random(pr_steamspawn) * vrange / 255);
   
   while(thangle >= 256)
      thangle -= 256;
   while(thangle < 0)
      thangle += 256;
         
   while(tvangle >= 256)
      tvangle -= 256;
   while(tvangle < 0)
      tvangle += 256;
   
   // Make angles angle_t
   hangle = ((unsigned int)thangle * (ANG90/64));
   vangle = ((unsigned int)tvangle * (ANG90/64));

   // Spawn thing
   steamthing = P_SpawnMobj(mo->x, mo->y, mo->z, thingtype);
   
   // Give it some momentum
   // angular speed is the hypotenuse of the x and y speeds
   angularspeed = FixedMul(speed, finecosine[vangle >> ANGLETOFINESHIFT]);
   steamthing->momx = FixedMul(angularspeed, finecosine[hangle >> ANGLETOFINESHIFT]);
   steamthing->momy = FixedMul(angularspeed, finesine[hangle >> ANGLETOFINESHIFT]);
   steamthing->momz = FixedMul(speed, finesine[vangle >> ANGLETOFINESHIFT]);
}

//
// A_TargetJump
//
// Parameterized codepointer for branching based on whether a
// thing's target is valid and alive.
//
// args[0] : state number
//
void A_TargetJump(Mobj *mo)
{
   int statenum;
   
   if((statenum = E_ArgAsStateNumNI(mo->state->args, 0, mo)) < 0)
      return;
   
   // 1) must be valid
   // 2) must be alive
   // 3) if a super friend, target cannot be a friend
   if(mo->target && mo->target->health > 0 &&
      !((mo->flags & mo->target->flags & MF_FRIEND) && 
        mo->flags3 & MF3_SUPERFRIEND))
      P_SetMobjState(mo, statenum);
}

//
// A_EjectCasing
//
// A pointer meant for spawning bullet casing objects.
// Parameters:
//   args[0] : distance in front in 16th's of a unit
//   args[1] : distance from middle in 16th's of a unit (negative = left)
//   args[2] : z height relative to player's viewpoint in 16th's of a unit
//   args[3] : thingtype to toss
//
void A_EjectCasing(Mobj *actor)
{
   angle_t angle = actor->angle;
   fixed_t x, y, z;
   fixed_t frontdist;
   int     frontdisti;
   fixed_t sidedist;
   fixed_t zheight;
   int     thingtype;
   Mobj *mo;

   frontdisti = E_ArgAsInt(actor->state->args, 0, 0);
   
   frontdist  = frontdisti * FRACUNIT / 16;
   sidedist   = E_ArgAsInt(actor->state->args, 1, 0) * FRACUNIT / 16;
   zheight    = E_ArgAsInt(actor->state->args, 2, 0) * FRACUNIT / 16;

   // account for mlook - EXPERIMENTAL
   if(actor->player)
   {
      int pitch = actor->player->pitch;
            
      z = actor->z + actor->player->viewheight + zheight;
      
      // modify height according to pitch - hack warning.
      z -= (pitch / ANGLE_1) * ((10 * frontdisti / 256) * FRACUNIT / 32);
   }
   else
      z = actor->z + zheight;

   x = actor->x + FixedMul(frontdist, finecosine[angle>>ANGLETOFINESHIFT]);
   y = actor->y + FixedMul(frontdist, finesine[angle>>ANGLETOFINESHIFT]);

   // adjust x/y along a vector orthogonal to the source object's angle
   angle = angle - ANG90;

   x += FixedMul(sidedist, finecosine[angle>>ANGLETOFINESHIFT]);
   y += FixedMul(sidedist, finesine[angle>>ANGLETOFINESHIFT]);

   thingtype = E_ArgAsThingNum(actor->state->args, 3);

   mo = P_SpawnMobj(x, y, z, thingtype);

   mo->angle = sidedist >= 0 ? angle : angle + ANG180;
}

//
// A_CasingThrust
//
// A casing-specific thrust function.
//    args[0] : lateral force in 16ths of a unit
//    args[1] : z force in 16ths of a unit
//
void A_CasingThrust(Mobj *actor)
{
   fixed_t moml, momz;

   moml = E_ArgAsInt(actor->state->args, 0, 0) * FRACUNIT / 16;
   momz = E_ArgAsInt(actor->state->args, 1, 0) * FRACUNIT / 16;
   
   actor->momx = FixedMul(moml, finecosine[actor->angle>>ANGLETOFINESHIFT]);
   actor->momy = FixedMul(moml, finesine[actor->angle>>ANGLETOFINESHIFT]);
   
   // randomize
   actor->momx += P_SubRandom(pr_casing) << 8;
   actor->momy += P_SubRandom(pr_casing) << 8;
   actor->momz = momz + (P_SubRandom(pr_casing) << 8);
}

// EOF

