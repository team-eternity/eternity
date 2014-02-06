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

#include "b_botmap.h"
#include "b_util.h"

//
// PathArray
//
// Class for bot's pathfinding. Taken from AutoWolf
//
class PathArray : public ZoneObject
{
protected:
   
   //
   // Node
   //
   // Dijkstra/A* search node
   //
	struct Node
	{
      BSeg *seg;                    // node to reach
      BSubsec *ss;                  // subsector beyond the seg
      fixed_t x, y;
		int64_t f_score, g_score, h_score;  // total, so-far, to-dest heuristics
		int prev, next;                 // previous and next index on path
		bool open;                   // node still open for search?
	};
   fixed_t finalx, finaly;
   
   PODCollection<Node> nodes; // array of nodes
   PODCollection<Node> straightNodes;  // array of straightened nodes
   PODCollection<int> pathIndices;  // indices of resolved path (reversed)
   
   int numOpenNodes;                   // number of open nodes
	bool pathexists;                 // whether the path has been built
   
   RandomGenerator random;       // random generator for choosing nodes
   
   std::unordered_map<BSubsec *, int> ssNodeMap;
	
	int addNode(const Node &node);
   
   void straightenPath(int start, int final, fixed_t height);
   
   void getNodeMid(int index, fixed_t &x, fixed_t &y)
   {
      if(nodes[index].next > -1)
      {
         // midway between the seg crossings
         x = (nodes[index].x + nodes[nodes[index].next].x) / 2;
         y = (nodes[index].y + nodes[nodes[index].next].y) / 2;
      }
      else
      {
         // just the final coordinate
         x = finalx;
         y = finaly;
      }
   }
   v2fixed_t getNodeMid(int index)
   {
      v2fixed_t ret;
      getNodeMid(index, ret.x, ret.y);
      return ret;
   }
   
public:
	int addStartNode(fixed_t startx, fixed_t starty);
	int addStartNode(fixed_t startx, fixed_t starty, fixed_t destx, fixed_t desty,
                    bool negate = false);
	int bestScoreIndex() ;
	void finish(int index, const v2fixed_t &vec, fixed_t height);
	int openCoordsIndex(BSubsec &ss) ;
	int straightPathCoordsIndex(fixed_t cx, fixed_t cy) ;
	~PathArray();
	void updateNode(int ichange, int index, BSeg *seg, BSubsec *ss, int64_t dist);
	void updateNode(int ichange, int index, BSeg *seg, BSubsec *ss, int64_t dist,
                   fixed_t destx, fixed_t desty, bool negate = false);
   
   // Node closing accessor
	void closeNode(int index) {nodes[index].open = false; --numOpenNodes;}
   // pathexists accessor
   bool exists() const {return pathexists;}
   // node coordinates accessor
	void getRef(int index, BSeg **seg, BSubsec **ss)
	{
		*seg = nodes[index].seg;
		*ss = nodes[index].ss;
	}
   void getStraightCoords(int index, fixed_t &setx, fixed_t &sety)
   {
      setx = straightNodes[index].x;
      sety = straightNodes[index].y;
   }
   // get finality
   void getFinalCoord(fixed_t &destx, fixed_t &desty)
   {
      destx = finalx;
      desty = finaly;
   }
   // previous index accessor (or -1)
	int getPrevIndex(int index)
	{
		return index >= 0 ? nodes[index].prev : -1;
	}
   // next index accessor (or -1)
	int getNextStraightIndex(int index)
	{
		return index >= 0 ? straightNodes[index].next : -1;
	}
   // numNodes accessor
	int length() const {return (int)nodes.getLength();}
   // Empty the array (FIXME: merge with reset?)
	void makeEmpty()
   {
      nodes.makeEmpty();
      numOpenNodes = 0;
      ssNodeMap.clear();
      pathexists = false;
   }
   void clear()
   {
      nodes.clear();
      numOpenNodes = 0;
      ssNodeMap.clear();
      pathexists = false;
   }

   //
   // Constructor
   //
	PathArray() : numOpenNodes(0), pathexists(false), finalx(0), finaly(0)
	{
      random.initialize((unsigned)time(nullptr));
	}
   // pathexists disabling accessor
	void reset() {pathexists = false; }
};

#endif /* defined(__EternityEngine__b_path__) */

// EOF

