// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
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
// Rewritten to support input interfaces as well as press only, release only,
// activate only and deactivate only bindings.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "am_map.h"
#include "c_batch.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_dwfile.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_iface.h"
#include "d_io.h"        // SoM 3/14/2002: strncasecmp
#include "d_main.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "e_hash.h"
#include "g_newbind.h"
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

#define USING_NEW_BINDS 0

#define BINDNUMCHAINS 31
#define MAX_LOAD_FACTOR 0.7

#define addKey(number, name) \
   keys[number] = new InputKey(number, name); \
   pImpl->hash.addObject(*keys[number]); \
   if(pImpl->hash.getLoadFactor() > MAX_LOAD_FACTOR) \
      pImpl->hash.rebuild(pImpl->hash.getNumChains() * 2)

extern vfont_t *menu_font_normal;

// Used by the keybindings menu, the responder sets the action name and the
// interface name when the user selects an action to bind a key to, and once
// the user selects a key, we then bind that key to the action for the given
// interface.  If the interface name is blank, then the key is bound to that
// action for any supporting interface.
static menuwidget_t binding_widget = {
   MN_BindDrawer, MN_BindResponder, NULL, true
};
static qstring binding_action_name;
static qstring binding_interface_name;

class bindHashPimpl : public ZoneObject
{
public:
   EHashTable<KeyBind, ENCStringHashKey, &KeyBind::key,
              &KeyBind::links> hash;

   bindHashPimpl() : ZoneObject(), hash(BINDNUMCHAINS) {}

   virtual ~bindHashPimpl() { hash.destroy(); }
};

class keyHashPimpl : public ZoneObject
{
public:
   EHashTable<InputKey, ENCStringHashKey, &InputKey::key,
              &InputKey::links> hash;

   keyHashPimpl() : ZoneObject(), hash(KBSS_NUM_KEYS) {}

   virtual ~keyHashPimpl() { hash.destroy(); }
};

KeyBindingsSubSystem key_bindings;

KeyBind::KeyBind(InputAction *new_action)
   : ZoneObject()
{
   action = new_action;
   key = estrdup(action->getName());
   type = normal;
}

KeyBind::KeyBind(InputAction *new_action, uint8_t new_type)
   : ZoneObject()
{
   action = new_action;
   key = estrdup(new_action->getName());
   type = new_type;
}

KeyBind::~KeyBind()
{
   if(key)
      efree(key);

   key = NULL;
}

InputAction* KeyBind::getAction() const
{
   return action;
}

uint8_t KeyBind::getType() const
{
   return type;
}

void KeyBind::setType(uint8_t new_type)
{
   type = new_type;
}

InputKey::InputKey(int new_number, const char *new_name)
   : ZoneObject()
{
   number = new_number;
   name = estrdup(new_name);
   bind_count = 0;
   pImpl = new bindHashPimpl();

   key = name;
}

InputKey::~InputKey()
{
   KeyBind *kb;

   while((kb = pImpl->hash.tableIterator(NULL)))
   {
      pImpl->hash.removeObject(kb);
      delete kb;
   }

   delete pImpl;

   efree(name);
}

KeyBind* InputKey::getBind(InputAction *action) const
{
   KeyBind *kb = NULL;

   while((kb = pImpl->hash.keyIterator(kb, action->getName())))
   {
      if(kb->getAction() == action)
         return kb;
   }

   return NULL;
}

void InputKey::writeBindingsOfType(qstring &buf, uint8_t bind_type) const
{
   KeyBind *kb = NULL;
   InputAction *action = NULL;
   InputInterface *iface = NULL;
   InputInterface **iitr = NULL;

   for(iitr = input_interfaces.begin(); iitr != input_interfaces.end(); iitr++)
   {
      bool found_bind = false;

      iface = *iitr;

      while((kb = pImpl->hash.tableIterator(kb)))
      {
         action = kb->getAction();

         if((action->getInterface() == iface) && (kb->getType() == bind_type))
         {
            if(!found_bind)
            {
               buf << "bind " << name << " \"" << action->getName();
               found_bind = true;
            }
            else
               buf << ";" << action->getName();
         }
      }

      if(found_bind)
         buf << "\" " << iface->getName() << "\n";
   }
}

const char* InputKey::getName() const
{
   return name;
}

int InputKey::getNumber() const
{
   return number;
}

