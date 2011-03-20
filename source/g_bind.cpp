// Emacs style mode select -*- C++ -*-
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
//
// Key Bindings
//
// Rather than use the old system of binding a key to an action, we bind
// an action to a key. This way we can have as many actions as we want
// bound to a particular key.
//
// By Simon Howard, Revised by James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomdef.h"
#include "doomstat.h"

#include "am_map.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_main.h"
#include "d_deh.h"
#include "d_gi.h"
#include "g_game.h"
#include "m_argv.h"
#include "mn_engin.h"
#include "mn_misc.h"
#include "m_misc.h"
#include "w_wad.h"
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "g_bind.h"
#include "e_fonts.h"

// Action variables
// These variables are asserted as positive values when the action
// they represent has been performed by the player via key pressing.

// Game Actions -- These are handled in g_game.c

int action_forward;     // forward movement
int action_backward;    // backward movement
int action_left;        // left movement
int action_right;       // right movement
int action_moveleft;    // key-strafe left
int action_moveright;   // key-strafe right
int action_use;         // object activation
int action_speed;       // running
int action_attack;      // firing current weapon
int action_strafe;      // strafe in any direction
int action_flip;        // 180 degree turn
int action_jump;        // jump

int action_mlook;       // mlook activation
int action_lookup;      // key-look up
int action_lookdown;    // key-look down
int action_center;      // key-look centerview

int action_weapon1;     // select weapon 1
int action_weapon2;     // select weapon 2
int action_weapon3;     // select weapon 3
int action_weapon4;     // select weapon 4
int action_weapon5;     // select weapon 5
int action_weapon6;     // select weapon 6
int action_weapon7;     // select weapon 7
int action_weapon8;     // select weapon 8
int action_weapon9;     // select weapon 9

int action_nextweapon;  // toggle to next-favored weapon

int action_weaponup;    // haleyjd: next weapon in order
int action_weapondown;  // haleyjd: prev weapon in order

int action_frags;       // show frags

int action_autorun;     // autorun

// Menu Actions -- handled by MN_Responder

int action_menu_help;
int action_menu_toggle;
int action_menu_setup;
int action_menu_up;
int action_menu_down;
int action_menu_confirm;
int action_menu_previous;
int action_menu_left;
int action_menu_right;
int action_menu_pageup;
int action_menu_pagedown;
int action_menu_contents;

// AutoMap Actions -- handled by AM_Responder

int action_map_toggle;
int action_map_gobig;
int action_map_follow;
int action_map_mark;
int action_map_clear;
int action_map_grid;

// Console Actions -- handled by C_Responder

int action_console_pageup;
int action_console_pagedown;
int action_console_toggle;
int action_console_tab;
int action_console_enter;
int action_console_up;
int action_console_down;
int action_console_backspace;

//
// Handler Functions
//

enum
{
   at_variable,
   at_function,
   at_conscmd,     // console command
};

//
// Actions List
//

typedef struct keyaction_s
{
   char *name;        // text description

   // haleyjd 07/03/04: key binding classes
   int bclass;
   
   int type;
   
   union keyactiondata
   {
      int *variable;  // variable -- if non-zero, action activated (key down)
      binding_handler Handler;
   } value;
   
   struct keyaction_s *next; // haleyjd: used for console bindings

} keyaction_t;

