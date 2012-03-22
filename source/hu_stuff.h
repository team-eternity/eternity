// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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

#ifndef HU_STUFF_H__
#define HU_STUFF_H__

#include <map>
#include <string>

#include "doomtype.h"

struct event_t;
struct vfont_t;
struct patch_t;

enum
{
   WIDGET_MISC,
   WIDGET_PATCH,
   WIDGET_TEXT,
   WIDGET_CUSTOM,
};

// haleyjd 06/04/05: HUD rewrite

typedef struct hu_widget_s
{
   // overridable functions (virtuals in a sense)

   void (*ticker)(struct hu_widget_s *); // ticker: called each gametic
   void (*drawer)(struct hu_widget_s *); // drawer: called when drawn
   void (*eraser)(struct hu_widget_s *); // eraser: called when erased
   void (*clear) (struct hu_widget_s *); // clear : called on reinit

   // id data
   int type;                 // widget type
   char *name;               // name of this widget
   // struct hu_widget_s *next; // next in hash chain
   bool disabled;            // disable flag
   bool prevdisabled;        // previous state of disable flag
} hu_widget_t;

// erase data rect
typedef struct tw_erase_s
{
   int x1, y1, x2, y2;
} tw_erase_t;

// text widget flag values
enum
{
   TW_AUTOMAP_ONLY = 0x00000001, // appears in automap only
   TW_NOCLEAR      = 0x00000002, // dynamic widget with no clear func
   TW_BOXED        = 0x00000004, // 10/08/05: optional box around text
   TW_TRANS        = 0x00000008, // [CG] Alpha-blended background.
};

typedef struct hu_textwidget_s
{
   hu_widget_t widget;   // parent widget
   
   int x, y;             // coords on screen
   vfont_t *font;        // font object
   char *message;        // text to draw
   char *alloc;          // if non-NULL, widget owns the message
   int cleartic;         // gametic in which to clear the widget (0=never)
   tw_erase_t erasedata; // rect area to erase
   int flags;            // special flags
   int color;            // 02/12/06: needed to allow colored text drawing (col # + 1)
   int bg_opacity;       // [CG] Optional transparent background.
} hu_textwidget_t;

typedef struct hu_patchwidget_s
{
   hu_widget_t widget; // parent widget

   int x, y;           // screen location
   byte *color;        // color range translation to use
   int tl_level;       // translucency level
   char patchname[9];  // patch name -- haleyjd 06/15/06
   patch_t *patch;     // screen patch
} hu_patchwidget_t;

typedef struct hu_customwidget_s
{
   hu_widget_t widget;
   int x;
   int y;
   int height;
   int width;
   int bg_color;
   int bg_opacity;
} hu_customwidget_t;

// [CG] Added for c/s so the target's name (if any) can be stored in the
//      crosshair itself.
typedef struct hu_crosshairwidget_s
{
   hu_widget_t widget; // parent widget

   int x, y;           // screen location
   byte *color;        // color range translation to use
   int tl_level;       // translucency level
   char patchname[9];  // patch name -- haleyjd 06/15/06
   patch_t *patch;     // screen patch
   char *target_name;
} hu_crosshairwidget_t;

typedef std::map<std::string, hu_widget_t*> WidgetHash;

extern bool chat_on;
extern int obituaries;
extern int obcolour;       // the colour of death messages
extern int showMessages;   // Show messages has default, 0 = off, 1 = on
extern int mess_colour;    // the colour of normal messages
extern int center_mess_colour;
extern int center_mess_large;
extern int default_center_mess_colour;
extern int default_center_mess_large;
extern char *chat_macros[10];
extern WidgetHash widgets;

extern const char *shiftxform;

void HU_ClearWidgetHash(void);
bool HU_AddWidgetToHash(hu_widget_t *widget);
hu_widget_t* HU_WidgetForName(const char *name);
void HU_UpdateEraseData(hu_textwidget_t *tw);
void HU_DynamicCustomWidget(const char *name, int x, int y, int width,
                            int height, int bg_color, int bg_opacity);
void HU_DynamicTextWidget(const char *name, int x, int y, int font,
                          const char *message, int cleartic, int flags);
void HU_Init(void);
void HU_Drawer(void);
void HU_Ticker(void);
bool HU_Responder(event_t *ev);

void HU_Start(void);

void HU_WriteText(const char *s, int x, int y);
void HU_PlayerMsg(const char *s);
void HU_CenterMessage(const char *s);
void HU_CenterMsgTimedColor(const char *s, const char *color, int tics);

#define CROSSHAIRS 3
extern int crosshairnum;       // 0= none
extern bool crosshair_hilite;
extern bool adjust_crosshair_pitch;

// haleyjd 02/12/06: lost and new options
extern bool hu_showtime;
extern bool hu_showcoords;
extern int  hu_timecolor;
extern int  hu_levelnamecolor;
extern int  hu_coordscolor;

extern char *hud_fontname;

#endif

// EOF
