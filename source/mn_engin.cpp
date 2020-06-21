// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2017 James Haley et al.
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
// New menu
//
// The menu engine. All the menus themselves are in mn_menus.c
//
// By Simon Howard
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"      // haleyjd: global game mode info
#include "d_io.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "g_bind.h"    // haleyjd: dynamic key bindings
#include "g_game.h"
#include "hal/i_timer.h"
#include "hu_over.h"
#include "i_video.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "mn_engin.h"
#include "mn_emenu.h"
#include "mn_htic.h"
#include "mn_items.h"
#include "mn_menus.h"
#include "mn_misc.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_patch.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

//=============================================================================
//
// Global Variables
//

bool inhelpscreens; // indicates we are in or just left a help screen

// menu error message
char menu_error_message[128];
int menu_error_time = 0;

// haleyjd 02/12/06: option to allow the toggle menu action to back up 
// through menus instead of exiting directly, to emulate ports like zdoom.
bool menu_toggleisback; 

// input for typing in new value
command_t *input_command = NULL;       // NULL if not typing in
int input_cmdtype = c_typed;           // haleyjd 07/15/09

vfont_t *menu_font;
vfont_t *menu_font_big;
vfont_t *menu_font_normal;

// haleyjd 08/25/09
char *mn_background;
const char *mn_background_flat;

//=============================================================================
//
// Static Declarations and Data
//

static qstring input_buffer;

static void MN_PageMenu(menu_t *newpage);

menu_t *drawing_menu; // menu currently being drawn

// haleyjd 02/04/06: small menu pointer
#define NUMSMALLPTRS 8
static int smallptrs[NUMSMALLPTRS];
static int16_t smallptr_dims[2]; // 0 = width, 1 = height
static int smallptr_idx;
static int smallptr_coords[2][2];

//=============================================================================
// 
// Menu Drawing - Utilities
//

#define SKULL_HEIGHT 19
#define BLINK_TIME 8

//
// MN_DrawSmallPtr
//
// An external routine for other modules that need to draw a small menu
// pointer for purposes other than its normal use.
//
void MN_DrawSmallPtr(int x, int y)
{
   V_DrawPatch(x, y, &subscreen43, 
               PatchLoader::CacheNum(wGlobalDir, smallptrs[smallptr_idx], PU_CACHE));
}

//
// MN_GetItemVariable
//
void MN_GetItemVariable(menuitem_t *item)
{
   // get variable if neccesary
   if(!item->var)
   {
      command_t *cmd;

      // use data for variable name
      if(!(cmd = C_GetCmdForName(item->data)))
      {
         C_Printf(FC_ERROR "variable not found: %s\n", item->data);
         item->type = it_info;   // turn into normal unselectable text
         item->var = NULL;
         return;
      }

      item->var = cmd->variable;
   }
}

//
// MN_FindFirstSelectable
//
// haleyjd 09/18/08: Finds the first selectable item in the menu,
// and returns the index of the item in the menuitem array.
//
static int MN_FindFirstSelectable(menu_t *menu)
{
   int i = 0, ret = 0;

   for(; menu->menuitems[i].type != it_end; ++i)
   {
      if(!is_a_gap(&menu->menuitems[i]))
      {
         ret = i;
         break;
      }
   }

   return ret;
}

//
// MN_FindLastSelectable
//
// haleyjd 09/18/08: Finds the last selectable item in the menu,
// and returns the index of the item in the menuitem array.
//
static int MN_FindLastSelectable(menu_t *menu)
{
   int i = 0, ret = 0;

   // first, find end
   for(; menu->menuitems[i].type != it_end; ++i)
      ; /* do nothing loop */
   
   // back up one, but not beyond the beginning
   if(i > 0)
      --i;

   // search backward
   for(; i >= 0; --i)
   {
      if(!is_a_gap(&menu->menuitems[i]))
      {
         ret = i;
         break;
      }
   }

   return ret;
}

//
// MN_CalcWidestWidth
//
// haleyjd 03/22/09: A fix for LALIGNED menus: the menu item values will
// all draw at a fixed position from the widest description string.
//
static void MN_CalcWidestWidth(menu_t *menu)
{
   int itemnum;

   if(menu->widest_width) // do only once
      return;

   for(itemnum = 0; menu->menuitems[itemnum].type != it_end; ++itemnum)
   {
      menuitem_t *item = &(menu->menuitems[itemnum]);

      // only LALIGNED items are taken into account
      if(item->flags & MENUITEM_LALIGNED)
      {
         int desc_width = 
            item->flags & MENUITEM_BIGFONT ?
               V_FontStringWidth(menu_font_big, item->description) 
               : MN_StringWidth(item->description);

         if(desc_width > menu->widest_width)
            menu->widest_width = desc_width;
      }
   }
}

// 
// MN_propagateBigFontFlag
//
// haleyjd 05/08/13: If a menu has mf_bigfont, set MENUITEM_BIGFONT on all its
// items.
//
static void MN_propagateBigFontFlag(menu_t *menu)
{
   int itemnum = 0;
   menuitem_t *item;

   for(; (item = &menu->menuitems[itemnum])->type != it_end; itemnum++)
      item->flags |= MENUITEM_BIGFONT;
}

