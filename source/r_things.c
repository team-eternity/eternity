// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2001 James Haley
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
//  Refresh of things represented by sprites --
//  i.e. map objects and particles.
//
//  Particle code largely from zdoom, thanks to Randy Heit.
//
//-----------------------------------------------------------------------------

#include "c_io.h"
#include "doomstat.h"
#include "w_wad.h"
#include "g_game.h"
#include "d_main.h"
#include "p_skin.h"
#include "r_main.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "r_draw.h"
#include "r_things.h"
#include "m_argv.h"
#include "p_user.h"
#include "e_edf.h"
#include "p_info.h"

#define MINZ        (FRACUNIT*4)
#define BASEYCENTER 100

extern int columnofs[];

typedef struct maskdraw_s
{
  int x1;
  int x2;
  int column;
  int topclip;
  int bottomclip;
} maskdraw_t;

// top and bottom of portal silhouette
static float portaltop[MAX_SCREENWIDTH];
static float portalbottom[MAX_SCREENWIDTH];

static float *ptop, *pbottom;

// haleyjd 10/09/06: optional vissprite limit
int r_vissprite_limit;
static int r_vissprite_count;

//
// R_SetMaskedSilhouette
//

void R_SetMaskedSilhouette(float *top, float *bottom)
{
   if(!top || !bottom)
   {
      register float *topp = portaltop, *bottomp = portalbottom, 
                     *stopp = portaltop + MAX_SCREENWIDTH;

      while(topp < stopp)
      {
         *topp++ = 0;
         *bottomp++ = view.height - 1.0f;
      }
   }
   else
   {
      memcpy(portaltop, top, sizeof(float) * MAX_SCREENWIDTH);
      memcpy(portalbottom, bottom, sizeof(float) * MAX_SCREENWIDTH);
   }
}

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//

extern int global_cmap_index; // haleyjd: NGCS

float pscreenheightarray[MAX_SCREENWIDTH]; // for psprites

static lighttable_t **spritelights;        // killough 1/25/98 made static

// constant arrays
//  used for psprite clipping and initializing clipping

float zeroarray[MAX_SCREENWIDTH];
float screenheightarray[MAX_SCREENWIDTH];
int lefthanded=0;

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up and range check thing_t sprites patches

spritedef_t *sprites;
int numsprites;

#define MAX_SPRITE_FRAMES 29          /* Macroized -- killough 1/25/98 */

static spriteframe_t sprtemp[MAX_SPRITE_FRAMES];
static int maxframe;

// haleyjd: global particle system state

int        numParticles;
int        activeParticles;
int        inactiveParticles;
particle_t *Particles;
int        particle_trans;

