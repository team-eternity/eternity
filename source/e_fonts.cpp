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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: EDF font definitions.
// Authors: James Haley, Ioan Chera, Max Waine
//

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"

#include "z_zone.h"
#include "i_system.h"

#include "autopalette.h"
#include "c_io.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"
#include "e_hash.h"
#include "f_finale.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "in_lude.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "mn_engin.h"
#include "r_draw.h"
#include "v_font.h"
#include "v_image.h"
#include "v_misc.h"
#include "v_patch.h"
#include "v_patchfmt.h"
#include "v_png.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_auto.h"

#include "e_lib.h"
#include "e_edf.h"
#include "e_fonts.h"

//=============================================================================
//
// Data Tables
//

constexpr const char ITEM_FONT_ID[]     = "id";
constexpr const char ITEM_FONT_START[]  = "start";
constexpr const char ITEM_FONT_END[]    = "end";
constexpr const char ITEM_FONT_CY[]     = "linesize";
constexpr const char ITEM_FONT_SPACE[]  = "spacesize";
constexpr const char ITEM_FONT_DW[]     = "widthdelta";
constexpr const char ITEM_FONT_ABSH[]   = "tallestchar";
constexpr const char ITEM_FONT_CW[]     = "centerwidth";
constexpr const char ITEM_FONT_LFMT[]   = "linearformat";
constexpr const char ITEM_FONT_LLUMP[]  = "linearlump";
constexpr const char ITEM_FONT_REQUAN[] = "requantize";
constexpr const char ITEM_FONT_FILTER[] = "filter";
constexpr const char ITEM_FONT_COLOR[]  = "colorable";
constexpr const char ITEM_FONT_UPPER[]  = "uppercase";
constexpr const char ITEM_FONT_CENTER[] = "blockcentered";
constexpr const char ITEM_FONT_USPACE[] = "usespacesize";
constexpr const char ITEM_FONT_POFFS[]  = "patchnumoffset";
constexpr const char ITEM_FONT_COLORD[] = "defaultcolor";
constexpr const char ITEM_FONT_COLORN[] = "normalcolor";
constexpr const char ITEM_FONT_COLORH[] = "highlightcolor";
constexpr const char ITEM_FONT_COLORE[] = "errorcolor";
constexpr const char ITEM_FONT_COLORS[] = "colortables";

constexpr const char ITEM_FILTER_CHARS[] = "chars";
constexpr const char ITEM_FILTER_START[] = "start";
constexpr const char ITEM_FILTER_END[]   = "end";
constexpr const char ITEM_FILTER_MASK[]  = "mask";

constexpr const char ITEM_COLOR_BRICK[]   = "brick";
constexpr const char ITEM_COLOR_TAN[]     = "tan";
constexpr const char ITEM_COLOR_GRAY[]    = "gray";
constexpr const char ITEM_COLOR_GREEN[]   = "green";
constexpr const char ITEM_COLOR_BROWN[]   = "brown";
constexpr const char ITEM_COLOR_GOLD[]    = "gold";
constexpr const char ITEM_COLOR_RED[]     = "red";
constexpr const char ITEM_COLOR_BLUE[]    = "blue";
constexpr const char ITEM_COLOR_ORANGE[]  = "orange";
constexpr const char ITEM_COLOR_YELLOW[]  = "yellow";
constexpr const char ITEM_COLOR_CUSTOM1[] = "custom1";
constexpr const char ITEM_COLOR_CUSTOM2[] = "custom2";
constexpr const char ITEM_COLOR_CUSTOM3[] = "custom3";
constexpr const char ITEM_COLOR_CUSTOM4[] = "custom4";

constexpr const char ITEM_DELTA_NAME[]    = "name";

static cfg_opt_t filter_opts[] =
{
   CFG_STR(ITEM_FILTER_CHARS, nullptr, CFGF_LIST),
   CFG_STR(ITEM_FILTER_START, "",      CFGF_NONE),
   CFG_STR(ITEM_FILTER_END,   "",      CFGF_NONE),
   CFG_STR(ITEM_FILTER_MASK,  "",      CFGF_NONE),
   CFG_END()
};

static const char *fontcolornames[CR_LIMIT] =
{
   ITEM_COLOR_BRICK,
   ITEM_COLOR_TAN,
   ITEM_COLOR_GRAY,
   ITEM_COLOR_GREEN,
   ITEM_COLOR_BROWN,
   ITEM_COLOR_GOLD,
   ITEM_COLOR_RED,
   ITEM_COLOR_BLUE,
   ITEM_COLOR_ORANGE,
   ITEM_COLOR_YELLOW,
   ITEM_COLOR_CUSTOM1,
   ITEM_COLOR_CUSTOM2,
   ITEM_COLOR_CUSTOM3,
   ITEM_COLOR_CUSTOM4
};

