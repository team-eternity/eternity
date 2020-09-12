//
// The Eternity Engine
// Copyright(C) 2020 James Haley, Max Waine, et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Strife-specific menu code
//
// Authors: Max Waine
//

#include "mn_engin.h"
#include "mn_menus.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"

//
// Strife-Only Menus
//
// Only the menus that need specific restructuring for Strife
// are here; everything that could be shared has been.
//

// Main Strife Menu

static void MN_SMainMenuDrawer();

static menuitem_t mn_smain_items[] =
{
   { it_runcmd, "New Game",  "mn_newgame",  "M_NGAME"  },
   { it_runcmd, "Options",   "mn_options",  "M_OPTION" },
   { it_runcmd, "Load Game", "mn_loadgame", "M_LOADG"  },
   { it_runcmd, "Save Game", "mn_savegame", "M_SAVEG"  },
   { it_runcmd, "Quit Game", "mn_quit",     "M_QUITG"  },
   { it_end }
};

menu_t menu_smain =
{
   mn_smain_items,            // menu items
   nullptr, nullptr, nullptr, // pages
   100, 56,                   // x, y offsets
   0,                         // start with 'new game' selected
   mf_skullmenu | mf_bigfont, // a skull menu
   MN_SMainMenuDrawer         // special drawer
};

static void MN_SMainMenuDrawer()
{
   V_DrawPatch(94, 2, &subscreen43,
               PatchLoader::CacheName(wGlobalDir, "M_STRIFE", PU_CACHE));
}

// STRIFE_TODO: newGameMenu

// EOF

