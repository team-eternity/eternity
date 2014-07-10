//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
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
//
//  ZDBSP format support based on code (C) 2002,2003 Randy Heit
// 
//------------------------------------------------------------------------

//
// MODIFIED BY Ioan Chera for AutoDoom, 20130812
//

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "analyze.h"
#include "../b_glbsp.h" // IOANCH: AutoDoom interface
#include "blockmap.h"
#include "level.h"
#include "node.h"
#include "reject.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


#define DEBUG_LOAD      0
#define DEBUG_BSP       0

#define ALLOC_BLKNUM  1024


// per-level variables

boolean_g lev_doing_normal;
boolean_g lev_doing_hexen;

static boolean_g lev_force_v3;
static boolean_g lev_force_v5;


#define LEVELARRAY(TYPE, BASEVAR, NUMVAR)  \
    TYPE ** BASEVAR = NULL;  \
    int NUMVAR = 0;


LEVELARRAY(vertex_t,  lev_vertices,   num_vertices)
LEVELARRAY(linedef_t, lev_linedefs,   num_linedefs)
LEVELARRAY(sidedef_t, lev_sidedefs,   num_sidedefs)
LEVELARRAY(sector_t,  lev_sectors,    num_sectors)
LEVELARRAY(thing_t,   lev_things,     num_things)

static LEVELARRAY(seg_t,     segs,       num_segs)
static LEVELARRAY(subsec_t,  subsecs,    num_subsecs)
static LEVELARRAY(node_t,    nodes,      num_nodes)
static LEVELARRAY(wall_tip_t,wall_tips,  num_wall_tips)


int num_normal_vert = 0;
int num_gl_vert = 0;
int num_complete_seg = 0;


/* ----- allocation routines ---------------------------- */

#define ALLIGATOR(TYPE, BASEVAR, NUMVAR)  \
{  \
  if ((NUMVAR % ALLOC_BLKNUM) == 0)  \
  {  \
    BASEVAR = static_cast<TYPE**>(UtilRealloc(BASEVAR, (NUMVAR + ALLOC_BLKNUM) *   \
        sizeof(TYPE *)));  \
  }  \
  BASEVAR[NUMVAR] = (TYPE *) UtilCalloc(sizeof(TYPE));  \
  NUMVAR += 1;  \
  return BASEVAR[NUMVAR - 1];  \
}


vertex_t *NewVertex(void)
  ALLIGATOR(vertex_t, lev_vertices, num_vertices)

linedef_t *NewLinedef(void)
  ALLIGATOR(linedef_t, lev_linedefs, num_linedefs)

sidedef_t *NewSidedef(void)
  ALLIGATOR(sidedef_t, lev_sidedefs, num_sidedefs)

sector_t *NewSector(void)
  ALLIGATOR(sector_t, lev_sectors, num_sectors)

thing_t *NewThing(void)
  ALLIGATOR(thing_t, lev_things, num_things)

seg_t *NewSeg(void)
  ALLIGATOR(seg_t, segs, num_segs)

subsec_t *NewSubsec(void)
  ALLIGATOR(subsec_t, subsecs, num_subsecs)

node_t *NewNode(void)
  ALLIGATOR(node_t, nodes, num_nodes)

wall_tip_t *NewWallTip(void)
  ALLIGATOR(wall_tip_t, wall_tips, num_wall_tips)


/* ----- free routines ---------------------------- */

#define FREEMASON(TYPE, BASEVAR, NUMVAR)  \
{  \
  int i;  \
  for (i=0; i < NUMVAR; i++)  \
    UtilFree(BASEVAR[i]);  \
  if (BASEVAR)  \
    UtilFree(BASEVAR);  \
  BASEVAR = NULL; NUMVAR = 0;  \
}


void FreeVertices(void)
  FREEMASON(vertex_t, lev_vertices, num_vertices)

void FreeLinedefs(void)
  FREEMASON(linedef_t, lev_linedefs, num_linedefs)

void FreeSidedefs(void)
  FREEMASON(sidedef_t, lev_sidedefs, num_sidedefs)

void FreeSectors(void)
  FREEMASON(sector_t, lev_sectors, num_sectors)

void FreeThings(void)
  FREEMASON(thing_t, lev_things, num_things)

void FreeSegs(void)
  FREEMASON(seg_t, segs, num_segs)

void FreeSubsecs(void)
  FREEMASON(subsec_t, subsecs, num_subsecs)

void FreeNodes(void)
  FREEMASON(node_t, nodes, num_nodes)

void FreeWallTips(void)
  FREEMASON(wall_tip_t, wall_tips, num_wall_tips)


/* ----- lookup routines ------------------------------ */

#define LOOKERUPPER(BASEVAR, NUMVAR, NAMESTR)  \
{  \
  if (index < 0 || index >= NUMVAR)  \
    FatalError("No such %s number #%d", NAMESTR, index);  \
    \
  return BASEVAR[index];  \
}

vertex_t *LookupVertex(int index)
  LOOKERUPPER(lev_vertices, num_vertices, "vertex")

linedef_t *LookupLinedef(int index)
  LOOKERUPPER(lev_linedefs, num_linedefs, "linedef")
  
sidedef_t *LookupSidedef(int index)
  LOOKERUPPER(lev_sidedefs, num_sidedefs, "sidedef")
  
sector_t *LookupSector(int index)
  LOOKERUPPER(lev_sectors, num_sectors, "sector")
  
thing_t *LookupThing(int index)
  LOOKERUPPER(lev_things, num_things, "thing")
  
seg_t *LookupSeg(int index)
  LOOKERUPPER(segs, num_segs, "seg")
  
subsec_t *LookupSubsec(int index)
  LOOKERUPPER(subsecs, num_subsecs, "subsector")
  
node_t *LookupNode(int index)
  LOOKERUPPER(nodes, num_nodes, "node")