bool InputKey::isBoundTo(InputAction *action) const
{
   if(bind_count && getBind(action))
      return true;

   return false;
}

bool InputKey::isBoundTo(const char *action_name) const
{
   if(bind_count && pImpl->hash.objectForKey(action_name))
      return true;

   return false;
}

bool InputKey::isOnlyBoundTo(const char *action_name) const
{
   if((bind_count == 1) && pImpl->hash.objectForKey(action_name))
      return true;

   return false;
}

KeyBind* InputKey::keyBindIterator(KeyBind *kb) const
{
   return pImpl->hash.tableIterator(kb);
}

void InputKey::writeBindings(qstring &buf) const
{
   // [CG] Uppercase alphabetical keys are never saved.
   if((strlen(name) == 1) && isalpha(*name) && isupper(*name))
      return;

   // [CG] Write normal binds first, then go down the types,
   //      activates_on_press, activates_on_release, etc.
   writeBindingsOfType(buf, KeyBind::normal);
   writeBindingsOfType(buf, KeyBind::activates_on_press);
   writeBindingsOfType(buf, KeyBind::activates_on_release);
   writeBindingsOfType(buf, KeyBind::activates_on_press_and_release);
   writeBindingsOfType(buf, KeyBind::deactivates_on_press);
   writeBindingsOfType(buf, KeyBind::deactivates_on_release);
   writeBindingsOfType(buf, KeyBind::deactivates_on_press_and_release);
   writeBindingsOfType(buf, KeyBind::reverse);
}

void InputKey::bind(InputAction *action)
{
   bind(action, KeyBind::normal);
}

void InputKey::bind(InputAction *action, uint8_t type)
{
   KeyBind *kb = getBind(action);

   if(kb)
   {
      kb->setType(type);
      return;
   }

   unbind(action); // [CG] Unbind the key first, if it's already bound.

   pImpl->hash.addObject(new KeyBind(action, type));
   if(pImpl->hash.getLoadFactor() > MAX_LOAD_FACTOR)
      pImpl->hash.rebuild(pImpl->hash.getNumChains() * 2);
   bind_count++;
}

void InputKey::bind(const char *action_name)
{
   bind(action_name, KeyBind::normal);
}

void InputKey::bind(const char *action_name, uint8_t type)
{
   InputAction *action = NULL;
   InputInterface *iface = NULL;
   InputInterface **iitr = NULL;

   unbind(action_name); // [CG] Unbind the key first, if it's already bound.

   for(iitr = input_interfaces.begin(); iitr != input_interfaces.end(); iitr++)
   {
      iface = *iitr;

      action = iface->getAction(action_name);

      if(action)
      {
         pImpl->hash.addObject(new KeyBind(action, type));
         if(pImpl->hash.getLoadFactor() > MAX_LOAD_FACTOR)
            pImpl->hash.rebuild(pImpl->hash.getNumChains() * 2);
         bind_count++;
      }
   }
}

bool InputKey::unbind()
{
   KeyBind *kb = NULL;
   bool deleted_bind = false;

   while((kb = pImpl->hash.tableIterator(NULL)))
   {
      pImpl->hash.removeObject(kb);
      delete kb;
      deleted_bind = true;
   }

   bind_count = 0;

   return deleted_bind;
}

bool InputKey::unbind(const char *action_name)
{
   KeyBind *kb = NULL;
   bool deleted_bind = false;

   while((kb = pImpl->hash.objectForKey(action_name)))
   {
      pImpl->hash.removeObject(kb);
      delete kb;
      bind_count--;
      deleted_bind = true;
   }

   return deleted_bind;
}

bool InputKey::unbind(const char *action_name, InputInterface *iface)
{
   InputAction *action = iface->getAction(action_name);

   if(!action)
      return false;

   return unbind(action);
}

bool InputKey::unbind(InputAction *action)
{
   KeyBind *kb = getBind(action);

   if(!kb)
      return false;

   pImpl->hash.removeObject(kb);
   delete kb;
   bind_count--;
   return true;
}

