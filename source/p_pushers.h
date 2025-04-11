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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   BOOM Push / Pull / Current Effects
//
//-----------------------------------------------------------------------------

#ifndef P_PUSHERS_H__
#define P_PUSHERS_H__

#include "p_tick.h" // for Thinker

struct line_t;
class  Mobj;
class  SaveArchive;

// phares 3/20/98: added new model of Pushers for push/pull effects

class PushThinker : public Thinker
{
   DECLARE_THINKER_TYPE(PushThinker, Thinker)

protected:
   void Think() override;

public:
   // Methods
   virtual void serialize(SaveArchive &arc) override;
   
   // Data Members
   enum
   {
      p_push,
      p_pull,
      p_wind,
      p_current,
   }; 

   int   type;
   Mobj *source;        // Point source if point pusher
   int   x_mag;         // X Strength
   int   y_mag;         // Y Strength
   int   magnitude;     // Vector strength for point pusher
   int   radius;        // Effective radius for point pusher
   int   x;             // X of point source if point pusher
   int   y;             // Y of point source if point pusher
   int   affectee;      // Number of affected sector
};

Mobj *P_GetPushThing(int); // phares 3/23/98
void P_SpawnPushers();

#endif

// EOF


