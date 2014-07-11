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

static const int g_allocBlkNum = 1024;


// per-level variables

boolean_g glbsp::level::lev_doing_normal;
static boolean_g lev_doing_hexen;

static boolean_g lev_force_v3;
static boolean_g lev_force_v5;


#define LEVELARRAY(TYPE, BASEVAR, NUMVAR)  \
    TYPE ** BASEVAR = NULL;  \
    int NUMVAR = 0;


LEVELARRAY(glbsp::level::Vertex,  glbsp::level::lev_vertices,   glbsp::level::num_vertices)
LEVELARRAY(glbsp::level::Linedef, glbsp::level::lev_linedefs,   glbsp::level::num_linedefs)
LEVELARRAY(glbsp::level::Sidedef, glbsp::level::lev_sidedefs,   glbsp::level::num_sidedefs)
LEVELARRAY(glbsp::level::Sector,  glbsp::level::lev_sectors,    glbsp::level::num_sectors)
LEVELARRAY(glbsp::level::Thing,   glbsp::level::lev_things,     glbsp::level::num_things)

static LEVELARRAY(glbsp::level::Seg,     segs,       glbsp::level::num_segs)
static LEVELARRAY(glbsp::level::Subsec,  subsecs,    glbsp::level::num_subsecs)
static LEVELARRAY(glbsp::level::Node,    nodes,      glbsp::level::num_nodes)
static LEVELARRAY(glbsp::level::WallTip,wall_tips,  num_wall_tips)


int glbsp::level::num_normal_vert = 0;
int glbsp::level::num_gl_vert = 0;
int glbsp::level::num_complete_seg = 0;


/* ----- allocation routines ---------------------------- */

#define ALLIGATOR(TYPE, BASEVAR, NUMVAR)  \
{  \
  if ((NUMVAR % g_allocBlkNum) == 0)  \
  {  \
    BASEVAR = static_cast<TYPE**>(UtilRealloc(BASEVAR, (NUMVAR + g_allocBlkNum) *   \
        sizeof(TYPE *)));  \
  }  \
  BASEVAR[NUMVAR] = (TYPE *) UtilCalloc(sizeof(TYPE));  \
  NUMVAR += 1;  \
  return BASEVAR[NUMVAR - 1];  \
}


glbsp::level::Vertex *glbsp::level::NewVertex()
  ALLIGATOR(Vertex, lev_vertices, num_vertices)

glbsp::level::Linedef *glbsp::level::NewLinedef()
  ALLIGATOR(Linedef, lev_linedefs, num_linedefs)

glbsp::level::Sidedef *glbsp::level::NewSidedef()
  ALLIGATOR(Sidedef, lev_sidedefs, num_sidedefs)

glbsp::level::Sector *glbsp::level::NewSector()
  ALLIGATOR(Sector, lev_sectors, num_sectors)

glbsp::level::Thing *glbsp::level::NewThing()
  ALLIGATOR(Thing, lev_things, num_things)

glbsp::level::Seg *glbsp::level::NewSeg()
  ALLIGATOR(Seg, segs, num_segs)

glbsp::level::Subsec *glbsp::level::NewSubsec()
  ALLIGATOR(Subsec, subsecs, num_subsecs)

glbsp::level::Node *glbsp::level::NewNode()
  ALLIGATOR(Node, nodes, num_nodes)

glbsp::level::WallTip *glbsp::level::NewWallTip(void)
  ALLIGATOR(WallTip, wall_tips, num_wall_tips)


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


static void FreeVertices()
  FREEMASON(glbsp::level::Vertex, glbsp::level::lev_vertices, glbsp::level::num_vertices)

static void FreeLinedefs()
  FREEMASON(glbsp::level::Linedef, glbsp::level::lev_linedefs, glbsp::level::num_linedefs)

static void FreeSidedefs()
  FREEMASON(glbsp::level::Sidedef, glbsp::level::lev_sidedefs, glbsp::level::num_sidedefs)

static void FreeSectors()
  FREEMASON(glbsp::level::Sector, glbsp::level::lev_sectors, glbsp::level::num_sectors)

static void FreeThings()
  FREEMASON(glbsp::level::Thing, glbsp::level::lev_things, glbsp::level::num_things)

static void FreeSegs()
  FREEMASON(glbsp::level::Seg, segs, glbsp::level::num_segs)

static void FreeSubsecs()
  FREEMASON(glbsp::level::Subsec, subsecs, glbsp::level::num_subsecs)

static void FreeNodes()
  FREEMASON(glbsp::level::Node, nodes, glbsp::level::num_nodes)

static void FreeWallTips()
  FREEMASON(glbsp::level::WallTip, wall_tips, num_wall_tips)


/* ----- lookup routines ------------------------------ */