void InputKey::handleEvent(event_t *ev, InputInterface *iface)
{
   KeyBind *kb = NULL;

   while((kb = pImpl->hash.tableIterator(kb)))
   {
      if(kb->getAction()->getInterface() != iface)
         continue;

      if(ev->type == ev_keydown)
      {
         switch(kb->getType())
         {
            case KeyBind::activates_on_press:
            case KeyBind::activates_on_press_and_release:
            case KeyBind::activates_on_press_and_deactivates_on_release:
               kb->getAction()->activate(this);
               break;
            case KeyBind::deactivates_on_press:
            case KeyBind::deactivates_on_press_and_release:
            case KeyBind::deactivates_on_press_and_activates_on_release:
               kb->getAction()->deactivate(this);
               break;
            default:
               break;
         }
      }
      else if(ev->type == ev_keyup)
      {
         switch(kb->getType())
         {
            case KeyBind::activates_on_release:
            case KeyBind::activates_on_press_and_release:
            case KeyBind::deactivates_on_press_and_activates_on_release:
               kb->getAction()->activate(this);
               break;
            case KeyBind::deactivates_on_release:
            case KeyBind::deactivates_on_press_and_release:
            case KeyBind::activates_on_press_and_deactivates_on_release:
               kb->getAction()->deactivate(this);
               break;
            default:
               break;
         }
      }
   }
}

void InputKey::activateAllBinds()
{
   KeyBind *kb = NULL;

   while((kb = pImpl->hash.tableIterator(kb)))
      kb->getAction()->activate();
}

void InputKey::deactivateAllBinds()
{
   KeyBind *kb = NULL;

   while((kb = pImpl->hash.tableIterator(kb)))
      kb->getAction()->deactivate();
}

bool InputKey::operator == (const char *key_name) const
{
   if((strcasecmp(name, key_name)))
      return false;

   return true;
}

bool InputKey::operator == (unsigned char key_number) const
{
   if(number == key_number)
      return true;

   return false;
}

bool InputKey::operator != (const char *key_name) const
{
   if((strcasecmp(name, key_name)))
      return true;

   return false;
}

bool InputKey::operator != (unsigned char key_number) const
{
   if(number == key_number)
      return false;

   return true;
}

InputAction::InputAction(const char *new_name, InputInterface *new_iface)
{
   name = estrdup(new_name);
   iface = new_iface;
   key = name;
   activation_count = 0;
}

InputAction::~InputAction()
{
   if(name)
      efree(name);
}

const char* InputAction::getName() const
{
   return name;
}

InputInterface* InputAction::getInterface() const
{
   return iface;
}

const char* InputAction::getBoundKeyNames() const
{
   static qstring bound_key_names;

   int i;
   InputKey *key = NULL;
   bool found_a_key = false;

   bound_key_names.clear();

   for(i = 0; i < KBSS_NUM_KEYS; i++)
   {
      key = key_bindings.getKey(i);

      if(key->isBoundTo(name))
      {
         if(found_a_key)
            bound_key_names << ", ";
         else
            found_a_key = true;

         bound_key_names << key->getName();
      }
   }

   return bound_key_names.constPtr();
}

bool InputAction::isActive() const
{
   if(activation_count > 0)
      return true;

   return false;
}

const char* InputAction::getFirstBoundKeyName() const
{
   int i;
   InputKey *key = NULL;

   for(i = 0; i < KBSS_NUM_KEYS; i++)
   {
      key = key_bindings.getKey(i);

      if(key->isBoundTo(name))
         return key->getName();
   }

   return "none";
}

void InputAction::activate()
{
   activation_count++;
}

void InputAction::activate(InputKey *key)
{
   int key_number = key->getNumber();

   if(!activating_keys[key_number])
   {
      activating_keys[key_number] = true;
      activation_count++;
   }
}

void InputAction::deactivate()
{
   for(int i = 0; i < KBSS_NUM_KEYS; i++)
      activating_keys[i] = false;

   activation_count = 0;
}

void InputAction::deactivate(InputKey *key)
{
   int key_number = key->getNumber();

   if(activating_keys[key_number])
   {
      activating_keys[key_number] = false;

      if(activation_count > 0)
         activation_count--;
   }
}

KeyBindingsSubSystem::KeyBindingsSubSystem()
{
   pImpl = new keyHashPimpl();
   cfg_file = NULL;
}

