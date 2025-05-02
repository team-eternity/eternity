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
// Purpose: Heretic action functions.
// Authors: James Haley, Ioan Chera, Max Waine, Simone Ivanish
//

#include "z_zone.h"

#include "a_args.h"
#include "a_common.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobjcol.h"
#include "p_mobj.h"
#include "p_portalcross.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"

//
// P_spawnGlitter
//
// haleyjd 08/02/13: shared code for teleglitter spawners.
//
static void P_spawnGlitter(Mobj *actor, int type)
{
    // ioanch 20160116: make it portal-aware BOTH beyond line and sector gates
    fixed_t   dy  = ((P_Random(pr_tglit) & 31) - 16) * FRACUNIT;
    fixed_t   dx  = ((P_Random(pr_tglit) & 31) - 16) * FRACUNIT;
    v2fixed_t pos = P_LinePortalCrossing(*actor, dx, dy);

    Mobj *mo = P_SpawnMobj(pos.x, pos.y, P_ExtremeSectorAtPoint(actor, surf_floor)->srf.floor.getZAt(pos), type);
    mo->momz = FRACUNIT / 4;
}

//
// A_SpawnTeleGlitter
//
// haleyjd 08/02/13: Original teleglitter spawn, for better Heretic action
// function compatibility. This is a special case of A_SpawnGlitter (see
// a_general.cpp)
//
void A_SpawnTeleGlitter(actionargs_t *actionargs)
{
    P_spawnGlitter(actionargs->actor, E_SafeThingName("TeleGlitterRed"));
}

//
// A_SpawnTeleGlitter2
//
// As above.
//
void A_SpawnTeleGlitter2(actionargs_t *actionargs)
{
    P_spawnGlitter(actionargs->actor, E_SafeThingName("TeleGlitterBlue"));
}

//
// A_AccelGlitter
//
// Increases object's z momentum by 50%
//
void A_AccelGlitter(actionargs_t *actionargs)
{
    Mobj *actor  = actionargs->actor;
    actor->momz += actor->momz / 2;
}

//
// A_InitKeyGizmo
//
// haleyjd 08/02/13: Original key gizmo init, for better Heretic action function
// compatibility. This is a special case of A_SpawnAbove (see a_general.cpp)
//
void A_InitKeyGizmo(actionargs_t *actionargs)
{
    Mobj       *gizmo     = actionargs->actor;
    const char *stateName = nullptr;

    if(gizmo->type == E_ThingNumForName("KeyGizmoBlue"))
        stateName = "Blue";
    else if(gizmo->type == E_ThingNumForName("KeyGizmoGreen"))
        stateName = "Green";
    else if(gizmo->type == E_ThingNumForName("KeyGizmoYellow"))
        stateName = "Yellow";

    if(!stateName)
        return;

    int               orbType = E_SafeThingName("KeyGizmoOrb");
    const mobjinfo_t *mi      = mobjinfo[orbType];
    const state_t    *state   = E_GetStateForMobjInfo(mi, stateName);

    Mobj *mo = P_SpawnMobj(gizmo->x, gizmo->y, gizmo->z + 60 * FRACUNIT, orbType);
    if(state)
        P_SetMobjState(mo, state->index);
}

//=============================================================================
//
// Heretic Mummy
//

void A_MummyAttack(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        int dmg = ((P_Random(pr_mumpunch) & 7) + 1) * 2;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
        S_StartSound(actor, sfx_mumat2);
        return;
    }

    S_StartSound(actor, sfx_mumat1);
}

void A_MummyAttack2(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;
    Mobj *mo;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        int dmg = ((P_Random(pr_mumpunch2) & 7) + 1) * 2;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
        return;
    }

    mo = P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_MUMMYFX1), actor->z + actor->info->missileheight);

    P_SetTarget<Mobj>(&mo->tracer, actor->target);
}

void A_MummySoul(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;
    Mobj *mo;
    int   soulType = E_SafeThingType(MT_MUMMYSOUL);

    mo       = P_SpawnMobj(actor->x, actor->y, actor->z + 10 * FRACUNIT, soulType);
    mo->momz = FRACUNIT;
}

//
// A_HticDrop
//
// Deprecated; is now just an alias for A_Fall.
//
void A_HticDrop(actionargs_t *actionargs)
{
    A_Fall(actionargs);
}

//
// Function to maintain Heretic-style seekers
// TODO: Replace all P_HticTracer called with P_SeekerMissile calls?
//
static void P_HticTracer(Mobj *actor, angle_t threshold, angle_t maxturn)
{
    P_SeekerMissile(actor, threshold, maxturn, seekcenter_e::no);
}

//
// A_HticTracer
//
// Parameterized pointer for Heretic-style tracers. I wanted
// to merge this with A_GenTracer, but the logic looks incompatible
// no matter how I rewrite it.
//
// args[0]: threshold in degrees
// args[1]: maxturn in degrees
//
void A_HticTracer(actionargs_t *actionargs)
{
    Mobj      *actor = actionargs->actor;
    arglist_t *args  = actionargs->args;
    angle_t    threshold, maxturn;

    threshold = (angle_t)(E_ArgAsInt(args, 0, 0));
    maxturn   = (angle_t)(E_ArgAsInt(args, 1, 0));

    // convert from integer angle to angle_t
    threshold = (angle_t)(((uint64_t)threshold << 32) / 360);
    maxturn   = (angle_t)(((uint64_t)maxturn << 32) / 360);

    P_HticTracer(actor, threshold, maxturn);
}

// Non-parameterized versions

//
// A_MummyFX1Seek
//
// Nitrogolem missile seeking
//
void A_MummyFX1Seek(actionargs_t *actionargs)
{
    P_HticTracer(actionargs->actor, HTICANGLE_1 * 10, HTICANGLE_1 * 20);
}

//
// A_ClinkAttack
//
// Sabreclaw's melee attack
//
void A_ClinkAttack(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        int dmg = (P_Random(pr_clinkatk) % 7) + 3;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
    }
}

//=============================================================================
//
// Disciple Actions
//

void A_GhostOff(actionargs_t *actionargs)
{
    actionargs->actor->flags3 &= ~MF3_GHOST;
}

void A_WizardAtk1(actionargs_t *actionargs)
{
    A_FaceTarget(actionargs);
    actionargs->actor->flags3 &= ~MF3_GHOST;
}

void A_WizardAtk2(actionargs_t *actionargs)
{
    A_FaceTarget(actionargs);
    actionargs->actor->flags3 |= MF3_GHOST;
}

