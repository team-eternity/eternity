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
//      DECORATE-compatible or -inspired action functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_args.h"
#include "doomstat.h"
#include "d_gi.h"
#include "e_args.h"
#include "e_inventory.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
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

//
// A_AlertMonsters
//
// ZDoom codepointer #3, implemented from scratch using wiki
// documentation. 100% GPL version.
// 
void A_AlertMonsters(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
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
   mo->skin   = NULL;
   mo->sprite = mo->state->sprite;
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
      P_SetPspritePtr(player, actionargs->pspr, state->index);

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

      ammo = E_GetItemOwnedAmount(p, w->ammo);

      if(ammo < w->ammopershot)
         P_SetPspritePtr(p, actionargs->pspr, statenum);
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

      P_SetPspritePtr(player, pspr, statenum);
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
         angle_t tang = P_PointToAngle(actor->x, actor->y,
#ifdef R_LINKEDPORTALS
                                        getThingX(actor, target),
                                        getThingY(actor, target));
#else
                                        target->x, target->y);
#endif
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
   
   actor->special = E_ArgAsInt(args, 0, 0);

   for(int i = 1; i >= 5; i++)
      actor->args[i - 1] = E_ArgAsInt(args, i, 0);
}

// EOF