keyaction_t keyactions[NUMKEYACTIONS] =
{
   // Game Actions

   {"forward",   kac_game,       at_variable,     {&action_forward}},
   {"backward",  kac_game,       at_variable,     {&action_backward}},
   {"left",      kac_game,       at_variable,     {&action_left}},
   {"right",     kac_game,       at_variable,     {&action_right}},
   {"moveleft",  kac_game,       at_variable,     {&action_moveleft}},
   {"moveright", kac_game,       at_variable,     {&action_moveright}},
   {"use",       kac_game,       at_variable,     {&action_use}},
   {"strafe",    kac_game,       at_variable,     {&action_strafe}},
   {"attack",    kac_game,       at_variable,     {&action_attack}},
   {"flip",      kac_game,       at_variable,     {&action_flip}},
   {"speed",     kac_game,       at_variable,     {&action_speed}},
   {"jump",      kac_game,       at_variable,     {&action_jump}},      //  -- joek 12/22/07
   {"autorun",   kac_game,       at_variable,     {&action_autorun}},

   {"mlook",     kac_game,       at_variable,     {&action_mlook}},
   {"lookup",    kac_game,       at_variable,     {&action_lookup}},
   {"lookdown",  kac_game,       at_variable,     {&action_lookdown}},
   {"center",    kac_game,       at_variable,     {&action_center}},

   {"weapon1",   kac_game,       at_variable,     {&action_weapon1}},
   {"weapon2",   kac_game,       at_variable,     {&action_weapon2}},
   {"weapon3",   kac_game,       at_variable,     {&action_weapon3}},
   {"weapon4",   kac_game,       at_variable,     {&action_weapon4}},
   {"weapon5",   kac_game,       at_variable,     {&action_weapon5}},
   {"weapon6",   kac_game,       at_variable,     {&action_weapon6}},
   {"weapon7",   kac_game,       at_variable,     {&action_weapon7}},
   {"weapon8",   kac_game,       at_variable,     {&action_weapon8}},
   {"weapon9",   kac_game,       at_variable,     {&action_weapon9}},
   {"nextweapon",kac_game,       at_variable,     {&action_nextweapon}},
   {"weaponup",  kac_game,       at_variable,     {&action_weaponup}},
   {"weapondown",kac_game,       at_variable,     {&action_weapondown}},

   {"frags",     kac_hud,        at_variable,     {&action_frags}},

   // Menu Actions

   {"menu_toggle",       kac_menu,    at_variable,     {&action_menu_toggle}},
   {"menu_help",         kac_menu,    at_variable,     {&action_menu_help}},
   {"menu_setup",        kac_menu,    at_variable,     {&action_menu_setup}},
   {"menu_up",           kac_menu,    at_variable,     {&action_menu_up}},
   {"menu_down",         kac_menu,    at_variable,     {&action_menu_down}},
   {"menu_confirm",      kac_menu,    at_variable,     {&action_menu_confirm}},
   {"menu_previous",     kac_menu,    at_variable,     {&action_menu_previous}},
   {"menu_left",         kac_menu,    at_variable,     {&action_menu_left}},
   {"menu_right",        kac_menu,    at_variable,     {&action_menu_right}},
   {"menu_pageup",       kac_menu,    at_variable,     {&action_menu_pageup}},
   {"menu_pagedown",     kac_menu,    at_variable,     {&action_menu_pagedown}},
   {"menu_contents",     kac_menu,    at_variable,     {&action_menu_contents}},

   // Automap Actions

   {"map_right",         kac_map,     at_function,     {NULL}},
   {"map_left",          kac_map,     at_function,     {NULL}},
   {"map_up",            kac_map,     at_function,     {NULL}},
   {"map_down",          kac_map,     at_function,     {NULL}},
   {"map_zoomin",        kac_map,     at_function,     {NULL}},
   {"map_zoomout",       kac_map,     at_function,     {NULL}},

   {"map_toggle",        kac_map,     at_variable,     {&action_map_toggle}},
   {"map_gobig",         kac_map,     at_variable,     {&action_map_gobig}},
   {"map_follow",        kac_map,     at_variable,     {&action_map_follow}},
   {"map_mark",          kac_map,     at_variable,     {&action_map_mark}},
   {"map_clear",         kac_map,     at_variable,     {&action_map_clear}},
   {"map_grid",          kac_map,     at_variable,     {&action_map_grid}},

   {"console_pageup",    kac_console, at_variable,     {&action_console_pageup}},
   {"console_pagedown",  kac_console, at_variable,     {&action_console_pagedown}},
   {"console_toggle",    kac_console, at_variable,     {&action_console_toggle}},
   {"console_tab",       kac_console, at_variable,     {&action_console_tab}},
   {"console_enter",     kac_console, at_variable,     {&action_console_enter}},
   {"console_up",        kac_console, at_variable,     {&action_console_up}},
   {"console_down",      kac_console, at_variable,     {&action_console_down}},
   {"console_backspace", kac_console, at_variable,     {&action_console_backspace}},
};

