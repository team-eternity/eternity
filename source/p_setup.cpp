//
// The Eternity Engine
// Copyright (C) 2021 James Haley et al.
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
// Purpose: Do all the WAD I/O, get map description,
//          set up initial state and misc. LUTs.
// Authors: James Haley, Ioan Chera
//


#include <memory>
#include "z_zone.h"

#include "a_small.h"
#include "acs_intr.h"
#include "am_map.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "d_main.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_compatibility.h"
#include "e_exdata.h" // haleyjd: ExtraData!
#include "e_reverbs.h"
#include "e_ttypes.h"
#include "e_udmf.h"  // IOANCH 20151206: UDMF
#include "ev_specials.h"
#include "g_demolog.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "in_lude.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_binary.h"
#include "m_hash.h"
#include "p_anim.h"  // haleyjd: lightning
#include "p_chase.h"
#include "p_enemy.h"
#include "p_hubs.h"
#include "p_info.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_mobjcol.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_scroll.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "p_tick.h"
#include "polyobj.h"
#include "r_context.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_dynseg.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_things.h"
#include "s_musinfo.h"
#include "s_sndseq.h"
#include "s_sound.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_levels.h"
#include "w_wad.h"
#include "z_auto.h"

extern const char *level_error;

//
// Miscellaneous constants
//
enum
{
   // vertex distance limit over which NOT to fix slime trails. Useful for
   // the new vanilla Doom rendering trick discovered by Linguica. Link here:
   // http://www.doomworld.com/vb/doom-editing/74354-stupid-bsp-tricks/
   LINGUORTAL_THRESHOLD = 8 * FRACUNIT,   
};

//
// ZNodeType
//
// IOANCH: ZDoom node type enum
//
enum ZNodeType
{
   ZNodeType_Invalid,
   ZNodeType_Uncompressed_Normal,
   ZNodeType_Uncompressed_GL,
   ZNodeType_Uncompressed_GL2,
   ZNodeType_Uncompressed_GL3,
   ZNodeType_Compressed_Normal,
   ZNodeType_Compressed_GL,
   ZNodeType_Compressed_GL2,
   ZNodeType_Compressed_GL3,
};

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

bool     newlevel = false;
char     levelmapname[10];

int      numvertexes;
vertex_t *vertexes;

int      numsegs;
seg_t    *segs;

int      numsectors;
sector_t *sectors;

// haleyjd 01/05/14: sector interpolation data
sectorinterp_t *sectorinterps;

// ioanch: list of sector bounding boxes for sector portal seg rejection (coarse)
// length: numsectors * 4
sectorbox_t *pSectorBoxes;

// haleyjd 01/12/14: sector sound environment zones
int         numsoundzones;
soundzone_t *soundzones;

int         numsubsectors;
subsector_t *subsectors;

int      numnodes;
node_t   *nodes;
fnode_t  *fnodes;

int      numlines;
int      numlinesPlusExtra;
line_t   *lines;

int      numsides;
side_t   *sides;

int      numthings;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.

int       bmapwidth, bmapheight;  // size in mapblocks

// killough 3/1/98: remove blockmap limit internally:
int       *blockmap;              // was short -- killough

// offsets in blockmap are from here
int       *blockmaplump;          // was short -- killough

fixed_t   bmaporgx, bmaporgy;     // origin of block map

Mobj    **blocklinks;             // for thing chains

byte     *portalmap;              // haleyjd: for portals

bool      skipblstart;            // MaxW: Skip initial blocklist short

//
// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without the special effect, this could
// be used as a PVS lookup as well.
//

byte *rejectmatrix;

static int gTotalLinesForRejectOverflow;  // ioanch 20160309: for REJECT fix

// Maintain single and multi player starting spots.

// 1/11/98 killough: Remove limit on deathmatch starts
mapthing_t *deathmatchstarts;      // killough
size_t     num_deathmatchstarts;   // killough

mapthing_t *deathmatch_p;
mapthing_t playerstarts[MAXPLAYERS];

// haleyjd 06/14/10: level wad directory
static WadDirectory *setupwad;

// Current level's hash digest, for showing on console
static qstring p_currentLevelHashDigest;

//
// ShortToLong
//
// haleyjd 06/19/06: Happy birthday to me ^_^
// Inline routine to convert a short value to a long, preserving the value
// -1 but treating any other negative value as unsigned.
//
inline static int ShortToLong(int16_t value)
{
   return (value == -1) ? -1l : (int)value & 0xffff;
}

//
// ShortToNodeChild
//
// haleyjd 06/14/10: Inline routine to convert a short value to a node child
// number. 0xFFFF and 0x8000 are special values.
//
inline static void ShortToNodeChild(int *loc, uint16_t value)
{
   // e6y: support for extended nodes
   if(value == 0xffff)
   {
      *loc = -1l;
   }
   else if(value & 0x8000)
   {
      // Convert to extended type
      *loc  = value & ~0x8000;

      // haleyjd 11/06/10: check for invalid subsector reference
      if(*loc >= numsubsectors)
      {
         C_Printf(FC_ERROR "Warning: BSP tree references invalid subsector #%d\a\n", *loc);
         *loc = 0;
      }

      *loc |= NF_SUBSECTOR;
   }
   else
      *loc = (int)value & 0xffff;
}

//
// SafeUintIndex
//
// haleyjd 12/04/08: Inline routine to convert an effectively unsigned short
// index into a long index, safely checking against the provided upper bound
// and substituting the value of 0 in the event of an overflow.
//
inline static int SafeUintIndex(int16_t input, int limit, 
                                const char *func, int index, const char *item)
{
   int ret = (int)(SwapShort(input)) & 0xffff;

   if(ret >= limit)
   {
      C_Printf(FC_ERROR "Error: %s #%d - bad %s #%d\a\n", 
               func, index, item, ret);
      ret = 0;
   }

   return ret;
}

//
// SafeRealUintIndex
//
// haleyjd 06/14/10: Matching routine for indices that are already in unsigned
// short format.
//
inline static int SafeRealUintIndex(uint16_t input, int limit, 
                                    const char *func, int index, const char *item)
{
   int ret = (int)(SwapUShort(input)) & 0xffff;

   if(ret >= limit)
   {
      C_Printf(FC_ERROR "Error: %s #%d - bad %s #%d\a\n", 
               func, index, item, ret);
      ret = 0;
   }

   return ret;
}

//=============================================================================
//
// Level Loading
//

#define DOOM_VERTEX_LEN 4
#define PSX_VERTEX_LEN  8

//
// P_LoadConsoleVertexes
//
// haleyjd 12/12/13: Support for console maps' fixed-point vertices.
//
static void P_LoadConsoleVertexes(int lump)
{
   ZAutoBuffer buf;

   // Determine number of vertexes
   numvertexes = setupwad->lumpLength(lump) / PSX_VERTEX_LEN;

   // Allocate
   vertexes = estructalloctag(vertex_t, numvertexes, PU_LEVEL);

   // Load lump
   setupwad->cacheLumpAuto(lump, buf);
   auto data = buf.getAs<byte *>();

   // Copy and convert vertex coordinates
   for(int i = 0; i < numvertexes; i++)
   {
      vertexes[i].x = GetBinaryDWord(data);
      vertexes[i].y = GetBinaryDWord(data);

      vertexes[i].fx = M_FixedToFloat(vertexes[i].x);
      vertexes[i].fy = M_FixedToFloat(vertexes[i].y);
   }
}

//
// P_LoadVertexes
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadVertexes(int lump)
{
   ZAutoBuffer buf;
   
   // Determine number of vertexes:
   //  total lump length / vertex record length.
   numvertexes = setupwad->lumpLength(lump) / DOOM_VERTEX_LEN;

   // Allocate zone memory for buffer.
   vertexes = estructalloctag(vertex_t, numvertexes, PU_LEVEL);
   
   // Load data into cache.
   setupwad->cacheLumpAuto(lump, buf);
   auto data = buf.getAs<byte *>();
   
   // Copy and convert vertex coordinates, internal representation as fixed.
   for(int i = 0; i < numvertexes; i++)
   {
      vertexes[i].x = GetBinaryWord(data) << FRACBITS;
      vertexes[i].y = GetBinaryWord(data) << FRACBITS;
      
      // SoM: Cardboard stores float versions of vertices.
      vertexes[i].fx = M_FixedToFloat(vertexes[i].x);
      vertexes[i].fy = M_FixedToFloat(vertexes[i].y);
   }
}

// SoM 5/13/09: calculate seg length
static void P_CalcSegLength(seg_t *lseg)
{
   float dx, dy;

   dx = lseg->v2->fx - lseg->v1->fx;
   dy = lseg->v2->fy - lseg->v1->fy;
   lseg->len = sqrtf(dx * dx + dy * dy);
}

//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadSegs(int lump)
{
   int  i;
   byte *data;
   
   numsegs = setupwad->lumpLength(lump) / sizeof(mapseg_t);
   segs = estructalloctag(seg_t, numsegs, PU_LEVEL);
   data = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));
   
   for(i = 0; i < numsegs; ++i)
   {
      seg_t *li = segs + i;
      mapseg_t *ml = (mapseg_t *)data + i;
      
      int side, linedef;
      line_t *ldef;

      // haleyjd 06/19/06: convert indices to unsigned
      li->v1 = &vertexes[SafeUintIndex(ml->v1, numvertexes, "seg", i, "vertex")];
      li->v2 = &vertexes[SafeUintIndex(ml->v2, numvertexes, "seg", i, "vertex")];

      li->offset = (float)(SwapShort(ml->offset));

      // haleyjd 06/19/06: convert indices to unsigned
      linedef = SafeUintIndex(ml->linedef, numlines, "seg", i, "line");
      ldef = &lines[linedef];
      li->linedef = ldef;
      side = SwapShort(ml->side);

      if(side < 0 || side > 1)
      {
         level_error = "Seg line side number out of range";
         return;
      }

      li->sidedef = &sides[ldef->sidenum[side]];
      li->frontsector = sides[ldef->sidenum[side]].sector;

      // killough 5/3/98: ignore 2s flag if second sidedef missing:
      if(ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=-1)
         li->backsector = sides[ldef->sidenum[side^1]].sector;
      else
         li->backsector = nullptr;

      P_CalcSegLength(li);
   }
   
   Z_Free(data);
}

//
// P_LoadSegs_V4
//
// ioanch 20160204: DEEPBSP segs
//
static void P_LoadSegs_V4(int lump)
{
   numsegs = setupwad->lumpLength(lump) / sizeof(mapseg_v4_t);
   segs = estructalloctag(seg_t, numsegs, PU_LEVEL);
   auto data = static_cast<byte *>(setupwad->cacheLumpNum(lump, PU_STATIC));

   if(!numsegs || !segs || !data)
   {
      Z_Free(data);
      level_error = "no segs in level";
      return;
   }

   for(int i = 0; i < numsegs; ++i)
   {
      seg_t *li = segs + i;
      auto ml = reinterpret_cast<const mapseg_v4_t *>(data) + i;
      int v1, v2;

      int side, linedef;
      line_t *ldef;

      v1 = SwapLong(ml->v1);
      if(v1 >= numvertexes || v1 < 0)
      {
         C_Printf(FC_ERROR "Error: seg #%d - bad vertex #%d\a\n", i, v1);
         v1 = 0;  // reset it without failing
      }
      v2 = SwapLong(ml->v2);
      if(v2 >= numvertexes || v2 < 0)
      {
         C_Printf(FC_ERROR "Error: seg #%d - bad vertex #%d\a\n", i, v2);
         v2 = 0;  // reset it without failing
      }
      li->v1 = &vertexes[v1];
      li->v2 = &vertexes[v2];

      li->offset = static_cast<float>(SwapShort(ml->offset));

      linedef = SafeUintIndex(ml->linedef, numlines, "seg", i, "line");
      ldef = &lines[linedef];

      li->linedef = ldef;
      side = SwapShort(ml->side);

      if(side < 0 || side > 1)
      {
         level_error = "Seg line side number out of range";
         Z_Free(data);
         return;
      }

      li->sidedef = &sides[ldef->sidenum[side]];
      li->frontsector = sides[ldef->sidenum[side]].sector;

      // killough 5/3/98: ignore 2s flag if second sidedef missing:
      if(ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=-1)
         li->backsector = sides[ldef->sidenum[side^1]].sector;
      else
         li->backsector = nullptr;

      P_CalcSegLength(li);
   }
   Z_Free(data);
}

//
// P_LoadSubsectors
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadSubsectors(int lump)
{
   mapsubsector_t *mss;
   byte *data;
   int  i;
   
   numsubsectors = setupwad->lumpLength(lump) / sizeof(mapsubsector_t);
   if(numsubsectors <= 0)
   {
      level_error = "no subsectors in level (is this compressed ZDBSP?)";
      return;
   }
   subsectors = estructalloctag(subsector_t, numsubsectors, PU_LEVEL);
   data = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));
   
   for(i = 0; i < numsubsectors; ++i)
   {
      mss = &(((mapsubsector_t *)data)[i]);

      // haleyjd 06/19/06: convert indices to unsigned
      subsectors[i].numlines  = (int)SwapShort(mss->numsegs ) & 0xffff;
      subsectors[i].firstline = (int)SwapShort(mss->firstseg) & 0xffff;
   }
   
   Z_Free(data);
}

//
// P_LoadSubsectors_V4
//
// ioanch 20160204: DeepBSP nodes. Thanks to PrBoom for implementing it.
//
static void P_LoadSubsectors_V4(int lump)
{
   
   numsubsectors = setupwad->lumpLength(lump) / sizeof(mapsubsector_v4_t);
   subsectors = estructalloctag(subsector_t, numsubsectors, PU_LEVEL);

   auto data = static_cast<mapsubsector_v4_t *>(
      setupwad->cacheLumpNum(lump, PU_STATIC));

   if(!numsubsectors || !data)
   {
      level_error = "no subsectors in level";
      Z_Free(data);
      return;
   }

   for(int i = 0; i < numsubsectors; ++i)
   {
      subsectors[i].numlines = static_cast<int>(SwapUShort(data[i].numsegs)) 
         & 0xffff;
      subsectors[i].firstline = static_cast<int>(SwapLong(data[i].firstseg));
   }

   Z_Free(data);
}

//
// P_InitSector
//
// Shared initialization code for sectors across all map formats.
// IOANCH 20151212: made extern
//
void P_InitSector(sector_t *ss)
{
   // haleyjd 09/24/06: determine what the default sound sequence is
   int defaultSndSeq = LevelInfo.noAutoSequences ? 0 : -1;

   ss->nextsec = -1; //jff 2/26/98 add fields to support locking out
   ss->prevsec = -1; // stair retriggering until build completes

   // killough 3/7/98:
   ss->heightsec     = -1;   // sector used to get floor and ceiling height
   ss->srf.floor.lightsec = -1;   // sector used to get floor lighting
   // killough 3/7/98: end changes

   // killough 4/11/98 sector used to get ceiling lighting:
   ss->srf.ceiling.lightsec = -1;

   // killough 4/4/98: colormaps:
   // haleyjd 03/04/07: modifications for per-sector colormap logic
   // MaxW  2016/08/06: Modified to not set colormap if already set (UDMF).
   int tempcolmap = ((ss->intflags & SIF_SKY) ? global_fog_index : global_cmap_index);
   if(LevelInfo.mapFormat == LEVEL_FORMAT_UDMF_ETERNITY)
   {
      if(ss->bottommap == -1)
         ss->bottommap = tempcolmap;
      if(ss->midmap == -1)
         ss->midmap = tempcolmap;
      if(ss->topmap == -1)
         ss->topmap = tempcolmap;
   }
   else
      ss->bottommap = ss->midmap = ss->topmap = tempcolmap;

   //ss->bottommap = ss->midmap = ss->topmap =
   //   ((ss->intflags & SIF_SKY) ? global_fog_index : global_cmap_index);

   // SoM 9/19/02: Initialize the attached sector list for 3dsides
   ss->srf.ceiling.attached = ss->srf.floor.attached = nullptr;
   // SoM 11/9/04: 
   ss->srf.ceiling.attsectors = ss->srf.floor.attsectors = nullptr;

   // SoM 10/14/07:
   ss->srf.ceiling.asurfaces = ss->srf.floor.asurfaces = nullptr;

   // SoM: init portals
   ss->srf.ceiling.pflags = ss->srf.floor.pflags = 0;
   ss->srf.ceiling.portal = ss->srf.floor.portal = nullptr;
   ss->groupid = R_NOGROUP;

   // SoM: These are kept current with floorheight and ceilingheight now
   ss->srf.floor.heightf = M_FixedToFloat(ss->srf.floor.height);
   ss->srf.ceiling.heightf = M_FixedToFloat(ss->srf.ceiling.height);

   // needs to be defaulted as it starts as nonzero
   ss->srf.floor.scale = { 1.0f, 1.0f };
   ss->srf.ceiling.scale = { 1.0f, 1.0f };

   // haleyjd 09/24/06: sound sequences -- set default
   ss->sndSeqID = defaultSndSeq;

   // haleyjd 01/12/14: set sound zone to -1 
   ss->soundzone = -1;

   // CPP_FIXME: temporary placement construction for sound origins
   ::new (&ss->soundorg)  PointThinker;
   ::new (&ss->csoundorg) PointThinker;
}

