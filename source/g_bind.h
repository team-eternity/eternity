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
// Rewritten to support press/release/activate/deactivate only bindings,
// as well as to allow a key to be bound to multiple actions in the same
// key action category.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#ifndef G_BIND_H__
#define G_BIND_H__

#include "e_hash.h"
#include "mn_engin.h"

#define KBSS_NUM_KEYS 256
#define KBSS_INITIAL_KEY_ACTION_CHAIN_SIZE 127
#define KBSS_MAX_LOAD_FACTOR 0.7

// haleyjd 07/03/04: key binding classes
// [CG] Renamed to key binding "category" to avoid collision with C++ keyword.
enum input_action_category_e
{
   kac_none    =  1,
   kac_player  =  2,      // player  bindings -- handled by G_BuildTiccmd
   kac_menu    =  4,      // menu    bindings -- handled by MN_Responder
   kac_map     =  8,      // map     bindings -- handled by AM_Responder
   kac_console = 16,      // console bindings -- handled by C_Responder
   kac_hud     = 32,      // hud     bindings -- handled by HU_Responder
   kac_command = 64,      // command bindings -- handled by C_RunTextCmd
   kac_max,
};

class KeyBind : public ZoneObject
{
private:
   char *hidden_key_name;
   char *hidden_action_name;
   int key_number;
   input_action_category_e category;
   bool pressed;
   unsigned short flags;

public:
   static short const activate_only   = 1; // key only activates the action
   static short const deactivate_only = 2; // key only deactivates the action
   static short const press_only      = 4; // only triggered by a key press
   static short const release_only    = 8; // only triggered by a key release

   DLListItem<KeyBind> key_to_action_links;
   DLListItem<KeyBind> action_to_key_links;
   DLListItem<KeyBind> unbind_links;

   const char *keys_to_actions_key;
   const char *actions_to_keys_key;

   KeyBind(int new_key_number, const char *new_key_name,
           const char *new_action_name, input_action_category_e new_category,
           unsigned short new_flags);
   ~KeyBind();

   int                           getKeyNumber()                    const;
   const char*                   getKeyName()                      const;
   const char*                   getActionName()                   const;
   const bool                    isPressed()                       const;
   const bool                    isReleased()                      const;
   void                          press();
   void                          release();
   const input_action_category_e getCategory()                     const;
   const unsigned short          getFlags()                        const;
   bool                          isNormal()                        const;
   bool                          isActivateOnly()                  const;
   bool                          isDeactivateOnly()                const;
   bool                          isPressOnly()                     const;
   bool                          isReleaseOnly()                   const;
   bool                          keyIs(const char *key_name)       const;
   bool                          actionIs(const char *action_name) const;

};

class InputKey : public ZoneObject
{
private:
   char *hidden_name;
   int number;
   bool disabled;

public:
   DLListItem<InputKey> links;
   const char *key;

   InputKey(int new_number, const char *new_name);
   ~InputKey();
   const char* getName()    const;
   int         getNumber()  const;
   bool        isDisabled() const;
   void        disable();
   void        enable();

};

//
// InputAction
//
class InputAction : public ZoneObject
{
protected:
   char *hidden_name;
   input_action_category_e category;
   char *bound_keys_description;

public:
   DLListItem<InputAction> links;
   const char *key;

   InputAction(const char *new_name, input_action_category_e new_category);
   ~InputAction();

   bool                          handleEvent(event_t *ev, KeyBind *kb);
   const char*                   getName()                             const;
   const input_action_category_e getCategory()                         const;
   const char*                   getDescription()                      const;
   void                          setDescription(const char *new_description);

   virtual void                  activate(KeyBind *kb, event_t *ev);
   virtual void                  deactivate(KeyBind *kb, event_t *ev);
   virtual void                  print()                               const;
   virtual bool                  isActive()                            const;
   virtual bool                  mayActivate(KeyBind *kb)              const;
   virtual bool                  mayDeactivate(KeyBind *kb)            const;
   virtual void                  Think();

};

class KeyBindingsSubSystem
{
private:

