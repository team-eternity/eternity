//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Refresh/rendering module, shared data struct definitions.
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#ifndef R_DEFS_H__
#define R_DEFS_H__

// haleyjd 12/15/10: lighting data is required here
#include "r_lighting.h"

#include "m_surf.h"

// SoM: Slopes need vectors!
#include "m_vector.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

struct line_t;
struct particle_t;
struct planehash_t;
struct portal_t;
struct sector_t;
struct ereverb_t;
struct ETerrain;
struct dynavertex_t;

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
enum silhouette_e : byte
{
    SIL_NONE = 0,
    SIL_BOTTOM,
    SIL_TOP,
    SIL_BOTH,
};

extern int r_blockmap;

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
// like some DOOM-alikes ("wt", "WebView") do.
//
struct vertex_t
{
    fixed_t x, y;

    // SoM: Cardboard
    // These should always be kept current to x and y
    float fx, fy;

    int polyindex; // index inside hosting polyobject, if applicable
};

// SoM: for attaching surfaces (floors and ceilings) to each other
// SoM: these are flags now
enum attachedtype_e
{
    AS_FLOOR         = 0x01,
    AS_CEILING       = 0x02,
    AS_MIRRORFLOOR   = 0x04,
    AS_MIRRORCEILING = 0x08,
};

struct attachedsurface_t
{
    sector_t *sector;
    int       type;
};

// haleyjd 12/28/08: sector flags
enum
{
    // BOOM generalized sector properties
    SECF_SECRET        = 0x00000001, // bit 7 of generalized special
    SECF_FRICTION      = 0x00000002, // bit 8 of generalized special
    SECF_PUSH          = 0x00000004, // bit 9 of generalized special
    SECF_KILLSOUND     = 0x00000008, // bit A of generalized special
    SECF_KILLMOVESOUND = 0x00000010, // bit B of generalized special
    SECF_INSTANTDEATH  = 0x00000020, // bit C of generalized special (MBF21)
    SECF_MONSTERDEATH  = 0x00000040, // bit D of generalized special (MBF21)

    // Hexen phased lighting
    SECF_PHASEDLIGHT   = 0x00000080, // spawned with sequence start special
    SECF_LIGHTSEQUENCE = 0x00000100, // spawned with sequence special
    SECF_LIGHTSEQALT   = 0x00000200, // spawned with sequence alt special

    // UDMF given
    SECF_FLOORLIGHTABSOLUTE = 0x00000400, // lightfloor is set absolutely
    SECF_CEILLIGHTABSOLUTE  = 0x00000800, // lightceiling is set absolutely
};

// haleyjd 12/31/08: sector damage flags
enum
{
    SDMG_LEAKYSUIT       = 0x00000001, // rad suit leaks at random
    SDMG_IGNORESUIT      = 0x00000002, // rad suit is ignored entirely
    SDMG_ENDGODMODE      = 0x00000004, // turns off god mode if on
    SDMG_EXITLEVEL       = 0x00000008, // exits when player health <= 10
    SDMG_TERRAINHIT      = 0x00000010, // damage causes a terrain hit
    SDMG_INSTAEXITNORMAL = 0x00000020, // exits to next map (only for MBF21 instadeath)
    SDMG_INSTAEXITSECRET = 0x00000040, // exits to secret map (only for MBF21 instadeath)
};

// haleyjd 08/30/09: internal sector flags
enum
{
    SIF_SKY       = 0x00000001, // sector is sky
    SIF_WASSECRET = 0x00000002, // sector was previously secret
    SIF_PHASESCAN = 0x00000004, // being scanned for phased light
    SIF_PORTALBOX = 0x00000008  // sector is a line portal buffer: all its lines
                                // are either 1-sided walls or portal walls.
};

// haleyjd 06/22/14: Heretic push types
enum
{
    SECTOR_HTIC_NONE,    // nothing
    SECTOR_HTIC_CURRENT, // created by types 20-39
    SECTOR_HTIC_WIND     // created by types 40-51
};

