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
// Heads-Up Display
//
// haleyjd: Rewritten a third time. Displays widgets using a system
// of C++ classes.
//
// By James Haley and Simon Howard
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "a_small.h"
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "g_bind.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_inventory.h"
#include "hu_stuff.h"
#include "hu_over.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_map.h"
#include "p_maputl.h"   // ioanch 20160101: for fixing the autoaim bug
#include "p_setup.h"
#include "p_spec.h"
#include "r_draw.h"
#include "r_patch.h"
#include "r_state.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

char *chat_macros[10];
extern const char* shiftxform;
extern const char english_shiftxform[]; // haleyjd: forward declaration
//boolean chat_on;
bool chat_active = false;
int obituaries = 0;
int obcolour = CR_BRICK;       // the colour of death messages

vfont_t *hud_font;
char *hud_fontname;

static bool HU_ChatRespond(const event_t *ev);

//=============================================================================

//
// True if to allow automap widget. Overlay mode is stricter.
//
inline static bool HU_allowMapWidget()
{
   return automapactive && !automap_overlay;
}

// widget hash table
HUDWidget *HUDWidget::hu_chains[NUMWIDGETCHAINS];

//
// HUDWidget::WidgetForName
//
// Retrieves a widget given its name, using hashing.
// Returns nullptr if no such widget exists.
//
HUDWidget *HUDWidget::WidgetForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMWIDGETCHAINS;
   HUDWidget *cur = hu_chains[key];

   while(cur && strncasecmp(name, cur->name, 33))
      cur = cur->next;

   return cur;
}

//
// HUDWidget::AddWidgetToHash
//
// Adds a widget to the hash table, but only if one of the given
// name doesn't exist. Returns true if successful.
//
bool HUDWidget::AddWidgetToHash(HUDWidget *widget)
{
   int key;

   // make sure one doesn't already exist by this name
   if(WidgetForName(widget->name))
   {
#ifdef RANGECHECK
      // for debug, cause an error to alert programmer of internal mishaps
      I_Error("HUDWidget::AddWidgetToHash: duplicate mnemonic %s\n", widget->name);
#endif
      return false;
   }

   key = D_HashTableKey(widget->name) % NUMWIDGETCHAINS;
   widget->next = hu_chains[key];
   hu_chains[key] = widget;

   return true;
}

//
// HUDWidget::StartWidgets
//
// Called from HU_Start to clear all widgets.
//
void HUDWidget::StartWidgets()
{
   // call all widget clear functions
   for(HUDWidget *widget : hu_chains)
   {
      while(widget)
      {
         widget->clear();
         widget = widget->next;
      }
   }
}

//
// HUDWidget::DrawWidgets
//
// Called from HU_Drawer to draw all widgets
//
void HUDWidget::DrawWidgets()
{
   // call all widget drawer functions
   for(HUDWidget *widget : hu_chains)
   {
      while(widget && !widget->disabled)
      {
         widget->drawer();
         widget = widget->next;
      }
   }
}

//
// HUDWidget::TickWidgets
//
// Called from HU_Ticker to drive all per-tic widget logic.
//
void HUDWidget::TickWidgets()
{
   // call all widget ticker functions
   for(HUDWidget *widget : hu_chains)
   {
      while(widget)
      {
         widget->ticker();
         widget = widget->next;
      }
   }
}

//=============================================================================
//
// Main HUD System Functions; Called externally.
//

static void HU_InitMsgWidget();
static void HU_InitCrossHair();
static void HU_InitWarnings();
static void HU_InitCenterMessage();
static void HU_InitLevelTime();
static void HU_InitLevelName();
static void HU_InitChat();
static void HU_InitCoords();
static void HU_InitStats();

//
// HU_InitNativeWidgets
//
// Sets up all the native widgets. Called from HU_Init below at startup.
//
static void HU_InitNativeWidgets()
{   
   HU_InitMsgWidget();
   HU_InitCrossHair();
   HU_InitWarnings();
   HU_InitCenterMessage();
   HU_InitLevelTime();
   HU_InitLevelName();
   HU_InitChat();
   HU_InitCoords();
   HU_InitStats();

   // HUD_FIXME: generalize?
   HU_FragsInit();
}

//
// HU_Init
//
// Called once at game startup to initialize the HUD system.
//
void HU_Init()
{
   shiftxform = english_shiftxform;

   if(!(hud_font = E_FontForName(hud_fontname)))
      I_Error("HU_Init: bad EDF font name %s\n", hud_fontname);

   HU_LoadFonts(); // overlay font
   HU_InitNativeWidgets();
}

