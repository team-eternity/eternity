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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   -Loads and initializes texture and flat animation sequences
//   -Implements utility functions for all linedef/sector special handlers
//   -Dispatches walkover and gun line triggers
//   -Initializes and implements special sector types
//   -Implements donut linedef triggers
//   -Initializes and implements BOOM linedef triggers for
//     Friction
//
// haleyjd 10/13/2011: TODO - module is oversized; split up.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "a_small.h"
#include "acs_intr.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_englsh.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "ev_sectors.h"
#include "ev_specials.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_portal.h"
#include "p_pushers.h"
#include "p_saveg.h"
#include "p_scroll.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "p_things.h"
#include "p_tick.h"
#include "p_user.h"
#include "polyobj.h"
#include "m_argv.h"
#include "m_bbox.h"                                         // phares 3/20/98
#include "m_random.h"
#include "m_swap.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_plane.h"    // killough 10/98
#include "r_portal.h"
#include "r_ripple.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

//
// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
//
typedef struct anim_s
{
  bool        istexture;
  int         picnum;
  int         basepic;
  int         numpics;
  int         speed;
} anim_t;

//
//      source animation definition
//
#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

struct animdef_t
{
   uint8_t istexture;      // 0xff terminates; if false, it is a flat
   char    endname[9];           
   char    startname[9];
   int     speed;
}; //jff 3/23/98 pack to read from memory

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

#define MAXANIMS 32                   // no longer a strict limit -- killough
static anim_t *lastanim, *anims;      // new structure w/o limits -- killough
static size_t maxanims;

// killough 3/7/98: Initialize generalized scrolling
static void P_SpawnFriction();    // phares 3/16/98

extern int allow_pushers;
extern int variable_friction;     // phares 3/20/98

// haleyjd 01/24/04: portals
typedef enum
{
   portal_plane,
   portal_horizon,
   portal_skybox,
   portal_anchored,
   portal_twoway,
   portal_linked
} portal_type;

typedef enum
{
   portal_ceiling,
   portal_floor,
   portal_both,
   portal_lineonly, // SoM: Added for linked line-line portals.
} portal_effect;

static void P_SpawnPortal(line_t *, int);

//
// P_InitPicAnims
//
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
void P_InitPicAnims(void)
{
   int         i, p;
   animdef_t   *animdefs; //jff 3/23/98 pointer to animation lump
   int         flags;
   
   //  Init animation
   //jff 3/23/98 read from predefined or wad lump instead of table
   animdefs = (animdef_t *)wGlobalDir.cacheLumpName("ANIMATED", PU_STATIC);

   lastanim = anims;
   for(i = 0; animdefs[i].istexture != 0xff; i++)
   {
      flags = TF_ANIMATED;
      
      // 1/11/98 killough -- removed limit by array-doubling
      if(lastanim >= anims + maxanims)
      {
         size_t newmax = maxanims ? maxanims*2 : MAXANIMS;
         anims = erealloc(anim_t *, anims, newmax*sizeof(*anims)); // killough
         lastanim = anims + maxanims;
         maxanims = newmax;
      }

      if(animdefs[i].istexture)
      {
         // different episode ?
         if(R_CheckForWall(animdefs[i].startname) == -1)
            continue;
         
         lastanim->picnum = R_FindWall(animdefs[i].endname);
         lastanim->basepic = R_FindWall(animdefs[i].startname);
      }
      else
      {
         if(R_CheckForFlat(animdefs[i].startname) == -1)
            continue;
         
         lastanim->picnum = R_FindFlat(animdefs[i].endname);
         lastanim->basepic = R_FindFlat(animdefs[i].startname);
      }
      
      lastanim->istexture = !!animdefs[i].istexture;
      lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;
      lastanim->speed = SwapLong(animdefs[i].speed); // killough 5/5/98: add LONG()

      // SoM: just to make sure
      if(lastanim->numpics <= 0)
         continue;

      // sf: include support for swirly water hack
      if(lastanim->speed < 65536 && lastanim->numpics != 1)
      {
         if(lastanim->numpics < 2)
         {
            I_Error("P_InitPicAnims: bad cycle from %s to %s\n",
                     animdefs[i].startname,
                     animdefs[i].endname);
         }
      }
      else
      {
         // SoM: it's swirly water
         flags |= TF_SWIRLY;
      }
      
      // SoM: add flags
      for(p = lastanim->basepic; p <= lastanim->picnum; p++)
         textures[p]->flags |= flags;

      lastanim++;
   }

   Z_ChangeTag(animdefs, PU_CACHE); //jff 3/23/98 allow table to be freed
}

//=============================================================================
//
// Linedef and Sector Special Implementation Utility Functions
//

//
// getSide()
//
// Will return a side_t*
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
// Note: if side=1 is specified, it must exist or results undefined
//
side_t *getSide(int currentSector, int line, int side)
{
   return &sides[sectors[currentSector].lines[line]->sidenum[side]];
}

//
// getSector()
//
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
// Note: if side=1 is specified, it must exist or results undefined
//
sector_t *getSector(int currentSector, int line, int side)
{
   return 
     sides[sectors[currentSector].lines[line]->sidenum[side]].sector;
}

//
// twoSided()
//
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
// modified to return actual two-sidedness rather than presence
// of 2S flag unless compatibility optioned
//
// killough 11/98: reformatted
//
int twoSided(int sector, int line)
{
   //jff 1/26/98 return what is actually needed, whether the line
   //has two sidedefs, rather than whether the 2S flag is set
   
   return 
      comp[comp_model] ? 
         sectors[sector].lines[line]->flags & ML_TWOSIDED :
         sectors[sector].lines[line]->sidenum[1] != -1;
}

//
// getNextSector()
//
// Return sector_t * of sector next to current across line.
//
// Note: returns NULL if not two-sided line, or both sides refer to sector
//
// killough 11/98: reformatted
//
sector_t *getNextSector(const line_t *line, const sector_t *sec)
{
   //jff 1/26/98 check unneeded since line->backsector already
   //returns NULL if the line is not two sided, and does so from
   //the actual two-sidedness of the line, rather than its 2S flag
   //
   //jff 5/3/98 don't retn sec unless compatibility
   // fixes an intra-sector line breaking functions
   // like floor->highest floor

   return 
      comp[comp_model] && !(line->flags & ML_TWOSIDED) ? 
         NULL :
         line->frontsector == sec ? 
            comp[comp_model] || line->backsector != sec ?
               line->backsector : 
               NULL : 
            line->frontsector;
}

//
// P_FindLowestFloorSurrounding()
//
// Returns the fixed point value of the lowest floor height
// in the sector passed or its surrounding sectors.
//
// killough 11/98: reformatted
//
fixed_t P_FindLowestFloorSurrounding(const sector_t* sec)
{
   fixed_t floor = sec->floorheight;
   const sector_t *other;
   int i;
   
   for(i = 0; i < sec->linecount; i++)
   {
      if((other = getNextSector(sec->lines[i], sec)) &&
         other->floorheight < floor)
         floor = other->floorheight;
   }
   
   return floor;
}

//
// P_FindHighestFloorSurrounding()
//
// Passed a sector, returns the fixed point value of the largest
// floor height in the surrounding sectors, not including that passed
//
// NOTE: if no surrounding sector exists -32000*FRACUINT is returned
//       if compatibility then -500*FRACUNIT is the smallest return possible
//
// killough 11/98: reformatted
//
fixed_t P_FindHighestFloorSurrounding(const sector_t *sec)
{
   fixed_t floor = -500*FRACUNIT;
   const sector_t *other;
   int i;

   //jff 1/26/98 Fix initial value for floor to not act differently
   //in sections of wad that are below -500 units
   
   if(!comp[comp_model])          //jff 3/12/98 avoid ovf
      floor = -32000*FRACUNIT;      // in height calculations

   for(i = 0; i < sec->linecount; i++)
   {
      if((other = getNextSector(sec->lines[i],sec)) &&
         other->floorheight > floor)
         floor = other->floorheight;
   }
   
   return floor;
}

//
// P_FindNextHighestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the smallest floor height in a surrounding sector larger than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// Rewritten by Lee Killough to avoid fixed array and to be faster
//
fixed_t P_FindNextHighestFloor(const sector_t *sec, int currentheight)
{
   sector_t *other;
   int i;
   
   for(i=0; i < sec->linecount; i++)
   {
      if((other = getNextSector(sec->lines[i],sec)) &&
         other->floorheight > currentheight)
      {
         int height = other->floorheight;
         while (++i < sec->linecount)
         {
            if((other = getNextSector(sec->lines[i],sec)) &&
               other->floorheight < height &&
               other->floorheight > currentheight)
               height = other->floorheight;
         }
         return height;
      }
   }
   return currentheight;
}

//
// P_FindNextLowestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the largest floor height in a surrounding sector smaller than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t P_FindNextLowestFloor(const sector_t *sec, int currentheight)
{
   sector_t *other;
   int i;
   
   for(i=0; i < sec->linecount; i++)
   {
      if((other = getNextSector(sec->lines[i],sec)) &&
         other->floorheight < currentheight)
      {
         int height = other->floorheight;
         while (++i < sec->linecount)
         {
            if((other = getNextSector(sec->lines[i],sec)) &&
               other->floorheight > height &&
               other->floorheight < currentheight)
               height = other->floorheight;
         }
         return height;
      }
   }
   return currentheight;
}

//
// P_FindNextLowestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the largest ceiling height in a surrounding sector smaller than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t P_FindNextLowestCeiling(const sector_t *sec, int currentheight)
{
   sector_t *other;
   int i;
   
   for(i=0 ;i < sec->linecount ; i++)
   {
      if((other = getNextSector(sec->lines[i],sec)) &&
         other->ceilingheight < currentheight)
      {
         int height = other->ceilingheight;
         while (++i < sec->linecount)
         {
            if((other = getNextSector(sec->lines[i],sec)) &&
               other->ceilingheight > height &&
               other->ceilingheight < currentheight)
               height = other->ceilingheight;
         }
        return height;
      }
   }
   return currentheight;
}

