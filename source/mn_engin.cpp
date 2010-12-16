// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
// New menu
//
// The menu engine. All the menus themselves are in mn_menus.c
//
// By Simon Howard
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_gi.h"      // haleyjd: global game mode info
#include "d_io.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_main.h"
#include "d_deh.h"
#include "g_bind.h"    // haleyjd: dynamic key bindings
#include "g_game.h"
#include "hu_over.h"
#include "i_video.h"
#include "m_swap.h"
#include "mn_engin.h"
#include "mn_emenu.h"
#include "mn_menus.h"
#include "mn_misc.h"
#include "psnprntf.h"
#include "r_defs.h"
#include "r_draw.h"
#include "s_sound.h"
#include "w_wad.h"
#include "v_video.h"
#include "e_fonts.h"

//=============================================================================
//
// Global Variables
//

boolean inhelpscreens; // indicates we are in or just left a help screen

// menu error message
char menu_error_message[128];
int menu_error_time = 0;

// haleyjd 02/12/06: option to allow the toggle menu action to back up 
// through menus instead of exiting directly, to emulate ports like zdoom.
boolean menu_toggleisback; 

        // input for typing in new value
static command_t *input_command = NULL;       // NULL if not typing in
static int input_cmdtype = c_typed;           // haleyjd 07/15/09

// haleyjd 04/29/02: needs to be unsigned
static unsigned char input_buffer[1024] = "";

vfont_t *menu_font;
vfont_t *menu_font_big;
vfont_t *menu_font_normal;

// haleyjd 08/25/09
char *mn_background;
const char *mn_background_flat;

//=============================================================================
//
// Static Declarations and Data
//

static void MN_PageMenu(menu_t *newpage);

// sliders
enum
{
   slider_left,
   slider_right,
   slider_mid,
   slider_slider,
   num_slider_gfx
};
static patch_t *slider_gfx[num_slider_gfx];

static menu_t *drawing_menu; // menu currently being drawn

static int skulls[2]; // skull pointer

// haleyjd 02/04/06: small menu pointer
#define NUMSMALLPTRS 8
static int smallptrs[NUMSMALLPTRS];
static int16_t smallptr_dims[2]; // 0 = width, 1 = height
static int smallptr_idx;
static int smallptr_coords[2][2];

//=============================================================================
// 
// Menu Drawing - Utilities
//

// gap from variable description to value
#define GAP 20
#define SKULL_HEIGHT 19
#define BLINK_TIME 8

// haleyjd 08/30/06: emulated old menus have fixed item size of 16
#define EMULATED_ITEM_SIZE 16 

// width of slider, in mid-patches
#define SLIDE_PATCHES 9

// haleyjd 05/01/10: item alignment defines
enum
{
   ALIGNMENT_RIGHT,
   ALIGNMENT_LEFT,
   ALIGNMENT_CENTER
};

//
// MN_DrawSmallPtr
//
// An external routine for other modules that need to draw a small menu
// pointer for purposes other than its normal use.
//
void MN_DrawSmallPtr(int x, int y)
{
   V_DrawPatch(x, y, &vbscreen, 
               (patch_t *)(W_CacheLumpNum(smallptrs[smallptr_idx], PU_CACHE)));
}

//
// MN_GetItemVariable
//
static void MN_GetItemVariable(menuitem_t *item)
{
   // get variable if neccesary
   if(!item->var)
   {
      command_t *cmd;

      // use data for variable name
      if(!(cmd = C_GetCmdForName(item->data)))
      {
         C_Printf(FC_ERROR "variable not found: %s\n", item->data);
         item->type = it_info;   // turn into normal unselectable text
         item->var = NULL;
         return;
      }

      item->var = cmd->variable;
   }
}

//
// MN_FindFirstSelectable
//
// haleyjd 09/18/08: Finds the first selectable item in the menu,
// and returns the index of the item in the menuitem array.
//
static int MN_FindFirstSelectable(menu_t *menu)
{
   int i = 0, ret = 0;

   for(; menu->menuitems[i].type != it_end; ++i)
   {
      if(!is_a_gap(&menu->menuitems[i]))
      {
         ret = i;
         break;
      }
   }

   return ret;
}

//
// MN_FindLastSelectable
//
// haleyjd 09/18/08: Finds the last selectable item in the menu,
// and returns the index of the item in the menuitem array.
//
static int MN_FindLastSelectable(menu_t *menu)
{
   int i = 0, ret = 0;

   // first, find end
   for(; menu->menuitems[i].type != it_end; ++i)
      ; /* do nothing loop */
   
   // back up one, but not beyond the beginning
   if(i > 0)
      --i;

   // search backward
   for(; i >= 0; --i)
   {
      if(!is_a_gap(&menu->menuitems[i]))
      {
         ret = i;
         break;
      }
   }

   return ret;
}

//
// MN_DrawSlider
//
// draw a 'slider' (for sound volume, etc)
//
static int MN_DrawSlider(int x, int y, int pct)
{
   int i;
   int draw_x = x;
   int slider_width = 0;       // find slider width in pixels
   int16_t wl, wm, ws, hs;

   // load slider gfx
   slider_gfx[slider_left]   = (patch_t *)W_CacheLumpName("M_SLIDEL", PU_STATIC);
   slider_gfx[slider_right]  = (patch_t *)W_CacheLumpName("M_SLIDER", PU_STATIC);
   slider_gfx[slider_mid]    = (patch_t *)W_CacheLumpName("M_SLIDEM", PU_STATIC);
   slider_gfx[slider_slider] = (patch_t *)W_CacheLumpName("M_SLIDEO", PU_STATIC);

   wl = SwapShort(slider_gfx[slider_left]->width);
   wm = SwapShort(slider_gfx[slider_mid]->width);
   ws = SwapShort(slider_gfx[slider_slider]->width);
   hs = SwapShort(slider_gfx[slider_slider]->height);

   // haleyjd 04/09/2010: offset y relative to menu font
   if(menu_font->absh > hs + 1)
   {
      int yamt = menu_font->absh - hs;

      // tend toward the higher pixel when amount is odd
      if(yamt % 2)
         ++yamt;

      y += yamt / 2;
   }
  
   V_DrawPatch(draw_x, y, &vbscreen, slider_gfx[slider_left]);
   draw_x += wl;
  
   for(i = 0; i < SLIDE_PATCHES; ++i)
   {
      V_DrawPatch(draw_x, y, &vbscreen, slider_gfx[slider_mid]);
      draw_x += wm - 1;
   }
   
   V_DrawPatch(draw_x, y, &vbscreen, slider_gfx[slider_right]);
  
   // find position to draw slider patch
   
   slider_width = (wm - 1) * SLIDE_PATCHES;
   draw_x = wl + (pct * (slider_width - ws)) / 100;
   
   V_DrawPatch(x + draw_x, y, &vbscreen, slider_gfx[slider_slider]);

   // haleyjd: set slider gfx purgable
   Z_ChangeTag(slider_gfx[slider_left],   PU_CACHE);
   Z_ChangeTag(slider_gfx[slider_right],  PU_CACHE);
   Z_ChangeTag(slider_gfx[slider_mid],    PU_CACHE);
   Z_ChangeTag(slider_gfx[slider_slider], PU_CACHE);

   return x + draw_x /*+ ws / 2*/;
}

//
// MN_DrawThermo
//
// haleyjd 08/30/06: Needed for old menu emulation; this draws an old-style
// huge slider.
//
// TODO/FIXME: allow DeHackEd replacement of patch names
//
static void MN_DrawThermo(int x, int y, int thermWidth, int thermDot)
{
   int xx, i;

   xx = x;
   V_DrawPatch(xx, y, &vbscreen,
               (patch_t *)W_CacheLumpName("M_THERML", PU_CACHE));
   
   xx += 8;
   
   for(i = 0; i < thermWidth; ++i)
   {
      V_DrawPatch(xx, y, &vbscreen,
                  (patch_t *)W_CacheLumpName("M_THERMM", PU_CACHE));
      xx += 8;
   }
   
   V_DrawPatch(xx, y, &vbscreen,
               (patch_t *)W_CacheLumpName("M_THERMR", PU_CACHE));
   
   V_DrawPatch((x + 8) + thermDot*8, y, &vbscreen,
               (patch_t *)W_CacheLumpName("M_THERMO", PU_CACHE));
}

