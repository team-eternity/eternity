// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
#include "m_qstr.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "w_wad.h"

/*
static bool fixedColor = false;
static int fixedColNum = 0;
static char *altMap = nullptr;
static bool shadowChar = false;
static bool absCentered = false; // 03/04/06: every line will be centered
*/

//
// V_FontLineWidth
//
// Finds the width of the string up to the first \n or the end.
//
static int V_FontLineWidth(vfont_t *font, const unsigned char *s)
{
   int length = 0;        // current line width
   unsigned char c;
   patch_t *patch = nullptr;
   
   for(; *s; s++)
   {
      c = *s;

      if(c >= 128) // color or control code
         continue;
      
      if(c == '\n') // newline
         break;

      // normalize character
      if(font->upper)
         c = ectype::toUpper(c) - font->start;
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
// V_FontWriteTextEx
//
// Generalized bitmapped font drawing code. A vfont_t object contains
// all the information necessary to format the text. Support for
// translucency and color range translation table codes is present,
// and features including the ability to have case-sensitive fonts and
// fonts which center their characters within uniformly spaced blocks
// have been added or absorbed from other code.
//
void V_FontWriteTextEx(const vtextdraw_t &textdraw)
{
   patch_t *patch = nullptr;   // patch for current character -OR-
   byte    *src   = nullptr;   // source char for linear font
   int     w;               // width of patch
   
   const unsigned char *ch; // pointer to string
   unsigned int c;          // current character
   int cx, cy, tx;          // current screen position

   byte *color = nullptr;      // current color range translation tbl
   bool tl = false;         // current translucency state
   bool useAltMap = false;  // using alternate colormap source?

   vfont_t *font      = textdraw.font;
   VBuffer *screen    = textdraw.screen;
   unsigned int flags = textdraw.flags;

   if(!screen)
      screen = &vbscreen;

   if(font->color)
   {
      // haleyjd 03/27/03: use fixedColor if it was set, else use default,
      // which may come from GameModeInfo.
      if(flags & VTXT_FIXEDCOLOR)
         color = font->colrngs[textdraw.fixedColNum];
      else
         color = font->colrngs[font->colorDefault];
   }
   
   // haleyjd 10/04/05: support alternate colormap sources
   if(textdraw.altMap)
   {
      color = (byte *)textdraw.altMap;
      useAltMap = true;
   }
      
   ch = (const unsigned char *)textdraw.s;

   // haleyjd 03/29/06: special treatment - if first character is 143
   // (abscenter toggle), then center line
   
   cx = (*ch == TEXT_CONTROL_ABSCENTER) ? 
          (screen->unscaledw - V_FontLineWidth(font, ch)) >> 1 : textdraw.x;
   cy = textdraw.y;
   
   while((c = *ch++))
   {
      // color and control codes
      if(c >= TEXT_COLOR_MIN)
      {
         switch(c)
         {
         case TEXT_CONTROL_TRANS: // translucency toggle
            tl ^= true;
            break;
         case TEXT_CONTROL_SHADOW: // shadow toggle
            flags ^= VTXT_SHADOW;
            break;
         case TEXT_CONTROL_ABSCENTER: // abscenter toggle
            flags ^= VTXT_ABSCENTER;
            break;
         default:
            if(font->color && !useAltMap) // not all fonts support translations
            {
               int colnum;

               // haleyjd: allow use of gamemode-dependent defaults
               switch(c)
               {
               case TEXT_COLOR_NORMAL:
                  colnum = font->colorNormal;
                  break;
               case TEXT_COLOR_HI:
                  colnum = font->colorHigh;
                  break;
               case TEXT_COLOR_ERROR:
                  colnum = font->colorError;
                  break;
               default:
                  colnum = c - 128;
                  break;
               }

               // check that colrng number is within bounds
               if(colnum >= 0 && colnum < CR_LIMIT)
                  color = font->colrngs[colnum];
            }
            break;
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
      else if(c == '\n')
      {
         cx = (flags & VTXT_ABSCENTER) ? 
               (screen->unscaledw - V_FontLineWidth(font, ch)) >> 1 : textdraw.x;
         cy += font->cy;
         continue;
      }
      else if(c == '\a') // don't draw BELs in linear fonts
         continue;
      
      // normalize character
      if(font->upper)
         c = ectype::toUpper(c) - font->start;
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
      
      if(font->linear && font->data)
      {
         // TODO: translucent masked block support
         // TODO: shadowed linear?
         int lx = (c & 31) * font->lsize;
         int ly = (c >> 5) * font->lsize;
         src = font->data + ly * (font->lsize << 5) + lx;

         V_DrawMaskedBlockTR(tx, cy, screen, font->lsize, font->lsize,
                             font->lsize << 5, src, color);
      }
      else
      {
         // draw character
         if(tl)
            V_DrawPatchTL(tx, cy, screen, patch, color, FTRANLEVEL);
         else
         {
            // haleyjd 10/04/05: text shadowing
            if(flags & VTXT_SHADOW)
               V_DrawPatchShadowed(tx, cy, screen, patch, color, FRACUNIT);
            else
               V_DrawPatchTranslated(tx, cy, screen, patch, color, false);
         }
      }
      
      // move over in x direction, applying width delta if appropriate
      cx += (font->centered ? font->cw : (w - font->dw));
   }   
}

//
// V_FontWriteText
//
// Write text normally.
//
void V_FontWriteText(vfont_t *font, const char *s, int x, int y, VBuffer *screen)
{
   edefstructvar(vtextdraw_t, text);

   text.font   = font;
   text.s      = s;
   text.x      = x;
   text.y      = y;
   text.screen = screen;
   text.flags  = VTXT_NORMAL;

   V_FontWriteTextEx(text);
}

//
// V_FontWriteTextColored
//
// Write text in a particular colour.
//
void V_FontWriteTextColored(vfont_t *font, const char *s, int color, int x, int y,
                            VBuffer *screen)
{
   if(color < 0 || color >= CR_LIMIT)
   {
      C_Printf("V_FontWriteTextColored: invalid color %i\n", color);
      return;
   }

   edefstructvar(vtextdraw_t, text);

   text.font   = font;
   text.s      = s;
   text.x      = x;
   text.y      = y;
   text.screen = screen;
   
   text.flags       = VTXT_FIXEDCOLOR;
   text.fixedColNum = color;

   V_FontWriteTextEx(text);
}

//
// V_FontWriteTextMapped
//
// Write text using a specified colormap.
//
void V_FontWriteTextMapped(vfont_t *font, const char *s, int x, int y, char *map,
                           VBuffer *screen)
{
   edefstructvar(vtextdraw_t, text);

   text.font   = font;
   text.s      = s;
   text.x      = x;
   text.y      = y;
   text.screen = screen;
   text.flags  = VTXT_NORMAL;
   text.altMap = map;

   V_FontWriteTextEx(text);
}

//
// V_FontWriteTextShadowed
//
// Write text with a shadow effect.
//
void V_FontWriteTextShadowed(vfont_t *font, const char *s, int x, int y,
                             VBuffer *screen, int color)
{
   edefstructvar(vtextdraw_t, text);

   text.font   = font;
   text.s      = s;
   text.x      = x;
   text.y      = y;
   text.screen = screen;
   text.flags  = VTXT_SHADOW;

   if(color >= 0 && color < CR_LIMIT)
   {
      text.flags |= VTXT_FIXEDCOLOR;
      text.fixedColNum = color;
   }

   V_FontWriteTextEx(text);
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
   patch_t *patch = nullptr;
   
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
         c = ectype::toUpper(c) - font->start;
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
   patch_t *patch = nullptr;
   
   c = (unsigned char)pChar;

   if(c >= 128) // color or control code
      return 0;
      
   if(c == '\n') // newline
      return 0;

   // normalize character
   if(font->upper)
      c = ectype::toUpper(c) - font->start;
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

//
// finds the narrowest character in the font
//
int16_t V_FontMinWidth(vfont_t *font)
{
   unsigned int i;
   int16_t w = INT16_MAX, pw;

   if(font->linear)
      return font->lsize;

   for(i = 0; i < font->size; ++i)
   {
      if(font->fontgfx[i])
      {
         pw = font->fontgfx[i]->width;

         if(pw < w)
            w = pw;
      }
   }

   return w;
}

//
// V_FontFitTextToRect
//
// Modify text so that it fits within a given rect, based on the width and height
// of characters in the provided font.
//
void V_FontFitTextToRect(vfont_t *font, char *msg, int x1, int y1, int x2, int y2)
{
   // get full message width and height
   bool fitsWidth  = false;
   int  fullWidth  = V_FontStringWidth(font, msg);
   int  fullHeight = V_FontStringHeight(font, msg);

   // fits within the width?
   if(x1 + fullWidth <= x2)
   {
      fitsWidth = true;

      // fits within the height?
      if(y1 + fullHeight <= y2)
         return; // no modification necessary.
   }

   // Adjust for width first, if needed.
   if(!fitsWidth)
   {
      char *rover = msg;
      char *currentBreakPos = nullptr;
      int width = 0;
      int widthSinceLastBreak = 0;

      while(*rover)
      {
         int charWidth = V_FontCharWidth(font, *rover);
         width += charWidth;
         widthSinceLastBreak += charWidth;

         if(*rover == ' ')
         {
            currentBreakPos = rover;
            widthSinceLastBreak = 0;
         }
         else if(*rover == '\n')
         {
            currentBreakPos = nullptr;
            width = widthSinceLastBreak = 0;
         }

         if(x1 + width > x2) // need to break
         {
            if(currentBreakPos)
            {
               *currentBreakPos = '\n';
               width = widthSinceLastBreak;
            }
            currentBreakPos = nullptr;
         }
         ++rover;
      }

      // recalculate the full height for the modified string
      fullHeight = V_FontStringHeight(font, msg);
   }

   // Adjust for height, if needed
   if(y1 + fullHeight > y2)
   {
      char *rover = msg + strlen(msg) - 1;
      
      // Clip off lines until it fits.
      while(rover != msg && y1 + fullHeight > y2)
      {
         if(*rover == '\n')
         {
            *rover = '\0';
            fullHeight -= font->cy;
         }
         --rover;
      }
   }
}

//
// V_FontFitTextToRect
//
// Modify text so that it fits within a given rect, based on the width and height
// of characters in the provided font.
//
// Variant for qstrings. There are functions in the class that make the process
// more efficient for height clipping purposes.
//
void V_FontFitTextToRect(vfont_t *font, qstring &msg, int x1, int y1, int x2, int y2)
{
   // get full message width and height
   bool fitsWidth  = false;
   int  fullWidth  = V_FontStringWidth(font, msg.constPtr());
   int  fullHeight = V_FontStringHeight(font, msg.constPtr());

   // fits within the width?
   if(x1 + fullWidth <= x2)
   {
      fitsWidth = true;

      // fits within the height?
      if(y1 + fullHeight <= y2)
         return; // no modification necessary.
   }

   // Adjust for width first, if needed.
   if(!fitsWidth)
   {
      size_t currentBreakPos = 0;
      int width = 0;
      int widthSinceLastBreak = 0;

      for(size_t i = 0; i < msg.length(); i++)
      {
         char c = msg[i];
         int  charWidth = V_FontCharWidth(font, c);

         width += charWidth;
         widthSinceLastBreak += charWidth;

         if(c == ' ')
         {
            widthSinceLastBreak = 0;
            currentBreakPos = i;
         }
         else if(c == '\n')
         {
            width = widthSinceLastBreak = 0;
            currentBreakPos = 0;
         }

         if(x1 + width > x2) // need to break
         {
            if(currentBreakPos)
            {
               msg[currentBreakPos] = '\n';
               width = widthSinceLastBreak;
            }
            currentBreakPos = 0;
         }
      }

      // recalculate the full height for the modified string
      fullHeight = V_FontStringHeight(font, msg.constPtr());
   }

   // Clip off lines until it fits.
   while(y1 + fullHeight > y2)
   {
      size_t lastSlashN = msg.findLastOf('\n');
      if(lastSlashN == qstring::npos)
         break; // Oh well...
      msg.truncate(lastSlashN);
      fullHeight -= font->cy;
   }
}

//
// V_FontGetUsedColors
//
// Determines all of the colors that are used by patches in a
// patch font. Linear fonts are not supported here yet.
//
byte *V_FontGetUsedColors(vfont_t *font)
{
   if(font->linear) // not supported yet...
      return nullptr;

   byte *colorsUsed = ecalloc(byte *, 1, 256);

   for(unsigned int i = 0; i < font->size; i++)
   {
      if(font->fontgfx[i])
         PatchLoader::GetUsedColors(font->fontgfx[i], colorsUsed);
   }

   return colorsUsed;
}

// EOF

