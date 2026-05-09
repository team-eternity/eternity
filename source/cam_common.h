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
//------------------------------------------------------------------------------
//
// Purpose: Common data for traversing.
// Authors: James Haley, Ioan Chera
//

#ifndef CAM_COMMON_H_
#define CAM_COMMON_H_

#include "m_collection.h"
#include "p_maputl.h"
#include "r_defs.h"

//
// Holds opening data just for the routines here
//
struct tracelineopening_t
{
    fixed_t           openrange;
    Surfaces<fixed_t> open;

    void calculate(const line_t *linedef);
    void calculateAtPoint(const line_t &line, v2fixed_t pos);
};

#endif

// EOF

