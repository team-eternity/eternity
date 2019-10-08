// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
//      Version of traversing functions from P_, that use self-contained vars
//      Meant to prevent any global variable interference and possible desync
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__b_trace__
#define __EternityEngine__b_trace__

#include <unordered_map>
#include "../m_fixed.h"
#include "../p_map.h"
#include "../p_maputl.h"

struct intercept_t;
typedef struct polyobj_s polyobj_t;

//
// RTraversal
//
// Reentrant path traversal engine, based on P_TRACE.H/CPP
//
class RTraversal
{
public:
   linetracer_t   m_trace;
   int            m_validcount;
   intercept_t*   m_intercepts;
   intercept_t*   m_intercept_p;
   doom_mapinter_t m_clip;
   
   bool Execute(divline_t trace, int flags, bool (*trav)(intercept_t*, void*), void* parm);
   
   RTraversal();
   
   ~RTraversal()
   {
      efree(m_lineValidcount);
      efree(m_intercepts);
   }

   static bool TRaimTraverse(intercept_t*, void*);

   fixed_t SafeAimLineAttack(Mobj* t1, angle_t angle, fixed_t distance, int mask);

private:
    void LineOpening(const line_t* linedef, const Mobj* mo);

   std::unordered_map<const polyobj_t*, int> m_polyValidcount;
   int*                                      m_lineValidcount;
   size_t         m_num_intercepts;
   
   bool blockLinesIterator(v2int_t v, const std::function<bool(line_t*)> &func);
   bool blockThingsIterator(v2int_t v, const std::function<bool(Mobj*)> &func);
   
   bool addLineIntercepts(line_t* ld);
   bool addThingIntercepts(Mobj* thing);
   
   bool traverseIntercepts(bool(*func)(intercept_t*, void*), fixed_t maxfrac, void* parm);
   
   void checkIntercept();
};



#endif /* defined(__EternityEngine__b_trace__) */
