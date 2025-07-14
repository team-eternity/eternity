// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//    Custom damage types, or "Means of Death" flags.
//
//-----------------------------------------------------------------------------

#ifndef E_MOD_H__
#define E_MOD_H__

#include "m_dllist.h"
#include "m_fixed.h"
#include "info.h"

struct playerclass_t;

//
// morph species exclude placeholders. Values must be negative because positive values are
// mobjtype_t
//
enum
{
   MorphExcludeListEnd = -1,     // this is the terminator of the "exclude" list
   MorphExcludeInanimate = -2,   // from @inanimate
};

struct emodmorph_t
{
   mobjtype_t speciesID;
   mobjtype_t *excludedID;
   playerclass_t *pclass;
};

//
// emod structure
//
// Holds the info for a damage type.
//
struct emod_t
{
   DLListItem<emod_t>  numlinks;  // for numeric hash
   DLListItem<emod_t>  namelinks; // for name hash
   char               *name;      // name key
   int                 num;       // number key

   char *obituary;
   char *selfobituary;
   bool obitIsIndirect;
   bool selfObitIsIndirect;
   bool sourceless;

   emodmorph_t morph;

   fixed_t absolutePush;   // if set, push things by this amount
   fixed_t absoluteHop;    // if set, hop gravity things by this amount

   // For faster damagetype lookups in metatables
   size_t dfKeyIndex;
};

emod_t *E_DamageTypeForName(const char *name);
emod_t *E_DamageTypeForNum(int num);
int     E_DamageTypeNumForName(const char *name);

// This is actually in e_things.c but should be prototyped here.
const char *E_ModFieldName(const char *base, const emod_t *mod);

// EDF-only stuff
#ifdef NEED_EDF_DEFINITIONS

constexpr const char EDF_SEC_MOD[] = "damagetype";
constexpr const char EDF_SEC_MORPHTYPE[] = "morphtype";

extern cfg_opt_t edf_dmgtype_opts[];
extern cfg_opt_t edf_morphtype_opts[];

void E_ProcessDamageTypes(cfg_t *cfg);
void E_PrepareMorphTypes(cfg_t *cfg);
void E_ProcessMorphTypes(cfg_t *cfg);

#endif // NEED_EDF_DEFINITIONS

#endif

// EOF

