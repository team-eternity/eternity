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
//      Bot path structure. Closely set to either A* or Dijkstra algorithm,
//      but can always be modified.
//
//-----------------------------------------------------------------------------

#include <queue>
#include <vector>
#include "../z_zone.h"

#include "b_path.h"
#include "../d_player.h"
#include "../e_things.h"
#include "../p_maputl.h"
#include "../p_spec.h"
#include "../r_state.h"

////////////////////////////////////////////////////////////////////////////////
//
// Public methods
//
////////////////////////////////////////////////////////////////////////////////

//
// PathFinder::FindNextGoal
//
// Uses a search for the nearest goal (not knowing where to go)
// It uses Dijkstra graph search, needed because of varying subsector sizes.
//
bool PathFinder::FindNextGoal(v2fixed_t pos, BotPath& path,
                              bool(*isGoal)(const BSubsec&, BotPathEnd&, void*),
                              void* parm)
{
    HeapEntry nhe;
    m_dijkHeap.makeEmpty<true>();

    const BSubsec* first = &m_map->ssectors[0];
    
    //compare.o = this;
    //compare.first = first;
    
    db[1].IncrementValidcount();

   const BSubsec& source = m_map->pointInSubsector(pos);

    path.start = pos;

    //const BSubsec** front = db[1].ssqueue;
    //const BSubsec** back = db[1].ssqueue;
    BotPathEnd coord;

    int index = (int)(&source - first);
   
    db[1].ssvisit[index] = db[1].validcount;
    db[1].ssprev[index] = nullptr;
    db[1].ssdist[index] = 0;

    nhe.ss = &source;
    nhe.dist = 0;
    m_dijkHeap.add(nhe);

//    std::push_heap(front, back, compare);
    const BSubsec* t;
    const BNeigh* n;
    path.inv.makeEmpty<true>();
   path.sss.clear();
    const TeleItem* bytele;
    fixed_t tentative;

//    std::make_heap(front, back, compare);
    fixed_t founddist;
    while (!m_dijkHeap.isEmpty())
    {
        std::pop_heap(m_dijkHeap.begin(), m_dijkHeap.end());
        t = m_dijkHeap.back().ss;    // get the extracted one
        founddist = m_dijkHeap.pop().dist;
        if (founddist != db[1].ssdist[t - first])
            continue;
        
        if (isGoal(*t, coord, parm))
        {
            path.last = t;
            path.end = coord;
           path.sss.insert(t);
            for (;;)
            {
                n = db[1].ssprev[t - first];
                if (!n)
                    break;
                path.inv.add(n);
               path.sss.insert(n->myss);
                t = n->myss;
            }
            return true;
        }
        
        for (const BNeigh& neigh : t->neighs)
        {
            // Distance shall be from centre of source to middle of seg
            
            bytele = checkTeleportation(neigh);
            index = (int)(neigh.otherss - first);
            if (bytele)
            {
                index = (int)(bytele->ss - first);
                
               tentative = getAdjustedDistance(db[1].ssdist[t - first],
                                               (t->mid - neigh.v - neigh.d / 2).sqrtabs(), t);
                
                if (db[1].ssvisit[index] != db[1].validcount
                    || tentative < db[1].ssdist[index])
                {
                    pushSubsectorToHeap(neigh, index, *bytele->ss, tentative);
                }
            }
            else
            {
                // Hack to make the edge subsectors (which are never 'simple') less attractive to the pathfinder.
                // Needed because the bot tends to easily fall off ledges because it chooses the subsector
                // closest to the edge.
                // FIXME: Still needs improvement.
//                if(!msec->isInstanceOf(RTTI(SimpleMSector)))
//                {
//                    tentative = db[1].ssdist[t - first] + 2 * neigh.dist;
//                }
//                else

               tentative = getAdjustedDistance(db[1].ssdist[t - first], neigh.dist, t);
                
                if ((db[1].ssvisit[index] != db[1].validcount
                     || tentative < db[1].ssdist[index])
                     && m_map->canPass(*t, *neigh.otherss, m_player->mo->height))
                {
                    pushSubsectorToHeap(neigh, index, *neigh.otherss, tentative);
                }
            }
        }
    }
    return false;
}

void PathFinder::pushSubsectorToHeap(const BNeigh& neigh, int index, const BSubsec& ss, fixed_t tentative)
{
    db[1].ssvisit[index] = db[1].validcount;
    db[1].ssprev[index] = &neigh;
    db[1].ssdist[index] = tentative;

    HeapEntry &nhe = m_dijkHeap.addNew();
    nhe.dist = tentative;
    nhe.ss = &ss;
    std::push_heap(m_dijkHeap.begin(), m_dijkHeap.end());
}

//
// PathFinder::AvailableGoals
//
// Looks for any goals from point X. Uses simple breadth first search.
// Returns all the good subsectors into the collection
// Uses a function for criteria.
//
bool PathFinder::AvailableGoals(const BSubsec& source,
                                std::unordered_set<const BSubsec*>* dests,
                                PathResult(*isGoal)(const BSubsec&, void*),
                                void* parm)
{
    db[0].IncrementValidcount();

    const BSubsec** front = db[0].ssqueue;
    const BSubsec** back = db[0].ssqueue;

    const BSubsec* first = &m_map->ssectors[0];
    db[0].ssvisit[&source - first] = db[0].validcount;
    *back++ = &source;
    const BSubsec* t;
    PathResult res;
    const TeleItem* bytele;
    while (front < back)
    {
        t = *front++;
        res = isGoal(*t, parm);
        if (res == PathAdd && dests)
            dests->insert(t);
        else if (res == PathDone)
            return true;
        for (const BNeigh& neigh : t->neighs)
        {
            bytele = checkTeleportation(neigh);
            if (bytele)
            {
                if (db[0].ssvisit[bytele->ss - first] != db[0].validcount)
                {
                    db[0].ssvisit[bytele->ss - first] = db[0].validcount;
                    *back++ = bytele->ss;
                }
            }
            else if (db[0].ssvisit[neigh.otherss - first] != db[0].validcount
                && m_map->canPass(*t, *neigh.otherss, m_player->mo->height))
            {
                db[0].ssvisit[neigh.otherss - first] = db[0].validcount;
                *back++ = neigh.otherss;
            }
        }
    }

    return false;
}

