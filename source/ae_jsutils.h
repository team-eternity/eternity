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

#ifndef AE_JSUTILS_H__
#define AE_JSUTILS_H__

#ifdef EE_FEATURE_AEONJS

const char *AeonJS_GetStringBytesSafe(JSContext *cx, jsval value, jsval *root);

//
// AeonJS_GetPrivate
//
// Retrieve the private data pointer from a particular JSObject
//
template<typename T> inline T *AeonJS_GetPrivate(JSContext *cx, JSObject *obj)
{
   return static_cast<T *>(JS_GetPrivate(cx, obj));
}

//
// AeonJS_GetThisPrivate
//
// Retrieve the private data pointer from the "this" object on which a native
// method was invoked, in particular, inside a JSFastNative.
//
template<typename T> inline T *AeonJS_GetThisPrivate(JSContext *cx, jsval *vp)
{
   return static_cast<T *>(JS_GetPrivate(cx, JS_THIS_OBJECT(cx, vp)));
}

#endif

#endif

// EOF