/* ----- reading routines ------------------------------ */


//
// CheckForNormalNodes
//
int CheckForNormalNodes(void)
{
   // IOANCH: modified to just return true

  return TRUE;
}

//
// GetVertices
//
// IOANCH: modified to read from AutoDoom-GLBSP interface
void GetVertices(void)
{
   int i;
   double x,y;
   
  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetVertices: num = %d\n", count);
# endif


   // IOANCH: interface
   i = 0;
   while (B_GLBSP_GetNextVertex(&x, &y))
   {
      vertex_t *vert = NewVertex();
      vert->x = (float_g) x;
      vert->y = (float_g) y;
      vert->index = i++;
   }

  num_normal_vert = num_vertices;
  num_gl_vert = 0;
  num_complete_seg = 0;
}

//
// GetSectors
//
// IOANCH: AutoDoom
void GetSectors(void)
{
  int i;
  int flh, clh;

  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetSectors: num = %d\n", count);
# endif

   i = 0;
   while (B_GLBSP_GetNextSector(&flh, &clh))
   {
      sector_t *sector = NewSector();
      sector->floor_h = flh;
      sector->ceil_h = clh;
      memset(sector->floor_tex, '-', sizeof(sector->floor_tex));
      memset(sector->ceil_tex, '-', sizeof(sector->ceil_tex));
      sector->light = 128;
      sector->special = 0;
      sector->tag = 0;
      sector->coalesce = FALSE;
      sector->index = i++;
      sector->warned_facing = -1;
   }
}

//
// GetThings
//
// IOANCH: this merely adds a player start, just to be safe
void GetThings(void)
{
   thing_t *thing;
  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetThings: num = %d\n", count);
# endif

   thing = NewThing();
   thing->x = thing->y = 0;
   thing->type = 1;
   thing->options = 7;
   thing->index = 0;
}

//
// GetThingsHexen
//
void GetThingsHexen(void)
{
  int i, count=-1;
  raw_hexen_thing_t *raw;
  lump_t *lump = FindLevelLump("THINGS");

  if (lump)
    count = lump->length / sizeof(raw_hexen_thing_t);

  if (!lump || count == 0)
  {
    // Note: no error if no things exist, even though technically a map
    // will be unplayable without the player starts.
    PrintWarn("Couldn't find any Things!\n");
    return;
  }

  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetThingsHexen: num = %d\n", count);
# endif

  raw = (raw_hexen_thing_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    thing_t *thing = NewThing();

    thing->x = SINT16(raw->x);
    thing->y = SINT16(raw->y);

    thing->type = UINT16(raw->type);
    thing->options = UINT16(raw->options);

    thing->index = i;
  }
}

//
// GetSidedefs
//
// IOANCH: AutoDoom
void GetSidedefs(void)
{
  int i;
  int index;

  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetSidedefs: num = %d\n", count);
# endif


   i = 0;
   while (B_GLBSP_GetNextSidedef(&index))
   {
      sidedef_t *side = NewSidedef();
      side->sector = LookupSector(index);
      if(side->sector)
         side->sector->ref_count++;
      side->x_offset = side->y_offset = 0;
      memset(side->upper_tex, '-', sizeof(side->upper_tex));
      memset(side->lower_tex, '-', sizeof(side->lower_tex));
      memset(side->mid_tex, '-', sizeof(side->mid_tex));
      
      side->index = i++;
   }
}

static INLINE_G sidedef_t *SafeLookupSidedef(uint16_t num)
{
  if (num == 0xFFFF)
    return NULL;

  if ((int)num >= num_sidedefs && (int16_t)(num) < 0)
    return NULL;

  return LookupSidedef(num);
}

//
// GetLinedefs
//
void GetLinedefs(void)
{
  int i;
  int startIdx, endIdx, rightIdx, leftIdx;

  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetLinedefs: num = %d\n", count);
# endif

   i = 0;

   while (B_GLBSP_GetNextLinedef(&startIdx, &endIdx, &rightIdx, &leftIdx))
   {
      linedef_t *line;
      vertex_t *start = LookupVertex(startIdx);
      vertex_t *end = LookupVertex(endIdx);
      start->ref_count++;
      end->ref_count++;
      
      line = NewLinedef();
      line->start = start;
      line->end = end;
      line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) &&
      (fabs(start->y - end->y) < DIST_EPSILON);
      line->flags = leftIdx >= 0 && rightIdx >= 0 ?
      LINEFLAG_TWO_SIDED : 0;  // IOANCH: two sided
      line->type = 0;
      line->tag = 0;
      line->two_sided = (line->flags & LINEFLAG_TWO_SIDED) ? TRUE : FALSE;
      line->is_precious = (line->tag >= 900 && line->tag < 1000) ?
      TRUE : FALSE;
      line->right = SafeLookupSidedef(rightIdx);
      line->left = SafeLookupSidedef(leftIdx);
      if (line->right)
      {
         line->right->ref_count++;
         line->right->on_special |= (line->type > 0) ? 1 : 0;
      }
      
      if (line->left)
      {
         line->left->ref_count++;
         line->left->on_special |= (line->type > 0) ? 1 : 0;
      }
      line->self_ref = (line->left && line->right &&
                        (line->left->sector == line->right->sector));
      
      line->index = i++;

   }

}

