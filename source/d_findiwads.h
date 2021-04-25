//
// Copyright (C) 2019 James Haley et al.
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
// Purpose: Code for automated location of users' IWAD files and important
//  PWADs. Some code is derived from Chocolate Doom, by Simon Howard, used
//  under terms of the GPLv2.
//
// Authors: James Haley
//

#ifndef D_FINDIWADS_H__
#define D_FINDIWADS_H__

class qstring;

void D_CheckPathForIWADs(const qstring &path);
void D_FindIWADs();

#endif

// EOF