void A_WizardAtk3(actionargs_t *actionargs)
{
    Mobj   *actor = actionargs->actor;
    Mobj   *mo;
    angle_t angle;
    fixed_t momz;
    fixed_t z         = actor->z + actor->info->missileheight;
    int     wizfxType = E_SafeThingType(MT_WIZFX1);

    actor->flags3 &= ~MF3_GHOST;
    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        int dmg = ((P_Random(pr_wizatk) & 7) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
        return;
    }

    mo    = P_SpawnMissile(actor, actor->target, wizfxType, z);
    momz  = mo->momz;
    angle = mo->angle;
    P_SpawnMissileAngle(actor, wizfxType, angle - (ANG45 / 8), momz, z);
    P_SpawnMissileAngle(actor, wizfxType, angle + (ANG45 / 8), momz, z);
}

//=============================================================================
//
// Serpent Rider D'Sparil Actions
//

void A_Sor1Chase(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(actor->counters[0])
    {
        // decrement fast walk timer
        actor->counters[0]--;
        actor->tics -= 3;
        // don't make tics less than 1
        if(actor->tics < 1)
            actor->tics = 1;

        // Make sure to account for the skill5 tic reset in vanilla Heretic
        // (the tics -> 3 bump was not added in Eternity's A_Chase because that
        // would prevent very fast custom monsters from working properly).
        if(GameModeInfo->flags & GIF_CHASEFAST &&
           (gameskill >= sk_nightmare || fastparm || actor->flags3 & MF3_ALWAYSFAST) //
           && actor->tics < 3)
        {
            actor->tics = 3;
        }
    }

    A_Chase(actionargs);
}

void A_Sor1Pain(actionargs_t *actionargs)
{
    actionargs->actor->counters[0] = 20; // Number of steps to walk fast
    A_Pain(actionargs);
}

void A_Srcr1Attack(actionargs_t *actionargs)
{
    Mobj   *actor = actionargs->actor;
    Mobj   *mo;
    fixed_t mheight    = actor->z + 48 * FRACUNIT;
    int     srcrfxType = E_SafeThingType(MT_SRCRFX1);

    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    // bite attack
    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, ((P_Random(pr_sorc1atk) & 7) + 1) * 8, MOD_HIT);
        return;
    }

    if(actor->health > (actor->getModifiedSpawnHealth() * 2) / 3)
    {
        // regular attack, one fire ball
        P_SpawnMissile(actor, actor->target, srcrfxType, mheight);
    }
    else
    {
        // "limit break": 3 fire balls
        mo            = P_SpawnMissile(actor, actor->target, srcrfxType, mheight);
        fixed_t momz  = mo->momz;
        angle_t angle = mo->angle;
        P_SpawnMissileAngle(actor, srcrfxType, angle - HTICANGLE_1 * 3, momz, mheight);
        P_SpawnMissileAngle(actor, srcrfxType, angle + HTICANGLE_1 * 3, momz, mheight);

        // desperation -- attack twice
        if(actor->health * 3 < actor->getModifiedSpawnHealth())
        {
            if(actor->counters[1])
            {
                actor->counters[1] = 0;
            }
            else
            {
                actor->counters[1] = 1;
                P_SetMobjState(actor, E_SafeState(S_SRCR1_ATK4));
            }
        }
    }
}

//
// A_SorcererRise
//
// Spawns the normal D'Sparil after the Chaos Serpent dies.
//
void A_SorcererRise(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;
    Mobj *mo;
    int   sorc2Type = E_SafeThingType(MT_SORCERER2);

    actor->flags &= ~MF_SOLID;
    mo            = P_SpawnMobj(actor->x, actor->y, actor->z, sorc2Type);
    mo->angle     = actor->angle;

    // transfer friendliness
    P_transferFriendship(*mo, *actor);

    // add to appropriate thread
    mo->updateThinker();

    if(actor->target && !(mo->flags & MF_FRIEND))
        P_SetTarget<Mobj>(&mo->target, actor->target);

    P_SetMobjState(mo, E_SafeState(S_SOR2_RISE1));
}

//=============================================================================
//
// D'Sparil Sound Actions
//
// For Heretic state layout compatibility; for user mods, use A_PlaySoundEx.
//

void A_SorZap(actionargs_t *actionargs)
{
    S_StartSound(nullptr, sfx_sorzap);
}

void A_SorRise(actionargs_t *actionargs)
{
    S_StartSound(nullptr, sfx_sorrise);
}

void A_SorDSph(actionargs_t *actionargs)
{
    S_StartSound(nullptr, sfx_sordsph);
}

void A_SorDExp(actionargs_t *actionargs)
{
    S_StartSound(nullptr, sfx_sordexp);
}

void A_SorDBon(actionargs_t *actionargs)
{
    S_StartSound(nullptr, sfx_sordbon);
}

void A_SorSightSnd(actionargs_t *actionargs)
{
    S_StartSound(nullptr, sfx_sorsit);
}

//=============================================================================
//
// Normal D'Sparil Actions
//

// haleyjd 11/19/02:
// Teleport spots for D'Sparil -- these are more or less identical
// to boss spots used for DOOM II MAP30, and I have generalized
// the code that looks for them so it can be used here (and elsewhere
// as needed). This automatically removes the Heretic boss spot
// limit, of course.

static MobjCollection sorcspots;

void P_SpawnSorcSpots()
{
    sorcspots.mobjType = "DsparilTeleSpot";
    sorcspots.makeEmpty();
    sorcspots.collectThings();
}

void P_DSparilTeleport(Mobj *actor)
{
    bossteleport_t bt;

    bt.mc          = &sorcspots;                       // use sorcspots
    bt.rngNum      = pr_sorctele2;                     // use this rng
    bt.boss        = actor;                            // teleport D'Sparil
    bt.state       = E_SafeState(S_SOR2_TELE1);        // set him to this state
    bt.fxtype      = E_SafeThingType(MT_SOR2TELEFADE); // spawn a DSparil TeleFade
    bt.zpamt       = 0;                                // add 0 to fx z coord
    bt.hereThere   = BOSSTELE_ORIG;                    // spawn fx only at origin
    bt.soundNum    = sfx_htelept;                      // use htic teleport sound
    bt.minDistance = 128 * FRACUNIT;                   // minimum distance

    P_BossTeleport(&bt);
}

