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
//----------------------------------------------------------------------------
//
// Menu Item Classes
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomstat.h"
#include "g_bind.h"
#include "m_compare.h"
#include "m_qstr.h"
#include "mn_engin.h"
#include "mn_items.h"
#include "mn_misc.h"
#include "r_patch.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"

// Defines

// gap from variable description to value
#define MENU_GAP_SIZE 20

//=============================================================================
//
// MenuItem Base Class
//

// RTTI proxy for MenuItem base class
IMPLEMENT_RTTI_TYPE(MenuItem)

//
// MenuItem::drawPatchForItem
//
// haleyjd 05/01/10: Draws an item's patch if appropriate. Returns true if
// MN_DrawMenuItem should return. item_height may be modified on return.
// 
bool MenuItem::drawPatchForItem(menuitem_t *item, int &item_height, int alignment)
{
   int lumpnum;
   int x = item->x;
   int y = item->y;

   // default to text-based message if patch missing
   if((lumpnum = W_CheckNumForName(item->patch)) >= 0)
   {
      patch_t *patch = PatchLoader::CacheNum(wGlobalDir, lumpnum, PU_CACHE);
      int16_t  width = patch->width;
      item_height = patch->height + 1;

      // check for left-aligned
      if(alignment != ALIGNMENT_LEFT)
         x -= width;

      // possibly allow sub-classed items to adjust their x coordinate here
      x = adjustPatchXCoord(x, width);

      // haleyjd 06/27/11: don't translate patches, due to weird CR lumps from BOOM :/
      V_DrawPatch(x, y, &subscreen43, patch);
      return true;
   }

   // no patch was drawn, so do normal item drawing in MN_DrawMenuItem
   return false;
}

//
// MenuItem::drawDescription
//
// Draws a generic menuitem description.
// May modify item_height and item_width
//
void MenuItem::drawDescription(menuitem_t *item, int &item_height, 
                               int &item_width, int alignment, int color)
{
   int x, y;
   
   item_width = 
      (item->flags & MENUITEM_BIGFONT) ?
         V_FontStringWidth(menu_font_big, item->description) : 
         MN_StringWidth(item->description);
      
   if(item->flags & MENUITEM_CENTERED || alignment == ALIGNMENT_CENTER)
      x = (SCREENWIDTH - item_width) / 2;
   else if(item->flags & MENUITEM_LALIGNED)
      x = 12;
   else
      x = item->x - (alignment == ALIGNMENT_LEFT ? 0 : item_width);

   y = item->y;

   // write description
   if(shouldDrawDescription(item))
   {
      if(item->flags & MENUITEM_BIGFONT)
      {
         V_FontWriteTextColored(menu_font_big, item->description, 
                                GameModeInfo->bigFontItemColor, x, y, 
                                &subscreen43);
         item_height = V_FontStringHeight(menu_font_big, item->description);
      }
      else
         MN_WriteTextColored(item->description, color, x, y);
   }

   // haleyjd 02/04/06: set coordinates for small pointers
   // left pointer:
   MN_SetLeftSmallPtr(x, y, item_height);

   // right pointer:
   MN_SetRightSmallPtr(x, y, item_width, item_height);
}

//=============================================================================
//
// Utilities
//

//
// MN_truncateInput
//
// haleyjd 10/18/10: Avoid losing sight of the input caret when editing.
//
static void MN_truncateInput(qstring &qstr, int x)
{
   int width = MN_StringWidth(qstr.constPtr());

   if(x + width > SCREENWIDTH - 8) // too wide to fit?
   {
      int subbed_width = 0;
      int leftbound = SCREENWIDTH - 8;
      int dotwidth  = MN_StringWidth("...");
      const char *start = qstr.constPtr();
      const char *end   = qstr.bufferAt(qstr.length() - 1);

      while(start != end && (x + dotwidth + width - subbed_width > leftbound))
      {
         subbed_width += V_FontCharWidth(menu_font, *start);
         ++start;
      }
      
      if(start != end)
      {
         const char *temp = Z_Strdupa(start); // make a temp copy
         qstr = "...";
         qstr += temp;
      }
   }
}