//
// R_InstallSpriteLump
//
// Local function for R_InitSprites.
//
static void R_InstallSpriteLump(int lump, unsigned frame,
                                unsigned rotation, boolean flipped)
{
   if(frame >= MAX_SPRITE_FRAMES || rotation > 8)
      I_Error("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

   if((int)frame > maxframe)
      maxframe = frame;

   if(rotation == 0)
   {    // the lump should be used for all rotations
      int r;
      for(r = 0; r < 8 ; ++r)
         if(sprtemp[frame].lump[r]==-1)
         {
            sprtemp[frame].lump[r] = lump - firstspritelump;
            sprtemp[frame].flip[r] = (byte) flipped;
            sprtemp[frame].rotate = false; //jff 4/24/98 if any subbed, rotless
         }
      return;
   }

   // the lump is only used for one rotation
   
   if(sprtemp[frame].lump[--rotation] == -1)
   {
      sprtemp[frame].lump[rotation] = lump - firstspritelump;
      sprtemp[frame].flip[rotation] = (byte) flipped;
      sprtemp[frame].rotate = true; //jff 4/24/98 only change if rot used
   }
}

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
// Empirically verified to have excellent hash
// properties across standard Doom sprites:

#define R_SpriteNameHash(s) ((unsigned int)((s)[0]-((s)[1]*3-(s)[3]*2-(s)[2])*2))

void R_InitSpriteDefs(char **namelist)
{
   size_t numentries = lastspritelump-firstspritelump+1;
   struct rsprhash_s { int index, next; } *hash;
   unsigned int i;
      
   if(!numentries || !*namelist)
      return;
   
   // count the number of sprite names
   for(i = 0; namelist[i]; ++i)
      ; // do nothing
   
   numsprites = (signed int)i;

   sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_STATIC, NULL);
   
   // Create hash table based on just the first four letters of each sprite
   // killough 1/31/98
   
   hash = malloc(sizeof(*hash) * numentries); // allocate hash table

   for(i = 0; i < numentries; ++i) // initialize hash table as empty
      hash[i].index = -1;

   for(i = 0; i < numentries; ++i)  // Prepend each sprite to hash chain
   {                                // prepend so that later ones win
      int j = R_SpriteNameHash(w_GlobalDir.lumpinfo[i+firstspritelump]->name) % numentries;
      hash[i].next = hash[j].index;
      hash[j].index = i;
   }

   // scan all the lump names for each of the names,
   //  noting the highest frame letter.

   for(i = 0; i < (unsigned int)numsprites; ++i)
   {
      const char *spritename = namelist[i];
      int j = hash[R_SpriteNameHash(spritename) % numentries].index;
      
      if(j >= 0)
      {
         memset(sprtemp, -1, sizeof(sprtemp));
         maxframe = -1;
         do
         {
            register lumpinfo_t *lump = w_GlobalDir.lumpinfo[j + firstspritelump];

            // Fast portable comparison -- killough
            // (using int pointer cast is nonportable):

            if(!((lump->name[0] ^ spritename[0]) |
                 (lump->name[1] ^ spritename[1]) |
                 (lump->name[2] ^ spritename[2]) |
                 (lump->name[3] ^ spritename[3])))
            {
               R_InstallSpriteLump(j+firstspritelump,
                                   lump->name[4] - 'A',
                                   lump->name[5] - '0',
                                   false);
               if(lump->name[6])
                  R_InstallSpriteLump(j+firstspritelump,
                                      lump->name[6] - 'A',
                                      lump->name[7] - '0',
                                      true);
            }
         }
         while((j = hash[j].next) >= 0);

         // check the frames that were found for completeness
         if((sprites[i].numframes = ++maxframe))  // killough 1/31/98
         {
            int frame;
            for (frame = 0; frame < maxframe; frame++)
            {
               switch((int)sprtemp[frame].rotate)
               {
               case -1:
                  // no rotations were found for that frame at all
                  I_Error("R_InitSprites: No patches found "
                          "for %.8s frame %c", namelist[i], frame+'A');
                  break;
                  
               case 0:
                  // only the first rotation is needed
                  break;
                  
               case 1:
                  // must have all 8 frames
                  {
                     int rotation;
                     for(rotation=0 ; rotation<8 ; rotation++)
                     {
                        if(sprtemp[frame].lump[rotation] == -1)
                           I_Error ("R_InitSprites: Sprite %.8s frame %c "
                                    "is missing rotations",
                                    namelist[i], frame+'A');
                     }
                     break;
                  }
               }
            }
            // allocate space for the frames present and copy sprtemp to it
            sprites[i].spriteframes =
               Z_Malloc(maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
            memcpy(sprites[i].spriteframes, sprtemp,
                   maxframe*sizeof(spriteframe_t));
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
   free(hash);             // free hash table
}

//
// GAME FUNCTIONS
//

static vissprite_t *vissprites, **vissprite_ptrs;  // killough
static size_t num_vissprite, num_vissprite_alloc, num_vissprite_ptrs;

//
// R_InitSprites
// Called at program start.
//

void R_InitSprites(char **namelist)
{
   int i;
   for(i = 0; i < MAX_SCREENWIDTH; ++i)    // killough 2/8/98
      zeroarray[i] = 0.0f;
   R_InitSpriteDefs(namelist);
}

//
// R_ClearSprites
//
// Called at frame start.
//
void R_ClearSprites(void)
{
   num_vissprite = 0; // killough

   r_vissprite_count = 0; // haleyjd
}

// SoM 12/13/03: the masked stack
static maskedstack_t *mstack = NULL;
static int stacksize = 0, stackmax = 0;

void R_PushMasked(void)
{
   if(stacksize == stackmax)
   {
      stackmax += 10;
      mstack = realloc(mstack, sizeof(maskedstack_t) * stackmax);
   }

   if(!stacksize)
   {
      mstack[0].firstds = mstack[0].firstsprite = 0;
   }
   else
   {
      mstack[stacksize].firstds     = mstack[stacksize-1].lastds;
      mstack[stacksize].firstsprite = mstack[stacksize-1].lastsprite;
   }

   mstack[stacksize].lastds     = ds_p - drawsegs;
   mstack[stacksize].lastsprite = num_vissprite;

   memcpy(mstack[stacksize].ceilingclip, portaltop,    MAX_SCREENWIDTH * sizeof(float));
   memcpy(mstack[stacksize].floorclip,   portalbottom, MAX_SCREENWIDTH * sizeof(float));
   stacksize++;
}

//
// R_NewVisSprite
//
// Creates a new vissprite if needed, or recycles an unused one.
//
vissprite_t *R_NewVisSprite(void)
{
   if(num_vissprite >= num_vissprite_alloc)             // killough
   {
      num_vissprite_alloc = num_vissprite_alloc ? num_vissprite_alloc*2 : 128;
      vissprites = realloc(vissprites,num_vissprite_alloc*sizeof(*vissprites));
   }

   return vissprites + num_vissprite++;
}

//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//

float *mfloorclip, *mceilingclip;

cb_maskedcolumn_t maskedcolumn;

void R_DrawMaskedColumn(column_t *tcolumn)
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
      if(column.y1 <= column.y2 && column.y2 < viewheight)
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
// R_DrawVisSprite
//
//  mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite(vissprite_t *vis, int x1, int x2)
{
   column_t *tcolumn;
   int      texturecolumn;
   float    frac;
   patch_t  *patch;
   boolean  footclipon = false;
   float baseclip = 0;
   int w;

   if(vis->patch == -1)
   {
      // this vissprite belongs to a particle
      R_DrawParticle(vis);
      return;
   }
  
   patch = W_CacheLumpNum(vis->patch+firstspritelump, PU_CACHE);
   
   column.colormap = vis->colormap;
   
   // killough 4/11/98: rearrange and handle translucent sprites
   // mixed with translucent/non-translucent 2s normals
   
   // sf: shadow draw now done by mobj flags, not a null colormap
   
   if(vis->mobjflags & MF_SHADOW)   // shadow draw
   {
      colfunc = r_column_engine->DrawFuzzColumn;    // killough 3/14/98
   }
   else if(vis->mobjflags3 & MF3_TLSTYLEADD)
   {
      // haleyjd 02/08/05: additive translucency support
      if(vis->colour)
      {
         colfunc = r_column_engine->DrawAddTRColumn;
         column.translation = translationtables[vis->colour - 1];
      }
      else
         colfunc = r_column_engine->DrawAddColumn;

      column.translevel = vis->translucency;
   }
   else if(vis->translucency < FRACUNIT && general_translucency)
   {
      // haleyjd: zdoom-style translucency
      // 01/12/04: changed translation handling
      if(vis->colour)
      {
         colfunc = r_column_engine->DrawFlexTRColumn;
         column.translation = translationtables[vis->colour - 1];
      }
      else
         colfunc = r_column_engine->DrawFlexColumn;

      column.translevel = vis->translucency;
   }
   else if(vis->mobjflags & MF_TRANSLUCENT && general_translucency) // phares
   {
      // haleyjd 02/08/05: allow translated BOOM tl columns too
      if(vis->colour)
      {
         colfunc = r_column_engine->DrawTLTRColumn;
         column.translation = translationtables[vis->colour - 1];
      }
      else
         colfunc = r_column_engine->DrawTLColumn;
      
      tranmap = main_tranmap; // killough 4/11/98
   }
   else if(vis->colour)
   {
      // haleyjd 01/12/04: changed translation handling
      colfunc = r_column_engine->DrawTRColumn;
      column.translation = translationtables[vis->colour - 1];
   }
   else
      colfunc = r_column_engine->DrawColumn;  // killough 3/14/98, 4/11/98


   column.step = M_FloatToFixed(vis->ystep);
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

   w = SwapShort(patch->width);

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
         
         tcolumn = (column_t *)((byte *) patch +
            SwapLong(patch->columnofs[texturecolumn]));
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
         
         tcolumn = (column_t *)((byte *) patch +
            SwapLong(patch->columnofs[texturecolumn]));
         R_DrawMaskedColumn(tcolumn);
      }
   }
   colfunc = r_column_engine->DrawColumn; // killough 3/14/98
}

//
// R_ProjectSprite
//
// Generates a vissprite for a thing if it might be visible.
//
void R_ProjectSprite(mobj_t *thing)
{
   fixed_t       gzt;            // killough 3/27/98
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int           lump;
   boolean       flip;
   vissprite_t   *vis;
   int           heightsec;      // killough 3/27/98

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

   // SoM: Cardboard translate the mobj coords and just project the sprite.
   tempx = M_FixedToFloat(thing->x) - view.x;
   tempy = M_FixedToFloat(thing->y) - view.y;
   roty  = (tempy * view.cos) + (tempx * view.sin);

   // lies in front of the front view plane
   if(roty < 1.0f)
      return;

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
      angle_t ang = R_PointToAngle(thing->x, thing->y);
      unsigned int rot = (ang - thing->angle + (unsigned int)(ANG45/2)*9) >> 29;
      lump = sprframe->lump[rot];
      flip = (boolean)sprframe->flip[rot];
   }
   else
   {
      // use single rotation for all views
      lump = sprframe->lump[0];
      flip = (boolean) sprframe->flip[0];
   }


   // Calculate the edges of the shape
   swidth = M_FixedToFloat(spritewidth[lump]);
   stopoffset = M_FixedToFloat(spritetopoffset[lump]);
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
   tz1 = thing->yscale * stopoffset + M_FixedToFloat(thing->z - thing->floorclip) - view.z;
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
   gzt = thing->z + (fixed_t)(spritetopoffset[lump] * thing->yscale);

   // killough 3/27/98: exclude things totally separated
   // from the viewer, by either water or fake ceilings
   // killough 4/11/98: improve sprite clipping for underwater/fake ceilings

   heightsec = thing->subsector->sector->heightsec;
   
   if(heightsec != -1)   // only clip things which are in special sectors
   {
      // haleyjd: and yet ANOTHER assumption!
      int phs = viewcamera ? viewcamera->heightsec :
                   viewplayer->mo->subsector->sector->heightsec;
      if(phs != -1 && viewz < sectors[phs].floorheight ?
           thing->z >= sectors[heightsec].floorheight :
           gzt < sectors[heightsec].floorheight)
         return;
      if(phs != -1 && viewz > sectors[phs].ceilingheight ?
           gzt < sectors[heightsec].ceilingheight &&
           viewz >= sectors[heightsec].ceilingheight :
           thing->z >= sectors[heightsec].ceilingheight)
         return;
   }
  
   // haleyjd 10/09/06: optional vissprite limit ^____^
   if(r_vissprite_limit != -1 && ++r_vissprite_count > r_vissprite_limit)
      return;

   // store information in a vissprite
   vis = R_NewVisSprite();
   
   // killough 3/27/98: save sector for special clipping later   
   vis->heightsec = heightsec;
   
   vis->mobjflags  = thing->flags;
   vis->mobjflags3 = thing->flags3; // haleyjd
   vis->colour = thing->colour;
   vis->gx = thing->x;
   vis->gy = thing->y;
   vis->gz = thing->z;
   vis->gzt = gzt;                          // killough 3/27/98

   // Cardboard
   vis->x1 = x1 < 0.0f ? 0 : intx1;
   vis->x2 = x2 >= view.width ? viewwidth - 1 : intx2;

   vis->xstep = flip ? -(swidth * pstep) : swidth * pstep;
   vis->startx = flip ? swidth - 1.0f : 0.0f;

   vis->dist = idist;
   vis->scale = distyscale * thing->yscale;
   vis->ystep = 1.0f / (vis->scale);

   vis->ytop = y1;
   vis->ybottom = y2;
#ifdef R_LINKEDPORTALS
   vis->sector = thing->subsector->sector - sectors;
#endif
   vis->pcolor = 0;

   //if(x1 < vis->x1)
      vis->startx += vis->xstep * (vis->x1 - x1);

   vis->translucency = thing->translucency; // haleyjd 09/01/02

   // haleyjd 11/14/02: ghost flag
   if(thing->flags3 & MF3_GHOST)
      vis->translucency = HTIC_GHOST_TRANS;

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
   else if(LevelInfo.useFullBright && (thing->frame & FF_FULLBRIGHT)) // haleyjd
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
}

//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
//
void R_AddSprites(sector_t* sec, int lightlevel)
{
   mobj_t *thing;
   particle_t *ptcl;
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

   // haleyjd 02/20/04: Handle all particles in sector.

   if(drawparticles)
   {
      for(ptcl = sec->ptcllist; ptcl; ptcl = (particle_t *)(ptcl->seclinks.next))
         R_ProjectParticle(ptcl);
   }
}

//
// R_DrawPSprite
//
// Draws player gun sprites.
//
void R_DrawPSprite(pspdef_t *psp)
{
   float         tx;
   float         x1, x2, w;
   float         oldycenter;
   
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int           lump;
   boolean       flip;
   vissprite_t   *vis;
   vissprite_t   avis;
   
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
   flip = ( (boolean) sprframe->flip[0] ) ^ lefthanded;
   
   // calculate edges of the shape
   tx  = M_FixedToFloat(psp->sx) - 160.0f;
   tx -= M_FixedToFloat(spriteoffset[lump]);

      // haleyjd
   w = (float)(spritewidth[lump] >> FRACBITS);

   x1 = (view.xcenter + tx * view.pspritexscale);
   
   // off the right side
   if(x1 > viewwidth)
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
      x2 = view.width - tmpx;    // viewwidth-x1
   }
   
   // store information in a vissprite
   vis = &avis;
   vis->mobjflags = 0;
   vis->mobjflags3 = 0; // haleyjd
   
   // killough 12/98: fix psprite positioning problem
   vis->texturemid = (BASEYCENTER<<FRACBITS) /* + FRACUNIT/2 */ -
                      (psp->sy - spritetopoffset[lump]);

   vis->x1 = x1 < 0.0f ? 0 : (int)x1;
   vis->x2 = x2 >= view.width ? viewwidth - 1 : (int)x2;
   vis->ystep = view.pspriteystep; // ANYRES
   vis->colour = 0;      // sf: default colourmap
   vis->translucency = FRACUNIT; // haleyjd: default zdoom trans.
   vis->footclip = 0; // haleyjd
   vis->scale = view.pspriteyscale;
   vis->ytop = (view.height * 0.5f) - (M_FixedToFloat(vis->texturemid) * vis->scale);
   vis->ybottom = vis->ytop + (spriteheight[lump] * vis->scale);
#ifdef R_LINKEDPORTALS
   vis->sector = viewplayer->mo->subsector->sector - sectors;
#endif
   vis->pcolor = 0;
   
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
   
   if(viewplayer->powers[pw_invisibility] > 4*32 || 
      viewplayer->powers[pw_invisibility] & 8)
   {
      // sf: shadow draw now detected by flags
      vis->mobjflags |= MF_SHADOW;                  // shadow draw
      vis->colormap = colormaps[global_cmap_index]; // haleyjd: NGCS -- was 0
   }
   else if(viewplayer->powers[pw_ghost] > 4*32 || // haleyjd: ghost
           viewplayer->powers[pw_ghost] & 8)
   {
      vis->translucency = HTIC_GHOST_TRANS;
      vis->colormap = spritelights[MAXLIGHTSCALE-1];
   }
   else if(fixedcolormap)
      vis->colormap = fixedcolormap;           // fixed color
   else if(psp->state->frame & FF_FULLBRIGHT)
      vis->colormap = fullcolormap;            // full bright // killough 3/20/98
   else
      vis->colormap = spritelights[MAXLIGHTSCALE-1];  // local light
   
   if(psp->trans) // translucent gunflash
      vis->mobjflags |= MF_TRANSLUCENT;

   oldycenter = view.ycenter;
   view.ycenter = (view.height * 0.5f);
   
   R_DrawVisSprite (vis, vis->x1, vis->x2);
   
   view.ycenter = oldycenter;
}