//
// MN_CalcWidestWidth
//
// haleyjd 03/22/09: A fix for LALIGNED menus: the menu item values will
// all draw at a fixed position from the widest description string.
//
static void MN_CalcWidestWidth(menu_t *menu)
{
   int itemnum;

   if(menu->widest_width) // do only once
      return;

   for(itemnum = 0; menu->menuitems[itemnum].type != it_end; ++itemnum)
   {
      menuitem_t *item = &(menu->menuitems[itemnum]);

      // only LALIGNED items are taken into account
      if(item->flags & MENUITEM_LALIGNED)
      {
         int desc_width = 
            item->flags & MENUITEM_BIGFONT ?
               V_FontStringWidth(menu_font_big, item->description) 
               : MN_StringWidth(item->description);

         if(desc_width > menu->widest_width)
            menu->widest_width = desc_width;
      }
   }
}

//=============================================================================
//
// Menu Item Drawers
//
// haleyjd 05/01/10: broke up MN_DrawMenuItem into routines per item type.
//

//
// MN_drawPatchForItem
//
// haleyjd 05/01/10: Draws an item's patch if appropriate. Returns true if
// MN_DrawMenuItem should return. item_height may be modified on return.
//
static boolean MN_drawPatchForItem(menuitem_t *item, int *item_height, 
                                   int color, int alignment)
{
   patch_t *patch;
   int lumpnum;
   int x = item->x;
   int y = item->y;

   lumpnum = W_CheckNumForName(item->patch);

   // default to text-based message if patch missing
   if(lumpnum >= 0)
   {
      int16_t width;
      patch = (patch_t *)(W_CacheLumpNum(lumpnum, PU_CACHE));
      *item_height = SwapShort(patch->height) + 1;
      width  = SwapShort(patch->width);

      // check for left-aligned
      if(alignment != ALIGNMENT_LEFT) 
         x -= width;

      // adjust x if a centered title
      if(item->type == it_title)
         x = (SCREENWIDTH - width)/2;

      V_DrawPatchTranslated(x, y, &vbscreen, patch, colrngs[color], 0);

      // haleyjd 05/16/04: hack for traditional menu support;
      // this was hard-coded in the old system
      if(drawing_menu->flags & mf_emulated)
         *item_height = EMULATED_ITEM_SIZE; 

      if(item->type != it_bigslider)
         return true; // return from MN_DrawMenuItem
   }

   return false; // no patch was drawn, do not return from MN_DrawMenuItem
}

//
// MN_titleDescription
//
// Draws the description for a title item.
// Returns item height.
//
static int MN_titleDescription(menuitem_t *item)
{
   // draw the description centered
   const char *text = item->description;
   int x = (SCREENWIDTH - V_FontStringWidth(menu_font_big, text)) / 2;

   if(!(drawing_menu->flags & mf_skullmenu) &&
      GameModeInfo->flags & GIF_SHADOWTITLES)
   {
      V_FontWriteTextShadowed(menu_font_big, text, x, item->y);
   }
   else
   {
      V_FontWriteText(menu_font_big, text, x, item->y);
   }
      
   return V_FontStringHeight(menu_font_big, text);
}

//
// MN_genericDescription
//
// Draws a generic menuitem description.
// May modify *item_height and *item_width
//
static void MN_genericDescription(menuitem_t *item, 
                                  int *item_height, int *item_width,
                                  int alignment, int color)
{
   int x, y;
   
   *item_width = 
      (item->flags & MENUITEM_BIGFONT) ?
         V_FontStringWidth(menu_font_big, item->description) : 
         MN_StringWidth(item->description);
      
   if(item->flags & MENUITEM_CENTERED || alignment == ALIGNMENT_CENTER)
      x = (SCREENWIDTH - *item_width) / 2;
   else if(item->flags & MENUITEM_LALIGNED)
      x = 12;
   else
      x = item->x - (alignment == ALIGNMENT_LEFT ? 0 : *item_width);

   y = item->y;

   // write description
   if(item->flags & MENUITEM_BIGFONT)
   {
      V_FontWriteText(menu_font_big, item->description, x, y);
      *item_height = V_FontStringHeight(menu_font_big, item->description);
   }
   else
      MN_WriteTextColored(item->description, color, x, y);

   // haleyjd 02/04/06: set coordinates for small pointers
   // left pointer:
   smallptr_coords[0][0] = x - (smallptr_dims[0] + 1);
   smallptr_coords[0][1] = y + ((*item_height - smallptr_dims[1]) / 2);

   // right pointer:
   smallptr_coords[1][0] = x + *item_width + 2;
   smallptr_coords[1][1] = smallptr_coords[0][1];
}

//
// MN_drawItemBinding
//
// Draws a keybinding item.
//
static void MN_drawItemBinding(menuitem_t *item, int color, int alignment,
                               int desc_width)
{
   const char *boundkeys = G_BoundKeys(item->data);
   int x = item->x;
   int y = item->y;

   if(drawing_menu->flags & mf_background)
   {
      // include gap on fullscreen menus
      x += GAP;

      // adjust color for different colored variables
      if(color == GameModeInfo->unselectColor)
         color = GameModeInfo->variableColor;
   }
         
   // write variable value text
   MN_WriteTextColored(boundkeys, color, 
                       x + (alignment == ALIGNMENT_LEFT ? desc_width : 0), y);
}

//
// MN_truncateInput
//
// haleyjd 10/18/10: Avoid losing sight of the input caret when editing.
//
static void MN_truncateInput(qstring_t *qstr, int x)
{
   int width = MN_StringWidth(QStrConstPtr(qstr));

   if(x + width > SCREENWIDTH - 8) // too wide to fit?
   {
      int subbed_width = 0;
      int leftbound = SCREENWIDTH - 8;
      int dotwidth  = MN_StringWidth("...");
      const char *start = QStrConstPtr(qstr);
      const char *end   = QStrBufferAt(qstr, QStrLen(qstr) - 1);

      while(start != end && (x + dotwidth + width - subbed_width > leftbound))
      {
         subbed_width += V_FontCharWidth(menu_font, *start);
         ++start;
      }
      
      if(start != end)
      {
         const char *temp = Z_Strdupa(start); // make a temp copy
         QStrCat(QStrCopy(qstr, "..."), temp);
      }
   }
}

//
// MN_truncateValue
//
// haleyjd 10/18/10: Avoid long values going off screen, as it is unsightly
//
static void MN_truncateValue(qstring_t *qstr, int x)
{
   int width = MN_StringWidth(QStrConstPtr(qstr));
   
   if(x + width > SCREENWIDTH - 8) // too wide to fit?
   {
      int subbed_width = 0;
      int leftbound = (SCREENWIDTH - 8) - MN_StringWidth("...");
      const char *start = QStrConstPtr(qstr);
      const char *end   = QStrBufferAt(qstr, QStrLen(qstr) - 1);

      while(end != start && (x + width - subbed_width > leftbound))
      {
         subbed_width += V_FontCharWidth(menu_font, *end);
         --end;
      }

      // truncate the value at end position, and concatenate dots
      if(end != start)
         QStrCat(QStrTruncate(qstr, end - start), "...");
   }
}

