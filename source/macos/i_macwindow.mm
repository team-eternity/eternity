//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Ioan Chera, et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// Purpose: macOS Objective-C specific Window functions 
// Authors: Ioan Chera
//

#import <AppKit/AppKit.h>

#include "SDL_config_macosx.h"
#include "SDL.h"
#include "SDL_syswm.h"

#include "../z_zone.h"

bool I_IsMacViewOccluded(SDL_Window *window)
{
   if(!window)
      return true;
   
   edefstructvar(SDL_SysWMinfo, info);
   SDL_VERSION(&info.version);
   
   if(!SDL_GetWindowWMInfo(window, &info))
   {
      NSLog(@"%s: get WM info failed: %s", __func__, SDL_GetError());
      return false;
   }
   if(info.subsystem != SDL_SYSWM_COCOA)
   {
      NSLog(@"%s: bad subsystem %d", __func__, info.subsystem);
      return false;
   }
   
   NSWindow *macwindow = info.info.cocoa.window;
   return !(macwindow.occlusionState & NSWindowOcclusionStateVisible);
}

// EOF
