//
// The Eternity Engine
// Copyright (C) 2018 James Haley et al.
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
// Purpose: Refresh of things represented by sprites i.e. map objects and particles.
//          Particle code largely from ZDoom, thanks to Marisa Heit.
// Authors: Stephen McGranahan, James Haley, Ioan Chera, Max Waine
//

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
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
#include "r_context.h"
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

extern int global_cmap_index; // haleyjd: NGCS

//=============================================================================
// 
// Defines

#define MINZ        (FRACUNIT*4)
#define BASEYCENTER 100

#define IS_FULLBRIGHT(actor) \
   (((actor)->frame & FF_FULLBRIGHT) || ((actor)->flags4 & MF4_BRIGHT))

#define CLIP_UNDEF (-2)

enum
{
   MAX_ROTATIONS = 8
};

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
   float *buffer = emalloctag(float *, w*2 * sizeof(float), PU_VALLOC, nullptr);
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
   float *buf = emalloctag(float *, 2 * w * sizeof(*portaltop), PU_VALLOC, nullptr);

   for(int i = 0; i < 2*w; i++)
      buf[i] = 0.0f;

   portaltop    = buf;
   portalbottom = buf + w;
}

// Used solely by R_DrawPlayerSprites and passed as mfloorclip
static float *pscreenheightarray; // for psprites

VALLOCATION(pscreenheightarray)
{
   pscreenheightarray = ecalloctag(float *, w, sizeof(float), PU_VALLOC, nullptr);
}

// Max number of particles
static int numParticles;

VALLOCATION(pstack)
{
   R_ForEachContext([](rendercontext_t &basecontext) {
      spritecontext_t &context = basecontext.spritecontext;

      poststack_t   *&pstack       = context.pstack;
      int            &pstacksize   = context.pstacksize;
      int            &pstackmax    = context.pstackmax;
      maskedrange_t *&unusedmasked = context.unusedmasked;

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

      pstack       = nullptr;
      pstacksize   = 0;
      pstackmax    = 0;
      unusedmasked = nullptr;
   });
}

// Used solely by R_drawSpriteInDSRange and passed as mfloorclip/mceilingclip
static float *clipbot;
static float *cliptop;

VALLOCATION(clipbot)
{
   float *buffer = ecalloctag(float *, w*2, sizeof(float), PU_VALLOC, nullptr);
   clipbot = buffer;
   cliptop = buffer + w;
}

//=============================================================================
//
// Functions
//

// Forward declarations:
static void R_drawParticle(const contextbounds_t &bounds, vissprite_t *vis,
                           const float *const mfloorclip, const float *const mceilingclip);
static void R_projectParticle(cmapcontext_t &cmapcontext, spritecontext_t &spritecontext,
                              const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                              const contextbounds_t &bounds, particle_t *particle);