//
// MN_drawItemToggleOrVariable
//
// Draws a toggle or variable item.
//
static void MN_drawItemToggleVar(menuitem_t *item, int color, 
                                 int alignment, int desc_width)
{
   static qstring_t varvalue; // temp buffer
   int x = item->x;
   int y = item->y;
         
   if(drawing_menu->flags & mf_background)
   {
      // include gap on fullscreen menus
      if(item->flags & MENUITEM_LALIGNED)
         x = 8 + drawing_menu->widest_width + 16;  // haleyjd: use widest_width
      else
         x += GAP;
      
      // adjust colour for different coloured variables
      if(color == GameModeInfo->unselectColor) 
         color = GameModeInfo->variableColor;
   }

   if(alignment == ALIGNMENT_LEFT)
      x += desc_width;

   // create variable description:
   // Use console variable descriptions.

   QStrClearOrCreate(&varvalue, 1024);
   MN_GetItemVariable(item);

   // display input buffer if inputting new var value
   if(input_command && item->var == input_command->variable)
   {
      QStrPutc(QStrCopy(&varvalue, (char *)input_buffer), '_');
      MN_truncateInput(&varvalue, x);
   }
   else
   {
      QStrCopy(&varvalue, C_VariableStringValue(item->var));
      MN_truncateValue(&varvalue, x);
   }         

   // draw it
   MN_WriteTextColored(QStrConstPtr(&varvalue), color, x, y);
}

//
// MN_drawItemSlider
//
// Draws a slider item.
//
static void MN_drawItemSlider(menuitem_t *item, int color, int alignment,
                              int desc_width)
{
   variable_t *var;
   int x = item->x;
   int y = item->y;

   MN_GetItemVariable(item);

   // draw slider -- ints or floats (haleyjd 04/22/10)
   if((var = item->var))
   {
      if(var->type == vt_int || var->type == vt_toggle)
      {
         int range = var->max - var->min;
         int posn;
         
         if(var->type == vt_int)
            posn = *(int *)var->variable - var->min;
         else
            posn = (int)(*(boolean *)var->variable) - var->min;

         MN_DrawSlider(x + GAP, y, (posn*100) / range);
      }
      else if(var->type == vt_float)
      {
         double range = var->dmax - var->dmin;
         double posn  = *(double *)var->variable - var->dmin;
         int sliderxpos;

         sliderxpos = MN_DrawSlider(x + GAP, y, (int)((posn*100) / range));

         if(drawing_menu &&
            item - drawing_menu->menuitems == drawing_menu->selected)
         {
            char doublebuf[128];
            int box_x, box_y, box_w, box_h, text_x, text_y, text_w, text_h;

            psnprintf(doublebuf, sizeof(doublebuf), "%.2f", *(double *)var->variable);
            text_w = V_FontStringWidth(menu_font, doublebuf);
            text_h = V_FontStringHeight(menu_font, doublebuf);
            text_x = sliderxpos - text_w / 2;
            text_y = y - (text_h + 4 + 2) - 1;

            box_x = text_x - 4;
            box_y = text_y - 4;
            box_w = text_w + 8;
            box_h = text_h + 8;

            V_DrawBox(box_x, box_y, box_w, box_h);
            V_FontWriteText(menu_font, doublebuf, text_x, text_y);
         }
      }
   }
}

//
// MN_drawItemBigSlider
//
// Draws a big slider, vanilla-style, for emulated menus.
//
static void MN_drawItemBigSlider(menuitem_t *item, int color, int alignment,
                                 int desc_width)
{
   int x = item->x;
   int y = item->y;

   MN_GetItemVariable(item);
   
   if(item->var && item->var->type == vt_int)
   {
      // note: the thermometer is drawn in the position of the next menu
      // item, so place a gap underneath it.
      // FIXME/TODO: support on non-emulated menus
      MN_DrawThermo(x, y + EMULATED_ITEM_SIZE, 
                    item->var->max - item->var->min + 1, 
                    *(int *)item->var->variable);
   }
}

//
// MN_drawItemAutomap
//
// Draws an automap color selection item.
//
static void MN_drawItemAutomap(menuitem_t *item, int color, int alignment,
                               int desc_width)
{
   int ix = item->x;
   int iy = item->y;
   int amcolor;
   byte block[BLOCK_SIZE*BLOCK_SIZE];
         
   MN_GetItemVariable(item);

   if(!item->var || item->var->type != vt_int)
      return;
         
   // find colour of this variable from console variable
   amcolor = *(int *)item->var->variable;
         
   // create block
   // border
   memset(block, GameModeInfo->blackIndex, BLOCK_SIZE*BLOCK_SIZE);
         
   if(amcolor)
   {
      int bx, by;

      // middle
      for(bx = 1; bx < BLOCK_SIZE - 1; bx++)
         for(by = 1; by < BLOCK_SIZE - 1; by++)
            block[by * BLOCK_SIZE + bx] = (byte)amcolor;
   }
         
   // draw it         
   V_DrawBlock(ix + GAP, iy - 1, &vbscreen, BLOCK_SIZE, BLOCK_SIZE, block);

   // draw patch w/cross         
   if(!amcolor)
      V_DrawPatch(ix + GAP + 1, iy, &vbscreen, (patch_t *)W_CacheLumpName("M_PALNO", PU_CACHE));
}

typedef void (*mn_itemdrawerfunc_t)(menuitem_t *item, int color, int alignment,
                                    int desc_width);

// haleyjd: item drawer methods
static mn_itemdrawerfunc_t MN_itemDrawerFuncs[] =
{
   NULL,                 // it_gap
   NULL,                 // it_runcmd
   MN_drawItemToggleVar, // it_variable
   MN_drawItemToggleVar, // it_variable_nd
   MN_drawItemToggleVar, // it_toggle
   NULL,                 // it_title
   NULL,                 // it_info
   MN_drawItemSlider,    // it_slider
   MN_drawItemBigSlider, // it_bigslider
   MN_drawItemAutomap,   // it_automap
   MN_drawItemBinding,   // it_binding
   NULL                  // it_end
};

//
// MN_DrawMenuItem
//
// draw a menu item. returns the height in pixels
//
static int MN_DrawMenuItem(menuitem_t *item, int x, int y, int color)
{
   int desc_width = 0;
   int alignment;
   int item_height = menu_font->cy; // haleyjd: most items are the size of one text line

   // haleyjd: determine alignment
   if(drawing_menu->flags & mf_centeraligned)
      alignment = ALIGNMENT_CENTER;
   else if(drawing_menu->flags & (mf_skullmenu | mf_leftaligned))
      alignment = ALIGNMENT_LEFT;
   else
      alignment = ALIGNMENT_RIGHT;

   item->x = x; item->y = y;        // save x,y to item
   item->flags |= MENUITEM_POSINIT; // haleyjd 04/14/03

   // skip drawing if a gap
   if(item->type == it_gap)
   {
      // haleyjd 08/30/06: emulated menus have fixed item size
      if(drawing_menu->flags & mf_emulated)
         item_height = EMULATED_ITEM_SIZE;

      // haleyjd 10/09/05: gap size override for menus
      if(drawing_menu->gap_override)
         item_height = drawing_menu->gap_override;

      return item_height;
   }
 
   // draw an alternate patch?

   // haleyjd: gamemodes that use big menu font don't use pics, ever
   if(item->patch && !(GameModeInfo->flags & GIF_MNBIGFONT) &&
      MN_drawPatchForItem(item, &item_height, color, alignment))
      return item_height; // if returned true, we are done.

   // draw description text
   
   if(item->type == it_title)
      item_height = MN_titleDescription(item);
   else if(item->type != it_bigslider || !item->patch)
      MN_genericDescription(item, &item_height, &desc_width, alignment, color);

   // draw other data: variable data etc.

   if(MN_itemDrawerFuncs[item->type])
      MN_itemDrawerFuncs[item->type](item, color, alignment, desc_width);

   return item_height;
}

//=============================================================================
//
// Menu Item Help Strings
//
// haleyjd: Routines to get a help message for a menu item.
//