static cfg_opt_t color_opts[] =
{
   CFG_STR(ITEM_COLOR_BRICK,   "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_TAN,     "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_GRAY,    "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_GREEN,   "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_BROWN,   "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_GOLD,    "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_RED,     "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_BLUE,    "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_ORANGE,  "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_YELLOW,  "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_CUSTOM1, "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_CUSTOM2, "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_CUSTOM3, "", CFGF_LIST),
   CFG_STR(ITEM_COLOR_CUSTOM4, "", CFGF_LIST),
   CFG_END()
};

#define FONT_FIELDS \
   CFG_INT(ITEM_FONT_ID,     -1,          CFGF_NONE), \
   CFG_STR(ITEM_FONT_START,  "",          CFGF_NONE), \
   CFG_STR(ITEM_FONT_END,    "",          CFGF_NONE), \
   CFG_INT(ITEM_FONT_CY,     0,           CFGF_NONE), \
   CFG_INT(ITEM_FONT_SPACE,  0,           CFGF_NONE), \
   CFG_INT(ITEM_FONT_DW,     0,           CFGF_NONE), \
   CFG_INT(ITEM_FONT_ABSH,   0,           CFGF_NONE), \
   CFG_INT(ITEM_FONT_CW,     0,           CFGF_NONE), \
   CFG_STR(ITEM_FONT_LFMT,   "linear",    CFGF_NONE), \
   CFG_STR(ITEM_FONT_LLUMP,  "",          CFGF_NONE), \
   CFG_INT(ITEM_FONT_POFFS,  0,           CFGF_NONE), \
   CFG_SEC(ITEM_FONT_FILTER, filter_opts, CFGF_MULTI|CFGF_NOCASE), \
   CFG_SEC(ITEM_FONT_COLORS, color_opts,  CFGF_NOCASE), \
   CFG_STR(ITEM_FONT_COLORD, "",          CFGF_NONE), \
   CFG_STR(ITEM_FONT_COLORN, "",          CFGF_NONE), \
   CFG_STR(ITEM_FONT_COLORH, "",          CFGF_NONE), \
   CFG_STR(ITEM_FONT_COLORE, "",          CFGF_NONE), \
                                                      \
   CFG_BOOL(ITEM_FONT_COLOR,  false,      CFGF_NONE), \
   CFG_BOOL(ITEM_FONT_UPPER,  false,      CFGF_NONE), \
   CFG_BOOL(ITEM_FONT_CENTER, false,      CFGF_NONE), \
   CFG_BOOL(ITEM_FONT_USPACE, false,      CFGF_NONE), \
   CFG_BOOL(ITEM_FONT_REQUAN, false,      CFGF_NONE), \
                                                      \
   CFG_END()

cfg_opt_t edf_font_opts[] =
{
   FONT_FIELDS
};

cfg_opt_t edf_fntdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, nullptr, CFGF_NONE),
   FONT_FIELDS
};

// linear font formats
enum
{
   FONT_FMT_LINEAR,
   FONT_FMT_PATCH,
   FONT_FMT_PNG,
   NUM_FONT_FMTS
};

// NOTE: unused
/*
static const char *fontfmts[NUM_FONT_FMTS] =
{
   "linear",  // a linear block of pixel data; the default format.
   "patch",   // a DOOM patch, which will be converted to linear.
   "png"      // a PNG graphic
};
*/

typedef EHashTable<vfont_t, EIntHashKey, 
                   &vfont_t::num, &vfont_t::numlinks> EFontNumHash;
typedef EHashTable<vfont_t, ENCStringHashKey, 
                   &vfont_t::name, &vfont_t::namelinks> EFontNameHash;

static EFontNumHash  e_font_numhash;
static EFontNameHash e_font_namehash;

// Keep a string table needed for reloading linear graphics
static Collection<qstring> e_linearLumpNames;

//=============================================================================
//
// Hashing Routines
//

//
// E_AddFontToNameHash
//
// Puts the vfont_t into the name hash table.
//
static void E_AddFontToNameHash(vfont_t *font)
{
   e_font_namehash.addObject(font);
}

// need forward declaration for E_AutoAllocFontNum
static void E_AddFontToNumHash(vfont_t *font);

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_fontnum = D_MAXINT;

