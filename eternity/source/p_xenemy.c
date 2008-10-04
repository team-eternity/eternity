// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007 James Haley
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
//
// Hexen- and ZDoom-inspired action functions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "a_small.h"
#include "doomstat.h"
#include "p_enemy.h"
#include "p_xenemy.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "e_sound.h"
#include "s_sound.h"
#include "e_lib.h"

//
// T_QuakeThinker
//
// Earthquake effect thinker.
//
void T_QuakeThinker(quakethinker_t *qt)
{
   int i, tics;
   mobj_t *soundorg = (mobj_t *)&qt->origin;
   sfxinfo_t *quakesound;
   
   // quake is finished?
   if(qt->duration == 0)
   {
      S_StopSound(soundorg, CHAN_ALL);
      P_RemoveThinker(&(qt->origin.thinker));
      return;
   }
   
   quakesound = E_SoundForName("Earthquake");

   // loop quake sound
   if(quakesound && !S_CheckSoundPlaying(soundorg, quakesound))
      S_StartSfxInfo(soundorg, quakesound, 127, ATTN_NORMAL, true, CHAN_AUTO);

   tics = qt->duration--;

   // do some rumbling
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i])
      {
         player_t *p  = &players[i];
         mobj_t   *mo = p->mo;
         fixed_t  dst = P_AproxDistance(qt->origin.x - mo->x, 
                                        qt->origin.y - mo->y);

         // test if player is in quake radius
         // haleyjd 04/16/07: only set p->quake when qt->intensity is greater;
         // this way, the strongest quake in effect always wins out
         if(dst < qt->quakeRadius && p->quake < qt->intensity)
            p->quake = qt->intensity;

         // every 2 tics, the player may be damaged
         if(!(tics & 1))
         {
            angle_t  thrustangle;   
            
            // test if in damage radius and on floor
            if(dst < qt->damageRadius && mo->z <= mo->floorz)
            {
               if(P_Random(pr_quake) < 50)
               {
                  P_DamageMobj(mo, NULL, NULL, P_Random(pr_quakedmg) % 8 + 1,
                               MOD_QUAKE);
               }
               thrustangle = (359 * P_Random(pr_quake) / 255) * ANGLE_1;
               P_ThrustMobj(mo, thrustangle, qt->intensity * FRACUNIT / 2);
            }
         }
      }
   }
}

//
// P_StartQuake
//
// Starts an earthquake at each object with the tid in args[4]
//
boolean P_StartQuake(long *args)
{
   mobj_t *mo  = NULL;
   boolean ret = false;

   while((mo = P_FindMobjFromTID(args[4], mo, NULL)))
   {
      quakethinker_t *qt;
      ret = true;

      qt = Z_Malloc(sizeof(*qt), PU_LEVSPEC, NULL);
      qt->origin.thinker.function = T_QuakeThinker;

      P_AddThinker(&(qt->origin.thinker));

      qt->intensity    = args[0];
      qt->duration     = args[1];
      qt->damageRadius = args[2] * (64 * FRACUNIT);
      qt->quakeRadius  = args[3] * (64 * FRACUNIT);

      qt->origin.x       = mo->x;
      qt->origin.y       = mo->y;
      qt->origin.z       = mo->z;
#ifdef R_LINKEDPORTALS
      qt->origin.groupid = mo->groupid;
#endif
   }

   return ret;
}