// Seg skew types
enum skewType_e
{
    SKEW_NONE = 0,
    SKEW_FRONT_FLOOR,
    SKEW_FRONT_CEILING,
    SKEW_BACK_FLOOR,
    SKEW_BACK_CEILING,
    NUMSKEWTYPES
};

constexpr int SKEW_FRONT_TO_BACK = SKEW_BACK_FLOOR - SKEW_FRONT_FLOOR;

//
// Slope Structures
//

// SoM: Map-slope struct. Stores both floating point and fixed point versions
// of the vectors.
struct pslope_t
{
    // --- Information used in clipping/projection ---
    // Origin vector for the plane. Z is always the host sector's floor height.
    v3fixed_t o;
    v3float_t of;

    // The normal of the 3d plane the slope creates.
    v3fixed_t normal;
    v3float_t normalf;

    // 2-Dimensional vector (x, y) normalized. Used to determine distance from
    // the origin in 2d mapspace.
    v2fixed_t d;
    v2float_t df;

    // The rate at which z changes based on distance from the origin plane.
    fixed_t zdelta;
    float   zdeltaf;

    //
    // IMPORTANT
    //
    // The following are references to the containing sector. They work simply because we follow the
    // rule that each sector may have its own slope, no slopes are shared. If we decide to share
    // slopes, we MUST make sure to decouple the following fields from pslope_t
    //

    // Offset of this slope's origin from surface's height, set on sector assignment and kept constant
    fixed_t surfaceZOffset;
    float   surfaceZOffsetF; // floating-point variant
};

//
// Sector Actions
//

static constexpr int NUMLINEARGS = 5;

// sector action flags
enum sectoractionflags_e : unsigned int
{
    SEC_ACTION_NOTREPEAT  = 0x00000001,
    SEC_ACTION_PROJECTILE = 0x00000002,
    SEC_ACTION_MONSTER    = 0x00000004,
    SEC_ACTION_NOPLAYER   = 0x00000008,
    SEC_ACTION_ENTER      = 0x00000010,
    SEC_ACTION_EXIT       = 0x00000020,
};

struct sectoraction_t
{
    DLListItem<sectoraction_t> links;

    Mobj *mo;
    int   actionflags;
};

// sector interpolation values
struct sectorinterp_t
{
    bool interpolated; // if true, interpolated

    fixed_t prevfloorheight; // previous values, stored for interpolation
    fixed_t prevceilingheight;
    float   prevfloorheightf;
    float   prevceilingheightf;

    fixed_t backfloorheight; // backup values, used as cache during rendering
    fixed_t backceilingheight;
    float   backfloorheightf;
    float   backceilingheightf;

    // as the above sets of values, but for slope origin Z
    float prevfloorslopezf;
    float prevceilingslopezf;

    float backfloorslopezf;
    float backceilingslopezf;
};

//
// Sector box info
//
struct sectorbox_t
{
    fixed_t box[4]; // bounding box per sector
    float   fbox[4];
};

using sectorboxvisit_t = Surfaces<uint64_t>;

//
// Sound Zones
//
struct soundzone_t
{
    ereverb_t *reverb;
};

static const unsigned secf_surfLightAbsolute[surf_NUM] = { SECF_FLOORLIGHTABSOLUTE, SECF_CEILLIGHTABSOLUTE };

//
// Sector surface structure
//
struct surface_t
{
    fixed_t height;
    int     pic; // MUST BE CACHED IF MODIFIED AT RUNTIME

    // thinker_t for reversable actions
    // jff 2/22/98 make thinkers on
    // floors, ceilings independent of one another
    Thinker *data;

    // killough 3/7/98: floor and ceiling texture offsets
    v2fixed_t offset;
    v2float_t scale;

    // haleyjd 07/04/07: Happy July 4th :P
    // Angles for flat rotation!
    float angle, baseangle;

    // killough 4/11/98: support for lightlevels coming from another sector
    int lightsec;

    // ioanch: UDMF-given floor and ceiling delta light level
    int16_t lightdelta;