//
// MN_variableHelp
//
// Gets help string for a variable item.
//
static const char *MN_variableHelp(menuitem_t *item, char *msgbuffer)
{
   if(input_command)
      return "Press escape to cancel";
   else
   {
      char *key = G_FirstBoundKey("menu_confirm");
      psnprintf(msgbuffer, 64, "Press %s to change", key);
      return msgbuffer;
   }
}

//
// MN_toggleHelp
//
// Gets help string for a toggle item.
//
static const char *MN_toggleHelp(menuitem_t *item, char *msgbuffer)
{
   // enter to change boolean variables
   // left/right otherwise

   if(item->var->type == vt_toggle ||
      (item->var->type == vt_int && item->var->max - item->var->min == 1))
   {
      char *key = G_FirstBoundKey("menu_confirm");
      psnprintf(msgbuffer, 64, "press %s to change", key);
      return msgbuffer;
   }
   else
      return "use left/right to change value";
}

//
// MN_runcmdHelp
//
// Gets help string for a runcmd item.
//
static const char *MN_runcmdHelp(menuitem_t *item, char *msgbuffer)
{
   char *key = G_FirstBoundKey("menu_confirm");
   psnprintf(msgbuffer, 64, "press %s to execute", key);
   return msgbuffer;
}

//
// MN_sliderHelp
//
// Gets help string for a slider item.
//
static const char *MN_sliderHelp(menuitem_t *item, char *msgbuffer)
{
   return "use left/right to change value";
}

typedef const char *(*mn_helpfunc_t)(menuitem_t *item, char *msgbuffer);

static mn_helpfunc_t MN_helpStrFuncs[] =
{
   NULL,            // it_gap
   MN_runcmdHelp,   // it_runcmd
   MN_variableHelp, // it_variable
   MN_variableHelp, // it_variable_nd
   MN_toggleHelp,   // it_toggle
   NULL,            // it_title
   NULL,            // it_info
   MN_sliderHelp,   // it_slider
   MN_sliderHelp,   // it_bigslider
   NULL,            // it_automap
   NULL,            // it_binding
   NULL             // it_end
};

//=============================================================================
//
// Menu Drawing
//

//
// MN_drawPointer
//
// Utility for MN_DrawMenu - draws the appropriate pointer for the currently
// selected menu item.
//
static void MN_drawPointer(menu_t *menu, int y, int itemnum, int item_height)
{
   if(menu->flags & mf_skullmenu)
   {      
      int item_x = menu->x - 30;                         // 30 left
      int item_y = y + (item_height - SKULL_HEIGHT) / 2; // midpoint

      // haleyjd 05/16/04: hack for traditional menu support
      if(menu->flags & mf_emulated)
      {
         item_x = menu->x - 32;
         item_y = menu->y - 5 + itemnum * 16; // fixed-height items
      }

      V_DrawPatch(item_x, item_y, &vbscreen,
         (patch_t *)(W_CacheLumpNum(skulls[(menutime / BLINK_TIME) % 2], PU_CACHE)));
   }
   else
   {
      // haleyjd 02/04/06: draw small pointers

      // draw left pointer
      V_DrawPatch(smallptr_coords[0][0], smallptr_coords[0][1], &vbscreen,
         (patch_t *)(W_CacheLumpNum(smallptrs[smallptr_idx], PU_CACHE)));

      // draw right pointer
      V_DrawPatch(smallptr_coords[1][0], smallptr_coords[1][1], &vbscreen, 
         (patch_t *)(W_CacheLumpNum(smallptrs[(NUMSMALLPTRS - smallptr_idx) % NUMSMALLPTRS],
                                    PU_CACHE)));
   }
}

//
// MN_drawPageIndicator
//
// Draws a prev or next page indicator near the bottom of the screen for
// multipage menus.
// Pass false to draw a prev indicator, or true to draw a next indicator.
//
static void MN_drawPageIndicator(boolean next)
{
   char msgbuffer[64];
   const char *actionname, *speckeyname, *replkeyname, *fmtstr, *key;
   
   if(next) // drawing a next indicator
   {
      actionname  = "menu_pagedown"; // name of action
      speckeyname = "pgdn";          // special default key to check for
      replkeyname = "page down";     // pretty name to replace with
      fmtstr      = "%s ->";         // format string for indicator
   }
   else     // drawing a prev indicator
   {
      actionname  = "menu_pageup";
      speckeyname = "pgup";
      replkeyname = "page up";
      fmtstr      = "<- %s";
   }

   // get name of first key bound to the prev or next menu action
   key = G_FirstBoundKey(actionname);

   // replace pgup / pgdn with prettier names, since they are the defaults
   if(!strcasecmp(key, speckeyname))
      key = replkeyname;

   psnprintf(msgbuffer, sizeof(msgbuffer), fmtstr, key);

   MN_WriteTextColored(msgbuffer, CR_GOLD, 
                       next ? 310 - MN_StringWidth(msgbuffer) : 10, 
                       SCREENHEIGHT - (2 * menu_font->absh + 1));
}

//
// MN_drawStatusText
//
// Draws the help and error messages centered at the bottom of options menus.
//
static void MN_drawStatusText(const char *text, int color)
{
   int x, y;

   // haleyjd: fix y coordinate to use appropriate text metrics
   x = 160 - MN_StringWidth(text) / 2;
   y = SCREENHEIGHT - menu_font->absh;

   MN_WriteTextColored(text, color, x, y);
}

//
// MN_DrawMenu
//
// MAIN FUNCTION
//
// Draw a menu.
//
void MN_DrawMenu(menu_t *menu)
{
   int y;
   int itemnum;

   if(!menu) // haleyjd 04/20/03
      return;

   drawing_menu = menu; // needed by DrawMenuItem
   y = menu->y; 
      
   // draw background

   if(menu->flags & mf_background)
   {
      // haleyjd 04/04/10: draw menus higher in some game modes
      y -= GameModeInfo->menuOffset;
      V_DrawBackground(mn_background_flat, &vbscreen);
   }

   // haleyjd: calculate widest width for LALIGNED flag
   MN_CalcWidestWidth(menu);

   // menu-specific drawer function, if any

   if(menu->drawer)
      menu->drawer();

   // draw items in menu

   for(itemnum = 0; menu->menuitems[itemnum].type != it_end; ++itemnum)
   {
      int item_height;
      int item_color;

      // choose item colour based on selected item

      item_color = menu->selected == itemnum &&
         !(menu->flags & mf_skullmenu) ? 
            GameModeInfo->selectColor : GameModeInfo->unselectColor;
      
      // draw item

      item_height = MN_DrawMenuItem(&menu->menuitems[itemnum],
                                    menu->x, y, item_color);
      
      // if selected item, draw skull / pointer next to it

      if(menu->selected == itemnum)
         MN_drawPointer(menu, y, itemnum, item_height);
      
      // go down by item height
      y += item_height;
   }

   if(menu->flags & mf_skullmenu)
      return; // no help msg in skull menu

   // choose help message to print
   
   if(menu_error_time) // error message takes priority
   {
      // make it flash
      if((menu_error_time / 8) % 2)
         MN_drawStatusText(menu_error_message, CR_TAN);
   }
   else
   {
      char msgbuffer[64];
      const char *helpmsg = "";
      menuitem_t *menuitem;

      // write some help about the item
      menuitem = &menu->menuitems[menu->selected];
      
      if(MN_helpStrFuncs[menuitem->type])
         helpmsg = MN_helpStrFuncs[menuitem->type](menuitem, msgbuffer);

      MN_drawStatusText(helpmsg, CR_GOLD);
   }

   // haleyjd 10/07/05: draw next/prev messages for menus that
   // have multiple pages.
   if(menu->prevpage)
      MN_drawPageIndicator(false);

   if(menu->nextpage)
      MN_drawPageIndicator(true);
}

