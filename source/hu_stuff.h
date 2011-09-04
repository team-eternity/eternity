// Emacs style mode select -*- C -*-
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

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

#include "d_event.h"
#include "v_font.h"
#include "m_dllist.h"

enum
{
   WIDGET_MISC,
   WIDGET_PATCH,
   WIDGET_TEXT,
};

// haleyjd 06/04/05: HUD rewrite

typedef struct hu_widget_s
{
   // [CG] HUD widgets are now hashable
   mdllistitem_t link;
   // overridable functions (virtuals in a sense)

   void (*ticker)(struct hu_widget_s *); // ticker: called each gametic
   void (*drawer)(struct hu_widget_s *); // drawer: called when drawn
   void (*eraser)(struct hu_widget_s *); // eraser: called when erased
   void (*clear) (struct hu_widget_s *); // clear : called on reinit

   // id data
   int type;                 // widget type
   char *name;               // name of this widget
   // struct hu_widget_s *next; // next in hash chain
   boolean disabled;         // disable flag
   boolean prevdisabled;     // previous state of disable flag
} hu_widget_t;

// [CG] Moved from hu_stuff.c.

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

extern boolean chat_on;
extern int obituaries;
extern int obcolour;       // the colour of death messages
extern int showMessages;   // Show messages has default, 0 = off, 1 = on
extern int mess_colour;    // the colour of normal messages
extern char *chat_macros[10];

// [CG] Externalized shiftxform.
extern const char *shiftxform;

// [CG] Externalized a few functions.
void HU_ClearWidgetHash(void);
boolean HU_AddWidgetToHash(hu_widget_t *widget);
hu_widget_t* HU_WidgetForName(const char *name);
void HU_UpdateEraseData(hu_textwidget_t *tw);
void HU_DynamicTextWidget(const char *name, int x, int y, int font,
                          char *message, int cleartic, int flags);

void HU_Init(void);
void HU_Drawer(void);
void HU_Ticker(void);
boolean HU_Responder(event_t *ev);

void HU_Start(void);

void HU_WriteText(const char *s, int x, int y);
void HU_PlayerMsg(const char *s);
void HU_CenterMessage(const char *s);
void HU_CenterMsgTimedColor(const char *s, const char *color, int tics);
void HU_Erase(void);

#define CROSSHAIRS 3
extern int crosshairnum;       // 0= none
extern boolean crosshair_hilite;

// haleyjd 02/12/06: lost and new options
extern boolean hu_showtime;
extern boolean hu_showcoords;
extern int hu_timecolor;
extern int hu_levelnamecolor;
extern int hu_coordscolor;

extern const char *hud_fontname;

#endif

// EOF
