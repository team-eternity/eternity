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
//  Refresh of things represented by sprites --
//  i.e. map objects and particles.
//
//  Particle code largely from zdoom, thanks to Randy Heit.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "d_main.h"
#include "doomstat.h"
#include "e_edf.h"
#include "e_exdata.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_compare.h"
#include "m_swap.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_maputl.h"   // ioanch 20160125
#include "p_partcl.h"
#include "p_portal.h"
#include "p_portalblockmap.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_user.h"
#include "r_bsp.h"
#include "r_draw.h"
#include "r_interpolate.h"
#include "r_main.h"
#include "r_patch.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_pcheck.h"   // ioanch 20160109: for sprite rendering through portals
#include "r_segs.h"
#include "r_state.h"
#include "r_things.h"
#include "v_alloc.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

//=============================================================================
//
// External Declarations
//

extern int *columnofs;
extern int global_cmap_index; // haleyjd: NGCS

//=============================================================================
// 
// Defines

#define MINZ        (FRACUNIT*4)
#define BASEYCENTER 100

#define MAX_SPRITE_FRAMES 29          /* Macroized -- killough 1/25/98 */

#define IS_FULLBRIGHT(actor) \
   (((actor)->frame & FF_FULLBRIGHT) || ((actor)->flags4 & MF4_BRIGHT))

#define CLIP_UNDEF (-2)

//=============================================================================
//
// Globals
//

// constant arrays
//  used for psprite clipping and initializing clipping

float *zeroarray;
float *screenheightarray;
int    lefthanded = 0;

VALLOCATION(zeroarray)
{
   float *buffer = emalloctag(float *, w*2 * sizeof(float), PU_VALLOC, NULL);
   zeroarray = buffer;
   screenheightarray = buffer + w;

   for(float *itr = buffer, *end = itr + w*2; itr != end; ++itr)
      *itr = 0.0f;
}

// variables used to look up and range check thing_t sprites patches

spritedef_t *sprites;
int numsprites;

// haleyjd: global particle system state

int        activeParticles;
int        inactiveParticles;
particle_t *Particles;
int        particle_trans;

float *mfloorclip, *mceilingclip;

cb_maskedcolumn_t maskedcolumn;

//=============================================================================
//
// Structures
//

struct maskdraw_t
{
  int x1;
  int x2;
  int column;
  int topclip;
  int bottomclip;
};

//
// A vissprite_t is a thing that will be drawn during a refresh.
// i.e. a sprite object that is partly visible.
//
// haleyjd 12/15/10: Moved out of r_defs.h
//
struct vissprite_t
{
  int     x1, x2;
  fixed_t gx, gy;              // for line side calculation
  fixed_t gz, gzt;             // global bottom / top for silhouette clipping
  fixed_t texturemid;
  int     patch;
  byte    drawstyle;

  float   startx;
  float   dist, xstep;
  float   ytop, ybottom;
  float   scale;

  // for color translation and shadow draw, maxbright frames as well
        // sf: also coloured lighting
  lighttable_t *colormap;
  int colour;   //sf: translated colour

  // killough 3/27/98: height sector for underwater/fake ceiling support
  int heightsec;

  uint16_t translucency; // haleyjd: zdoom-style translucency
  int tranmaplump;

  fixed_t footclip; // haleyjd: foot clipping

  int    sector; // SoM: sector the sprite is in.

};

// haleyjd 04/25/10: drawsegs optimization
struct drawsegs_xrange_t
{
   int x1, x2;
   drawseg_t *user;
};

//=============================================================================
//
// Statics
//

// top and bottom of portal silhouette
static float *portaltop;
static float *portalbottom;

VALLOCATION(portaltop)
{
   float *buf = emalloctag(float *, 2 * w * sizeof(*portaltop), PU_VALLOC, NULL);

   for(int i = 0; i < 2*w; i++)
      buf[i] = 0.0f;

   portaltop    = buf;
   portalbottom = buf + w;
}

static float *ptop, *pbottom;

// haleyjd 04/25/10: drawsegs optimization
static drawsegs_xrange_t *drawsegs_xrange;
static unsigned int drawsegs_xrange_size = 0;
static int drawsegs_xrange_count = 0;

static float *pscreenheightarray; // for psprites

VALLOCATION(pscreenheightarray)
{
   pscreenheightarray = ecalloctag(float *, w, sizeof(float), PU_VALLOC, NULL);
}

static lighttable_t **spritelights; // killough 1/25/98 made static

static spriteframe_t sprtemp[MAX_SPRITE_FRAMES];
static int maxframe;

// Max number of particles
static int numParticles;

static vissprite_t *vissprites, **vissprite_ptrs;  // killough
static size_t num_vissprite, num_vissprite_alloc, num_vissprite_ptrs;

// SoM 12/13/03: the post-BSP stack
static poststack_t   *pstack       = NULL;
static int            pstacksize   = 0;
static int            pstackmax    = 0;
static maskedrange_t *unusedmasked = NULL;

VALLOCATION(pstack)
{
   if(pstack)
   {
      // free all maskedrange_t on the pstack
      for(int i = 0; i < pstacksize; i++)
      {
         if(pstack[i].masked)
         {
            efree(pstack[i].masked->ceilingclip);
            efree(pstack[i].masked);
         }
      }

      // free the pstack
      efree(pstack);
   }

   // free the maskedrange freelist 
   maskedrange_t *mr = unusedmasked;
   while(mr)
   {
      maskedrange_t *next = mr->next;
      efree(mr->ceilingclip);
      efree(mr);
      mr = next;
   }

   pstack       = NULL;
   pstacksize   = 0;
   pstackmax    = 0;
   unusedmasked = NULL;
}

// haleyjd: made static global
static float *clipbot;
static float *cliptop;

VALLOCATION(clipbot)
{
   float *buffer = ecalloctag(float *, w*2, sizeof(float), PU_VALLOC, NULL);
   clipbot = buffer;
   cliptop = buffer + w;
}

//=============================================================================
//
// Functions
//

// Forward declarations:
static void R_DrawParticle(vissprite_t *vis);
static void R_ProjectParticle(particle_t *particle);

//
// R_SetMaskedSilhouette
//
void R_SetMaskedSilhouette(const float *top, const float *bottom)
{
   if(!top || !bottom)
   {
      float *topp  = portaltop, *bottomp = portalbottom, 
            *stopp = portaltop + video.width;

      while(topp < stopp)
      {
         *topp++ = 0.0f;
         *bottomp++ = view.height - 1.0f;
      }
   }
   else
   {
      memcpy(portaltop,    top,    sizeof(*portaltop   ) * video.width);
      memcpy(portalbottom, bottom, sizeof(*portalbottom) * video.width);
   }
}

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//

//
// INITIALIZATION FUNCTIONS
//

//
// R_InstallSpriteLump
//
// Local function for R_InitSprites.
//
static void R_InstallSpriteLump(lumpinfo_t *lump, int lumpnum, unsigned frame,
                                unsigned rotation, bool flipped)
{
   if(frame >= MAX_SPRITE_FRAMES || rotation > 8)
      I_Error("R_InstallSpriteLump: Bad frame characters in lump %s\n", lump->name);

   if((int)frame > maxframe)
      maxframe = frame;

   if(rotation == 0)
   {
      // the lump should be used for all rotations
      for(int r = 0; r < 8; r++)
      {
         if(sprtemp[frame].lump[r]==-1)
         {
            sprtemp[frame].lump[r] = lumpnum - firstspritelump;
            sprtemp[frame].flip[r] = (byte) flipped;
            sprtemp[frame].rotate = 0; //jff 4/24/98 if any subbed, rotless
         }
      }
      return;
   }

   // the lump is only used for one rotation
   if(sprtemp[frame].lump[--rotation] == -1)
   {
      sprtemp[frame].lump[rotation] = lumpnum - firstspritelump;
      sprtemp[frame].flip[rotation] = (byte) flipped;
      sprtemp[frame].rotate = 1; //jff 4/24/98 only change if rot used
   }
}

// Empirically verified to have excellent hash
// properties across standard Doom sprites:
#define R_SpriteNameHash(s) ((unsigned int)((s)[0]-((s)[1]*3-(s)[3]*2-(s)[2])*2))

// haleyjd 10/10/11: externalized structure due to template limitations with
// local struct types in the pre-C++11 standard
struct rsprhash_s { int index, next; };

