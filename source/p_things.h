//
// Copyright (C) 2014 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// 
// Mobj-related line specials. Mostly from Hexen.
//
//-----------------------------------------------------------------------------

#ifndef P_THINGS_H__
#define P_THINGS_H__

#include "p_tick.h"
#include "r_defs.h" // needed for NUMLINEARGS

class LevelActionThinker : public Thinker
{
   DECLARE_THINKER_TYPE(LevelActionThinker, Thinker)

protected:
   int special;
   int args[NUMLINEARGS];
   int mobjtype;

public:
   virtual void Think() override;
   virtual void serialize(SaveArchive &arc) override;

   static void Spawn(int pSpecial, int *pArgs, int pMobjType);
};

void P_SpawnLevelActions();

#endif

// EOF

