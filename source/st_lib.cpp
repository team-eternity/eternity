//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: The status bar widget code.
// Authors: James Haley, Max Waine
//

#include "z_zone.h"

#include "doomdef.h"
#include "doomstat.h"
#include "m_swap.h"
#include "r_main.h"
#include "r_patch.h"
#include "st_lib.h"
#include "st_stuff.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

int sts_always_red;      //jff 2/18/98 control to disable status color changes
int sts_pct_always_gray; // killough 2/21/98: always gray %'s? bug or feature?

patch_t*    sttminus;

//
// STlib_init()
//
// Hack display negative frags. Loads and store the stminus lump.
//
// Passed nothing, returns nothing
//
void STlib_init()
{
   sttminus = PatchLoader::CacheName(wGlobalDir, "STTMINUS", PU_STATIC);
}

//
// STlib_initNum()
//
// Initializes an st_number_t widget
//
// Passed the widget, its position, the patches for the digits, a pointer
// to the value displayed, a pointer to the on/off control, and the width
// Returns nothing
//
void STlib_initNum(st_number_t *n, int x, int y, patch_t **pl, int num,
                   int max, bool *on, int width)
{
   n->x     = x;
   n->y     = y;
   n->width = width;
   n->num   = num;
   n->max   = max;
   n->on    = on;
   n->p     = pl;
}

//
// STlib_drawNum()
// 
// A fairly efficient way to draw a number based on differences from the 
// old number.
//
// Passed a st_number_t widget, a color range for output, and a flag
// indicating whether refresh is needed.
// Returns nothing
//
// jff 2/16/98 add color translation to digit output
//
static void STlib_drawNum(st_number_t *n, byte *outrng, int alpha)
{
   int   numdigits = n->width;
   int   num = n->num;
   int   w = n->p[0]->width;
   int   x;

   int   neg;

   // if non-number, do not draw it
   if(num == INT_MIN)
      return;

   neg = num < 0;

   if(neg)
   {
      if(numdigits == 2 && num < -9)
         num = -9;
      else if(numdigits == 3 && num < -99)
         num = -99;

      num = -num;
   }

   x = n->x;

   //jff 2/16/98 add color translation to digit output
   // in the special case of 0, you draw 0
   if(!num)
   {
      //jff 2/18/98 allow use of faster draw routine from config
      V_DrawPatchTL(x - w, n->y, &subscreen43, n->p[0], 
                    sts_always_red ? nullptr : outrng, alpha);
   }

   // draw the new number
   //jff 2/16/98 add color translation to digit output
   while(num && numdigits--)
   {
      x -= w;
      //jff 2/18/98 allow use of faster draw routine from config
      V_DrawPatchTL(x, n->y, &subscreen43, n->p[ num % 10 ],
                    sts_always_red ? nullptr : outrng, alpha);
      num /= 10;
   }

   // draw a minus sign if necessary
   //jff 2/16/98 add color translation to digit output
   if(neg)
   {
      //jff 2/18/98 allow use of faster draw routine from config
      V_DrawPatchTL(x - 8, n->y, &subscreen43, sttminus,
                    sts_always_red ? nullptr : outrng, alpha);
   }
}

//
// STlib_updateNum()
//
// Draws a number conditionally based on the widget's enable
//
// Passed a number widget, the output color range, and a refresh flag
// Returns nothing
//
// jff 2/16/98 add color translation to digit output
//
void STlib_updateNum(st_number_t *n, byte *outrng, int alpha)
{
   if(*n->on)
      STlib_drawNum(n, outrng, alpha);
}

//
// STlib_initPercent()
//
// Initialize a st_percent_t number with percent sign widget
//
// Passed a st_percent_t widget, the position, the digit patches, a pointer
// to the number to display, a pointer to the enable flag, and patch
// for the percent sign.
// Returns nothing.
//
void STlib_initPercent(st_percent_t *p, int x, int y, patch_t **pl, int num,
                       bool *on, patch_t *percent)
{
   STlib_initNum(&p->n, x, y, pl, num, 200, on, 3);
   p->p = percent;
}