//
// As MN_truncateValue but it purely truncates, no dots
//
static void MN_truncateValueNoDots(qstring &qstr, int x)
{
   const int width = MN_StringWidth(qstr.constPtr());

   if(x + width > SCREENWIDTH) // too wide to fit?
   {
      const char *start = qstr.constPtr();
      const char *end   = qstr.bufferAt(qstr.length());

      int subbed_width = 0;
      while(end != start && (x + width - subbed_width > SCREENWIDTH))
      {
         subbed_width += V_FontCharWidth(menu_font, *end);
         --end;
      }

      // truncate the value at end position, and concatenate dots
      if(end != start)
         qstr.truncate(end - start);
   }
}

//
// haleyjd 10/18/10: Avoid long values going off screen, as it is unsightly
//
static void MN_truncateValue(qstring &qstr, int x)
{
   int width = MN_StringWidth(qstr.constPtr());

   if(x + width > SCREENWIDTH) // too wide to fit?
   {
      const char *start     = qstr.constPtr();
      const char *end       = qstr.bufferAt(qstr.length());
      const int   leftbound = SCREENWIDTH - MN_StringWidth("...");

      int subbed_width = 0;
      while(end != start && (x + width - subbed_width > leftbound))
      {
         subbed_width += V_FontCharWidth(menu_font, *end);
         --end;
      }

      // truncate the value at end position, and concatenate dots
      if(end != start)
      {
         qstr.truncate(end - start);
         qstr += "...";
      }
   }
}

//
// Scroll a value that goes of the screen but we want to see all of
//
void MN_scrollValue(qstring &qstr, const int x)
{
   const int valueWidth  = MN_StringWidth(qstr.constPtr());
   const int textOverrun = emax(int(ceilf(float((x + valueWidth) - SCREENWIDTH) / menu_font->cw)), 0);
   if(textOverrun)
   {
      int textOffset = emax((gametic - (mn_lastSelectTic + TICRATE)) / 12, 0);
      if(textOffset > textOverrun)
      {
         textOffset = textOverrun;
         if(gametic - mn_lastScrollTic > TICRATE * 2)
         {
            mn_lastSelectTic = gametic;
            mn_lastScrollTic = gametic;
         }

      }
      else
         mn_lastScrollTic = gametic;

      if(textOffset)
         qstr.erase(0, textOffset);
   }

   MN_truncateValueNoDots(qstr, x);
}

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

// width of slider, in mid-patches
#define SLIDE_PATCHES 9

//
// MN_drawSlider
//
// draw a 'slider' (for sound volume, etc)
//
static int MN_drawSlider(int x, int y, int pct)
{
   int draw_x = x;
   int slider_width = 0;       // find slider width in pixels
   int16_t wl, wm, ws, hs;

   // load slider gfx
   slider_gfx[slider_left  ] = PatchLoader::CacheName(wGlobalDir, "M_SLIDEL", PU_STATIC);
   slider_gfx[slider_right ] = PatchLoader::CacheName(wGlobalDir, "M_SLIDER", PU_STATIC);
   slider_gfx[slider_mid   ] = PatchLoader::CacheName(wGlobalDir, "M_SLIDEM", PU_STATIC);
   slider_gfx[slider_slider] = PatchLoader::CacheName(wGlobalDir, "M_SLIDEO", PU_STATIC);

   wl = slider_gfx[slider_left  ]->width;
   wm = slider_gfx[slider_mid   ]->width;
   ws = slider_gfx[slider_slider]->width;
   hs = slider_gfx[slider_slider]->height;

   // haleyjd 04/09/2010: offset y relative to menu font
   if(menu_font->absh > hs + 1)
   {
      int yamt = menu_font->absh - hs;

      // tend toward the higher pixel when amount is odd
      if(yamt % 2)
         ++yamt;

      y += yamt / 2;
   }
  
   V_DrawPatch(draw_x, y, &subscreen43, slider_gfx[slider_left]);
   draw_x += wl;
  
   for(int i = 0; i < SLIDE_PATCHES; i++)
   {
      V_DrawPatch(draw_x, y, &subscreen43, slider_gfx[slider_mid]);
      draw_x += wm - 1;
   }
   
   V_DrawPatch(draw_x, y, &subscreen43, slider_gfx[slider_right]);
  
   // find position to draw slider patch
   
   slider_width = (wm - 1) * SLIDE_PATCHES;
   draw_x = wl + (pct * (slider_width - ws)) / 100;
   
   V_DrawPatch(x + draw_x, y, &subscreen43, slider_gfx[slider_slider]);

   // haleyjd: set slider gfx purgable
   Z_ChangeTag(slider_gfx[slider_left  ], PU_CACHE);
   Z_ChangeTag(slider_gfx[slider_right ], PU_CACHE);
   Z_ChangeTag(slider_gfx[slider_mid   ], PU_CACHE);
   Z_ChangeTag(slider_gfx[slider_slider], PU_CACHE);

   return x + draw_x /*+ ws / 2*/;
}