void A_Srcr2Decide(actionargs_t *actionargs)
{
    static const int chance[] = { 192, 120, 120, 120, 64, 64, 32, 16, 0 };

    Mobj *actor = actionargs->actor;
    int   index = actor->health / (actor->getModifiedSpawnHealth() / 8);

    // if no spots, no teleportation
    if(sorcspots.isEmpty())
        return;

    if(P_Random(pr_sorctele1) < chance[index])
        P_DSparilTeleport(actor);
}

void A_Srcr2Attack(actionargs_t *actionargs)
{
    Mobj   *actor = actionargs->actor;
    int     chance;
    fixed_t z           = actor->z + actor->info->missileheight;
    int     sor2fx1Type = E_SafeThingType(MT_SOR2FX1);
    int     sor2fx2Type = E_SafeThingType(MT_SOR2FX2);

    if(!actor->target)
        return;

    S_StartSound(nullptr, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        // ouch!
        int dmg = ((P_Random(pr_soratk1) & 7) + 1) * 20;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
        return;
    }

    chance = (actor->health * 2 < actor->getModifiedSpawnHealth()) ? 96 : 48;

    if(P_Random(pr_soratk2) < chance)
    {
        Mobj *mo;

        // spawn wizards -- transfer friendliness
        mo = P_SpawnMissileAngle(actor, sor2fx2Type, actor->angle - ANG45, FRACUNIT / 2, z);
        P_transferFriendship(*mo, *actor);

        mo = P_SpawnMissileAngle(actor, sor2fx2Type, actor->angle + ANG45, FRACUNIT / 2, z);
        P_transferFriendship(*mo, *actor);
    }
    else
    {
        // shoot blue bolt
        P_SpawnMissile(actor, actor->target, sor2fx1Type, z);
    }
}

void A_BlueSpark(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;
    Mobj *mo;
    int   sparkType = E_SafeThingType(MT_SOR2FXSPARK);

    for(int i = 0; i < 2; ++i)
    {
        mo = P_SpawnMobj(actor->x, actor->y, actor->z, sparkType);

        mo->momx = P_SubRandom(pr_bluespark) << 9;
        mo->momy = P_SubRandom(pr_bluespark) << 9;
        mo->momz = FRACUNIT + (P_Random(pr_bluespark) << 8);
    }
}

void A_GenWizard(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;
    Mobj *mo;
    Mobj *fog;
    int   wizType = E_SafeThingType(MT_WIZARD);
    int   fogType = E_SafeThingType(MT_HTFOG);

    mo = P_SpawnMobj(actor->x, actor->y, actor->z - mobjinfo[wizType]->height / 2, wizType);

    // ioanch 20160116: portal aware
    if(!P_CheckPosition(mo, mo->x, mo->y) || !P_CheckFloorCeilingForSpawning(*mo))
    {
        // doesn't fit, so remove it immediately
        mo->remove();
        return;
    }

    // transfer friendliness
    P_transferFriendship(*mo, *actor);

    // add to appropriate thread
    mo->updateThinker();

    // Check for movements.
    if(!P_TryMove(mo, mo->x, mo->y, false))
    {
        // can't move, remove it immediately
        mo->remove();
        return;
    }

    // set this missile object to die
    actor->momx = actor->momy = actor->momz = 0;
    P_SetMobjState(actor, mobjinfo[actor->type]->deathstate);
    actor->flags &= ~MF_MISSILE;

    // spawn a telefog
    fog = P_SpawnMobj(actor->x, actor->y, actor->z, fogType);
    S_StartSound(fog, sfx_htelept);
}

void A_Sor2DthInit(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    actor->counters[0] = 7; // Animation loop counter

    // kill monsters early
    // kill only friends or enemies depending on friendliness
    P_Massacre((actor->flags & MF_FRIEND) ? 1 : 2);
}

void A_Sor2DthLoop(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(--actor->counters[0])
    {
        // Need to loop
        P_SetMobjState(actor, E_SafeState(S_SOR2_DIE4));
    }
}

static const char *kwds_A_HticExplode[] = {
    "default",       //    0
    "dsparilbspark", //    1
    "floorfire",     //    2
    "timebomb",      //    3
};

static argkeywd_t hticexpkwds = { kwds_A_HticExplode, earrlen(kwds_A_HticExplode) };

//
// A_HticExplode
//
// Parameterized pointer, enables several different Heretic
// explosion actions
//
void A_HticExplode(actionargs_t *actionargs)
{
    Mobj      *actor  = actionargs->actor;
    arglist_t *args   = actionargs->args;
    int        damage = 128;

    int action = E_ArgAsKwd(args, 0, &hticexpkwds, 0);

    switch(action)
    {
    case 1: // 1 -- D'Sparil FX1 explosion, random damage
        damage = 80 + (P_Random(pr_sorfx1xpl) & 31);
        break;
    case 2: // 2 -- Maulotaur floor fire, constant 24 damage
        damage = 24;
        break;
    case 3: // 3 -- Time Bomb of the Ancients, special effects
        actor->z += 32 * FRACUNIT;
        actor->backupPosition(); // don't show the bomb popping up
        actor->translucency = FRACUNIT;
        break;
    default: // ???
        break;
    }

    P_RadiusAttack(actor, actor->target, damage, damage, actor->info->mod, 0);

    // ioanch 20160116: portal aware Z
    E_ExplosionHitWater(actor, damage);
}

struct boss_spec_htic_t
{
    unsigned int thing_flag;
    unsigned int level_flag;
    int          flagfield;
};

#define NUM_HBOSS_SPECS 5

static boss_spec_htic_t hboss_specs[NUM_HBOSS_SPECS] = {
    { MF2_E1M8BOSS, BSPEC_E1M8, 2 },
    { MF2_E2M8BOSS, BSPEC_E2M8, 2 },
    { MF2_E3M8BOSS, BSPEC_E3M8, 2 },
    { MF2_E4M8BOSS, BSPEC_E4M8, 2 },
    { MF3_E5M8BOSS, BSPEC_E5M8, 3 },
};

