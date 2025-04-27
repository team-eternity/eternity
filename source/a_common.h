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
// Purpose: Common action functions shared by all games.
// Authors: James Haley, Ioan Chera
//

#ifndef A_COMMON_H__
#define A_COMMON_H__

// Required for pr_class_t:
#include "m_random.h"

struct actionargs_t;
struct player_t;
class Mobj;
class SaveArchive;

void P_MakeSeeSound(Mobj *actor, pr_class_t rngnum);

void A_Chase(actionargs_t *actionargs);
void A_Die(actionargs_t *actionargs);
void A_Explode(actionargs_t *actionargs);
void A_FaceTarget(actionargs_t *actionargs);
void A_Fall(actionargs_t *actionargs);
void A_Look(actionargs_t *actionargs);
void A_Pain(actionargs_t *actionargs);

void P_CheckCustomBossActions(const Mobj &mo, const player_t &player);
void P_ArchiveHereticWeapons(SaveArchive &arc);

#endif

// EOF