//
// MN_drawThermo
//
// haleyjd 08/30/06: Needed for old menu emulation; this draws an old-style
// huge slider.
//
// TODO/FIXME: allow DeHackEd replacement of patch names
//
static void MN_drawThermo(int x, int y, int thermWidth, int thermDot)
{
   int xx, i;

   xx = x;
   V_DrawPatch(xx, y, &subscreen43,
               PatchLoader::CacheName(wGlobalDir, "M_THERML", PU_CACHE));
   
   xx += 8;
   
   for(i = 0; i < thermWidth; i++)
   {
      V_DrawPatch(xx, y, &subscreen43,
                  PatchLoader::CacheName(wGlobalDir, "M_THERMM", PU_CACHE));
      xx += 8;
   }
   
   V_DrawPatch(xx, y, &subscreen43,
               PatchLoader::CacheName(wGlobalDir, "M_THERMR", PU_CACHE));
   
   V_DrawPatch((x + 8) + thermDot*8, y, &subscreen43,
               PatchLoader::CacheName(wGlobalDir, "M_THERMO", PU_CACHE));
}

//=============================================================================
//
// Gap
//
// it_gap is a blank space in the menu; it is unselectable, has no graphics,
// and has no real special behaviors.
//

class MenuItemGap : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemGap, MenuItem)
};

IMPLEMENT_RTTI_TYPE(MenuItemGap)

//=============================================================================
//
// Title
//
// Serves as the title of a menu, at the top
//

class MenuItemTitle : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemTitle, MenuItem)

protected:
   virtual int adjustPatchXCoord(int x, int16_t width) override
   {
      // adjust x to center title
      return (SCREENWIDTH - width)/2;
   }

   virtual void drawDescription(menuitem_t *item, int &item_height, 
                                int &item_width, int alignment, int color) override
   {
      // draw the description centered
      const char *text = item->description;

      edefstructvar(vtextdraw_t, vdt);
      vdt.font        = menu_font_big;
      vdt.s           = text;
      vdt.x           = (SCREENWIDTH - V_FontStringWidth(menu_font_big, text)) / 2;
      vdt.y           = item->y;
      vdt.screen      = &subscreen43;
      vdt.flags       = VTXT_FIXEDCOLOR;
      vdt.fixedColNum = GameModeInfo->titleColor;
      
      if(!(drawing_menu->flags & mf_skullmenu) && GameModeInfo->flags & GIF_SHADOWTITLES)
         vdt.flags |= VTXT_SHADOW;

      V_FontWriteTextEx(vdt);

      item_height = V_FontStringHeight(menu_font_big, text);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemTitle)

//=============================================================================
//
// Info
//
// A strictly informational item
//

class MenuItemInfo : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemInfo, MenuItem)
};

IMPLEMENT_RTTI_TYPE(MenuItemInfo)

//=============================================================================
//
// RunCmd
//
// Runs a console command by responding to the confirm action. Typically
// displays a text string as its description.
//

class MenuItemRunCmd : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemRunCmd, MenuItem)

