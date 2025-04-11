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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//  The status bar widget definitions and prototypes
//
//-----------------------------------------------------------------------------

#ifndef ST_LIB_H__
#define ST_LIB_H__

struct patch_t;

//
// Widget Types
//

//
// Number widget
//
struct st_number_t
{
   int       x;      // upper right-hand corner
   int       y;      //  of the number (right-justified)
   int       width;  // max # of digits in number
   int       num;    // current value
   int       max;    // max value
   bool     *on;     // pointer to bool stating whether to update number
   patch_t **p;      // list of patches for 0-9
};

//
// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
//
struct st_percent_t
{
   st_number_t  n; // number information
   patch_t     *p; // percent sign graphic
};

// Multiple Icon widget
struct st_multicon_t
{
   int       x;       // center-justified location of icons
   int       y;
   int      *inum;    // pointer to current icon
   bool     *on;      // pointer to bool stating whether to update icon
   patch_t **p;       // list of icons
   int       data;    // user data
};

//
// Binary Icon widget
//
struct st_binicon_t
{
   int      x;      // center-justified location of icon
   int      y;
   bool    *val;    // pointer to current icon status
   bool    *on;     // pointer to bool stating whether to update icon
   patch_t *p;      // icon
   int      data;   // user data
};

//
// Widget creation, access, and update routines
//

// Initializes widget library.
// More precisely, initialize STMINUS,
//  everything else is done somewhere else.
void STlib_init();

// Number widget routines
void STlib_initNum(st_number_t *n, int x, int y, patch_t **pl, int num, 
                   int max, bool *on, int width);

// jff 1/16/98 add color translation to digit output
void STlib_updateNum(st_number_t *n, byte *outrng, int alpha);

// Percent widget routines
void STlib_initPercent(st_percent_t *p, int x, int y, patch_t **pl, int num,
                       bool *on, patch_t *percent);

// jff 1/16/98 add color translation to percent output
void STlib_updatePercent(st_percent_t *per, byte *outrng, int alpha);


// Multiple Icon widget routines
void STlib_initMultIcon(st_multicon_t *mi, int x, int y, patch_t **il, int *inum,
                        bool* on);

void STlib_updateMultIcon(st_multicon_t *mi, int alpha);

// Binary Icon widget routines
void STlib_initBinIcon(st_binicon_t *b, int x, int y, patch_t *i, bool *val,
                       bool *on);

void STlib_updateBinIcon(st_binicon_t *bi);

#endif


//----------------------------------------------------------------------------
//
// $Log: st_lib.h,v $
// Revision 1.5  1998/05/11  10:44:46  jim
// formatted/documented st_lib
//
// Revision 1.4  1998/02/19  16:55:12  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/18  00:59:16  jim
// Addition of HUD
//
// Revision 1.2  1998/01/26  19:27:55  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

