// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------

#ifndef HU_STUFF_H__
#define HU_STUFF_H__

struct event_t;

enum
{
   WIDGET_MISC,
   WIDGET_PATCH,
   WIDGET_TEXT,
};

extern bool chat_on;
extern int obituaries;
extern int obcolour;       // the colour of death messages
extern int showMessages;   // Show messages has default, 0 = off, 1 = on
extern int mess_colour;    // the colour of normal messages
extern char *chat_macros[10];

void HU_Init(void);
void HU_Drawer(void);
void HU_Ticker(void);
bool HU_Responder(const event_t *ev);

void HU_Start(void);

void HU_WriteText(const char *s, int x, int y);
void HU_PlayerMsg(const char *s);
void HU_CenterMessage(const char *s);
void HU_CenterMsgTimedColor(const char *s, const char *color, int tics);

#define CROSSHAIRS 3
extern int crosshairnum;       // 0= none
extern bool crosshair_hilite;

// haleyjd 02/12/06: lost and new options
extern bool hu_showtime;
extern bool hu_showcoords;
extern int  hu_timecolor;
extern int  hu_levelnamecolor;
extern int  hu_coordscolor;

extern char *hud_fontname;

#endif

// EOF