//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
// (4 chars exactly) to be used.
//
// Builds the sprite rotation matrixes to account
// for horizontally flipped sprites.
//
// Will report an error if the lumps are inconsistent.
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
//
// A sprite that is flippable will have an additional
//  letter/number appended.
//
// The rotation character can be 0 to signify no rotations.
//
// 1/25/98, 1/31/98 killough : Rewritten for performance
//
static void R_InitSpriteDefs(char **namelist)
{
   size_t numentries = lastspritelump - firstspritelump + 1;
   rsprhash_s *hash;
   unsigned int i;
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();

   if(!numentries || !*namelist)
      return;
   
   // count the number of sprite names
   for(i = 0; namelist[i]; i++)
      ; // do nothing
   
   numsprites = (signed int)i;

   sprites = estructalloctag(spritedef_t, numsprites, PU_RENDERER);
   
   // Create hash table based on just the first four letters of each sprite
   // killough 1/31/98
   
   hash = estructalloc(rsprhash_s, numentries); // allocate hash table

   for(i = 0; i < numentries; i++) // initialize hash table as empty
      hash[i].index = -1;

   for(i = 0; i < numentries; i++)  // Prepend each sprite to hash chain
   {                                // prepend so that later ones win
      int j = R_SpriteNameHash(lumpinfo[i+firstspritelump]->name) % numentries;
      hash[i].next = hash[j].index;
      hash[j].index = i;
   }

   // scan all the lump names for each of the names,
   //  noting the highest frame letter.

   for(i = 0; i < (unsigned int)numsprites; i++)
   {
      const char *spritename = namelist[i];
      int j = hash[R_SpriteNameHash(spritename) % numentries].index;
      
      if(j >= 0)
      {
         memset(sprtemp, -1, sizeof(sprtemp));
         maxframe = -1;
         do
         {
            lumpinfo_t *lump = lumpinfo[j + firstspritelump];

            // Fast portable comparison -- killough
            // (using int pointer cast is nonportable):

            if(!((lump->name[0] ^ spritename[0]) |
                 (lump->name[1] ^ spritename[1]) |
                 (lump->name[2] ^ spritename[2]) |
                 (lump->name[3] ^ spritename[3])))
            {
               R_InstallSpriteLump(lump, j+firstspritelump,
                                   lump->name[4] - 'A',
                                   lump->name[5] - '0',
                                   false);
               if(lump->name[6])
                  R_InstallSpriteLump(lump, j+firstspritelump,
                                      lump->name[6] - 'A',
                                      lump->name[7] - '0',
                                      true);
            }
         }
         while((j = hash[j].next) >= 0);

         // check the frames that were found for completeness
         if((sprites[i].numframes = ++maxframe))  // killough 1/31/98
         {
            for(int frame = 0; frame < maxframe; frame++)
            {
               switch(sprtemp[frame].rotate)
               {
               case -1:
                  // no rotations were found for that frame at all
                  I_Error("R_InitSprites: No patches found for %.8s frame %c\n", 
                          namelist[i], frame + 'A');
                  break;
                  
               case 0:
                  // only the first rotation is needed
                  break;
                  
               case 1:
                  // must have all 8 frames
                  for(int rotation = 0; rotation < 8; rotation++)
                  {
                     if(sprtemp[frame].lump[rotation] == -1)
                     {
                        I_Error ("R_InitSprites: Sprite %.8s frame %c is missing rotations\n",
                                 namelist[i], frame + 'A');
                     }
                  }
                  break;
               }
            }
            // allocate space for the frames present and copy sprtemp to it
            sprites[i].spriteframes = estructalloctag(spriteframe_t, maxframe, PU_RENDERER);
            memcpy(sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
         }
      }
      else
      {
         // haleyjd 08/15/02: problem here.
         // If j was -1 above, meaning that there are no lumps for the sprite
         // present, the sprite is left uninitialized. This creates major 
         // problems in R_PrecacheLevel if a thing tries to subsequently use
         // that sprite. Instead, set numframes to 0 and spriteframes to NULL.
         // Then, check for these values before loading any sprite.
         
         sprites[i].numframes = 0;
         sprites[i].spriteframes = NULL;
      }
   }
   efree(hash);             // free hash table
}

//
// GAME FUNCTIONS
//

//
// R_InitSprites
//
// Called at program start.
//
void R_InitSprites(char **namelist)
{
   R_InitSpriteDefs(namelist);
   R_InitSpriteProjSpan();
}

//
// R_ClearSprites
//
// Called at frame start.
//
void R_ClearSprites()
{
   num_vissprite = 0; // killough
}

//
// R_PushPost
//
// Pushes a new element on the post-BSP stack. 
//
void R_PushPost(bool pushmasked, pwindow_t *window)
{
   poststack_t *post;
   
   if(pstacksize == pstackmax)
   {
      pstackmax += 10;
      pstack = erealloc(poststack_t *, pstack, sizeof(poststack_t) * pstackmax);
   }
   
   post = pstack + pstacksize;

   if(window)
   {
      post->overlay = window->poverlay;
      window->poverlay = nullptr;   // clear reference
   }
   else
      post->overlay = nullptr;

   if(pushmasked)
   {
      int i;
      
      // Get an unused maskedrange object, or allocate a new one
      if(unusedmasked)
      {
         post->masked = unusedmasked;
         unusedmasked = unusedmasked->next;

         post->masked->next = NULL;
         post->masked->firstds = post->masked->lastds =
            post->masked->firstsprite = post->masked->lastsprite = 0;
      }
      else
      {
         post->masked = estructalloc(maskedrange_t, 1);
       
         float *buf = emalloc(float *, 2 * video.width * sizeof(float));
         post->masked->ceilingclip = buf;
         post->masked->floorclip   = buf + video.width;
      }
         
      for(i = pstacksize - 1; i >= 0; i--)
      {
         if(pstack[i].masked)
            break;
      }
      
      if(i >= 0)
      {
         post->masked->firstds     = pstack[i].masked->lastds;
         post->masked->firstsprite = pstack[i].masked->lastsprite;
      }
      else
         post->masked->firstds = post->masked->firstsprite = 0;
         
      post->masked->lastds     = int(ds_p - drawsegs);
      post->masked->lastsprite = int(num_vissprite);

      memcpy(post->masked->ceilingclip, portaltop,    sizeof(*portaltop)    * video.width);
      memcpy(post->masked->floorclip,   portalbottom, sizeof(*portalbottom) * video.width);
   }
   else
      post->masked = NULL;

   pstacksize++;
}

//
// R_NewVisSprite
//
// Creates a new vissprite if needed, or recycles an unused one.
//
static vissprite_t *R_NewVisSprite()
{
   if(num_vissprite >= num_vissprite_alloc)             // killough
   {
      num_vissprite_alloc = num_vissprite_alloc ? num_vissprite_alloc*2 : 128;
      vissprites = erealloc(vissprite_t *, vissprites, num_vissprite_alloc*sizeof(*vissprites));
   }

   return vissprites + num_vissprite++;
}

//
// R_DrawMaskedColumn
//
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
static void R_DrawMaskedColumn(column_t *tcolumn)
{
   float y1, y2;
   fixed_t basetexturemid = column.texmid;
   
   column.texheight = 0; // killough 11/98

   while(tcolumn->topdelta != 0xff)
   {
      // calculate unclipped screen coordinates for post
      y1 = maskedcolumn.ytop + (maskedcolumn.scale * tcolumn->topdelta);
      y2 = y1 + (maskedcolumn.scale * tcolumn->length) - 1;

      column.y1 = (int)(y1 < mceilingclip[column.x] ? mceilingclip[column.x] : y1);
      column.y2 = (int)(y2 > mfloorclip[column.x]   ? mfloorclip[column.x]   : y2);

      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(column.y1 <= column.y2 && column.y2 < viewwindow.height)
      {
         column.source = (byte *)tcolumn + 3;
         column.texmid = basetexturemid - (tcolumn->topdelta << FRACBITS);

         colfunc();
      }

      tcolumn = (column_t *)((byte *)tcolumn + tcolumn->length + 4);
   }

   column.texmid = basetexturemid;
}

//
// R_DrawNewMaskedColumn
//
void R_DrawNewMaskedColumn(texture_t *tex, texcol_t *tcol)
{
   float y1, y2;
   fixed_t basetexturemid = column.texmid;
   
   column.texheight = 0; // killough 11/98

   while(tcol)
   {
      // calculate unclipped screen coordinates for post
      y1 = maskedcolumn.ytop + (maskedcolumn.scale * tcol->yoff);
      y2 = y1 + (maskedcolumn.scale * tcol->len) - 1;

      column.y1 = (int)(y1 < mceilingclip[column.x] ? mceilingclip[column.x] : y1);
      column.y2 = (int)(y2 > mfloorclip[column.x]   ? mfloorclip[column.x]   : y2);

      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(column.y1 <= column.y2 && column.y2 < viewwindow.height)
      {
         column.source = tex->buffer + tcol->ptroff;
         column.texmid = basetexturemid - (tcol->yoff << FRACBITS);

         // Drawn by either R_DrawColumn
         //  or (SHADOW) R_DrawFuzzColumn.
         colfunc();
      }

      tcol = tcol->next;
   }

   column.texmid = basetexturemid;
}

//
// R_DrawVisSprite
//
//  mfloorclip and mceilingclip should also be set.
//
static void R_DrawVisSprite(vissprite_t *vis, int x1, int x2)
{
   column_t *tcolumn;
   int       texturecolumn;
   float     frac;
   patch_t  *patch;
   bool      footclipon = false;
   float     baseclip = 0;
   int       w;

   if(vis->patch == -1)
   {
      // this vissprite belongs to a particle
      R_DrawParticle(vis);
      return;
   }
  
   patch = PatchLoader::CacheNum(wGlobalDir, vis->patch+firstspritelump, PU_CACHE);
   
   column.colormap = vis->colormap;
   
   // killough 4/11/98: rearrange and handle translucent sprites
   // mixed with translucent/non-translucent 2s normals

   if(vis->colour)
      column.translation = translationtables[vis->colour - 1];
   
   column.translevel = vis->translucency;
   column.translevel += 1;
   if(vis->tranmaplump >= 0)
   {
      tranmap = static_cast<byte *>(wGlobalDir.cacheLumpNum(vis->tranmaplump,
                                                            PU_CACHE));
   }
   else if(vis->drawstyle == VS_DRAWSTYLE_SUB)
      tranmap = main_submap;
   else
      tranmap = main_tranmap; // killough 4/11/98   
   
   // haleyjd: faster selection for drawstyles
   colfunc = r_column_engine->ByVisSpriteStyle[vis->drawstyle][!!vis->colour];

   //column.step = M_FloatToFixed(vis->ystep);
   column.step = M_FloatToFixed(1.0f / vis->scale);
   column.texmid = vis->texturemid;
   maskedcolumn.scale = vis->scale;
   maskedcolumn.ytop = vis->ytop;
   frac = vis->startx;
   
   // haleyjd 10/10/02: foot clipping
   if(vis->footclip)
   {
      footclipon = true;
      baseclip = vis->ybottom - M_FixedToFloat(vis->footclip) * maskedcolumn.scale;
   }

   w = patch->width;

   // haleyjd: use a separate loop for footclip things, to minimize
   // overhead for regular sprites and to require no separate loop
   // just to update mfloorclip
   if(footclipon)
   {
      for(column.x=vis->x1 ; column.x<=vis->x2 ; column.x++, frac += vis->xstep)
      {
         // haleyjd: if baseclip is higher than mfloorclip for this
         // column, swap it in for that column
         if(baseclip < mfloorclip[column.x])
            mfloorclip[column.x] = baseclip;

         texturecolumn = (int)frac;
         
         // haleyjd 09/16/07: Cardboard requires this rangecheck, made nonfatal
         if(texturecolumn < 0 || texturecolumn >= w)
            continue;
         
         tcolumn = (column_t *)((byte *) patch + patch->columnofs[texturecolumn]);
         R_DrawMaskedColumn(tcolumn);
      }
   }
   else
   {
      for(column.x = vis->x1; column.x <= vis->x2; column.x++, frac += vis->xstep)
      {
         texturecolumn = (int)frac;
         
         // haleyjd 09/16/07: Cardboard requires this rangecheck, made nonfatal
         if(texturecolumn < 0 || texturecolumn >= w)
            continue;
         
         tcolumn = (column_t *)((byte *) patch + patch->columnofs[texturecolumn]);
         R_DrawMaskedColumn(tcolumn);
      }
   }
   colfunc = r_column_engine->DrawColumn; // killough 3/14/98
}

struct spritepos_t
{
   fixed_t x, y, z;
};

// ioanch 20160109: added offset arguments
static void R_interpolateThingPosition(const Mobj *thing, spritepos_t &pos)
{
   if(view.lerp == FRACUNIT)
   {
      pos.x = thing->x;
      pos.y = thing->y;
      pos.z = thing->z;
   }
   else
   {
      pos.x = lerpCoord(view.lerp, thing->prevpos.x, thing->x);
      pos.y = lerpCoord(view.lerp, thing->prevpos.y, thing->y);
      pos.z = lerpCoord(view.lerp, thing->prevpos.z, thing->z);
   }
}

static void R_interpolatePSpritePosition(const pspdef_t &pspr, v2fixed_t &pos)
{
   if(view.lerp == FRACUNIT)
   {
      pos.x = pspr.sx;
      pos.y = pspr.sy;
   }
   else
   {
      pos.x = lerpCoord(view.lerp, pspr.prevpos.x, pspr.sx);
      pos.y = lerpCoord(view.lerp, pspr.prevpos.y, pspr.sy);
   }
}

//
// R_ProjectSprite
//
// Generates a vissprite for a thing if it might be visible.
// ioanch 20160109: added optional arguments for offsetting the sprite
//
static void R_ProjectSprite(Mobj *thing, v3fixed_t *delta = nullptr,
                            const line_t *portalline = nullptr)
{
   spritepos_t    spritepos;
   fixed_t        gzt;            // killough 3/27/98
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int            lump;
   bool           flip;
   vissprite_t   *vis;
   int            heightsec;      // killough 3/27/98
   sector_t      *sec;            // haleyjd: for interpolation

   float tempx, tempy;
   float rotx, roty;
   float distxscale, distyscale;
   float tx1, tx2, tz1, tz2;
   float idist;
   float swidth, stopoffset, sleftoffset;
   float x1, x2, y1, y2;
   float pstep = 0.0f;
   int   intx1, intx2;

   // haleyjd 04/18/99: MF2_DONTDRAW
   //         09/01/02: zdoom-style translucency
   if((thing->flags2 & MF2_DONTDRAW) || !thing->translucency)
      return; // don't generate vissprite

   // haleyjd 01/05/14: interpolate thing positions
   // ioanch 20160109: portal rendering
   R_interpolateThingPosition(thing, spritepos);
   if(delta)
   {
      spritepos.x += delta->x;
      spritepos.y += delta->y;
      spritepos.z += delta->z;
   }

   // SoM: Cardboard translate the mobj coords and just project the sprite.
   tempx = M_FixedToFloat(spritepos.x) - view.x;
   tempy = M_FixedToFloat(spritepos.y) - view.y;
   roty  = (tempy * view.cos) + (tempx * view.sin);

   // lies in front of the front view plane
   if(roty < 1.0f)
      return;

   // ioanch 20160125: reject sprites in front of portal line when rendering
   // line portal
   if(portalrender.w && portalrender.w->portal &&
      portalrender.w->portal->type != R_SKYBOX)
   {
      const renderbarrier_t &barrier = portalrender.w->barrier;
      if(portalrender.w->line && portalrender.w->line != portalline &&
         P_PointOnDivlineSide(spritepos.x, spritepos.y, &barrier.dln.dl) == 0)
      {
         return;
      }
      if(!portalrender.w->line)
      {
         dlnormal_t dl1, dl2;
         if(R_PickNearestBoxLines(barrier.bbox, dl1, dl2) &&
            (P_PointOnDivlineSide(spritepos.x, spritepos.y, &dl1.dl) == 0 ||
               (dl2.dl.x != D_MAXINT && 
                  P_PointOnDivlineSide(spritepos.x, spritepos.y, &dl2.dl) == 0)))
         {
            return;
         }
      }
   }

   rotx = (tempx * view.cos) - (tempy * view.sin);

   // decide which patch to use for sprite relative to player
   if((unsigned int)thing->sprite >= (unsigned int)numsprites)
   {
      // haleyjd 08/12/02: modified error handling
      doom_printf(FC_ERROR "Bad sprite number %i\n", thing->sprite);

      // blank the thing's state sprite and frame so that this error does not
      // occur perpetually, flooding the message widget and console.
      if(thing->state)
      {
         thing->state->sprite = blankSpriteNum;
         thing->state->frame  = 0;
      }
      thing->sprite = blankSpriteNum;
      thing->frame  = 0;
      return;
   }

   sprdef = &sprites[thing->sprite];
   
   if(((thing->frame&FF_FRAMEMASK) >= sprdef->numframes) ||
      !(sprdef->spriteframes))
   {
      // haleyjd 08/12/02: modified error handling
      doom_printf(FC_ERROR "Bad frame %i for sprite %s",
                  thing->frame & FF_FRAMEMASK, 
                  spritelist[thing->sprite]);
      if(thing->state)
      {
         thing->state->sprite = blankSpriteNum;
         thing->state->frame  = 0;
      }
      thing->sprite = blankSpriteNum;
      thing->frame  = 0;
      return;
   }

   sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];
   
   if(sprframe->rotate)
   {
      // SoM: Use old rotation code
      // choose a different rotation based on player view
      angle_t ang = R_PointToAngle(spritepos.x, spritepos.y);
      unsigned int rot = (ang - thing->angle + (unsigned int)(ANG45/2)*9) >> 29;
      lump = sprframe->lump[rot];
      flip = !!sprframe->flip[rot];
   }
   else
   {
      // use single rotation for all views
      lump = sprframe->lump[0];
      flip = !!sprframe->flip[0];
   }


   // Calculate the edges of the shape
   swidth      = M_FixedToFloat(spritewidth[lump]);
   stopoffset  = M_FixedToFloat(spritetopoffset[lump]);
   sleftoffset = M_FixedToFloat(spriteoffset[lump]);


   tx1 = rotx - (flip ? swidth - sleftoffset : sleftoffset) * thing->xscale;
   tx2 = tx1 + swidth * thing->xscale;

   idist = 1.0f / roty;
   distxscale = idist * view.xfoc;

   x1 = view.xcenter + (tx1 * distxscale);
   if(x1 >= view.width)
      return;

   x2 = view.xcenter + (tx2 * distxscale);
   if(x2 < 0.0f)
      return;

   intx1 = (int)(x1 + 0.999f);
   intx2 = (int)(x2 - 0.001f);

   distyscale = idist * view.yfoc;
   // SoM: forgot about footclipping
   tz1 = thing->yscale * stopoffset + M_FixedToFloat(spritepos.z - thing->floorclip) - view.z;
   y1  = view.ycenter - (tz1 * distyscale);
   if(y1 >= view.height)
      return;

   tz2 = tz1 - spriteheight[lump] * thing->yscale;
   y2  = view.ycenter - (tz2 * distyscale) - 1.0f;
   if(y2 < 0.0f)
      return;

   if(x2 >= x1)
      pstep = 1.0f / (x2 - x1 + 1.0f);

   // Cardboard
   // SoM: Block of old code that stays
   gzt = spritepos.z + (fixed_t)(spritetopoffset[lump] * thing->yscale);

   // killough 3/27/98: exclude things totally separated
   // from the viewer, by either water or fake ceilings
   // killough 4/11/98: improve sprite clipping for underwater/fake ceilings

   // ioanch 20160109: offset sprites always use the R_PointInSubsector
   sec = (view.lerp == FRACUNIT && !delta ? thing->subsector->sector :
          R_PointInSubsector(spritepos.x, spritepos.y)->sector);
   heightsec = sec->heightsec;
   
   if(heightsec != -1) // only clip things which are in special sectors
   {
      auto &hsec = sectors[heightsec];
      int   phs  = view.sector->heightsec;
      
      if(phs != -1 && viewz < sectors[phs].floorheight ?
         thing->z >= hsec.floorheight : gzt < hsec.floorheight)
         return;
      if(phs != -1 && viewz > sectors[phs].ceilingheight ?
         gzt < hsec.ceilingheight && viewz >= hsec.ceilingheight :
         thing->z >= hsec.ceilingheight)
         return;
   }

   // store information in a vissprite
   vis = R_NewVisSprite();
   
   // killough 3/27/98: save sector for special clipping later   
   vis->heightsec = heightsec;
   
   vis->colour = thing->colour;
   vis->gx     = spritepos.x;
   vis->gy     = spritepos.y;
   vis->gz     = spritepos.z;
   vis->gzt    = gzt;                          // killough 3/27/98

   // Cardboard
   vis->x1 = x1 < 0.0f ? 0 : intx1;
   vis->x2 = x2 >= view.width ? viewwindow.width - 1 : intx2;

   vis->xstep = flip ? -(swidth * pstep) : swidth * pstep;
   vis->startx = flip ? swidth - 1.0f : 0.0f;

   vis->dist = idist;
   vis->scale = distyscale * thing->yscale;

   vis->ytop = y1;
   vis->ybottom = y2;
   vis->sector = int(sec - sectors); // haleyjd: use interpolated sector

   //if(x1 < vis->x1)
      vis->startx += vis->xstep * (vis->x1 - x1);

   // haleyjd 09/01/02
   vis->translucency = uint16_t(thing->translucency - 1);
   vis->tranmaplump = -1;

   // haleyjd 11/14/02: ghost flag
   if(thing->flags3 & MF3_GHOST && vis->translucency == FRACUNIT - 1)
      vis->translucency = HTIC_GHOST_TRANS - 1;

   // haleyjd 10/12/02: foot clipping
   vis->footclip = thing->floorclip;

   // haleyjd: moved this down, added footclip term
   // This needs to be scaled down (?) I don't get why this works...
   vis->texturemid = (fixed_t)((gzt - viewz - vis->footclip) / thing->yscale);

   vis->patch = lump;

   // get light level
   if(thing->flags & MF_SHADOW)     // sf
      vis->colormap = colormaps[global_cmap_index]; // haleyjd: NGCS -- was 0
   else if(fixedcolormap)
      vis->colormap = fixedcolormap;      // fixed map
   else if(LevelInfo.useFullBright && IS_FULLBRIGHT(thing)) // haleyjd
      vis->colormap = fullcolormap;       // full bright  // killough 3/20/98
   else
   {     
      // diminished light
      // SoM: ANYRES
      int index = (int)(idist * 2560.0f);
      if(index >= MAXLIGHTSCALE)
         index = MAXLIGHTSCALE-1;
      vis->colormap = spritelights[index];
   }

   vis->drawstyle = VS_DRAWSTYLE_NORMAL;

   // haleyjd 01/22/11: determine special drawstyles
   if(thing->flags & MF_SHADOW)
      vis->drawstyle = VS_DRAWSTYLE_SHADOW;
   else if(general_translucency)
   {
      if(thing->tranmap >= 0)
      {
         vis->drawstyle = VS_DRAWSTYLE_TRANMAP;
         vis->tranmaplump = thing->tranmap;
      }
      else if(thing->flags3 & MF3_TLSTYLEADD)
         vis->drawstyle = VS_DRAWSTYLE_ADD;
      else if(thing->flags4 & MF4_TLSTYLESUB)
         vis->drawstyle = VS_DRAWSTYLE_SUB;
      else if(vis->translucency < FRACUNIT - 1)
         vis->drawstyle = VS_DRAWSTYLE_ALPHA;
      else if(thing->flags & MF_TRANSLUCENT)
         vis->drawstyle = VS_DRAWSTYLE_TRANMAP;
   }
}

