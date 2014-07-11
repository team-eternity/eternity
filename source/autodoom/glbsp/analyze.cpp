//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
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

#include "system.h"

#include "z_zone.h"

#include "analyze.h"
#include "blockmap.h"
#include "level.h"
#include "node.h"
#include "reject.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


#define DEBUG_WALLTIPS   0
#define DEBUG_POLYOBJ    0
#define DEBUG_WINDOW_FX  0

#define POLY_BOX_SZ  10

/* ----- polyobj handling ----------------------------- */

static void MarkPolyobjSector(glbsp::level::Sector *sector)
{
  int i;
    
  if (! sector)
    return;

# if DEBUG_POLYOBJ
  PrintDebug("  Marking SECTOR %d\n", sector->index);
# endif

  /* already marked ? */
  if (sector->has_polyobj)
    return;

  /* mark all lines of this sector as precious, to prevent the sector
   * from being split.
   */ 
  sector->has_polyobj = TRUE;

  for (i = 0; i < glbsp::level::num_linedefs; i++)
  {
    glbsp::level::Linedef *L = glbsp::level::lev_linedefs[i];

    if ((L->right && L->right->sector == sector) ||
        (L->left && L->left->sector == sector))
    {
      L->is_precious = TRUE;
    }
  }
}

static void MarkPolyobjPoint(float_g x, float_g y)
{
  int i;
  int inside_count = 0;
 
  float_g best_dist = 999999;
  const glbsp::level::Linedef *best_match = NULL;
  glbsp::level::Sector *sector = NULL;

  float_g x1, y1;
  float_g x2, y2;

  // -AJA- First we handle the "awkward" cases where the polyobj sits
  //       directly on a linedef or even a vertex.  We check all lines
  //       that intersect a small box around the spawn point.

  int bminx = static_cast<int> (x - POLY_BOX_SZ);
  int bminy = static_cast<int> (y - POLY_BOX_SZ);
  int bmaxx = static_cast<int> (x + POLY_BOX_SZ);
  int bmaxy = static_cast<int> (y + POLY_BOX_SZ);

  for (i = 0; i < glbsp::level::num_linedefs; i++)
  {
    const glbsp::level::Linedef *L = glbsp::level::lev_linedefs[i];

    if (glbsp::blockmap::CheckLinedefInsideBox(bminx, bminy, bmaxx, bmaxy,
          static_cast<int> (L->start->x), static_cast<int> (L->start->y),
          static_cast<int> (L->end->x), static_cast<int> (L->end->y)))
    {
#     if DEBUG_POLYOBJ
      PrintDebug("  Touching line was %d\n", L->index);
#     endif

      if (L->left)
        MarkPolyobjSector(L->left->sector);

      if (L->right)
        MarkPolyobjSector(L->right->sector);

      inside_count++;
    }
  }

  if (inside_count > 0)
    return;

  // -AJA- Algorithm is just like in DEU: we cast a line horizontally
  //       from the given (x,y) position and find all linedefs that
  //       intersect it, choosing the one with the closest distance.
  //       If the point is sitting directly on a (two-sided) line,
  //       then we mark the sectors on both sides.

  for (i = 0; i < glbsp::level::num_linedefs; i++)
  {
    const glbsp::level::Linedef *L = glbsp::level::lev_linedefs[i];

    float_g x_cut;

    x1 = L->start->x;  y1 = L->start->y;
    x2 = L->end->x;    y2 = L->end->y;

    /* check vertical range */
    if (fabs(y2 - y1) < DIST_EPSILON)
      continue;

    if ((y > (y1 + DIST_EPSILON) && y > (y2 + DIST_EPSILON)) || 
        (y < (y1 - DIST_EPSILON) && y < (y2 - DIST_EPSILON)))
      continue;

    x_cut = x1 + (x2 - x1) * (y - y1) / (y2 - y1) - x;

    if (fabs(x_cut) < fabs(best_dist))
    {
      /* found a closer linedef */

      best_match = L;
      best_dist = x_cut;
    }
  }

  if (! best_match)
  {
    PrintWarn("Bad polyobj thing at (%1.0f,%1.0f).\n", x, y);
    return;
  }

  y1 = best_match->start->y;
  y2 = best_match->end->y;

# if DEBUG_POLYOBJ
  PrintDebug("  Closest line was %d Y=%1.0f..%1.0f (dist=%1.1f)\n",
      best_match->index, y1, y2, best_dist);
# endif

  /* sanity check: shouldn't be directly on the line */
# if DEBUG_POLYOBJ
  if (fabs(best_dist) < DIST_EPSILON)
  {
    PrintDebug("  Polyobj FAILURE: directly on the line (%d)\n",
        best_match->index);
  }
# endif
 
  /* check orientation of line, to determine which side the polyobj is
   * actually on.
   */
  if ((y1 > y2) == (best_dist > 0))
    sector = best_match->right ? best_match->right->sector : NULL;
  else
    sector = best_match->left ? best_match->left->sector : NULL;

# if DEBUG_POLYOBJ
  PrintDebug("  Sector %d contains the polyobj.\n", 
      sector ? sector->index : -1);
# endif

  if (! sector)
  {
    PrintWarn("Invalid Polyobj thing at (%1.0f,%1.0f).\n", x, y);
    return;
  }

  MarkPolyobjSector(sector);
}

