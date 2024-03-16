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
//  all external data is defined here
//  most of the data is loaded into different structures at run time
//  some internal structures shared by many modules are here
//
//-----------------------------------------------------------------------------

#ifndef DOOMDATA_H__
#define DOOMDATA_H__

// The most basic types we use, portability.
#include "doomtype.h"
#include "m_fixed.h" // ioanch 20151218: fixed point mapthing coordinates

// haleyjd 10/03/05: all these structures should be byte aligned
#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

//
// Map level types.
// The following data structures define the persistent format
// used in the lumps of the WAD files.
//

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum
{
   ML_LABEL,             // A separator, name, ExMx or MAPxx
   ML_THINGS,            // Monsters, items..
   ML_LINEDEFS,          // LineDefs, from editing
   ML_SIDEDEFS,          // SideDefs, from editing
   ML_VERTEXES,          // Vertices, edited and BSP splits generated
   ML_SEGS,              // LineSegs, from LineDefs split by BSP
   ML_SSECTORS,          // SubSectors, list of LineSegs
   ML_NODES,             // BSP nodes
   ML_SECTORS,           // Sectors, from editing
   ML_REJECT,            // LUT, sector-sector visibility
   ML_BLOCKMAP,          // LUT, motion clipping, walls/grid element
   ML_BEHAVIOR,          // haleyjd 10/03/05: behavior, used to id hexen maps

   // PSX
   ML_LEAFS = ML_BEHAVIOR, // haleyjd 12/12/13: for identifying console map formats

   // Doom 64
   ML_LIGHTS,
   ML_MACROS
};

// A single Vertex.
struct mapvertex_t
{
   int16_t x, y;
};

// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
struct mapsidedef_t
{
   int16_t textureoffset;
   int16_t rowoffset;
   char    toptexture[8];
   char    bottomtexture[8];
   char    midtexture[8];
   int16_t sector;  // Front sector, towards viewer.
};

// A LineDef, as used for editing, and as input to the BSP builder.

struct maplinedef_t
{
   int16_t v1;
   int16_t v2;
   int16_t flags;
   int16_t special;
   int16_t tag;
   int16_t sidenum[2];  // sidenum[1] will be -1 if one sided
};

#define NUMHXLINEARGS 5

struct maplinedefhexen_t
{
   int16_t v1;
   int16_t v2;
   int16_t flags;
   byte    special;
   byte    args[NUMHXLINEARGS];
   int16_t sidenum[2];
};

//
// LineDef attributes.
//

// Solid, is an obstacle.
#define ML_BLOCKING             1

// Blocks monsters only.
#define ML_BLOCKMONSTERS        2

// Backside will not be drawn if not two sided.
#define ML_TWOSIDED             4

// If a texture is pegged, the texture will have
// the end exposed to air held constant at the
// top or bottom of the texture (stairs or pulled
// down things) and will move with a height change
// of one of the neighbor sectors.
// Unpegged textures always have the first row of
// the texture at the top pixel of the line for both
// top and bottom textures (use next to windows).

// upper texture unpegged
#define ML_DONTPEGTOP           8

// lower texture unpegged
#define ML_DONTPEGBOTTOM        16

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET               32

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK           64

// Don't draw on the automap at all.
#define ML_DONTDRAW             128

// Set if already seen, thus drawn in automap.
#define ML_MAPPED               256

//jff 3/21/98 Set if line absorbs use by player
//allow multiple push/switch triggers to be used on one push
#define ML_PASSUSE              512

// SoM 9/02/02: 3D Middletexture flag!
#define ML_3DMIDTEX             1024

// haleyjd 05/02/06: Although it was believed until now that a reserved line
// flag was unnecessary, a problem with Ultimate DOOM E2M7 has disproven this
// theory. It has roughly 1000 linedefs with 0xFE00 masked into the flags, so
// making the next line flag reserved and using it to toggle off ALL extended
// flags will preserve compatibility for such maps. I have been told this map
// is one of the first ever created, so it may have something to do with that.
#define ML_RESERVED             2048

// MBF21
#define ML_BLOCKLANDMONSTERS    4096
#define ML_BLOCKPLAYERS         8192

// haleyjd 01/22/11: internal line flags
enum
{
   MLI_DYNASEGLINE = 0x01, // Consider only via dynasegs for rendering, etc.
   MLI_FLOORPORTALCOPIED = 0x02, // ioanch 20160219: for type 385
   MLI_CEILINGPORTALCOPIED = 0x04, // ioanch 20160219: for type 385
   MLI_1SPORTALLINE = 0x08,   // wall portal from a single-sided line
   MLI_MOVINGPORTAL = 0x10,   // line is on a moving portal
};

