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

#ifndef B_MSECTOR_H_
#define B_MSECTOR_H_

#include "b_lineeffect.h"
#include "../e_rtti.h"
#include "../m_dllist.h"
#include "../m_fixed.h"
#include "../p_map3d.h"
#include "../r_defs.h"

class InBuffer;
class OutBuffer;

//
// MetaSector
//
// Class which maps to a dominant sector, depending on floor or ceiling
//
class MetaSector : public RTTIObject
{
   DECLARE_ABSTRACT_TYPE(MetaSector, RTTIObject)
public:
   // The way to get the resultant sector
   virtual fixed_t getFloorHeight() const = 0;
   virtual fixed_t getAltFloorHeight() const = 0;
   virtual fixed_t getCeilingHeight() const = 0;
   virtual const sector_t *getFloorSector() const = 0;
   virtual const sector_t *getCeilingSector() const = 0;
   
   virtual ~MetaSector() {}
   
   DLListItem<MetaSector> listLink;

   virtual void writeToFile(OutBuffer& file) const = 0;
   virtual bool convertIndicesToPointers() = 0;

   static MetaSector* readFromFile(InBuffer& file);
};

//
// SimpleMSector
//
// MetaSector for a single sector
//
class SimpleMSector : public MetaSector
{
   DECLARE_RTTI_TYPE(SimpleMSector, MetaSector)
   
   friend class TempBotMap;
	friend class BotMap;
   const sector_t *sector;
public:
	
   fixed_t getFloorHeight() const override
   {
      return LevelStateStack::Floor(*sector);
   }
   fixed_t getAltFloorHeight() const override
   {
       return LevelStateStack::AltFloor(*sector);
   }
   fixed_t getCeilingHeight() const override
   {
       return LevelStateStack::Ceiling(*sector);
   }
   const sector_t *getFloorSector() const override
   {
      return sector;
   }
   const sector_t *getCeilingSector() const override
   {
      return sector;
   }

   virtual void writeToFile(OutBuffer& file) const override;
   virtual bool convertIndicesToPointers() override;

   static SimpleMSector* readFromFile(InBuffer& file);
};

//
// ThingMSector
//
// MetaSector from a thing
//
class ThingMSector : public MetaSector
{
   DECLARE_RTTI_TYPE(ThingMSector, MetaSector)
   
   friend class TempBotMap;
   friend class BotMap;
   const sector_t *sector;
   const Mobj *mobj;
public:
   fixed_t getFloorHeight() const override
   {
      return
          mobj->flags & MF_SPAWNCEILING ? LevelStateStack::Floor(*sector) :
          (P_Use3DClipping() ? LevelStateStack::Floor(*sector) + mobj->height : D_MAXINT);
   }
   fixed_t getAltFloorHeight() const override
   {
        return
          mobj->flags & MF_SPAWNCEILING ? LevelStateStack::AltFloor(*sector) :
          (P_Use3DClipping() ? LevelStateStack::AltFloor(*sector) + mobj->height : D_MAXINT);
   }
   fixed_t getCeilingHeight() const override
   {
      return
          !(mobj->flags & MF_SPAWNCEILING) ? LevelStateStack::Ceiling(*sector) :
          (P_Use3DClipping() ? LevelStateStack::Ceiling(*sector) - mobj->height : D_MININT);
   }
   const sector_t *getFloorSector() const override
   {
      return sector;
   }
   const sector_t *getCeilingSector() const override
   {
      return sector;
   }

   virtual void writeToFile(OutBuffer& file) const override;
   virtual bool convertIndicesToPointers() override;

   static ThingMSector* readFromFile(InBuffer& file);
};

//
// LineMSector
//
// MetaSector from a line
//
class LineMSector : public MetaSector
{
   DECLARE_RTTI_TYPE(LineMSector, MetaSector)
   
   friend class TempBotMap;
	friend class BotMap;
   const sector_t *sector[2];
   const line_t *line;
public:
   fixed_t getFloorHeight() const override
   {
      return line->flags & ML_BLOCKING || !line->backsector ? D_MAXINT :
      LevelStateStack::Floor(*getFloorSector());
   }
   fixed_t getAltFloorHeight() const override
   {
      return line->flags & ML_BLOCKING || !line->backsector ? D_MAXINT :
      LevelStateStack::AltFloor(*getFloorSector());
   }
   fixed_t getCeilingHeight() const override
   {
      return line->flags & ML_BLOCKING || !line->backsector ? D_MININT :
      LevelStateStack::Ceiling(*getCeilingSector());
   }
   const sector_t *getFloorSector() const override
   {
       return sector[1] ? (LevelStateStack::Floor(*sector[0])
                           > LevelStateStack::Floor(*sector[1]) ? sector[0] :
                          sector[1]) : sector[0];
   }
   const sector_t *getCeilingSector() const override
   {
      return sector[1] ? (LevelStateStack::Ceiling(*sector[0]) < LevelStateStack::Ceiling(*sector[1]) ? sector[0] :
                          sector[1]) : sector[0];
   }

   virtual void writeToFile(OutBuffer& file) const override;
   virtual bool convertIndicesToPointers() override;

   static LineMSector* readFromFile(InBuffer& file);
};

//
// CompoundMSector
//
// MetaSector from several overlapped ones
// FIXME: use a binary tree instead of an array. Fortunately, there are few
// elements. But still...
//
class CompoundMSector : public MetaSector
{
   DECLARE_RTTI_TYPE(CompoundMSector, MetaSector)
   
   friend class TempBotMap;
	friend class BotMap;
   const MetaSector **msectors;
   int numElem;
public:
   CompoundMSector() : msectors(NULL), numElem(0)
   {
   }
   virtual ~CompoundMSector()
   {
      efree(msectors);
   }
   fixed_t getFloorHeight() const override;
   fixed_t getAltFloorHeight() const override;
   fixed_t getCeilingHeight() const override;
   const sector_t *getFloorSector() const override;
   const sector_t *getCeilingSector() const override;

   virtual void writeToFile(OutBuffer& file) const override;
   virtual bool convertIndicesToPointers() override;

   static CompoundMSector* readFromFile(InBuffer& file);
};

#endif

// EOF

