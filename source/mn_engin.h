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
//
// Menu engine.
//
// All the main functions of the menu
//
// By Simon Howard
//
//----------------------------------------------------------------------------

#ifndef MN_ENGIN_H__
#define MN_ENGIN_H__

struct command_t;
struct event_t;
class  qstring;
struct variable_t;
struct vfont_t;

//
// menu_t
//

// menu item flags

#define MENUITEM_POSINIT  0x00000001
#define MENUITEM_BIGFONT  0x00000002
#define MENUITEM_CENTERED 0x00000004
#define MENUITEM_LALIGNED 0x00000008

// whether a menu item is a 'gap' item
// ie. one that cannot be selected

#define is_a_gap(it) \
   ((it)->type == it_gap  || (it)->type == it_title || (it)->type == it_info)

// haleyjd 05/01/10: item alignment defines
enum
{
   ALIGNMENT_RIGHT,
   ALIGNMENT_LEFT,
   ALIGNMENT_CENTER
};

// item types
enum
{
   it_gap,              // empty line
   it_runcmd,           // run console command
   it_variable,         // variable; enter pressed to type in new value
   it_variable_nd,      // variable, but doesn't set default value -- haleyjd
   it_toggle,           // togglable variable; can use left/right to change value
   it_title,            // the menu title
   it_info,             // information / section header
   it_slider,           // slider
   it_bigslider,        // big slider -- haleyjd 08/30/06
   it_automap,          // an automap colour
   it_binding,		// haleyjd: a key binding
   it_end,              // last menuitem in the list

   it_numtypes          // number of item types
};

struct menuitem_t
{
  int type; // item types
  
  const char *description;  // the describing name of this item

  // useful data for the item:
  // console command if console
  // variable name if variable, etc
  const char *data;         

  const char *patch; // patch to use or nullptr

  unsigned int flags;   // haleyjd 03/29/05: menu item flags

  /*** internal stuff used by menu code ***/
  int x, y;
  variable_t *var;        // ptr to console variable
  char *dyndescription;   // for EDF use only
  char *dyndata;
  char *dynpatch;
};

// haleyjd 10/07/05: Menu engine changes:
// 1. menuitems are no longer contained inside the menu_t structure,
//    since this was wasting tons of memory.
// 2. paged menu support -- menus may have more than one page of 
//    options again, just like they did in BOOM.
// 3. (10/15/05) table of contents support

// menu flags
enum
{
   mf_skullmenu     = 1,   // show skull rather than highlight
   mf_background    = 2,   // show background
   mf_leftaligned   = 4,   // left-aligned menu
   mf_centeraligned = 8,   // center-aligned menu - haleyjd 02/04/06
   mf_emulated      = 16,  // emulated old menu   - haleyjd 08/30/06
   mf_bigfont       = 32,  // draw all items big  - haleyjd 05/08/13

   // internal flags - don't expose these to EDF; if you add more 
   // user flags, put them above and bump the value of these up.
   mf_initialized   = 64   // already init'd      - haleyjd 05/08/13
};

struct menu_t
{
   // 10/07/05: pointer to item array
   menuitem_t *menuitems;

   // 10/07/05: pointers to additional page menus
   menu_t *prevpage;
   menu_t *nextpage;
   menu_t *rootpage;   // haleyjd 11/02/06: first page of a set

   // x,y offset of menu
   int x, y;
  
   // currently selected item
   int selected;
   
   // menu flags
   unsigned int flags;               
   
   void (*drawer)(void);              // separate drawer function 

   const char **content_names;    // table of contents stuff, optional
   menu_t     **content_pages;
   
   int gap_override;              // haleyjd 10/09/05: override gap size

   void (*open)(menu_t *menu);    // haleyjd 11/12/09: special open menu function

   // internal fields
   char name[33];                 // haleyjd 03/14/06: for dynamic menus
   menu_t *dynanext;

   menu_t *curpage;               // haleyjd 10/02/06: for multipage menus
   int widest_width;              // haleyjd 03/22/09: for LALIGNED flag
};

// menu 'widgets':
// A structured way for the menu to display things
// other than the usual menus
//
// if current_menuwidget is not nullptr, the drawer in
// the menuwidget pointed to by it is called by
// MN_Drawer. Also events caught by MN_Responder are
// sent to current_menuwidget->responder

struct menuwidget_t
{
  void (*drawer)();
  bool (*responder)(event_t *ev, int);
  void (*ticker)();   // haleyjd 05/29/06
  bool fullscreen;    // haleyjd: optimization for fullscreen widgets
  
  menuwidget_t *prev; // haleyjd 08/31/12: previous on stack, if any
};

// haleyjd 08/31/12: A proper widget stack
void   MN_PushWidget(menuwidget_t *widget);
void   MN_PopWidget();
size_t MN_NumActiveWidgets();

// responder for events

bool MN_Responder(event_t *ev);

// Called by main loop,
// only used for menu (skull cursor) animation.

void MN_Ticker();

// Called by main loop,
// draws the menus directly into the screen buffer.

void MN_DrawMenu(menu_t *menu);
void MN_Drawer();

bool MN_CheckFullScreen();

// Called by D_DoomMain,
// loads the config file.

void MN_Init();

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.

void MN_StartControlPanel();

void MN_ForcedLoadGame(char *msg); // killough 5/15/98: forced loadgames

void MN_DrawCredits();    // killough 11/98

void MN_ActivateMenu();
void MN_StartMenu(menu_t *menu);         // sf 10/99
void MN_PrevMenu();
void MN_ClearMenus();                    // sf 10/99

// font functions
void MN_WriteText(const char *s, int x, int y);
void MN_WriteTextColored(const char *s, int colour, int x, int y);
int  MN_StringWidth(const char *s);
int  MN_StringHeight(const char *s);

void MN_ErrorMsg(const char *s, ...);

enum boxwidget_e
{
   boxwidget_menupage,
   boxwidget_command
};
void MN_SetupBoxWidget(const char *title, const char **item_names,
                       boxwidget_e type, menu_t **pages, const char **cmds);
void MN_ShowBoxWidget();

void MN_DrawSmallPtr(int x, int y); // haleyjd 03/13/06
void MN_SetLeftSmallPtr(int x, int y, int height);
void MN_SetRightSmallPtr(int x, int y, int width, int height);

void MN_GetItemVariable(menuitem_t *item);

extern menu_t *current_menu;                  // current menu active
extern menu_t *drawing_menu;                  // current menu drawing
extern menuwidget_t *current_menuwidget;      // current widget being drawn

// size of automap colour blocks
#define BLOCK_SIZE 9

// haleyjd 08/30/06: emulated old menus have fixed item size of 16
#define EMULATED_ITEM_SIZE 16 

// menu error message
extern char menu_error_message[128];
extern int menu_error_time;

extern int hide_menu;
extern int menutime;

// haleyjd
extern int quickSaveSlot;
extern bool menu_toggleisback;
extern char *mn_fontname;
extern char *mn_bigfontname;
extern char *mn_normalfontname;
extern char *mn_background;
extern const char *mn_background_flat;

extern vfont_t *menu_font;
extern vfont_t *menu_font_big;
extern vfont_t *menu_font_normal;

extern command_t *input_command;
extern int input_cmdtype;
qstring &MN_GetInputBuffer();

#endif
                            
// EOF