//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites(void)
{
   int i, lightnum;
   pspdef_t *psp;
   sector_t tmpsec;
   int floorlightlevel, ceilinglightlevel;
   
   // sf: psprite switch
   if(!showpsprites || viewcamera) return;
   
   R_SectorColormap(viewplayer->mo->subsector->sector);

   // get light level
   // killough 9/18/98: compute lightlevel from floor and ceiling lightlevels
   // (see r_bsp.c for similar calculations for non-player sprites)

   R_FakeFlat(viewplayer->mo->subsector->sector, &tmpsec,
              &floorlightlevel, &ceilinglightlevel, 0);
   lightnum = ((floorlightlevel+ceilinglightlevel) >> (LIGHTSEGSHIFT+1)) 
                 + (extralight * LIGHTBRIGHT);

   if(lightnum < 0)
      spritelights = scalelight[0];
   else if(lightnum >= LIGHTLEVELS)
      spritelights = scalelight[LIGHTLEVELS-1];
   else
      spritelights = scalelight[lightnum];

   for(i = 0; i < viewwidth; ++i)
      pscreenheightarray[i] = view.height - 1.0f;
   
   // clip to screen bounds
   mfloorclip = pscreenheightarray;
   mceilingclip = zeroarray;

   // add all active psprites
   for(i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
      if(psp->state)
         R_DrawPSprite(psp);
}

//
// R_SortVisSprites
//
// Rewritten by Lee Killough to avoid using unnecessary
// linked lists, and to use faster sorting algorithm.
//

// killough 9/22/98: inlined memcpy of pointer arrays

/* Julian 6/7/2001

        1) cleansed macro layout
        2) remove esi,edi,ecx from cloberred regs since used in constraints
           (useless on old gcc, error maker on modern versions)
*/

#ifdef DJGPP

#define bcopyp(d, s, n) \
asm(\
" cld\n"\
"rep\n" \
"movsl" :\
: "D"(d), "S"(s), "c"(n) : "%cc")

#else

#define bcopyp(d, s, n) memcpy(d, s, (n) * sizeof(void *))

#endif

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
      for(i = 1; i < n; ++i)
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

void R_SortVisSprites(void)
{
   if(num_vissprite)
   {
      int i = num_vissprite;
      
      // If we need to allocate more pointers for the vissprites,
      // allocate as many as were allocated for sprites -- killough
      // killough 9/22/98: allocate twice as many
      
      if(num_vissprite_ptrs < num_vissprite*2)
      {
         free(vissprite_ptrs);  // better than realloc -- no preserving needed
         vissprite_ptrs = malloc((num_vissprite_ptrs = num_vissprite_alloc*2)
                                  * sizeof *vissprite_ptrs);
      }

      while(--i >= 0)
         vissprite_ptrs[i] = vissprites+i;

      // killough 9/22/98: replace qsort with merge sort, since the keys
      // are roughly in order to begin with, due to BSP rendering.
      
      msort(vissprite_ptrs, vissprite_ptrs + num_vissprite, num_vissprite);
   }
}


//
// R_SortVisSpriteRange
//
// Sorts only a subset of the vissprites, for portal rendering.
//
void R_SortVisSpriteRange(int first, int last)
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
         free(vissprite_ptrs);  // better than realloc -- no preserving needed
         vissprite_ptrs = malloc((num_vissprite_ptrs = num_vissprite_alloc*2)
                                  * sizeof *vissprite_ptrs);
      }

      while(--i >= 0)
         vissprite_ptrs[i] = vissprites+i+first;

      // killough 9/22/98: replace qsort with merge sort, since the keys
      // are roughly in order to begin with, due to BSP rendering.
      
      msort(vissprite_ptrs, vissprite_ptrs + numsprites, numsprites);
   }
}