//
// P_FindNextHighestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the smallest ceiling height in a surrounding sector larger than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t P_FindNextHighestCeiling(const sector_t *sec, int currentheight)
{
   sector_t *other;
   int i;
   
   for(i=0; i < sec->linecount; i++)
   {
      if((other = getNextSector(sec->lines[i],sec)) &&
         other->ceilingheight > currentheight)
      {
         int height = other->ceilingheight;
         while (++i < sec->linecount)
         {
            if((other = getNextSector(sec->lines[i],sec)) &&
               other->ceilingheight < height &&
               other->ceilingheight > currentheight)
               height = other->ceilingheight;
         }
         return height;
      }
   }
   return currentheight;
}

//
// P_FindLowestCeilingSurrounding()
//
// Passed a sector, returns the fixed point value of the smallest
// ceiling height in the surrounding sectors, not including that passed
//
// NOTE: if no surrounding sector exists 32000*FRACUINT is returned
//       but if compatibility then MAXINT is the return
//
// killough 11/98: reformatted
//
fixed_t P_FindLowestCeilingSurrounding(const sector_t* sec)
{
   const sector_t *other;
   fixed_t height = D_MAXINT;
   int i;

   if(!comp[comp_model])
      height = 32000*FRACUNIT; //jff 3/12/98 avoid ovf in height calculations

   if(demo_version >= 333)
   {
      // SoM: ignore attached sectors.
      for(i = 0; i < sec->linecount; i++)
      {
         if((other = getNextSector(sec->lines[i],sec)) &&
            other->ceilingheight < height)
         {
            int j;

            for(j = 0; j < sec->c_asurfacecount; j++)
               if(sec->c_asurfaces[j].sector == other)
                  break;
            
            if(j == sec->c_asurfacecount)
               height = other->ceilingheight;
         }
      }
   }
   else
   {      
      for(i = 0; i < sec->linecount; i++)
      {
         if((other = getNextSector(sec->lines[i],sec)) && other->ceilingheight < height)
            height = other->ceilingheight;
      }
   }

   return height;
}

//
// P_FindHighestCeilingSurrounding()
//
// Passed a sector, returns the fixed point value of the largest
// ceiling height in the surrounding sectors, not including that passed
//
// NOTE: if no surrounding sector exists -32000*FRACUINT is returned
//       but if compatibility then 0 is the smallest return possible
//
// killough 11/98: reformatted
//
fixed_t P_FindHighestCeilingSurrounding(const sector_t* sec)
{
   const sector_t *other;
   fixed_t height = 0;
   int i;

   //jff 1/26/98 Fix initial value for floor to not act differently
   //in sections of wad that are below 0 units

   if(!comp[comp_model])
      height = -32000*FRACUNIT; //jff 3/12/98 avoid ovf in
   
   // height calculations
   for(i=0; i < sec->linecount; i++)
      if((other = getNextSector(sec->lines[i],sec)) &&
         other->ceilingheight > height)
         height = other->ceilingheight;
      
   return height;
}

//
// P_FindShortestTextureAround()
//
// Passed a sector number, returns the shortest lower texture on a
// linedef bounding the sector.
//
// Note: If no lower texture exists 32000*FRACUNIT is returned.
//       but if compatibility then MAXINT is returned
//
// jff 02/03/98 Add routine to find shortest lower texture
//
// killough 11/98: reformatted
//
fixed_t P_FindShortestTextureAround(int secnum)
{
   const sector_t *sec = &sectors[secnum];
   int i, minsize = D_MAXINT;

   // haleyjd 05/07/04: repair texture comparison error that was
   // fixed in BOOM v2.02 but missed in MBF -- texture #0 is used
   // for "-", meaning no texture, but if used as an index, will get
   // the height of the first "garbage" texture (ie. AASTINKY)
   int lowtexnum = (demo_version == 202 || demo_version >= 331);

   if(!comp[comp_model])
      minsize = 32000<<FRACBITS; //jff 3/13/98 prevent overflow in height calcs
   
   for(i = 0; i < sec->linecount; i++)
   {
      if(twoSided(secnum, i))
      {
         const side_t *side;
         if((side = getSide(secnum,i,0))->bottomtexture >= lowtexnum &&
            textures[side->bottomtexture]->heightfrac < minsize)
            minsize = textures[side->bottomtexture]->heightfrac;
         if((side = getSide(secnum,i,1))->bottomtexture >= lowtexnum &&
            textures[side->bottomtexture]->heightfrac < minsize)
            minsize = textures[side->bottomtexture]->heightfrac;
      }
   }
   
   return minsize;
}

//
// P_FindShortestUpperAround()
//
// Passed a sector number, returns the shortest upper texture on a
// linedef bounding the sector.
//
// Note: If no upper texture exists 32000*FRACUNIT is returned.
//       but if compatibility then MAXINT is returned
//
// jff 03/20/98 Add routine to find shortest upper texture
//
// killough 11/98: reformatted
//
fixed_t P_FindShortestUpperAround(int secnum)
{
   const sector_t *sec = &sectors[secnum];
   int i, minsize = D_MAXINT;

   // haleyjd 05/07/04: repair texture comparison error that was
   // fixed in BOOM v2.02 but missed in MBF -- texture #0 is used
   // for "-", meaning no texture, but if used as an index, will get
   // the height of the first "garbage" texture (ie. AASTINKY)
   int lowtexnum = (demo_version == 202 || demo_version >= 331);

   if(!comp[comp_model])
      minsize = 32000<<FRACBITS; //jff 3/13/98 prevent overflow in height calcs

   for(i = 0; i < sec->linecount; i++)
   {
      if(twoSided(secnum, i))
      {
         const side_t *side;
         if((side = getSide(secnum,i,0))->toptexture >= lowtexnum)
            if(textures[side->toptexture]->heightfrac < minsize)
               minsize = textures[side->toptexture]->heightfrac;
         if((side = getSide(secnum,i,1))->toptexture >= lowtexnum)
            if(textures[side->toptexture]->heightfrac < minsize)
               minsize = textures[side->toptexture]->heightfrac;
      }
   }

   return minsize;
}

//
// P_FindModelFloorSector()
//
// Passed a floor height and a sector number, return a pointer to a
// a sector with that floor height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model floor
//  around a sector specified by sector number
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using FloorMoveThinker
//
// killough 11/98: reformatted
// 
sector_t *P_FindModelFloorSector(fixed_t floordestheight, int secnum)
{
   sector_t *sec = &sectors[secnum]; //jff 3/2/98 woops! better do this

   //jff 5/23/98 don't disturb sec->linecount while searching
   // but allow early exit in old demos

   int i, lineCount = sec->linecount;
   
   for(i = 0; 
       i < (demo_compatibility && sec->linecount < lineCount ? sec->linecount : lineCount); 
       i++)
   {
      if(twoSided(secnum, i) &&
         (sec = getSector(secnum, i,
          getSide(secnum,i,0)->sector-sectors == secnum))->floorheight == floordestheight)
      {
         return sec;
      }
   }
      
   return NULL;
}

//
// P_FindModelCeilingSector()
//
// Passed a ceiling height and a sector number, return a pointer to a
// a sector with that ceiling height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model ceiling
//  around a sector specified by sector number
//  used only from generalized ceiling types
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using CeilingThinker
//
// killough 11/98: reformatted
// haleyjd 09/23/02: reformatted again

sector_t *P_FindModelCeilingSector(fixed_t ceildestheight, int secnum)
{
   sector_t *sec = &sectors[secnum]; //jff 3/2/98 woops! better do this

   //jff 5/23/98 don't disturb sec->linecount while searching
   // but allow early exit in old demos

   int i, lineCount = sec->linecount;

   for(i = 0; 
       i < (demo_compatibility && sec->linecount < lineCount ? sec->linecount : lineCount); 
       i++)
   {
      if(twoSided(secnum, i) &&
         (sec = getSector(secnum, i,
          getSide(secnum,i,0)->sector-sectors == secnum))->ceilingheight == ceildestheight)
      {
         return sec;
      }
   }

   return NULL;
}

//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//

// Find the next sector with the same tag as a linedef.
// Rewritten by Lee Killough to use chained hashing to improve speed

int P_FindSectorFromLineTag(const line_t *line, int start)
{
   start = 
      (start >= 0 ? sectors[start].nexttag :
       sectors[(unsigned int)line->tag % (unsigned int)numsectors].firsttag);
  
   while(start >= 0 && sectors[start].tag != line->tag)
      start = sectors[start].nexttag;
   
   return start;
}

// killough 4/16/98: Same thing, only for linedefs

int P_FindLineFromLineTag(const line_t *line, int start)
{
   start = 
      (start >= 0 ? lines[start].nexttag :
       lines[(unsigned int)line->tag % (unsigned int)numlines].firsttag);
  
   while(start >= 0 && lines[start].tag != line->tag)
      start = lines[start].nexttag;
   
   return start;
}

// sf: same thing but from just a number

int P_FindSectorFromTag(const int tag, int start)
{
   start = 
      (start >= 0 ? sectors[start].nexttag :
       sectors[(unsigned int)tag % (unsigned int)numsectors].firsttag);
  
   while(start >= 0 && sectors[start].tag != tag)
      start = sectors[start].nexttag;
  
   return start;
}

//
// P_InitTagLists
//
// Hash the sector tags across the sectors and linedefs.
//
static void P_InitTagLists()
{
   int i;
   
   for(i = numsectors; --i >= 0; )   // Initially make all slots empty.
      sectors[i].firsttag = -1;
   
   for(i = numsectors; --i >= 0; )   // Proceed from last to first sector
   {                                 // so that lower sectors appear first
      int j = (unsigned int)sectors[i].tag % (unsigned int)numsectors; // Hash func
      sectors[i].nexttag = sectors[j].firsttag;   // Prepend sector to chain
      sectors[j].firsttag = i;
   }
   
   // killough 4/17/98: same thing, only for linedefs
   
   for(i = numlines; --i >= 0; )   // Initially make all slots empty.
      lines[i].firsttag = -1;
   
   for(i = numlines; --i >= 0; )   // Proceed from last to first linedef
   {                               // so that lower linedefs appear first
      // haleyjd 05/16/09: unified id into tag;
      // added mapformat parameter to test here:
      if(LevelInfo.mapFormat == LEVEL_FORMAT_DOOM || 
         LevelInfo.mapFormat == LEVEL_FORMAT_PSX  ||
         lines[i].tag != -1)
      {
         int j = (unsigned int)lines[i].tag % (unsigned int)numlines; // Hash func
         lines[i].nexttag = lines[j].firsttag;   // Prepend linedef to chain
         lines[j].firsttag = i;
      }
   }
}