#define DOOM_SECTOR_SIZE 26
#define PSX_SECTOR_SIZE  28

//
// P_LoadPSXSectors
//
// haleyjd 12/12/13: support for PSX port's sector format.
//
static void P_LoadPSXSectors(int lumpnum)
{
   ZAutoBuffer buf;
   char namebuf[9];
   
   numsectors  = setupwad->lumpLength(lumpnum) / PSX_SECTOR_SIZE;
   sectors     = estructalloctag(sector_t, numsectors, PU_LEVEL);
   
   setupwad->cacheLumpAuto(lumpnum, buf);
   auto data = buf.getAs<byte *>();

   // init texture name buffer to ensure null-termination
   memset(namebuf, 0, sizeof(namebuf));

   for(int i = 0; i < numsectors; i++)
   {
      sector_t *ss = sectors + i;

      ss->srf.floor.height = GetBinaryWord(data) << FRACBITS;
      ss->srf.ceiling.height = GetBinaryWord(data) << FRACBITS;
      GetBinaryString(data, namebuf, 8);
      ss->srf.floor.pic = R_FindFlat(namebuf);
      GetBinaryString(data, namebuf, 8);
      P_SetSectorCeilingPic(ss, R_FindFlat(namebuf));
      ss->lightlevel         = *data++;
      ++data;                // skip color ID for now
      ss->special            = GetBinaryWord(data);
      ss->tag                = GetBinaryWord(data);
      data += 2;             // skip flags field for now

      // scale up light levels (experimental)
      ss->lightlevel = (ss->lightlevel * 11 / 18) + 96;

      P_InitSector(ss);
   }
}

//
// P_LoadSectors
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadSectors(int lumpnum)
{
   ZAutoBuffer buf;
   char namebuf[9];
   
   numsectors  = setupwad->lumpLength(lumpnum) / DOOM_SECTOR_SIZE;
   sectors     = estructalloctag(sector_t, numsectors, PU_LEVEL);
   
   setupwad->cacheLumpAuto(lumpnum, buf);
   auto data = buf.getAs<byte *>();

   // init texture name buffer to ensure null-termination
   memset(namebuf, 0, sizeof(namebuf));

   for(int i = 0; i < numsectors; i++)
   {
      sector_t *ss = sectors + i;

      ss->srf.floor.height = GetBinaryWord(data) << FRACBITS;
      ss->srf.ceiling.height = GetBinaryWord(data) << FRACBITS;
      GetBinaryString(data, namebuf, 8);
      ss->srf.floor.pic = R_FindFlat(namebuf);
      // haleyjd 08/30/09: set ceiling pic using function
      GetBinaryString(data, namebuf, 8);
      P_SetSectorCeilingPic(ss, R_FindFlat(namebuf));
      ss->lightlevel         = GetBinaryWord(data);
      ss->special            = GetBinaryWord(data);
      ss->tag                = GetBinaryWord(data);
    
      P_InitSector(ss);
   }
}

//
// haleyjd 01/05/14: Create sector interpolation structures.
//
static void P_createSectorInterps()
{
   sectorinterps = estructalloctag(sectorinterp_t, numsectors, PU_LEVEL);

   for(int i = 0; i < numsectors; i++)
   {
      sectorinterps[i].prevfloorheight    = sectors[i].srf.floor.height;
      sectorinterps[i].prevceilingheight  = sectors[i].srf.ceiling.height;
      sectorinterps[i].prevfloorheightf   = sectors[i].srf.floor.heightf;
      sectorinterps[i].prevceilingheightf = sectors[i].srf.ceiling.heightf;

      if(sectors[i].srf.floor.slope)
         sectorinterps[i].prevfloorslopezf = sectors[i].srf.floor.slope->of.z;
      if(sectors[i].srf.ceiling.slope)
         sectorinterps[i].prevceilingslopezf = sectors[i].srf.ceiling.slope->of.z;
   }
}

//
// Setup sector bounding boxes
//
static void P_createSectorBoundingBoxes()
{
   pSectorBoxes = estructalloctag(sectorbox_t, numsectors, PU_LEVEL);

   for(int i = 0; i < numsectors; ++i)
   {
      const sector_t &sector = sectors[i];
      fixed_t *box = pSectorBoxes[i].box;
      float *fbox = pSectorBoxes[i].fbox;
      M_ClearBox(box);
      M_ClearBox(fbox);
      for(int j = 0; j < sector.linecount; ++j)
      {
         const line_t &line = *sector.lines[j];
         M_AddToBox(box, line.v1->x, line.v1->y);
         M_AddToBox(box, line.v2->x, line.v2->y);
         M_AddToBox2(fbox, line.v1->fx, line.v1->fy);
         M_AddToBox2(fbox, line.v2->fx, line.v2->fy);
      }
   }
}

//
// P_propagateSoundZone
//
// haleyjd 01/12/14: Recursive routine to propagate a sound zone from a
// sector to all its neighboring sectors which border it by a 2S line which
// is not marked as a sound boundary.
//
static void P_propagateSoundZone(sector_t *sector, int zoneid)
{
   // if we already touched this sector somehow, return immediately
   if(sector->soundzone == zoneid)
      return;

   // set the zone to the sector
   sector->soundzone = zoneid;

   // iterate on the sector linedef list to find neighboring sectors
   for(int ln = 0; ln < sector->linecount; ln++)
   {
      auto line = sector->lines[ln];

      // must be 2S and not a zone boundary line
      if(!line->backsector || (line->extflags & EX_ML_ZONEBOUNDARY))
         continue;

      auto next = ((line->backsector != sector) ? line->backsector : line->frontsector);

      // if not already in the same sound zone, propagate recursively.
      if(next->soundzone != zoneid)
         P_propagateSoundZone(next, zoneid);
   }
}

//
// P_CreateSoundZones
//
// haleyjd 01/12/14: create sound environment zones for the map by using an
// alert-like propagation method.
//
static void P_CreateSoundZones()
{
   numsoundzones = 0;

   for(int secnum = 0; secnum < numsectors; secnum++)
   {
      auto sec = &sectors[secnum];
      
      // if the sector hasn't become part of a zone yet, do propagation for it
      if(sec->soundzone == -1)
         P_propagateSoundZone(sec, numsoundzones++);
   }

   // allocate soundzones
   soundzones = estructalloctag(soundzone_t, numsoundzones, PU_LEVEL);
   
   // set all zones to level default reverb, or engine default if level default
   // is not a valid reverb.
   ereverb_t *defReverb;
   if(!(defReverb = E_ReverbForID(LevelInfo.defaultEnvironment)))
      defReverb = E_GetDefaultReverb();

   for(int zonenum = 0; zonenum < numsoundzones; zonenum++)
      soundzones[zonenum].reverb = defReverb;
}

//
// P_CalcNodeCoefficients
//
// haleyjd 06/14/10: Separated from P_LoadNodes, this routine precalculates
// general line equation coefficients for a node, which are used during the
// process of dynaseg generation.
//
static void P_CalcNodeCoefficients(node_t *node, fnode_t *fnode)
{
   // haleyjd 05/16/08: keep floating point versions as well for dynamic
   // seg splitting operations
   double fx = (double)node->x;
   double fy = (double)node->y;
   double fdx = (double)node->dx;
   double fdy = (double)node->dy;

   // haleyjd 05/20/08: precalculate general line equation coefficients
   fnode->a = -fdy;
   fnode->b = fdx;
   fnode->c = fdy * fx - fdx * fy;
   fnode->len = sqrt(fdx * fdx + fdy * fdy);
}

//
// P_CalcNodeCoefficients2
//
// IOANCH 20151217: high-resolution nodes (from XGL3 ZDBSP nodes)
//
static void P_CalcNodeCoefficients2(const node_t &node, fnode_t &fnode)
{
   double fx = M_FixedToDouble(node.x);
   double fy = M_FixedToDouble(node.y);
   double fdx = M_FixedToDouble(node.dx);
   double fdy = M_FixedToDouble(node.dy);
   
   // IOANCH: same as code above
   fnode.a = -fdy;
   fnode.b = fdx;
   fnode.c = fdy * fx - fdx * fy;
   fnode.len = sqrt(fdx * fdx + fdy * fdy);
}

//
// P_LoadNodes
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadNodes(int lump)
{
   byte *data;
   int  i;
   
   numnodes = setupwad->lumpLength(lump) / sizeof(mapnode_t);

   // haleyjd 12/07/13: Doom engine is supposed to tolerate zero-length
   // nodes. All vanilla BSP walks are hacked to account for it by returning
   // subsector 0 if an attempt is made to access nodes[-1]
   if(!numnodes)
   {
      // ioanch 20160204: also check numsubsectors!
      if(numsubsectors <= 0)
         level_error = "no nodes in level";
      else
         C_Printf("trivial map (no nodes, one subsector)\n");
      nodes  = nullptr;
      fnodes = nullptr;
      return;
   }

   nodes  = estructalloctag(node_t,  numnodes, PU_LEVEL);
   fnodes = estructalloctag(fnode_t, numnodes, PU_LEVEL);
   data   = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));

   for(i = 0; i < numnodes; i++)
   {
      node_t *no = nodes + i;
      mapnode_t *mn = (mapnode_t *)data + i;
      int j;

      no->x  = SwapShort(mn->x);
      no->y  = SwapShort(mn->y);
      no->dx = SwapShort(mn->dx);
      no->dy = SwapShort(mn->dy);

      // haleyjd: calculate floating-point data
      P_CalcNodeCoefficients(no, &fnodes[i]);

      no->x  <<= FRACBITS;
      no->y  <<= FRACBITS;
      no->dx <<= FRACBITS;
      no->dy <<= FRACBITS;

      for(j = 0; j < 2; ++j)
      {
         int k;
         ShortToNodeChild(&(no->children[j]), SwapUShort(mn->children[j]));

         for(k = 0; k < 4; ++k)
            no->bbox[j][k] = SwapShort(mn->bbox[j][k]) << FRACBITS;
      }
   }
   
   Z_Free(data);
}

//
// P_LoadNodes_V4
//
// ioanch 20160204: load DEEPBSP nodes. Based on PrBoom code
//
static void P_LoadNodes_V4(int lump)
{
   numnodes = (setupwad->lumpLength(lump) - 8) / sizeof(mapnode_v4_t);
   auto data = static_cast<byte *>(setupwad->cacheLumpNum(lump, PU_STATIC));

   // haleyjd 12/07/13: Doom engine is supposed to tolerate zero-length
   // nodes. All vanilla BSP walks are hacked to account for it by returning
   // subsector 0 if an attempt is made to access nodes[-1]
   if(!numnodes || !data)
   {
      // ioanch 20160204: also check numsubsectors!
      Z_Free(data);  // free it if not null at this point
      if(numsubsectors <= 0)
         level_error = "no nodes in level";
      else
         C_Printf("trivial map (no nodes, one subsector)\n");
      nodes  = nullptr;
      fnodes = nullptr;
      return;
   }

   nodes = estructalloctag(node_t, numnodes, PU_LEVEL);
   fnodes = estructalloctag(fnode_t, numnodes, PU_LEVEL);
   
   // skip header
   data += 8;

   for(int i = 0; i < numnodes; ++i)
   {
      node_t *no = nodes + i;
      const mapnode_v4_t *mn = reinterpret_cast<const mapnode_v4_t *>(data) + i;
      int j;

      no->x = SwapShort(mn->x);
      no->y = SwapShort(mn->y);
      no->dx = SwapShort(mn->dx);
      no->dy = SwapShort(mn->dy);

      // haleyjd: calculate floating-point data
      P_CalcNodeCoefficients(no, &fnodes[i]);

      no->x  <<= FRACBITS;
      no->y  <<= FRACBITS;
      no->dx <<= FRACBITS;
      no->dy <<= FRACBITS;

      for(j = 0; j < 2; ++j)
      {
         int k;
         no->children[j] = SwapLong(mn->children[j]);
         for(k = 0; k < 4; ++k)
            no->bbox[j][k] = SwapShort(mn->bbox[j][k]) << FRACBITS;
      }
   }
   Z_Free(data - 8);
}

//
// P_CheckForDeePBSPv4Nodes
//
// ioanch 20160204: DeepBSP extended nodes support. Based on how it's already
// implemented in PrBoom
// Check for valid DEEPBSP nodes
//
static bool P_CheckForDeePBSPv4Nodes(int lumpnum)
{
   // Check size to avoid crashes
   if(setupwad->lumpLength(lumpnum + ML_NODES) < 8)
      return false;
   const void *data = setupwad->cacheLumpNum(lumpnum + ML_NODES, PU_CACHE);
   if(!memcmp(data, "xNd4\0\0\0\0", 8))
   {
      C_Printf("DeePBSP v4 Extended nodes detected\n");
      return true;
   }
   return false;
}

//=============================================================================
//
// haleyjd 06/14/10: ZDoom uncompressed nodes support
//

//
// http://zdoom.org/wiki/ZDBSP#Compressed_Nodes
// IOANCH 20151213: modified to use the NODES lump num and return the signature
// Also added actual node lump, if it's SSECTORS in a classic map
//
static ZNodeType P_checkForZDoomNodes(int nodelumpnum, int *actualNodeLump, bool udmf)
{
   const void *data;
   
   *actualNodeLump = nodelumpnum;
   bool glNodesFallback = false;

   // haleyjd: be sure something is actually there
   // ioanch: actually check for 4 bytes so we can memcmp for "XNOD"
   if(setupwad->lumpLength(nodelumpnum) < 4)
   {
      if(!udmf)
      {
         *actualNodeLump = nodelumpnum - ML_NODES + ML_SSECTORS;
         glNodesFallback = true;
         if(setupwad->lumpLength(*actualNodeLump) < 4)
            return ZNodeType_Invalid;
      }
      else
         return ZNodeType_Invalid;
   }

   // haleyjd: load at PU_CACHE and it may stick around for later.
   data = setupwad->cacheLumpNum(*actualNodeLump, PU_CACHE);

   if(!udmf && !glNodesFallback)
   {
      if(!memcmp(data, "XNOD", 4))
      {
         // only classic maps with NODES having XNOD
         C_Printf("ZDoom uncompressed normal nodes detected\n");
         return ZNodeType_Uncompressed_Normal;
      }
      if(!memcmp(data, "ZNOD", 4))
      {
         // Warn but don't quit (ZNOD may be accidental)
         C_Printf("Warning: ZNOD header detected; ZDBSP compressed nodes are not supported\n");
         return ZNodeType_Invalid;
      }
   }


   if(glNodesFallback || udmf)
   {
      if(!memcmp(data, "XGLN", 4))
      {
         C_Printf("ZDoom uncompressed GL nodes version 1 detected\n");
         return ZNodeType_Uncompressed_GL;
      }
      if(!memcmp(data, "XGL2", 4))
      {
         C_Printf("ZDoom uncompressed GL nodes version 2 detected\n");
         return ZNodeType_Uncompressed_GL2;
      }
      if(!memcmp(data, "XGL3", 4))
      {
         C_Printf("ZDoom uncompressed GL nodes version 3 detected\n");
         return ZNodeType_Uncompressed_GL3;
      }
      if(!memcmp(data, "ZGLN", 4) || !memcmp(data, "ZGL2", 4) || !memcmp(data, "ZGL3", 4))
      {
         // Warn but don't quit (ZGL* may be accidental)
         C_Printf("Warning: ZGL* header detected; ZDBSP compressed nodes are not supported\n");
         return ZNodeType_Invalid;
      }
   }


   return ZNodeType_Invalid;
}

//
// CheckZNodesOverflow
//
// For safe reading of ZDoom nodes lump.
//
static void CheckZNodesOverflowFN(int *size, int count)
{
   (*size) -= count;

   if((*size) < 0)
      level_error = "Overflow in ZDoom XNOD lump";
}