// haleyjd: made static global
static float clipbot[MAX_SCREENWIDTH];
static float cliptop[MAX_SCREENWIDTH];

//
// R_DrawSprite
//
void R_DrawSprite(vissprite_t *spr)
{
   drawseg_t *ds;
   int     x;
   int     r1;
   int     r2;
   float dist;
   float fardist;
   
   for(x = spr->x1; x <= spr->x2; ++x)
      clipbot[x] = cliptop[x] = -2;

   // Scan drawsegs from end to start for obscuring segs.
   // The first drawseg that has a greater scale is the clip seg.
   
   // Modified by Lee Killough:
   // (pointer check was originally nonportable
   // and buggy, by going past LEFT end of array):
   
   //    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

   for(ds = ds_p; ds-- > drawsegs; )  // new -- killough
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
         for(x = r1; x <= r2; ++x)
            if(clipbot[x] == -2)
               clipbot[x] = ds->sprbottomclip[x];

      // top sil
      if(ds->silhouette & SIL_TOP && spr->gzt > ds->tsilheight)
         for(x = r1; x <= r2; ++x)
            if(cliptop[x] == -2)
               cliptop[x] = ds->sprtopclip[x];
   }

   // Clip the sprite against deep water and/or fake ceilings.

   if(spr->heightsec != -1) // only things in specially marked sectors
   {
      float h, mh;
      
      int phs = viewcamera ? viewcamera->heightsec :
                   viewplayer->mo->subsector->sector->heightsec;

      mh = M_FixedToFloat(sectors[spr->heightsec].floorheight) - view.z;
      if(sectors[spr->heightsec].floorheight > spr->gz &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(mh <= 0.0 || (phs != -1 && viewz > sectors[phs].floorheight))
         {
            // clip bottom
            for(x = spr->x1; x <= spr->x2; ++x)
               if(clipbot[x] == -2 || h < clipbot[x])
                  clipbot[x] = h;
         }
         else  // clip top
            if(phs != -1 && viewz <= sectors[phs].floorheight) // killough 11/98
               for(x = spr->x1; x <= spr->x2; ++x)
                  if(cliptop[x] == -2 || h > cliptop[x])
                     cliptop[x] = h;
      }

      mh = M_FixedToFloat(sectors[spr->heightsec].ceilingheight) - view.z;
      if(sectors[spr->heightsec].ceilingheight < spr->gzt &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(phs != -1 && viewz >= sectors[phs].ceilingheight)
         {
            // clip bottom
            for(x = spr->x1; x <= spr->x2; ++x)
               if(clipbot[x] == -2 || h < clipbot[x])
                  clipbot[x] = h;
         }
         else  // clip top
            for(x = spr->x1; x <= spr->x2; ++x)
               if(cliptop[x] == -2 || h > cliptop[x])
                  cliptop[x] = h;
      }
   }
   // killough 3/27/98: end special clipping for deep water / fake ceilings

   // all clipping has been performed, so draw the sprite
   // check for unclipped columns
   
   for(x = spr->x1; x <= spr->x2; ++x)
   {
      if(clipbot[x] == -2)
         clipbot[x] = view.height - 1.0f;
      
      if(cliptop[x] == -2)
         cliptop[x] = 0.0f;
   }

   mfloorclip = clipbot;
   mceilingclip = cliptop;
   R_DrawVisSprite(spr, spr->x1, spr->x2);
}


