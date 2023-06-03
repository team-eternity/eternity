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
struct spritecontext_t;
struct contextbounds_t;
struct portal_t;
struct portalcontext_t;
struct portalrender_t;
struct viewpoint_t;
struct cbviewpoint_t;
struct bspcontext_t;
struct cmapcontext_t;
struct rendercontext_t;
struct vissprite_t;
class  ZoneHeap;

using R_ColumnFunc = void (*)(cb_column_t &);

// Constant arrays used for psprite clipping and initializing clipping.

extern float *zeroarray;
extern float *screenheightarray;

// Data for rendering sprites across linedef portals correctly
struct maskedparent_t
{
   bool fromlineportal;    // whether it's from line portal
   int curindex;           // current index, needed to keep track
   const line_t *line;     // linedef of portal, which can be either wall or edge
   const portal_t *portal; // the portal, as it's not a straightforward pointer from line
   float dist1, dist2;     // for checking if intersecting sprite
   float x1frac, x2frac;
};

// SoM 12/13/03: the stack for use with portals
struct maskedrange_t
{
   int firstds, lastds;
   int firstsprite, lastsprite;

   // SoM: Cardboard
   float *floorclip;
   float *ceilingclip;
   
   // ioanch: for updating the stack list
   maskedparent_t parent;
   
   // for unused head
   struct maskedrange_t *next;
};

struct poststack_t
{
   planehash_t   *overlay;
   maskedrange_t *masked;
};

void R_PushPost(bspcontext_t &bspcontext, spritecontext_t &spritecontext, ZoneHeap &heap,
                const contextbounds_t &bounds, bool pushmasked, pwindow_t *window, 
                const maskedparent_t &parent);

void R_ClearBadSpritesAndFrames();

// SoM: Cardboard
void R_SetMaskedSilhouette(const contextbounds_t &bounds,
                           const float *top, const float *bottom);

struct cb_maskedcolumn_t;
struct texture_t;
struct texcol_t;

void R_DrawNewMaskedColumn(const R_ColumnFunc colfunc,
                           cb_column_t &column, const cb_maskedcolumn_t &maskedcolumn,
                           const texture_t *tex, const texcol_t *tcolumn,
                           const float *const mfloorclip, const float *const mceilingclip);
void R_AddSprites(cmapcontext_t &cmapcontext,
                  spritecontext_t &spritecontext,
                  ZoneHeap &heap,
                  const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                  const contextbounds_t &bounds,
                  const portalrender_t &portalrender,
                  sector_t *sec, int); // killough 9/18/98
void R_InitSprites(char **namelist);
void R_ClearSprites(spritecontext_t &context);
void R_ClearMarkedSprites(spritecontext_t &context, ZoneHeap &heap);
void R_DrawPostBSP(rendercontext_t &context);
void R_DrawPlayerSprites();
void R_ClearParticles(void);
void R_InitParticles(void);
particle_t *newParticle(void);


void R_LinkSpriteProj(Mobj &thing);
void R_UnlinkSpriteProj(Mobj &thing);

void R_ScanForSpritesOverlappingWallPortals(const viewpoint_t &viewpoint,
                                            const portalcontext_t &portalcontext,
                                            const spritecontext_t &spritecontext);

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
   const spriteprojnode_t *parent;        // parent in the tree (null if it's mobj) - LINE only
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