//
// DetectPolyobjSectors
//
// Based on code courtesy of Janis Legzdinsh.
//
void glbsp::analyze::DetectPolyobjSectors()
{
  int i;
  int hexen_style;

  // -JL- There's a conflict between Hexen polyobj thing types and Doom thing
  //      types. In Doom type 3001 is for Imp and 3002 for Demon. To solve
  //      this problem, first we are going through all lines to see if the
  //      level has any polyobjs. If found, we also must detect what polyobj
  //      thing types are used - Hexen ones or ZDoom ones. That's why we
  //      are going through all things searching for ZDoom polyobj thing
  //      types. If any found, we assume that ZDoom polyobj thing types are
  //      used, otherwise Hexen polyobj thing types are used.

  // -JL- First go through all lines to see if level contains any polyobjs
  for (i = 0; i < glbsp::level::num_linedefs; i++)
  {
    const level::Linedef *L = level::lev_linedefs[i];

    if (L->type == HEXTYPE_POLY_START || L->type == HEXTYPE_POLY_EXPLICIT)
      break;
  }

  if (i == glbsp::level::num_linedefs)
  {
    // -JL- No polyobjs in this level
    return;
  }

  // -JL- Detect what polyobj thing types are used - Hexen ones or ZDoom ones
  hexen_style = TRUE;
  
  for (i = 0; i < level::num_things; i++)
  {
    const level::Thing *T = level::LookupThing(i);

    if (T->type == ZDOOM_PO_SPAWN_TYPE || T->type == ZDOOM_PO_SPAWNCRUSH_TYPE)
    {
      // -JL- A ZDoom style polyobj thing found
      hexen_style = FALSE;
      break;
    }
  }

# if DEBUG_POLYOBJ
  PrintDebug("Using %s style polyobj things\n",
      hexen_style ? "HEXEN" : "ZDOOM");
# endif
   
  for (i = 0; i < level::num_things; i++)
  {
    const level::Thing *T = level::LookupThing(i);

    float_g x = (float_g) T->x;
    float_g y = (float_g) T->y;

    // ignore everything except polyobj start spots
    if (hexen_style)
    {
      // -JL- Hexen style polyobj things
      if (T->type != PO_SPAWN_TYPE && T->type != PO_SPAWNCRUSH_TYPE)
        continue;
    }
    else
    {
      // -JL- ZDoom style polyobj things
      if (T->type != ZDOOM_PO_SPAWN_TYPE && T->type != ZDOOM_PO_SPAWNCRUSH_TYPE)
        continue;
    }

#   if DEBUG_POLYOBJ
    PrintDebug("Thing %d at (%1.0f,%1.0f) is a polyobj spawner.\n", i, x, y);
#   endif
 
    MarkPolyobjPoint(x, y);
  }
}

/* ----- analysis routines ----------------------------- */

static bool VertexCompare(uint16_t vert1, uint16_t vert2)
{

  if (vert1 == vert2)
    return 0;

  const glbsp::level::Vertex *A = glbsp::level::lev_vertices[vert1];
  const glbsp::level::Vertex *B = glbsp::level::lev_vertices[vert2];

  if ((int)A->x != (int)B->x)
    return (int)A->x < (int)B->x;
  
  return (int)A->y < (int)B->y;
}

