// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
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
// Heads-Up Display
//
// haleyjd: Rewritten a second time. Displays widgets using a system
// of pseudoclass structures, complete with pseudo-inheritance and
// pseudo-polymorphism. This may as well be C++, except it's actually
// more flexible. This module is more open to scripting now, as a result.
//
// By James Haley and Simon Howard
//
//----------------------------------------------------------------------------

#include <stdio.h>

#include "doomdef.h"
#include "doomstat.h"
#include "c_net.h"
#include "c_io.h"
#include "d_deh.h"
#include "d_event.h"
#include "e_hash.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "hu_over.h"
#include "p_info.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_draw.h"
#include "s_sound.h"
#include "v_video.h"
#include "v_font.h"
#include "w_wad.h"
#include "am_map.h"
#include "d_gi.h"
#include "d_io.h"
#include "m_qstr.h"
#include "a_small.h"
#include "e_fonts.h"
#include "g_dmflag.h" // [CG] Added.

// [CG] Added.
#include "cs_hud.h"
#include "cs_main.h"
#include "cs_score.h"
#include "cl_main.h"
#include "sv_main.h"

char *chat_macros[10];
const char* shiftxform;
const char english_shiftxform[];
//boolean chat_on;
boolean chat_active = false;
int obituaries = 0;
int obcolour = CR_BRICK;       // the colour of death messages

vfont_t *hud_font;
vfont_t *hud_overfont; // [CG] Added for target names.
const char *hud_fontname = NULL;

static boolean HU_ChatRespond(event_t *ev);

// haleyjd 06/04/05: Complete HUD rewrite.

// Widget Hashing

#define NUMWIDGETCHAINS 17

// static hu_widget_t *hu_chains[NUMWIDGETCHAINS];
static ehash_t *widget_hash = NULL;
E_KEYFUNC(hu_widget_t, name);

//
// Widget Superclass Functionality
//

//
// HU_WidgetForName
//
// Retrieves a widget given its name, using hashing.
// Returns NULL if no such widget exists.
//
// [CG] Un-static'd.
// static hu_widget_t *HU_WidgetForName(const char *name)
hu_widget_t *HU_WidgetForName(const char *name)
{
   return E_HashObjectForKey(widget_hash, &name);
   /*
   int key = D_HashTableKey(name) % NUMWIDGETCHAINS;
   hu_widget_t *cur = hu_chains[key];

   while(cur && strncasecmp(name, cur->name, 33))
      cur = cur->next;

   return cur;
   */
}

//
// HU_AddWidgetToHash
//
// Adds a widget to the hash table, but only if one of the given
// name doesn't exist. Returns true if successful.
//
boolean HU_AddWidgetToHash(hu_widget_t *widget)
{
   // int key;

   // make sure one doesn't already exist by this name
   if(HU_WidgetForName(widget->name))
   {
#ifdef RANGECHECK
      // for debug, cause an error to alert programmer of internal mishaps
      I_Error("HU_AddWidgetToHash: duplicate mnemonic %s\n", widget->name);
#endif
      return false;
   }

   /*
   key = D_HashTableKey(widget->name) % NUMWIDGETCHAINS;
   widget->next = hu_chains[key];
   hu_chains[key] = widget;
   */
   E_HashAddObject(widget_hash, widget);

   return true;
}

//
// HU_ToggleWidget
//
// Sets the disable flag on a widget and properly propagates the state
// to the previous disable state member, so that widgets can still
// erase properly.
//
d_inline static void HU_ToggleWidget(hu_widget_t *widget, boolean disable)
{
   widget->prevdisabled = widget->disabled;
   widget->disabled = disable;
}

//
// HU_NeedsErase
//
// Returns true or false to indicate whether or not a widget needs
// erasing. Sets prevdisabled to disabled to end erasing after the
// first frame this is called for disabled widgets.
//
d_inline static boolean HU_NeedsErase(hu_widget_t *widget)
{
   // needs erase if enabled, or if WAS enabled on last frame
   boolean ret = !widget->disabled || !widget->prevdisabled;

   widget->prevdisabled = widget->disabled;

   return ret;
}

//
// Main HUD System Functions; Called externally.
//

static void HU_InitMsgWidget(void);
static void HU_InitCrossHair(void);
static void HU_InitWarnings(void);
static void HU_InitCenterMessage(void);
static void HU_InitLevelTime(void);
static void HU_InitLevelName(void);
static void HU_InitChat(void);
static void HU_InitCoords(void);

//
// HU_InitNativeWidgets
//
// Sets up all the native widgets. Called from HU_Init below at startup.
//
static void HU_InitNativeWidgets(void)
{   
   HU_InitMsgWidget();
   HU_InitCrossHair();
   HU_InitWarnings();
   HU_InitCenterMessage();
   HU_InitLevelTime();
   HU_InitLevelName();
   HU_InitChat();
   HU_InitCoords();

   // HUD_FIXME: generalize?
   HU_FragsInit();

   // [CG] Added.
   CS_InitNetWidget();
   CS_InitTimerWidget();
   CS_InitChatWidget();
   CS_InitQueueWidget();
   CS_InitTeamWidget();
}

void HU_ClearWidgetHash(void)
{
   hu_widget_t *w = NULL;

   while((w = (hu_widget_t *)E_HashTableIterator(widget_hash, w)) != NULL)
      E_HashRemoveObject(widget_hash, w);
}

//
// HU_Init
//
// Called once at game startup to initialize the HUD system.
//
void HU_Init(void)
{
   shiftxform = english_shiftxform;

   if(!(hud_font = E_FontForName(hud_fontname)))
      I_Error("HU_Init: bad EDF font name %s\n", hud_fontname);

   HU_LoadFont(); // overlay font

   // [CG] Use ehash_t instead of old hashing mechanism.
   if(widget_hash == NULL)
   {
      widget_hash = calloc(1, sizeof(ehash_t));
      E_StrHashInit(
         widget_hash, NUMWIDGETCHAINS, E_KEYFUNCNAME(hu_widget_t, name), NULL
      );
   }
   else
      HU_ClearWidgetHash();

   HU_InitNativeWidgets();
}

//
// HU_Start
//
// Called to reinitialize the HUD system due to start of a new map
// or change of player viewpoint.
//
void HU_Start(void)
{
   // int i;
   hu_widget_t *w = NULL;

   /*
   // call all widget clear functions
   for(i = 0; i < NUMWIDGETCHAINS; ++i)
   {
      widget = hu_chains[i];

      while(widget)
      {
         if(widget->clear)
            widget->clear(widget);
         widget = widget->next;
      }
   }
   */

   while((w = (hu_widget_t *)E_HashTableIterator(widget_hash, w)) != NULL)
   {
      if(w->clear)
         w->clear(w);
   }

#ifndef EE_NO_SMALL_SUPPORT
   // execute script event handlers
   if(gameScriptLoaded)
      SM_OptScriptCallback(&GameScript, "OnHUDStart");

   if(levelScriptLoaded)
      SM_OptScriptCallback(&LevelScript, "OnHUDStart");
#endif
}