//
// R_DrawSpriteInDSRange
//
// Draws a sprite within a given drawseg range, for portals.
//
void R_DrawSpriteInDSRange(vissprite_t* spr, int firstds, int lastds)
{
   drawseg_t *ds;
   //float     clipbot[MAX_SCREENWIDTH];       // killough 2/8/98:
   //float     cliptop[MAX_SCREENWIDTH];       // change to MAX_*
   int     x;
   int     r1;
   int     r2;
   float dist;
   float fardist;

   for(x = spr->x1; x <= spr->x2; ++x)
      clipbot[x] = cliptop[x] = -2;

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
         for(x = r1; x <= r2; ++x)
            if(clipbot[x] == -2)
               clipbot[x] = ds->sprbottomclip[x];

      // top sil
      if(ds->silhouette & SIL_TOP && spr->gzt > ds->tsilheight)
         for(x = r1; x <= r2; ++x)
            if(cliptop[x] == -2)
               cliptop[x] = ds->sprtopclip[x];
   }

   // Clip the sprite against deep water and/or fake ceilings.

   if(spr->heightsec != -1) // only things in specially marked sectors
   {
      float h, mh;
      
      int phs = viewcamera ? viewcamera->heightsec :
                   viewplayer->mo->subsector->sector->heightsec;

      mh = M_FixedToFloat(sectors[spr->heightsec].floorheight) - view.z;
      if(sectors[spr->heightsec].floorheight > spr->gz &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(mh <= 0.0 || (phs != -1 && viewz > sectors[phs].floorheight))
         {
            // clip bottom
            for(x = spr->x1; x <= spr->x2; ++x)
               if(clipbot[x] == -2 || h < clipbot[x])
                  clipbot[x] = h;
         }
         else  // clip top
            if(phs != -1 && viewz <= sectors[phs].floorheight) // killough 11/98
               for(x = spr->x1; x <= spr->x2; ++x)
                  if(cliptop[x] == -2 || h > cliptop[x])
                     cliptop[x] = h;
      }

      mh = M_FixedToFloat(sectors[spr->heightsec].ceilingheight) - view.z;
      if(sectors[spr->heightsec].ceilingheight < spr->gzt &&
         (h = view.ycenter - (mh * spr->scale)) >= 0.0f &&
         (h < view.height))
      {
         if(phs != -1 && viewz >= sectors[phs].ceilingheight)
         {
            // clip bottom
            for(x = spr->x1; x <= spr->x2; ++x)
               if(clipbot[x] == -2 || h < clipbot[x])
                  clipbot[x] = h;
         }
         else  // clip top
            for(x = spr->x1; x <= spr->x2; ++x)
               if(cliptop[x] == -2 || h > cliptop[x])
                  cliptop[x] = h;
      }
   }
   // killough 3/27/98: end special clipping for deep water / fake ceilings

