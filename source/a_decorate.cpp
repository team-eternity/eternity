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
//------------------------------------------------------------------------------
//
// Purpose: DECORATE-compatible or -inspired action functions.
// Authors: James Haley, Ioan Chera, Max Waine
//

#include "z_zone.h"

#include "a_args.h"
#include "doomstat.h"
#include "d_gi.h"
#include "e_args.h"
#include "e_exdata.h"
#include "e_inventory.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "ev_specials.h"
#include "m_compare.h"
#include "p_mobj.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"
#include "v_misc.h"

//
// A_AlertMonsters
//
// ZDoom codepointer #3, implemented from scratch using wiki
// documentation. 100% GPL version.
//
void A_AlertMonsters(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   if(actionargs->pspr)
      P_NoiseAlert(mo, mo);
   if(mo->target)
      P_NoiseAlert(mo->target, mo->target);
}

//
// A_CheckPlayerDone
//
// ZDoom codepointer #4. Needed for Heretic support also.
// Extension: 
//    args[0] == state DeHackEd number to transfer into
//
void A_CheckPlayerDone(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int statenum;
   
   if((statenum = E_ArgAsStateNumNI(actionargs->args, 0, actor)) < 0)
      return;

   if(!actor->player)
      P_SetMobjState(actor, statenum);
}

//
// A_ClearSkin
//
// Codepointer needed for Heretic/Hexen/Strife support.
//
void A_ClearSkin(actionargs_t *actionargs)
{
   Mobj *mo   = actionargs->actor;
   fixed_t prevSpriteRadius = P_GetSpriteOrBoxRadius(*mo);
   mo->skin   = nullptr;
   mo->sprite = mo->state->sprite;
   P_RefreshSpriteTouchingSectorList(mo, prevSpriteRadius);
}

//
// A_FadeIn
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : alpha step
//
void A_FadeIn(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   mo->translucency += E_ArgAsFixed(actionargs->args, 0, 0);
   
   if(mo->translucency < 0)
      mo->translucency = 0;
   else if(mo->translucency > FRACUNIT)
      mo->translucency = FRACUNIT;
}

//
// A_FadeOut
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : alpha step
//
void A_FadeOut(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   mo->translucency -= E_ArgAsFixed(actionargs->args, 0, 0);
   
   if(mo->translucency < 0)
      mo->translucency = 0;
   else if(mo->translucency > FRACUNIT)
      mo->translucency = FRACUNIT;
}

//
// A_Jump
//
// Primary jumping pointer for use in DECORATE states.
//
// args[0] : chance
// args[N] : offset || state label
//
void A_Jump(actionargs_t *actionargs)
{
   int        chance, choice;
   Mobj      *actor  = actionargs->actor;
   player_t  *player = actor->player;
   arglist_t *al     = actionargs->args;
   auto       at     = actionargs->actiontype;
   state_t   *state;

   // no args?
   if(!al || al->numargs < 2)
      return;

   // get chance for jump in args[0]
   if(!(chance = E_ArgAsInt(al, 0, 0)))
      return;

   // determine if it will jump
   if(P_Random(pr_decjump) >= chance)
      return;

   // play Russian roulette to determine the state to which it will jump;
   // increment by one to skip over the chance argument
   choice = (P_Random(pr_decjump2) % (al->numargs - 1)) + 1;

   // if the state is found, jump to it.
   if(at == actionargs_t::MOBJFRAME && (state = E_ArgAsStateLabel(actor, al, choice)))
      P_SetMobjState(actor, state->index);
   else if(at == actionargs_t::WEAPONFRAME && (state = E_ArgAsStateLabel(player, al, choice)))
      P_SetPspritePtr(*player, actionargs->pspr, state->index);

}

//
// A_JumpIfNoAmmo
//
// ZDoom-compatible ammo jump. For weapons only!
//    args[0] : state to jump to if not enough ammo
//
void A_JumpIfNoAmmo(actionargs_t *actionargs)
{
   if(actionargs->pspr)
   {
      player_t *p     = actionargs->actor->player;
      int statenum    = E_ArgAsStateNumNI(actionargs->args, 0, p);
      weaponinfo_t *w = p->readyweapon;
      int ammo;

      // validate state
      if(statenum < 0)
         return;

      if(!w->ammo) // no-ammo weapon?
         return;

      ammo = E_GetItemOwnedAmount(*p, w->ammo);

      if(ammo < w->ammopershot)
         P_SetPspritePtr(*p, actionargs->pspr, statenum);
   }
}

