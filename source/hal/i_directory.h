//
// Copyright(C) 2013 David Hill et al.
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
// DESCRIPTION:
//    Directory Manipulation
//
//-----------------------------------------------------------------------------

#ifndef I_DIRECTORY_H__
#define I_DIRECTORY_H__

#include "i_platform.h"
#include "../m_qstr.h"

class qstring;


bool I_CreateDirectory(qstring const &path);

const char *I_PlatformInstallDirectory();

void I_GetRealPath(const char *path, qstring &real);

#endif

// EOF

