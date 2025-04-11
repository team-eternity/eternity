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
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    EDF Font Definitions
//
//-----------------------------------------------------------------------------

#ifndef E_FONTS_H__
#define E_FONTS_H__

struct vfont_t;

#ifdef NEED_EDF_DEFINITIONS

constexpr const char EDF_SEC_FONT[]      = "font";
constexpr const char EDF_SEC_FNTDELTA[]  = "fontdelta";

constexpr const char ITEM_FONT_HUD[]     = "hu_font";
constexpr const char ITEM_FONT_HUDO[]    = "hu_overlayfont";
constexpr const char ITEM_FONT_MENU[]    = "mn_font";
constexpr const char ITEM_FONT_BMENU[]   = "mn_font_big";
constexpr const char ITEM_FONT_NMENU[]   = "mn_font_normal";
constexpr const char ITEM_FONT_FINAL[]   = "f_font";
constexpr const char ITEM_FONT_FTITLE[]  = "f_titlefont";
constexpr const char ITEM_FONT_INTR[]    = "in_font";
constexpr const char ITEM_FONT_INTRB[]   = "in_font_big";
constexpr const char ITEM_FONT_INTRBN[]  = "in_font_bignum";
constexpr const char ITEM_FONT_CONS[]    = "c_font";

constexpr const char ITEM_FONT_HUDFSS[]  = "hu_fssmallfont";
constexpr const char ITEM_FONT_HUDFSM[]  = "hu_fsmediumfont";
constexpr const char ITEM_FONT_HUDFSL[]  = "hu_fsslargefont";

extern cfg_opt_t edf_font_opts[];
extern cfg_opt_t edf_fntdelta_opts[];
void E_ProcessFonts(cfg_t *);
void E_ProcessFontDeltas(cfg_t *);

#endif

void     E_ReloadFonts();

vfont_t *E_FontForName(const char *);
vfont_t *E_FontForNum(int);
int      E_FontNumForName(const char *);

#endif

// EOF