//
// MN_initializeMenu
//
// haleyjd 05/08/13: Perform all first-time menu initialization tasks here.
//
static void MN_initializeMenu(menu_t *menu)
{
   if(menu->flags & mf_initialized)
      return;

   // propagate big font flag
   if(menu->flags & mf_bigfont)
      MN_propagateBigFontFlag(menu);

   // mark as initialized
   menu->flags |= mf_initialized;
}

//
// MN_DrawMenuItem
//
// draw a menu item. returns the height in pixels
//
static int MN_DrawMenuItem(menuitem_t *item, int x, int y, int color)
{
   int desc_width = 0;
   int alignment;
   int item_height = menu_font->cy; // haleyjd: most items are the size of one text line
   MenuItem *menuItemClass = MenuItemInstanceForType[item->type];

   // haleyjd: determine alignment
   if(drawing_menu->flags & mf_centeraligned)
      alignment = ALIGNMENT_CENTER;
   else if(drawing_menu->flags & (mf_skullmenu | mf_leftaligned))
      alignment = ALIGNMENT_LEFT;
   else
      alignment = ALIGNMENT_RIGHT;

   item->x = x; item->y = y;        // save x,y to item
   item->flags |= MENUITEM_POSINIT; // haleyjd 04/14/03

   // skip drawing if a gap
   if(item->type == it_gap)
   {
      // haleyjd 08/30/06: emulated menus have fixed item size
      if(drawing_menu->flags & mf_emulated)
         item_height = EMULATED_ITEM_SIZE;

      // haleyjd 10/09/05: gap size override for menus
      if(drawing_menu->gap_override)
         item_height = drawing_menu->gap_override;

      return item_height;
   }
 
   // draw an alternate patch?

   // haleyjd: gamemodes that use big menu font don't use pics, ever
   if(item->patch && !(GameModeInfo->flags & GIF_MNBIGFONT) &&
      menuItemClass->drawPatchForItem(item, item_height, alignment))
   {
      // haleyjd 05/16/04: hack for traditional menu support;
      // this was hard-coded in the old system
      if(drawing_menu->flags & mf_emulated)
         item_height = EMULATED_ITEM_SIZE; 

      return item_height; // if returned true, we are done.
   }

   // draw description text
   menuItemClass->drawDescription(item, item_height, desc_width, alignment, color);

   // draw other data: variable data etc.
   menuItemClass->drawData(item, color, alignment, desc_width);

   if(drawing_menu->flags & mf_emulated)
      item_height = EMULATED_ITEM_SIZE;

   return item_height;
}

//=============================================================================
//
// Menu Drawing
//

//
// MN_SetLeftSmallPtr
//
// Set the location of the menu small left pointer
//
void MN_SetLeftSmallPtr(int x, int y, int height)
{
   smallptr_coords[0][0] = x - (smallptr_dims[0] + 1);
   smallptr_coords[0][1] = y + ((height - smallptr_dims[1]) / 2);
}

//
// MN_SetRightSmallPtr
//
// Set the location of the menu small right pointer
//
void MN_SetRightSmallPtr(int x, int y, int width, int height)
{
   smallptr_coords[1][0] = x + width + 2;
   smallptr_coords[1][1] = y + ((height - smallptr_dims[1]) / 2);
}

//
// MN_drawPointer
//
// Utility for MN_DrawMenu - draws the appropriate pointer for the currently
// selected menu item.
//
static void MN_drawPointer(menu_t *menu, int y, int itemnum, int item_height)
{
   if(menu->flags & mf_skullmenu)
   {      
      int item_x = menu->x - 30;                         // 30 left
      int item_y = y + (item_height - SKULL_HEIGHT) / 2; // midpoint

      // haleyjd 05/16/04: hack for traditional menu support
      if(menu->flags & mf_emulated)
      {
         item_x = menu->x - 32;
         item_y = menu->y - 5 + itemnum * 16; // fixed-height items
      }

      gimenucursor_t *cursor = GameModeInfo->menuCursor;
      const char *patch = cursor->patches[(menutime / cursor->blinktime) % cursor->numpatches];

      V_DrawPatch(item_x, item_y, &subscreen43, PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE));
   }
   else
   {
      // haleyjd 02/04/06: draw small pointers

      // draw left pointer
      V_DrawPatch(smallptr_coords[0][0], smallptr_coords[0][1], &subscreen43,
         PatchLoader::CacheNum(wGlobalDir, smallptrs[smallptr_idx], PU_CACHE));

      // draw right pointer
      V_DrawPatch(smallptr_coords[1][0], smallptr_coords[1][1], &subscreen43, 
         PatchLoader::CacheNum(wGlobalDir, 
                               smallptrs[(NUMSMALLPTRS - smallptr_idx) % NUMSMALLPTRS],
                               PU_CACHE));
   }
}