//
// MN_CheckFullScreen
//
// Called by D_Drawer to see if the menu is in full-screen mode --
// this allows the game to skip all other drawing, keeping the
// framerate at 35 fps.
//
boolean MN_CheckFullScreen(void)
{
   if(!menuactive || !current_menu)
      return false;

   if(!(current_menu->flags & mf_background) || hide_menu ||
      (current_menuwidget && !current_menuwidget->fullscreen))
      return false;

   return true;
}

//=============================================================================
//
// Menu Module Functions
//
// Drawer, ticker etc.
//

#define MENU_HISTORY 128

boolean menuactive = false;             // menu active?
menu_t *current_menu;   // the current menu_t being displayed
static menu_t *menu_history[MENU_HISTORY];   // previously selected menus
static int menu_history_num;                 // location in history
int hide_menu = 0;      // hide the menu for a duration of time
int menutime = 0;

// menu widget for alternate drawer + responder
menuwidget_t *current_menuwidget = NULL; 

int quickSaveSlot;  // haleyjd 02/23/02: restored from MBF

static void MN_InitFonts(void);

//
// MN_SetBackground
//
// Called to change the menu background.
//
static void MN_SetBackground(void)
{
   // TODO: allow BEX string replacement?
   if(mn_background && mn_background[0] && strcmp(mn_background, "default") &&
      R_CheckForFlat(mn_background) != -1)
   {
      mn_background_flat = mn_background;
   }
   else
      mn_background_flat = GameModeInfo->menuBackground;
}

//
// MN_Init
//
// Primary menu initialization. Called at startup.
//
void MN_Init(void)
{
   char *cursorPatch1 = GameModeInfo->menuCursor->patch1;
   char *cursorPatch2 = GameModeInfo->menuCursor->patch2;
   int i;

   skulls[0] = W_GetNumForName(cursorPatch1);
   skulls[1] = W_GetNumForName(cursorPatch2);

   // haleyjd 02/04/06: load small pointer
   for(i = 0; i < NUMSMALLPTRS; ++i)
   {
      char name[9];

      psnprintf(name, 9, "EEMNPTR%d", i);

      smallptrs[i] = W_GetNumForName(name);
   }

   // get width and height from first patch
   {
      patch_t *ptr0 = (patch_t *)(W_CacheLumpNum(smallptrs[0], PU_CACHE));
      smallptr_dims[0] = SwapShort(ptr0->width);
      smallptr_dims[1] = SwapShort(ptr0->height);
   }
      
   quickSaveSlot = -1; // haleyjd: -1 == no slot selected yet

   // haleyjd: init heretic stuff if appropriate
   if(GameModeInfo->type == Game_Heretic)
      MN_HInitSkull(); // initialize spinning skulls
   
   MN_InitMenus();     // create menu commands in mn_menus.c
   MN_InitFonts();     // create menu fonts
   MN_SetBackground(); // set background

   // haleyjd 07/03/09: sync up mn_classic_menus
   MN_LinkClassicMenus(mn_classic_menus);
}

//////////////////////////////////
// ticker

void MN_Ticker(void)
{
   if(menu_error_time)
      menu_error_time--;
   if(hide_menu)                   // count down hide_menu
      hide_menu--;
   menutime++;

   // haleyjd 02/04/06: determine small pointer index
   if((menutime & 3) == 0)
      smallptr_idx = (smallptr_idx + 1) % NUMSMALLPTRS;

   // haleyjd 05/29/06: tick for any widgets
   if(current_menuwidget && current_menuwidget->ticker)
      current_menuwidget->ticker();
}

////////////////////////////////
// drawer

void MN_Drawer(void)
{ 
   // redraw needed if menu hidden
   if(hide_menu) redrawsbar = redrawborder = true;
   
   // activate menu if displaying widget
   if(current_menuwidget && !menuactive)
      MN_StartControlPanel();
      
   if(current_menuwidget)
   {
      // alternate drawer
      if(current_menuwidget->drawer)
         current_menuwidget->drawer();
      return;
   }

   // haleyjd: moved down to here
   if(!menuactive || hide_menu) 
      return;
 
   MN_DrawMenu(current_menu);
}

static void MN_ShowContents(void);

extern const char *shiftxform;

