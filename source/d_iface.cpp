// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Input interfaces and event handling.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_event.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_hash.h"
#include "g_bind.h"
#include "g_game.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "mn_engin.h"
#include "s_sound.h"
#include "v_misc.h"

extern bool menu_toggleisback;

#define MAX_LOAD_FACTOR 0.7
#define INITIAL_ACTION_CHAIN_SIZE 127

static DLListItem<InputInterface> *active_interfaces = NULL;
static Collection<InputInterface*> interfaces;

InputInterfaceGroup input_interfaces;

static uint8_t D_getBindType(const char *key_name, const char *action_name)
{
   bool key_starts_with_plus = false;
   bool key_starts_with_minus = false;
   bool action_starts_with_plus = false;
   bool action_starts_with_minus = false;
   bool action_starts_with_bang = false;
   size_t key_name_length = strlen(key_name);
   size_t action_name_length = strlen(action_name);

   if(key_name_length > 1)
   {
      if(*key_name == '+')
         key_starts_with_plus = true;
      else if(*key_name == '-')
         key_starts_with_minus = true;
   }

   if(*action_name == '+')
      action_starts_with_plus = true;
   else if(*action_name == '-')
      action_starts_with_minus = true;
   else if(*action_name == '!')
      action_starts_with_bang = true;

   if(key_starts_with_plus)
   {
      if(action_starts_with_plus)
         return KeyBind::activates_on_press;
      else if(action_starts_with_minus)
         return KeyBind::deactivates_on_press;
      else if(action_starts_with_bang)
         return 0;
   }
   else if(key_starts_with_minus)
   {
      if(action_starts_with_plus)
         return KeyBind::activates_on_release;
      else if(action_starts_with_minus)
         return KeyBind::deactivates_on_release;
      else if(action_starts_with_bang)
         return 0;
   }
   else if(action_starts_with_plus)
      return KeyBind::activates_on_press_and_release;
   else if(action_starts_with_minus)
      return KeyBind::deactivates_on_press_and_release;
   else if(action_starts_with_bang)
      return KeyBind::reverse;
   else
      return KeyBind::normal;

   return 0;
}

InputInterface* InputInterfaceGroup::getFrontInterface()
{
   if(active_interfaces)
      return active_interfaces->dllObject;

   return NULL;
}

InputInterface* InputInterfaceGroup::getFrontNonConsoleInterface()
{
   InputInterface *front_interface = getFrontInterface();

   if((*front_interface) != ii_console)
      return front_interface;
   else if(active_interfaces && active_interfaces->dllNext)
      return active_interfaces->dllNext->dllObject;
   else
      return NULL;
}

void InputInterfaceGroup::registerInterface(InputInterface *interface)
{
   if(!interfaces.isEmpty()) // [CG] Don't register an interface multiple times
   {
      Collection<InputInterface *>::iterator iitr;

      for(iitr = interfaces.begin(); iitr != interfaces.end(); iitr++)
      {
         if((**iitr) == interface)
            return; // [CG] Interface already registered.
      }
   }

   interfaces.add(interface);
}

InputInterface* InputInterfaceGroup::getInterface(qstring *interface_name)
{
   return getInterface(interface_name->constPtr());
}

InputInterface* InputInterfaceGroup::getInterface(qstring& interface_name)
{
   return getInterface(interface_name.constPtr());
}

InputInterface* InputInterfaceGroup::getInterface(const char *interface_name)
{
   if(!interfaces.isEmpty())
   {
      Collection<InputInterface *>::iterator iitr;

      for(iitr = interfaces.begin(); iitr != interfaces.end(); iitr++)
      {
         if((**iitr) == interface_name)
            return *iitr;
      }
   }

   return NULL;
}

InputInterface* InputInterfaceGroup::getInterface(int interface_number)
{
   if(!interfaces.isEmpty())
   {
      Collection<InputInterface *>::iterator iitr;

      for(iitr = interfaces.begin(); iitr != interfaces.end(); iitr++)
      {
         if((**iitr) == interface_number)
            return *iitr;
      }
   }

   return NULL;
}

InputInterface** InputInterfaceGroup::begin()
{
   return interfaces.begin();
}

InputInterface** InputInterfaceGroup::end()
{
   return interfaces.end();
}

void InputInterfaceGroup::deactivateAll()
{
   Collection<InputInterface *>::iterator iitr;

   for(iitr = interfaces.begin(); iitr != interfaces.end(); iitr++)
      (*iitr)->deactivate();
}

InputInterface::InputInterface(const char *new_name, int new_handler_num,
                               int new_accepted_events)
{
   name = new_name;
   handler_num = new_handler_num;
   accepted_events = new_accepted_events;
   active = false;
   parent = NULL;

   actions = new EHashTable<
      InputAction, ENCStringHashKey, &InputAction::key, &InputAction::links
   >(INITIAL_ACTION_CHAIN_SIZE);

   registerHandledActions();
}

InputInterface::~InputInterface() {}

