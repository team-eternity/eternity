// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Bot helper map
//      Contains an equivalent map of the real map in which an actor with 0
//      width can fit in the same space as one with width on the real map
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "b_botmap.h"
#include "b_botmaptemp.h"
#include "b_compression.h"
#include "b_glbsp.h"
#include "b_msector.h"
#include "../c_io.h"
#include "../d_files.h"
#include "../doomstat.h"
#include "../e_player.h"
#include "../ev_specials.h"
#include "../m_bbox.h"
#include "../m_buffer.h"
#include "../m_hash.h"
#include "../m_utils.h"
#include "../m_qstr.h"
#include "../p_map.h"
#include "../p_maputl.h"
#include "../p_setup.h"
#include "../r_defs.h"
#include "../r_main.h"
#include "../r_state.h"
#include "../v_misc.h"

BotMap *botMap;

bool BotMap::demoPlayingFlag;

const int CACHE_BUFFER_SIZE = 16384;//512 * 1024;

static const char* const BOTMAP_CACHE_MAGIC = "BOTMAP10";

//
// String representation
//
qstring BotMap::Line::toString() const
{
   qstring result("BotMapLine(v=(");
   result << M_FixedToDouble(v[0]->x) << " " << M_FixedToDouble(v[0]->y) << " ";
   result << M_FixedToDouble(v[1]->x) << " " << M_FixedToDouble(v[1]->y) << ")";
   result << " specline=" << (specline ? eindex(specline - ::lines) : -1);
   result << " msec=(" << (msec[0] ? msec[0]->toString().constPtr() : "null");
   result << " " << (msec[1] ? msec[1]->toString().constPtr() : "null") << ")";
   return result;
}

