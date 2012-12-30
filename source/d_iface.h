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

#ifndef D_IFACE_H__
#define D_IFACE_H__

// [CG] FIXME: This module needs e_hash, which needs <string.h>.  As far as I
//             can tell, either I add z_zone.h here or I add it everywhere
//             else.

#include "z_zone.h"

#include "e_hash.h"
#include "g_bind.h"

// The current input handling interface
enum
{
   ii_none         = 0,  // No interface
   ii_demoscreen   = 1,  // Demoscreen interface
   ii_console      = 2,  // Console interface
   ii_menu         = 4,  // Menu interface
   ii_game         = 8,  // Game interface
   ii_automap      = 16, // Automap interface
   ii_intermission = 32, // Intermission interface
   ii_finale       = 64, // Finale interface
};

#define ii_all (ii_demoscreen | ii_console | ii_menu | ii_game | ii_automap | \
                ii_intermission | ii_finale)

#define ii_level (ii_game | ii_automap)
#define ii_playing (ii_game | ii_automap | ii_intermission | ii_finale)

class InputInterface
{
protected:
   const char     *name;
   int             handler_num;
   int             accepted_events;
   bool            active;
   InputInterface *parent;

   EHashTable<InputAction, ENCStringHashKey, &InputAction::key,
              &InputAction::links> *actions;

   virtual void preprocessKeyEvent(event_t *ev);
   virtual void preprocessMouseEvent(event_t *ev);
   virtual void preprocessJoystickEvent(event_t *ev);

   void preprocessEvent(event_t *ev);
   void registerAction(const char *action_name);

public:

   InputInterface(const char *new_name, int new_handler_num,
                  int new_accepted_events);
   ~InputInterface();

   virtual void init() {}
   virtual void draw() {}
   virtual void tick() {}

   virtual bool isFullScreen() const;

   virtual void activate();
   virtual void deactivate();
   virtual void toggle();
   virtual void registerHandledActions();

   virtual bool handleEvent(event_t *ev);
   virtual void setParent(InputInterface *new_parent);

   bool         handlesEvent(event_t *ev)               const;
   bool         supportsAction(const char *action_name) const;
   bool         supportsAction(qstring *action_name)    const;
   bool         supportsAction(InputAction *action)     const;
   bool         actionIsActive(const char *action_name) const;
   InputAction* getAction(const char *action_name)      const;
   InputAction* getAction(qstring *action_name)         const;
   InputAction* actionIterator(InputAction *action)     const;

   bool            isUpFront()      const;
   bool            isActive()       const;
   bool            isSubInterface() const;
   InputInterface* getParent()      const;
   const char*     getName()        const;
   int             getNumber()      const;

   void handleBindCommand(qstring *key_name, qstring *action_names,
                          bool print_error);
   void deactivateAction(const char *action_name);
   bool checkAction(const char *action_name);
   bool checkAndClearAction(const char *action_name);
   bool checkAndClearActions(const char *a1_name, const char *a2_name);

   bool operator == (const char *other)           const;
   bool operator == (const int other)             const;
   bool operator == (const InputInterface *other) const;
   bool operator == (const InputInterface& other) const;
   bool operator != (const char *other)           const;
   bool operator != (const int other)             const;
   bool operator != (const InputInterface *other) const;
   bool operator != (const InputInterface& other) const;
};

class InputInterfaceGroup
{
public:
   InputInterface*  getFrontInterface();
   InputInterface*  getFrontNonConsoleInterface();
   void             registerInterface(InputInterface *interface);
   InputInterface*  getInterface(qstring *interface_name);
   InputInterface*  getInterface(qstring& interface_name);
   InputInterface*  getInterface(const char *interface_name);
   InputInterface*  getInterface(int interface_number);
   InputInterface** begin();
   InputInterface** end();
   void             deactivateAll();
};

extern InputInterfaceGroup input_interfaces;

#endif

// EOF