//
// A_JumpIfTargetInLOS
//
// ZDoom codepointer #1, implemented from scratch using wiki
// documentation. 100% GPL version.
//
// args[0] : statenum
// args[1] : fov
// args[2] : proj_target
//
void A_JumpIfTargetInLOS(actionargs_t *actionargs)
{
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;
   int statenum;

   if(actionargs->pspr)
   {
      // called from a weapon frame
      player_t *player = actor->player;
      pspdef_t *pspr   = actionargs->pspr;

      // see if the player has an autoaim target
      P_BulletSlope(actor);
      if(!clip.linetarget)
         return;

      // prepare to jump!
      if((statenum = E_ArgAsStateNumNI(args, 0, player)) < 0)
         return;

      P_SetPspritePtr(*player, pspr, statenum);
   }
   else
   {
      Mobj *target = actor->target;
      bool    seek = !!E_ArgAsInt(args, 2, 0);
      fixed_t ffov = E_ArgAsFixed(args, 1, 0);

      // if a missile, determine what to do from args[2]
      if(actor->flags & MF_MISSILE)
      {
         if(seek)
            target = actor->tracer;
      }

      // no target? nothing else to do
      if(!target)
         return;

      // check fov if one is specified
      if(ffov)
      {
         angle_t fov  = FixedToAngle(ffov);
         angle_t tang = P_PointToAngle(actor->x, actor->y, getThingX(actor, target),
                                       getThingY(actor, target));
         angle_t minang = actor->angle - FixedToAngle(fov) / 2;
         angle_t maxang = actor->angle + FixedToAngle(fov) / 2;

         // if the angles are backward, compare differently
         if((minang > maxang) ? tang < minang && tang > maxang
                              : tang < minang || tang > maxang)
         {
            return;
         }
      }

      // check line of sight
      if(!P_CheckSight(actor, target))
         return;

      // prepare to jump!
      if((statenum = E_ArgAsStateNumNI(args, 0, actor)) < 0)
         return;

      P_SetMobjState(actor, statenum);
   }
}

//
// A_SetTranslucent
//
// ZDoom codepointer #2, implemented from scratch using wiki
// documentation. 100% GPL version.
// 
// args[0] : alpha
// args[1] : mode
//
void A_SetTranslucent(actionargs_t *actionargs)
{
   Mobj      *mo    = actionargs->actor;
   arglist_t *args  = actionargs->args;
   fixed_t    alpha = E_ArgAsFixed(args, 0, 0);
   int        mode  = E_ArgAsInt(args, 1, 0);

   // rangecheck alpha
   if(alpha < 0)
      alpha = 0;
   else if(alpha > FRACUNIT)
      alpha = FRACUNIT;

   // unset any relevant flags first
   mo->flags  &= ~MF_SHADOW;      // no shadow
   mo->flags  &= ~MF_TRANSLUCENT; // no BOOM translucency
   mo->flags3 &= ~MF3_TLSTYLEADD; // no additive
   mo->translucency = FRACUNIT;   // no flex translucency

   // set flags/translucency properly
   switch(mode)
   {
   case 0: // 0 == normal translucency
      mo->translucency = alpha;
      break;
   case 1: // 1 == additive
      mo->translucency = alpha;
      mo->flags3 |= MF3_TLSTYLEADD;
      break;
   case 2: // 2 == spectre fuzz
      mo->flags |= MF_SHADOW;
      break;
   default: // ???
      break;
   }
}

//=============================================================================
//
// A_PlaySoundEx
//
// This one's a beast so it's down here by its lonesome.
//

// Old keywords for A_PlaySoundEx: deprecated

static const char *kwds_channel_old[] =
{
   "chan_auto",   // 0 },
   "chan_weapon", // 1 },
   "chan_voice",  // 2 },
   "chan_item",   // 3 },
   "chan_body",   // 4 },
   "5",           // 5 },
   "6",           // 6 },
   "7",           // 7 },
};