// IOANCH 20151217: updated for XGLN and XGL2
struct mapseg_znod_t
{
   uint32_t v1;
   union // IOANCH
   {
      uint32_t v2;
      uint32_t partner; // XGLN, XGL2
   };
   uint32_t linedef; // IOANCH: use 32-bit instead of 16-bit   
   byte     side;
};

// IOANCH: modified to support XGL3 nodes
struct mapnode_znod_t
{
  union
  {
     struct
     {
        int16_t x;  // Partition line from (x,y) to x+dx,y+dy)
        int16_t y;
        int16_t dx;
        int16_t dy;             
     };
     struct
     {
        int32_t x32;  // Partition line from (x,y) to x+dx,y+dy)
        int32_t y32;
        int32_t dx32;
        int32_t dy32;             
     };
  };
  // Bounding box for each child, clip against view frustum.
  int16_t bbox[2][4];
  // If NF_SUBSECTOR its a subsector, else it's a node of another subtree.
  int32_t children[2];
};

//
// R_DynaSegOffset
//
// Computes the offset value of the seg relative to its parent linedef.
// Not terribly fast.
// Derived from BSP 5.2 SplitDist routine.
//
// haleyjd 06/14/10: made global for map loading in p_setup.c and added
//                   side parameter.
// ioanch: made it local again, because dynasegs will use different coordinates for interpolation.
//
static void R_calcSegOffset(seg_t *lseg, const line_t *line, int side)
{
   float dx = (side ? line->v2->fx : line->v1->fx) - lseg->v1->fx;
   float dy = (side ? line->v2->fy : line->v1->fy) - lseg->v1->fy;

   lseg->offset = sqrtf(dx * dx + dy * dy);
}

//
// P_LoadZSegs
//
// Loads segs from ZDoom uncompressed nodes
// IOANCH 20151217: use signature
//
static void P_LoadZSegs(byte *data, ZNodeType signature)
{
   // IOANCH TODO: read the segs according to signature
   int i;
   
   subsector_t *ss = ::subsectors; // IOANCH: for GL znodes
   int actualSegIndex = 0;
   seg_t *prevSegToSet = nullptr;
   vertex_t *firstV1 = nullptr;   // IOANCH: first vertex of first seg

   for(i = 0; i < numsegs; i++, ++actualSegIndex)
   {
      line_t *ldef;
      uint32_t v1, v2 = 0;
      uint32_t linedef;
      byte side;
      seg_t *li = segs+actualSegIndex;
      mapseg_znod_t ml;
      
      // IOANCH: shift the seg back
      //::segs[actualSegIndex] = ::segs[i];
      
      // IOANCH: increment current subsector if applicable
      if(signature != ZNodeType_Uncompressed_Normal)
      {
         if(actualSegIndex >= ss->firstline + ss->numlines)
         {
            ++ss;
            ss->firstline = actualSegIndex;
            firstV1 = nullptr;
         }
      }

      // haleyjd: FIXME - see no verification of vertex indices
      v1 = ml.v1 = GetBinaryUDWord(data);
      if(signature == ZNodeType_Uncompressed_Normal)   // IOANCH: only set directly for nonGL
         v2 = ml.v2 = GetBinaryUDWord(data);
      else
      {
         if(actualSegIndex == ss->firstline && !firstV1) // only set it once
            firstV1 = ::vertexes + v1;
         else if(prevSegToSet)
         {
            // set the second vertex of previous
            prevSegToSet->v2 = ::vertexes + v1;   
            P_CalcSegLength(prevSegToSet);
            prevSegToSet = nullptr;   // consume it
         }
         
         ml.partner = GetBinaryUDWord(data);   // IOANCH: not used in EE
      }
      
      // IOANCH
      if(signature == ZNodeType_Uncompressed_Normal || signature == ZNodeType_Uncompressed_GL)
         ml.linedef = GetBinaryUWord(data);
      else
         ml.linedef = GetBinaryUDWord(data);
      ml.side    = *data++;
      
      if((signature == ZNodeType_Uncompressed_GL && ml.linedef == 0xffff)
         || ((signature == ZNodeType_Uncompressed_GL2 || signature == ZNodeType_Uncompressed_GL3) 
             && ml.linedef == 0xffffffff))
      {
         --actualSegIndex;
         --ss->numlines;
         continue;   // skip strictly GL nodes
      }

      linedef = SafeRealUintIndex(ml.linedef, numlines, "seg", actualSegIndex, 
                                  "line");

      ldef = &lines[linedef];
      li->linedef = ldef;
      side = ml.side;

      //e6y: fix wrong side index
      if(side != 0 && side != 1)
         side = 1;

      li->sidedef     = &sides[ldef->sidenum[side]];
      li->frontsector =  sides[ldef->sidenum[side]].sector;

      // killough 5/3/98: ignore 2s flag if second sidedef missing:
      if(ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1] != -1)
         li->backsector = sides[ldef->sidenum[side^1]].sector;
      else
         li->backsector = nullptr;

      li->v1 = &vertexes[v1];
      if(signature == ZNodeType_Uncompressed_Normal)
         li->v2 = &vertexes[v2];
      else
      {
         if(actualSegIndex + 1 == ss->firstline + ss->numlines)
         {
            li->v2 = firstV1;
            if(firstV1) // firstV1 can be null because of malformed subsectors
            {
               P_CalcSegLength(li);
               firstV1 = nullptr;
            }
            else
               level_error = "Bad ZDBSP nodes; can't start level.";
         }
         else
         {
            prevSegToSet = li;
         }
      }

      if(li->v2)  // IOANCH: only count if v2 is available.
         P_CalcSegLength(li);
      R_calcSegOffset(li, ldef, side);
   }
   
   // IOANCH: update the seg count
   ::numsegs = actualSegIndex;
}

#define CheckZNodesOverflow(size, count) \
   CheckZNodesOverflowFN(&(size), (count)); \
   if(level_error) \
   { \
      Z_Free(lumpptr); \
      return; \
   }

//
// P_LoadZNodes
//
// Loads ZDoom uncompressed nodes.
// IOANCH 20151217: check signature and use different gl nodes if needed
// ioanch: 20151221: fixed some memory leaks. Also moved the bounds checks 
// before attempting to allocate memory, so the app won't terminate.
//
static void P_LoadZNodes(int lump, ZNodeType signature)
{
   byte *data, *lumpptr;
   unsigned int i;
   int len;

   uint32_t orgVerts, newVerts;
   uint32_t numSubs, currSeg;
   uint32_t numSegs;
   uint32_t numNodes;
   vertex_t *newvertarray = nullptr;

   data = lumpptr = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));
   len  = setupwad->lumpLength(lump);

   // skip header
   CheckZNodesOverflow(len, 4);
   data += 4;

   // Read extra vertices added during node building
   CheckZNodesOverflow(len, sizeof(orgVerts));  
   orgVerts = GetBinaryUDWord(data);

   CheckZNodesOverflow(len, sizeof(newVerts));
   newVerts = GetBinaryUDWord(data);

   // ioanch: moved before the potential allocation
   CheckZNodesOverflow(len, newVerts * 2 * sizeof(int32_t));
   if(orgVerts + newVerts == (unsigned int)numvertexes)
   {
      newvertarray = vertexes;
   }
   else
   {
      // ioanch: use tag (here and elsewhere)
      newvertarray = estructalloctag(vertex_t, orgVerts + newVerts, PU_LEVEL);
      memcpy(newvertarray, vertexes, orgVerts * sizeof(vertex_t));
   }
   
   for(i = 0; i < newVerts; i++)
   {
      int vindex = i + orgVerts;

      newvertarray[vindex].x = (fixed_t)GetBinaryDWord(data);
      newvertarray[vindex].y = (fixed_t)GetBinaryDWord(data);

      // SoM: Cardboard stores float versions of vertices.
      newvertarray[vindex].fx = M_FixedToFloat(newvertarray[vindex].x);
      newvertarray[vindex].fy = M_FixedToFloat(newvertarray[vindex].y);
   }

   if(vertexes != newvertarray)
   {
      // fixup linedef vertex pointers
      for(i = 0; i < (unsigned int)numlinesPlusExtra; i++)
      {
         lines[i].v1 = lines[i].v1 - vertexes + newvertarray;
         lines[i].v2 = lines[i].v2 - vertexes + newvertarray;
      }
      efree(vertexes);
      vertexes = newvertarray;
      numvertexes = (int)(orgVerts + newVerts);
   }

   // Read the subsectors
   CheckZNodesOverflow(len, sizeof(numSubs));
   numSubs = GetBinaryUDWord(data);

   numsubsectors = (int)numSubs;
   if(numsubsectors <= 0)
   {
      level_error = "no subsectors in level";
      Z_Free(lumpptr);
      return;
   }

   CheckZNodesOverflow(len, numSubs * sizeof(uint32_t));
   subsectors = estructalloctag(subsector_t, numsubsectors, PU_LEVEL);

   for(i = currSeg = 0; i < numSubs; i++)
   {
      subsectors[i].firstline = (int)currSeg;
      subsectors[i].numlines  = (int)(GetBinaryUDWord(data));
      currSeg += subsectors[i].numlines;
   }

   // Read the segs
   CheckZNodesOverflow(len, sizeof(numSegs));
   numSegs = GetBinaryUDWord(data);

   // The number of segs stored should match the number of
   // segs used by subsectors.
   if(numSegs != currSeg)
   {
      level_error = "incorrect number of segs in nodes";
      Z_Free(lumpptr);
      return;
   }

   numsegs = (int)numSegs;

   // IOANCH 20151217: set reading size
   int totalSegSize;
   if(signature == ZNodeType_Uncompressed_Normal || signature == ZNodeType_Uncompressed_GL)
      totalSegSize = numsegs * 11; // haleyjd: hardcoded original structure size
   else
      totalSegSize = numsegs * 13; // IOANCH: DWORD linedef
   
   CheckZNodesOverflow(len, totalSegSize);
   segs = estructalloctag(seg_t, numsegs, PU_LEVEL);
   P_LoadZSegs(data, signature);
   
   data += totalSegSize;
   
   // Read nodes
   CheckZNodesOverflow(len, sizeof(numNodes));
   numNodes = GetBinaryUDWord(data);

   numnodes = numNodes;
   CheckZNodesOverflow(len, numNodes * 32);
   nodes  = estructalloctag(node_t,  numNodes, PU_LEVEL);
   fnodes = estructalloctag(fnode_t, numNodes, PU_LEVEL);

   for (i = 0; i < numNodes; i++)
   {
      int j, k;
      node_t *no = nodes + i;
      mapnode_znod_t mn;

      if(signature == ZNodeType_Uncompressed_GL3)
      {
         mn.x32  = GetBinaryDWord(data);
         mn.y32  = GetBinaryDWord(data);
         mn.dx32 = GetBinaryDWord(data);
         mn.dy32 = GetBinaryDWord(data);
      }
      else
      {
         mn.x  = GetBinaryWord(data);
         mn.y  = GetBinaryWord(data);
         mn.dx = GetBinaryWord(data);
         mn.dy = GetBinaryWord(data);
      }

      for(j = 0; j < 2; j++)
         for(k = 0; k < 4; k++)
            mn.bbox[j][k] = GetBinaryWord(data);

      for(j = 0; j < 2; j++)
         mn.children[j] = GetBinaryDWord(data);

      if(signature == ZNodeType_Uncompressed_GL3)
      {
         no->x = mn.x32;
         no->y = mn.y32;
         no->dx = mn.dx32;
         no->dy = mn.dy32;
         
         P_CalcNodeCoefficients2(*no, fnodes[i]);
      }
      else
      {
         no->x  = mn.x; 
         no->y  = mn.y; 
         no->dx = mn.dx;
         no->dy = mn.dy;

         P_CalcNodeCoefficients(no, &fnodes[i]);

         no->x  <<= FRACBITS;
         no->y  <<= FRACBITS;
         no->dx <<= FRACBITS;
         no->dy <<= FRACBITS;
      }

      for(j = 0; j < 2; j++)
      {
         no->children[j] = (unsigned int)(mn.children[j]);

         for(k = 0; k < 4; k++)
            no->bbox[j][k] = (fixed_t)mn.bbox[j][k] << FRACBITS;
      }
   }

   Z_Free(lumpptr);
}

//
// End ZDoom nodes
//
//=============================================================================

// IOANCH 20151214: deleted static definitions
static void P_ConvertPSXThing(mapthing_t *mthing);

//
// P_CheckThingDoomBan
//
// IOANCH 20151213: Checks that a thing type is banned from Doom 1
// Needed as a separate function because of UDMF
//
bool P_CheckThingDoomBan(int16_t type)
{
   if(demo_version < 331 && GameModeInfo->type == Game_DOOM &&
      GameModeInfo->id != commercial)
   {
      switch(type)
      {
      case 68:  // Arachnotron
      case 64:  // Archvile
      case 88:  // Boss Brain
      case 89:  // Boss Shooter
      case 69:  // Hell Knight
      case 67:  // Mancubus
      case 71:  // Pain Elemental
      case 65:  // Former Human Commando
      case 66:  // Revenant
      case 84:  // Wolf SS
         return true;
      }
   }
   return false;
}

//
// P_LoadThings
//
// killough 5/3/98: reformatted, cleaned up
//
// sf: added spawnedthings for scripting
//
// haleyjd: added missing player start detection
//
static void P_LoadThings(int lump)
{
   int  i;
   byte *data = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));
   mapthing_t *mapthings;
   
   numthings = setupwad->lumpLength(lump) / sizeof(mapthingdoom_t); //sf: use global

   // haleyjd 03/03/07: allocate full mapthings
   mapthings = ecalloc(mapthing_t *, numthings, sizeof(mapthing_t));
   
   for(i = 0; i < numthings; i++)
   {
      mapthingdoom_t *mt = (mapthingdoom_t *)data + i;
      mapthing_t     *ft = &mapthings[i];
      
      // haleyjd 09/11/06: wow, this should be up here.
      ft->type = SwapShort(mt->type);

      // Do not spawn cool, new monsters if !commercial
      // haleyjd: removing this for Heretic and DeHackEd
      // IOANCH 20151213: use function
      if(P_CheckThingDoomBan(ft->type))
      {
         if(demo_compatibility) // emulate old behaviour where thing processing is totally abandoned
            break;
         continue;
      }
      
      // Do spawn all other stuff.
      // ioanch 20151218: fixed point coordinates
      ft->x       = SwapShort(mt->x) << FRACBITS;
      ft->y       = SwapShort(mt->y) << FRACBITS;
      ft->angle   = SwapShort(mt->angle);      
      ft->options = SwapShort(mt->options);

      // PSX special behaviors
      if(LevelInfo.mapFormat == LEVEL_FORMAT_PSX)
         P_ConvertPSXThing(ft);

      // haleyjd 10/05/05: convert heretic things
      if(LevelInfo.levelType == LI_TYPE_HERETIC)
         P_ConvertHereticThing(ft);

      // haleyjd 12/27/13: convert Doom extended thing numbers
      P_ConvertDoomExtendedSpawnNum(ft);
      
      P_SpawnMapThing(ft);
   }
   
   // haleyjd: all player things for players in this game should now be valid
   if(GameType != gt_dm)
   {
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i] && !players[i].mo)
            level_error = "Missing required player start";
      }
   }

   Z_Free(data);
   Z_Free(mapthings);
}