//
// R_AddSprites
//
// During BSP traversal, this adds sprites by sector.
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
//
void R_AddSprites(sector_t* sec, int lightlevel)
{
   Mobj *thing;
   int    lightnum;
   
   // BSP is traversed by subsector.
   // A sector might have been split into several
   //  subsectors during BSP building.
   // Thus we check whether its already added.

   if(sec->validcount == validcount)
      return;
   
   // Well, now it will be done.
   sec->validcount = validcount;
   
   lightnum = (lightlevel >> LIGHTSEGSHIFT)+(extralight * LIGHTBRIGHT);
   
   if(lightnum < 0)
      spritelights = scalelight[0];
   else if(lightnum >= LIGHTLEVELS)
      spritelights = scalelight[LIGHTLEVELS-1];
   else
      spritelights = scalelight[lightnum];
   
   // Handle all things in sector.
   
   for(thing = sec->thinglist; thing; thing = thing->snext)
      R_ProjectSprite(thing);

   // ioanch 20160109: handle partial sprite projections
   for(auto item = sec->spriteproj; item; item = item->dllNext)
      if(!((*item)->mobj->intflags & MIF_HIDDENBYQUAKE))
         R_ProjectSprite((*item)->mobj, &(*item)->delta, (*item)->portalline);

   // haleyjd 02/20/04: Handle all particles in sector.

   if(drawparticles)
   {
      DLListItem<particle_t> *link;

      for(link = sec->ptcllist; link; link = link->dllNext)
         R_ProjectParticle(*link);
   }
}