static argkeywd_t channelkwdsold = { kwds_channel_old, NUMSCHANNELS };

static const char *kwds_attn_old[] =
{
   "attn_normal", // 0
   "attn_idle",   // 1
   "attn_static", // 2
   "attn_none",   // 3
};

static argkeywd_t attnkwdsold = { kwds_attn_old, ATTN_NUM };

// New keywords: preferred

static const char *kwds_channel_new[] =
{
   "auto",   // 0
   "weapon", // 1
   "voice",  // 2
   "item",   // 3
   "body",   // 4
   "5",      // 5
   "6",      // 6
   "7",      // 7
};

static argkeywd_t channelkwdsnew = { kwds_channel_new, NUMSCHANNELS };

static const char *kwds_attn_new[] =
{
   "normal", // 0
   "idle",   // 1
   "static", // 2
   "none",   // 3
};

static argkeywd_t attnkwdsnew = { kwds_attn_new, ATTN_NUM };

//
// A_PlaySoundEx
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : sound
// args[1] : subchannel
// args[2] : loop?
// args[3] : attenuation
// args[4] : EE extension - volume
// args[5] : EE extension - reverb
//
void A_PlaySoundEx(actionargs_t *actionargs)
{
   arglist_t *args = actionargs->args;   
   soundparams_t params;

   params.origin = actionargs->actor;
   params.sfx    = E_ArgAsSound(args, 0);

   if(!params.sfx)
      return;
   
   // handle channel
   params.subchannel = E_ArgAsKwd(args, 1, &channelkwdsold, -1);
   if(params.subchannel == -1)
   {
      E_ResetArgEval(args, 1);
      params.subchannel = E_ArgAsKwd(args, 1, &channelkwdsnew, 0);
   }

   params.loop = !!E_ArgAsInt(args, 2, 0);

   // handle attenuation
   params.attenuation = E_ArgAsKwd(args, 3, &attnkwdsold, -1);
   
   if(params.attenuation == -1)
   {
      E_ResetArgEval(args, 3);
      params.attenuation = E_ArgAsKwd(args, 3, &attnkwdsnew, 0);
   }

   params.volumeScale = E_ArgAsInt(args, 4, 0);

   params.reverb = !!E_ArgAsInt(args, 5, 1);

   // rangechecking
   if(params.attenuation < 0 || params.attenuation >= ATTN_NUM)
      params.attenuation = ATTN_NORMAL;

   if(params.subchannel < CHAN_AUTO || params.subchannel >= NUMSCHANNELS)
      params.subchannel = CHAN_AUTO;

   // note: volume 0 == 127, for convenience
   if(params.volumeScale <= 0 || params.volumeScale > 127)
      params.volumeScale = 127;

   S_StartSfxInfo(params);
}

//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : special
// args[1] : arg0
// args[2] : arg1
// args[3] : arg2
// args[4] : arg3
// args[5] : arg4
//
void A_SetSpecial(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   arglist_t *args = actionargs->args;

   int specnum = E_ArgAsInt(args, 0, 0);
   const char *specname = E_ArgAsString(args, 0, "");

   int special = 0;
   if(specnum)
      special = EV_ActionForACSAction(specnum);
   else
   {
      if(estrempty(specname))
         return;
      special = E_LineSpecForName(specname);
   }

   if(!special)
   {
      // Check for deliberate '0' value
      char *endptr = nullptr;
      strtol(specname, &endptr, 0);
      if(*endptr) // not just '0'
      {
         doom_printf(FC_ERROR "A_SetSpecial: unknown special '%s'\n", specname);
         return;
      }  // otherwise we have special 0 coming from value "0"
   }

   actor->special = special;

   for(int i = 1; i <= 5; i++)
      actor->args[i - 1] = E_ArgAsInt(args, i, 0);
}

enum seekermissile_flags : unsigned int
{
   SMF_LOOK     = 0x00000001,
   SMF_PRECISE  = 0x00000002,
   SMF_CURSPEED = 0x00000004,
};

static dehflags_t seekermissile_flaglist[] =
{
   { "normal",   0x00000000   },
   { "look",     SMF_LOOK     },
   { "precise",  SMF_PRECISE  },
   { "curspeed", SMF_CURSPEED },
   { nullptr,    0            }
};