const int num_keyactions = sizeof(keyactions) / sizeof(*keyactions);

// Console Bindings
//
// Console bindings are stored seperately and are added to list 
// dynamically.
//
// haleyjd: fraggle used an array of these and reallocated it every
// time it was full. Not exactly model efficiency. I have modified the 
// system to use a linked list.

static keyaction_t *cons_keyactions = NULL;

//
// The actual key bindings
//

#define NUM_KEYS 256

typedef struct doomkey_s
{
   char *name;
   boolean keydown[NUMKEYACTIONCLASSES];
   keyaction_t *bindings[NUMKEYACTIONCLASSES];
} doomkey_t;

static doomkey_t keybindings[NUM_KEYS];

static keyaction_t *G_KeyActionForName(const char *name);

//
// G_InitKeyBindings
//
// Set up key names and various details
//
void G_InitKeyBindings(void)
{
   int i;
   
   // various names for different keys
   
   keybindings[KEYD_RIGHTARROW].name  = "rightarrow";
   keybindings[KEYD_LEFTARROW].name   = "leftarrow";
   keybindings[KEYD_UPARROW].name     = "uparrow";
   keybindings[KEYD_DOWNARROW].name   = "downarrow";
   keybindings[KEYD_ESCAPE].name      = "escape";
   keybindings[KEYD_ENTER].name       = "enter";
   keybindings[KEYD_TAB].name         = "tab";
   
   keybindings[KEYD_F1].name          = "f1";
   keybindings[KEYD_F2].name          = "f2";
   keybindings[KEYD_F3].name          = "f3";
   keybindings[KEYD_F4].name          = "f4";
   keybindings[KEYD_F5].name          = "f5";
   keybindings[KEYD_F6].name          = "f6";
   keybindings[KEYD_F7].name          = "f7";
   keybindings[KEYD_F8].name          = "f8";
   keybindings[KEYD_F9].name          = "f9";
   keybindings[KEYD_F10].name         = "f10";
   keybindings[KEYD_F11].name         = "f11";
   keybindings[KEYD_F12].name         = "f12";
   
   keybindings[KEYD_BACKSPACE].name   = "backspace";
   keybindings[KEYD_PAUSE].name       = "pause";
   keybindings[KEYD_MINUS].name       = "-";
   keybindings[KEYD_RSHIFT].name      = "shift";
   keybindings[KEYD_RCTRL].name       = "ctrl";
   keybindings[KEYD_RALT].name        = "alt";
   keybindings[KEYD_CAPSLOCK].name    = "capslock";
   
   keybindings[KEYD_INSERT].name      = "insert";
   keybindings[KEYD_HOME].name        = "home";
   keybindings[KEYD_END].name         = "end";
   keybindings[KEYD_PAGEUP].name      = "pgup";
   keybindings[KEYD_PAGEDOWN].name    = "pgdn";
   keybindings[KEYD_SCROLLLOCK].name  = "scrolllock";
   keybindings[KEYD_SPACEBAR].name    = "space";
   keybindings[KEYD_NUMLOCK].name     = "numlock";
   keybindings[KEYD_DEL].name         = "delete";
   
   keybindings[KEYD_MOUSE1].name      = "mouse1";
   keybindings[KEYD_MOUSE2].name      = "mouse2";
   keybindings[KEYD_MOUSE3].name      = "mouse3";
   keybindings[KEYD_MOUSE4].name      = "mouse4";
   keybindings[KEYD_MOUSE5].name      = "mouse5";
   keybindings[KEYD_MWHEELUP].name    = "wheelup";
   keybindings[KEYD_MWHEELDOWN].name  = "wheeldown";
   
   keybindings[KEYD_JOY1].name        = "joy1";
   keybindings[KEYD_JOY2].name        = "joy2";
   keybindings[KEYD_JOY3].name        = "joy3";
   keybindings[KEYD_JOY4].name        = "joy4";
   keybindings[KEYD_JOY5].name        = "joy5";
   keybindings[KEYD_JOY6].name        = "joy6";
   keybindings[KEYD_JOY7].name        = "joy7";
   keybindings[KEYD_JOY8].name        = "joy8";

   keybindings[KEYD_KP0].name         = "kp_0";
   keybindings[KEYD_KP1].name         = "kp_1";
   keybindings[KEYD_KP2].name         = "kp_2";
   keybindings[KEYD_KP3].name         = "kp_3";
   keybindings[KEYD_KP4].name         = "kp_4";
   keybindings[KEYD_KP5].name         = "kp_5";
   keybindings[KEYD_KP6].name         = "kp_6";
   keybindings[KEYD_KP7].name         = "kp_7";
   keybindings[KEYD_KP8].name         = "kp_8";
   keybindings[KEYD_KP9].name         = "kp_9";
   keybindings[KEYD_KPPERIOD].name    = "kp_period";
   keybindings[KEYD_KPDIVIDE].name    = "kp_slash";
   keybindings[KEYD_KPMULTIPLY].name  = "kp_star";
   keybindings[KEYD_KPMINUS].name     = "kp_minus";
   keybindings[KEYD_KPPLUS].name      = "kp_plus";
   keybindings[KEYD_KPENTER].name     = "kp_enter";
   keybindings[KEYD_KPEQUALS].name    = "kp_equals";
   
   keybindings[','].name = "<";
   keybindings['.'].name = ">";
   keybindings['`'].name = "tilde";
   
   for(i = 0; i < NUM_KEYS; ++i)
   {
      // fill in name if not set yet
      
      if(!keybindings[i].name)
      {
         char tempstr[32];
         
         // build generic name
         if(isprint(i))
            sprintf(tempstr, "%c", i);
         else
            sprintf(tempstr, "key%02i", i);
         
         keybindings[i].name = Z_Strdup(tempstr, PU_STATIC, 0);
      }
      
      memset(keybindings[i].bindings, 0, 
             NUMKEYACTIONCLASSES * sizeof(keyaction_t *));
   }

   // haleyjd 10/09/05: init callback functions for some keyactions
   keyactions[ka_map_right].value.Handler   = AM_HandlerRight;
   keyactions[ka_map_left].value.Handler    = AM_HandlerLeft;
   keyactions[ka_map_up].value.Handler      = AM_HandlerUp;
   keyactions[ka_map_down].value.Handler    = AM_HandlerDown;
   keyactions[ka_map_zoomin].value.Handler  = AM_HandlerZoomin;
   keyactions[ka_map_zoomout].value.Handler = AM_HandlerZoomout;
}

