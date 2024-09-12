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
//      Action Pointer Functions
//      that are associated with states/frames.
//
//      Generalized/Parameterized action functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_args.h"
#include "a_common.h"
#include "acs_intr.h"
#include "c_io.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_mod.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_string.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "e_weapons.h"
#include "ev_specials.h"
#include "hu_stuff.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_portalcross.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"

//
// [XA] 03/02/20: Common A_Mushroom* stuff.
//
void P_Mushroom(Mobj *actor, const int ShotType, const int n, const fixed_t misc1, const fixed_t misc2, const int damage, const int radius)
{
   int i, j;

   if(actor == nullptr) {
      return;
   }

   // make normal explosion
   P_RadiusAttack(actor, actor->target, damage, radius, actor->info->mod, 0);
   E_ExplosionHitWater(actor, radius);

   // launch mushroom cloud
   for(i = -n; i <= n; i += 8)
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
                                     actor->z + actor->info->missileheight,
                                     x, y, z);
         
         mo->momx = FixedMul(mo->momx, misc2);
         mo->momy = FixedMul(mo->momy, misc2);         // Slow down a bit
         mo->momz = FixedMul(mo->momz, misc2);
         mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
      }
   }
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//
void A_Mushroom(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int n = actor->damage;
   int ShotType;
   
   // Mushroom parameters are part of code pointer's state
   fixed_t misc1 = 
      actor->state->misc1 ? actor->state->misc1 : FRACUNIT*4;
   fixed_t misc2 = 
      actor->state->misc2 ? actor->state->misc2 : FRACUNIT/2;

   // haleyjd: extended parameter support requested by Mordeth:
   // allow specification of thing type in args[0]

   ShotType = E_ArgAsThingNumG0(args, 0);

   if(ShotType < 0/* || ShotType == -1*/)
      ShotType = E_SafeThingType(MT_FATSHOT);

   P_Mushroom(actor, ShotType, n, misc1, misc2, 128, 128);
}

//
// A_MushroomEx
//
// Extended A_Mushroom, with proper args for everything.
//
// args[0] -- thing type (DeHackEd num)
// args[1] -- thing count
// args[2] -- vertical range
// args[3] -- horizontal range
// args[4] -- splash damage
// args[5] -- splash radius
//
void A_MushroomEx(actionargs_t *actionargs)
{
   Mobj     *actor = actionargs->actor;
   arglist_t *args = actionargs->args;
   int thingtype;
   int thingcount;
   fixed_t vrange, hrange;
   int damage, radius;

   thingtype  = E_ArgAsThingNumG0(args, 0);
   thingcount = E_ArgAsInt       (args, 1, actor->damage);
   vrange     = E_ArgAsFixed     (args, 2, 4 * FRACUNIT);
   hrange     = E_ArgAsFixed     (args, 3, 1 * FRACUNIT / 2);
   damage     = E_ArgAsInt       (args, 4, 128);
   radius     = E_ArgAsInt       (args, 5, 128);

   P_Mushroom(actor, thingtype, thingcount, vrange, hrange, damage, radius);
}

//
// killough 11/98
//
// The following were inspired by Len Pitre
//
// A small set of highly-sought-after code pointers
//

void A_Spawn(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   if(mo->state->misc1)
   {
      Mobj *newmobj;

      // haleyjd 03/06/03 -- added error check
      //         07/05/03 -- adjusted for EDF
      int thingtype = E_SafeThingType(mo->state->misc1);
      
      newmobj = 
         P_SpawnMobj(mo->x, mo->y, 
                     (mo->state->misc2 << FRACBITS) + mo->z,
                     thingtype);
      if(newmobj)
         P_transferFriendship(*newmobj, *mo);
   }
}

static const char *kwds_A_Turn[] =
{
   "usemisc1",
   "usecounterasdeg",
   "usecounterasbam",
   "useconstant"
};

static argkeywd_t turnkwds =
{
   kwds_A_Turn,
   sizeof(kwds_A_Turn) / sizeof(const char *)
};