    // SoM 9/19/02: Better way to move 3dsides with a sector.
    // SoM 11/09/04: Improved yet again!
    int  numattached;
    int *attached;
    int  numsectors;
    int *attsectors;

    // SoM 10/14/07: And now surfaces of other sectors can be attached to a
    // sector's floor and/or ceiling
    int                asurfacecount;
    attachedsurface_t *asurfaces;

    // Flags for portals
    unsigned pflags;
    // Portals
    portal_t *portal;

    // Cardboard optimization
    // They are set in R_subsector and R_FakeFlat and are
    // only valid for that sector for that frame.
    float heightf;

    // SoM 5/10/09: Happy birthday to me. Err, Slopes!
    pslope_t *slope;

    // haleyjd 10/17/10: terrain type overrides
    ETerrain *terrain;

    fixed_t        getZAt(fixed_t x, fixed_t y) const;
    inline fixed_t getZAt(v2fixed_t v) const { return getZAt(v.x, v.y); }
};

//
// All sector_t properties required for rendering
// MUST BE TRIVIALLY CONSTRUCTABLE
//
struct rendersector_t
{
    int      linecount;
    line_t **lines;

    // Keep name short because it's very frequently used.
    Surfaces<surface_t> srf;

    int16_t lightlevel;

    // killough 3/7/98: support flat heights drawn at another sector's heights
    int heightsec; // other sector, or -1 if no other sector

    int bottommap, midmap, topmap; // killough 4/4/98: dynamic colormaps

    // killough 10/98: support skies coming from sidedefs. Allows scrolling
    // skies and other effects. No "level info" kind of lump is needed,
    // because you can use an arbitrary number of skies per level with this
    // method. This field only applies when skyflatnum is used for floorpic
    // or ceilingpic, because the rest of Doom needs to know which is sky
    // and which isn't, etc.

    int sky;

    int groupid;

    // haleyjd 12/28/08: sector flags, for ED/UDMF use. Replaces stupid BOOM
    // generalized sector types outside of DOOM-format maps.
    unsigned int flags;
    unsigned int intflags; // internal flags
};

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
struct sector_t : rendersector_t
{
    // flag set for various uses
    enum
    {
        floor   = 1,
        ceiling = 2
    };

    int16_t      special;
    int16_t      tag;
    int16_t      leakiness;         // ioanch (UDMF): probability / 256 that the suit will leak
    int          nexttag, firsttag; // killough 1/30/98: improves searches for tags.
    int          soundtraversed;    // 0 = untraversed, 1,2 = sndlines-1
    Mobj        *soundtarget;       // thing that made a sound (or null)
    fixed_t      blockbox[4];       // mapblock bounding box for height changes
    PointThinker soundorg;          // origin for any sounds played by the sector
    PointThinker csoundorg;         // haleyjd 10/16/06: separate sound origin for ceiling
    int          validcount;        // if == validcount, already checked
    Mobj        *thinglist;         // list of mobjs in sector
    // ioanch 20160109: keep references to portal interfacing things
    DLListItem<spriteprojnode_t> *spriteproj; // bipartite of sector/mobj sprite proj

    // killough 8/28/98: friction is a sector property, not an mobj property.
    // these fields used to be in Mobj, but presented performance problems
    // when processed as mobj properties. Fix is to make them sector properties.
    int friction, movefactor;

    // ioanch: deleted lightingdata

    // jff 2/26/98 lockout machinery for stairbuilding
    int stairlock; // -2 on first locked -1 after thinker done 0 normally
    int prevsec;   // -1 or number of sector for previous step
    int nextsec;   // -1 or number of next step sector

    // list of mobjs that are at least partially in the sector
    // thinglist is a subset of touching_thinglist
    msecnode_t *touching_thinglist;            // phares 3/14/98
    msecnode_t *touching_thinglist_by_sprites; // sprite width based

    // haleyjd 03/12/03: Heretic wind specials
    int     hticPushType;
    angle_t hticPushAngle;
    fixed_t hticPushForce;

