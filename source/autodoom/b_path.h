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

#ifndef __EternityEngine__b_path__
#define __EternityEngine__b_path__

#include <map>
#include "b_botmap.h"
#include "b_util.h"
#include "../m_collection.h"

//
// PathFinder
//
// Is linked with the botmap
//
class BotPath
{
public:
    PODCollection<const BNeigh*>    inv;    // path from end to start
    const BSubsec*                  last;
    v2fixed_t                       start;
    v2fixed_t                       end;

    BotPath() :last(nullptr)
    {
    }
};

enum PathResult
{
    PathNo,
    PathAdd,
    PathDone
};

class PathFinder
{
public:
    PathFinder(const BotMap* map = nullptr) : 
        m_map(map),
        m_plheight(0)
    {
        memset(db, 0, sizeof(db));
        db[0].o = this;
        db[1].o = this;
    }

    ~PathFinder()
    {
        Clear();
    }

    bool FindNextGoal(fixed_t x, fixed_t y, BotPath& path, bool(*isGoal)(const BSubsec&, v2fixed_t&, void*), void* parm = nullptr);
    bool AvailableGoals(const BSubsec& source, std::unordered_set<const BSubsec*>* dests, PathResult(*isGoal)(const BSubsec&, void*), void* parm = nullptr);

    void SetPlayerHeight(fixed_t value)
    {
        m_plheight = value;
    }

    void SetMap(const BotMap* map)
    {
        m_map = map;
        Clear();
    }

    void Clear()
    {
        db[0].Clear();
        db[1].Clear();
        m_teleCache.clear();
        m_dijkHeap.clear();
    }

private:
    struct TeleItem
    {
        const BSubsec*  ss;
        v2fixed_t       v;
    };

    struct DataBox
    {
        unsigned short  validcount;
        unsigned short* ssvisit;
        unsigned        sscount;
        const BNeigh**  ssprev;
        const BSubsec** ssqueue;
        fixed_t*        ssdist;  // used by dijkstra

        const PathFinder* o;

        void Clear()
        {
            efree(ssvisit);
            ssvisit = nullptr;
            efree(ssprev);
            ssprev = nullptr;
            efree(ssqueue);
            ssqueue = nullptr;
            efree(ssdist);
            ssdist = nullptr;
            sscount = 0;
            validcount = 0;
        }

        void IncrementValidcount();
    };

    struct HeapEntry
    {
        const BSubsec* ss;
        fixed_t dist;
        bool operator < (const HeapEntry& o) const
        {
            return dist > o.dist;
        }
    };

    PODCollection<HeapEntry>    m_dijkHeap;
    
    const TeleItem* checkTeleportation(const BNeigh& neigh);

    const BotMap*   m_map;
    DataBox         db[2];

    // OPTIM NOTE: please measure whether short or int is better
    fixed_t         m_plheight;

    std::map<const line_t*, TeleItem> m_teleCache; // teleporter cache
};

#endif /* defined(__EternityEngine__b_path__) */

// EOF