//
// A_HticBossDeath
//
// Heretic boss deaths
//
void A_HticBossDeath(actionargs_t *actionargs)
{
    Mobj    *actor = actionargs->actor;
    Thinker *th;

    // Now check the UMAPINFO bossactions
    // We need a player for this. NOTE: we don't require a living player here.
    const player_t *thePlayer = nullptr;
    int             i;
    for(i = 0; i < MAXPLAYERS; i++)
        if(playeringame[i] && players[i].health > 0)
        {
            thePlayer = players + i;
            break;
        }
    // none alive found
    if(i == MAXPLAYERS || !thePlayer)
        thePlayer = players; // pick player 1, even if dead

    P_CheckCustomBossActions(*actor, *thePlayer);

    for(boss_spec_htic_t &hboss_spec : hboss_specs)
    {
        unsigned int flags = hboss_spec.flagfield == 2 ? actor->flags2 : actor->flags3;

        // to activate a special, the thing must be a boss that triggers
        // it, and the map must have the special enabled.
        if((flags & hboss_spec.thing_flag) && (LevelInfo.bossSpecs & hboss_spec.level_flag))
        {
            for(th = thinkercap.next; th != &thinkercap; th = th->next)
            {
                Mobj *mo;
                if((mo = thinker_cast<Mobj *>(th)))
                {
                    unsigned int moflags = hboss_spec.flagfield == 2 ? mo->flags2 : mo->flags3;
                    if(mo != actor && (moflags & hboss_spec.thing_flag) && mo->health > 0)
                        return; // other boss not dead
                }
            }

            // victory!
            switch(hboss_spec.level_flag)
            {
            default:
            case BSPEC_E2M8:
            case BSPEC_E3M8:
            case BSPEC_E4M8:
            case BSPEC_E5M8:
                // if a friendly boss dies, kill only friends
                // if an enemy boss dies, kill only enemies
                P_Massacre((actor->flags & MF_FRIEND) ? 1 : 2);

                // fall through
            case BSPEC_E1M8: //
                EV_DoFloor(nullptr, 666, lowerFloor);
                break;
            } // end switch
        } // end if
    } // end for
}

//=============================================================================
//
// Pods and Pod Generators
//

void A_PodPain(actionargs_t *actionargs)
{
    int   count;
    int   chance;
    Mobj *actor = actionargs->actor;
    Mobj *goo;
    int   gooType = E_SafeThingType(MT_PODGOO);

    chance = P_Random(pr_podpain);

    if(chance < 128)
        return;

    count = (chance > 240) ? 2 : 1;

    for(int i = 0; i < count; ++i)
    {
        goo = P_SpawnMobj(actor->x, actor->y, actor->z + 48 * FRACUNIT, gooType);
        P_SetTarget<Mobj>(&goo->target, actor);

        goo->momx = P_SubRandom(pr_podpain) << 9;
        goo->momy = P_SubRandom(pr_podpain) << 9;
        goo->momz = (FRACUNIT >> 1) + (P_Random(pr_podpain) << 9);
    }
}

void A_RemovePod(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    // actor->tracer points to the generator that made this pod --
    // this method is save game safe and doesn't require any new
    // fields

    if(actor->tracer)
    {
        if(actor->tracer->counters[0] > 0)
            actor->tracer->counters[0]--;
    }
}

// Note on MAXGENPODS: unlike the limit on PE lost souls, this
// limit makes inarguable sense. If you remove it, areas with
// pod generators will become so crowded with pods that they'll
// begin flying around the map like mad. So, this limit isn't a
// good candidate for removal; it's a necessity.

#define MAXGENPODS 16

void A_MakePod(actionargs_t *actionargs)
{
    angle_t angle;
    fixed_t move;
    Mobj   *actor = actionargs->actor;
    Mobj   *mo;
    fixed_t x, y;

    // limit pods per generator to avoid crowding, slow-down
    if(actor->counters[0] >= MAXGENPODS)
        return;

    x = actor->x;
    y = actor->y;

    mo = P_SpawnMobj(x, y, ONFLOORZ, E_SafeThingType(MT_POD));
    if(!P_CheckPosition(mo, x, y))
    {
        mo->remove();
        return;
    }

    P_SetMobjState(mo, E_SafeState(S_POD_GROW1));
    S_StartSound(mo, sfx_newpod);

    // give the pod some random momentum
    angle = P_Random(pr_makepod) << 24;
    move  = 9 * FRACUNIT >> 1;

    P_ThrustMobj(mo, angle, move);

    // use tracer field to link pod to generator, and increment
    // generator's pod count
    P_SetTarget<Mobj>(&mo->tracer, actor);
    actor->counters[0]++;
}

//=============================================================================
//
// Volcano Actions
//

//
// A_VolcanoSet
//
// For Heretic state layout compatibility. A more general pointer is A_SetTics.
//
void A_VolcanoSet(actionargs_t *actionargs)
{
    actionargs->actor->tics = 105 + (P_Random(pr_settics) & 127);
}

//
// A_VolcanoBlast
//
// Called when a volcano is ready to erupt.
//
void A_VolcanoBlast(actionargs_t *actionargs)
{
    int     ballType = E_SafeThingType(MT_VOLCANOBLAST);
    int     i, numvolcballs;
    Mobj   *actor = actionargs->actor;
    Mobj   *volcball;
    angle_t angle;

    // spawn 1 to 3 volcano balls
    numvolcballs = (P_Random(pr_volcano) % 3) + 1;

    for(i = 0; i < numvolcballs; ++i)
    {
        volcball = P_SpawnMobj(actor->x, actor->y, actor->z + 44 * FRACUNIT, ballType);
        P_SetTarget<Mobj>(&volcball->target, actor);
        S_StartSound(volcball, sfx_bstatk);

        // shoot at a random angle
        volcball->angle = P_Random(pr_volcano) << 24;
        angle           = volcball->angle >> ANGLETOFINESHIFT;

        // give it some momentum
        volcball->momx = finecosine[angle];
        volcball->momy = finesine[angle];
        volcball->momz = (5 * FRACUNIT >> 1) + (P_Random(pr_volcano) << 10);

        // check if it hit something immediately
        P_CheckMissileSpawn(volcball);
    }
}

