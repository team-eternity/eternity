// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
// Shared intermission code
//
//-----------------------------------------------------------------------------

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

class IntermissionInterface : public InputInterface
{
private:
   wbstartstruct_t *stats;

public:
   IntermissionInterface();

   void init();
   void draw();
   void tick();

   void activate();
   bool isFullScreen();

   void setStats(wbstartstruct_t *new_stats);
};

extern IntermissionInterface Intermission;

void IN_AddCameras(void);
void IN_slamBackground(void);
void IN_checkForAccelerate(void);
void IN_DrawBackground(void);

#endif

// EOF
