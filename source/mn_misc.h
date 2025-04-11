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
//--------------------------------------------------------------------------
//
// Misc menu stuff
//
// By Simon Howard 
//
//----------------------------------------------------------------------------

#ifndef MN_MISC_H__
#define MN_MISC_H__

#include "d_keywds.h"

// pop-up messages

void MN_Alert(E_FORMAT_STRING(const char *message), ...) E_PRINTF(1, 2);
void MN_Question(const char *message, const char *command);
void MN_QuestionFunc(const char *message, void (*handler)(void));

// map colour selection

enum class mapColorType_e : bool
{
   mandatory,
   optional
};

void MN_SelectColor(const char *variable_name, const mapColorType_e color_type);

#endif /* MN_MISC_H__ */

// EOF
