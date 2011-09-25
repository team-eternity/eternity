// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3: 
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
// DESCRIPTION:
//      Rendering of moving objects, sprites.
//
//-----------------------------------------------------------------------------

#ifndef R_THINGS_H__
#define R_THINGS_H__

struct sector_t;
struct particle_t;
struct planehash_t;

// Constant arrays used for psprite clipping and initializing clipping.

extern float zeroarray[MAX_SCREENWIDTH];
extern float screenheightarray[MAX_SCREENWIDTH];

// Vars for R_DrawMaskedColumn

extern float *mfloorclip, *mceilingclip;

// haleyjd 10/09/06: optional vissprite limit
extern int r_vissprite_limit;

// SoM 12/13/03: the stack for use with portals
struct maskedrange_t
{
   int firstds, lastds;
   int firstsprite, lastsprite;

   // SoM: Cardboard
   float floorclip[MAX_SCREENWIDTH];
   float ceilingclip[MAX_SCREENWIDTH];
   
   // for unused head
   struct maskedrange_t *next;
};

struct poststack_t
{
   planehash_t   *overlay;
   maskedrange_t *masked;
};

void R_PushPost(bool pushmasked, planehash_t *overlay);

// SoM: Cardboard
void R_SetMaskedSilhouette(float *top, float *bottom);

struct texture_t;
struct texcol_t;

void R_DrawNewMaskedColumn(texture_t *tex, texcol_t *tcolumn);
void R_AddSprites(sector_t *sec, int); // killough 9/18/98
void R_AddPSprites(void);
void R_DrawSprites(void);
void R_InitSprites(char **namelist);
void R_ClearSprites(void);
void R_DrawPostBSP(void);
void R_ClearParticles(void);
void R_InitParticles(void);
particle_t *newParticle(void);

typedef struct cb_maskedcolumn_s
{
   float ytop;
   float scale;
} cb_maskedcolumn_t;

extern cb_maskedcolumn_t maskedcolumn;

#endif

//----------------------------------------------------------------------------
//
// $Log: r_things.h,v $
// Revision 1.4  1998/05/03  22:46:19  killough
// beautification
//
// Revision 1.3  1998/02/09  03:23:27  killough
// Change array decl to use MAX screen width/height
//
// Revision 1.2  1998/01/26  19:27:49  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