//
// HU_Drawer
//
// Called from D_Display to draw all HUD elements.
//
void HU_Drawer(void)
{
   // int i;
   hu_widget_t *w = NULL;

#ifndef EE_NO_SMALL_SUPPORT
   // execute script event handlers
   if(gameScriptLoaded)
      SM_OptScriptCallback(&GameScript, "OnHUDPreDraw");

   if(levelScriptLoaded)
      SM_OptScriptCallback(&LevelScript, "OnHUDPreDraw");
#endif

   // call all widget drawer functions
   /*
   for(i = 0; i < NUMWIDGETCHAINS; ++i)
   {
      widget = hu_chains[i];

      while(widget && !widget->disabled) // haleyjd 06/15/06: oops!
      {
         if(widget->drawer)
            widget->drawer(widget);
         widget = widget->next;
      }
   }
   */

   while((w = (hu_widget_t *)E_HashTableIterator(widget_hash, w)) != NULL)
   {
      if(w->drawer && !w->disabled)
         w->drawer(w);
   }

   // HUD_FIXME: generalize?
   // draw different modules
   // [CG] C/S has its own scoreboard.
   if(clientserver)
      CS_ShowScores();
   else
      HU_FragsDrawer();
   HU_OverlayDraw();
}

//
// HU_Ticker
//
// Called from G_Ticker when gamestate is GS_LEVEL. Updates widgets
// for the current gametic.
//
void HU_Ticker(void)
{
   // int i;
   hu_widget_t *w = NULL;

   // call all widget ticker functions
   /*
   for(i = 0; i < NUMWIDGETCHAINS; ++i)
   {
      widget = hu_chains[i];

      while(widget)
      {
         if(widget->ticker)
            widget->ticker(widget);
         widget = widget->next;
      }
   }
   */

   while((w = (hu_widget_t *)E_HashTableIterator(widget_hash, w)) != NULL)
   {
      if(w->ticker)
         w->ticker(w);
   }
}

// I don't know what this is doing here, but it can stay for now.
boolean altdown = false;

//
// HU_Responder
//
// Called from G_Responder. Has priority over any other events
// intercepted by that function.
//
boolean HU_Responder(event_t *ev)
{
   if(ev->data1 == KEYD_LALT)
      altdown = (ev->type == ev_keydown);
   
   // only the chat widget can respond to events (for now at least)
   return HU_ChatRespond(ev);
}

//
// HU_Erase
// 
// Called from D_Display to erase widgets displayed on the previous
// frame. This clears up areas of the screen not redrawn each tick.
//
void HU_Erase(void)
{
   // int i;
   hu_widget_t *w = NULL;

   if(!viewwindowx || automapactive)
      return;

   // call all widget erase functions
   /*
   for(i = 0; i < NUMWIDGETCHAINS; ++i)
   {
      widget = hu_chains[i];

      while(widget)
      {
         if(widget->eraser && HU_NeedsErase(widget))
            widget->eraser(widget);
         widget = widget->next;
      }
   }
   */

   while((w = (hu_widget_t *)E_HashTableIterator(widget_hash, w)) != NULL)
   {
      if(w->eraser && HU_NeedsErase(w))
         w->eraser(w);
   }

   // HUD_FIXME: generalize?
   // run indiv. module erasers
   HU_FragsErase();
}

//
// Normal Player Message Widget
//

#define MAXHUDMESSAGES 16
#define MAXHUDMSGLEN   256

typedef struct hu_msgwidget_s
{
   hu_widget_t widget; // parent widget

   char messages[MAXHUDMESSAGES][MAXHUDMSGLEN]; // messages
   int  current_messages;              // number of messages
   int  scrolltime;                    // leveltime when message will scroll up
} hu_msgwidget_t;

static hu_msgwidget_t msg_widget;

// globals related to the msg_widget

int hud_msg_lines;   // number of message lines in window up to 16
int hud_msg_scrollup;// whether message list scrolls up
int message_timer;   // timer used for normal messages
int showMessages;    // Show messages has default, 0 = off, 1 = on
int mess_colour = CR_RED;      // the colour of normal messages

//
// HU_MessageTick
//
// Updates the message widget on each gametic.
//
static void HU_MessageTick(hu_widget_t *widget)
{
   int i;
   hu_msgwidget_t *mw = (hu_msgwidget_t *)widget;
   
   // check state of message scrolling
   if(!hud_msg_scrollup)
      return;

   // move up messages
   if(leveltime >= mw->scrolltime)
   {
      for(i = 0; i < mw->current_messages - 1; ++i)
         strncpy(mw->messages[i], mw->messages[i + 1], MAXHUDMSGLEN);

      if(mw->current_messages)
         mw->current_messages--;
      mw->scrolltime = leveltime + (message_timer * 35) / 1000;
   }
}

//
// HU_MessageDraw
//
// Drawer for the player message widget.
//
static void HU_MessageDraw(hu_widget_t *widget)
{
   int i, y;
   hu_msgwidget_t *mw = (hu_msgwidget_t *)widget;
   
   if(!showMessages)
      return;
   
   // go down a bit if chat active
   y = chat_active ? 8 : 0;

   // [CG] Go down even more if chat is active AND a queue message is
   //      displayed.
   if(QUEUE_MESSAGE_DISPLAYED)
      y += 8;
   
   for(i = 0; i < mw->current_messages; i++, y += 8)
   {
      int x = 0;
      char *msg = mw->messages[i];

      // haleyjd 12/26/02: center messages in Heretic
      // FIXME/TODO: make this an option in DOOM?
      if(GameModeInfo->type == Game_Heretic)
         x = (SCREENWIDTH - V_FontStringWidth(hud_font, msg)) >> 1;
      
      // haleyjd 06/04/05: use V_FontWriteTextColored like it should.
      // Color codes within strings will still override the default.
      V_FontWriteTextColored(hud_font, msg, mess_colour, x, y);
   }
}

//
// HU_MessageErase
//
// Erases the player message widget.
//
static void HU_MessageErase(hu_widget_t *widget)
{
   // haleyjd 06/04/05: added one to account for chat
   R_VideoErase(0, 0, SCREENWIDTH, 8 * (hud_msg_lines + 1));
}

//
// HU_MessageClear
//
// Sets the number of messages being displayed to zero.
//
static void HU_MessageClear(hu_widget_t *widget)
{
   ((hu_msgwidget_t *)widget)->current_messages = 0;
}

//
// HU_InitMsgWidget
//
// Called from HU_InitNativeWidgets. Sets up the player message widget.
//
static void HU_InitMsgWidget(void)
{
   // set up the object id
   if(!msg_widget.widget.name)
   {
      msg_widget.widget.name = strdup("_HU_MsgWidget");
   }

   msg_widget.widget.type = WIDGET_MISC;

   // set up object virtuals
   msg_widget.widget.ticker = HU_MessageTick;
   msg_widget.widget.drawer = HU_MessageDraw;
   msg_widget.widget.eraser = HU_MessageErase;
   msg_widget.widget.clear  = HU_MessageClear;

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&msg_widget);
}

//
// Global functions that manipulate the message widget
//

//
// HU_PlayerMsg
//
// Adds a new message to the player.
//
void HU_PlayerMsg(const char *s)
{
   if(msg_widget.current_messages == hud_msg_lines)  // display full
   {
      int i;
      
      // scroll up      
      for(i = 0; i < hud_msg_lines - 1; ++i)
         strncpy(msg_widget.messages[i], msg_widget.messages[i+1], MAXHUDMSGLEN);
      
      strncpy(msg_widget.messages[hud_msg_lines - 1], s, MAXHUDMSGLEN);
   }
   else            // add one to the end
   {
      strncpy(msg_widget.messages[msg_widget.current_messages], s, MAXHUDMSGLEN);
      msg_widget.current_messages++;
   }
   
   msg_widget.scrolltime = leveltime + (message_timer * 35) / 1000;
}

//
// Patch Widget: this type of widget just displays a screen patch.
//

typedef struct hu_patchwidget_s
{
   hu_widget_t widget; // parent widget

   int x, y;           // screen location
   byte *color;        // color range translation to use
   int tl_level;       // translucency level
   char patchname[9];  // patch name -- haleyjd 06/15/06
   patch_t *patch;     // screen patch
} hu_patchwidget_t;

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

