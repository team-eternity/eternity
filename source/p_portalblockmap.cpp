//
// The Eternity Engine
// Copyright(C) 2018 James Haley, Ioan Chera, et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
// Purpose: linked portal blockmap.
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "p_maputl.h"
#include "p_portalblockmap.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"

PortalBlockmap gPortalBlockmap;
LinePortalRenderBlockmap pLPortalMap;

//
// Initializes the portal blockmap at level start, when map blocks are set.
// Doesn't yet add polyobject portal lines; those are added dynamically.
//
void PortalBlockmap::mapInit()
{
   mInitializing = true;
   mBlocks.clear();
   mLineRef.clear();
   for(int i = 0; i < bmapwidth * bmapheight; ++i)
      mBlocks.add(PODCollection<portalblockentry_t>());
   for(int i = 0; i < numlines; ++i)
   {
      mLineRef.add(PODCollection<lineref_t>());
      linkLine(lines[i]);
   }
   // Now also look in empty blocks for sectors
   for(int i = 0; i < bmapwidth * bmapheight; ++i)
   {
      const int offset = *(blockmap + i);
      const int *list = blockmaplump + offset;
      // FIXME: we really need to store this check in a function
      if((!demo_compatibility && demo_version < 342) || (demo_version >= 342 && skipblstart))
         ++list;
      if(*list >= 0 && *list < numlines)
         continue;
      // We found an empty block now. Just get the sector from the centre.
      const sector_t &sector = *R_PointInSubsector(
         bmaporgx + i % bmapwidth * MAPBLOCKSIZE + MAPBLOCKSIZE / 2,
         bmaporgy + i / bmapwidth * MAPBLOCKSIZE + MAPBLOCKSIZE / 2)->sector;
      checkLinkSector(sector, sector.f_portal, false, i);
      checkLinkSector(sector, sector.c_portal, true, i);
   }
   mInitializing = false;
}

//
// Unlinks a mobile linedef from the portal map. Assumed to have portal.
//
void PortalBlockmap::unlinkLine(const line_t &line)
{
   PODCollection<lineref_t> &linerefs = mLineRef[&line - lines];
   for(const lineref_t &lineref : linerefs)
   {
      PODCollection<portalblockentry_t> &block = mBlocks[lineref.mapindex];
      const portalblockentry_t &lastentry = block.pop();
      if(lineref.blockindex == block.getLength())
         continue;
      if(lastentry.type == portalblocktype_e::line)   // update index to the new upcoming one
         mLineRef[lastentry.line - lines][lastentry.linerefindex].blockindex = lineref.blockindex;
      block[lineref.blockindex] = lastentry; // transfer the last entry for deletion.
   }
   linerefs.makeEmpty();
}

