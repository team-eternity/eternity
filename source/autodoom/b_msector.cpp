// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Metasectors (for bot map)
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "b_msector.h"

IMPLEMENT_RTTI_TYPE(MetaSector)
IMPLEMENT_RTTI_TYPE(SimpleMSector)
IMPLEMENT_RTTI_TYPE(ThingMSector)
IMPLEMENT_RTTI_TYPE(LineMSector)
IMPLEMENT_RTTI_TYPE(CompoundMSector)

//
// CompoundMSector::getFloorHeight
//
// Gets the effective floor height of a compound metasector
//
fixed_t CompoundMSector::getFloorHeight() const
{
   fixed_t max = D_MININT, val;
   for (int i = 0; i < numElem; ++i)
   {
      if((val = msectors[i]->getFloorHeight()) > max)
         max = val;
   }
   return max;
}

//
// CompoundMSector::getCeilingHeight
//
// Gets the effective ceiling height
//
fixed_t CompoundMSector::getCeilingHeight() const
{
   fixed_t min = D_MAXINT, val;
   for (int i = 0; i < numElem; ++i)
   {
      if((val = msectors[i]->getCeilingHeight()) < min)
         min = val;
   }
   return min;
}

//
// CompoundMSector::getFloorSector
//
// Gets the effective floor height sector
//
const sector_t *CompoundMSector::getFloorSector() const
{
   fixed_t max = D_MININT, val;
   const sector_t *smax, *sval;
   for (int i = 0; i < numElem; ++i)
   {
      if((val = LevelStateStack::Floor(*(sval = msectors[i]->getFloorSector()))) > max)
      {
         max = val;
         smax = sval;
      }
   }
   return smax;
}

//
// CompoundMSector::getFloorSector
//
// Gets the effective ceiling height sector
//
const sector_t *CompoundMSector::getCeilingSector() const
{
   fixed_t min = D_MAXINT, val;
   const sector_t *smin, *sval;
   for (int i = 0; i < numElem; ++i)
   {
      if((val = LevelStateStack::Ceiling(*(sval = msectors[i]->getCeilingSector()))) <
         min)
      {
         min = val;
         smin = sval;
      }
   }
   return smin;
}

// EOF

