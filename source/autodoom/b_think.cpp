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

#include "b_think.h"
#include "b_util.h"
#include "../d_event.h"
#include "../d_gi.h"
//#include "../d_ticcmd.h"
//#include "../doomdef.h"
#include "../doomstat.h"
#include "../e_edf.h"
#include "../e_player.h"
#include "../ev_specials.h"
#include "../g_dmflag.h"
#include "../metaapi.h"
#include "../p_maputl.h"
#include "../p_spec.h"
#include "../r_state.h"

// The commands that the bots will send to the players to be added in G_Ticker
Bot bots[MAXPLAYERS];
std::unordered_map<player_t *, Bot *> botDict;

//
// Bot::mapInit
//
// Initialize bot for new map. Mostly cleanup stuff from previous session
//
void Bot::mapInit()
{
//   if(!active || gamestate != GS_LEVEL)
//      return;  // do nothing if out of game
   path.clear(); // reset path
   B_EmptyTableAndDelete(goalTable);   // remove all objectives
   B_EmptyTableAndDelete(goalEvents);  // remove all previously listed events
   memset(prevPathIdx, 0xff, sizeof(prevPathIdx));
   prevCtr = 0;
   searchstage = 0;
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
}

//
// Bot::wantArmour
//
// Whether it should pick up armour
//
bool Bot::wantArmour(itemeffect_t *ie) const
{
   int hits = ie->getInt("saveamount", 0);
   int savefactor = ie->getInt("savefactor", 1);
   int savedivisor = ie->getInt("savedivisor", 3);
   if(!hits || !savedivisor || !savefactor)
      return false;
   if(!(ie->getInt("alwayspickup", 0)) && pl->armorpoints >= hits)
      return false;
   
   return true;
}

//
// Bot::objectOfInterest
//
// Returns true if there's something good in that subsector
//
bool Bot::objectOfInterest(const BSeg *targsg, const BSubsec *targss,
                           v2fixed_t &destcoord)
{
   //itemeffect_t *ie;
   //weapontype_t wt;
   
   // Look for items
   std::unordered_map<spritenum_t, PlayerStats>::const_iterator effect, nopick;
   for (auto it = targss->mobjlist.begin(); it != targss->mobjlist.end(); ++it)
   {
      const Mobj &item = **it;
      if(&item == pl->mo)  // ignore player avatar
         continue;
      fixed_t fh = targss->msector->getFloorHeight();
      if(fh + pl->mo->height < item.z || fh > item.z + item.height)
         continue;
      if(item.flags & MF_SPECIAL)
      {
         if(item.sprite < 0 || item.sprite >= NUMSPRITES)   // avoid invalid
            continue;

         effect = effectStats.find(item.sprite);
         nopick = nopickStats.find(item.sprite);
         if(effect == effectStats.cend())
         {
            // unknown (new) item
            // Does it have nopick stats?
            if(nopick == nopickStats.cend())
            {
               // no. Totally unknown
               goalTable.setV2Fixed(BOT_PICKUP, destcoord = B_CoordXY(item));
               return true;
            }
            else
            {
               // yes. Is it greater than current status?
               if(nopick->second.greaterThan(*pl))
               {
                  // Yes. It might be pickable now
                  goalTable.setV2Fixed(BOT_PICKUP, destcoord = B_CoordXY(item));
                  return true;
               }
            }
         }
         else
         {
            // known item.
            // currently just try to pick it up
            if(nopick == nopickStats.cend() || 
               effect->second.fillsGap(*pl, nopick->second))
            {
               goalTable.setV2Fixed(BOT_PICKUP, destcoord = B_CoordXY(item));
               return true;
            }
         }
      }
   }
   
   // Look for special lines
   for (auto it = targss->linelist.begin(); it != targss->linelist.end(); ++it)
   {
      const line_t &line = **it;
      const ev_action_t *action = EV_ActionForSpecial(line.special);
      
      if(action)
      {
         if(action->type == &W1ActionType || action->type == &S1ActionType ||
            (action->type == &WRActionType && searchstage > 0) ||
            (action->type == &SRActionType && searchstage > 0) ||
            (action->type == &DRActionType && searchstage > 0))
         {
            bool found = true;
            if(line.tag)
            {
               for (int secnum = P_FindSectorFromLineTag(&line, -1);
                    secnum >= 0;
                    secnum = P_FindSectorFromLineTag(&line, secnum))
               {
                  found = false;
                  const sector_t &sector = sectors[secnum];
                  if(!sector.floordata && !sector.ceilingdata)
                  {
                     found = true;
                     break;
                  }
               }
            }
            else
            {
               const sector_t *sector = line.backsector;
               if(sector)
                  found = !sector->floordata && !sector->ceilingdata;
            }
            if(!found)
               continue;
            goalTable.setV2Fixed(BOT_WALKTRIG, B_CoordXY(*line.v1));
            destcoord.x = (line.v1->x + line.v2->x) / 2;
            destcoord.y = (line.v1->y + line.v2->y) / 2;
            return true;
         }
      }
   }
   
   return false;
}

