// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Main bot thinker
//
//-----------------------------------------------------------------------------

#include <queue>
#include "../z_zone.h"

#include "b_analysis.h"
#include "b_itemlearn.h"
#include "b_statistics.h"
#include "b_think.h"
#include "b_trace.h"
#include "b_util.h"
#include "b_vocabulary.h"
#include "../cam_sight.h"
#include "../d_event.h"
#include "../d_gi.h"
#include "../doomstat.h"
#include "../e_edf.h"
#include "../e_exdata.h"
#include "../e_inventory.h"
#include "../e_player.h"
#include "../e_states.h"
#include "../e_weapons.h"
#include "../ev_specials.h"
#include "../hu_stuff.h"
#include "../in_lude.h"
#include "../m_compare.h"
#include "../m_qstr.h"
#include "../p_spec.h"
#include "../r_state.h"

void A_WeaponReady(actionargs_t *);

enum
{
    // 1 / (1 - f) where f is ORIG_FRICTION. Multiply this with velocity to
    // obtain the distance the object will move before stopping while on ground
    DRIFT_TIME = (fixed_t)(0xffffffffll / (FRACUNIT - ORIG_FRICTION)),
    DRIFT_TIME_INV = FRACUNIT - ORIG_FRICTION,

   URGENT_CHAT_INTERVAL_SEC = 20,
   IDLE_CHAT_INTERVAL_SEC = 300, // Five minutes
   DUNNO_CONCESSION_SEC = 3,
   DEATH_REVIVE_INTERVAL = 128,

   WEAPONCHANGE_HYST_NUM = 3,
   WEAPONCHANGE_HYST_DEN = 2,
};

enum
{
    SearchStage_Normal,
    SearchStage_ExitSecret,
    SearchStage_ExitNormal,
    SearchStage_PitItems,
};

// The commands that the bots will send to the players to be added in G_Ticker
Bot bots[MAXPLAYERS];

//
// Bot::mapInit
//
// Initialize bot for new map. Mostly cleanup stuff from previous session
//
void Bot::mapInit()
{
   B_EmptyTableAndDelete(goalTable);   // remove all objectives
   B_EmptyTableAndDelete(goalEvents);  // remove all previously listed events
   prevCtr = 0;
   m_searchstage = SearchStage_Normal;

   m_finder.SetMap(botMap);
   m_finder.SetPlayer(pl);
   m_hasPath = false;
    
    m_lastPathSS = nullptr;

   m_deepTriedLines.clear();
   m_deepSearchMode = DeepNormal;
   m_deepAvailSsectors.clear();
   m_deepRepeat = nullptr;
    m_justGotLost = false;
    m_intoSwitch = false;
    m_goalTimer = 0;
    m_dropSS.clear();
    
    justPunched = 0;
    m_lastPosition.x = pl->mo->x;
    m_lastPosition.y = pl->mo->y;
    m_currentTargetMobj = nullptr;

   m_exitDelay = 0;
   m_lastHelpCry = 0;
   m_lastDunnoMessage = 0;
   m_lastExitMessage = 0;
}

