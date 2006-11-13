// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
// EDF Player Class Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_PLAYER_H__
#define E_PLAYER_H__

// macros
#define NUMEDFSKINCHAINS 17

//
// playerclass_t structure
//
typedef struct playerclass_s
{
   skin_t *defaultskin; // pointer to default skin

   // hashing data
   char mnemonic[33];
   struct playerclass_s *next;
} playerclass_t;


// EDF-only stuff
#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_SKIN "skin"
extern cfg_opt_t edf_skin_opts[];

#define EDF_SEC_PCLASS "playerclass"
extern cfg_opt_t edf_pclass_opts[];

void E_ProcessSkins(cfg_t *cfg);
void E_ProcessPlayerClasses(cfg_t *cfg);

boolean E_NeedDefaultPlayerData(void);

#endif // NEED_EDF_DEFINITIONS

#endif

// EOF