//
// GetLinedefsHexen
//
void GetLinedefsHexen(void)
{
  int i, j, count=-1;
  raw_hexen_linedef_t *raw;
  lump_t *lump = FindLevelLump("LINEDEFS");

  if (lump)
    count = lump->length / sizeof(raw_hexen_linedef_t);

  if (!lump || count == 0)
    FatalError("Couldn't find any Linedefs");

  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetLinedefsHexen: num = %d\n", count);
# endif

  raw = (raw_hexen_linedef_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    linedef_t *line;

    vertex_t *start = LookupVertex(UINT16(raw->start));
    vertex_t *end   = LookupVertex(UINT16(raw->end));

    start->ref_count++;
    end->ref_count++;

    line = NewLinedef();

    line->start = start;
    line->end   = end;

    // check for zero-length line
    line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) && 
        (fabs(start->y - end->y) < DIST_EPSILON);

    line->flags = UINT16(raw->flags);
    line->type = UINT8(raw->type);
    line->tag  = 0;

    /* read specials */
    for (j=0; j < 5; j++)
      line->specials[j] = UINT8(raw->specials[j]);

    // -JL- Added missing twosided flag handling that caused a broken reject
    line->two_sided = (line->flags & LINEFLAG_TWO_SIDED) ? TRUE : FALSE;

    line->right = SafeLookupSidedef(UINT16(raw->sidedef1));
    line->left  = SafeLookupSidedef(UINT16(raw->sidedef2));

    // -JL- Added missing sidedef handling that caused all sidedefs to be pruned
    if (line->right)
    {
      line->right->ref_count++;
      line->right->on_special |= (line->type > 0) ? 1 : 0;
    }

    if (line->left)
    {
      line->left->ref_count++;
      line->left->on_special |= (line->type > 0) ? 1 : 0;
    }

    line->self_ref = (line->left && line->right &&
        (line->left->sector == line->right->sector));

    line->index = i;
  }
}


static INLINE_G int TransformSegDist(const seg_t *seg)
{
  float_g sx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
  float_g sy = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

  return (int) ceil(UtilComputeDist(seg->start->x - sx, seg->start->y - sy));
}

static INLINE_G int TransformAngle(angle_g angle)
{
  int result;
  
  result = (int)(angle * 65536.0 / 360.0);
  
  if (result < 0)
    result += 65536;

  return (result & 0xFFFF);
}

static int SegCompare(const void *p1, const void *p2)
{
  const seg_t *A = ((const seg_t **) p1)[0];
  const seg_t *B = ((const seg_t **) p2)[0];

  if (A->index < 0)
    InternalError("Seg %p never reached a subsector !", A);

  if (B->index < 0)
    InternalError("Seg %p never reached a subsector !", B);

  return (A->index - B->index);
}


/* ----- writing routines ------------------------------ */

static const uint8_t *lev_v2_magic = (uint8_t *) "gNd2";
static const uint8_t *lev_v3_magic = (uint8_t *) "gNd3";
static const uint8_t *lev_v5_magic = (uint8_t *) "gNd5";

// IOANCH: added AutoDoom routines

static INLINE_G int VertexIndexAutoDoom(const vertex_t *v)
{
   if(v->index & IS_GL_VERTEX)
      return (v->index & ~IS_GL_VERTEX) + num_normal_vert;
   return v->index;
}

static void PutAutoDoomVertices(void)
{
   int i;
   B_GLBSP_CreateVertexArray(num_vertices);

   for(i = 0; i < num_vertices; ++i)
   {
      vertex_t *vert = lev_vertices[i];
      B_GLBSP_PutVertex((double)vert->x, (double)vert->y,
                        VertexIndexAutoDoom(vert));
   }
}

void PutVertices(char *name, int do_gl)
{
  int count, i;
  lump_t *lump;

  DisplayTicker();

  if (do_gl)
    lump = CreateGLLump(name);
  else
    lump = CreateLevelLump(name);

  for (i=0, count=0; i < num_vertices; i++)
  {
    raw_vertex_t raw;
    vertex_t *vert = lev_vertices[i];

    if ((do_gl ? 1 : 0) != ((vert->index & IS_GL_VERTEX) ? 1 : 0))
    {
      continue;
    }

    raw.x = SINT16(I_ROUND(vert->x));
    raw.y = SINT16(I_ROUND(vert->y));

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;
  }

  if (count != (do_gl ? num_gl_vert : num_normal_vert))
    InternalError("PutVertices miscounted (%d != %d)", count,
      do_gl ? num_gl_vert : num_normal_vert);

  if (lev_doing_normal && ! do_gl && count > 65534)
    MarkHardFailure(LIMIT_VERTEXES);
  else if (count > 32767)
    MarkSoftFailure(do_gl ? LIMIT_GL_VERT : LIMIT_VERTEXES);
}

void PutV2Vertices(int do_v5)
{
  int count, i;
  lump_t *lump;

  DisplayTicker();

  lump = CreateGLLump("GL_VERT");

  if (do_v5)
      AppendLevelLump(lump, lev_v5_magic, 4);
  else
      AppendLevelLump(lump, lev_v2_magic, 4);

  for (i=0, count=0; i < num_vertices; i++)
  {
    raw_v2_vertex_t raw;
    vertex_t *vert = lev_vertices[i];

    if (! (vert->index & IS_GL_VERTEX))
      continue;

    raw.x = SINT32((int)(vert->x * 65536.0));
    raw.y = SINT32((int)(vert->y * 65536.0));

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;
  }

  if (count != num_gl_vert)
    InternalError("PutV2Vertices miscounted (%d != %d)", count,
      num_gl_vert);

  if (count > 32767)
    MarkSoftFailure(LIMIT_GL_VERT);
}

void PutSectors(void)
{
  int i;
  lump_t *lump = CreateLevelLump("SECTORS");

  DisplayTicker();

  for (i=0; i < num_sectors; i++)
  {
    raw_sector_t raw;
    sector_t *sector = lev_sectors[i];

    raw.floor_h = SINT16(sector->floor_h);
    raw.ceil_h  = SINT16(sector->ceil_h);

    memcpy(raw.floor_tex, sector->floor_tex, sizeof(raw.floor_tex));
    memcpy(raw.ceil_tex,  sector->ceil_tex,  sizeof(raw.ceil_tex));

    raw.light = UINT16(sector->light);
    raw.special = UINT16(sector->special);
    raw.tag   = SINT16(sector->tag);

    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_sectors > 65534)
    MarkHardFailure(LIMIT_SECTORS);
  else if (num_sectors > 32767)
    MarkSoftFailure(LIMIT_SECTORS);
}