void G_ClearKeyStates(void)
{
   int i, j;

   for(i = 0; i < NUM_KEYS; ++i)
   {
      for(j = 0; j < NUMKEYACTIONCLASSES; ++j)
      {
         keybindings[i].keydown[j] = false;
            
         if(keybindings[i].bindings[j])
         {
            switch(keybindings[i].bindings[j]->type)
            {
            case at_variable:
               *(keybindings[i].bindings[j]->value.variable) = 0;
               break;
            default:
               break;
            }
         } // end if
      } // end for
   } // end for
}

//
// G_KeyActionForName
//
// Obtain a keyaction from its name
//
static keyaction_t *G_KeyActionForName(const char *name)
{
   int i;
   keyaction_t *prev, *temp, *newaction;
   
   // sequential search
   // this is only called every now and then
   
   for(i = 0; i < num_keyactions; ++i)
   {
      if(!strcasecmp(name, keyactions[i].name))
         return &keyactions[i];
   }
   
   // check console keyactions
   // haleyjd: TOTALLY rewritten to use linked list
      
   if(cons_keyactions)
   {
      temp = cons_keyactions;
      while(temp)
      {
         if(!strcasecmp(name, temp->name))
            return temp;
         
         temp = temp->next;
      }
   }
   else
   {
      // first time only - cons_keyactions was NULL
      cons_keyactions = (keyaction_t *)(Z_Malloc(sizeof(keyaction_t), PU_STATIC, 0));
      cons_keyactions->bclass = kac_cmd;
      cons_keyactions->type = at_conscmd;
      cons_keyactions->name = Z_Strdup(name, PU_STATIC, 0);
      cons_keyactions->next = NULL;
      
      return cons_keyactions;
   }
  
   // not in list -- add
   prev = NULL;
   temp = cons_keyactions;
   while(temp)
   {
      prev = temp;
      temp = temp->next;
   }
   newaction = (keyaction_t *)(Z_Malloc(sizeof(keyaction_t), PU_STATIC, 0));
   newaction->bclass = kac_cmd;
   newaction->type = at_conscmd;
   newaction->name = Z_Strdup(name, PU_STATIC, 0);
   newaction->next = NULL;
   
   if(prev) prev->next = newaction;

   return newaction;
}