   InputKey *keys[KBSS_NUM_KEYS];

   EHashTable<InputKey, ENCStringHashKey, &InputKey::key,
              &InputKey::links> *names_to_keys;

   EHashTable<InputAction, ENCStringHashKey, &InputAction::key,
              &InputAction::links> *names_to_actions;

   EHashTable<KeyBind, ENCStringHashKey, &KeyBind::keys_to_actions_key,
              &KeyBind::key_to_action_links> *keys_to_actions;

   EHashTable<KeyBind, ENCStringHashKey, &KeyBind::actions_to_keys_key,
              &KeyBind::action_to_key_links> *actions_to_keys;

   // name of configuration file to read from/write to.
   char *cfg_file;

   // name of action we are editing
   const char *binding_action;

   void updateBoundKeyDescription(InputAction *action);
   void bindKeyToAction(InputKey *key, const char *action_name,
                        unsigned short flags);

public:

   static menuwidget_t binding_widget;

   KeyBindingsSubSystem();

   static int getCategoryIndex(int category);

   void         initialize();
   void         setKeyBindingsFile(const char *filename);
   void         setBindingAction(const char *new_binding_action);
   const char*  getBindingAction();
   const char*  getBoundKeys(const char *action_name);
   const char*  getFirstBoundKey(const char *action_name);
   void         reEnableKeys();
   void         reset();
   bool         handleKeyEvent(event_t *ev, int categories);
   void         runInputActions();
   void         loadKeyBindings();
   void         saveKeyBindings();
   InputKey*    getKey(int index);
   InputKey*    getKey(const char *key_name);
   InputAction* getAction(const char *action_name);
   KeyBind*     keyBindIterator(KeyBind *kb);
   KeyBind*     keyBindIterator(KeyBind *kb, const char *key_name);
   InputAction* actionIterator(InputAction *action);
   void         removeBind(KeyBind **kb);
   void         createBind(InputKey *key, InputAction *action,
                           unsigned short flags);
   void         bindKeyToActions(const char *key_name,
                                 const qstring &action_names);

};

extern KeyBindingsSubSystem key_bindings;

void        G_EditBinding(const char *action_name);
void        G_Bind_AddCommands();
void        G_BindDrawer();
bool        G_BindResponder(event_t *ev);

// action variables

extern int action_forward;
extern int action_backward;
extern int action_left;
extern int action_right;
extern int action_moveleft;
extern int action_moveright;
extern int action_lookup;
extern int action_lookdown;
extern int action_flyup;
extern int action_flydown;
extern int action_flycenter;
extern int action_use;
extern int action_speed;
extern int action_attack;
extern int action_strafe;
extern int action_flip;
extern int action_jump;
extern int action_autorun;

extern int action_mlook;
extern int action_center;
extern int action_weapon1;
extern int action_weapon2;
extern int action_weapon3;
extern int action_weapon4;
extern int action_weapon5;
extern int action_weapon6;
extern int action_weapon7;
extern int action_weapon8;
extern int action_weapon9;
extern int action_nextweapon;
extern int action_weaponup;
extern int action_weapondown;


extern int action_frags;

extern int action_menu_help;
extern int action_menu_toggle;
extern int action_menu_setup;
extern int action_menu_up;
extern int action_menu_down;
extern int action_menu_confirm;
extern int action_menu_previous;
extern int action_menu_left;
extern int action_menu_right;
extern int action_menu_pageup;
extern int action_menu_pagedown;
extern int action_menu_contents;

extern int action_map_toggle;
extern int action_map_right;
extern int action_map_left;
extern int action_map_up;
extern int action_map_down;
extern int action_map_zoomin;
extern int action_map_zoomout;
extern int action_map_gobig;
extern int action_map_follow;
extern int action_map_mark;
extern int action_map_clear;
extern int action_map_grid;

extern int action_console_pageup;
extern int action_console_pagedown;
extern int action_console_toggle;
extern int action_console_tab;
extern int action_console_enter;
extern int action_console_up;
extern int action_console_down;
extern int action_console_backspace;

#endif

// EOF
