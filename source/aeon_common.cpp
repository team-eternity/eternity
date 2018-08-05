// The Eternity Engine
// Copyright(C) 2018 James Haley, Max Waine, et al.
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
// Purpose: Common Aeon functions and definitions
// Authors: James Haley, Max Waine
//

#include "aeon_common.h"
#include "aeon_fixed.h"
#include "c_io.h"
#include "m_fixed.h"
#include "m_qstr.h"

void ASPrint(int i)
{
   C_Printf("%d\n", i);
}

void ASPrint(unsigned int u)
{
   C_Printf("%u\n", u);
}

void ASPrint(float f)
{
   C_Printf("%f\n", f);
}

// EOF

