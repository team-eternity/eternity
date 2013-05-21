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

#ifndef G_BIND_H__
#define G_BIND_H__

#define KBSS_NUM_KEYS 256

struct event_t;

class bindHashPimpl;
class keyHashPimpl;

class qstring;
class InputAction;
class InputInterface;
class KeyBindingsSubSystem;

class KeyBind : public ZoneObject
{
public:
   // [bind +a "+left"]
   static uint8_t const activates_on_press                            = 1;

   // [bind -a "+left"]
   static uint8_t const activates_on_release                          = 2;

   // [bind a "+left"]
   static uint8_t const activates_on_press_and_release                = 3;

   // [bind +a "-left"]
   static uint8_t const deactivates_on_press                          = 4;

   // [bind -a "-left"]
   static uint8_t const deactivates_on_release                        = 5;

   // [bind a "-left"]
   static uint8_t const deactivates_on_press_and_release              = 6;

   // [bind a "left"]
   static uint8_t const activates_on_press_and_deactivates_on_release = 7;

   // [bind a "!left"]
   static uint8_t const deactivates_on_press_and_activates_on_release = 8;

   // For convenience
   static uint8_t const normal = activates_on_press_and_deactivates_on_release;
   static uint8_t const reverse = deactivates_on_press_and_activates_on_release;

private:
   uint8_t      type;
   InputAction *action;

public:
   char *key;

   DLListItem<KeyBind> links;

   KeyBind(InputAction *new_action);
   KeyBind(InputAction *new_action, uint8_t new_type);
   ~KeyBind();

   InputAction* getAction()   const;
   uint8_t      getType()     const;

   void setType(uint8_t new_type);
};

class InputKey : public ZoneObject
{
private:

   int number;
   char *name;
   uint16_t bind_count;

   bindHashPimpl *pImpl;

   KeyBind* getBind(InputAction *action)                         const;
   void     writeBindingsOfType(qstring &buf, uint8_t bind_type) const;

public:

   char *key;

   DLListItem<InputKey> links;

   InputKey(int new_number, const char *new_name);
   ~InputKey();

   /* [CG] InputKeys can be tricky, here's a cheat sheet.
    *
    *      Each interface has a list of supported actions.  For an example,
    *      let's use Menu and Game.  Menu supports the "screenshot" action,
    *      and Game does as well.  It's likely that the same key is bound to
    *      the "screenshot" action in both interfaces, let's say that key is
    *      the "Print Screen" key.  That means that the "Print Screen" InputKey
    *      has two KeyBinds inside of it, one bound to the "screenshot" action
    *      in the Game interface, and one bound to the "screenshot" action in
    *      the Menu interface.
    *
    *      In the Keybindings menu, none of the actions currently specify which
    *      interface to which they belong.  They tell the key bindings sub-
    *      system to bind/unbind key "x" to/from action "y".  In the case of
    *      binding, this does exactly that, "x" is bound to "y" in ALL
    *      interfaces.  If "x" is bound to "y" in ANY interface, "x" is unbound
    *      from "y" in ALL interfaces.
    *
    *      This brings up the convention of using string action names vs.
    *      pointers to InputAction instances.  InputAction instances contain
    *      their supporting interface; in the above example, there are two
    *      separate "screenshot" InputAction instances, one bound to the Menu
    *      interface and another bound to the Game interface.
    *
    *      Using string action names implies that whatever method you are
    *      invoking will be applied across ALL interfaces supporting that
    *      action.  Using a InputAction pointer implies that whatever method
    *      you are invoking applies just to that action inside its interface.
    *      And when you specify an interface (like in
    *      InputKey::unbind(const char *action_name, InputInterface *iface)),
    *      you imply that you expect the method to only apply that action
    *      inside that interface.
    */

   const char* getName()                              const;
   int         getNumber()                            const;
   bool        isBoundTo(const char *action_name)     const;
   bool        isBoundTo(InputAction *action)         const;
   bool        isOnlyBoundTo(const char *action_name) const;
   KeyBind*    keyBindIterator(KeyBind *kb)           const;
   void        writeBindings(qstring &buf)            const;

   void bind(InputAction *action);
   void bind(InputAction *action, uint8_t type);
   void bind(const char *action_name);
   void bind(const char *action_name, uint8_t type);
   bool unbind();
   bool unbind(const char *action_name);
   bool unbind(const char *action_name, InputInterface *iface);
   bool unbind(InputAction *action);
   void handleEvent(event_t *ev, InputInterface *iface);
   void activateAllBinds();
   void deactivateAllBinds();

   bool operator == (const char *key_name)     const;
   bool operator == (unsigned char key_number) const;
   bool operator != (const char *key_name)     const;
   bool operator != (unsigned char key_number) const;
};

class InputAction : public ZoneObject
{
private:
   char *name;
   InputInterface *iface;
   int activation_count;
   bool activating_keys[KBSS_NUM_KEYS];

public:
   char *key;
   DLListItem<InputAction> links;

   // [CG] TODO: Add help information.

   InputAction(const char *new_name, InputInterface *new_iface);
   ~InputAction();

   const char*     getName()              const;
   InputInterface* getInterface()         const;
   const char*     getBoundKeyNames()     const;
   bool            isActive()             const;
   const char*     getFirstBoundKeyName() const;

   void activate();
   void activate(InputKey *key);
   void deactivate();
   void deactivate(InputKey *key);
};

class KeyBindingsSubSystem
{
private:

   InputKey *keys[KBSS_NUM_KEYS];

   keyHashPimpl *pImpl;

   // name of configuration file to read from/write to.
   char *cfg_file;

public:

   bool altdown;
   bool ctrldown;
   bool shiftdown;

   KeyBindingsSubSystem();

   void initialize();

   const char* getBindingActionName()       const;
   const char* getBindingInterfaceName()    const;
   InputKey*   getKeyFromEvent(event_t *ev) const;
   InputKey*   getKey(int index)            const;
   InputKey*   getKey(const char *key_name) const;
   InputKey*   getKey(qstring *key_name)    const;
   void        writeActions(qstring &buf)   const;

   const char* getKeysBoundToAction(const char *action_name,
                                    const char *interface_name) const;

   void loadKeyBindings();
   void saveKeyBindings();
   void clearKeyStates();

   void setKeyBindingsFile(const char *filename);
   void startMenuBind(const char *new_binding_action_name,
                      const char *new_binding_interface_name);
   bool handleEvent(event_t *ev);
};

extern KeyBindingsSubSystem key_bindings;

void MN_EditKeyBinding(const char *action_name, const char *interface_name);
void MN_BindDrawer();
bool MN_BindResponder(event_t *ev);

void G_Bind_AddCommands();

#endif

// EOF
