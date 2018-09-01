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

#include "b_botmap.h"
#include "b_msector.h"
#include "../m_buffer.h"
#include "../p_setup.h"
#include "../r_state.h"

IMPLEMENT_RTTI_TYPE(MetaSector)
IMPLEMENT_RTTI_TYPE(SimpleMSector)
IMPLEMENT_RTTI_TYPE(ThingMSector)
IMPLEMENT_RTTI_TYPE(LineMSector)
IMPLEMENT_RTTI_TYPE(CompoundMSector)

// How to write to cache:
// Byte 1: expected type
// 0: SIMPLE
// 1: LINE
// 2: THING
// 3: COMPOUND
// 255: end (no more msectors)
// For 0-2: UInt32: index of sector/line/solid-decor
// For 3: UInt32 size + array of UInt32.
enum
{
    MSEC_SIMPLE = 0,
    MSEC_LINE = 1,
    MSEC_THING = 2,
    MSEC_COMPOUND = 3,
};

//
// MetaSector::readFromFile
//
// Tries to create a metasector from a file
//
MetaSector* MetaSector::readFromFile(InBuffer& file)
{
    uint8_t u8;
    
    if (!file.readUint8(u8))
        return nullptr;
    switch (u8)
    {
    case MSEC_SIMPLE:
        return SimpleMSector::readFromFile(file);
    case MSEC_LINE:
        return LineMSector::readFromFile(file);
    case MSEC_THING:
        return ThingMSector::readFromFile(file);
    case MSEC_COMPOUND:
        return CompoundMSector::readFromFile(file);
    }

    return nullptr;
}

//
// SimpleMSector::writeToFile
//
void SimpleMSector::writeToFile(OutBuffer& file) const
{
    file.writeUint8(MSEC_SIMPLE);
    file.writeSint32(sector ? (int32_t)(sector - ::sectors) : -1);
}
SimpleMSector* SimpleMSector::readFromFile(InBuffer& file)
{
    auto sms = new SimpleMSector;
    file.readSint32T((uintptr_t&)sms->sector);
    return sms;
}
bool SimpleMSector::convertIndicesToPointers()
{
   return B_ConvertPtrToArrayItem(sector, ::sectors, ::numsectors);
}

//
// String representation
//
qstring SimpleMSector::toString() const
{
   qstring result("SimpleMSector(");
   result << (sector ? eindex(sector - ::sectors) : -1) << ")";
   return result;
}

//
// LineMSector::writeToFile
//
void LineMSector::writeToFile(OutBuffer& file) const
{
    file.writeUint8(MSEC_LINE);
    file.writeSint32(sector[0] ? (int32_t)(sector[0] - ::sectors) : -1);
    file.writeSint32(sector[1] ? (int32_t)(sector[1] - ::sectors) : -1);
    file.writeSint32(line ? (int32_t)(line - ::lines) : -1);
}
LineMSector* LineMSector::readFromFile(InBuffer& file) 
{
    auto lms = new LineMSector;
    file.readSint32T((uintptr_t&)lms->sector[0]);
    file.readSint32T((uintptr_t&)lms->sector[1]);
    file.readSint32T((uintptr_t&)lms->line);
    return lms;
}
bool LineMSector::convertIndicesToPointers()
{
   return B_ConvertPtrToArrayItem(sector[0], ::sectors, ::numsectors)
   && B_ConvertPtrToArrayItem(sector[1], ::sectors, ::numsectors)
   && B_ConvertPtrToArrayItem(line, ::lines, ::numlines);
}

//
// String representation
//
qstring LineMSector::toString() const
{
   qstring result("LineMSector(line=");
   result << (line ? eindex(line - ::lines) : -1) << " sectors=(";
   result << (sector[0] ? eindex(sector[0] - ::sectors) : -1) << ", " <<
   (sector[1] ? eindex(sector[1] - ::sectors) : -1) << "))";
   return result;
}

//
// ThingMSector::writeToFile
//
void ThingMSector::writeToFile(OutBuffer& file) const
{
    file.writeUint8(MSEC_THING);
    file.writeSint32(sector ? (int32_t)(sector - ::sectors) : -1);
    file.writeSint32(mobj ? p_mobjIndexMap[mobj] : -1);
}
ThingMSector* ThingMSector::readFromFile(InBuffer& file)
{
    auto tms = new ThingMSector;
    file.readSint32T((uintptr_t&)tms->sector);
    file.readSint32T((uintptr_t&)tms->mobj);
    return tms;
}
bool ThingMSector::convertIndicesToPointers()
{
   return B_ConvertPtrToArrayItem(sector, ::sectors, ::numsectors)
   && B_ConvertPtrToCollItem(mobj, p_indexMobjMap, ::numthings);
}
//
// String representation
//
qstring ThingMSector::toString() const
{
   qstring result("ThingMSector(sector=");
   result << (sector ? eindex(sector - ::sectors) : -1) <<", mobj=";
   if(mobj)
   {
      result << "(name=" << mobj->info->name << " x=" <<
      M_FixedToDouble(mobj->x) << " y=" << M_FixedToDouble(mobj->y) << "))";
   }
   else
      result << "(null))";
   return result;
}

//
// CompoundMSector::writeToFile
//
void CompoundMSector::writeToFile(OutBuffer& file) const
{
    file.writeUint8(MSEC_COMPOUND);
    file.writeSint32(numElem);
    for (int i = 0; i < numElem; ++i)
    {
        file.writeSint32(msectors[i] ? botMap->msecIndexMap[msectors[i]] : -1);
    }
}
CompoundMSector* CompoundMSector::readFromFile(InBuffer& file)
{
   // NOTE: since we don't have all metasectors yet, set temporary invalid
   // values instead

   auto cms = new CompoundMSector;
   file.readSint32T(cms->numElem);
   if (!B_CheckAllocSize(cms->numElem))
   {
       delete cms;
       return nullptr;
   }

   cms->msectors = emalloc(decltype(cms->msectors), cms->numElem *
                           sizeof(*cms->msectors));

   for (int i = 0; i < cms->numElem; ++i)
   {
       file.readSint32T((uintptr_t&)cms->msectors[i]);
   }

   return cms;
}
bool CompoundMSector::convertIndicesToPointers()
{
   for(int i = 0; i < numElem; ++i)
   {
      if(!B_ConvertPtrToCollItem(msectors[i], ::botMap->metasectors,
                                 ::botMap->metasectors.getLength()))
      {
         return false;
      }
   }
   return true;
}

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
fixed_t CompoundMSector::getAltFloorHeight() const
{
   fixed_t max = D_MININT, val;
   for (int i = 0; i < numElem; ++i)
   {
      if((val = msectors[i]->getAltFloorHeight()) > max)
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

//
// String representation
//
qstring CompoundMSector::toString() const
{
   qstring result("CompoundMSector(");
   if(!msectors)
   {
      result << "null)";
      return result;
   }
   for(int i = 0; i < numElem; ++i)
   {
      if(!msectors[i])
         result << "(null) ";
      else
         result << msectors[i]->toString() << " ";
   }
   result << ")";
   return result;
}

// EOF

