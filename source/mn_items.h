//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//----------------------------------------------------------------------------
//
// Menu Item Classes
//
//----------------------------------------------------------------------------

#ifndef MN_ITEMS_H__
#define MN_ITEMS_H__

#include "e_rtti.h"

struct menuitem_t;

//
// MenuItem
//
// Defines the interface for all menu item classes
//
class MenuItem : public RTTIObject
{
   DECLARE_RTTI_TYPE(MenuItem, RTTIObject)

protected:
   virtual int  adjustPatchXCoord(int x, int16_t width) { return x; }
   virtual bool shouldDrawDescription(menuitem_t *item) { return true; }

public:
   virtual bool drawPatchForItem(menuitem_t *item, int &item_height, int alignment);
   virtual void drawDescription(menuitem_t *item, int &item_height, int &item_width,
                                int alignment, int color);
   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) {}
   virtual const char *getHelpString(menuitem_t *item, char *msgbuffer) { return ""; }
   virtual void onConfirm(menuitem_t *item) {}
   virtual void onLeft(menuitem_t *item, bool altdown, bool shiftdown) {}
   virtual void onRight(menuitem_t *item, bool altdown, bool shiftdown) {}
};

extern MenuItem *MenuItemInstanceForType[];

#endif

// EOF