//
// P_LoadHexenThings
//
// haleyjd: Loads a Hexen-format THINGS lump.
//
static void P_LoadHexenThings(int lump)
{
   int  i;
   byte *data = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));
   mapthing_t *mapthings;
   
   numthings = setupwad->lumpLength(lump) / sizeof(mapthinghexen_t);

   // haleyjd 03/03/07: allocate full mapthings
   mapthings = ecalloc(mapthing_t *, numthings, sizeof(mapthing_t));
   
   for(i = 0; i < numthings; i++)
   {
      mapthinghexen_t *mt = (mapthinghexen_t *)data + i;
      mapthing_t      *ft = &mapthings[i];
      
      ft->tid     = SwapShort(mt->tid);
      // ioanch 20151218: fixed point coordinates
      ft->x       = SwapShort(mt->x) << FRACBITS;
      ft->y       = SwapShort(mt->y) << FRACBITS;
      ft->height  = SwapShort(mt->height) << FRACBITS;
      ft->angle   = SwapShort(mt->angle);
      ft->type    = SwapShort(mt->type);
      ft->options = SwapShort(mt->options);

      // note: args are already in order since they're just bytes
      ft->special = mt->special;
      ft->args[0] = mt->args[0];
      ft->args[1] = mt->args[1];
      ft->args[2] = mt->args[2];
      ft->args[3] = mt->args[3];
      ft->args[4] = mt->args[4];

      // haleyjd 10/05/05: convert heretic things
      if(LevelInfo.levelType == LI_TYPE_HERETIC)
         P_ConvertHereticThing(ft);

      // haleyjd 12/27/13: convert Doom extended thing numbers
      P_ConvertDoomExtendedSpawnNum(ft);
      
      P_SpawnMapThing(ft);
   }
   
   // haleyjd: all player things for players in this game
   //          should now be valid in SP or co-op
   if(GameType != gt_dm)
   {
      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(playeringame[i] && !players[i].mo)
            level_error = "Missing required player start";
      }
   }

   Z_Free(data);
   Z_Free(mapthings);
}

//
// P_InitLineDef
//
// haleyjd 03/28/11: Shared code for DOOM- and Hexen-format linedef loading.
// IOANCH 20151213: made global
//
void P_InitLineDef(line_t *ld)
{
   vertex_t *v1 = ld->v1, *v2 = ld->v2;

   // Graphical properties
   ld->tranlump = -1;    // killough 4/11/98: no translucency by default
   ld->alpha    =  1.0f; // haleyjd 11/11/10: flex/additive; default to opaque

   // Slopes
   ld->dx = v2->x - v1->x;
   ld->dy = v2->y - v1->y;
   ld->slopetype = !ld->dx ? ST_VERTICAL : !ld->dy ? ST_HORIZONTAL :
      FixedDiv(ld->dy, ld->dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

   // SoM: Calculate line normal
   P_MakeLineNormal(ld);

   // Bounding box
   if(v1->x < v2->x)
   {
      ld->bbox[BOXLEFT]  = v1->x;
      ld->bbox[BOXRIGHT] = v2->x;
   }
   else
   {
      ld->bbox[BOXLEFT]  = v2->x;
      ld->bbox[BOXRIGHT] = v1->x;
   }

   if(v1->y < v2->y)
   {
      ld->bbox[BOXBOTTOM] = v1->y;
      ld->bbox[BOXTOP]    = v2->y;
   }
   else
   {
      ld->bbox[BOXBOTTOM] = v2->y;
      ld->bbox[BOXTOP]    = v1->y;
   }

   // haleyjd 12/04/08: rangechecking side numbers for safety
   if(ld->sidenum[0] >= numsides)
   {
      C_Printf(FC_ERROR "Line error: bad side 0 #%d\a\n", ld->sidenum[0]);
      ld->sidenum[0] = 0;
   }
   if(ld->sidenum[1] >= numsides)
   {
      C_Printf(FC_ERROR "Line error: bad side 1 #%d\a\n", ld->sidenum[1]);
      ld->sidenum[1] = 0;
   }

   // killough 4/4/98: support special sidedef interpretation below
   if(ld->sidenum[0] != -1 && ld->special)
      sides[*ld->sidenum].special = ld->special;

   // CPP_FIXME: temporary placement construction for sound origins
   ::new (&ld->soundorg) PointThinker;

   // haleyjd 04/19/09: position line sound origin
   ld->soundorg.x = ld->v1->x + ld->dx / 2;
   ld->soundorg.y = ld->v1->y + ld->dy / 2;
   ld->soundorg.groupid = R_NOGROUP;
}

#define ML_PSX_MIDMASKED      0x200
#define ML_PSX_MIDTRANSLUCENT 0x400
#define ML_PSX_MIDSOLID       0x800

//
// P_PostProcessLineFlags
//
// haleyjd 04/30/11: Make some line flags consistent.
// IOANCH 20151213: made global
//
void P_PostProcessLineFlags(line_t *ld)
{
   // EX_ML_BLOCKALL implies that ML_BLOCKING should be set.
   if(ld->extflags & EX_ML_BLOCKALL)
      ld->flags |= ML_BLOCKING;
}

//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//        ^^^
// ??? killough ???
// Does this mean secrets used to be linedef-based, rather than sector-based?
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
// killough 5/3/98: reformatted, cleaned up
// haleyjd 2/26/05: ExtraData extensions
//
static void P_LoadLineDefs(int lump, UDMFSetupSettings &setupSettings)
{
   byte *data;

   numlines = setupwad->lumpLength(lump) / sizeof(maplinedef_t);
   numlinesPlusExtra = numlines + NUM_LINES_EXTRA;
   lines    = estructalloctag(line_t, numlinesPlusExtra, PU_LEVEL);
   data     = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));

   for(int i = 0; i < numlines; i++)
   {
      maplinedef_t *mld = (maplinedef_t *)data + i;
      line_t *ld = lines + i;

      ld->flags   = SwapShort(mld->flags);
      ld->special = (int)(SwapShort(mld->special)) & 0xffff;
      ld->tag     = SwapShort(mld->tag);
      ld->args[0] = ld->tag;

      // haleyjd 06/19/06: convert indices to unsigned
      ld->v1 = &vertexes[SafeUintIndex(mld->v1, numvertexes, "line", i, "vertex")];
      ld->v2 = &vertexes[SafeUintIndex(mld->v2, numvertexes, "line", i, "vertex")];

      // haleyjd 06/19/06: convert indices, except -1, to unsigned
      ld->sidenum[0] = ShortToLong(SwapShort(mld->sidenum[0]));
      ld->sidenum[1] = ShortToLong(SwapShort(mld->sidenum[1]));

      // haleyjd 03/28/11: do shared loading logic in one place
      P_InitLineDef(ld);

      // haleyjd 02/03/13: get ExtraData static init binding
      int edlinespec = EV_SpecialForStaticInit(EV_STATIC_EXTRADATA_LINEDEF);

      // haleyjd 02/26/05: ExtraData
      // haleyjd 04/20/08: Implicit ExtraData lines
      if((edlinespec && ld->special == edlinespec) ||
         EV_IsParamLineSpec(ld->special) || EV_IsParamStaticInit(ld->special))
         E_LoadLineDefExt(ld, ld->special == edlinespec, setupSettings);

      // haleyjd 04/30/11: Do some post-ExtraData line flag adjustments
      P_PostProcessLineFlags(ld);
   }
   Z_Free(data);
}

// these flags are shared with Hexen in the normal flags fields
#define HX_SHAREDFLAGS \
   (ML_BLOCKING|ML_BLOCKMONSTERS|ML_TWOSIDED|ML_DONTPEGTOP|ML_DONTPEGBOTTOM| \
    ML_SECRET|ML_SOUNDBLOCK|ML_DONTDRAW|ML_MAPPED)

// line flags belonging only to Hexen
#define HX_ML_REPEAT_SPECIAL 0x0200

// special activation crap -- definitely the only place this will be used
#define HX_SPAC_SHIFT 10
#define HX_SPAC_MASK  0x1c00
#define HX_GET_SPAC(flags) ((flags & HX_SPAC_MASK) >> HX_SPAC_SHIFT)

enum
{
   HX_SPAC_CROSS,  // when player crosses line
   HX_SPAC_USE,    // when player uses line
   HX_SPAC_MCROSS, // when monster crosses line
   HX_SPAC_IMPACT, // when projectile hits line
   HX_SPAC_PUSH,   // when player/monster pushes line
   HX_SPAC_PCROSS, // when projectile crosses line
   HX_SPAC_NUMSPAC
};

// haleyjd 02/28/07: tablified
static int spac_flags_tlate[HX_SPAC_NUMSPAC] =
{
   EX_ML_CROSS  | EX_ML_PLAYER,                   // SPAC_CROSS
   EX_ML_USE    | EX_ML_PLAYER,                   // SPAC_USE
   EX_ML_CROSS  | EX_ML_MONSTER,                  // SPAC_MCROSS
   EX_ML_IMPACT | EX_ML_PLAYER  | EX_ML_MISSILE,  // SPAC_IMPACT
   EX_ML_PUSH,                                    // SPAC_PUSH
   EX_ML_CROSS  | EX_ML_MISSILE                   // SPAC_PCROSS
};

//
// P_ConvertHexenLineFlags
//
// Uses the defines above to fiddle with the Hexen map format's line flags
// and turn them into something meaningful to Eternity. The values get split
// between line->flags and line->extflags.
//
static void P_ConvertHexenLineFlags(line_t *line)
{
   // extract Hexen special activation information
   int16_t spac = HX_GET_SPAC(line->flags);

   // translate into ExtraData extended line flags
   line->extflags = spac_flags_tlate[spac];

   // check for repeat flag
   if(line->flags & HX_ML_REPEAT_SPECIAL)
      line->extflags |= EX_ML_REPEAT;

   // clear line flags to those that are shared with DOOM
   line->flags &= HX_SHAREDFLAGS;
}

//
// P_LoadHexenLineDefs
//
// haleyjd: Loads a Hexen-format LINEDEFS lump.
//
static void P_LoadHexenLineDefs(int lump)
{
   byte *data;
   int  i;

   numlines = setupwad->lumpLength(lump) / sizeof(maplinedefhexen_t);
   numlinesPlusExtra = numlines + NUM_LINES_EXTRA;
   lines    = estructalloctag(line_t, numlinesPlusExtra, PU_LEVEL);
   data     = (byte *)(setupwad->cacheLumpNum(lump, PU_STATIC));

   for(i = 0; i < numlines; ++i)
   {
      maplinedefhexen_t *mld = (maplinedefhexen_t *)data + i;
      line_t *ld = lines + i;

      ld->flags   = SwapShort(mld->flags);
      ld->special = mld->special;
      
      for(int argnum = 0; argnum < NUMHXLINEARGS; argnum++)
         ld->args[argnum] = mld->args[argnum];

      // Convert line flags after setting special?
      P_ConvertHexenLineFlags(ld);

      ld->tag = -1;    // haleyjd 02/27/07

      // haleyjd 06/19/06: convert indices to unsigned
      ld->v1 = &vertexes[SafeUintIndex(mld->v1, numvertexes, "line", i, "vertex")];
      ld->v2 = &vertexes[SafeUintIndex(mld->v2, numvertexes, "line", i, "vertex")];

      // haleyjd 06/19/06: convert indices, except -1, to unsigned
      ld->sidenum[0] = ShortToLong(SwapShort(mld->sidenum[0]));
      ld->sidenum[1] = ShortToLong(SwapShort(mld->sidenum[1]));

      // haleyjd 03/28/11: do shared loading logic in one place
      P_InitLineDef(ld);
   }
   Z_Free(data);
}

//
// P_LoadLineDefs2
//
// killough 4/4/98: delay using sidedefs until they are loaded
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadLineDefs2()
{
   line_t *ld = lines;

   for(int i = numlines; i--; ld++)
   {
      // killough 11/98: fix common wad errors (missing sidedefs):
      
      if(ld->sidenum[0] == -1)
      {
         C_Printf(FC_ERROR "line error: missing first sidedef replaced\a\n");
         ld->sidenum[0] = 0;  // Substitute dummy sidedef for missing right side
      }

      if(ld->sidenum[1] == -1)
         ld->flags &= ~ML_TWOSIDED;  // Clear 2s flag for missing left side

      // handle PSX line flags
      if(LevelInfo.mapFormat == LEVEL_FORMAT_PSX)
      {
         if(ld->flags & ML_PSX_MIDTRANSLUCENT)
         {
            ld->alpha = 3.0f/4.0f;
            ld->extflags |= EX_ML_ADDITIVE;
         }

         if(ld->sidenum[1] != -1)
         {
            // 2S line midtextures without any of the 3 flags set don't draw.
            if(!(ld->flags & (ML_PSX_MIDMASKED|ML_PSX_MIDTRANSLUCENT|ML_PSX_MIDSOLID)))
            {
               sides[ld->sidenum[0]].midtexture = 0;
               sides[ld->sidenum[1]].midtexture = 0;
            }

            // All 2S lines clip their textures to their sector heights
            ld->extflags |= EX_ML_CLIPMIDTEX;
         }

         ld->flags &= 0x1FF; // clear all extended flags
      }

      // haleyjd 05/02/06: Reserved line flag. If set, we must clear all
      // BOOM or later extended line flags. This is necessitated by E2M7.
      if(ld->flags & ML_RESERVED)
         ld->flags &= 0x1FF;

      // haleyjd 03/13/05: removed redundant -1 check for first side
      ld->frontsector = sides[ld->sidenum[0]].sector;
      ld->backsector  = ld->sidenum[1] != -1 ? sides[ld->sidenum[1]].sector : nullptr;
      
      // haleyjd 02/06/13: lookup static init
      int staticFn = EV_StaticInitForSpecial(ld->special);

      int lump, j;
      switch(staticFn)
      {                       // killough 4/11/98: handle special types
      case EV_STATIC_TRANSLUCENT: // killough 4/11/98: translucent 2s textures
         lump = sides[*ld->sidenum].special; // translucency from sidedef
         if(!ld->args[0])                    // if tag == 0,
            ld->tranlump = lump;             // affect this linedef only
         else
         {
            for(j = 0; j < numlines; ++j)    // if tag != 0,
            {
               if(lines[j].tag == ld->args[0])   // affect all matching linedefs
                  lines[j].tranlump = lump;
            }
         }
         break;

      default:
         break;
      }
   } // end for
}

//
// P_LoadSideDefs
//
// killough 4/4/98: split into two functions
//
static void P_LoadSideDefs(int lump)
{
   numsides = setupwad->lumpLength(lump) / sizeof(mapsidedef_t);
   sides    = estructalloctag(side_t, numsides, PU_LEVEL);
}