//
// E_AutoAllocFontNum
//
// Automatically assigns an unused font number to a font
// which was not given one by the author, to allow reference by name
// anywhere without the chore of number allocation.
//
static bool E_AutoAllocFontNum(vfont_t *font)
{
   int num;

#ifdef RANGECHECK
   if(font->num > 0)
      I_Error("E_AutoAllocFontNum: called for vfont_t with valid num\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_fontnum < 0)
      return false;

   do
   {
      num = edf_alloc_fontnum--;
   } 
   while(num >= 0 && E_FontForNum(num));

   // ran out while looking for an unused number?
   if(num < 0)
      return false;

   // assign it!
   font->num = num;
   E_AddFontToNumHash(font);

   return true;
}

//
// E_AddFontToNumHash
//
// Puts the vfont_t into the numeric hash table.
//
static void E_AddFontToNumHash(vfont_t *font)
{
   // Auto-assign a numeric key to all fonts which don't have
   // a valid one explicitly specified.
   if(font->num < 0)
   {
      E_AutoAllocFontNum(font);
      return;
   }

   e_font_numhash.addObject(font);
}

//
// E_DelFontFromNumHash
//
// Removes the given vfont_t from the numeric hash table.
// For overrides changing the numeric key of an existing font.
//
static void E_DelFontFromNumHash(vfont_t *font)
{
   e_font_numhash.removeObject(font);
}

//=============================================================================
//
// Resource Disposal
//

//
// E_IsLinearLumpUsed
//
// Runs down font hash chains to see if any other linear font is
// using the resource in question, before freeing it.
// Returns true if the resource is used, and false otherwise.
//
static bool E_IsLinearLumpUsed(vfont_t *font, byte *data)
{
   vfont_t *rover = nullptr;

   // run down all hash chains
   while((rover = e_font_namehash.tableIterator(rover)))
   {
      if(rover != font && rover->linear && rover->data &&
         rover->data == data)
         return true; // is used
   }

   // didn't find one, doc
   return false;
}

//
// E_IsPatchUsed
//
// Returns true if some other font is using the patch in question,
// to support proper disposal of shared resources.
//
static bool E_IsPatchUsed(vfont_t *font, patch_t *p)
{
   vfont_t *rover = nullptr;

   // run down all hash chains
   while((rover = e_font_namehash.tableIterator(rover)))
   {
      if(rover != font && rover->fontgfx)
      {
         // run down the font graphics
         for(unsigned int j = 0; j < font->size; j++)
            if(font->fontgfx[j] == p)
               return true; // got a match
      }
   }

   // didn't find one, doc
   return false;
}

//
// E_DisposePatches
//
// Dumps all dumpable patches when overwriting a font.
//
static void E_DisposePatches(vfont_t *font)
{
   if(!font->fontgfx)
      return;

   for(unsigned int i = 0; i < font->size; i++)
   {
      if(font->fontgfx[i] && !E_IsPatchUsed(font, font->fontgfx[i]))
         Z_ChangeTag(font->fontgfx[i], PU_CACHE); // make purgable
   }

   // get rid of the patch array
   efree(font->fontgfx);
   font->fontgfx = nullptr;
}

//=============================================================================
//
// Initialization / Resource Loading
//

//
// E_LoadLinearFont
//
// Populates a pre-allocated vfont_t with information on a linear font 
// graphic.
//
static void E_LoadLinearFont(vfont_t *font, const char *name, int fmt,
                             bool requantize)
{
   int w = 0, h = 0, size = 0, i, lumpnum;
   bool foundsize = false;

   // in case this font was changed from patch to block:
   E_DisposePatches(font);

   // handle disposal of previous font graphics 
   if(font->data && !E_IsLinearLumpUsed(font, font->data))
   {
      efree(font->data);
      font->data = nullptr;
   }

   lumpnum = W_GetNumForName(name);

   size = W_LumpLength(lumpnum);

   if(fmt == FONT_FMT_PNG)
   {
      // convert PNG to linear
      VPNGImage fontpng;

      if(fontpng.readFromLump(wGlobalDir, lumpnum))
      {
         AutoPalette pal(wGlobalDir);
         bool is8Bit256    = (fontpng.getBitDepth() == 8 && 
                              fontpng.getNumColors() == 256);
         bool doRequantize = (!is8Bit256 || requantize);

         font->data = fontpng.getAs8Bit(doRequantize ? pal.get() : nullptr);

         w = static_cast<int>(fontpng.getWidth());
         h = static_cast<int>(fontpng.getHeight());

         size = w * h;
      }
   }
   else if(fmt == FONT_FMT_PATCH)
   {
      // convert patch to linear
      patch_t *p = PatchLoader::CacheNum(wGlobalDir, lumpnum, PU_STATIC);
      font->data = V_PatchToLinear(p, false, 0, &w, &h);

      // done with lump
      Z_ChangeTag(p, PU_CACHE);

      size = w * h;
   }
   else
      font->data = (byte *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));

   // check for proper dimensions
   for(i = 5; i <= 32; i++)
   {
      if(i * i * 128 == size)
      {
         font->lsize = i;
         foundsize = true;
         break;
      }
   }
   
   if(!foundsize)
   {
      E_EDFLoggedWarning(2, "Invalid lump dimensions for linear font %s\n", name);
      if(!E_IsLinearLumpUsed(font, font->data))
         efree(font->data);
      font->data = nullptr; // This font won't be able to draw, too bad.
   }

   font->start = 0;
   font->end   = 127;
   font->size  = 128; // full ASCII range is supported

   // set metrics - linear fonts are monospace
   font->cw    = font->lsize;
   font->cy    = font->lsize;
   font->dw    = 0;
   font->absh  = font->lsize;
   if(!font->useSpaceSize)
      font->space = font->lsize;

   // set flags
   font->centered = false; // not block-centered
   font->color    = true;  // supports color translations
   font->linear   = true;  // is linear, obviously ;)
   font->upper    = false; // not all-caps

   // patch array is unused
   font->fontgfx = nullptr;
}

