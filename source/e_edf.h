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
//------------------------------------------------------------------------------
//
// Purpose: EDF core.
// Authors: James Haley, Max Waine
//

#ifndef __E_EDF_H__
#define __E_EDF_H__

// sprite variables

extern int blankSpriteNum;

void E_ProcessEDF(const char *filename);
void E_ProcessNewEDF(void);

void E_EDFSetEnableValue(const char *, int); // enables

void E_EDFLogPuts(const char *msg);
void E_EDFLogPrintf(E_FORMAT_STRING(const char *msg), ...) E_PRINTF(1, 2);

[[noreturn]] void E_EDFLoggedErr(int lv, E_FORMAT_STRING(const char *msg), ...) E_PRINTF(2, 3);
void E_EDFLoggedWarning(int lv, E_FORMAT_STRING(const char *msg), ...) E_PRINTF(2, 3);

#endif

// EOF