//
// HU_PatchWidgetDraw
//
// Default drawing function for patch widgets. All the logic for
// deciding whether the patch is translucent and/or translated is
// already implemented in V_DrawPatchTL via the generalized patch
// drawing system.
//
static void HU_PatchWidgetDraw(hu_widget_t *widget)
{
   hu_patchwidget_t *pw = (hu_patchwidget_t *)widget;

   if(!pw->patch)
      return;

   // be sure the patch is loaded
   pw->patch = W_CacheLumpName(pw->patchname, PU_CACHE);

   V_DrawPatchTL(pw->x, pw->y, &vbscreen, pw->patch, pw->color, pw->tl_level);
}

//
// HU_PatchWidgetErase
//
// Default erase function for patch widgets.
//
static void HU_PatchWidgetErase(hu_widget_t *widget)
{
   int x, x2, y, y2, w , h;
   hu_patchwidget_t *pw = (hu_patchwidget_t *)widget;
   patch_t *patch = pw->patch;

   if(patch && pw->tl_level != 0)
   {
      // haleyjd 06/08/05: must adjust for patch offsets
      x = pw->x - SwapShort(patch->leftoffset);
      y = pw->y - SwapShort(patch->topoffset);
      w = SwapShort(patch->width);
      h = SwapShort(patch->height);
      
      x2 = x + w - 1;
      y2 = y + h - 1;

      // haleyjd 12/28/05: must clip to screen
      if(x < 0)
         x = 0;
      if(x2 >= SCREENWIDTH)
         x2 = SCREENWIDTH - 1;

      w = x2 - x + 1;

      // no width to erase?
      if(w <= 0)
         return;

      if(y < 0)
         y = 0;
      if(y2 >= SCREENHEIGHT)
         y2 = SCREENHEIGHT - 1;

      h = y2 - y + 1;

      // no height to erase?
      if(h <= 0)
         return;
      
      R_VideoErase((unsigned int)x, (unsigned int)y, 
                   (unsigned int)w, (unsigned int)h);
   }
}

//
// HU_PatchWidgetDefaults
//
// Sets up default handler functions for a patch widget.
//
static void HU_PatchWidgetDefaults(hu_patchwidget_t *pw)
{
   pw->widget.drawer = HU_PatchWidgetDraw;
   pw->widget.eraser = HU_PatchWidgetErase;
   pw->widget.type   = WIDGET_PATCH;
}

//
// HU_DynamicPatchWidget
//
// Adds a dynamically allocated patch widget to the hash table.
// For scripting.
//
static void HU_DynamicPatchWidget(char *name, int x, int y, int color,
                                  int tl_level, char *patch)
{
   hu_patchwidget_t *newpw = calloc(1, sizeof(hu_patchwidget_t));

   // set id
   if(!newpw->widget.name)
   {
      newpw->widget.name = strdup(name);
   }

   // add to hash
   if(!HU_AddWidgetToHash((hu_widget_t *)newpw))
   {
      // if addition was unsuccessful, we delete it now
      free(newpw);
      return;
   }

   // set virtuals
   HU_PatchWidgetDefaults(newpw);

   // set properties
   newpw->x = x;
   newpw->y = y;
   if(color >= 0 && color < CR_LIMIT)
      newpw->color = colrngs[color];
   newpw->tl_level = tl_level;

   // 06/15/06: copy patch name
   strncpy(newpw->patchname, patch, 9);

   // pre-cache patch -- haleyjd 06/15/06: use PU_CACHE
   newpw->patch = W_CacheLumpName(patch, PU_CACHE);
}

//
// Pop-up Warning Boxes
//
// Several different things that appear, quake-style, to warn you of
// problems.
//

// Open Socket Warning
//
// Problem with network leads or something like that

static hu_patchwidget_t opensocket_widget;

//
// HU_WarningsDrawer
//
// Draws the warnings widgets. Calls down to the default drawer for
// patches when the proper conditions are met. Normally I'd set the
// conditions via a ticker but for opensocket that won't work if the
// game is frozen.
//
static void HU_WarningsDrawer(hu_widget_t *widget)
{
   hu_patchwidget_t *pw = (hu_patchwidget_t *)widget;

   if(pw == &opensocket_widget)
   {
      if(opensocket)
         HU_PatchWidgetDraw(widget);
   }
}

//
// HU_InitWarnings
//
// Sets up the VPO and open socket warning patch widgets.
//
static void HU_InitWarnings(void)
{   
   // set up socket
   if(!opensocket_widget.widget.name)
   {
      opensocket_widget.widget.name = strdup("_HU_OpenSocketWidget");
   }

   opensocket_widget.widget.type = WIDGET_PATCH;

   opensocket_widget.widget.drawer = HU_WarningsDrawer;
   opensocket_widget.widget.eraser = HU_PatchWidgetErase;

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&opensocket_widget);
   
   strncpy(opensocket_widget.patchname, "OPENSOCK", 9);
   opensocket_widget.patch = W_CacheLumpName("OPENSOCK", PU_CACHE);
   opensocket_widget.color = NULL;
   opensocket_widget.tl_level = FRACUNIT;
   opensocket_widget.x = 20;
   opensocket_widget.y = 20;
}

//
// Text Widgets
//

//
// HU_TextWidgetDraw
//
// Default drawing function for a text widget.
//
static void HU_TextWidgetDraw(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   // Do not ever draw automap-only widgets if not in automap mode.
   // This fixes a long-standing bug automatically.
   if(tw->flags & TW_AUTOMAP_ONLY && !automapactive)
      return;

   // 10/08/05: boxed message support
   // [CG] Alpha-blended background support.
   if(tw->flags & TW_BOXED)
   {
      int width, height;

      width  = V_FontStringWidth (tw->font, tw->message);
      height = V_FontStringHeight(tw->font, tw->message);

      V_DrawBox(tw->x - 4, tw->y - 4, width + 8, height + 8);
   }
   else if(tw->flags & TW_TRANS)
   {
      int x = tw->x - 4;
      int y = tw->y - 4;
      int w = V_FontStringWidth (tw->font, tw->message) + 8;
      int h = V_FontStringHeight(tw->font, tw->message) + 8;
      int bg_color = GameModeInfo->blackIndex;
      int bg_opacity = tw->bg_opacity;

      V_ColorBlockTLScaled(&vbscreen, bg_color, x, y, w, h, bg_opacity);
   }

   if(tw->message && (!tw->cleartic || leveltime < tw->cleartic))
   {
      if(tw->color)
         V_FontWriteTextColored(tw->font, tw->message, tw->color - 1, tw->x, tw->y);
      else
         V_FontWriteText(tw->font, tw->message, tw->x, tw->y);
   }
}

//
// HU_ClearEraseData
//
// haleyjd 04/10/05: Sets the erase area data for a text widget to its 
// initial state.  This is called after the erase area has been wiped, 
// so that the area does not continue to grow in size indefinitely and 
// take up the entire screen.
//
d_inline static void HU_ClearEraseData(hu_textwidget_t *tw)
{
   tw->erasedata.x1 = tw->erasedata.y1 =  D_MAXINT;
   tw->erasedata.x2 = tw->erasedata.y2 = -D_MAXINT;
}

