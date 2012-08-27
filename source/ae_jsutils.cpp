// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//----------------------------------------------------------------------------
//
// Aeon Engine - SpiderMonkey JavaScript Engine Utilities
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifdef EE_FEATURE_AEONJS

#include "z_zone.h"

#include "ae_jsengine.h"
#include "hal/i_platform.h"
#include "m_qstr.h"

// JSAPI Configuration
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
#include "ae_jscfgw32.h"
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
#include "ae_jscfglinux.h"
#endif

// JSAPI
#include "../js/src/jsapi.h"

//
// AeonJS_SafeGetStringBytes
//
// Takes some of the pain out of converting jsval to JSString by handling
// return value checks and GC rooting. "root" is NOT an optional parameter!
//
const char *AeonJS_SafeGetStringBytes(JSContext *cx, jsval value, jsval *root)
{
   const char *retval = "";
   JSString   *jstr   = JS_ValueToString(cx, value);

   if(jstr)
   {
      *root  = STRING_TO_JSVAL(jstr);
      retval = JS_GetStringBytes(jstr);
   }

   return retval;
}

//
// AeonJS_SafeInstanceOf
//
// Test if a jsval is an object, and if so, if it is an instance of the
// provided JSClass.
//
bool AeonJS_SafeInstanceOf(JSContext *cx, JSClass *jsClass, jsval val)
{
   JSObject *obj;

   return (JSVAL_IS_OBJECT(val) && 
           (obj = JSVAL_TO_OBJECT(val)) &&
           JS_InstanceOf(cx, obj, jsClass, NULL));
}

#endif

// EOF