//
// Links a linedef to the portal map. Assumed to have a portal and not to be already in the map.
// Largely copied from P_CreateBlockMap.
// Edge portals not necessary, because they're already part of sector portals.
//
void PortalBlockmap::linkLine(const line_t &line)
{
   const int minx = bmaporgx / FRACUNIT;
   const int miny = bmaporgy / FRACUNIT;

   // starting coordinates
   const int x = (line.v1->x / FRACUNIT) - minx;
   const int y = (line.v1->y / FRACUNIT) - miny;

   // x-y deltas
   int adx = line.dx / FRACUNIT;
   const int dx = adx < 0 ? -1 : 1;
   int ady = line.dy / FRACUNIT;
   int dy = ady < 0 ? -1 : 1;

   // difference in preferring to move across y (>0) 
   // instead of x (<0)
   int diff = !adx ? 1 : !ady ? -1 :
      (((x >> MAPBTOFRAC) << MAPBTOFRAC) +
      (dx > 0 ? MAPBLOCKUNITS - 1 : 0) - x) * (ady = D_abs(ady)) * dx -
         (((y >> MAPBTOFRAC) << MAPBTOFRAC) +
      (dy > 0 ? MAPBLOCKUNITS - 1 : 0) - y) * (adx = D_abs(adx)) * dy;

   // starting block, and pointer to its blocklist structure
   int b = (y >> MAPBTOFRAC) * bmapwidth + (x >> MAPBTOFRAC);

   // ending block
   const int bend = (((line.v2->y / FRACUNIT) - miny) >> MAPBTOFRAC) *
      bmapwidth + (((line.v2->x / FRACUNIT) - minx) >> MAPBTOFRAC);

   // delta for pointer when moving across y
   dy *= bmapwidth;

   // deltas for diff inside the loop
   adx <<= MAPBTOFRAC;
   ady <<= MAPBTOFRAC;

   // Now we simply iterate block-by-block until we reach the end block.
   unsigned tot = bmapwidth * bmapheight;
   while((unsigned int)b < tot)    // failsafe -- should ALWAYS be true
   {
      // At initialization, lines with front and back sectors are also checked.
      // So when initializing is true, do check for linedef portal.
      if(!mInitializing || (line.portal && line.portal->type == R_LINKED))
      {
         portalblockentry_t &entry = mBlocks[b].addNew();
         entry.ldata = &line.portal->data.link;
         entry.type = portalblocktype_e::line;
         entry.line = &line;
         entry.linerefindex = eindex(mLineRef[&line - lines].getLength());
         lineref_t &lineref = mLineRef[&line - lines].addNew();
         lineref.mapindex = b;
         lineref.blockindex = eindex(mBlocks[b].getLength() - 1);
      }

      if(mInitializing)
      {
         checkLinkSector(*line.frontsector, line.frontsector->f_portal, false, b);
         checkLinkSector(*line.frontsector, line.frontsector->c_portal, true, b);
         if(line.backsector)
         {
            checkLinkSector(*line.backsector, line.backsector->f_portal, false, b);
            checkLinkSector(*line.backsector, line.backsector->c_portal, true, b);
         }
      }

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

//
// Links a sector to blockmap. Currently not dynamic and shall only be called at mapinit time.
// Not optimized for gametime.
// Sector NOT assumed to have a linked portals or unadded.
//
void PortalBlockmap::checkLinkSector(const sector_t &sector, const portal_t *portal, bool isceiling,
   int mapindex)
{
   if(!portal || portal->type != R_LINKED)
      return;
   // Look if sector/portal pair already added
   for(const portalblockentry_t &entry : mBlocks[mapindex])
      if(entry.type == portalblocktype_e::sector && entry.sector == &sector &&
         entry.ldata == &portal->data.link)
      {
         return;  // already added
      }

   portalblockentry_t &entry = mBlocks[mapindex].addNew();
   entry.ldata = &portal->data.link;
   entry.type = portalblocktype_e::sector;
   entry.sector = &sector;
   entry.isceiling = isceiling;
}

//
// Initialization per map. Makes PU_LEVEL allocations
//
void LinePortalRenderBlockmap::mapInit()
{
   mMap.clear();
   int numblocks = bmapwidth * bmapheight;
   for(int i = 0; i < numblocks; ++i)
   {
      int offset = blockmap[i];
      const int *list = blockmaplump + offset;
      // MaxW: 2016/02/02: if before 3.42 always skip, skip if all blocklists start w/ 0
      // killough 2/22/98: demo_compatibility check
      // skip 0 starting delimiter -- phares
      if((!demo_compatibility && demo_version < 342) ||
         (demo_version >= 342 && skipblstart))
      {
         list++;
      }

      PODCollection<const line_t *> coll;

      for(; *list != -1; ++list)
      {
         if(*list >= numlines)
            continue;
         const line_t &line = lines[*list];
         if(line.backsector &&
            (line.pflags & PS_PASSABLE ||
            (line.extflags & EX_ML_LOWERPORTAL &&
               line.backsector->f_portal &&
               line.backsector->f_portal->type == R_LINKED) ||
               (line.extflags & EX_ML_UPPERPORTAL &&
                  line.backsector->c_portal &&
                  line.backsector->c_portal->type == R_LINKED)))
         {
            coll.add(&line);
         }
      }

      mMap.add(coll);
   }

   mValids = ecalloctag(decltype(mValids), numlines, sizeof(*mValids), PU_LEVEL,
      nullptr);
}

//
// Does the iteration
//
bool LinePortalRenderBlockmap::iterator(int x, int y, void *data,
   bool(*func)(const line_t &, void *data)) const
{
   if(x < 0 || x >= bmapwidth || y < 0 || y >= bmapheight)
      return true;
   int i = y * bmapwidth + x;
   const PODCollection<const line_t *> coll = mMap[i];
   for(const line_t *line : coll)
   {
      if(mValids[line - lines] == mValidcount)
         continue;
      mValids[line - lines] = mValidcount;
      if(!func(*line, data))
         return false;
   }
   return true;
}


//
// P_BlockHasLinkedPortalLines
//
// ioanch 20160228: return true if block has portalmap 1 or a polyportal
// It's coarse
//
bool P_BlockHasLinkedPortals(int index, bool includesectors)
{
   // safe for overflow
   if(index < 0 || index >= bmapheight * bmapwidth)
      return false;
   if(portalmap[index] & (PMF_LINE |
      (includesectors ? PMF_FLOOR | PMF_CEILING : 0)))
   {
      return true;
   }

   for(const DLListItem<polymaplink_t> *plink = polyblocklinks[index]; plink;
      plink = plink->dllNext)
   {
      if((*plink)->po->hasLinkedPortals)
         return true;
   }
   return false;
}

// EOF