//
// MN_drawPageIndicator
//
// Draws a prev or next page indicator near the bottom of the screen for
// multipage menus.
// Pass false to draw a prev indicator, or true to draw a next indicator.
//
static void MN_drawPageIndicator(bool next)
{
   char msgbuffer[64];
   const char *actionname, *speckeyname, *replkeyname, *fmtstr, *key;
   
   if(next) // drawing a next indicator
   {
      actionname  = "menu_pagedown"; // name of action
      speckeyname = "pgdn";          // special default key to check for
      replkeyname = "page down";     // pretty name to replace with
      fmtstr      = "%s ->";         // format string for indicator
   }
   else     // drawing a prev indicator
   {
      actionname  = "menu_pageup";
      speckeyname = "pgup";
      replkeyname = "page up";
      fmtstr      = "<- %s";
   }

   // get name of first key bound to the prev or next menu action
   key = G_FirstBoundKey(actionname);

   // replace pgup / pgdn with prettier names, since they are the defaults
   if(!strcasecmp(key, speckeyname))
      key = replkeyname;

   psnprintf(msgbuffer, sizeof(msgbuffer), fmtstr, key);

   MN_WriteTextColored(msgbuffer, CR_GOLD, 
                       next ? 310 - MN_StringWidth(msgbuffer) : 10, 
                       SCREENHEIGHT - (2 * menu_font->absh + 1));
}

//
// MN_drawStatusText
//
// Draws the help and error messages centered at the bottom of options menus.
//
static void MN_drawStatusText(const char *text, int color)
{
   int x, y;

   // haleyjd: fix y coordinate to use appropriate text metrics
   x = 160 - MN_StringWidth(text) / 2;
   y = SCREENHEIGHT - menu_font->absh;

   MN_WriteTextColored(text, color, x, y);
}

//
// MN_DrawMenu
//
// MAIN FUNCTION
//
// Draw a menu.
//
void MN_DrawMenu(menu_t *menu)
{
   int y;
   int itemnum;

   if(!menu) // haleyjd 04/20/03
      return;

   drawing_menu = menu; // needed by DrawMenuItem
   y = menu->y; 
      
   // draw background

   if(menu->flags & mf_background)
   {
      // haleyjd 04/04/10: draw menus higher in some game modes
      y -= GameModeInfo->menuOffset;
      V_DrawBackground(mn_background_flat, &vbscreen);
   }

   // haleyjd: calculate widest width for LALIGNED flag
   MN_CalcWidestWidth(menu);

   // menu-specific drawer function, if any

   if(menu->drawer)
      menu->drawer();

   // draw items in menu

   for(itemnum = 0; menu->menuitems[itemnum].type != it_end; ++itemnum)
   {
      menuitem_t *mi = &menu->menuitems[itemnum];
      int item_height;
      int item_color;

      // choose item colour based on selected item
      if(mi->type == it_info)
         item_color = GameModeInfo->infoColor;
      else
      {
         if(menu->selected == itemnum && !(menu->flags & mf_skullmenu))
            item_color = GameModeInfo->selectColor;
         else
            item_color = GameModeInfo->unselectColor;
      }
      
      // draw item
      item_height = MN_DrawMenuItem(mi, menu->x, y, item_color);
      
      // if selected item, draw skull / pointer next to it
      if(menu->selected == itemnum)
         MN_drawPointer(menu, y, itemnum, item_height);
      
      // go down by item height
      y += item_height;
   }

   if(menu->flags & mf_skullmenu)
      return; // no help msg in skull menu

   // choose help message to print
   
   if(menu_error_time) // error message takes priority
   {
      // make it flash
      if((menu_error_time / 8) % 2)
         MN_drawStatusText(menu_error_message, CR_TAN);
   }
   else
   {
      char msgbuffer[64];
      const char *helpmsg;
      menuitem_t *menuitem;

      // write some help about the item
      menuitem = &menu->menuitems[menu->selected];
      
      MenuItem *menuItemClass = MenuItemInstanceForType[menuitem->type];
      helpmsg = menuItemClass->getHelpString(menuitem, msgbuffer);

      MN_drawStatusText(helpmsg, CR_GOLD);
   }

   // haleyjd 10/07/05: draw next/prev messages for menus that
   // have multiple pages.
   if(menu->prevpage)
      MN_drawPageIndicator(false);

   if(menu->nextpage)
      MN_drawPageIndicator(true);
}

//
// MN_CheckFullScreen
//
// Called by D_Drawer to see if the menu is in full-screen mode --
// this allows the game to skip all other drawing, keeping the
// framerate at 35 fps.
//
bool MN_CheckFullScreen(void)
{
   if(!menuactive || !current_menu)
      return false;

   if(!(current_menu->flags & mf_background) || hide_menu ||
      (current_menuwidget && !current_menuwidget->fullscreen))
      return false;

   return true;
}

//=============================================================================
//
// Menu Module Functions
//
// Drawer, ticker etc.
//

#define MENU_HISTORY 128

bool menuactive = false;             // menu active?
menu_t *current_menu;   // the current menu_t being displayed
static menu_t *menu_history[MENU_HISTORY];   // previously selected menus
static int menu_history_num;                 // location in history
int hide_menu = 0;      // hide the menu for a duration of time
int menutime = 0;

// menu widget for alternate drawer + responder
menuwidget_t *current_menuwidget  = nullptr;
static PODCollection<menuwidget_t *> menuwidget_stack;
static bool widget_consume_text = false; // consume text after widget is closed

int quickSaveSlot;  // haleyjd 02/23/02: restored from MBF

static void MN_InitFonts(void);

//
// MN_PushWidget
//
// Push a new widget onto the widget stack
//
void MN_PushWidget(menuwidget_t *widget)
{
   menuwidget_stack.add(widget);
   if(current_menuwidget)
      widget->prev = current_menuwidget;
   current_menuwidget = widget;
}

