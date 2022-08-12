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

#include "hal/i_gamepads.h"

#include "am_map.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_dwfile.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_io.h"        // SoM 3/14/2002: strncasecmp
#include "d_main.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "g_bind.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_qstr.h"
#include "m_utils.h"
#include "mn_engin.h"
#include "mn_misc.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

//
// Handler Functions
//

enum
{
   at_variable,
   at_conscmd     // console command
};

//
// Actions List
//

struct keyaction_t
{
   const char *name;  // text description
   int bclass;        // haleyjd 07/03/04: key binding classes
   int type;          // type of action (at_variable or at_conscmd)
   int num;           // unique ID number (for at_variable, index into keyactions)
   keyaction_t *next; // haleyjd: used for console bindings
};

keyaction_t keyactions[NUMKEYACTIONS] =
{
   // Null Action
   { nullptr,             kac_game,    at_variable },

   // Game Actions

   { "forward",           kac_game,    at_variable },
   { "backward",          kac_game,    at_variable },
   { "left",              kac_game,    at_variable },
   { "right",             kac_game,    at_variable },
   { "moveleft",          kac_game,    at_variable },
   { "moveright",         kac_game,    at_variable },
   { "use",               kac_game,    at_variable },
   { "strafe",            kac_game,    at_variable },
   { "attack",            kac_game,    at_variable },
   { "altattack",         kac_game,    at_variable },
   { "reload",            kac_game,    at_variable },
   { "zoom",              kac_game,    at_variable },
   { "user1",             kac_game,    at_variable },
   { "user2",             kac_game,    at_variable },
   { "user3",             kac_game,    at_variable },
   { "user4",             kac_game,    at_variable },
   { "flip",              kac_game,    at_variable },
   { "speed",             kac_game,    at_variable },
   { "jump",              kac_game,    at_variable }, // -- joek 12/22/07
   { "autorun",           kac_game,    at_variable },

   { "mlook",             kac_game,    at_variable },
   { "lookup",            kac_game,    at_variable },
   { "lookdown",          kac_game,    at_variable },
   { "center",            kac_game,    at_variable },

   { "flyup",             kac_game,    at_variable },
   { "flydown",           kac_game,    at_variable },
   { "flycenter",         kac_game,    at_variable },

   { "weapon1",           kac_game,    at_variable },
   { "weapon2",           kac_game,    at_variable },
   { "weapon3",           kac_game,    at_variable },
   { "weapon4",           kac_game,    at_variable },
   { "weapon5",           kac_game,    at_variable },
   { "weapon6",           kac_game,    at_variable },
   { "weapon7",           kac_game,    at_variable },
   { "weapon8",           kac_game,    at_variable },
   { "weapon9",           kac_game,    at_variable },
   { "nextweapon",        kac_game,    at_variable },
   { "weaponup",          kac_game,    at_variable },
   { "weapondown",        kac_game,    at_variable },

   // HUD Actions

   { "frags",             kac_hud,     at_variable },

   // Menu Actions

   { "menu_toggle",       kac_menu,    at_variable },
   { "menu_help",         kac_menu,    at_variable },
   { "menu_setup",        kac_menu,    at_variable },
   { "menu_up",           kac_menu,    at_variable },
   { "menu_down",         kac_menu,    at_variable },
   { "menu_confirm",      kac_menu,    at_variable },
   { "menu_previous",     kac_menu,    at_variable },
   { "menu_left",         kac_menu,    at_variable },
   { "menu_right",        kac_menu,    at_variable },
   { "menu_pageup",       kac_menu,    at_variable },
   { "menu_pagedown",     kac_menu,    at_variable },
   { "menu_contents",     kac_menu,    at_variable },

   // Automap Actions

   { "map_right",         kac_map,     at_variable },
   { "map_left",          kac_map,     at_variable },
   { "map_up",            kac_map,     at_variable },
   { "map_down",          kac_map,     at_variable },
   { "map_zoomin",        kac_map,     at_variable },
   { "map_zoomout",       kac_map,     at_variable },
   { "map_toggle",        kac_map,     at_variable },
   { "map_gobig",         kac_map,     at_variable },
   { "map_follow",        kac_map,     at_variable },
   { "map_mark",          kac_map,     at_variable },
   { "map_clear",         kac_map,     at_variable },
   { "map_grid",          kac_map,     at_variable },
   { "map_overlay",       kac_map,     at_variable },

   // Console Actions

   { "console_pageup",    kac_console, at_variable },
   { "console_pagedown",  kac_console, at_variable },
   { "console_toggle",    kac_console, at_variable },
   { "console_tab",       kac_console, at_variable },
   { "console_enter",     kac_console, at_variable },
   { "console_up",        kac_console, at_variable },
   { "console_down",      kac_console, at_variable },
   { "console_backspace", kac_console, at_variable },

   // Inventory Actions

   { "inventory_left",     kac_game,    at_variable },
   { "inventory_right",    kac_game,    at_variable },
   { "inventory_use",      kac_game,    at_variable },
   { "inventory_drop",     kac_game,    at_variable },

   // Demos

   { "joindemo",          kac_game,    at_variable },
};

