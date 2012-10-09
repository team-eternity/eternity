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
// Proxy classes for controlled interactions between objects with arbitrary
// lifetimes.
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef M_PROXY_H__
#define M_PROXY_H__

#include "i_system.h"
#include "m_dllist.h"

class Proxy;

// Abstract base class for all proxy relationships
class ProxyRelationship
{
protected:
   friend class Proxy;
   virtual void release(Proxy *p) = 0;
   virtual DLListItem<ProxyRelationship> *next(const Proxy *p) const = 0;

public:
   virtual Proxy *other(const Proxy *original, const Proxy *current) const = 0;
   virtual bool  in(const Proxy *p) const = 0;
};

// In order to participate in interactions mediated by a ProxyCoupling, an
// object needs to either inherit from this class, or contain as a direct
// member an instance of it.
class Proxy
{
private:
   friend class ProxyCoupling;
   DLListItem<ProxyRelationship> *root; // root for couplings containing this object

public:
   // A proxy starts out free of any interactions.
   Proxy() : root(NULL) {}

   // A destructing proxy is released from all interactions to which it is a
   // party.
   virtual ~Proxy()
   {
      DLListItem<ProxyRelationship> *rover = root;
      while(rover)
      {
         // Must cache next in case the relationship is deleted via loss of
         // its last reference.
         DLListItem<ProxyRelationship> *next = rover->dllObject->next(this);
         rover->dllObject->release(this);
         rover = next;
      }
   }

   // Find the first relationship to another given proxy
   ProxyRelationship *findRelationshipTo(const Proxy *other) const
   {
      DLListItem<ProxyRelationship> *rover = root;
      while(rover)
      {
         if(rover->dllObject->in(other))
            return rover->dllObject;
      }
   }
};

// ProxyCoupling: A relationship between two proxies.
class ProxyCoupling : public ProxyRelationship
{
public:
   // Operation results (three-state logic)
   enum
   {
      RESULT_FAIL = -1,
      RESULT_FALSE,
      RESULT_TRUE
   };

   // Typedef for a safe mediated operation
   typedef int (*SafeOp)(Proxy *, Proxy *);

protected:
   Proxy *one; // first object in this interaction
   Proxy *two; // second object in this interaction
   DLListItem<ProxyRelationship> oneLinks; // next coupling for "one"
   DLListItem<ProxyRelationship> twoLinks; // next coupling for "two"

   // virtuals

   // Called when a proxy is releasing itself from this interaction.
   // Once both parties no longer referece the relationship, it can
   // free itself in a limited form of garbage collection.
   virtual void release(Proxy *p)
   {
      if(p == one)
         one = NULL;
      else if(p == two)
         two = NULL;

      if(!(one || two)) // No more references? Delete self.
         delete this;
   }

   // Obtain the next relationship for the given proxy object.
   virtual DLListItem<ProxyRelationship> *next(const Proxy *p) const
   {
      if(p == one)
         return oneLinks.dllNext;
      else if(p == two)
         return twoLinks.dllNext;
      else
         return NULL;
   }

public:
   // Construct a new two-party relationship between two proxied objects.
   // This class should always be allocated with operator "new"; it is
   // a self-managing object which garbage collects itself when its referring
   // proxies have released it. pOne and pTwo must be distinct objects.
   ProxyCoupling(Proxy *pOne, Proxy *pTwo)
      : one(pOne), two(pTwo), oneLinks(), twoLinks()
   {
      if(one == two)
         I_Error("ProxyCoupling: cannot couple an object to itself!\n");

      // Root into both proxies
      oneLinks.insert(this, &one->root);
      twoLinks.insert(this, &two->root);
   }

   // Nullify proxy pointers for safety.
   virtual ~ProxyCoupling()
   {
      one = two = NULL;
   }

   // Get the other party in the relationship
   virtual Proxy *other(const Proxy *original, const Proxy *current) const
   {
      if(!current)
      {
         if(original == one)
            return two;
         else if(original == two)
            return one;
      }
      
      return NULL;
   }

   // Test if the proxy is in a given relationship
   virtual bool in(const Proxy *p) const { return (p == one || p == two); }
      
   // Perform a safe operation between the two proxied objects. If either
   // object has released the proxy, the operation will not be performed,
   // and RESULT_FAIL will be returned.
   int safeOperation(SafeOp op) const
   {
      return (one && two) ? op(one, two) : RESULT_FAIL;
   }
};


#endif

// EOF