//
// Parameterized turn.
// * misc1 == constant turn amount
//
// * args[0] == special mode select
//              * 0 == compatibility (use misc1 like normal)
//              * 1 == use counter specified in args[1] as degrees
//              * 2 == use counter specified in args[1] as angle_t
//              * 3 == use constant value in args[1]
// * args[1] == counter number for mode 2; constant for mode 3
//
void A_Turn(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int cnum;
   int mode;
   angle_t angle;

   mode = E_ArgAsKwd(args, 0, &turnkwds, 0);

   switch(mode)
   {
   default:
   case 0: // default, compatibility mode
      angle = static_cast<angle_t>((static_cast<uint64_t>(mo->state->misc1) << 32) / 360);
      break;
   case 1: // use a counter as degrees
      cnum = E_ArgAsInt(args, 1, 0);

      if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
         return; // invalid

      if(const int degs = mo->counters[cnum]; degs % 45)
         angle = static_cast<angle_t>(ANGLE_1 * degs);
      else
         angle = static_cast<angle_t>(ANG45 * (degs / 45));
      break;
   case 2: // use a counter as angle_t
      cnum = E_ArgAsInt(args, 1, 0);

      if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
         return; // invalid

      angle = static_cast<angle_t>(mo->counters[cnum]);
      break;
   case 3: // use constant ("immediate operand" mode)
      angle = E_ArgAsAngle(args, 1, 0);
      break;
   }

   mo->angle += angle;
}