// Console Bindings
//
// Console bindings are stored seperately and are added to list 
// dynamically.
//
// haleyjd: fraggle used an array of these and reallocated it every
// time it was full. Not exactly model efficiency. I have modified the 
// system to use a linked list.

static keyaction_t *cons_keyactions = nullptr;

//
// The actual key bindings
//

struct doomkey_t
{
   const char *name;
   bool keydown[NUMKEYACTIONCLASSES];
   keyaction_t *bindings[NUMKEYACTIONCLASSES];
};

static doomkey_t keybindings[NUMKEYS];
// Counts how many keys per class are being pressed
static int gGameKeyCount[NUMKEYACTIONCLASSES][NUMKEYACTIONS];

static keyaction_t *G_KeyActionForName(const char *name);

// Some keys have special names. Ones not listed here have a default name
// generated for them at startup.
struct keyname_t
{
   int keyCode;
   const char *name;
} keyNames[] =
{
   { KEYD_COMMA,      "<"          },
   { KEYD_PERIOD,     ">"          },
   { KEYD_ACCGRAVE,   "tilde"      },
   { KEYD_RIGHTARROW, "rightarrow" },
   { KEYD_LEFTARROW,  "leftarrow"  },
   { KEYD_UPARROW,    "uparrow"    },
   { KEYD_DOWNARROW,  "downarrow"  },
   { KEYD_ESCAPE,     "escape"     },
   { KEYD_ENTER,      "enter"      },
   { KEYD_TAB,        "tab"        },
   { KEYD_F1,         "f1"         },
   { KEYD_F2,         "f2"         },
   { KEYD_F3,         "f3"         },
   { KEYD_F4,         "f4"         },
   { KEYD_F5,         "f5"         },
   { KEYD_F6,         "f6"         },
   { KEYD_F7,         "f7"         },
   { KEYD_F8,         "f8"         },
   { KEYD_F9,         "f9"         },
   { KEYD_F10,        "f10"        },
   { KEYD_F11,        "f11"        },
   { KEYD_F12,        "f12"        },
   { KEYD_BACKSPACE,  "backspace"  },
   { KEYD_NONUSBACKSLASH, "iso key 1" },
   { KEYD_NONUSHASH,  "iso key 2"  },
   { KEYD_PAUSE,      "pause"      },
   { KEYD_MINUS,      "-"          },
   { KEYD_RSHIFT,     "shift"      },
   { KEYD_RCTRL,      "ctrl"       },
   { KEYD_RALT,       "alt"        },
   { KEYD_CAPSLOCK,   "capslock"   },
   { KEYD_INSERT,     "insert"     },
   { KEYD_HOME,       "home"       },
   { KEYD_END,        "end"        },
   { KEYD_PAGEUP,     "pgup"       },
   { KEYD_PAGEDOWN,   "pgdn"       },
   { KEYD_PRINTSCREEN, "printscreen" },
   { KEYD_SCROLLLOCK, "scrolllock" },
   { KEYD_SPACEBAR,   "space"      },
   { KEYD_NUMLOCK,    "numlock"    },
   { KEYD_DEL,        "delete"     },
   { KEYD_MOUSE1,     "mouse1"     },
   { KEYD_MOUSE2,     "mouse2"     },
   { KEYD_MOUSE3,     "mouse3"     },
   { KEYD_MOUSE4,     "mouse4"     },
   { KEYD_MOUSE5,     "mouse5"     },
   { KEYD_MOUSE6,     "mouse6"     },
   { KEYD_MOUSE7,     "mouse7"     },
   { KEYD_MOUSE8,     "mouse8"     },
   { KEYD_MWHEELUP,   "wheelup"    },
   { KEYD_MWHEELDOWN, "wheeldown"  },
   { KEYD_KP0,        "kp_0"       },
   { KEYD_KP1,        "kp_1"       },
   { KEYD_KP2,        "kp_2"       },
   { KEYD_KP3,        "kp_3"       },
   { KEYD_KP4,        "kp_4"       },
   { KEYD_KP5,        "kp_5"       },
   { KEYD_KP6,        "kp_6"       },
   { KEYD_KP7,        "kp_7"       },
   { KEYD_KP8,        "kp_8"       },
   { KEYD_KP9,        "kp_9"       },
   { KEYD_KPPERIOD,   "kp_period"  },
   { KEYD_KPDIVIDE,   "kp_slash"   },
   { KEYD_KPMULTIPLY, "kp_star"    },
   { KEYD_KPMINUS,    "kp_minus"   },
   { KEYD_KPPLUS,     "kp_plus"    },
   { KEYD_KPENTER,    "kp_enter"   },
   { KEYD_KPEQUALS,   "kp_equals"  },
   { KEYD_JOY01,      "joy1"       },
   { KEYD_JOY02,      "joy2"       },
   { KEYD_JOY03,      "joy3"       },
   { KEYD_JOY04,      "joy4"       },
   { KEYD_JOY05,      "joy5"       },
   { KEYD_JOY06,      "joy6"       },
   { KEYD_JOY07,      "joy7"       },
   { KEYD_JOY08,      "joy8"       },
   { KEYD_JOY09,      "joy9"       },
   { KEYD_JOY10,      "joy10"      },
   { KEYD_JOY11,      "joy11"      },
   { KEYD_JOY12,      "joy12"      },
   { KEYD_JOY13,      "joy13"      },
   { KEYD_JOY14,      "joy14"      },
   { KEYD_JOY15,      "joy15"      },
   { KEYD_JOY16,      "joy16"      },
   { KEYD_HAT1RIGHT,  "hat1right"  },
   { KEYD_HAT1UP,     "hat1up"     },
   { KEYD_HAT1LEFT,   "hat1left"   },
   { KEYD_HAT1DOWN,   "hat1down"   },
   { KEYD_HAT2RIGHT,  "hat2right"  },
   { KEYD_HAT2UP,     "hat2up"     },
   { KEYD_HAT2LEFT,   "hat2left"   },
   { KEYD_HAT2DOWN,   "hat2down"   },
   { KEYD_HAT3RIGHT,  "hat3right"  },
   { KEYD_HAT3UP,     "hat3up"     },
   { KEYD_HAT3LEFT,   "hat3left"   },
   { KEYD_HAT3DOWN,   "hat3down"   },
   { KEYD_HAT4RIGHT,  "hat4right"  },
   { KEYD_HAT4UP,     "hat4up"     },
   { KEYD_HAT4LEFT,   "hat4left"   },
   { KEYD_HAT4DOWN,   "hat4down"   },
   { KEYD_AXISON01,   "axis1"      },
   { KEYD_AXISON02,   "axis2"      },
   { KEYD_AXISON03,   "axis3"      },
   { KEYD_AXISON04,   "axis4"      },
   { KEYD_AXISON05,   "axis5"      },
   { KEYD_AXISON06,   "axis6"      },
   { KEYD_AXISON07,   "axis7"      },
   { KEYD_AXISON08,   "axis8"      },
};