//
// HU_UpdateEraseData
//
// haleyjd 04/10/05: Updates a text widget's erase data structure
// by expanding the rect to include the boundaries of a new message. The
// erase area must expand until an erasure actually occurs.
//
// [CG] Un-static'd
// static void HU_UpdateEraseData(hu_textwidget_t *tw)
void HU_UpdateEraseData(hu_textwidget_t *tw)
{
   int x, y, w, h;

   if(!tw->message)
      return;

   x = tw->x;
   y = tw->y;
   w = V_FontStringWidth (tw->font, tw->message);
   h = V_FontStringHeight(tw->font, tw->message);

   // haleyjd 10/08/05: boxed text support
   if(tw->flags & TW_BOXED)
   {
      x -= 4;
      y -= 4;
      w += 8;
      h += 8;
   }

   // haleyjd 02/12/06: use proper variables here (yikes!)
   if(x < tw->erasedata.x1)
      tw->erasedata.x1 = x;
   if(y < tw->erasedata.y1)
      tw->erasedata.y1 = y;
   if(x + w - 1 > tw->erasedata.x2)
      tw->erasedata.x2 = x + w - 1;
   if(y + h - 1 > tw->erasedata.y2)
      tw->erasedata.y2 = y + h - 1;

   // haleyjd 12/29/05: must bound to screen edges
   if(tw->erasedata.x1 < 0)
      tw->erasedata.x1 = 0;
   if(tw->erasedata.x2 >= SCREENWIDTH)
      tw->erasedata.x2 = SCREENWIDTH - 1;
   if(tw->erasedata.y1 < 0)
      tw->erasedata.y1 = 0;
   if(tw->erasedata.y2 >= SCREENHEIGHT)
      tw->erasedata.y2 = SCREENHEIGHT - 1;
}

//
// HU_TextWidgetErase
//
// haleyjd 04/10/05: Default function for erasing HUD text widgets. 
//
static void HU_TextWidgetErase(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;
   int w, h;

   // is erasedata "empty" ?
   if(tw->erasedata.x1 == D_MAXINT)
      return;

   // is area of zero width or height?
   w = tw->erasedata.x2 - tw->erasedata.x1 + 1;
   h = tw->erasedata.y2 - tw->erasedata.y1 + 1;

   if(w <= 0 || h <= 0)
      return;

   // erase the largest rect the message widget has occupied since the
   // last time it was cleared
   R_VideoErase(tw->erasedata.x1, tw->erasedata.y1, w, h);

   // do not keep erasing larger and larger portions of the screen when 
   // it is totally unnecessary.
   if(!tw->message || (tw->cleartic != 0 && leveltime > tw->cleartic + 1))
      HU_ClearEraseData(tw);
}

//
// HU_TextWidgetClear
//
// Default clear function for a text widget.
//
static void HU_TextWidgetClear(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   // if this widget owns its message, free it
   if(tw->alloc)
   {
      free(tw->alloc);
      tw->alloc = NULL;
   }
   
   tw->message  = NULL;
   tw->cleartic = 0;    // haleyjd 08/23/07: woops!
}

//
// HU_TextWidgetDefaults
//
// Sets up default values for a normal text widget.
//
static void HU_TextWidgetDefaults(hu_textwidget_t *tw)
{
   HU_ClearEraseData(tw);

   tw->widget.drawer = HU_TextWidgetDraw;
   tw->widget.eraser = HU_TextWidgetErase;
   tw->widget.clear  = HU_TextWidgetClear;
   tw->widget.type   = WIDGET_TEXT;

   // all widgets default to normal hud font
   tw->font = hud_font;

   // [CG] All widgets start with 100% background opacity.
   tw->bg_opacity = FRACUNIT;
}

//
// HU_DynAutomapTick
//
// A suitable ticker for dynamic automap-only text widgets.
//
static void HU_DynAutomapTick(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   tw->message = automapactive ? tw->alloc : NULL;
}

//
// HU_DynamicTextWidget
//
// Adds a dynamically allocated text widget to the hash table.
// For scripting.
// 
// [CG] Un-static'd
// static void HU_DynamicTextWidget(const char *name, int x, int y, int font,
//                                  char *message, int cleartic, int flags)
void HU_DynamicTextWidget(const char *name, int x, int y, int font,
                          char *message, int cleartic, int flags)
{
   hu_textwidget_t *newtw = calloc(1, sizeof(hu_textwidget_t));

   // set id
   if(!newtw->widget.name)
      newtw->widget.name = strdup(name);

   // add to hash
   if(!HU_AddWidgetToHash((hu_widget_t *)newtw))
   {
      // if addition was unsuccessful, we delete it now
      free(newtw);
      return;
   }

   // set virtuals
   HU_TextWidgetDefaults(newtw);

   // for automap-only widgets, add a ticker
   if(flags & TW_AUTOMAP_ONLY)
      newtw->widget.ticker = HU_DynAutomapTick;

   // if no clear, remove the clear func
   if(flags & TW_NOCLEAR)
      newtw->widget.clear = NULL;

   // set properties
   newtw->x = x;
   newtw->y = y;   
   if(!(newtw->font = E_FontForNum(font)))
      newtw->font = hud_font;
   newtw->cleartic = cleartic >= 0 ? cleartic : 0;
   newtw->flags = flags;

   // set message
   newtw->message = newtw->alloc = strdup(message);

   HU_UpdateEraseData(newtw);
}

//
// Center-of-screen, Quake-style Message
//

static hu_textwidget_t centermessage_widget;

//
// HU_InitCenterMessage
//
// Sets up the center message widget.
//
static void HU_InitCenterMessage(void)
{
   // set id
   if(!centermessage_widget.widget.name ||
      !strlen(centermessage_widget.widget.name))
   {
      centermessage_widget.widget.name = strdup("_HU_CenterMsgWidget");
   }

   // set virtuals
   HU_TextWidgetDefaults(&centermessage_widget);

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&centermessage_widget);

   // set data
   centermessage_widget.x = 0;
   centermessage_widget.y = 0;
   centermessage_widget.message = NULL;
   centermessage_widget.cleartic = 0;
}

static const char *centermsg_color;

//
// HU_CenterMessage
//
// haleyjd 04/27/04: rewritten to use qstring
//
void HU_CenterMessage(const char *s)
{
   static qstring_t qstr;
   int st_height = GameModeInfo->StatusBar->height;
   hu_textwidget_t *tw = &centermessage_widget;

   QStrClearOrCreate(&qstr, 128);

   // haleyjd 02/28/06: colored center message
   if(centermsg_color)
   {
      QStrCat(&qstr, centermsg_color);
      centermsg_color = NULL;
   }
   
   QStrCat(&qstr, s);
  
   // [CG] This is no longer const.
   // tw->message = QStrConstPtr(&qstr);
   tw->message = QStrBuffer(&qstr);
   tw->x = (SCREENWIDTH  - V_FontStringWidth(hud_font, s)) / 2;
   tw->y = (SCREENHEIGHT - V_FontStringHeight(hud_font, s) -
            ((scaledviewheight == SCREENHEIGHT) ? 0 : st_height - 8)) / 2;
   tw->cleartic = leveltime + (message_timer * 35) / 1000;

   HU_UpdateEraseData(tw);
   
   // print message to console also
   C_Printf("%s\n", s);
}

//
// HU_CenterMessageTimed
//
// haleyjd: timed center message. Originally for FraggleScript,
// now revived for Small.
//
void HU_CenterMessageTimed(const char *s, int tics)
{
   HU_CenterMessage(s);
   centermessage_widget.cleartic = leveltime + tics;
}

//
// HU_CenterMessageTimedColor
//
// haleyjd: as above but with special coloration. For ACS.
//
void HU_CenterMsgTimedColor(const char *s, const char *color, int tics)
{
   centermsg_color = color;
   HU_CenterMessageTimed(s, tics);
}

// haleyjd 03/03/07: moved crosshair widget down below centermessage for fix

//
// Crosshair Widget
//

static hu_crosshairwidget_t crosshair_widget;