//
// P_FindMinSurroundingLight
//
// Passed a sector and a light level, returns the smallest light level
// in a surrounding sector less than that passed. If no smaller light
// level exists, the light level passed is returned.
//
// killough 11/98: reformatted
//
int P_FindMinSurroundingLight(const sector_t *sector, int min)
{
   const sector_t *check;

   for(int i = 0; i < sector->linecount; i++)
   {
      if((check = getNextSector(sector->lines[i], sector)) &&
         check->lightlevel < min)
         min = check->lightlevel;
   }

   return min;
}

//
// P_SectorActive()
//
// Passed a linedef special class (floor, ceiling, lighting) and a sector
// returns whether the sector is already busy with a linedef special of the
// same class. If old demo compatibility true, all linedef special classes
// are the same.
//
// jff 2/23/98 added to prevent old demos from
//  succeeding in starting multiple specials on one sector
//
// killough 11/98: reformatted

int P_SectorActive(special_e t, const sector_t *sec)
{
   return demo_compatibility ?  // return whether any thinker is active
     sec->floordata || sec->ceilingdata || sec->lightingdata :
     t == floor_special ? !!sec->floordata :        // return whether
     t == ceiling_special ? !!sec->ceilingdata :    // thinker of same
     t == lighting_special ? !!sec->lightingdata :  // type is active
     1; // don't know which special, must be active, shouldn't be here
}

//
// P_IsSecret()
//
// Passed a sector, returns if the sector secret type is still active, i.e.
// secret type is set and the secret has not yet been obtained.
//
// jff 3/14/98 added to simplify checks for whether sector is secret
//  in automap and other places
//
bool P_IsSecret(const sector_t *sec)
{
   return (sec->flags & SECF_SECRET);
}

//
// P_WasSecret()
//
// Passed a sector, returns if the sector secret type is was active, i.e.
// secret type was set and the secret has been obtained already.
//
// jff 3/14/98 added to simplify checks for whether sector is secret
//  in automap and other places
//
bool P_WasSecret(const sector_t *sec)
{
   return (sec->intflags & SIF_WASSECRET) == SIF_WASSECRET;
}

//
// StartLineScript
//
// haleyjd 06/01/04: starts a script from a linedef.
//
void P_StartLineScript(line_t *line, Mobj *thing)
{
   ACS_ExecuteScriptNumber(line->tag, gamemap, 0, line->args, NUMLINEARGS, 
                           thing, line, 0);
}

//=============================================================================
//
// Events
//
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_CrossSpecialLine - Walkover Trigger Dispatcher
//
// Called every time a thing origin is about
//  to cross a line with a non 0 special, whether a walkover type or not.
//
// jff 02/12/98 all W1 lines were fixed to check the result from the EV_
//  function before clearing the special. This avoids losing the function
//  of the line, should the sector already be active when the line is
//  crossed. Change is qualified by demo_compatibility.
//
// killough 11/98: change linenum parameter to a line_t pointer

void P_CrossSpecialLine(line_t *line, int side, Mobj *thing)
{
   // EV_SPECIALS TODO: This function should return success or failure to 
   // the caller.
   EV_ActivateSpecialLineWithSpac(line, side, thing, SPAC_CROSS);
}

//
// P_ShootSpecialLine - Gun trigger special dispatcher
//
// Called when a thing shoots a special line with bullet, shell, saw, or fist.
//
// jff 02/12/98 all G1 lines were fixed to check the result from the EV_
// function before clearing the special. This avoids losing the function
// of the line, should the sector already be in motion when the line is
// impacted. Change is qualified by demo_compatibility.
//
// haleyjd 03/13/05: added side argument for param line specials
//
void P_ShootSpecialLine(Mobj *thing, line_t *line, int side)
{
   // EV_SPECIALS TODO: This function should return success or failure to 
   // the caller.
   EV_ActivateSpecialLineWithSpac(line, side, thing, SPAC_IMPACT);
}

        // sf: changed to enable_nuke for console
int enable_nuke = 1;  // killough 12/98: nukage disabling cheat

//
// P_PlayerInSpecialSector
//
// Called every tic that the player origin is in a special sector
//
// Changed to ignore sector types the engine does not recognize
//
void P_PlayerInSpecialSector(player_t *player)
{
   sector_t *sector = player->mo->subsector->sector;

   // TODO: waterzones should damage whenever you're in them
   // Falling, not all the way down yet?
   // Sector specials don't apply in mid-air
   if(player->mo->z != sector->floorheight)
      return;

   // haleyjd 12/28/08: We handle secrets uniformly now, through the
   // sector flags field. We also keep track of former secret status
   // much more smartly (and permanently).
   if(sector->flags & SECF_SECRET)
   {
      player->secretcount++;             // credit the player
      sector->intflags |= SIF_WASSECRET; // remember secretness for automap
      sector->flags &= ~SECF_SECRET;     // clear the flag
   }

   // Has hit ground

   // haleyjd 12/31/08: generalized sector damage engine
   if(enable_nuke && sector->damage > 0) // killough 12/98: nukage disabling cheat
   {
      if(!player->powers[pw_ironfeet]          ||  // no rad suit?
         sector->damageflags & SDMG_IGNORESUIT ||  // ignores suit?
         (sector->damageflags & SDMG_LEAKYSUIT &&  // suit leaks?
          (P_Random(pr_slimehurt) < 5))
        )
      {
         // disables god mode?
         // killough 2/21/98: add compatibility switch on godmode cheat clearing;
         //                   does not affect invulnerability
         if(sector->damageflags & SDMG_ENDGODMODE && comp[comp_god])
            player->cheats &= ~CF_GODMODE;

         // check time
         if(sector->damagemask <= 0 || !(leveltime % sector->damagemask))
         {
            // do the damage
            P_DamageMobj(player->mo, NULL, NULL, sector->damage, 
                         sector->damagemod);

            // possibly cause a terrain hit
            if(sector->damageflags & SDMG_TERRAINHIT)
               E_HitFloor(player->mo);
         }

         // possibly exit the level
         if(sector->damageflags & SDMG_EXITLEVEL && player->health <= 10)
            G_ExitLevel();
      }
   }

   // phares 3/19/98:
   //
   // If FRICTION_MASK or PUSH_MASK is set, we don't care at this
   // point, since the code to deal with those situations is
   // handled by Thinkers.
}

//
// P_PlayerOnSpecialFlat
//
// haleyjd 08/23/05: Inflicts terrain-based environmental damage
// on players.
//
void P_PlayerOnSpecialFlat(const player_t *player)
{
   //sector_t *sector = player->mo->subsector->sector;
   ETerrain *terrain;
   fixed_t floorz;

   if(full_demo_version < make_full_version(339, 21))
      floorz = player->mo->subsector->sector->floorheight;
   else
      floorz = player->mo->floorz; // use more correct floorz

   // TODO: waterzones should damage whenever you're in them
   // Falling, not all the way down yet?
   // Sector specials don't apply in mid-air
   if(player->mo->z != floorz)
      return;

   terrain = E_GetThingFloorType(player->mo, true);

   if(enable_nuke && // haleyjd: allow nuke cheat to disable terrain damage too
      terrain->damageamount && !(leveltime & terrain->damagetimemask))
   {
      P_DamageMobj(player->mo, NULL, NULL, terrain->damageamount,
                   terrain->damagetype);

      if(terrain->splash)
         S_StartSoundName(player->mo, terrain->splash->sound);
   }
}

//
// P_UpdateSpecials
//
// Check level timer, frag counter,
// animate flats, scroll walls,
// change button textures
//
// Reads and modifies globals:
//  levelTimer, levelTimeCount,
//  levelFragLimit, levelFragLimitCount
//

// sf: rearranged variables

int             levelTimeLimit;
int             levelFragLimit; // Ty 03/18/98 Added -frags support

void P_UpdateSpecials()
{
   anim_t *anim;
   int    pic;

   // Downcount level timer, exit level if elapsed
   if(levelTimeLimit && leveltime >= levelTimeLimit*35*60 )
      G_ExitLevel();

   // Check frag counters, if frag limit reached, exit level // Ty 03/18/98
   //  Seems like the total frags should be kept in a simple
   //  array somewhere, but until they are...
   if(levelFragLimit)  // we used -frags so compare count
   {
      int pnum;
      for(pnum = 0; pnum < MAXPLAYERS; pnum++)
      {
         if(!playeringame[pnum])
            continue;
          // sf: use hu_frags.c frag counter
         if(players[pnum].totalfrags >= levelFragLimit)
            break;
      }
      if(pnum < MAXPLAYERS)       // sf: removed exitflag (ugh)
         G_ExitLevel();
   }

   // Animate flats and textures globally
   for(anim = anims; anim < lastanim; anim++)
   {
      for(int i = anim->basepic; i < anim->basepic + anim->numpics; i++)
      {
         if((i >= flatstart && i < flatstop && r_swirl) || anim->speed > 65535 || anim->numpics == 1)
            texturetranslation[i] = i;
         else
         {
            pic = anim->basepic + 
                  ((leveltime/anim->speed + i) % anim->numpics);

            texturetranslation[i] = pic;
         }
      }
   }
   
   // update buttons (haleyjd 10/16/05: button stuff -> p_switch.c)
   P_RunButtons();
}

//=============================================================================
//
// Sector and Line special thinker spawning at level startup
//

//
// P_SetupHeightTransfer
//
// haleyjd 03/04/07: New function to handle setting up the 242 deep water and
// its related effects. We want to transfer certain properties from the
// heightsec to the real sector now, so that normal sectors can have those
// properties without being part of a 242 effect.
//
// Namely, colormaps.
//
static void P_SetupHeightTransfer(int linenum, int secnum)
{
   int s;
   sector_t *heightsec = &sectors[secnum];

   for(s = -1; (s = P_FindSectorFromLineTag(lines + linenum, s)) >= 0; )
   {
      sectors[s].heightsec = secnum;

      // transfer colormaps to affected sectors instead of getting them from
      // the heightsec during the rendering process
      sectors[s].topmap    = heightsec->topmap;
      sectors[s].midmap    = heightsec->midmap;
      sectors[s].bottommap = heightsec->bottommap;
   }
}