//
// A_VolcBallImpact
//
// Called when a volcano ball hits something.
//
void A_VolcBallImpact(actionargs_t *actionargs)
{
    int     sballType = E_SafeThingType(MT_VOLCANOTBLAST);
    Mobj   *actor     = actionargs->actor;
    Mobj   *svolcball;
    angle_t angle;

    // if the thing hit the floor, move it up so that the little
    // volcano balls don't hit the floor immediately
    if(actor->z <= actor->zref.floor)
    {
        actor->flags  |= MF_NOGRAVITY;
        actor->flags2 &= ~MF2_LOGRAV;
        actor->z      += 28 * FRACUNIT;
    }

    // do some radius damage
    P_RadiusAttack(actor, actor->target, 25, 25, actor->info->mod, 0);

    // spawn 4 little volcano balls
    for(unsigned int i = 0; i < 4; ++i)
    {
        svolcball = P_SpawnMobj(actor->x, actor->y, actor->z, sballType);

        // pass on whatever shot the original volcano ball
        P_SetTarget<Mobj>(&svolcball->target, actor->target);

        svolcball->angle = i * ANG90;
        angle            = svolcball->angle >> ANGLETOFINESHIFT;

        // give them some momentum
        svolcball->momx = FixedMul(7 * FRACUNIT / 10, finecosine[angle]);
        svolcball->momy = FixedMul(7 * FRACUNIT / 10, finesine[angle]);
        svolcball->momz = FRACUNIT + (P_Random(pr_svolcano) << 9);

        // check if it hit something immediately
        P_CheckMissileSpawn(svolcball);
    }
}

//=============================================================================
//
// Knight Actions
//

//
// A_KnightAttack
//
// Shoots one of two missiles, depending on whether a Knight
// Ghost or some other object uses it.
//
void A_KnightAttack(actionargs_t *actionargs)
{
    Mobj *actor      = actionargs->actor;
    int   ghostType  = E_ThingNumForDEHNum(MT_KNIGHTGHOST);
    int   axeType    = E_SafeThingType(MT_KNIGHTAXE);
    int   redAxeType = E_SafeThingType(MT_REDAXE);

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        int dmg = ((P_Random(pr_knightat1) & 7) + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
        S_StartSound(actor, sfx_kgtat2);
    }
    else
    {
        S_StartSound(actor, actor->info->attacksound);

        if(actor->type == ghostType || P_Random(pr_knightat2) < 40)
        {
            P_SpawnMissile(actor, actor->target, redAxeType, actor->z + 36 * FRACUNIT);
        }
        else
        {
            P_SpawnMissile(actor, actor->target, axeType, actor->z + 36 * FRACUNIT);
        }
    }
}

//
// A_DripBlood
//
// Throws some Heretic blood objects out from the source thing.
//
void A_DripBlood(actionargs_t *actionargs)
{
    Mobj   *actor = actionargs->actor;
    Mobj   *mo;
    fixed_t x, y;

    y = actor->y + (P_SubRandom(pr_dripblood) << 11);
    x = actor->x + (P_SubRandom(pr_dripblood) << 11);

    mo = P_SpawnMobj(x, y, actor->z, E_SafeThingType(MT_HTICBLOOD));

    mo->momx = P_SubRandom(pr_dripblood) << 10;
    mo->momy = P_SubRandom(pr_dripblood) << 10;

    mo->flags2 |= MF2_LOGRAV;
}

//=============================================================================
//
// Beast Actions
//

void A_BeastAttack(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        int dmg = ((P_Random(pr_beastbite) & 7) + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
    }
    else
        P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_BEASTBALL), actor->z + actor->info->missileheight);
}

void A_BeastPuff(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(P_Random(pr_puffy) > 64)
    {
        fixed_t x, y, z;
        Mobj   *mo;

        z = actor->z + (P_SubRandom(pr_puffy) << 10);
        y = actor->y + (P_SubRandom(pr_puffy) << 10);
        x = actor->x + (P_SubRandom(pr_puffy) << 10);

        mo = P_SpawnMobj(x, y, z, E_SafeThingType(MT_PUFFY));

        // pass on the beast so that it doesn't hurt itself
        P_SetTarget<Mobj>(&mo->target, actor->target);
    }
}

//=============================================================================
//
// Ophidian Actions
//

void A_SnakeAttack(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
    {
        // avoid going through other attack frames if target is gone
        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    S_StartSound(actor, actor->info->attacksound);
    A_FaceTarget(actionargs);
    P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_SNAKEPRO_A), actor->z + actor->info->missileheight);
}

void A_SnakeAttack2(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
    {
        // avoid going through other attack frames if target is gone
        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    S_StartSound(actor, actor->info->attacksound);
    A_FaceTarget(actionargs);
    P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_SNAKEPRO_B), actor->z + actor->info->missileheight);
}

//=============================================================================
//
// Maulotaur Actions
//

//
// A_MinotaurAtk1
//
// Maulotaur melee attack. Big hammer, squishes player.
//
void A_MinotaurAtk1(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
        return;

    S_StartSound(actor, sfx_stfpow);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, ((P_Random(pr_minatk1) & 7) + 1) * 4, MOD_HIT);

        // if target is player, make the viewheight go down
        player_t *player = actor->target->player;
        if(player != nullptr)
            player->deltaviewheight = -16 * FRACUNIT;
    }
}

//
// P_CheckMntrCharge
//
// Returns true if the Maulotaur should do a charge attack.
//
inline static bool P_CheckMntrCharge(fixed_t dist, Mobj *actor, Mobj *target)
{
    return (target->z + target->height > actor->z && // check heights
            target->z + target->height < actor->z + actor->height && dist > 64 * FRACUNIT &&
            dist < 512 * FRACUNIT &&     // check distance
            P_Random(pr_mindist) < 150); // random factor
}

//
// P_CheckFloorFire
//
// Returns true if the Maulotaur should use floor fire.
//
inline static bool P_CheckFloorFire(fixed_t dist, Mobj *target)
{
    return (target->z == target->zref.floor && // target on floor?
            dist < 576 * FRACUNIT &&           // target in range?
            P_Random(pr_mindist) < 220         // random factor
    );
}

//
// A_MinotaurDecide
//
// Picks a Maulotaur attack.
//
void A_MinotaurDecide(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;
    Mobj *target;
    int   dist;

    if(!(target = actor->target))
        return;

    S_StartSound(actor, sfx_minsit);

    dist = P_AproxDistance(actor->x - getTargetX(actor), actor->y - getTargetY(actor));

    // charge attack
    if(P_CheckMntrCharge(dist, actor, target))
    {
        A_FaceTarget(actionargs);

        // set to charge state and start skull-flying
        P_SetMobjStateNF(actor, E_SafeState(S_MNTR_ATK4_1));
        actor->flags    |= MF_SKULLFLY;
        actor->intflags |= MIF_SKULLFLYSEE;

        // give him momentum
        angle_t angle = actor->angle >> ANGLETOFINESHIFT;
        actor->momx   = FixedMul(13 * FRACUNIT, finecosine[angle]);
        actor->momy   = FixedMul(13 * FRACUNIT, finesine[angle]);

        // set a timer
        actor->counters[0] = TICRATE >> 1;
    }
    else if(P_CheckFloorFire(dist, target))
    {
        // floor fire
        P_SetMobjState(actor, E_SafeState(S_MNTR_ATK3_1));
        actor->counters[1] = 0;
    }
    else
        A_FaceTarget(actionargs);

    // Fall through to swing attack
}