//
// HU_Start
//
// Called to reinitialize the HUD system due to start of a new map
// or change of player viewpoint.
//
void HU_Start()
{
   HUDWidget::StartWidgets();

#if 0
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
void HU_Drawer()
{
#if 0
   // execute script event handlers
   if(gameScriptLoaded)
      SM_OptScriptCallback(&GameScript, "OnHUDPreDraw");

   if(levelScriptLoaded)
      SM_OptScriptCallback(&LevelScript, "OnHUDPreDraw");
#endif

   HUDWidget::DrawWidgets();

   // HUD_FIXME: generalize?
   // draw different modules
   int leftoffset, rightoffset;
   HU_FragsDrawer();
   HU_OverlayDraw(leftoffset, rightoffset);

   HU_BuffsDraw(leftoffset, rightoffset);

   // Temporary on-top overlays
   HU_InventoryDrawSelector();
}

//
// HU_Ticker
//
// Called from G_Ticker when gamestate is GS_LEVEL. Updates widgets
// for the current gametic.
//
void HU_Ticker()
{
   HUDWidget::TickWidgets();
}

// I don't know what this is doing here, but it can stay for now.
bool altdown = false;

//
// HU_Responder
//
// Called from G_Responder. Has priority over any other events
// intercepted by that function.
//
bool HU_Responder(const event_t *ev)
{
   if(ev->data1 == KEYD_LALT)
      altdown = (ev->type == ev_keydown);
   
   // only the chat widget can respond to events (for now at least)
   return HU_ChatRespond(ev);
}

//=============================================================================
//
// Normal Player Message Widget
//

#define MAXHUDMESSAGES 16
#define MAXHUDMSGLEN   256

class HUDMessageWidget : public HUDWidget
{
protected:
   char messages[MAXHUDMESSAGES][MAXHUDMSGLEN]; // messages
   int  current_messages;                       // number of messages
   int  scrolltime;                             // leveltime when message will scroll up

public:
   void addMessage(const char *s);

   virtual void ticker();
   virtual void drawer();
   virtual void clear();
};

static HUDMessageWidget msg_widget;

// globals related to the msg_widget

int hud_msg_lines;        // number of message lines in window up to 16
int hud_msg_scrollup;     // whether message list scrolls up
int message_timer;        // timer used for normal messages
int showMessages;         // Show messages has default, 0 = off, 1 = on
int mess_align;           // whether or not messages are default, left, or centre aligned
int mess_colour = CR_RED; // the colour of normal messages

//
// HUDMessageWidget::ticker
//
// Updates the message widget on each gametic.
//
void HUDMessageWidget::ticker()
{
   // check state of message scrolling
   if(!hud_msg_scrollup)
      return;

   // move up messages
   if(leveltime >= scrolltime)
   {
      for(int i = 0; i < current_messages - 1; i++)
         strncpy(messages[i], messages[i + 1], MAXHUDMSGLEN);

      if(current_messages)
         --current_messages;
      scrolltime = leveltime + (message_timer * 35) / 1000;
   }
}

//
// HUDMessageWidget::drawer
//
// Drawer for the player message widget.
//
void HUDMessageWidget::drawer()
{
   if(!showMessages)
      return;

   edefstructvar(vtextdraw_t, vtd);

   vtd.font        = hud_font;
   vtd.screen      = &vbscreenyscaled;
   vtd.flags       = VTXT_FIXEDCOLOR;
   vtd.fixedColNum = mess_colour;
   
   // go down a bit if chat active
   vtd.y = chat_active ? hud_font->absh : 0;
   
   for(int i = 0; i < current_messages; i++)
   {
      char *msg = messages[i];
      
      // haleyjd 12/26/02: center messages in proper gamemodes
      // haleyjd 08/26/12: center also if in widescreen modes
      // MaxW: 2019/09/08: Respect user's alignment setting
      if(mess_align == MSGALIGN_CENTRE ||
         (mess_align == MSGALIGN_DEFAULT && (GameModeInfo->flags & GIF_CENTERHUDMSG ||
          vbscreen.getVirtualAspectRatio() > 4 * FRACUNIT / 3)))
      {
         vtd.x = (vtd.screen->unscaledw - V_FontStringWidth(hud_font, msg)) / 2;
         vtd.flags |= VTXT_ABSCENTER;
      }
      else
         vtd.x = 0;
      
      vtd.s = msg;

      V_FontWriteTextEx(vtd);

      vtd.y += V_FontStringHeight(hud_font, msg);
   }
}

//
// HUDMessageWidget::clear
//
// Sets the number of messages being displayed to zero.
//
void HUDMessageWidget::clear()
{
   current_messages = 0;
}

//
// HUDMessageWidget::addMessage
//
// Add a message to the message widget.
//
void HUDMessageWidget::addMessage(const char *s)
{
   char *dest;
   if(hud_msg_lines <= 0)
      return;

   if(current_messages >= hud_msg_lines) // display full
   {
      current_messages = hud_msg_lines;   // cap it
      // scroll up      
      for(int i = 0; i < hud_msg_lines - 1; i++)
         strncpy(messages[i], messages[i+1], MAXHUDMSGLEN);
      
      dest = messages[hud_msg_lines - 1];
   }
   else 
   {
      // add one to the end
      dest = messages[current_messages];
      ++current_messages;
   }

   psnprintf(dest, MAXHUDMSGLEN, "%s", s);
   V_FontFitTextToRect(hud_font, dest, 0, 0, 320, 200);

   scrolltime = leveltime + (message_timer * 35) / 1000;
}

//
// HU_InitMsgWidget
//
// Called from HU_InitNativeWidgets. Sets up the player message widget.
//
static void HU_InitMsgWidget()
{
   // set up the object id
   msg_widget.setName("_HU_MsgWidget");
   msg_widget.setType(WIDGET_MISC);

   // add to hash
   HUDWidget::AddWidgetToHash(&msg_widget);   
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
   msg_widget.addMessage(s);
}

//=============================================================================
//
// Patch Widget: this type of widget just displays a screen patch.
//

class HUDPatchWidget : public HUDWidget
{
protected:
   int      x;            // x coordinate
   int      y;            // y coordinate
   byte    *color;        // color range table
   int      tl_level;     // translucency level
   char     patchname[9]; // patch name
   patch_t *patch;        // screen patch

public:
   virtual void drawer();

   void initProps(int px, int py, int pcol, int ptl, char ppatch[9])
   {
      // set properties
      x = px;
      y = py;
      if(pcol >= 0 && pcol < CR_LIMIT)
         color = colrngs[pcol];
      tl_level = ptl;

      // 06/15/06: copy patch name
      strncpy(patchname, ppatch, 9);

      // pre-cache patch -- haleyjd 06/15/06: use PU_CACHE
      patch = PatchLoader::CacheName(wGlobalDir, ppatch, PU_CACHE);
   }
};

//
// HUDPatchWidget::drawer
//
// Default drawing function for patch widgets. All the logic for
// deciding whether the patch is translucent and/or translated is
// already implemented in V_DrawPatchTL via the generalized patch
// drawing system.
//
void HUDPatchWidget::drawer()
{
   // be sure the patch is loaded
   patch = PatchLoader::CacheName(wGlobalDir, patchname, PU_CACHE);

   V_DrawPatchTL(x, y, &subscreen43, patch, color, tl_level);
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
   HUDPatchWidget *newpw = new HUDPatchWidget();

   // set id
   newpw->setName(name);
   newpw->setType(WIDGET_PATCH);

   // add to hash
   if(!HUDWidget::AddWidgetToHash(newpw))
   {
      // if addition was unsuccessful, we delete it now
      delete newpw;
      return;
   }

   newpw->initProps(x, y, color, tl_level, patch);
}

//=============================================================================
//
// Pop-up Warning Boxes
//
// Several different things that appear, quake-style, to warn you of
// problems.
//

class HUDOpenSocketWidget : public HUDPatchWidget
{
public:
   virtual void drawer();

   void init();
};

// Open Socket Warning
//
// Problem with network leads or something like that

static HUDOpenSocketWidget opensocket_widget;

//
// HU_WarningsDrawer
//
// Draws the warnings widgets. Calls down to the default drawer for
// patches when the proper conditions are met. Normally I'd set the
// conditions via a ticker but for opensocket that won't work if the
// game is frozen.
//
void HUDOpenSocketWidget::drawer()
{
   if(opensocket)
      HUDPatchWidget::drawer();
}

void HUDOpenSocketWidget::init()
{
   strncpy(patchname, "OPENSOCK", 9);
   patch    = PatchLoader::CacheName(wGlobalDir, "OPENSOCK", PU_CACHE);
   color    = nullptr;
   tl_level = FRACUNIT;
   x        = 20;
   y        = 20;
}

//
// HU_InitWarnings
//
// Sets up the open socket warning patch widget.
//
static void HU_InitWarnings()
{   
   // set up socket
   opensocket_widget.setName("_HU_OpenSocketWidget");
   opensocket_widget.setType(WIDGET_PATCH);

   // add to hash
   HUDWidget::AddWidgetToHash(&opensocket_widget);
 
   opensocket_widget.init();
}

//=============================================================================
//
// Text Widgets
//

// text widget flag values
enum
{
   TW_AUTOMAP_ONLY = 0x00000001, // appears in automap only
   TW_NOCLEAR      = 0x00000002, // dynamic widget with no clear func
   TW_BOXED        = 0x00000004, // 10/08/05: optional box around text
};

class HUDTextWidget : public HUDWidget
{
protected:
   int         x;        // x coordinate
   int         y;        // y coordinate
   vfont_t    *font;     // font object
   const char *message;  // text to draw
   char       *alloc;    // if non-nullptr, widget owns the message
   int         flags;    // special flags
   int         color;    // allow colored text drawing (col # + 1)

public:
   int cleartic; // gametic in which to clear (0 = never)

   virtual void drawer();
   virtual void clear();

   void initProps(int px, int py, int fontNum, int pcleartic, int pflags,
                  const char *pmsg)
   {
      x = px;
      y = py;   
      
      if(!(font = E_FontForNum(fontNum)))
         font = hud_font;
      
      cleartic = pcleartic >= 0 ? pcleartic : 0;
      flags    = pflags;

      message = alloc = estrdup(pmsg);
   }

   void initEmpty()
   {
      x        = 0;
      y        = 0;
      message  = nullptr;
      cleartic = 0;
      font     = hud_font;
   }

   void setMessage(const char *pmsg, int px, int py, int pcleartic)
   {
      message  = pmsg;
      x        = px;
      y        = py;
      cleartic = pcleartic;
   }
};

//
// HUDTextWidget::drawer
//
// Default drawing function for a text widget.
//
void HUDTextWidget::drawer()
{
   // Do not ever draw automap-only widgets if not in automap mode.
   // This fixes a long-standing bug automatically.
   if(flags & TW_AUTOMAP_ONLY && !HU_allowMapWidget())
      return;

   // 10/08/05: boxed message support
   if(flags & TW_BOXED)
   {
      int width, height;

      width  = V_FontStringWidth (font, message);
      height = V_FontStringHeight(font, message);

      V_DrawBox(x - 4, y - 4, width + 8, height + 8);
   }

   if(message && (!cleartic || leveltime < cleartic))
   {
      edefstructvar(vtextdraw_t, vdt);
      
      vdt.font   = font;
      vdt.s      = message;
      vdt.x      = x;
      vdt.y      = y;
      vdt.screen = &subscreen43;
      if(color)
      {
         vdt.flags       = VTXT_FIXEDCOLOR;
         vdt.fixedColNum = color - 1;
      }
      else
         vdt.flags = VTXT_NORMAL;

      V_FontWriteTextEx(vdt);
   }
}

//
// HUDTextWidget::clear
//
// Default clear function for a text widget.
//
void HUDTextWidget::clear()
{
   if(flags & TW_NOCLEAR)
      return;

   // if this widget owns its message, free it
   if(alloc)
   {
      efree(alloc);
      alloc = nullptr;
   }
   
   message  = nullptr;
   cleartic = 0;
}

//
// HU_DynamicTextWidget
//
// Adds a dynamically allocated text widget to the hash table.
// For scripting.
// 
static void HU_DynamicTextWidget(const char *name, int x, int y, int font,
                                 char *message, int cleartic, int flags)
{
   HUDTextWidget *newtw = new HUDTextWidget();

   // set id
   //strncpy(newtw->widget.name, name, 33);
   newtw->setName(name);
   newtw->setType(WIDGET_TEXT);

   // add to hash
   if(!HUDWidget::AddWidgetToHash(newtw))
   {
      // if addition was unsuccessful, we delete it now
      delete newtw;
      return;
   }

   // set properties
   newtw->initProps(x, y, font, cleartic, flags, message);
}

//=============================================================================
//
// Center-of-screen, Quake-style Message
//

static HUDTextWidget centermessage_widget;

//
// HU_InitCenterMessage
//
// Sets up the center message widget.
//
static void HU_InitCenterMessage()
{
   // set id
   centermessage_widget.setName("_HU_CenterMsgWidget");

   // add to hash
   HUDWidget::AddWidgetToHash(&centermessage_widget);

   // set data
   centermessage_widget.initEmpty();
}

static const char *centermsg_color;

//
// HU_CenterMessage
//
// haleyjd 04/27/04: rewritten to use qstring
//
void HU_CenterMessage(const char *s)
{
   static qstring qstr(128);
   int st_height = GameModeInfo->StatusBar->height;

   qstr.clear();

   // haleyjd 02/28/06: colored center message
   if(centermsg_color)
   {
      qstr += centermsg_color;
      centermsg_color = nullptr;
   }
   
   qstr += s;
  
   centermessage_widget.setMessage(qstr.constPtr(),
      (SCREENWIDTH - V_FontStringWidth(hud_font, s)) / 2,
      (SCREENHEIGHT - V_FontStringHeight(hud_font, s) -
       ((scaledwindow.height == SCREENHEIGHT) ? 0 : st_height - 8)) / 2,
       leveltime + (message_timer * 35) / 1000);
   
   // print message to console also
   C_Printf("%s\n", s);
}

//
// HU_CenterMessageTimed
//
// haleyjd: timed center message. Originally for FraggleScript,
// now revived for Small.
//
static void HU_CenterMessageTimed(const char *s, int tics)
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

//=============================================================================
//
// Crosshair Widget
//
// haleyjd 03/03/07: moved crosshair widget down below centermessage for fix
//

// globals related to the crosshair
int crosshairs[CROSSHAIRS];
byte *targetcolour, *notargetcolour, *friendcolour;
int crosshairnum;       // 0 = none
bool crosshair_hilite; // haleyjd 06/07/05
bool crosshair_scale = true;
const char *cross_str[]= { "none", "cross", "angle" }; // for console

class HUDCrossHairWidget : public HUDPatchWidget
{
public:
   virtual void ticker();
   virtual void drawer();

   void initProps()
   {
      color = notargetcolour;
      patch = nullptr; // determined dynamically
   }
};

static HUDCrossHairWidget crosshair_widget;

//
// HUDCrossHairWidget::ticker
//
// Updates the crosshair on each gametic.
//
void HUDCrossHairWidget::ticker()
{
   // default to no target
   color = notargetcolour;

   // fast as possible: don't bother with this crap if the crosshair 
   // isn't going to be displayed anyway   
   if(!crosshairnum || !crosshair_hilite || crosshairs[crosshairnum-1] == -1)
      return;

   // search for targets
   
   // ioanch 20160101: don't let P_AimLineAttack change global trace.attackrange
   fixed_t oldAttackRange = trace.attackrange;
   Mobj *oldLineTarget = clip.linetarget;
   P_AimLineAttack(players[displayplayer].mo,
                   players[displayplayer].mo->angle, 
                   16*64*FRACUNIT, false);
   trace.attackrange = oldAttackRange;

   if(clip.linetarget)
   {
      // target found
      color = targetcolour; // default
      
      // haleyjd 06/06/09: some special behaviors
      if(clip.linetarget->flags & MF_FRIEND)
         color = friendcolour;

      if(((clip.linetarget->flags  & MF_SHADOW || 
           clip.linetarget->flags3 & MF3_GHOST) && M_Random() & 0x0F) ||
         clip.linetarget->flags4 & MF4_TOTALINVISIBLE)
      {
         color = notargetcolour;
      }
   }

   // Restore previous reference on exit
   if(clip.linetarget != oldLineTarget)
      P_SetTarget(&clip.linetarget, oldLineTarget);
}

//
// HUDCrossHairWidget::drawer
//
// Draws the crosshair patch.
//
void HUDCrossHairWidget::drawer()
{
   VBuffer *buffer;
   int      drawx, drawy, h, w;
   byte    *pal = color;

   // haleyjd 03/03/07: don't display while showing a center message
   if(!crosshairnum || crosshairs[crosshairnum - 1] == -1 ||
      viewcamera || (automapactive && !automap_overlay) || centermessage_widget.cleartic > leveltime)
      return;

   patch = PatchLoader::CacheNum(wGlobalDir, crosshairs[crosshairnum - 1], PU_CACHE);

   // where to draw??
   w = patch->width;
   h = patch->height;

   if(!crosshair_scale)
   {
      drawx  = (video.width  + 1 - w) / 2;
      drawy  = viewwindow.y + (viewwindow.height + 1 - h) / 2;
      buffer = &vbscreenfullres;
   }
   else
   {
      drawx  = (SCREENWIDTH + 1 - w) / 2;
      drawy  = scaledwindow.y + (scaledwindow.height + 1 - h) / 2;
      buffer = &subscreen43;
   }

   if(pal == notargetcolour)
      V_DrawPatchTL(drawx, drawy, buffer, patch, pal, FTRANLEVEL);
   else
      V_DrawPatchTranslated(drawx, drawy, buffer, patch, pal, false);

}

//
// HU_InitCrossHair
//
// Sets up the crosshair widget and associated globals.
//
static void HU_InitCrossHair()
{
   // haleyjd TODO: support user-added crosshairs
   crosshairs[0] = W_CheckNumForName("CROSS1");
   crosshairs[1] = W_CheckNumForName("CROSS2");
   
   notargetcolour = cr_red;
   targetcolour   = cr_green;
   friendcolour   = cr_blue;

   // set up widget object
   crosshair_widget.initProps();

   // set up the object id
   crosshair_widget.setName("_HU_CrosshairWidget");
   crosshair_widget.setType(WIDGET_PATCH);

   // add to hash
   HUDWidget::AddWidgetToHash(&crosshair_widget);
}

//=============================================================================
//
// Elapsed level time (automap)
//

class HUDLevelTimeWidget : public HUDTextWidget
{
public:
   virtual void ticker();
   virtual void clear();

   void initProps()
   {
      if(!(GameModeInfo->flags & GIF_HUDSTATBARNAME))
      {
         x = 240;
         y = 10;
      }
      else
      {
         // haleyjd: use proper font metrics
         x = SCREENWIDTH - 60;
         y = SCREENHEIGHT - ST_HEIGHT - hud_font->absh;
      }
      message  = nullptr;
      font     = hud_font;
      cleartic = 0;
      flags    = TW_AUTOMAP_ONLY;
   }
};

static HUDLevelTimeWidget leveltime_widget;

// haleyjd 02/12/06: configuration variables
bool hu_showtime;       // enable/disable flag for level time
int hu_timecolor;          // color of time text

//
// HUDLevelTimeWidget::ticker
//
// Updates the automap level timer.
//
void HUDLevelTimeWidget::ticker()
{
   static char timestr[32];
   int seconds;
   
   if(!HU_allowMapWidget() || !hu_showtime)
   {
      message = nullptr;
      return;
   }
   
   seconds = leveltime / 35;
   timestr[0] = '\0';
   
   psnprintf(timestr, sizeof(timestr), "%c%02i:%02i:%02i", 
             hu_timecolor + 128, seconds/3600, (seconds%3600)/60, seconds%60);
   
   message = timestr;        
}

//
// HUDLevelTimeWidget::clear
//
// Handles HU_Start actions for the level time widget
//
void HUDLevelTimeWidget::clear()
{
   if(GameModeInfo->flags & GIF_HUDSTATBARNAME && LevelInfo.levelName)
   {
      int len = V_FontStringWidth(hud_font, LevelInfo.levelName);

      // If level name is too long, move the timer up one row;
      // otherwise, restore to the normal position.
      if(len >= x)
         y = SCREENHEIGHT - ST_HEIGHT - (hud_font->absh * 2 + 1);
      else
         y = SCREENHEIGHT - ST_HEIGHT - hud_font->absh;
   }
}

//
// HU_InitLevelTime
//
// Sets up the level time widget for the automap.
//
static void HU_InitLevelTime()
{
   // set id
   leveltime_widget.setName("_HU_LevelTimeWidget");
   leveltime_widget.setType(WIDGET_TEXT);

   // add to hash
   HUDWidget::AddWidgetToHash(&leveltime_widget);

   // set data
   leveltime_widget.initProps();
}

//=============================================================================
//
// Automap level name display
//

class HUDLevelNameWidget : public HUDTextWidget
{
public:
   virtual void ticker();

   void initProps()
   {
      if(!(GameModeInfo->flags & GIF_HUDSTATBARNAME))
      {
         x = 20;
         y = 145;
      }
      else
      {
         // haleyjd: use proper font metrics
         x = 0;
         y = SCREENHEIGHT - ST_HEIGHT - hud_font->absh;
      }
      message  = nullptr;
      font     = hud_font;
      cleartic = 0;
      flags    = TW_AUTOMAP_ONLY;
   }
};

static HUDLevelNameWidget levelname_widget;

// haleyjd 02/12/06: configuration variables
int hu_levelnamecolor;

//
// HUDLevelNameWidget::ticker
//
// Updates the automap level name widget.
//
void HUDLevelNameWidget::ticker()
{
   message = HU_allowMapWidget() ? LevelInfo.levelName : nullptr;
   color   = hu_levelnamecolor + 1;
}

//
// HU_InitLevelName
//
// Sets up the level name widget for the automap.
//
static void HU_InitLevelName()
{
   // set id
   levelname_widget.setName("_HU_LevelNameWidget");
   levelname_widget.setType(WIDGET_TEXT);

   // add to hash
   HUDWidget::AddWidgetToHash(&levelname_widget);

   // set data
   levelname_widget.initProps();
}

//=============================================================================
//
// Chat message display
//

class HUDChatWidget : public HUDTextWidget
{
public:
   virtual void ticker();
   virtual void clear() {}
};

static HUDChatWidget chat_widget;

char chatinput[100] = "";

//
// HUDChatWidget::ticker
//
// Updates the chat message widget.
//
void HUDChatWidget::ticker()
{
   static char tempchatmsg[128];
   
   if(chat_active)
   {
      psnprintf(tempchatmsg, sizeof(tempchatmsg), "%s_", chatinput);
      message = tempchatmsg;
   }
   else
      message = nullptr;
}

//
// HU_InitChat
//
// Sets up the chat message widget.
//
static void HU_InitChat()
{
   // set id
   chat_widget.setName("_HU_ChatWidget");
   chat_widget.setType(WIDGET_TEXT);

   // add to hash
   HUDWidget::AddWidgetToHash(&chat_widget);

   // set data
   chat_widget.initEmpty();
}

//
// HU_ChatRespond
//
// Responds to chat-related events.
//
static bool HU_ChatRespond(const event_t *ev)
{
   char ch = 0;
   static bool shiftdown;
   static bool discardinput = false;

   // haleyjd 06/11/08: get HUD actions
   int action = G_KeyResponder(ev, kac_hud);
   
   if(ev->data1 == KEYD_RSHIFT) 
      shiftdown = (ev->type == ev_keydown);
   (void)shiftdown;

   if(action == ka_frags)
      hu_showfrags = (ev->type == ev_keydown);

   if(ev->type != ev_keydown && ev->type != ev_text)
      return false;

   if(!chat_active)
   {
      if(ev->data1 == key_chat && netgame) 
      {       
         chat_active = true; // activate chat
         chatinput[0] = 0;   // empty input string
         if(ectype::isPrint(ev->data1))
            discardinput = true; // avoid activation key also appearing in input string
         return true;
      }
      return false;
   }

   if(ev->type == ev_text && discardinput)
   {
      discardinput = false;
      return true;
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

   if(ev->type == ev_keydown && ectype::isPrint(ev->data1))
      return true; // eat keydown inputs that have text equivalent

   if(ev->type == ev_text)
      ch = ev->data1;
   
   if(ectype::isPrint(ch))
   {
      psnprintf(chatinput, sizeof(chatinput), "%s%c", chatinput, ch);
      return true;
   }
   return false;
}

//==============================================================================
//
// Automap coordinate display
//
//   Yet Another Lost MBF Feature
//   Restored by Quasar (tm)
//

class HUDCoordWidget : public HUDTextWidget
{
public:
   enum
   {
      COORDTYPE_X,
      COORDTYPE_Y,
      COORDTYPE_Z,
      COORDTYPE_A
   };

protected:
   int coordType;

public:
   virtual void ticker();

   void initProps(int ct)
   {
      coordType = ct;

      message  = nullptr;
      font     = hud_font;
      cleartic = 0;

      x = SCREENWIDTH - 64;
      y = SCREENHEIGHT / 2 + font->absh * (coordType - 2);

      // HTIC_FIXME: Handle through GameModeInfo directly?
      if(GameModeInfo->type == Game_Heretic)
         y -= 42 / 2;
      else
         y -= ST_HEIGHT / 2;
   }
};

static HUDCoordWidget coordx_widget;
static HUDCoordWidget coordy_widget;
static HUDCoordWidget coordz_widget;
static HUDCoordWidget coorda_widget;

// haleyjd 02/12/06: configuration variables
bool hu_alwaysshowcoords;
bool hu_showcoords;
int  hu_coordscolor;

//
// HUDAutomapCoordWidget::ticker
//
// Updates automap coordinate widgets.
//
void HUDCoordWidget::ticker()
{
   player_t *plyr;
   fixed_t x, y, z;
   
   static char coordxstr[16];
   static char coordystr[16];
   static char coordzstr[16];
   static char coordastr[16];

   if(!hu_alwaysshowcoords && (!HU_allowMapWidget() || !hu_showcoords))
   {
      message = nullptr;
      return;
   }
   plyr = &players[displayplayer];

   AM_Coordinates(plyr->mo, x, y, z);

   if(coordType == COORDTYPE_X)
   {
      snprintf(coordxstr, sizeof(coordxstr), "%cX: %-5d", hu_coordscolor + 128, x >> FRACBITS);
      message = coordxstr;
   }
   else if(coordType == COORDTYPE_Y)
   {
      snprintf(coordystr, sizeof(coordystr), "%cY: %-5d", hu_coordscolor + 128, y >> FRACBITS);
      message = coordystr;
   }
   else if(coordType == COORDTYPE_Z)
   {
      snprintf(coordzstr, sizeof(coordzstr), "%cZ: %-5d", hu_coordscolor + 128, z >> FRACBITS);
      message = coordzstr;
   }
   else
   {
      snprintf(coordastr, sizeof(coordastr), "%cA: %-.0f", hu_coordscolor + 128,
              static_cast<double>(plyr->mo->angle) / ANGLE_1);
      message = coordastr;
   }
}

//
// HU_InitCoords
//
// Initializes the automap coordinate widgets.
//
static void HU_InitCoords()
{
   // set ids
   coordx_widget.setName("_HU_CoordXWidget");
   coordy_widget.setName("_HU_CoordYWidget");
   coordz_widget.setName("_HU_CoordZWidget");
   coorda_widget.setName("_HU_CoordAWidget");

   // set types
   coordx_widget.setType(WIDGET_TEXT);
   coordy_widget.setType(WIDGET_TEXT);
   coordz_widget.setType(WIDGET_TEXT);
   coorda_widget.setType(WIDGET_TEXT);

   // add to hash
   HUDWidget::AddWidgetToHash(&coordx_widget);
   HUDWidget::AddWidgetToHash(&coordy_widget);
   HUDWidget::AddWidgetToHash(&coordz_widget);
   HUDWidget::AddWidgetToHash(&coorda_widget);

   // set data
   coordx_widget.initProps(HUDCoordWidget::COORDTYPE_X);
   coordy_widget.initProps(HUDCoordWidget::COORDTYPE_Y);
   coordz_widget.initProps(HUDCoordWidget::COORDTYPE_Z);
   coorda_widget.initProps(HUDCoordWidget::COORDTYPE_A);
}

//==============================================================================
//
// HUD level stats widget
//
// This code is derived from Eternity's Github pull 563 by Joshua Woodie
//

class HUDStatWidget : public HUDTextWidget
{
public:
   enum
   {
      STATTYPE_KILLS,
      STATTYPE_ITEMS,
      STATTYPE_SECRETS
   };

   virtual void ticker();

   void initProps(int st)
   {
      statType = st;

      if(!(GameModeInfo->flags & GIF_HUDSTATBARNAME))
         x = 20;
      else
         x = 0;

      y = 8 + statType * hud_font->absh;

      message  = nullptr;
      font     = hud_font;
      cleartic = 0;
   }

protected:
   int statType;
};

static HUDStatWidget statkill_widget;
static HUDStatWidget statitem_widget;
static HUDStatWidget statsecr_widget;

void HUDStatWidget::ticker()
{
   static char statkillstr[64];
   static char statitemstr[64];
   static char statsecrstr[64];

   if(!HU_allowMapWidget())
   {
      message = nullptr;
      return;
   }

   player_t *plr = &players[displayplayer];

   if(statType == STATTYPE_KILLS)
   {
      snprintf(statkillstr, sizeof(statkillstr), "K: %i/%i", plr->killcount, totalkills);
      message = statkillstr;
   }
   else if(statType == STATTYPE_ITEMS)
   {
      snprintf(statitemstr, sizeof(statitemstr), "I: %i/%i", plr->itemcount, totalitems);
      message = statitemstr;
   }
   else if(statType == STATTYPE_SECRETS && !hud_hidestatus)
   {
      snprintf(statsecrstr, sizeof(statsecrstr), "S: %i/%i", plr->secretcount, totalsecret);
      message = statsecrstr;
   }
   else
   {
      message = nullptr;
   }
}

static void HU_InitStats()
{
   statkill_widget.setName("_HU_StatKillWidget");
   statitem_widget.setName("_HU_StatItemWidget");
   statsecr_widget.setName("_HU_StatSecrWidget");

   statkill_widget.setType(WIDGET_TEXT);
   statitem_widget.setType(WIDGET_TEXT);
   statsecr_widget.setType(WIDGET_TEXT);

   HUDWidget::AddWidgetToHash(&statkill_widget);
   HUDWidget::AddWidgetToHash(&statitem_widget);
   HUDWidget::AddWidgetToHash(&statsecr_widget);

   statkill_widget.initProps(HUDStatWidget::STATTYPE_KILLS);
   statitem_widget.initProps(HUDStatWidget::STATTYPE_ITEMS);
   statsecr_widget.initProps(HUDStatWidget::STATTYPE_SECRETS);
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

const char *align_str[] = { "default", "left", "centered" };

VARIABLE_BOOLEAN(showMessages,  nullptr,                onoff);
VARIABLE_INT(mess_align,        nullptr, 0, 2,             align_str);
VARIABLE_INT(mess_colour,       nullptr, 0, CR_BUILTIN,    textcolours);

VARIABLE_BOOLEAN(obituaries,    nullptr,                onoff);
VARIABLE_INT(obcolour,          nullptr, 0, CR_BUILTIN,    textcolours);
VARIABLE_INT(crosshairnum,      nullptr, 0, CROSSHAIRS-1,  cross_str);
VARIABLE_INT(hud_msg_lines,     nullptr, 0, 14,            nullptr);
VARIABLE_INT(message_timer,     nullptr, 0, 100000,        nullptr);

// haleyjd 02/12/06: lost/new hud options
VARIABLE_TOGGLE(hu_showtime,         nullptr,                yesno);
VARIABLE_TOGGLE(hu_showcoords,       nullptr,                yesno);
VARIABLE_TOGGLE(hu_alwaysshowcoords, nullptr,                yesno);
VARIABLE_INT(hu_timecolor,           nullptr, 0, CR_BUILTIN,    textcolours);
VARIABLE_INT(hu_levelnamecolor,      nullptr, 0, CR_BUILTIN,    textcolours);
VARIABLE_INT(hu_coordscolor,         nullptr, 0, CR_BUILTIN,    textcolours);

VARIABLE_BOOLEAN(hud_msg_scrollup,  nullptr,            yesno);
VARIABLE_TOGGLE(crosshair_hilite,   nullptr,            onoff);
VARIABLE_TOGGLE(crosshair_scale,    nullptr,            onoff);

CONSOLE_VARIABLE(hu_obituaries, obituaries, 0) {}
CONSOLE_VARIABLE(hu_obitcolor, obcolour, 0) {}
CONSOLE_VARIABLE(hu_crosshair, crosshairnum, 0) {}
CONSOLE_VARIABLE(hu_crosshair_hilite, crosshair_hilite, 0) {}
CONSOLE_VARIABLE(hu_crosshair_scale, crosshair_scale, 0) {}
CONSOLE_VARIABLE(hu_messages, showMessages, 0)
{
   doom_printf("%s", DEH_String(showMessages ? "MSGON" : "MSGOFF"));
}
CONSOLE_VARIABLE(hu_messagealignment, mess_align, 0) {}
CONSOLE_VARIABLE(hu_messagecolor, mess_colour, 0) {}
CONSOLE_NETCMD(say, cf_netvar, netcmd_chat)
{
   S_StartInterfaceSound(GameModeInfo->c_ChatSound);
   
   doom_printf("%s: %s", players[Console.cmdsrc].name, Console.args.constPtr());
}

CONSOLE_VARIABLE(hu_messagelines, hud_msg_lines, 0) {}
CONSOLE_VARIABLE(hu_messagescroll, hud_msg_scrollup, 0) {}
CONSOLE_VARIABLE(hu_messagetime, message_timer, 0) {}

// haleyjd 02/12/06: lost/new hud options
CONSOLE_VARIABLE(hu_showtime, hu_showtime, 0) {}
CONSOLE_VARIABLE(hu_showcoords, hu_showcoords, 0) {}
CONSOLE_VARIABLE(hu_alwaysshowcoords, hu_alwaysshowcoords, 0) {}
CONSOLE_VARIABLE(hu_timecolor, hu_timecolor, 0) {}
CONSOLE_VARIABLE(hu_levelnamecolor, hu_levelnamecolor, 0) {}
CONSOLE_VARIABLE(hu_coordscolor, hu_coordscolor, 0) {}

#if 0
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
      efree(name);
      return 0;
   }

   switch(widget->type)
   {
   case WIDGET_TEXT:
      {
         hu_textwidget_t *tw = (hu_textwidget_t *)widget;

         tw->x = params[2];
         tw->y = params[3];
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

   efree(name);
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
      efree(name);
      return -1;
   }
   
   HU_DynamicPatchWidget(name, params[3], params[4], params[5], params[6], patch);

   efree(name);
   efree(patch);

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
      efree(name);
      return -1;
   }

   if((widget = HU_WidgetForName(name)) && widget->type == WIDGET_PATCH)
   {
      pw = (hu_patchwidget_t *)widget;

      // 08/16/06: bug fix -- copy patch name
      strncpy(pw->patchname, patch, 9);

      // pre-cache the patch graphic
      pw->patch = PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE);
   }

   efree(name);
   efree(patch);

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
         pw->color = params[2] >= 0 && params[2] < CR_LIMIT ? colrngs[params[1]] : nullptr;

      if(params[3] >= 0 && params[3] <= FRACUNIT)
         pw->tl_level = params[3];
   }

   efree(name);

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
      efree(name);
      return -1;
   }

   HU_DynamicTextWidget(name, params[3], params[4], params[5], msg,
                        params[6] != 0 ? leveltime + params[6] : 0, params[7]);

   efree(name);
   efree(msg);

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
      char *tempbuf = emalloc(char *, size+1);
      
      tw = (hu_textwidget_t *)widget;

      if(tw->message)
         psnprintf(tempbuf, size+1, "%s", tw->message);
      else
         tempbuf[0] = '\0';
      
      amx_SetString(deststr, tempbuf, packed, 0);

      efree(tempbuf);
   }

   efree(name);

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
      efree(name);
      return -1;
   }

   if((widget = HU_WidgetForName(name)) && widget->type == WIDGET_TEXT)
   {
      tw = (hu_textwidget_t *)widget;

      if(tw->alloc)
         efree(tw->alloc);

      tw->message = tw->alloc = estrdup(value);

      tw->cleartic = params[3] != 0 ? leveltime + params[3] : 0;
   }

   efree(name);
   efree(value);

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
      HU_ToggleWidget(widget, !!params[2]);

   efree(name);

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

   efree(text);

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
   if(hud_enabled && hud_overlaylayout > 0) // Boom HUD enabled, return style
      return (cell)hud_overlaylayout + 1;
   else if(viewwindow.height == video.height)         // Fullscreen (no HUD)
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
   { nullptr, nullptr }
};
#endif

// EOF
