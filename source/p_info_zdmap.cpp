//
// The Eternity Engine
// Copyright (C) 2021 James Haley et al.
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
// Purpose: Hexen and ZDoom MAPINFO lump processing
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "e_things.h"
#include "ev_specials.h"
#include "f_finale.h"
#include "g_game.h"
#include "metaapi.h"
#include "p_info.h"
#include "xl_mapinfo.h"


//
// Handle MAPINFO next and secretnext which work both for next level and finale
//
static void P_handleMapInfoNext(const MetaTable *xlmi)
{
   //
   // Sets the normal or secret next option. Returns true if it's a finale special
   //
   auto setNextOrFinale = [](const char *s, bool secret) -> bool
   {
      int &finaleType = secret ? LevelInfo.finaleSecretType : LevelInfo.finaleType;
      const char *&nextLevel = secret ? LevelInfo.nextSecret : LevelInfo.nextLevel;

      bool getfinale = true;

      // Check for finale!
      // TODO: check if endgameC and endgameW must be with capitals
      if(!strcasecmp(s, "endgame1"))
         finaleType = FINALE_DOOM_CREDITS;
      else if(!strcasecmp(s, "endgame2"))
         finaleType = FINALE_DOOM_DEIMOS;
      else if(!strcasecmp(s, "endgame3") || !strcasecmp(s, "endbunny"))
         finaleType = FINALE_DOOM_BUNNY;
      else if((!strcasecmp(s, "endgamec") && s[7] == 'C') || !strcasecmp(s, "endcast"))
         LevelInfo.endOfGame = true;
      else if((!strcasecmp(s, "endgamew") && s[7] == 'W') || !strcasecmp(s, "endunderwater"))
         finaleType = FINALE_HTIC_WATER;
      else if(!strcasecmp(s, "enddemon"))
         finaleType = FINALE_HTIC_DEMON;
      else
      {
         nextLevel = s;
         getfinale = false;
      }
      // FIXME: endpic support (figure out the syntax) and others
      if(getfinale)
         P_EnsureDefaultStoryText(secret);
      return getfinale;
   };

   // Check prior end-of-game

   const char *next = xlmi->getString("next", nullptr);
   const char *secretnext = xlmi->getString("secretnext", nullptr);

   if(!next && !secretnext)   // no next map set
      return;

   bool nextfinale = false;
   if(next)
      nextfinale = setNextOrFinale(next, false);
   bool secretnextfinale = false;
   if(secretnext)
      secretnextfinale = setNextOrFinale(secretnext, true);

   // Disable any previously set restrictions
   if(nextfinale)
      LevelInfo.finaleSecretOnly = false;
   if(secretnextfinale)
      LevelInfo.finaleNormalOnly = false;

   // Check for end of game and disable if we actually set the next map
   if(LevelInfo.endOfGame)
   {
      if(next && !nextfinale)
         LevelInfo.finaleSecretOnly = true;
      else if(secretnext && !secretnextfinale)
         LevelInfo.finaleNormalOnly = true;
   }
}


//
// Helper to add MAPINFO custom boss action not covered by bossspec flags
//
static void P_addMapInfoBossAction(const mobjtype_t type, const char *name, int args0, int args1)
{
   auto action = estructalloctag(levelaction_t, 1, PU_LEVEL);
   action->flags = levelaction_t::BOSS_ONLY;
   action->mobjtype = type;
   const ev_binding_t *binding = EV_UDMFEternityBindingForName(name);
   I_Assert(binding, "Binding must exist\n");
   action->special = binding->actionNumber;
   action->args[0] = args0;
   action->args[1] = args1;
   action->next = LevelInfo.actions;
   LevelInfo.actions = action;
}

