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
#include "../p_maputl.h"

////////////////////////////////////////////////////////////////////////////////
//
// PathArray Private methods
//
////////////////////////////////////////////////////////////////////////////////

//
// PathArray::addNode
//
// Adds a node to the nodes array. It allocates if needed.
//
int PathArray::addNode(const Node &node)
{
   nodes.add(node);
   numOpenNodes += (int)node.open;
   ssNodeMap[node.ss] = (int)nodes.getLength() - 1;
	return (int)nodes.getLength() - 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// Public methods
//
////////////////////////////////////////////////////////////////////////////////

//
// PathArray::addStartNode
//
// Adds the first node, on FindShortestPath initialization
//
int PathArray::addStartNode(fixed_t startx, fixed_t starty)
{
   // Normal case: no known destination or repellent
	return addStartNode(startx, starty, startx, starty);
}

int PathArray::addStartNode(fixed_t startx, fixed_t starty, fixed_t destx,
                            fixed_t desty, bool negate)
{
	Node node;
   
   // May be inaccurate
	fixed_t dist = P_AproxDistance(destx - startx, desty - starty);
   
   BSubsec &ss = botMap->pointInSubsector(startx, starty);
	
	// This resets and creates new data with possible negative distance
	node.g_score = 0;
	node.h_score = negate ? -dist : dist;
	node.f_score = node.g_score + node.h_score;
	node.open = true;
   node.seg = NULL;
   node.x = startx;
   node.y = starty;
   node.ss = &ss;
	node.prev = -1;
	node.next = -1;
	
	return addNode(node);
}

//
// PathArray::bestScoreIndex
//
// Returns the node with the minimum (best) distance score
//
int PathArray::bestScoreIndex()
{
	int64_t fmin = 0x7fffffffffffffff;
   int imin = -1, j = numOpenNodes;
	for(int i = (int)nodes.getLength() - 1; i >= 0; --i)
	{
		if(nodes[i].open)
      {
         --j;
         if(nodes[i].f_score < fmin)
         {
            fmin = nodes[i].f_score;
            imin = i;
         }
         if(!j)
         {
            break;
         }
      }
	}
	return imin;
}

//
// PathArray::straightenPath
//
// Attempts to straighten path between two extreme nodes. If it can't, it tries
// to do it recursively to a random point.
//
void PathArray::straightenPath(int start_idx, int final_idx, fixed_t height)
{
   if(start_idx == final_idx)   // start is neighbouring final
      return;
   
   // Obtain the indices in the path[] array
   int start = pathIndices[start_idx];
   int final = pathIndices[final_idx];

   // Structure for atempting to straighten the path
   struct attemptNode
   {
      BSeg *seg;
      BSubsec *ss;
   };
   
   // get final node's point
   v2fixed_t endpoint = getNodeMid(final);
   int j;
   
   bool blocked = false, found = false;
   BSeg *sg, *lastseg = nullptr;
   BSubsec *ss, *nextSS = nullptr;
   double ix, iy;
   
   
   PODCollection<attemptNode> attempted;
   
   // iterate each path node
   for(ss = nodes[start].ss; ss; ss = nextSS)
   {
      if(ss == nodes[final].ss)
         break;   // okay
      
      found = false;
      sg = nullptr;
      nextSS = nullptr;
      blocked = false;
      // iterate each of subsector's segs
      for(j = 0; j < ss->nsegs; ++j)
      {
         sg = ss->segs + j;
         if(sg == lastseg) // skip the segment from previous intersection
            continue;
         
         // got an intersecting segment
         if(B_SegmentsIntersect(nodes[start], endpoint, *sg->v[0], *sg->v[1]))
         {
            found = true;
            // line and segments intersect. Do something.
            if(!sg->partner || !sg->partner->owner)
            {
               blocked = true;     // can't, it's a wall
               break;
            }
            
            nextSS = sg->partner->owner;
            
            if(!botMap->canPass(*ss, *nextSS, height))
               blocked = true;   // don't attempt to pass this
      
            break;   // get out of loop
         }
      }
      
      // now see if blocked
      // if blocked, then start search from next node.
      // if not blocked, then
      if(!found || blocked)
      {
         // example: start_idx = 15, final_idx = 0.
         // midnode = 0 + 1 + random() % 14 => 1..14
         
         // do recursively and return
         if(start_idx - final_idx >= 2)
         {
            int midnode = final_idx + 1 + random() % (start_idx - final_idx - 1);
            straightenPath(start_idx, midnode, height);
            straightenPath(midnode, final_idx, height);
         }
         else
         {
            // directly. We have a problem here, man!
            // Just use what's already given, then
            straightNodes.add(nodes[final]);
            (straightNodes.end() - 1)->next = (int)straightNodes.getLength();
         }
         return;
      }
      else
      {
         // add info indices
         lastseg = sg->partner;
         attemptNode newan = {sg, nextSS};
         attempted.add(newan);
      }
   }
   
   // everything went okay, now add the indices to the straightnodes
   for (auto it = attempted.begin(); it != attempted.end(); ++it)
   {
      Node &newStr = straightNodes.addNew();
      newStr.seg = it->seg;
      newStr.ss = it->ss;
      const BSeg &sg = *it->seg;
      B_IntersectionPoint(LineEq::MakeFixed(nodes[start], endpoint),
                          LineEq::MakeFixed(*sg.v[0], *sg.v[1]),
                          ix, iy);
      newStr.x = M_DoubleToFixed(ix);
      newStr.y = M_DoubleToFixed(iy);
      newStr.next = (int)straightNodes.getLength();
      newStr.prev = newStr.next - 2;
   }
}

//
// PathArray::finish
//
// Finalizes the path, preparing the .next fields
//
void PathArray::finish(int index, const v2fixed_t &vec, fixed_t height)
{
   finalx = vec.x;
   finaly = vec.y;
	nodes[index].next = -1;

	int final = index;   // keep final index value
   int index2 = index;
   
   pathIndices.resize(0); // init path indices
   pathIndices.add(final);
   
	for(index = nodes[index].prev; index != -1;
       index2 = index, index = nodes[index].prev)
   {
      pathIndices.add(index);
		nodes[index].next = index2;
   }
	pathexists = true;
   
   // Straighten path
   // Try and see if straight line exists between final node and start node
   // (must have start coordinates, contained in the first node, without a seg)
   // If it does, bingo. Change path to a straight one. Add missing nodes if
   // needed.
   
   // If it doesn't, then do this procedure recursively on final-middle and
   // middle-start
   
   straightNodes.resize(0);
   straightNodes.add(nodes[0]);  // add initial index to list
   straightNodes[0].next = 1;
   
   if(pathIndices.getLength() > 1)
      straightenPath((int)pathIndices.getLength() - 1, 0, height);
   
   (straightNodes.end() - 1)->next = -1;
}

//
// PathArray::openCoordsIndex
//
// Returns the index of the cx/cy coordinate, if open. Returns -2 if closed or
// -1 if non-existent. FIXME: use 2d map for this?
//
int PathArray::openCoordsIndex(BSubsec &ss)
{
   if (ssNodeMap.count(&ss))
   {
      int i = ssNodeMap.at(&ss);
      if(!nodes[i].open)
         return -2;
      return i;
   }
   return -1;
}

//
// PathArray::straightPathCoordsIndex
//
// Returns the index of the cx/cy coordinate, if it's on a path, or -1 else
//
int PathArray::straightPathCoordsIndex(fixed_t cx, fixed_t cy)
{
   BSubsec &ss = botMap->pointInSubsector(cx, cy);
	for(int i = 0; i != -1; i = straightNodes[i].next)
	{
		if(straightNodes[i].ss == &ss)
			return i;
      
	}
	return -1;
}

//
// PathArray::~PathArray
//
// Destructor
//
PathArray::~PathArray()
{
}

//
// PathArray::updateNode
//
// Updates the score of the given node, or even creates a new one
//
void PathArray::updateNode(int ichange, int index, BSeg *seg, BSubsec *ss,
                           int64_t dist)
{
	updateNode(ichange, index, seg, ss, dist, seg->mid.x, seg->mid.y);
}

void PathArray::updateNode(int ichange, int index, BSeg *seg, BSubsec *ss,
                           int64_t dist, fixed_t destx, fixed_t desty, bool negate)
{
	dist += nodes[index].g_score;
	if(ichange == -1)
	{
		Node node;
		fixed_t apdist = P_AproxDistance(destx - seg->mid.x, desty - seg->mid.y);
		node.g_score = dist;
		node.h_score = negate ? -apdist : apdist;
		node.f_score = node.g_score + node.h_score;
		node.open = true;
		node.seg = seg;
		node.ss = ss;
      node.x = seg->mid.x;
      node.y = seg->mid.y;
		node.prev = index;
      
		addNode(node);
	}
	else if(dist < nodes[ichange].g_score)
	{
		nodes[ichange].g_score = dist;
		nodes[ichange].f_score = dist + nodes[ichange].h_score;
		nodes[ichange].prev = index;
	}
}

//
// PathFinder::FindGoals
//
// Looks for any goals from point X. Uses simple breadth first search.
// Returns all the good subsectors into the collection
// Uses a function for criteria.
//
void PathFinder::FindGoals(const BSubsec& source, PODCollection<const BSubsec*>& dests, bool(*isGoal)(const BSubsec&, void*), void* parm)
{
    IncrementValidcount();

    const BSubsec** front = m_ssqueue;
    const BSubsec** back = m_ssqueue;

    const BSubsec* first = &m_map->ssectors[0];
    m_ssvisit[&source - first] = m_validcount;
    *back++ = &source;
    const BSubsec* t;
    while (front < back)
    {
        t = *front++;
        if (isGoal(*t, parm))
            dests.add(t);
        for (const BSubsec* neigh : t->neighs)
        {
            if (m_ssvisit[neigh - first] != m_validcount)
            {
                m_ssvisit[neigh - first] = m_validcount;
                *back++ = neigh;
            }
        }
    }
}

void PathFinder::IncrementValidcount()
{   
    if (m_sscount != m_map->ssectors.getLength())
    {
        size_t s = (m_sscount = m_map->ssectors.getLength()) * sizeof(*m_ssvisit);
        m_ssvisit = erealloc(decltype(m_ssvisit), m_ssvisit, s);
        memset(m_ssvisit, 0, s);
        m_validcount = 0;

        m_ssqueue = erealloc(decltype(m_ssqueue), m_ssqueue, m_sscount * sizeof(*m_ssqueue));
    }
    ++m_validcount;
    if (!m_validcount)  // wrap around: reset former validcounts!
        memset(m_ssvisit, 0xff, m_sscount * sizeof(*m_ssvisit));
}

// EOF

