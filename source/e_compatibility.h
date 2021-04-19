//
// Copyright (C) 2020 James Haley, Max Waine, Ioan Chera et al.
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
//----------------------------------------------------------------------------
//
// Purpose: EDF compatibility WAD support
// Authors: Ioan Chera
//

#ifndef E_COMPATIBILITY_H_
#define E_COMPATIBILITY_H_

constexpr const char EDF_SEC_COMPATIBILITY[] = "compatibilityhacks";

struct cfg_opt_t;
struct cfg_t;

//
// List of settings overridable by EDF for compatibility.
// CAUTION when adding fields! Check if any arrays use NUM_overridableSetting.
//
enum overridableSetting_e
{
   overridableSetting_stsTraditionalKeys,
   NUM_overridableSetting
};

extern cfg_opt_t edf_compatibility_opts[];

void E_ProcessCompatibilities(cfg_t *cfg);
void E_RestoreCompatibilities();
void E_ApplyCompatibility(const char *digest);

int E_Get(overridableSetting_e setting);

#endif

// EOF