void PutSidedefs(void)
{
  int i;
  lump_t *lump = CreateLevelLump("SIDEDEFS");

  DisplayTicker();

  for (i=0; i < num_sidedefs; i++)
  {
    raw_sidedef_t raw;
    sidedef_t *side = lev_sidedefs[i];

    raw.sector = (side->sector == NULL) ? SINT16(-1) :
        UINT16(side->sector->index);

    raw.x_offset = SINT16(side->x_offset);
    raw.y_offset = SINT16(side->y_offset);

    memcpy(raw.upper_tex, side->upper_tex, sizeof(raw.upper_tex));
    memcpy(raw.lower_tex, side->lower_tex, sizeof(raw.lower_tex));
    memcpy(raw.mid_tex,   side->mid_tex,   sizeof(raw.mid_tex));
 
    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_sidedefs > 65534)
    MarkHardFailure(LIMIT_SIDEDEFS);
  else if (num_sidedefs > 32767)
    MarkSoftFailure(LIMIT_SIDEDEFS);
}

static void PutAutoDoomLinedefs(void)
{
   int i;
   B_GLBSP_CreateLineArray(num_linedefs);

   for (i = 0; i < num_linedefs; ++i)
   {
      linedef_t *line = lev_linedefs[i];
      B_GLBSP_PutLine(line->start->index, line->end->index,
                      line->right ? line->right->index : -1,
                      line->left ? line->left->index : -1, i);
   }
}

void PutLinedefs(void)
{
  int i;
  lump_t *lump = CreateLevelLump("LINEDEFS");

  DisplayTicker();

  for (i=0; i < num_linedefs; i++)
  {
    raw_linedef_t raw;
    linedef_t *line = lev_linedefs[i];

    raw.start = UINT16(line->start->index);
    raw.end   = UINT16(line->end->index);

    raw.flags = UINT16(line->flags);
    raw.type  = UINT16(line->type);
    raw.tag   = SINT16(line->tag);

    raw.sidedef1 = line->right ? UINT16(line->right->index) : 0xFFFF;
    raw.sidedef2 = line->left  ? UINT16(line->left->index)  : 0xFFFF;

    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_linedefs > 65534)
    MarkHardFailure(LIMIT_LINEDEFS);
  else if (num_linedefs > 32767)
    MarkSoftFailure(LIMIT_LINEDEFS);
}

void PutLinedefsHexen(void)
{
  int i, j;
  lump_t *lump = CreateLevelLump("LINEDEFS");

  DisplayTicker();

  for (i=0; i < num_linedefs; i++)
  {
    raw_hexen_linedef_t raw;
    linedef_t *line = lev_linedefs[i];

    raw.start = UINT16(line->start->index);
    raw.end   = UINT16(line->end->index);

    raw.flags = UINT16(line->flags);
    raw.type  = UINT8(line->type);

    // write specials
    for (j=0; j < 5; j++)
      raw.specials[j] = UINT8(line->specials[j]);

    raw.sidedef1 = line->right ? UINT16(line->right->index) : 0xFFFF;
    raw.sidedef2 = line->left  ? UINT16(line->left->index)  : 0xFFFF;

    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_linedefs > 65534)
    MarkHardFailure(LIMIT_LINEDEFS);
  else if (num_linedefs > 32767)
    MarkSoftFailure(LIMIT_LINEDEFS);
}

static INLINE_G uint16_t VertexIndex16Bit(const vertex_t *v)
{
  if (v->index & IS_GL_VERTEX)
    return (uint16_t) ((v->index & ~IS_GL_VERTEX) | 0x8000U);

  return (uint16_t) v->index;
}

static INLINE_G uint32_t VertexIndex32BitV5(const vertex_t *v)
{
  if (v->index & IS_GL_VERTEX)
    return (uint32_t) ((v->index & ~IS_GL_VERTEX) | 0x80000000U);

  return (uint32_t) v->index;
}

void PutSegs(void)
{
  int i, count;
  lump_t *lump = CreateLevelLump("SEGS");

  DisplayTicker();

  // sort segs into ascending index
  qsort(segs, num_segs, sizeof(seg_t *), SegCompare);

  for (i=0, count=0; i < num_segs; i++)
  {
    raw_seg_t raw;
    seg_t *seg = segs[i];

    // ignore minisegs and degenerate segs
    if (! seg->linedef || seg->degenerate)
      continue;

    raw.start   = UINT16(VertexIndex16Bit(seg->start));
    raw.end     = UINT16(VertexIndex16Bit(seg->end));
    raw.angle   = UINT16(TransformAngle(seg->p_angle));
    raw.linedef = UINT16(seg->linedef->index);
    raw.flip    = UINT16(seg->side);
    raw.dist    = UINT16(TransformSegDist(seg));

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;

#   if DEBUG_BSP
    PrintDebug("PUT SEG: %04X  Vert %04X->%04X  Line %04X %s  "
        "Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index,
        UINT16(raw.start), UINT16(raw.end), UINT16(raw.linedef), 
        seg->side ? "L" : "R", UINT16(raw.angle), 
        seg->start->x, seg->start->y, seg->end->x, seg->end->y);
#   endif
  }

  if (count != num_complete_seg)
    InternalError("PutSegs miscounted (%d != %d)", count,
      num_complete_seg);

  if (count > 65534)
    MarkHardFailure(LIMIT_SEGS);
  else if (count > 32767)
    MarkSoftFailure(LIMIT_SEGS);
}

// IOANCH: added autodoom routine