void InputInterface::preprocessKeyEvent(event_t *ev)
{
   InputKey *key = NULL;

   if((ev->type != ev_keydown) && (ev->type != ev_keyup))
      return;

   key = key_bindings.getKeyFromEvent(ev);

   if(!key)
      return;

   key->handleEvent(ev, this);
}

void InputInterface::preprocessMouseEvent(event_t *ev)
{
}

void InputInterface::preprocessJoystickEvent(event_t *ev)
{
}

void InputInterface::preprocessEvent(event_t *ev)
{
   if((ev->type == ev_keydown) || (ev->type == ev_keyup))
      preprocessKeyEvent(ev);
   else if(ev->type == ev_mouse)
      preprocessMouseEvent(ev);
   else if(ev->type == ev_joystick)
      preprocessJoystickEvent(ev);
   else
   {
      I_Error(
         "InputInterface::preprocessEvent: Unknown event type %d.\n", ev->type
      );
   }
}

void InputInterface::registerAction(const char *action_name)
{
   if(actions->objectForKey(action_name)) // [CG] 1 registration per action
   {
      I_Error(
         "InputInterface::registerAction: %s: Action %s already registered.\n",
         name,
         action_name
      );
   }

   actions->addObject(new InputAction(action_name, this));

   if(actions->getLoadFactor() > MAX_LOAD_FACTOR)
      actions->rebuild(actions->getNumChains() * 2);
}

bool InputInterface::isFullScreen() const
{
   return false;
}

void InputInterface::activate()
{
   if(!active)
   {
      DLListItem<InputInterface> *node = new DLListItem<InputInterface>;
      node->insert(this, &active_interfaces);
      active = true;
   }
}

void InputInterface::deactivate()
{
   bool removed_interface = false;
   DLListItem<InputInterface> *node;

   if(active)
   {
      for(node = active_interfaces; node; node = node->dllNext)
      {
         if(node->dllObject == this)
         {
            node->remove();
            delete node;
            removed_interface = true;
            break;
         }
      }

      active = false;

      key_bindings.clearKeyStates();
   }
}

void InputInterface::toggle()
{
   if(active)
      deactivate();
   else
      activate();
}

void InputInterface::registerHandledActions()
{
   registerAction("toggle_menu");
   registerAction("help_screen");
   registerAction("setup");
   registerAction("screenshot");
}

bool InputInterface::handleEvent(event_t *ev)
{
   preprocessEvent(ev);

   if(checkAndClearAction("toggle_menu")) // toggle menu
   {
      // start up main menu or kill menu
      if(Menu.isUpFront()) // If Menu is active, either go back a menu or close
      {
         if(menu_toggleisback)
         {
            // haleyjd 02/12/06: allow zdoom-style navigation
            MN_PrevMenu();
         }
         else
         {
            Menu.deactivate();
            S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
         }
      }
      else // Menu was not active, so activate it.
         MN_StartControlPanel();

      return true;
   }

   if(checkAndClearAction("help_screen"))
   {
      C_RunTextCmd("help");
      return true;
   }

   if(checkAndClearAction("setup"))
   {
      C_RunTextCmd("mn_options");
      return true;
   }

   if(checkAndClearAction("screenshot"))
   {
      G_ScreenShot();
      return true;
   }

   return false;
}

void InputInterface::setParent(InputInterface *new_parent)
{
   parent = new_parent;
}

bool InputInterface::handlesEvent(event_t *ev) const
{
   if((accepted_events & ev->type) == 0)
      return false;

   return true;
}

bool InputInterface::supportsAction(const char *action_name) const
{
   if(actions->objectForKey(action_name))
      return true;

   return false;
}

bool InputInterface::supportsAction(qstring *action_name) const
{
   if(actions->objectForKey(action_name->constPtr()))
      return true;

   return false;
}

bool InputInterface::supportsAction(InputAction *action) const
{
   if(actions->objectForKey(action->getName()))
      return true;

   return false;
}

bool InputInterface::actionIsActive(const char *action_name) const
{
   InputAction *action = actions->objectForKey(action_name);

   if(!action)
   {
      I_Error(
         "InputInterface::actionIsActive: No such action %s in "
         "the %s interface.\n",
         action_name,
         name
      );
   }

   return action->isActive();
}

InputAction* InputInterface::getAction(const char *action_name) const
{
   return actions->objectForKey(action_name);
}

InputAction* InputInterface::getAction(qstring *action_name) const
{
   return actions->objectForKey(action_name->constPtr());
}

InputAction* InputInterface::actionIterator(InputAction *action) const
{
   return actions->tableIterator(action);
}

bool InputInterface::isUpFront() const
{
   if(active_interfaces && active_interfaces->dllObject == this)
      return true;

   return false;
}

bool InputInterface::isActive() const
{
   return active;
}

bool InputInterface::isSubInterface() const
{
   if(parent)
      return true;

   return false;
}

InputInterface* InputInterface::getParent() const
{
   return parent;
}

const char* InputInterface::getName() const
{
   return name;
}

int InputInterface::getNumber() const
{
   return handler_num;
}