//
// A_MinotaurCharge
//
// Called while the Maulotaur is charging.
//
void A_MinotaurCharge(actionargs_t *actionargs)
{
    int   puffType = E_SafeThingType(MT_PHOENIXPUFF);
    Mobj *actor    = actionargs->actor;

    if(actor->counters[0]) // test charge timer
    {
        // spawn some smoke and count down the charge
        Mobj *puff = P_SpawnMobj(actor->x, actor->y, actor->z, puffType);
        puff->momz = 2 * FRACUNIT;
        --actor->counters[0];
    }
    else
    {
        // end of the charge
        actor->flags    &= ~MF_SKULLFLY;
        actor->intflags &= ~MIF_SKULLFLYSEE;
        P_SetMobjState(actor, actor->info->seestate);
    }
}

//
// A_MinotaurAtk2
//
// Fireball attack for Maulotaur
//
void A_MinotaurAtk2(actionargs_t *actionargs)
{
    int   mntrfxType = E_SafeThingType(MT_MNTRFX1);
    Mobj *actor      = actionargs->actor;
    Mobj *mo;

    if(!actor->target)
        return;

    S_StartSound(actor, sfx_minat2);

    if(P_CheckMeleeRange(actor)) // hit directly
    {
        P_DamageMobj(actor->target, actor, actor, ((P_Random(pr_minatk2) & 7) + 1) * 5, MOD_HIT);
    }
    else // missile spread attack
    {
        fixed_t z = actor->z + 40 * FRACUNIT;

        // shoot a missile straight
        mo = P_SpawnMissile(actor, actor->target, mntrfxType, z);
        S_StartSound(mo, sfx_minat2);

        // shoot 4 more missiles in a spread
        fixed_t momz  = mo->momz;
        angle_t angle = mo->angle;
        P_SpawnMissileAngle(actor, mntrfxType, angle - (ANG45 / 8), momz, z);
        P_SpawnMissileAngle(actor, mntrfxType, angle + (ANG45 / 8), momz, z);
        P_SpawnMissileAngle(actor, mntrfxType, angle - (ANG45 / 16), momz, z);
        P_SpawnMissileAngle(actor, mntrfxType, angle + (ANG45 / 16), momz, z);
    }
}

//
// A_MinotaurAtk3
//
// Performs floor fire attack, or melee if in range.
//
void A_MinotaurAtk3(actionargs_t *actionargs)
{
    int   mntrfxType = E_SafeThingType(MT_MNTRFX2);
    Mobj *actor      = actionargs->actor;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, ((P_Random(pr_minatk3) & 7) + 1) * 5, MOD_HIT);

        // if target is player, decrease viewheight
        player_t *player = actor->target->player;
        if(player != nullptr)
            player->deltaviewheight = -16 * FRACUNIT;
    }
    else
    {
        // floor fire attack
        Mobj *mo = P_SpawnMissile(actor, actor->target, mntrfxType, ONFLOORZ);
        S_StartSound(mo, sfx_minat1);
    }

    if(P_Random(pr_minatk3) < 192 && actor->counters[1] == 0)
    {
        P_SetMobjState(actor, E_SafeState(S_MNTR_ATK3_4));
        actor->counters[1] = 1;
    }
}

//
// A_MntrFloorFire
//
// Called by floor fire missile as it moves.
// Spawns small burning flames.
//
void A_MntrFloorFire(actionargs_t *actionargs)
{
    int     mntrfxType = E_SafeThingType(MT_MNTRFX3);
    Mobj   *actor      = actionargs->actor;
    Mobj   *mo;
    fixed_t x, y;

    // set actor to floor
    actor->z = actor->zref.floor;

    // determine spawn coordinates for small flame
    x = actor->x + (P_SubRandom(pr_mffire) << 10);
    y = actor->y + (P_SubRandom(pr_mffire) << 10);

    // spawn the flame
    mo = P_SpawnMobj(x, y, ONFLOORZ, mntrfxType);

    // pass on the Maulotaur as the source of damage
    P_SetTarget<Mobj>(&mo->target, actor->target);

    // give it a bit of momentum and then check to see if it hit something
    mo->momx = 1;
    P_CheckMissileSpawn(mo);
}

//=============================================================================
//
// Iron Lich Actions
//

//
// A_LichFire
//
// Spawns a column of expanding fireballs. Called by A_LichAttack,
// but also available separately.
//
void A_LichFire(actionargs_t *actionargs)
{
    int   headfxType, frameNum;
    Mobj *actor = actionargs->actor;
    Mobj *target, *baseFire, *fire;
    int   i;

    headfxType = E_SafeThingType(MT_LICHFX3);
    frameNum   = E_SafeState(S_LICHFX3_4);

    if(!(target = actor->target))
        return;

    // spawn the parent fireball
    baseFire = P_SpawnMissile(actor, target, headfxType, actor->z + actor->info->missileheight);

    // set it to S_HEADFX3_4 so that it doesn't grow
    P_SetMobjState(baseFire, frameNum);

    S_StartSound(actor, sfx_hedat1);

    for(i = 0; i < 5; i++)
    {
        fire = P_SpawnMobj(baseFire->x, baseFire->y, baseFire->z, headfxType);

        // pass on the lich as the originator
        P_SetTarget<Mobj>(&fire->target, baseFire->target);

        // inherit the motion properties of the parent fireball
        fire->angle = baseFire->angle;
        fire->momx  = baseFire->momx;
        fire->momy  = baseFire->momy;
        fire->momz  = baseFire->momz;

        // start out with zero damage
        fire->damage  = 0;
        fire->flags4 |= MF4_NOZERODAMAGE;

        // set a counter for growth
        fire->counters[0] = (i + 1) << 1;

        P_CheckMissileSpawn(fire);
    }
}

