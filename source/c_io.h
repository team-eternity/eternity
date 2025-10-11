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
// Purpose: Console I/O.
// Authors: James Haley, Ioan Chera, Max Waine
//

#ifndef C_IO_H__
#define C_IO_H__

#include "d_keywds.h"

static constexpr int INPUTLENGTH = 512;
static constexpr int LINELENGTH  = 128;

struct event_t;
class qstring;

void C_Init(void);
void C_InitBackdrop();
void C_Ticker(void);
void C_Drawer(void);
bool C_Responder(event_t *ev);
void C_Update(void);

void C_Printf(E_FORMAT_STRING(const char *s), ...) E_PRINTF(1, 2);
void C_Puts(const char *s);
void C_Separator(void);

void C_SetConsole(void);
void C_Popup(void);
void C_InstaPopup(void);

// haleyjd
void C_OpenConsoleLog(qstring *filename);
void C_CloseConsoleLog(void);
void C_DumpMessages(qstring *filename);

// sf 9/99: made a #define
#define consoleactive (Console.current_height || gamestate == GS_CONSOLE)
#define c_moving      (Console.current_height != Console.current_target)

extern int c_height; // the height of the console
extern int c_speed;  // pixels/tic it moves

extern char *c_fontname;

#endif

// EOF

