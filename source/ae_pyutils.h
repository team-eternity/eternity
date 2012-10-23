// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 Kate Stone
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   Various miscellaneous functions for use with Python.
//
//-----------------------------------------------------------------------------

#ifndef __PY_UTILS_H__
#define __PY_UTILS_H__

#include "m_collection.h"
#include "m_qstr.h"

typedef Collection<qstring> QStringCollection;

QStringCollection* Split (qstring& name, const char splitchar);
QStringCollection* Partition (qstring& str, char middle);

#endif // __PY_UTILS_H__