//
// G_KeyForName
//
// Obtain a key from its name
//
static int G_KeyForName(const char *name)
{
   int i;
   
   for(i = 0; i < NUM_KEYS; ++i)
   {
      if(!strcasecmp(keybindings[i].name, name))
         return tolower(i);
   }
   
   return -1;
}

//
// G_BindKeyToAction
//
static void G_BindKeyToAction(const char *key_name, const char *action_name)
{
   int key;
   keyaction_t *action;
   
   // get key
   if((key = G_KeyForName(key_name)) < 0)
   {
      C_Printf("unknown key '%s'\n", key_name);
      return;
   }

   // get action   
   if(!(action = G_KeyActionForName(action_name)))
   {
      C_Printf("unknown action '%s'\n", action_name);
      return;
   }

   // haleyjd 07/03/04: support multiple binding classes
   keybindings[key].bindings[action->bclass] = action;
}

//
// G_BoundKeys
//
// Get an ascii description of the keys bound to a particular action
//
const char *G_BoundKeys(const char *action)
{
   int i;
   static char ret[1024];   // store list of keys bound to this   
   keyaction_t *ke;
   
   if(!(ke = G_KeyActionForName(action)))
      return "unknown action";
   
   ret[0] = '\0';   // clear ret
   
   // sequential search -ugh

   // FIXME: buffer overflow possible!
   
   for(i = 0; i < NUM_KEYS; ++i)
   {
      if(keybindings[i].bindings[ke->bclass] == ke)
      {
         if(ret[0])
            strcat(ret, " + ");
         strcat(ret, keybindings[i].name);
      }
   }
   
   return ret[0] ? ret : "none";
}

//
// G_FirstBoundKey
//
// Get an ascii description of the first key bound to a particular 
// action.
//
const char *G_FirstBoundKey(const char *action)
{
   int i;
   static char ret[1024];
   keyaction_t *ke;
   
   if(!(ke = G_KeyActionForName(action)))
      return "unknown action";
   
   ret[0] = '\0';   // clear ret
   
   // sequential search -ugh
   
   for(i = 0; i < NUM_KEYS; ++i)
   {
      if(keybindings[i].bindings[ke->bclass] == ke)
      {
         strcpy(ret, keybindings[i].name);
         break;
      }
   }
   
   return ret[0] ? ret : "none";
}