//
// Handle MAPINFO boss specials. Unlike EMAPINFO, they can branch off both into bossspec and
// bossaction
//
static void P_handleMapInfoBossSpecials(const MetaTable &xlmi)
{
   auto hasflag = [&xlmi](const char *name)
   {
      return xlmi.getInt(name, -1) >= 0;
   };

   struct bossspecialbinding_t
   {
      const char *name; // name of the binding key in MAPINFO
      unsigned bspec;   // LevelInfo boss special flag where the monster flag natively maps to
      unsigned mflag2;  // Monster flag who represents the traditional monster class (e.g. baron)
   };

   struct specialactionbinding_t
   {
      const char *name;          // name of the binding key in MAPINFO
      unsigned nativebspecs;     // One or more native LevelInfo specs where this action is native
      const char *specialname;   // Name of actual special to execute if action is not native
      int args[NUMLINEARGS];     // Args for the special to execute
   };

   // Conditions: which monsters to die
   // NOTE: currently Heretic monsters aren't implemented. We only focus on supporting cross-port 
   // maps which get this treatment, and those usually aim Boom. The cross-port Heretic megawads are
   // all vanilla.
   static const bossspecialbinding_t bossspecialbindings[] =
   {
      { "baronspecial", BSPEC_E1M8, MF2_E1M8BOSS },
      { "cyberdemonspecial", BSPEC_E2M8 | BSPEC_E4M6, MF2_E2M8BOSS | MF2_E4M6BOSS },
      { "spidermastermindspecial", BSPEC_E3M8 | BSPEC_E4M8, MF2_E3M8BOSS | MF2_E4M8BOSS },
   };

   // Actions: what to do
   static const specialactionbinding_t specialactionbindings[] =
   {
      { "specialaction_lowerfloor", BSPEC_E1M8 | BSPEC_E4M8, "Floor_LowerToLowest",
            { 666, 8, 0, 0, 0 } },
      { "specialaction_exitlevel", BSPEC_E2M8 | BSPEC_E3M8, "Exit_Normal", { 0, 0, 0, 0, 0 } },
      { "specialaction_opendoor", BSPEC_E4M6, "Door_Open", { 666, 64, 0, 0, 0 } },
   };

   for(const bossspecialbinding_t &monsterBinding : bossspecialbindings)
   {
      if(!hasflag(monsterBinding.name))
         continue;

      for(const specialactionbinding_t &actionBinding : specialactionbindings)
      {
         if(!hasflag(actionBinding.name))
            continue;

         // If the action coincides with the base game association (e.g. barons or other 
         // MF2_E1M8BOSS to lower floors), then just assign LevelInfo.bossSpecs.
         unsigned commonbspecs = actionBinding.nativebspecs & monsterBinding.bspec;
         if(commonbspecs)
            LevelInfo.bossSpecs |= commonbspecs;
         else
         {
            // Otherwise we need to use custom boss level actions. Thanks to UMAPINFO for driving
            // these into Eternity.
            E_ForEachMobjInfoWithAnyFlags2(monsterBinding.mflag2,
               [](const mobjinfo_t &info, void *context)
               {
                  auto actionBinding = static_cast<const specialactionbinding_t *>(context);
                  P_addMapInfoBossAction(info.index, actionBinding->specialname,
                     actionBinding->args[0], actionBinding->args[1]);
                  return true;
               }, const_cast<specialactionbinding_t *>(&actionBinding));
         }
      }
   }
}

//
// P_applyHexenMapInfo
//
// haleyjd 01/26/14: Applies data from Hexen MAPINFO
//
void P_ApplyHexenMapInfo()
{
   MetaTable *xlmi = nullptr;
   const char *s = nullptr;
   int i;

   if(!(xlmi = XL_MapInfoForMapName(gamemapname)))
      return;

   LevelInfo.levelName = xlmi->getString("name", "");

   // sky textures
   if((s = xlmi->getString("sky1", nullptr)))
   {
      LevelInfo.skyName = s;
      // FIXME: currently legacy MAPINFO is still integer-only
      LevelInfo.skyDelta = M_DoubleToFixed(xlmi->getDouble("sky1delta", 0));

      LevelInfo.enableBoomSkyHack = false;
   }
   if((s = xlmi->getString("sky2", nullptr)))
   {
      LevelInfo.sky2Name = s;
      LevelInfo.sky2Delta = M_DoubleToFixed(xlmi->getDouble("sky2delta", 0));

      LevelInfo.enableBoomSkyHack = false;
   }

   // double skies
   if((i = xlmi->getInt("doublesky", -1)) >= 0)
      LevelInfo.doubleSky = !!i;

   // lightning
   if((i = xlmi->getInt("lightning", -1)) >= 0)
      LevelInfo.hasLightning = !!i;

   // colormap
   if((s = xlmi->getString("fadetable", nullptr)))
      LevelInfo.colorMap = s;

   // TODO: cluster, warptrans

   P_handleMapInfoNext(xlmi);

   // titlepatch for intermission
   if((s = xlmi->getString("titlepatch", nullptr)))
      LevelInfo.levelPic = s;

   // TODO: cdtrack

   // par time
   if((i = xlmi->getInt("par", -1)) >= 0)
      LevelInfo.partime = i;

   if((s = xlmi->getString("music", nullptr)))
      LevelInfo.musicName = s;

   // flags
   if((i = xlmi->getInt("nointermission", -1)) >= 0)
      LevelInfo.killStats = !!i;
   if((i = xlmi->getInt("evenlighting", -1)) >= 0)
      LevelInfo.unevenLight = !i;
   if((i = xlmi->getInt("map07special", -1)) >= 0)
      LevelInfo.bossSpecs |= BSPEC_MAP07_1 | BSPEC_MAP07_2;
   if((i = xlmi->getInt("noautosequences", -1)) >= 0)
      LevelInfo.noAutoSequences = !!i;
   if((i = xlmi->getInt("nojump", -1)) >= 0)
      LevelInfo.disableJump = true;

   P_handleMapInfoBossSpecials(*xlmi);

   /*
   Stuff with "Unfinished Business":
   qstring name;
   qstring next;
   qstring secretnext;
   qstring titlepatch;
   */
}

// EOF