//
// E_VerifyFilter
//
// Because it's exceptionally dangerous to allow the specification of format
// strings by the end user, I'm going to make sure it cannot be abused.
//
static void E_VerifyFilter(const char *str)
{
   const char *rover = str;
   bool inpct = false;
   bool foundpct = false;

   while(*rover != '\0')
   {
      if(inpct)
      {
         // formatting lengths are ok, and are indeed necessary
         if((*rover >= '0' && *rover <= '9') || *rover == '.')
         {
            ++rover;
            continue;
         }

         switch(*rover)
         {
         // allowed characters:
         case 'c': // char
         case 'i': // general int
         case 'd': // int
         case 'x': // hex
         case 'X': // upper-case hex
         case 'o': // octal
         case 'u': // unsigned
            inpct = false;
            break;
         default: // screw you, hacker boy
            E_EDFLoggedErr(2, 
               "E_VerifyFilter: '%s' has bad format specifier '%c'\n",
               str, *rover);
         }
      }
      
      if(*rover == '%')
      {
         if(foundpct) // already found one? dirty hackers.
         {
            E_EDFLoggedErr(2, 
               "E_VerifyFilter: '%s' has too many format specifiers\n",
               str);
         }
         foundpct = true;
         inpct = true;
      }

      ++rover; // let's not forget to do this!
   }
}

//
// E_FreeFilterData
//
static void E_FreeFilterData(vfontfilter_t *f)
{
   if(f->chars)
   {
      efree(f->chars);
      f->chars = nullptr;
   }

   if(f->mask)
   {
      efree((void *)f->mask);
      f->mask = nullptr;
   }
}

//
// E_FreeFontFilters
//
// Frees all the filters in a font that is being overwritten.
//
static void E_FreeFontFilters(vfont_t *font)
{
   if(!font->filters)
      return;

   for(unsigned int i = 0; i < font->numfilters; ++i)
      E_FreeFilterData(&(font->filters[i]));

   efree(font->filters);
   font->filters = nullptr;
}

//
// E_ProcessFontFilter
//
// Patch fonts can specify any number of filter objects (at least one is
// required), which have two methods for specifying the lump that belongs
// to a given character or range of characters. The filters use mask strings
// that are C format strings, but the validity of these are checked at load
// by the routine above.
//
static void E_ProcessFontFilter(cfg_t *sec, vfontfilter_t *f)
{
   unsigned int numchars;
   const char *tempstr;
   char *pos = nullptr;
   int tempnum = 0;

   // the filter works in one of two ways:
   // 1. specifies a list of characters to which it applies
   // 2. specifies a range of characters from start to end

   // is it a list?
   if((numchars = cfg_size(sec, ITEM_FILTER_CHARS)) > 0)
   {
      f->chars    = ecalloc(unsigned int *, numchars, sizeof(unsigned int));
      f->numchars = numchars;

      for(unsigned int i = 0; i < numchars; i++)
      {
         pos = nullptr;
         tempstr = cfg_getnstr(sec, ITEM_FILTER_CHARS, i);

         if(strlen(tempstr) > 1)
            tempnum = static_cast<int>(strtol(tempstr, &pos, 0));

         if(pos && *pos == '\0')
            f->chars[i] = tempnum;
         else
            f->chars[i] = *tempstr;
      }
   }
   else
   {
      // get fields
      pos = nullptr;
      tempstr = cfg_getstr(sec, ITEM_FILTER_START);
      
      if(strlen(tempstr) > 1)
         tempnum = static_cast<int>(strtol(tempstr, &pos, 0));

      if(pos && *pos == '\0') // is it a number?
         f->start = tempnum;
      else
         f->start = *tempstr; // interpret as character

      pos = nullptr;
      tempstr = cfg_getstr(sec, ITEM_FILTER_END);
      
      if(strlen(tempstr) > 1)
         tempnum = static_cast<int>(strtol(tempstr, &pos, 0));

      if(pos && *pos == '\0') // is it a number?
         f->end = tempnum;
      else
         f->end = *tempstr; // interpret as character
   }

   // get mask
   f->mask = cfg_getstrdup(sec, ITEM_FILTER_MASK);

   // verify mask string to prevent hacks
   E_VerifyFilter(f->mask);
}

