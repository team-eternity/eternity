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

#include "z_zone.h"

#include "am_svg.h"
#include "autopalette.h"
#include "c_io.h"
#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "m_buffer.h"
#include "m_compare.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "p_skin.h"
#include "s_sound.h"
#include "v_misc.h"

// SVG markup macros

#define SVG_ROOT "<svg version=\"1.1\" baseProfile=\"full\" xmlns=\"http://www.w3.org/2000/svg\" style=\"background:black;stroke-width:1\">"
#define SVG_ROOT_END "</svg>"

// SVG adjustments
#define SVG_DOWN_SCALE 5

AutomapSvgWriter am_svgWriter;

void AutomapSvgWriter::addLine(double x0, double y0, double x1, double y1, int color)
{
   Line &line = mLines.addNew();
   line.x0 = x0;
   line.y0 = y0;
   line.x1 = x1;
   line.y1 = y1;
   line.color = color;

   double smallerX = emin(x0, x1);
   double biggerX = emax(x0, x1);
   double smallerY = emin(y0, y1);
   double biggerY = emax(y0, y1);
   if(smallerX < mMinX)
      mMinX = smallerX;
   if(biggerX > mMaxX)
      mMaxX = biggerX;
   if(smallerY < mMinY)
      mMinY = smallerY;
   if(biggerY > mMaxY)
      mMaxY = biggerY;
}

void AutomapSvgWriter::write()
{
   // Code based on M_ScreenShot
   bool success = false;
   char *path = nullptr;
   size_t len = M_StringAlloca(&path, 1, 6, userpath);
   psnprintf(path, len, "%s/shots", userpath);

   errno = 0;

   if(!access(path, W_OK))
   {
      static int shot;
      char *subPath = nullptr;
      int tries = 10000;

      len = M_StringAlloca(&subPath, 1, sizeof("/automap00000.svg"), path);
      do
      {
         psnprintf(subPath, len, "%s/automap%02d.svg", path, shot++);
      } while(!access(subPath, F_OK) && --tries);

      OutBuffer outBuffer;
      outBuffer.setThrowing(true);

      if(tries && outBuffer.CreateFile(subPath, 512 * 1024, BufferedFileBase::NENDIAN))
      {
         try
         {
            qstring lineString;

            AutoPalette palette(wGlobalDir);

            outBuffer.Write(SVG_ROOT, sizeof(SVG_ROOT) - 1);

            for (Line &line : mLines)
            {
               // Make sure to revert y coordinates
               // FIXME: Eternity's hungry consumption of 1078 bytes per float annoys me
               lineString.Printf(0, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" style=\"stroke:#%02x%02x%02x\"/>",
                                 (line.x0 - mMinX) / SVG_DOWN_SCALE,
                                 (mMaxY - line.y0) / SVG_DOWN_SCALE,
                                 (line.x1 - mMinX) / SVG_DOWN_SCALE,
                                 (mMaxY - line.y1) / SVG_DOWN_SCALE,
                                 palette[3 * line.color],
                                 palette[3 * line.color + 1],
                                 palette[3 * line.color + 2]);
               outBuffer.Write(lineString.constPtr(), lineString.length());
            }

            outBuffer.Write(SVG_ROOT_END, sizeof(SVG_ROOT_END) - 1);

            outBuffer.Close();   // if i don't call this, nothing will get written
            
            success = true;
         }
         catch(const BufferedIOException& )
         {
            success = false;  // just to make sure
         }
      }
   }

   if(!success)
   {
      doom_printf("%s", errno ? strerror(errno): FC_ERROR "Could not take SVG automap snapshot\n");
      S_StartInterfaceSound(GameModeInfo->playerSounds[sk_oof]);
   }
   else
      S_StartInterfaceSound(GameModeInfo->c_BellSound);
}

// EOF
