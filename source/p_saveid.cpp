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

#include <assert.h>
#include "z_zone.h"
#include "e_edf.h"
#include "e_sprite.h"
#include "e_states.h"
#include "e_things.h"
#include "p_saveg.h"
#include "p_saveid.h"
#include "r_data.h"
#include "r_draw.h"
#include "w_wad.h"

// IMPORTANT: as usual, when you change the saving structure, increment the save version in p_saveg
// and use that value in the new functions added here.

// For all internal names, use \r\a\t. It's a safe way to avoid user-defined names
#define RAT "\r\a\t"

//
// Archive colo(u)r translation from the 256-byte T_START/T_END tables
//
void Archive_ColorTranslation(SaveArchive &arc, int &colour)
{
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      if(arc.isSaving())
      {
         // IMPORTANT: the internal name must be longer than 8 characters.
         if(colour <= TRANSLATIONCOLOURS)
            fieldname.Printf(27, "Internal" RAT "%d", colour);
         else
            fieldname = R_TranslationNameForNum(colour);
      }
      arc.archiveCachedString(fieldname);
      if(arc.isLoading())
      {
         long index;
         char *endptr;

         // Check that it's Internal{###} and that ### is entirely a number within range, without
         // other garbage. If so, use the internal tables
         if(!fieldname.strNCmp("Internal" RAT, 11) &&
            (index = strtol(fieldname.constPtr() + 11, &endptr, 10)) <= TRANSLATIONCOLOURS &&
            index >= 0 && !*endptr)
         {
            colour = static_cast<int>(index);
         }
         else
         {
            // Otherwise look for the lump
            colour = R_TranslationNumForName(fieldname.constPtr());
            if(colour == -1)
               colour = 0; // safe default
         }
      }
   }
   else
      arc << colour;
}

//
// Archive a floor/ceiling texture
//
void Archive_Flat(SaveArchive &arc, int &flat)
{
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      if(arc.isSaving())
         fieldname = flat == -1 ? "no flat" RAT : textures[flat]->name;
      arc.archiveCachedString(fieldname);
      if(arc.isLoading())
         flat = fieldname == "no flat" RAT ? -1 : R_FindFlat(fieldname.constPtr());
   }
   else
      arc << flat;
}
void Archive_Flat(SaveArchive &arc, int16_t &flat)
{
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      if(arc.isSaving())
         fieldname = flat == -1 ? "no flat" RAT : textures[flat]->name;
      arc.archiveCachedString(fieldname);
      if(arc.isLoading())
         flat = fieldname == "no flat" RAT ? -1 : R_FindFlat(fieldname.constPtr());
   }
   else
      arc << flat;
}

//
// Save the mobj state
//
void Archive_MobjState_Save(SaveArchive &arc, const state_t &state)
{
   assert(arc.isSaving());
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      fieldname = state.name;
      arc.archiveCachedString(fieldname);
   }
   else
   {
      statenum_t index = state.index;
      arc << index;
   }
}

//
// Load the mobj state
//
state_t &Archive_MobjState_Load(SaveArchive &arc)
{
   int temp;
   assert(arc.isLoading());
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      arc.archiveCachedString(fieldname);
      temp = E_StateNumForNameIncludingDecorate(fieldname.constPtr());
   }
   else
      arc << temp;
   if(temp < 0 || temp >= NUMSTATES)
      temp = NullStateNum;
   return *states[temp];
}

//
// Mobjtype
//
void Archive_MobjType(SaveArchive &arc, mobjtype_t &type)
{
   if(arc.saveVersion() >= 7)
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

//
// Sprite
//
void Archive_SpriteNum(SaveArchive &arc, spritenum_t &sprite)
{
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      if(arc.isSaving())
         fieldname = sprnames[sprite];
      arc.archiveCachedString(fieldname);
      if(arc.isLoading())
      {
         sprite = E_SpriteNumForName(fieldname.constPtr());
         if(sprite == -1)
            sprite = blankSpriteNum;
      }
   }
   else
      arc << sprite;
}

//
// Archive 256x256 translucency map reference
//
void Archive_TranslucencyMap(SaveArchive &arc, int &tranmap)
{
   if(arc.saveVersion() >= 7)
   {
      qstring fieldname;
      if(arc.isSaving())
         fieldname = tranmap >= 0 ? wGlobalDir.getLumpName(tranmap) : "none";
      arc.archiveCachedString(fieldname);
      if(arc.isLoading())
         tranmap = fieldname == "none" ? -1 : W_CheckNumForName(fieldname.constPtr());
   }
   else
      arc << tranmap;
}

// EOF
