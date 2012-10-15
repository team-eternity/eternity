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

namespace AeonJS
{
   const char *SafeGetStringBytes(JSContext *cx, jsval value, jsval *root);
   bool        SafeInstanceOf(JSContext *cx, JSClass *jsClass, jsval val);


   //
   // AeonJS::GetPrivate
   //
   // Retrieve the private data pointer from a particular JSObject
   //
   template<typename T> inline T *GetPrivate(JSContext *cx, JSObject *obj)
   {
      return static_cast<T *>(JS_GetPrivate(cx, obj));
   }

   //
   // AeonJS::GetThisPrivate
   //
   // Retrieve the private data pointer from the "this" object on which a native
   // method was invoked, in particular, inside a JSFastNative.
   //
   template<typename T> inline T *GetThisPrivate(JSContext *cx, jsval *vp)
   {
      return static_cast<T *>(JS_GetPrivate(cx, JS_THIS_OBJECT(cx, vp)));
   }

   //
   // AutoNamedRoot
   //
   // This object keeps track of a named JS GC root and will release it when
   // the object is destroyed - useful for tying GC roots to C++ stack frames.
   // Either use the full constructor, or call init() when you have an object
   // to root. The AutoNamedRoot is not reusable; subsequent calls to init()
   // will fail with a false return value. Test isValid() before using!
   //
   class AutoNamedRoot
   {
   private:
      JSContext *cx;
      void      *root;
      AutoNamedRoot(const AutoNamedRoot &other) {} // Not copyable.

   public:
      AutoNamedRoot() : cx(NULL), root(NULL) {}
      
      AutoNamedRoot(JSContext *pcx, void *ptr, const char *name)
         : cx(pcx), root(ptr)
      {         
         if(!JS_AddNamedRoot(cx, &root, name))
            root = NULL;
      }
      
      ~AutoNamedRoot()
      {
         if(root)
         {
            JS_RemoveRoot(cx, &root);
            root = NULL;
         }
      }
   
      // JS rooting can potentially fail, so ALWAYS check this!
      bool isValid() const { return root != NULL; }
      
      // Call to late-initialize an AutoNamedRoot
      bool init(JSContext *pcx, void *ptr, const char *name)
      {
         bool res = false;

         // Cannot root a NULL value; useful for rooting the return
         // value of a JSAPI call without having to separately check it
         // for a non-NULL return value first.
         if(ptr && !isValid())
         {
            cx   = pcx;
            root = ptr;
            if(JS_AddNamedRoot(cx, &root, name))
               res = true;
            else
            {
               root = NULL;
               res  = false;
            }
         }
       
         return res;
      }
   };

   //
   // AutoContext
   //
   // This object keeps track of a JSContext and will destroy it when the
   // object is destroyed.
   //
   class AutoContext
   {
   public:
      JSContext *ctx;

      AutoContext(JSRuntime *rt, size_t stackChunkSize)
      {
         ctx = JS_NewContext(rt, stackChunkSize);
      }

      ~AutoContext()
      {
         if(ctx)
         {
            JS_DestroyContext(ctx);
            ctx = NULL;
         }
      }
   };

   const char *SafeGetStringBytes(JSContext *cx, jsval value, AutoNamedRoot &root);
}

#endif

#endif

// EOF