//
// MN_Responder
//
// haleyjd 07/03/04: rewritten to use enhanced key binding system
//
boolean MN_Responder(event_t *ev)
{
   // haleyjd 04/29/02: these need to be unsigned
   unsigned char tempstr[128];
   unsigned char ch;
   int *menuSounds = GameModeInfo->menuSounds; // haleyjd
   static boolean ctrldown = false;
   static boolean shiftdown = false;
   static boolean altdown = false;

   // haleyjd 07/03/04: call G_KeyResponder with kac_menu to filter
   // for menu-class actions
   G_KeyResponder(ev, kac_menu);

   // haleyjd 10/07/05
   if(ev->data1 == KEYD_RCTRL)
      ctrldown = (ev->type == ev_keydown);

   // haleyjd 03/11/06
   if(ev->data1 == KEYD_RSHIFT)
      shiftdown = (ev->type == ev_keydown);

   // haleyjd 07/05/10
   if(ev->data1 == KEYD_RALT)
      altdown = (ev->type == ev_keydown);

   // we only care about key presses
   switch(ev->type)
   {
   case ev_keydown:
      break;
   default: // not interested in anything else
      return false;
   }

   // are we displaying a widget?
   if(current_menuwidget)
   {
      return
         current_menuwidget->responder ?
            current_menuwidget->responder(ev) : false;
   }

   // are we inputting a new value into a variable?
   if(input_command)
   {
      unsigned char ch = 0;
      variable_t *var = input_command->variable;
      
      if(ev->data1 == KEYD_ESCAPE)        // cancel input
         input_command = NULL;      
      else if(ev->data1 == KEYD_ENTER)
      {
         if(input_buffer[0] || (input_command->flags & cf_allowblank))
         {
            if(input_buffer[0])
            {
               // place " marks round the new value
               // FIXME/TODO: use qstring for input_buffer
               char *temp = Z_Strdupa((const char *)input_buffer);
               psnprintf((char *)input_buffer, sizeof(input_buffer), "\"%s\"", temp);
            }
            else
            {
               input_buffer[0] = '*';
               input_buffer[1] = '\0';
            }

            // set the command
            Console.cmdtype = input_cmdtype;
            C_RunCommand(input_command, (const char *)input_buffer);
            input_command = NULL;
            input_cmdtype = c_typed;
            return true; // eat key
         }
      }
      // check for backspace
      else if(ev->data1 == KEYD_BACKSPACE && input_buffer[0])
      {
         input_buffer[strlen((char *)input_buffer)-1] = '\0';
         return true; // eatkey
      }
      
      // probably just a normal character
      
      // only care about valid characters
      // dont allow too many characters on one command line

      if(ev->character)
         ch = (unsigned char)(ev->character);
      else if(ev->data1 > 31 && ev->data1 < 127)
         ch = shiftdown ? shiftxform[ev->data1] : ev->data1; // shifted?

      if(ch > 31 && ch < 127)
      {
         if(strlen((char *)input_buffer) <=
            ((var->type == vt_string) ? (unsigned int)var->max :
             (var->type == vt_int || var->type == vt_toggle) ? 33 : 20))
         {
            input_buffer[strlen((char *)input_buffer) + 1] = 0;
            input_buffer[strlen((char *)input_buffer)] = ch;
         }
      }
      
      return true;
   } 

   if(devparm && ev->data1 == key_help)
   {
      G_ScreenShot();
      return true;
   }
  
   if(action_menu_toggle)
   {
      // toggle menu
      action_menu_toggle = false;

      // start up main menu or kill menu
      if(menuactive)
      {
         if(menu_toggleisback) // haleyjd 02/12/06: allow zdoom-style navigation
            MN_PrevMenu();
         else
         {
            MN_ClearMenus();
            S_StartSound(NULL, menuSounds[MN_SND_DEACTIVATE]);
         }
      }
      else 
         MN_StartControlPanel();      
      
      return true;
   }

   if(action_menu_help)
   {
      action_menu_help = false;
      C_RunTextCmd("help");
      return true;
   }

   if(action_menu_setup)
   {
      action_menu_setup = false;
      C_RunTextCmd("mn_options");
      return true;
   }
   
   // not interested in other keys if not in menu
   if(!menuactive)
      return false;

   if(action_menu_up)
   {
      boolean cancelsnd = false;
      action_menu_up = false;
      
      // skip gaps
      do
      {
         if(--current_menu->selected < 0)
         {
            int i;
            
            if(!ctrldown) // 3/13/09: control cancels these behaviors
            {
               // haleyjd: in paged menus, go to the previous page
               if(current_menu->prevpage)
               {
                  // set selected item to the first selectable item
                  current_menu->selected = MN_FindFirstSelectable(current_menu);
                  cancelsnd = true; // paging makes a sound already, so remember this
                  MN_PageMenu(current_menu->prevpage); // modifies current_menu!
               }
               else if(current_menu->nextpage)
               {
                  // 03/13/09: don't do normal wrap behavior on first page
                  current_menu->selected = MN_FindFirstSelectable(current_menu);
                  cancelsnd = true; // cancel the sound cuz we didn't do anything
                  break; // exit the loop
               }               
            }
            
            // jump to end of menu
            for(i = 0; current_menu->menuitems[i].type != it_end; ++i)
               ; /* do-nothing loop */
            current_menu->selected = i-1;
         }
      }
      while(is_a_gap(&current_menu->menuitems[current_menu->selected]));
      
      if(!cancelsnd)
         S_StartSound(NULL, menuSounds[MN_SND_KEYUPDOWN]); // make sound
      
      return true;  // eatkey
   }
  
   if(action_menu_down)
   {
      boolean cancelsnd = false;
      action_menu_down = false;
      
      do
      {
         ++current_menu->selected;
         if(current_menu->menuitems[current_menu->selected].type == it_end)
         {
            if(!ctrldown) // 03/13/09: control cancels these behaviors
            {
               // haleyjd: in paged menus, go to the next page               
               if(current_menu->nextpage)
               {
                  // set selected item to the last selectable item
                  current_menu->selected = MN_FindLastSelectable(current_menu);
                  cancelsnd = true;             // don't make a double sound below
                  MN_PageMenu(current_menu->nextpage); // modifies current_menu!
               }
               else if(current_menu->prevpage)
               {
                  // don't wrap the pointer when on the last page
                  current_menu->selected = MN_FindLastSelectable(current_menu);
                  cancelsnd = true; // cancel the sound cuz we didn't do anything
                  break; // exit the loop
               }
            }
            
            current_menu->selected = 0;     // jump back to start
         }
      }
      while(is_a_gap(&current_menu->menuitems[current_menu->selected]));
      
      if(!cancelsnd)
         S_StartSound(NULL, menuSounds[MN_SND_KEYUPDOWN]); // make sound

      return true;  // eatkey
   }
   
   if(action_menu_confirm)
   {
      menuitem_t *menuitem = &current_menu->menuitems[current_menu->selected];

      action_menu_confirm = false;
      
      switch(menuitem->type)
      {
      case it_runcmd:
         S_StartSound(NULL, menuSounds[MN_SND_COMMAND]); // make sound
         Console.cmdtype = c_menu;
         C_RunTextCmd(menuitem->data);
         break;
         
      case it_toggle:
         // boolean values only toggled on enter
         if((menuitem->var->type != vt_toggle && menuitem->var->type != vt_int) ||
            menuitem->var->max - menuitem->var->min > 1) 
            break;
         
         // toggle value now
         psnprintf((char *)tempstr, sizeof(tempstr), "%s /", menuitem->data);
         C_RunTextCmd((char *)tempstr);
         
         S_StartSound(NULL, menuSounds[MN_SND_COMMAND]); // make sound
         break;
         
      case it_variable:
      case it_variable_nd:
         // get input for new value
         input_command = C_GetCmdForName(menuitem->data);
         
         // haleyjd 07/23/04: restore starting input_buffer with
         // the current value of string variables
         if(input_command->variable->type == vt_string)
         {
            char *str = *(char **)(input_command->variable->variable);
            
            if(current_menu != GameModeInfo->saveMenu || 
               strcmp(str, DEH_String("EMPTYSTRING")))
               strcpy((char *)input_buffer, str);
            else
               input_buffer[0] = 0;
         }
         else
            input_buffer[0] = 0;             // clear input buffer

         // haleyjd 07/15/09: set input_cmdtype for it_variable_nd:
         //   default value will not be set by console for type == c_menu.
         if(menuitem->type == it_variable_nd)
            input_cmdtype = c_menu;
         else
            input_cmdtype = c_typed;

         break;

      case it_automap:
         MN_SelectColour(menuitem->data);
         break;

      case it_binding:
         G_EditBinding(menuitem->data);
         break;
         
      default:
         break;
      }

      return true;
   }
  
   if(action_menu_previous)
   {
      action_menu_previous = false;
      MN_PrevMenu();
      return true;
   }
   
   // decrease value of variable
   if(action_menu_left)
   {
      menuitem_t *menuitem;
      action_menu_left = false;
      
      // haleyjd 10/07/05: if ctrl is down, go to previous menu
      if(ctrldown)
      {
         if(current_menu->prevpage)
            MN_PageMenu(current_menu->prevpage);
         else
            S_StartSound(NULL, GameModeInfo->c_BellSound);
         
         return true;
      }
      
      menuitem = &current_menu->menuitems[current_menu->selected];
      
      switch(menuitem->type)
      {
      case it_slider:
      case it_bigslider: // haleyjd 08/31/06: old-school big slider
      case it_toggle:
         // no on-off int values
         if(menuitem->var->type == vt_int || menuitem->var->type == vt_toggle)
         {
            if(menuitem->var->max - menuitem->var->min == 1) 
               break;

            // change variable
            psnprintf((char *)tempstr, sizeof(tempstr), "%s -", menuitem->data);
         }
         else if(menuitem->var->type == vt_float)
         {
            double range = menuitem->var->dmax - menuitem->var->dmin;
            double value = *(double *)(menuitem->var->variable);

            if(altdown)
               value -= 0.1;
            else if(shiftdown)
               value -= 0.01;
            else
               value -= range / 10.0;

            if(value < menuitem->var->dmin)
               value = menuitem->var->dmin;
            else if(value > menuitem->var->dmax)
               value = menuitem->var->dmax;

            psnprintf((char *)tempstr, sizeof(tempstr), "%s \"%.2f\"", 
                      menuitem->data, value);
         }
         C_RunTextCmd((char *)tempstr);
         S_StartSound(NULL, menuSounds[MN_SND_KEYLEFTRIGHT]);
         break;
      default:
         break;
      }
      return true;
   }
  
   // increase value of variable
   if(action_menu_right)
   {
      menuitem_t *menuitem;
      action_menu_right = false;
      
      // haleyjd 10/07/05: if ctrl is down, go to next menu
      if(ctrldown)
      {
         if(current_menu->nextpage)
            MN_PageMenu(current_menu->nextpage);
         else
            S_StartSound(NULL, GameModeInfo->c_BellSound);
         
         return true;
      }
      
      menuitem = &current_menu->menuitems[current_menu->selected];
      
      switch(menuitem->type)
      {
      case it_slider:
      case it_bigslider: // haleyjd 08/31/06: old-school big slider
      case it_toggle:
         // no on-off int values
         if(menuitem->var->type == vt_int || menuitem->var->type == vt_toggle)
         {
            if(menuitem->var->max - menuitem->var->min == 1) 
               break;
         
            // change variable
            psnprintf((char *)tempstr, sizeof(tempstr), "%s +", menuitem->data);
         }
         else if(menuitem->var->type == vt_float)
         {
            double range = menuitem->var->dmax - menuitem->var->dmin;
            double value = *(double *)(menuitem->var->variable);

            if(altdown)
               value += 0.1;
            else if(shiftdown)
               value += 0.01;
            else
               value += range / 10.0;

            if(value < menuitem->var->dmin)
               value = menuitem->var->dmin;
            else if(value > menuitem->var->dmax)
               value = menuitem->var->dmax;

            psnprintf((char *)tempstr, sizeof(tempstr), "%s \"%.2f\"", 
                      menuitem->data, value);
         }
         C_RunTextCmd((char *)tempstr);
         S_StartSound(NULL, menuSounds[MN_SND_KEYLEFTRIGHT]);
         break;
         
      default:
         break;
      }
      
      return true;
   }

   // haleyjd 10/07/05: page up action -- return to previous page
   if(action_menu_pageup)
   {
      action_menu_pageup = false;

      if(current_menu->prevpage)
         MN_PageMenu(current_menu->prevpage);
      else
         S_StartSound(NULL, GameModeInfo->c_BellSound);
      
      return true;
   }

   // haleyjd 10/07/05: page down action -- go to next page
   if(action_menu_pagedown)
   {
      action_menu_pagedown = false;

      if(current_menu->nextpage)
         MN_PageMenu(current_menu->nextpage);
      else
         S_StartSound(NULL, GameModeInfo->c_BellSound);

      return true;
   }

   // haleyjd 10/15/05: table of contents widget!
   if(action_menu_contents)
   {
      action_menu_contents = false;

      if(current_menu->content_names && current_menu->content_pages)
         MN_ShowContents();
      else
         S_StartSound(NULL, GameModeInfo->c_BellSound);

      return true;
   }

   // search for matching item in menu

   if(ev->character)
      ch = tolower((unsigned char)(ev->character));
   else
      ch = tolower(ev->data1);
   
   if(ch >= 'a' && ch <= 'z')
   {  
      // sf: experimented with various algorithms for this
      //     this one seems to work as it should

      int n = current_menu->selected;
      
      do
      {
         n++;
         if(current_menu->menuitems[n].type == it_end) 
            n = 0; // loop round

         // ignore unselectables
         if(!is_a_gap(&current_menu->menuitems[n])) 
         {
            if(tolower(current_menu->menuitems[n].description[0]) == ch)
            {
               // found a matching item!
               current_menu->selected = n;
               return true; // eat key
            }
         }
      } while(n != current_menu->selected);
   }
   
   return false; //!!current_menu;
}

