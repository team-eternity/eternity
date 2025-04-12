//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Ioan Chera, et al.
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
// Purpose: Isolated demo logging functions.
// Authors: Ioan Chera
//

#ifndef G_DEMOTEST_H__
#define G_DEMOTEST_H__

#include "d_keywds.h"

void G_DemoLogInit(const char *path);
void G_DemoLog(E_FORMAT_STRING(const char *format), ...) E_PRINTF(1, 2);
void G_DemoLogStats();
bool G_DemoLogEnabled();
void G_DemoLogSetExited(bool value);

#endif

// EOF

