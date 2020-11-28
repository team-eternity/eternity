// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
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
// DESCRIPTION:
//      Rendering of moving objects, sprites.
//
//-----------------------------------------------------------------------------

#ifndef R_THINGS_H__
#define R_THINGS_H__

#define MAX_SPRITE_FRAMES 29          /* Macroized -- killough 1/25/98 */

struct line_t;
struct sector_t;
struct particle_t;
struct planehash_t;
struct pwindow_t;
struct cb_column_t;
struct rendercontext_t;
struct spritecontext_t;
struct contextbounds_t;

// Constant arrays used for psprite clipping and initializing clipping.

extern float *zeroarray;
extern float *screenheightarray;

// SoM 12/13/03: the stack for use with portals
struct maskedrange_t
{
   int firstds, lastds;
   int firstsprite, lastsprite;

   // SoM: Cardboard
   float *floorclip;
   float *ceilingclip;
   
   // for unused head
   struct maskedrange_t *next;
};

struct poststack_t
{
   planehash_t   *overlay;
   maskedrange_t *masked;
};

void R_PushPost(spritecontext_t &context, const contextbounds_t &bounds,
                bool pushmasked, pwindow_t *window);

// SoM: Cardboard
void R_SetMaskedSilhouette(const contextbounds_t &context,
                           const float *top, const float *bottom);

struct cb_maskedcolumn_t;
struct texture_t;
struct texcol_t;

void R_DrawNewMaskedColumn(const rendercontext_t &context,
                           cb_column_t &column, const cb_maskedcolumn_t &maskedcolumn,
                           const texture_t *tex, const texcol_t *tcolumn,
                           const float *const mfloorclip, const float *const mceilingclip);
void R_AddSprites(spritecontext_t &context, const contextbounds_t &bounds,
                  sector_t *sec, int); // killough 9/18/98
void R_InitSprites(char **namelist);
void R_ClearSprites(spritecontext_t &context);
void R_DrawPostBSP(spritecontext_t &spritecontext, planecontext_t &planecontext,
                   void (*&colfunc)(cb_column_t &), const contextbounds_t &bounds);
void R_DrawPlayerSprites();
void R_ClearParticles(void);
void R_InitParticles(void);
particle_t *newParticle(void);

struct cb_maskedcolumn_t
{
   float ytop;
   float scale;
};

///////////////////////////////////////////////////////////////////////////////
//
// ioanch 20160109: rendering of sprites cut by sector portals
//

//
// spriteprojnode_t
//
// Portal sprite reference node
//
struct spriteprojnode_t
{
   Mobj *mobj;                            // source mobj
   const sector_t *sector;                // sector where this appears
   v3fixed_t delta;                       // portal accumulated delta (do not
                                          // link offsets)
   const line_t *portalline;              // portal line (if applicable)
   DLListItem<spriteprojnode_t> mobjlink; // vertical link (links separate layers)
   DLListItem<spriteprojnode_t> sectlink; // horizontal link (links separate mobjs)
   DLListItem<spriteprojnode_t> freelink; // free list link (for recycling)
};

void R_RemoveMobjProjections(Mobj *mobj);
void R_CheckMobjProjections(Mobj *mobj, bool checklines);

///////////////////////////////////////////////////////////////////////////////

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