#define LOOKERUPPER(BASEVAR, NUMVAR, NAMESTR)  \
{  \
  if (index < 0 || index >= NUMVAR)  \
    FatalError("No such %s number #%d", NAMESTR, index);  \
    \
  return BASEVAR[index];  \
}

glbsp::level::Vertex *glbsp::level::LookupVertex(int index)
  LOOKERUPPER(lev_vertices, num_vertices, "vertex")

glbsp::level::Linedef *glbsp::level::LookupLinedef(int index)
  LOOKERUPPER(lev_linedefs, num_linedefs, "linedef")
  
glbsp::level::Sidedef *glbsp::level::LookupSidedef(int index)
  LOOKERUPPER(lev_sidedefs, num_sidedefs, "sidedef")
  
glbsp::level::Sector *glbsp::level::LookupSector(int index)
  LOOKERUPPER(lev_sectors, num_sectors, "sector")
  
glbsp::level::Thing *glbsp::level::LookupThing(int index)
  LOOKERUPPER(lev_things, num_things, "thing")
  
glbsp::level::Seg *glbsp::level::LookupSeg(int index)
  LOOKERUPPER(segs, num_segs, "seg")
  
glbsp::level::Subsec *glbsp::level::LookupSubsec(int index)
  LOOKERUPPER(subsecs, num_subsecs, "subsector")
  
glbsp::level::Node *glbsp::level::LookupNode(int index)
  LOOKERUPPER(nodes, num_nodes, "node")


/* ----- reading routines ------------------------------ */


//
// GetVertices
//
// IOANCH: modified to read from AutoDoom-GLBSP interface
static void GetVertices()
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
      glbsp::level::Vertex *vert = glbsp::level::NewVertex();
      vert->x = (float_g) x;
      vert->y = (float_g) y;
      vert->index = i++;
   }

  glbsp::level::num_normal_vert = glbsp::level::num_vertices;
  glbsp::level::num_gl_vert = 0;
  glbsp::level::num_complete_seg = 0;
}

//
// GetSectors
//
// IOANCH: AutoDoom
static void GetSectors()
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
      glbsp::level::Sector *sector = glbsp::level::NewSector();
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
static void GetThings()
{
  glbsp::level::Thing *thing;
  DisplayTicker();

# if DEBUG_LOAD
  PrintDebug("GetThings: num = %d\n", count);
# endif

   thing = glbsp::level::NewThing();
   thing->x = thing->y = 0;
   thing->type = 1;
   thing->options = 7;
   thing->index = 0;
}

//
// GetThingsHexen
//
static void GetThingsHexen()
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
    glbsp::level::Thing *thing = glbsp::level::NewThing();

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
static void GetSidedefs()
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
      glbsp::level::Sidedef *side = glbsp::level::NewSidedef();
      side->sector = glbsp::level::LookupSector(index);
      if(side->sector)
         side->sector->ref_count++;
      side->x_offset = side->y_offset = 0;
      memset(side->upper_tex, '-', sizeof(side->upper_tex));
      memset(side->lower_tex, '-', sizeof(side->lower_tex));
      memset(side->mid_tex, '-', sizeof(side->mid_tex));
      
      side->index = i++;
   }
}

static inline glbsp::level::Sidedef *SafeLookupSidedef(uint16_t num)
{
  if (num == 0xFFFF)
    return NULL;

  if ((int)num >= glbsp::level::num_sidedefs && (int16_t)(num) < 0)
    return NULL;

  return glbsp::level::LookupSidedef(num);
}

//
// GetLinedefs
//
static void GetLinedefs()
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
      glbsp::level::Linedef *line;
      glbsp::level::Vertex *start = glbsp::level::LookupVertex(startIdx);
      glbsp::level::Vertex *end = glbsp::level::LookupVertex(endIdx);
      start->ref_count++;
      end->ref_count++;
      
      line = glbsp::level::NewLinedef();
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
static void GetLinedefsHexen()
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
    glbsp::level::Linedef *line;

    glbsp::level::Vertex *start = glbsp::level::LookupVertex(UINT16(raw->start));
    glbsp::level::Vertex *end   = glbsp::level::LookupVertex(UINT16(raw->end));

    start->ref_count++;
    end->ref_count++;

    line = glbsp::level::NewLinedef();

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


static inline int TransformSegDist(const glbsp::level::Seg &seg)
{
  float_g sx = seg.side ? seg.linedef->end->x : seg.linedef->start->x;
  float_g sy = seg.side ? seg.linedef->end->y : seg.linedef->start->y;

  return (int) ceil(UtilComputeDist(seg.start->x - sx, seg.start->y - sy));
}

static inline int TransformAngle(angle_g angle)
{
  int result;
  
  result = (int)(angle * 65536.0 / 360.0);
  
  if (result < 0)
    result += 65536;

  return (result & 0xFFFF);
}