public:
   virtual const char *getHelpString(menuitem_t *item, char *msgbuffer) override
   {
      const char *key = G_FirstBoundKey("menu_confirm");
      psnprintf(msgbuffer, 64, "Press %s to execute", key);
      return msgbuffer;
   }

   virtual void onConfirm(menuitem_t *item) override
   {
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_COMMAND]); // make sound
      Console.cmdtype = c_menu;
      C_RunTextCmd(item->data);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemRunCmd)

//=============================================================================
//
// Valued
//
// Super-class for menu items which display a value of some type to the right
// of their description.
//

class MenuItemValued : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemValued, MenuItem)

public:
   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) override
   {
      qstring varvalue;
      int x = item->x;
      int y = item->y;

      if(drawing_menu->flags & mf_background)
      {
         // include gap on fullscreen menus
         if(item->flags & MENUITEM_LALIGNED)
            x = 8 + drawing_menu->widest_width + 16; // haleyjd: use widest_width
         else
            x += MENU_GAP_SIZE;

         // adjust colour for different coloured variables
         if(color == GameModeInfo->unselectColor)
            color = GameModeInfo->variableColor;
      }

      if(alignment == ALIGNMENT_LEFT)
         x += desc_width;

      // create variable description; use console variable descriptions.
      MN_GetItemVariable(item);

      // display input buffer if inputting new var value
      if(input_command && item->var == input_command->variable)
      {
         varvalue = MN_GetInputBuffer();
         varvalue += '_';
         MN_truncateInput(varvalue, x);
      }
      else
      {
         varvalue = C_VariableStringValue(item->var);
         MN_truncateValue(varvalue, x);
      }

      // draw it
      MN_WriteTextColored(varvalue.constPtr(), color, x, y);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemValued)

//=============================================================================
//
// Variable
//
// Displays and allows directly editing the value of a console variable.
//

class MenuItemVariable : public MenuItemValued
{
   DECLARE_RTTI_TYPE(MenuItemVariable, MenuItemValued)

public:
   virtual const char *getHelpString(menuitem_t *item, char *msgbuffer) override
   {
      if(input_command)
         return "Press escape to cancel";
      else
      {
         const char *key = G_FirstBoundKey("menu_confirm");
         psnprintf(msgbuffer, 64, "Press %s to change", key);
         return msgbuffer;
      }
   }

   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) override
   {
      qstring varvalue;
      int x = item->x;
      int y = item->y;

      if(drawing_menu->flags & mf_background)
      {
         // include gap on fullscreen menus
         if(item->flags & MENUITEM_LALIGNED)
            x = 8 + drawing_menu->widest_width + 16; // haleyjd: use widest_width
         else
            x += MENU_GAP_SIZE;

         // adjust colour for different coloured variables
         if(color == GameModeInfo->unselectColor)
            color = GameModeInfo->variableColor;
      }

      if(alignment == ALIGNMENT_LEFT)
         x += desc_width;

      // create variable description; use console variable descriptions.
      MN_GetItemVariable(item);


      // display input buffer if inputting new var value
      if(input_command && item->var == input_command->variable)
      {
         varvalue = MN_GetInputBuffer();
         varvalue += '_';
         MN_truncateInput(varvalue, x);
      }
      else
      {
         varvalue = C_VariableStringValue(item->var);
         if(selected)
            MN_scrollValue(varvalue, x);
         else
            MN_truncateValue(varvalue, x);
      }

      // draw it
      MN_WriteTextColored(varvalue.constPtr(), color, x, y);
   }

   virtual void onConfirm(menuitem_t *item) override
   {
      qstring &input_buffer = MN_GetInputBuffer();

      // get input for new value
      input_command = C_GetCmdForName(item->data);
      input_buffer.clear();

      // haleyjd 07/23/04: restore starting input_buffer with the current value
      // of string variables      
      if(input_command->variable->type == vt_string)
      {
         char *str = *(char **)(input_command->variable->variable);

         if(current_menu != GameModeInfo->saveMenu ||
            strcmp(str, DEH_String("EMPTYSTRING")))
            input_buffer = str;
      }

      input_cmdtype = c_typed;
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemVariable)