//
// E_LoadPatchFont
//
// Creates the fontgfx array and precaches all patches as determined via
// execution of the filter objects in the font.
// 
static void E_LoadPatchFont(vfont_t *font)
{
   unsigned int i, j, k, m;
   char lumpname[9];

   // dump any pre-existing patch array
   E_DisposePatches(font);

   // in case this font was changed from block to patch:
   if(font->data && !E_IsLinearLumpUsed(font, font->data))
   {
      efree(font->data);
      font->data = nullptr;
      font->linear = false;
   }

   // first calculate font size
   font->size = (font->end - font->start + 1);

   if(font->size <= 0)
      E_EDFLoggedErr(2, "E_LoadPatchFont: font %s size <= 0\n", font->name);

   // init all to nullptr at beginning
   font->fontgfx = ecalloc(patch_t **, font->size, sizeof(patch_t *));

   for(i = 0, j = font->start; i < font->size; i++, j++)
   {
      vfontfilter_t *filter, *filtertouse = nullptr;

      // run down filters until a match is found
      for(k = 0; k < font->numfilters; k++)
      {
         filter = &(font->filters[k]);
         if(filter->numchars)
         {
            for(m = 0; m < filter->numchars; m++)
               if(j == filter->chars[m])
                  filtertouse = filter;
         }
         else if(j >= filter->start && j <= filter->end)
            filtertouse = filter;

         if(filtertouse)
            break;
      }

      if(filtertouse)
      {
         int lnum;

         memset(lumpname, 0, sizeof(lumpname));
         psnprintf(lumpname, sizeof(lumpname), 
                   filtertouse->mask, j - font->patchnumoffset);

         if((lnum = W_CheckNumForName(lumpname)) >= 0) // no errors here.
            font->fontgfx[i] = PatchLoader::CacheNum(wGlobalDir, lnum, PU_STATIC);
      }
   }
}

//
// E_setFontColor
//
static void E_setFontColor(cfg_t *sec, vfont_t *font, const char *name,
                           int vfont_t::*field, int gamemodeinfo_t::*gmiField)
{
   int color = E_StrToNumLinear(fontcolornames, CR_LIMIT, cfg_getstr(sec, name));
   if(color >= 0 && color < CR_LIMIT)
      font->*field = color;
   else
      font->*field = GameModeInfo->*gmiField;
}

//
// E_loadTranslation
//
static void E_loadTranslation(vfont_t *font, int index, const char *lumpname)
{
   if(index >= 0 && index < CR_LIMIT)
   {
      font->colrngs[index] = colrngs[index]; // start out with global colrng

      // defaults are all blank
      if(strlen(lumpname) == 0)
         return;

      // identity map?
      if(!strcasecmp(lumpname, "@identity"))
      {
         font->colrngs[index] = R_GetIdentityMap();
         return;
      }

      int lumpnum = 
         wGlobalDir.checkNumForNameNSG(lumpname, lumpinfo_t::ns_translations);
      if(lumpnum >= 0)
      {
         if(wGlobalDir.lumpLength(lumpnum) >= 256)
         {
            font->colrngs[index] = 
               static_cast<byte *>(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));
         }
      }
      else
      {
         // Try parsing it as a color translation string
         font->colrngs[index] = E_ParseTranslation(lumpname, PU_STATIC);
      }
   }
}

//=============================================================================
//
// EDF Processing
//

