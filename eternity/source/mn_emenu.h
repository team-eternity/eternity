// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
// Dynamic Menus -- EDF Subsystem 
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef MN_EMENU_H__
#define MN_EMENU_H__

#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_MENU "menu"

extern cfg_opt_t edf_menu_opts[];

void MN_ProcessMenus(cfg_t *cfg);

#endif

menu_t *MN_DynamicMenuForName(const char *name);
void    MN_AddDynaMenuCommands(void);

#endif

// EOF