//
// MN_PopWidget
//
// Back up one widget on the stack
//
void MN_PopWidget()
{
   size_t len;

   // Pop the top widget off.
   if(menuwidget_stack.getLength() > 0)
      menuwidget_stack.pop();

   if(current_menuwidget)
      current_menuwidget->prev = nullptr;

   // If there's still an active widget, return to it.
   // Otherwise, cancel out.
   if((len = menuwidget_stack.getLength()) > 0)
      current_menuwidget = menuwidget_stack[len - 1];
   else
      current_menuwidget = nullptr;
   widget_consume_text = true;
}

//
// MN_ClearWidgetStack
//
// Called when the menu system is closing. Let's make sure all widgets have
// been popped before then, since they won't be there when we come back to
// the menus later.
//
static void MN_ClearWidgetStack()
{
   menuwidget_stack.clear();
   current_menuwidget = NULL;
}

//
// MN_NumActiveWidgets
//
// Return the number of widgets currently on the stack.
//
size_t MN_NumActiveWidgets()
{
   return menuwidget_stack.getLength();
}

//
// MN_SetBackground
//
// Called to change the menu background.
//
static void MN_SetBackground()
{
   // TODO: allow BEX string replacement?
   if(mn_background && mn_background[0] && strcmp(mn_background, "default") &&
      R_CheckForFlat(mn_background) != -1)
   {
      mn_background_flat = mn_background;
   }
   else
      mn_background_flat = GameModeInfo->menuBackground;
}

//
// MN_Init
//
// Primary menu initialization. Called at startup.
//
void MN_Init()
{
   int i;

   // haleyjd 02/04/06: load small pointer
   for(i = 0; i < NUMSMALLPTRS; i++)
   {
      char name[9];

      psnprintf(name, 9, "EEMNPTR%d", i);

      smallptrs[i] = W_GetNumForName(name);
   }

   // get width and height from first patch
   {
      patch_t *ptr0 = PatchLoader::CacheNum(wGlobalDir, smallptrs[0], PU_CACHE);
      smallptr_dims[0] = ptr0->width;
      smallptr_dims[1] = ptr0->height;
   }
      
   quickSaveSlot = -1; // haleyjd: -1 == no slot selected yet

   // haleyjd: init heretic stuff if appropriate
   if(GameModeInfo->type == Game_Heretic)
      MN_HInitSkull(); // initialize spinning skulls
   
   MN_InitMenus();     // create menu commands in mn_menus.c
   MN_InitFonts();     // create menu fonts
   MN_SetBackground(); // set background

   // haleyjd 07/03/09: sync up mn_classic_menus
   MN_LinkClassicMenus(mn_classic_menus);
}

//////////////////////////////////
// ticker

void MN_Ticker()
{
   if(menu_error_time)
      menu_error_time--;
   if(hide_menu)                   // count down hide_menu
      hide_menu--;
   menutime++;

   // haleyjd 02/04/06: determine small pointer index
   if((menutime & 3) == 0)
      smallptr_idx = (smallptr_idx + 1) % NUMSMALLPTRS;

   // haleyjd 05/29/06: tick for any widgets
   if(current_menuwidget && current_menuwidget->ticker)
      current_menuwidget->ticker();
}

////////////////////////////////
// drawer

void MN_Drawer()
{ 
   // activate menu if displaying widget
   if(current_menuwidget && !menuactive)
      MN_StartControlPanel();
      
   if(current_menuwidget)
   {
      // alternate drawer
      if(current_menuwidget->drawer)
         current_menuwidget->drawer();
      return;
   }

   // haleyjd: moved down to here
   if(!menuactive || hide_menu) 
      return;
 
   MN_DrawMenu(current_menu);
}

static void MN_ShowContents(void);

extern const char *shiftxform;

//
// MN_GetInputBuffer
//
// haleyjd 09/01/14: Get a reference to the menu engine's input buffer.
//
qstring &MN_GetInputBuffer()
{
   return input_buffer;
}