//
// R_DrawPSprite
//
// Draws player gun sprites.
//
static void R_DrawPSprite(pspdef_t *psp)
{
   float         tx;
   float         x1, x2, w;
   float         oldycenter;
   
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int            lump;
   bool           flip;
   vissprite_t   *vis;
   vissprite_t    avis;
   
   // haleyjd: total invis. psprite disable
   
   if(viewplayer->mo->flags2 & MF2_DONTDRAW)
      return;
   
   // decide which patch to use
   // haleyjd 08/14/02: should not be rangecheck, modified error
   // handling
   
   if((unsigned int)psp->state->sprite >= (unsigned int)numsprites)
   {
      doom_printf(FC_ERROR "Bad sprite number %i", 
                  psp->state->sprite);
      psp->state->sprite = blankSpriteNum;
      psp->state->frame = 0;
   }

   sprdef = &sprites[psp->state->sprite];
   
   if(((psp->state->frame&FF_FRAMEMASK) >= sprdef->numframes) ||
      !(sprdef->spriteframes))
   {
      doom_printf(FC_ERROR "Bad frame %d for sprite %s",
                 (int)(psp->state->frame & FF_FRAMEMASK), 
                 spritelist[psp->state->sprite]);
      psp->state->sprite = blankSpriteNum;
      psp->state->frame = 0;
      // reset sprdef
      sprdef = &sprites[psp->state->sprite];
   }

   sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];
   
   lump = sprframe->lump[0];
   flip = !!(sprframe->flip[0] ^ lefthanded);
   
   // calculate edges of the shape
   v2fixed_t pspos;
   R_interpolatePSpritePosition(*psp, pspos);

   tx  = M_FixedToFloat(pspos.x) - 160.0f;
   tx -= M_FixedToFloat(spriteoffset[lump]);

      // haleyjd
   w = (float)(spritewidth[lump] >> FRACBITS);

   x1 = (view.xcenter + tx * view.pspritexscale);
   
   // off the right side
   if(x1 > viewwindow.width)
      return;
   
   tx += w;
   x2  = (view.xcenter + tx * view.pspritexscale) - 1.0f;
   
   // off the left side
   if(x2 < 0)
      return;
 
   if(lefthanded)
   {
      float tmpx = x1;
      x1 = view.width - x2;
      x2 = view.width - tmpx;    // viewwindow.width-x1
   }
   
   // store information in a vissprite
   vis = &avis;
   
   // killough 12/98: fix psprite positioning problem
   vis->texturemid = (BASEYCENTER<<FRACBITS) /* + FRACUNIT/2 */ -
                      (pspos.y - spritetopoffset[lump]);

   vis->x1           = x1 < 0.0f ? 0 : (int)x1;
   vis->x2           = x2 >= view.width ? viewwindow.width - 1 : (int)x2;
   vis->colour       = 0;      // sf: default colourmap
   vis->translucency = FRACUNIT - 1; // haleyjd: default zdoom trans.
   vis->tranmaplump  = -1;
   vis->footclip     = 0; // haleyjd
   vis->scale        = view.pspriteyscale;
   vis->ytop         = (view.height * 0.5f) - (M_FixedToFloat(vis->texturemid) * vis->scale);
   vis->ybottom      = vis->ytop + (spriteheight[lump] * vis->scale);
   vis->sector       = int(view.sector - sectors);

   // haleyjd 07/01/07: use actual pixel range to scale graphic
   if(flip)
   {
      vis->xstep  = -(w / (x2 - x1 + 1.0f));
      vis->startx = (float)(spritewidth[lump] >> FRACBITS) - 1.0f;
   }
   else
   {
      vis->xstep  = w / (x2 - x1 + 1.0f);
      vis->startx = 0.0f;
   }
   
   if(vis->x1 > x1)
      vis->startx += vis->xstep * (vis->x1-x1);
   
   vis->patch = lump;

   vis->drawstyle = VS_DRAWSTYLE_NORMAL;
   
   if(viewplayer->powers[pw_invisibility] > 4*32 || 
      viewplayer->powers[pw_invisibility] & 8)
   {
      // sf: shadow draw now detected by flags
      vis->drawstyle = VS_DRAWSTYLE_SHADOW;         // shadow draw
      vis->colormap  = colormaps[global_cmap_index]; // haleyjd: NGCS -- was 0
   }
   else if((viewplayer->powers[pw_ghost] > 4*32 || // haleyjd: ghost
            viewplayer->powers[pw_ghost] & 8) &&
           general_translucency)
   {
      vis->drawstyle    = VS_DRAWSTYLE_ALPHA;
      vis->translucency = HTIC_GHOST_TRANS - 1;
      vis->colormap     = spritelights[MAXLIGHTSCALE-1];
   }
   else if(fixedcolormap)
      vis->colormap = fixedcolormap;           // fixed color
   else if(psp->state->frame & FF_FULLBRIGHT)
      vis->colormap = fullcolormap;            // full bright // killough 3/20/98
   else
      vis->colormap = spritelights[MAXLIGHTSCALE-1];  // local light
   
   if(psp->trans && general_translucency) // translucent gunflash
      vis->drawstyle = VS_DRAWSTYLE_TRANMAP;

   oldycenter = view.ycenter;
   view.ycenter = (view.height * 0.5f);
   
   R_DrawVisSprite(vis, vis->x1, vis->x2);
   
   view.ycenter = oldycenter;
}

