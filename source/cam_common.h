//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Ioan Chera, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Reentrant path traversing, used by several new functions.
// Authors: James Haley, Ioan Chera
//

#ifndef CAM_COMMON_H_
#define CAM_COMMON_H_

#include "m_collection.h"
#include "p_maputl.h"
#include "r_defs.h"

#define VALID_ALLOC(set, n) ((set) = ecalloc(byte *, 1, (((n) + 7) & ~7) / 8))
#define VALID_FREE(set) efree(set)

//
// PathTraverser setup
//
struct PTDef
{
   enum earlyout_e
   {
      eo_no,
      eo_always,
      eo_noearlycheck,
   };

   bool(*trav)(const intercept_t *in, void *context, const divline_t &trace);
   earlyout_e earlyOut;
   uint32_t flags;
};

//
// Reentrant path-traverse caller
//
class PathTraverser
{
public:
   bool traverse(fixed_t cx, fixed_t cy, fixed_t tx, fixed_t ty);
   bool traverse(v2fixed_t c, v2fixed_t t)
   {
      return traverse(c.x, c.y, t.x, t.y);
   }
   PathTraverser(const PTDef &indef, void *incontext);
   ~PathTraverser()
   {
      VALID_FREE(validlines);
      VALID_FREE(validpolys);
   }

   divline_t trace;
private:
   bool checkLine(size_t linenum);
   bool blockLinesIterator(int x, int y);
   bool blockThingsIterator(int x, int y);
   bool traverseIntercepts() const;

   const PTDef def;
   void *const context;
   byte *validlines;
   byte *validpolys;
   struct
   {
      bool hitpblock;
      bool addedportal;
   } portalguard;
   PODCollection<intercept_t> intercepts;
};

//
// Holds opening data just for the routines here
//
struct tracelineopening_t
{
   fixed_t openrange;
   Surfaces<fixed_t> open;

   void calculate(const line_t *linedef);
   void calculateAtPoint(const line_t &line, v2fixed_t pos);
};

#endif

// EOF