static void PutAutoDoomSegs(void)
{
   int i, count;
   qsort(segs, num_segs, sizeof(seg_t *), SegCompare);
   B_GLBSP_CreateSegArray(num_segs);
   for(i = 0, count = 0; i < num_segs; ++i)
   {
      seg_t *seg = segs[i];
      if(seg->degenerate)
         continue;
      B_GLBSP_PutSegment(VertexIndexAutoDoom(seg->start),
                         VertexIndexAutoDoom(seg->end),
                         seg->side,
                         seg->linedef ? seg->linedef->index : -1,
                         seg->partner ? seg->partner->index : -1,
                         count++);
   }
}

void PutGLSegs(void)
{
  int i, count;
  lump_t *lump = CreateGLLump("GL_SEGS");

  DisplayTicker();

  // sort segs into ascending index
  qsort(segs, num_segs, sizeof(seg_t *), SegCompare);

  for (i=0, count=0; i < num_segs; i++)
  {
    raw_gl_seg_t raw;
    seg_t *seg = segs[i];

    // ignore degenerate segs
    if (seg->degenerate)
      continue;

    raw.start = UINT16(VertexIndex16Bit(seg->start));
    raw.end   = UINT16(VertexIndex16Bit(seg->end));
    raw.side  = UINT16(seg->side);

    if (seg->linedef)
      raw.linedef = UINT16(seg->linedef->index);
    else
      raw.linedef = UINT16(0xFFFF);

    if (seg->partner)
      raw.partner = UINT16(seg->partner->index);
    else
      raw.partner = UINT16(0xFFFF);

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;

#   if DEBUG_BSP
    PrintDebug("PUT GL SEG: %04X  Line %04X %s  Partner %04X  "
      "(%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index, UINT16(raw.linedef), 
      seg->side ? "L" : "R", UINT16(raw.partner),
      seg->start->x, seg->start->y, seg->end->x, seg->end->y);
#   endif
  }

  if (count != num_complete_seg)
    InternalError("PutGLSegs miscounted (%d != %d)", count,
      num_complete_seg);

  if (count > 65534)
    InternalError("PutGLSegs with %d (> 65534) segs", count);
  else if (count > 32767)
    MarkSoftFailure(LIMIT_GL_SEGS);
}

void PutV3Segs(int do_v5)
{
  int i, count;
  lump_t *lump = CreateGLLump("GL_SEGS");

  if (! do_v5)
      AppendLevelLump(lump, lev_v3_magic, 4);

  DisplayTicker();

  // sort segs into ascending index
  qsort(segs, num_segs, sizeof(seg_t *), SegCompare);

  for (i=0, count=0; i < num_segs; i++)
  {
    raw_v3_seg_t raw;
    seg_t *seg = segs[i];

    // ignore degenerate segs
    if (seg->degenerate)
      continue;

    if (do_v5)
    {
      raw.start = UINT32(VertexIndex32BitV5(seg->start));
      raw.end   = UINT32(VertexIndex32BitV5(seg->end));
    }
    else
    {
      raw.start = UINT32(seg->start->index);
      raw.end   = UINT32(seg->end->index);
    }

    raw.side  = UINT16(seg->side);

    if (seg->linedef)
      raw.linedef = UINT16(seg->linedef->index);
    else
      raw.linedef = UINT16(0xFFFF);

    if (seg->partner)
      raw.partner = UINT32(seg->partner->index);
    else
      raw.partner = UINT32(0xFFFFFFFF);

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;

#   if DEBUG_BSP
    PrintDebug("PUT V3 SEG: %06X  Line %04X %s  Partner %06X  "
      "(%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index, UINT16(raw.linedef), 
      seg->side ? "L" : "R", UINT32(raw.partner),
      seg->start->x, seg->start->y, seg->end->x, seg->end->y);
#   endif
  }

  if (count != num_complete_seg)
    InternalError("PutGLSegs miscounted (%d != %d)", count,
      num_complete_seg);
}

static void PutAutoDoomSubsecs()
{
   int i;
   B_GLBSP_CreateSubsectorArray(num_subsecs);
   for (i = 0; i < num_subsecs; ++i)
   {
      subsec_t *sub = subsecs[i];
      B_GLBSP_PutSubsector(sub->seg_list->index, sub->seg_count, i);
   }
}

void PutSubsecs(char *name, int do_gl)
{
  int i;
  lump_t *lump;

  DisplayTicker();

  if (do_gl)
    lump = CreateGLLump(name);
  else
    lump = CreateLevelLump(name);

  for (i=0; i < num_subsecs; i++)
  {
    raw_subsec_t raw;
    subsec_t *sub = subsecs[i];

    raw.first = UINT16(sub->seg_list->index);
    raw.num   = UINT16(sub->seg_count);

    AppendLevelLump(lump, &raw, sizeof(raw));

#   if DEBUG_BSP
    PrintDebug("PUT SUBSEC %04X  First %04X  Num %04X\n",
      sub->index, UINT16(raw.first), UINT16(raw.num));
#   endif
  }

  if (num_subsecs > 32767)
    MarkHardFailure(do_gl ? LIMIT_GL_SSECT : LIMIT_SSECTORS);
}

void PutV3Subsecs(int do_v5)
{
  int i;
  lump_t *lump;

  DisplayTicker();

  lump = CreateGLLump("GL_SSECT");

  if (! do_v5)
      AppendLevelLump(lump, lev_v3_magic, 4);

  for (i=0; i < num_subsecs; i++)
  {
    raw_v3_subsec_t raw;
    subsec_t *sub = subsecs[i];

    raw.first = UINT32(sub->seg_list->index);
    raw.num   = UINT32(sub->seg_count);

    AppendLevelLump(lump, &raw, sizeof(raw));

#   if DEBUG_BSP
    PrintDebug("PUT V3 SUBSEC %06X  First %06X  Num %06X\n",
      sub->index, UINT32(raw.first), UINT32(raw.num));
#   endif
  }

  if (!do_v5 && num_subsecs > 32767)
    MarkHardFailure(LIMIT_GL_SSECT);
}

