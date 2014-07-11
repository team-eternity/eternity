//------------------------------------------------------------------------
// LEVEL : Level structures & read/write functions.
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2007 Andrew Apted
//
//  Based on 'BSP 2.3' by Colin Reed, Lee Killough and others.
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

#ifndef __GLBSP_LEVEL_H__
#define __GLBSP_LEVEL_H__

#include "structs.h"
#include "wad.h"

namespace glbsp
{
  namespace level
  {
    struct Node;
    struct Sector;
    struct SuperBlock;

    // a wall_tip is where a wall meets a vertex
    struct WallTip
    {
      // link in list.  List is kept in ANTI-clockwise order.
      WallTip *next;
      WallTip *prev;
      
      // angle that line makes at vertex (degrees).
      angle_g angle;
      
      // sectors on each side of wall.  Left is the side of increasing
      // angles, right is the side of decreasing angles.  Either can be
      // NULL for one sided walls.
      Sector *left;
      Sector *right;
    };
    
    struct Vertex
    {
      // coordinates
      float_g x, y;
      
      // vertex index.  Always valid after loading and pruning of unused
      // vertices has occurred.  For GL vertices, bit 30 will be set.
      int index;
      
      // reference count.  When building normal node info, unused vertices
      // will be pruned.
      int ref_count;
      
      // usually NULL, unless this vertex occupies the same location as a
      // previous vertex.  Only used during the pruning phase.
      Vertex *equiv;
      
      // set of wall_tips
      WallTip *tip_set;
      
      // contains a duplicate vertex, needed when both normal and V2 GL
      // nodes are being built at the same time (this is the vertex used
      // for the normal segs).  Normally NULL.  Note: the wall tips on
      // this vertex are not created.
      Vertex *normal_dup;
    };
    
    struct Sector
    {
      // sector index.  Always valid after loading & pruning.
      int index;
      
      // allow segs from other sectors to coexist in a subsector.
      char coalesce;
      
      // -JL- non-zero if this sector contains a polyobj.
      int has_polyobj;
      
      // reference count.  When building normal nodes, unused sectors will
      // be pruned.
      int ref_count;
      
      // heights
      int floor_h, ceil_h;
      
      // textures
      char floor_tex[8];
      char ceil_tex[8];
      
      // attributes
      int light;
      int special;
      int tag;
      
      // used when building REJECT table.  Each set of sectors that are
      // isolated from other sectors will have a different group number.
      // Thus: on every 2-sided linedef, the sectors on both sides will be
      // in the same group.  The rej_next, rej_prev fields are a link in a
      // RING, containing all sectors of the same group.
      int rej_group;
      
      Sector *rej_next;
      Sector *rej_prev;
      
      // suppress superfluous mini warnings
      int warned_facing;
      char warned_unclosed;
    };
    
    struct Sidedef
    {
      // adjacent sector.  Can be NULL (invalid sidedef)
      Sector *sector;
      
      // offset values
      int x_offset, y_offset;
      
      // texture names
      char upper_tex[8];
      char lower_tex[8];
      char mid_tex[8];
      
      // sidedef index.  Always valid after loading & pruning.
      int index;
      
      // reference count.  When building normal nodes, unused sidedefs will
      // be pruned.
      int ref_count;
      
      // usually NULL, unless this sidedef is exactly the same as a
      // previous one.  Only used during the pruning phase.
      Sidedef *equiv;
      
      // this is true if the sidedef is on a special line.  We don't merge
      // these sidedefs together, as they might scroll, or change texture
      // when a switch is pressed.
      int on_special;
    };
    
    
    struct Linedef
    {
      // link for list
      Linedef *next;
      
      Vertex *start;    // from this vertex...
      Vertex *end;      // ... to this vertex
      
      Sidedef *right;   // right sidedef
      Sidedef *left;    // left sidede, or NULL if none
      
      // line is marked two-sided
      char two_sided;
      
      // prefer not to split
      char is_precious;
      
      // zero length (line should be totally ignored)
      char zero_len;
      
      // sector is the same on both sides
      char self_ref;
      
      // one-sided linedef used for a special effect (windows).
      // The value refers to the opposite sector on the back side.
      Sector * window_effect;
      
      int flags;
      int type;
      int tag;
      
      // Hexen support
      int specials[5];
      
      // normally NULL, except when this linedef directly overlaps an earlier
      // one (a rarely-used trick to create higher mid-masked textures).
      // No segs should be created for these overlapping linedefs.
      Linedef *overlap;
      
      // linedef index.  Always valid after loading & pruning of zero
      // length lines has occurred.
      int index;
    };
    
    struct Thing
    {
      int x, y;
      int type;
      int options;
      
      // other info (angle, and hexen stuff) omitted.  We don't need to
      // write the THING lump, only read it.
      
      // Always valid (thing indices never change).
      int index;
    };
    
    struct Seg
    {
      // link for list
      Seg *next;
      
      Vertex *start;   // from this vertex...
      Vertex *end;     // ... to this vertex
      
