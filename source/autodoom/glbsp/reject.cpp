//------------------------------------------------------------------------
// REJECT : Generate the reject table
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "reject.h"
#include "level.h"
#include "node.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


#define DEBUG_REJECT  0


//
// InitReject
//
// Puts each sector into individual groups.
// 
static void InitReject(void)
{
  int i;

  for (i=0; i < glbsp::level::num_sectors; i++)
  {
    glbsp::level::Sector *sec = glbsp::level::LookupSector(i);

    sec->rej_group = i;
    sec->rej_next = sec->rej_prev = sec;
  }
}

//
// GroupSectors
//
// Algorithm: Initially all sectors are in individual groups.  Now we
// scan the linedef list.  For each 2-sectored line, merge the two
// sector groups into one.  That's it !
//
static void GroupSectors(void)
{
  int i;

  for (i=0; i < glbsp::level::num_linedefs; i++)
  {
    glbsp::level::Linedef *line = glbsp::level::LookupLinedef(i);
    glbsp::level::Sector *sec1, *sec2, *tmp;

    if (! line->right || ! line->left)
      continue;
    
    // the standard DOOM engine will not allow sight past lines
    // lacking the TWOSIDED flag, so we can skip them here too.
    if (! line->two_sided)
      continue;

    sec1 = line->right->sector;
    sec2 = line->left->sector;
    
    if (! sec1 || ! sec2 || sec1 == sec2)
      continue;

    // already in the same group ?
    if (sec1->rej_group == sec2->rej_group)
      continue;

    // swap sectors so that the smallest group is added to the biggest
    // group.  This is based on the assumption that sector numbers in
    // wads will generally increase over the set of linedefs, and so
    // (by swapping) we'll tend to add small groups into larger
    // groups, thereby minimising the updates to 'rej_group' fields
    // that is required when merging.

    if (sec1->rej_group > sec2->rej_group)
    {
      tmp = sec1; sec1 = sec2; sec2 = tmp;
    }
    
    // update the group numbers in the second group
    
    sec2->rej_group = sec1->rej_group;

    for (tmp=sec2->rej_next; tmp != sec2; tmp=tmp->rej_next)
      tmp->rej_group = sec1->rej_group;
    
    // merge 'em baby...

    sec1->rej_next->rej_prev = sec2;
    sec2->rej_next->rej_prev = sec1;

    tmp = sec1->rej_next; 
    sec1->rej_next = sec2->rej_next;
    sec2->rej_next = tmp;
  }
}

#if DEBUG_REJECT
static void CountGroups(void)
{
  // Note: this routine is destructive to the group numbers
  
  int i;

  for (i=0; i < glbsp::level::num_sectors; i++)
  {
    glbsp::level::Sector *sec = glbsp::level::LookupSector(i);
    glbsp::level::Sector *tmp;

    int group = sec->rej_group;
    int num = 0;

    if (group < 0)
      continue;

    sec->rej_group = -1;
    num++;

    for (tmp=sec->rej_next; tmp != sec; tmp=tmp->rej_next)
    {
      tmp->rej_group = -1;
      num++;
    }

    PrintDebug("Group %d  Sectors %d\n", group, num);
  }
}
#endif

//
// CreateReject
//
static void CreateReject(uint8_t *matrix)
{
  int view, target;

  for (view=0; view < glbsp::level::num_sectors; view++)
  for (target=0; target < view; target++)
  {
    glbsp::level::Sector *view_sec = glbsp::level::LookupSector(view);
    glbsp::level::Sector *targ_sec = glbsp::level::LookupSector(target);

    int p1, p2;

    if (view_sec->rej_group == targ_sec->rej_group)
      continue;

    // for symmetry, do two bits at a time

    p1 = view * glbsp::level::num_sectors + target;
    p2 = target * glbsp::level::num_sectors + view;
    
    matrix[p1 >> 3] |= (1 << (p1 & 7));
    matrix[p2 >> 3] |= (1 << (p2 & 7));
  }
}

//
// PutReject
//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
void PutReject(void)
{
  int reject_size;
  uint8_t *matrix;
  lump_t *lump;

  DisplayTicker();

  InitReject();
  GroupSectors();
  
  reject_size = (glbsp::level::num_sectors * glbsp::level::num_sectors + 7) / 8;
  matrix = static_cast<uint8_t*>(UtilCalloc(reject_size));

  CreateReject(matrix);

# if DEBUG_REJECT
  CountGroups();
# endif

  lump = CreateLevelLump("REJECT");

  AppendLevelLump(lump, matrix, reject_size);

  PrintVerbose("Added simple reject lump\n");

  UtilFree(matrix);
}