//
// STlib_updatePercent()
//
// Draws a number/percent conditionally based on the widget's enable
//
// Passed a precent widget, the output color range, and a refresh flag
// Returns nothing
//
// jff 2/16/98 add color translation to digit output
//
void STlib_updatePercent(st_percent_t *per, byte *outrng, int alpha)
{
   if(*per->n.on) // killough 2/21/98: fix percents not updated
   {
      byte *tlate = nullptr;

      // jff 2/18/98 allow use of faster draw routine from config
      // also support gray-only percents
      if(!sts_always_red)
         tlate = sts_pct_always_gray ? cr_gray : outrng;

      V_DrawPatchTL(per->n.x, per->n.y, &subscreen43, per->p, tlate, alpha);
   }
   
   STlib_updateNum(&per->n, outrng, alpha);
}

//
// STlib_initMultIcon()
//
// Initialize a st_multicon_t widget, used for a multigraphic display
// like the status bar's keys.
//
// Passed a st_multicon_t widget, the position, the graphic patches, a pointer
// to the numbers representing what to display, and pointer to the enable flag
// Returns nothing.
//
void STlib_initMultIcon(st_multicon_t *i, int x, int y, patch_t **il, int *inum,
                        bool *on)
{
   i->x       = x;
   i->y       = y;
   i->inum    = inum;
   i->on      = on;
   i->p       = il;
}

//
// STlib_updateMultIcon()
//
// Draw a st_multicon_t widget, used for a multigraphic display
// like the status bar's keys. Displays each when the control
// numbers change or refresh is true
//
// Passed a st_multicon_t widget, and a refresh flag
// Returns nothing.
//
void STlib_updateMultIcon(st_multicon_t *mi, int alpha)
{
   if(*mi->on)
   {
      // killough 2/16/98: redraw only if != -1
      if(*mi->inum != -1)
         V_DrawPatchTL(mi->x, mi->y, &subscreen43, mi->p[*mi->inum], nullptr, alpha);
   }
}

//
// STlib_initBinIcon()
//
// Initialize a st_binicon_t widget, used for a multinumber display
// like the status bar's weapons, that are present or not.
//
// Passed a st_binicon_t widget, the position, the digit patches, a pointer
// to the flags representing what is displayed, and pointer to the enable flag
// Returns nothing.
//
void STlib_initBinIcon(st_binicon_t *b, int x, int y, patch_t *i, 
                       bool *val, bool *on)
{
   b->x   = x;
   b->y   = y;
   b->val = val;
   b->on  = on;
   b->p   = i;
}

//
// STlib_updateBinIcon()
//
// Draw a st_binicon_t widget, used for a multinumber display
// like the status bar's weapons that are present or not. Displays each
// when the control flag changes or refresh is true
//
// Passed a st_binicon_t widget, and a refresh flag
// Returns nothing.
//
void STlib_updateBinIcon(st_binicon_t *bi)
{
   if(*bi->on)
   {
      if(*bi->val)
         V_DrawPatch(bi->x, bi->y, &subscreen43, bi->p);
   }
}

//----------------------------------------------------------------------------
//
// $Log: st_lib.c,v $
// Revision 1.8  1998/05/11  10:44:42  jim
// formatted/documented st_lib
//
// Revision 1.7  1998/05/03  22:58:17  killough
// Fix header #includes at top, nothing else
//
// Revision 1.6  1998/02/23  04:56:34  killough
// Fix percent sign problems
//
// Revision 1.5  1998/02/19  16:55:09  jim
// Optimized HUD and made more configurable
//
// Revision 1.4  1998/02/18  00:59:13  jim
// Addition of HUD
//
// Revision 1.3  1998/02/17  06:17:03  killough
// Add support for erasing keys in status bar
//
// Revision 1.2  1998/01/26  19:24:56  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