#ifdef R_LINKEDPORTALS
   // SoM: special clipping for linked portals
   if(useportalgroups)
   {
      float h, mh;

      sector_t *sector = sectors + spr->sector;

      mh = M_FixedToFloat(sector->floorheight) - view.z;
      if(R_LinkedFloorActive(sector) && sector->floorheight > spr->gz)
      {
         h = view.ycenter - (mh * spr->scale);
         if(h < 0) h = 0;
         if(h >= view.height) h = view.height - 1;

         for(x = spr->x1; x <= spr->x2; x++)
            if(clipbot[x] == -2 || h < clipbot[x])
               clipbot[x] = h;
      }


      mh = M_FixedToFloat(sector->ceilingheight) - view.z;
      if(R_LinkedCeilingActive(sector) && sector->ceilingheight < spr->gzt)
      {
         h = view.ycenter - (mh * spr->scale);
         if(h < 0) h = 0;
         if(h >= view.height) h = view.height - 1;

         for(x = spr->x1; x <= spr->x2; x++)
            if(cliptop[x] == -2 || h > cliptop[x])
               cliptop[x] = h;
      }
   }
#endif

   // all clipping has been performed, so draw the sprite
   // check for unclipped columns
   
   for(x = spr->x1; x <= spr->x2; ++x)
   {
      if(clipbot[x] == -2 || clipbot[x] > pbottom[x])
         clipbot[x] = pbottom[x];
      
      if(cliptop[x] == -2 || cliptop[x] < ptop[x])
         cliptop[x] = ptop[x];
    }

   mfloorclip = clipbot;
   mceilingclip = cliptop;
   R_DrawVisSprite(spr, spr->x1, spr->x2);
}


