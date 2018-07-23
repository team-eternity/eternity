//
// The Eternity Engine
// Copyright (C) 2018 James Haley et al.
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
// Purpose: Generic EDF metatable builder. Has features such as inheritance.
// Authors: Ioan Chera
//

#ifndef E_EDFMETATABLE_H_
#define E_EDFMETATABLE_H_

struct cfg_t;
class MetaTable;

extern cfg_opt_t edf_generic_tprops[];

void E_BuildGlobalMetaTableFromEDF(cfg_t *cfg, const char *secname,
                                   const char *deltaname, MetaTable &table);

#endif

// EOF