//
// P_SpawnSpecials
//
// After the map has been loaded, scan for specials that spawn thinkers
//
void P_SpawnSpecials()
{
   // sf: -timer moved to d_main.c
   //     -avg also
   
   // sf: changed -frags: not loaded at start of every level
   //     to allow changing by console

   // Init special sectors.
   EV_SpawnSectorSpecials();

   P_RemoveAllActiveCeilings();  // jff 2/22/98 use killough's scheme
   
   PlatThinker::RemoveAllActivePlats(); // killough

   // clear buttons (haleyjd 10/16/05: button stuff -> p_switch.c)
   P_ClearButtons();

   // P_InitTagLists() must be called before P_FindSectorFromLineTag()
   // or P_FindLineFromLineTag() can be called.

   P_InitTagLists();   // killough 1/30/98: Create xref tables for tags
   
   P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers
   
   P_SpawnFriction();  // phares 3/12/98: New friction model using linedefs
   
   P_SpawnPushers();   // phares 3/20/98: New pusher model using linedefs

   for(int i = 0; i < numlines; i++)
   {
      line_t *line = &lines[i];
      int staticFn = EV_StaticInitForSpecial(line->special);

      switch(staticFn)
      {
         int s, sec;

         // killough 3/7/98:
         // support for drawn heights coming from different sector
      case EV_STATIC_TRANSFER_HEIGHTS:
         sec = static_cast<int>(sides[*lines[i].sidenum].sector-sectors);
         P_SetupHeightTransfer(i, sec); // haleyjd 03/04/07
         break;

         // killough 3/16/98: Add support for setting
         // floor lighting independently (e.g. lava)
      case EV_STATIC_LIGHT_TRANSFER_FLOOR:
         sec = static_cast<int>(sides[*lines[i].sidenum].sector-sectors);
         for(s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
            sectors[s].floorlightsec = sec;
         break;

         // killough 4/11/98: Add support for setting
         // ceiling lighting independently
      case EV_STATIC_LIGHT_TRANSFER_CEILING:
         sec = static_cast<int>(sides[*lines[i].sidenum].sector-sectors);
         for(s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
            sectors[s].ceilinglightsec = sec;
         break;

         // killough 10/98:
         //
         // Support for sky textures being transferred from sidedefs.
         // Allows scrolling and other effects (but if scrolling is
         // used, then the same sector tag needs to be used for the
         // sky sector, the sky-transfer linedef, and the scroll-effect
         // linedef). Still requires user to use F_SKY1 for the floor
         // or ceiling texture, to distinguish floor and ceiling sky.

      case EV_STATIC_SKY_TRANSFER:         // Regular sky
      case EV_STATIC_SKY_TRANSFER_FLIPPED: // Same, only flipped
         for(s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
            sectors[s].sky = i | PL_SKYFLAT;
         break;

         // SoM 9/19/02
         // Support for attaching sectors to each other. When a sector
         // is attached to another sector, the master sector's floor
         // and/or ceiling will move all 3d sides of the attached
         // sectors. The 3d sides, will then be tested in P_MoveFlat
         // and will affect weather or not the sector will keep moving,
         // thus keeping compatibility for all thinker types.
      case EV_STATIC_3DMIDTEX_ATTACH_FLOOR:
         P_AttachLines(&lines[i], false);
         break;
      case EV_STATIC_3DMIDTEX_ATTACH_CEILING:
         P_AttachLines(&lines[i], true);
         break;

         // SoM 12/10/03: added skybox/portal specials
         // haleyjd 01/24/04: functionalized code to reduce footprint
      case EV_STATIC_PORTAL_PLANE_CEILING:
      case EV_STATIC_PORTAL_PLANE_FLOOR:
      case EV_STATIC_PORTAL_PLANE_CEILING_FLOOR:
      case EV_STATIC_PORTAL_HORIZON_CEILING:
      case EV_STATIC_PORTAL_HORIZON_FLOOR:
      case EV_STATIC_PORTAL_HORIZON_CEILING_FLOOR:
      case EV_STATIC_PORTAL_SKYBOX_CEILING:
      case EV_STATIC_PORTAL_SKYBOX_FLOOR:
      case EV_STATIC_PORTAL_SKYBOX_CEILING_FLOOR:
      case EV_STATIC_PORTAL_ANCHORED_CEILING:
      case EV_STATIC_PORTAL_ANCHORED_FLOOR:
      case EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR:
      case EV_STATIC_PORTAL_TWOWAY_CEILING:
      case EV_STATIC_PORTAL_TWOWAY_FLOOR:
      case EV_STATIC_PORTAL_LINKED_CEILING:
      case EV_STATIC_PORTAL_LINKED_FLOOR:
      case EV_STATIC_PORTAL_LINKED_LINE2LINE:
         P_SpawnPortal(&lines[i], staticFn);
         break;
      
         // haleyjd 02/28/07: Line_SetIdentification
         // TODO: allow upper byte in args[2] for Hexen-format maps
      case EV_STATIC_LINE_SET_IDENTIFICATION: 
         P_SetLineID(&lines[i], lines[i].args[0]);
         lines[i].special = 0; // clear special
         break;

         // SoM 10/14/07: Surface/Surface attachments
      case EV_STATIC_ATTACH_SET_CEILING_CONTROL:
      case EV_STATIC_ATTACH_SET_FLOOR_CONTROL:
         P_AttachSectors(&lines[i], staticFn);
         break;

         // SoM 05/10/09: Slopes
      case EV_STATIC_SLOPE_FSEC_FLOOR:
      case EV_STATIC_SLOPE_FSEC_CEILING:
      case EV_STATIC_SLOPE_FSEC_FLOOR_CEILING:
      case EV_STATIC_SLOPE_BSEC_FLOOR:
      case EV_STATIC_SLOPE_BSEC_CEILING:
      case EV_STATIC_SLOPE_BSEC_FLOOR_CEILING:
      case EV_STATIC_SLOPE_BACKFLOOR_FRONTCEILING:
      case EV_STATIC_SLOPE_FRONTFLOOR_BACKCEILING:
         P_SpawnSlope_Line(i, staticFn);
         break;

         // haleyjd 10/16/10: ExtraData sector
      case EV_STATIC_EXTRADATA_SECTOR:         
         E_LoadSectorExt(&lines[i]);
         break;

      default: // Not a static special, or not handled here
         break;
      }
   }

   // SoM: This seems like the place to put this.
   if(!P_BuildLinkTable())
   {
      // SoM: There was an error... so kill the groupids
      for(int i = 0; i < numsectors; i++)
         R_SetSectorGroupID(sectors + i, 0);
   }

   // haleyjd 02/20/06: spawn polyobjects
   Polyobj_InitLevel();

   // haleyjd 06/18/14: spawn level actions
   P_SpawnLevelActions();
}

// 
// P_SpawnDeferredSpecials
//
// SoM: Specials that copy slopes, etc., need to be collected in a separate pass
//
void P_SpawnDeferredSpecials()
{
   for(int i = 0; i < numlines; i++)
   {
      line_t *line = &lines[i];

      // haleyjd 02/05/13: lookup the static init function
      int staticFn = EV_StaticInitForSpecial(line->special);

      switch(staticFn)
      {         
      case EV_STATIC_SLOPE_FRONTFLOOR_TAG: 
      case EV_STATIC_SLOPE_FRONTCEILING_TAG:
      case EV_STATIC_SLOPE_FRONTFLOORCEILING_TAG:
         // SoM: Copy slopes
         P_CopySectorSlope(line, staticFn);
         break;

      default: // Not a function handled here
         break;
      }
   }

   for(int i = 0; i < numsectors; i++)
   {
      sector_t *sec = &sectors[i];

      // spawn phased light sequences
      if(sec->flags & SECF_PHASEDLIGHT)
         PhasedLightThinker::SpawnSequence(sec, 1);
   }
}

// haleyjd 04/11/10:
// e6y
// restored boom's friction code

//
// Add a friction thinker to the thinker list
//
// Add_Friction adds a new friction thinker to the list of active thinkers.
//
static void Add_Friction(int friction, int movefactor, int affectee)
{
   FrictionThinker *f = new FrictionThinker;

   f->friction   = friction;
   f->movefactor = movefactor;
   f->affectee   = affectee;

   f->addThinker();
}

IMPLEMENT_THINKER_TYPE(FrictionThinker)

//
// This is where abnormal friction is applied to objects in the sectors.
// A friction thinker has been spawned for each sector where less or
// more friction should be applied. The amount applied is proportional to
// the length of the controlling linedef.
//
void FrictionThinker::Think()
{
   sector_t   *sec;
   Mobj     *thing;
   msecnode_t *node;

   if(compatibility || !variable_friction)
      return;

   sec = sectors + this->affectee;

   // Be sure the special sector type is still turned on. If so, proceed.
   // Else, bail out; the sector type has been changed on us.
   if(!(sec->flags & SECF_FRICTION))
      return;

   // Assign the friction value to players on the floor, non-floating,
   // and clipped. Normally the object's friction value is kept at
   // ORIG_FRICTION and this thinker changes it for icy or muddy floors.

   // In Phase II, you can apply friction to Things other than players.

   // When the object is straddling sectors with the same
   // floorheight that have different frictions, use the lowest
   // friction value (muddy has precedence over icy).

   node = sec->touching_thinglist; // things touching this sector
   while(node)
   {
      thing = node->m_thing;
      if(thing->player &&
         !(thing->flags & (MF_NOGRAVITY | MF_NOCLIP)) &&
         thing->z <= sec->floorheight)
      {
         if((thing->friction == ORIG_FRICTION) ||     // normal friction?
            (this->friction < thing->friction))
         {
            thing->friction   = this->friction;
            thing->movefactor = this->movefactor;
         }
      }
      node = node->m_snext;
   }
}

//
// FrictionThinker::serialize
//
// haleyjd 12/25/10: This was actually missing, but is in fact needed in the
// event that a user tries to save the game during playback of a BOOM demo.
//
void FrictionThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << friction << movefactor << affectee;
}

//=============================================================================
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects
//
// As the player moves, friction is applied by decreasing the x and y
// momentum values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.
//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
//=====================================

//
// P_SpawnFriction
//
// Initialize the sectors where friction is increased or decreased
//
static void P_SpawnFriction()
{
   int i;
   line_t *line = lines;
   
   // killough 8/28/98: initialize all sectors to normal friction first
   for(i = 0; i < numsectors; i++)
   {
      // haleyjd: special hacks may have already set the friction, so
      // skip any value that's not zero (now zeroed in P_LoadSectors)
      if(!sectors[i].friction)
      {
         sectors[i].friction   = ORIG_FRICTION;
         sectors[i].movefactor = ORIG_FRICTION_FACTOR;
      }
   }

   // haleyjd 02/03/13: get the friction transfer static init binding
   int fricspec;
   if(!(fricspec = EV_SpecialForStaticInit(EV_STATIC_FRICTION_TRANSFER)))
      return; // not defined for this map

   for(i = 0 ; i < numlines ; i++, line++)
   {
      if(line->special == fricspec)
      {
         int length   = P_AproxDistance(line->dx, line->dy) >> FRACBITS;
         int friction = (0x1EB8 * length) / 0x80 + 0xD000;
         int movefactor, s;

         // The following check might seem odd. At the time of movement,
         // the move distance is multiplied by 'friction/0x10000', so a
         // higher friction value actually means 'less friction'.

         if(friction > ORIG_FRICTION)       // ice
            movefactor = ((0x10092  - friction) * 0x70) / 0x158;
         else
            movefactor = ((friction - 0xDB34  ) * 0x0A) / 0x80;

         if(demo_version >= 203)
         { 
            // killough 8/28/98: prevent odd situations
            if(friction > FRACUNIT)
               friction = FRACUNIT;
            if(friction < 0)
               friction = 0;
            if(movefactor < 32)
               movefactor = 32;
         }

         for(s = -1; (s = P_FindSectorFromLineTag(line, s)) >= 0 ;)
         {
            // killough 8/28/98:
            //
            // Instead of spawning thinkers, which are slow and expensive,
            // modify the sector's own friction values. Friction should be
            // a property of sectors, not objects which reside inside them.
            // Original code scanned every object in every friction sector
            // on every tic, adjusting its friction, putting unnecessary
            // drag on CPU. New code adjusts friction of sector only once
            // at level startup, and then uses this friction value.
            
            // e6y: boom's friction code for boom compatibility
            if(!demo_compatibility && demo_version < 203)
               Add_Friction(friction, movefactor, s);
            
            sectors[s].friction   = friction;
            sectors[s].movefactor = movefactor;
         }
      }
   }
}

//
// phares 3/12/98: End of friction effects
//
//=============================================================================

// haleyjd 08/22/05: TerrainTypes moved to e_ttypes.c

//==========================
//
// haleyjd: Misc New Stuff
//
//==========================

//
// P_FindLine  
//
// A much nicer line finding function.
// haleyjd 02/27/07: rewritten to get rid of Raven code and to speed up in the
// same manner as P_FindLineFromLineTag by using in-table tag hash.
//
line_t *P_FindLine(int tag, int *searchPosition)
{
   line_t *line = NULL;
   
   int start = 
      (*searchPosition >= 0 ? lines[*searchPosition].nexttag :
       lines[(unsigned int)tag % (unsigned int)numlines].firsttag);
  
   while(start >= 0 && lines[start].tag != tag)
      start = lines[start].nexttag;

   if(start >= 0)
      line = &lines[start];

   *searchPosition = start;
   
   return line;
}

//
// P_SetLineID
//
// haleyjd 05/16/09: For Hexen
//
void P_SetLineID(line_t *line, int id)
{
   // remove from any chain it's already in
   if(line->tag >= 0)
   {
      int chain = (unsigned int)line->tag % (unsigned int)numlines;
      int i;
      line_t *prevline = NULL;

      // walk the chain
      for(i = lines[chain].firsttag; i != -1; i = lines[i].nexttag)
      {
         if(line == &lines[i])
         {
            // remove this line
            if(prevline)
               prevline->nexttag = line->nexttag; // prev->next = this->next
            else
               lines[chain].firsttag = line->nexttag; // list = this->next
         }

         // not a match, keep looking
         // record this line in case it's the one before the one we're looking for
         prevline = &lines[i]; 
      }
   }

   // set the new id
   line->tag = id;
   line->nexttag = -1;

   if(line->tag >= 0)
   {
      int chain = (unsigned int)line->tag % (unsigned int)numlines; // Hash func
   
      line->nexttag = lines[chain].firsttag;   // Prepend linedef to chain
      lines[chain].firsttag = static_cast<int>(line - lines);
   }
}

//=============================================================================
//
// haleyjd 09/06/07: Sector Special Transfer Logic
//
// This new set of functions and the corresponding spectransfer_t structure,
// which is now held inside floor and ceiling movement thinkers, allows
// extending the special transfer logic to new fields in the sector_t
// structure. Besides eliminating redundant logic formerly scattered throughout
// the floor and ceiling modules, this is necessitated by some ExtraData sector
// features.
//

// haleyjd 12/28/08: the following sector flags are considered to be part of
// the sector special (not all sector flags may be considered to be such).

#define SPECIALFLAGSMASK \
   (SECF_SECRET|SECF_FRICTION|SECF_PUSH|SECF_KILLSOUND|SECF_KILLMOVESOUND)

//
// P_SetupSpecialTransfer
//
// haleyjd 09/06/07: This function is called to populate a spectransfer_t
// structure with data from a sector.
//
void P_SetupSpecialTransfer(const sector_t *sector, spectransfer_t *spec)
{
   spec->newspecial  = sector->special;
   spec->flags       = sector->flags & SPECIALFLAGSMASK;
   spec->damage      = sector->damage;
   spec->damagemask  = sector->damagemask;
   spec->damagemod   = sector->damagemod;
   spec->damageflags = sector->damageflags;
}

//
// P_ZeroSpecialTransfer
//
// haleyjd 09/06/07: function to create a spectransfer_t that zeroes the sector
// special.
//
void P_ZeroSpecialTransfer(spectransfer_t *spec)
{
   // currently nothing special must be done, just memset it
   memset(spec, 0, sizeof(spectransfer_t));
}

//
// P_TransferSectorSpecial
//
// haleyjd 09/02/07: This function must now be called to accomplish transfer of
// specials from one sector to another. There is now other data in the sector_t
// structure which needs to be transferred along with the special so that
// features like customizable sector damage can work in the same manner and be
// switched on or off by floor/ceiling transfer line types.
//
void P_TransferSectorSpecial(sector_t *sector, const spectransfer_t *spec)
{
   sector->special     = spec->newspecial;
   sector->flags       = (sector->flags & ~SPECIALFLAGSMASK) | spec->flags;
   sector->damage      = spec->damage;
   sector->damagemask  = spec->damagemask;
   sector->damagemod   = spec->damagemod;
   sector->damageflags = spec->damageflags;
}

//
// P_DirectTransferSectorSpecial
//
// haleyjd 09/09/07: function to directly transfer a special and accompanying
// data from one sector to another.
//
void P_DirectTransferSectorSpecial(const sector_t *src, sector_t *dest)
{
   dest->special     = src->special;
   dest->flags      &= ~SPECIALFLAGSMASK;
   dest->flags      |= src->flags & SPECIALFLAGSMASK;
   dest->damage      = src->damage;
   dest->damagemask  = src->damagemask;
   dest->damagemod   = src->damagemod;
   dest->damageflags = src->damageflags;
}

//
// P_ZeroSectorSpecial
//
// haleyjd 09/09/07: Directly sets a sector's special and accompanying data to
// a non-special state.
//
void P_ZeroSectorSpecial(sector_t *sec)
{
   sec->special     = 0;
   sec->flags      &= ~SPECIALFLAGSMASK;
   sec->damage      = 0;
   sec->damagemask  = 0;
   sec->damagemod   = MOD_UNKNOWN;
   sec->damageflags = 0;
}

//============================================================================
//
// 3D Sides
//
// SoM: New functions to facilitate scrolling of 3d sides to make
// use as doors/lifts
//


//
// SoM 9/19/2002
// P_Scroll3DSides
//
// Runs through the given attached sector list and scrolls both
// sides of any linedef it finds with same tag.
//
bool P_Scroll3DSides(const sector_t *sector, bool ceiling, fixed_t delta,
                     int crush)
{
   bool     ok = true;
   int      i;
   line_t  *line;

   int numattached;
   int *attached;
   int numattsectors;
   int *attsectors;

   if(ceiling)
   {
      numattached = sector->c_numattached;
      attached = sector->c_attached;
      numattsectors = sector->c_numsectors;
      attsectors = sector->c_attsectors;
   }
   else
   {
      numattached = sector->f_numattached;
      attached = sector->f_attached;
      numattsectors = sector->f_numsectors;
      attsectors = sector->f_attsectors;
   }

   // Go through the sectors list one sector at a time.
   // Move any qualifying linedef's side offsets up/down based
   // on delta. 
   for(i = 0; i < numattached; ++i)
   {
#ifdef RANGECHECK  // haleyjd: made RANGECHECK
      if(attached[i] < 0 || attached[i] >= numlines)
         I_Error("P_Scroll3DSides: attached[i] is not a valid linedef index.\n");
#endif

      line = lines + attached[i];

      if(!(line->flags & (ML_TWOSIDED|ML_3DMIDTEX)) || line->sidenum[1] == -1)
         continue;

      sides[line->sidenum[0]].rowoffset += delta;
      sides[line->sidenum[1]].rowoffset += delta;

   }

   for(i = 0; i < numattsectors; ++i)
   {
      if(P_CheckSector(sectors + attsectors[i], crush, delta, 2))
         ok = false;
   }

   return ok;
}

//
// SoM 9/19/2002
// P_AttachLines
//
// Attaches all sectors that have lines with same tag as cline to
// cline's front sector.
//
// SoM 11/9/04: Now attaches lines and records another list of sectors
//
void P_AttachLines(const line_t *cline, bool ceiling)
{
   // FIXME / TODO: replace with a collection
   static int maxattach = 0;
   static int numattach = 0;
   static int alistsize = 0;
   static int *attached = NULL, *alist = NULL;

   int start = 0, i;
   line_t *line;

   if(!cline->frontsector)
      return;

   numattach = 0;

   // Check to ensure that this sector doesn't already 
   // have attachments.
   if(!ceiling && cline->frontsector->f_numattached)
   {
      numattach = cline->frontsector->f_numattached;

      if(numattach >= maxattach)
      {
         maxattach = numattach + 5;
         attached = erealloc(int *, attached, sizeof(int) * maxattach);
      }

      memcpy(attached, cline->frontsector->f_attached, sizeof(int) * numattach);
      Z_Free(cline->frontsector->f_attached);
      cline->frontsector->f_attached = NULL;
      cline->frontsector->f_numattached = 0;
      Z_Free(cline->frontsector->f_attsectors);
   }
   else if(ceiling && cline->frontsector->c_numattached)
   {
      numattach = cline->frontsector->c_numattached;

      if(numattach >= maxattach)
      {
         maxattach = numattach + 5;
         attached = erealloc(int *, attached, sizeof(int) * maxattach);
      }

      // haleyjd: check for safety
      if(!attached)
         I_Error("P_AttachLines: no attached list\n");

      memcpy(attached, cline->frontsector->c_attached, sizeof(int) * numattach);
      Z_Free(cline->frontsector->c_attached);
      cline->frontsector->c_attached = NULL;
      cline->frontsector->c_numattached = 0;
      cline->frontsector->c_numattached = 0;
      Z_Free(cline->frontsector->c_attsectors);
   }

   // Search the lines list. Check for every tagged line that
   // has the 3dmidtex lineflag, then add the line to the attached list.
   for(start = -1; (start = P_FindLineFromLineTag(cline,start)) >= 0; )
   {
      if(start != cline-lines)
      {
         line = lines+start;

         if(!line->frontsector || !line->backsector ||
            !(line->flags & ML_3DMIDTEX))
            continue;

         for(i = 0; i < numattach;i++)
         {
            if(line - lines == attached[i])
            break;
         }

         if(i == numattach)
         {
            if(numattach == maxattach)
            {
              maxattach += 5;

              attached = erealloc(int *, attached, sizeof(int) * maxattach);
            }

            attached[numattach++] = static_cast<int>(line - lines);
         }
         
         // SoM 12/8/02: Don't attach the backsector.
      }
   } // end for

   // haleyjd: static analyzer says this could happen, so let's just be safe.
   if(!attached)
      I_Error("P_AttachLines: nothing to attach to sector %d\n",
              static_cast<int>(cline->frontsector - sectors));

   // Copy the list to the c_attached or f_attached list.
   if(ceiling)
   {
      cline->frontsector->c_numattached = numattach;
      cline->frontsector->c_attached = (int *)(Z_Malloc(sizeof(int) * numattach, PU_LEVEL, 0));
      memcpy(cline->frontsector->c_attached, attached, sizeof(int) * numattach);

      alist = cline->frontsector->c_attached;
      alistsize = cline->frontsector->c_numattached;
   }
   else
   {
      cline->frontsector->f_numattached = numattach;
      cline->frontsector->f_attached = (int *)(Z_Malloc(sizeof(int) * numattach, PU_LEVEL, 0));
      memcpy(cline->frontsector->f_attached, attached, sizeof(int) * numattach);

      alist = cline->frontsector->f_attached;
      alistsize = cline->frontsector->f_numattached;
   }

   // (re)create the sectors list.
   numattach = 0;
   for(start = 0; start < alistsize; ++start)
   {
      int front = static_cast<int>(lines[alist[start]].frontsector - sectors);
      int back  = static_cast<int>(lines[alist[start]].backsector - sectors);

      // Check the frontsector for uniqueness in the list.
      for(i = 0; i < numattach; ++i)
      {
         if(attached[i] == front)
            break;
      }

      if(i == numattach)
      {
         if(numattach == maxattach)
         {
            maxattach += 5;
            attached = erealloc(int *, attached, sizeof(int) * maxattach);
         }
         attached[numattach++] = front;
      }

      // Check the backsector for uniqueness in the list.
      for(i = 0; i < numattach; ++i)
      {
         if(attached[i] == back)
            break;
      }

      if(i == numattach)
      {
         if(numattach == maxattach)
         {
            maxattach += 5;
            attached = erealloc(int *, attached, sizeof(int) * maxattach);
         }
         attached[numattach++] = back;
      }
   }

   // Copy the attached sectors list.
   if(ceiling)
   {
      cline->frontsector->c_numsectors = numattach;
      cline->frontsector->c_attsectors = (int *)(Z_Malloc(sizeof(int) * numattach, PU_LEVEL, 0));
      memcpy(cline->frontsector->c_attsectors, attached, sizeof(int) * numattach);
   }
   else
   {
      cline->frontsector->f_numsectors = numattach;
      cline->frontsector->f_attsectors = (int *)(Z_Malloc(sizeof(int) * numattach, PU_LEVEL, 0));
      memcpy(cline->frontsector->f_attsectors, attached, sizeof(int) * numattach);
   }
}

//
// P_MoveAttached
//
// Moves all attached surfaces.
//
bool P_MoveAttached(const sector_t *sector, bool ceiling, fixed_t delta,
                    int crush)
{
   int i;

   int count;
   attachedsurface_t *list;

   bool ok = true;
   
   if(ceiling)
   {
      count = sector->c_asurfacecount;
      list = sector->c_asurfaces;
   }
   else
   {
      count = sector->f_asurfacecount;
      list = sector->f_asurfaces;
   }

   for(i = 0; i < count; i++)
   {
      if(list[i].type & AS_CEILING)
      {
         P_SetCeilingHeight(list[i].sector, list[i].sector->ceilingheight + delta);
         if(P_CheckSector(list[i].sector, crush, delta, 1))
            ok = false;
      }
      else if(list[i].type & AS_MIRRORCEILING)
      {
         P_SetCeilingHeight(list[i].sector, list[i].sector->ceilingheight - delta);
         if(P_CheckSector(list[i].sector, crush, -delta, 1))
            ok = false;
      }

      if(list[i].type & AS_FLOOR)
      {
         P_SetFloorHeight(list[i].sector, list[i].sector->floorheight + delta);
         if(P_CheckSector(list[i].sector, crush, delta, 0))
            ok = false;
      }
      else if(list[i].type & AS_MIRRORFLOOR)
      {
         P_SetFloorHeight(list[i].sector, list[i].sector->floorheight - delta);
         if(P_CheckSector(list[i].sector, crush, -delta, 0))
            ok = false;
      }
   }

   return ok;
}

//
// SoM 10/14/2007
// P_AttachSectors
//
// Attaches all sectors with like-tagged attachment lines to line->frontsector
//
void P_AttachSectors(const line_t *line, int staticFn)
{
   // FIXME / TODO: replace with a collection
   static int numattached = 0;
   static int maxattached = 0;
   static attachedsurface_t *attached = NULL;

   bool ceiling = (staticFn == EV_STATIC_ATTACH_SET_CEILING_CONTROL);
   sector_t *sector = line->frontsector;

   int start = 0, i;
   line_t *slaveline;

   if(!sector) 
      return;

   numattached = 0;
   
   // Check to ensure that this sector doesn't already 
   // have attachments.
   if(!ceiling && sector->f_asurfacecount)
   {
      numattached = sector->f_asurfacecount;

      if(numattached >= maxattached)
      {
         maxattached = numattached + 5;
         attached = erealloc(attachedsurface_t *, attached, 
                             sizeof(attachedsurface_t) * maxattached);
      }

      // haleyjd: check for safety
      if(!attached)
         I_Error("P_AttachSector: no attached list\n");

      memcpy(attached, sector->f_asurfaces, sizeof(attachedsurface_t) * numattached);
      Z_Free(sector->f_asurfaces);
      sector->f_asurfaces = NULL;
      sector->f_asurfacecount = 0;
   }
   else if(ceiling && sector->c_asurfacecount)
   {
      numattached = sector->c_asurfacecount;

      if(numattached >= maxattached)
      {
         maxattached = numattached + 5;
         attached = erealloc(attachedsurface_t *, attached, 
                             sizeof(attachedsurface_t) * maxattached);
      }

      memcpy(attached, sector->c_asurfaces, sizeof(attachedsurface_t) * numattached);
      Z_Free(sector->c_asurfaces);
      sector->c_asurfaces = NULL;
      sector->c_asurfacecount = 0;
   }

   // Search the lines list. Check for every tagged line that
   // has the appropriate special, then add the line's frontsector to the attached list.
   for(start = -1; (start = P_FindLineFromLineTag(line,start)) >= 0; )
   {
      attachedtype_e type;

      if(start != line-lines)
      {
         slaveline = lines+start;

         if(!slaveline->frontsector)
            continue;

         // haleyjd 02/05/13: get static init for slave line special
         int slavefunc = EV_StaticInitForSpecial(slaveline->special);

         if(slavefunc == EV_STATIC_ATTACH_FLOOR_TO_CONTROL) 
         {
            // Don't attach a floor to itself
            if(slaveline->frontsector == sector && 
               staticFn == EV_STATIC_ATTACH_SET_FLOOR_CONTROL)
               continue;

            // search the list of attachments
            for(i = 0; i < numattached; i++)
            {
               if(attached[i].sector == slaveline->frontsector)
               {
                  if(!(attached[i].type & (AS_FLOOR | AS_MIRRORFLOOR)))
                     attached[i].type |= AS_FLOOR;

                  break;
               }
            }

            if(i < numattached)
               continue;

            type = AS_FLOOR;
         }
         else if(slavefunc == EV_STATIC_ATTACH_CEILING_TO_CONTROL)
         {
            // Don't attach a ceiling to itself
            if(slaveline->frontsector == sector && 
               staticFn == EV_STATIC_ATTACH_SET_CEILING_CONTROL)
               continue;

            // search the list of attachments
            for(i = 0; i < numattached; i++)
            {
               if(attached[i].sector == slaveline->frontsector)
               {
                  if(!(attached[i].type & (AS_CEILING | AS_MIRRORCEILING)))
                     attached[i].type |= AS_CEILING;

                  break;
               }
            }

            if(i < numattached)
               continue;

            type = AS_CEILING;
         }
         else if(slavefunc == EV_STATIC_ATTACH_MIRROR_FLOOR)
         {
            // Don't attach a floor to itself
            if(slaveline->frontsector == sector && 
               staticFn == EV_STATIC_ATTACH_SET_FLOOR_CONTROL)
               continue;

            // search the list of attachments
            for(i = 0; i < numattached; i++)
            {
               if(attached[i].sector == slaveline->frontsector)
               {
                  if(!(attached[i].type & (AS_FLOOR | AS_MIRRORFLOOR)))
                     attached[i].type |= AS_MIRRORFLOOR;

                  break;
               }
            }

            if(i < numattached)
               continue;

            type = AS_MIRRORFLOOR;
         }
         else if(slavefunc == EV_STATIC_ATTACH_MIRROR_CEILING)
         {
            // Don't attach a ceiling to itself
            if(slaveline->frontsector == sector && 
               staticFn == EV_STATIC_ATTACH_SET_CEILING_CONTROL)
               continue;

            // search the list of attachments
            for(i = 0; i < numattached; i++)
            {
               if(attached[i].sector == slaveline->frontsector)
               {
                  if(!(attached[i].type & (AS_CEILING | AS_MIRRORCEILING)))
                     attached[i].type |= AS_MIRRORCEILING;
                  break;
               }
            }

            if(i < numattached)
               continue;

            type = AS_MIRRORCEILING;
         }
         else
            continue;


         // add sector
         if(numattached == maxattached)
         {
            maxattached += 5;
            attached = erealloc(attachedsurface_t *, attached, 
                                sizeof(attachedsurface_t) * maxattached);
         }

         attached[numattached].sector = slaveline->frontsector;
         attached[numattached].type = type;
         numattached++;
      }
   } // end for

   // Copy the list to the sector.
   if(ceiling)
   {
      sector->c_asurfacecount = numattached;
      sector->c_asurfaces = 
         (attachedsurface_t *)(Z_Malloc(sizeof(attachedsurface_t) * numattached, PU_LEVEL, 0));
      memcpy(sector->c_asurfaces, attached, sizeof(attachedsurface_t) * numattached);
   }
   else
   {
      sector->f_asurfacecount = numattached;
      sector->f_asurfaces = 
         (attachedsurface_t *)(Z_Malloc(sizeof(attachedsurface_t) * numattached, PU_LEVEL, 0));
      memcpy(sector->f_asurfaces, attached, sizeof(attachedsurface_t) * numattached);
   }
}

//=============================================================================
//
// Portals
//

//
// P_SetPortal
//
static void P_SetPortal(sector_t *sec, line_t *line, portal_t *portal, portal_effect effects)
{
   if(portal->type == R_LINKED && sec->groupid == R_NOGROUP)
   {
      // Add the sector and all adjacent sectors to the from group
      P_GatherSectors(sec, portal->data.link.fromid);
   }
   
   switch(effects)
   {
   case portal_ceiling:
      sec->c_portal = portal;
      P_CheckCPortalState(sec);
      break;
   case portal_floor:
      sec->f_portal = portal;
      P_CheckFPortalState(sec);
      break;
   case portal_both:
      sec->c_portal = sec->f_portal = portal;
      P_CheckCPortalState(sec);
      P_CheckFPortalState(sec);
      break;
   case portal_lineonly:
      line->portal = portal;
      P_CheckLPortalState(line);
      break;
   default:
      I_Error("P_SetPortal: unknown portal effect\n");
   }
}

//
// P_getPortalProps
//
// haleyjd 02/05/13: Get the proper portal type and effect values for a static
// init function ordinal.
//
static void P_getPortalProps(int staticFn, portal_type &type, portal_effect &effects)
{
   struct staticportalprop_t 
   {
      int staticFn;
      portal_type type;
      portal_effect effects;
   };
   static staticportalprop_t props[] =
   {
      { EV_STATIC_PORTAL_PLANE_CEILING,          portal_plane,    portal_ceiling  },
      { EV_STATIC_PORTAL_PLANE_FLOOR,            portal_plane,    portal_floor    },
      { EV_STATIC_PORTAL_PLANE_CEILING_FLOOR,    portal_plane,    portal_both     },
      { EV_STATIC_PORTAL_HORIZON_CEILING,        portal_horizon,  portal_ceiling  },
      { EV_STATIC_PORTAL_HORIZON_FLOOR,          portal_horizon,  portal_floor    },
      { EV_STATIC_PORTAL_HORIZON_CEILING_FLOOR,  portal_horizon,  portal_both     },
      { EV_STATIC_PORTAL_SKYBOX_CEILING,         portal_skybox,   portal_ceiling  },
      { EV_STATIC_PORTAL_SKYBOX_FLOOR,           portal_skybox,   portal_floor    },
      { EV_STATIC_PORTAL_SKYBOX_CEILING_FLOOR,   portal_skybox,   portal_both     },
      { EV_STATIC_PORTAL_ANCHORED_CEILING,       portal_anchored, portal_ceiling  },
      { EV_STATIC_PORTAL_ANCHORED_FLOOR,         portal_anchored, portal_floor    },
      { EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR, portal_anchored, portal_both     },
      { EV_STATIC_PORTAL_TWOWAY_CEILING,         portal_twoway,   portal_ceiling  },
      { EV_STATIC_PORTAL_TWOWAY_FLOOR,           portal_twoway,   portal_floor    },
      { EV_STATIC_PORTAL_LINKED_CEILING,         portal_linked,   portal_ceiling  },
      { EV_STATIC_PORTAL_LINKED_FLOOR,           portal_linked,   portal_floor    },
      { EV_STATIC_PORTAL_LINKED_LINE2LINE,       portal_linked,   portal_lineonly },
   };

   for(size_t i = 0; i < earrlen(props); i++)
   {
      if(props[i].staticFn == staticFn)
      {
         type    = props[i].type;
         effects = props[i].effects;
         break;
      }
   }
}

//
// P_SpawnPortal
//
// Code by SoM, functionalized by Quasar.
// Spawns a portal and attaches it to floors and/or ceilings of appropriate 
// sectors, and to lines with special 289.
//
static void P_SpawnPortal(line_t *line, int staticFn)
{
   portal_type   type    = portal_plane;
   portal_effect effects = portal_ceiling;
   int       CamType = E_ThingNumForName("EESkyboxCam"); // find the skybox camera object
   sector_t *sector;
   portal_t *portal = NULL;
   Mobj     *skycam;
   fixed_t   planez = 0;
   int       anchortype = 0; // SoM 3-10-04: new plan.
   int       anchorfunc = 0; // haleyjd 02/05/13
   int       s;
   int       fromid, toid;

   // haleyjd: get type and effects from static init function
   P_getPortalProps(staticFn, type, effects);

   if(!(sector = line->frontsector))
      return;

   // create the appropriate type of portal
   switch(type)
   {
   case portal_plane:
      portal = R_GetPlanePortal(&sector->ceilingpic, 
                                &sector->ceilingheight, 
                                &sector->lightlevel, 
                                &sector->ceiling_xoffs, 
                                &sector->ceiling_yoffs,
                                &sector->ceilingbaseangle,
                                &sector->ceilingangle);
      break;

   case portal_horizon:
      portal = R_GetHorizonPortal(&sector->floorpic, &sector->ceilingpic, 
                                  &sector->floorheight, &sector->ceilingheight,
                                  &sector->lightlevel, &sector->lightlevel,
                                  &sector->floor_xoffs, &sector->floor_yoffs,
                                  &sector->ceiling_xoffs, &sector->ceiling_yoffs,
                                  &sector->floorbaseangle, &sector->floorangle,
                                  &sector->ceilingbaseangle, &sector->ceilingangle);
      break;

   case portal_skybox:
      skycam = sector->thinglist;
      while(skycam)
      {
         if(skycam->type == CamType)
            break;
         skycam = skycam->snext;
      }
      if(!skycam)
      {
         C_Printf(FC_ERROR "Skybox found with no skybox camera\a\n");
         return;
      }
      
      portal = R_GetSkyBoxPortal(skycam);
      break;

   case portal_anchored:
      // determine proper anchor type (see below)
      if(staticFn == EV_STATIC_PORTAL_ANCHORED_CEILING || 
         staticFn == EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR)
         anchorfunc = EV_STATIC_PORTAL_ANCHOR;
      else
         anchorfunc = EV_STATIC_PORTAL_ANCHOR_FLOOR;

      // haleyjd: get anchortype for func
      anchortype = EV_SpecialForStaticInit(anchorfunc);

      // find anchor line
      for(s = -1; (s = P_FindLineFromLineTag(line, s)) >= 0; )
      {
         // SoM 3-10-04: Two different anchor linedef codes so I can tag 
         // two anchored portals to the same sector.
         if(lines[s].special != anchortype || line == &lines[s])
            continue;

         break;
      }
      if(s < 0)
      {
         C_Printf(FC_ERROR "No anchor line for portal.\a\n");
         return;
      }

      portal = R_GetAnchoredPortal(static_cast<int>(line - lines), s);
      break;

   case portal_twoway:
      // two way and linked portals can only be applied to either the floor or ceiling.
      if(staticFn == EV_STATIC_PORTAL_TWOWAY_CEILING)
         anchorfunc = EV_STATIC_PORTAL_TWOWAY_ANCHOR;
      else
         anchorfunc = EV_STATIC_PORTAL_TWOWAY_ANCHOR_FLOOR;

      // haleyjd: get anchortype for func
      anchortype = EV_SpecialForStaticInit(anchorfunc);

      // find anchor line
      for(s = -1; (s = P_FindLineFromLineTag(line, s)) >= 0; )
      {
         // SoM 3-10-04: Two different anchor linedef codes so I can tag 
         // two anchored portals to the same sector.
         if(lines[s].special != anchortype || line == &lines[s])
            continue;
         break;
      }
      if(s < 0)
      {
         C_Printf(FC_ERROR "No anchor line for portal.\a\n");
         return;
      }

      portal = R_GetTwoWayPortal(static_cast<int>(line - lines), s);
      break;

   case portal_linked:
      if(demo_version < 333)
         return;

      // linked portals can only be applied to either the floor or ceiling.
      if(staticFn == EV_STATIC_PORTAL_LINKED_CEILING) 
      {
         anchorfunc = EV_STATIC_PORTAL_LINKED_ANCHOR;
         planez = sector->floorheight;
      }
      else if(staticFn == EV_STATIC_PORTAL_LINKED_FLOOR)
      {
         anchorfunc = EV_STATIC_PORTAL_LINKED_ANCHOR_FLOOR;
         planez = sector->ceilingheight;
      }
      else if(staticFn == EV_STATIC_PORTAL_LINKED_LINE2LINE) 
      {
         // Line-Line linked portals
         anchorfunc = EV_STATIC_PORTAL_LINKED_L2L_ANCHOR;
         planez = 0; // SoM: What should this really be? I dunno.
      }

      // haleyjd: get anchortype for func
      anchortype = EV_SpecialForStaticInit(anchorfunc);

      // find anchor line
      for(s = -1; (s = P_FindLineFromLineTag(line, s)) >= 0; )
      {
         // SoM 3-10-04: Two different anchor linedef codes so I can tag 
         // two anchored portals to the same sector.
         if(lines[s].special != anchortype || line == &lines[s] 
           || lines[s].frontsector == NULL)
            continue;

         break;
      }
      if(s < 0)
      {
         C_Printf(FC_ERROR "No anchor line for portal. (line %i)\a\n", line - lines);
         return;
      }

      // Setup main groups. Keep in mind, the linedef that actually creates the 
      // portal will be on the 'other side' of that portal, so it is actually the 
      // 'to group' and the anchor line is in the 'from group'
      if(sector->groupid == R_NOGROUP)
         P_CreatePortalGroup(sector);
         
      if(lines[s].frontsector->groupid == R_NOGROUP)
         P_CreatePortalGroup(lines[s].frontsector);
      
      toid = sector->groupid;
      fromid = lines[s].frontsector->groupid;
            
      portal = R_GetLinkedPortal(static_cast<int>(line - lines), s, planez, fromid, toid);

      // Special case where the portal was created with the line-to-line portal type
      if(staticFn == EV_STATIC_PORTAL_LINKED_LINE2LINE)
      {
         P_SetPortal(lines[s].frontsector, lines + s, portal, portal_lineonly);
         
         portal = R_GetLinkedPortal(s, static_cast<int>(line - lines), planez, toid, fromid);
         P_SetPortal(sector, line, portal, portal_lineonly);
         return;
      }
      break;

   default:
      I_Error("P_SpawnPortal: unknown portal type\n");
   }

   // attach portal to tagged sector floors/ceilings
   // SoM: TODO: Why am I not checking groupids?
   for(s = -1; (s = P_FindSectorFromLineTag(line, s)) >= 0; )
   {
      P_SetPortal(sectors + s, NULL, portal, effects);
   }

   // attach portal to like-tagged 289 lines
   for(s = -1; (s = P_FindLineFromLineTag(line, s)) >= 0; )
   {
      if(line == &lines[s] || !lines[s].frontsector)
         continue;

      int xferfunc = EV_StaticInitForSpecial(lines[s].special);
      
      if(xferfunc == EV_STATIC_PORTAL_LINE)
         P_SetPortal(lines[s].frontsector, lines + s, portal, portal_lineonly);
      else if(xferfunc == EV_STATIC_PORTAL_APPLY_FRONTSECTOR)
         P_SetPortal(lines[s].frontsector, lines + s, portal, effects);
      else
         continue;

      lines[s].special = 0;
   }
}

#if 0
//
// Small Natives
//

static cell AMX_NATIVE_CALL sm_sectorspecial(AMX *amx, cell *params)
{   
   int special = (int)params[1];
   int id      = (int)params[2];
   int secnum = -1;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   while((secnum = P_FindSectorFromTag(id, secnum)) >= 0)
   {
      sectors[secnum].special = special;
   }

   return 0;
}

//
// 07/31/04: support setting/changing sector colormaps
//
static cell AMX_NATIVE_CALL sm_sectorcolormap(AMX *amx, cell *params)
{
   char *name;
   int err, lumpnum;
   int pos    = (int)params[2];
   int id     = (int)params[3];
   int secnum = -1;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   if((err = SM_GetSmallString(amx, &name, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   // any unfound lump just clears the respective colormap
   if((lumpnum = R_ColormapNumForName(name)) < 0)
      lumpnum = 0;

   while((secnum = P_FindSectorFromTag(id, secnum)) >= 0)
   {
      sector_t *s = &sectors[secnum];

      switch(pos)
      {
      case 0: // middle
         s->midmap = lumpnum;
         break;
      case 1: // bottom
         s->bottommap = lumpnum;
         break;
      case 2: // top
         s->topmap = lumpnum;
         break;
      case 3: // all
         s->midmap = s->bottommap = s->topmap = lumpnum;
         break;
      }
   }

   Z_Free(name);

   return 0;
}

AMX_NATIVE_INFO pspec_Natives[] =
{
   { "_SectorSpecial",  sm_sectorspecial },
   { "_SectorColormap", sm_sectorcolormap },
   { NULL,               NULL }
};
#endif

//----------------------------------------------------------------------------
//
// $Log: p_spec.c,v $
// Revision 1.56  1998/05/25  10:40:30  killough
// Fix wall scrolling bug
//
// Revision 1.55  1998/05/23  10:23:32  jim
// Fix numeric changer loop corruption
//
// Revision 1.54  1998/05/11  06:52:56  phares
// Documentation
//
// Revision 1.53  1998/05/07  00:51:34  killough
// beautification
//
// Revision 1.52  1998/05/04  11:47:23  killough
// Add #include d_deh.h
//
// Revision 1.51  1998/05/04  02:22:06  jim
// formatted p_specs, moved a coupla routines to p_floor
//
// Revision 1.50  1998/05/03  22:06:30  killough
// Provide minimal required headers at top (no other changes)
//
// Revision 1.49  1998/04/17  18:57:51  killough
// fix comment
//
// Revision 1.48  1998/04/17  18:49:02  killough
// Fix lack of animation in flats
//
// Revision 1.47  1998/04/17  10:24:47  killough
// Add P_FindLineFromLineTag(), add CARRY_CEILING macro
//
// Revision 1.46  1998/04/14  18:49:36  jim
// Added monster only and reverse teleports
//
// Revision 1.45  1998/04/12  02:05:25  killough
// Add ceiling light setting, start ceiling carriers
//
// Revision 1.44  1998/04/06  11:05:23  jim
// Remove LEESFIXES, AMAP bdg->247
//
// Revision 1.43  1998/04/06  04:39:04  killough
// Make scroll carriers carry all things underwater
//
// Revision 1.42  1998/04/01  16:39:11  jim
// Fix keyed door message on gunfire
//
// Revision 1.41  1998/03/29  20:13:35  jim
// Fixed use of 2S flag in Donut linedef
//
// Revision 1.40  1998/03/28  18:13:24  killough
// Fix conveyor bug (carry objects not touching but overhanging)
//
// Revision 1.39  1998/03/28  05:32:48  jim
// Text enabling changes for DEH
//
// Revision 1.38  1998/03/23  18:38:48  jim
// Switch and animation tables now lumps
//
// Revision 1.37  1998/03/23  15:24:41  phares
// Changed pushers to linedef control
//
// Revision 1.36  1998/03/23  03:32:36  killough
// Make "oof" sounds have true mobj origins (for spy mode hearing)
// Make carrying floors carry objects hanging over edges of sectors
//
// Revision 1.35  1998/03/20  14:24:36  jim
// Gen ceiling target now shortest UPPER texture
//
// Revision 1.34  1998/03/20  00:30:21  phares
// Changed friction to linedef control
//
// Revision 1.33  1998/03/18  23:14:02  jim
// Deh text additions
//
// Revision 1.32  1998/03/16  15:43:33  killough
// Add accelerative scrollers, merge Jim's changes
//
// Revision 1.29  1998/03/13  14:05:44  jim
// Fixed arith overflow in some linedef types
//
// Revision 1.28  1998/03/12  21:54:12  jim
// Freed up 12 linedefs for use as vectors
//
// Revision 1.26  1998/03/09  10:57:55  jim
// Allowed Lee's change to 0 tag trigger compatibility
//
// Revision 1.25  1998/03/09  07:23:43  killough
// Add generalized scrollers, renumber some linedefs
//
// Revision 1.24  1998/03/06  12:34:39  jim
// Renumbered 300+ linetypes under 256 for DCK
//
// Revision 1.23  1998/03/05  16:59:10  jim
// Fixed inability of monsters/barrels to use new teleports
//
// Revision 1.22  1998/03/04  07:33:04  killough
// Fix infinite loop caused by multiple carrier references
//
// Revision 1.21  1998/03/02  15:32:57  jim
// fixed errors in numeric model sector search and 0 tag trigger defeats
//
// Revision 1.20  1998/03/02  12:13:57  killough
// Add generalized scrolling flats & walls, carrying floors
//
// Revision 1.19  1998/02/28  01:24:53  jim
// Fixed error in 0 tag trigger fix
//
// Revision 1.17  1998/02/24  08:46:36  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.16  1998/02/23  23:47:05  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.15  1998/02/23  04:52:33  killough
// Allow god mode cheat to work on E1M8 unless compatibility
//
// Revision 1.14  1998/02/23  00:42:02  jim
// Implemented elevators
//
// Revision 1.12  1998/02/17  05:55:06  killough
// Add silent teleporters
// Change RNG calling sequence
// Cosmetic changes
//
// Revision 1.11  1998/02/13  03:28:06  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.10  1998/02/08  05:35:39  jim
// Added generalized linedef types
//
// Revision 1.8  1998/02/02  13:34:26  killough
// Performance tuning, program beautification
//
// Revision 1.7  1998/01/30  14:43:54  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.4  1998/01/27  16:19:29  jim
// Fixed subroutines used by linedef triggers and a NULL ref in Donut
//
// Revision 1.3  1998/01/26  19:24:26  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/25  20:24:45  jim
// Fixed crusher floor, lowerandChange floor types, and unknown sector special error
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

