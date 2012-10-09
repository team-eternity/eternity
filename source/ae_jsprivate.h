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
// Aeon Engine - JavaScript Private Data Base Class
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef AE_JSPRIVATE_H__
#define AE_JSPRIVATE_H__

#include "e_rtti.h"
#include "m_proxy.h"

namespace AeonJS
{
   // Derive all JSObject private data from this class, for type safety. If
   // native methods are accessed via .call / .apply / .bind, they may not be
   // invoked on the same type of object for which they were created.
   class PrivateData : public RTTIObject
   {
      DECLARE_RTTI_TYPE(PrivateData, RTTIObject)

   protected:
      // This class contains a proxy, in the event it is needed for maintaining
      // references to objects with lifetimes outside the control of the
      // JSObject to which this private data belongs (ex: an Mobj instance).
      Proxy proxy;

   public:
      PrivateData() : Super(), proxy()
      {
      }
   };
}

#endif

// EOF