//
// Bot::routePath
//
// Does the pathfinding
//
bool Bot::routePath()
{
	int j;
		 
   // Reset the search path
   prevPathIdx[0] = prevPathIdx[1] = -1;
	path.makeEmpty();
   B_EmptyTableAndDelete(goalTable);   // remove all objectives
   
   fixed_t startx, starty;
   path.addStartNode(startx = pl->mo->x, starty = pl->mo->y);    // Normal
   
	int imin, ifound;
	int64_t tentative_g_add;
   
   BSeg *seg;
   BSubsec *ss;
   
   v2fixed_t destcoord;
	
	while(1)
	{
		// This finds the index of the best score node
		imin = path.bestScoreIndex();
		if(imin < 0)
			break;
		
		path.closeNode(imin);
		
		path.getRef(imin, &seg, &ss);
		
		// This checks if a destination has been found on the spot
      if(objectOfInterest(seg, ss, destcoord))
      {
         // found goal
//         v2fixed_t destcoord = static_cast<MetaV2Fixed*>
//         (goalTable.getObjectType(METATYPE(MetaV2Fixed)))->getValue();
         
         B_Log("Path found to (%d %d)\n", destcoord.x >> FRACBITS, destcoord.y >> FRACBITS);
         path.finish(imin, destcoord, pl->mo->height);
         return true;
      }
      
		// This looks in all eight directions.
		for(j = 0; j < ss->nsegs; ++j)
		{
         BSeg &nextSeg = ss->segs[j];
         if(!nextSeg.partner)
            continue;
//         if(nextSeg.ln && nextSeg.ln->msec[!nextSeg.isback] == botMap->nullMSec)
//            continue;
         BSubsec &nextSS = *nextSeg.partner->owner;
         
         if (!botMap->canPass(*ss, nextSS, pl->mo->height))
            continue;
						
         
			// this looks if the entry exists in the list
			ifound = path.openCoordsIndex(nextSS);
			
			if(ifound == -2)
         {
            // found it as closed. Don't try to visit it again, so pass
				continue;
         }
			
			// This sets the score, depending on situation
         if(seg)
            tentative_g_add = P_AproxDistance(nextSeg.mid.x - seg->mid.x,
                                              nextSeg.mid.y - seg->mid.y);
         else
            tentative_g_add = P_AproxDistance(nextSeg.mid.x - startx,
                                              nextSeg.mid.y - starty);
         
			// Increase the effective (time) distance if a closed door is in
         // the way
			
         path.updateNode(ifound, imin, &nextSeg, &nextSS, tentative_g_add);
		}
	}
   
   // Found nothing with this search, so try to search for more on next attempt
   // ObjectOfInterest will look for more
   ++searchstage;
   
	return false;
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

//
// Bot::doNonCombatAI
//
// Does whatever needs to be done when not fighting
//
void Bot::doNonCombatAI()
{
   if(!path.exists())
   {
      if(!routePath())
      {
         cmd->sidemove += random.range(-pl->pclass->sidemove[0],
                                       pl->pclass->sidemove[0]);
         cmd->forwardmove += random.range(-pl->pclass->forwardmove[0],
                                       pl->pclass->forwardmove[0]);
         return;
      }
   }
   // found path to exit
	int nowon = path.straightPathCoordsIndex(pl->mo->x, pl->mo->y);
	
	if(nowon < 0 || prevCtr % 100 == 0)
	{
		// Reset if out of the path, or if a pushwall stopped moving
      searchstage = 0;
		path.reset();
		return;
	}
   
   if(goalTable.hasKey(BOT_WALKTRIG) && prevCtr % 14 == 0)
      cmd->buttons |= BT_USE;
   
   int nexton = path.getNextStraightIndex(nowon);
   fixed_t mx, my, nx, ny;
   mx = pl->mo->x;
   my = pl->mo->y;
   if(nexton == -1)
      path.getFinalCoord(nx, ny);
   else
      path.getStraightCoords(nexton, nx, ny);
   
   if(goalAchieved())
      path.reset();  // only reset if reached destination
   
   angle_t tangle = P_PointToAngle(mx, my, nx, ny);
   angle_t dangle = tangle - pl->mo->angle;

//   if(dangle < ANG45 || dangle > ANG270 + ANG45)
   cmd->forwardmove += FixedMul(2*pl->pclass->forwardmove[1],
                                B_AngleCosine(dangle));
   cmd->sidemove -= FixedMul(2*pl->pclass->sidemove[1], B_AngleSine(dangle));

   int16_t angleturn = (int16_t)(tangle >> 16) - (int16_t)(pl->mo->angle >> 16);
   angleturn >>= 3;
   
   if(angleturn > 1500)
      angleturn = 1500;
   if(angleturn < -1500)
      angleturn = -1500;
   
   cmd->angleturn += angleturn;
//   printf("%d\n", angleturn);
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
   
   ++prevCtr;
   
   // Get current values
   ss = &botMap->pointInSubsector(pl->mo->x, pl->mo->y);
   cmd = &pl->cmd;
   
   // Do non-combat for now
   doNonCombatAI();
   
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
      botDict[players + i] = bots + i;
   }
}

//
// Bot::getNopickStats
//
// Gets the nopick state, creating one if not existing
//
PlayerStats &Bot::getNopickStats(spritenum_t spnum)
{
   auto nopick = nopickStats.find(spnum);
   if(nopick == nopickStats.cend())
   {
      nopickStats.emplace(spnum, PlayerStats(true));
      return nopickStats.find(spnum)->second;
   }

   return nopick->second;
}

//
// Bot::getEffectStats
//
// Gets the effect state, creating one if not existing
//
PlayerStats &Bot::getEffectStats(spritenum_t spnum)
{
   auto effect = effectStats.find(spnum);
   if(effect == effectStats.cend())
   {
      effectStats.emplace(spnum, PlayerStats(false));
      return effectStats.find(spnum)->second;
   }

   return effect->second;
}

// EOF