//
// Bot::capCommands
//
// Limits all movement tic commands within "legal" values, to prevent human's
// tic command from being added to bot's, resulting in an otherwise impossible
// to achieve running speed
//
void Bot::capCommands()
{
   fixed_t fm = pl->pclass->forwardmove[1];
   
   if(cmd->forwardmove > fm)      cmd->forwardmove = fm;
   else if(cmd->forwardmove < -fm)      cmd->forwardmove = -fm;
   
   if(cmd->sidemove > fm)      cmd->sidemove = fm;
   else if(cmd->sidemove < -fm)      cmd->sidemove = -fm;

   // Fix the SSG choice in vanilla mode
   if(demo_compatibility && (cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT ==
      wp_supershotgun)
   {
      cmd->buttons &= ~BT_WEAPONMASK;
      cmd->buttons |= wp_shotgun << BT_WEAPONSHIFT;
   }
}

//
// Bot::goalAchieved
//
// Returns true if current goal has been noticed in event table. Removes all
// unsought events
//
bool Bot::goalAchieved()
{
   v2fixed_t goalcoord;
   MetaV2Fixed *metaob = nullptr;
   
   if(!goalTable.getNumItems())
      return true;   // no goal existing, so just cancel trip
   
   while ((metaob = goalEvents.getNextTypeEx<MetaV2Fixed>(nullptr)))
   {
      goalcoord = goalTable.getV2Fixed(metaob->getKey(), B_MakeV2Fixed(D_MAXINT, D_MAXINT));

      if(goalcoord == metaob->getValue())
      {
         // found a goal with the event's key and type
         // now do a case-by-case test
         B_EmptyTableAndDelete(goalEvents);
         B_EmptyTableAndDelete(goalTable);
         return true;
      }
      goalEvents.removeObject(metaob);
      delete metaob;
   }
   return false;
}

PathResult Bot::reachableItem(const BSubsec& ss, void* v)
{
    Bot& self = *(Bot*)v;
    BotPathEnd dummy;

    bool result = objOfInterest(ss, dummy, &self);

	// for DeepCheckLosses objOfInterest will always return false
    return result ? (self.m_deepSearchMode == DeepBeyond ? PathDone : PathAdd) : PathNo;
}

bool Bot::checkDeadEndTrap(const BSubsec& targss)
{
    if(m_searchstage >= SearchStage_PitItems || m_deepSearchMode != DeepNormal
       || ss == &targss)
    {
        return true;
    }

    LevelStateStack::Clear();
    m_deepAvailSsectors.clear();

    m_deepSearchMode = DeepAvail;
    m_finder.AvailableGoals(*ss, &m_deepAvailSsectors, reachableItem, this);
    m_deepSearchMode = DeepCheckLosses;
    m_finder.AvailableGoals(targss, nullptr, reachableItem, this);
    m_deepSearchMode = DeepNormal;

    LevelStateStack::Clear();

    return m_deepAvailSsectors.empty();
}

bool Bot::shouldUseSpecial(const line_t& line, const BSubsec& liness)
{
    VanillaLineSpecial vls = static_cast<VanillaLineSpecial>(line.special);
    switch (vls)
    {
        // sure goals
    case VLS_S1ExitLevel:
    case VLS_WRExitLevel:
          if(m_searchstage >= SearchStage_ExitNormal)
          {
             if(shouldChat(URGENT_CHAT_INTERVAL_SEC, m_lastExitMessage))
             {
                m_lastExitMessage = gametic;
                HU_Say(pl, "I'm going to the exit now!");
             }
             return true;
          }
        return false;
    case VLS_S1SecretExit:
    case VLS_WRSecretExit:
          if(m_searchstage >= SearchStage_ExitSecret)
          {
             if(shouldChat(URGENT_CHAT_INTERVAL_SEC, m_lastExitMessage))
             {
                m_lastExitMessage = gametic;
                HU_Say(pl, "Let's take the secret exit!");
             }
             return true;
          }
        return false;

        // would only block or cause harm
    case VLS_W1CloseDoor:
//    case VLS_W1FastCeilCrushRaise:
    case VLS_W1CloseDoor30:
//    case VLS_W1CeilingCrushAndRaise:
    case VLS_SRCloseDoor:
    case VLS_SRCeilingLowerToFloor:
    case VLS_W1CeilingLowerAndCrush:
//    case VLS_S1CeilingCrushAndRaise:
    case VLS_S1CloseDoor:
    case VLS_WRCeilingLowerAndCrush:
//    case VLS_WRCeilingCrushAndRaise:
    case VLS_WRCloseDoor:
    case VLS_WRCloseDoor30:
//    case VLS_WRFastCeilCrushRaise:
    case VLS_WRDoorBlazeClose:
    case VLS_W1DoorBlazeClose:
    case VLS_S1DoorBlazeClose:
    case VLS_SRDoorBlazeClose:
//    case VLS_W1SilentCrushAndRaise:
    case VLS_W1CeilingLowerToFloor:
//    case VLS_WRSilentCrushAndRaise:
    case VLS_WRCeilingLowerToFloor:
//    case VLS_S1FastCeilCrushRaise:
//    case VLS_S1SilentCrushAndRaise:
    case VLS_S1CeilingLowerAndCrush:
    case VLS_S1CloseDoor30:
//    case VLS_SRFastCeilCrushRaise:
//    case VLS_SRCeilingCrushAndRaise:
//    case VLS_SRSilentCrushAndRaise:
    case VLS_SRCeilingLowerAndCrush:
    case VLS_SRCloseDoor30:
        return false;

        // more complex, so for now they aren't targetted
    case VLS_W1PlatStop:
    case VLS_W1CeilingCrushStop:
    case VLS_WRCeilingCrushStop:
    case VLS_SRChangeOnlyNumeric:
    case VLS_WRPlatStop:
    case VLS_W1ChangeOnly:
    case VLS_WRChangeOnly:
    case VLS_S1PlatStop:
    case VLS_S1CeilingCrushStop:
    case VLS_SRCeilingCrushStop:
    case VLS_S1ChangeOnly:
    case VLS_SRChangeOnly:
    case VLS_W1ChangeOnlyNumeric:
    case VLS_WRChangeOnlyNumeric:
    case VLS_S1ChangeOnlyNumeric:
    case VLS_WRStartLineScript1S:
    case VLS_W1StartLineScript:
    case VLS_W1StartLineScript1S:
    case VLS_SRStartLineScript:
    case VLS_S1StartLineScript:
    case VLS_GRStartLineScript:
    case VLS_G1StartLineScript:
    case VLS_WRStartLineScript:
        return false;

        // useless
    case VLS_W1LightTurnOn: 
    case VLS_W1LightTurnOn255:
    case VLS_W1StartLightStrobing:
    case VLS_W1LightsVeryDark:
    case VLS_WRLightsVeryDark:
    case VLS_WRLightTurnOn:
    case VLS_WRLightTurnOn255:
    case VLS_W1TurnTagLightsOff:
    case VLS_SRLightTurnOn255:
    case VLS_SRLightsVeryDark:
    case VLS_WRStartLightStrobing:
    case VLS_WRTurnTagLightsOff:
    case VLS_S1LightTurnOn:
    case VLS_S1LightsVeryDark:
    case VLS_S1LightTurnOn255:
    case VLS_S1StartLightStrobing:
    case VLS_S1TurnTagLightsOff:
    case VLS_SRLightTurnOn:
    case VLS_SRStartLightStrobing:
    case VLS_SRTurnTagLightsOff:
    case VLS_W1TeleportMonsters:
    case VLS_WRTeleportMonsters:
    case VLS_W1SilentLineTRMonsters:
    case VLS_WRSilentLineTRMonsters:
    case VLS_W1SilentLineTeleMonsters:
    case VLS_WRSilentLineTeleMonsters:
    case VLS_W1SilentTeleportMonsters:
    case VLS_WRSilentTeleportMonsters:
        return false;

        // personnel teleportation: already handled in the path finder
    case VLS_W1Teleport:
    case VLS_WRTeleport:
    case VLS_S1Teleport:
    case VLS_SRTeleport:
    case VLS_W1SilentTeleport:
    case VLS_WRSilentTeleport:
    case VLS_S1SilentTeleport:
    case VLS_SRSilentTeleport:
        return false;

    case VLS_W1SilentLineTeleport:
    case VLS_WRSilentLineTeleport:
    case VLS_W1SilentLineTeleportReverse:
    case VLS_WRSilentLineTeleportReverse:
        return false;

    default:
        break;
    }
    
    // now that we got some lines out of the way, decide quickly to use once-
    // only types
    const ev_action_t* action = EV_ActionForSpecial(line.special);
    if(action && (action->type == &S1ActionType
                  || action->type == &W1ActionType))
    {
        if(m_deepSearchMode == DeepNormal
           && m_searchstage < SearchStage_PitItems)
        {
			LevelStateStack::Clear();
			m_deepTriedLines.clear();
			m_deepAvailSsectors.clear();
			
			m_deepTriedLines.insert(&line);

			m_deepSearchMode = DeepAvail;
			m_finder.AvailableGoals(*ss, &m_deepAvailSsectors, reachableItem, this);
			m_deepSearchMode = DeepNormal;
					
			// Now apply the change
			if (!LevelStateStack::Push(line, *pl))
				return false;
				
			// Now search again: see if anything is lost.
			m_deepSearchMode = DeepCheckLosses;
			m_finder.AvailableGoals(liness, nullptr, reachableItem, this);
			m_deepSearchMode = DeepNormal;
			
			LevelStateStack::Clear();
			
			// Return true if all available ssectors have been checked
			return m_deepAvailSsectors.empty();
		}
		
		bool result = LevelStateStack::Push(line, *pl);
        LevelStateStack::Clear();       
        return result;
        // just push them, as long as they're not the blocking type and have any
        // effect
    }

    if (m_deepSearchMode == DeepNormal)
    {
        LevelStateStack::Clear();
        m_deepTriedLines.clear();
        m_deepAvailSsectors.clear();

        m_deepTriedLines.insert(&line);

        m_deepSearchMode = DeepAvail;
        m_finder.AvailableGoals(*ss, &m_deepAvailSsectors, reachableItem, this);
        m_deepSearchMode = DeepNormal;

        // Now apply the change
        if (!LevelStateStack::Push(line, *pl))
            return false;

        m_deepSearchMode = DeepBeyond;

        // FIXME: this seems to trigger other problems, such as not using
        // some elevator triggers
        bool result = false, tempresult;
        const BSubsec* repsave = nullptr;
        do
        {
            m_deepRepeat = nullptr;
            tempresult = m_finder.AvailableGoals(repsave ? *repsave : liness,
                                             nullptr, reachableItem, this);
            repsave = m_deepRepeat;
            if(!m_deepRepeat)
                result |= tempresult;
        } while (m_deepRepeat);
        m_deepRepeat = nullptr;
        
        m_deepSearchMode = DeepNormal;

        LevelStateStack::Clear();
        return result;
    }

    return false;
}

static bool BTR_switchReachTraverse(intercept_t *in, void *parm)
{
   const line_t &l = *in->d.line;
   if(&l == parm)
      return true;

   if(l.special)
      return !demo_compatibility && in->d.line->flags & ML_PASSUSE;

   // no special. Check if blocking.
   if(l.extflags & EX_ML_BLOCKALL || l.sidenum[1] == -1 ||
      l.frontsector->floorheight >= l.frontsector->ceilingheight ||
      l.backsector->floorheight >= l.backsector->ceilingheight ||
      l.frontsector->floorheight >= l.backsector->ceilingheight ||
      l.backsector->floorheight >= l.frontsector->ceilingheight)
   {
      return false;
   }

   return true;
}

static bool B_checkSwitchReach(fixed_t mox, fixed_t moy, const v2fixed_t &coord,
                               const line_t &swline)
{
   if(B_ExactDistance(mox - coord.x, moy - coord.y) >= USERANGE)
      return false;
   RTraversal trav;

   return trav.Execute(mox, moy, coord.x, coord.y, PT_ADDLINES,
                       BTR_switchReachTraverse, const_cast<line_t *>(&swline));
}

// This version checks if a line is by itself reachable, without assuming a user
static bool B_checkSwitchReach(fixed_t range, const line_t &swline)
{
   RTraversal trav;

   angle_t ang = P_PointToAngle(swline.v1->x, swline.v1->y,
                                swline.v2->x, swline.v2->y) - ANG90;
   unsigned fang = ang >> ANGLETOFINESHIFT;

   fixed_t mx, my;
   mx = swline.v1->x + swline.dx / 2;
   my = swline.v1->y + swline.dy / 2;
   return trav.Execute(mx + FixedMul(range, finecosine[fang]),
                       my + FixedMul(range, finesine[fang]), mx, my,
                       PT_ADDLINES, BTR_switchReachTraverse,
                       const_cast<line_t *>(&swline));
}

//
// Checks if an item sprite is gettable
//
bool Bot::checkItemType(const Mobj *special) const
{
   spritenum_t sprite = special->sprite;
   if(sprite < 0 || sprite >= NUMSPRITES)
      return false;

   const e_pickupfx_t *pickup = special->info->pickupfx ?
   special->info->pickupfx : E_PickupFXForSprNum(sprite);
   if(!pickup || (pickup->flags & PXFX_COMMERCIALONLY && demo_version < 335 &&
                  GameModeInfo->id != commercial) || !pickup->numEffects)
   {
      return false;
   }

   bool dropped = !!(special->flags & MF_DROPPED);

   bool hadeffect = false;
   for(unsigned i = 0; i < pickup->numEffects; ++i)
   {
      const itemeffect_t *effect = pickup->effects[i];
      if(!effect)
         continue;
      hadeffect = true;
      switch(effect->getInt("class", ITEMFX_NONE))
      {
         case ITEMFX_HEALTH:
            if(B_CheckBody(pl, effect))
               return true;
            break;
         case ITEMFX_ARMOR:
            if(B_CheckArmour(pl, effect))
               return true;
            break;
         case ITEMFX_AMMO:
            if(B_CheckAmmoPickup(pl, effect, dropped, special->dropamount))
               return true;
            break;
         case ITEMFX_POWER:
            if(B_CheckPowerForItem(pl, effect))
               return true;
            break;
         case ITEMFX_WEAPONGIVER:
            if(B_CheckWeapon(pl, effect, dropped, special))
               return true;
            break;
         case ITEMFX_ARTIFACT:
            if(B_CheckInventoryItem(pl, effect))
               return true;
            break;
         default:
            break;
      }
   }

   if(!hadeffect)
      return false;

   if(pickup->flags & PFXF_GIVESBACKPACKAMMO && B_CheckBackpack(pl))
      return true;
   return false;
}

//
// Returns true if there's an object of interest (item, switch or anything else
// which can be a goal) in given subsector. Outputs the object's exact
// coordinates.
//
bool Bot::objOfInterest(const BSubsec& ss, BotPathEnd& coord, void* v)
{

    Bot& self = *(Bot*)v;

	auto ssit = self.m_deepAvailSsectors.find(&ss);
	if(ssit != self.m_deepAvailSsectors.end())
	{
		if(self.m_deepSearchMode == DeepBeyond)
			return false;
		if(self.m_deepSearchMode == DeepCheckLosses)
		{
			self.m_deepAvailSsectors.erase(ssit);
			return false;
		}
	}
	if(self.m_deepSearchMode == DeepCheckLosses)
		return false;
	
    if(self.m_searchstage >= SearchStage_ExitNormal)
    {
       const sector_t &floorsector = *ss.msector->getFloorSector();
       if(floorsector.damageflags & SDMG_EXITLEVEL)
       {
          coord.kind = BotPathEnd::KindCoord;
          coord.coord = ss.mid;
          self.goalTable.setV2Fixed(BOT_FLOORSECTOR,
                                    v2fixed_t{(int)(&floorsector - ::sectors),
                                       0});
          return true;
       }
    }

    const Mobj* item;
    fixed_t fh;
    const Mobj& plmo = *self.pl->mo;
    for (auto it = ss.mobjlist.begin(); it != ss.mobjlist.end(); ++it)
    {
        item = *it;
        if (item == &plmo)
            continue;
        fh = ss.msector->getFloorHeight();
        if (self.m_deepSearchMode == DeepNormal && (fh + plmo.height <= item->z
                                                    || fh >= item->z
                                                    + item->height))
        {
            continue;
        }
        if (item->flags & MF_SPECIAL)
        {
           if(self.checkItemType(item))
           {
              if (self.m_deepSearchMode == DeepNormal)
              {
                 coord.kind = BotPathEnd::KindCoord;
                 coord.coord = B_CoordXY(*item);
                 self.goalTable.setV2Fixed(BOT_PICKUP, coord.coord);
              }
              return self.checkDeadEndTrap(ss);
           }
        }
       else
       {
          // TODO: boss specials
       }
    }

    // look for walkover triggers.
    const BotMap::Line* bline;
    for (const BNeigh& neigh : ss.neighs)
    {
        bline = neigh.line;

        // Don't handle teleporters here as goals. They're handled in b_path.
        // And unlike teleporters, we don't care about the way we enter them.
        if(bline && bline->specline
           && !B_IsWalkTeleportation(bline->specline->special)
           && botMap->canPass(*neigh.myss, *neigh.otherss, self.pl->mo->height)
           && self.handleLineGoal(ss, coord, *bline->specline))
        {
            return true;
        }
    }

    // look for other triggers.
    for (auto it = ss.linelist.begin(); it != ss.linelist.end(); ++it)
    {
       const line_t &line = *it->first;
       if(line.frontsector->isShut())  // quick out
          continue;
       // Check if it's really accessible
       if(!B_checkSwitchReach(it->second, line))
          continue;

       if(self.handleLineGoal(ss, coord, line))
          return true;
    }

    return false;
}

//
// Bot::handleLineGoal
//
// Called by objOfInterest when it has to decide on a line
//
bool Bot::handleLineGoal(const BSubsec &ss, BotPathEnd &coord, const line_t& line)
{
    const ev_action_t* action = EV_ActionForSpecial(line.special);
    if (action && (action->type == &S1ActionType
                   || action->type == &SRActionType
                   || action->type == &W1ActionType
                   || action->type == &WRActionType
                   || action->type == &DRActionType))
    {
        // OK, this might be viable. But check.
        // DeepCheckLosses not reached here
        if (m_deepSearchMode == DeepAvail)
        {
            m_deepTriedLines.insert(&line);
            return true;
        }
        else if (m_deepSearchMode == DeepBeyond)
        {
            if (!m_deepTriedLines.count(&line))
            {
                if (shouldUseSpecial(line, ss))
                    return true;
                m_deepTriedLines.insert(&line);

                // ONLY return true if pushing was useful.

                if(LevelStateStack::Push(line, *pl,
                                         ss.msector->getFloorSector()))
                {
                    m_deepRepeat = &ss;
                    return true;
                }
            }
            //return false;
        }
        else if (shouldUseSpecial(line, ss))
        {
            coord.kind = BotPathEnd::KindWalkLine;
            coord.walkLine = &line;
            v2fixed_t crd = B_CoordXY(*line.v1);
            //crd.x += self.random.range(-16, 16) * FRACUNIT;
            //crd.y += self.random.range(-16, 16) * FRACUNIT;
            goalTable.setV2Fixed(BOT_WALKTRIG,crd);
            return true;
        }
    }
    return false;
}

//
// Bot::enemyVisible()
//
// Returns true if an enemy is visible now
// Code inspired from P_LookForMonsters
//
void Bot::enemyVisible(PODCollection<Target>& targets)
{
    // P_BlockThingsIterator is safe to use outside of demo correctness
    camsightparams_t cam;
    cam.setLookerMobj(pl->mo);

    fixed_t dist;
    Target* newt;
    if (!botMap->livingMonsters.isEmpty())
    {

        auto it = botMap->livingMonsters.begin();
//        auto bit = botMap->livingMonsters.before_begin();
        while(it != botMap->livingMonsters.end())
        {
            const Mobj* m = *it;
            if (m->health <= 0 || !m->isBotTargettable())
            {
//                ++it;
                botMap->livingMonsters.erase(it);
                continue;
            }
            cam.setTargetMobj(m);
            if (CAM_CheckSight(cam))
            {
                dist = B_ExactDistance(*pl->mo, *m);
                if (dist < MISSILERANGE / 2)
                {
                    newt = &targets.addNew();
                    newt->coord = B_CoordXY(*m);
                    newt->dangle = P_PointToAngle(*pl->mo, *m) - pl->mo->angle;
                    newt->dist = dist;
                    newt->isLine = false;
                    newt->mobj = m;
                    std::push_heap(targets.begin(), targets.end());
                }
            }
            
            ++it;
        }
    }
    
    v2fixed_t lvec;
    angle_t lang;
    const sector_t* sector;
    fixed_t bulletheight = pl->mo->z + 33 * FRACUNIT;   // FIXME: don't hard-code
    for(const line_t* line : botMap->gunLines)
    {
        sector = line->frontsector;
        if(!sector || sector->floorheight > bulletheight
           || sector->ceilingheight < bulletheight)
        {
            continue;
        }
        lvec.x = (line->v1->x + line->v2->x) / 2;
        lvec.y = (line->v1->y + line->v2->y) / 2;
        lang = P_PointToAngle(*line->v1, *line->v2);
        lang -= ANG90;
        lang >>= ANGLETOFINESHIFT;
        lvec.x += FixedMul(FRACUNIT, finecosine[lang]);
        lvec.y += FixedMul(FRACUNIT, finesine[lang]);
        cam.tgroupid = line->frontsector->groupid;
        cam.tx = lvec.x;
        cam.ty = lvec.y;
        cam.tz = sector->floorheight;
        cam.theight = sector->ceilingheight - sector->floorheight;
        
        if(CAM_CheckSight(cam) && LevelStateStack::Push(*line, *pl))
        {
            LevelStateStack::Pop();
            dist = B_ExactDistance(*pl->mo, lvec);
            if (dist < MISSILERANGE / 2)
            {
                newt = &targets.addNew();
                newt->coord = lvec;
                newt->dangle = P_PointToAngle(*pl->mo, lvec) - pl->mo->angle;
                newt->dist = dist;
                newt->isLine = true;
                newt->gline = line;
                std::push_heap(targets.begin(), targets.end());
            }
        }
    }
}

void Bot::pickRandomWeapon(const Target& target)
{
    weapontype_t guns[NUMWEAPONS];
    int num = 0;
    
    if(pl->powers[pw_strength])
        guns[num++] = wp_fist;

   const weaponinfo_t *const ssg = E_WeaponForDEHNum(wp_supershotgun);
   const weaponinfo_t *const sg = E_WeaponForDEHNum(wp_shotgun);

    if(!E_PlayerOwnsWeapon(pl, ssg) && E_PlayerOwnsWeapon(pl, sg) &&
       P_WeaponHasAmmo(pl, sg))
    {
        guns[num++] = wp_shotgun;
    }

   const weaponinfo_t *const cg = E_WeaponForDEHNum(wp_chaingun);
    if(E_PlayerOwnsWeapon(pl, cg) && P_WeaponHasAmmo(pl, cg))
        guns[num++] = wp_chaingun;

   const weaponinfo_t *const rl = E_WeaponForDEHNum(wp_missile);
    if(E_PlayerOwnsWeapon(pl, rl) && P_WeaponHasAmmo(pl, rl) &&
       P_AproxDistance(pl->mo->x - target.coord.x,
                       pl->mo->y - target.coord.y) > 200 * FRACUNIT)
    {
        guns[num++] = wp_missile;
    }

   const weaponinfo_t *const pg = E_WeaponForDEHNum(wp_plasma);
    if(E_PlayerOwnsWeapon(pl, pg) && P_WeaponHasAmmo(pl, pg))
        guns[num++] = wp_plasma;

   const weaponinfo_t *const bfg = E_WeaponForDEHNum(wp_bfg);
    if(E_PlayerOwnsWeapon(pl, bfg) && P_WeaponHasAmmo(pl, bfg))
        guns[num++] = wp_bfg;

   if(E_PlayerOwnsWeapon(pl, ssg) && P_WeaponHasAmmo(pl, ssg))
      guns[num++] = demo_compatibility ? wp_shotgun : wp_supershotgun;

   const weaponinfo_t *const pistol = E_WeaponForDEHNum(wp_pistol);
    if(num == 0 && P_WeaponHasAmmo(pl, pistol))
        guns[num++] = wp_pistol;
    if(num == 0)
        guns[num++] = wp_fist;
    
    cmd->buttons |= BT_CHANGE;
    weapontype_t result = guns[random() % num];

    // Make sure to release the fire key when changing to guns with safety
    if (result == wp_missile || result == wp_bfg)
        cmd->buttons &= ~BT_ATTACK;
    cmd->buttons |= result << BT_WEAPONSHIFT;
}

//
// Rough approximation of player health, counting also his armour
//
inline static int B_calcPlayerHealth(const player_t *pl)
{
   return pl->armordivisor ? pl->mo->health + emin(pl->armorpoints * pl->armorfactor / pl->armordivisor, pl->mo->health) : pl->mo->health;
}

//
// Whether the bot should chat, optionally limited by a timeout (with a given
// variable)
//
bool Bot::shouldChat(int intervalSec, int timekeeper) const
{
   // TODO: also check whether players[0] is not human-controlled
   return pl - players > 0 && (!intervalSec || !timekeeper ||
                               timekeeper + intervalSec * TICRATE < gametic);
}

const Bot::Target *Bot::pickBestTarget(const PODCollection<Target>& targets, CombatInfo &cinfo)
{
   const Target* highestThreat = &targets[0];
   double totalThreat = 0;
   if (!targets[0].isLine)
   {
      double highestThreatRating = -DBL_MAX, threat;
      fixed_t shortestDist = D_MAXINT;

      const Target* closestThreat = &targets[0];
      double closestThreatRating;

      const Target* currentTargetInSight = nullptr;
      double currentTargetRating;

      const Target *summonerThreat = nullptr;
      double summonerThreatRating = -DBL_MAX;

      for (const Target& target : targets)
      {
         if (target.isLine)
            continue;
         if (sentient(target.mobj) && !(target.mobj->flags & MF_FRIEND) &&
            target.mobj->target != pl->mo)
         {
            continue;   // ignore monsters not hunting player
         }
         if (target.mobj == m_currentTargetMobj)
         {
            currentTargetInSight = &target;  // current target sighted
            currentTargetRating = B_GetMonsterThreatLevel(m_currentTargetMobj->info);
         }

         threat = B_GetMonsterThreatLevel(target.mobj->info);
         totalThreat += threat;

         // keep track of summoners and pick the worst one
         if(B_MobjIsSummoner(*target.mobj) && threat > summonerThreatRating)
         {
            summonerThreat = &target;
            summonerThreatRating = threat;
         }

         if (threat > highestThreatRating)
         {
            highestThreat = &target;
            highestThreatRating = threat;
         }
         if (target.dist < shortestDist)
         {
            closestThreat = &target;
            shortestDist = target.dist;
            closestThreatRating = B_GetMonsterThreatLevel(target.mobj->info);
         }

         // set combat info
         if (!cinfo.hasShooters && B_MobjHasMissileAttack(*target.mobj) && !target.mobj->player)
            cinfo.hasShooters = true;
      }

      if(summonerThreat)   // always prioritize summoners
      {
         highestThreat = summonerThreat;
         highestThreatRating = summonerThreatRating;
      }

      if(shouldChat(URGENT_CHAT_INTERVAL_SEC, m_lastHelpCry) &&
         !highestThreat->isLine &&
         totalThreat >= B_calcPlayerHealth(pl))
      {
         m_lastHelpCry = gametic;
         qstring message;
         qstring name(highestThreat->mobj->info->name);
         VOC_AddSpaces(name);

         message << "Help! " << VOC_IndefiniteArticle(name.constPtr()) << " " <<
            name << " is killing me!";
         HU_Say(pl, message.constPtr());
      }

      // Keep shooting current monster but choose another one if it's more threatening
      if (currentTargetInSight && B_GetMonsterThreatLevel(m_currentTargetMobj->info) >= highestThreatRating)
      {
         highestThreat = currentTargetInSight;
         highestThreatRating = currentTargetRating;
      }

      // but prioritize close targets if not much weaker
      if (closestThreat->dist < highestThreat->dist / 2 &&
         (closestThreatRating > highestThreatRating / 4 || closestThreat->dist < 2 * MELEERANGE))
      {
         highestThreat = closestThreat;
      }
   }
   return highestThreat;
}

void Bot::pickBestWeapon(const Target &target)
{
   if(target.isLine)
      return;

   if(!pl->readyweapon || pl->readyweapon->dehnum < 0 ||
      pl->readyweapon->dehnum >= earrlen(g_botweapons))
   {
      return;
   }

   const Mobj &t = *target.mobj;
   fixed_t dist = target.dist;

   const BotWeaponInfo &bwi = g_botweapons[pl->readyweapon->dehnum];
   double currentDmgRate =
   bwi.calcHitscanDamage(dist, t.radius, t.height,
                         !!pl->powers[pw_strength], false) /
   (double)bwi.refireRate;
   double maxDmgRate = currentDmgRate;
   double newDmgRate;
   int maxI = -1;

   // check weapons
   for(int i = 0; i < NUMWEAPONS; ++i)
   {
      const weaponinfo_t *winfo = E_WeaponForDEHNum(i);
      if(!winfo)
         continue;
      if(!E_PlayerOwnsWeapon(pl, winfo) || pl->readyweapon == winfo ||
         !E_GetItemOwnedAmount(pl, winfo->ammo) ||
         g_botweapons[i].flags &
         (BWI_DANGEROUS | BWI_ULTIMATE))
      {
         continue;
      }
      newDmgRate = g_botweapons[i].calcHitscanDamage(dist, t.radius,
                                                     t.height,
                                                     !!pl->powers[pw_strength], false) / (double)g_botweapons[i].refireRate;
      if(newDmgRate > WEAPONCHANGE_HYST_NUM * maxDmgRate / WEAPONCHANGE_HYST_DEN)
      {
         maxDmgRate = newDmgRate;
         maxI = i;
      }
      if(g_botweapons[i].flags & BWI_TAP_SNIPE)
      {
         newDmgRate = g_botweapons[i].calcHitscanDamage(dist, t.radius, t.height, !!pl->powers[pw_strength], true) / (double)g_botweapons[i].oneShotRate;
         if(newDmgRate > WEAPONCHANGE_HYST_NUM * maxDmgRate / WEAPONCHANGE_HYST_DEN)
         {
            maxDmgRate = newDmgRate;
            maxI = i;
         }
      }
   }
   if(maxI >= 0)
   {
      cmd->buttons |= BT_CHANGE;
      cmd->buttons |= maxI << BT_WEAPONSHIFT;
   }
}

void Bot::doCombatAI(const PODCollection<Target>& targets)
{
    fixed_t mx, my, nx, ny;
    mx = pl->mo->x;
    my = pl->mo->y;
    nx = targets[0].coord.x;
    ny = targets[0].coord.y;
    angle_t dangle, tangle;
    tangle = P_PointToAngle(mx, my, nx, ny);
    dangle = tangle - pl->mo->angle;

    // Find the most threatening monster enemy, if the first target is not a line

    CombatInfo cinfo = {};
    const Target *highestThreat = pickBestTarget(targets, cinfo);

    if (!cinfo.hasShooters && targets[0].dist > 5 * MELEERANGE)
       return; // if no shooters, don't attack monsters who are too far

    // Save the threat
    P_SetTarget(&m_currentTargetMobj,
                highestThreat->isLine ? nullptr : highestThreat->mobj);

    angle_t highestTangle = P_PointToAngle(mx, my, highestThreat->coord.x, highestThreat->coord.y);

    int16_t angleturn = (int16_t)(highestTangle >> 16) - (int16_t)(pl->mo->angle >> 16);
    angleturn >>= 2;
    //if (!angleturn)
    {
//            angleturn += random.range(-128, 128);

    }

    if (angleturn > 1500)
        angleturn = 1500;
    if (angleturn < -1500)
        angleturn = -1500;

    if (!m_intoSwitch)
        cmd->angleturn = angleturn * random.range(1, 2);

   if(!highestThreat->isLine)
      pickBestWeapon(*highestThreat);

    RTraversal rt;
    rt.SafeAimLineAttack(pl->mo, pl->mo->angle, MISSILERANGE / 2, 0);
    if (rt.m_clip.linetarget && !rt.m_clip.linetarget->player && pl->readyweapon)
    {
       const BotWeaponInfo &bwi = g_botweapons[pl->readyweapon->dehnum];
       const Mobj &t = *rt.m_clip.linetarget;
       fixed_t dist = B_ExactDistance(t.x - mx, t.y - my);

       const double currentDmgRate =
       bwi.calcHitscanDamage(dist, t.radius, t.height,
                             !!pl->powers[pw_strength], false) /
       (double)bwi.refireRate;

       cmd->buttons |= BT_ATTACK;
       if(bwi.flags & BWI_TAP_SNIPE)
       {
          if(bwi.calcHitscanDamage(dist, t.radius, t.height,
                                   !!pl->powers[pw_strength], true) /
             (double)bwi.oneShotRate > currentDmgRate)
          {
             // start aiming
             if(pl->psprites[0].state->action != A_WeaponReady)
             {
//                B_Log("aim");
                cmd->buttons &= ~BT_ATTACK;  // don't attack unless weaponready
             }
          }
       }
    }

    if(targets[0].isLine && pl->readyweapon)
    {
        angle_t vang[2];
        vang[0] = P_PointToAngle(mx, my, targets[0].gline->v1->x,
                                 targets[0].gline->v1->y);
        vang[1] = P_PointToAngle(mx, my, targets[0].gline->v2->x,
                                 targets[0].gline->v2->y);
        
        if(vang[1] - vang[0] > pl->mo->angle - vang[0] ||
           vang[0] - vang[1] > pl->mo->angle - vang[1])
        {
            cmd->buttons |= BT_ATTACK;
            static const int hitscans[] = {wp_pistol, wp_shotgun, wp_chaingun,
                wp_supershotgun};
            switch (pl->readyweapon->dehnum)
            {
                case wp_fist:
                case wp_missile:
                case wp_plasma:
                case wp_bfg:
                case wp_chainsaw:
                    cmd->buttons |= BT_CHANGE;
                    cmd->buttons |= hitscans[random() % earrlen(hitscans)];
                    break;
                    
                default:
                    break;
            }
        }
    }
    else if (pl->readyweapon && pl->readyweapon->dehnum == wp_missile/* && emax(D_abs(mx - nx),
                                              D_abs(my - ny)) <= 128 * FRACUNIT*/)
    {
       // avoid using the rocket launcher for now...
        pickRandomWeapon(targets[0]);
    }
//    else if (random.range(1, 300) == 1)
//    {
//        pickRandomWeapon(targets[0]);
//    }

    if(!targets[0].isLine && pl->readyweapon)
    {

       int explosion = B_MobjDeathExplosion(*targets[0].mobj);

        if (pl->readyweapon->dehnum == wp_fist ||
            pl->readyweapon->dehnum == wp_chainsaw)
        {
            if (highestThreat != &targets[0] || explosion > pl->health / 4)
            {
                pickRandomWeapon(targets[0]);
            }
           if(!stepLedges(true, 0, 0))
           {
            cmd->forwardmove = FixedMul(2 * pl->pclass->forwardmove[1],
                B_AngleCosine(dangle));
            cmd->sidemove = -FixedMul(2 * pl->pclass->sidemove[1],
                                      B_AngleSine(dangle));
            if(justPunched--)
                cmd->forwardmove -= pl->pclass->forwardmove[1]
                ;
           }
        }
        else
        {
           if (random() % 128 == 0 ||
              (D_abs(m_realVelocity.x) < FRACUNIT && D_abs(m_realVelocity.y) < FRACUNIT))
           {
              toggleStrafeState();
           }

           // Check ledges!
            if (random() % 8 != 0)
            {
               if(!stepLedges(true, 0, 0))
               {
                   fixed_t dist = B_ExactDistance(nx - mx, ny - my);

                  // TODO: portal-aware
                  int explodist = (emax(D_abs(nx - mx), D_abs(ny - my)) - pl->mo->radius) >> FRACBITS;
                  if(explodist < 0)
                     explodist = 0;

                   if (cinfo.hasShooters && dist < 384 * FRACUNIT)
                   {
                      // only circle-strafe if there are blowers
                       cmd->forwardmove = FixedMul(2 * pl->pclass->forwardmove[1],
                           B_AngleSine(dangle)) * m_combatStrafeState;
                       cmd->sidemove = FixedMul(2 * pl->pclass->sidemove[1],
                           B_AngleCosine(dangle)) * m_combatStrafeState;
                   }
                  if (dist < (explosion ? (explosion + 128) * FRACUNIT : MELEERANGE) + targets[0].mobj->radius)
                   {
                       cmd->forwardmove = -FixedMul(2 * pl->pclass->forwardmove[1],
                           B_AngleCosine(dangle));
                       cmd->sidemove = FixedMul(2 * pl->pclass->sidemove[1],
                           B_AngleSine(dangle));

                      // if in range and too close to die, STOP SHOOTING
                      if(explosion - explodist > pl->health / 2)
                      {
                         cmd->buttons &= ~BT_ATTACK;
                      }
                   }
               }
               else
               {
                  toggleStrafeState();
               }
            }

        }
    }
}

//
// Bot::doNonCombatAI
//
// Does whatever needs to be done when not fighting
//
void Bot::doNonCombatAI()
{
    if (!m_hasPath)
    {
        // TODO: object of interest
        LevelStateStack::SetKeyPlayer(pl);
        if(!m_finder.FindNextGoal(pl->mo->x, pl->mo->y, m_path, objOfInterest, this))
        {
            ++m_searchstage;
            cmd->sidemove += random.range(-pl->pclass->sidemove[0],
                pl->pclass->sidemove[0]);
            cmd->forwardmove += random.range(-pl->pclass->forwardmove[0],
                pl->pclass->forwardmove[0]);
           if(m_searchstage > DUNNO_CONCESSION_SEC * TICRATE && shouldChat(IDLE_CHAT_INTERVAL_SEC, m_lastDunnoMessage))
           {
              m_lastDunnoMessage = gametic;
              HU_Say(pl, "Dunno where to go now...");
           }
            return;
        }
        m_hasPath = true;
        m_runfast = false;
       m_lastDunnoMessage = 0;   // will reset here so new events appear
    }

    // found path to exit
    fixed_t mx, my, nx, ny;
    bool dontMove = false;
    const BSubsec* nextss = nullptr;

    v2fixed_t endCoord;
   const line_t *swline = nullptr;
    if (m_path.end.kind == BotPathEnd::KindCoord)
    {
        endCoord = m_path.end.coord;
    }
    else if (m_path.end.kind == BotPathEnd::KindWalkLine)
    {
        swline = m_path.end.walkLine;
        endCoord = B_ProjectionOnSegment(pl->mo->x, pl->mo->y,
                                         swline->v1->x, swline->v1->y,
                                         swline->dx, swline->dy,
                                         pl->mo->radius);
    }
    else
        endCoord = { 0, 0 };

    if (ss == m_path.last)
    {
        nx = endCoord.x;
        ny = endCoord.y;
        m_lastPathSS = ss;
    }
    else
    {
        // from end to path to beginning
        bool onPath = false;
        for (const BNeigh* neigh : m_path.inv)
        {
            if (!botMap->canPass(*neigh->myss, *neigh->otherss, pl->mo->height))
            {
                break;
            }
            if(!m_runfast)
            {
                const PlatThinker* pt = thinker_cast<const PlatThinker*>
                    (neigh->otherss->msector->getFloorSector()->floordata);
                
                if(pt && pt->wait > 0)
                {
                    B_Log("Run fast");
                    m_runfast = true;
                }
            }
            if (neigh->myss == ss)
            {
                v2fixed_t nn = B_ProjectionOnSegment(pl->mo->x, pl->mo->y,
                                                     neigh->v.x, neigh->v.y,
                                                     neigh->d.x, neigh->d.y, pl->mo->radius);
                nx = nn.x;
                ny = nn.y;
                if (!botMap->canPassNow(*neigh->myss, *neigh->otherss, pl->mo->height))
                {
                    dontMove = true;
                }
                {
                    const sector_t* nsector = neigh->otherss->msector->getCeilingSector();
                    const sector_t* msector = ss->msector->getCeilingSector();
                    
                    if(nsector != msector)
                    {
                        const CeilingThinker* ct = thinker_cast
                        <const CeilingThinker*>
                        (nsector->ceilingdata);
                        
                        if(ct && ct->crush > 0 && ct->direction == plat_down)
                        {
                            dontMove = true;
                        }
                    }
                }
                m_lastPathSS = ss;
                if(random() % 64 == 0 && m_dropSS.count(ss))
                {
                    B_Log("Removed goner %d",
                          (int)(ss - &botMap->ssectors[0]));
                    m_dropSS.erase(ss);
                }
                nextss = neigh->otherss;
                onPath = true;
                break;
            }
        }
        
        if (!onPath)
        {
            // not on path, so reset
            if (m_lastPathSS)
            {
                if (!botMap->canPassNow(*ss, *m_lastPathSS,
                    pl->mo->height))
                {
                    B_Log("Inserted goner %d",
                        (int)(m_lastPathSS - &botMap->ssectors[0]));
                    m_dropSS.insert(m_lastPathSS);
                    for (const BNeigh& n : m_lastPathSS->neighs)
                    {
                        if (B_ExactDistance(n.otherss->mid.x - m_lastPathSS->mid.x,
                            n.otherss->mid.y - m_lastPathSS->mid.y)
                            < 128 * FRACUNIT)
                        {
                            m_dropSS.insert(n.otherss);
                        }
                    }
                }
                m_lastPathSS = nullptr;
            }
            m_searchstage = SearchStage_Normal;
            m_hasPath = false;
            if (random() % 3 == 0)
                m_justGotLost = true;
            return;
        }
    }

    mx = pl->mo->x;
    my = pl->mo->y;
    m_intoSwitch = false;
    if (goalTable.hasKey(BOT_WALKTRIG) &&
        B_checkSwitchReach(mx, my, endCoord, *swline))
    {
        m_intoSwitch = true;
        if(prevCtr % 2 == 0)
            cmd->buttons |= BT_USE;
    }
    else if(nextss)
    {
        LevelStateStack::UseRealHeights(true);
        const sector_t* nextsec = nextss->msector->getCeilingSector();
        LevelStateStack::UseRealHeights(false);

       if(botMap->sectorFlags[nextsec - ::sectors].isDoor)
       {
          auto doorTh = thinker_cast<VerticalDoorThinker *>
          (nextsec->ceilingdata);
          if(!doorTh || doorTh->direction == plat_down)
          {
             m_intoSwitch = true;
             if(prevCtr % 2 == 0)
                cmd->buttons |= BT_USE;
          }
       }
    }

    if (goalAchieved())
    {
        m_searchstage = SearchStage_Normal;
        m_hasPath = false;
        return;
    }

    bool moveslow = false;
    if(m_justGotLost)
    {
        moveslow = (m_justGotLost && B_ExactDistance(mx - m_path.start.x,
                                                         my - m_path.start.y)
                                        < pl->mo->radius * 2);
        
        if(!moveslow)
            m_justGotLost = false;
    }
    moveslow |= m_dropSS.count(ss) ? true : false;
    
    angle_t tangle = P_PointToAngle(mx, my, nx, ny);
    angle_t dangle = tangle - pl->mo->angle;

    if (random() % 128 == 0)
        m_straferunstate = random.range(-1, 1);
    if (!m_intoSwitch)
        tangle += ANG45 * m_straferunstate;

    int16_t angleturn = (int16_t)(tangle >> 16) - (int16_t)(pl->mo->angle >> 16);
    angleturn >>= 3;

    if (angleturn > 1500)
        angleturn = 1500;
    if (angleturn < -1500)
        angleturn = -1500;

    if (!dontMove && !(B_ExactDistance(endCoord.x - mx, endCoord.y - my)
                       < 16 * FRACUNIT
          && D_abs(angleturn) > 300))
    {
        if(m_runfast)
            cmd->forwardmove += FixedMul((moveslow ? 1 : 2)
                                         * pl->pclass->forwardmove[moveslow ? 0 : 1],
                                         B_AngleCosine(dangle));
        if (m_intoSwitch && ss == m_path.last/* && cmd->forwardmove < 0*/)
        {
            cmd->forwardmove += FixedMul(pl->pclass->forwardmove[0] / 4,
                                        B_AngleCosine(dangle));
        }
        else
        {
            if(!m_runfast)
                cruiseControl(nx, ny, moveslow);
            else
                cmd->sidemove -= FixedMul((moveslow ? 1 : 2)
                                          * pl->pclass->sidemove[moveslow ? 0 : 1],
                                          B_AngleSine(dangle));
        }
    }

    if (!m_runfast || m_intoSwitch)
       cmd->angleturn += angleturn;

    // If not moving while trying to, budge a bit to avoid stuck moments
    if ((cmd->sidemove || cmd->forwardmove) && D_abs(m_realVelocity.x) < FRACUNIT && D_abs(m_realVelocity.y) < FRACUNIT)
    {
        cmd->sidemove += random.range(-pl->pclass->sidemove[1],
            pl->pclass->sidemove[1]);
        cmd->forwardmove += random.range(-pl->pclass->forwardmove[1],
            pl->pclass->forwardmove[1]);
    }
//   printf("%d\n", angleturn);
}

//
// Checks if the bot is about to fall into a pit, and recovers before falling
// nx and ny are the intended destination coordinates. Returns true if it hit
// a ledge.
//
bool Bot::stepLedges(bool avoid, fixed_t nx, fixed_t ny)
{
    fixed_t mx = pl->mo->x;
    fixed_t my = pl->mo->y;

    v2fixed_t landing;
    landing.x = mx + FixedMul(m_realVelocity.x, DRIFT_TIME);
    landing.y = my + FixedMul(m_realVelocity.y, DRIFT_TIME);

   return (landing.x != mx || landing.y != my) &&
   !botMap->pathTraverse(mx, my, landing.x, landing.y, [this, mx, my, avoid, nx, ny](const BotMap::Line &line, const divline_t &dl, fixed_t frac) {

        divline_t mdl;
        mdl.x = line.v[0]->x;
        mdl.y = line.v[0]->y;
        mdl.dx = line.v[1]->x - mdl.x;
        mdl.dy = line.v[1]->y - mdl.y;
        int s = P_PointOnDivlineSide(dl.x, dl.y, &mdl);

        if(botMap->canPassNow(line.msec[s], line.msec[s ^ 1], pl->mo->height) &&
           !botMap->canPassNow(line.msec[s ^ 1], line.msec[s], pl->mo->height))
        {
            v2fixed_t targvel;
           if(!avoid)
           {
              targvel.x = FixedMul(nx - mx, DRIFT_TIME_INV) - m_realVelocity.x;
              targvel.y = FixedMul(ny - my, DRIFT_TIME_INV) - m_realVelocity.y;
           }
           else
           {
              // try to avoid the ledge instead
              // select the position in front of the line
              // we have our direction divline and the crossed line
              // we must try to get back
              // get the intersection vector. We have frac
              v2fixed_t dest;
              dest.x = dl.x + FixedMul(dl.dx, frac);
              dest.y = dl.y + FixedMul(dl.dy, frac);
              angle_t fang = P_PointToAngle(dl.x, dl.y, dl.x + dl.dx, dl.y + dl.dy) >> ANGLETOFINESHIFT;
              dest.x -= (pl->mo->radius >> FRACBITS) * finecosine[fang];
              dest.y -= (pl->mo->radius >> FRACBITS) * finesine[fang];

              targvel.x = FixedMul(dest.x - mx, DRIFT_TIME_INV) - m_realVelocity.x;
              targvel.y = FixedMul(dest.y - my, DRIFT_TIME_INV) - m_realVelocity.y;
           }

            if(D_abs(targvel.x) <= FRACUNIT && D_abs(targvel.y) <= FRACUNIT)
            {
                cmd->forwardmove += pl->pclass->forwardmove[0]; // hack
                return false;
            }

            angle_t tangle = P_PointToAngle(0, 0, targvel.x, targvel.y);
            angle_t dangle = tangle - pl->mo->angle;
            fixed_t finedangle = dangle >> ANGLETOFINESHIFT;

            cmd->forwardmove += FixedMul(pl->pclass->forwardmove[1],
                                         finecosine[finedangle]);
            cmd->sidemove -= FixedMul(pl->pclass->sidemove[1], finesine[finedangle]);


         return false;
      }

      return true;
   });
}

void Bot::cruiseControl(fixed_t nx, fixed_t ny, bool moveslow)
{

   if(stepLedges(false, nx, ny))
      return;

//    const fixed_t runSpeed = moveslow && !m_runfast ? 8 * FRACUNIT
//    : 16 * FRACUNIT;
#if 0

    // Objective: make sure landing is on top of nx/ny.
    // To do it, I need to set velocity to something appropriate.
    v2fixed_t targvel;
    targvel.x = FixedMul(nx - mx, DRIFT_TIME_INV) - m_realVelocity.x;
    targvel.y = FixedMul(ny - my, DRIFT_TIME_INV) - m_realVelocity.y;

    if(D_abs(targvel.x) <= FRACUNIT && D_abs(targvel.y) <= FRACUNIT)
    {
        cmd->forwardmove += pl->pclass->forwardmove[0]; // hack
        return;
    }

    angle_t tangle = P_PointToAngle(0, 0, targvel.x, targvel.y);
    angle_t dangle = tangle - pl->mo->angle;
    fixed_t finedangle = dangle >> ANGLETOFINESHIFT;

    cmd->forwardmove += FixedMul(pl->pclass->forwardmove[1],
                                 finecosine[finedangle]);
    cmd->sidemove -= FixedMul(pl->pclass->sidemove[1], finesine[finedangle]);
#endif
//#if 0

    // Suggested speed: 15.5
    fixed_t runSpeed = moveslow && !m_runfast ? 8 * FRACUNIT : 16 * FRACUNIT;
    if(m_straferunstate)
        runSpeed = runSpeed * 64 / 50;
    angle_t tangle = P_PointToAngle(pl->mo->x, pl->mo->y, nx, ny);
    angle_t dangle = tangle - pl->mo->angle;

    // Intended speed: forwardly, V*cos(dangle)
    //                 sidedly,  -V*sin(dangle)
    
    // Forward move shall increase momx by a*cos(pangle) and momy by
    // a*sin(pangle)
    
    // Side move momx by a*cos(pangle-90)=a*sin(pangle) and momy by
    // -a*cos(pangle)
    
    const fixed_t fineangle = pl->mo->angle >> ANGLETOFINESHIFT;
//    const fixed_t ctg_fine = (dangle + ANG90) >> ANGLETOFINESHIFT;
    const fixed_t finedangle = dangle >> ANGLETOFINESHIFT;
    
    fixed_t momf = FixedMul(m_realVelocity.x, finecosine[fineangle])
    + FixedMul(m_realVelocity.y, finesine[fineangle]);
    
    fixed_t moms = FixedMul(m_realVelocity.x, finesine[fineangle])
    - FixedMul(m_realVelocity.y, finecosine[fineangle]);



    fixed_t tmomf, tmoms;
    // dangle   tmomf   tmoms
    // 0        1       0
    // 30       v3/2    -1/2
    // 60       1/2     -v3/2
    // 90       0       -1
    // alpha    cos     sin

    tmomf = FixedMul(runSpeed, finecosine[finedangle]);
    tmoms = -FixedMul(runSpeed, finesine[finedangle]);

//    if(dangle < ANG45 || dangle >= ANG270 + ANG45)
//    {
//        tmomf = runSpeed;
//        tmoms = -FixedMul(finetangent[ctg_fine], tmomf);
//    }
//    else if(dangle >= ANG45 && dangle < ANG90 + ANG45)
//    {
//        tmoms = -runSpeed;
//        tmomf = FixedMul(tmoms, finetangent[(ctg_fine + 2048) % 4096]);
//    }
//    else if(dangle < ANG180 + ANG45 && dangle >= ANG90 + ANG45)
//    {
//        tmomf = -runSpeed;
//        tmoms = -FixedMul(finetangent[ctg_fine], tmomf);
//    }
//    else
//    {
//        tmoms = runSpeed;
//        tmomf = FixedMul(tmoms, finetangent[(ctg_fine + 2048) % 4096]);
//    }

    if(tmomf > 0)
    {
//        if(!m_runfast && momf > 0 && momf < runSpeed / 4)
//            cmd->forwardmove += pl->pclass->forwardmove[0];
//        else
            if(momf < tmomf)
            cmd->forwardmove += pl->pclass->forwardmove[1];
        else
            cmd->forwardmove -= pl->pclass->forwardmove[1];
    }
    else if(tmomf < 0)
    {
//        if(!m_runfast && momf < 0 && momf > runSpeed / 4)
//            cmd->forwardmove -= pl->pclass->forwardmove[0];
//        else
            if(momf > tmomf)
            cmd->forwardmove -= pl->pclass->forwardmove[1];
        else
            cmd->forwardmove += pl->pclass->forwardmove[1];
    }
    
    if(tmoms > 0)
    {
//        if(!m_runfast && moms > 0 && moms < runSpeed / 4)
//            cmd->sidemove += pl->pclass->sidemove[0];
//        else
            if(moms < tmoms)
            cmd->sidemove += pl->pclass->sidemove[1];
        else
            cmd->sidemove -= pl->pclass->sidemove[1];
    }
    else if(tmoms < 0)
    {
//        if(!m_runfast && moms < 0 && moms > runSpeed / 4)
//            cmd->sidemove -= pl->pclass->sidemove[0];
//        else
            if(moms > tmoms)
            cmd->sidemove -= pl->pclass->sidemove[1];
        else
            cmd->sidemove += pl->pclass->sidemove[1];
    }

//    B_Log("v(%g %g) f(%d) s(%d) momf(%g) moms(%g) tmomf(%g) tmoms(%g) angle(%g) dangle(%g)",
//          M_FixedToDouble(m_realVelocity.x), M_FixedToDouble(m_realVelocity.y),
//          cmd->forwardmove, cmd->sidemove, M_FixedToDouble(momf),
//          M_FixedToDouble(moms), M_FixedToDouble(tmomf), M_FixedToDouble(tmoms),
//          pl->mo->angle / 4294967296. * 360, dangle / 4294967296. * 360);
//    B_Log("%g\t%g\t%g\t%g\t%d\t%d", M_FixedToDouble(momf), M_FixedToDouble(moms),
//          M_FixedToDouble(tmomf), M_FixedToDouble(tmoms), cmd->forwardmove / 5,
//          cmd->sidemove / 5);
//#endif
}

//
// Assuming bot's player is not the console player, reproduce key behaviour from
// G_BuildTiccmd that's not necessarily user input activated.
//
void Bot::simulateBaseTiccmd()
{
   int newweapon = wp_nochange;
   if(!demo_compatibility && pl->attackdown & AT_PRIMARY && !P_CheckAmmo(pl))
   {
      newweapon = P_SwitchWeaponOld(pl);
      if(newweapon != wp_nochange)
      {
         pl->cmd.buttons |= BT_CHANGE;
         pl->cmd.buttons |= newweapon << BT_WEAPONSHIFT;
      }
   }
}

//
// Bot::doCommand
//
// Called from G_Ticker right before ticcmd is passed into the player. Gets the
// tic command which may have already been copied to the player, and updates it
// with bot output. Cannot just reset what was produced by G_BuildTiccmd,
// because that also handles unrelated stuff.
//
void Bot::doCommand()
{
   if(!active)
      return;  // do nothing if out of game
   if(pl - players != consoleplayer)
      simulateBaseTiccmd();
   if(gamestate == GS_INTERMISSION)
   {
      if(GameModeInfo->interfuncs->TallyDone())
      {
         if(m_exitDelay < 100)
            m_exitDelay++;
         else
            pl->cmd.buttons ^= BT_USE; // mash it
      }
      return;
   }
   if(gamestate != GS_LEVEL)
      return;

   ++prevCtr;

   // Update the velocity
   m_realVelocity.x = pl->mo->x - m_lastPosition.x;
   m_realVelocity.y = pl->mo->y - m_lastPosition.y;
   m_lastPosition.x = pl->mo->x;
   m_lastPosition.y = pl->mo->y;
   
   // Get current values
   ss = &botMap->pointInSubsector(pl->mo->x, pl->mo->y);
   cmd = &pl->cmd;

    if(pl->health <= 0)
    {
       if(prevCtr % DEATH_REVIVE_INTERVAL == 0)
          cmd->buttons |= BT_USE; // respawn
       return; // don't try anything else in this case
    }

   // Do non-combat for now

   doNonCombatAI();
   PODCollection<Target> targets;
    enemyVisible(targets);
    if (!targets.isEmpty())
   {
       doCombatAI(targets);
   }
    else
        justPunched = 0;
    
   
   // Limit commands before exiting
   capCommands();
}

//
// Bot::InitBots
//
// Must be called from initialization to set the player references (both bots
// and players are allocated globally). Note that they already start active.
//
void Bot::InitBots()
{
   for (int i = 0; i < MAXPLAYERS; ++i)
   {
      bots[i].pl = players + i;
   }

   B_AnalyzeWeapons();
}

// EOF