static int SegCompare(const void *p1, const void *p2)
{
  const glbsp::level::Seg *A = ((const glbsp::level::Seg **) p1)[0];
  const glbsp::level::Seg *B = ((const glbsp::level::Seg **) p2)[0];

  if (A->index < 0)
    InternalError("Seg %p never reached a subsector !", A);

  if (B->index < 0)
    InternalError("Seg %p never reached a subsector !", B);

  return (A->index - B->index);
}


/* ----- writing routines ------------------------------ */

// IOANCH: added AutoDoom routines

static inline int VertexIndexAutoDoom(const glbsp::level::Vertex &v)
{
   if(v.index & IS_GL_VERTEX)
      return (v.index & ~IS_GL_VERTEX) + glbsp::level::num_normal_vert;
   return v.index;
}

static void PutAutoDoomVertices()
{
   int i;
   B_GLBSP_CreateVertexArray(glbsp::level::num_vertices);

   for(i = 0; i < glbsp::level::num_vertices; ++i)
   {
     glbsp::level::Vertex *vert = glbsp::level::lev_vertices[i];
      B_GLBSP_PutVertex((double)vert->x, (double)vert->y,
                        VertexIndexAutoDoom(*vert));
   }
}

static void PutAutoDoomLinedefs()
{
   int i;
   B_GLBSP_CreateLineArray(glbsp::level::num_linedefs);

   for (i = 0; i < glbsp::level::num_linedefs; ++i)
   {
      glbsp::level::Linedef *line = glbsp::level::lev_linedefs[i];
      B_GLBSP_PutLine(line->start->index, line->end->index,
                      line->right ? line->right->index : -1,
                      line->left ? line->left->index : -1, i);
   }
}

static inline uint16_t VertexIndex16Bit(const glbsp::level::Vertex *v)
{
  if (v->index & IS_GL_VERTEX)
    return (uint16_t) ((v->index & ~IS_GL_VERTEX) | 0x8000U);

  return (uint16_t) v->index;
}

static inline uint32_t VertexIndex32BitV5(const glbsp::level::Vertex *v)
{
  if (v->index & IS_GL_VERTEX)
    return (uint32_t) ((v->index & ~IS_GL_VERTEX) | 0x80000000U);

  return (uint32_t) v->index;
}

// IOANCH: added autodoom routine

static void PutAutoDoomSegs(void)
{
   int i, count;
   qsort(segs, glbsp::level::num_segs, sizeof(glbsp::level::Seg *), SegCompare);
   B_GLBSP_CreateSegArray(glbsp::level::num_segs);
   for(i = 0, count = 0; i < glbsp::level::num_segs; ++i)
   {
      glbsp::level::Seg *seg = segs[i];
      if(seg->degenerate)
         continue;
      B_GLBSP_PutSegment(VertexIndexAutoDoom(*seg->start),
                         VertexIndexAutoDoom(*seg->end),
                         seg->side,
                         seg->linedef ? seg->linedef->index : -1,
                         seg->partner ? seg->partner->index : -1,
                         count++);
   }
}

static void PutAutoDoomSubsecs()
{
   int i;
   B_GLBSP_CreateSubsectorArray(glbsp::level::num_subsecs);
   for (i = 0; i < glbsp::level::num_subsecs; ++i)
   {
      glbsp::level::Subsec *sub = subsecs[i];
      B_GLBSP_PutSubsector(sub->seg_list->index, sub->seg_count, i);
   }
}

static int node_cur_index;

static void PutOneAutoDoomNode(glbsp::level::Node *node)
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

static void PutAutoDoomNodes(glbsp::level::Node *root)
{
   B_GLBSP_CreateNodeArray(glbsp::level::num_nodes);
   node_cur_index = 0;
   PutOneAutoDoomNode(root);
}

/* ----- whole-level routines --------------------------- */

//
// LoadLevel
//
void glbsp::level::LoadLevel()
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
      analyze::DetectDuplicateSidedefs();

    if (cur_info->merge_vert)
      analyze::DetectDuplicateVertices();

    if (!cur_info->no_prune)
      analyze::PruneLinedefs();

    // always prune vertices (ignore -noprune), otherwise all the
    // unused vertices from seg splits would keep accumulating.
    analyze::PruneVertices();

    if (!cur_info->no_prune)
      analyze::PruneSidedefs();

    if (cur_info->prune_sect)
      analyze::PruneSectors();
  }
 
  analyze::CalculateWallTips();

  if (lev_doing_hexen)
  {
    // -JL- Find sectors containing polyobjs
    analyze::DetectPolyobjSectors();
  }

  analyze::DetectOverlappingLines();

  if (cur_info->window_fx)
    analyze::DetectWindowEffects();
}

//
// FreeLevel
//
void glbsp::level::FreeLevel()
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
// SaveLevel
//
// IOANCH: this one will not distinguish versions. It will use special Put
// functions for AutoDoom
void glbsp::level::SaveLevel(Node *root_node)
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

