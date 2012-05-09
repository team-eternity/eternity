// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2004 James Haley
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
//--------------------------------------------------------------------------
//
// Unified Font Engine
//
// haleyjd 01/14/05
//
// The functions in this module replace code that was previously
// duplicated with special-case constants inserted in at least four
// different modules.  This provides a uniform interface for all fonts
// and makes the addition of more fonts easier.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "d_gi.h"
#include "doomstat.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

static bool fixedColor = false;
static int fixedColNum = 0;
static char *altMap = NULL;
static bool shadowChar = false;
static bool absCentered = false; // 03/04/06: every line will be centered

//
// V_FontLineWidth
//
// Finds the width of the string up to the first \n or the end.
//
static int V_FontLineWidth(vfont_t *font, const unsigned char *s)
{
   int length = 0;        // current line width
   unsigned char c;
   patch_t *patch = NULL;
   
   for(; *s; s++)
   {
      c = *s;

      if(c >= 128) // color or control code
         continue;
      
      if(c == '\n') // newline
         break;

      // normalize character
      if(font->upper)
         c = toupper(c) - font->start;
      else
         c = c - font->start;

      if(c >= font->size)
         length += font->space;
      else if(font->linear)
         length += font->lsize - font->dw;
      else
      {
         if(!(patch = font->fontgfx[c]))
            length += font->space;
         else
            length += patch->width - font->dw;
      }
   }   
   
   return length;
}

//
// V_FontWriteText
//
// Generalized bitmapped font drawing code. A vfont_t object contains
// all the information necessary to format the text. Support for
// translucency and color range translation table codes is present,
// and features including the ability to have case-sensitive fonts and
// fonts which center their characters within uniformly spaced blocks
// have been added or absorbed from other code.
//
void V_FontWriteText(vfont_t *font, const char *s, int x, int y)
{
   patch_t *patch = NULL;   // patch for current character -OR-
   byte    *src   = NULL;   // source char for linear font
   int     w;               // width of patch
   
   const unsigned char *ch; // pointer to string
   unsigned int c;          // current character
   int cx, cy, tx;          // current screen position

   byte *color = NULL;      // current color range translation tbl
   bool tl = false;         // current translucency state
   bool useAltMap = false;  // using alternate colormap source?

   if(font->color)
   {
      if(fixedColor)
      {
         // haleyjd 03/27/03: use fixedColor if it was set
         color = (byte *)colrngs[fixedColNum];
         fixedColor = false;
      }
      else
      {
         // haleyjd: get default text color from GameModeInfo
         color = *(GameModeInfo->defTextTrans); // Note: ptr to ptr
      }
   }
   
   // haleyjd 10/04/05: support alternate colormap sources
   if(altMap)
   {
      color = (byte *)altMap;
      altMap = NULL;
      useAltMap = true;
   }
      
   ch = (const unsigned char *)s;

   // haleyjd 03/29/06: special treatment - if first character is 143
   // (abscenter toggle), then center line
   
   cx = (*ch == TEXT_CONTROL_ABSCENTER) ? 
          (SCREENWIDTH - V_FontLineWidth(font, ch)) >> 1 : x;
   cy = y;
   
   while((c = *ch++))
   {
      // color and control codes
      if(c >= TEXT_COLOR_MIN)
      {
         if(c == TEXT_CONTROL_TRANS) // translucency toggle
         {
            tl ^= true;
         }
         else if(c == TEXT_CONTROL_SHADOW) // shadow toggle
         {
            shadowChar ^= true;
         }
         else if(c == TEXT_CONTROL_ABSCENTER) // abscenter toggle
         {
            absCentered ^= true;
         }
         else if(font->color && !useAltMap) // not all fonts support translations
         {
            int colnum;
            
            // haleyjd: allow use of gamemode-dependent defaults
            switch(c)
            {
            case TEXT_COLOR_NORMAL:
               colnum = GameModeInfo->colorNormal;
               break;
            case TEXT_COLOR_HI:
               colnum = GameModeInfo->colorHigh;
               break;
            case TEXT_COLOR_ERROR:
               colnum = GameModeInfo->colorError;
               break;
            default:
               colnum = c - 128;
               break;
            }

            // check that colrng number is within bounds
            if(colnum < 0 || colnum >= CR_LIMIT)
            {
               C_Printf("V_FontWriteText: invalid color %i\n", colnum);
               continue;
            }
            else
               color = (byte *)colrngs[colnum];
         }
         continue;
      }

      // special characters
      if(c == '\t')
      {
         cx = (cx / 40) + 1;
         cx =  cx * 40;
         continue;
      }
      if(c == '\n')
      {
         cx = absCentered ? (SCREENWIDTH - V_FontLineWidth(font, ch)) >> 1 : x;
         cy += font->cy;
         continue;
      }
      if(c == '\a') // don't draw BELs in linear fonts
         continue;
      
      // normalize character
      if(font->upper)
         c = toupper(c) - font->start;
      else
         c = c - font->start;

      // check if within font bounds, draw a space if not
      if(c >= font->size || (!font->linear && !(patch = font->fontgfx[c])))
      {
         cx += font->space;
         continue;
      }

      w = font->linear ? font->lsize : patch->width;

      // possibly adjust x coordinate for centering
      tx = (font->centered ? cx + (font->cw >> 1) - (w >> 1) : cx);

      // check against screen bounds
      // haleyjd 12/29/05: text is now clipped by patch drawing code
      
      if(font->linear)
      {
         // TODO: translucent masked block support
         // TODO: shadowed linear?
         int lx = (c & 31) * font->lsize;
         int ly = (c >> 5) * font->lsize;
         src = font->data + ly * (font->lsize << 5) + lx;

         V_DrawMaskedBlockTR(tx, cy, &vbscreen, font->lsize, font->lsize,
                             font->lsize << 5, src, color);
      }
      else
      {
         // draw character
         if(tl)
            V_DrawPatchTL(tx, cy, &vbscreen, patch, color, FTRANLEVEL);
         else
         {
            // haleyjd 10/04/05: text shadowing
            if(shadowChar)
            {
               //char *cm = (char *)(colormaps[0] + 33*256);
               //V_DrawPatchTL(tx + 2, cy + 2, &vbscreen, patch, cm, FRACUNIT*2/3);
               V_DrawPatchShadowed(tx, cy, &vbscreen, patch, color, FRACUNIT);
            }
            else
               V_DrawPatchTranslated(tx, cy, &vbscreen, patch, color, false);
         }
      }
      
      // move over in x direction, applying width delta if appropriate
      cx += (font->centered ? font->cw : (w - font->dw));
   }
   
   // reset text shadowing on exit
   shadowChar = false;

   // reset absCentered on exit
   absCentered = false;
}