// globals related to the crosshair
int crosshairs[CROSSHAIRS];
byte *targetcolour, *notargetcolour, *friendcolour;
int crosshairnum;       // 0 = none
boolean crosshair_hilite; // haleyjd 06/07/05
char *cross_str[]= { "none", "cross", "angle" }; // for console

//
// HU_CrossHairTick
//
// Updates the crosshair on each gametic.
//
static void HU_CrossHairTick(hu_widget_t *widget)
{
   hu_crosshairwidget_t *crosshair = (hu_crosshairwidget_t *)widget;

   // default to no target
   crosshair->color = notargetcolour;

   // [CG] default to no target name as well.
   crosshair->target_name = NULL;

   // fast as possible: don't bother with this crap if the crosshair 
   // isn't going to be displayed anyway
   // [CG] There's a DMFLAG for this now as well.
   if((clientserver && (dmflags2 & dmf_allow_crosshair) == 0) ||
      !crosshairnum ||
      !crosshair_hilite ||
      crosshairs[crosshairnum-1] == -1)
   {
      return;
   }

   // search for targets
   
   P_AimLineAttack(players[displayplayer].mo,
                   players[displayplayer].mo->angle, 
                   16*64*FRACUNIT, 0);

   if(clip.linetarget)
   {
      // target found
      crosshair->color = targetcolour; // default

      // [CG] Set the crosshair's target name to the target's name if the
      //      target is a player.
      if(clip.linetarget->player)
      {
         crosshair->target_name = &clip.linetarget->player->name[0];
      }
      
      // haleyjd 06/06/09: some special behaviors
      if(clip.linetarget->flags & MF_FRIEND)
         crosshair->color = friendcolour;

      if(((clip.linetarget->flags  & MF_SHADOW || 
           clip.linetarget->flags3 & MF3_GHOST) && M_Random() & 0x0F) ||
         clip.linetarget->flags2 & MF2_DONTDRAW)
      {
         crosshair->color = notargetcolour;
         crosshair->target_name = NULL;
      }
   }
}

//
// HU_CrossHairDraw
//
// Draws the crosshair patch.
//
static void HU_CrossHairDraw(hu_widget_t *widget)
{
   int drawx, drawy, h, w;
   hu_crosshairwidget_t *crosshair = (hu_crosshairwidget_t *)widget;
   byte *pal = crosshair->color;
   patch_t *patch;
   
   // haleyjd 03/03/07: don't display while showing a center message
   if(!crosshairnum || crosshairs[crosshairnum - 1] == -1 || 
      viewcamera || automapactive || centermessage_widget.cleartic > leveltime)
      return;

   // [CG] There's a DMFLAGS for this now as well.
   if(clientserver && (dmflags2 & dmf_allow_crosshair) == 0)
   {
      return;
   }

   patch = W_CacheLumpNum(crosshairs[crosshairnum - 1], PU_CACHE);

   // where to draw??

   w = SwapShort(patch->width);
   h = SwapShort(patch->height);
   
   drawx = (SCREENWIDTH - w) / 2;

   // haleyjd 04/09/05: this kludge moves the crosshair to within
   // a tolerable distance of the player's true vertical aim when
   // the screen size is less than full.
   if(scaledviewheight != SCREENHEIGHT)
   {
      // use 1/5 of the displayplayer's pitch angle in integer degrees
      int angle = players[displayplayer].pitch / (ANGLE_1*5);
      drawy = scaledwindowy + (scaledviewheight - h) / 2 + angle;
   }
   else
      drawy = scaledwindowy + (scaledviewheight - h) / 2;
  
   if(pal == notargetcolour)
   {
      V_DrawPatchTL(drawx, drawy, &vbscreen, patch, pal, FTRANLEVEL);
   }
   else
   {
      V_DrawPatchTranslated(drawx, drawy, &vbscreen, patch, pal, false);

      // [CG] Crosshair is resting on a target, so if the server allows it and
      //      the client's so configured, draw the target's name.
      if(crosshair->target_name &&
         display_target_names &&
         (!clientserver || dmflags2 & dmf_allow_target_names))
      {
         V_FontWriteTextColored(
            hud_overfont, crosshair->target_name, CR_GOLD,
            (SCREENWIDTH >> 1) -
               (V_FontStringWidth(hud_overfont, crosshair->target_name) >> 1),
            ST_Y -
               (V_FontStringHeight(hud_overfont, crosshair->target_name + 3))
         );
      }
   }
}

//
// HU_InitCrossHair
//
// Sets up the crosshair widget and associated globals.
//
void HU_InitCrossHair(void)
{
   // haleyjd TODO: support user-added crosshairs
   crosshairs[0] = W_CheckNumForName("CROSS1");
   crosshairs[1] = W_CheckNumForName("CROSS2");
   
   notargetcolour = cr_red;
   targetcolour   = cr_green;
   friendcolour   = cr_blue;

   // set up widget object
   crosshair_widget.color = notargetcolour;
   crosshair_widget.patch = NULL; // determined dynamically

   // set up the object id
   if(!crosshair_widget.widget.name)
   {
      crosshair_widget.widget.name = strdup("_HU_CrosshairWidget");
   }

   // set type
   crosshair_widget.widget.type = WIDGET_PATCH;

   // set up object virtuals
   crosshair_widget.widget.ticker = HU_CrossHairTick;
   crosshair_widget.widget.drawer = HU_CrossHairDraw;

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&crosshair_widget);
}




//
// Elapsed level time (automap)
//

static hu_textwidget_t leveltime_widget;

// haleyjd 02/12/06: configuration variables
boolean hu_showtime;       // enable/disable flag for level time
int hu_timecolor;          // color of time text

//
// HU_LevelTimeTick
//
// Updates the automap level timer.
//
static void HU_LevelTimeTick(hu_widget_t *widget)
{
   static char timestr[32];
   int seconds;
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;
   
   if(!automapactive || !hu_showtime)
   {
      tw->message = NULL;
      return;
   }
   
   seconds = leveltime / 35;
   timestr[0] = '\0';
   
   psnprintf(timestr, sizeof(timestr), "%c%02i:%02i:%02i", 
             hu_timecolor + 128, seconds/3600, (seconds%3600)/60, seconds%60);
   
   tw->message = timestr;        
}

//
// HU_LevelTimeClear
//
// Handles HU_Start actions for the level time widget
//
static void HU_LevelTimeClear(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   if(GameModeInfo->flags & GIF_HUDSTATBARNAME && LevelInfo.levelName)
   {
      int len = V_FontStringWidth(hud_font, LevelInfo.levelName);

      // If level name is too long, move the timer up one row;
      // otherwise, restore to the normal position.
      if(len >= tw->x)
         tw->y = SCREENHEIGHT - ST_HEIGHT - (hud_font->absh * 2 + 1);
      else
         tw->y = SCREENHEIGHT - ST_HEIGHT - hud_font->absh;
   }
}

//
// HU_InitLevelTime
//
// Sets up the level time widget for the automap.
//
static void HU_InitLevelTime(void)
{
   // set id
   if(!leveltime_widget.widget.name)
   {
      leveltime_widget.widget.name = strdup("_HU_LevelTimeWidget");
   }

   leveltime_widget.widget.type = WIDGET_TEXT;

   // set virtuals
   leveltime_widget.widget.drawer = HU_TextWidgetDraw;
   leveltime_widget.widget.ticker = HU_LevelTimeTick;
   leveltime_widget.widget.clear  = HU_LevelTimeClear;
   
   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&leveltime_widget);

   // set data
   if(!(GameModeInfo->flags & GIF_HUDSTATBARNAME))
   {
      leveltime_widget.x = 240;
      leveltime_widget.y = 10;
   }
   else
   {
      // haleyjd: use proper font metrics
      leveltime_widget.x = SCREENWIDTH - 60;
      leveltime_widget.y = SCREENHEIGHT - ST_HEIGHT - hud_font->absh;
   }
   leveltime_widget.message = NULL;
   leveltime_widget.font = hud_font;
   leveltime_widget.cleartic = 0;
   leveltime_widget.flags = TW_AUTOMAP_ONLY;
}