void InputInterface::handleBindCommand(qstring *key_name,
                                       qstring *action_names,
                                       bool print_error)
{
   size_t key_name_length = key_name->length();
   size_t action_names_length = action_names->length();
   qstring token;
   InputKey *key;
   InputAction *action;
   const char *base_key_name;
   const char *action_name;
   uint8_t type;

   type = D_getBindType(key_name->constPtr(), action_names->constPtr());

   base_key_name = key_name->constPtr();

   // [CG] If the interface doesn't handle key presses, but the bind only
   //      activates on a key press, don't bind it.
   if(((type == KeyBind::activates_on_press) ||
       (type == KeyBind::deactivates_on_press)) &&
      ((accepted_events & ev_keydown) == 0))
   {
      if(print_error)
         C_Printf(FC_ERROR "%s does not support press-only binds.\n", name);
      return;
   }

   if(((type == KeyBind::activates_on_release) ||
       (type == KeyBind::deactivates_on_release)) &&
      ((accepted_events & ev_keyup) == 0))
   {
      if(print_error)
         C_Printf(FC_ERROR "%s does not support release-only binds.\n", name);
      return;
   }

   if((type == KeyBind::activates_on_press)  ||
      (type == KeyBind::activates_on_release) ||
      (type == KeyBind::deactivates_on_press) ||
      (type == KeyBind::deactivates_on_release))
   {
      base_key_name++;
   }

   if(!(key = key_bindings.getKey(base_key_name)))
   {
      if(print_error)
         C_Printf(FC_ERROR "Key %s not found.\n", base_key_name);
      return;
   }

   for(size_t i = 0; i < action_names_length; i++)
   {
      char c = action_names->charAt(i);

      if(c == '\"')
         continue;

      if(c == ';' || i == (action_names_length - 1))
      {

         if((i == (action_names_length - 1)))
            token << c;

         if(token.length())
         {
            action_name = token.constPtr();
            type = D_getBindType(key_name->constPtr(), action_name);

            if(!type)
            {
               C_Printf(FC_ERROR "Invalid bind syntax.");
               continue;
            }

            if((type == KeyBind::activates_on_press)             ||
               (type == KeyBind::activates_on_release)           ||
               (type == KeyBind::activates_on_press_and_release) ||
               (type == KeyBind::deactivates_on_press)           ||
               (type == KeyBind::deactivates_on_release)         ||
               (type == KeyBind::deactivates_on_press_and_release))
            {
               action_name++;
            }

            if(!(action = actions->objectForKey(action_name)))
            {
               if(print_error)
                  C_Printf(FC_ERROR "Action %s not found.\n", action_name);
               continue;
            }

            key->bind(action, type);

            token.clear();
         }
      }
      else if(i < (action_names_length - 1))
         token << c;
   }
}

void InputInterface::deactivateAction(const char *action_name)
{
   InputAction *action = getAction(action_name);

   if(!action)
   {
      I_Error(
         "InputInterface::deactivateAction: %s: Action %s not supported.\n",
         name,
         action_name
      );
   }

   action->deactivate();
}

bool InputInterface::checkAction(const char *action_name)
{
   return actionIsActive(action_name);
}

bool InputInterface::checkAndClearAction(const char *action_name)
{
   bool was_active = actionIsActive(action_name);

   deactivateAction(action_name);
   return was_active;
}

bool InputInterface::checkAndClearActions(const char *a1_name,
                                          const char *a2_name)
{
   bool was_active = actionIsActive(a1_name);

   if(!was_active)
      was_active = actionIsActive(a2_name);

   deactivateAction(a1_name);
   deactivateAction(a2_name);

   return was_active;
}

bool InputInterface::operator == (const char *other) const
{
   if((!strcasecmp((this)->getName(), other)))
      return true;

   return false;
}

bool InputInterface::operator == (const int other) const
{
   if(other == handler_num)
      return true;

   return false;
}

bool InputInterface::operator == (const InputInterface *other) const
{
   if((!(strcasecmp((this)->getName(), other->getName()))) &&
      ((this)->getNumber() == other->getNumber()))
      return true;

   return false;
}

bool InputInterface::operator == (const InputInterface& other) const
{
   if((!(strcasecmp((this)->getName(), other.getName()))) &&
      ((this)->getNumber() == other.getNumber()))
      return true;

   return false;
}

bool InputInterface::operator != (const char *other) const
{
   if((strcasecmp((this)->getName(), other)))
      return true;

   return false;
}

bool InputInterface::operator != (const int other) const
{
   if(other != handler_num)
      return true;

   return false;
}

bool InputInterface::operator != (const InputInterface *other) const
{
   if(((strcasecmp((this)->getName(), other->getName()))) ||
      ((this)->getNumber() != other->getNumber()))
      return true;

   return false;
}

bool InputInterface::operator != (const InputInterface& other) const
{
   if(((strcasecmp((this)->getName(), other.getName()))) &&
      ((this)->getNumber() != other.getNumber()))
      return true;

   return false;
}