//
// V_FontWriteTextColored
//
// Write text in a particular colour.
//
void V_FontWriteTextColored(vfont_t *font, const char *s, int color, int x, int y)
{
   if(color < 0 || color >= CR_LIMIT)
   {
      C_Printf("V_FontWriteTextColored: invalid color %i\n", color);
      return;
   }

   fixedColor  = true;
   fixedColNum = color;

   V_FontWriteText(font, s, x, y);
}

//
// V_FontWriteTextMapped
//
// Write text using a specified colormap.
//
void V_FontWriteTextMapped(vfont_t *font, const char *s, int x, int y, char *map)
{
   altMap = map;
   V_FontWriteText(font, s, x, y);
}

//
// V_FontWriteTextShadowed
//
// Write text with a shadow effect.
//
void V_FontWriteTextShadowed(vfont_t *font, const char *s, int x, int y)
{
   shadowChar = true;
   V_FontWriteText(font, s, x, y);
}

//
// V_FontSetAbsCentered
//
// Sets the next string to be printed with absolute centering.
//
void V_FontSetAbsCentered(void)
{
   absCentered = true;
}

//
// V_FontStringHeight
//
// Finds the tallest height in pixels of the string
//
int V_FontStringHeight(vfont_t *font, const char *s)
{
   unsigned char c;
   int height;
   
   height = font->cy; // a string is always at least one line high
   
   // add an extra cy for each newline found
   while((c = (unsigned char)*s++))
   {
      if(c == '\n')
         height += font->cy;
   }
   
   return height;
}

//
// V_FontStringWidth
//
// Finds the width of the longest line in the string
//
int V_FontStringWidth(vfont_t *font, const char *s)
{
   int length = 0;        // current line width
   int longest_width = 0; // longest line width so far
   unsigned char c;
   patch_t *patch = NULL;
   
   for(; *s; s++)
   {
      c = (unsigned char)*s;

      if(c >= 128) // color or control code
         continue;
      
      if(c == '\n') // newline
      {
         if(length > longest_width) 
            longest_width = length;
         length = 0; // next line;
         continue;	  
      }

      // normalize character
      if(font->upper)
         c = toupper(c) - font->start;
      else
         c = c - font->start;

      if(c >= font->size)
         length += font->space;
      else if(font->linear)
         length += font->lsize - font->dw;
      else
      {
         if(!(patch = font->fontgfx[c]))
            length += font->space;
         else
            length += (patch->width - font->dw);
      }
   }
   
   if(length > longest_width)
      longest_width = length; // check last line
   
   return longest_width;
}

//
// V_FontCharWidth
//
// haleyjd 10/18/10: return the width of a single character
//
int V_FontCharWidth(vfont_t *font, char pChar)
{
   unsigned char c;
   int width;
   patch_t *patch = NULL;
   
   c = (unsigned char)pChar;

   if(c >= 128) // color or control code
      return 0;
      
   if(c == '\n') // newline
      return 0;

   // normalize character
   if(font->upper)
      c = toupper(c) - font->start;
   else
      c = c - font->start;

   if(c >= font->size)
      width = font->space;
   else if(font->linear)
      width = font->lsize - font->dw;
   else
   {
      if(!(patch = font->fontgfx[c]))
         width = font->space;
      else
         width = (patch->width - font->dw);
   }
   
   return width;
}

//
// V_FontMaxWidth
//
// haleyjd 05/25/06: finds the widest character in the font.
//
int16_t V_FontMaxWidth(vfont_t *font)
{
   unsigned int i;
   int16_t w = 0, pw;

   if(font->linear)
      return font->lsize;

   for(i = 0; i < font->size; ++i)
   {
      if(font->fontgfx[i])
      {
         pw = font->fontgfx[i]->width;

         if(pw > w)
            w = pw;
      }
   }

   return w;
}

// EOF