//
// G_KeyResponder
//
// The main driver function for the entire key binding system
//
boolean G_KeyResponder(event_t *ev, int bclass)
{
   static boolean ctrldown;
   boolean ret = false;

   // do not index out of bounds
   if(ev->data1 >= NUM_KEYS)
      return ret;
   
   if(ev->data1 == KEYD_RCTRL)      // ctrl
      ctrldown = (ev->type == ev_keydown);
   
   if(ev->type == ev_keydown)
   {
      int key = tolower(ev->data1);
      
      if(opensocket &&                 // netgame disconnect binding
         ctrldown && ev->data1 == 'd')
      {
         char buffer[128];
         
         psnprintf(buffer, sizeof(buffer),
                   "disconnect from server?\n\n%s", DEH_String("PRESSYN"));
         MN_Question(buffer, "disconnect leaving");
         
         // dont get stuck thinking ctrl is down
         ctrldown = false;
         return true;
      }

      //if(!keybindings[key].keydown[bclass])
      {
         keybindings[key].keydown[bclass] = true;
                  
         if(keybindings[key].bindings[bclass])
         {
            switch(keybindings[key].bindings[bclass]->type)
            {
            case at_variable:
               *(keybindings[key].bindings[bclass]->value.variable) = 1;
               break;

            case at_function:
               keybindings[key].bindings[bclass]->value.Handler(ev);
               break;
               
            case at_conscmd:
               if(!consoleactive) // haleyjd: not in console.
                  C_RunTextCmd(keybindings[key].bindings[bclass]->name);
               break;
               
            default:
               break;
            }
            ret = true;
         }
      }
   }
   else if(ev->type == ev_keyup)
   {
      int key = tolower(ev->data1);

      keybindings[key].keydown[bclass] = false;

      if(keybindings[key].bindings[bclass])
      {
         switch(keybindings[key].bindings[bclass]->type)
         {
         case at_variable:
            *(keybindings[key].bindings[bclass]->value.variable) = 0;
            break;

         case at_function:
            keybindings[key].bindings[bclass]->value.Handler(ev);
            break;
            
         default:
            break;
         }
         ret = true;
      }
   }

   return ret;
}

//===========================================================================
//
// Binding selection widget
//
// For menu: when we select to change a key binding the widget is used
// as the drawer and responder
//
//===========================================================================

static const char *binding_action; // name of action we are editing

extern vfont_t *menu_font_normal;

//
// G_BindDrawer
//
// Draw the prompt box
//
void G_BindDrawer(void)
{
   const char *msg = "\n -= input new key =- \n";
   int x, y, width, height;
   
   // draw the menu in the background   
   MN_DrawMenu(current_menu);
   
   width  = V_FontStringWidth(menu_font_normal, msg);
   height = V_FontStringHeight(menu_font_normal, msg);
   x = (SCREENWIDTH  - width)  / 2;
   y = (SCREENHEIGHT - height) / 2;
   
   // draw box   
   V_DrawBox(x - 4, y - 4, width + 8, height + 8);

   // write text in box   
   V_FontWriteText(menu_font_normal, msg, x, y);
}

//
// G_BindResponder
//
// Responder for widget
//
boolean G_BindResponder(event_t *ev)
{
   keyaction_t *action;
   
   if(ev->type != ev_keydown)
      return false;

   // do not index out of bounds
   if(ev->data1 >= NUM_KEYS)
      return false;
   
   // got a key - close box
   current_menuwidget = NULL;

   if(action_menu_toggle) // cancel
   {
      action_menu_toggle = false;
      return true;
   }
   
   if(!(action = G_KeyActionForName(binding_action)))
   {
      C_Printf(FC_ERROR "unknown action '%s'\n", binding_action);
      return true;
   }

   // bind new key to action, if it is not already bound -- if it is,
   // remove it
   
   if(keybindings[ev->data1].bindings[action->bclass] != action)
      keybindings[ev->data1].bindings[action->bclass] = action;
   else
      keybindings[ev->data1].bindings[action->bclass] = NULL;

   // haleyjd 10/16/05: clear state of action involved
   keybindings[ev->data1].keydown[action->bclass] = false;
   if(action->type == at_variable)
      *(action->value.variable) = 0;
   
   return true;
}