//
// MN_Responder
//
// haleyjd 07/03/04: rewritten to use enhanced key binding system
//
bool MN_Responder(event_t *ev)
{
   // haleyjd 04/29/02: these need to be unsigned
   unsigned char tempstr[128];
   int *menuSounds = GameModeInfo->menuSounds; // haleyjd
   static bool ctrldown = false;
   static bool shiftdown = false;
   static bool altdown = false;
   static unsigned lastacceptedtime = 0;

   memset(tempstr, 0, sizeof(tempstr));

   // haleyjd 07/03/04: call G_KeyResponder with kac_menu to filter
   // for menu-class actions
   int action = G_KeyResponder(ev, kac_menu);

   // haleyjd 10/07/05
   if(ev->data1 == KEYD_RCTRL)
      ctrldown = (ev->type == ev_keydown);

   // haleyjd 03/11/06
   if(ev->data1 == KEYD_RSHIFT)
      shiftdown = (ev->type == ev_keydown);

   // haleyjd 07/05/10
   if(ev->data1 == KEYD_RALT)
      altdown = (ev->type == ev_keydown);

   // menu doesn't want keyup events
   if(ev->type == ev_keyup)
      return false;

   if(widget_consume_text && ev->type == ev_text)
   {
      widget_consume_text = false;
      return true;
   }

   if(ev->repeat &&
      !(input_command && ev->type == ev_keydown && ev->data1 == KEYD_BACKSPACE))
   {
      const unsigned int currtime = i_haltimer.GetTicks();
      // only accept repeated input every 120 ms
      if(currtime < lastacceptedtime + 120)
         return false;
      lastacceptedtime = currtime;
   }

   // are we displaying a widget?
   if(current_menuwidget)
   {
      current_menuwidget->responder(ev, action);
      return true;
   }

   // are we inputting a new value into a variable?
   if(ev->type == ev_keydown && input_command)
   {
      if(action == ka_menu_toggle) // cancel input
         input_command = NULL;      
      else if(action == ka_menu_confirm)
      {
         if(input_buffer.length() || (input_command->flags & cf_allowblank))
         {
            if(input_buffer.length())
            {
               // place quotation marks around the new value
               input_buffer.makeQuoted();
            }
            else
               input_buffer = "*"; // flag to clear the variable

            // set the command
            Console.cmdtype = input_cmdtype;
            C_RunCommand(input_command, input_buffer.constPtr());
            input_command = nullptr;
            input_cmdtype = c_typed;
            return true; // eat key
         }
      }
      else if(ev->data1 == KEYD_BACKSPACE) // check for backspace
      {
         input_buffer.Delc();
         return true; // eat key
      }

      return true;
   }
   else if(ev->type == ev_text && input_command)
   {
      // just a normal character
      variable_t *var = input_command->variable;
      const unsigned char ich = static_cast<unsigned char>(ev->data1);

      // dont allow too many characters on one command line
      size_t maxlen = 20u;
      switch(var->type)
      {
      case vt_string:
         maxlen = static_cast<size_t>(var->max);
         break;
      case vt_int:
      case vt_toggle:
         maxlen = 33u;
         break;
      default:
         break;
      }
      if(ectype::isPrint(ich) && input_buffer.length() < maxlen)
         input_buffer += static_cast<char>(ich);

      return true; // eat key
   }

   if(devparm && ev->data1 == key_help)
   {
      G_ScreenShot();
      return true;
   }
  
   if(action == ka_menu_toggle)
   {
      // start up main menu or kill menu
      if(menuactive)
      {
         if(menu_toggleisback) // haleyjd 02/12/06: allow zdoom-style navigation
            MN_PrevMenu();
         else
         {
            MN_ClearMenus();
            S_StartInterfaceSound(menuSounds[MN_SND_DEACTIVATE]);
         }
      }
      else 
         MN_StartControlPanel();      
      
      return true;
   }

   if(action == ka_menu_help)
   {
      C_RunTextCmd("help");
      return true;
   }

   if(action == ka_menu_setup)
   {
      C_RunTextCmd("mn_options");
      return true;
   }
   
   // not interested in other keys if not in menu
   if(!menuactive)
      return false;

   if(action == ka_menu_up)
   {
      bool cancelsnd = false;

      // skip gaps
      do
      {
         if(--current_menu->selected < 0)
         {
            int i;
            
            if(!ctrldown) // 3/13/09: control cancels these behaviors
            {
               // haleyjd: in paged menus, go to the previous page
               if(current_menu->prevpage)
               {
                  // set selected item to the first selectable item
                  current_menu->selected = MN_FindFirstSelectable(current_menu);
                  cancelsnd = true; // paging makes a sound already, so remember this
                  MN_PageMenu(current_menu->prevpage); // modifies current_menu!
               }
               else if(current_menu->nextpage)
               {
                  // 03/13/09: don't do normal wrap behavior on first page
                  current_menu->selected = MN_FindFirstSelectable(current_menu);
                  cancelsnd = true; // cancel the sound cuz we didn't do anything
                  break; // exit the loop
               }               
            }
            
            // jump to end of menu
            for(i = 0; current_menu->menuitems[i].type != it_end; i++)
               ; /* do-nothing loop */
            current_menu->selected = i - 1;
         }
      }
      while(is_a_gap(&current_menu->menuitems[current_menu->selected]));
      
      if(!cancelsnd)
         S_StartInterfaceSound(menuSounds[MN_SND_KEYUPDOWN]); // make sound
      
      return true;  // eatkey
   }
  
   if(action == ka_menu_down)
   {
      bool cancelsnd = false;
      
      do
      {
         ++current_menu->selected;
         if(current_menu->menuitems[current_menu->selected].type == it_end)
         {
            if(!ctrldown) // 03/13/09: control cancels these behaviors
            {
               // haleyjd: in paged menus, go to the next page               
               if(current_menu->nextpage)
               {
                  // set selected item to the last selectable item
                  current_menu->selected = MN_FindLastSelectable(current_menu);
                  cancelsnd = true;             // don't make a double sound below
                  MN_PageMenu(current_menu->nextpage); // modifies current_menu!
               }
               else if(current_menu->prevpage)
               {
                  // don't wrap the pointer when on the last page
                  current_menu->selected = MN_FindLastSelectable(current_menu);
                  cancelsnd = true; // cancel the sound cuz we didn't do anything
                  break; // exit the loop
               }
            }
            
            current_menu->selected = 0;     // jump back to start
         }
      }
      while(is_a_gap(&current_menu->menuitems[current_menu->selected]));
      
      if(!cancelsnd)
         S_StartInterfaceSound(menuSounds[MN_SND_KEYUPDOWN]); // make sound

      return true;  // eatkey
   }
   
   if(action == ka_menu_confirm)
   {
      menuitem_t *menuItem      = &current_menu->menuitems[current_menu->selected];
      MenuItem   *menuItemClass = MenuItemInstanceForType[menuItem->type];
     
      menuItemClass->onConfirm(menuItem);
      return true;
   }
  
   if(action == ka_menu_previous)
   {
      MN_PrevMenu();
      return true;
   }
   
   // decrease value of variable
   if(action == ka_menu_left)
   {
      // haleyjd 10/07/05: if ctrl is down, go to previous menu
      if(ctrldown)
      {
         if(current_menu->prevpage)
            MN_PageMenu(current_menu->prevpage);
         else
            S_StartInterfaceSound(GameModeInfo->c_BellSound);
         
         return true;
      }
      
      menuitem_t *menuItem      = &current_menu->menuitems[current_menu->selected];
      MenuItem   *menuItemClass = MenuItemInstanceForType[menuItem->type];
      
      menuItemClass->onLeft(menuItem, altdown, shiftdown);
      return true;
   }
  
   // increase value of variable
   if(action == ka_menu_right)
   {
      // haleyjd 10/07/05: if ctrl is down, go to next menu
      if(ctrldown)
      {
         if(current_menu->nextpage)
            MN_PageMenu(current_menu->nextpage);
         else
            S_StartInterfaceSound(GameModeInfo->c_BellSound);
         
         return true;
      }
      
      menuitem_t *menuItem      = &current_menu->menuitems[current_menu->selected];
      MenuItem   *menuItemClass = MenuItemInstanceForType[menuItem->type];
      
      menuItemClass->onRight(menuItem, altdown, shiftdown);      
      return true;
   }

   // haleyjd 10/07/05: page up action -- return to previous page
   if(action == ka_menu_pageup)
   {
      if(current_menu->prevpage)
         MN_PageMenu(current_menu->prevpage);
      else
         S_StartInterfaceSound(GameModeInfo->c_BellSound);
      
      return true;
   }

   // haleyjd 10/07/05: page down action -- go to next page
   if(action == ka_menu_pagedown)
   {
      if(current_menu->nextpage)
         MN_PageMenu(current_menu->nextpage);
      else
         S_StartInterfaceSound(GameModeInfo->c_BellSound);

      return true;
   }

   // haleyjd 10/15/05: table of contents widget!
   if(action == ka_menu_contents)
   {
      if(current_menu->content_names && current_menu->content_pages)
         MN_ShowContents();
      else
         S_StartInterfaceSound(GameModeInfo->c_BellSound);

      return true;
   }

   // search for matching item in menu

   if(ev->type == ev_text && ectype::isLower(ev->data1))
   {  
      // sf: experimented with various algorithms for this
      //     this one seems to work as it should

      int n = current_menu->selected;
      
      do
      {
         n++;
         if(current_menu->menuitems[n].type == it_end) 
            n = 0; // loop round

         // ignore unselectables
         if(!is_a_gap(&current_menu->menuitems[n])) 
         {
            if(ectype::toLower(current_menu->menuitems[n].description[0]) == ev->data1)
            {
               // found a matching item!
               current_menu->selected = n;
               return true; // eat key
            }
         }
      } while(n != current_menu->selected);
   }
   
   return true; 
}