    // haleyjd 09/24/06: sound sequence id
    int sndSeqID;

    DLListItem<particle_t> *ptcllist; // haleyjd 02/20/04: list of particles in sector

    // haleyjd 12/31/08: sector damage properties
    int          damage;      // if > 0, sector is damaging
    int          damagemask;  // damage is done when !(leveltime % mask)
    int          damagemod;   // damage method to use
    unsigned int damageflags; // special damage behaviors

    // haleyjd 08/30/09 - used by the lightning code
    int16_t oldlightlevel;

    // haleyjd 01/15/12: sector actions
    DLListItem<sectoraction_t> *actions;

    // haleyjd 01/12/14: sound environment
    int soundzone;
};

//
// The SideDef.
//

struct side_t
{
    fixed_t offset_base_x; // add this to the calculated texture column
    fixed_t offset_base_y; // add this to the calculated texture top

    fixed_t offset_top_x;    // x offset for toptexture only
    fixed_t offset_top_y;    // y offset for toptexture only
    fixed_t offset_bottom_x; // x offset for bottomtexture only
    fixed_t offset_bottom_y; // y offset for bottomtexture only
    fixed_t offset_mid_x;    // x offset for midtexture only
    fixed_t offset_mid_y;    // y offset for midtexture only

    int16_t light_base;   // light offset for sidedef (or overall if flag is set)
    int16_t light_top;    // light offset for top texture (or overall if flag is set)
    int16_t light_mid;    // light offset for mid texture (or overall if flag is set)
    int16_t light_bottom; // light offset for bottom texture (or overall if flag is set)

    // Texture indices. We do not maintain names here.
    int16_t toptexture;    // MUST BE CACHED IF MODIFIED AT RUNTIME
    int16_t bottomtexture; // MUST BE CACHED IF MODIFIED AT RUNTIME
    int16_t midtexture;    // MUST BE CACHED IF MODIFIED AT RUNTIME

    uint16_t  intflags; // keep intflags here (we may also afford to edit "special")
    sector_t *sector;   // Sector the SideDef is facing.

    // killough 4/4/98, 4/11/98: highest referencing special linedef's type,
    // or lump number of special effect. Allows texture names to be overloaded
    // for other functions.
    int special;

    uint16_t flags;

    inline int topSkewType() const { return (intflags & SDI_SKEW_TOP_MASK) >> SDI_SKEW_TOP_SHIFT; }
    inline int bottomSkewType() const { return (intflags & SDI_SKEW_BOTTOM_MASK) >> SDI_SKEW_BOTTOM_SHIFT; }
    inline int middleSkewType() const { return (intflags & SDI_SKEW_MIDDLE_MASK) >> SDI_SKEW_MIDDLE_SHIFT; }
};

//
// Move clipping aid for LineDefs.
//
enum slopetype_t
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
};

struct seg_t;

struct line_t
{
    vertex_t *v1, *v2; // Vertices, from v1 to v2.
    fixed_t   dx, dy;  // Precalculated v2 - v1 for side checking.
    int16_t   flags;   // Animation related.
    int       special;
    int       tag; // haleyjd 02/27/07: line id's

    // haleyjd 06/19/06: extended from short to long for 65535 sidedefs
    int sidenum[2]; // Visual appearance: SideDefs.

    fixed_t      bbox[4];     // A bounding box, for the linedef's extent
    slopetype_t  slopetype;   // To aid move clipping.
    sector_t    *frontsector; // Front and back sector.
    sector_t    *backsector;
    int          validcount; // if == validcount, already checked
    int          tranlump;   // killough 4/11/98: translucency filter, -1 == none: MUST BE CACHED IF MODIFIED AT RUNTIME
    int          firsttag, nexttag; // killough 4/17/98: improves searches for tags.
    PointThinker soundorg;          // haleyjd 04/19/09: line sound origin
    int          intflags;          // haleyjd 01/22/11: internal flags

    // SoM 12/10/03: wall portals
    int       pflags;
    portal_t *portal;