//
// Subclass of variable item which doesn't set the default value of the cvar
// it references, so that changes don't stick, ie., in the config file.
//
class MenuItemVariableND : public MenuItemVariable
{
   DECLARE_RTTI_TYPE(MenuItemVariableND, MenuItemVariable)

public:
   virtual void onConfirm(menuitem_t *item) override
   {
      Super::onConfirm(item);

      // haleyjd 07/15/09: set input_cmdtype for it_variable_nd:
      //  default value will not be set by console for type == c_menu.
      input_cmdtype = c_menu;
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemVariableND)

//=============================================================================
//
// Slidable
//
// Slidable items are valued variables that respond to left and right events.
//

class MenuItemSlidable : public MenuItemValued
{
   DECLARE_RTTI_TYPE(MenuItemSlidable, MenuItemValued)

public:
   virtual const char *getHelpString(menuitem_t *item, char *msgbuffer) override
   {
      return "Use left/right to change value";
   }

   virtual void onLeft(menuitem_t *item, bool altdown, bool shiftdown) override
   {
      qstring tempstr(1024);

      // no on-off int values
      if(item->var->type == vt_int || item->var->type == vt_toggle)
      {
         if(item->var->max - item->var->min == 1)
            return;

         // change variable
         tempstr << item->data << " -";         
      }
      else if(item->var->type == vt_float)
      {
         double range = item->var->dmax - item->var->dmin;
         double value = *(double *)(item->var->variable);

         if(altdown)
            value -= 0.1;
         else if(shiftdown)
            value -= 0.01;
         else
            value -= range / 10.0;

         if(value < item->var->dmin)
            value = item->var->dmin;
         else if(value > item->var->dmax)
            value = item->var->dmax;

         tempstr.Printf(1024, "%s \"%.2f\"", item->data, value);
      }
      else // haleyjd 05/30/11: for anything else, have to assume it's specially coded
         tempstr << item->data << " -";

      C_RunTextCmd(tempstr.constPtr());
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
   }

   virtual void onRight(menuitem_t *item, bool altdown, bool shiftdown) override
   {
      qstring tempstr(1024);

      // no on-off int values
      if(item->var->type == vt_int || item->var->type == vt_toggle)
      {
         if(item->var->max - item->var->min == 1) 
            return;

         // change variable
         tempstr << item->data << " +";
      }
      else if(item->var->type == vt_float)
      {
         double range = item->var->dmax - item->var->dmin;
         double value = *(double *)(item->var->variable);

         if(altdown)
            value += 0.1;
         else if(shiftdown)
            value += 0.01;
         else
            value += range / 10.0;

         if(value < item->var->dmin)
            value = item->var->dmin;
         else if(value > item->var->dmax)
            value = item->var->dmax;

         tempstr.Printf(1024, "%s \"%.2f\"", item->data, value);
      }
      else // haleyjd 05/30/11: for anything else, have to assume it's specially coded
         tempstr << item->data << " +";

      C_RunTextCmd(tempstr.constPtr());
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemSlidable)

//=============================================================================
//
// Toggle
//
// Still displays like a variable, but allows its value to be changed by
// pressing left or right, to toggle through the possible range of values.
//

class MenuItemToggle : public MenuItemSlidable
{
   DECLARE_RTTI_TYPE(MenuItemToggle, MenuItemSlidable)

public:
   virtual const char *getHelpString(menuitem_t *item, char *msgbuffer) override
   {
      // enter to change boolean variables
      // left/right otherwise
      if(item->var->type == vt_toggle ||
         (item->var->type == vt_int && item->var->max - item->var->min == 1))
      {
         const char *key = G_FirstBoundKey("menu_confirm");
         psnprintf(msgbuffer, 64, "Press %s to change", key);
         return msgbuffer;
      }
      else
         return Super::getHelpString(item, msgbuffer);
   }