//
// P_SetupSidedefTextures
//
// IOANCH 20151213: moved this here because it's used both by the Doom/Hexen
// formats and the UDMF format, called elsewhere
//
void P_SetupSidedefTextures(side_t &sd, const char *bottomTexture, 
                            const char *midTexture, const char *topTexture)
{
   // killough 4/4/98: allow sidedef texture names to be overloaded
   // killough 4/11/98: refined to allow colormaps to work as wall
   // textures if invalid as colormaps but valid as textures.
   // haleyjd 02/06/13: look up static init function

   int staticFn = EV_StaticInitForSpecial(sd.special);
   int cmap;
   sector_t &sec = *sd.sector;

   switch(staticFn)
   {
   case EV_STATIC_TRANSFER_HEIGHTS:
      // variable colormap via 242 linedef
      if((cmap = R_ColormapNumForName(bottomTexture)) < 0)
         sd.bottomtexture = R_FindWall(bottomTexture);
      else
      {
         sec.bottommap = cmap | COLORMAP_BOOMKIND;
         sd.bottomtexture = 0;
      }
      if((cmap = R_ColormapNumForName(midTexture)) < 0)
         sd.midtexture = R_FindWall(midTexture);
      else
      {
         sec.midmap = cmap | COLORMAP_BOOMKIND;
         sd.midtexture = 0;
      }
      if((cmap = R_ColormapNumForName(topTexture)) < 0)
         sd.toptexture = R_FindWall(topTexture);
      else
      {
         sec.topmap = cmap | COLORMAP_BOOMKIND;
         sd.toptexture = 0;
      }
      break;

   case EV_STATIC_TRANSLUCENT:
      // killough 4/11/98: apply translucency to 2s normal texture
      if(strncasecmp("TRANMAP", midTexture, 8))
      {
         sd.special = W_CheckNumForName(midTexture);
         if(sd.special < 0 || W_LumpLength(sd.special) != 65536)
         {
            // not found or not apparently a tranmap lump, try texture.
            sd.special = 0;
            sd.midtexture = R_FindWall(midTexture);
         }
         else
         {
            // bump it up by one to make a tranmap index; clear texture.
            ++sd.special;
            sd.midtexture = 0;
         }
      }
      else
      {
         // is "TRANMAP", which is generated as tranmap #0
         sd.special = 0;
         sd.midtexture = 0;
      }
      sd.toptexture = R_FindWall(topTexture);
      sd.bottomtexture = R_FindWall(bottomTexture);
      break;

   default:
      sd.midtexture = R_FindWall(midTexture);
      sd.toptexture = R_FindWall(topTexture);
      sd.bottomtexture = R_FindWall(bottomTexture);
      break;
   }
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSideDefs2(int lumpnum)
{
   byte *lump = (byte *)(setupwad->cacheLumpNum(lumpnum, PU_STATIC));
   byte *data = lump;
   int  i;
   char toptexture[9], bottomtexture[9], midtexture[9];

   // haleyjd: initialize texture name buffers for null termination
   memset(toptexture,    0, sizeof(toptexture));
   memset(bottomtexture, 0, sizeof(bottomtexture));
   memset(midtexture,    0, sizeof(midtexture));

   for(i = 0; i < numsides; i++)
   {
      side_t *sd = sides + i;
      int secnum;

      sd->textureoffset = GetBinaryWord(data) << FRACBITS;
      sd->rowoffset     = GetBinaryWord(data) << FRACBITS; 

      // haleyjd 05/26/10: read texture names into buffers
      GetBinaryString(data, toptexture,    8);
      GetBinaryString(data, bottomtexture, 8);
      GetBinaryString(data, midtexture,    8);

      // haleyjd 06/19/06: convert indices to unsigned
      secnum = SafeUintIndex(GetBinaryWord(data), numsectors, "side", i, "sector");
      sd->sector = &sectors[secnum];

      // IOANCH 20151213: this will be in a separate function, because UDMF
      // also uses it
      P_SetupSidedefTextures(*sd, bottomtexture, midtexture, toptexture);
   }

   Z_Free(lump);
}

// haleyjd 10/10/11: externalized structure due to pre-C++11 template limitations
struct bmap_t { int n, nalloc, *list; }; // blocklist structure

//
// Boom variant of blockmap creation, which will fix PrBoom+ demos recorded with -complevel 9. Not
// a solution for MBF -complevel however.
//
// Code copied from PrBoom+, possibly inherited itself from older ports. Specific stuff kept here.
//
static void P_createBlockMapBoom()
{
   //
   // jff 10/6/98
   // New code added to speed up calculation of internal blockmap
   // Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
   //

   static const int blkshift = 7;               /* places to shift rel position for cell num */
   static const int blkmask = (1 << blkshift) - 1; /* mask for rel position within cell */
   // ioanch: blkmargin removed, was 0
   // jff 10/8/98 use guardband>0
   // jff 10/12/98 0 ok with + 1 in rows,cols

   struct linelist_t        // type used to list lines in each block
   {
      int num;
      struct linelist_t *next;
   };

   //
   // Subroutine to add a line number to a block list
   // It simply returns if the line is already in the block
   //
   int NBlocks;                   // number of cells = nrows*ncols

   int xorg, yorg;                 // blockmap origin (lower left)
   int nrows, ncols;               // blockmap dimensions
   linelist_t **blocklists = nullptr;  // array of pointers to lists of lines
   int *blockcount = nullptr;          // array of counters of line lists
   int *blockdone = nullptr;           // array keeping track of blocks/line
   int linetotal = 0;              // total length of all blocklists
   int map_minx = INT_MAX;          // init for map limits search
   int map_miny = INT_MAX;
   int map_maxx = INT_MIN;
   int map_maxy = INT_MIN;

   // scan for map limits, which the blockmap must enclose

   for(int i = 0; i < numvertexes; i++)
   {
      fixed_t t;

      if((t = vertexes[i].x) < map_minx)
         map_minx = t;
      else if(t > map_maxx)
         map_maxx = t;
      if((t = vertexes[i].y) < map_miny)
         map_miny = t;
      else if(t > map_maxy)
         map_maxy = t;
   }
   map_minx >>= FRACBITS;    // work in map coords, not fixed_t
   map_maxx >>= FRACBITS;
   map_miny >>= FRACBITS;
   map_maxy >>= FRACBITS;

   // set up blockmap area to enclose level plus margin

   xorg = map_minx;
   yorg = map_miny;
   ncols = (map_maxx - xorg + 1 + blkmask) >> blkshift;  //jff 10/12/98
   nrows = (map_maxy - yorg + 1 + blkmask) >> blkshift;  //+1 needed for
   NBlocks = ncols * nrows;                                  //map exactly 1 cell

   // create the array of pointers on NBlocks to blocklists
   // also create an array of linelist counts on NBlocks
   // finally make an array in which we can mark blocks done per line

   // CPhipps - calloc's
   blocklists = ecalloc(linelist_t **, NBlocks, sizeof(linelist_t *));
   blockcount = ecalloc(int *, NBlocks, sizeof(int));
   blockdone = emalloc(int *, NBlocks * sizeof(int));

   auto AddBlockLine = [blocklists, blockcount, blockdone](int blockno, int lineno)
   {
      linelist_t *l;

      if(blockdone[blockno])
         return;

      l = emalloc(linelist_t *, sizeof(linelist_t));
      l->num = lineno;
      l->next = blocklists[blockno];
      blocklists[blockno] = l;
      blockcount[blockno]++;
      blockdone[blockno] = 1;
   };

   // initialize each blocklist, and enter the trailing -1 in all blocklists
   // note the linked list of lines grows backwards

   for(int i = 0; i < NBlocks; i++)
   {
      blocklists[i] = emalloc(linelist_t *, sizeof(linelist_t));
      blocklists[i]->num = -1;
      blocklists[i]->next = nullptr;
      blockcount[i]++;
   }

   // For each linedef in the wad, determine all blockmap blocks it touches,
   // and add the linedef number to the blocklists for those blocks

   for(int i = 0; i < numlines; i++)
   {
      int x1 = lines[i].v1->x >> FRACBITS;         // lines[i] map coords
      int y1 = lines[i].v1->y >> FRACBITS;
      int x2 = lines[i].v2->x >> FRACBITS;
      int y2 = lines[i].v2->y >> FRACBITS;
      int dx = x2 - x1;
      int dy = y2 - y1;
      int vert = !dx;                            // lines[i] slopetype
      int horiz = !dy;
      int spos = (dx ^ dy) > 0;
      int sneg = (dx ^ dy) < 0;
      int bx, by;                                 // block cell coords
      int minx = x1 > x2 ? x2 : x1;                 // extremal lines[i] coords
      int maxx = x1 > x2 ? x1 : x2;
      int miny = y1 > y2 ? y2 : y1;
      int maxy = y1 > y2 ? y1 : y2;

      // no blocks done for this linedef yet

      memset(blockdone, 0, NBlocks * sizeof(int));

      // The line always belongs to the blocks containing its endpoints

      bx = (x1 - xorg) >> blkshift;
      by = (y1 - yorg) >> blkshift;
      AddBlockLine(by * ncols + bx, i);
      bx = (x2 - xorg) >> blkshift;
      by = (y2 - yorg) >> blkshift;
      AddBlockLine(by * ncols + bx, i);

      // For each column, see where the line along its left edge, which
      // it contains, intersects the Linedef i. Add i to each corresponding
      // blocklist.

      if(!vert)    // don't interesect vertical lines with columns
      {
         for(int j = 0; j < ncols; j++)
         {
            // intersection of Linedef with x=xorg+(j<<blkshift)
            // (y-y1)*dx = dy*(x-x1)
            // y = dy*(x-x1)+y1*dx;

            int x = xorg + (j << blkshift);       // (x,y) is intersection
            int y = dy * (x - x1) / dx + y1;
            int yb = (y - yorg) >> blkshift;      // block row number
            int yp = (y - yorg) & blkmask;        // y position within block

            if(yb < 0 || yb > nrows - 1)     // outside blockmap, continue
               continue;

            if(x < minx || x > maxx)       // line doesn't touch column
               continue;

            // The cell that contains the intersection point is always added

            AddBlockLine(ncols * yb + j, i);

            // if the intersection is at a corner it depends on the slope
            // (and whether the line extends past the intersection) which
            // blocks are hit

            if(yp == 0)        // intersection at a corner
            {
               if(sneg)       //   \ - blocks x,y-, x-,y
               {
                  if(yb > 0 && miny < y)
                     AddBlockLine(ncols * (yb - 1) + j, i);
                  if(j > 0 && minx < x)
                     AddBlockLine(ncols * yb + j - 1, i);
               }
               else if(spos)  //   / - block x-,y-
               {
                  if(yb > 0 && j > 0 && minx < x)
                     AddBlockLine(ncols * (yb - 1) + j - 1, i);
               }
               else if(horiz) //   - - block x-,y
               {
                  if(j > 0 && minx < x)
                     AddBlockLine(ncols * yb + j - 1, i);
               }
            }
            else if(j > 0 && minx < x) // else not at corner: x-,y
               AddBlockLine(ncols * yb + j - 1, i);
         }
      }

      // For each row, see where the line along its bottom edge, which
      // it contains, intersects the Linedef i. Add i to all the corresponding
      // blocklists.

      if(!horiz)
      {
         for(int j = 0; j < nrows; j++)
         {
            // intersection of Linedef with y=yorg+(j<<blkshift)
            // (x,y) on Linedef i satisfies: (y-y1)*dx = dy*(x-x1)
            // x = dx*(y-y1)/dy+x1;

            int y = yorg + (j << blkshift);       // (x,y) is intersection
            int x = (dx * (y - y1)) / dy + x1;
            int xb = (x - xorg) >> blkshift;      // block column number
            int xp = (x - xorg) & blkmask;        // x position within block

            if (xb < 0 || xb > ncols - 1)   // outside blockmap, continue
               continue;

            if (y < miny || y > maxy)     // line doesn't touch row
               continue;

            // The cell that contains the intersection point is always added

            AddBlockLine(ncols * j + xb, i);

            // if the intersection is at a corner it depends on the slope
            // (and whether the line extends past the intersection) which
            // blocks are hit

            if(xp==0)        // intersection at a corner
            {
               if(sneg)       //   \ - blocks x,y-, x-,y
               {
                  if(j > 0 && miny < y)
                     AddBlockLine(ncols * (j - 1) + xb, i);
                  if (xb > 0 && minx < x)
                     AddBlockLine(ncols * j + xb - 1, i);
               }
               else if(vert)  //   | - block x,y-
               {
                  if(j > 0 && miny < y)
                     AddBlockLine(ncols * (j - 1) + xb, i);
               }
               else if(spos)  //   / - block x-,y-
               {
                  if(xb > 0 && j > 0 && miny < y)
                     AddBlockLine(ncols * (j - 1) + xb - 1, i);
               }
            }
            else if(j > 0 && miny < y) // else not on a corner: x,y-
               AddBlockLine(ncols * (j - 1) + xb, i);
         }
      }
   }

   // Add initial 0 to all blocklists
   // count the total number of lines (and 0's and -1's)

   memset(blockdone, 0, NBlocks * sizeof(int));
   linetotal = 0;
   for(int i = 0; i < NBlocks; i++)
   {
      AddBlockLine(i, 0);
      linetotal += blockcount[i];
   }

   // Create the blockmap lump

   blockmaplump = emalloctag(int *, sizeof(*blockmaplump) * (4 + NBlocks + linetotal), PU_LEVEL,
                             nullptr);
   // blockmap header

   blockmaplump[0] = bmaporgx = xorg << FRACBITS;
   blockmaplump[1] = bmaporgy = yorg << FRACBITS;
   blockmaplump[2] = bmapwidth  = ncols;
   blockmaplump[3] = bmapheight = nrows;

   // offsets to lists and block lists

   for(int i = 0; i < NBlocks; i++)
   {
      linelist_t *bl = blocklists[i];

      int offs = blockmaplump[4 + i] =   // set offset to block's list
      (i ? blockmaplump[4 + i - 1] : 4 + NBlocks) + (i ? blockcount[i - 1] : 0);

      // add the lines in each block's list to the blockmaplump
      // delete each list node as we go

      while(bl)
      {
         linelist_t *tmp = bl->next;
         blockmaplump[offs++] = bl->num;

         efree(bl);
         bl = tmp;
      }
   }
   skipblstart = true;

   // free all temporary storage
   efree(blocklists);
   efree(blockcount);
   efree(blockdone);
}

//
// P_CreateBlockMap
//
// killough 10/98: Rewritten to use faster algorithm.
//
// New procedure uses Bresenham-like algorithm on the linedefs, adding the
// linedef to each block visited from the beginning to the end of the linedef.
//
// The algorithm's complexity is on the order of nlines*total_linedef_length.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.
//
static void P_CreateBlockMap()
{
   unsigned int i;
   fixed_t minx = INT_MAX, miny = INT_MAX,
           maxx = INT_MIN, maxy = INT_MIN;

   C_Printf("P_CreateBlockMap: rebuilding blockmap for level\n");

   if(demo_version >= 200 && demo_version < 203)
      return P_createBlockMapBoom();   // use Boom mode (which is also in PrBoom+)

   // First find limits of map

   // This fixes MBF's code, which has a bug where maxx/maxy
   // are wrong if the 0th node has the largest x or y
   if(demo_version > 401 && numvertexes)
   {
      minx = maxx = vertexes->x >> FRACBITS;
      miny = maxy = vertexes->y >> FRACBITS;
   }

   for(i = 0; i < (unsigned int)numvertexes; i++)
   {
      if((vertexes[i].x >> FRACBITS) < minx)
         minx = vertexes[i].x >> FRACBITS;
      else if((vertexes[i].x >> FRACBITS) > maxx)
         maxx = vertexes[i].x >> FRACBITS;

      if((vertexes[i].y >> FRACBITS) < miny)
         miny = vertexes[i].y >> FRACBITS;
      else if((vertexes[i].y >> FRACBITS) > maxy)
         maxy = vertexes[i].y >> FRACBITS;
   }

   // Save blockmap parameters
   
   bmaporgx   = minx << FRACBITS;
   bmaporgy   = miny << FRACBITS;
   bmapwidth  = ((maxx - minx) >> MAPBTOFRAC) + 1;
   bmapheight = ((maxy - miny) >> MAPBTOFRAC) + 1;

   // Compute blockmap, which is stored as a 2d array of variable-sized 
   // lists.
   //
   // Pseudocode:
   //
   // For each linedef:
   //
   //   Map the starting and ending vertices to blocks.
   //
   //   Starting in the starting vertex's block, do:
   //
   //     Add linedef to current block's list, dynamically resizing it.
   //
   //     If current block is the same as the ending vertex's block,
   //     exit loop.
   //
   //     Move to an adjacent block by moving towards the ending block
   //     in either the x or y direction, to the block which contains 
   //     the linedef.

   {
      unsigned tot = bmapwidth * bmapheight;                  // size of blockmap
      bmap_t *bmap = ecalloc(bmap_t *, sizeof *bmap, tot);    // array of blocklists

      for(i = 0; i < (unsigned int)numlines; i++)
      {
         // starting coordinates
         int x = (lines[i].v1->x >> FRACBITS) - minx;
         int y = (lines[i].v1->y >> FRACBITS) - miny;
         
         // x-y deltas
         int adx = lines[i].dx >> FRACBITS, dx = adx < 0 ? -1 : 1;
         int ady = lines[i].dy >> FRACBITS, dy = ady < 0 ? -1 : 1; 

         // difference in preferring to move across y (>0) 
         // instead of x (<0)
         int diff = !adx ? 1 : !ady ? -1 :
          (((x >> MAPBTOFRAC) << MAPBTOFRAC) + 
           (dx > 0 ? MAPBLOCKUNITS-1 : 0) - x) * (ady = D_abs(ady)) * dx -
          (((y >> MAPBTOFRAC) << MAPBTOFRAC) + 
           (dy > 0 ? MAPBLOCKUNITS-1 : 0) - y) * (adx = D_abs(adx)) * dy;

         // starting block, and pointer to its blocklist structure
         int b = (y >> MAPBTOFRAC) * bmapwidth + (x >> MAPBTOFRAC);

         // ending block
         int bend = (((lines[i].v2->y >> FRACBITS) - miny) >> MAPBTOFRAC) *
            bmapwidth + (((lines[i].v2->x >> FRACBITS) - minx) >> MAPBTOFRAC);

         // delta for pointer when moving across y
         dy *= bmapwidth;

         // deltas for diff inside the loop
         adx <<= MAPBTOFRAC;
         ady <<= MAPBTOFRAC;

         // Now we simply iterate block-by-block until we reach the end block.
         while((unsigned int) b < tot)    // failsafe -- should ALWAYS be true
         {
            // Increase size of allocated list if necessary
            if(bmap[b].n >= bmap[b].nalloc)
            {
               bmap[b].nalloc = bmap[b].nalloc ? bmap[b].nalloc * 2 : 8;
               bmap[b].list = erealloc(int *, bmap[b].list,
                                       bmap[b].nalloc*sizeof*bmap->list);
            }

            // Add linedef to end of list
            bmap[b].list[bmap[b].n++] = i;

            // If we have reached the last block, exit
            if(b == bend)
               break;

            // Move in either the x or y direction to the next block
            if(diff < 0)
            {
               diff += ady;
               b += dx;
            }
            else
            {
               diff -= adx;
               b += dy;
            }
         }
      }

      // Compute the total size of the blockmap.
      //
      // Compression of empty blocks is performed by reserving two 
      // offset words at tot and tot+1.
      //
      // 4 words, unused if this routine is called, are reserved at 
      // the start.

      {
         // we need at least 1 word per block, plus reserved's
         int count = tot + 6;

         for(i = 0; i < tot; i++)
         {
            // 1 header word + 1 trailer word + blocklist
            if(bmap[i].n)
               count += bmap[i].n + 2; 
         }

         // Allocate blockmap lump with computed count
         blockmaplump = emalloctag(int *, sizeof(*blockmaplump) * count,  PU_LEVEL, nullptr);
      }

      // Now compress the blockmap.
      {
         int ndx = tot += 4;      // Advance index to start of linedef lists
         bmap_t *bp = bmap;       // Start of uncompressed blockmap

         blockmaplump[ndx++] = 0;  // Store an empty blockmap list at start
         blockmaplump[ndx++] = -1; // (Used for compression)

         for(i = 4; i < tot; i++, bp++)
         {
            if(bp->n)                          // Non-empty blocklist
            {
               blockmaplump[blockmaplump[i] = ndx++] = 0;  // Store index & header
               do
                  blockmaplump[ndx++] = bp->list[--bp->n]; // Copy linedef list
               while (bp->n);
               blockmaplump[ndx++] = -1;                   // Store trailer
               efree(bp->list);                             // Free linedef list
            }
            else     // Empty blocklist: point to reserved empty blocklist
               blockmaplump[i] = tot;
         }
         
         efree(bmap);    // Free uncompressed blockmap
      }
   }

   skipblstart = true;
}

static const char *bmaperrormsg;

//
// P_VerifyBlockMap
//
// haleyjd 03/04/10: do verification on validity of blockmap.
//
static bool P_VerifyBlockMap(int count)
{
   bool isvalid = true;
   int x, y;
   int *maxoffs = blockmaplump + count;

   bmaperrormsg = nullptr;

   skipblstart = true;

   for(y = 0; y < bmapheight; y++)
   {
      for(x = 0; x < bmapwidth; x++)
      {
         int offset;
         int *list, *tmplist;
         int *blockoffset;

         offset = y * bmapwidth + x;
         blockoffset = blockmaplump + offset + 4;
         
         // check that block offset is in bounds
         if(blockoffset >= maxoffs)
         {
            isvalid = false;
            bmaperrormsg = "offset overflow";
            break;
         }
         
         offset = *blockoffset;         
         list   = blockmaplump + offset;

         if(*list != 0)
            skipblstart = false;

         // scan forward for a -1 terminator before maxoffs
         for(tmplist = list; ; tmplist++)
         {
            // we have overflowed the lump?
            if(tmplist >= maxoffs)
            {
               isvalid = false;
               bmaperrormsg = "open blocklist";
               break;
            }
            if(*tmplist == -1) // found -1
               break;
         }
         if(!isvalid) // if the list is not terminated, break now
            break;

         // scan the list for out-of-range linedef indicies in list
         for(tmplist = list; *tmplist != -1; tmplist++)
         {
            if(*tmplist < 0 || *tmplist >= numlines)
            {
               isvalid = false;
               bmaperrormsg = "index >= numlines";
               break;
            }
         }
         if(!isvalid) // if a list has a bad linedef index, break now
            break;
      }

      // break out early on any error
      if(!isvalid)
         break;
   }

   return isvalid;
}

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//
static void P_LoadBlockMap(int lump)
{
   // IOANCH 20151215: no lump means no data. So that Eternity will generate.
   int len   = lump >= 0 ? setupwad->lumpLength(lump) : 0;
   int count = len / 2;
   
   // sf: -blockmap checkparm made into variable
   // also checking for levels without blockmaps (0 length)
   // haleyjd 03/04/10: blockmaps of less than 8 bytes cannot be valid
   if(r_blockmap || len < 8 || count >= 0x10000)
   {
      P_CreateBlockMap();
   }
   else
   {
      int i;
      int16_t *wadblockmaplump = (int16_t *)(setupwad->cacheLumpNum(lump, PU_LEVEL));
      blockmaplump = emalloctag(int *, sizeof(*blockmaplump) * count, PU_LEVEL, nullptr);

      // killough 3/1/98: Expand wad blockmap into larger internal one,
      // by treating all offsets except -1 as unsigned and zero-extending
      // them. This potentially doubles the size of blockmaps allowed,
      // because Doom originally considered the offsets as always signed.

      blockmaplump[0] = SwapShort(wadblockmaplump[0]);
      blockmaplump[1] = SwapShort(wadblockmaplump[1]);
      blockmaplump[2] = (int)(SwapShort(wadblockmaplump[2])) & 0xffff;
      blockmaplump[3] = (int)(SwapShort(wadblockmaplump[3])) & 0xffff;

      for(i = 4; i < count; i++)
      {
         int16_t t = SwapShort(wadblockmaplump[i]);          // killough 3/1/98
         blockmaplump[i] = t == -1 ? -1l : (int) t & 0xffff;
      }

      Z_Free(wadblockmaplump);

      bmaporgx   = blockmaplump[0] << FRACBITS;
      bmaporgy   = blockmaplump[1] << FRACBITS;
      bmapwidth  = blockmaplump[2];
      bmapheight = blockmaplump[3];

      // haleyjd 03/04/10: check for blockmap problems
      if(!(demo_compatibility || P_VerifyBlockMap(count)))
      {
         C_Printf(FC_ERROR "Blockmap error: %s\a\n", bmaperrormsg);
         Z_Free(blockmaplump);
         blockmaplump = nullptr;
         P_CreateBlockMap();
      }
   }

   // clear out mobj chains
   count      = sizeof(*blocklinks) * bmapwidth * bmapheight;
   blocklinks = ecalloctag(Mobj **, 1, count, PU_LEVEL, nullptr);
   blockmap   = blockmaplump + 4;

   // haleyjd 2/22/06: setup polyobject blockmap
   count = sizeof(*polyblocklinks) * bmapwidth * bmapheight;
   polyblocklinks = ecalloctag(DLListItem<polymaplink_t> **, 1, count, PU_LEVEL, nullptr);

   // haleyjd 05/17/13: setup portalmap
   count = sizeof(*portalmap) * bmapwidth * bmapheight;
   portalmap = ecalloctag(byte *, 1, count, PU_LEVEL, nullptr);
}


//
// AddLineToSector
//
// Utility function for P_GroupLines below.
//
static void AddLineToSector(sector_t *s, line_t *l)
{
   M_AddToBox(s->blockbox, l->v1->x, l->v1->y);
   M_AddToBox(s->blockbox, l->v2->x, l->v2->y);
   *s->lines++ = l;
}

//
// P_GroupLines
//
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
// killough 5/3/98: reformatted, cleaned up
// killough 8/24/98: rewrote to use faster algorithm
//
static void P_GroupLines()
{
   int i, total;
   line_t **linebuffer;

   // look up sector number for each subsector
   for(i = 0; i < numsubsectors; i++)
      subsectors[i].sector = segs[subsectors[i].firstline].sidedef->sector;

   // count number of lines in each sector
   for(i = 0; i < numlines; i++)
   {
      lines[i].frontsector->linecount++;
      if(lines[i].backsector && 
         lines[i].backsector != lines[i].frontsector)
      {
         lines[i].backsector->linecount++;
      }
   }

   // compute total number of lines and clear bounding boxes
   for(total = 0, i = 0; i < numsectors; i++)
   {
      total += sectors[i].linecount;
      M_ClearBox(sectors[i].blockbox);
   }

   // ioanch 20160309: remember this value for REJECT overflow
   gTotalLinesForRejectOverflow = total;

   // build line tables for each sector
   linebuffer = emalloctag(line_t **, total * sizeof(*linebuffer), PU_LEVEL, nullptr);

   for(i = 0; i < numsectors; i++)
   {
      sectors[i].lines = linebuffer;
      linebuffer += sectors[i].linecount;
   }
  
   for(i = 0; i < numlines; i++)
   {
      AddLineToSector(lines[i].frontsector, &lines[i]);
      if(lines[i].backsector && 
         lines[i].backsector != lines[i].frontsector)
      {
         AddLineToSector(lines[i].backsector, &lines[i]);
      }
   }

   for(i = 0; i < numsectors; i++)
   {
      sector_t *sector = sectors+i;
      int block;

      // adjust pointers to point back to the beginning of each list
      sector->lines -= sector->linecount;
      
      // haleyjd 04/28/10: divide before add to lessen chance of overflow;
      //    this only affects sounds, so there is no compatibility check.
      //    EE does not bother with presentation compatibility for sounds
      //    except where playability is a concern (ie. in deathmatch).

      // set the degenMobj to the middle of the bounding box
      sector->soundorg.x = sector->blockbox[BOXRIGHT ] / 2 + 
                           sector->blockbox[BOXLEFT  ] / 2;
      sector->soundorg.y = sector->blockbox[BOXTOP   ] / 2 + 
                           sector->blockbox[BOXBOTTOM] / 2;
      // SoM: same for group id.
      // haleyjd: note - groups have not been built yet, so this is just for
      // initialization.
      sector->soundorg.groupid = sector->groupid;

      // haleyjd 10/16/06: copy all properties to ceiling origin
      sector->csoundorg = sector->soundorg;

      // adjust bounding box to map blocks
      block = (sector->blockbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block >= bmapheight ? bmapheight-1 : block;
      sector->blockbox[BOXTOP]=block;

      block = (sector->blockbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block < 0 ? 0 : block;
      sector->blockbox[BOXBOTTOM]=block;

      block = (sector->blockbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block >= bmapwidth ? bmapwidth-1 : block;
      sector->blockbox[BOXRIGHT]=block;

      block = (sector->blockbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block < 0 ? 0 : block;
      sector->blockbox[BOXLEFT]=block;
   }
}

//
// P_RemoveSlimeTrails
//
// killough 10/98: Remove slime trails.
//
// Slime trails are inherent to Doom's coordinate system -- i.e. there is
// nothing that a node builder can do to prevent slime trails ALL of the time,
// because it's a product of the integer coordinate system, and just because
// two lines pass through exact integer coordinates, doesn't necessarily mean
// that they will intersect at integer coordinates. Thus we must allow for
// fractional coordinates if we are to be able to split segs with node lines,
// as a node builder must do when creating a BSP tree.
//
// A wad file does not allow fractional coordinates, so node builders are out
// of luck except that they can try to limit the number of splits (they might
// also be able to detect the degree of roundoff error and try to avoid splits
// with a high degree of roundoff error). But we can use fractional coordinates
// here, inside the engine. It's like the difference between square inches and
// square miles, in terms of granularity.
//
// For each vertex of every seg, check to see whether it's also a vertex of
// the linedef associated with the seg (i.e, it's an endpoint). If it's not
// an endpoint, and it wasn't already moved, move the vertex towards the
// linedef by projecting it using the law of cosines. Formula:
//
//      2        2                         2        2
//    dx  x0 + dy  x1 + dx dy (y0 - y1)  dy  y0 + dx  y1 + dx dy (x0 - x1)
//   {---------------------------------, ---------------------------------}
//                  2     2                            2     2
//                dx  + dy                           dx  + dy
//
// (x0,y0) is the vertex being moved, and (x1,y1)-(x1+dx,y1+dy) is the
// reference linedef.
//
// Segs corresponding to orthogonal linedefs (exactly vertical or horizontal
// linedefs), which comprise at least half of all linedefs in most wads, don't
// need to be considered, because they almost never contribute to slime trails
// (because then any roundoff error is parallel to the linedef, which doesn't
// cause slime). Skipping simple orthogonal lines lets the code finish quicker.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.
//
// Firelines (TM) is a Rezistered Trademark of MBF Productions
//
// ioanch 20190222: rewritten to lighten up the tabs and use floating-point.
//
static void P_RemoveSlimeTrails()             // killough 10/98
{
   byte *hit; 
   int i;
   
   // haleyjd: don't mess with vertices in old demos, for safety.
   if(demo_version < 203)
      return;

   hit = ecalloc(byte *, 1, numvertexes); // Hitlist for vertices

   for(i = 0; i < numsegs; i++)            // Go through each seg
   {
      const line_t *l = segs[i].linedef;   // The parent linedef
      if(!l->dx || !l->dy)                 // We can ignore orthogonal lines
         continue;
      vertex_t *v = segs[i].v1;

      do
      {
         if(hit[v - vertexes])   // If we haven't processed vertex
            continue;
         hit[v - vertexes] = 1;        // Mark this vertex as processed
         if(v == l->v1 || v == l->v2)  // Exclude endpoints of linedefs
            continue;

         // Project the vertex back onto the parent linedef
         // ioanch: just use doubles
         double dx2 = pow(M_FixedToDouble(l->dx), 2);
         double dy2 = pow(M_FixedToDouble(l->dy), 2);
         double dxy = M_FixedToDouble(l->dx) * M_FixedToDouble(l->dy);
         double s = dx2 + dy2;
         float x0 = v->fx;
         float y0 = v->fy;
         float x1 = l->v1->fx;
         float y1 = l->v1->fy;
         v->fx = static_cast<float>((dx2 * x0 + dy2 * x1 + dxy * (y0 - y1)) / s);
         v->fy = static_cast<float>((dy2 * y0 + dx2 * y1 + dxy * (x0 - x1)) / s);

         // ioanch: add linguortal support, from PrBoom+/[crispy]
         // demo_version check needed, for similar reasons as above
         static const double threshold = M_FixedToDouble(LINGUORTAL_THRESHOLD);
         if(demo_version >= 342 && (fabs(x0 - v->fx) > threshold || fabs(y0 - v->fy) > threshold))
         {
            v->fx = x0;  // reset
            v->fy = y0;
         }
         else
         {
            // If accepted, update the fixed coordinates too
            v->x = M_FloatToFixed(v->fx);
            v->y = M_FloatToFixed(v->fy);
         }

      } // Obfuscated C contest entry:   :)
      while((v != segs[i].v2) && (v = segs[i].v2));
   }

   // free hit list
   efree(hit);
}

//
// P_padRejectArray
//
// ioanch 20160309: directly from Chocolate-Doom (by fraggle)
// https://github.com/chocolate-doom/chocolate-doom/blob/master/src/doom/p_setup.c
//
// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.
//
static void P_padRejectArray(byte *array, unsigned int len)
{
   unsigned int i;
   unsigned int byte_num;
   byte *dest;

   // Values to pad the REJECT array with:

   uint32_t rejectpad[4] =
   {
      static_cast<uint32_t>(((gTotalLinesForRejectOverflow * 4 + 3) & ~3)
                                + 24),      // Size
      0,                                    // Part of z_zone block header
      50,                                   // PU_LEVEL
      0x1d4a11                              // DOOM_CONST_ZONEID
   };

   // Copy values from rejectpad into the destination array.

   dest = array;

   for(i = 0; i < len && i < sizeof(rejectpad); ++i)
   {
      byte_num = i % 4;
      *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
      ++dest;
   }

   // We only have a limited pad size.  Print a warning if the
   // REJECT lump is too small.

   if(len > sizeof(rejectpad))
   {
      C_Printf("PadRejectArray: REJECT lump too short to pad! (%i > %i)\n",
               len, (int) sizeof(rejectpad));
   }
}

//
// P_LoadReject
//
// haleyjd 01/26/04: Although DOOM accepted them due to differing
// Z_Malloc behavior, Eternity was crashing on levels with zero-
// length reject lumps. This function will test to see if the reject
// lump is zero in size, and if so, will generate a reject with all
// zeroes. This is preferable to adding checks to see if a reject
// matrix exists, in my opinion. This could be improved by actually
// generating a meaningful reject, but that will have to wait.
//
static void P_LoadReject(int lump)
{
   int size;
   int expectedsize;

   // IOANCH 20151215: support missing lump
   size = lump >= 0 ? setupwad->lumpLength(lump) : 0;

   // haleyjd: round numsectors^2 to next higher multiple of 8, then divide by
   // 8 to get the expected reject size for this level
   expectedsize = (((numsectors * numsectors) + 7) & ~7) / 8;

   // 12/04/08: be super-smart about REJECT lumps:
   // 1. if size >= expectedsize, all is good.
   // 2. if size <  expectedsize, allocate a zero-filled buffer and copy
   //    in whatever exists from the actual lump.
   if(size >= expectedsize)
      rejectmatrix = (byte *)(setupwad->cacheLumpNum(lump, PU_LEVEL));
   else
   {
      // set to all zeroes so that the reject has no effect
      rejectmatrix = ecalloctag(byte *, 1, expectedsize, PU_LEVEL, nullptr);

      // Pad remaining space with 0xff if specified on command line)
      if(M_CheckParm("-reject_pad_with_ff"))
         memset(rejectmatrix, 0xff, expectedsize);

      if(size > 0)
      {
         byte *temp = (byte *)(setupwad->cacheLumpNum(lump, PU_CACHE));
         memcpy(rejectmatrix, temp, size);
      }
      // ioanch 20160309: REJECT overflow fix. From chocolate-doom (by
      // fraggle):
      // Also only do it if MBF or less
      if(demo_version <= 203)
         P_padRejectArray(rejectmatrix + size, expectedsize - size);
   }

   // warn on too-large rejects, but do nothing special.
   if(size > expectedsize)
      C_Printf(FC_ERROR "P_LoadReject: warning - reject is too large\a\n");
}

//
// Map lumps table
//
// For Doom- and Hexen-format maps.
//
static const char *levellumps[] =
{
   "label",    // ML_LABEL,    A separator, name, ExMx or MAPxx
   "THINGS",   // ML_THINGS,   Monsters, items..
   "LINEDEFS", // ML_LINEDEFS, LineDefs, from editing
   "SIDEDEFS", // ML_SIDEDEFS, SideDefs, from editing
   "VERTEXES", // ML_VERTEXES, Vertices, edited and BSP splits generated
   "SEGS",     // ML_SEGS,     LineSegs, from LineDefs split by BSP
   "SSECTORS", // ML_SSECTORS, SubSectors, list of LineSegs
   "NODES",    // ML_NODES,    BSP nodes
   "SECTORS",  // ML_SECTORS,  Sectors, from editing
   "REJECT",   // ML_REJECT,   LUT, sector-sector visibility
   "BLOCKMAP", // ML_BLOCKMAP  LUT, motion clipping, walls/grid element
   "BEHAVIOR"  // ML_BEHAVIOR  haleyjd: ACS bytecode; used to id hexen maps
};

//
// Lumps that are used in console map formats
//
static const char *consolelumps[] =
{
   "LEAFS",
   "LIGHTS",
   "MACROS"
};

//
// P_checkConsoleFormat
//
// haleyjd 12/12/13: Check for supported console map formats
//
static int P_checkConsoleFormat(const WadDirectory *dir, int lumpnum)
{
   int          numlumps = dir->getNumLumps();
   lumpinfo_t **lumpinfo = dir->getLumpInfo();

   for(int i = ML_LEAFS; i <= ML_MACROS; i++)
   {
      int ln = lumpnum + i;
      if(ln >= numlumps ||     // past the last lump?
         strncmp(lumpinfo[ln]->name, consolelumps[i - ML_LEAFS], 8))
      {
         if(i == ML_LIGHTS)
            return LEVEL_FORMAT_PSX; // PSX
         else
            return LEVEL_FORMAT_INVALID; // invalid map
      }
   }

   // If we got here, dealing with Doom 64 format. (TODO: Not supported ... yet?)
   return LEVEL_FORMAT_INVALID; //LEVEL_FORMAT_DOOM64;
}

//
// P_CheckLevel
//
// sf 11/9/99: We need to do this now because we no longer have to conform to
// the MAPxy or ExMy standard previously imposed.
// IOANCH 20151213: added MGLA, optional parameter to assign values to.
//
int P_CheckLevel(const WadDirectory *dir, int lumpnum, maplumpindex_t *mgla, bool *udmf)
{
   int          numlumps = dir->getNumLumps();
   lumpinfo_t **lumpinfo = dir->getLumpInfo();

   if(mgla) // reset it to negative (missing) values
      memset(mgla, -1, sizeof(*mgla));
   if(udmf)
      *udmf = false;

   // IOANCH 20151206: check for UDMF lumps structure
   if(lumpnum + 1 < numlumps && !strncmp(lumpinfo[lumpnum + 1]->name, "TEXTMAP", 8))
   {
      // found a TEXTMAP. Look for ENDMAP
      bool foundEndMap = false;
      const char *lname;

      for(int i = lumpnum + 2; i < numlumps; ++i)
      {
         lname = lumpinfo[i]->name;
         if(mgla)
         {
            if(!strncmp(lname, "ZNODES", 8))
               mgla->nodes = i;
            else if(!strncmp(lname, levellumps[ML_REJECT], 8))
               mgla->reject = i;
            else if(!strncmp(lname, levellumps[ML_BLOCKMAP], 8))
               mgla->blockmap = i;
            else if(!strncmp(lname, levellumps[ML_BEHAVIOR], 8))
               mgla->behavior = i;
         }

         if(!strncmp(lname, "ENDMAP", 8))
         {
            foundEndMap = true;
            break;
         }
      }
      if(!foundEndMap || (mgla && mgla->nodes < 0))
         return LEVEL_FORMAT_INVALID;  // must have ENDMAP
      // Found ENDMAP. This may be a valid UDMF lump. Return it
      if(udmf)
         *udmf = true;
      return LEVEL_FORMAT_DOOM;  // consider "Doom" format even for UDMF.
   }
   
   for(int i = ML_THINGS; i <= ML_BEHAVIOR; i++)
   {
      int ln = lumpnum + i;
      if(ln >= numlumps || strncmp(lumpinfo[ln]->name, levellumps[i], 8))     // past the last lump?
      {
         // If "BEHAVIOR" wasn't found, we assume we are dealing with
         // a DOOM-format map, and it is not an error; any other missing
         // lump means the map is invalid.

         if(i == ML_BEHAVIOR)
         {
            // If the current lump is named LEAFS, it's a console map
            if(ln < numlumps && !strncmp(lumpinfo[ln]->name, "LEAFS", 8))
               return P_checkConsoleFormat(dir, lumpnum);
            else
               return LEVEL_FORMAT_DOOM;
         }
         else
            return LEVEL_FORMAT_INVALID;
      }
      else if(mgla)
      {
         // IOANCH 20151213: respect MGLA even if it's not UDMF
         switch(i)
         {
         case ML_SEGS:
            mgla->segs = ln;
            break;
         case ML_SSECTORS:
            mgla->ssectors = ln;
            break;
         case ML_NODES:
            mgla->nodes = ln;
            break;
         case ML_REJECT:
            mgla->reject = ln;
            break;
         case ML_BLOCKMAP:
            mgla->blockmap = ln;
            break;
         case ML_BEHAVIOR:
            mgla->behavior = ln;
            break;
         }
      }
   }

   // if we got here, we're dealing with a Hexen-format map
   return LEVEL_FORMAT_HEXEN;
}

void P_InitThingLists(); // haleyjd

//=============================================================================
//
// P_SetupLevel - Main Level Setup Routines
//

// P_SetupLevel subroutines

//
// P_SetupLevelError
// 
// haleyjd 02/18/10: Error handling routine for P_SetupLevel
// IOANCH 20151210: Made global
//
void P_SetupLevelError(const char *msg, const char *levelname)
{
   C_Printf(FC_ERROR "%s: '%s'\n", msg, levelname);
   C_SetConsole();
}

//
// P_NewLevelMsg
//
// Called when loading a new map.
// haleyjd 06/04/05: moved here and renamed from HU_NewLevel
//
static void P_NewLevelMsg()
{   
   C_Printf("\n");
   C_Separator();
   C_Printf(FC_GRAY "  %s\n\n", LevelInfo.levelName);
   C_InstaPopup();       // put console away
}

//
// P_ClearPlayerVars
//
// haleyjd 2/18/10: clears various player-related data.
//
static void P_ClearPlayerVars()
{
   for(int i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i] && players[i].playerstate == PST_DEAD)
         players[i].playerstate = PST_REBORN;

      players[i].killcount = players[i].secretcount = players[i].itemcount = 0;

      memset(players[i].frags, 0, sizeof(players[i].frags));

      // haleyjd: explicitly nullify old player object pointers
      players[i].mo = nullptr;

      // haleyjd: explicitly nullify old player attacker
      players[i].attacker = nullptr;
   }

   totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
   wminfo.partime = 180;

   // Initial height of PointOfView will be set by player think.
   players[consoleplayer].viewz = players[consoleplayer].prevviewz = 1;
}

//
// P_PreZoneFreeLevel
//
// haleyjd 2/18/10: actions that must be performed immediately prior to 
// Z_FreeTags should be kept here.
//
static void P_PreZoneFreeLevel()
{
   //==============================================
   // Clear player data

   P_ClearPlayerVars();

   //==============================================
   // Scripting

#if 0
   // haleyjd 03/15/03: clear levelscript callbacks
   SM_RemoveCallbacks(SC_VM_LEVELSCRIPT);
#endif

   // haleyjd 01/07/07: reset ACS interpreter state
   ACS_InitLevel();

   //==============================================
   // Sound Engine

   // haleyjd 06/06/06: stop sound sequences
   S_StopAllSequences();
   
   // haleyjd 10/19/06: stop looped sounds
   S_StopLoopedSounds();

   // Make sure all sourced sounds are stopped
   S_StopSounds(false);

   //==============================================
   // Renderer

   // haleyjd: stop particle engine
   R_ClearParticles();

   // SoM: initialize portals
   R_InitPortals();

   // haleyjd 05/16/08: clear dynamic segs
   R_ClearDynaSegs();

   //==============================================
   // Playsim

   // haleyjd 02/16/10: clear followcam pointers
   P_FollowCamOff();

   // sf: free the psecnode_t linked list in p_map.c
   P_FreeSecNodeList(); 

   // Clear all global references
   P_ClearGlobalLevelReferences();
}

//
// P_InitNewLevel
//
// Performs (re)initialization of subsystems after Z_FreeTags.
//
// IF THIS RETURNS false, IT MUST HAVE CALLED P_SetupLevelError.
//
static bool P_InitNewLevel(int lumpnum, WadDirectory *waddir)
{
   //==============================================
   // Playsim

   // re-initialize thinker list
   Thinker::InitThinkers();   
   
   // haleyjd 02/02/04 -- clear the TID hash table
   P_InitTIDHash();     

   // SoM: I can't believe I forgot to call this!
   P_InitPortals(); 

   //==============================================
   // Map data scripts

   // load MapInfo
   qstring error;
   if(!P_LoadLevelInfo(waddir, lumpnum, W_GetManagedDirFN(waddir), &error))
   {
      P_SetupLevelError(error.constPtr(), gamemapname);
      return false;
   }
   
   // haleyjd 10/08/03: load ExtraData
   E_LoadExtraData();         

   //==============================================
   // Renderer

   // load the sky
   R_StartSky();

   // haleyjd: set global colormap -- see r_data.c
   R_SetGlobalLevelColormap();
   
   //==============================================
   // Wake up subsystems

   // haleyjd 01/21/14: clear marks here along with everything else
   AM_clearMarks();

   // wake up heads-up display
   HU_Start();
   
   // must be after P_LoadLevelInfo as the music lump name is gotten there
   S_Start();
   
   // console message
   P_NewLevelMsg();

   //==============================================
   // MUSINFO
   S_MusInfoClear();

   return true;
}

//
// P_DeathMatchSpawnPlayers
//
// If deathmatch, randomly spawn the active players
//
static void P_DeathMatchSpawnPlayers()
{
   if(GameType == gt_dm)
   {
      for(int i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i])
         {
            players[i].mo = nullptr;
            G_DeathMatchSpawnPlayer(i);
         }
      }
   }
}

//
// P_InitThingLists
//
// haleyjd 11/19/02: Sets up all dynamically allocated thing lists.
//
void P_InitThingLists()
{
   // haleyjd: allow to work in any game mode
   // killough 3/26/98: Spawn icon landings:
   if(GameModeInfo->id == commercial || demo_version >= 331)
      P_SpawnBrainTargets();

   // haleyjd: spawn D'Sparil teleport spots
   P_SpawnSorcSpots();

   // haleyjd 08/15/13: spawn EDF-specified collections
   MobjCollections.collectAllThings();

   // haleyjd 04/08/03: spawn camera spots
   IN_AddCameras();

   // haleyjd 06/06/06: initialize environmental sequences
   S_InitEnviroSpots();
}

//
// Computes the compatibility hash. Use the same content as in GZDoom:
// github.com/coelckers/gzdoom: src/p_openmap.cpp#MapData::GetChecksum
//
static void P_resolveCompatibilities(const WadDirectory &dir, int lumpnum, bool isUdmf,
                                     int behaviorIndex)
{
   p_currentLevelHashDigest.clear();
   E_RestoreCompatibilities();
   HashData md5(HashData::MD5);
   ZAutoBuffer buf;

   if(isUdmf)
   {
      dir.cacheLumpAuto(lumpnum + 1, buf); // TEXTMAP
      md5.addData(buf.getAs<const uint8_t *>(), static_cast<uint32_t>(buf.getSize()));
   }
   else
   {

      dir.cacheLumpAuto(lumpnum, buf); // level marker
      md5.addData(buf.getAs<const uint8_t*>(), static_cast<uint32_t>(buf.getSize()));

      dir.cacheLumpAuto(lumpnum + ML_THINGS, buf);
      md5.addData(buf.getAs<const uint8_t*>(), static_cast<uint32_t>(buf.getSize()));

      dir.cacheLumpAuto(lumpnum + ML_LINEDEFS, buf);
      md5.addData(buf.getAs<const uint8_t*>(), static_cast<uint32_t>(buf.getSize()));

      dir.cacheLumpAuto(lumpnum + ML_SIDEDEFS, buf);
      md5.addData(buf.getAs<const uint8_t*>(), static_cast<uint32_t>(buf.getSize()));

      dir.cacheLumpAuto(lumpnum + ML_SECTORS, buf);
      md5.addData(buf.getAs<const uint8_t*>(), static_cast<uint32_t>(buf.getSize()));
   }

   if(behaviorIndex != -1)
   {
      dir.cacheLumpAuto(behaviorIndex, buf);
      md5.addData(buf.getAs<const uint8_t*>(), static_cast<uint32_t>(buf.getSize()));
   }

   md5.wrapUp();

   char *digest = md5.digestToString();
   E_ApplyCompatibility(digest);
   p_currentLevelHashDigest = digest;
   efree(digest);
}

//
// Console command to get the currently obtained MD5 checksum (if available)
//
CONSOLE_COMMAND(mapchecksum, 0)
{
   if(p_currentLevelHashDigest.empty())
      C_Puts("Current map MD5 checksum not available.");
   else
      C_Printf("Current map MD5: %s\n", p_currentLevelHashDigest.constPtr());
}

//
// CHECK_ERROR
//
// Checks to see if level_error has been set to a valid error message.
// If so, P_SetupLevelError is called with the message and P_SetupLevel
// will return. The game will enter GS_CONSOLE state.
//
#define CHECK_ERROR() \
   do { \
      if(level_error) \
      { \
         P_SetupLevelError(level_error, mapname); \
         return; \
      } \
   } while(0)

//
// P_SetupLevel
//
// killough 5/3/98: reformatted, cleaned up
//
void P_SetupLevel(WadDirectory *dir, const char *mapname, int playermask,
                  skill_t skill)
{
   lumpinfo_t **lumpinfo;
   int lumpnum, acslumpnum = -1;

   G_DemoLog("%d\tSetup %s\n", gametic, mapname);
   G_DemoLogSetExited(false);

   // haleyjd 07/28/10: we are no longer in GS_LEVEL during the execution of
   // this routine.
   gamestate = GS_LOADING;

   // haleyjd 06/14/10: support loading levels from private wad directories
   setupwad = dir;
   lumpinfo = setupwad->getLumpInfo();
   
   // get the map name lump number
   if((lumpnum = setupwad->checkNumForName(mapname)) == -1)
   {
      P_SetupLevelError("Map not found", mapname);
      return;
   }

   // IOANCH 20151213: udmf
   maplumpindex_t mgla;  // will be set by P_CheckLevel
   bool isUdmf = false;

   // determine map format; if invalid, abort
   if((LevelInfo.mapFormat = P_CheckLevel(setupwad, lumpnum, &mgla, &isUdmf)) 
      == LEVEL_FORMAT_INVALID)
   {
      P_SetupLevelError("Not a valid level", mapname);
      return;
   }

   P_resolveCompatibilities(*setupwad, lumpnum, isUdmf, mgla.behavior);

   if(isUdmf || demo_version >= 401)
   {
      P_PointOnLineSide = P_PointOnLineSidePrecise;
      P_PointOnDivlineSide = P_PointOnDivlineSidePrecise;
   }
   else
   {
      P_PointOnLineSide = P_PointOnLineSideClassic;
      P_PointOnDivlineSide = P_PointOnDivlineSideClassic;
   }

   // haleyjd 07/22/04: moved up
   newlevel   = (lumpinfo[lumpnum]->source != WadDirectory::IWADSource);

   strncpy(levelmapname, mapname, 8);
   leveltime = 0;

   // perform pre-Z_FreeTags actions
   P_PreZoneFreeLevel();
   
   // free the old level
   Z_FreeTags(PU_LEVEL, PU_LEVEL);

   // perform post-Z_FreeTags actions
   if(!P_InitNewLevel(lumpnum, dir))
      return;  // The error was thrown by P_InitNewLevel if it got false.

   // note: most of this ordering is important
   
   // killough 3/1/98: P_LoadBlockMap call moved down to below
   // killough 4/4/98: split load of sidedefs into two parts,
   // to allow texture names to be used in special linedefs

   level_error = nullptr; // reset

   ScrollThinker::RemoveAllScrollers();

   // IOANCH 20151206: load UDMF
   UDMFParser udmf;  // prepare UDMF processor
   UDMFSetupSettings setupSettings;
   if(isUdmf)
   {
      if(!udmf.parse(*setupwad, lumpnum + 1))
      {
         P_SetupLevelError(udmf.error().constPtr(), mapname);
         return;
      }

      //
      // Update map format
      //
      if((LevelInfo.mapFormat = udmf.getMapFormat()) == LEVEL_FORMAT_INVALID)
      {
         P_SetupLevelError("Unsupported UDMF namespace", mapname);
         return;
      }

      // start UDMF loading
      udmf.loadVertices();
      udmf.loadSectors(setupSettings);
   }
   else
   {
      switch(LevelInfo.mapFormat)
      {
      case LEVEL_FORMAT_PSX:
         P_LoadConsoleVertexes(lumpnum + ML_VERTEXES);
         P_LoadPSXSectors(lumpnum + ML_SECTORS);
         break;
      default:
         P_LoadVertexes(lumpnum + ML_VERTEXES);
         P_LoadSectors (lumpnum + ML_SECTORS);
         break;
      }
   }
   
   // possible error: missing flats
   CHECK_ERROR();

   // IOANCH 20151212: UDMF
   if(isUdmf)
      udmf.loadSidedefs();
   else
      P_LoadSideDefs(lumpnum + ML_SIDEDEFS); // killough 4/4/98

   // haleyjd 10/03/05: handle multiple map formats
   // IOANCH 20151212: UDMF
   if(isUdmf)
   {
      if(!udmf.loadLinedefs(setupSettings))
      {
         P_SetupLevelError(udmf.error().constPtr(), mapname);
         return;
      }
   }
   else switch(LevelInfo.mapFormat)
   {
   case LEVEL_FORMAT_DOOM:
   case LEVEL_FORMAT_PSX:
      P_LoadLineDefs(lumpnum + ML_LINEDEFS, setupSettings);
      break;
   case LEVEL_FORMAT_HEXEN:
      P_LoadHexenLineDefs(lumpnum + ML_LINEDEFS);
      break;
   }

   // IOANCH 20151213: udmf
   if(isUdmf)
   {
      if(!udmf.loadSidedefs2())
      {
         P_SetupLevelError(udmf.error().constPtr(), mapname);
         return;
      }
   }
   else
      P_LoadSideDefs2(lumpnum + ML_SIDEDEFS);
   
   P_LoadLineDefs2();                      // killough 4/4/98
   
   // IOANCH 20151213: use mgla here and elsewhere
   
   P_LoadBlockMap (mgla.blockmap); // killough 3/1/98
   
   // If it's UDMF, vertices can have extra precision, requiring better geometry calculations.
   R_PointOnSide = R_PointOnSideClassic;  // set classic function unless otherwise set later

   // IOANCH: at this point, mgla.nodes is valid. Check ZDoom node signature too
   ZNodeType znodeSignature;
   int actualNodeLump = -1;
   if((znodeSignature = P_checkForZDoomNodes(mgla.nodes, 
      &actualNodeLump, isUdmf)) != ZNodeType_Invalid && actualNodeLump >= 0)
   {
      P_LoadZNodes(actualNodeLump, znodeSignature);
      if(znodeSignature == ZNodeType_Uncompressed_GL3)
         R_PointOnSide = R_PointOnSidePrecise;

      CHECK_ERROR();
   }
   else if(P_CheckForDeePBSPv4Nodes(lumpnum))   // ioanch 20160204: also DeePBSP
   {
      P_LoadSubsectors_V4(lumpnum + ML_SSECTORS);
      CHECK_ERROR();
      P_LoadNodes_V4(lumpnum + ML_NODES);
      CHECK_ERROR();
      P_LoadSegs_V4(lumpnum + ML_SEGS);
      CHECK_ERROR();
   }
   else
   {
      // IOANCH 20151215: make sure everything is valid. Can happen if UDMF has
      // invalid ZNODES entry
      if(mgla.ssectors < 0 || mgla.segs < 0)
      {
         P_SetupLevelError("UDMF levels don't support vanilla BSP", mapname);
         return;
      }

      // IOANCH: at this point, it's not a UDMF map so mgla will be valid
      P_LoadSubsectors(mgla.ssectors);
      P_LoadNodes     (mgla.nodes);

      // possible error: missing nodes or subsectors
      CHECK_ERROR();

      P_LoadSegs(mgla.segs); 

      // possible error: malformed segs
      CHECK_ERROR();
   }

   // ioanch 20160309: reversed P_GroupLines with P_LoadReject to fix the
   // overrun
   P_GroupLines();
   P_LoadReject(mgla.reject); // haleyjd 01/26/04

   I_Assert(lines && numlinesPlusExtra == numlines + NUM_LINES_EXTRA,
            "lines not initialized\n");
   lines[numlines] = lines[0];   // use the first line as a base for the "junk" line

   // Create bounding boxes now
   P_createSectorBoundingBoxes();

   // haleyjd 01/12/14: build sound environment zones
   P_CreateSoundZones();

   // killough 10/98: remove slime trails from wad
   P_RemoveSlimeTrails(); 

   // haleyjd 08/19/13: call new function to handle bodyque
   G_ClearPlayerCorpseQueue();
   deathmatch_p = deathmatchstarts;

   // haleyjd 10/03/05: handle multiple map formats
   // IOANCH 20151214: UDMF things
   if(isUdmf)
   {
      if(!udmf.loadThings())
      {
         P_SetupLevelError(udmf.error().constPtr(), mapname);
         return;
      }
   }
   else switch(LevelInfo.mapFormat)
   {
   case LEVEL_FORMAT_DOOM:
   case LEVEL_FORMAT_PSX:
      P_LoadThings(lumpnum + ML_THINGS);
      break;
   case LEVEL_FORMAT_HEXEN:
      P_LoadHexenThings(lumpnum + ML_THINGS);
      break;
   }

   // spawn players randomly in deathmatch
   P_DeathMatchSpawnPlayers();

   // possible error: missing player or deathmatch spots
   CHECK_ERROR();

   // haleyjd: init all thing lists (boss brain spots, etc)
   P_InitThingLists(); 

   // clear special respawning queue
   iquehead = iquetail = 0;
   
   // set up world state
   P_SpawnSpecials(setupSettings);

   // SoM: Deferred specials that need to be spawned after P_SpawnSpecials
   P_SpawnDeferredSpecials(setupSettings);

   // haleyjd 01/05/14: create sector interpolation data
   // MaxW: After specials are spawned so slope data is set
   P_createSectorInterps();

   // haleyjd
   P_InitLightning();

   // preload graphics
   if(precache)
      R_PrecacheLevel();

   R_SetViewSize(screenSize+3); //sf

   // haleyjd 07/28/2010: NOW we are in GS_LEVEL. Not before.
   // 01/13/2011: Moved up a bit. The below actions want GS_LEVEL gamestate :>
   gamestate = GS_LEVEL;

   // haleyjd: keep the chasecam on between levels
   if(camera == &chasecam)
      P_ResetChasecam();
   else if(camera == &walkcamera)
      P_ResetWalkcam();
   else
      camera = nullptr;        // camera off

   R_RefreshContexts();

   // haleyjd 01/07/07: initialize ACS for Hexen maps
   //         03/19/11: also allow for DOOM-format maps via MapInfo
   // IOANCH 20151214: changed to use mgla
   if(mgla.behavior >= 0)
      acslumpnum = mgla.behavior;
   else if(LevelInfo.acsScriptLump)
      acslumpnum = setupwad->checkNumForNameNSG(LevelInfo.acsScriptLump, lumpinfo_t::ns_acs);

   ACS_LoadLevelScript(dir, acslumpnum);
}

//
// P_Init
//
void P_Init()
{
   P_InitParticleEffects();  // haleyjd 09/30/01
   P_InitSwitchList();
   P_InitPicAnims();
   P_InitHexenAnims();
   R_InitSprites(spritelist);
   P_InitHubs();
   E_InitTerrainTypes();     // haleyjd 07/03/99
}

//
// OLO Support. - sf
//
// OLOs were something I came up with a while ago when making my 'onslaunch'
// launcher. They are lumps which hold information about the lump: which
// deathmatch type they are best played on etc. which was read by onslaunch
// which adjusted its launch settings appropriately. More importantly,
// they hold the level names which I use here..
//

#if 0
olo_t olo;
int olo_loaded = false;

void P_LoadOlo(void)
{
   int lumpnum;
   char *lump;
   
   if((lumpnum = W_CheckNumForName("OLO")) == -1)
      return;
   
   lump = (char *)(wGlobalDir.CacheLumpNum(lumpnum, PU_CACHE));
   
   if(strncmp(lump, "OLO", 3))
      return;
   
   memcpy(&olo, lump, sizeof(olo_t));
   
   olo_loaded = true;
}
#endif

//=============================================================================
//
// DoomEd Number Disambiguation
//
// Rather than have doomednums be universally contingent on the gamemode, 
// Eternity moves them around into unambiguous ranges. The 7000 range is 
// reserved for thing types translated from other Doom engine games (Heretic,
// Hexen, and Strife). The 6000 range is reserved for "reverse" translation of
// Doom objects (ie. in any game, an Arch-vile can be spawned using 6064).
//
// The "remapped" ranges of doomednums (those that conflict between games)
// are 5-254, 2001-2048, and 3001-3006, though the exact ranges depend on the
// individual game in question
//

//
// P_ConvertDoomExtendedSpawnNum
//
// Checks for and converts a 6000-range doomednum into the corresponding Doom
// object. This is called regardless of the gamemode, after any gamemode
// specific translations occur.
// 6005 - 6089 -> 5    - 89
// 6201 - 6249 -> 2001 - 2049
// 6301 - 6306 -> 3001 - 3006
// IOANCH 20151213: made public
//
void P_ConvertDoomExtendedSpawnNum(mapthing_t *mthing)
{
   int16_t num = mthing->type;

   // Only convert doomednum if there's no existing thingtype for this one
   if(const int typenum = P_FindDoomedNum(num); typenum == -1 || typenum == NUMMOBJTYPES)
   {
      if(num >= 6005 && num <= 6089)
         num -= 6000;
      else if(num >= 6201 && num <= 6249)
         num -= 4200;
      else if(num >= 6301 && num <= 6306)
         num -= 3300;
   }

   mthing->type = num;
}

// null, player starts 1-4, deathmatch spots, and teleport destinations are
// common across all games
#define DEN_MIN     5
#define DEN_DMSPOT  11
#define DEN_TELEMAN 14

//
// P_ConvertHereticThing
//
// Converts Heretic doomednums into an Eternity-compatible range:
// 5    - 96   -> 7005 - 7096
// 2001 - 2005 -> 7201 - 7205
// 2035        -> 7235
// Covers all Heretic thingtypes.
// IOANCH 20151213: made public
//
void P_ConvertHereticThing(mapthing_t *mthing)
{
   int16_t num = mthing->type;

   if(num < DEN_MIN || num == DEN_DMSPOT || num == DEN_TELEMAN)
      return;

   if(num <= 96)
      num += 7000;
   else if((num >= 2001 && num <= 2005) || num == 2035)
      num += 5200;

   mthing->type = num;
}

//
// P_ConvertHexenThing
//
// Converts Hexen doomednums into an Eternity-compatible range:
// 5    - 124  -> 7305 - 7424
// 140         -> 7440
// 254         -> 7554
// 3000 - 3002 -> 9300 - 9302
// Others in 8000, 9000, 10000, and 14000 ranges do not conflict.
//
static void P_ConvertHexenThing(mapthing_t *mthing)
{
   int16_t num = mthing->type;

   if(num < DEN_MIN || num == DEN_DMSPOT || num == DEN_TELEMAN)
      return;

   if(num <= 124 || num == 140 || num == 254)
      num += 7300;
   else if(num >= 3000 && num <= 3002)
      num += 6300; // ZDoom-compatible polyobject spot numbers

   mthing->type = num;
}

//
// P_ConvertStrifeThing
//
// Converts Strife doomednums into an Eternity-compatible range:
// 5    - 236  -> 7605 - 7836
// 2001 - 2048 -> 7901 - 7948
// 3001 - 3006 -> 7951 - 7956
// Covers all Strife thingtypes.
//
static void P_ConvertStrifeThing(mapthing_t *mthing)
{
   int16_t num = mthing->type;

   if(num < DEN_MIN || num == DEN_DMSPOT || num == DEN_TELEMAN)
      return;

   if(num <= 236)
      num += 7600;
   else if(num >= 2001 && num <= 2048)
      num += 5900;
   else if(num >= 3001 && num <= 3006)
      num += 4950;

   mthing->type = num;
}

#define DEN_PSXCHAIN   64
#define DEN_NMSPECTRE  889
#define DEN_PSXSPECTRE 890
#define DEN_EECHAIN    891
#define DEN_DEMON      3002

//
// P_ConvertPSXThing
//
// Take care of oddities in the PSX map format.
//
static void P_ConvertPSXThing(mapthing_t *mthing)
{
   // Demon spawn redirections
   if(mthing->type == DEN_DEMON)
   {
      int16_t flags = mthing->options & MTF_PSX_SPECTRE;

      if(flags == MTF_PSX_SPECTRE)
         mthing->type = DEN_PSXSPECTRE;
      else if(flags == MTF_PSX_NIGHTMARE)
         mthing->type = DEN_NMSPECTRE;
   }
   else if(mthing->type == DEN_PSXCHAIN)
      mthing->type = DEN_EECHAIN;

   mthing->options &= ~MTF_PSX_SPECTRE;
}

//----------------------------------------------------------------------------
//
// $Log: p_setup.c,v $
// Revision 1.16  1998/05/07  00:56:49  killough
// Ignore translucency lumps that are not exactly 64K long
//
// Revision 1.15  1998/05/03  23:04:01  killough
// beautification
//
// Revision 1.14  1998/04/12  02:06:46  killough
// Improve 242 colomap handling, add translucent walls
//
// Revision 1.13  1998/04/06  04:47:05  killough
// Add support for overloading sidedefs for special uses
//
// Revision 1.12  1998/03/31  10:40:42  killough
// Remove blockmap limit
//
// Revision 1.11  1998/03/28  18:02:51  killough
// Fix boss spawner savegame crash bug
//
// Revision 1.10  1998/03/20  00:30:17  phares
// Changed friction to linedef control
//
// Revision 1.9  1998/03/16  12:35:36  killough
// Default floor light level is sector's
//
// Revision 1.8  1998/03/09  07:21:48  killough
// Remove use of FP for point/line queries and add new sector fields
//
// Revision 1.7  1998/03/02  11:46:10  killough
// Double blockmap limit, prepare for when it's unlimited
//
// Revision 1.6  1998/02/27  11:51:05  jim
// Fixes for stairs
//
// Revision 1.5  1998/02/17  22:58:35  jim
// Fixed bug of vanishinb secret sectors in automap
//
// Revision 1.4  1998/02/02  13:38:48  killough
// Comment out obsolete reload hack
//
// Revision 1.3  1998/01/26  19:24:22  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:02:21  killough
// Generalize and simplify level name generation
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

