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

enum
{
    MSGALIGN_DEFAULT,
    MSGALIGN_LEFT,
    MSGALIGN_CENTRE,
};

//
// Widget Superclass Functionality
//
// haleyjd 06/04/05: Complete HUD rewrite.
//
class HUDWidget : public ZoneObject
{
protected:
    int        type;     // widget type
    char       name[33]; // name of this widget
    HUDWidget *next;     // next in hash chain
    bool       disabled; // disable flag

    enum
    {
        NUMWIDGETCHAINS = 17
    };

    static HUDWidget *hu_chains[NUMWIDGETCHAINS];

public:
    void setName(const char *widgetName) { strncpy(name, widgetName, sizeof(name)); }

    void setType(int widgetType) { type = widgetType; }

    // overridable functions (virtuals in a sense)
    virtual void ticker() {} // ticker: called each gametic
    virtual void drawer() {} // drawer: called when drawn
    virtual void eraser() {} // eraser: called when erased
    virtual void clear() {}  // clear : called on reinit

    static HUDWidget *WidgetForName(const char *name);
    static bool       AddWidgetToHash(HUDWidget *widget);
    static void       StartWidgets();
    static void       DrawWidgets();
    static void       TickWidgets();
};

extern bool  chat_on;
extern int   obituaries;
extern int   obcolour;     // the colour of death messages
extern int   showMessages; // Show messages has default, 0 = off, 1 = on
extern int   mess_align;   // the alignment of normal messages
extern int   mess_colour;  // the colour of normal messages
extern char *chat_macros[10];

void HU_Init(void);
void HU_Drawer(void);
void HU_Ticker(void);
bool HU_Responder(const event_t *ev);

void HU_Start(void);

void HU_PlayerMsg(const char *s);
void HU_CenterMessage(const char *s);
void HU_CenterMsgTimedColor(const char *s, const char *color, int tics);

#define CROSSHAIRS 3
extern int  crosshairnum; // 0= none
extern bool crosshair_hilite;
extern bool crosshair_scale;

// haleyjd 02/12/06: lost and new options
extern bool hu_showtime;
extern bool hu_showcoords;
extern bool hu_alwaysshowcoords;
extern int  hu_timecolor;
extern int  hu_levelnamecolor;
extern int  hu_coordscolor;

extern char *hud_fontname;

#endif

// EOF