void KeyBindingsSubSystem::initialize()
{
   int i;
   qstring key_name;

   for(i = 0; i < KBSS_NUM_KEYS; ++i)
      keys[i] = NULL;

   // various names for different keys
   addKey(KEYD_RIGHTARROW, "rightarrow");
   addKey(KEYD_LEFTARROW,  "leftarrow");
   addKey(KEYD_UPARROW,    "uparrow");
   addKey(KEYD_DOWNARROW,  "downarrow");
   addKey(KEYD_ESCAPE,     "escape");
   addKey(KEYD_ENTER,      "enter");
   addKey(KEYD_TAB,        "tab");
   addKey(KEYD_F1,         "f1");
   addKey(KEYD_F2,         "f2");
   addKey(KEYD_F3,         "f3");
   addKey(KEYD_F4,         "f4");
   addKey(KEYD_F5,         "f5");
   addKey(KEYD_F6,         "f6");
   addKey(KEYD_F7,         "f7");
   addKey(KEYD_F8,         "f8");
   addKey(KEYD_F9,         "f9");
   addKey(KEYD_F10,        "f10");
   addKey(KEYD_F11,        "f11");
   addKey(KEYD_F12,        "f12");

   addKey(KEYD_BACKSPACE,  "backspace");
   addKey(KEYD_PAUSE,      "pause");
   addKey(KEYD_MINUS,      "-");
   addKey(KEYD_RSHIFT,     "shift");
   addKey(KEYD_RCTRL,      "ctrl");
   addKey(KEYD_RALT,       "alt");
   addKey(KEYD_CAPSLOCK,   "capslock");

   addKey(KEYD_INSERT,     "insert");
   addKey(KEYD_HOME,       "home");
   addKey(KEYD_END,        "end");
   addKey(KEYD_PAGEUP,     "pgup");
   addKey(KEYD_PAGEDOWN,   "pgdn");
   addKey(KEYD_SCROLLLOCK, "scrolllock");
   addKey(KEYD_SPACEBAR,   "space");
   addKey(KEYD_NUMLOCK,    "numlock");
   addKey(KEYD_DEL,        "delete");

   addKey(KEYD_MOUSE1,     "mouse1");
   addKey(KEYD_MOUSE2,     "mouse2");
   addKey(KEYD_MOUSE3,     "mouse3");
   addKey(KEYD_MOUSE4,     "mouse4");
   addKey(KEYD_MOUSE5,     "mouse5");
   addKey(KEYD_MWHEELUP,   "wheelup");
   addKey(KEYD_MWHEELDOWN, "wheeldown");

   addKey(KEYD_JOY1,       "joy1");
   addKey(KEYD_JOY2,       "joy2");
   addKey(KEYD_JOY3,       "joy3");
   addKey(KEYD_JOY4,       "joy4");
   addKey(KEYD_JOY5,       "joy5");
   addKey(KEYD_JOY6,       "joy6");
   addKey(KEYD_JOY7,       "joy7");
   addKey(KEYD_JOY8,       "joy8");

   addKey(KEYD_KP0,        "kp_0");
   addKey(KEYD_KP1,        "kp_1");
   addKey(KEYD_KP2,        "kp_2");
   addKey(KEYD_KP3,        "kp_3");
   addKey(KEYD_KP4,        "kp_4");
   addKey(KEYD_KP5,        "kp_5");
   addKey(KEYD_KP6,        "kp_6");
   addKey(KEYD_KP7,        "kp_7");
   addKey(KEYD_KP8,        "kp_8");
   addKey(KEYD_KP9,        "kp_9");
   addKey(KEYD_KPPERIOD,   "kp_period");
   addKey(KEYD_KPDIVIDE,   "kp_slash");
   addKey(KEYD_KPMULTIPLY, "kp_star");
   addKey(KEYD_KPMINUS,    "kp_minus");
   addKey(KEYD_KPPLUS,     "kp_plus");
   addKey(KEYD_KPENTER,    "kp_enter");
   addKey(KEYD_KPEQUALS,   "kp_equals");
   addKey(KEYD_ACCGRAVE,   "backtick");

   addKey(KEYD_PRINT,      "printscreen");
   addKey(KEYD_SYSRQ,      "sysrq");

   for(i = 0; i < KBSS_NUM_KEYS; ++i)
   {
      if(!keys[i]) // build generic name if not set yet
      {
         if(isprint(i))
            key_name.Printf(0, "%c", i);
         else
            key_name.Printf(0, "key%i", i);

         addKey(i, key_name.constPtr());
      }
   }
}

const char* KeyBindingsSubSystem::getBindingActionName() const
{
   return binding_action_name.constPtr();
}

const char* KeyBindingsSubSystem::getBindingInterfaceName() const
{
   return binding_interface_name.constPtr();
}

