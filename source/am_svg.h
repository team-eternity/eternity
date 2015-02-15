// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2015 Ioan Chera
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Automap SVG Snapshot Taker
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__am_svg__
#define __EternityEngine__am_svg__

#include "m_collection.h"

//
// AutomapSvgWriter
//
// Writes the map to a SVG file as a snapshot.
//
class AutomapSvgWriter
{
public:
   void reset()
   {
      mLines.makeEmpty();
      mMinX = DBL_MAX;
      mMinY = DBL_MAX;
      mMaxX = -DBL_MAX;
      mMaxY = -DBL_MAX;
   }
   
   void addLine(double x0, double y0, double x1, double y1, int color);

   void write();

private:
   struct Line
   {
      double x0, y0, x1, y1;
      int color;
   };

   PODCollection<Line> mLines;
   double mMinX, mMaxX, mMinY, mMaxY;
};

extern AutomapSvgWriter am_svgWriter;

#endif /* defined(__EternityEngine__am_svg__) */