   virtual void onConfirm(menuitem_t *item) override
   {
      qstring tempstr;

      // boolean values only toggled on enter
      if((item->var->type != vt_toggle && item->var->type != vt_int) ||
         item->var->max - item->var->min > 1)
         return;

      // toggle value now
      tempstr << item->data << " /";
      C_RunTextCmd(tempstr.constPtr());
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_COMMAND]);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemToggle)

//=============================================================================
//
// Slider
//
// A slidable value that displays a small slider bar rather the variable's 
// numeric value.
//

class MenuItemSlider : public MenuItemSlidable
{
   DECLARE_RTTI_TYPE(MenuItemSlider, MenuItemSlidable)

public:
   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) override
   {
      variable_t *var;
      int x = item->x;
      int y = item->y;

      MN_GetItemVariable(item);

      // draw slider -- ints or floats (haleyjd 04/22/10)
      if(!(var = item->var))
         return;

      if(var->type == vt_int || var->type == vt_toggle)
      {
         int range = var->max - var->min;
         int posn;

         if(var->type == vt_int)
            posn = *(int *)var->variable - var->min;
         else
            posn = (int)(*(bool *)var->variable) - var->min;

         MN_drawSlider(x + MENU_GAP_SIZE, y, (posn*100) / range);
      }
      else if(var->type == vt_float)
      {
         double range = var->dmax - var->dmin;
         double posn  = *(double *)var->variable - var->dmin;
         int sliderxpos;

         sliderxpos = MN_drawSlider(x + MENU_GAP_SIZE, y, (int)((posn*100) / range));

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
            V_FontWriteText(menu_font, doublebuf, text_x, text_y, &subscreen43);
         }
      }
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemSlider)

//=============================================================================
//
// Big Slider
//
// This type of slider is a vanilla classic item and only appears when old
// menus are being emulated.
//

class MenuItemBigSlider : public MenuItemSlidable
{
   DECLARE_RTTI_TYPE(MenuItemBigSlider, MenuItemSlidable)

protected:
   // A big slider should only draw its description if it has no patch
   virtual bool shouldDrawDescription(menuitem_t *item) override { return !item->patch; }

public:
   virtual bool drawPatchForItem(menuitem_t *item, int &item_height, int alignment) override
   {
      Super::drawPatchForItem(item, item_height, alignment);

      // Big sliders need to both draw a patch and perform their normal item drawing,
      // so the parent class return value is ignored, and false is returned to cause
      // MN_DrawMenuItem to continue.
      return false;
   }

   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) override
   {
      int x = item->x;
      int y = item->y;

      MN_GetItemVariable(item);

      if(item->var && item->var->type == vt_int)
      {
         // note: the thermometer is drawn in the position of the next menu
         // item, so place a gap underneath it.
         // FIXME/TODO: support on non-emulated menus
         MN_drawThermo(x, y + EMULATED_ITEM_SIZE,
                       item->var->max - item->var->min + 1,
                       *(int *)item->var->variable);
      }
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemBigSlider)

//=============================================================================
//
// Automap Color
//
// An automap color selector is a special item type used on the automap color
// configuration menus.
//

class MenuItemAutomap : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemAutomap, MenuItem)

public:
   virtual void handleIndex0(const int amcolor, const int ix, const int iy)
   {
      // No cross patch for non-optional colours
   }

   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) override
   {
      int ix = item->x;
      int iy = item->y;
      int amcolor;
      byte block[BLOCK_SIZE * BLOCK_SIZE];

      MN_GetItemVariable(item);

      if(!item->var || item->var->type != vt_int)
         return;

      // find colour of this variable from console variable
      amcolor = *(int *)item->var->variable;

      // create block
      // border
      memset(block, GameModeInfo->blackIndex, BLOCK_SIZE * BLOCK_SIZE);

      if(amcolor)
      {
         // middle
         for(int bx = 1; bx < BLOCK_SIZE - 1; bx++)
         {
            for(int by = 1; by < BLOCK_SIZE - 1; by++)
               block[by * BLOCK_SIZE + bx] = (byte)amcolor;
         }
      }

      // draw it
      V_DrawBlock(ix + MENU_GAP_SIZE, iy - 1, &subscreen43, BLOCK_SIZE, BLOCK_SIZE, block);

      handleIndex0(amcolor, ix, iy);
   }

   virtual void onConfirm(menuitem_t *item) override
   {
      MN_SelectColor(item->data, mapColorType_e::mandatory);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemAutomap)