//
// R_SetMaskedSilhouette
//
void R_SetMaskedSilhouette(const contextbounds_t &bounds,
                           const float *top, const float *bottom)
{
   if(!top || !bottom)
   {
      float *topp    = portaltop    + bounds.startcolumn;
      float *bottomp = portalbottom + bounds.startcolumn;
      float *stopp   = portaltop    + bounds.endcolumn;

      while(topp < stopp)
      {
         *topp++ = 0.0f;
         *bottomp++ = view.height - 1.0f;
      }
   }
   else
   {
      memcpy(portaltop    + bounds.startcolumn, top    + bounds.startcolumn, sizeof(*portaltop   ) * bounds.numcolumns);
      memcpy(portalbottom + bounds.startcolumn, bottom + bounds.startcolumn, sizeof(*portalbottom) * bounds.numcolumns);
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
// Local function for R_InitSprites.
//
static void R_installSpriteLump(const lumpinfo_t *lump, int lumpnum, unsigned frame,
                                unsigned rotation, bool flipped,
                                spriteframe_t *const sprtemp, int &maxframe)
{
   if(frame >= MAX_SPRITE_FRAMES || rotation > MAX_ROTATIONS)
   {
      C_Printf(FC_ERROR "R_installSpriteLump: Bad frame characters in lump %s\n", lump->name);
      return;
   }

   if((int)frame > maxframe)
      maxframe = frame;

   if(rotation == 0)
   {
      // the lump should be used for all rotations
      for(int r = 0; r < MAX_ROTATIONS; r++)
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
static void R_initSpriteDefs(char **namelist)
{
   static spriteframe_t sprtemp[MAX_SPRITE_FRAMES];
   static int maxframe;

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
               R_installSpriteLump(
                  lump, j+firstspritelump,
                  lump->name[4] - 'A', lump->name[5] - '0',
                  false, // not flipped
                  sprtemp, maxframe
               );
               if(lump->name[6])
                  R_installSpriteLump(
                     lump, j+firstspritelump,
                     lump->name[6] - 'A', lump->name[7] - '0',
                     true, // flipped
                     sprtemp, maxframe
                  );
            }
         }
         while((j = hash[j].next) >= 0);

         // check the frames that were found for completeness
         if(++maxframe)  // killough 1/31/98
         {
            bool valid = true;
            for(int frame = 0; frame < maxframe; frame++)
            {
               switch(sprtemp[frame].rotate)
               {
               case -1:
                  // no rotations were found for that frame at all
                  break;
                  
               case 0:
                  // only the first rotation is needed
                  break;
                  
               case 1:
                  // must have all 8 frames
                  for(int rotation = 0; rotation < MAX_ROTATIONS; rotation++)
                  {
                     if(sprtemp[frame].lump[rotation] == -1)
                     {
                        C_Printf(FC_ERROR "R_InitSprites: Sprite %.8s frame %c is missing rotations\n",
                                 namelist[i], frame + 'A');
                        valid = false;
                        break;
                     }
                  }
                  break;
               }
            }
            if(valid)
            {
               // allocate space for the frames present and copy sprtemp to it
               sprites[i].numframes = maxframe;
               sprites[i].spriteframes = estructalloctag(spriteframe_t, maxframe, PU_RENDERER);
               memcpy(sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
            }
         }
      }
      else
      {
         // haleyjd 08/15/02: problem here.
         // If j was -1 above, meaning that there are no lumps for the sprite
         // present, the sprite is left uninitialized. This creates major 
         // problems in R_PrecacheLevel if a thing tries to subsequently use
         // that sprite. Instead, set numframes to 0 and spriteframes to nullptr.
         // Then, check for these values before loading any sprite.
         
         sprites[i].numframes = 0;
         sprites[i].spriteframes = nullptr;
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
   R_initSpriteDefs(namelist);
   R_InitSpriteProjSpan();
}

//
// Called at frame start.
//
void R_ClearSprites(spritecontext_t &context)
{
   context.num_vissprite = 0; // killough
}

//
// Pushes a new element on the post-BSP stack.
//
void R_PushPost(bspcontext_t &bspcontext, spritecontext_t &spritecontext,
                const contextbounds_t &bounds, bool pushmasked, pwindow_t *window)
{
   drawseg_t     *&drawsegs     = bspcontext.drawsegs;
   unsigned int   &maxdrawsegs  = bspcontext.maxdrawsegs;
   drawseg_t     *&ds_p         = bspcontext.ds_p;
   poststack_t   *&pstack       = spritecontext.pstack;
   int            &pstacksize   = spritecontext.pstacksize;
   int            &pstackmax    = spritecontext.pstackmax;
   maskedrange_t *&unusedmasked = spritecontext.unusedmasked;

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

         post->masked->next = nullptr;
         post->masked->firstds = post->masked->lastds =
            post->masked->firstsprite = post->masked->lastsprite = 0;
      }
      else
      {
         post->masked = estructalloc(maskedrange_t, 1);

         float *buf = emalloc(float *, 2 * bounds.numcolumns * sizeof(float));
         post->masked->ceilingclip = buf;
         post->masked->floorclip   = buf + bounds.numcolumns;
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
      post->masked->lastsprite = int(spritecontext.num_vissprite);

      memcpy(post->masked->ceilingclip, portaltop    + bounds.startcolumn, sizeof(*portaltop)    * bounds.numcolumns);
      memcpy(post->masked->floorclip,   portalbottom + bounds.startcolumn, sizeof(*portalbottom) * bounds.numcolumns);
   }
   else
      post->masked = nullptr;

   pstacksize++;
}

//
// Creates a new vissprite if needed, or recycles an unused one.
//
static vissprite_t *R_newVisSprite(spritecontext_t &context)
{
   vissprite_t *&vissprites          = context.vissprites;
   size_t       &num_vissprite       = context.num_vissprite;
   size_t       &num_vissprite_alloc = context.num_vissprite_alloc;

   if(num_vissprite >= num_vissprite_alloc)             // killough
   {
      num_vissprite_alloc = num_vissprite_alloc ? num_vissprite_alloc*2 : 128;
      vissprites = erealloc(vissprite_t *, vissprites, num_vissprite_alloc*sizeof(*vissprites));
   }

   return vissprites + num_vissprite++;
}

//
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
static void R_drawMaskedColumn(const R_ColumnFunc colfunc,
                               cb_column_t &column, const cb_maskedcolumn_t &maskedcolumn,
                               column_t *tcolumn,
                               const float *const mfloorclip, const float *const mceilingclip)
{
   float y1, y2;
   fixed_t basetexturemid = column.texmid;
   
   column.texheight = 0; // killough 11/98

   int top = 0;
   while(tcolumn->topdelta != 0xff)
   {
      top = tcolumn->topdelta <= top ? tcolumn->topdelta + top : tcolumn->topdelta;

      // calculate unclipped screen coordinates for post
      y1 = maskedcolumn.ytop + (maskedcolumn.scale * top);
      y2 = y1 + (maskedcolumn.scale * tcolumn->length) - 1;

      column.y1 = (int)(y1 < mceilingclip[column.x] ? mceilingclip[column.x] : y1);
      column.y2 = (int)(y2 > mfloorclip[column.x]   ? mfloorclip[column.x]   : y2);

      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(column.y1 <= column.y2 && column.y2 < viewwindow.height)
      {
         column.source = (byte *)tcolumn + 3;
         column.texmid = basetexturemid - (top << FRACBITS);

         colfunc(column);
      }

      tcolumn = (column_t *)((byte *)tcolumn + tcolumn->length + 4);
   }

   column.texmid = basetexturemid;
}

//
// R_DrawNewMaskedColumn
//
void R_DrawNewMaskedColumn(const R_ColumnFunc colfunc,
                           cb_column_t &column, const cb_maskedcolumn_t &maskedcolumn,
                           const texture_t *const tex, const texcol_t *tcol,
                           const float *const mfloorclip, const float *const mceilingclip)
{
   float y1, y2;
   fixed_t basetexturemid = column.texmid;
   
   column.texheight = 0; // killough 11/98

   const byte *const texend = tex->bufferdata + tex->width * tex->height + 1;

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
         byte *localstart = tex->bufferdata + tcol->ptroff;
         column.source = localstart;
         column.texmid = basetexturemid - (tcol->yoff << FRACBITS);

         byte *const last = localstart + tcol->len;
         byte orig = 0;
         if(last < texend && last > tex->bufferdata)
         {
            orig = *last;
            *last = last[-1];
         }

         byte origstart = localstart[-1];
         localstart[-1] = *localstart;

         // Drawn by either R_DrawColumn
         //  or (SHADOW) R_DrawFuzzColumn.
         colfunc(column);
         if(last < texend && last > tex->bufferdata)
            *last = orig;
         localstart[-1] = origstart;
      }

      tcol = tcol->next;
   }

   column.texmid = basetexturemid;
}