///////////////////////////////////////////////////////////////////////////
//
// Other Menu Functions
//

//
// MN_ActivateMenu
//
// make menu 'clunk' sound on opening
//
void MN_ActivateMenu()
{
   if(!menuactive)  // activate menu if not already
   {
      menuactive = true;
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_ACTIVATE]);
   }
}

//
// MN_StartMenu
//
// Start a menu
//
void MN_StartMenu(menu_t *menu)
{
   if(!menuactive)
   {
      MN_ActivateMenu();
      current_menu = menu;      
      menu_history_num = 0;  // reset history
   }
   else
   {
      menu_history[menu_history_num++] = current_menu;
      current_menu = menu;
   }

   // haleyjd 10/02/06: if the menu being activated has a curpage set, 
   // redirect to that page now; this allows us to return to whatever
   // page the user was last viewing.
   if(current_menu->curpage)
      current_menu = current_menu->curpage;

   // haleyjd 05/09/13: perform initialization tasks on the menu
   MN_initializeMenu(current_menu);

   menu_error_time = 0;      // clear error message

   // haleyjd 11/12/09: custom menu open actions
   if(current_menu->open)
      current_menu->open(current_menu);
}

//
// MN_PageMenu
//
// haleyjd 10/07/05: sets the menu engine to a new menu page.
//
static void MN_PageMenu(menu_t *newpage)
{
   if(!menuactive)
      return;

   current_menu = newpage;

   // haleyjd 10/02/06: if this page has a rootpage, set the rootpage's
   // curpage value to this page, in order to remember what page we're on.
   if(current_menu->rootpage)
      current_menu->rootpage->curpage = current_menu;

   menu_error_time = 0;

   S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
}

//
// MN_PrevMenu
//
// go back to a previous menu
//
void MN_PrevMenu()
{
   if(--menu_history_num < 0)
      MN_ClearMenus();
   else
      current_menu = menu_history[menu_history_num];
   
   menu_error_time = 0;          // clear errors

   S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_PREVIOUS]);
}