      // linedef that this seg goes along, or NULL if miniseg
      Linedef *linedef;
      
      // adjacent sector, or NULL if invalid sidedef or miniseg
      Sector *sector;
      
      // 0 for right, 1 for left
      int side;
      
      // seg on other side, or NULL if one-sided.  This relationship is
      // always one-to-one -- if one of the segs is split, the partner seg
      // must also be split.
      Seg *partner;
      
      // seg index.  Only valid once the seg has been added to a
      // subsector.  A negative value means it is invalid -- there
      // shouldn't be any of these once the BSP tree has been built.
      int index;
      
      // when 1, this seg has become zero length (integer rounding of the
      // start and end vertices produces the same location).  It should be
      // ignored when writing the SEGS or V1 GL_SEGS lumps.  [Note: there
      // won't be any of these when writing the V2 GL_SEGS lump].
      int degenerate;
      
      // the superblock that contains this seg, or NULL if the seg is no
      // longer in any superblock (e.g. now in a subsector).
      SuperBlock *block;
      
      // precomputed data for faster calculations
      float_g psx, psy;
      float_g pex, pey;
      float_g pdx, pdy;
      
      float_g p_length;
      float_g p_angle;
      float_g p_para;
      float_g p_perp;
      
      // linedef that this seg initially comes from.  For "real" segs,
      // this is just the same as the 'linedef' field above.  For
      // "minisegs", this is the linedef of the partition line.
      Linedef *source_line;
    };

    struct Subsec
    {
      // list of segs
      Seg *seg_list;
      
      // count of segs
      int seg_count;
      
      // subsector index.  Always valid, set when the subsector is
      // initially created.
      int index;
      
      // approximate middle point
      float_g mid_x;
      float_g mid_y;
    };
    
    struct BBox
    {
      int minx, miny;
      int maxx, maxy;
    };
    
    struct Child
    {
      // child node or subsector (one must be NULL)
      Node *node;
      Subsec *subsec;
      
      // child bounding box
      BBox bounds;
    };
    
    struct Node
    {
      // IOANCH: changed type from int to float_g
      float_g x, y;     // starting point
      float_g dx, dy;   // offset to ending point
      
      // right & left children
      Child r;
      Child l;
      
      // node index.  Only valid once the NODES or GL_NODES lump has been
      // created.
      int index;
      
      // the node is too long, and the (dx,dy) values should be halved
      // when writing into the NODES lump.
      int too_long;
    };
    
    struct SuperBlock
    {
      // parent of this block, or NULL for a top-level block
      SuperBlock *parent;
      
      // coordinates on map for this block, from lower-left corner to
      // upper-right corner.  Pseudo-inclusive, i.e (x,y) is inside block
      // if and only if x1 <= x < x2 and y1 <= y < y2.
      int x1, y1;
      int x2, y2;
      
      // sub-blocks.  NULL when empty.  [0] has the lower coordinates, and
      // [1] has the higher coordinates.  Division of a square always
      // occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
      SuperBlock *subs[2];
      
      // number of real segs and minisegs contained by this block
      // (including all sub-blocks below it).
      int real_num;
      int mini_num;
      
      // list of segs completely contained by this block.
      Seg *segs;
    };
    /* ----- Level data arrays ----------------------- */
    
    extern int num_vertices;
    extern int num_linedefs;
    extern int num_sidedefs;
    extern int num_sectors;
    extern int num_things;
    extern int num_segs;
    extern int num_subsecs;
    extern int num_nodes;
    extern int num_normal_vert;
    extern int num_gl_vert;
    extern int num_complete_seg;
    
    extern boolean_g lev_doing_normal;
    
    extern Vertex  ** lev_vertices;
    extern Linedef ** lev_linedefs;
    extern Sidedef ** lev_sidedefs;
    extern Sector  ** lev_sectors;
    extern Thing   ** lev_things;

    /* ----- function prototypes ----------------------- */
    
    // allocation routines
    Vertex *NewVertex();
    Linedef *NewLinedef();
    Sidedef *NewSidedef();
    Sector *NewSector();
    Thing *NewThing();
    Seg *NewSeg();
    Subsec *NewSubsec();
    Node *NewNode();
    WallTip *NewWallTip(void);

    // lookup routines
    Vertex *LookupVertex(int index);
    Linedef *LookupLinedef(int index);
    Sidedef *LookupSidedef(int index);
    Sector *LookupSector(int index);
    Thing *LookupThing(int index);
    Seg *LookupSeg(int index);
    Subsec *LookupSubsec(int index);
    Node *LookupNode(int index);

    // load all level data for the current level
    void LoadLevel();

    // free all level data
    void FreeLevel();

    // save the newly computed NODE info etc..
    void SaveLevel(Node *root_node);
  }
}

#define IS_GL_VERTEX  (1 << 30)
#define SUPER_IS_LEAF(s)  \
    ((s)->x2-(s)->x1 <= 256 && (s)->y2-(s)->y1 <= 256)

#endif /* __GLBSP_LEVEL_H__ */