//
//  mfloorclip and mceilingclip should also be set.
//
static void R_drawVisSprite(const contextbounds_t &bounds, vissprite_t *vis,
                            float *const mfloorclip, float *const mceilingclip)
{
   column_t *tcolumn;
   int       texturecolumn;
   float     frac;
   patch_t  *patch;
   bool      footclipon = false;
   float     baseclip = 0;
   int       w;

   cb_column_t column = {};

   if(vis->patch == -1)
   {
      // this vissprite belongs to a particle
      R_drawParticle(bounds, vis, mfloorclip, mceilingclip);
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
   const R_ColumnFunc colfunc = r_column_engine->ByVisSpriteStyle[vis->drawstyle][!!vis->colour];

   //column.step = M_FloatToFixed(vis->ystep);
   column.step = M_FloatToFixed(1.0f / vis->scale);
   column.texmid = vis->texturemid;
   frac = vis->startx;
   
   // haleyjd 10/10/02: foot clipping
   if(vis->footclip)
   {
      footclipon = true;
      baseclip = vis->ybottom - M_FixedToFloat(vis->footclip) * vis->scale;
   }

   w = patch->width;

   // haleyjd: use a separate loop for footclip things, to minimize
   // overhead for regular sprites and to require no separate loop
   // just to update mfloorclip
   const cb_maskedcolumn_t maskedcolumn = { vis->ytop, vis->scale };
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
         R_drawMaskedColumn(colfunc, column, maskedcolumn, tcolumn, mfloorclip, mceilingclip);
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
         R_drawMaskedColumn(colfunc, column, maskedcolumn, tcolumn, mfloorclip, mceilingclip);
      }
   }
}

struct spritepos_t
{
   fixed_t x, y, z;
};

// ioanch 20160109: added offset arguments
static void R_interpolateThingPosition(Mobj *thing, spritepos_t &pos)
{
   if(view.lerp == FRACUNIT)
   {
      pos.x = thing->x;
      pos.y = thing->y;
      pos.z = thing->z;
   }
   else
   {
      const linkdata_t *ldata;
      if((ldata = thing->prevpos.ldata))
      {
         pos.x = lerpCoord(view.lerp, thing->prevpos.x + ldata->delta.x, thing->x);
         pos.y = lerpCoord(view.lerp, thing->prevpos.y + ldata->delta.y, thing->y);
         pos.z = lerpCoord(view.lerp, thing->prevpos.z + ldata->delta.z, thing->z);
      }
      else
      {
         pos.x = lerpCoord(view.lerp, thing->prevpos.x, thing->x);
         pos.y = lerpCoord(view.lerp, thing->prevpos.y, thing->y);
         pos.z = lerpCoord(view.lerp, thing->prevpos.z, thing->z);
      }
   }
}

static void R_interpolatePSpritePosition(const pspdef_t &pspr, v2fixed_t &pos)
{
   if(view.lerp == FRACUNIT)
      pos = pspr.renderpos;
   else
   {
      pos.x = lerpCoord(view.lerp, pspr.prevpos.x, pspr.renderpos.x);
      pos.y = lerpCoord(view.lerp, pspr.prevpos.y, pspr.renderpos.y);
   }
}

//
// Projects the sprite back across the portal to the original layer
//
static void R_projectSpriteAcrossPortal(spritecontext_t &spritecontext, const vissprite_t &ovis,
                                        const pwindow_t &pwindow, const viewpoint_t &view)
{
   fixed_t height = ovis.gzt - ovis.gz;
   vissprite_t *bvis = R_newVisSprite(spritecontext);
   *bvis = ovis;
   if(view.angle != pwindow.vangle)
   {
      // rotated portal: apply the complex operation
      v3double_t sprdelta =
      {
         M_FixedToDouble(ovis.gx - view.x),
         M_FixedToDouble(ovis.gy - view.y),
         M_FixedToDouble(ovis.gz - view.z)
      };
      double sprdist = hypot(hypot(sprdelta.x, sprdelta.y), sprdelta.z);
      double sprang = atan2(sprdelta.y, sprdelta.x);
      angle_t deltafang = pwindow.vangle - view.angle;
      double targang = sprang - (double)deltafang / ANG180 * M_PI;
      v3double_t targpos =
      {
         M_FixedToDouble(pwindow.vx),
         M_FixedToDouble(pwindow.vy),
         M_FixedToDouble(pwindow.vz)
      };
      targpos.x += sprdist * cos(targang);
      targpos.y += sprdist * sin(targang);
      targpos.z += sprdelta.z;

      bvis->gx = M_DoubleToFixed(targpos.x);
      bvis->gy = M_DoubleToFixed(targpos.y);
      bvis->gz = M_DoubleToFixed(targpos.z);
   }
   else
   {
      // normal case: no rotation
      bvis->gx += pwindow.vx - view.x;
      bvis->gy += pwindow.vy - view.y;
      bvis->gz += pwindow.vz - view.z;
   }
   bvis->gzt = bvis->gz + height;;
   bvis->sector = eindex(R_PointInSubsector(bvis->gx, bvis->gy)->sector - sectors);

   poststack_t *pstack = spritecontext.pstack;
   vissprite_t *vissprites = spritecontext.vissprites;
   maskedrange_t *masked = pstack[pwindow.postbspfrom].masked;
   if(!masked) // can't have sprites on the source area
      return;

   // Now shift the mask ahead
   int pstacksize = spritecontext.pstacksize;
   vissprite_t mover = *bvis;

   for(int i = pwindow.postbspfrom; i < pstacksize; ++i)
   {
      maskedrange_t *masked = pstack[i].masked;
      if(!masked)
         continue;
      vissprite_t aux;
      if(i == pwindow.postbspfrom || masked->lastsprite > masked->firstsprite)
      {
         aux = vissprites[masked->lastsprite];
         vissprites[masked->lastsprite] = mover;
         mover = aux;
      }
      ++masked->lastsprite;
      if(i > pwindow.postbspfrom)
         ++masked->firstsprite;
   }
   *bvis = mover;
}