///////////////////////////////////////////////////////////////////////////
//
// Other Menu Functions
//

//
// MN_ActivateMenu
//
// make menu 'clunk' sound on opening
//
void MN_ActivateMenu(void)
{
   if(!menuactive)  // activate menu if not already
   {
      menuactive = true;
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_ACTIVATE]);
   }
}

//
// MN_StartMenu
//
// Start a menu
//
void MN_StartMenu(menu_t *menu)
{
   if(!menuactive)
   {
      MN_ActivateMenu();
      current_menu = menu;      
      menu_history_num = 0;  // reset history
   }
   else
   {
      menu_history[menu_history_num++] = current_menu;
      current_menu = menu;
   }

   // haleyjd 10/02/06: if the menu being activated has a curpage set, 
   // redirect to that page now; this allows us to return to whatever
   // page the user was last viewing.
   if(current_menu->curpage)
      current_menu = current_menu->curpage;
   
   menu_error_time = 0;      // clear error message
   redrawsbar = redrawborder = true;  // need redraw

   // haleyjd 11/12/09: custom menu open actions
   if(current_menu->open)
      current_menu->open();
}

//
// MN_PageMenu
//
// haleyjd 10/07/05: sets the menu engine to a new menu page.
//
static void MN_PageMenu(menu_t *newpage)
{
   if(!menuactive)
      return;

   current_menu = newpage;

   // haleyjd 10/02/06: if this page has a rootpage, set the rootpage's
   // curpage value to this page, in order to remember what page we're on.
   if(current_menu->rootpage)
      current_menu->rootpage->curpage = current_menu;

   menu_error_time = 0;
   redrawsbar = redrawborder = true;

   S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
}

//
// MN_PrevMenu
//
// go back to a previous menu
//
void MN_PrevMenu(void)
{
   if(--menu_history_num < 0)
      MN_ClearMenus();
   else
      current_menu = menu_history[menu_history_num];
   
   menu_error_time = 0;          // clear errors
   redrawsbar = redrawborder = true;  // need redraw
   S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_PREVIOUS]);
}

//
// MN_ClearMenus
//
// turn off menus
//
void MN_ClearMenus(void)
{
   Console.enabled = true; // haleyjd 03/11/06: re-enable console
   menuactive = false;
   redrawsbar = redrawborder = true;  // need redraw
}

CONSOLE_COMMAND(mn_clearmenus, 0)
{
   MN_ClearMenus();
}

CONSOLE_COMMAND(mn_prevmenu, 0)
{
   MN_PrevMenu();
}

CONSOLE_COMMAND(forceload, cf_hidden)
{
   G_ForcedLoadGame();
   MN_ClearMenus();
}

void MN_ForcedLoadGame(char *msg)
{
   MN_Question(msg, "forceload");
}

//
// MN_ErrorMsg
//
// display error msg in popup display at bottom of screen
//
void MN_ErrorMsg(const char *s, ...)
{
   va_list args;
   
   va_start(args, s);
   pvsnprintf(menu_error_message, sizeof(menu_error_message), s, args);
   va_end(args);
   
   menu_error_time = 140;
}

//
// MN_StartControlPanel
//
// activate main menu
//
void MN_StartControlPanel(void)
{
   // haleyjd 08/31/06: not if menu is already active
   if(menuactive)
      return;

   // haleyjd 05/16/04: traditional DOOM main menu support
   // haleyjd 08/31/06: support for all of DOOM's original menus
   if(GameModeInfo->flags & GIF_CLASSICMENUS && 
      (traditional_menu || mn_classic_menus))
   {
      MN_StartMenu(&menu_old_main);
   }
   else
      MN_StartMenu(GameModeInfo->mainMenu);
}

//=============================================================================
//
// Menu Font Drawing
//
// copy of V_* functions
// these do not leave a 1 pixel-gap between chars, I think it looks
// better for the menu
//
// haleyjd: duplicate code eliminated via use of vfont engine.
//

// haleyjd 02/25/09: font names set by EDF:
const char *mn_fontname;
const char *mn_normalfontname;
const char *mn_bigfontname;

//
// MN_InitFonts
//
// Called at startup from MN_Init. Sets global references to the menu fonts
// defined via EDF.
//
static void MN_InitFonts(void)
{
   if(!(menu_font = E_FontForName(mn_fontname)))
      I_Error("MN_InitFonts: bad EDF font name %s\n", mn_fontname);

   if(!(menu_font_big = E_FontForName(mn_bigfontname)))
      I_Error("MN_InitFonts: bad EDF font name %s\n", mn_bigfontname);

   if(!(menu_font_normal = E_FontForName(mn_normalfontname)))
      I_Error("MN_InitFonts: bad EDF font name %s\n", mn_normalfontname);
}

//
// MN_WriteText
//
// haleyjd: This is now just a thunk for convenience's sake.
//
void MN_WriteText(const char *s, int x, int y)
{
   V_FontWriteText(menu_font, s, x, y);
}

//
// MN_WriteTextColored
//
// haleyjd: Ditto.
//   05/02/10: finally renamed from MN_WriteTextColoured. Sorry fraggle ;)
//
void MN_WriteTextColored(const char *s, int colour, int x, int y)
{
   V_FontWriteTextColored(menu_font, s, colour, x, y);
}

//
// MN_StringWidth
//
// haleyjd: And this too.
//
int MN_StringWidth(const char *s)
{
   return V_FontStringWidth(menu_font, s);
}

//=============================================================================
// 
// Box Widget
//
// haleyjd 10/15/05: Generalized code for box widgets.
//

