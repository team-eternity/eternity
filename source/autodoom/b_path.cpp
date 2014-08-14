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

#include "../z_zone.h"

#include "b_path.h"
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
// Currently just BFS is enough
//
bool PathFinder::FindNextGoal(fixed_t x, fixed_t y, BotPath& path, bool(*isGoal)(const BSubsec&, v2fixed_t&, void*), void* parm)
{
    db[1].IncrementValidcount();

    const BSubsec& source = m_map->pointInSubsector(x, y);

    path.start.x = x;
    path.start.y = y;

    const BSubsec** front = db[1].ssqueue;
    const BSubsec** back = db[1].ssqueue;
    v2fixed_t coord;

    const BSubsec* first = &m_map->ssectors[0];
    db[1].ssvisit[&source - first] = db[1].validcount;
    db[1].ssprev[&source - first] = nullptr;

    *back++ = &source;
    const BSubsec* t;
    const BNeigh* n;
    path.inv.makeEmpty<true>();
    const TeleItem* bytele;
    while (front < back)
    {
        t = *front++;
        if (isGoal(*t, coord, parm))
        {
            path.last = t;
            path.end = coord;
            for (;;)
            {
                n = db[1].ssprev[t - first];
                if (!n)
                    break;
                path.inv.add(n);
                t = n->seg->owner;
            }
            return true;
        }
        for (const BNeigh& neigh : t->neighs)
        {
            bytele = checkTeleportation(neigh);
            if (bytele)
            {
                if (db[1].ssvisit[bytele->ss - first] != db[1].validcount)
                {
                    db[1].ssvisit[bytele->ss - first] = db[1].validcount;
                    db[1].ssprev[bytele->ss - first] = &neigh;
                    *back++ = bytele->ss;
                }
            }
            else if (db[1].ssvisit[neigh.ss - first] != db[1].validcount && m_map->canPass(*t, *neigh.ss, m_plheight))
            {
                db[1].ssvisit[neigh.ss - first] = db[1].validcount;
                db[1].ssprev[neigh.ss - first] = &neigh;
                *back++ = neigh.ss;
            }
        }
    }
    return false;
}

//
// PathFinder::AvailableGoals
//
// Looks for any goals from point X. Uses simple breadth first search.
// Returns all the good subsectors into the collection
// Uses a function for criteria.
//
bool PathFinder::AvailableGoals(const BSubsec& source, std::unordered_set<const BSubsec*>* dests, PathResult(*isGoal)(const BSubsec&, void*), void* parm)
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
            else if (db[0].ssvisit[neigh.ss - first] != db[0].validcount && m_map->canPass(*t, *neigh.ss, m_plheight))
            {
                db[0].ssvisit[neigh.ss - first] = db[0].validcount;
                *back++ = neigh.ss;
            }
        }
    }

    return false;
}

const PathFinder::TeleItem* PathFinder::checkTeleportation(const BNeigh& neigh)
{
    const BotMap::Line* bline = neigh.seg->ln;
    if (!bline 
        || !bline->specline 
        || !B_IsWalkTeleportation(bline->specline->special) 
        || (neigh.seg->dx ^ bline->specline->dx) < 0 
        || (neigh.seg->dy ^ bline->specline->dy) < 0
        || !m_map->canPass(*neigh.seg->owner, *neigh.ss, m_plheight))
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
        for (i = -1; (i = P_FindSectorFromLineTag(line, i)) >= 0;)
        {
            for (thinker = thinkercap.next; thinker != &thinkercap; thinker = thinker->next)
            {
                if (!(m = thinker_cast<Mobj *>(thinker)))
                    continue;
                if (m->type == E_ThingNumForDEHNum(MT_TELEPORTMAN) &&
                    m->subsector->sector - ::sectors == i)
                {
                    TeleItem ti;
                    ti.v.x = m->x;
                    ti.v.y = m->y;
                    ti.ss = &botMap->pointInSubsector(ti.v.x, ti.v.y);
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
        size_t s = (sscount = (unsigned)o->m_map->ssectors.getLength()) * sizeof(*ssvisit);
        ssvisit = erealloc(decltype(ssvisit), ssvisit, s);
        memset(ssvisit, 0, s);
        validcount = 0;

        ssqueue = erealloc(decltype(ssqueue), ssqueue, sscount * sizeof(*ssqueue));

        ssprev = erealloc(decltype(ssprev), ssprev, sscount * sizeof(*ssprev));
    }
    ++validcount;
    if (!validcount)  // wrap around: reset former validcounts!
        memset(ssvisit, 0xff, sscount * sizeof(*ssvisit));

}

// EOF