//
// Same as A_Turn but updates momx and momy.
//
void A_TurnProjectile(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   A_Turn(actionargs);

   const angle_t an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void A_Face(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
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
// * args[3] == damagetype, either string or int
//
void A_Scratch(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int damage, cnum;
   int mode;

   // haleyjd: demystified
   if(!mo->target)
      return;

   mode = E_ArgAsKwd(args, 0, &scratchkwds, 0);

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
      cnum = E_ArgAsInt(args, 1, 0);

      if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
         return; // invalid

      damage = mo->counters[cnum];
      break;
   case 3: // use constant ("immediate operand" mode)
      damage = E_ArgAsInt(args, 1, 0);
      break;
   }

   A_FaceTarget(actionargs);

   if(P_CheckMeleeRange(mo))
   {
      if(mo->state->misc2)
         S_StartSound(mo, mo->state->misc2);
      else
      {
         soundparams_t params;

         // check to see if args[2] is a valid sound
         if((params.sfx = E_ArgAsSound(args, 2)))
            S_StartSfxInfo(params.setNormalDefaults(mo));
      }

      // Set mod to 4th arg
      const int mod = E_ArgAsDamageType(args, 3, MOD_HIT)->num;
      P_DamageMobj(mo->target, mo, mo, damage, mod);
   }
}

void A_PlaySound(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   S_StartSound(mo->state->misc2 ? nullptr : mo, mo->state->misc1);
}

void A_RandomJump(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

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
void A_LineEffect(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   // haleyjd 05/02/04: bug fix:
   // This line can end up being referred to long after this
   // function returns, thus it must be made static or memory
   // corruption is possible.
   I_Assert(numlinesPlusExtra > numlines, "Must have one extra line\n");

   // ioanch: use the footer "shadow" linedef from the lines list. This will allow any thinkers who
   // adopt this linedef to be serialized properly when saving, so it won't crash after loading.
   line_t &junk = lines[numlines];

   if(!(mo->intflags & MIF_LINEDONE))                // Unless already used up
   {
      junk = *lines;                                 // Fake linedef set to 1st
      if((junk.special = mo->state->misc1))          // Linedef type
      {
         // ioanch: remove the fake player, it was causing undefined behaviour and crashes.
         // Instead, mark the activation instance as "byCodepointer", which will allow the monster
         // to trigger linedefs and open locked doors, just like in MBF.

         junk.args[0] = junk.tag = mo->state->misc2;            // Sector tag for linedef
         // Only try use or cross
         if(!EV_ActivateSpecialLineWithSpac(&junk, 0, mo, nullptr, SPAC_USE, true))
            EV_ActivateSpecialLineWithSpac(&junk, 0, mo, nullptr, SPAC_CROSS, true);
         if(!junk.special)                           // If type cleared,
            mo->intflags |= MIF_LINEDONE;            // no more for this thing
      }
   }
}

//
// haleyjd: Start Eternity Engine action functions
//

//
// A_SpawnAbove
//
// Parameterized pointer to spawn a solid object above the one
// calling the pointer.
//
// args[0] -- thing type (DeHackEd num)
// args[1] -- state number (< 0 == no state transition)
// args[2] -- amount to add to z coordinate
//
void A_SpawnAbove(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int thingtype;
   int statenum;
   fixed_t zamt;
   Mobj *mo;

   thingtype = E_ArgAsThingNum(args, 0);
   statenum  = E_ArgAsStateNumG0(args, 1, actor);
   zamt      = (fixed_t)(E_ArgAsInt(args, 2, 0) * FRACUNIT);

   mo = P_SpawnMobj(actor->x, actor->y, actor->z + zamt, thingtype);

   if(statenum >= 0 && statenum < NUMSTATES)
      P_SetMobjState(mo, statenum);
}

//
// A_SpawnGlitter
//
// Parameterized code pointer to spawn inert objects with some
// positive z momentum
//
// Parameters:
// args[0] - object type (use DeHackEd number)
// args[1] - z momentum (scaled by FRACUNIT/8)
//
void A_SpawnGlitter(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   Mobj      *glitter;
   int        glitterType;
   fixed_t    initMomentum;
   fixed_t    x, y, z;

   glitterType  = E_ArgAsThingNum(args, 0);
   initMomentum = (fixed_t)(E_ArgAsInt(args, 1, 0) * FRACUNIT / 8);

   // special defaults

   // default momentum of zero == 1/4 unit per tic
   if(!initMomentum)
      initMomentum = FRACUNIT >> 2;

   // randomize spawning coordinates within a 32-unit square
   // ioanch 20160107: correctly spawn behind line portal
   fixed_t dx = ((P_Random(pr_tglit) & 31) - 16) * FRACUNIT;
   fixed_t dy = ((P_Random(pr_tglit) & 31) - 16) * FRACUNIT;
   v2fixed_t pos = P_LinePortalCrossing(*actor, dx, dy);
   
   x = pos.x;
   y = pos.y;

   z = actor->zref.floor;

   glitter = P_SpawnMobj(x, y, z, glitterType);

   // give it some upward momentum
   glitter->momz = initMomentum;
}

enum spawnex_flags : unsigned int
{
   SPAWNEX_ABSOLUTEANGLE    = 0x00000001,
   SPAWNEX_ABSOLUTEVELOCITY = 0x00000002,
   SPAWNEX_ABSOLUTEPOSITION = 0x00000004,
   SPAWNEX_CHECKPOSITION    = 0x00000008
};

static dehflags_t spawnex_flaglist[] =
{
   { "normal",           0x00000000               },
   { "absoluteangle",    SPAWNEX_ABSOLUTEANGLE    },
   { "absolutevelocity", SPAWNEX_ABSOLUTEVELOCITY },
   { "absoluteposition", SPAWNEX_ABSOLUTEPOSITION },
   { "checkposition"   , SPAWNEX_CHECKPOSITION    },
   { nullptr,            0 }
};

static dehflagset_t spawnex_flagset =
{
   spawnex_flaglist, // flaglist
   0,                // mode
};

//
// A_SpawnEx
//
// Super-flexible parameterized pointer to spawn an object
// with varying position, velocity, and all that jazz.
//
// args[0] -- thing type (DeHackEd num)
// args[1] -- flags (see above)
// args[2] -- x-offset (forwards/backwards)
// args[3] -- y-offset (right/left)
// args[4] -- z-offset (up/down)
// args[5] -- x-velocity
// args[6] -- y-velocity
// args[7] -- z-velocity
// args[8] -- angle
// args[9] -- chance (out of 256) for the object to spawn; default is 256.
//
void A_SpawnEx(actionargs_t *actionargs)
{
   Mobj     *actor = actionargs->actor;
   arglist_t *args = actionargs->args;
   int thingtype;
   unsigned int flags;
   angle_t angle;
   fixed_t xpos, ypos, zpos;
   fixed_t xvel, yvel, zvel;
   v2fixed_t tempvec;
   int spawnchance;
   Mobj *mo;

   // [XA] check spawnchance first since there's no point in
   // even grabbing the rest of the args if we're doing nothing.
   spawnchance = E_ArgAsInt(args, 9, 256);
   if(P_Random(pr_spawnexchance) >= spawnchance)
      return; // look, ma, it's nothing!

   thingtype = E_ArgAsThingNumG0(args, 0);
   flags = E_ArgAsFlags(args, 1, &spawnex_flagset);
   xpos = E_ArgAsFixed(args, 2, 0);
   ypos = E_ArgAsFixed(args, 3, 0);
   zpos = E_ArgAsFixed(args, 4, 0);
   xvel = E_ArgAsFixed(args, 5, 0);
   yvel = E_ArgAsFixed(args, 6, 0);
   zvel = E_ArgAsFixed(args, 7, 0);
   angle = E_ArgAsAngle(args, 8, 0);

   if(!(flags & SPAWNEX_ABSOLUTEANGLE))
      angle += actor->angle;
   P_RotatePoint(xpos, ypos, angle);

   if(!(flags & SPAWNEX_ABSOLUTEPOSITION)) {
      tempvec = P_LinePortalCrossing(*actor, xpos, ypos);
      xpos = tempvec.x;
      ypos = tempvec.y;
      zpos += actor->z;
   }

   if(!(flags & SPAWNEX_ABSOLUTEVELOCITY))
      P_RotatePoint(xvel, yvel, angle);

   mo = P_SpawnMobj(xpos, ypos, zpos, thingtype);
   if(mo == nullptr)
      return;

   if((flags & SPAWNEX_CHECKPOSITION) && !P_CheckPositionExt(mo, mo->x, mo->y, mo->z))
      mo->remove();
   else
   {
      P_transferFriendship(*mo, *actor);
      mo->angle = angle;
      mo->momx = xvel;
      mo->momy = yvel;
      mo->momz = zvel;

      // TODO: Flag to make it set tracer?
      // If we're spawning a projectile then we want to set its target as its owner
      if(mo->flags & MF_MISSILE)
      {
         // If the spawner is (or spawned as) a projectile then set target as spawner's owner (if it exists)
         if(((actor->flags & MF_MISSILE) || actor->info->flags & MF_MISSILE) && actor->target)
            P_SetTarget<Mobj>(&mo->target, actor->target);
         else
            P_SetTarget<Mobj>(&mo->target, actor);
      }
   }
}

//
// A_SetFlags
//
// A parameterized codepointer that turns on thing flags
//
// args[0] == 0, 1, 2, 3, 4, 5 -- flags field to affect (0 == combined)
// args[1] == flags value to OR with thing flags
//
void A_SetFlags(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int flagfield;
   unsigned int *flags;

   flagfield = E_ArgAsInt(args, 0, 0);

   if(!(flags = E_ArgAsThingFlags(args, 1)))
      return;
   
   switch(flagfield)
   {
   case 0:
      actor->flags  |= (unsigned int)flags[0];
      actor->flags2 |= (unsigned int)flags[1];
      actor->flags3 |= (unsigned int)flags[2];
      actor->flags4 |= (unsigned int)flags[3];
      actor->flags5 |= (unsigned int)flags[4];
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
   case 5:
      actor->flags5 |= (unsigned int)flags[4];
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
void A_UnSetFlags(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int flagfield;
   unsigned int *flags;

   flagfield = E_ArgAsInt(args, 0, 0);

   if(!(flags = E_ArgAsThingFlags(args, 1)))
      return;

   switch(flagfield)
   {
   case 0:
      actor->flags  &= ~flags[0];
      actor->flags2 &= ~flags[1];
      actor->flags3 &= ~flags[2];
      actor->flags4 &= ~flags[3];
      actor->flags5 &= ~flags[4];
      break;
   case 1:
      actor->flags  &= ~flags[0];
      break;
   case 2:
      actor->flags2 &= ~flags[1];
      break;
   case 3:
      actor->flags3 &= ~flags[2];
      break;
   case 4:
      actor->flags4 &= ~flags[3];
      break;
   case 5:
      actor->flags5 &= ~flags[4];
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
// args[2-MAX] - parameters to script (must accept 3 params)
//
void A_StartScript(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int scriptnum = E_ArgAsInt(args, 0, 0);
   int selectvm  = E_ArgAsKwd(args, 1, &sscriptkwds, 0);
   
   if(selectvm < 2)
   {
      /* nothing */ ;
   }
   else
   {
      int argc = E_GetArgCount(args);

      if(argc > 2)
      {
         uint32_t argv[EMAXARGS - 2];
         argc -= 2;

         for(int i = 0; i < argc; ++i)
             argv[i] = E_ArgAsInt(args, i + 2, 0);

         ACS_ExecuteScriptIResult(scriptnum, argv, argc, actor, nullptr, 0, nullptr);
      }
      else
      {
         ACS_ExecuteScriptIResult(scriptnum, nullptr, 0, actor, nullptr, 0, nullptr);
      }
   }
}

//
// A_StartScriptNamed
//
// Same as A_StartScript, but for named scripts.
//
void A_StartScriptNamed(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   const char *scriptname = E_ArgAsString(args, 0, "");
   int selectvm = E_ArgAsKwd(args, 1, &sscriptkwds, 0);

   if(selectvm < 2)
   {
      /* nothing */ ;
   }
   else
   {
      int argc = E_GetArgCount(args);

      if(argc > 2)
      {
         uint32_t argv[EMAXARGS - 2];
         argc -= 2;

         for(int i = 0; i < argc; ++i)
             argv[i] = E_ArgAsInt(args, i + 2, 0);

         ACS_ExecuteScriptSResult(scriptname, argv, argc, actor, nullptr, 0, nullptr);
      }
      else
      {
         ACS_ExecuteScriptSResult(scriptname, nullptr, 0, actor, nullptr, 0, nullptr);
      }
   }
}

//
// A_FaceMoveDir
//
// Face a walking object in the direction it is moving.
// haleyjd TODO: this is not documented or available in BEX yet.
//
void A_FaceMoveDir(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   static angle_t moveangles[NUMDIRS] = { 0, 32, 64, 96, 128, 160, 192, 224 };

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
void A_GenRefire(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int statenum;
   int chance;

   statenum = E_ArgAsStateNum(args, 0, actor);
   chance   = E_ArgAsInt(args, 1, 0);

   A_FaceTarget(actionargs);

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
void A_GenTracer(actionargs_t *actionargs)
{
   Mobj    *actor = actionargs->actor;
   angle_t  exact;
   fixed_t  dist;
   fixed_t  slope;
   Mobj    *dest;
  
   // adjust direction
   dest = actor->tracer;
   
   if(!dest || dest->health <= 0)
      return;

   // ioanch 20151230: portal-aware
   fixed_t dx = getThingX(actor, dest);
   fixed_t dy = getThingY(actor, dest);
   fixed_t dz = getThingZ(actor, dest);

   // change angle
   exact = P_PointToAngle(actor->x, actor->y, dx, dy);

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
   dist = P_AproxDistance(dx - actor->x, dy - actor->y);
   
   dist = dist / actor->info->speed;

   if(dist < 1)
      dist = 1;

   slope = (dz + 40*FRACUNIT - actor->z) / dist;
   
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
void A_SetTics(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int baseamt = E_ArgAsInt(args, 0, 0);
   int rnd     = E_ArgAsInt(args, 1, 0);
   int counter = E_ArgAsKwd(args, 2, &settickwds, 0);

   // if counter toggle is set, args[0] is a counter number
   if(counter)
   {
      if(baseamt < 0 || baseamt >= NUMMOBJCOUNTERS)
         return; // invalid
      baseamt = actor->counters[baseamt];
   }

   actor->tics = baseamt + (rnd ? P_Random(pr_settics) % rnd : 0);
}

void A_WeaponSetTics(actionargs_t* actionargs)
{
   arglist_t* args = actionargs->args;
   const player_t* player = actionargs->actor ? actionargs->actor->player : nullptr;
   pspdef_t* pspr = actionargs->pspr;
   if (!player || !pspr)
   {
      doom_warningf("Invalid A_WeaponSetTics from non-player");
      return;  // invalid
   }

   int baseamt = E_ArgAsInt(args, 0, 0);
   int rnd = E_ArgAsInt(args, 1, 0);
   int usecounter = E_ArgAsKwd(args, 2, &settickwds, 0);

   // if counter toggle is set, args[0] is a counter number
   if (usecounter)
   {
      if (baseamt < 0 || baseamt >= NUMWEAPCOUNTERS)
      {
         doom_warningf("Invalid A_WeaponSetTics counter %d", baseamt);
         return; // invalid
      }
      baseamt = *E_GetIndexedWepCtrForPlayer(player, baseamt);
   }

   pspr->tics = baseamt + (rnd ? P_Random(pr_wpnsettics) % rnd : 0);
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
void A_MissileAttack(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   Mobj      *mo;
   int        type, a, statenum;
   bool       homing, hastarget = true;
   fixed_t    z;
   angle_t    ang;

   if(!actor->target || actor->target->health <= 0)
      hastarget = false;

   type     = E_ArgAsThingNum(args, 0);   
   homing   = !!E_ArgAsKwd(args, 1, &missileatkkwds, 0);   
   z        = (fixed_t)(E_ArgAsInt(args, 2, 0) * FRACUNIT);
   a        = E_ArgAsInt(args, 3, 0);
   statenum = E_ArgAsStateNumG0(args, 4, actor);

   if(hastarget)
   {
      A_FaceTarget(actionargs);

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
   z = actor->z + actor->info->missileheight + z;

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

      fixed_t momz = P_MissileMomz(target->x - actor->x,
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
void A_MissileSpread(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int     type, num, a, i;
   fixed_t z, momz;
   angle_t angsweep, ang, astep;
   int     statenum;

   if(!actor->target)
      return;

   type     = E_ArgAsThingNum(args,      0);
   num      = E_ArgAsInt(args,           1, 0);
   z        = (fixed_t)(E_ArgAsInt(args, 2, 0) * FRACUNIT);
   a        = E_ArgAsInt(args,           3, 0);
   statenum = E_ArgAsStateNumG0(args,    4, actor);

   if(num < 2)
      return;

   A_FaceTarget(actionargs);

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
   z = actor->z + actor->info->missileheight + z;

   ang = actor->angle - angsweep / 2;
   astep = angsweep / (num - 1);

   for(i = 0; i < num; ++i)
   {
      // calculate z momentum
      momz = P_MissileMomz(getTargetX(actor) - actor->x,
                           getTargetY(actor) - actor->y,
                           getTargetZ(actor) - actor->z, mobjinfo[type]->speed);

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
// args[5] : puff type
//
void A_BulletAttack(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args = actionargs->args;
   soundparams_t params;
   int i, accurate, numbullets, damage, dmgmod, slope;
   
   if(!actor->target)
      return;

   params.sfx = E_ArgAsSound(args, 0);
   numbullets = E_ArgAsInt(args,   2, 0);
   damage     = E_ArgAsInt(args,   3, 0);
   dmgmod     = E_ArgAsInt(args,   4, 0);
   const char *pufftype = E_ArgAsString(args, 5, nullptr);

   // handle accuracy

   // try old keywords first
   accurate = E_ArgAsKwd(args, 1, &bulletkwdsold, -1);

   // try new keywords second
   if(accurate == -1)
   {
      E_ResetArgEval(args, 1);
      accurate = E_ArgAsKwd(args, 1, &bulletkwdsnew, 0);
   }

   if(!accurate)
      accurate = 1;

   if(dmgmod < 1)
      dmgmod = 1;
   else if(dmgmod > 256)
      dmgmod = 256;

   A_FaceTarget(actionargs);
   S_StartSfxInfo(params.setNormalDefaults(actor));

   slope = P_AimLineAttack(actor, actor->angle, MISSILERANGE, false);

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

         P_LineAttack(actor, angle, MISSILERANGE, slope, dmg, pufftype);
      }
      else if(accurate == 3) // ssg spread
      {
         angle += P_SubRandom(pr_monmisfire) << 19;         
         slope += P_SubRandom(pr_monmisfire) << 5;

         P_LineAttack(actor, angle, MISSILERANGE, slope, dmg, pufftype);
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

void A_ThingSummon(actionargs_t *actionargs)
{
   fixed_t    x, y, z;
   Mobj      *actor = actionargs->actor;
   Mobj      *newmobj;
   arglist_t *args = actionargs->args;
   angle_t    an;
   int        type, prestep, deltaz, kill_or_remove, make_child;

   type    = E_ArgAsThingNum(args, 0);
   prestep = E_ArgAsInt(args, 1, 0) << FRACBITS;
   deltaz  = E_ArgAsInt(args, 2, 0) << FRACBITS;

   kill_or_remove = !!E_ArgAsKwd(args, 3, &killremovekwds, 0);
   make_child     = !!E_ArgAsKwd(args, 4, &makechildkwds, 0);
   
   // good old-fashioned pain elemental style spawning
   
   an = actor->angle >> ANGLETOFINESHIFT;
   
   prestep = prestep + 3*(actor->info->radius + mobjinfo[type]->radius)/2;
   
   // ioanch 20160107: spawn past portals in front of spawner
   v2fixed_t relpos = { actor->x + FixedMul(prestep, finecosine[an]),
                        actor->y + FixedMul(prestep, finesine[an]) };
   v2fixed_t pos = P_LinePortalCrossing(*actor, relpos - v2fixed_t{ actor->x, actor->y });

   x = pos.x;
   y = pos.y;
   z = actor->z + deltaz;

   // Check whether the thing is being spawned through a 1-sided
   // wall or an impassible line, or a "monsters can't cross" line.
   // If it is, then we don't allow the spawn.
   
   // ioanch 20160107: use position directly next to summoner.
   if(Check_Sides(actor, relpos.x, relpos.y, type))
      return;

   newmobj = P_SpawnMobj(x, y, z, type);
   
   // Check to see if the new thing's z value is above the
   // ceiling of its new sector, or below the floor. If so, kill it.

   if(!P_CheckFloorCeilingForSpawning(*newmobj))
   {
      actionargs_t dieaction;

      dieaction.actiontype = actionargs_t::MOBJFRAME;
      dieaction.actor      = newmobj;
      dieaction.args       = ESAFEARGS(newmobj);
      dieaction.pspr       = nullptr;

      // kill it immediately
      switch(kill_or_remove)
      {
      case 0:
         A_Die(&dieaction);
         break;
      case 1:
         newmobj->remove();
         break;
      }
      return;
   }                                                         
   
   // spawn thing with same friendliness
   P_transferFriendship(*newmobj, *actor);

   // killough 8/29/98: add to appropriate thread
   newmobj->updateThinker();
   
   // Check for movements.
   // killough 3/15/98: don't jump over dropoffs:

   if(!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
   {
      actionargs_t dieaction;

      dieaction.actiontype = actionargs_t::MOBJFRAME;
      dieaction.actor      = newmobj;
      dieaction.args       = ESAFEARGS(newmobj);
      dieaction.pspr       = nullptr;

      // kill it immediately
      switch(kill_or_remove)
      {
      case 0:
         A_Die(&dieaction);
         break;
      case 1:
         newmobj->remove();
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

void A_KillChildren(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   Thinker   *th;
   int kill_or_remove = !!E_ArgAsKwd(args, 0, &killremovekwds, 0);

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;

      if(!(mo = thinker_cast<Mobj *>(th)))
         continue;

      if(mo->intflags & MIF_ISCHILD && mo->tracer == actor)
      {
         actionargs_t dieaction;

         dieaction.actiontype = actionargs_t::MOBJFRAME;
         dieaction.actor      = mo;
         dieaction.args       = ESAFEARGS(mo);
         dieaction.pspr       = nullptr;

         switch(kill_or_remove)
         {
         case 0:
            A_Die(&dieaction);
            break;
         case 1:
            mo->remove();
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
void A_AproxDistance(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int *dest = nullptr;
   fixed_t distance;
   int cnum;

   cnum = E_ArgAsInt(args, 0, 0);

   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   dest = &(actor->counters[cnum]);

   if(!actor->target)
   {
      *dest = -1;
      return;
   }
   
   distance = P_AproxDistance(actor->x - getTargetX(actor), actor->y - getTargetY(actor));

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
void A_ShowMessage(actionargs_t *actionargs)
{
   arglist_t *args = actionargs->args;
   edf_string_t *msg;
   int type;

   // find the message
   if(!(msg = E_ArgAsEDFString(args, 0)))
      return;

   type = E_ArgAsKwd(args, 1, &messagekwds, 0);

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
void A_AmbientThinker(actionargs_t *actionargs)
{
   soundparams_t  params;
   Mobj          *mo  = actionargs->actor;
   EAmbience_t   *amb = E_AmbienceForNum(mo->args[0]);

   // nothing to play?
   if(!amb || !amb->sound)
      return;

   params.loop = false;

   // run thinker actions for corresponding ambience type
   switch(amb->type)
   {
   case E_AMBIENCE_CONTINUOUS:
      if(S_CheckSoundPlaying(mo, amb->sound)) // not time yet?
         return;
      params.loop = true;
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

   params.origin      = mo;
   params.sfx         = amb->sound;
   params.volumeScale = amb->volume;
   params.attenuation = amb->attenuation;
   params.subchannel  = CHAN_AUTO;
   params.reverb      = amb->reverb;

   // time to play the sound
   S_StartSfxInfo(params);
}

void A_SteamSpawn(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   Mobj *steamthing;
   int thingtype;
   int vrange, hrange;
   int tvangle, thangle;
   angle_t vangle, hangle;
   fixed_t speed, angularspeed;
   
   // Get the thingtype of the thing we're spewing (a steam cloud for example)
   thingtype = E_ArgAsThingNum(args, 0);
   
   // And the speed to fire it out at
   speed = (fixed_t)(E_ArgAsInt(args, 4, 0) << FRACBITS);

   // Make our angles byteangles
   hangle = (mo->angle / (ANG90/64));
   thangle = hangle;
   tvangle = (E_ArgAsInt(args, 2, 0) * 256) / 360;
   // As well as the spread ranges
   hrange = (E_ArgAsInt(args, 1, 0) * 256) / 360;
   vrange = (E_ArgAsInt(args, 3, 0) * 256) / 360;
   
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
   angularspeed     = FixedMul(speed,        finecosine[vangle >> ANGLETOFINESHIFT]);
   steamthing->momx = FixedMul(angularspeed, finecosine[hangle >> ANGLETOFINESHIFT]);
   steamthing->momy = FixedMul(angularspeed, finesine[hangle >> ANGLETOFINESHIFT]);
   steamthing->momz = FixedMul(speed,        finesine[vangle >> ANGLETOFINESHIFT]);
}

//
// A_TargetJump
//
// Parameterized codepointer for branching based on whether a
// thing's target is valid and alive.
//
// args[0] : state number
//
void A_TargetJump(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int statenum;
   
   if((statenum = E_ArgAsStateNumNI(args, 0, mo)) < 0)
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
void A_EjectCasing(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   angle_t    angle = actor->angle;
   fixed_t    x, y, z;
   fixed_t    frontdist;
   int        frontdisti;
   fixed_t    sidedist;
   fixed_t    zheight;
   int        thingtype;
   Mobj      *mo;

   frontdisti = E_ArgAsInt(args, 0, 0);
   
   frontdist  = frontdisti * FRACUNIT / 16;
   sidedist   = E_ArgAsInt(args, 1, 0) * FRACUNIT / 16;
   zheight    = E_ArgAsInt(args, 2, 0) * FRACUNIT / 16;

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

   thingtype = E_ArgAsThingNum(args, 3);

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
void A_CasingThrust(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   fixed_t moml, momz;

   moml = E_ArgAsInt(args, 0, 0) * FRACUNIT / 16;
   momz = E_ArgAsInt(args, 1, 0) * FRACUNIT / 16;
   
   actor->momx = FixedMul(moml, finecosine[actor->angle>>ANGLETOFINESHIFT]);
   actor->momy = FixedMul(moml, finesine[actor->angle>>ANGLETOFINESHIFT]);
   
   // randomize
   actor->momx += P_SubRandom(pr_casing) << 8;
   actor->momy += P_SubRandom(pr_casing) << 8;
   actor->momz  = momz + (P_SubRandom(pr_casing) << 8);
}

//
// A_DetonateEx
//
// Semi-equivalent to ZDoom A_Explode extensions.
//
// args[0] : damage
// args[1] : radius
// args[2] : hurt self?
// args[3] : alert?
// TODO: args[4] : full damage radius
// TODO: args[5] : nail count
// TODO: args[6] : nail damage
// args[7] : z clip?
// args[8] : spot is source?
//
void A_DetonateEx(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int   damage   =  E_ArgAsInt(args, 0, 128);
   int   radius   =  E_ArgAsInt(args, 1, 128);
   bool  hurtself = (E_ArgAsInt(args, 2, 1) == 1);
   bool  alert    = (E_ArgAsInt(args, 3, 0) == 1);
   bool  zclip    = (E_ArgAsInt(args, 7, 0) == 1);
   bool  spotsrc  = (E_ArgAsInt(args, 8, 0) == 1);
   Mobj *source   = actor->target;
   unsigned int flags = 0;

   // invalid damage or radius, return immediately.
   if(damage <= 0 || radius <= 0)
      return;

   // if there's no target, or spot is designated as being the source,
   // set source == actor.
   if(!source || spotsrc)
      source = actor;

   // doesn't damage self?
   if(!hurtself)
      flags |= RAF_NOSELFDAMAGE;

   // checks z height?
   if(zclip)
      flags |= RAF_CLIPHEIGHT;

   // Do it.
   P_RadiusAttack(actor, source, damage, radius, actor->info->mod, flags);

   // optional noise alert, as in Strife 
   if(alert)
      P_NoiseAlert(source, actor);

   // cause a terrain hit
   // ioanch 20160116: portal aware Z
   E_ExplosionHitWater(actor, radius);
}

//
// A_SelfDestruct
//
// Detonates a missile mid-flight.
// If actor is not a missile, A_Die
// is called instead. 'Nuff said.
//
void A_SelfDestruct(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(actor->flags & (MF_MISSILE | MF_BOUNCES))
      P_ExplodeMissile(actor, nullptr);
   else
      A_Die(actionargs);
}

// EOF

