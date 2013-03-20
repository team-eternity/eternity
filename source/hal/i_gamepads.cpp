// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//    Hardware Abstraction Layer for Gamepads and Joysticks
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "../c_runcmd.h"

#include "i_gamepads.h"

// Platform-Specific Driver Modules:
#ifdef _SDL_VER
#include "../sdl/i_sdlgamepads.h"
#endif

// Globals

// current device number -- saved in config file
int i_joysticknum;

// Module-private data

// Master list of gamepads from all drivers
static PODCollection<HALGamePad *> masterGamePadList;

// Currently selected and active gamepad object, if any.
static HALGamePad *activePad;

// Generic sensitivity values, for drivers that need them
int i_joysticksens;

// haleyjd 04/15/02: joystick sensitivity variables
VARIABLE_INT(i_joysticksens, NULL, 0, 32767, NULL);

CONSOLE_VARIABLE(i_joysticksens, i_joysticksens, 0) {}

//=============================================================================
//
// HALGamePadDriver Methods
//

//
// HALGamePadDriver::addDevice
//
// Add a device to the devices collection.
//
void HALGamePadDriver::addDevice(HALGamePad *device)
{
   devices.add(device);
}

//=============================================================================
//
// HALGamePad Methods
//

IMPLEMENT_RTTI_TYPE(HALGamePad)

//=============================================================================
//
// Global Interface
//

//
// HAL Joy Driver Struct
//
struct halpaddriveritem_t
{
   int id;
   const char *name;
   HALGamePadDriver *driver;
};

static halpaddriveritem_t halPadDriverTable[] =
{
   // SDL DirectInput Driver
   {
      0,
      "SDL DirectInput",
#ifdef _SDL_VER
      &i_sdlGamePadDriver
#else
      NULL
#endif
   },

   // Terminating entry
   { -1, NULL, NULL }
};

//
// I_SelectDefaultGamePad
//
// Select the gamepad configured in the configuration file, if it can be
// found. Otherwise, nothing will happen and activePad will remain NULL.
//
bool I_SelectDefaultGamePad()
{
   HALGamePad *pad = NULL;

   // Deselect any active device first.
   if(activePad)
   {
      activePad->deselect();
      activePad = NULL;
   }

   if(i_joysticknum >= 0)
   {
      // search through the master directory for a pad with this number
      PODCollection<HALGamePad *>::iterator itr = masterGamePadList.begin();

      for(; itr != masterGamePadList.end(); itr++)
      {
         if((*itr)->num == i_joysticknum)
         {
            pad = *itr; // found it.
            break;
         }
      }
   }

   // Select the device if it was found.
   if(pad)
   {      
      if(pad->select())
         activePad = pad;
   }

   return (activePad != NULL);
}

//
// I_EnumerateGamePads
//
// Enumerate all gamepads.
//
void I_EnumerateGamePads()
{
   // Have all drivers re-enumerate their pads
   halpaddriveritem_t *item = halPadDriverTable;

   while(item->name)
   {
      if(item->driver)
         item->driver->enumerateDevices();
      ++item;
   }

   // build the master pad directory
   masterGamePadList.clear();

   item = halPadDriverTable;
   while(item->name)
   {
      if(item->driver)
      {
         PODCollection<HALGamePad *> &pads = item->driver->devices;
         PODCollection<HALGamePad *>::iterator itr = pads.begin();

         for(; itr != pads.end(); itr++)
            masterGamePadList.add(*itr);
      }
      ++item;
   }
}

//
// I_InitGamePads
//
// Call at startup to initialize the gamepad/joystick HAL. All modules
// implementing gamepad drivers in the current build will be initialized in
// turn and the master list of available devices will be built.
//
void I_InitGamePads()
{
   // Initialize all supported gamepad drivers
   halpaddriveritem_t *item = halPadDriverTable;

   while(item->name)
   {
      if(item->driver)
         item->driver->initialize();
      ++item;
   }

   // Enumerate gamepads in all supported drivers and build the master lookup
   I_EnumerateGamePads();

   // Select default pad
   I_SelectDefaultGamePad();
}

//
// I_ShutdownGamePads
//
// Call at shutdown to clean up all gamepad drivers.
//
void I_ShutdownGamePads()
{
   halpaddriveritem_t *item = halPadDriverTable;

   while(item->name)
   {
      if(item->driver)
         item->driver->shutdown();
      ++item;
   }
}

//
// I_PollActiveGamePad
//
// Get input from the currently active gamepad, if any.
// Returns NULL if there is no device active.
//
HALGamePad::padstate_t *I_PollActiveGamePad()
{
   if(activePad)
   {
      activePad->poll();
      return &activePad->state;
   }
   else
      return NULL;
}

// haleyjd 04/15/02: windows joystick commands
CONSOLE_COMMAND(i_joystick, 0)
{
   if(Console.argc != 1)
      return;

   i_joysticknum = Console.argv[0]->toInt();
   I_SelectDefaultGamePad();
}

// EOF

