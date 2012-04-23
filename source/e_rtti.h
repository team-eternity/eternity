// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 David Hill, James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Base for classes with custom RTTI. Adapted from the original ThinkerType.
//
//-----------------------------------------------------------------------------

#ifndef E_RTTI_H__
#define R_RTTI_H__

#include "z_zone.h"

//
// RTTIObject
//
class RTTIObject : public ZoneObject
{
public:
   class Type
   {
   private:
      void addType();

      Type *next;
      static Type **rttiTypes;

   protected:
      Type(const char *pName, Type *pParent)
       : parent(pParent), name(pName)
      {
         addType();
      }

      Type       *const parent;
      const char *const name;

   public:
      typedef RTTIObject Class;
      friend class RTTIObject;

      bool isAncestorOf(const Type *type) const
      {
         while(type)
         {
            if(type == this)
               return true;
            type = type->parent;
         }
         return false;
      }

      static Type *FindType(const char *pName);

      template<typename T> static T *FindType(const char *pName)
      {
         Type *type = FindType(pName);

         if(!T::Class::StaticType.isAncestorOf(type))
            return NULL;

         return static_cast<T *>(type);
      }

      virtual RTTIObject *newObject() const {return new RTTIObject;}
   };

   static Type StaticType;
   virtual const Type *getDynamicType() const { return &StaticType; }
   const char *getClassName() const { return getDynamicType()->name; }

   bool isInstanceOf(const Type *type) const
   {
      return (getDynamicType() == type);
   }

   bool isAncestorOf(const Type *type) const
   {
      return getDynamicType()->isAncestorOf(type);
   }

   bool isDescendantOf(const Type *type) const
   {
      return type->isAncestorOf(getDynamicType());
   }
};

//
// DECLARE_RTTI_TYPE
//
#define DECLARE_RTTI_TYPE(name, inherited) \
public: \
   typedef inherited Super; \
   class Type : public Super::Type \
   { \
   protected: \
      Type(char const *pName, Super::Type *pParent) \
       : Super::Type( pName, pParent ) {} \
   public: \
      typedef name Class; \
      friend class name; \
      virtual name *newObject() const { return new name ; } \
   }; \
   static Type StaticType; \
   virtual const Type *getDynamicType() const { return &StaticType; } \
private:

//
// IMPLEMENT_RTTI_TYPE
//
#define IMPLEMENT_RTTI_TYPE(name) \
name::Type name::StaticType(#name, &Super::StaticType);

// Inspired by ZDoom :P
#define RUNTIME_CLASS(cls) (&cls::StaticType)

#endif //E_RTTI_H__

// EOF

