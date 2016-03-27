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
//      Thing classification. State walker.
//
//-----------------------------------------------------------------------------

#ifndef B_CLASSIFIER_H_
#define B_CLASSIFIER_H_


class Mobj;
struct actionargs_t;
struct mobjinfo_t;

// external dependencies
extern void A_BossDeath(actionargs_t *actionargs);
extern void A_HticBossDeath(actionargs_t *actionargs);
extern void A_KeenDie(actionargs_t *actionargs);

void B_UpdateMobjInfoSet(int numthingsalloc);

bool B_IsMobjSolidDecor(const Mobj &mo);
bool B_IsMobjHostile(const Mobj& mo);
bool B_IsMobjHitscanner(const Mobj& mo);
bool B_IsMobjExplosiveDeath(const Mobj& mo);
bool B_MonsterIsInPreAttack(const Mobj& mo);
bool B_MobjUsesCodepointer(const Mobj& mo, void(*action)(actionargs_t *args));

#endif 

// EOF