//
// R_DrawPlayerSprites
//
static void R_DrawPlayerSprites()
{
   int i, lightnum;
   pspdef_t *psp;
   sector_t tmpsec;
   int floorlightlevel, ceilinglightlevel;
   
   // sf: psprite switch
   if(!showpsprites || viewcamera) return;
   
   R_SectorColormap(view.sector);

   // get light level
   // killough 9/18/98: compute lightlevel from floor and ceiling lightlevels
   // (see r_bsp.c for similar calculations for non-player sprites)

   R_FakeFlat(view.sector, &tmpsec, &floorlightlevel, &ceilinglightlevel, 0);
   lightnum = ((floorlightlevel+ceilinglightlevel) >> (LIGHTSEGSHIFT+1)) 
                 + (extralight * LIGHTBRIGHT);

   if(lightnum < 0)
      spritelights = scalelight[0];
   else if(lightnum >= LIGHTLEVELS)
      spritelights = scalelight[LIGHTLEVELS-1];
   else
      spritelights = scalelight[lightnum];

   for(i = 0; i < viewwindow.width; ++i)
      pscreenheightarray[i] = view.height - 1.0f;
   
   // clip to screen bounds
   mfloorclip   = pscreenheightarray;
   mceilingclip = zeroarray;

   // add all active psprites
   for(i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
      if(psp->state)
         R_DrawPSprite(psp);
}

#define bcopyp(d, s, n) memcpy(d, s, (n) * sizeof(void *))

//
// killough 9/2/98: merge sort
//
static void msort(vissprite_t **s, vissprite_t **t, int n)
{
   if(n >= 16)
   {
      int n1 = n/2, n2 = n - n1;
      vissprite_t **s1 = s, **s2 = s + n1, **d = t;

      msort(s1, t, n1);
      msort(s2, t, n2);
      
      while((*s1)->dist > (*s2)->dist ?
            (*d++ = *s1++, --n1) : (*d++ = *s2++, --n2));

      if(n2)
         bcopyp(d, s2, n2);
      else
         bcopyp(d, s1, n1);
      
      bcopyp(s, t, n);
   }
   else
   {
      int i;
      for(i = 1; i < n; i++)
      {
         vissprite_t *temp = s[i];
         if(s[i-1]->dist < temp->dist)
         {
            int j = i;
            while((s[j] = s[j-1])->dist < temp->dist && --j);
            s[j] = temp;
         }
      }
   }
}

#if 0
//
// R_SortVisSprites
//
// Rewritten by Lee Killough to avoid using unnecessary
// linked lists, and to use faster sorting algorithm.
//
static void R_SortVisSprites()
{
   if(num_vissprite)
   {
      int i = num_vissprite;
      
      // If we need to allocate more pointers for the vissprites,
      // allocate as many as were allocated for sprites -- killough
      // killough 9/22/98: allocate twice as many
      
      if(num_vissprite_ptrs < num_vissprite*2)
      {
         efree(vissprite_ptrs);  // better than realloc -- no preserving needed
         num_vissprite_ptrs = num_vissprite_alloc * 2;
         vissprite_ptrs = emalloc(vissprite_t **, 
                                  num_vissprite_ptrs * sizeof *vissprite_ptrs);
      }

      while(--i >= 0)
         vissprite_ptrs[i] = vissprites+i;

      // killough 9/22/98: replace qsort with merge sort, since the keys
      // are roughly in order to begin with, due to BSP rendering.
      
      msort(vissprite_ptrs, vissprite_ptrs + num_vissprite, num_vissprite);
   }
}
#endif

//
// R_SortVisSpriteRange
//
// Sorts only a subset of the vissprites, for portal rendering.
//
static void R_SortVisSpriteRange(int first, int last)
{
   unsigned int numsprites = last - first;
   
   if(numsprites > 0)
   {
      int i = numsprites;
      
      // If we need to allocate more pointers for the vissprites,
      // allocate as many as were allocated for sprites -- killough
      // killough 9/22/98: allocate twice as many
      
      if(num_vissprite_ptrs < numsprites*2)
      {
         efree(vissprite_ptrs);  // better than realloc -- no preserving needed
         num_vissprite_ptrs = num_vissprite_alloc * 2;
         vissprite_ptrs = emalloc(vissprite_t **, 
                                  num_vissprite_ptrs * sizeof *vissprite_ptrs);
      }

      while(--i >= 0)
         vissprite_ptrs[i] = vissprites+i+first;

      // killough 9/22/98: replace qsort with merge sort, since the keys
      // are roughly in order to begin with, due to BSP rendering.
      
      msort(vissprite_ptrs, vissprite_ptrs + numsprites, numsprites);
   }
}

//
// R_DrawSpriteInDSRange
//
// Draws a sprite within a given drawseg range, for portals.
//
static void R_DrawSpriteInDSRange(vissprite_t *spr, int firstds, int lastds)
{
   drawseg_t *ds;
   int        x;
   int        r1;
   int        r2;
   float      dist;
   float      fardist;

   for(x = spr->x1; x <= spr->x2; x++)
      clipbot[x] = cliptop[x] = CLIP_UNDEF;

   // haleyjd 04/25/10:
   // e6y: optimization
   if(drawsegs_xrange_count)
   {
      drawsegs_xrange_t *dsx = drawsegs_xrange;

      // drawsegs_xrange is sorted by ::x1
      // haleyjd: way faster to use a pointer here
      while((ds = dsx->user))
      {
         // determine if the drawseg obscures the sprite
         if(dsx->x1 > spr->x2 || dsx->x2 < spr->x1)
         {
            ++dsx;
            continue;      // does not cover sprite
         }
         ++dsx;

         if(ds->dist1 > ds->dist2)
         {
            fardist = ds->dist2;
            dist = ds->dist1;
         }
         else
         {
            fardist = ds->dist1;
            dist = ds->dist2;
         }

         if(dist < spr->dist || (fardist < spr->dist &&
            !R_PointOnSegSide(spr->gx, spr->gy, ds->curline)))
         {
            if(ds->maskedtexturecol) // masked mid texture?
            {
               r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
               r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
               R_RenderMaskedSegRange(ds, r1, r2);
            }
            continue;                // seg is behind sprite
         }

         r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
         r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

         // clip this piece of the sprite
         // killough 3/27/98: optimized and made much shorter

         // bottom sil
         if(ds->silhouette & SIL_BOTTOM && spr->gz < ds->bsilheight)
         {
            for(x = r1; x <= r2; x++)
            {
               if(clipbot[x] == CLIP_UNDEF)
                  clipbot[x] = ds->sprbottomclip[x];
            }
         }

         // top sil
         if(ds->silhouette & SIL_TOP && spr->gzt > ds->tsilheight)
         {
            for(x = r1; x <= r2; x++)
            {
               if(cliptop[x] == CLIP_UNDEF)
                  cliptop[x] = ds->sprtopclip[x];
            }
         }
      }
   }
   else
   {
      // Scan drawsegs from end to start for obscuring segs.
      // The first drawseg that has a greater scale is the clip seg.

      for(ds = drawsegs + lastds; ds-- > drawsegs + firstds; )
      {      
         // determine if the drawseg obscures the sprite
         if(ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
            (!ds->silhouette && !ds->maskedtexturecol))
            continue; // does not cover sprite

         r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
         r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

         if (ds->dist1 > ds->dist2)
         {
            fardist = ds->dist2;
            dist = ds->dist1;
         }
         else
         {
            fardist = ds->dist1;
            dist = ds->dist2;
         }

         if(dist < spr->dist || (fardist < spr->dist &&
            !R_PointOnSegSide(spr->gx, spr->gy, ds->curline)))
         {
            if(ds->maskedtexturecol) // masked mid texture?
               R_RenderMaskedSegRange(ds, r1, r2);
            continue;                // seg is behind sprite
         }

         // clip this piece of the sprite
         // killough 3/27/98: optimized and made much shorter

         // bottom sil
         if(ds->silhouette & SIL_BOTTOM && spr->gz < ds->bsilheight)
         {
            for(x = r1; x <= r2; x++)
            {
               if(clipbot[x] == CLIP_UNDEF)
                  clipbot[x] = ds->sprbottomclip[x];
            }
         }

         // top sil
         if(ds->silhouette & SIL_TOP && spr->gzt > ds->tsilheight)
         {
            for(x = r1; x <= r2; x++)
            {
               if(cliptop[x] == CLIP_UNDEF)
                  cliptop[x] = ds->sprtopclip[x];
            }
         }
      }
   }

   // Clip the sprite against deep water and/or fake ceilings.

   if(spr->heightsec != -1) // only things in specially marked sectors
   {
      float h, mh;
      
      int phs = view.sector->heightsec;

      mh = M_FixedToFloat(sectors[spr->heightsec].floorheight) - view.z;
      if(sectors[spr->heightsec].floorheight > spr->gz &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(mh <= 0.0 || (phs != -1 && viewz > sectors[phs].floorheight))
         {
            // clip bottom
            for(x = spr->x1; x <= spr->x2; x++)
            {
               if(clipbot[x] == CLIP_UNDEF || h < clipbot[x])
                  clipbot[x] = h;
            }
         }
         else  // clip top
         {
            if(phs != -1 && viewz <= sectors[phs].floorheight) // killough 11/98
            {
               for(x = spr->x1; x <= spr->x2; x++)
               {
                  if(cliptop[x] == CLIP_UNDEF || h > cliptop[x])
                     cliptop[x] = h;
               }
            }
         }
      }

      mh = M_FixedToFloat(sectors[spr->heightsec].ceilingheight) - view.z;
      if(sectors[spr->heightsec].ceilingheight < spr->gzt &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(phs != -1 && viewz >= sectors[phs].ceilingheight)
         {
            // clip bottom
            for(x = spr->x1; x <= spr->x2; x++)
            {
               if(clipbot[x] == CLIP_UNDEF || h < clipbot[x])
                  clipbot[x] = h;
            }
         }
         else  // clip top
         {
            for(x = spr->x1; x <= spr->x2; x++)
            {
               if(cliptop[x] == CLIP_UNDEF || h > cliptop[x])
                  cliptop[x] = h;
            }
         }
      }
   }
   // killough 3/27/98: end special clipping for deep water / fake ceilings

   // SoM: special clipping for linked portals
   if(useportalgroups)
   {
      float h, mh;

      sector_t *sector = sectors + spr->sector;

      mh = M_FixedToFloat(sector->floorheight) - view.z;
      if(sector->f_pflags & PS_PASSABLE && sector->floorheight > spr->gz)
      {
         h = eclamp(view.ycenter - (mh * spr->scale), 0.0f, view.height - 1);

         for(x = spr->x1; x <= spr->x2; x++)
         {
            if(clipbot[x] == CLIP_UNDEF || h < clipbot[x])
               clipbot[x] = h;
         }
      }

      mh = M_FixedToFloat(sector->ceilingheight) - view.z;
      if(sector->c_pflags & PS_PASSABLE && sector->ceilingheight < spr->gzt)
      {
         h = eclamp(view.ycenter - (mh * spr->scale), 0.0f, view.height - 1);

         for(x = spr->x1; x <= spr->x2; x++)
         {
            if(cliptop[x] == CLIP_UNDEF || h > cliptop[x])
               cliptop[x] = h;
         }
      }
   }

   // all clipping has been performed, so draw the sprite
   // check for unclipped columns
   
   for(x = spr->x1; x <= spr->x2; x++)
   {
      if(clipbot[x] == CLIP_UNDEF || clipbot[x] > pbottom[x])
         clipbot[x] = pbottom[x];

      if(cliptop[x] == CLIP_UNDEF || cliptop[x] < ptop[x])
         cliptop[x] = ptop[x];
   }

   mfloorclip   = clipbot;
   mceilingclip = cliptop;
   R_DrawVisSprite(spr, spr->x1, spr->x2);
}

//
// R_DrawPostBSP
//
// Draws the items in the Post-BSP stack.
//
void R_DrawPostBSP()
{
   maskedrange_t *masked;
   drawseg_t     *ds;
   int           firstds, lastds, firstsprite, lastsprite;
 
   while(pstacksize > 0)
   {
      --pstacksize;

      if((masked = pstack[pstacksize].masked))
      {
         firstds     = masked->firstds;
         lastds      = masked->lastds;
         firstsprite = masked->firstsprite;
         lastsprite  = masked->lastsprite;

         if(lastsprite > firstsprite)
         {
            R_SortVisSpriteRange(firstsprite, lastsprite);

            // haleyjd 04/25/10: 
            // e6y
            // Reducing of cache misses in the following R_DrawSprite()
            // Makes sense for scenes with huge amount of drawsegs.
            // ~12% of speed improvement on epic.wad map05
            drawsegs_xrange_count = 0;
            if(num_vissprite > 0)
            {
               if(drawsegs_xrange_size <= maxdrawsegs+1)
               {
                  // haleyjd: fix reallocation to track 2x size
                  drawsegs_xrange_size =  2 * (maxdrawsegs+1);
                  drawsegs_xrange = 
                     erealloc(drawsegs_xrange_t *, drawsegs_xrange, 
                              drawsegs_xrange_size * sizeof(*drawsegs_xrange));
               }
               for(ds = drawsegs + lastds; ds-- > drawsegs + firstds; )
               {
                  if (ds->silhouette || ds->maskedtexturecol)
                  {
                     drawsegs_xrange[drawsegs_xrange_count].x1   = ds->x1;
                     drawsegs_xrange[drawsegs_xrange_count].x2   = ds->x2;
                     drawsegs_xrange[drawsegs_xrange_count].user = ds;
                     drawsegs_xrange_count++;
                  }
               }
               // haleyjd: terminate with a NULL user for faster loop - adds ~3 FPS
               drawsegs_xrange[drawsegs_xrange_count].user = NULL;
            }

            ptop    = masked->ceilingclip;
            pbottom = masked->floorclip;

            for(int i = lastsprite - firstsprite; --i >= 0; )
               R_DrawSpriteInDSRange(vissprite_ptrs[i], firstds, lastds);         // killough
         }

         // render any remaining masked mid textures

         // Modified by Lee Killough:
         // (pointer check was originally nonportable
         // and buggy, by going past LEFT end of array):

         for(ds = drawsegs + lastds; ds-- > drawsegs + firstds; )  // new -- killough
         {
            if(ds->maskedtexturecol)
               R_RenderMaskedSegRange(ds, ds->x1, ds->x2);
         }
         
         // Done with the masked range
         pstack[pstacksize].masked = NULL;
         masked->next = unusedmasked;
         unusedmasked = masked;
         
         masked = NULL;
      }       
      
      if(pstack[pstacksize].overlay)
      {
         // haleyjd 09/04/06: handle through column engine
         if(r_column_engine->ResetBuffer)
            r_column_engine->ResetBuffer();
            
         R_DrawPlanes(pstack[pstacksize].overlay);
         R_FreeOverlaySet(pstack[pstacksize].overlay);
      }
   }

   // draw the psprites on top of everything
   //  but does not draw on side views
   if(!viewangleoffset)
      R_DrawPlayerSprites();
}

///////////////////////////////////////////////////////////////////////////////
//
// ioanch 20160109: sprite projection through sector portals
//

// recycle bin of spriteproj objects
static DLList<spriteprojnode_t, &spriteprojnode_t::freelink> spriteprojfree;

//
// R_freeProjNode
//
// Clears all data and unlinks from important lists, putting it in the free bin
//
inline static void R_freeProjNode(spriteprojnode_t *node)
{
   node->mobj = nullptr;
   node->sector = nullptr;
   node->portalline = nullptr;
   memset(&node->delta, 0, sizeof(node->delta));
   node->mobjlink.remove();
   node->sectlink.remove();
   spriteprojfree.insert(node);
}

//
// R_RemoveMobjProjections
//
// Removes the chain from a mobj
//
void R_RemoveMobjProjections(Mobj *mobj)
{
   DLListItem<spriteprojnode_t> *proj, *next;
   for(proj = mobj->spriteproj; proj; proj = next)
   {
      next = proj->dllNext;
      R_freeProjNode(proj->dllObject);
   }
   mobj->spriteproj = nullptr;
}

//
// R_newProjNode
//
// Allocates a new node or picks a free one
//
static spriteprojnode_t *R_newProjNode()
{
   if(spriteprojfree.head)
   {
      auto ret = spriteprojfree.head;
      ret->remove();
      return ret->dllObject;
   }
   return estructalloc(spriteprojnode_t, 1);
}

//
// R_addProjNode
//
// Helper function for below. Returns the next sector
//
inline static sector_t *R_addProjNode(Mobj *mobj, const linkdata_t *data,
                                      v3fixed_t &delta,
                                      DLListItem<spriteprojnode_t> *&item,
                                      DLListItem<spriteprojnode_t> **&tail,
                                      const line_t *line)
{
   sector_t *sector;

   delta.x += data->deltax;
   delta.y += data->deltay;
   delta.z += data->deltaz;
   sector = R_PointInSubsector(mobj->x + delta.x, mobj->y + delta.y)->sector;
   if(!item)
   {
      // no more items in the list: then simply add them, without
      spriteprojnode_t *newnode = R_newProjNode();
      newnode->delta = delta;
      newnode->mobj = mobj;
      newnode->sector = sector;
      newnode->portalline = line;
      newnode->mobjlink.insert(newnode, tail);
      newnode->sectlink.insert(newnode, &sector->spriteproj);
      tail = &newnode->mobjlink.dllNext;
   }
   else
   {
      if((*item)->sector != sector)
      {
         (*item)->sectlink.remove();
         (*item)->sector = sector;
         (*item)->sectlink.insert((*item), &sector->spriteproj);
      }
      (*item)->delta = delta;
      (*item)->portalline = line;
      tail = &item->dllNext;
      item = item->dllNext;
      
   }
   
   return sector;
}

//
// Data passed through iterator
//
struct mobjprojinfo_t
{
   Mobj *mobj;
   fixed_t bbox[4];
   DLListItem<spriteprojnode_t> **item;
   DLListItem<spriteprojnode_t> ***tail;
   fixed_t scaledbottom, scaledtop;
};

//
// Iterator called by R_CheckMobjProjection
//
static bool RIT_checkMobjProjection(const line_t &line, void *vdata)
{
   const auto &mpi = *static_cast<mobjprojinfo_t *>(vdata);
   if(line.bbox[BOXLEFT] >= mpi.bbox[BOXRIGHT] ||
      line.bbox[BOXBOTTOM] >= mpi.bbox[BOXTOP] ||
      line.bbox[BOXRIGHT] <= mpi.bbox[BOXLEFT] ||
      line.bbox[BOXTOP] <= mpi.bbox[BOXBOTTOM] ||
      P_PointOnLineSide(mpi.mobj->x, mpi.mobj->y, &line) == 1 ||
      P_BoxOnLineSide(mpi.bbox, &line) != -1 ||
      line.intflags & MLI_MOVINGPORTAL)
   {
      return true;
   }

   const linkdata_t *data = nullptr, *data2 = nullptr;

   if(line.pflags & PS_PASSABLE)
      data = &line.portal->data.link;
   else
   {
      if(line.extflags & EX_ML_LOWERPORTAL &&
         line.backsector->f_pflags & PS_PASSABLE &&
         mpi.mobj->z + mpi.scaledbottom < line.backsector->floorheight)
      {
         data = &line.backsector->f_portal->data.link;
      }
      if(line.extflags & EX_ML_UPPERPORTAL &&
         line.backsector->c_pflags & PS_PASSABLE &&
         mpi.mobj->z + mpi.scaledtop > line.backsector->ceilingheight)
      {
         data2 = &line.backsector->c_portal->data.link;
      }
   }
   v3fixed_t v = { 0, 0, 0 };
   if(data)
      R_addProjNode(mpi.mobj, data, v, *mpi.item, *mpi.tail, &line);
   if(data2)
   {
      v.x = v.y = v.z = 0;
      R_addProjNode(mpi.mobj, data2, v, *mpi.item, *mpi.tail, &line);
   }
   return true;
}

//
// R_CheckMobjProjections
//
// Looks above and below for portals and prepares projection nodes
//
void R_CheckMobjProjections(Mobj *mobj, bool checklines)
{
   sector_t *sector = mobj->subsector->sector;

   bool overflown = (unsigned)mobj->sprite >= (unsigned)numsprites ||
   (mobj->frame & FF_FRAMEMASK) >= sprites[mobj->sprite].numframes;

   DLListItem<spriteprojnode_t> *item = mobj->spriteproj;

   if(mobj->flags & MF_NOSECTOR || overflown ||
      (!(sector->f_pflags & PS_PASSABLE) && !(sector->c_pflags & PS_PASSABLE) &&
       !checklines))
   {
      if(item)
         R_RemoveMobjProjections(mobj);
      return;
   }

   const linkdata_t *data;

   DLListItem<spriteprojnode_t> **tail = &mobj->spriteproj;

   const spritespan_t &span =
   r_spritespan[mobj->sprite][mobj->frame & FF_FRAMEMASK];

   fixed_t scaledtop = M_FloatToFixed(span.top * mobj->yscale + 0.5f);
   fixed_t scaledbottom = M_FloatToFixed(span.bottom * mobj->yscale - 0.5f);

   v3fixed_t delta = {0, 0, 0};
   int loopprot = 0;
   while(++loopprot < SECTOR_PORTAL_LOOP_PROTECTION && sector &&
         sector->f_pflags & PS_PASSABLE &&
         P_FloorPortalZ(*sector) > mobj->z + scaledbottom)
   {
      // always accept first sector
      data = R_FPLink(sector);
      sector = R_addProjNode(mobj, data, delta, item, tail, nullptr);
   }

   // restart from mobj's group
   sector = mobj->subsector->sector;
   delta.x = delta.y = delta.z = 0;
   while(++loopprot < SECTOR_PORTAL_LOOP_PROTECTION && sector &&
         sector->c_pflags & PS_PASSABLE &&
         P_CeilingPortalZ(*sector) < mobj->z + scaledtop)
   {
      // always accept first sector
      data = R_CPLink(sector);
      sector = R_addProjNode(mobj, data, delta, item, tail, nullptr);
   }

   // Now check line portals
   pLPortalMap.newSession();
   mobjprojinfo_t mpi;
   fixed_t xspan = M_FloatToFixed(span.side * mobj->xscale);
   mpi.mobj = mobj;
   mpi.bbox[BOXLEFT] = mobj->x - xspan;
   mpi.bbox[BOXRIGHT] = mobj->x + xspan;
   mpi.bbox[BOXBOTTOM] = mobj->y - xspan;
   mpi.bbox[BOXTOP] = mobj->y + xspan;
   mpi.scaledbottom = scaledbottom;
   mpi.scaledtop = scaledtop;
   mpi.item = &item;
   mpi.tail = &tail;
   int bx1 = (mpi.bbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
   int bx2 = (mpi.bbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
   int by1 = (mpi.bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   int by2 = (mpi.bbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

   for(int by = by1; by <= by2; ++by)
      for(int bx = bx1; bx <= bx2; ++bx)
         pLPortalMap.iterator(bx, by, &mpi, RIT_checkMobjProjection);

   // remove trailing items
   DLListItem<spriteprojnode_t> *next;
   for(; item; item = next)
   {
      next = item->dllNext;
      R_freeProjNode(item->dllObject);
   }
}

//=============================================================================
//
// haleyjd 09/30/01
//
// Particle Rendering
// This incorporates itself mostly seamlessly within the vissprite system, 
// incurring only minor changes to the functions above.
//
// For code adapted from ZDoom:
//
// Copyright 1998-2012 Randy Heit  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions 
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

//
// newParticle
//
// Tries to find an inactive particle in the Particles list
// Returns NULL on failure
//
particle_t *newParticle()
{
   particle_t *result = NULL;
   if(inactiveParticles != -1)
   {
      result = Particles + inactiveParticles;
      inactiveParticles = result->next;
      result->next = activeParticles;
      activeParticles = int(result - Particles);
   }

   return result;
}

//
// R_InitParticles
//
// Allocate the particle list and initialize it
//
void R_InitParticles()
{
   int i;

   numParticles = 0;

   if((i = M_CheckParm("-numparticles")) && i < myargc - 1)
      numParticles = atoi(myargv[i+1]);
   
   if(numParticles == 0) // assume default
      numParticles = 4000;
   else if(numParticles < 100)
      numParticles = 100;
   
   Particles = (particle_t *)(Z_Malloc(numParticles*sizeof(particle_t), PU_STATIC, NULL));
   R_ClearParticles();
}

//
// R_ClearParticles
//
// set up the particle list
//
void R_ClearParticles()
{
   int i;
   
   memset(Particles, 0, numParticles*sizeof(particle_t));
   activeParticles = -1;
   inactiveParticles = 0;
   for(i = 0; i < numParticles - 1; i++)
      Particles[i].next = i + 1;
   Particles[i].next = -1;
}

//
// R_ProjectParticle
//
static void R_ProjectParticle(particle_t *particle)
{
   fixed_t gzt;
   int x1, x2;
   vissprite_t *vis;
   sector_t    *sector = NULL;
   int heightsec = -1;
   
   float tempx, tempy, ty1, tx1, tx2, tz;
   float idist, xscale, yscale;
   float y1, y2;

   // SoM: Cardboard translate the mobj coords and just project the sprite.
   tempx = M_FixedToFloat(particle->x) - view.x;
   tempy = M_FixedToFloat(particle->y) - view.y;
   ty1   = (tempy * view.cos) + (tempx * view.sin);

   // lies in front of the front view plane
   if(ty1 < 1.0f)
      return;

   // invisible?
   if(!particle->trans)
      return;

   tx1 = (tempx * view.cos) - (tempy * view.sin);

   tx2 = tx1 + 1.0f;

   idist = 1.0f / ty1;
   xscale = idist * view.xfoc;
   yscale = idist * view.yfoc;
   
   // calculate edges of the shape
   x1 = (int)(view.xcenter + (tx1 * xscale));
   x2 = (int)(view.xcenter + (tx2 * xscale));

   if(x2 < x1) x2 = x1;
   
   // off either side?
   if(x1 >= viewwindow.width || x2 < 0)
      return;

   tz = M_FixedToFloat(particle->z) - view.z;

   y1 = (view.ycenter - (tz * yscale));
   y2 = (view.ycenter - ((tz - 1.0f) * yscale));
   
   if(y2 < 0.0f || y1 >= view.height)
      return;
   
   gzt = particle->z + 1;
   
   // killough 3/27/98: exclude things totally separated
   // from the viewer, by either water or fake ceilings
   // killough 4/11/98: improve sprite clipping for underwater/fake ceilings
   
   {
      // haleyjd 02/20/04: use subsector now stored in particle
      subsector_t *subsector = particle->subsector;
      sector = subsector->sector;
      heightsec = sector->heightsec;

      if(particle->z < sector->floorheight || 
	 particle->z > sector->ceilingheight)
	 return;
   }
   
   // only clip particles which are in special sectors
   if(heightsec != -1)
   {
      int phs = view.sector->heightsec;
      
      if(phs != -1 && 
	 viewz < sectors[phs].floorheight ?
	 particle->z >= sectors[heightsec].floorheight :
         gzt < sectors[heightsec].floorheight)
         return;

      if(phs != -1 && 
	 viewz > sectors[phs].ceilingheight ?
	 gzt < sectors[heightsec].ceilingheight &&
	 viewz >= sectors[heightsec].ceilingheight :
         particle->z >= sectors[heightsec].ceilingheight)
         return;
   }
   
   // store information in a vissprite
   vis = R_NewVisSprite();
   vis->heightsec = heightsec;
   vis->gx = particle->x;
   vis->gy = particle->y;
   vis->gz = particle->z;
   vis->gzt = gzt;
   vis->texturemid = vis->gzt - viewz;
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= viewwindow.width ? viewwindow.width-1 : x2;
   vis->colour = particle->color;
   vis->patch = -1;
   vis->translucency = static_cast<uint16_t>(particle->trans - 1);
   vis->tranmaplump = -1;
   // Cardboard
   vis->dist = idist;
   vis->xstep = 1.0f / xscale;
   vis->ytop = y1;
   vis->ybottom = y2;
   vis->scale = yscale;
   vis->sector = int(sector - sectors);

   if(fixedcolormap ==
      fullcolormap + INVERSECOLORMAP*256*sizeof(lighttable_t))
   {
      vis->colormap = fixedcolormap;
   } 
   else
   {
      R_SectorColormap(sector);

      if(LevelInfo.useFullBright && (particle->styleflags & PS_FULLBRIGHT))
      {
         vis->colormap = fullcolormap;
      }
      else
      {
         lighttable_t **ltable;
         sector_t tmpsec;
         int floorlightlevel, ceilinglightlevel, lightnum, index;

         R_FakeFlat(sector, &tmpsec, &floorlightlevel, 
                    &ceilinglightlevel, false);

         lightnum = (floorlightlevel + ceilinglightlevel) / 2;
         lightnum = (lightnum >> LIGHTSEGSHIFT) + (extralight * LIGHTBRIGHT);
         
         if(lightnum >= LIGHTLEVELS || fixedcolormap)
            ltable = scalelight[LIGHTLEVELS - 1];      
         else if(lightnum < 0)
            ltable = scalelight[0];
         else
            ltable = scalelight[lightnum];
         
         index = (int)(idist * 2560.0f);
         if(index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE - 1;
         
         vis->colormap = ltable[index];
      }
   }
}

//
// R_DrawParticle
//
// haleyjd: this function had to be mostly rewritten
//
static void R_DrawParticle(vissprite_t *vis)
{
   int x1, x2, ox1, ox2;
   int yl, yh;
   byte color;

   ox1 = x1 = vis->x1;
   ox2 = x2 = vis->x2;

   if(x1 < 0)
      x1 = 0;
   if(x2 >= viewwindow.width)
      x2 = viewwindow.width - 1;

   // due to square shape, it is unnecessary to clip the entire
   // particle
   
   if(vis->ybottom > mfloorclip[ox1])
      vis->ybottom = mfloorclip[ox1];
   if(vis->ybottom > mfloorclip[ox2])
      vis->ybottom = mfloorclip[ox2];

   if(vis->ytop < mceilingclip[ox1])
      vis->ytop = mceilingclip[ox1];
   if(vis->ytop < mceilingclip[ox2])
      vis->ytop = mceilingclip[ox2];

   yl = (int)vis->ytop;
   yh = (int)vis->ybottom;

   color = vis->colormap[vis->colour];

   {
      int xcount, ycount, spacing;
      byte *dest;

      xcount = x2 - x1 + 1;
      
      ycount = yh - yl;      
      if(ycount < 0)
         return;
      ++ycount;

      spacing = video.pitch - xcount;
      dest    = R_ADDRESS(x1, yl);

      // haleyjd 02/08/05: rewritten to remove inner loop invariants
      if(general_translucency && particle_trans)
      {
         unsigned int bg, fg;
         unsigned int *fg2rgb, *bg2rgb;
         unsigned int fglevel, bglevel;

         // look up translucency information
         fglevel = ((unsigned int)(vis->translucency) + 1) & ~0x3ff;
         bglevel = FRACUNIT - fglevel;
         fg2rgb  = Col2RGB8[fglevel >> 10];
         bg2rgb  = Col2RGB8[bglevel >> 10];
         fg      = fg2rgb[color]; // foreground color is invariant

         do // step in y
         {
            int count = xcount;

            do // step in x
            {
               bg = bg2rgb[*dest];
               bg = (fg + bg) | 0x1f07c1f;
               *dest++ = RGB32k[0][0][bg & (bg >> 15)];
            } 
            while(--count);
            dest += spacing;  // go to next row
         } 
         while(--ycount);
      }
      else // opaque (fast, and looks terrible)
      {
         do // step in y
         {
            int count = xcount;
            
            do // step in x
               *dest++ = color;
            while(--count);
            dest += spacing;  // go to next row
         } 
         while(--ycount);
      } // end else [!general_translucency]
   } // end local block
}

//----------------------------------------------------------------------------
//
// $Log: r_things.c,v $
// Revision 1.22  1998/05/03  22:46:41  killough
// beautification
//
// Revision 1.21  1998/05/01  15:26:50  killough
// beautification
//
// Revision 1.20  1998/04/27  02:04:43  killough
// Fix incorrect I_Error format string
//
// Revision 1.19  1998/04/24  11:03:26  jim
// Fixed bug in sprites in PWAD
//
// Revision 1.18  1998/04/13  09:45:30  killough
// Fix sprite clipping under fake ceilings
//
// Revision 1.17  1998/04/12  02:02:19  killough
// Fix underwater sprite clipping, add wall translucency
//
// Revision 1.16  1998/04/09  13:18:48  killough
// minor optimization, plus fix ghost sprites due to huge z-height diffs
//
// Revision 1.15  1998/03/31  19:15:27  killough
// Fix underwater sprite clipping bug
//
// Revision 1.14  1998/03/28  18:15:29  killough
// Add true deep water / fake ceiling sprite clipping
//
// Revision 1.13  1998/03/23  03:41:43  killough
// Use 'fullcolormap' for fully-bright colormap
//
// Revision 1.12  1998/03/16  12:42:37  killough
// Optimize away some function pointers
//
// Revision 1.11  1998/03/09  07:28:16  killough
// Add primitive underwater support
//
// Revision 1.10  1998/03/02  11:48:59  killough
// Add failsafe against texture mapping overflow crashes
//
// Revision 1.9  1998/02/23  04:55:52  killough
// Remove some comments
//
// Revision 1.8  1998/02/20  22:53:22  phares
// Moved TRANMAP initialization to w_wad.c
//
// Revision 1.7  1998/02/20  21:56:37  phares
// Preliminarey sprite translucency
//
// Revision 1.6  1998/02/09  03:23:01  killough
// Change array decl to use MAX screen width/height
//
// Revision 1.5  1998/02/02  13:32:49  killough
// Performance tuning, program beautification
//
// Revision 1.4  1998/01/26  19:24:50  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  06:13:58  killough
// Performance tuning
//
// Revision 1.2  1998/01/23  20:28:14  jim
// Basic sprite/flat functionality in PWAD added
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