//
// box_widget_t
//
// "Inherits" from menuwidget_t and extends it to contain a list of commands
// or menu pages.
//
typedef struct box_widget_s
{
   menuwidget_t widget;      // the actual menu widget object

   const char *title;        // title string

   const char **item_names;  // item strings for box

   int        type;          // type: either contents or cmds

   menu_t     **pages;       // menu pages array for contents box
   const char **cmds;        // commands for command box

   int        width;         // precalculated width
   int        height;        // precalculated height
   int        selection_idx; // currently selected item index
   int        maxidx;        // precalculated maximum index value
} box_widget_t;

//
// MN_BoxSetDimensions
//
// Calculates the box width, height, and maxidx by considering all
// strings contained in the box.
//
static void MN_BoxSetDimensions(box_widget_t *box)
{
   int width, i = 0;
   const char *curname = box->item_names[0];

   box->width  = -1;
   box->height =  0;

   while(curname)
   {
      width = MN_StringWidth(curname);

      if(width > box->width)
         box->width = width;

      // add string height + 1 for every string in box
      box->height += V_FontStringHeight(menu_font_normal, curname) + 1;

      curname = box->item_names[++i];
   }

   // set maxidx
   box->maxidx = i - 1;

   // account for title
   width = MN_StringWidth(box->title);
   if(width > box->width)
      box->width = width;

   // leave a gap after title
   box->height += V_FontStringHeight(menu_font_normal, box->title) + 8;

   // add 9 to width and 8 to height to account for box border
   box->width  += 9;
   box->height += 8;

   // 04/22/06: add space for a small menu ptr
   box->width += smallptr_dims[0] + 2;
}

//
// MN_BoxWidgetDrawer
//
// Draw a menu box widget.
//
static void MN_BoxWidgetDrawer(void)
{
   int x, y, i = 0;
   const char *curname;
   box_widget_t *box;

   // get a pointer to the box_widget
   box = (box_widget_t *)current_menuwidget;

   // draw the menu in the background
   MN_DrawMenu(current_menu);
   
   x = (SCREENWIDTH  - box->width ) >> 1;
   y = (SCREENHEIGHT - box->height) >> 1;

   // draw box background
   V_DrawBox(x, y, box->width, box->height);

   curname = box->item_names[0];

   // step out from borders (room was left in width, height calculations)
   x += 4 + smallptr_dims[0] + 2;
   y += 4;

   // write title
   MN_WriteTextColored(box->title, CR_GOLD, x, y);

   // leave a gap
   y += V_FontStringHeight(menu_font_normal, box->title) + 8;

   // write text in box
   while(curname)
   {
      int color = GameModeInfo->unselectColor;
      int height = V_FontStringHeight(menu_font_normal, curname);
      
      if(box->selection_idx == i)
      {
         color = GameModeInfo->selectColor;

         // draw small pointer
         MN_DrawSmallPtr(x - (smallptr_dims[0] + 1), 
                         y + ((height - smallptr_dims[1]) >> 1));
      }

      MN_WriteTextColored(curname, color, x, y);

      y += height + 1;
      
      curname = box->item_names[++i];
   }
}

//
// MN_BoxWidgetResponder
//
// Handle events to a menu box widget.
//
static boolean MN_BoxWidgetResponder(event_t *ev)
{
   // get a pointer to the box widget
   box_widget_t *box = (box_widget_t *)current_menuwidget;

   // toggle and previous dismiss the widget
   if(action_menu_toggle || action_menu_previous)
   {
      action_menu_toggle = action_menu_previous = false;
      current_menuwidget = NULL;
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_DEACTIVATE]); // cha!
      return true;
   }

   // up/left: move selection to previous item with wrap
   if(action_menu_up || action_menu_left)
   {
      action_menu_up = action_menu_left = false;
      if(--box->selection_idx < 0)
         box->selection_idx = box->maxidx;
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
      return true;
   }

   // down/right: move selection to next item with wrap
   if(action_menu_down || action_menu_right)
   {
      action_menu_down = action_menu_right = false;
      if(++box->selection_idx > box->maxidx)
         box->selection_idx = 0;
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
      return true;
   }

   // confirm: clear widget and set menu page or run command
   if(action_menu_confirm)
   {
      action_menu_confirm = false;
      current_menuwidget = NULL;

      switch(box->type)
      {
      case 0: // menu page widget
         if(box->pages)
            MN_PageMenu(box->pages[box->selection_idx]);
         break;
      case 1: // command widget
         if(box->cmds)
            C_RunTextCmd(box->cmds[box->selection_idx]);
         break;
      default:
         break;
      }
      
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_COMMAND]); // pow!
      return true;
   }

   return false;
}

//
// Global box widget object. There can only be one box widget active at a
// time, unless the type was to be made global and initialized via a different
// routine. There's not any need for that currently.
//
static box_widget_t menu_box_widget =
{
   // widget:
   {
      MN_BoxWidgetDrawer,
      MN_BoxWidgetResponder,
      NULL,
      true // is fullscreen, draws current menu in background
   },

   // all other fields are set dynamically
};

//
// MN_SetupBoxWidget
//
// Sets up the menu_box_widget object above to show specific information.
//
void MN_SetupBoxWidget(const char *title, const char **item_names,
                       int type, menu_t **pages, const char **cmds)
{
   menu_box_widget.title = title;
   menu_box_widget.item_names = item_names;
   menu_box_widget.type = type;
   
   switch(type)
   {
   case 0: // menu page widget
      menu_box_widget.pages = pages;
      menu_box_widget.cmds  = NULL;
      break;
   case 1: // command widget
      menu_box_widget.pages = NULL;
      menu_box_widget.cmds  = cmds;
      break;
   default:
      break;
   }

   menu_box_widget.selection_idx = 0;

   MN_BoxSetDimensions(&menu_box_widget);
}

//
// MN_ShowBoxWidget
//
// For external access.
//
void MN_ShowBoxWidget(void)
{
   current_menuwidget = &(menu_box_widget.widget);
}

//
// MN_ShowContents
//
// Sets up the menu box widget to display the contents box for the
// current menu. Code in MN_Responder already verifies that the menu
// defines table of contents information before calling this function.
// This allows ease of access to all pages in a multipage menu.
//
static void MN_ShowContents(void)
{
   int i = 0;
   menu_t *rover;

   if(!menuactive)
      return;

   MN_SetupBoxWidget("contents", current_menu->content_names, 0,
                     current_menu->content_pages, NULL);

   // haleyjd 05/02/10: try to find the current menu in the list of pages
   // (it should be there unless something really screwy is going on).

   rover = current_menu->content_pages[0];

   while(rover)
   {
      if(rover == current_menu)
         break; // found it

      // try next one
      rover = current_menu->content_pages[++i];
   }

   if(rover) // only if valid (should always be...)
      menu_box_widget.selection_idx = i;

   current_menuwidget = &(menu_box_widget.widget);

   S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
}


//=============================================================================
//
// Console Commands
//

VARIABLE_TOGGLE(menu_toggleisback, NULL, yesno);
CONSOLE_VARIABLE(mn_toggleisback, menu_toggleisback, 0) {}

VARIABLE_STRING(mn_background, NULL, 8);
CONSOLE_VARIABLE(mn_background, mn_background, 0)
{
   MN_SetBackground();
}

extern void MN_AddMenus(void);              // mn_menus.c
extern void MN_AddMiscCommands(void);       // mn_misc.c

void MN_AddCommands(void)
{
   C_AddCommand(mn_clearmenus);
   C_AddCommand(mn_prevmenu);
   C_AddCommand(forceload);
   C_AddCommand(mn_toggleisback);
   C_AddCommand(mn_background);
   
   MN_AddMenus();               // add commands to call the menus
   MN_AddMiscCommands();
   MN_AddDynaMenuCommands();    // haleyjd 03/13/06
}

// EOF