//
// Automap level name display
//

static hu_textwidget_t levelname_widget;

// haleyjd 02/12/06: configuration variables
int hu_levelnamecolor;

//
// HU_LevelNameTick
//
// Updates the automap level name widget.
//
static void HU_LevelNameTick(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   // [CG] This is no longer const.
   // tw->message = automapactive ? LevelInfo.levelName : NULL;
   tw->message = automapactive ? (char *)LevelInfo.levelName : NULL;
   tw->color   = hu_levelnamecolor + 1;
}

//
// HU_InitLevelName
//
// Sets up the level name widget for the automap.
//
static void HU_InitLevelName(void)
{
   // set id
   if(!levelname_widget.widget.name)
   {
      levelname_widget.widget.name = strdup("_HU_LevelNameWidget");
   }

   levelname_widget.widget.type = WIDGET_TEXT;

   // set virtuals
   levelname_widget.widget.drawer = HU_TextWidgetDraw;
   levelname_widget.widget.ticker = HU_LevelNameTick;

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&levelname_widget);

   // set data
   if(!(GameModeInfo->flags & GIF_HUDSTATBARNAME))
   {
      levelname_widget.x = 20;
      levelname_widget.y = 145;
   }
   else
   {
      // haleyjd: use proper font metrics
      levelname_widget.x = 0;
      levelname_widget.y = SCREENHEIGHT - ST_HEIGHT - hud_font->absh;
   }
   levelname_widget.message = NULL;
   levelname_widget.font = hud_font;
   levelname_widget.cleartic = 0;
   levelname_widget.flags = TW_AUTOMAP_ONLY;
}

//
// Chat message display
//

static hu_textwidget_t chat_widget;

char chatinput[100] = "";

//
// HU_ChatTick
//
// Updates the chat message widget.
//
static void HU_ChatTick(hu_widget_t *widget)
{
   static char tempchatmsg[128];
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;
   
   if(chat_active)
   {
      psnprintf(tempchatmsg, sizeof(tempchatmsg), "%s_", chatinput);
      tw->message = tempchatmsg;
      HU_UpdateEraseData(tw);
   }
   else
   {
      tw->message = NULL;
   }
}

//
// HU_InitChat
//
// Sets up the chat message widget.
//
static void HU_InitChat(void)
{
   // set id
   if(!chat_widget.widget.name)
   {
      chat_widget.widget.name = strdup("_HU_ChatWidget");
   }

   // set virtuals
   HU_TextWidgetDefaults(&chat_widget);
   
   // overrides
   chat_widget.widget.ticker = HU_ChatTick;
   chat_widget.widget.clear  = NULL;

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&chat_widget);

   // set data
   chat_widget.x = 0;
   chat_widget.y = 0;
   chat_widget.message = NULL;
   chat_widget.font = hud_font;
   chat_widget.cleartic = 0;
}

//
// HU_ChatRespond
//
// Responds to chat-related events.
//
static boolean HU_ChatRespond(event_t *ev)
{
   char ch = 0;
   static boolean shiftdown;

   // haleyjd 06/11/08: get HUD actions
   G_KeyResponder(ev, kac_hud);
   
   if(ev->data1 == KEYD_RSHIFT) 
      shiftdown = (ev->type == ev_keydown);
   
   if(ev->type != ev_keydown)
      return false;
   
   if(!chat_active)
   {
      if(ev->data1 == key_chat && netgame) 
      {       
         chat_active = true; // activate chat
         chatinput[0] = 0;   // empty input string
         return true;
      }
      return false;
   }
  
   if(altdown && ev->type == ev_keydown &&
      ev->data1 >= '0' && ev->data1 <= '9')
   {
      // chat macro
      char tempstr[100];
      psnprintf(tempstr, sizeof(tempstr),
                "say \"%s\"", chat_macros[ev->data1-'0']);
      C_RunTextCmd(tempstr);
      chat_active = false;
      return true;
   }
  
   if(ev->data1 == KEYD_ESCAPE)    // kill chat
   {
      chat_active = false;
      return true;
   }
  
   if(ev->data1 == KEYD_BACKSPACE && chatinput[0])
   {
      chatinput[strlen(chatinput)-1] = 0;      // remove last char
      return true;
   }
  
   if(ev->data1 == KEYD_ENTER)
   {
      char tempstr[100];
      psnprintf(tempstr, sizeof(tempstr), "say \"%s\"", chatinput);
      C_RunTextCmd(tempstr);
      chat_active = false;
      return true;
   }

   if(ev->character)
      ch = ev->character;
   else if(ev->data1 > 31 && ev->data1 < 127)
      ch = shiftdown ? shiftxform[ev->data1] : ev->data1; // shifted?
   
   if(ch > 31 && ch < 127)
   {
      psnprintf(chatinput, sizeof(chatinput), "%s%c", chatinput, ch);
      return true;
   }
   return false;
}

//
// Automap coordinate display
//
//   Yet Another Lost MBF Feature
//   Restored by Quasar (tm)
//

static hu_textwidget_t coordx_widget;
static hu_textwidget_t coordy_widget;
static hu_textwidget_t coordz_widget;

// haleyjd 02/12/06: configuration variables
boolean hu_showcoords;
int hu_coordscolor;

//
// HU_CoordTick
//
// Updates automap coordinate widgets.
//
static void HU_CoordTick(hu_widget_t *widget)
{
   player_t *plyr;
   fixed_t x, y, z;
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;
   
   // haleyjd: wow, big bug here -- these buffers were not static
   // and thus corruption was occuring when the function returned
   static char coordxstr[16];
   static char coordystr[16];
   static char coordzstr[16];

   if(!automapactive || !hu_showcoords)
   {
      tw->message = NULL;
      return;
   }
   plyr = &players[displayplayer];

   AM_Coordinates(plyr->mo, &x, &y, &z);

   if(tw == &coordx_widget)
   {
      sprintf(coordxstr, "%cX: %-5d", hu_coordscolor + 128, x >> FRACBITS);
      tw->message = coordxstr;
   }
   else if(tw == &coordy_widget)
   {
      sprintf(coordystr, "%cY: %-5d", hu_coordscolor + 128, y >> FRACBITS);
      tw->message = coordystr;
   }
   else
   {
      sprintf(coordzstr, "%cZ: %-5d", hu_coordscolor + 128, z >> FRACBITS);
      tw->message = coordzstr;
   }
}