menuwidget_t binding_widget = { G_BindDrawer, G_BindResponder, NULL, true };

//
// G_EditBinding
//
// Main Function
//
void G_EditBinding(const char *action)
{
   current_menuwidget = &binding_widget;
   binding_action = action;
}

//===========================================================================
//
// Load/Save defaults
//
//===========================================================================

// default script:

static char *cfg_file = NULL; 

void G_LoadDefaults(void)
{
   char *temp = NULL;
   size_t len;
   DWFILE dwfile, *file = &dwfile;

   len = M_StringAlloca(&temp, 1, 18, basegamepath);

   // haleyjd 03/15/03: fix for -cdrom
   // haleyjd 07/03/04: FIXME: doesn't work for linux
   // haleyjd 11/23/06: use basegamepath
   // haleyjd 08/29/09: allow use_doom_config override
#ifdef EE_CDROM_SUPPORT
   if(cdrom_mode)
      psnprintf(temp, len, "%s", "c:/doomdata/keys.csc");
   else
#endif
   {
      if(GameModeInfo->type == Game_DOOM && use_doom_config)
         psnprintf(temp, len, "%s/doom/keys.csc", basepath);
      else
         psnprintf(temp, len, "%s/keys.csc", basegamepath);
   }
   
   cfg_file = strdup(temp);

   if(access(cfg_file, R_OK))
   {
      C_Printf("keys.csc not found, using defaults\n");
      D_OpenLump(file, W_GetNumForName("KEYDEFS"));
   }
   else
      D_OpenFile(file, cfg_file, "r");

   if(!D_IsOpen(file))
      I_Error("G_LoadDefaults: couldn't open default key bindings\n");

   // haleyjd 03/08/06: test for zero length
   if(!D_IsLump(file) && D_FileLength(file) == 0)
   {
      // try the lump because the file is zero-length...
      C_Printf("keys.csc is zero length, trying KEYDEFS\n");
      D_Fclose(file);
      D_OpenLump(file, W_GetNumForName("KEYDEFS"));
      if(!D_IsOpen(file) || D_FileLength(file) == 0)
         I_Error("G_LoadDefaults: KEYDEFS lump is empty\n");
   }

   C_RunScript(file);

   D_Fclose(file);
}

void G_SaveDefaults(void)
{
   FILE *file;
   int i, j;

   if(!cfg_file)         // check defaults have been loaded
      return;
   
   if(!(file = fopen(cfg_file, "w")))
   {
      C_Printf(FC_ERROR"Couldn't open keys.csc for write access.\n");
      return;
   }

  // write key bindings

   for(i = 0; i < NUM_KEYS; ++i)
   {
      // haleyjd 07/03/04: support multiple binding classes
      for(j = 0; j < NUMKEYACTIONCLASSES; ++j)
      {
         if(keybindings[i].bindings[j])
         {
            const char *keyname = keybindings[i].name;

            // haleyjd 07/10/09: semicolon requires special treatment.
            if(keyname[0] == ';')
               keyname = "\";\"";

            fprintf(file, "bind %s \"%s\"\n",
                    keyname,
                    keybindings[i].bindings[j]->name);
         }
      }
   }
   
   fclose(file);
}

//===========================================================================
//
// Console Commands
//
//===========================================================================