    // SoM 05/11/09: Pre-calculated 2D normal for the line
    float nx, ny;

    // haleyjd 02/26/05: ExtraData fields
    unsigned int extflags;          // activation flags for param specials
    int          args[NUMLINEARGS]; // argument values for param specials
    float        alpha;             // alpha

    // ioanch 20160312
    line_t *beyondportalline; // reference to a sector beyond a 1-side portal
};

struct rpolyobj_t;
struct rpolybsp_t;

//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//  indicating the visible walls that define
//  (all or some) sides of a convex BSP leaf.
//
struct subsector_t
{
    sector_t *sector;

    // haleyjd 06/19/06: converted from short to long for 65535 segs
    int numlines, firstline;

    DLListItem<rpolyobj_t> *polyList; // haleyjd 05/15/08: list of polyobj fragments
    rpolybsp_t             *bsp;      // haleyjd 05/05/13: sub-BSP tree
};

// phares 3/14/98
//
// Sector list node showing all sectors an object appears in.
//
// There are two threads that flow through these nodes. The first thread
// starts at touching_thinglist in a sector_t and flows through the m_snext
// links to find all mobjs that are entirely or partially in the sector.
// The second thread starts at touching_sectorlist in an Mobj and flows
// through the m_tnext links to find all sectors a thing touches. This is
// useful when applying friction or push effects to sectors. These effects
// can be done as thinkers that act upon all objects touching their sectors.
// As an mobj moves through the world, these nodes are created and
// destroyed, with the links changed appropriately.
//
// For the links, nullptr means top or end of list.

struct msecnode_t
{
    sector_t   *m_sector; // a sector containing this object
    Mobj       *m_thing;  // this object
    msecnode_t *m_tprev;  // prev msecnode_t for this thing
    msecnode_t *m_tnext;  // next msecnode_t for this thing
    msecnode_t *m_sprev;  // prev msecnode_t for this sector
    msecnode_t *m_snext;  // next msecnode_t for this sector
    bool        visited;  // killough 4/4/98, 4/7/98: used in search algorithms
};

//
// The LineSeg.
//
struct seg_t
{
    union
    {
        struct
        {
            vertex_t *v1, *v2;
        };
        struct
        {
            dynavertex_t *dyv1, *dyv2;
        };
    };
    float   offset;
    side_t *sidedef;
    line_t *linedef;

    // Sector references.
    // Could be retrieved from linedef, too
    // (but that would be slower -- killough)
    // backsector is nullptr for one sided lines

    sector_t *frontsector, *backsector;
    bool      frontside;

    // SoM: Precached seg length in float format
    float len;
};

//
// BSP node.
//
struct node_t
{
    fixed_t x, y, dx, dy; // Partition line.
    fixed_t bbox[2][4];   // Bounding box for each child.
    int     children[2];  // If NF_SUBSECTOR, it's a subsector.
};

//
// fnode
//
// haleyjd 12/07/12: The fnode structure holds floating-point general line
// equation coefficients and float versions of partition line coordinates and
// lengths. It is kept separate from node_t for purposes of not causing that
// structure to become cache inefficient.
//
struct fnode_t
{
    double a, b, c; // haleyjd 05/20/08: coefficients for general line equation
    double len;     // length of partition line, for normalization
};

//
// OTHER TYPES
//

//
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//

struct spriteframe_t
{
    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    int rotate;

    // Lump to use for view angles 0-7.
    int16_t lump[8];

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte flip[8];
};

//
// A sprite definition:
//  a number of animation frames.
//

struct spritedef_t
{
    int            numframes;
    spriteframe_t *spriteframes;
};

// SoM: Information used in texture mapping sloped planes
struct rslope_t
{
    // A is a vector, which represents how movement through x and y screen space changes the u texture coordinate
    // B is a vector, which represents how movement through x and y screen space changes the v texture coordinate

    // Alternatively: A maps screen coordinates to u coords, B maps screen coordinates to v coords,
    //                and C maps screen coordinates to inverse plane distances

    v3double_t A, B, C;
    double     zat, plight, shade;
};