//
// HU_InitCoords
//
// Initializes the automap coordinate widgets.
//   
static void HU_InitCoords(void)
{
   // set ids
   if(!coordx_widget.widget.name)
   {
      coordx_widget.widget.name = strdup("_HU_CoordXWidget");
   }
   if(!coordy_widget.widget.name)
   {
      coordy_widget.widget.name = strdup("_HU_CoordYWidget");
   }
   if(!coordz_widget.widget.name)
   {
      coordz_widget.widget.name = strdup("_HU_CoordZWidget");
   }

   coordx_widget.widget.type =
      coordy_widget.widget.type =
      coordz_widget.widget.type = WIDGET_TEXT;

   // set virtuals
   coordx_widget.widget.drawer = HU_TextWidgetDraw;
   coordy_widget.widget.drawer = HU_TextWidgetDraw;
   coordz_widget.widget.drawer = HU_TextWidgetDraw;
   coordx_widget.widget.ticker = HU_CoordTick;
   coordy_widget.widget.ticker = HU_CoordTick;
   coordz_widget.widget.ticker = HU_CoordTick;

   // add to hash
   HU_AddWidgetToHash((hu_widget_t *)&coordx_widget);
   HU_AddWidgetToHash((hu_widget_t *)&coordy_widget);
   HU_AddWidgetToHash((hu_widget_t *)&coordz_widget);

   // set data
   if(GameModeInfo->type == Game_Heretic)
   {
      coordx_widget.x = coordy_widget.x = coordz_widget.x = 20;
      coordx_widget.y = 10;
      coordy_widget.y = 19;
      coordz_widget.y = 28;
   }
   else
   {
      coordx_widget.x = coordy_widget.x = coordz_widget.x = SCREENWIDTH - 64;
      coordx_widget.y = 8;
      coordy_widget.y = 17;
      coordz_widget.y = 25;
   }
   coordx_widget.message = coordy_widget.message = coordz_widget.message = NULL;
   coordx_widget.font = coordy_widget.font = coordz_widget.font = hud_font;
   coordx_widget.cleartic = coordy_widget.cleartic = coordz_widget.cleartic = 0;
   coordx_widget.flags = coordy_widget.flags = coordz_widget.flags = TW_AUTOMAP_ONLY;
}

////////////////////////////////////////////////////////////////////////
//
// Tables
//

const char* shiftxform;

const char english_shiftxform[] =
{
  0,
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31,
  ' ', '!', '"', '#', '$', '%', '&',
  '"', // shift-'
  '(', ')', '*', '+',
  '<', // shift-,
  '_', // shift--
  '>', // shift-.
  '?', // shift-/
  ')', // shift-0
  '!', // shift-1
  '@', // shift-2
  '#', // shift-3
  '$', // shift-4
  '%', // shift-5
  '^', // shift-6
  '&', // shift-7
  '*', // shift-8
  '(', // shift-9
  ':',
  ':', // shift-;
  '<',
  '+', // shift-=
  '>', '?', '@',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '[', // shift-[
  '|', // shift-backslash - OH MY GOD DOES WATCOM SUCK
  ']', // shift-]
  '"', '_',
  '\'', // shift-`
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '{', '|', '}', '~', 127
};

/////////////////////////////////////////////////////////////////////////
//
// Console Commands
//

VARIABLE_BOOLEAN(showMessages,  NULL,                   onoff);
VARIABLE_INT(mess_colour,       NULL, 0, CR_LIMIT-1,    textcolours);

VARIABLE_BOOLEAN(obituaries,    NULL,                   onoff);
VARIABLE_INT(obcolour,          NULL, 0, CR_LIMIT-1,    textcolours);
VARIABLE_INT(crosshairnum,      NULL, 0, CROSSHAIRS-1,  cross_str);
VARIABLE_INT(hud_msg_lines,     NULL, 0, 14,            NULL);
VARIABLE_INT(message_timer,     NULL, 0, 100000,        NULL);

// haleyjd 02/12/06: lost/new hud options
VARIABLE_TOGGLE(hu_showtime,    NULL,                   yesno);
VARIABLE_TOGGLE(hu_showcoords,  NULL,                   yesno);
VARIABLE_INT(hu_timecolor,      NULL, 0, CR_LIMIT-1,    textcolours);
VARIABLE_INT(hu_levelnamecolor, NULL, 0, CR_LIMIT-1,    textcolours);
VARIABLE_INT(hu_coordscolor,    NULL, 0, CR_LIMIT-1,    textcolours);

VARIABLE_BOOLEAN(hud_msg_scrollup,  NULL,               yesno);
VARIABLE_TOGGLE(crosshair_hilite,   NULL,               onoff);

CONSOLE_VARIABLE(hu_obituaries, obituaries, 0) {}
CONSOLE_VARIABLE(hu_obitcolor, obcolour, 0) {}
CONSOLE_VARIABLE(hu_crosshair, crosshairnum, 0) {}
CONSOLE_VARIABLE(hu_crosshair_hilite, crosshair_hilite, 0) {}
CONSOLE_VARIABLE(hu_messages, showMessages, 0) {}
CONSOLE_VARIABLE(hu_messagecolor, mess_colour, 0) {}

CONSOLE_NETCMD(say, cf_netvar, netcmd_chat)
{
   if(!clientserver)
   {
      S_StartSound(NULL, GameModeInfo->c_ChatSound);
      doom_printf(
         "%s: %s",
         players[Console.cmdsrc].name,
         QStrConstPtr(&Console.args)
      );
   }
   else if(strlen(QStrConstPtr(&Console.args)) > MAX_STRING_SIZE)
      doom_printf("Message too long.");
   else if(CS_CLIENT)
      CL_BroadcastMessage(QStrConstPtr(&Console.args));
   else if(CS_SERVER)
      SV_Say(QStrConstPtr(&Console.args));
}

CONSOLE_COMMAND(rcon, cf_netonly)
{
   if(CS_CLIENT)
      CL_RCONMessage(QStrConstPtr(&Console.args));
   else
      doom_printf("Servers don't need RCON.");
}

CONSOLE_NETCMD(to_server, cf_netvar, netcmd_chat)
{
   if(CS_CLIENT)
      CL_ServerMessage(QStrConstPtr(&Console.args));
   else
      doom_printf("Can't send messages to yourself.");
}

CONSOLE_NETCMD(to_player, cf_netvar, netcmd_chat)
{
   if(CS_CLIENT)
   {
      if(QStrAtoi(&Console.argv[0]) == consoleplayer)
         doom_printf("Can't send messages to yourself.");
      else
      {
         CL_PlayerMessage(
            QStrAtoi(&Console.argv[0]),
            QStrConstPtr(&Console.argv[1])
         );
      }
   }
   else if(CS_SERVER)
   {
      SV_SayToPlayer(
         QStrAtoi(&Console.argv[0]),
         QStrConstPtr(&Console.argv[1])
      );
   }
}

CONSOLE_NETCMD(to_team, cf_netvar, netcmd_chat)
{
   // [CG] I can't see any use for server => team communication, so this
   //      just broadcasts in that case.
   printf("Sending message to team.\n");
   if(CS_CLIENT)
      CL_TeamMessage(QStrConstPtr(&Console.args));
   else if(CS_SERVER)
      SV_Say(QStrConstPtr(&Console.args));
}

CONSOLE_VARIABLE(hu_messagelines, hud_msg_lines, 0) {}
CONSOLE_VARIABLE(hu_messagescroll, hud_msg_scrollup, 0) {}
CONSOLE_VARIABLE(hu_messagetime, message_timer, 0) {}

// haleyjd 02/12/06: lost/new hud options
CONSOLE_VARIABLE(hu_showtime, hu_showtime, 0) {}
CONSOLE_VARIABLE(hu_showcoords, hu_showcoords, 0) {}
CONSOLE_VARIABLE(hu_timecolor, hu_timecolor, 0) {}
CONSOLE_VARIABLE(hu_levelnamecolor, hu_levelnamecolor, 0) {}
CONSOLE_VARIABLE(hu_coordscolor, hu_coordscolor, 0) {}


extern void HU_FragsAddCommands(void);
extern void HU_OverAddCommands(void);