CONSOLE_COMMAND(bind, 0)
{
   if(Console.argc >= 2)
   {
      G_BindKeyToAction(QStrConstPtr(&Console.argv[0]), 
                        QStrConstPtr(&Console.argv[1]));
   }
   else if(Console.argc == 1)
   {
      int key = G_KeyForName(QStrConstPtr(&Console.argv[0]));

      if(key < 0)
         C_Printf(FC_ERROR "no such key!\n");
      else
      {
         // haleyjd 07/03/04: multiple binding class support
         int j;
         boolean foundBinding = false;
         
         for(j = 0; j < NUMKEYACTIONCLASSES; ++j)
         {
            if(keybindings[key].bindings[j])
            {
               C_Printf("%s bound to %s (class %d)\n",
                        keybindings[key].name,
                        keybindings[key].bindings[j]->name,
                        keybindings[key].bindings[j]->bclass);
               foundBinding = true;
            }
         }
         if(!foundBinding)
            C_Printf("%s not bound\n", keybindings[key].name);
      }
   }
   else
      C_Printf("usage: bind key action\n");
}

// haleyjd: utility functions
CONSOLE_COMMAND(listactions, 0)
{
   int i;
   
   for(i = 0; i < num_keyactions; ++i)
      C_Printf("%s\n", keyactions[i].name);
}

CONSOLE_COMMAND(listkeys, 0)
{
   int i;

   for(i = 0; i < NUM_KEYS; ++i)
      C_Printf("%s\n", keybindings[i].name);
}

CONSOLE_COMMAND(unbind, 0)
{
   int key;
   int bclass = -1;

   if(Console.argc < 1)
   {
      C_Printf("usage: unbind key [class]\n");
      return;
   }

   // allow specification of a binding class
   if(Console.argc == 2)
   {
      bclass = QStrAtoi(&Console.argv[1]);

      if(bclass < 0 || bclass >= NUMKEYACTIONCLASSES)
      {
         C_Printf(FC_ERROR "invalid action class %d\n", bclass);
         return;
      }
   }
   
   if((key = G_KeyForName(QStrConstPtr(&Console.argv[0]))) != -1)
   {
      if(bclass == -1)
      {
         // unbind all actions
         int j;

         C_Printf("unbound key %s from all actions\n", Console.argv[0]);

         if(keybindings[key].bindings[kac_menu] ||
            keybindings[key].bindings[kac_console])
         {
            C_Printf(FC_ERROR " console and menu actions ignored\n");
         }
         
         for(j = 0; j < NUMKEYACTIONCLASSES; ++j)
         {
            // do not release menu or console actions in this manner
            if(j != kac_menu && j != kac_console)
               keybindings[key].bindings[j] = NULL;
         }
      }
      else
      {
         keyaction_t *ke = keybindings[key].bindings[bclass];

         if(ke)
         {
            C_Printf("unbound key %s from action %s\n", Console.argv[0], ke->name);
            keybindings[key].bindings[bclass] = NULL;
         }
         else
            C_Printf("key %s has no binding in class %d\n", Console.argv[0], bclass);
      }
   }
   else
     C_Printf("unknown key %s\n", Console.argv[0]);
}

CONSOLE_COMMAND(unbindall, 0)
{
   int i, j;
   
   C_Printf("clearing all key bindings\n");
   
   for(i = 0; i < NUM_KEYS; ++i)
   {
      for(j = 0; j < NUMKEYACTIONCLASSES; ++j)
         keybindings[i].bindings[j] = NULL;
   }
}

//
// bindings
//
// haleyjd 12/11/01
// list all active bindings to the console
//
CONSOLE_COMMAND(bindings, 0)
{
   int i, j;

   for(i = 0; i < NUM_KEYS; i++)
   {
      for(j = 0; j < NUMKEYACTIONCLASSES; ++j)
      {
         if(keybindings[i].bindings[j])
         {
            C_Printf("%s : %s\n",
                     keybindings[i].name,
                     keybindings[i].bindings[j]->name);
         }
      }
   }
}

void G_Bind_AddCommands()
{
   C_AddCommand(bind);
   C_AddCommand(listactions);
   C_AddCommand(listkeys);
   C_AddCommand(unbind);
   C_AddCommand(unbindall);
   C_AddCommand(bindings);
}

// EOF