InputKey* KeyBindingsSubSystem::getKeyFromEvent(event_t *ev) const
{
   // do not index out of bounds
   if((ev->data1 >= KBSS_NUM_KEYS) || (ev->data1 < 0))
      return NULL;

   // Don't return keys for events that aren't key presses or releases.
   if((ev->type != ev_keydown) && (ev->type != ev_keyup))
      return NULL;

   return keys[tolower(ev->data1)];
}

InputKey* KeyBindingsSubSystem::getKey(int index) const
{
   if((index >= KBSS_NUM_KEYS) || (index < 0))
   {
      I_Error(
         "KeyBindingsSubSystem::getKey: key index %d outside 0-%d.\n",
         index,
         KBSS_NUM_KEYS - 1
      );
   }

   return keys[index];
}

InputKey* KeyBindingsSubSystem::getKey(const char *key_name) const
{
   return pImpl->hash.objectForKey(key_name);
}

InputKey* KeyBindingsSubSystem::getKey(qstring *key_name) const
{
   return pImpl->hash.objectForKey(key_name->constPtr());
}

void KeyBindingsSubSystem::writeActions(qstring &buf) const
{
   InputAction *action = NULL;
   InputInterface *iface = NULL;
   InputInterface **iitr = NULL;

   for(iitr = input_interfaces.begin(); iitr != input_interfaces.end(); iitr++)
   {
      iface = *iitr;

      while((action = iface->actionIterator(action)))
         buf << iface->getName() << ": " << action->getName() << "\n";
   }
}

const char*
KeyBindingsSubSystem::getKeysBoundToAction(const char *action_name,
                                           const char *interface_name) const
{
   static qstring buf;

   int i;
   bool found_key = false;
   InputKey *key = NULL;
   InputAction *action = NULL;
   InputInterface *iface = NULL;
   InputInterface **iitr = NULL;

   if(!interface_name)
   {
      // [CG] Looking for keys bound to an action in any interface here.

      buf.clear();

      for(i = 0; i < KBSS_NUM_KEYS; ++i)
      {
         key = key_bindings.getKey(i);

         if(key->isBoundTo(action_name))
         {
            if(found_key)
               buf << ", ";
            else
               found_key = true;

            buf << key->getName();
         }
      }

      return buf.constPtr();
   }

   if(!(iface = input_interfaces.getInterface(interface_name)))
   {
      I_Error(
         "KeyBindingsSubSystem::getKeysBoundToAction: no such interface %s.\n",
         interface_name
      );
   }

   if(!(action = iface->getAction(action_name)))
   {
      I_Error(
         "KeyBindingsSubSystem::getKeysBoundToAction: "
         "%s does not support action %s.\n",
         interface_name,
         action_name
      );
   }

   return action->getBoundKeyNames();
}

void KeyBindingsSubSystem::setKeyBindingsFile(const char *filename)
{
   if(cfg_file)
      efree(cfg_file);

   cfg_file = estrdup(filename);
}

void KeyBindingsSubSystem::startMenuBind(const char *new_binding_action_name,
                                         const char *new_binding_interface_name)
{
   binding_action_name.Printf(0, new_binding_action_name);
   if(new_binding_interface_name)
      binding_interface_name.Printf(0, new_binding_interface_name);
   else
      binding_interface_name.clear();
}

// Check for alt/ctrl/shift keys.
bool KeyBindingsSubSystem::handleEvent(event_t *ev)
{
   switch(ev->data1)
   {
   case KEYD_RALT:
      altdown = (ev->type == ev_keydown);
      return true;
   case KEYD_RCTRL:
      ctrldown = (ev->type == ev_keydown);
      return true;
   case KEYD_RSHIFT:
      shiftdown = (ev->type == ev_keydown);
      return true;
   default:
      return false;
   }
}