//
// A_FadeIn
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : alpha step
//
void A_FadeIn(mobj_t *mo)
{
   mo->translucency += mo->state->args[0];
   
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
void A_FadeOut(mobj_t *mo)
{
   mo->translucency -= mo->state->args[0];
   
   if(mo->translucency < 0)
      mo->translucency = 0;
   else if(mo->translucency > FRACUNIT)
      mo->translucency = FRACUNIT;
}

//
// A_ClearSkin
//
// Codepointer needed for Heretic/Hexen/Strife support.
//
void A_ClearSkin(mobj_t *mo)
{
   mo->skin   = NULL;
   mo->sprite = mo->state->sprite;
}

E_Keyword_t kwds_A_PlaySoundEx[] =
{
   { "chan_auto",   0 },
   { "chan_weapon", 1 },
   { "chan_voice",  2 },
   { "chan_item",   3 },
   { "chan_body",   4 },
   { "attn_normal", 0 },
   { "attn_idle",   1 },
   { "attn_static", 2 },
   { "attn_none",   3 },
   { NULL }
};

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
//
void A_PlaySoundEx(mobj_t *mo)
{
   int sound    =   mo->state->args[0];
   int channel  =   mo->state->args[1];
   boolean loop = !!mo->state->args[2];
   int attn     =   mo->state->args[3];
   int volume   =   mo->state->args[4];
   sfxinfo_t *sfx = NULL;

   if(!(sfx = E_SoundForDEHNum(sound)))
      return;

   // rangechecking
   if(attn < 0 || attn >= ATTN_NUM)
      attn = ATTN_NORMAL;

   if(channel < CHAN_AUTO || channel >= NUMSCHANNELS)
      channel = CHAN_AUTO;

   // note: volume 0 == 127, for convenience
   if(volume <= 0 || volume > 127)
      volume = 127;

   S_StartSfxInfo(mo, sfx, volume, attn, loop, channel);
}

//==============================================================================
// 
// Hexen Codepointers
//

//
// A_SetInvulnerable
//
// Not strictly necessary for EE but a convenience routine nonetheless.
//
void A_SetInvulnerable(mobj_t *actor)
{
   actor->flags |= MF2_INVULNERABLE;
}

void A_UnSetInvulnerable(mobj_t *actor)
{
   actor->flags &= ~MF2_INVULNERABLE;
}

//
// A_SetReflective
//
// As above.
//
void A_SetReflective(mobj_t *actor)
{
   actor->flags |= MF2_REFLECTIVE;

   // note: no special case for Centaurs; use SetFlags pointer instead
}

void A_UnSetReflective(mobj_t *actor)
{
   actor->flags &= ~MF2_REFLECTIVE;
}

//
// P_UpdateMorphedMonster
//
boolean P_UpdateMorphedMonster(mobj_t *actor, int tics)
{
   // HEXEN_TODO
   return false;
}

//
// A_PigLook
//
void A_PigLook(mobj_t *actor)
{
   if(P_UpdateMorphedMonster(actor, 10))
      return;

   A_Look(actor);
}

//
// A_PigChase
//
void A_PigChase(mobj_t *actor)
{
   if(P_UpdateMorphedMonster(actor, 3))
      return;

   A_Chase(actor);
}

//
// A_PigAttack
//
void A_PigAttack(mobj_t *actor)
{
   if(P_UpdateMorphedMonster(actor, 18))
      return;

   if(!actor->target)
      return;

   /*
   HEXEN_TODO
   if(P_CheckMeleeRange(actor))
   {
      P_DamageMobj(actor->target, actor, actor, 2+(P_Random()&1));
      S_StartSound(actor, SFX_PIG_ATTACK);
   }
   */
}

//
// A_PigPain
//
void A_PigPain(mobj_t *actor)
{
   A_Pain(actor);

   if(actor->z <= actor->floorz)
      actor->momz = 7 * FRACUNIT / 2;
}

// HEXEN_TODO: Dark Servant Maulotaur variation

//
// A_HexenExplode
//
// Hexen explosion effects pointer
//
void A_HexenExplode(mobj_t *actor)
{
   // HEXEN_TODO
   // Note: requires new blast radius semantics
}

// HEXEN_TODO: A_FreeTargMobj needed?

//
// A_SerpentUnHide
//
void A_SerpentUnHide(mobj_t *actor)
{
   actor->flags2    &= ~MF2_DONTDRAW;
   actor->floorclip  = 24 * FRACUNIT;
}

//
// A_SerpentHide
//
void A_SerpentHide(mobj_t *actor)
{
   actor->flags2    |= MF2_DONTDRAW;
   actor->floorclip  = 0;
}

//
// A_SerpentChase
//
void A_SerpentChase(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_LowerFloorClip
//
// Note: was A_SerpentRaiseHump in Hexen
// Parameters:
// * args[0] == amount to lower floorclip in eighths of a unit
//
void A_LowerFloorClip(mobj_t *actor)
{
   fixed_t lowerAmt = actor->state->args[0] * FRACUNIT / 8;

   actor->floorclip -= lowerAmt;
}

//
// A_RaiseFloorClip
//
// Note: was A_SerpentLowerHump in Hexen
// Parameters:
// * args[0] == amount to raise floorclip in eighths of a unit
//
void A_RaiseFloorClip(mobj_t *actor)
{
   fixed_t raiseAmt = actor->state->args[0] * FRACUNIT / 8;

   actor->floorclip += raiseAmt;
}

//
// A_SerpentHumpDecide
//
void A_SerpentHumpDecide(mobj_t *actor)
{
   // HEXEN_TODO
}

// EOF