//
// G_InitKeyBindings
//
// Set up key names and various details
//
void G_InitKeyBindings()
{
   // various names for different keys
   for(size_t kn = 0; kn < earrlen(keyNames); kn++)
      keybindings[keyNames[kn].keyCode].name = keyNames[kn].name;

   for(int i = 0; i < NUMKEYS; i++)
   {
      // fill in name if not set yet

      if(!keybindings[i].name)
      {
         char tempstr[32];

         // build generic name
         if(ectype::isPrint(i))
            sprintf(tempstr, "%c", i);
         else
            sprintf(tempstr, "key%x", i);

         keybindings[i].name = Z_Strdup(tempstr, PU_STATIC, nullptr);
      }

      memset(keybindings[i].bindings, 0, NUMKEYACTIONCLASSES * sizeof(keyaction_t *));
   }

   for(int i = 0; i < NUMKEYACTIONS; i++)
      keyactions[i].num = i;
}

void G_ClearKeyStates()
{
   for(doomkey_t &keybinding : keybindings)
   {
      for(bool &curkey : keybinding.keydown)
         curkey = false;
   }
   memset(gGameKeyCount, 0, sizeof(gGameKeyCount));
}

//
// G_KeyActionForName
//
// Obtain a keyaction from its name
//
static keyaction_t *G_KeyActionForName(const char *name)
{
   static int cons_actionnum = NUMKEYACTIONS;
   keyaction_t *prev, *temp, *newaction;

   // sequential search
   // this is only called every now and then

   for(keyaction_t &keyaction : keyactions)
   {
      if(!keyaction.name)
         continue;
      if(!strcasecmp(name, keyaction.name))
         return &keyaction;
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
      // first time only - cons_keyactions was nullptr
      cons_keyactions = estructalloc(keyaction_t, 1);
      cons_keyactions->bclass = kac_cmd;
      cons_keyactions->type   = at_conscmd;
      cons_keyactions->name   = Z_Strdup(name, PU_STATIC, nullptr);
      cons_keyactions->next   = nullptr;
      cons_keyactions->num    = cons_actionnum++;

      return cons_keyactions;
   }

   // not in list -- add
   prev = nullptr;
   temp = cons_keyactions;
   while(temp)
   {
      prev = temp;
      temp = temp->next;
   }
   newaction = estructalloc(keyaction_t, 1);
   newaction->bclass = kac_cmd;
   newaction->type = at_conscmd;
   newaction->name = Z_Strdup(name, PU_STATIC, nullptr);
   newaction->next = nullptr;
   newaction->num  = cons_actionnum++;

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
   for(int i = 0; i < NUMKEYS; i++)
   {
      if(!strcasecmp(keybindings[i].name, name))
         return ectype::toLower(i);
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
void G_BoundKeys(const char *action, qstring &outstr)
{
   keyaction_t *ke;

   if(!(ke = G_KeyActionForName(action)))
   {
      outstr = "unknown action";
      return;
   }

   // sequential search -ugh
   for(doomkey_t &keybinding : keybindings)
   {
      if(keybinding.bindings[ke->bclass] == ke)
      {
         if(outstr.length() > 0)
            outstr += " + ";
         outstr += keybinding.name;
      }
   }

   if(!outstr.length())
      outstr = "none";
}

//
// G_FirstBoundKey
//
// Get an ascii description of the first key bound to a particular 
// action.
//
const char *G_FirstBoundKey(const char *action)
{
   static char ret[1024];
   keyaction_t *ke;

   if(!(ke = G_KeyActionForName(action)))
      return "unknown action";

   ret[0] = '\0';   // clear ret

   // sequential search -ugh

   for(doomkey_t &keybinding : keybindings)
   {
      if(keybinding.bindings[ke->bclass] == ke)
      {
         strcpy(ret, keybinding.name);
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
int G_KeyResponder(const event_t *ev, int bclass, bool *allreleased)
{
   int ret = ka_nothing;
   keyaction_t *action = nullptr;

   if(allreleased)
      *allreleased = false;

   // do not index out of bounds
   if(ev->data1 >= NUMKEYS)
      return ret;
   
   if(ev->type == ev_keydown)
   {
      int key = ectype::toLower(ev->data1);

      bool wasdown = keybindings[key].keydown[bclass];
      keybindings[key].keydown[bclass] = true;

      if((action = keybindings[key].bindings[bclass]))
      {
         switch(keybindings[key].bindings[bclass]->type)
         {
         case at_conscmd:
            if(!consoleactive) // haleyjd: not in console.
               C_RunTextCmd(keybindings[key].bindings[bclass]->name);
            break;

         default:
            break;
         }

         ret = action->num;
         if(!wasdown && ret < NUMKEYACTIONS)
            ++gGameKeyCount[bclass][ret];
      }
   }
   else if(ev->type == ev_keyup)
   {
      int key = ectype::toLower(ev->data1);

      bool wasdown = keybindings[key].keydown[bclass];
      keybindings[key].keydown[bclass] = false;

      if((action = keybindings[key].bindings[bclass]))
      {
         ret = action->num;
         if(ret < NUMKEYACTIONS)
         {
            if(wasdown)
               --gGameKeyCount[bclass][ret];
            if(allreleased && !gGameKeyCount[bclass][ret])
               *allreleased = true;
         }
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

static const char *binding_action; // name of action we are editing

extern vfont_t *menu_font_normal;

//
// G_BindDrawer
//
// Draw the prompt box
//
static void G_BindDrawer()
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
   V_FontWriteText(menu_font_normal, msg, x, y, &subscreen43);
}

//
// G_BindResponder
//
// Responder for widget
//
static bool G_BindResponder(event_t *ev, int mnaction)
{
   keyaction_t *action;
   
   if(ev->type != ev_keydown)
      return false;

   // do not index out of bounds
   if(ev->data1 >= NUMKEYS)
      return false;
   
   // got a key - close box
   MN_PopWidget();

   if(mnaction == ka_menu_toggle) // cancel
      return true;
   
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
      keybindings[ev->data1].bindings[action->bclass] = nullptr;

   // haleyjd 10/16/05: clear state of action involved
   bool wasdown = keybindings[ev->data1].keydown[action->bclass];
   keybindings[ev->data1].keydown[action->bclass] = false;
   if(wasdown && action->num < NUMKEYACTIONS)
      --gGameKeyCount[action->bclass][action->num];
   
   return true;
}

menuwidget_t binding_widget = { G_BindDrawer, G_BindResponder, nullptr, true };

//
// G_EditBinding
//
// Main Function
//
void G_EditBinding(const char *action)
{
   MN_PushWidget(&binding_widget);
   binding_action = action;
}

//===========================================================================
//
// Analog Axis Binding
//

// Axis action bindings
int axisActions[HALGamePad::MAXAXES];

// Axis orientations. This should SO not be necessary.
int axisOrientation[HALGamePad::MAXAXES];

// Names for axis actions
static const char *axisActionNames[axis_max] =
{
   "none",
   "move",
   "strafe",
   "turn",
   "look",
   "fly",
};

//
// G_CreateAxisActionVars
//
// Build console variables for all axis action bindings.
//
void G_CreateAxisActionVars()
{
   for(int i = 0; i < HALGamePad::MAXAXES; i++)
   {
      qstring     name;
      variable_t *variable;
      command_t  *command;

      variable = estructalloc(variable_t, 1);
      variable->variable  = &axisActions[i];
      variable->v_default = nullptr;
      variable->type      = vt_int;
      variable->min       = axis_none;
      variable->max       = axis_max - 1;
      variable->defines   = axisActionNames;

      command = estructalloc(command_t, 1);
      name << "g_axisaction" << i+1;
      command->name     = name.duplicate();
      command->type     = ct_variable;
      command->variable = variable;

      C_AddCommand(command);

      variable = estructalloc(variable_t, 1);
      variable->variable  = &axisOrientation[i];
      variable->v_default = nullptr;
      variable->type      = vt_int;
      variable->min       = -1;
      variable->max       =  1;
      static const char *orientationDefines[] = { "reversed", "default", "normal" };
      variable->defines = orientationDefines;

      command = estructalloc(command_t, 1);
      name.clear() << "g_axisorientation" << i+1;
      command->name     = name.duplicate();
      command->type     = ct_variable;
      command->variable = variable;

      C_AddCommand(command);
   }
}

//===========================================================================
//
// Gamepad Profiles
//

//
// G_clearGamepadBindings
//
// Clears out all gamepad button and axis bindings in order to reset the
// control scheme to a default state.
//
static void G_clearGamepadBindings()
{
   // clear button bindings
   for(int key = 0; key < HALGamePad::MAXBUTTONS; key++)
   {
      int vkc = KEYD_JOY01 + key;

      for(keyaction_t *&binding : keybindings[vkc].bindings)
         binding = nullptr;
   }

   // clear hats
   for(int hat = 0; hat < HALGamePad::MAXHATS; hat++)
   {
      int vkc = KEYD_HAT1RIGHT + 4 * hat;
      for(int i = 0; i < 4; ++i)
         for(keyaction_t *&binding : keybindings[vkc + i].bindings)
            binding = nullptr;
   }

   // clear axis actions, orientations, and trigger bindings
   for(int axis = 0; axis < HALGamePad::MAXAXES; axis++)
   {
      axisActions[axis] = axis_none;
      axisOrientation[axis] = 0;     // zero is agnostic

      int vkc = KEYD_AXISON01 + axis;

      for(keyaction_t *&binding : keybindings[vkc].bindings)
         binding = nullptr;
   }
}

//
// G_profileForName
//
// Look up a gamepad profile by name
//
static int G_profileForName(const char *name)
{
   qstring lfn;
   lfn << "gamepads/" << name;
   lfn.addDefaultExtension(".txt");
   return wGlobalDir.checkNumForLFN(lfn.constPtr(), lumpinfo_t::ns_pads);
}

//
// G_ExecuteGamepadProfile
//
// haleyjd 05/11/13: Run a console script that can setup default bindings
// for a particular model/brand of gamepad.
//
bool G_ExecuteGamepadProfile(const char *name)
{
   int lumpnum;

   if((lumpnum = G_profileForName(name)) >= 0)
   {
      DWFILE dwfile;

      dwfile.openLump(lumpnum);

      if(dwfile.isOpen())
      {
         G_clearGamepadBindings();
         C_RunScript(&dwfile);
         return true;
      }
   }

   return false;
}

CONSOLE_COMMAND(g_padprofile, 0)
{
   if(Console.argc < 1)
   {
      C_Printf("Usage: g_padprofile profilename\n");
      return;
   }

   G_ExecuteGamepadProfile(Console.argv[0]->constPtr());
}

//===========================================================================
//
// Load/Save defaults
//

// default script:

static char *cfg_file = nullptr; 

void G_LoadDefaults()
{
   qstring temp;
   DWFILE dwfile;

   // haleyjd 11/23/06: use basegamepath
   // haleyjd 08/29/09: allow use_doom_config override
   if(GameModeInfo->type == Game_DOOM && use_doom_config)
   {
      temp = userpath;
      temp.pathConcatenate("doom/keys.csc");
   }
   else
   {
      temp = usergamepath;
      temp.pathConcatenate("keys.csc");
   }

   cfg_file = temp.duplicate();

   if(access(cfg_file, R_OK))
   {
      C_Printf("keys.csc not found, using defaults\n");
      dwfile.openLump(W_GetNumForName("KEYDEFS"));
   }
   else
      dwfile.openFile(cfg_file, "r");

   if(!dwfile.isOpen())
      I_Error("G_LoadDefaults: couldn't open default key bindings\n");

   // haleyjd 03/08/06: test for zero length
   if(!dwfile.isLump() && dwfile.fileLength() == 0)
   {
      // try the lump because the file is zero-length...
      C_Printf("keys.csc is zero length, trying KEYDEFS\n");
      dwfile.openLump(W_GetNumForName("KEYDEFS"));
      if(!dwfile.isOpen() || dwfile.fileLength() == 0)
         I_Error("G_LoadDefaults: KEYDEFS lump is empty\n");
   }

   C_RunScript(&dwfile);
}

void G_SaveDefaults()
{
   FILE *file;

   if(!cfg_file)         // check defaults have been loaded
      return;

   if(!(file = fopen(cfg_file, "w")))
   {
      C_Printf(FC_ERROR "Couldn't open keys.csc for write access.\n");
      return;
   }

  // write key bindings

   for(doomkey_t &keybinding : keybindings)
   {
      // haleyjd 07/03/04: support multiple binding classes
      for(keyaction_t *&binding : keybinding.bindings)
      {
         if(binding)
         {
            const char *keyname = keybinding.name;

            // haleyjd 07/10/09: semicolon requires special treatment.
            if(keyname[0] == ';')
               keyname = "\";\"";

            fprintf(file, "bind %s \"%s\"\n", keyname, binding->name);
         }
      }
   }

   // write axis actions and orientations

   for(int i = 0; i < HALGamePad::MAXAXES; i++)
   {
      if(axisActions[i] != axis_none)
      {
         fprintf(file, "g_axisaction%d %s\n",
                 i+1, axisActionNames[axisActions[i]]);
      }
      if(axisOrientation[i] != 0)
      {
         fprintf(file, "g_axisorientation%d %d\n",
                 i+1, axisOrientation[i]);
      }
   }

   fclose(file);
}

//===========================================================================
//
// Console Commands
//

CONSOLE_COMMAND(bind, 0)
{
   if(Console.argc >= 2)
   {
      G_BindKeyToAction(Console.argv[0]->constPtr(),
                        Console.argv[1]->constPtr());
   }
   else if(Console.argc == 1)
   {
      int key = G_KeyForName(Console.argv[0]->constPtr());

      if(key < 0)
         C_Printf(FC_ERROR "no such key!\n");
      else
      {
         // haleyjd 07/03/04: multiple binding class support
         bool foundBinding = false;

         for(keyaction_t *&binding : keybindings[key].bindings)
         {
            if(binding)
            {
               C_Printf("%s bound to %s (class %d)\n",
                        keybindings[key].name,
                        binding->name,
                        binding->bclass);
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
   for(keyaction_t &keyaction : keyactions)
   {
      if(keyaction.name)
         C_Printf("%s\n", keyaction.name);
   }
}

CONSOLE_COMMAND(listkeys, 0)
{
   for(doomkey_t &keybinding : keybindings)
      C_Printf("%s\n", keybinding.name);
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
      bclass = Console.argv[1]->toInt();

      if(bclass < 0 || bclass >= NUMKEYACTIONCLASSES)
      {
         C_Printf(FC_ERROR "invalid action class %d\n", bclass);
         return;
      }
   }
   
   if((key = G_KeyForName(Console.argv[0]->constPtr())) != -1)
   {
      if(bclass == -1)
      {
         // unbind all actions
         int j;

         C_Printf("unbound key %s from all actions\n", Console.argv[0]->constPtr());

         if(keybindings[key].bindings[kac_menu] ||
            keybindings[key].bindings[kac_console])
         {
            C_Printf(FC_ERROR " console and menu actions ignored\n");
         }

         for(j = 0; j < NUMKEYACTIONCLASSES; j++)
         {
            // do not release menu or console actions in this manner
            if(j != kac_menu && j != kac_console)
               keybindings[key].bindings[j] = nullptr;
         }
      }
      else
      {
         keyaction_t *ke = keybindings[key].bindings[bclass];

         if(ke)
         {
            C_Printf("unbound key %s from action %s\n", Console.argv[0]->constPtr(), ke->name);
            keybindings[key].bindings[bclass] = nullptr;
         }
         else
            C_Printf("key %s has no binding in class %d\n", Console.argv[0]->constPtr(), bclass);
      }
   }
   else
      C_Printf("unknown key %s\n", Console.argv[0]->constPtr());
}

CONSOLE_COMMAND(unbindall, 0)
{
   C_Printf("clearing all key bindings\n");

   for(doomkey_t &keybinding : keybindings)
   {
      for(keyaction_t *&binding : keybinding.bindings)
         binding = nullptr;
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
   for(doomkey_t &keybinding : keybindings)
   {
      for(keyaction_t *&binding : keybinding.bindings)
      {
         if(binding)
         {
            C_Printf("%s : %s\n",
                     keybinding.name,
                     binding->name);
         }
      }
   }
}

// EOF
