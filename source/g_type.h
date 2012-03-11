// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Extensible game types.
//
//----------------------------------------------------------------------------

#ifndef G_TYPE_H__
#define G_TYPE_H__

#include "doomdef.h"
#include "info.h"
#include "m_fixed.h"

struct emod_t;
struct line_t;
struct player_t;

class Mobj;

enum
{
   xgt_none,
   xgt_cooperative,
   xgt_duel,
   xgt_deathmatch,
   xgt_capture_the_flag
};

enum
{
   sov_normal,
   sov_impressive,
   sov_humiliation,
   sov_blowout,
};

class BaseGameType : public ZoneObject
{
protected:
   char *name;
   char *all_caps_name;
   uint8_t id;

public:
   BaseGameType(const char *new_name);
   BaseGameType(const char *new_name, uint8_t new_id);
   ~BaseGameType();

   uint8_t     getID() const;
   const char* getName();
   const char* getCapitalizedName();

   virtual int getStrengthOfVictory(float low_score, float high_score);

   virtual bool shouldExitLevel();

   virtual void handleNewLevel(skill_t skill, char *name);

   virtual void handleLoadLevel();

   virtual void handleExitLevel();

   virtual bool usesFlagsAsScore();

   virtual void handleActorSpawned(fixed_t x, fixed_t y, fixed_t z,
                                   mobjtype_t type, Mobj *actor);

   virtual void handleActorDamaged(Mobj *target, Mobj *inflictor, Mobj *source, 
                                   int damage, int mod);

   virtual void handleActorKilled(Mobj *source, Mobj *target, emod_t *mod);

   virtual void handleActorRemoved(Mobj *actor);

   virtual bool handleActorTouchedSpecial(Mobj *special, Mobj *toucher);

   virtual void handleActorUsedSpecialLine(Mobj *thing, line_t *line,
                                           int side);

   virtual void handleActorShotSpecialLine(Mobj *thing, line_t *line,
                                           int side);

   virtual void handleActorCrossedSpecialLine(Mobj *thing, line_t *line,
                                              int side);

   virtual void handleActorPositionSet(Mobj *thing);

   virtual void handleActorPositionUnset(Mobj *thing);

   virtual void handleActorStateChanged(Mobj* mobj, statenum_t state,
                                        bool still_exists);

   virtual void handlePlayerInSpecialSector(player_t *player);

   virtual void handleClientSpectated(int clientnum);

   virtual void handleClientChangedTeam(int clientnum);

   virtual void handleClientDisconnected(int clientnum);

   virtual void tick();
};

void G_SetGameType(uint8_t game_type_id);
void G_NewGameType();

#endif