//=============================================================================
//
// Optional Automap Color
//
// As above, but index 0 means that it is not rendered.
//

class MenuItemAutomapOpt : public MenuItemAutomap
{
   DECLARE_RTTI_TYPE(MenuItemAutomapOpt, MenuItemAutomap)

public:
   virtual void handleIndex0(const int amcolor, const int ix, const int iy) override
   {
      // draw patch w/cross
      if(!amcolor)
         V_DrawPatch(ix + MENU_GAP_SIZE + 1, iy, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_PALNO", PU_CACHE));
   }

   virtual void onConfirm(menuitem_t* item) override
   {
      MN_SelectColor(item->data, mapColorType_e::optional);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemAutomapOpt)

//=============================================================================
//
// Key Binding
//
// Displays the keys bound to a particular key action
//

class MenuItemBinding : public MenuItem
{
   DECLARE_RTTI_TYPE(MenuItemBinding, MenuItem)

public:
   virtual const char *getHelpString(menuitem_t *item, char *msgbuffer) override
   {
      extern menuwidget_t binding_widget;

      if(current_menuwidget == &binding_widget)
         return "Press any input to bind/unbind";
      else
      {
         const char *key = G_FirstBoundKey("menu_confirm");
         psnprintf(msgbuffer, 64, "Press %s to start binding/unbinding", key);
         return msgbuffer;
      }
   }

   virtual void drawData(menuitem_t *item, int color, int alignment, int desc_width, bool selected) override
   {
      qstring boundkeys;
      int x = item->x;
      int y = item->y;

      G_BoundKeys(item->data, boundkeys);

      if(drawing_menu->flags & mf_background)
      {
         // include gap on fullscreen menus
         x += MENU_GAP_SIZE;

         // adjust color for different colored variables
         if(color == GameModeInfo->unselectColor)
            color = GameModeInfo->variableColor;
      }

      if(alignment == ALIGNMENT_LEFT)
         x += desc_width;

      if(selected)
         MN_scrollValue(boundkeys, x);
      else
         MN_truncateValue(boundkeys, x);

      // write variable value text
      MN_WriteTextColored(boundkeys.constPtr(), color, x, y);
   }

   virtual void onConfirm(menuitem_t *item) override
   {
      G_EditBinding(item->data);
   }
};

IMPLEMENT_RTTI_TYPE(MenuItemBinding)

//=============================================================================
//
// Static singletons and lookup (class instance for item type)
// For now at least, we just need a single instance of these objects. It might
// be possible to transition later to a model which merges the data from the
// menuitem_t structs into these items at runtime, and instantiate them based
// off the struct arrays, using those as the runtime menus, but, that's neither
// here nor now.
//

static MenuItemGap        mn_gap;
static MenuItemRunCmd     mn_runcmd;
static MenuItemVariable   mn_variable;
static MenuItemVariableND mn_variable_nd;
static MenuItemToggle     mn_toggle;
static MenuItemTitle      mn_title;
static MenuItemInfo       mn_info;
static MenuItemSlider     mn_slider;
static MenuItemBigSlider  mn_bigslider;
static MenuItemAutomap    mn_automap;
static MenuItemAutomapOpt mn_automap_opt;
static MenuItemBinding    mn_binding;
static MenuItem           mn_end;

MenuItem *MenuItemInstanceForType[it_numtypes] =
{
   &mn_gap,         // it_gap
   &mn_runcmd,      // it_runcmd
   &mn_variable,    // it_variable
   &mn_variable_nd, // it_variable_nd
   &mn_toggle,      // it_toggle
   &mn_title,       // it_title
   &mn_info,        // it_info
   &mn_slider,      // it_slider
   &mn_bigslider,   // it_bigslider
   &mn_automap,     // it_automap
   &mn_automap_opt, // it_automap_opt
   &mn_binding,     // it_binding
   &mn_end          // it_end
};

// EOF