//
// A_LichWhirlwind
//
// Spawns a heat-seeking tornado. Called by A_LichAttack, but also
// available separately.
//
void A_LichWhirlwind(actionargs_t *actionargs)
{
    int   wwType = E_SafeThingType(MT_WHIRLWIND);
    Mobj *mo, *target;
    Mobj *actor = actionargs->actor;

    if(!(target = actor->target))
        return;

    mo = P_SpawnMissile(actor, target, wwType, actor->z);

    // use mo->tracer to track target
    P_SetTarget<Mobj>(&mo->tracer, target);

    mo->counters[0] = 20 * TICRATE; // duration
    mo->counters[1] = 50;           // timer for active sound
    mo->counters[2] = 60;           // explocount limit

    S_StartSound(actor, sfx_hedat3);
}

//
// A_LichAttack
//
// Main Iron Lich attack logic.
//
void A_LichAttack(actionargs_t *actionargs)
{
    int   fxType = E_SafeThingType(MT_LICHFX1);
    Mobj *actor  = actionargs->actor;
    Mobj *target;
    int   randAttack, dist;

    // Distance threshold = 512 units
    // Probabilities:
    // Attack       Close   Far
    // -----------------------------
    // Ice ball       20%   60%
    // Fire column    40%   20%
    // Whirlwind      40% : 20%

    if(!(target = actor->target))
        return;

    A_FaceTarget(actionargs);

    // hit directly when in melee range
    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(target, actor, actor, ((P_Random(pr_lichmelee) & 7) + 1) * 6, MOD_HIT);
        return;
    }

    // determine distance and use it to alter attack probabilities
    dist = P_AproxDistance(actor->x - getTargetX(actor), actor->y - getTargetY(actor)) > 512 * FRACUNIT;

    randAttack = P_Random(pr_lichattack);

    if(randAttack < (dist ? 150 : 50))
    {
        // ice attack
        P_SpawnMissile(actor, target, fxType, actor->z + actor->info->missileheight);
        S_StartSound(actor, sfx_hedat2);
    }
    else if(randAttack < (dist ? 200 : 150))
        A_LichFire(actionargs);
    else
        A_LichWhirlwind(actionargs);
}

//
// A_WhirlwindSeek
//
// Special homing maintenance pointer for whirlwinds.
//
void A_WhirlwindSeek(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    // decrement duration counter
    if((actor->counters[0] -= 3) < 0)
    {
        actor->momx = actor->momy = actor->momz = 0;
        P_SetMobjState(actor, actor->info->deathstate);
        actor->flags &= ~MF_MISSILE;
        return;
    }

    // decrement active sound counter
    if((actor->counters[1] -= 3) < 0)
    {
        actor->counters[1] = 58 + (P_Random(pr_whirlseek) & 31);
        S_StartSound(actor, sfx_hedat3);
    }

    // test if tracer has become an invalid target
    if(actor->tracer && (actor->tracer->flags3 & MF3_GHOST || actor->tracer->health < 0))
    {
        Mobj *originator = actor->target;
        Mobj *origtarget = originator ? originator->target : nullptr;

        // See if the Lich has a new target; if so, maybe chase it now.
        // This keeps the tornado from sitting around uselessly.
        if(originator && origtarget && actor->tracer != origtarget && //
           origtarget->health > 0 &&                                  //
           !(origtarget->flags3 & MF3_GHOST) &&                       //
           !(originator->flags & origtarget->flags & MF_FRIEND))
            P_SetTarget<Mobj>(&actor->tracer, origtarget);
        else
            return;
    }

    // follow the target
    P_HticTracer(actor, HTICANGLE_1 * 10, HTICANGLE_1 * 30);
}

//
// A_LichIceImpact
//
// Called when a Lich ice ball hits something. Shatters into
// shards that fly in all directions.
//
void A_LichIceImpact(actionargs_t *actionargs)
{
    int     fxType = E_SafeThingType(MT_LICHFX2);
    angle_t angle;
    Mobj   *actor = actionargs->actor;
    Mobj   *shard;

    for(unsigned int i = 0; i < 8; ++i)
    {
        shard = P_SpawnMobj(actor->x, actor->y, actor->z, fxType);
        P_SetTarget<Mobj>(&shard->target, actor->target);

        // send shards out every 45 degrees
        shard->angle = i * ANG45;

        // set momenta
        angle       = shard->angle >> ANGLETOFINESHIFT;
        shard->momx = FixedMul(shard->info->speed, finecosine[angle]);
        shard->momy = FixedMul(shard->info->speed, finesine[angle]);
        shard->momz = -3 * FRACUNIT / 5;

        // check the spawn to see if it hit immediately
        P_CheckMissileSpawn(shard);
    }
}

//
// A_LichFireGrow
//
// Called by Lich fire pillar fireballs so that they can expand.
//
void A_LichFireGrow(actionargs_t *actionargs)
{
    int   frameNum = E_SafeState(S_LICHFX3_4);
    Mobj *actor    = actionargs->actor;

    actor->z += 9 * FRACUNIT;

    if(--actor->counters[0] == 0) // count down growth timer
    {
        actor->damage = actor->info->damage; // restore normal damage
        P_SetMobjState(actor, frameNum);     // don't grow any more
    }
}

//=============================================================================
//
// Imp Actions
//

//
// A_ImpChargeAtk
//
// Almost identical to the Lost Soul's attack, but adds a frequent
// failure to attack so that the imps do not constantly charge.
// Note that this makes them nearly paralyzed in "Black Plague"
// skill level, however...
//
void A_ImpChargeAtk(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target || P_Random(pr_impcharge) > 64)
    {
        P_SetMobjState(actor, actor->info->seestate);
    }
    else
    {
        S_StartSound(actor, actor->info->attacksound);

        P_SkullFly(actor, 12 * FRACUNIT, true);
    }
}

//
// A_ImpMeleeAtk
//
void A_ImpMeleeAtk(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random(pr_impmelee) & 7), MOD_HIT);
    }
}

//
// A_ImpMissileAtk
//
// Leader Imp's missile/melee attack
//
void A_ImpMissileAtk(actionargs_t *actionargs)
{
    int   fxType = E_SafeThingType(MT_IMPBALL);
    Mobj *actor  = actionargs->actor;

    if(!actor->target)
        return;

    S_StartSound(actor, actor->info->attacksound);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random(pr_impmelee2) & 7), MOD_HIT);
    }
    else
        P_SpawnMissile(actor, actor->target, fxType, actor->z + actor->info->missileheight);
}