//
// Adjusts distance if the sector is painful
//
fixed_t PathFinder::getAdjustedDistance(fixed_t base, fixed_t add, const BSubsec *t) const
{
   fixed_t tentative = 0;
   int factor = 1;

   const sector_t *sector = t->msector->getFloorSector();
   if(enable_nuke && sector->damage > 0 &&
      (!m_player->powers[pw_ironfeet] ||
       sector->damageflags & SDMG_IGNORESUIT))
   {
      if(sector->damagemask <= 0)
         factor = sector->damage * 32;
      else
         factor = sector->damage * 32 / sector->damagemask;
      if(factor < 1)
         factor = 1;
   }
   tentative = base + add * factor;
   if(tentative < 0) // overflow case
      return D_MAXINT;
   return tentative;
}

//
// PathFinder::checkTeleportation
//
// Checks whether neigh is a teleportation separator. Returns a corresponding
// TeleItem if so, nullptr otherwise.
//
const PathFinder::TeleItem* PathFinder::checkTeleportation(const BNeigh& neigh)
{
    const BotMap::Line* bline = neigh.line;
    if (!bline 
        || !bline->specline 
        || !B_IsWalkTeleportation(bline->specline->special) 
        || (neigh.d.x ^ bline->specline->dx) < 0 
        || (neigh.d.y ^ bline->specline->dy) < 0
        || !m_map->canPass(*neigh.myss, *neigh.otherss, m_player->mo->height))
    {
        return nullptr;
    }

    // Now we have our ways
    const line_t* line = bline->specline;
    int i;
    Thinker* thinker;
    const Mobj* m;
    if (!m_teleCache.count(line))
    {
        // CODE LARGELY COPIED FROM EV_Teleport
        for (i = -1; (i = P_FindSectorFromLineArg0(line, i)) >= 0;)
        {
            for (thinker = thinkercap.next; thinker != &thinkercap;
                 thinker = thinker->next)
            {
                if (!(m = thinker_cast<Mobj *>(thinker)))
                    continue;
                if (m->type == E_ThingNumForDEHNum(MT_TELEPORTMAN) &&
                    m->subsector->sector - ::sectors == i)
                {
                    TeleItem ti;
                    ti.v = v2fixed_t(*m);
                    ti.ss = &botMap->pointInSubsector(ti.v);
                    m_teleCache[line] = ti;
                    return &m_teleCache[line];
                }
            }
        }
    }
    else
    {
        return &m_teleCache[line];
    }

    return nullptr;
}

//
// PathFinder::DataBox::IncrementValidcount
//
// Must be called to update validcount and ensure correct data sets
//
void PathFinder::DataBox::IncrementValidcount()
{   
    if (sscount != o->m_map->ssectors.getLength())
    {
        size_t s = (sscount = (unsigned)o->m_map->ssectors.getLength())
        * sizeof(*ssvisit);
        
        ssvisit = erealloc(decltype(ssvisit), ssvisit, s);
        memset(ssvisit, 0, s);
        validcount = 0;

        ssqueue = erealloc(decltype(ssqueue), ssqueue,
                           sscount * sizeof(*ssqueue));

        ssprev = erealloc(decltype(ssprev), ssprev, sscount * sizeof(*ssprev));
        
        ssdist = erealloc(decltype(ssdist), ssdist, sscount * sizeof(*ssdist));
    }
    ++validcount;
    if (!validcount)  // wrap around: reset former validcounts!
        memset(ssvisit, 0xff, sscount * sizeof(*ssvisit));

}

//
// Does a breadth-first (neighbourhood-based, but not distance-aware) search
// from a starting subsector ahead.
//
// There are two functions to use here:
//  passfunc(): returns true if neigh can be passed (usually by a player).
//  scanfunc(): returns true if subsector has goal.
// Both functions can capture external parameters, so it's feasible to return
// false from scanfunc() but still use it to gather data.
//
bool B_FindBreadthFirst(const BSubsec &first,
                        std::function<bool(const BNeigh &)> &&passfunc,
                        std::function<bool(const BSubsec &)> &&scanfunc)
{
   std::queue<const BSubsec *> q;
   std::vector<bool> visited;
   visited.resize(botMap->ssectors.getLength());

   const BSubsec &fss = botMap->ssectors[0];

   q.push(&first);
   visited[&first - &fss] = true;
   while(!q.empty())
   {
      const BSubsec *ss = q.front();
      q.pop();
      if(scanfunc(*ss))
         return true;
      for(const auto &neigh : ss->neighs)
      {
         if(!passfunc(neigh))
            continue;
         const BSubsec *nss = neigh.otherss;
         if(visited[nss - &fss])
            continue;
         q.push(nss);
         visited[nss - &fss] = true;
      }
   }
   return false;
}

// EOF