void KeyBindingsSubSystem::loadKeyBindings()
{
   char *temp = NULL;
   size_t len;
   DWFILE dwfile, *file = &dwfile;

   len = M_StringAlloca(&temp, 1, 18, usergamepath);

   // haleyjd 11/23/06: use basegamepath
   // haleyjd 08/29/09: allow use_doom_config override
   if(GameModeInfo->type == Game_DOOM && use_doom_config)
      psnprintf(temp, len, "%s/doom/keys.csc", userpath);
   else
      psnprintf(temp, len, "%s/keys.csc", usergamepath);

   setKeyBindingsFile(temp);

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

void KeyBindingsSubSystem::saveKeyBindings()
{
   int i;
   FILE *file;
   qstring buf;
   InputKey *key;

   if(!cfg_file)         // check defaults have been loaded
      return;

   if(!(file = fopen(cfg_file, "w")))
   {
      C_Printf(FC_ERROR "Couldn't open keys.csc for write access.\n");
      return;
   }

   // write key bindings
   for(i = 0, key = keys[i]; i < KBSS_NUM_KEYS; i++, key = keys[i])
      key->writeBindings(buf);

   // Save command batches
   C_SaveCommandBatches(buf);

   fprintf(file, "%s", buf.constPtr());
   fclose(file);
}

void KeyBindingsSubSystem::clearKeyStates()
{
   for(int i = 0; i < KBSS_NUM_KEYS; i++)
      keys[i]->deactivateAllBinds();
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
// MN_EditKeyBinding
//
// Main Function
//
void MN_EditKeyBinding(const char *action_name, const char *interface_name)
{
   current_menuwidget = &binding_widget;
   key_bindings.startMenuBind(action_name, interface_name);
   MN_PushWidget(&binding_widget);
}

//
// MN_BindDrawer
//
// Draw the prompt box
//
void MN_BindDrawer()
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
// MN_BindResponder
//
// Responder for keybinding widget
//
bool MN_BindResponder(event_t *ev)
{
   InputKey *key = key_bindings.getKeyFromEvent(ev);
   const char *action_name = key_bindings.getBindingActionName();
   const char *interface_name = key_bindings.getBindingInterfaceName();

   if(!key)
      return false;

   if(ev->type != ev_keydown)
      return false;

   // got a key - close box
   MN_PopWidget();

#if MENU_INTERFACE_ENABLED
   if(Menu.actionIsActive("menu_toggle")) // cancel
   {
      Menu.deactivateAction("menu_toggle");
      return true;
   }
#endif

   if(interface_name)
   {
      InputAction *action = NULL;
      InputInterface *iface = input_interfaces.getInterface(interface_name);

      if(!iface)
      {
         I_Error(
            "Invalid interface (%s) set for action %s.\n",
            interface_name,
            action_name
         );
      }

      action = iface->getAction(action_name);

      if(!action)
      {
         I_Error(
            "Interface %s does not support action %s.\n",
            interface_name,
            action_name
         );
      }

      if(key->isBoundTo(action)) // un-binding
         key->unbind(action);
      else // binding!
         key->bind(action);
   }
   else if(key->isBoundTo(action_name)) // un-binding
      key->unbind(action_name);
   else // binding!
      key->bind(action_name);

   return true;
}

//===========================================================================
//
// Load/Save defaults
//
//===========================================================================

// default script:

//=============================================================================
//
// Console Commands
//

#if USING_NEW_BINDS

//
// bind
//
// Bind a key to one or more actions.
//
CONSOLE_COMMAND(bind, 0)
{
   if((Console.argc < 1) || (Console.argc > 3))
   {
      C_Printf("usage: bind key <action(s)> <interface>\n");
      return;
   }

   if(Console.argc == 1) // [CG] Lists the key's bound actions
   {
      InputKey *key = NULL;
      InputInterface *iface = NULL;
      InputInterface **iitr = NULL;
      bool found_bind = false;

      if(!(key = key_bindings.getKey(Console.argv[0]->constPtr())))
      {
         C_Printf(FC_ERROR "no such key!\n");
         return;
      }

      for(iitr = input_interfaces.begin();
          iitr != input_interfaces.end();
          iitr++)
      {
         KeyBind *kb = NULL;

         iface = *iitr;

         while((kb = key->keyBindIterator(kb)))
         {
            if((iface->supportsAction(kb->getAction())))
            {
               C_Printf(
                  "%s: %s bound to %s\n",
                  iface->getName(),
                  key->getName(),
                  kb->getAction()->getName()
               );
               found_bind = true;
            }
         }
      }

      if(!found_bind)
         C_Printf("%s is not bound.\n", key->getName());
   }
   else if(Console.argc == 2)
   {
      InputKey *key = NULL;
      InputInterface *iface = NULL;
      InputInterface **iitr = NULL;

      if(!(key = key_bindings.getKey(Console.argv[0]->constPtr())))
      {
         C_Printf(FC_ERROR "no such key!\n");
         return;
      }

      if(Console.argv[1]->length() == 0)
      {
         C_Printf(FC_ERROR "empty action list!\n");
         return;
      }

      // [CG] Try and run this bind command in all interfaces, since no
      //      interface was specified in the command.
      for(iitr = input_interfaces.begin();
          iitr != input_interfaces.end();
          iitr++)
      {
         iface = *iitr;
         iface->handleBindCommand(Console.argv[0], Console.argv[1], false);
      }
   }
   else if(Console.argc == 3)
   {
      // [CG] An interface was specified in the command, so run the bind
      //      command in that interface.
      InputInterface *iface = input_interfaces.getInterface(Console.argv[2]);

      if(!iface)
      {
         C_Printf(
            FC_ERROR "No such interface %s.\n", Console.argv[2]->constPtr()
         );
         return;
      }

      iface->handleBindCommand(Console.argv[0], Console.argv[1], true);
   }
}

// haleyjd: utility functions
CONSOLE_COMMAND(listactions, 0)
{
   qstring buf;

   key_bindings.writeActions(buf);
   C_Printf(buf.constPtr());
}

CONSOLE_COMMAND(listkeys, 0)
{
   int i;

   for(i = 0; i < KBSS_NUM_KEYS; ++i)
      C_Printf("%s\n", key_bindings.getKey(i)->getName());
}

// [CG] Actions can have the same name across interfaces, for example a game
//      screenshot and a menu screenshot.  Even if these actions have the same
//      implementation, they have to be separately created and registered with
//      their supporting interfaces.
//
//      If a user unbinds a key from such an action without specifying the
//      interface, then the key is unbound from any action with that name
//      across all interfaces.  If an interface is specified, then the key is
//      unbound from that action only within that interface.
//
//      Interfaces must then run their "checkProtectedBinds" method to ensure
//      that basic functionality is still preserved.

CONSOLE_COMMAND(unbind, 0)
{
   KeyBind *kb = NULL;
   InputKey *key = NULL;
   qstring *kname = NULL;
   bool action_found = false;

   if((Console.argc < 1) || (Console.argc > 3))
   {
      C_Printf("usage: unbind key <action> <interface>\n");
      return;
   }

   kname = Console.argv[0];

   if(!(key = key_bindings.getKey(kname)))
   {
      C_Printf(FC_ERROR "no such key!\n");
      return;
   }

   if(Console.argc == 1)
   {
      if(key->unbind())
         C_Printf("unbound key %s from all actions\n", kname->constPtr());
      else
         C_Printf(FC_ERROR "no actions bound to key %s.\n", kname->constPtr());
   }
   else if(Console.argc == 2)
   {
      qstring *aname = Console.argv[1];

      if(!key->unbind(aname->constPtr()))
      {
         C_Printf(
            FC_ERROR "key %s not bound to %s.\n",
            kname->constPtr(),
            aname->constPtr()
         );
      }
      else
      {
         C_Printf(
            "unbound key %s from %s.\n", kname->constPtr(), aname->constPtr()
         );
      }
   }
   else if(Console.argc == 3)
   {
      qstring *aname = Console.argv[1];
      qstring *iname = Console.argv[2];
      InputAction *action = NULL;
      InputInterface *iface = input_interfaces.getInterface(iname);

      if(!iface)
      {
         C_Printf(FC_ERROR "no such interface %s", iname->constPtr());
         return;
      }

      if(!(action = iface->getAction(aname)))
      {
         C_Printf(
            FC_ERROR "%s does not support action %s.\n",
            iface->getName(),
            aname->constPtr()
         );
      }

      if(!key->unbind(action))
      {
         C_Printf(
            FC_ERROR "%s: key %s not bound to %s.\n",
            iface->getName(),
            kname->constPtr(),
            action->getName()
         );
      }
      else
      {
         C_Printf(
            "%s: unbound key %s from %s.\n",
            iface->getName(),
            kname->constPtr(),
            action->getName()
         );
      }
   }
}

CONSOLE_COMMAND(unbindall, 0)
{
   int i;

   for(i = 0; i < KBSS_NUM_KEYS; i++)
      key_bindings.getKey(i)->unbind();

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
   int i;
   KeyBind *kb;
   InputKey *key;

   for(i = 0; i < KBSS_NUM_KEYS; i++)
   {
      kb = NULL;
      key = key_bindings.getKey(i);
      while((kb = key->keyBindIterator(kb)))
         C_Printf("%s : %s\n", key->getName(), kb->getAction()->getName());
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

#endif

// EOF