static int node_cur_index;

static void PutOneAutoDoomNode(node_t *node)
{
   if(node->r.node)
      PutOneAutoDoomNode(node->r.node);
   if(node->l.node)
      PutOneAutoDoomNode(node->l.node);
   
   node->index = node_cur_index++;
   
   B_GLBSP_PutNode(node->x, node->y, node->dx / (node->too_long ? 2 : 1),
                   node->dy / (node->too_long ? 2 : 1),
                   node->r.node ? node->r.node->index : node->r.subsec->index,
                   node->l.node ? node->l.node->index : node->l.subsec->index,
                   node->r.subsec ? 1 : 0, node->l.subsec ? 1 : 0, node->index);
}

static void PutOneNode(node_t *node, lump_t *lump)
{
  raw_node_t raw;

  if (node->r.node)
    PutOneNode(node->r.node, lump);

  if (node->l.node)
    PutOneNode(node->l.node, lump);

  node->index = node_cur_index++;

  raw.x  = SINT16(node->x);
  raw.y  = SINT16(node->y);
  raw.dx = SINT16(node->dx / (node->too_long ? 2 : 1));
  raw.dy = SINT16(node->dy / (node->too_long ? 2 : 1));

  raw.b1.minx = SINT16(node->r.bounds.minx);
  raw.b1.miny = SINT16(node->r.bounds.miny);
  raw.b1.maxx = SINT16(node->r.bounds.maxx);
  raw.b1.maxy = SINT16(node->r.bounds.maxy);

  raw.b2.minx = SINT16(node->l.bounds.minx);
  raw.b2.miny = SINT16(node->l.bounds.miny);
  raw.b2.maxx = SINT16(node->l.bounds.maxx);
  raw.b2.maxy = SINT16(node->l.bounds.maxy);

  if (node->r.node)
    raw.right = UINT16(node->r.node->index);
  else if (node->r.subsec)
    raw.right = UINT16(node->r.subsec->index | 0x8000);
  else
    InternalError("Bad right child in node %d", node->index);

  if (node->l.node)
    raw.left = UINT16(node->l.node->index);
  else if (node->l.subsec)
    raw.left = UINT16(node->l.subsec->index | 0x8000);
  else
    InternalError("Bad left child in node %d", node->index);

  AppendLevelLump(lump, &raw, sizeof(raw));

# if DEBUG_BSP
  PrintDebug("PUT NODE %04X  Left %04X  Right %04X  "
    "(%d,%d) -> (%d,%d)\n", node->index, UINT16(raw.left),
    UINT16(raw.right), node->x, node->y,
    node->x + node->dx, node->y + node->dy);
# endif
}

static void PutOneV5Node(node_t *node, lump_t *lump)
{
  raw_v5_node_t raw;

  if (node->r.node)
    PutOneV5Node(node->r.node, lump);

  if (node->l.node)
    PutOneV5Node(node->l.node, lump);

  node->index = node_cur_index++;

  raw.x  = SINT16(node->x);
  raw.y  = SINT16(node->y);
  raw.dx = SINT16(node->dx / (node->too_long ? 2 : 1));
  raw.dy = SINT16(node->dy / (node->too_long ? 2 : 1));

  raw.b1.minx = SINT16(node->r.bounds.minx);
  raw.b1.miny = SINT16(node->r.bounds.miny);
  raw.b1.maxx = SINT16(node->r.bounds.maxx);
  raw.b1.maxy = SINT16(node->r.bounds.maxy);

  raw.b2.minx = SINT16(node->l.bounds.minx);
  raw.b2.miny = SINT16(node->l.bounds.miny);
  raw.b2.maxx = SINT16(node->l.bounds.maxx);
  raw.b2.maxy = SINT16(node->l.bounds.maxy);

  if (node->r.node)
    raw.right = UINT32(node->r.node->index);
  else if (node->r.subsec)
    raw.right = UINT32(node->r.subsec->index | 0x80000000U);
  else
    InternalError("Bad right child in V5 node %d", node->index);

  if (node->l.node)
    raw.left = UINT32(node->l.node->index);
  else if (node->l.subsec)
    raw.left = UINT32(node->l.subsec->index | 0x80000000U);
  else
    InternalError("Bad left child in V5 node %d", node->index);

  AppendLevelLump(lump, &raw, sizeof(raw));

# if DEBUG_BSP
  PrintDebug("PUT V5 NODE %08X  Left %08X  Right %08X  "
    "(%d,%d) -> (%d,%d)\n", node->index, UINT32(raw.left),
    UINT32(raw.right), node->x, node->y,
    node->x + node->dx, node->y + node->dy);
# endif
}

static void PutAutoDoomNodes(node_t *root)
{
   B_GLBSP_CreateNodeArray(num_nodes);
   node_cur_index = 0;
   PutOneAutoDoomNode(root);
}

void PutNodes(char *name, int do_gl, int do_v5, node_t *root)
{
  lump_t *lump;

  DisplayTicker();

  if (do_gl)
    lump = CreateGLLump(name);
  else
    lump = CreateLevelLump(name);

  node_cur_index = 0;

  if (root)
  {
    if (do_v5)
      PutOneV5Node(root, lump);
    else
      PutOneNode(root, lump);
  }

  if (node_cur_index != num_nodes)
    InternalError("PutNodes miscounted (%d != %d)",
      node_cur_index, num_nodes);

  if (!do_v5 && node_cur_index > 32767)
    MarkHardFailure(LIMIT_NODES);
}


/* ----- ZDBSP format writing --------------------------- */

