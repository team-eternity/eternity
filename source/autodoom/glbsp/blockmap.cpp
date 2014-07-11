//------------------------------------------------------------------------
// BLOCKMAP : Generate the blockmap
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2007 Andrew Apted
//
//  Based on 'BSP 2.3' by Colin Reed, Lee Killough and others.
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
//-----------------------------------------------------------------------------

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "blockmap.h"
#include "level.h"
#include "node.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


#define DEBUG_BLOCKMAP  0


static int block_x, block_y;
static int block_w, block_h;
static int block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

//
// GetBlockmapBounds
//
void glbsp::blockmap::GetBlockmapBounds(int *x, int *y, int *w, int *h)
{
  *x = block_x; *y = block_y;
  *w = block_w; *h = block_h;
}

//
// CheckLinedefInsideBox
//
int glbsp::blockmap::CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax,
    int x1, int y1, int x2, int y2)
{
  int count = 2;
  int tmp;

  for (;;)
  {
    if (y1 > ymax)
    {
      if (y2 > ymax)
        return FALSE;
        
      x1 = x1 + (int) ((x2-x1) * (double)(ymax-y1) / (double)(y2-y1));
      y1 = ymax;
      
      count = 2;
      continue;
    }

    if (y1 < ymin)
    {
      if (y2 < ymin)
        return FALSE;
      
      x1 = x1 + (int) ((x2-x1) * (double)(ymin-y1) / (double)(y2-y1));
      y1 = ymin;
      
      count = 2;
      continue;
    }

    if (x1 > xmax)
    {
      if (x2 > xmax)
        return FALSE;
        
      y1 = y1 + (int) ((y2-y1) * (double)(xmax-x1) / (double)(x2-x1));
      x1 = xmax;

      count = 2;
      continue;
    }

    if (x1 < xmin)
    {
      if (x2 < xmin)
        return FALSE;
        
      y1 = y1 + (int) ((y2-y1) * (double)(xmin-x1) / (double)(x2-x1));
      x1 = xmin;

      count = 2;
      continue;
    }

    count--;

    if (count == 0)
      break;

    /* swap end points */
    tmp=x1;  x1=x2;  x2=tmp;
    tmp=y1;  y1=y2;  y2=tmp;
  }

  /* linedef touches block */
  return TRUE;
}

/* ----- top level funcs ------------------------------------ */

static void FindBlockmapLimits(glbsp::level::BBox &bbox)
{
  int i;

  int mid_x = 0;
  int mid_y = 0;

  bbox.minx = bbox.miny = SHRT_MAX;
  bbox.maxx = bbox.maxy = SHRT_MIN;

  for (i=0; i < glbsp::level::num_linedefs; i++)
  {
    glbsp::level::Linedef *L = glbsp::level::LookupLinedef(i);

    if (! L->zero_len)
    {
      float_g x1 = L->start->x;
      float_g y1 = L->start->y;
      float_g x2 = L->end->x;
      float_g y2 = L->end->y;

      int lx = (int)floor(MIN(x1, x2));
      int ly = (int)floor(MIN(y1, y2));
      int hx = (int)ceil(MAX(x1, x2));
      int hy = (int)ceil(MAX(y1, y2));

      if (lx < bbox.minx) bbox.minx = lx;
      if (ly < bbox.miny) bbox.miny = ly;
      if (hx > bbox.maxx) bbox.maxx = hx;
      if (hy > bbox.maxy) bbox.maxy = hy;

      // compute middle of cluster (roughly, so we don't overflow)
      mid_x += (lx + hx) / 32;
      mid_y += (ly + hy) / 32;
    }
  }

  if (glbsp::level::num_linedefs > 0)
  {
    block_mid_x = (mid_x / glbsp::level::num_linedefs) * 16;
    block_mid_y = (mid_y / glbsp::level::num_linedefs) * 16;
  }

# if DEBUG_BLOCKMAP
  PrintDebug("Blockmap lines centered at (%d,%d)\n", block_mid_x, block_mid_y);
# endif
}

//
// InitBlockmap
//
void glbsp::blockmap::InitBlockmap()
{
  level::BBox map_bbox;

  /* find limits of linedefs, and store as map limits */
  FindBlockmapLimits(map_bbox);

  PrintVerbose("Map goes from (%d,%d) to (%d,%d)\n",
      map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);

  block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  block_w = ((map_bbox.maxx - block_x) / 128) + 1;
  block_h = ((map_bbox.maxy - block_y) / 128) + 1;
  block_count = block_w * block_h;

}