//
// Now what is a visplane, anyway?
//
// Go to http://classicgaming.com/doom/editing/ to find out -- killough
//

// SoM: Or, since that url is no longer valid, you could just explain it here!
// A visplane is basically a list of columns that a floor or ceiling occupies
// in screen space. Doom's renderer then rasterizes them (turns them into
// horizontal spans) in a rather quick single pass so they can be textured
// in a quick, constant-z texture mapping loop.

struct visplane_t
{
    visplane_t               *next;     // Next visplane in hash chain -- killough
    int                       chainnum; // The index of this visplane's hash chain, for optimisation
    int                       picnum, lightlevel, minx, maxx;
    fixed_t                   height;
    const lighttable_t *const (*colormap)[MAXLIGHTZ];
    const lighttable_t       *fullcolormap;  // SoM: Used by slopes.
    const lighttable_t       *fixedcolormap; // haleyjd 10/16/06
    v2fixed_t                 offs;          // killough 2/28/98: Support scrolling flats
    v2float_t                 scale;

    // SoM: The plane silhouette arrays are allocated based on screen-size now.
    int *top;
    int *bottom;

    // This is the width of the above silhouette buffers. If a video mode is set
    // that is wider than max_width, the visplane's buffers need to be
    // reallocated to use the wider screen res.
    unsigned int max_width;

    fixed_t viewx, viewy, viewz;

    // SoM: Cardboard optimization
    float xoffsf, yoffsf, heightf;
    float viewxf, viewyf, viewzf;

    float angle, viewsin, viewcos; // haleyjd 01/05/08: angle

    // Slopes!
    pslope_t *pslope;
    rslope_t  rslope;

    // Needed for overlays
    // This is the table the visplane currently belongs to
    planehash_t *table;
    // This is the blending flags from the portal surface (flags & PS_OVERLAYFLAGS)
    int bflags;
    // Opacity of the overlay (255 - opaque, 0 - translucent)
    byte opacity;
};

struct planehash_t
{
    int          chaincount;
    visplane_t **chains;
    planehash_t *next; // if this planehash is part of a reusable list
};

#endif

//----------------------------------------------------------------------------
//
// $Log: r_defs.h,v $
// Revision 1.18  1998/04/27  02:12:59  killough
// Program beautification
//
// Revision 1.17  1998/04/17  10:36:44  killough
// Improve linedef tag searches (see p_spec.c)
//
// Revision 1.16  1998/04/12  02:08:31  killough
// Add ceiling light property, translucent walls
//
// Revision 1.15  1998/04/07  06:52:40  killough
// Simplify sector_thinglist traversal to use simpler markers
//
// Revision 1.14  1998/04/06  04:42:42  killough
// Add dynamic colormaps, thinglist_validcount
//
// Revision 1.13  1998/03/28  18:03:26  killough
// Add support for underwater sprite clipping
//
// Revision 1.12  1998/03/23  03:34:11  killough
// Add support for an arbitrary number of colormaps
//
// Revision 1.11  1998/03/20  00:30:33  phares
// Changed friction to linedef control
//
// Revision 1.10  1998/03/16  12:41:54  killough
// Add support for floor lightlevel from other sector
//
// Revision 1.9  1998/03/09  07:33:44  killough
// Add scroll effect fields, remove FP for point/line queries
//
// Revision 1.8  1998/03/02  11:49:58  killough
// Support for flats scrolling
//
// Revision 1.7  1998/02/27  11:50:49  jim
// Fixes for stairs
//
// Revision 1.6  1998/02/23  00:42:24  jim
// Implemented elevators
//
// Revision 1.5  1998/02/17  22:58:30  jim
// Fixed bug of vanishinb secret sectors in automap
//
// Revision 1.4  1998/02/09  03:21:20  killough
// Allocate MAX screen height/width in arrays
//
// Revision 1.3  1998/02/02  14:19:49  killough
// Improve searching of matching sector tags
//
// Revision 1.2  1998/01/26  19:27:36  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
