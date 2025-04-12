//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: System specific interface stuff.
// Authors: James Haley, Ioan Chera, Derek MacDonald
//

#ifndef D_MAIN_H__
#define D_MAIN_H__

#include "d_keywds.h"
#include "doomdef.h"

// jff make startskill globally visible
extern skill_t startskill;
extern char *startlevel;

void D_SetGameName(const char *iwad);

char *D_DoomExeDir(void);       // killough 2/16/98: path to executable's dir
extern char *basesavegame;      // killough 2/16/98: savegame path

//jff 1/24/98 make command line copies of play modes available
extern bool clnomonsters;  // checkparm of -nomonsters
extern bool clrespawnparm; // checkparm of -respawn
extern bool clfastparm;    // checkparm of -fast
//jff end of external declaration of command line playmode

extern bool nodrawers;
extern bool nosfxparm;
extern bool nomusicparm;

inline static bool D_noWindow()
{
   return nodrawers && nosfxparm && nomusicparm;
}

// Called by IO functions when input is detected.
struct event_t;
void D_PostEvent(const event_t* ev);

struct camera_t;
extern camera_t *camera;

//
// BASE LEVEL
//

void D_PageTicker();
void D_DrawPillars();
void D_DrawWings();
void D_AdvanceDemo();
void D_StartTitle();
void D_DoomMain();

// sf: display a message to the player: either in text mode or graphics
void usermsg(E_FORMAT_STRING(const char *s), ...) E_PRINTF(1, 2);
void startupmsg(const char *func, const char *desc);

#endif

//----------------------------------------------------------------------------
//
// $Log: d_main.h,v $
// Revision 1.7  1998/05/06  15:32:19  jim
// document g_game.c, change externals
//
// Revision 1.5  1998/05/03  22:27:08  killough
// Add external declarations
//
// Revision 1.4  1998/02/23  04:15:01  killough
// Remove obsolete function prototype
//
// Revision 1.3  1998/02/17  06:10:39  killough
// Add D_DoomExeDir prototype, basesavegame decl.
//
// Revision 1.2  1998/01/26  19:26:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