static dehflagset_t seekermissile_flagset =
{
   seekermissile_flaglist, // flaglist
   0,                      // mode
};

//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : threshold
// args[1] : maxturnangle
// args[2] : flags
// args[3] : chance (out of 256) it will try acquire a target if it doesn't have one
// args[4] : distance
//
void A_SeekerMissile(actionargs_t *actionargs)
{
   Mobj      *dest;
   Mobj      *actor = actionargs->actor;
   arglist_t *args  = actionargs->args;

   const angle_t threshold    = eclamp<angle_t>(E_ArgAsAngle(args, 0, ANG90), 0, ANG90);
   const angle_t maxTurnAngle = eclamp<angle_t>(E_ArgAsAngle(args, 1, ANG90), 0, ANG90);
   const unsigned int flags   = E_ArgAsFlags(args, 2, &seekermissile_flagset);
   const int     chance       = E_ArgAsInt(args, 3, 50);
   const fixed_t distance     = E_ArgAsFixed(args, 4, 10 << FRACBITS) * MAPBLOCKUNITS;

   const fixed_t speed = [flags, actor] {
      if(!(flags & SMF_CURSPEED))
         return actor->info->speed;
      else
      {
         const double fx = M_FixedToDouble(actor->momx);
         const double fy = M_FixedToDouble(actor->momy);
         const double fz = M_FixedToDouble(actor->momz);
         return M_DoubleToFixed(sqrt(fx*fx + fy*fy + fz*fz));
      }
   }();

   // adjust direction
   dest = actor->tracer;

   if(!dest || dest->health <= 0)
   {
      if((flags & SMF_LOOK) && P_Random(pr_seekermissile) < chance)
      {
         bool success = false;

         // Code based on A_BouncingBFG
         for(int i = 1; i < 40; i++)  // offset angles from its attack angle
         {
            // Angle fans outward, preferring nearer angles
            angle_t an = actor->angle;
            if(i & 2)
               an += (ANG360 / 40) * (i / 2);
            else
               an -= (ANG360 / 40) * (i / 2);

            P_AimLineAttack(actor, an, distance, true);

            // don't aim for shooter, or for friends of shooter
            if(clip.linetarget)
            {
               if(clip.linetarget == actor->target ||
                  (clip.linetarget->flags & actor->target->flags & MF_FRIEND))
                  continue;

               if(clip.linetarget)
               {
                  P_SetTarget<Mobj>(&actor->tracer, clip.linetarget);
                  dest = clip.linetarget;
                  success = true;
                  break;
               }
            }
         }

         if(!success)
            return;
      }
      else
         return;
   }

   const fixed_t dx = getThingX(actor, dest);
   const fixed_t dy = getThingY(actor, dest);
   const fixed_t dz = getThingZ(actor, dest);

   // change angle
   angle_t exact = P_PointToAngle(actor->x, actor->y, dx, dy);

   if(exact != actor->angle)
   {
      // MaxW: I've been informed ZDoom halves turning if the
      // angle towards the target is larger than the threshold
      angle_t turnAngle = exact - actor->angle;
      if(turnAngle > threshold)
         turnAngle = maxTurnAngle / 2;
      else
         turnAngle = maxTurnAngle;

      if(exact - actor->angle > 0x80000000)
      {
         actor->angle -= turnAngle;
         if(exact - actor->angle < 0x80000000)
            actor->angle = exact;
      }
      else
      {
         actor->angle += turnAngle;
         if(exact - actor->angle > 0x80000000)
            actor->angle = exact;
      }
   }

   exact = actor->angle >> ANGLETOFINESHIFT;
   actor->momx = FixedMul(speed, finecosine[exact]);
   actor->momy = FixedMul(speed, finesine[exact]);

   // change slope
   fixed_t dist = P_AproxDistance(dx - actor->x, dy - actor->y);
   dist = emax(dist / speed, 1);

   if(flags & SMF_PRECISE)
      actor->momz = (dz + (dest->height >> 1) - actor->z) / dist;
   else
   {
      const fixed_t slope = (dz + 40 * FRACUNIT - actor->z) / dist;

      if(slope < actor->momz)
         actor->momz -= FRACUNIT / 8;
      else
         actor->momz += FRACUNIT / 8;
   }
}

// EOF