//
// E_ProcessFont
//
// Processes a single EDF font object.
//
static void E_ProcessFont(cfg_t *const sec, bool delta)
{
   vfont_t *font;
   const char *tempstr;
   int tempnum = 0;

   const char *title = delta ? cfg_getstr(sec, ITEM_DELTA_NAME) : cfg_title(sec);

   // The fonts were already pre-created; retrieve the vfont_t structure for
   // this definition.
   font = E_FontForName(title);

   if(delta && font == nullptr)
      E_EDFLoggedErr(2, "E_ProcessFont: Invalid delta 'name' value: '%s'\n", title);

   // If the font is neither linear nor has any filters, it hasn't been 
   // processed before, or in other words, this is a new font by name.
   // Otherwise, this definition's fields are treated additively over the
   // existing font's data.
   const bool def = !(font->linear || font->numfilters);

   const auto IS_SET = [sec, def](const char *const name) -> bool {
      return def || cfg_size(sec, name) > 0;
   };

   // process start
   if(IS_SET(ITEM_FONT_START))
   {
      char *pos = nullptr;
      tempstr = cfg_getstr(sec, ITEM_FONT_START);

      if(strlen(tempstr) > 1)
         tempnum = static_cast<int>(strtol(tempstr, &pos, 0));

      if(pos && *pos == '\0') // it is a number?
         font->start = tempnum; 
      else
         font->start = *tempstr; // interpret as character
   }

   // process end
   if(IS_SET(ITEM_FONT_END))
   {
      char *pos = nullptr;
      tempstr = cfg_getstr(sec, ITEM_FONT_END);

      if(strlen(tempstr) > 1)
         tempnum = static_cast<int>(strtol(tempstr, &pos, 0));

      if(pos && *pos == '\0') // is it a number?
         font->end = tempnum;
      else
         font->end = *tempstr; // interpret as character
   }

   // process linebreak height
   if(IS_SET(ITEM_FONT_CY))
      font->cy = cfg_getint(sec, ITEM_FONT_CY);

   // process space size
   if(IS_SET(ITEM_FONT_SPACE))
      font->space = cfg_getint(sec, ITEM_FONT_SPACE);

   // process width delta
   if(IS_SET(ITEM_FONT_DW))
      font->dw = cfg_getint(sec, ITEM_FONT_DW);

   // process absolute height (tallest character)
   if(IS_SET(ITEM_FONT_ABSH))
      font->absh = cfg_getint(sec, ITEM_FONT_ABSH);

   // process centered width
   if(IS_SET(ITEM_FONT_CW))
      font->cw = cfg_getint(sec, ITEM_FONT_CW);

   // process colorable flag
   if(IS_SET(ITEM_FONT_COLOR))
      font->color = cfg_getbool(sec, ITEM_FONT_COLOR);

   // process uppercase flag
   if(IS_SET(ITEM_FONT_UPPER))
      font->upper = cfg_getbool(sec, ITEM_FONT_UPPER);

   // process blockcentered flag
   if(IS_SET(ITEM_FONT_CENTER))
      font->centered = cfg_getbool(sec, ITEM_FONT_CENTER);

   if(IS_SET(ITEM_FONT_USPACE))
      font->useSpaceSize = cfg_getbool(sec, ITEM_FONT_USPACE);

   // haleyjd 09/06/12: colors
   // default color
   if(IS_SET(ITEM_FONT_COLORD))
   {
      E_setFontColor(sec, font, ITEM_FONT_COLORD,
         &vfont_t::colorDefault, &gamemodeinfo_t::defTextTrans);
   }

   // normal color
   if(IS_SET(ITEM_FONT_COLORN))
   {
      E_setFontColor(sec, font, ITEM_FONT_COLORN, 
         &vfont_t::colorNormal, &gamemodeinfo_t::colorNormal);
   }
   
   // high color
   if(IS_SET(ITEM_FONT_COLORH))
   {
      E_setFontColor(sec, font, ITEM_FONT_COLORH, 
         &vfont_t::colorHigh, &gamemodeinfo_t::colorHigh);
   }

   // error color
   if(IS_SET(ITEM_FONT_COLORE))
   {
      E_setFontColor(sec, font, ITEM_FONT_COLORE, 
         &vfont_t::colorError, &gamemodeinfo_t::colorError);
   }

   // haleyjd 09/06/12: translation tables
   if(cfg_size(sec, ITEM_FONT_COLORS) > 0)
   {
      cfg_t *colors = cfg_getsec(sec, ITEM_FONT_COLORS);

      for(int col = 0; col < CR_LIMIT; col++)
      {
         qstring translation;
         
         E_CfgListToCommaString(colors, fontcolornames[col], translation);

         E_loadTranslation(font, col, translation.constPtr());
      }
   }
   else if(def)
   {
      // When defining, adapt defaults for all colors if none were specified
      for(int col = 0; col < CR_LIMIT; col++)
         E_loadTranslation(font, col, "");
   }

   // process linear lump - if defined, this is a linear font automatically
   if(cfg_size(sec, ITEM_FONT_LLUMP) > 0)
   {
      if(delta && font->numfilters)
      {
         // handle disposal of pre-existing filters
         E_FreeFontFilters(font);
      }

      bool requantize = false;
      int format;
      tempstr = cfg_getstr(sec, ITEM_FONT_LLUMP);

      // try intuit the font's image format
      ZAutoBuffer lumpBuffer;
      vimgformat_e vfmt = FORMAT_INVALID;
      int lumpnum = W_GetNumForName(tempstr);
      wGlobalDir.cacheLumpAuto(lumpnum, lumpBuffer);
      vfmt = VImageManager::detectResourceFormat(lumpBuffer.get(), lumpBuffer.getSize());

      switch(vfmt)
      {
      case FORMAT_LINEAR:
         format = FONT_FMT_LINEAR;
         break;
      case FORMAT_PATCH:
         format = FONT_FMT_PATCH;
         break;
      case FORMAT_PNG:
         format = FONT_FMT_PNG;
         break;
      case FORMAT_INVALID:
      default:
         format = NUM_FONT_FMTS;
         break;
      }

      if(format == NUM_FONT_FMTS)
         E_EDFLoggedErr(2, "E_ProcessFont: couldn't detect format of linearlump of font '%s'\n", tempstr);

      // check for requantization flag (for PNGs)
      requantize = cfg_getbool(sec, ITEM_FONT_REQUAN);

      e_linearLumpNames.add(qstring(tempstr));
      font->linearreload.nameIndex = e_linearLumpNames.getLength() - 1;
      font->linearreload.format = format;
      font->linearreload.requantize = requantize;
      E_LoadLinearFont(font, tempstr, format, requantize);
   }
   else
   {
      unsigned int curnumfilters = font->numfilters;
      unsigned int numfilters    = cfg_size(sec, ITEM_FONT_FILTER);

      if(delta && font->linear)
      {
         if(font->data && !E_IsLinearLumpUsed(font, font->data))
            efree(font->data);
         font->data  = nullptr;
         font->linear = false;
      }

      // at least one filter is required; if this font is being modified, it 
      // may already possess filters.
      if(!curnumfilters && !numfilters)
         E_EDFLoggedErr(2, "E_ProcessFont: at least one filter is required\n");

      if(numfilters)
      {
         // handle disposal of pre-existing filters
         E_FreeFontFilters(font);

         // allocate the font filters
         font->filters    = ecalloc(vfontfilter_t *, numfilters, sizeof(vfontfilter_t));
         font->numfilters = numfilters;
      
         for(unsigned int i = 0; i < numfilters; i++)
         {
            E_ProcessFontFilter(cfg_getnsec(sec, ITEM_FONT_FILTER, i), 
                                &(font->filters[i]));
         }

         font->patchnumoffset = cfg_getint(sec, ITEM_FONT_POFFS);

         // load the font
         E_LoadPatchFont(font);
      }
   }

   E_EDFLogPrintf("\t\t%s font %s\n", 
                  def && !delta ? "Defined" : "Modified", font->name);
}

