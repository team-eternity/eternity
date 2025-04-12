//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine et al.
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
//------------------------------------------------------------------------------
//
// Purpose: ptoing's modern HUD.
// Authors: Max Waine
//

#ifndef HU_MODERN_H__
#define HU_MODERN_H__

// Required for HUDOverlay
#include "hu_over.h"

struct VBuffer;

class ModernHUD : public HUDOverlay
{
protected:
   virtual void DrawStatus (int x, int y);
   virtual void DrawHealth (int x, int y);
   virtual void DrawArmor  (int x, int y);
   virtual void DrawWeapons(int x, int y);
   virtual void DrawAmmo   (int x, int y);
   virtual void DrawKeys   (int x, int y);
   virtual void DrawFrags  (int x, int y);
   virtual void Setup();

   VBuffer *m_screen;
};

extern ModernHUD modern_overlay;

#endif

// EOF