// internal sidedef flags
enum
{
   SDI_VERTICALLYSCROLLING = 0x0001,   // sidedef is targeted for vertical scrolling (needed by skies)

   SDI_SKEW_TOP_MASK       = 0x000E,   // top solid seg skew type: bits 2, 3, and 4
   SDI_SKEW_TOP_SHIFT      = 1,

   SDI_SKEW_BOTTOM_MASK    = 0x0070,   // bottom solid seg skew type: bits 5, 6, and 7
   SDI_SKEW_BOTTOM_SHIFT   = 4,

   SDI_SKEW_MIDDLE_MASK    = 0x0380,   // middle masked seg skew type: bits 8, 9, and 10
   SDI_SKEW_MIDDLE_SHIFT   = 7,
};

// sidedef flags
enum
{
   SDF_LIGHT_BASE_ABSOLUTE   = 0x0001,
   SDF_LIGHT_TOP_ABSOLUTE    = 0x0002,
   SDF_LIGHT_MID_ABSOLUTE    = 0x0004,
   SDF_LIGHT_BOTTOM_ABSOLUTE = 0x0008,
};

// Sector definition, from editing.
struct mapsector_t
{
   int16_t floorheight;
   int16_t ceilingheight;
   char  floorpic[8];
   char  ceilingpic[8];
   int16_t lightlevel;
   int16_t special;
   int16_t tag;
};

// SubSector, as generated by BSP.
struct mapsubsector_t
{
   int16_t numsegs;
   int16_t firstseg;    // Index of first one; segs are stored sequentially.
};

// ioanch 20160204: DeePBSP subsector, thanks to PrBoom for the implementation
struct mapsubsector_v4_t
{
   uint16_t numsegs; // same signedness as in PrBoom
   int32_t firstseg;
};

// LineSeg, generated by splitting LineDefs
// using partition lines selected by BSP builder.
struct mapseg_t
{
   int16_t v1;
   int16_t v2;
   int16_t angle;
   int16_t linedef;
   int16_t side;
   int16_t offset;
};

// ioanch 20160204: DEEPBSP seg (based on PRBOOM implementation)
struct mapseg_v4_t
{
   int32_t v1;
   int32_t v2;
   int16_t angle;
   int16_t linedef;
   int16_t side;
   int16_t offset;
};

// BSP node structure.

// Indicate a leaf.
#define NF_SUBSECTOR    0x80000000

struct mapnode_t
{
   int16_t x;  // Partition line from (x,y) to x+dx,y+dy)
   int16_t y;
   int16_t dx;
   int16_t dy;
   // Bounding box for each child, clip against view frustum.
   int16_t bbox[2][4];
   // If NF_SUBSECTOR its a subsector, else it's a node of another subtree.
   uint16_t children[2];
};

// ioanch 20160204: add DEEPBSP nodes
struct mapnode_v4_t
{
   int16_t x;
   int16_t y;
   int16_t dx;
   int16_t dy;
   int16_t bbox[2][4];
   int32_t children[2]; // 32-bit here.
};

// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
struct mapthingdoom_t
{
   int16_t x;
   int16_t y;
   int16_t angle;
   int16_t type;
   int16_t options;
};

#define NUMHXTARGS 5

struct mapthinghexen_t
{
   int16_t tid;
   int16_t x;
   int16_t y;
   int16_t height;
   int16_t angle;
   int16_t type;
   int16_t options;
   byte    special;
   byte    args[NUMHXTARGS];
};

// haleyjd 03/03/07: New mapthing_t structure. The structures above are used to
// read things from the wad lump, but this new mapthing_t is used to store the
// data in memory now. This eliminates some weirdness and redundancies

// ioanch 20151218: use fixed point coordinates.

#define NUMMTARGS 5

struct mapthing_t
{
   int16_t tid;       // scripting id
   fixed_t x;         // x coord
   fixed_t y;         // y coord
   fixed_t height;    // z height relative to floor
   int16_t angle;     // angle in wad format
   int16_t type;      // doomednum
   int16_t options;   // bitflags
   uint32_t extOptions; // ioanch 20151218: extended options (needed by UDMF)
   fixed_t healthModifier; // Overriding health, based on UDMF thing health.
                           // 0 to ignore

   int     special;   // scripting special
   int     args[NUMMTARGS]; // arguments for special

   int     recordnum; // for ExtraData hashing
   int     next;
};

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

#endif // __DOOMDATA__

//----------------------------------------------------------------------------
//
// $Log: doomdata.h,v $
// Revision 1.4  1998/05/03  22:39:10  killough
// beautification
//
// Revision 1.3  1998/03/23  06:42:57  jim
// linedefs reference initial version
//
// Revision 1.2  1998/01/26  19:26:37  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