//
// E_ProcessFontVars
//
// Processes setting of native subsystem fonts via EDF globals.
//
static void E_ProcessFontVars(cfg_t *cfg)
{
   // 02/25/09: set native module font names
   E_ReplaceString(hud_fontname,      cfg_getstrdup(cfg, ITEM_FONT_HUD));
   E_ReplaceString(hud_overfontname,  cfg_getstrdup(cfg, ITEM_FONT_HUDO));
   E_ReplaceString(hud_fssmallname,   cfg_getstrdup(cfg, ITEM_FONT_HUDFSS));
   E_ReplaceString(hud_fsmediumname,  cfg_getstrdup(cfg, ITEM_FONT_HUDFSM));
   E_ReplaceString(hud_fslargename,   cfg_getstrdup(cfg, ITEM_FONT_HUDFSL));
   E_ReplaceString(mn_fontname,       cfg_getstrdup(cfg, ITEM_FONT_MENU));
   E_ReplaceString(mn_bigfontname,    cfg_getstrdup(cfg, ITEM_FONT_BMENU));
   E_ReplaceString(mn_normalfontname, cfg_getstrdup(cfg, ITEM_FONT_NMENU));
   E_ReplaceString(f_fontname,        cfg_getstrdup(cfg, ITEM_FONT_FINAL));
   E_ReplaceString(f_titlefontname,   cfg_getstrdup(cfg, ITEM_FONT_FTITLE));
   E_ReplaceString(in_fontname,       cfg_getstrdup(cfg, ITEM_FONT_INTR));
   E_ReplaceString(in_bigfontname,    cfg_getstrdup(cfg, ITEM_FONT_INTRB));
   E_ReplaceString(in_bignumfontname, cfg_getstrdup(cfg, ITEM_FONT_INTRBN));
   E_ReplaceString(c_fontname,        cfg_getstrdup(cfg, ITEM_FONT_CONS));
}