//
// BotMap::getTouchedBlocks
//
// Lists the blocks the line touches into a collection, code from P_Setup.cpp,
// P_CreateBlockmap
//
void BotMap::getTouchedBlocks(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                              const std::function<void(int)> &func) const
{
   fixed_t minx = botMap->bMapOrgX >> FRACBITS;
   fixed_t miny = botMap->bMapOrgY >> FRACBITS;
   unsigned tot = botMap->bMapWidth * botMap->bMapHeight;
   int x = (x1 >> FRACBITS) - minx;
   int y = (y1 >> FRACBITS) - miny;
   
   // x-y deltas
   int adx = (x2 - x1) >> FRACBITS, dx = adx < 0 ? -1 : 1;
   int ady = (y2 - y1) >> FRACBITS, dy = ady < 0 ? -1 : 1;
   
   // difference in preferring to move across y (>0)
   // instead of x (<0)
   int diff = !adx ? 1 : !ady ? -1 :
   (((x / BOTMAPBLOCKUNITS) * BOTMAPBLOCKUNITS) +
    (dx > 0 ? BOTMAPBLOCKUNITS-1 : 0) - x) * (ady = D_abs(ady)) * dx -
   (((y / BOTMAPBLOCKUNITS) * BOTMAPBLOCKUNITS) +
    (dy > 0 ? BOTMAPBLOCKUNITS-1 : 0) - y) * (adx = D_abs(adx)) * dy;
   
   // starting block, and pointer to its blocklist structure
   int b = (y / BOTMAPBLOCKUNITS) * botMap->bMapWidth + (x / BOTMAPBLOCKUNITS);
   
   // ending block
   int bend = (((y2 >> FRACBITS) - miny) / BOTMAPBLOCKUNITS) *
   botMap->bMapWidth + (((x2 >> FRACBITS) - minx) / BOTMAPBLOCKUNITS);
   
   // delta for pointer when moving across y
   dy *= botMap->bMapWidth;
   
   // deltas for diff inside the loop
   adx *= BOTMAPBLOCKUNITS;
   ady *= BOTMAPBLOCKUNITS;
   
   // Now we simply iterate block-by-block until we reach the end block.
   while((unsigned int) b < tot)    // failsafe -- should ALWAYS be true
   {
      func(b);
      
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
// BotMap::getBoxTouchedBlocks
//
// Obtains blocks touched by a rectangular box
//
void BotMap::getBoxTouchedBlocks(fixed_t top, fixed_t bottom,
                                  fixed_t left, fixed_t right,
                                  const std::function<void(int b)> &func) const
{
   
   int xl, xh, yl, yh, bx, by;
   
   xl = (left - bMapOrgX) / BOTMAPBLOCKSIZE;
   xh = (right - bMapOrgX) / BOTMAPBLOCKSIZE;
   yl = (bottom - bMapOrgY) / BOTMAPBLOCKSIZE;
   yh = (top - bMapOrgY) / BOTMAPBLOCKSIZE;
   
   for (bx = xl; bx <= xh; ++bx)
   {
      for (by = yl; by <= yh; ++by)
      {
         func(bx + by * bMapWidth);
      }
   }
}



BotMap::Subsec &BotMap::pointInSubsector(v2fixed_t pos) const
{
   int nodenum = this->numnodes - 1;
   while(!(nodenum & NF_SUBSECTOR))
      nodenum = this->nodes[nodenum].child[pointOnSide(pos, this->nodes[nodenum])];
   return ssectors[nodenum & ~NF_SUBSECTOR];
}

//
// Returns the corner farthest from a source point
//
v2fixed_t BotMap::Subsec::farthestCorner(v2fixed_t fsource) const
{
   const v2double_t source = {M_FixedToDouble(fsource.x), M_FixedToDouble(fsource.y)};
   v2double_t target;
   double maxdist = 0, dist;
   int maxi;
   for(int i = 0; i < nsegs; ++i)
   {
      target.x = M_FixedToDouble(segs[i].v[0]->x);
      target.y = M_FixedToDouble(segs[i].v[0]->y);
      dist = pow(target.x - source.x, 2) + pow(target.y - source.y, 2);
      if(dist > maxdist)
      {
         maxdist = dist;
         maxi = i;
      }
   }
   v2fixed_t ret;
   ret.x = segs[maxi].v[0]->x;
   ret.y = segs[maxi].v[0]->y;
   return ret;
}

int BotMap::pointOnSide(v2fixed_t pos, const Node &node) const
{
   if(!node.dx)
      return pos.x <= node.x ? node.dy > 0 : node.dy < 0;
   
   if(!node.dy)
      return pos.y <= node.y ? node.dx < 0 : node.dx > 0;

   pos -= node;
   
   // Try to quickly decide by looking at sign bits.
   if((node.dy ^ node.dx ^ pos.x ^ pos.y) < 0)
      return (node.dy ^ pos.x) < 0;  // (left is negative)
   // IOANCH: fixed an underflow problem happening when it was FixedMul with
   // >>FRACBITS on a factor
   return FixedMul64(pos.y, node.dx) >= FixedMul64(node.dy, pos.x);
}

//
// BotMap::unsetThingPosition
//
// Unsets a thing's position
//
void BotMap::unsetThingPosition(const Mobj *thing)
{
   if(mobjSecMap.count(thing))
   {
      for (auto it = mobjSecMap[thing].begin(); 
         it != mobjSecMap[thing].end(); 
         ++it) 
      {
         (*it)->mobjlist.erase(thing);
      }
      mobjSecMap[thing].makeEmpty();
   }
}

//
// BotMap::setThingPosition
//
// Sets a thing's position
//
void BotMap::setThingPosition(const Mobj *thing)
{
   fixed_t rad = thing->radius + botMap->radius;
   
   fixed_t top = thing->y + rad;
   fixed_t bottom = thing->y - rad;
   fixed_t left = thing->x - rad;
   fixed_t right = thing->x + rad;
   
   bool foundlines = false;
   getBoxTouchedBlocks(top, bottom, left, right, [&](int b)->void
   {
       if (b < 0 || b >= (int)segBlocks.getLength())
           return;
       // Iterate through all segs in the caught block
       for (auto it = segBlocks[b].begin();
           it != segBlocks[b].end(); ++it)
       {
           Seg *sg = *it;
           if (!sg->owner)
               continue;
           if (right <= sg->bbox[BOXLEFT] ||
               left >= sg->bbox[BOXRIGHT] ||
               top <= sg->bbox[BOXBOTTOM] ||
               bottom >= sg->bbox[BOXTOP])
           {
               continue;
           }
           if (B_BoxOnLineSide(top, bottom, left, right,
               sg->v[0]->x, sg->v[0]->y,
               sg->dx,
               sg->dy) == -1)
           {

               // if seg crosses thing bbox, add it
               mobjSecMap[thing].add(sg->owner);
               sg->owner->mobjlist.insert(thing);
               foundlines = true;
           }
       }
   });
   
   if(!foundlines)
   {
      // not found any intersections, now it's time to set the pointInSubsector
      Subsec &thingSec = pointInSubsector(v2fixed_t(*thing));
      thingSec.mobjlist.insert(thing);
      mobjSecMap[thing].add(&thingSec);
   }
}

//
// BotMap::operator new
//
// Overloaded to support user
//
void *BotMap::operator new(size_t size, int tag, BotMap **user)
{
   return ZoneObject::operator new(size, tag, (void **)user);
}

//
// ZoneObject::operator delete
//
// Needed
//
void BotMap::operator delete (void *p)
{
   ZoneObject::operator delete(p);
}

//
// ZoneObject::operator delete
//
// Needed
//
void BotMap::operator delete (void *p, int a, BotMap ** b)
{
   ZoneObject::operator delete(p, a, (void**)b);
}

//
// BotMap::canPass
//
// Returns true if one can pass from s1's metasec to s2's
//
bool BotMap::canPass(const MetaSector *ms1, const MetaSector *ms2, fixed_t height) const
{
   if (ms1 == ms2)
      return true;
   
   fixed_t flh[2] = { ms1->getFloorHeight(), ms2->getFloorHeight() };
   fixed_t alh[2] = { ms1->getAltFloorHeight(), ms2->getAltFloorHeight() };
   fixed_t clh[2] = { ms1->getCeilingHeight(), ms2->getCeilingHeight() };
   
   if((flh[1] == D_MAXINT && alh[1] == D_MAXINT) || clh[1] == D_MININT)
      return false;
   if(flh[1] - flh[0] > 24 * FRACUNIT && alh[1] - alh[0] > 24 * FRACUNIT)
      return false;
   if(clh[1] - flh[0] < height && clh[1] - alh[0] < height)
      return false;
   if(clh[1] - flh[1] < height && clh[1] - alh[1] < height)
      return false;
   if(clh[0] - flh[1] < height && clh[0] - alh[1] < height)
      return false;
   
   return true;
   
}
bool BotMap::canPass(const BSubsec &s1, const BSubsec &s2, fixed_t height) const
{
   const MetaSector* ms1 = s1.msector, *ms2 = s2.msector;

   return canPass(ms1, ms2, height);
}
bool BotMap::canPassNow(const MetaSector *ms1, const MetaSector *ms2,
                        fixed_t height) const
{
   if (ms1 == ms2)
      return true;
   
   fixed_t flh[2] = { ms1->getFloorHeight(), ms2->getFloorHeight() };
   fixed_t clh[2] = { ms1->getCeilingHeight(), ms2->getCeilingHeight() };
   
   if(flh[1] == D_MAXINT || clh[1] == D_MININT)
      return false;
   if(flh[1] - flh[0] > 24 * FRACUNIT)
      return false;
   if(clh[1] - flh[0] < height)
      return false;
   if(clh[1] - flh[1] < height)
      return false;
   if(clh[0] - flh[1] < height)
      return false;
   
   return true;
}
bool BotMap::canPassNow(const BSubsec &s1, const BSubsec &s2, fixed_t height) const
{
   const MetaSector* ms1 = s1.msector, *ms2 = s2.msector;

   return canPassNow(ms1, ms2, height);
}

//
// B_setMobjPositions
//
// Makes sure to have all mobjs registered on the blockmap
//
static void B_setMobjPositions()
{
   for(Thinker *th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;
      
      if(!(mo = thinker_cast<Mobj *>(th)))
         continue;
      
      botMap->setThingPosition(mo);
      
   }
}

//
// B_setSpecLinePositions
//
// Records all special lines on the bot map, so they can be detected by the pathfinder
//
static void B_setSpecLinePositions()
{
//   fixed_t addx, addy, len;
//   int n, j;
//   fixed_t lenx, leny;
   
   BSubsec *ss;
   botMap->gunLines.makeEmpty();
   
   for (int i = 0; i < numlines; ++i)
   {
      const line_t &line = lines[i];
      const ev_action_t *action = EV_ActionForSpecial(line.special);
      if(action)
      {
         // TODO: also take into account generalized actions
         if(action->type == &S1ActionType || action->type == &SRActionType
                 || action->type == &DRActionType)
         {
            v2fixed_t mid = (v2fixed_t(*line.v1) + *line.v2) / 2;
            angle_t ang = v2fixed_t(line.dx, line.dy).angle() - ANG90;

            mid += v2fixed_t::polar(USERANGE / 2, ang);

            ss = &botMap->pointInSubsector(mid);
            ss->linelist[&line] = USERANGE / 2;

            mid += v2fixed_t::polar(USERANGE / 2, ang);

            ss = &botMap->pointInSubsector(mid);
            if(!ss->linelist.count(&line))
               ss->linelist[&line] = USERANGE;
         }
         else if(action->type == &G1ActionType || action->type == &GRActionType)
            botMap->gunLines.add(&line);
      }
      
   }
}

//
// BotMap::createBlockMap
//
// Creates the blockmap
//
void BotMap::createBlockMap()
{
	// derive from level blockmap's size
	fixed_t extend = radius / 2 + 8 * FRACUNIT;
	bMapOrgX = bmaporgx - extend;
	bMapOrgY = bmaporgy - extend;
	// assume bmapwidth is exactly level width
	bMapWidth = (2 * extend + bmapwidth * MAPBLOCKSIZE)
		/ BOTMAPBLOCKSIZE + 1;
	bMapHeight = (2 * extend + bmapheight * MAPBLOCKSIZE)
		/ BOTMAPBLOCKSIZE + 1;
	// now, how can i forget about the level blockmap sizes?

	int bsz = botMap->bMapWidth * botMap->bMapHeight;

	for (int i = 0; i < bsz; ++i)
	{
		// Create botmap finals
		segBlocks.add();
      lineBlocks.add();
	}
}

//
// B_buildBotMapFromScratch
//
// Builds bot map from scratch and saves result to GZip-compressed data
//
static void B_buildTempBotMapFromScratch(fixed_t radius, const char *digest)
{
	// First create the transient bot map
	tempBotMap = new TempBotMap;

	// Generate it
	B_BEGIN_CLOCK
	tempBotMap->generateForRadius(radius);
	B_MEASURE_CLOCK(generateForRadius)
	
	// Move the metasector list to the final bot map
   for (DLListItem<MetaSector> *item = tempBotMap->getMsecList().head; item; item = item->dllNext)
   {
      botMap->metasectors.add(item->dllObject);
   }
	
	// Feed it into GLBSP. botMap will get in turn all needed data
	B_NEW_CLOCK
	B_GLBSP_Start();
	B_MEASURE_CLOCK(B_GLBSP_Start)

	// Prevent tempBotMap from crashing
	tempBotMap->getMsecList().head = nullptr;
	
	// Delete the temp. map
	B_NEW_CLOCK
	delete tempBotMap;
	B_MEASURE_CLOCK(deleteTempBotMap)

}

void BotMap::getAllLivingMonsters()
{
    Thinker* th;
    const Mobj* mo;
    livingMonsters.makeEmpty();
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        mo = thinker_cast<const Mobj*>(th);
        if (mo && mo->isBotTargettable())
        {
            livingMonsters.add(mo);
        }
    }
}

void BotMap::SpecialIsDoor(int n, SectorTrait::DoorInfo& door, const line_t* line)
{
    VanillaLineSpecial vls = (VanillaLineSpecial)n;
    door.lock = EV_LockDefIDForSpecial(n);
    door.valid = false;
    switch (vls)
    {
       case VLS_D1DoorBlazeOpen:
       case VLS_D1OpenDoor:
       case VLS_DRDoorBlazeRaise:
       case VLS_DRRaiseDoor:
       case VLS_D1OpenDoorBlue:
       case VLS_D1OpenDoorRed:
       case VLS_D1OpenDoorYellow:
       case VLS_DRRaiseDoorBlue:
       case VLS_DRRaiseDoorRed:
       case VLS_DRRaiseDoorYellow:
           door.valid = true;
           break;
       case VLS_SRDoorBlazeOpen:
       case VLS_SROpenDoor:
       case VLS_SRDoorBlazeRaise:
       case VLS_SRRaiseDoor:
       case VLS_SRDoorBlazeOpenBlue:
       case VLS_SRDoorBlazeOpenRed:
       case VLS_SRDoorBlazeOpenYellow:
          if(line->backsector && line->args[0] == line->backsector->tag)
             door.valid = true;
          break;
       default:
           door.valid = false;
           break;
    }
}

void BotMap::getDoorSectors()
{
   delete[] sectorFlags;
   sectorFlags = new SectorTrait[::numsectors];

    // A sector is a door if:
    // - it has at least two two-sided lines
    // - they all face outside
    // - they all are DR type
    // - they all have the same lockID

   // Check for doors
   PODCollection<const line_t *> doorlines;
    for (int i = 0; i < ::numsectors; ++i)
    {
       const sector_t &sec = ::sectors[i];
       doorlines.makeEmpty();

       SectorTrait::DoorInfo door = {}, defdoor = {};

        bool typeset = false;
        for (int j = 0; j < sec.linecount; ++j)
        {
            const line_t &line = *sec.lines[j];
            if (!line.backsector)
                continue;
            
            if (line.backsector != &sec)
            {
                // fail
                goto notDoor;
            }
            SpecialIsDoor(line.special, door, &line);
            if (!typeset)
            {
                if (!door.valid)
                    goto notDoor;
                typeset = true;
                defdoor = door;
            }
            else if (memcmp(&defdoor, &door, sizeof(door)))
            {
                goto notDoor;
            }

           // line seems valid
           doorlines.add(&line);
        }
        // okay
        sectorFlags[i].door = door;
       sectorFlags[i].doorlines = std::move(doorlines);
    notDoor:
        ;
    }
}

//
// BotMap::cacheToFile
//
// Stores the contents to file, for caching, for later loading
//
void BotMap::cacheToFile(const char* path) const
{
    // Sanity check
   B_Log("BotMap: saving to cache %s", path);

    GZCompression file;
    file.setThrowing(true);
    if (!file.createFile(path, CACHE_BUFFER_SIZE, BufferedFileBase::LENDIAN, CompressLevel_Space))
    {
        C_Printf(FC_ERROR "WARNING: can't create bot map cache file at %s\n", path);
        return;
    }

    // now we write it
   int total = static_cast<int>(numverts + metasectors.getLength() * 2 +
                                numlines + segs.getLength() +
                                ssectors.getLength() + numnodes +
                                segBlocks.getLength() + lineBlocks.getLength());
   int progress = 0;

   V_SetLoading(total, "Saving");
    
    try
    {
        // First, write the version magic
        file.write(BOTMAP_CACHE_MAGIC, strlen(BOTMAP_CACHE_MAGIC));

        // vertices
        file.writeSint32(numverts);
        int i;
        for (i = 0; i < numverts; ++i)
        {
           V_LoadingUpdateTicked(++progress);
            file.writeSint32(vertices[i].x);
            file.writeSint32(vertices[i].y);
        }

        // Build a metasector->index map
        msecIndexMap.clear();
        i = 0;
        for (const auto item : metasectors)
        {
           V_LoadingUpdateTicked(++progress);
            msecIndexMap[item] = i++;
        }

        // metasectors
        file.writeUint32((uint32_t)metasectors.getLength());
       int nullMSecIndex = -1;
       i = 0;
        for (const auto msec : metasectors)
        {
           V_LoadingUpdateTicked(++progress);
            msec->writeToFile(file);
           if(msec == nullMSec)
              nullMSecIndex = i;
           ++i;
        }
       file.writeSint32(nullMSecIndex);

        // lines
        file.writeSint32(numlines);
        for (i = 0; i < numlines; ++i)
        {
           V_LoadingUpdateTicked(++progress);
            file.writeSint32(lines[i].v[0] ? (int32_t)(lines[i].v[0] - vertices) : -1);
            file.writeSint32(lines[i].v[1] ? (int32_t)(lines[i].v[1] - vertices) : -1);
            file.writeSint32(lines[i].msec[0] ? msecIndexMap[lines[i].msec[0]] : -1);
            file.writeSint32(lines[i].msec[1] ? msecIndexMap[lines[i].msec[1]] : -1);
            file.writeSint32(lines[i].specline ? (int32_t)(lines[i].specline - ::lines) : -1);
        }

       file.writeSint32(bMapOrgX);
       file.writeSint32(bMapOrgY);
       file.writeSint32(bMapWidth);
       file.writeSint32(bMapHeight);

        // segs
        file.writeUint32((uint32_t)segs.getLength());
        for (const auto& seg : segs)
        {
           V_LoadingUpdateTicked(++progress);
            file.writeSint32(seg.v[0] ? (int32_t)(seg.v[0] - vertices) : -1);
            file.writeSint32(seg.v[1] ? (int32_t)(seg.v[1] - vertices) : -1);
            file.writeSint32(seg.dx);
            file.writeSint32(seg.dy);
            file.writeSint32(seg.ln ? (int32_t)(seg.ln - lines) : -1);
            file.writeUint8(seg.isback);
            file.writeSint32(seg.partner ? (int32_t)(seg.partner - &segs[0]) : -1);
            file.writeSint32(seg.bbox[0]);
            file.writeSint32(seg.bbox[1]);
            file.writeSint32(seg.bbox[2]);
            file.writeSint32(seg.bbox[3]);
            file.writeSint32(seg.mid.x);
            file.writeSint32(seg.mid.y);
            file.writeSint32(seg.owner ? (int32_t)(seg.owner - &ssectors[0]) : -1);
            file.writeUint32((uint32_t)seg.blocklist.getLength());
            for (auto j : seg.blocklist)
            {
                file.writeSint32(j);
            }
        }

        // ssectors
        file.writeUint32((uint32_t)ssectors.getLength());
        for (const auto& ssector : ssectors)
        {
           V_LoadingUpdateTicked(++progress);
            file.writeSint32(ssector.segs ? (int32_t)(ssector.segs - &segs[0]) : -1);
            file.writeSint32(ssector.msector ? msecIndexMap[ssector.msector] : -1);
            file.writeSint32(ssector.nsegs);
            // mobjlist dynamic
            // linelist dynamic
            file.writeSint32(ssector.mid.x);
            file.writeSint32(ssector.mid.y);
            file.writeUint32((uint32_t)ssector.neighs.getLength());
            for (const auto& neigh : ssector.neighs)
            {
                file.writeSint32(neigh.otherss ? (int32_t)(neigh.otherss - &ssectors[0]) : -1);
                file.writeSint32(neigh.myss ? (int32_t)(neigh.myss - &ssectors[0]) : -1);
                file.writeSint32(neigh.v.x);
                file.writeSint32(neigh.v.y);
                file.writeSint32(neigh.d.x);
                file.writeSint32(neigh.d.y);
                file.writeSint32(neigh.line ? (int32_t)(neigh.line - lines) : -1);
                file.writeSint32(neigh.dist);
            }
        }

        // nodes
        file.writeSint32(numnodes);
        for (i = 0; i < numnodes; ++i)
        {
           V_LoadingUpdateTicked(++progress);
            file.writeSint32(nodes[i].x);
            file.writeSint32(nodes[i].y);
            file.writeSint32(nodes[i].dx);
            file.writeSint32(nodes[i].dy);
            file.writeSint32(nodes[i].child[0]);
            file.writeSint32(nodes[i].child[1]);
        }

        file.writeUint32((uint32_t)segBlocks.getLength());
        for (const auto& coll : segBlocks)
        {
           V_LoadingUpdateTicked(++progress);
            file.writeUint32((uint32_t)coll.getLength());
            for (const auto pseg : coll)
            {
                file.writeSint32(pseg ? (int32_t)(pseg - &segs[0]) : -1);
            }
        }

       // From 04 onwards
       file.writeUint32((uint32_t)lineBlocks.getLength());
       for(const auto &coll : lineBlocks)
       {
          V_LoadingUpdateTicked(++progress);
          file.writeUint32((uint32_t)coll.getLength());
          for(const auto pline : coll)
          {
             file.writeSint32(pline ? (int32_t)(pline - &lines[0]) : -1);
          }
       }
       // end

        file.writeSint32(radius);

        // mobjSecMap dynamic
        // lineSecMap dynamic
        // livingMonsters dynamic
        // thrownProjectiles dynamic
        // sectorFlags dynamic
        // gunLines dynamic
       file.close();
    }
    catch (const BufferedIOException&)
    {
        C_Printf(FC_ERROR "WARNING: can't write bot map cache file at %s\n", path);
        file.close();

        // try to delete it if it fails in the middle of the write.
        if (remove(path) != 0)
        {
            C_Printf(FC_ERROR "WARNING: can't delete cache file at %s\n", path);
        }
    }
}

//
// BotMap::loadFromCache
//
// Tries to load bot map from a cache file
//
// Three stages:
// 1. Raw reading (reads pointers as row indices and only validates for magic header and dynamically allocated lists)
// 2. Validation (check as many values as possible that they're correct)
// 3. Pointer setup (converts raw indices to actual pointers)
//
void BotMap::loadFromCache(const char* path)
{
#define FAIL() do { delete botMap; botMap = nullptr; return; } while(0)

   B_Log("BotMap: loading from cache %s", path);

    GZExpansion file;
   file.setThrowing(true);
    if (!file.openFile(path, BufferedFileBase::LENDIAN))
    {
       B_Log("Couldn't open file");
        return ;
    }

   int i;
   try
   {
      char magic[9];
      magic[8] = 0;
      file.read(magic, 8);
      if (strcmp(magic, BOTMAP_CACHE_MAGIC))
      {
         return;
      }

      botMap = new (PU_STATIC, nullptr) BotMap;

      unsigned u;
      int32_t i32;
      uint32_t u32;

      file.readSint32T(botMap->numverts);
      if (!B_CheckAllocSize(botMap->numverts))
      {
          FAIL();
      }

      botMap->vertices = estructalloc(Vertex, botMap->numverts);
      for (i = 0; i < botMap->numverts; ++i)
      {
         file.readSint32T(botMap->vertices[i].x);
         file.readSint32T(botMap->vertices[i].y);
      }

      file.readUint32(u32);
      if (!B_CheckAllocSize(u32))
          FAIL();

      MetaSector* msec;
      for (u = 0; u < u32; ++u)
      {
         msec = MetaSector::readFromFile(file);
         if(!msec)
            FAIL();
         botMap->metasectors.add(msec);
      }

      file.readSint32T((uintptr_t&)botMap->nullMSec);

      file.readSint32T(botMap->numlines);
      if (!B_CheckAllocSize(botMap->numlines))
          FAIL();

      botMap->lines = estructalloc(Line, botMap->numlines);
      for (i = 0; i < botMap->numlines; ++i)
      {
          Line& ln = botMap->lines[i];
          file.readSint32T((uintptr_t&)ln.v[0]);
          file.readSint32T((uintptr_t&)ln.v[1]);
          file.readSint32T((uintptr_t&)ln.msec[0]);
          file.readSint32T((uintptr_t&)ln.msec[1]);
          file.readSint32T((uintptr_t&)ln.specline);
      }

      file.readSint32T(botMap->bMapOrgX);
      file.readSint32T(botMap->bMapOrgY);
      file.readSint32T(botMap->bMapWidth);
      file.readSint32T(botMap->bMapHeight);

      file.readUint32(u32);
      if (!B_CheckAllocSize(u32))
          FAIL();

      for(u = 0; u < u32; ++u)
      {
         Seg& sg = botMap->segs.addNew();

         file.readSint32T((uintptr_t&)sg.v[0]);
         file.readSint32T((uintptr_t&)sg.v[1]);
         file.readSint32T(sg.dx);
         file.readSint32T(sg.dy);
         file.readSint32T((uintptr_t&)sg.ln);
         uint8_t tmp;
         file.readUint8T(tmp);
         sg.isback = tmp ? true : false;
         file.readSint32T((uintptr_t&)sg.partner);

         file.readSint32T(sg.bbox[0]);
         file.readSint32T(sg.bbox[1]);
         file.readSint32T(sg.bbox[2]);
         file.readSint32T(sg.bbox[3]);

         file.readSint32T(sg.mid.x);
         file.readSint32T(sg.mid.y);

         file.readSint32T((uintptr_t&)sg.owner);

         uint32_t su32;
         file.readUint32(su32);
         if (!B_CheckAllocSize(su32))
             FAIL();
         for (uint32_t v = 0; v < su32; ++v)
         {
            file.readSint32(i32);
            sg.blocklist.add(i32);
         }
      }

      // ssectors

      file.readUint32(u32);
      if (!B_CheckAllocSize(u32))
         FAIL();

      for(u = 0; u < u32; ++u)
      {
         Subsec& ss = botMap->ssectors.addNew();

         file.readSint32T((uintptr_t&)ss.segs);

         file.readSint32T((uintptr_t&)ss.msector);
         file.readSint32T(ss.nsegs);
         file.readSint32T(ss.mid.x);
         file.readSint32T(ss.mid.y);
         
         uint32_t su32;
         file.readUint32(su32);
         if (!B_CheckAllocSize(su32))
             FAIL();
         for (uint32_t v = 0; v < su32; ++v)
         {
             Neigh& n = ss.neighs.addNew();
             file.readSint32T((uintptr_t&)n.otherss);
             file.readSint32T((uintptr_t&)n.myss);
             file.readSint32T(n.v.x);
             file.readSint32T(n.v.y);
             file.readSint32T(n.d.x);
             file.readSint32T(n.d.y);
             file.readSint32T((uintptr_t&)n.line);
             file.readSint32T(n.dist);
         }
      }

      file.readSint32T(botMap->numnodes);
      if (!B_CheckAllocSize(botMap->numnodes))
          FAIL();
      botMap->nodes = estructalloc(Node, botMap->numnodes);

      for (i = 0; i < botMap->numnodes; ++i)
      {
          Node& n = botMap->nodes[i];
          file.readSint32T(n.x);
          file.readSint32T(n.y);
          file.readSint32T(n.dx);
          file.readSint32T(n.dy);
          file.readSint32T(n.child[0]);
          file.readSint32T(n.child[1]);
      }

      file.readUint32(u32);
      if (!B_CheckAllocSize(u32))
          FAIL();
      for (u = 0; u < u32; ++u)
      {
          auto& coll = botMap->segBlocks.addNew();
          uint32_t su32, v;
          file.readUint32(su32);
          if (!B_CheckAllocSize(su32))
              FAIL();
          for (v = 0; v < su32; ++v)
          {
              auto& sg = coll.addNew();
              file.readSint32T((uintptr_t&)sg);
          }
      }

      // From 04 onwards
      file.readUint32(u32);
      if(!B_CheckAllocSize(u32))
         FAIL();
      for(u = 0; u < u32; ++u)
      {
         auto &coll = botMap->lineBlocks.addNew();
         uint32_t su32, v;
         file.readUint32(su32);
         if(!B_CheckAllocSize(su32))
            FAIL();
         for(v = 0; v < su32; ++v)
         {
            auto &ln = coll.addNew();
            file.readSint32T((uintptr_t &)ln);
         }
      }
      // end

      file.readSint32T(botMap->radius);
   }
   catch(const BufferedIOException&)
   {
      FAIL();
   }

   file.close();

   // now it's time to validate all the data
   // metasectors
   for (MetaSector *msec : botMap->metasectors)
   {
      if(!msec->convertIndicesToPointers())
         FAIL();
   }

   if(!B_ConvertPtrToCollItem(botMap->nullMSec, botMap->metasectors,
                              botMap->metasectors.getLength()))
   {
      FAIL();
   }

   // lines
   for (i = 0; i < botMap->numlines; ++i)
   {
      Line& ln = botMap->lines[i];
      if(!B_ConvertPtrToArrayItem(ln.v[0], botMap->vertices, botMap->numverts)
         || !B_ConvertPtrToArrayItem(ln.v[1], botMap->vertices, botMap->numverts)
         || !B_ConvertPtrToCollItem(ln.msec[0], botMap->metasectors, botMap->metasectors.getLength())
         || !B_ConvertPtrToCollItem(ln.msec[1], botMap->metasectors, botMap->metasectors.getLength())
         || !B_ConvertPtrToArrayItem(ln.specline, ::lines, ::numlines))
      {
         FAIL();
      }
   }

   // segs
   for(Seg& sg : botMap->segs)
   {
      if(!B_ConvertPtrToArrayItem(sg.v[0], botMap->vertices, botMap->numverts)
         || !B_ConvertPtrToArrayItem(sg.v[1], botMap->vertices, botMap->numverts)
         || !B_ConvertPtrToArrayItem(sg.ln, botMap->lines, botMap->numlines)
         || !B_ConvertPtrToArrayItem(sg.partner, &botMap->segs[0], botMap->segs.getLength())
         || !B_ConvertPtrToArrayItem(sg.owner, &botMap->ssectors[0], botMap->ssectors.getLength()))
      {
         FAIL();
      }
   }

   // subsecs
   for(Subsec& ss : botMap->ssectors)
   {
      if(!B_ConvertPtrToArrayItem(ss.segs, &botMap->segs[0], botMap->segs.getLength())
         || !B_ConvertPtrToCollItem(ss.msector, botMap->metasectors, botMap->metasectors.getLength()))
      {
         FAIL();
      }
      for (Neigh &n : ss.neighs)
      {
          if (!B_ConvertPtrToArrayItem(n.otherss, &botMap->ssectors[0], botMap->ssectors.getLength())
            || !B_ConvertPtrToArrayItem(n.myss, &botMap->ssectors[0], botMap->ssectors.getLength())
            || !B_ConvertPtrToArrayItem(n.line, botMap->lines, botMap->numlines))
         {
            FAIL();
         }
      }
   }

   // segblocks
   for(auto& coll : botMap->segBlocks)
   {
      for(Seg*& sg : coll)
      {
         if(!B_ConvertPtrToArrayItem(sg, &botMap->segs[0], botMap->segs.getLength()))
            FAIL();
      }
   }

   // From 04 onwards
   for(auto &coll : botMap->lineBlocks)
   {
      for(Line *&ln : coll)
      {
         if(!B_ConvertPtrToArrayItem(ln, botMap->lines, botMap->numlines))
            FAIL();
      }
   }
   // end

   // is this it?

#undef FAIL
}

//
// BotMap::addCornerNeighs
//
// Takes care to add corner neighs where two subsectors only join by that
//
void BotMap::addCornerNeighs()
{
    // Lists of neighbours for each subsector
    std::unordered_map<const Subsec *, std::unordered_set<const Subsec *>> ssJoinMap;
    // List of subsectors which have received punctual neighs
    std::unordered_map<const Subsec *, std::unordered_set<const Subsec *>> ssVisitedMap;
    // List of subsectors going into this vertex
    std::unordered_map<const Vertex *, std::unordered_set<Subsec *>> vertSsList;

    for (const Seg& seg : segs)
    {
        // Null is a valid value
        if (seg.partner)
        {
            ssJoinMap[seg.owner].insert(seg.partner->owner);
            ssJoinMap[seg.partner->owner].insert(seg.owner);

            vertSsList[seg.v[0]].insert(seg.partner->owner);
            vertSsList[seg.v[1]].insert(seg.partner->owner);
        }
        vertSsList[seg.v[0]].insert(seg.owner);
        vertSsList[seg.v[1]].insert(seg.owner);
    }

    int i;
    const Vertex *v;
    const std::unordered_set<Subsec *> *ssset;

    Neigh n;
    n.line = nullptr;
    n.d.x = n.d.y = 0;

    for (i = 0; i < numverts; ++i)
    {
        v = vertices + i;
        auto kt = vertSsList.find(v);
        if (kt == vertSsList.end())
            continue;

        ssset = &kt->second;
        for (auto it = ssset->begin(); it != ssset->end(); ++it)
        {
            for (auto jt = it; jt != ssset->cend(); ++jt)
            {
                if (jt == it)   // hack to be able to start one step ahead
                    continue;
                // Skip them if neighbours
                if (ssJoinMap[*it].count(*jt) || ssJoinMap[*jt].count(*it))
                    continue;
                // Add neighs, if not already
                if (!ssVisitedMap[*it].count(*jt))
                {
                    ssVisitedMap[*it].insert(*jt);

                    n.otherss = *jt;
                    n.myss = *it;
                    n.v.x = v->x;
                    n.v.y = v->y;
                    n.dist = ((*jt)->mid - (*it)->mid).sqrtabs();
                    (*it)->neighs.add(n);
                }
                if (!ssVisitedMap[*jt].count(*it))
                {
                    ssVisitedMap[*jt].insert(*it);

                    n.otherss = *it;
                    n.myss = *jt;
                    n.v.x = v->x;
                    n.v.y = v->y;
                    n.dist = ((*jt)->mid - (*it)->mid).sqrtabs();
                    (*jt)->neighs.add(n);
                }
            }
        }
    }
}

//
// B_BuildBotMap
//
// The main call to build bot map
//
void BotMap::Build()
{

	// Check for hash existence
	char* digest = g_levelHash.digestToString();
   qstring hashFileName("botmap-");
   hashFileName << digest << ".cache";
   qstring oldHashFileName(hashFileName);   // the old, uncompressed file needs to be deleted.
   hashFileName << ".gz";   // add the gz extension

   const char *oldfile = D_CheckAutoDoomPathFile(oldHashFileName.constPtr(), false);
   if (oldfile)
   {
       B_Log("Deleting old uncompressed cache file %s...", oldHashFileName.constPtr());
       if (remove(oldfile) < 0)
       {
           B_Log("WARNING: couldn't remove it (error %d: %s)", errno, strerror(errno));
       }
   }

   B_Log("Looking for level cache %s...", hashFileName.constPtr());
   const char* fpath = D_CheckAutoDoomPathFile(hashFileName.constPtr(), false);

   if (fpath)
   {
       // Try building from it
       BotMap::loadFromCache(fpath);
       if (botMap)
           botMap->changeTag(PU_LEVEL);
   }

   if (!fpath || !botMap)
   {
       // Create the BotMap
       B_BEGIN_CLOCK
           botMap = new (PU_LEVEL, nullptr) BotMap;
       B_MEASURE_CLOCK(newBotMap)

       fixed_t radius = mobjinfo[players[consoleplayer].pclass->type]->radius;
       botMap->radius = radius;

       // Create blockmap
       B_NEW_CLOCK
       botMap->createBlockMap();
       B_MEASURE_CLOCK(createBlockMap)

	   B_Log("Level cache not found or invalid");
	   B_buildTempBotMapFromScratch(radius, digest);
       B_NEW_CLOCK
       botMap->addCornerNeighs();
       B_MEASURE_CLOCK(addCornerNeighs)
       botMap->cacheToFile(M_SafeFilePath(g_autoDoomPath, hashFileName.constPtr()));
   }
   efree(digest);

   // init validstuff
   VALID_ALLOC(botMap->validLines, botMap->numlines);

   // Place all mobjs on it
   B_setMobjPositions();
   
   // Place all special lines on it
   B_setSpecLinePositions();

   // Find all living monsters
   botMap->getAllLivingMonsters();

   // Find all doors
   botMap->getDoorSectors();
}

// EOF