static bool SidedefCompare(uint16_t side1, uint16_t side2)
{
  int comp;

  if (side1 == side2)
    return 0;
  const glbsp::level::Sidedef *A = glbsp::level::lev_sidedefs[side1];
  const glbsp::level::Sidedef *B = glbsp::level::lev_sidedefs[side2];
  
  // don't merge sidedefs on special lines
  if (A->on_special || B->on_special)
    return side1 < side2;

  if (A->sector != B->sector)
  {
    if (A->sector == NULL) return true;
    if (B->sector == NULL) return false;

    return A->sector->index < B->sector->index;
  }

  if (A->x_offset != B->x_offset)
    return A->x_offset < B->x_offset;

  if (A->y_offset != B->y_offset)
    return A->y_offset < B->y_offset;

  // compare textures

  comp = memcmp(A->upper_tex, B->upper_tex, sizeof(A->upper_tex));
  if (comp) return comp < 0;
  
  comp = memcmp(A->lower_tex, B->lower_tex, sizeof(A->lower_tex));
  if (comp) return comp < 0;
  
  comp = memcmp(A->mid_tex, B->mid_tex, sizeof(A->mid_tex));
  if (comp) return comp < 0;

  // sidedefs must be the same
  return false;
}

void glbsp::analyze::DetectDuplicateVertices()
{
  int i;
  uint16_t* array = ecalloc(uint16_t*, level::num_vertices, sizeof(uint16_t));

  DisplayTicker();

  // sort array of indices
  for (i=0; i < level::num_vertices; i++)
    array[i] = i;
  
  std::sort(array, array + level::num_vertices, VertexCompare);

  // now mark them off
  for (i=0; i < level::num_vertices - 1; i++)
  {
    // duplicate ?
    if (!VertexCompare(array[i], array[i+1]) && !VertexCompare(array[i + 1], array[i]))
    {
      level::Vertex *A = level::lev_vertices[array[i]];
      level::Vertex *B = level::lev_vertices[array[i+1]];

      // found a duplicate !
      B->equiv = A->equiv ? A->equiv : A;
    }
  }

  efree(array);
}

void glbsp::analyze::DetectDuplicateSidedefs()
{
  int i;
  auto array = ecalloc(uint16_t*, level::num_sidedefs, sizeof(uint16_t));

  DisplayTicker();

  // sort array of indices
  for (i=0; i < level::num_sidedefs; i++)
    array[i] = i;
  
  std::sort(array, array + level::num_sidedefs, SidedefCompare);
  
  // now mark them off
  for (i=0; i < level::num_sidedefs - 1; i++)
  {
    // duplicate ?
    if (!SidedefCompare(array[i], array[i + 1]) && !SidedefCompare(array[i + 1], array[i]))
    {
      level::Sidedef *A = level::lev_sidedefs[array[i]];
      level::Sidedef *B = level::lev_sidedefs[array[i+1]];

      // found a duplicate !
      B->equiv = A->equiv ? A->equiv : A;
    }
  }

  efree(array);
}