//
// Generates a vissprite for a thing if it might be visible.
// ioanch 20160109: added optional arguments for offsetting the sprite
//
static void R_projectSprite(cmapcontext_t &cmapcontext,
                            spritecontext_t &spritecontext,
                            const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                            const contextbounds_t &bounds,
                            const portalrender_t &portalrender,
                            Mobj *thing,
                            lighttable_t *const *const spritelights,
                            v3fixed_t *delta = nullptr,
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
   tempx = M_FixedToFloat(spritepos.x) - cb_viewpoint.x;
   tempy = M_FixedToFloat(spritepos.y) - cb_viewpoint.y;
   roty  = (tempy * cb_viewpoint.cos) + (tempx * cb_viewpoint.sin);

   // lies in front of the front view plane
   if(roty < 1.0f)
      return;

   // ioanch 20160125: reject sprites in front of portal line when rendering
   // line portal
   const pwindow_t *pwindow = nullptr;
   if(portalrender.active && portalrender.w->portal->type != R_SKYBOX)
   {
      v2fixed_t offsetpos = { thing->x, thing->y };
      if(delta)
      {
         offsetpos.x += delta->x;
         offsetpos.y += delta->y;
      }
      v2float_t posf = v2float_t::fromFixed(offsetpos);

      const renderbarrier_t &barrier = portalrender.w->barrier;
      if(portalrender.w->type == pw_line && portalrender.w->line != portalline &&
         barrier.linegen.normal * (posf - barrier.linegen.start) >= 0)
      {
         return;
      }
      if(portalrender.w->type != pw_line)
      {
         if(portalrender.w->line && portalrender.w->line != portalline &&
            barrier.linegen.normal * (posf - barrier.linegen.start) >= 0)
         {
            return;
         }
         windowlinegen_t linegen1, linegen2;
         if(R_PickNearestBoxLines(cb_viewpoint, barrier.fbox, linegen1, linegen2))
         {

            if(linegen1.normal * (posf - linegen1.start) >= 0)
               return;
            if(linegen2.normal.nonzero() && linegen2.normal * (posf - linegen2.start) >= 0)
               return;
         }
      }
      pwindow = portalrender.w;
   }

   rotx = (tempx * cb_viewpoint.cos) - (tempy * cb_viewpoint.sin);

   // decide which patch to use for sprite relative to player
   if((unsigned int)thing->sprite >= (unsigned int)numsprites)
   {
      // haleyjd 08/12/02: modified error handling
      doom_printf(FC_ERROR "Bad sprite number %i for thingtype %s\n", thing->sprite, thing->info->name);

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
      !(sprdef->spriteframes) ||
      sprdef->spriteframes[thing->frame & FF_FRAMEMASK].rotate == -1)
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
      angle_t ang = R_PointToAngle(viewpoint.x, viewpoint.y, spritepos.x, spritepos.y);
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
   if(x1 >= bounds.fendcolumn)
      return;

   x2 = view.xcenter + (tx2 * distxscale);
   if(x2 < bounds.fstartcolumn)
      return;

   intx1 = (int)(x1 + 0.999f);
   intx2 = (int)(x2 - 0.001f);

   distyscale = idist * view.yfoc;
   // SoM: forgot about footclipping
   fixed_t floorclip = thing->floorclip;
   if(vanilla_heretic && thing->z > thing->subsector->sector->srf.floor.height)
   {
      // prevent ugly visuals when emulating the Heretic foot clip bug
      floorclip = 0;
   }
   tz1 = thing->yscale * stopoffset + M_FixedToFloat(spritepos.z - floorclip) - cb_viewpoint.z;
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

      if(phs != -1 && viewpoint.z < sectors[phs].srf.floor.height ?
         thing->z >= hsec.srf.floor.height : gzt < hsec.srf.floor.height)
         return;
      if(phs != -1 && viewpoint.z > sectors[phs].srf.ceiling.height ?
         gzt < hsec.srf.ceiling.height && viewpoint.z >= hsec.srf.ceiling.height :
         thing->z >= hsec.srf.ceiling.height)
         return;
   }

   // store information in a vissprite
   vis = R_newVisSprite(spritecontext);

   // killough 3/27/98: save sector for special clipping later
   vis->heightsec = heightsec;

   vis->colour = thing->colour;
   vis->gx     = spritepos.x;
   vis->gy     = spritepos.y;
   vis->gz     = spritepos.z;
   vis->gzt    = gzt;                          // killough 3/27/98

   // Cardboard
   vis->x1 = x1 <  bounds.fstartcolumn ? bounds.startcolumn   : intx1;
   vis->x2 = x2 >= bounds.fendcolumn   ? bounds.endcolumn - 1 : intx2;

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
   if(thing->flags3 & MF3_GHOST && vis->translucency == FRACUNIT - 1 && rTintTableIndex == -1)
      vis->translucency = HTIC_GHOST_TRANS - 1;

   // haleyjd 10/12/02: foot clipping
   vis->footclip = floorclip;

   // haleyjd: moved this down, added footclip term
   // This needs to be scaled down (?) I don't get why this works...
   vis->texturemid = (fixed_t)((gzt - viewpoint.z - vis->footclip) / thing->yscale);

   vis->patch = lump;

   // get light level
   if(thing->flags & MF_SHADOW)     // sf
      vis->colormap = colormaps[global_cmap_index]; // haleyjd: NGCS -- was 0
   else if(cmapcontext.fixedcolormap)
      vis->colormap = cmapcontext.fixedcolormap;      // fixed map
   else if(LevelInfo.useFullBright && IS_FULLBRIGHT(thing)) // haleyjd
      vis->colormap = cmapcontext.fullcolormap;       // full bright  // killough 3/20/98
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
      else if(rTintTableIndex != -1 && thing->flags3 & MF3_GHOST)
      {
         vis->drawstyle = VS_DRAWSTYLE_TRANMAP;
         vis->tranmaplump = rTintTableIndex;
      }
   }

   if(pwindow)
      for(int x = vis->x1; x <= vis->x2; ++x)
         if(y1 < pwindow->top[x] || y2 > pwindow->bottom[x])
         {
            R_projectSpriteAcrossPortal(spritecontext, *vis, *pwindow, viewpoint);
            break;
         }
}

