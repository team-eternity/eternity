// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 Samuel Villarreal
// Copyright(C) 2011 James Haley
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
// DESCRIPTION:
//   PNG Resource Reading and Conversion
//
//-----------------------------------------------------------------------------

#ifndef V_PNG_H__
#define V_PNG_H__

// Forward declare private implementation class
class VPNGImagePimpl;

class VPNGImage
{
private:
   VPNGImagePimpl *pImpl; // Private implementation object to hide libpng

   friend class VPNGImagePimpl;

public:

   // Methods
   bool readImage(byte *data);

   // Static routines
   static bool CheckPNGFormat(byte *data);
};

#endif

// EOF