void glbsp::analyze::PruneLinedefs()
{
  int i;
  int new_num;

  DisplayTicker();

  // scan all linedefs
  for (i=0, new_num=0; i < glbsp::level::num_linedefs; i++)
  {
    level::Linedef *L = level::lev_linedefs[i];

    // handle duplicated vertices
    while (L->start->equiv)
    {
      L->start->ref_count--;
      L->start = L->start->equiv;
      L->start->ref_count++;
    }

    while (L->end->equiv)
    {
      L->end->ref_count--;
      L->end = L->end->equiv;
      L->end->ref_count++;
    }

    // handle duplicated sidedefs
    while (L->right && L->right->equiv)
    {
      L->right->ref_count--;
      L->right = L->right->equiv;
      L->right->ref_count++;
    }

    while (L->left && L->left->equiv)
    {
      L->left->ref_count--;
      L->left = L->left->equiv;
      L->left->ref_count++;
    }

    // remove zero length lines
    if (L->zero_len)
    {
      L->start->ref_count--;
      L->end->ref_count--;

      UtilFree(L);
      continue;
    }

    L->index = new_num;
    level::lev_linedefs[new_num++] = L;
  }

  if (new_num < glbsp::level::num_linedefs)
  {
    PrintVerbose("Pruned %d zero-length linedefs\n", glbsp::level::num_linedefs - new_num);
    level::num_linedefs = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Linedefs");
}

void glbsp::analyze::PruneVertices()
{
  int i;
  int new_num;
  int unused = 0;

  DisplayTicker();

  // scan all vertices
  for (i=0, new_num=0; i < level::num_vertices; i++)
  {
    level::Vertex *V = level::lev_vertices[i];

    if (V->ref_count < 0)
      InternalError("Vertex %d ref_count is %d", i, V->ref_count);
    
    if (V->ref_count == 0)
    {
      if (V->equiv == NULL)
        unused++;

      UtilFree(V);
      continue;
    }

    V->index = new_num;
    level::lev_vertices[new_num++] = V;
  }

  if (new_num < level::num_vertices)
  {
    int dup_num = level::num_vertices - new_num - unused;

    if (unused > 0)
      PrintVerbose("Pruned %d unused vertices "
        "(this is normal if the nodes were built before)\n", unused);

    if (dup_num > 0)
      PrintVerbose("Pruned %d duplicate vertices\n", dup_num);

    level::num_vertices = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Vertices");
 
  level::num_normal_vert = level::num_vertices;
}

void glbsp::analyze::PruneSidedefs()
{
  int i;
  int new_num;
  int unused = 0;

  DisplayTicker();

  // scan all sidedefs
  for (i=0, new_num=0; i < level::num_sidedefs; i++)
  {
    level::Sidedef *S = level::lev_sidedefs[i];

    if (S->ref_count < 0)
      InternalError("Sidedef %d ref_count is %d", i, S->ref_count);
    
    if (S->ref_count == 0)
    {
      if (S->sector)
        S->sector->ref_count--;

      if (S->equiv == NULL)
        unused++;

      UtilFree(S);
      continue;
    }

    S->index = new_num;
    level::lev_sidedefs[new_num++] = S;
  }

  if (new_num < level::num_sidedefs)
  {
    int dup_num = level::num_sidedefs - new_num - unused;

    if (unused > 0)
      PrintVerbose("Pruned %d unused sidedefs\n", unused);

    if (dup_num > 0)
      PrintVerbose("Pruned %d duplicate sidedefs\n", dup_num);

    level::num_sidedefs = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Sidedefs");
}

void glbsp::analyze::PruneSectors()
{
  int i;
  int new_num;

  DisplayTicker();

  // scan all sectors
  for (i=0, new_num=0; i < level::num_sectors; i++)
  {
    level::Sector *S = level::lev_sectors[i];

    if (S->ref_count < 0)
      InternalError("Sector %d ref_count is %d", i, S->ref_count);
    
    if (S->ref_count == 0)
    {
      UtilFree(S);
      continue;
    }

    S->index = new_num;
    level::lev_sectors[new_num++] = S;
  }

  if (new_num < level::num_sectors)
  {
    PrintVerbose("Pruned %d unused sectors\n", level::num_sectors - new_num);
    level::num_sectors = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Sectors");
}

static inline int LineVertexLowest(const glbsp::level::Linedef &L)
{
  // returns the "lowest" vertex (normally the left-most, but if the
  // line is vertical, then the bottom-most) => 0 for start, 1 for end.

  return ((int)L.start->x < (int)L.end->x ||
          ((int)L.start->x == (int)L.end->x &&
           (int)L.start->y <  (int)L.end->y)) ? 0 : 1;
}

static bool LineStartCompare(int line1, int line2)
{
  const glbsp::level::Linedef *A = glbsp::level::lev_linedefs[line1];
  const glbsp::level::Linedef *B = glbsp::level::lev_linedefs[line2];

  const glbsp::level::Vertex *C;
  const glbsp::level::Vertex *D;

  if (line1 == line2)
    return false;

  // determine left-most vertex of each line
  C = LineVertexLowest(*A) ? A->end : A->start;
  D = LineVertexLowest(*B) ? B->end : B->start;

  if (static_cast<int>(C->x) != static_cast<int>(D->x))
    return static_cast<int>(C->x) < static_cast<int>(D->x);

  return static_cast<int>(C->y) < static_cast<int>(D->y);
}

static bool LineEndCompare(int line1, int line2)
{
  const glbsp::level::Linedef *A = glbsp::level::lev_linedefs[line1];
  const glbsp::level::Linedef *B = glbsp::level::lev_linedefs[line2];

  const glbsp::level::Vertex *C;
  const glbsp::level::Vertex *D;

  if (line1 == line2)
    return false;

  // determine right-most vertex of each line
  C = LineVertexLowest(*A) ? A->start : A->end;
  D = LineVertexLowest(*B) ? B->start : B->end;

  if (static_cast<int>(C->x) != static_cast<int>(D->x))
    return static_cast<int>(C->x) < static_cast<int>(D->x);

  return static_cast<int>(C->y) < static_cast<int>(D->y);
}

void glbsp::analyze::DetectOverlappingLines()
{
  // Algorithm:
  //   Sort all lines by left-most vertex.
  //   Overlapping lines will then be near each other in this set.
  //   Note: does not detect partially overlapping lines.

  int i;
  auto array = ecalloc(int*, glbsp::level::num_linedefs, sizeof(int));
  int count = 0;

  DisplayTicker();

  // sort array of indices
  for (i=0; i < glbsp::level::num_linedefs; i++)
    array[i] = i;
  
  std::sort(array, array + glbsp::level::num_linedefs, LineStartCompare);
  
  for (i=0; i < glbsp::level::num_linedefs - 1; i++)
  {
    int j;

    for (j = i+1; j < glbsp::level::num_linedefs; j++)
    {
      if (LineStartCompare(array[i], array[j]) || LineStartCompare(array[j], array [i]))
        break;

      if (!LineEndCompare(array[i], array[j]) && !LineEndCompare(array[j], array[i]))
      {
        glbsp::level::Linedef *A = glbsp::level::lev_linedefs[array[i]];
        glbsp::level::Linedef *B = glbsp::level::lev_linedefs[array[j]];

        // found an overlap !
        B->overlap = A->overlap ? A->overlap : A;

        count++;
      }
    }
  }

  if (count > 0)
  {
      PrintVerbose("Detected %d overlapped linedefs\n", count);
  }

  efree(array);
}

static void CountWallTips(const glbsp::level::Vertex &vert, int &one_sided, int &two_sided)
{
    const glbsp::level::WallTip *tip;

    one_sided = 0;
    two_sided = 0;

    for (tip=vert.tip_set; tip; tip=tip->next)
    {
      if (!tip->left || !tip->right)
        one_sided += 1;
      else
        two_sided += 1;
    }
}

static void TestForWindowEffect(glbsp::level::Linedef &L)
{
  // cast a line horizontally or vertically and see what we hit.
  // OUCH, we have to iterate over all linedefs.

  int i;

  float_g mx = (L.start->x + L.end->x) / 2.0;
  float_g my = (L.start->y + L.end->y) / 2.0;

  float_g dx = L.end->x - L.start->x;
  float_g dy = L.end->y - L.start->y;

  int cast_horiz = fabs(dx) < fabs(dy) ? 1 : 0;

  float_g back_dist = 999999.0;
  glbsp::level::Sector * back_open = NULL;
  int back_line = -1;

  float_g front_dist = 999999.0;
  const glbsp::level::Sector * front_open = NULL;
  int front_line = -1;

  for (i=0; i < glbsp::level::num_linedefs; i++)
  {
    glbsp::level::Linedef *N = glbsp::level::lev_linedefs[i];

    float_g dist;
    boolean_g is_front;
    const glbsp::level::Sidedef *hit_side;

    float_g dx2, dy2;

    if (N == &L || N->zero_len || N->overlap)
      continue;

    if (cast_horiz)
    {
      dx2 = N->end->x - N->start->x;
      dy2 = N->end->y - N->start->y;

      if (fabs(dy2) < DIST_EPSILON)
        continue;

      if ((MAX(N->start->y, N->end->y) < my - DIST_EPSILON) ||
          (MIN(N->start->y, N->end->y) > my + DIST_EPSILON))
        continue;

      dist = (N->start->x + (my - N->start->y) * dx2 / dy2) - mx;

      is_front = ((dy > 0) == (dist > 0)) ? TRUE : FALSE;

      dist = fabs(dist);
      if (dist < DIST_EPSILON)  // too close (overlapping lines ?)
        continue;

      hit_side = ((dy > 0) ^ (dy2 > 0) ^ !is_front) ? N->right : N->left;
    }
    else  /* vert */
    {
      dx2 = N->end->x - N->start->x;
      dy2 = N->end->y - N->start->y;

      if (fabs(dx2) < DIST_EPSILON)
        continue;

      if ((MAX(N->start->x, N->end->x) < mx - DIST_EPSILON) ||
          (MIN(N->start->x, N->end->x) > mx + DIST_EPSILON))
        continue;

      dist = (N->start->y + (mx - N->start->x) * dy2 / dx2) - my;

      is_front = ((dx > 0) != (dist > 0)) ? TRUE : FALSE;

      dist = fabs(dist);

      hit_side = ((dx > 0) ^ (dx2 > 0) ^ !is_front) ? N->right : N->left;
    }

    if (dist < DIST_EPSILON)  // too close (overlapping lines ?)
      continue;

    if (is_front)
    {
      if (dist < front_dist)
      {
        front_dist = dist;
        front_open = hit_side ? hit_side->sector : NULL;
        front_line = i;
      }
    }
    else
    {
      if (dist < back_dist)
      {
        back_dist = dist;
        back_open = hit_side ? hit_side->sector : NULL;
        back_line = i;
      }
    }
  }

#if DEBUG_WINDOW_FX
  PrintDebug("back line: %d  back dist: %1.1f  back_open: %s\n",
      back_line, back_dist, back_open ? "OPEN" : "CLOSED");
  PrintDebug("front line: %d  front dist: %1.1f  front_open: %s\n",
      front_line, front_dist, front_open ? "OPEN" : "CLOSED");
#endif

  if (back_open && front_open && L.right->sector == front_open)
  {
    L.window_effect = back_open;
    PrintMiniWarn("Linedef #%d seems to be a One-Sided Window (back faces sector #%d).\n", L.index, back_open->index);
  }
}

void glbsp::analyze::DetectWindowEffects()
{
  // Algorithm:
  //   Scan the linedef list looking for possible candidates,
  //   checking for an odd number of one-sided linedefs connected
  //   to a single vertex.  This idea courtesy of Graham Jackson.

  int i;
  int one_siders;
  int two_siders;

  for (i=0; i < glbsp::level::num_linedefs; i++)
  {
    level::Linedef *L = level::lev_linedefs[i];

    if (L->two_sided || L->zero_len || L->overlap || !L->right)
      continue;

    CountWallTips(*L->start, one_siders, two_siders);

    if ((one_siders % 2) == 1 && (one_siders + two_siders) > 1)
    {
#if DEBUG_WINDOW_FX
      PrintDebug("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
          i, L->start->index);
#endif
      TestForWindowEffect(*L);
      continue;
    }

    CountWallTips(*L->end, one_siders, two_siders);

    if ((one_siders % 2) == 1 && (one_siders + two_siders) > 1)
    {
#if DEBUG_WINDOW_FX
      PrintDebug("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
          i, L->end->index);
#endif
      TestForWindowEffect(*L);
    }
  }
}


/* ----- vertex routines ------------------------------- */

static void VertexAddWallTip(glbsp::level::Vertex &vert, float_g dx, float_g dy, glbsp::level::Sector *left, glbsp::level::Sector *right)
{
  glbsp::level::WallTip *tip = glbsp::level::NewWallTip();
  glbsp::level::WallTip *after;

  tip->angle = UtilComputeAngle(dx, dy);
  tip->left  = left;
  tip->right = right;

  // find the correct place (order is increasing angle)
  for (after=vert.tip_set; after && after->next; after=after->next)
  { }

  while (after && tip->angle + ANG_EPSILON < after->angle) 
    after = after->prev;
  
  // link it in
  tip->next = after ? after->next : vert.tip_set;
  tip->prev = after;

  if (after)
  {
    if (after->next)
      after->next->prev = tip;
    
    after->next = tip;
  }
  else
  {
    if (vert.tip_set)
      vert.tip_set->prev = tip;
    
    vert.tip_set = tip;
  }
}

void glbsp::analyze::CalculateWallTips(void)
{
  int i;

  DisplayTicker();

  for (i=0; i < glbsp::level::num_linedefs; i++)
  {
    level::Linedef *line = level::lev_linedefs[i];
    float_g x1, y1, x2, y2;
    level::Sector *left, *right;

    if (line->self_ref && cur_info->skip_self_ref)
      continue;

    x1 = line->start->x;
    y1 = line->start->y;
    x2 = line->end->x;
    y2 = line->end->y;

    left  = (line->left)  ? line->left->sector  : NULL;
    right = (line->right) ? line->right->sector : NULL;
    
    VertexAddWallTip(*line->start, x2-x1, y2-y1, left, right);
    VertexAddWallTip(*line->end,   x1-x2, y1-y2, right, left);
  }
 
# if DEBUG_WALLTIPS
  for (i=0; i < num_vertices; i++)
  {
    level::Vertex *vert = level::LookupVertex(i);
    level::WallTip *tip;

    PrintDebug("WallTips for vertex %d:\n", i);

    for (tip=vert->tip_set; tip; tip=tip->next)
    {
      PrintDebug("  Angle=%1.1f left=%d right=%d\n", tip->angle,
        tip->left ? tip->left->index : -1,
        tip->right ? tip->right->index : -1);
    }
  }
# endif
}

//
// NewVertexFromSplitSeg
//
glbsp::level::Vertex *glbsp::analyze::NewVertexFromSplitSeg(const level::Seg &seg, float_g x, float_g y)
{
  level::Vertex *vert = level::NewVertex();

  vert->x = x;
  vert->y = y;

  vert->ref_count = seg.partner ? 4 : 2;

  if (level::lev_doing_normal && cur_info->spec_version == 1)
  {
    vert->index = level::num_normal_vert;
    level::num_normal_vert++;
  }
  else
  {
    vert->index = level::num_gl_vert | IS_GL_VERTEX;
    level::num_gl_vert++;
  }

  // compute wall_tip info

  VertexAddWallTip(*vert, -seg.pdx, -seg.pdy, seg.sector,
      seg.partner ? seg.partner->sector : NULL);

  VertexAddWallTip(*vert, seg.pdx, seg.pdy,
      seg.partner ? seg.partner->sector : NULL, seg.sector);

  // create a duplex vertex if needed

  if (level::lev_doing_normal && cur_info->spec_version != 1)
  {
    vert->normal_dup = level::NewVertex();

    vert->normal_dup->x = x;
    vert->normal_dup->y = y;
    vert->normal_dup->ref_count = vert->ref_count;

    vert->normal_dup->index = level::num_normal_vert;
    level::num_normal_vert++;
  }

  return vert;
}

//
// NewVertexDegenerate
//
glbsp::level::Vertex *glbsp::analyze::NewVertexDegenerate(const level::Vertex &start, const level::Vertex &end)
{
  float_g dx = end.x - start.x;
  float_g dy = end.y - start.y;

  float_g dlen = UtilComputeDist(dx, dy);

  level::Vertex *vert = level::NewVertex();

  vert->ref_count = start.ref_count;

  if (level::lev_doing_normal)
  {
    vert->index = level::num_normal_vert;
    level::num_normal_vert++;
  }
  else
  {
    vert->index = level::num_gl_vert | IS_GL_VERTEX;
    level::num_gl_vert++;
  }

  // compute new coordinates

  vert->x = start.x;
  vert->y = start.x;

  if (dlen == 0)
    InternalError("NewVertexDegenerate: bad delta !");

  dx /= dlen;
  dy /= dlen;

  while (I_ROUND(vert->x) == I_ROUND(start.x) &&
         I_ROUND(vert->y) == I_ROUND(start.y))
  {
    vert->x += dx;
    vert->y += dy;
  }

  return vert;
}

//
// VertexCheckOpen
//
glbsp::level::Sector * glbsp::analyze::VertexCheckOpen(const level::Vertex &vert, float_g dx, float_g dy)
{
  const level::WallTip *tip;

  angle_g angle = UtilComputeAngle(dx, dy);

  // first check whether there's a wall_tip that lies in the exact
  // direction of the given direction (which is relative to the
  // vertex).

  for (tip=vert.tip_set; tip; tip=tip->next)
  {
    if (fabs(tip->angle - angle) < ANG_EPSILON ||
        fabs(tip->angle - angle) > (360.0 - ANG_EPSILON))
    {
      // yes, found one
      return NULL;
    }
  }

  // OK, now just find the first wall_tip whose angle is greater than
  // the angle we're interested in.  Therefore we'll be on the RIGHT
  // side of that wall_tip.

  for (tip=vert.tip_set; tip; tip=tip->next)
  {
    if (angle + ANG_EPSILON < tip->angle)
    {
      // found it
      return tip->right;
    }

    if (! tip->next)
    {
      // no more tips, thus we must be on the LEFT side of the tip
      // with the largest angle.

      return tip->left;
    }
  }
 
  InternalError("Vertex %d has no tips !", vert.index);
  return FALSE;
}

