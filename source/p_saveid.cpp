//
// The Eternity Engine
// Copyright(C) 2021 James Haley, Ioan Chera, et al.
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
// Purpose: archiving named objects by their name and not index, for forward compatibility.
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "e_things.h"
#include "p_saveg.h"
#include "p_saveid.h"

enum
{
   VER = 7, // the save version which introduces this
};

//
// Mobjtype
//
void Archive_MobjType(SaveArchive &arc, mobjtype_t &type)
{
   if(arc.saveVersion() >= VER)
   {
      qstring fieldname;
      if(arc.isSaving())
         fieldname = mobjinfo[type]->name;
      arc.archiveCachedString(fieldname);
      if(arc.isLoading())
         type = E_SafeThingName(fieldname.constPtr());
   }
   else
      arc << type;
}

// EOF