//
// R_DrawMasked
//
void R_DrawMasked(void)
{
   int firstds, lastds, firstsprite, lastsprite;
   int i;
   drawseg_t *ds;
 
   while(stacksize > 0)
   {
      --stacksize;

      firstds = mstack[stacksize].firstds;
      lastds  = mstack[stacksize].lastds;
      firstsprite = mstack[stacksize].firstsprite;
      lastsprite  = mstack[stacksize].lastsprite;

      R_SortVisSpriteRange(firstsprite, lastsprite);

      ptop    = mstack[stacksize].ceilingclip;
      pbottom = mstack[stacksize].floorclip;

      for(i = lastsprite - firstsprite; --i >= 0; )
         R_DrawSpriteInDSRange(vissprite_ptrs[i], firstds, lastds);         // killough

      // render any remaining masked mid textures

      // Modified by Lee Killough:
      // (pointer check was originally nonportable
      // and buggy, by going past LEFT end of array):

      for(ds=drawsegs + lastds ; ds-- > drawsegs + firstds; )  // new -- killough
         if(ds->maskedtexturecol)
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);
   }

   // draw the psprites on top of everything
   //  but does not draw on side views
   if(!viewangleoffset)
      R_DrawPlayerSprites();
}

//=====================================================================
//
// haleyjd 09/30/01
//
// Particle Rendering
// This incorporates itself mostly seamlessly within the
// vissprite system, incurring only minor changes to the functions
// above.