//
// MN_ClearMenus
//
// turn off menus
//
void MN_ClearMenus()
{
   Console.enabled = true; // haleyjd 03/11/06: re-enable console
   menuactive = false;
   MN_ClearWidgetStack();  // haleyjd 08/31/12: make sure widget stack is empty
}

CONSOLE_COMMAND(mn_clearmenus, 0)
{
   MN_ClearMenus();
}

CONSOLE_COMMAND(mn_prevmenu, 0)
{
   MN_PrevMenu();
}

CONSOLE_COMMAND(forceload, cf_hidden)
{
   G_ForcedLoadGame();
   MN_ClearMenus();
}

void MN_ForcedLoadGame(char *msg)
{
   MN_Question(msg, "forceload");
}

//
// MN_ErrorMsg
//
// display error msg in popup display at bottom of screen
//
void MN_ErrorMsg(const char *s, ...)
{
   va_list args;
   
   va_start(args, s);
   pvsnprintf(menu_error_message, sizeof(menu_error_message), s, args);
   va_end(args);
   
   menu_error_time = 140;
}

//
// MN_StartControlPanel
//
// activate main menu
//
void MN_StartControlPanel(void)
{
   // haleyjd 08/31/06: not if menu is already active
   if(menuactive)
      return;

   // haleyjd 05/16/04: traditional DOOM main menu support
   // haleyjd 08/31/06: support for all of DOOM's original menus
   MN_StartMenu(GameModeInfo->mainMenu);
}

//=============================================================================
//
// Menu Font Drawing
//
// copy of V_* functions
// these do not leave a 1 pixel-gap between chars, I think it looks
// better for the menu
//
// haleyjd: duplicate code eliminated via use of vfont engine.
//

// haleyjd 02/25/09: font names set by EDF:
char *mn_fontname;
char *mn_normalfontname;
char *mn_bigfontname;

//
// MN_InitFonts
//
// Called at startup from MN_Init. Sets global references to the menu fonts
// defined via EDF.
//
static void MN_InitFonts(void)
{
   if(!(menu_font = E_FontForName(mn_fontname)))
      I_Error("MN_InitFonts: bad EDF font name %s\n", mn_fontname);

   if(!(menu_font_big = E_FontForName(mn_bigfontname)))
      I_Error("MN_InitFonts: bad EDF font name %s\n", mn_bigfontname);

   if(!(menu_font_normal = E_FontForName(mn_normalfontname)))
      I_Error("MN_InitFonts: bad EDF font name %s\n", mn_normalfontname);
}

//
// MN_WriteText
//
// haleyjd: This is now just a thunk for convenience's sake.
//
void MN_WriteText(const char *s, int x, int y)
{
   V_FontWriteText(menu_font, s, x, y, &subscreen43);
}

//
// MN_WriteTextColored
//
// haleyjd: Ditto.
//   05/02/10: finally renamed from MN_WriteTextColoured. Sorry fraggle ;)
//
void MN_WriteTextColored(const char *s, int colour, int x, int y)
{
   V_FontWriteTextColored(menu_font, s, colour, x, y, &subscreen43);
}

//
// MN_StringWidth
//
// haleyjd: And this too.
//
int MN_StringWidth(const char *s)
{
   return V_FontStringWidth(menu_font, s);
}

//
// MN_StringHeight
//
int MN_StringHeight(const char *s)
{
   return V_FontStringHeight(menu_font, s);
}

//=============================================================================
// 
// Box Widget
//
// haleyjd 10/15/05: Generalized code for box widgets.
//

//
// box_widget_t
//
// "Inherits" from menuwidget_t and extends it to contain a list of commands
// or menu pages.
//
typedef struct box_widget_s
{
   menuwidget_t widget;      // the actual menu widget object

   const char *title;        // title string

   const char **item_names;  // item strings for box

   boxwidget_e type;          // type: either contents or cmds

   menu_t     **pages;       // menu pages array for contents box
   const char **cmds;        // commands for command box

   int        width;         // precalculated width
   int        height;        // precalculated height
   int        selection_idx; // currently selected item index
   int        maxidx;        // precalculated maximum index value
} box_widget_t;

//
// MN_BoxSetDimensions
//
// Calculates the box width, height, and maxidx by considering all
// strings contained in the box.
//
static void MN_BoxSetDimensions(box_widget_t *box)
{
   int width, i = 0;
   const char *curname = box->item_names[0];

   box->width  = -1;
   box->height =  0;

   while(curname)
   {
      width = MN_StringWidth(curname);

      if(width > box->width)
         box->width = width;

      // add string height + 1 for every string in box
      box->height += V_FontStringHeight(menu_font_normal, curname) + 1;

      curname = box->item_names[++i];
   }

   // set maxidx
   box->maxidx = i - 1;

   // account for title
   width = MN_StringWidth(box->title);
   if(width > box->width)
      box->width = width;

   // leave a gap after title
   box->height += V_FontStringHeight(menu_font_normal, box->title) + 8;

   // add 9 to width and 8 to height to account for box border
   box->width  += 9;
   box->height += 8;

   // 04/22/06: add space for a small menu ptr
   box->width += smallptr_dims[0] + 2;
}