//
// A_ImpDeath
//
// Called when the imp dies normally.
//
void A_ImpDeath(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    actor->flags  &= ~MF_SOLID;
    actor->flags2 |= MF2_FOOTCLIP;

    if(actor->z <= actor->zref.floor && actor->info->crashstate != NullStateNum)
    {
        actor->intflags |= MIF_CRASHED;
        P_SetMobjState(actor, actor->info->crashstate);
    }
}

//
// A_ImpXDeath1
//
// Called on imp extreme death. First half of action
//
void A_ImpXDeath1(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    actor->flags  &= ~MF_SOLID;
    actor->flags  |= MF_NOGRAVITY;
    actor->flags2 |= MF2_FOOTCLIP;

    // set special1 so the crashstate goes to the
    // extreme crash death
    actor->counters[0] = 666;
}

//
// A_ImpXDeath2
//
// Called on imp extreme death. Second half of action.
//
void A_ImpXDeath2(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    actor->flags &= ~MF_NOGRAVITY;

    if(actor->z <= actor->zref.floor && actor->info->crashstate != NullStateNum)
    {
        actor->intflags |= MIF_CRASHED;
        P_SetMobjState(actor, actor->info->crashstate);
    }
}

//
// A_ImpExplode
//
// Called from imp crashstate.
//
void A_ImpExplode(actionargs_t *actionargs)
{
    int   fxType1, fxType2, stateNum;
    Mobj *actor = actionargs->actor;
    Mobj *mo;

    // haleyjd 09/13/04: it's possible for an imp to enter its
    // crash state between calls to ImpXDeath1 and ImpXDeath2 --
    // if this happens, the NOGRAVITY flag must be cleared here,
    // or else it will remain indefinitely.

    actor->flags &= ~MF_NOGRAVITY;

    fxType1  = E_SafeThingType(MT_IMPCHUNK1);
    fxType2  = E_SafeThingType(MT_IMPCHUNK2);
    stateNum = E_SafeState(S_IMP_XCRASH1);

    mo       = P_SpawnMobj(actor->x, actor->y, actor->z, fxType1);
    mo->momx = P_SubRandom(pr_impcrash) << 10;
    mo->momy = P_SubRandom(pr_impcrash) << 10;
    mo->momz = 9 * FRACUNIT;

    mo       = P_SpawnMobj(actor->x, actor->y, actor->z, fxType2);
    mo->momx = P_SubRandom(pr_impcrash) << 10;
    mo->momy = P_SubRandom(pr_impcrash) << 10;
    mo->momz = 9 * FRACUNIT;

    // extreme death crash
    if(actor->counters[0] == 666)
        P_SetMobjState(actor, stateNum);
}

//=============================================================================
//
// Shared Routines
//

//
// A_ContMobjSound
//
// For Heretic state layout compatibility only; plays specific sounds for
// specific objects.
//
void A_ContMobjSound(actionargs_t *actionargs)
{
    Mobj *actor = actionargs->actor;

    if(actor->type == E_ThingNumForName("KnightAxe"))
        S_StartSoundName(actor, "ht_kgtatk");
    else if(actor->type == E_ThingNumForName("GolemShot"))
        S_StartSoundName(actor, "ht_mumhed");
}

void A_ESound(actionargs_t *actionargs)
{
    Mobj *mo = actionargs->actor;

    if(mo->type == E_ThingNumForName("HereticAmbienceWater"))
        S_StartSoundName(mo, "ht_waterfl");
    else if(mo->type == E_ThingNumForName("HereticAmbienceWind"))
        S_StartSoundName(mo, "ht_wind");
}

void A_FlameSnd(actionargs_t *actionargs)
{
    S_StartSound(actionargs->actor, sfx_hedat1); // Burn sound
}

//=============================================================================
//
// Player Projectiles
//

//
// A_PhoenixPuff
//
// Parameters:
// * args[0] : thing type
//
void A_PhoenixPuff(actionargs_t *actionargs)
{
    int     thingtype = E_SafeThingName("HereticPhoenixPuff");
    Mobj   *actor     = actionargs->actor;
    Mobj   *puff;
    angle_t angle;

    P_HticTracer(actor, HTICANGLE_1 * 5, HTICANGLE_1 * 10);

    puff = P_SpawnMobj(actor->x, actor->y, actor->z, thingtype);

    angle        = actor->angle + ANG90;
    angle      >>= ANGLETOFINESHIFT;
    puff->momx   = FixedMul(13 * FRACUNIT / 10, finecosine[angle]);
    puff->momy   = FixedMul(13 * FRACUNIT / 10, finesine[angle]);

    puff = P_SpawnMobj(actor->x, actor->y, actor->z, thingtype);

    angle        = actor->angle - ANG90;
    angle      >>= ANGLETOFINESHIFT;
    puff->momx   = FixedMul(13 * FRACUNIT / 10, finecosine[angle]);
    puff->momy   = FixedMul(13 * FRACUNIT / 10, finesine[angle]);
}

void A_FlameEnd(actionargs_t *actionargs)
{
    actionargs->actor->momz += static_cast<fixed_t>(1.5 * FRACUNIT);
}

void A_FloatPuff(actionargs_t *actionargs)
{
    actionargs->actor->momz += static_cast<fixed_t>(1.8 * FRACUNIT);
}

//=============================================================================
//
// Player Projectiles
//

void A_Feathers(actionargs_t *actionargs)
{
    Mobj *actor   = actionargs->actor;
    int   feather = E_SafeThingName("Feather");

    int count = actor->health > 0 ? //
                    P_Random(pr_feathers) < 32 ? 2 : 1 :
                    5 + (P_Random(pr_feathers) & 3);
    for(int i = 0; i < count; ++i)
    {
        Mobj *mo = P_SpawnMobj(actor->x, actor->y, actor->z + 20 * FRACUNIT, feather);
        P_SetTarget(&mo->target, actor);
        mo->momx = P_SubRandom(pr_feathers) << 8;
        mo->momy = P_SubRandom(pr_feathers) << 8;
        mo->momz = FRACUNIT + (P_SubRandom(pr_feathers) << 9);
        // Randomize state
        statenum_t snum;
        int        addrandom = P_Random(pr_feathers) & 7;

        int j;
        for(snum = mo->info->spawnstate, j = 0; j < addrandom && snum != NullStateNum; ++j)
        {
            const state_t *cur = states[snum];
            if(cur)
                snum = cur->nextstate;
        }
        if(snum != NullStateNum && snum != mo->info->spawnstate)
            P_SetMobjState(mo, snum);
    }
}

// EOF