//
// newParticle
//
// Tries to find an inactive particle in the Particles list
// Returns NULL on failure
//
particle_t *newParticle(void)
{
   particle_t *result = NULL;
   if(inactiveParticles != -1)
   {
      result = Particles + inactiveParticles;
      inactiveParticles = result->next;
      result->next = activeParticles;
      activeParticles = result - Particles;
   }

   return result;
}

//
// R_InitParticles
//
// Allocate the particle list and initialize it
//
void R_InitParticles(void)
{
   int i;

   numParticles = 0;

   if((i = M_CheckParm("-numparticles")) && i < myargc - 1)
      numParticles = atoi(myargv[i+1]);
   
   if(numParticles == 0) // assume default
      numParticles = 4000;
   else if(numParticles < 100)
      numParticles = 100;
   
   Particles = Z_Malloc(numParticles*sizeof(particle_t), PU_STATIC, NULL);
   R_ClearParticles();
}

//
// R_ClearParticles
//
// set up the particle list
//
void R_ClearParticles(void)
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
void R_ProjectParticle(particle_t *particle)
{
   fixed_t gzt;
   int x1, x2;
   vissprite_t *vis;
   sector_t    *sector = NULL;
   int heightsec = -1;
   
   float tempx, tempy, ty1, tx1, tx2, tz;
   float idist, xscale, yscale;
   float y1, y2;

   // FIXME: particles are bad in low detail...
   if(detailshift)
      return;

   // SoM: Cardboard translate the mobj coords and just project the sprite.
   tempx = M_FixedToFloat(particle->x) - view.x;
   tempy = M_FixedToFloat(particle->y) - view.y;
   ty1   = (tempy * view.cos) + (tempx * view.sin);

   // lies in front of the front view plane
   if(ty1 < 1.0f)
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
   if(x1 >= viewwidth || x2 < 0)
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
      int phs = viewcamera ? viewcamera->heightsec :
                viewplayer->mo->subsector->sector->heightsec;
      
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
   vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
   //vis->translation = NULL;
   vis->pcolor = particle->color;
   vis->patch = -1;
   vis->mobjflags = particle->trans;
   vis->mobjflags3 = 0; // haleyjd
   // Cardboard
   vis->dist = idist;
   vis->ystep = 1.0f / yscale;
   vis->xstep = 1.0f / xscale;
   vis->ytop = y1;
   vis->ybottom = y2;
   vis->scale = yscale;
#ifdef R_LINKEDPORTALS
   vis->sector = sector - sectors;  
#endif
   
   if(fixedcolormap ==
      fullcolormap + INVERSECOLORMAP*256*sizeof(lighttable_t))
   {
      vis->colormap = fixedcolormap;
   } 
   else
   {
      // haleyjd 01/12/02: wow is this code wrong! :)
      /*int index = xcale >> (LIGHTSCALESHIFT + hires);
      if(index >= MAXLIGHTSCALE) 
         index = MAXLIGHTSCALE-1;      
      vis->colormap = spritelights[index];*/

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
void R_DrawParticle(vissprite_t *vis)
{
   int x1, x2, ox1, ox2;
   int yl, yh;
   byte color;

   ox1 = x1 = vis->x1;
   ox2 = x2 = vis->x2;

   if(x1 < 0)
      x1 = 0;
   if(x2 >= viewwidth)
      x2 = viewwidth - 1;

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

   color = vis->colormap[vis->pcolor];

   {
      int xcount, ycount, spacing;
      byte *dest;

      xcount = x2 - x1 + 1;
      
      ycount = yh - yl;      
      if(ycount < 0)
         return;
      ++ycount;

      spacing = video.pitch - xcount;
      dest = ylookup[yl] + columnofs[x1];

      // haleyjd 02/08/05: rewritten to remove inner loop invariants
      if(general_translucency && particle_trans)
      {
         if(particle_trans == 1) // smooth (DosDOOM-style)
         {
            unsigned int bg, fg;
            unsigned int *fg2rgb, *bg2rgb;
            unsigned int fglevel, bglevel;

            // look up translucency information
            fglevel = (unsigned int)(vis->mobjflags) & ~0x3ff;
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
         else // general (BOOM)
         {
            do // step in y
            {
               int count = xcount;
               
               do // step in x
               {
                  *dest = main_tranmap[(*dest << 8) + color];
                  ++dest;
               } 
               while(--count);
               dest += spacing;  // go to next row
            } 
            while(--ycount);
         } // end else [particle_trans == 2]
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
