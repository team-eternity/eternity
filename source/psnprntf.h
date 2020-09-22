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

#ifndef PSNPRINTF_H
#define PSNPRINTF_H

#include "d_keywds.h"

int psnprintf(char *str, size_t n, E_FORMAT_STRING(const char *format), ...) E_PRINTF(3, 4);
int pvsnprintf(char *str, size_t n, E_FORMAT_STRING(const char *format), va_list ap) E_PRINTF(3, 0);

#endif /* ifdef PSNPRINTF_H */


