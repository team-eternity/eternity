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
// Purpose: Shared intermission code.
// Authors: James Haley, Ioan Chera
//

#ifndef IN_LUDE_H__
#define IN_LUDE_H__

#include "p_mobj.h" // HEADER-FIXME

struct wbstartstruct_t;

// Intermission object struct
struct interfns_t
{
   void (*Ticker)(void);         // called by IN_Ticker
   void (*DrawBackground)(void); // called various places
   void (*Drawer)(void);         // called by IN_Drawer
   void (*Start)(wbstartstruct_t *wbstartstruct); // called by IN_Start
};

// intercam
#define MAXCAMERAS 128

extern int intertime;
extern int acceleratestage;

class MobjCollection;
extern MobjCollection camerathings;
extern Mobj *wi_camera;

extern char *in_fontname;
extern char *in_bigfontname;
extern char *in_bignumfontname;

void IN_AddCameras(void);
void IN_slamBackground(void);
void IN_checkForAccelerate(void);
void IN_Ticker(void);
void IN_Drawer(void);
void IN_DrawBackground(void);
void IN_Start(wbstartstruct_t *wbstartstruct);

//
// Extended level info needed for intermission
//
struct intermapinfo_t
{
   const char *lumpname;   // this works as key too
   DLListItem<intermapinfo_t> link;

   const char *levelname;  // the level name in the automap
   const char *levelpic;   // the level pic in the intermission
   const char *enterpic;   // intermission background picture for entrance
   const char *exitpic;    // intermission exit picture
};

intermapinfo_t &IN_GetMapInfo(const char *lumpname);

#endif

// EOF
