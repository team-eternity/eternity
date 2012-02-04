// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
// Rewritten to support press/release/activate/deactivate only bindings,
// as well as to allow a key to be bound to multiple actions in the same
// key action category.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#include "z_zone.h"

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
#include "e_hash.h"
#include "g_bind.h"
#include "g_game.h"
#include "i_system.h"
#include "m_argv.h"
#include "mn_engin.h"
#include "mn_misc.h"
#include "m_misc.h"
#include "psnprntf.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

extern vfont_t *menu_font_normal;

#define NUM_KEYS 256
#define MAX_LOAD_FACTOR 0.7
#define INITIAL_KEY_ACTION_CHAIN_SIZE 128

#define G_addKey(number, name) \
   keys[number] = new InputKey(number, #name); \
   names_to_keys.addObject(*keys[number])

#define G_addVariableAction(n, c) \
   names_to_actions.addObject(new VariableInputAction(#n, c, &action_##n))

#define G_addFunctionAction(n, c, f) \
   names_to_actions.addObject(new FunctionInputAction(#n, c, &f))

#define G_addCommandAction(n) \
   names_to_actions.addObject(new CommandInputAction(n))

#define G_addRepeatableVariableAction(n, c) \
   names_to_actions.addObject(new VariableInputAction(#n, c, &action_##n, true))

#define G_addRepeatableFunctionAction(n, c, f) \
   names_to_actions.addObject(new FunctionInputAction(#n, c, &f, true))

#define G_addRepeatableCommandAction(n) \
   names_to_actions.addObject(new CommandInputAction(n, true))

class KeyBind : public ZoneObject
{
private:
   char *hidden_key_name;
   char *hidden_action_name;
   bool repeatable;
   bool pressed;
   unsigned short flags;
   input_action_category_e category;

public:
   static short const activate_only   = 1; // key only activates the action
   static short const deactivate_only = 2; // key only deactivates the action
   static short const press_only      = 4; // only triggered by a key press
   static short const release_only    = 8; // only triggered by a key release

   DLListItem<KeyBind> key_to_action_links;
   DLListItem<KeyBind> action_to_key_links;
   DLListItem<KeyBind> unbind_links;

   ENCStringHashKey keys_to_actions_key;
   ENCStringHashKey actions_to_keys_key;

   KeyBind(const char *new_key_name, const char *new_action_name,
           unsigned short new_flags)
      : ZoneObject(), flags(new_flags), repeatable(false), pressed(false)
   {
      hidden_key_name = estrdup(new_key_name);
      hidden_action_name = estrdup(new_action_name);
      keys_to_actions_key = (const char *)hidden_key_name;
      actions_to_keys_key = (const char *)hidden_action_name;
   }

   KeyBind(const char *new_key_name, const char *new_action_name,
           unsigned short new_flags, bool new_repeatable)
      : ZoneObject(), flags(new_flags), repeatable(new_repeatable),
        pressed(false)
   {
      hidden_key_name = estrdup(new_key_name);
      hidden_action_name = estrdup(new_action_name);
      keys_to_actions_key = (const char *)hidden_key_name;
      actions_to_keys_key = (const char *)hidden_action_name;
   }

   ~KeyBind()
   {
      efree(hidden_key_name);
      efree(hidden_action_name);
   }

   char* getKeyName() const { return hidden_key_name; }
   char* getActionName() const { return hidden_action_name; }
   bool isRepeatable() const { return repeatable; }
   bool isPressed() const { return pressed; }
   void press() { pressed = true; }
   void release() { pressed = false; }
   input_action_category_e getCategory() const { return category; }
   void setCategory(input_action_category_e new_cat) { category = new_cat; }
   unsigned short getFlags() const { return flags; }

   bool isNormal()
   {
      if(flags == 0)
         return true;

      return false;
   }

   bool isActivateOnly()
   {
      if(flags & activate_only)
         return true;

      return false;
   }

   bool isDeactivateOnly()
   {
      if(flags & deactivate_only)
         return true;

      return false;
   }

   bool isPressOnly()
   {
      if(flags & press_only)
         return true;

      return false;
   }

   bool isReleaseOnly()
   {
      if(flags & release_only)
         return true;

      return false;
   }
};

class InputKey : public ZoneObject
{
private:
   char *hidden_name;
   int number;
public:
   DLListItem<InputKey> links;
   ENCStringHashKey key;

   InputKey(int new_number, const char *new_name)
      : ZoneObject()
   {
      number = new_number;
      hidden_name = estrdup(new_name);
      key = (const char *)hidden_name;
   }

   ~InputKey() { efree(hidden_name); }

   char* getName() const { return hidden_name; }
   int getNumber() const { return number; }
};

class InputAction : public ZoneObject
{
protected:
   char *hidden_name;
   input_action_category_e category;
   bool repeatable;
   char *bound_keys_description;
public:
   DLListItem<InputAction> links;
   ENCStringHashKey key;

   InputAction(const char *new_name, input_action_category_e new_category)
      : ZoneObject(), category(new_category), repeatable(false)
   {
      hidden_name = estrdup(new_name);
      key = hidden_name;
      bound_keys_description = estrdup("none");
   }

   InputAction(const char *new_name, input_action_category_e new_category,
               bool new_repeatable)
      : ZoneObject(), category(new_category), repeatable(new_repeatable)
   {
      hidden_name = estrdup(new_name);
      key = hidden_name;
      bound_keys_description = estrdup("none");
   }

   ~InputAction()
   {
      efree(bound_keys_description);
      efree(hidden_name);
   }

   bool handleEvent(event_t *ev, KeyBind *kb)
   {
      /*
       |---------------------------------------------------------------------|
       | activate   | nothing    | bind +y "+left" | isPO && isAO            |
       | deactivate | nothing    | bind +u "-left" | isPO && isDO            |
       | activate   | nothing    | bind +i "left"  | isPO && (!isAO || isDO) |
       | nothing    | activate   | bind -o "+left" | isRO && isAO            |
       | nothing    | deactivate | bind -p "-left" | isRO && isDO            |
       | nothing    | activate   | bind -h "left"  | isRO && !(isAO || isDO) |
       | activate   | activate   | bind j "+left"  | isAO                    |
       | deactivate | deactivate | bind k "-left"  | isDO                    |
       | activate   | deactivate | bind l "left"   | isN                     |
       | deactivate | activate   | ...             | n/a                     |
       |---------------------------------------------------------------------|
      */

      if(kb->isPressOnly())
      {
         if(kb->isDeactivateOnly())
            deactivate(ev);
         else
            activate(ev);
      }
      else if(kb->isReleaseOnly())
      {
         if(kb->isDeactivateOnly())
            deactivate(ev);
         else
            activate(ev);
      }
      else if(kb->isActivateOnly())
         activate(ev);
      else if(kb->isDeactivateOnly())
         deactivate(ev);
      else if(ev->type == ev_keydown)
         activate(ev);
      else if(ev->type == ev_keyup)
         deactivate(ev);
      else
         return false;

      if(ev->type == ev_keydown)
         kb->press();
      else if(ev->type == ev_keyup)
         kb->release();

      return true;
   }


   char* getName() const { return hidden_name; }
   input_action_category_e const getCategory() { return category; }
   bool isRepeatable() const { return repeatable; }
   virtual void activate(event_t *ev) {}
   virtual void deactivate(event_t *ev) {}
   virtual bool active() { return false; }
   char* getDescription() const { return bound_keys_description; }

   void setDescription(const char *new_description)
   {
      if(bound_keys_description)
         efree(bound_keys_description);

      bound_keys_description = estrdup(new_description);
   }
};

class OneShotFunctionInputAction : public InputAction
{
protected:
   void(*callback)(event_t *ev);

public:
   OneShotFunctionInputAction(const char *new_name,
                              input_action_category_e new_category,
                              void(*new_callback)(event_t *ev))
      : InputAction(new_name, new_category)
   {
      callback = new_callback;
   }

   OneShotFunctionInputAction(const char *new_name,
                              input_action_category_e new_category,
                              void(*new_callback)(event_t *ev),
                              bool new_repeatable)
      : InputAction(new_name, new_category, new_repeatable)
   {
      callback = new_callback;
   }

   void activate(event_t *ev) { callback(ev); }
};

class FunctionInputAction : public InputAction
{
protected:
   void(*callback)(event_t *ev);

public:
   FunctionInputAction(const char *new_name,
                       input_action_category_e new_category,
                       void(*new_callback)(event_t *ev))
      : InputAction(new_name, new_category)
   {
      callback = new_callback;
   }

   FunctionInputAction(const char *new_name,
                       input_action_category_e new_category,
                       void(*new_callback)(event_t *ev),
                       bool new_repeatable)
      : InputAction(new_name, new_category, new_repeatable)
   {
      callback = new_callback;
   }

   void activate(event_t *ev) { callback(ev); }
   void deactivate(event_t *ev) { callback(ev); }
};

class VariableInputAction : public InputAction
{
private:
   int *var;

public:
   VariableInputAction(const char *new_name,
                       input_action_category_e new_category,
                       int *new_variable)
      : InputAction(new_name, new_category)
   {
      var = new_variable;
   }

   VariableInputAction(const char *new_name,
                       input_action_category_e new_category,
                       int *new_variable, bool new_repeatable)
      : InputAction(new_name, new_category, new_repeatable)
   {
      var = new_variable;
   }

   void activate(event_t *ev) { (*var)++; }
   void deactivate(event_t *ev) { if(*var) { (*var)--; } }
   bool active() { if(*var) { return true; } return false; }
};

class CommandInputAction : public InputAction
{
public:
   CommandInputAction(const char *new_name)
      : InputAction(new_name, kac_command) {}

   CommandInputAction(const char *new_name, bool new_repeatable)
      : InputAction(new_name, kac_command, new_repeatable) {}

   void activate(event_t *ev) { if(!consoleactive) C_RunTextCmd(getName()); }
};

static InputKey *keys[NUM_KEYS];

static EHashTable<InputKey, ENCStringHashKey, &InputKey::key,
                  &InputKey::links> names_to_keys;
static EHashTable<InputAction, ENCStringHashKey, &InputAction::key,
                  &InputAction::links> names_to_actions;
static EHashTable<KeyBind, ENCStringHashKey, &KeyBind::keys_to_actions_key,
                  &KeyBind::key_to_action_links> keys_to_actions;
static EHashTable<KeyBind, ENCStringHashKey, &KeyBind::actions_to_keys_key,
                  &KeyBind::action_to_key_links> actions_to_keys;

// name of configuration file to read from/write to.
static char *cfg_file = NULL;

// assorted keybindings menu data structures.
menuwidget_t binding_widget = { G_BindDrawer, G_BindResponder, NULL, true };
static const char *binding_action; // name of action we are editing

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
// G_categoryIndex
//
// Translate category flags into a category index.
//
static int G_categoryIndex(int category)
{
   int i, shifted;

   for(i = 0 ;; i++)
   {
      if((shifted = 1 << i) >= kac_max)
         return -1;

      if(shifted == category)
         return i;
   }
}

//
// G_updateBoundKeyDescription
//
// Update an action's ascii description of the keys bound to it.
//
static void G_updateBoundKeyDescription(InputAction *action)
{
   qstring description;
   KeyBind *kb = NULL;
   bool found_bind = false;

   while((kb = actions_to_keys.keyIterator(kb, action->getName())))
   {
      if(found_bind)
         description.concat(" + ");
      else
         found_bind = true;
      description.concat(kb->getKeyName());
   }

   if(found_bind)
      action->setDescription(description.getBuffer());
   else
      action->setDescription("none");
}

//
// G_keyNumberForName
//
// Obtain a key's number from its name.
//
static int G_keyNumberForName(const char *name)
{
   InputKey *key;

   if(!(key = names_to_keys.objectForKey(name)))
      return -1;

   return key->getNumber();
}

//
// G_createBind
//
// Create a new KeyBind.
//
static void G_createBind(InputKey *key, InputAction *action,
                         unsigned short flags)
{
   KeyBind *kb = NULL;
   const char *kname = key->getName();
   const char *aname = action->getName();
   size_t aname_len = strlen(aname);

   // Check if this key is already bound to this action, as there's no sense in
   // duplicate binds.
   while((kb = keys_to_actions.keyIterator(kb, key->getName())))
   {
      const char *kb_aname;
      size_t kb_aname_len;
      size_t max_len;

      if(kb->getFlags() != flags)
         continue; // Flags don't match, check next bind.

      kb_aname = kb->getActionName();
      kb_aname_len = strlen(kb_aname);

      if(kb_aname_len > aname_len)
         max_len = kb_aname_len;
      else
         max_len = aname_len;

      if(strncasecmp(aname, kb_aname, max_len))
         continue; // Action names don't match, check next bind.

      // Key is already bound to this action, so don't bind it again.
      return;
   }

   kb = new KeyBind(
      key->getName(), action->getName(), flags, action->isRepeatable()
   );
   kb->setCategory(action->getCategory());
   actions_to_keys.addObject(kb);
   keys_to_actions.addObject(kb);

   if(actions_to_keys.getLoadFactor() > MAX_LOAD_FACTOR)
      actions_to_keys.Rebuild(actions_to_keys.getNumChains() * 2);

   if(keys_to_actions.getLoadFactor() > MAX_LOAD_FACTOR)
      keys_to_actions.Rebuild(keys_to_actions.getNumChains() * 2);

   G_updateBoundKeyDescription(action);
}

//
// G_removeBind
//
// Remove a KeyBind.
//
static void G_removeBind(KeyBind **kb)
{
   InputAction *action = names_to_actions.objectForKey((*kb)->getActionName());

   keys_to_actions.removeObject(*kb);
   actions_to_keys.removeObject(*kb);
   delete *kb;

   if(action)
      G_updateBoundKeyDescription(action);
}

//
// G_bindKeyToAction
//
// Bind a key to an action.
//
static void G_bindKeyToAction(InputKey *key, const char *action_name,
                              unsigned short flags)
{
   InputAction *action = NULL;
   char *real_action_name = (char *)action_name;

   if(action_name[0] == '+')
   {
      flags |= KeyBind::activate_only;
      real_action_name++;
   }
   else if(action_name[0] == '-')
   {
      flags |= KeyBind::deactivate_only;
      real_action_name++;
   }

   if(!(action = names_to_actions.objectForKey(real_action_name)))
   {
      G_addCommandAction(real_action_name);
      action = names_to_actions.objectForKey(real_action_name);
   }

   G_createBind(key, action, flags);
}

//
// G_bindKeyToActions
//
// Bind a key to one or more actions.
//
static void G_bindKeyToActions(const char *key_name, const char *action_names)
{
   unsigned short flags = 0;
   size_t key_name_length = strlen(key_name);
   qstring buffer(strlen(action_names) + 1);
   int i, j;
   InputKey *key;

   if(!key_name_length)
   {
      C_Printf(FC_ERROR "empty key name.\n");
      return;
   }

   if(key_name_length > 1)
   {
      if(key_name[0] == '+')
      {
         flags |= KeyBind::press_only;
         key_name++;
      }
      else if(key_name[0] == '-')
      {
         flags |= KeyBind::release_only;
         key_name++;
      }
   }

   if(!(key = names_to_keys.objectForKey(key_name)))
   {
      C_Printf(FC_ERROR "unknown key '%s'\n", key_name);
      return;
   }

   buffer = action_names;
   buffer.LStrip('\"');
   buffer.RStrip('\"');
   if(!buffer.length())
   {
      C_Printf(FC_ERROR "empty action list.\n", key_name);
      return;
   }

   for(i = 1, j = 0; i < (int)buffer.length(); i++)
   {
      if(buffer.charAt(i) == ';')
      {
         if((i - j) > 1)
         {
            *buffer.bufferAt(i) = '\0';
            G_bindKeyToAction(key, (const char *)buffer.bufferAt(j), flags);
            *buffer.bufferAt(i) = ';';
         }
         j = i + 1;
      }
   }

   // [CG] If the action list didn't end in a ";", then there is one more
   //      action to bind.
   if(buffer.charAt(i - 1) != ';' && ((i - j) > 1))
      G_bindKeyToAction(key, (const char *)buffer.bufferAt(j), flags);
}

//
// G_InitKeyBindings
//
// Set up keys, actions, hashtables, and other various things.
//
void G_InitKeyBindings(void)
{
   int i;
   qstring key_name;
   command_t *command;

   names_to_keys.Initialize(NUM_KEYS);
   names_to_actions.Initialize(INITIAL_KEY_ACTION_CHAIN_SIZE);
   actions_to_keys.Initialize(INITIAL_KEY_ACTION_CHAIN_SIZE);
   keys_to_actions.Initialize(INITIAL_KEY_ACTION_CHAIN_SIZE);

   // various names for different keys
   G_addKey(KEYD_RIGHTARROW, rightarrow);
   G_addKey(KEYD_LEFTARROW, leftarrow);
   G_addKey(KEYD_UPARROW, uparrow);
   G_addKey(KEYD_DOWNARROW, downarrow);
   G_addKey(KEYD_ESCAPE, escape);
   G_addKey(KEYD_ENTER, enter);
   G_addKey(KEYD_TAB, tab);
   G_addKey(KEYD_F1, f1);
   G_addKey(KEYD_F2, f2);
   G_addKey(KEYD_F3, f3);
   G_addKey(KEYD_F4, f4);
   G_addKey(KEYD_F5, f5);
   G_addKey(KEYD_F6, f6);
   G_addKey(KEYD_F7, f7);
   G_addKey(KEYD_F8, f8);
   G_addKey(KEYD_F9, f9);
   G_addKey(KEYD_F10, f10);
   G_addKey(KEYD_F11, f11);
   G_addKey(KEYD_F12, f12);

   G_addKey(KEYD_BACKSPACE, backspace);
   G_addKey(KEYD_PAUSE, pause);
   G_addKey(KEYD_MINUS, -);
   G_addKey(KEYD_RSHIFT, shift);
   G_addKey(KEYD_RCTRL, ctrl);
   G_addKey(KEYD_RALT, alt);
   G_addKey(KEYD_CAPSLOCK, capslock);

   G_addKey(KEYD_INSERT, insert);
   G_addKey(KEYD_HOME, home);
   G_addKey(KEYD_END, end);
   G_addKey(KEYD_PAGEUP, pgup);
   G_addKey(KEYD_PAGEDOWN, pgdn);
   G_addKey(KEYD_SCROLLLOCK, scrolllock);
   G_addKey(KEYD_SPACEBAR, space);
   G_addKey(KEYD_NUMLOCK, numlock);
   G_addKey(KEYD_DEL, delete);

   G_addKey(KEYD_MOUSE1, mouse1);
   G_addKey(KEYD_MOUSE2, mouse2);
   G_addKey(KEYD_MOUSE3, mouse3);
   G_addKey(KEYD_MOUSE4, mouse4);
   G_addKey(KEYD_MOUSE5, mouse5);
   G_addKey(KEYD_MWHEELUP, wheelup);
   G_addKey(KEYD_MWHEELDOWN, wheeldown);

   G_addKey(KEYD_JOY1, joy1);
   G_addKey(KEYD_JOY2, joy2);
   G_addKey(KEYD_JOY3, joy3);
   G_addKey(KEYD_JOY4, joy4);
   G_addKey(KEYD_JOY5, joy5);
   G_addKey(KEYD_JOY6, joy6);
   G_addKey(KEYD_JOY7, joy7);
   G_addKey(KEYD_JOY8, joy8);

   G_addKey(KEYD_KP0, kp_0);
   G_addKey(KEYD_KP1, kp_1);
   G_addKey(KEYD_KP2, kp_2);
   G_addKey(KEYD_KP3, kp_3);
   G_addKey(KEYD_KP4, kp_4);
   G_addKey(KEYD_KP5, kp_5);
   G_addKey(KEYD_KP6, kp_6);
   G_addKey(KEYD_KP7, kp_7);
   G_addKey(KEYD_KP8, kp_8);
   G_addKey(KEYD_KP9, kp_9);
   G_addKey(KEYD_KPPERIOD, kp_period);
   G_addKey(KEYD_KPDIVIDE, kp_slash);
   G_addKey(KEYD_KPMULTIPLY, kp_star);
   G_addKey(KEYD_KPMINUS, kp_minus);
   G_addKey(KEYD_KPPLUS, kp_plus);
   G_addKey(KEYD_KPENTER, kp_enter);
   G_addKey(KEYD_KPEQUALS, kp_equals);
   G_addKey('`', tilde);

   for(i = 0; i < NUM_KEYS; ++i)
   {
      if(keys[i])
         continue;

      // build generic name if not set yet
      if(isprint(i))
         key_name.Printf(0, "%c", i);
      else
         key_name.Printf(0, "key%i", i);

      keys[i] = new InputKey(i, key_name.constPtr());
      names_to_keys.addObject(*keys[i]);
   }

   G_addVariableAction(forward, kac_player);
   G_addVariableAction(backward, kac_player);
   G_addVariableAction(left, kac_player);
   G_addVariableAction(right, kac_player);
   G_addVariableAction(moveleft, kac_player);
   G_addVariableAction(moveright, kac_player);
   G_addVariableAction(use, kac_player);
   G_addVariableAction(strafe, kac_player);
   G_addVariableAction(attack, kac_player);
   G_addVariableAction(flip, kac_player);
   G_addVariableAction(speed, kac_player);
   G_addVariableAction(jump, kac_player);
   G_addVariableAction(autorun, kac_player);
   G_addVariableAction(mlook, kac_player);
   G_addVariableAction(lookup, kac_player);
   G_addVariableAction(lookdown, kac_player);
   G_addVariableAction(center, kac_player);
   G_addVariableAction(weapon1, kac_player);
   G_addVariableAction(weapon2, kac_player);
   G_addVariableAction(weapon3, kac_player);
   G_addVariableAction(weapon4, kac_player);
   G_addVariableAction(weapon5, kac_player);
   G_addVariableAction(weapon6, kac_player);
   G_addVariableAction(weapon7, kac_player);
   G_addVariableAction(weapon8, kac_player);
   G_addVariableAction(weapon9, kac_player);
   G_addVariableAction(nextweapon, kac_player);
   G_addVariableAction(weaponup, kac_player);
   G_addVariableAction(weapondown, kac_player);
   G_addVariableAction(frags, kac_hud);
   G_addVariableAction(menu_toggle, kac_menu);
   G_addVariableAction(menu_help, kac_menu);
   G_addVariableAction(menu_setup, kac_menu);
   G_addVariableAction(menu_up, kac_menu);
   G_addVariableAction(menu_down, kac_menu);
   G_addVariableAction(menu_confirm, kac_menu);
   G_addVariableAction(menu_previous, kac_menu);
   G_addVariableAction(menu_left, kac_menu);
   G_addVariableAction(menu_right, kac_menu);
   G_addVariableAction(menu_pageup, kac_menu);
   G_addVariableAction(menu_pagedown, kac_menu);
   G_addVariableAction(menu_contents, kac_menu);
   G_addRepeatableFunctionAction(map_right, kac_map, AM_HandlerRight);
   G_addRepeatableFunctionAction(map_left, kac_map, AM_HandlerLeft);
   G_addRepeatableFunctionAction(map_up, kac_map, AM_HandlerUp);
   G_addRepeatableFunctionAction(map_down, kac_map, AM_HandlerDown);
   G_addRepeatableFunctionAction(map_zoomin, kac_map, AM_HandlerZoomin);
   G_addRepeatableFunctionAction(map_zoomout, kac_map, AM_HandlerZoomout);
   G_addVariableAction(map_toggle, kac_map);
   G_addVariableAction(map_gobig, kac_map);
   G_addVariableAction(map_follow, kac_map);
   G_addVariableAction(map_mark, kac_map);
   G_addVariableAction(map_clear, kac_map);
   G_addVariableAction(map_grid, kac_map);
   G_addVariableAction(console_pageup, kac_console);
   G_addVariableAction(console_pagedown, kac_console);
   G_addVariableAction(console_toggle, kac_console);
   G_addVariableAction(console_tab, kac_console);
   G_addVariableAction(console_enter, kac_console);
   G_addVariableAction(console_up, kac_console);
   G_addVariableAction(console_down, kac_console);
   G_addRepeatableVariableAction(console_backspace, kac_console);

   for(i = 0; i < CMDCHAINS; i++)
      for(command = cmdroots[i]; command; command = command->next)
         G_addCommandAction(command->name);
}

//
// G_BoundKeys
//
// Get an ascii description of all keys bound to a particular action.
//
const char* G_BoundKeys(const char *action_name)
{
   InputAction *action = names_to_actions.objectForKey(action_name);

   if(!action)
   {
      G_addCommandAction(action_name);
      action = names_to_actions.objectForKey(action_name);
   }

   return action->getDescription();
}

//
// G_FirstBoundKey
//
// Get an ascii description of the first key bound to a particular action.
//
const char* G_FirstBoundKey(const char *action_name)
{
   InputKey *key = NULL;
   KeyBind *kb = NULL;
   KeyBind *previous_kb = NULL;

   while((kb = actions_to_keys.keyIterator(kb, action_name)))
      previous_kb = kb;

   if(!previous_kb)
      return "none";

   if(!(key = names_to_keys.objectForKey(previous_kb->getKeyName())))
      return "none";

   return key->getName();
}

//
// G_ClearKeyStates(void)
//
// "Releases" all keys.
//
void G_ClearKeyStates(void)
{
   KeyBind *kb = NULL;
   InputAction *action = NULL;

   while((kb = keys_to_actions.tableIterator(kb)))
      kb->release();

   while((action = names_to_actions.tableIterator(action)))
      action->deactivate(NULL);
}

//
// G_KeyResponder
//
// The main driver function for the entire key binding system.
//
bool G_KeyResponder(event_t *ev, int categories)
{
   static bool ctrldown;
   static bool altdown;
   InputKey *key = NULL;
   InputAction *action = NULL;
   KeyBind *kb = NULL;
   bool processed_event = false;

   // do not index out of bounds
   if(ev->data1 >= NUM_KEYS)
      return false;

   // Check for ctrl/alt keys.
   if(ev->data1 == KEYD_RCTRL)
      ctrldown = (ev->type == ev_keydown);
   else if(ev->data1 == KEYD_RALT)
      altdown = (ev->type == ev_keydown);

   // netgame disconnect binding
   if(opensocket && ctrldown && ev->data1 == 'd')
   {
      char buffer[128];

      psnprintf(buffer, sizeof(buffer),
                "disconnect from server?\n\n%s", DEH_String("PRESSYN"));
      MN_Question(buffer, "disconnect leaving");

      // dont get stuck thinking ctrl is down
      ctrldown = false;

      return true;
   }

   // Alt-tab binding (ignore whatever is bound to tab if alt is down)
   if(altdown && ev->data1 == KEYD_TAB)
   {
      altdown = false;
      return true;
   }

   key = keys[tolower(ev->data1)];

   while((kb = keys_to_actions.keyIterator(kb, key->getName())))
   {
      if(ev->type == ev_keyup)
      {
         if(kb->isPressOnly())
            continue;
      }
      else if(ev->type == ev_keydown)
      {
         if(kb->isPressed() && !kb->isRepeatable())
            continue;

         if(kb->isReleaseOnly())
            continue;
      }

      if(!(action = names_to_actions.objectForKey(kb->getActionName())))
      {
         doom_printf("Action [%s] not found.", kb->getActionName());
         continue;
      }

      if((categories & action->getCategory()) == 0)
         continue;

      processed_event = action->handleEvent(ev, kb);
   }

   return processed_event;
}

//===========================================================================
//
// Binding selection widget
//
// For menu: when we select to change a key binding the widget is used
// as the drawer and responder
//
//===========================================================================

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
bool G_BindResponder(event_t *ev)
{
   InputKey *key;
   KeyBind *kb = NULL;
   InputAction *action = NULL;
   bool found_bind = false;
   size_t action_name_length = strlen(binding_action);
   DLListItem<KeyBind> *binds = NULL;

   if(ev->type != ev_keydown)
      return false;

   // do not index out of bounds
   if(ev->data1 >= NUM_KEYS)
      return false;

   key = keys[tolower(ev->data1)];

   // got a key - close box
   current_menuwidget = NULL;

   if(action_menu_toggle) // cancel
   {
      action_menu_toggle = false;
      return true;
   }

   if(!(action = names_to_actions.objectForKey(binding_action)))
   {
      C_Printf(FC_ERROR "unknown action '%s'\n", binding_action);
      return true;
   }

   // First check if the key is already bound to the action; if it is, the
   // player is unbinding the key from the action, so remove the bind.

   while((kb = keys_to_actions.keyIterator(kb, key->getName())))
      kb->unbind_links.insert(kb, &binds);

   while(binds)
   {
      InputAction *action = NULL;

      kb = binds->dllObject;
      binds->remove();

      if(!(action = names_to_actions.objectForKey(kb->getActionName())))
         continue;

      if(!(strncasecmp(binding_action, action->getName(), action_name_length)))
      {
         found_bind = true;
         G_removeBind(&kb);
      }
   }

   // If the bind wasn't found, the player is binding, not unbinding, so create
   // a new bind.
   if(!found_bind)
      G_createBind(key, action, 0);

   return true;
}

//
// G_EditBinding
//
// Main Function
//
void G_EditBinding(const char *action_name)
{
   current_menuwidget = &binding_widget;
   binding_action = action_name;
}

//===========================================================================
//
// Load/Save defaults
//
//===========================================================================

// default script:

void G_LoadDefaults(void)
{
   char *temp = NULL;
   size_t len;
   DWFILE dwfile, *file = &dwfile;
   InputAction *action = NULL;

   len = M_StringAlloca(&temp, 1, 18, basegamepath);

   // haleyjd 11/23/06: use basegamepath
   // haleyjd 08/29/09: allow use_doom_config override
   if(GameModeInfo->type == Game_DOOM && use_doom_config)
      psnprintf(temp, len, "%s/doom/keys.csc", basepath);
   else
      psnprintf(temp, len, "%s/keys.csc", basegamepath);

   cfg_file = estrdup(temp);

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

   while((action = names_to_actions.tableIterator(action)))
      G_updateBoundKeyDescription(action);
}

void G_SaveDefaults(void)
{
   FILE *file;
   InputKey *key = NULL;
   KeyBind *kb = NULL;

   if(!cfg_file)         // check defaults have been loaded
      return;

   if(!(file = fopen(cfg_file, "w")))
   {
      C_Printf(FC_ERROR"Couldn't open keys.csc for write access.\n");
      return;
   }

   // write key bindings
   while((key = names_to_keys.tableIterator(key)))
   {
      bool found_bind = false;

      if(strlen(key->getName()) == 1 && isalpha(*key->getName()) &&
                                        isupper(*key->getName()))
         continue;

      while((kb = keys_to_actions.keyIterator(kb, key->getName())))
      {
         if(!found_bind)
         {
            if(kb->isPressOnly())
               fprintf(file, "bind +%s \"", key->getName());
            else if(kb->isReleaseOnly())
               fprintf(file, "bind -%s \"", key->getName());
            else
               fprintf(file, "bind %s \"", key->getName());

            found_bind = true;
         }
         else
            fprintf(file, ";");

         if(kb->isActivateOnly())
            fprintf(file, "+%s", kb->getActionName());
         else if(kb->isDeactivateOnly())
            fprintf(file, "-%s", kb->getActionName());
         else
            fprintf(file, "%s", kb->getActionName());
      }

      if(found_bind)
         fprintf(file, "\"\n");
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
      G_bindKeyToActions(
         Console.argv[0]->constPtr(), Console.argv[1]->constPtr()
      );
   }
   else if(Console.argc == 1)
   {
      InputKey *key = NULL;
      InputAction *action = NULL;
      KeyBind *kb = NULL;
      const char *key_name = Console.argv[0]->constPtr();
      bool found_bind = false;


      if(!(key = names_to_keys.objectForKey(key_name)))
      {
         C_Printf(FC_ERROR "no such key!\n");
         return;
      }

      while((kb = keys_to_actions.keyIterator(kb, key_name)))
      {
         if(!(action = names_to_actions.objectForKey(kb->getActionName())))
            continue;

         C_Printf(
            "%s bound to %s (category %d)\n",
            key->getName(),
            action->getName(),
            G_categoryIndex(action->getCategory())
         );

         found_bind = true;
      }
   }
   else
      C_Printf("usage: bind key action\n");
}

// haleyjd: utility functions
CONSOLE_COMMAND(listactions, 0)
{
   InputAction *action = NULL;

   while((action = names_to_actions.tableIterator(action)))
      C_Printf("%s\n", action->getName());
}

CONSOLE_COMMAND(listkeys, 0)
{
   int i;

   for(i = 0; i < NUM_KEYS; ++i)
      C_Printf("%s\n", keys[i]->getName());
}

static int G_allActionCategories()
{
   static int action_categories = -1;
   int current_flags = (kac_none << 1);

   if(action_categories == -1)
   {
      action_categories = current_flags;
      while(current_flags < (kac_max - 1))
      {
         current_flags <<= 1;
         action_categories |= current_flags;
      }
   }

   return action_categories;
}

CONSOLE_COMMAND(unbind, 0)
{
   int category = -1;
   int category_flags;
   const char *kname;
   bool found_bind = false;
   bool ignored_console_or_menu_actions = false;
   KeyBind *kb = NULL;
   InputKey *key = NULL;
   DLListItem<KeyBind> *binds = NULL;

   if(Console.argc < 1)
   {
      C_Printf("usage: unbind key [category]\n");
      return;
   }

   kname = Console.argv[0]->constPtr();

   // allow specification of a binding category
   if(Console.argc == 2)
   {
      category = Console.argv[1]->toInt();
      category_flags = 1 << category;

      if((!(category_flags & G_allActionCategories())) ||
         (category_flags == kac_none))
      {
         C_Printf(FC_ERROR "invalid action category %d\n", category);
         C_Printf(
            FC_ERROR " %d/%d, %d/%d, %d.\n",
            category,
            G_allActionCategories(),
            category_flags,
            kac_none,
            category_flags & G_allActionCategories()
         );
         return;
      }
   }

   if(!(key = names_to_keys.objectForKey(kname)))
   {
      C_Printf("unknown key %s\n", kname);
      return;
   }

   while((kb = keys_to_actions.keyIterator(kb, key->getName())))
      kb->unbind_links.insert(kb, &binds);

   while(binds)
   {
      InputAction *action = NULL;
      input_action_category_e action_category;

      kb = binds->dllObject;
      binds->remove();

      if(!(action = names_to_actions.objectForKey(kb->getActionName())))
         continue;

      action_category = action->getCategory();

      if(category != -1)
      {
         if(action_category != category_flags)
            continue;
      }
      else if(action_category == kac_menu || action_category == kac_console)
      {
         ignored_console_or_menu_actions = true;
         continue;
      }

      found_bind = true;
      G_removeBind(&kb);
   }

   if(found_bind)
   {
      if(category == -1)
      {
         C_Printf("unbound key %s from all actions\n", kname);
         if(ignored_console_or_menu_actions)
            C_Printf(FC_ERROR " console and menu actions ignored\n");
      }
      else
         C_Printf("unbound key %s from category %d actions\n", kname, category);
   }
   else if(category == -1)
      C_Printf("No actions bound to key %s.\n", kname);
   else
      C_Printf("No actions bound to key %s in category %d.\n", kname, category);
}


CONSOLE_COMMAND(unbindall, 0)
{
   KeyBind *kb = NULL;

   while((kb = keys_to_actions.tableIterator(NULL)))
      G_removeBind(&kb);

   C_Printf("key bindings cleared\n");
}

//
// bindings
//
// haleyjd 12/11/01
// list all active bindings to the console
//
CONSOLE_COMMAND(bindings, 0)
{
   KeyBind *kb = NULL;

   while((kb = keys_to_actions.tableIterator(kb)))
      C_Printf("%s : %s\n", kb->getKeyName(), kb->getActionName());
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