//
// During BSP traversal, this adds sprites by sector.
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
//
void R_AddSprites(cmapcontext_t &cmapcontext,
                  spritecontext_t &spritecontext,
                  const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                  const contextbounds_t &bounds,
                  const portalrender_t &portalrender,
                  sector_t* sec, int lightlevel)
{
   Mobj          *thing;
   int            lightnum;
   lighttable_t **spritelights;

   // BSP is traversed by subsector.
   // A sector might have been split into several
   //  subsectors during BSP building.
   // Thus we check whether its already added.

   if(spritecontext.sectorvisited[sec - sectors])
      return;
   
   // Well, now it will be done.
   spritecontext.sectorvisited[sec - sectors] = true;
   
   lightnum = (lightlevel >> LIGHTSEGSHIFT)+(extralight * LIGHTBRIGHT);
   
   if(lightnum < 0)
      spritelights = cmapcontext.scalelight[0];
   else if(lightnum >= LIGHTLEVELS)
      spritelights = cmapcontext.scalelight[LIGHTLEVELS-1];
   else
      spritelights = cmapcontext.scalelight[lightnum];
   
   // Handle all things in sector.
   
   for(thing = sec->thinglist; thing; thing = thing->snext)
   {
      R_projectSprite(
         cmapcontext, spritecontext, viewpoint,
         cb_viewpoint, bounds, portalrender, thing, spritelights
      );
   }

   // ioanch 20160109: handle partial sprite projections
   for(auto item = sec->spriteproj; item; item = item->dllNext)
   {
      if(!((*item)->mobj->intflags & MIF_HIDDENBYQUAKE))
      {
         R_projectSprite(
            cmapcontext, spritecontext, viewpoint, cb_viewpoint, bounds, portalrender,
            (*item)->mobj, spritelights, &(*item)->delta, (*item)->portalline
         );
      }
   }

   // haleyjd 02/20/04: Handle all particles in sector.

   if(drawparticles)
   {
      DLListItem<particle_t> *link;

      for(link = sec->ptcllist; link; link = link->dllNext)
         R_projectParticle(cmapcontext, spritecontext, viewpoint, cb_viewpoint, bounds, *link);
   }
}

//
// Draws player gun sprites.
//
static void R_drawPSprite(const pspdef_t *psp,
                          lighttable_t *const *const spritelights,
                          float *const mfloorclip, float *const mceilingclip)
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
   void (*playeraction)(actionargs_t *) = viewplayer->psprites[0].state->action;
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

   if(scaledwindow.height == SCREENHEIGHT)
      vis->texturemid -= viewplayer->readyweapon->fullscreenoffset;

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
      if(rTintTableIndex != -1)
      {
         vis->drawstyle = VS_DRAWSTYLE_TRANMAP;
         vis->tranmaplump = rTintTableIndex;
      }
      else
      {
         vis->drawstyle    = VS_DRAWSTYLE_ALPHA;
         vis->translucency = HTIC_GHOST_TRANS - 1;
      }
      vis->colormap     = spritelights[MAXLIGHTSCALE-1];
   }
   else if(r_globalcontext.cmapcontext.fixedcolormap)
      vis->colormap = r_globalcontext.cmapcontext.fixedcolormap; // fixed color
   else if(psp->state->frame & FF_FULLBRIGHT)
      vis->colormap = r_globalcontext.cmapcontext.fullcolormap; // full bright // killough 3/20/98
   else
      vis->colormap = spritelights[MAXLIGHTSCALE-1];  // local light
   
   if(psp->trans && general_translucency) // translucent gunflash
      vis->drawstyle = VS_DRAWSTYLE_TRANMAP;

   oldycenter = view.ycenter;
   view.ycenter = (view.height * 0.5f);
   
   R_drawVisSprite(r_globalcontext.bounds, vis, mfloorclip, mceilingclip);
   
   view.ycenter = oldycenter;
}

