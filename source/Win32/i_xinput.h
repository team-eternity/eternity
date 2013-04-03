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
// XInput Gamepad Support
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef I_XINPUT_H__
#define I_XINPUT_H__

#ifdef EE_FEATURE_XINPUT

#include "../hal/i_gamepads.h"

class XInputGamePadDriver : public HALGamePadDriver
{
public:
   virtual bool initialize();
   virtual void shutdown();
   virtual void enumerateDevices();
   virtual int  getBaseDeviceNum() { return 0; }
};

extern XInputGamePadDriver i_xinputGamePadDriver;

class XInputGamePad : public HALGamePad
{
   DECLARE_RTTI_TYPE(XInputGamePad, HALGamePad)

protected:
   unsigned long dwUserIndex;

public:
   XInputGamePad(unsigned long userIdx = 0);

   virtual bool select();
   virtual void deselect();
   virtual void poll();
};

#endif

#endif

// EOF