static const uint8_t *lev_ZD_magic = (uint8_t *) "ZNOD";

void PutZVertices(void)
{
  int count, i;

  uint32_t orgverts = UINT32(num_normal_vert);
  uint32_t newverts = UINT32(num_gl_vert);

  ZLibAppendLump(&orgverts, 4);
  ZLibAppendLump(&newverts, 4);

  DisplayTicker();

  for (i=0, count=0; i < num_vertices; i++)
  {
    raw_v2_vertex_t raw;
    vertex_t *vert = lev_vertices[i];

    if (! (vert->index & IS_GL_VERTEX))
      continue;

    raw.x = SINT32((int)(vert->x * 65536.0));
    raw.y = SINT32((int)(vert->y * 65536.0));

    ZLibAppendLump(&raw, sizeof(raw));

    count++;
  }

  if (count != num_gl_vert)
    InternalError("PutZVertices miscounted (%d != %d)",
        count, num_gl_vert);
}

void PutZSubsecs(void)
{
  int i;
  int count;
  uint32_t raw_num = UINT32(num_subsecs);

  int cur_seg_index = 0;

  ZLibAppendLump(&raw_num, 4);
  DisplayTicker();

  for (i=0; i < num_subsecs; i++)
  {
    subsec_t *sub = subsecs[i];
    seg_t *seg;

    raw_num = UINT32(sub->seg_count);

    ZLibAppendLump(&raw_num, 4);

    // sanity check the seg index values
    count = 0;
    for (seg = sub->seg_list; seg; seg = seg->next, cur_seg_index++)
    {
      // ignore minisegs and degenerate segs
      if (! seg->linedef || seg->degenerate)
        continue;

      if (cur_seg_index != seg->index)
        InternalError("PutZSubsecs: seg index mismatch in sub %d (%d != %d)\n",
            i, cur_seg_index, seg->index);
      
      count++;
    }

    if (count != sub->seg_count)
      InternalError("PutZSubsecs: miscounted segs in sub %d (%d != %d)\n",
          i, count, sub->seg_count);
  }

  if (cur_seg_index != num_complete_seg)
    InternalError("PutZSubsecs miscounted segs (%d != %d)",
        cur_seg_index, num_complete_seg);
}

void PutZSegs(void)
{
  int i, count;
  uint32_t raw_num = UINT32(num_complete_seg);

  ZLibAppendLump(&raw_num, 4);
  DisplayTicker();

  for (i=0, count=0; i < num_segs; i++)
  {
    seg_t *seg = segs[i];

    // ignore minisegs and degenerate segs
    if (! seg->linedef || seg->degenerate)
      continue;

    if (count != seg->index)
      InternalError("PutZSegs: seg index mismatch (%d != %d)\n",
          count, seg->index);

    {
      uint32_t v1 = UINT32(VertexIndex32BitV5(seg->start));
      uint32_t v2 = UINT32(VertexIndex32BitV5(seg->end));

      uint16_t line = UINT16(seg->linedef->index);
      uint8_t  side = seg->side;

      ZLibAppendLump(&v1,   4);
      ZLibAppendLump(&v2,   4);
      ZLibAppendLump(&line, 2);
      ZLibAppendLump(&side, 1);
    }

    count++;
  }

  if (count != num_complete_seg)
    InternalError("PutZSegs miscounted (%d != %d)",
        count, num_complete_seg);
}

static void PutOneZNode(node_t *node)
{
  raw_v5_node_t raw;

  if (node->r.node)
    PutOneZNode(node->r.node);

  if (node->l.node)
    PutOneZNode(node->l.node);

  node->index = node_cur_index++;

  raw.x  = SINT16(node->x);
  raw.y  = SINT16(node->y);
  raw.dx = SINT16(node->dx / (node->too_long ? 2 : 1));
  raw.dy = SINT16(node->dy / (node->too_long ? 2 : 1));

  ZLibAppendLump(&raw.x,  2);
  ZLibAppendLump(&raw.y,  2);
  ZLibAppendLump(&raw.dx, 2);
  ZLibAppendLump(&raw.dy, 2);

  raw.b1.minx = SINT16(node->r.bounds.minx);
  raw.b1.miny = SINT16(node->r.bounds.miny);
  raw.b1.maxx = SINT16(node->r.bounds.maxx);
  raw.b1.maxy = SINT16(node->r.bounds.maxy);

  raw.b2.minx = SINT16(node->l.bounds.minx);
  raw.b2.miny = SINT16(node->l.bounds.miny);
  raw.b2.maxx = SINT16(node->l.bounds.maxx);
  raw.b2.maxy = SINT16(node->l.bounds.maxy);

  ZLibAppendLump(&raw.b1, sizeof(raw.b1));
  ZLibAppendLump(&raw.b2, sizeof(raw.b2));

  if (node->r.node)
    raw.right = UINT32(node->r.node->index);
  else if (node->r.subsec)
    raw.right = UINT32(node->r.subsec->index | 0x80000000U);
  else
    InternalError("Bad right child in V5 node %d", node->index);

  if (node->l.node)
    raw.left = UINT32(node->l.node->index);
  else if (node->l.subsec)
    raw.left = UINT32(node->l.subsec->index | 0x80000000U);
  else
    InternalError("Bad left child in V5 node %d", node->index);

  ZLibAppendLump(&raw.right, 4);
  ZLibAppendLump(&raw.left,  4);

# if DEBUG_BSP
  PrintDebug("PUT Z NODE %08X  Left %08X  Right %08X  "
    "(%d,%d) -> (%d,%d)\n", node->index, UINT32(raw.left),
    UINT32(raw.right), node->x, node->y,
    node->x + node->dx, node->y + node->dy);
# endif
}