//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites()
{
   int i, lightnum;
   const pspdef_t *psp;
   sector_t tmpsec;
   int floorlightlevel, ceilinglightlevel;
   lighttable_t **spritelights;
   
   // sf: psprite switch
   if(!showpsprites || viewcamera) return;
   
   R_SectorColormap(r_globalcontext.cmapcontext, r_globalcontext.view.z, view.sector);

   // get light level
   // killough 9/18/98: compute lightlevel from floor and ceiling lightlevels
   // (see r_bsp.c for similar calculations for non-player sprites)

   R_FakeFlat(r_globalcontext.view.z, view.sector, &tmpsec, &floorlightlevel, &ceilinglightlevel, 0);
   lightnum = ((floorlightlevel+ceilinglightlevel) >> (LIGHTSEGSHIFT+1)) 
                 + (extralight * LIGHTBRIGHT);

   if(lightnum < 0)
      spritelights = r_globalcontext.cmapcontext.scalelight[0];
   else if(lightnum >= LIGHTLEVELS)
      spritelights = r_globalcontext.cmapcontext.scalelight[LIGHTLEVELS-1];
   else
      spritelights = r_globalcontext.cmapcontext.scalelight[lightnum];

   for(i = 0; i < viewwindow.width; ++i)
      pscreenheightarray[i] = view.height - 1.0f;

   // add all active psprites
   for(i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
   {
      if(psp->state)
         R_drawPSprite(psp, spritelights, pscreenheightarray, zeroarray);
   }
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
// Sorts only a subset of the vissprites, for portal rendering.
//
static void R_sortVisSpriteRange(spritecontext_t &context, int first, int last)
{
   vissprite_t  *&vissprites          = context.vissprites;
   vissprite_t **&vissprite_ptrs      = context.vissprite_ptrs;
   size_t        &num_vissprite_alloc = context.num_vissprite_alloc;
   size_t        &num_vissprite_ptrs  = context.num_vissprite_ptrs;

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
// Draws a sprite within a given drawseg range, for portals.
//
static void R_drawSpriteInDSRange(cmapcontext_t &cmapcontext, spritecontext_t &spritecontext,
                                  const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                                  const contextbounds_t &bounds,
                                  drawseg_t *const drawsegs,
                                  vissprite_t *spr, int firstds, int lastds,
                                  float *ptop, float *pbottom)
{
   drawseg_t *ds;
   int        x;

   for(x = spr->x1; x <= spr->x2; x++)
      clipbot[x] = cliptop[x] = CLIP_UNDEF;

   //
   // Common handler both for the optimized and basic loops
   //
   auto handleOverlappingDrawSeg = [](cmapcontext_t &cmapcontext,
                                      const viewpoint_t &viewpoint,
                                      drawseg_t *ds,
                                      const vissprite_t *spr) {
      float dist, fardist;
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

      int r1, r2;
      if(dist < spr->dist || (fardist < spr->dist &&
         !R_PointOnSegSide(spr->gx, spr->gy, ds->curline)))
      {
         if(ds->maskedtexturecol) // masked mid texture?
         {
            r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
            r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
            R_RenderMaskedSegRange(cmapcontext, viewpoint.z, ds, r1, r2);
         }
         return;                // seg is behind sprite
      }

      r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
      r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

      // clip this piece of the sprite
      // killough 3/27/98: optimized and made much shorter

      // bottom sil
      if(ds->silhouette & SIL_BOTTOM && spr->gz < ds->bsilheight)
      {
         for(int x = r1; x <= r2; x++)
         {
            if(clipbot[x] == CLIP_UNDEF)
               clipbot[x] = ds->sprbottomclip[x];
         }
      }

      // top sil
      if(ds->silhouette & SIL_TOP && spr->gzt > ds->tsilheight)
      {
         for(int x = r1; x <= r2; x++)
         {
            if(cliptop[x] == CLIP_UNDEF)
               cliptop[x] = ds->sprtopclip[x];
         }
      }
   };

   // haleyjd 04/25/10:
   // e6y: optimization
   if(spritecontext.drawsegs_xrange_count)
   {
      drawsegs_xrange_t *dsx = spritecontext.drawsegs_xrange;

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

         handleOverlappingDrawSeg(cmapcontext, viewpoint, ds, spr);
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

         handleOverlappingDrawSeg(cmapcontext, viewpoint, ds, spr);
      }
   }

   // Clip the sprite against deep water and/or fake ceilings.

   if(spr->heightsec != -1) // only things in specially marked sectors
   {
      float h, mh;
      
      int phs = view.sector->heightsec;

      mh = M_FixedToFloat(sectors[spr->heightsec].srf.floor.height) - cb_viewpoint.z;
      if(sectors[spr->heightsec].srf.floor.height > spr->gz &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(mh <= 0.0 || (phs != -1 && viewpoint.z > sectors[phs].srf.floor.height))
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
            if(phs != -1 && viewpoint.z <= sectors[phs].srf.floor.height) // killough 11/98
            {
               for(x = spr->x1; x <= spr->x2; x++)
               {
                  if(cliptop[x] == CLIP_UNDEF || h > cliptop[x])
                     cliptop[x] = h;
               }
            }
         }
      }

      mh = M_FixedToFloat(sectors[spr->heightsec].srf.ceiling.height) - cb_viewpoint.z;
      if(sectors[spr->heightsec].srf.ceiling.height < spr->gzt &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(phs != -1 && viewpoint.z >= sectors[phs].srf.ceiling.height)
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

      mh = M_FixedToFloat(sector->srf.floor.height) - cb_viewpoint.z;
      if(sector->srf.floor.pflags & PS_PASSABLE && sector->srf.floor.height > spr->gz)
      {
         h = eclamp(view.ycenter - (mh * spr->scale), 0.0f, view.height - 1);

         for(x = spr->x1; x <= spr->x2; x++)
         {
            if(clipbot[x] == CLIP_UNDEF || h < clipbot[x])
               clipbot[x] = h;
         }
      }

      mh = M_FixedToFloat(sector->srf.ceiling.height) - cb_viewpoint.z;
      if(sector->srf.ceiling.pflags & PS_PASSABLE && sector->srf.ceiling.height < spr->gzt)
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

   // THREAD_FIXME: Verify correctness
   for(x = spr->x1; x <= spr->x2; x++)
   {
      if(clipbot[x] == CLIP_UNDEF || clipbot[x] > pbottom[x - bounds.startcolumn])
         clipbot[x] = pbottom[x - bounds.startcolumn];

      if(cliptop[x] == CLIP_UNDEF || cliptop[x] < ptop[x - bounds.startcolumn])
         cliptop[x] = ptop[x - bounds.startcolumn];
   }

   R_drawVisSprite(bounds, spr, clipbot, cliptop);
}

//
// Draws the items in the Post-BSP stack.
//
void R_DrawPostBSP(rendercontext_t &context)
{
   bspcontext_t    &bspcontext    = context.bspcontext;
   spritecontext_t &spritecontext = context.spritecontext;
   planecontext_t  &planecontext  = context.planecontext;

   drawseg_t *const drawsegs     = bspcontext.drawsegs;
   const unsigned   maxdrawsegs  = bspcontext.maxdrawsegs;
   poststack_t    *&pstack       = spritecontext.pstack;
   int             &pstacksize   = spritecontext.pstacksize;
   maskedrange_t  *&unusedmasked = spritecontext.unusedmasked;

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
            drawsegs_xrange_t *&drawsegs_xrange       = spritecontext.drawsegs_xrange;
            unsigned int       &drawsegs_xrange_size  = spritecontext.drawsegs_xrange_size;
            int                &drawsegs_xrange_count = spritecontext.drawsegs_xrange_count;

            R_sortVisSpriteRange(spritecontext, firstsprite, lastsprite);

            // haleyjd 04/25/10: 
            // e6y
            // Reducing of cache misses in the following R_DrawSprite()
            // Makes sense for scenes with huge amount of drawsegs.
            // ~12% of speed improvement on epic.wad map05
            drawsegs_xrange_count = 0;
            if(spritecontext.num_vissprite > 0)
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
               // haleyjd: terminate with a nullptr user for faster loop - adds ~3 FPS
               drawsegs_xrange[drawsegs_xrange_count].user = nullptr;
            }

            for(int i = lastsprite - firstsprite; --i >= 0; )
            {
               R_drawSpriteInDSRange(
                  context.cmapcontext,
                  spritecontext,
                  context.view, context.cb_view, context.bounds, drawsegs,
                  spritecontext.vissprite_ptrs[i], firstds, lastds,
                  masked->ceilingclip, masked->floorclip
               );         // killough
            }
         }

         // render any remaining masked mid textures

         // Modified by Lee Killough:
         // (pointer check was originally nonportable
         // and buggy, by going past LEFT end of array):

         for(ds = drawsegs + lastds; ds-- > drawsegs + firstds; )  // new -- killough
         {
            if(ds->maskedtexturecol)
               R_RenderMaskedSegRange(context.cmapcontext, context.view.z, ds, ds->x1, ds->x2);
         }
         
         // Done with the masked range
         pstack[pstacksize].masked = nullptr;
         masked->next = unusedmasked;
         unusedmasked = masked;
         
         masked = nullptr;
      }       
      
      if(pstack[pstacksize].overlay)
      {
         // haleyjd 09/04/06: handle through column engine
         if(r_column_engine->ResetBuffer)
            r_column_engine->ResetBuffer();
            
         R_DrawPlanes(
            context.cmapcontext,
            planecontext.mainhash, planecontext.spanstart,
            context.view.angle, pstack[pstacksize].overlay
         );
         R_FreeOverlaySet(planecontext.r_overlayfreesets, pstack[pstacksize].overlay);
      }
   }
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
inline static sector_t *R_addProjNode(Mobj *mobj, const linkdata_t *data, v3fixed_t &delta,
                                      DLListItem<spriteprojnode_t> *&item,
                                      DLListItem<spriteprojnode_t> **&tail, const line_t *line)
{
   sector_t *sector;

   delta += data->delta;
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
      P_PointOnLineSidePrecise(mpi.mobj->x, mpi.mobj->y, &line) == 1 ||
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
         line.backsector->srf.floor.pflags & PS_PASSABLE &&
         mpi.mobj->z + mpi.scaledbottom < line.backsector->srf.floor.height)
      {
         data = &line.backsector->srf.floor.portal->data.link;
      }
      if(line.extflags & EX_ML_UPPERPORTAL &&
         line.backsector->srf.ceiling.pflags & PS_PASSABLE &&
         mpi.mobj->z + mpi.scaledtop > line.backsector->srf.ceiling.height)
      {
         data2 = &line.backsector->srf.ceiling.portal->data.link;
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
      (!(sector->srf.floor.pflags & PS_PASSABLE) && !(sector->srf.ceiling.pflags & PS_PASSABLE) &&
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
         sector->srf.floor.pflags & PS_PASSABLE &&
         P_PortalZ(surf_floor, *sector) > emin(mobj->z, mobj->prevpos.z) + scaledbottom)
   {
      // always accept first sector
      data = R_FPLink(sector);
      sector = R_addProjNode(mobj, data, delta, item, tail, nullptr);
   }

   // restart from mobj's group
   sector = mobj->subsector->sector;
   delta.x = delta.y = delta.z = 0;
   while(++loopprot < SECTOR_PORTAL_LOOP_PROTECTION && sector &&
         sector->srf.ceiling.pflags & PS_PASSABLE &&
         P_PortalZ(surf_ceil, *sector) < emax(mobj->z, mobj->prevpos.z) + scaledtop)
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
   if(mobj->prevpos.ldata)
   {
      mpi.bbox[BOXLEFT] = mobj->x - xspan;
      mpi.bbox[BOXRIGHT] = mobj->x + xspan;
      mpi.bbox[BOXBOTTOM] = mobj->y - xspan;
      mpi.bbox[BOXTOP] = mobj->y + xspan;
   }
   else
   {
      mpi.bbox[BOXLEFT] = emin(mobj->x, mobj->prevpos.x) - xspan;
      mpi.bbox[BOXRIGHT] = emax(mobj->x, mobj->prevpos.x) + xspan;
      mpi.bbox[BOXBOTTOM] = emin(mobj->y, mobj->prevpos.y) - xspan;
      mpi.bbox[BOXTOP] = emax(mobj->y, mobj->prevpos.y) + xspan;
   }
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
// Code adapted from ZDoom is licenced under the GPLv3.
//
// Copyright 1998-2012 (C) Marisa Heit
//

//
// newParticle
//
// Tries to find an inactive particle in the Particles list
// Returns nullptr on failure
//
particle_t *newParticle()
{
   particle_t *result = nullptr;
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
   
   Particles = emalloctag(particle_t *, numParticles*sizeof(particle_t), PU_STATIC, nullptr);
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
// R_projectParticle
//
static void R_projectParticle(cmapcontext_t &cmapcontext, spritecontext_t &spritecontext,
                              const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                              const contextbounds_t &bounds, particle_t *particle)
{
   fixed_t gzt;
   int x1, x2;
   vissprite_t *vis;
   sector_t    *sector = nullptr;
   int heightsec = -1;
   
   float tempx, tempy, ty1, tx1, tx2, tz;
   float idist, xscale, yscale;
   float y1, y2;

   // SoM: Cardboard translate the mobj coords and just project the sprite.
   tempx = M_FixedToFloat(particle->x) - cb_viewpoint.x;
   tempy = M_FixedToFloat(particle->y) - cb_viewpoint.y;
   ty1   = (tempy * cb_viewpoint.cos) + (tempx * cb_viewpoint.sin);

   // lies in front of the front view plane
   if(ty1 < 1.0f)
      return;

   // invisible?
   if(!particle->trans)
      return;

   tx1 = (tempx * cb_viewpoint.cos) - (tempy * cb_viewpoint.sin);

   tx2 = tx1 + 1.0f;

   idist = 1.0f / ty1;
   xscale = idist * view.xfoc;
   yscale = idist * view.yfoc;

   // calculate edges of the shape
   x1 = (int)(view.xcenter + (tx1 * xscale));
   x2 = (int)(view.xcenter + (tx2 * xscale));

   if(x2 < x1) x2 = x1;

   // off either side?
   if(x1 >= bounds.endcolumn || x2 < bounds.startcolumn)
      return;

   tz = M_FixedToFloat(particle->z) - cb_viewpoint.z;

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

      if(particle->z < sector->srf.floor.height ||
	 particle->z > sector->srf.ceiling.height)
	 return;
   }

   // only clip particles which are in special sectors
   if(heightsec != -1)
   {
      int phs = view.sector->heightsec;

      if(phs != -1 &&
	 viewpoint.z < sectors[phs].srf.floor.height ?
	 particle->z >= sectors[heightsec].srf.floor.height :
         gzt < sectors[heightsec].srf.floor.height)
         return;

      if(phs != -1 &&
	 viewpoint.z > sectors[phs].srf.ceiling.height ?
	 gzt < sectors[heightsec].srf.ceiling.height &&
	 viewpoint.z >= sectors[heightsec].srf.ceiling.height :
         particle->z >= sectors[heightsec].srf.ceiling.height)
         return;
   }

   // store information in a vissprite
   vis = R_newVisSprite(spritecontext);
   vis->heightsec = heightsec;
   vis->gx = particle->x;
   vis->gy = particle->y;
   vis->gz = particle->z;
   vis->gzt = gzt;
   vis->texturemid = vis->gzt - viewpoint.z;
   vis->x1 = emax(bounds.startcolumn,   x1);
   vis->x2 = emin(bounds.endcolumn - 1, x2);
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
   
   if(cmapcontext.fixedcolormap ==
      cmapcontext.fullcolormap + INVERSECOLORMAP*256*sizeof(lighttable_t))
   {
      vis->colormap = cmapcontext.fixedcolormap;
   } 
   else
   {
      R_SectorColormap(cmapcontext, viewpoint.z, sector);

      if(LevelInfo.useFullBright && (particle->styleflags & PS_FULLBRIGHT))
      {
         vis->colormap = cmapcontext.fullcolormap;
      }
      else
      {
         lighttable_t **ltable;
         sector_t tmpsec;
         int floorlightlevel, ceilinglightlevel, lightnum, index;

         R_FakeFlat(viewpoint.z, sector, &tmpsec, &floorlightlevel, &ceilinglightlevel, false);

         lightnum = (floorlightlevel + ceilinglightlevel) / 2;
         lightnum = (lightnum >> LIGHTSEGSHIFT) + (extralight * LIGHTBRIGHT);
         
         if(lightnum >= LIGHTLEVELS || cmapcontext.fixedcolormap)
            ltable = cmapcontext.scalelight[LIGHTLEVELS - 1];
         else if(lightnum < 0)
            ltable = cmapcontext.scalelight[0];
         else
            ltable = cmapcontext.scalelight[lightnum];
         
         index = (int)(idist * 2560.0f);
         if(index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE - 1;
         
         vis->colormap = ltable[index];
      }
   }
}

//
// haleyjd: this function had to be mostly rewritten
//
static void R_drawParticle(const contextbounds_t &bounds, vissprite_t *vis,
                           const float *const mfloorclip, const float *const mceilingclip)
{
   int x1, x2, ox1, ox2;
   int yl, yh;
   byte color;

   ox1 = x1 = vis->x1;
   ox2 = x2 = vis->x2;

   if(x1 < bounds.startcolumn)
      x1 = bounds.startcolumn;
   if(x2 >= bounds.endcolumn)
      x2 = bounds.endcolumn - 1;

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

      spacing = video.pitch - ycount;
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
            int count = ycount;

            do // step in x
            {
               bg = bg2rgb[*dest];
               bg = (fg + bg) | 0x1f07c1f;
               *dest++ = RGB32k[0][0][bg & (bg >> 15)];
            } 
            while(--count);
            dest += spacing;  // go to next row
         } 
         while(--xcount);
      }
      else // opaque (fast, and looks terrible)
      {
         do // step in y
         {
            int count = ycount;
            
            do // step in x
               *dest++ = color;
            while(--count);
            dest += spacing;  // go to next row
         } 
         while(--xcount);
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
