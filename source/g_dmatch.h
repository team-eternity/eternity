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
//   Deathmatch game type.
//
//----------------------------------------------------------------------------

#ifndef G_DMATCH_H__
#define G_DMATCH_H__

class DeathmatchGameType : public BaseGameType
{
public:
   DeathmatchGameType(const char *new_name);
   DeathmatchGameType(const char *new_name, uint8_t new_type);

   virtual int getStrengthOfVictory(float low_score, float high_score);
   virtual bool shouldExitLevel();
   virtual void handleActorKilled(Mobj *source, Mobj *target, emod_t *mod);
};

#endif

