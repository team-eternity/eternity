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
#include "b_util.h"
#include "b_vocabulary.h"
#include "b_weapon.h"
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
#include "../ev_actions.h"
#include "../ev_specials.h"
#include "../g_game.h"
#include "../hu_stuff.h"
#include "../in_lude.h"
#include "../m_compare.h"
#include "../m_qstr.h"
#include "../p_maputl.h"
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
   SearchStage_NUM
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
   m_searchstage = SearchStage_Normal;

   m_finder.SetMap(botMap);
   m_finder.SetPlayer(pl);
   m_hasPath = false;

   m_lostPathInfo = {};

   m_deepTriedLines.clear();
   m_deepSearchMode = DeepNormal;
   m_deepAvailSsectors.clear();
   m_deepRepeat = nullptr;
   m_deepPromise.clear();
    m_justGotLost = false;
    m_intoSwitch = false;
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
      goalcoord = goalTable.getV2Fixed(metaob->getKey(), { D_MAXINT, D_MAXINT });

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

   if(result && self.m_deepSearchMode == DeepBeyond)
   {
      self.m_deepPromise.sss.insert(&ss);
      B_Log("Made a promise for %g %g", ss.mid.x/65536., ss.mid.y/65536.);
   }

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

Bot::SpecialChoice Bot::shouldUseSpecial(const line_t& line, const BSubsec& liness)
{
   const ev_action_t *action = EV_ActionForSpecial(line.special);
   if(!action)
      return SpecialChoice_no;

   auto exitcondition = [this, action, &line](unsigned stage, const char *announcement) {

        // FIXME: consider going immediately if the usual case is doomed to fail for any reason
        if(m_searchstage >= stage)
        {
           if(shouldChat(URGENT_CHAT_INTERVAL_SEC, m_lastExitMessage))
           {
              m_lastExitMessage = gametic;
              // FIXME: do not send it now, but when bot actually goes there
              HU_Say(pl, announcement);
           }
           return SpecialChoice_favourable;
        }
      return SpecialChoice_no;
   };

   EVActionFunc func = action->action;
   // TODO: also support the gun and push kinds, but those needs a better searching for goals
   if(func == EV_ActionSwitchExitLevel || func == EV_ActionExitLevel ||
      func == EV_ActionGunExitLevel || func == EV_ActionParamExitNormal)
   {
      return exitcondition(SearchStage_ExitNormal, "I'm going to the exit now!");
   }
   if(func == EV_ActionSwitchSecretExit || func == EV_ActionSecretExit ||
      func == EV_ActionGunExitLevel || func == EV_ActionParamExitSecret)
   {
      return exitcondition(SearchStage_ExitSecret, "Let's take the secret exit!");
   }
   if(func == EV_ActionTeleportNewMap)
      return exitcondition(SearchStage_ExitNormal, "I've found an exit, let's go!");
   if(func == EV_ActionTeleportEndGame)
      return exitcondition(SearchStage_ExitNormal, "Let's finish this!");

   // TODO: attached surface support. Without them, none of the ceiling lowering specials are useful
   // for the bot, except for rare combat or puzzle situations.

   // TODO: these might yet be useful if a pushable linedef surface depends on lowering ceiling
   if(EV_IsCeilingLoweringSpecial(line))
      return SpecialChoice_no;

   // These are always bad for the activator
   if(func == EV_ActionDamageThing || func == EV_ActionRadiusQuake)
      return SpecialChoice_no;

   // TODO: generalized and parameterized
   // These are more complex, so let's ignore them for now
   static const std::unordered_set<EVActionFunc> complex = {
      EV_ActionParamCeilingCrushStop,
      EV_ActionParamPlatStop,
      EV_ActionCeilingCrushStop,
      EV_ActionChangeOnly,
      EV_ActionChangeOnlyNumeric,
      EV_ActionPlatStop,
      EV_ActionStartLineScript,
   };

   // TODO: ceiling crush stop may be desired if it's permanent and can avoid blocking the player in
   // the future. For switches the timing is exact too.
   // TODO: change may be good if it removes slime (or harmful if it adds)
   // TODO: plat stop can be harmful if it's permanent
   // TODO: scripting is ultra complex. Maybe learn by trial and error?

   if(complex.count(func))
      return SpecialChoice_no;

   // The following are inconsequential to the computer. Maybe we should later add support if we
   // want better chance of passing the Turing test.
   static const std::unordered_set<EVActionFunc> inconsequential = {
      EV_ActionLightsVeryDark,
      EV_ActionLightTurnOn,
      EV_ActionLightTurnOn255,
      EV_ActionStartLightStrobing,
      EV_ActionTurnTagLightsOff,

      EV_ActionParamLightChangeToValue,
      EV_ActionParamLightFade,
      EV_ActionParamLightFlicker,
      EV_ActionParamLightGlow,
      EV_ActionParamLightLowerByValue,
      EV_ActionParamLightRaiseByValue,
      EV_ActionParamLightStrobe,
      EV_ActionParamLightStrobeDoom,
      EV_ActionParamSectorSetRotation,
      EV_ActionParamSectorSetFloorPanning,
      EV_ActionParamSectorSetCeilingPanning,
      EV_ActionParamSectorChangeSound,
   };
   if(inconsequential.count(func))
      return SpecialChoice_no;

   // Also avoid pillar builders
   if(func == EV_ActionPillarBuild || func == EV_ActionPillarBuildAndCrush)
      return SpecialChoice_no;
    
    // now that we got some lines out of the way, decide quickly to use once-
    // only types
   // TODO: also detect generalized specials
    if(!EV_IsRepeatableSpecial(line))
    {
        if(m_deepSearchMode == DeepNormal && m_searchstage < SearchStage_PitItems)
        {
			LevelStateStack::Clear();
			m_deepTriedLines.clear();
			m_deepAvailSsectors.clear();
			
			m_deepTriedLines.insert(&line);

			m_deepSearchMode = DeepAvail;
			m_finder.AvailableGoals(*ss, &m_deepAvailSsectors, reachableItem, this);
			m_deepSearchMode = DeepNormal;
					
			// Now apply the change
			if (!LevelStateStack::Push(line, *pl, nullptr))
				return SpecialChoice_no;
				
			// Now search again: see if anything is lost.
			m_deepSearchMode = DeepCheckLosses;
			m_finder.AvailableGoals(liness, nullptr, reachableItem, this);
			m_deepSearchMode = DeepNormal;
			
			LevelStateStack::Clear();
			
			// Return true if all available ssectors have been checked
			return m_deepAvailSsectors.empty() ? SpecialChoice_worth : SpecialChoice_no;
		}
		
		auto result = LevelStateStack::Push(line, *pl, nullptr);
        LevelStateStack::Clear();       
        return result ? SpecialChoice_worth : SpecialChoice_no;
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

       auto pushresult = LevelStateStack::Push(line, *pl, nullptr);
        // Now apply the change
        if (!pushresult)
            return SpecialChoice_no;

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
       if(result && pushresult == LevelStateStack::PushResult_timed)
          m_deepPromise.flags |= DeepPromise::URGENT;
        return result ? SpecialChoice_worth : SpecialChoice_no;
    }

    return SpecialChoice_no;
}