void HU_AddCommands(void)
{
   C_AddCommand(hu_obituaries);
   C_AddCommand(hu_obitcolor);
   C_AddCommand(hu_crosshair);
   C_AddCommand(hu_crosshair_hilite);
   C_AddCommand(hu_messages);
   C_AddCommand(hu_messagecolor);
   C_AddCommand(rcon);
   C_AddCommand(to_server);
   C_AddCommand(to_player);
   C_AddCommand(to_team);
   C_AddCommand(say);   
   C_AddCommand(hu_messagelines);
   C_AddCommand(hu_messagescroll);
   C_AddCommand(hu_messagetime);
   C_AddCommand(hu_showtime);
   C_AddCommand(hu_showcoords);
   C_AddCommand(hu_timecolor);
   C_AddCommand(hu_levelnamecolor);
   C_AddCommand(hu_coordscolor);
   
   HU_FragsAddCommands();
   HU_OverAddCommands();
}

#ifndef EE_NO_SMALL_SUPPORT
//
// Script functions
//

static cell AMX_NATIVE_CALL sm_movewidget(AMX *amx, cell *params)
{
   int err;
   char *name;
   hu_widget_t *widget;

   // get name of widget
   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   if(!(widget = HU_WidgetForName(name)))
   {
      free(name);
      return 0;
   }

   switch(widget->type)
   {
   case WIDGET_TEXT:
      {
         hu_textwidget_t *tw = (hu_textwidget_t *)widget;

         tw->x = params[2];
         tw->y = params[3];

         HU_UpdateEraseData(tw);
      }
      break;
   case WIDGET_PATCH:
      {
         hu_patchwidget_t *pw = (hu_patchwidget_t *)widget;

         pw->x = params[2];
         pw->y = params[3];
      }
      break;
   default:
      break;
   }

   free(name);
   return 0;
}

static cell AMX_NATIVE_CALL sm_newpatchwidget(AMX *amx, cell *params)
{
   int err;
   char *name, *patch;

   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }
   if((err = SM_GetSmallString(amx, &patch, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      free(name);
      return -1;
   }
   
   HU_DynamicPatchWidget(name, params[3], params[4], params[5], params[6], patch);

   free(name);
   free(patch);

   return 0;
}

static cell AMX_NATIVE_CALL sm_setwidgetpatch(AMX *amx, cell *params)
{
   int err;
   char *name, *patch;
   hu_widget_t *widget;
   hu_patchwidget_t *pw;

   // get name of widget
   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   // get name of patch
   if((err = SM_GetSmallString(amx, &patch, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      free(name);
      return -1;
   }

   if((widget = HU_WidgetForName(name)) && widget->type == WIDGET_PATCH)
   {
      pw = (hu_patchwidget_t *)widget;

      // 08/16/06: bug fix -- copy patch name
      strncpy(pw->patchname, patch, 9);

      // pre-cache the patch graphic
      pw->patch = W_CacheLumpName(patch, PU_CACHE);
   }

   free(name);
   free(patch);

   return 0;
}

static cell AMX_NATIVE_CALL sm_patchwidgetcolor(AMX *amx, cell *params)
{
   int err;
   char *name;
   hu_widget_t *widget;
   hu_patchwidget_t *pw;

   // get name of widget
   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   if((widget = HU_WidgetForName(name)) && widget->type == WIDGET_PATCH)
   {
      pw = (hu_patchwidget_t *)widget;

      if(params[2] != -2)
         pw->color = params[2] >= 0 && params[2] < CR_LIMIT ? colrngs[params[1]] : NULL;

      if(params[3] >= 0 && params[3] <= FRACUNIT)
         pw->tl_level = params[3];
   }

   free(name);

   return 0;
}

static cell AMX_NATIVE_CALL sm_newtextwidget(AMX *amx, cell *params)
{
   int err;
   char *name, *msg;

   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }
   if((err = SM_GetSmallString(amx, &msg, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      free(name);
      return -1;
   }

   HU_DynamicTextWidget(name, params[3], params[4], params[5], msg,
                        params[6] != 0 ? leveltime + params[6] : 0, params[7]);

   free(name);
   // [CG] The text widget will take ownership of the message pointer, no need
   //      to free it here.
   // free(msg);

   return 0;
}

static cell AMX_NATIVE_CALL sm_getwidgettext(AMX *amx, cell *params)
{
   int err, size, packed;
   cell *deststr;
   char *name;
   hu_widget_t *widget;
   hu_textwidget_t *tw;

   size   = (int)params[3];
   packed = (int)params[4];
   
   // resolve address of destination buffer
   if((err = amx_GetAddr(amx, params[2], &deststr)) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   // get name of widget
   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   if((widget = HU_WidgetForName(name)) && widget->type == WIDGET_TEXT)
   {
      char *tempbuf = malloc(size+1);
      
      tw = (hu_textwidget_t *)widget;

      if(tw->message)
         psnprintf(tempbuf, size+1, "%s", tw->message);
      else
         tempbuf[0] = '\0';
      
      amx_SetString(deststr, tempbuf, packed, 0);

      free(tempbuf);
   }

   free(name);

   return 0;
}

static cell AMX_NATIVE_CALL sm_setwidgettext(AMX *amx, cell *params)
{
   int err;
   char *name, *value;
   hu_widget_t *widget;
   hu_textwidget_t *tw;

   // get name of widget
   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   // get value
   if((err = SM_GetSmallString(amx, &value, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      free(name);
      return -1;
   }

   if((widget = HU_WidgetForName(name)) && widget->type == WIDGET_TEXT)
   {
      tw = (hu_textwidget_t *)widget;

      if(tw->alloc)
         free(tw->alloc);

      tw->message = tw->alloc = strdup(value);

      tw->cleartic = params[3] != 0 ? leveltime + params[3] : 0;

      HU_UpdateEraseData(tw);
   }

   free(name);
   free(value);

   return 0;
}

static cell AMX_NATIVE_CALL sm_togglewidget(AMX *amx, cell *params)
{
   int err;
   char *name;
   hu_widget_t *widget;

   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   if((widget = HU_WidgetForName(name)))
      HU_ToggleWidget(widget, params[2]);

   free(name);

   return 0;
}

static cell AMX_NATIVE_CALL sm_centermsgtimed(AMX *amx, cell *params)
{
   int tics, err;
   char *text;

   if((err = SM_GetSmallString(amx, &text, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   tics = params[2];

   HU_CenterMessageTimed(text, tics);

   free(text);

   return 0;
}

//
// sm_inautomap
//
// Returns true or false to indicate state of automap. Useful for turning
// off some HUD stuff like patch widgets.
//
static cell AMX_NATIVE_CALL sm_inautomap(AMX *amx, cell *params)
{
   return (cell)automapactive;
}

static cell AMX_NATIVE_CALL sm_gethudmode(AMX *amx, cell *params)
{
   if(hud_enabled && hud_overlaystyle > 0) // Boom HUD enabled, return style
      return (cell)hud_overlaystyle + 1;
   else if(viewheight == video.height)         // Fullscreen (no HUD)
      return 0;			
   else                                    // Vanilla style status bar
      return 1;
}

AMX_NATIVE_INFO hustuff_Natives[] =
{
   { "_MoveWidget",         sm_movewidget       },
   { "_NewPatchWidget",     sm_newpatchwidget   },
   { "_SetWidgetPatch",     sm_setwidgetpatch   },
   { "_PatchWidgetColor",   sm_patchwidgetcolor },
   { "_NewTextWidget",      sm_newtextwidget    },
   { "_GetWidgetText",      sm_getwidgettext    },
   { "_SetWidgetText",      sm_setwidgettext    },
   { "_ToggleWidget",       sm_togglewidget     },
   { "_CenterMsgTimed",     sm_centermsgtimed   },
   { "_GetHUDMode",         sm_gethudmode       },
   { "_InAutomap",          sm_inautomap        },
   { NULL, NULL }
};
#endif

// EOF