void PutZNodes(node_t *root)
{
  uint32_t raw_num = UINT32(num_nodes);

  ZLibAppendLump(&raw_num, 4);
  DisplayTicker();

  node_cur_index = 0;

  if (root)
    PutOneZNode(root);

  if (node_cur_index != num_nodes)
    InternalError("PutZNodes miscounted (%d != %d)",
      node_cur_index, num_nodes);
}

void SaveZDFormat(node_t *root_node)
{
  lump_t *lump;

  // leave SEGS and SSECTORS empty
  CreateLevelLump("SEGS");
  CreateLevelLump("SSECTORS");

  lump = CreateLevelLump("NODES");
 
  AppendLevelLump(lump, lev_ZD_magic, 4);

  ZLibBeginLump(lump);

  PutZVertices();
  PutZSubsecs();
  PutZSegs();
  PutZNodes(root_node);

  ZLibFinishLump();
}


/* ----- whole-level routines --------------------------- */

//
// LoadLevel
//
void LoadLevel(void)
{
  char *message;

  const char *level_name = GetLevelName();

   // IOANCH: disallow normal nodes
  lev_doing_normal = FALSE;

  // -JL- Identify Hexen mode by presence of BEHAVIOR lump
   // IOANCH: currently Hexen support is not considered
   lev_doing_hexen = FALSE;

  if (lev_doing_normal)
    message = UtilFormat("Building normal and GL nodes on %s%s",
        level_name, lev_doing_hexen ? " (Hexen)" : "");
  else
    message = UtilFormat("Building GL nodes on %s%s",
        level_name, lev_doing_hexen ? " (Hexen)" : "");
 
  lev_doing_hexen |= cur_info->force_hexen;

  DisplaySetBarText(1, message);

  PrintVerbose("\n\n");
  PrintMsg("%s\n", message);
  PrintVerbose("\n");

  UtilFree(message);

  GetVertices();
  GetSectors();
  GetSidedefs();

   // IOANCH warning: Hexen case currently not handled
  if (lev_doing_hexen)
  {
    GetLinedefsHexen();
    GetThingsHexen();
  }
  else
  {
    GetLinedefs();
    GetThings();
  }

  PrintVerbose("Loaded %d vertices, %d sectors, %d sides, %d lines, %d things\n", 
      num_vertices, num_sectors, num_sidedefs, num_linedefs, num_things);

  if (lev_doing_normal)
  {
    // NOTE: order here is critical

    if (cur_info->pack_sides)
      DetectDuplicateSidedefs();

    if (cur_info->merge_vert)
      DetectDuplicateVertices();

    if (!cur_info->no_prune)
      PruneLinedefs();

    // always prune vertices (ignore -noprune), otherwise all the
    // unused vertices from seg splits would keep accumulating.
    PruneVertices();

    if (!cur_info->no_prune)
      PruneSidedefs();

    if (cur_info->prune_sect)
      PruneSectors();
  }
 
  CalculateWallTips();

  if (lev_doing_hexen)
  {
    // -JL- Find sectors containing polyobjs
    DetectPolyobjSectors();
  }

  DetectOverlappingLines();

  if (cur_info->window_fx)
    DetectWindowEffects();
}

//
// FreeLevel
//
void FreeLevel(void)
{
  FreeVertices();
  FreeSidedefs();
  FreeLinedefs();
  FreeSectors();
  FreeThings();
  FreeSegs();
  FreeSubsecs();
  FreeNodes();
  FreeWallTips();
}

//
// PutGLOptions
//
void PutGLOptions(void)
{
  char option_buf[128];

  sprintf(option_buf, "-v%d -factor %d", cur_info->spec_version, cur_info->factor);

  if (cur_info->fast         ) strcat(option_buf, " -f");
  if (cur_info->force_normal ) strcat(option_buf, " -n");
  if (cur_info->merge_vert   ) strcat(option_buf, " -m");
  if (cur_info->pack_sides   ) strcat(option_buf, " -p");
  if (cur_info->prune_sect   ) strcat(option_buf, " -u");
  if (cur_info->skip_self_ref) strcat(option_buf, " -s");
  if (cur_info->window_fx    ) strcat(option_buf, " -y");

  if (cur_info->no_normal) strcat(option_buf, " -xn");
  if (cur_info->no_reject) strcat(option_buf, " -xr");
  if (cur_info->no_prune ) strcat(option_buf, " -xu");

  AddGLTextLine("OPTIONS", option_buf);
}

//
// PutGLChecksum
//
void PutGLChecksum(void)
{
  uint32_t crc;
  lump_t *lump;
  char num_buf[64];

  Adler32_Begin(&crc);

  lump = FindLevelLump("VERTEXES");

  if (lump && lump->length > 0)
    Adler32_AddBlock(&crc, static_cast<uint8_t*>(lump->data), lump->length);

  lump = FindLevelLump("LINEDEFS");

  if (lump && lump->length > 0)
    Adler32_AddBlock(&crc, static_cast<uint8_t*>(lump->data), lump->length);

  Adler32_Finish(&crc);

  sprintf(num_buf, "0x%08x", crc);

  AddGLTextLine("CHECKSUM", num_buf);
}

//
// SaveLevel
//
// IOANCH: this one will not distinguish versions. It will use special Put
// functions for AutoDoom
void SaveLevel(node_t *root_node)
{
  lev_force_v3 = (cur_info->spec_version == 3) ? TRUE : FALSE;
  lev_force_v5 = (cur_info->spec_version == 5) ? TRUE : FALSE;
  
  // Note: RoundOffBspTree will convert the GL vertices in segs to
  // their normal counterparts (pointer change: use normal_dup).

  if (cur_info->spec_version == 1)
    RoundOffBspTree(root_node);

   
   PutAutoDoomVertices();
   PutAutoDoomLinedefs();
   PutAutoDoomSegs();
   PutAutoDoomSubsecs();
   PutAutoDoomNodes(root_node);

}