static bool BTR_switchReachTraverse(const intercept_t *in, void *parm, const divline_t &trace)
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

static bool B_checkSwitchReach(v2fixed_t mopos, v2fixed_t coord, const line_t &swline)
{
   if((mopos - coord).sqrtabs() >= USERANGE)
      return false;
   return CAM_PathTraverse(mopos.x, mopos.y, coord.x, coord.y, CAM_ADDLINES,
                           const_cast<line_t *>(&swline), BTR_switchReachTraverse);
}

// This version checks if a line is by itself reachable, without assuming a user
static bool B_checkSwitchReach(fixed_t range, const line_t &swline)
{
   angle_t ang = P_PointToAngle(swline.v1->x, swline.v1->y, swline.v2->x, swline.v2->y) - ANG90;
   v2fixed_t mpos = v2fixed_t(*swline.v1) + v2fixed_t(swline.dx, swline.dy) / 2;

   v2fixed_t mposofs = mpos + v2fixed_t::polar(range, ang);

   return CAM_PathTraverse(mposofs.x, mposofs.y, mpos.x, mpos.y, CAM_ADDLINES,
                           const_cast<line_t *>(&swline), BTR_switchReachTraverse);
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
   if(!pickup || (pickup->flags & PFXF_COMMERCIALONLY && demo_version < 335 &&
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
// Check if other bots already found the same goal
//
bool Bot::otherBotsHaveGoal(const char *key, v2fixed_t coord) const
{
   if(m_searchstage > SearchStage_NUM)
      return false;
   for(Bot &bot : bots)
   {
      if(&bot == this || !bot.active)
         continue;
      if(bot.goalTable.getV2Fixed(key, v2fixed_t{ D_MININT, D_MININT }) == coord)
         return true;
   }
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
	
   if(self.m_deepSearchMode == DeepNormal)
   {
      if(self.m_deepPromise.flags & DeepPromise::BENEFICIAL && !self.m_deepPromise.sss.count(&ss))
         return false;
      if(self.m_deepPromise.sss.count(&ss))
      {
         B_Log("Go to promise %g %g", ss.mid.x / 65536., ss.mid.y / 65536.);
      }
      self.m_deepPromise.clear();
   }

    if(self.m_searchstage >= SearchStage_ExitNormal)
    {
       const sector_t &floorsector = *ss.msector->getFloorSector();
       auto goaltag = v2fixed_t{(int)(&floorsector - ::sectors), 0};
       if(floorsector.damageflags & SDMG_EXITLEVEL &&
          !self.otherBotsHaveGoal(BOT_FLOORSECTOR, goaltag))
       {
          if (self.m_deepSearchMode == DeepNormal)
          {
             coord.kind = BotPathEnd::KindCoord;
             coord.coord = ss.mid;
             self.goalTable.setV2Fixed(BOT_FLOORSECTOR, goaltag);
          }
          else if(self.m_deepSearchMode == DeepBeyond)
             self.m_deepPromise.flags |= DeepPromise::BENEFICIAL;
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
       auto fitheight = [fh, item, &plmo](fixed_t plheight)
       {
          return fh + plheight > item->z && fh < item->z + item->height;
       };
        if (self.m_deepSearchMode == DeepNormal && !fitheight(plmo.height))
            continue;
        if (item->flags & MF_SPECIAL)
        {
           auto goaltag = v2fixed_t(*item);
           if(self.checkItemType(item) && !self.otherBotsHaveGoal(BOT_PICKUP, goaltag))
           {
              bool result = self.checkDeadEndTrap(ss);
              if(result)
              {
                 if (self.m_deepSearchMode == DeepNormal)
                 {
                    coord.kind = BotPathEnd::KindCoord;
                    coord.coord = goaltag;
                    self.goalTable.setV2Fixed(BOT_PICKUP, goaltag);
                 }
                 else if(self.m_deepSearchMode == DeepBeyond)
                    self.m_deepPromise.flags |= DeepPromise::BENEFICIAL;
              }
              return result;
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

        if(bline && bline->specline
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
   // TODO: add support for the other activation types
   if(!EV_IsWalkSpecial(line) && !EV_IsSwitchSpecial(line))
      return false;
   if(EV_IsNonPlayerSpecial(line) || EV_IsTeleportationSpecial(line))
      return false;

     // OK, this might be viable. But check.
     // DeepCheckLosses not reached here
     if (m_deepSearchMode == DeepAvail)
     {
         m_deepTriedLines.insert(&line);
         return true;
     }
     if (m_deepSearchMode == DeepBeyond)
     {
         if (!m_deepTriedLines.count(&line))
         {
            SpecialChoice choice = shouldUseSpecial(line, ss);
            if(choice == SpecialChoice_favourable)
               m_deepPromise.flags |= DeepPromise::BENEFICIAL;
             if (choice)
                 return true;
             m_deepTriedLines.insert(&line);

             // ONLY return true if pushing was useful.

             if(LevelStateStack::Push(line, *pl, ss.msector->getFloorSector()))
             {
                 m_deepRepeat = &ss;
                 return true;
             }
         }
         //return false;
     }
     else
     {
        auto goaltag = v2fixed_t(*line.v1);
        if (shouldUseSpecial(line, ss) && !otherBotsHaveGoal(BOT_WALKTRIG, goaltag))
        {
            coord.kind = BotPathEnd::KindWalkLine;
            coord.walkLine = &line;
            //crd.x += self.random.range(-16, 16) * FRACUNIT;
            //crd.y += self.random.range(-16, 16) * FRACUNIT;
            goalTable.setV2Fixed(BOT_WALKTRIG,goaltag);
            return true;
        }
     }

    return false;
}

//
// Constructs a target from a Mobj
//
Bot::Target::Target(const Mobj &mobj, const Mobj &source, bool ismissile) :
type(ismissile ? TargetMissile : TargetMonster),
mobj(),  // vital to zero-init before setting target
coord(mobj)
{
   P_SetTarget(&this->mobj, &mobj);
   v2fixed_t delta = coord - source;
   dist = delta.sqrtabs();
   dangle = delta.angle() - source.angle;
}

//
// Constructs a target from a shootable line
//
Bot::Target::Target(const line_t &line, const Mobj &source) :
type(TargetLine),
gline(&line)
{
   coord = (v2fixed_t(*line.v1) + *line.v2) / 2;
   angle_t lang = v2fixed_t(line.dx, line.dy).angle();
   lang -= ANG90;
   lang >>= ANGLETOFINESHIFT;
   coord.x += FixedMul(FRACUNIT, finecosine[lang]);
   coord.y += FixedMul(FRACUNIT, finesine[lang]);
   v2fixed_t delta = coord - source;
   dist = delta.sqrtabs();
   dangle = delta.angle() - source.angle;
}

//
// Move assignment
//
Bot::Target &Bot::Target::operator = (Bot::Target &&other)
{
   type = other.type;
   other.type = TargetMonster;
   switch(type)
   {
      case TargetMonster:
      case TargetMissile:
         mobj = other.mobj;
         other.mobj = nullptr;
         break;
      case TargetLine:
         gline = other.gline;
         other.gline = nullptr;
         break;
   }
   coord = other.coord;
   dist = other.dist;
   dangle = other.dangle;
   return *this;
}

//
// Populates the target list for combat situations
//
void Bot::enemyVisible(Collection<Target>& targets)
{
    camsightparams_t cam;
    cam.setLookerMobj(pl->mo);

   // list gets modified during iteration, so don't use for looping
     auto it = botMap->livingMonsters.begin();
     while(it != botMap->livingMonsters.end())
     {
         const Mobj* m = *it;
         if (m->health <= 0 || !m->isBotTargettable())
         {
             botMap->livingMonsters.erase(it);
             continue;
         }

        Target target(*m, *pl->mo, false);
         if (target.dist < MISSILERANGE / 2)
         {
            cam.setTargetMobj(m);
            if(CAM_CheckSight(cam))
            {
               targets.add(std::move(target));
               std::push_heap(targets.begin(), targets.end());
            }
         }

         ++it;
     }

   for(auto it = botMap->thrownProjectiles.begin(); it != botMap->thrownProjectiles.end(); ++it)
   {
      const Mobj* m = *it;
      Target target(*m, *pl->mo, true);

      if (target.dist < MISSILERANGE / 2)
      {
         cam.setTargetMobj(m);
         if (CAM_CheckSight(cam))
         {
            targets.add(std::move(target));
            std::push_heap(targets.begin(), targets.end());
         }
      }
   }

    fixed_t bulletheight = pl->mo->z + 33 * FRACUNIT;   // FIXME: don't hard-code
    for(const line_t* line : botMap->gunLines)
    {
        const sector_t *sector = line->frontsector;
        if(!sector || sector->floorheight > bulletheight || sector->ceilingheight < bulletheight)
            continue;

        Target target(*line, *pl->mo);

        cam.tgroupid = line->frontsector->groupid;
        cam.tx = target.coord.x;
        cam.ty = target.coord.y;
        cam.tz = sector->floorheight;
        cam.theight = sector->ceilingheight - sector->floorheight;
        
        if(target.dist < MISSILERANGE / 2 && CAM_CheckSight(cam) &&
           LevelStateStack::Push(*line, *pl, nullptr))
        {
           LevelStateStack::Pop();
           targets.add(std::move(target));
           std::push_heap(targets.begin(), targets.end());
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

const Bot::Target *Bot::pickBestTarget(const Collection<Target>& targets, CombatInfo &cinfo)
{
   double totalThreat = 0;
   size_t i = 0;
   for(; i < targets.getLength(); ++i)
      if(targets[i].type != TargetMissile)
         break;
   const Target* highestThreat = i < targets.getLength() ? &targets[i] : &targets[0];
   if (i < targets.getLength() && targets[i].type != TargetLine)
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
         if (target.type != TargetMonster)
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
         highestThreat->type == TargetMonster &&
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
   cinfo.totalThreat = totalThreat;
   return highestThreat;
}

void Bot::pickBestWeapon(const Target &target)
{
   if(target.type != TargetMonster)
      return;

   const Mobj &t = *target.mobj;
   fixed_t dist = target.dist;

   const BotWeaponInfo &bwi = g_botweapons[pl->pclass][pl->readyweapon];
   double currentDmgRate =
   bwi.calcHitscanDamage(dist, t.radius, t.height,
                         !!pl->powers[pw_strength], false) /
   (double)bwi.refireRate;
   double maxDmgRate = currentDmgRate;
   double newDmgRate;
   const weaponinfo_t *maxWI = nullptr;

   // check weapons
   for(auto it = g_botweapons[pl->pclass].begin(); it != g_botweapons[pl->pclass].end(); ++it)
   {
      const weaponinfo_t *winfo = it->first;
      const BotWeaponInfo &obwi = it->second;
      if(!winfo)
         continue;
      if(!E_PlayerOwnsWeapon(pl, winfo) || pl->readyweapon == winfo ||
         !E_GetItemOwnedAmount(pl, winfo->ammo) || obwi.flags & (BWI_DANGEROUS | BWI_ULTIMATE))
      {
         continue;
      }
      
      newDmgRate = obwi.calcHitscanDamage(dist, t.radius, t.height, !!pl->powers[pw_strength],
                                          false) / (double)obwi.refireRate;

      if(newDmgRate > WEAPONCHANGE_HYST_NUM * maxDmgRate / WEAPONCHANGE_HYST_DEN)
      {
         maxDmgRate = newDmgRate;
         maxWI = winfo;
      }
      if(obwi.flags & BWI_TAP_SNIPE)
      {
         newDmgRate = obwi.calcHitscanDamage(dist, t.radius, t.height, !!pl->powers[pw_strength],
                                             true) / (double)obwi.oneShotRate;
         if(newDmgRate > WEAPONCHANGE_HYST_NUM * maxDmgRate / WEAPONCHANGE_HYST_DEN)
         {
            maxDmgRate = newDmgRate;
            maxWI = winfo;
         }
      }
   }
   if(maxWI)
   {
      if(demo_version >= 401)
      {
         cmd->weaponID = maxWI->id + 1;
         const weaponslot_t *slot = E_FindFirstWeaponSlot(pl, maxWI);
         cmd->slotIndex = slot->slotindex;
      }
      else
      {
         cmd->buttons |= BT_CHANGE;
         cmd->buttons |= maxWI->dehnum << BT_WEAPONSHIFT;
      }
   }
}

void Bot::doCombatAI(const Collection<Target>& targets)
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
                highestThreat->type != TargetMonster ? nullptr : highestThreat->mobj);

    angle_t highestTangle = P_PointToAngle(mx, my, highestThreat->coord.x, highestThreat->coord.y);

    int16_t angleturn = (int16_t)(highestTangle >> 16) - (int16_t)(pl->mo->angle >> 16);
    angleturn >>= 2;
    //if (!angleturn)
    {
//            angleturn += random.range(-128, 128);

    }

    if (angleturn > 2500)
        angleturn = 2500;
    if (angleturn < -2500)
        angleturn = -2500;

    if (!m_intoSwitch)
        cmd->angleturn = angleturn * random.range(1, 2);

   if(highestThreat->type == TargetMonster)
      pickBestWeapon(*highestThreat);

   Mobj *linetarget = 0;
   CAM_AimLineAttack(pl->mo, pl->mo->angle, MISSILERANGE / 2, true, &linetarget);

    if (linetarget && !linetarget->player && !(linetarget->flags & MF_FRIEND) && pl->readyweapon)
    {
       const BotWeaponInfo &bwi = g_botweapons[pl->pclass][pl->readyweapon];
       const Mobj &t = *linetarget;
       fixed_t dist = (v2fixed_t(t) - v2fixed_t(mx, my)).sqrtabs();

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

    if(targets[0].type == TargetLine && pl->readyweapon)
    {
        angle_t vang[2];
        vang[0] = P_PointToAngle(mx, my, targets[0].gline->v1->x, targets[0].gline->v1->y);
        vang[1] = P_PointToAngle(mx, my, targets[0].gline->v2->x, targets[0].gline->v2->y);
        
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
    else if (targets[0].type == TargetMonster && pl->readyweapon && pl->readyweapon->dehnum == wp_missile/* && emax(D_abs(mx - nx),
                                              D_abs(my - ny)) <= 128 * FRACUNIT*/)
    {
       // avoid using the rocket launcher for now...
        pickRandomWeapon(targets[0]);
    }
//    else if (random.range(1, 300) == 1)
//    {
//        pickRandomWeapon(targets[0]);
//    }

    if(targets[0].type != TargetLine && pl->readyweapon)
    {

       int explosion = B_MobjDeathExplosion(*targets[0].mobj);

        if (pl->readyweapon->dehnum == wp_fist ||
            pl->readyweapon->dehnum == wp_chainsaw)
        {
            if (highestThreat != &targets[0] || explosion > pl->health / 4)
            {
                pickRandomWeapon(targets[0]);
            }
           if(!stepLedges(true, {}))
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
           // FIXME: detect bot being stuck a bit better
           if (random() % 128 == 0 || m_realVelocity.chebabs() < FRACUNIT)
              toggleStrafeState();

           // Check ledges!
            if (random() % 8 != 0)
            {
               if(!stepLedges(true, {}))
               {
                   fixed_t dist = (v2fixed_t(nx, ny) - v2fixed_t(mx, my)).sqrtabs();

                  // TODO: portal-aware
                  int explodist = (emax(D_abs(nx - mx), D_abs(ny - my)) - pl->mo->radius) >> FRACBITS;
                  if(explodist < 0)
                     explodist = 0;

                  fixed_t straferange = 384 * FRACUNIT + M_DoubleToFixed(cinfo.totalThreat * 3);
                  fixed_t backoffrange = (explosion ? (explosion + 128) * FRACUNIT : MELEERANGE) + M_DoubleToFixed(cinfo.totalThreat * 3);

                  if (dist < backoffrange + targets[0].mobj->radius)
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
                   if (cinfo.hasShooters && dist < straferange)
                   {
                      // only circle-strafe if there are blowers
                       cmd->forwardmove += FixedMul(2 * pl->pclass->forwardmove[1],
                           B_AngleSine(dangle)) * m_combatStrafeState;
                       cmd->sidemove += FixedMul(2 * pl->pclass->sidemove[1],
                           B_AngleCosine(dangle)) * m_combatStrafeState;
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
// True if given neigh goes into a sector that will open up soon, or if it's an incoming crusher.
//
bool Bot::shouldWaitSector(const BNeigh &neigh) const
{
   if (!botMap->canPassNow(*ss, *neigh.otherss, pl->mo->height))
       return true;

    const sector_t* nsector = neigh.otherss->msector->getCeilingSector();
    const sector_t* msector = ss->msector->getCeilingSector();

    if(nsector != msector)
    {
        const CeilingThinker* ct = thinker_cast<const CeilingThinker*>(nsector->ceilingdata);

       // TODO: actually try to stop asap
        if(ct && ct->crush > 0 && ct->direction == plat_down)
            return true;
    }
   return false;
}

//
// True if bot would like to open the door
//
static bool B_wantOpenDoor(const sector_t &sector)
{
   auto doorTh = thinker_cast<VerticalDoorThinker *>(sector.ceilingdata);
   return (!doorTh &&
           sector.ceilingheight < P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT) ||
   (doorTh && doorTh->direction == plat_down);
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
        LevelStateStack::SetKeyPlayer(pl);
        if(!m_finder.FindNextGoal(v2fixed_t(*pl->mo), m_path, m_deepPromise.isUrgent(),
                                  objOfInterest, this))
        {
            ++m_searchstage;
           m_deepPromise.clear();
           B_Log("Lose promise");
            cmd->sidemove += random.range(-pl->pclass->sidemove[0], pl->pclass->sidemove[0]);
            cmd->forwardmove += random.range(-pl->pclass->forwardmove[0], pl->pclass->forwardmove[0]);
           if(m_searchstage > DUNNO_CONCESSION_SEC * TICRATE &&
              shouldChat(IDLE_CHAT_INTERVAL_SEC, m_lastDunnoMessage))
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
   v2fixed_t mpos = v2fixed_t(*pl->mo);
   v2fixed_t npos;
    bool dontMove = false;
    const BSubsec* doorss = nullptr;

    v2fixed_t endCoord;
   const line_t *swline = nullptr;
    if (m_path.end.kind == BotPathEnd::KindCoord)
        endCoord = m_path.end.coord;
    else if (m_path.end.kind == BotPathEnd::KindWalkLine)
    {
        swline = m_path.end.walkLine;
        endCoord = B_ProjectionOnSegment(mpos, v2fixed_t(*swline->v1),
                                         v2fixed_t(swline->dx, swline->dy), pl->mo->radius);
    }
    else
        endCoord = { 0, 0 };

    if (ss == m_path.last)
    {
       npos = endCoord;

       m_lostPathInfo.setEndCoord(*ss, npos);
    }
    else
    {
        // from end to path to beginning
        bool onPath = false;
       bool haveplat = false;
        for (const BNeigh* neigh : m_path.inv)
        {
            if (!botMap->canPass(*neigh->myss, *neigh->otherss, pl->mo->height))
                break;

           const PlatThinker* pt = thinker_cast<const PlatThinker*>
                 (neigh->otherss->msector->getFloorSector()->floordata);

             if(pt && pt->wait > 0)
             {
                if(!m_runfast)
                {
                   B_Log("Run fast");
                }
                m_runfast = true;
                haveplat = true;
             }

           const sector_t &sector = *neigh->otherss->msector->getCeilingSector();
           if(botMap->sectorFlags[&sector - ::sectors].door.valid && B_wantOpenDoor(sector))
              doorss = neigh->otherss;
           if(neigh->myss != ss)
              continue;

           if(!haveplat && m_runfast)
           {
              m_runfast = false;
              B_Log("stop running fast");
           }

           npos = B_ProjectionOnSegment(mpos, neigh->v, neigh->d, pl->mo->radius);
           dontMove = shouldWaitSector(*neigh);

           m_lostPathInfo.setMidSS(*ss, *neigh);

             if(random() % 64 == 0 && m_dropSS.count(ss))
             {
                 B_Log("Removed goner %d", (int)(ss - &botMap->ssectors[0]));
                 m_dropSS.erase(ss);
             }
             onPath = true;
             break;

        }
        
        if (!onPath)
        {
            // not on path, so reset
           bool recovering = false;
            if (m_lostPathInfo.ss)
            {
                if (!botMap->canPassNow(*ss, *m_lostPathInfo.ss, pl->mo->height))
                {
                    B_Log("Inserted goner %d", eindex(m_lostPathInfo.ss - &botMap->ssectors[0]));
                    m_dropSS.insert(m_lostPathInfo.ss);
                    for (const BNeigh& n : m_lostPathInfo.ss->neighs)
                    {
                       // Also add nearby subsectors to increase the slowdown area
                        if ((n.otherss->mid - m_lostPathInfo.ss->mid).sqrtabs() < 128 * FRACUNIT)
                            m_dropSS.insert(n.otherss);
                    }
                }
               else if(m_lostPathInfo.timeToRecover())
               {
                  if(m_lostPathInfo.islast)
                     npos = m_lostPathInfo.coord;
                  else
                  {
                     npos = B_ProjectionOnSegment(mpos, m_lostPathInfo.neigh->v,
                                                  m_lostPathInfo.neigh->d, pl->mo->radius);
                     dontMove = botMap->canPass(*ss, *m_lostPathInfo.neigh->otherss, pl->mo->height) && shouldWaitSector(*m_lostPathInfo.neigh);
                     if(dontMove)
                        m_lostPathInfo.counter--;
                  }
                  recovering = true;
               }
            }
           if(!recovering)
           {
              B_Log("Lost path");
              m_lostPathInfo = {};
               m_searchstage = SearchStage_Normal;
               m_hasPath = false;
               if (random() % 3 == 0)
                   m_justGotLost = true;
               return;
           }
        }
    }

    m_intoSwitch = false;
    if (goalTable.hasKey(BOT_WALKTRIG) && m_path.end.kind == BotPathEnd::KindWalkLine &&
        EV_IsSwitchSpecial(*m_path.end.walkLine) && B_checkSwitchReach(mpos, endCoord, *swline))
    {
        m_intoSwitch = true;
        if(gametic % 2 == 0)
            cmd->buttons |= BT_USE;
    }
    else if(doorss)
    {
        LevelStateStack::UseRealHeights(true);
        const sector_t& doorsec = *doorss->msector->getCeilingSector();
        LevelStateStack::UseRealHeights(false);

       const BotMap::SectorTrait &trait = botMap->sectorFlags[&doorsec - ::sectors];

       if(trait.door.valid && B_wantOpenDoor(doorsec))
       {
          for(const line_t *line : trait.doorlines)
          {
             v2fixed_t point = B_ProjectionOnSegment(mpos, v2fixed_t(*line->v1),
                                                     v2fixed_t(line->dx, line->dy), 0);
             if((point - mpos).sqrtabs() < USERANGE)
             {
                m_intoSwitch = true;
                if(gametic % 2 == 0)
                   cmd->buttons |= BT_USE;
                break;
             }
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
        moveslow = m_justGotLost && (mpos - m_path.start).sqrtabs() < pl->mo->radius * 2;
        
        if(!moveslow)
            m_justGotLost = false;
    }
    moveslow |= m_dropSS.count(ss) ? true : false;
    
    angle_t tangle = (npos - mpos).angle();
    angle_t dangle = tangle - pl->mo->angle;

   bool avoidturn = dangle > ANG45 && dangle < ANG270 + ANG45 &&
         (npos - mpos).sqrtabs() < 32 * FRACUNIT;


//   bool runfast = m_runfast || (/*dangle > ANG90 && dangle < ANG270 && */(npos - mpos).sqrtabs() < 32 * FRACUNIT);

    if (random() % 128 == 0)
        m_straferunstate = random.range(-2, 2) / 2;
    if (!m_intoSwitch)
        tangle += ANG45 * m_straferunstate;

    int16_t angleturn = (int16_t)(tangle >> 16) - (int16_t)(pl->mo->angle >> 16);
    angleturn >>= 2;

    if (angleturn > 2500)
        angleturn = 2500;
    if (angleturn < -2500)
        angleturn = -2500;

    if (!dontMove && ((endCoord - mpos).sqrtabs() >= 16 * FRACUNIT || D_abs(angleturn) <= 300 ||
                      !m_realVelocity))
    {
        if(m_runfast && !m_intoSwitch)
            cmd->forwardmove += FixedMul((moveslow ? 1 : 2)
                                         * pl->pclass->forwardmove[moveslow ? 0 : 1],
                                         B_AngleCosine(dangle));
        if (m_intoSwitch /*&& ss == m_path.last && cmd->forwardmove < 0*/)
        {
            cmd->forwardmove += FixedMul(pl->pclass->forwardmove[0] / 4, B_AngleCosine(dangle));
        }
        else
        {
            if(!m_runfast)
                cruiseControl(npos, moveslow);
            else
                cmd->sidemove -= FixedMul((moveslow ? 1 : 2)
                                          * pl->pclass->sidemove[moveslow ? 0 : 1],
                                          B_AngleSine(dangle));
        }
    }

    if ((!m_runfast && !avoidturn) || m_intoSwitch)
       cmd->angleturn += angleturn;

    // If not moving while trying to, budge a bit to avoid stuck moments
    if ((cmd->sidemove || cmd->forwardmove) && m_realVelocity.chebabs() < FRACUNIT)
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
bool Bot::stepLedges(bool avoid, v2fixed_t npos)
{
   auto mpos = v2fixed_t(*pl->mo);

   v2fixed_t movement = m_realVelocity.fixedmul(DRIFT_TIME);

   return movement &&
   !botMap->pathTraverse({ mpos, movement }, [this, mpos, avoid, npos](const BotMap::Line &line, const divline_t &dl, fixed_t frac) {

        int s = P_PointOnDivlineSide(dl.v, divline_t::points(*line.v[0], *line.v[1]));

        if(botMap->canPassNow(line.msec[s], line.msec[s ^ 1], pl->mo->height) &&
           !botMap->canPassNow(line.msec[s ^ 1], line.msec[s], pl->mo->height))
        {
            v2fixed_t targvel;
           if(!avoid)
              targvel = (npos - mpos).fixedmul(DRIFT_TIME_INV) - m_realVelocity;
           else
           {
              // try to avoid the ledge instead
              // select the position in front of the line
              // we have our direction divline and the crossed line
              // we must try to get back
              // get the intersection vector. We have frac
              v2fixed_t dest = dl.v + dl.dv.fixedmul(frac);
              dest -= v2fixed_t::polar(pl->mo->radius, dl.angle());

              targvel = (dest - mpos).fixedmul(DRIFT_TIME_INV) - m_realVelocity;
           }

            if(targvel.chebabs() <= FRACUNIT)
            {
                cmd->forwardmove += pl->pclass->forwardmove[0]; // hack
                return false;
            }

            fixed_t finedangle = (targvel.angle() - pl->mo->angle) >> ANGLETOFINESHIFT;

            cmd->forwardmove += FixedMul(pl->pclass->forwardmove[1], finecosine[finedangle]);
            cmd->sidemove -= FixedMul(pl->pclass->sidemove[1], finesine[finedangle]);

         return false;
      }

      return true;
   });
}

void Bot::cruiseControl(v2fixed_t npos, bool moveslow)
{

   if(stepLedges(false, npos))
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

    // FIXME: instead of hardcoding, calculate speed from friction and acceleration
    fixed_t runSpeed = moveslow && !m_runfast ? 8 * FRACUNIT : 16 * FRACUNIT;
    if(m_straferunstate)
        runSpeed = runSpeed * 64 / 50;
    angle_t tangle = (npos - *pl->mo).angle();
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
   {
      // If bot isn't the console player but is controlled from this computer, then it's not a real
      // multiplayer peer, so we need to initialize its ticcmd here.
      static playerinput_t zeroinput;
      G_BuildTiccmd(cmd, *pl, zeroinput, false);
   }
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

   // Update the velocity
   m_realVelocity = v2fixed_t(*pl->mo) - m_lastPosition;
   m_lastPosition = v2fixed_t(*pl->mo);
   
   // Get current values
   ss = &botMap->pointInSubsector(m_lastPosition);

    if(pl->health <= 0)
    {
       if(gametic % DEATH_REVIVE_INTERVAL == 0)
          cmd->buttons |= BT_USE; // respawn
       return; // don't try anything else in this case
    }

   // Do non-combat for now

   doNonCombatAI();
   Collection<Target> targets;
    enemyVisible(targets);
    if (!targets.isEmpty())
       doCombatAI(targets);
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
      bots[i].cmd = &bots[i].pl->cmd;
      B_AnalyzeWeapons(bots[i].pl->pclass);
   }
}

// EOF