//
// MN_BoxWidgetDrawer
//
// Draw a menu box widget.
//
static void MN_BoxWidgetDrawer()
{
   int x, y, i = 0;
   const char *curname;
   box_widget_t *box;

   // get a pointer to the box_widget
   box = (box_widget_t *)current_menuwidget;

   // draw the menu in the background
   MN_DrawMenu(current_menu);
   
   x = (SCREENWIDTH  - box->width ) >> 1;
   y = (SCREENHEIGHT - box->height) >> 1;

   // draw box background
   V_DrawBox(x, y, box->width, box->height);

   curname = box->item_names[0];

   // step out from borders (room was left in width, height calculations)
   x += 4 + smallptr_dims[0] + 2;
   y += 4;

   // write title
   MN_WriteTextColored(box->title, CR_GOLD, x, y);

   // leave a gap
   y += V_FontStringHeight(menu_font_normal, box->title) + 8;

   // write text in box
   while(curname)
   {
      int color = GameModeInfo->unselectColor;
      int height = V_FontStringHeight(menu_font_normal, curname);
      
      if(box->selection_idx == i)
      {
         color = GameModeInfo->selectColor;

         // draw small pointer
         MN_DrawSmallPtr(x - (smallptr_dims[0] + 1), 
                         y + ((height - smallptr_dims[1]) >> 1));
      }

      MN_WriteTextColored(curname, color, x, y);

      y += height + 1;
      
      curname = box->item_names[++i];
   }
}

//
// MN_BoxWidgetResponder
//
// Handle events to a menu box widget.
//
static bool MN_BoxWidgetResponder(event_t *ev, int action)
{
   // get a pointer to the box widget
   box_widget_t *box = (box_widget_t *)current_menuwidget;

   // toggle and previous dismiss the widget
   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      MN_PopWidget();
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]); // cha!
      return true;
   }

   // up/left: move selection to previous item with wrap
   if(action == ka_menu_up || action == ka_menu_left)
   {
      if(--box->selection_idx < 0)
         box->selection_idx = box->maxidx;
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
      return true;
   }

   // down/right: move selection to next item with wrap
   if(action == ka_menu_down || action == ka_menu_right)
   {
      if(++box->selection_idx > box->maxidx)
         box->selection_idx = 0;
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
      return true;
   }

   // confirm: clear widget and set menu page or run command
   if(action == ka_menu_confirm)
   {
      MN_PopWidget();

      switch(box->type)
      {
      case boxwidget_menupage:
         if(box->pages)
            MN_PageMenu(box->pages[box->selection_idx]);
         break;
      case boxwidget_command:
         if(box->cmds)
            C_RunTextCmd(box->cmds[box->selection_idx]);
         break;
      default:
         break;
      }
      
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_COMMAND]); // pow!
      return true;
   }

   return false;
}

//
// Global box widget object. There can only be one box widget active at a
// time, unless the type was to be made global and initialized via a different
// routine. There's not any need for that currently.
//
static box_widget_t menu_box_widget =
{
   // widget:
   {
      MN_BoxWidgetDrawer,
      MN_BoxWidgetResponder,
      NULL,
      true // is fullscreen, draws current menu in background
   },

   // all other fields are set dynamically
};

//
// MN_SetupBoxWidget
//
// Sets up the menu_box_widget object above to show specific information.
//
void MN_SetupBoxWidget(const char *title, const char **item_names,
                       boxwidget_e type, menu_t **pages, const char **cmds)
{
   menu_box_widget.title = title;
   menu_box_widget.item_names = item_names;
   menu_box_widget.type = type;
   
   switch(type)
   {
   case boxwidget_menupage: // menu page widget
      menu_box_widget.pages = pages;
      menu_box_widget.cmds  = NULL;
      break;
   case boxwidget_command: // command widget
      menu_box_widget.pages = NULL;
      menu_box_widget.cmds  = cmds;
      break;
   default:
      break;
   }

   menu_box_widget.selection_idx = 0;

   MN_BoxSetDimensions(&menu_box_widget);
}

//
// MN_ShowBoxWidget
//
// For external access.
//
void MN_ShowBoxWidget(void)
{
   MN_PushWidget(&(menu_box_widget.widget));
}

//
// MN_ShowContents
//
// Sets up the menu box widget to display the contents box for the
// current menu. Code in MN_Responder already verifies that the menu
// defines table of contents information before calling this function.
// This allows ease of access to all pages in a multipage menu.
//
static void MN_ShowContents(void)
{
   int i = 0;
   menu_t *rover;

   if(!menuactive)
      return;

   MN_SetupBoxWidget("contents", current_menu->content_names, boxwidget_menupage,
                     current_menu->content_pages, NULL);

   // haleyjd 05/02/10: try to find the current menu in the list of pages
   // (it should be there unless something really screwy is going on).

   rover = current_menu->content_pages[0];

   while(rover)
   {
      if(rover == current_menu)
         break; // found it

      // try next one
      rover = current_menu->content_pages[++i];
   }

   if(rover) // only if valid (should always be...)
      menu_box_widget.selection_idx = i;

   MN_PushWidget(&(menu_box_widget.widget));

   S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
}


//=============================================================================
//
// Console Commands
//

VARIABLE_TOGGLE(menu_toggleisback, NULL, yesno);
CONSOLE_VARIABLE(mn_toggleisback, menu_toggleisback, 0) {}

VARIABLE_STRING(mn_background, NULL, 8);
CONSOLE_VARIABLE(mn_background, mn_background, 0)
{
   MN_SetBackground();
}

// EOF
