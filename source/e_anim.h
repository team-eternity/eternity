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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
// Purpose: EDF animation definitions
// Authors: Ioan Chera
//


#ifndef E_ANIM_H_
#define E_ANIM_H_

constexpr const char EDF_SEC_ANIMATION[] = "animation";

#include "Confuse/confuse.h"
#include "m_collection.h"
#include "m_dllist.h"
#include "m_qstr.h"

//
// EDF animation definition
//
class EAnimDef : public ZoneObject
{
public:
   //
   // Type of animation
   //
   enum type_t
   {
      type_flat,
      type_wall
   };

   //
   // Flags
   //
   enum
   {
      SWIRL = 1,  // swirly texture
   };

   //
   // Pic in sequence (if Hexen style)
   //
   class Pic : public ZoneObject
   {
   public:
      Pic() : offset(), ticsmin(), ticsmax(), flags()
      {
      }

      void reset();

      qstring name;  // either direct name
      int offset;    // or offset to startpic. 1-based. If 0, name takes charge.
      int ticsmin;   // minimum time
      int ticsmax;   // if not -1, it's the maximum number of tics, randomly
      unsigned flags;
   };

   EAnimDef() : type(), tics(), flags(), link()
   {
      static Pic pic;
      pics.setPrototype(&pic);
   }

   void reset(type_t intype = type_wall);

   type_t type;      // kind (wall or flat)
   qstring startpic; // animation start pic
   qstring endpic;   // end pic (if Doom style)
   int tics;         // tics to end pic
   unsigned flags;
   Collection<Pic> pics;   // sequence of pics (if Hexen style)
   DLListItem<EAnimDef> link;
};

extern cfg_opt_t edf_anim_opts[];
extern PODCollection<EAnimDef *> eanimations;

void E_ProcessAnimations(cfg_t *cfg);
void E_AddAnimation(const EAnimDef &def);

bool E_IsHexenAnimation(const char *startpic, EAnimDef::type_t type);

#endif

// EOF