//
// E_hashFonts
//
// Create and add all fonts to the name and numeric hashes before processing
// begins, so that fonts can reference one another by name.
//
static void E_hashFonts(cfg_t *cfg, unsigned int numfonts)
{
   for(unsigned int i = 0; i < numfonts; i++)
   {
      cfg_t      *sec   = cfg_getnsec(cfg, EDF_SEC_FONT, i);
      const char *title = cfg_title(sec);
      int         num   = cfg_getint(sec, ITEM_FONT_ID);
      vfont_t    *font;

      // if one exists by this name already, modify it
      if((font = E_FontForName(title)))
      {
         // remove from numeric hash
         E_DelFontFromNumHash(font);

         // change the numeric key if a new one is specified
         if(num >= 0)
            font->num = num;

         // rehash (even if the key didn't change, this moves it to the front
         // of its hash chain)
         E_AddFontToNumHash(font);
      }
      else
      {
         font = estructalloc(vfont_t, 1);

         font->name = estrdup(title);
         font->num  = num;

         // add to hash tables
         E_AddFontToNameHash(font);
         E_AddFontToNumHash(font);
      }
   }
}

//=============================================================================
//
// Global Routines
//

//
// E_ProcessFonts
//
// Adds all fonts in the given cfg_t.
//
void E_ProcessFonts(cfg_t *cfg)
{
   unsigned int numfonts = cfg_size(cfg, EDF_SEC_FONT);

   // create and hash fonts
   E_EDFLogPrintf("\t* Hashing fonts\n"
                  "\t\t%u font(s) defined\n", numfonts);
   E_hashFonts(cfg, numfonts);
   
   // process fonts
   E_EDFLogPrintf("\t* Processing fonts\n");

   for(unsigned int i = 0; i < numfonts; i++)
      E_ProcessFont(cfg_getnsec(cfg, EDF_SEC_FONT, i), false);

   // process global font variables
   E_ProcessFontVars(cfg);
}

//
// Adds all fonts in the given cfg_t.
//
void E_ProcessFontDeltas(cfg_t *cfg)
{
   unsigned int numfontdeltas = cfg_size(cfg, EDF_SEC_FNTDELTA);
   
   // process fonts
   E_EDFLogPrintf("\t* Processing fontdeltas\n");

   for(unsigned int i = 0; i < numfontdeltas; i++)
   {
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_FNTDELTA, i);
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessFontDeltas: fontdelta requires name field\n");

      E_ProcessFont(deltasec, true);
   }

   // process global font variables
   E_ProcessFontVars(cfg);
}

//
// Reloads patch fonts when a new wad is loaded at runtime
//
void E_ReloadFonts()
{
   vfont_t *font = nullptr;

   // run down all hash chains
   // WARNING: do not use e_font_namehash here, because it's already iterated
   // inside the called function.
   while((font = e_font_numhash.tableIterator(font)))
   {
      if(!font->linear)
         E_LoadPatchFont(font);
      else if(font->linearreload.nameIndex < e_linearLumpNames.getLength())
      {
         E_LoadLinearFont(font,
            e_linearLumpNames[font->linearreload.nameIndex].constPtr(),
            font->linearreload.format,
            font->linearreload.requantize);
      }
   }
}

//
// E_FontForName
//
// Finds a font for the given name. If the name does not exist,
// nullptr is returned.
//
vfont_t *E_FontForName(const char *name)
{
   return e_font_namehash.objectForKey(name);
}

//
// E_FontForNum
//
// Finds a font for the given number. If the number is not found,
// nullptr is returned.
//
vfont_t *E_FontForNum(int num)
{
   return e_font_numhash.objectForKey(num);
}

//
// E_FontNumForName
//
// Given a name, returns the corresponding font number, or -1 if
// the name does not exist.
//
int E_FontNumForName(const char *name)
{ 
   vfont_t *font = E_FontForName(name);

   return font ? font->num : -1;
}

// EOF

