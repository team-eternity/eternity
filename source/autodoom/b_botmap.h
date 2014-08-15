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

#ifndef B_BOTMAP_H_
#define B_BOTMAP_H_

#include <forward_list>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "b_intset.h"
#include "b_msector.h"
#include "b_util.h"
#include "../e_rtti.h"
#include "../m_collection.h"
#include "../m_dllist.h"
#include "../m_fixed.h"
#include "../m_vector.h"
#include "../p_map3d.h"
#include "../r_defs.h"

#define BOTMAPBLOCKUNITS 128
#define BOTMAPBLOCKSIZE (BOTMAPBLOCKUNITS << FRACBITS)
#define BOTMAPBLOCKSHIFT   (FRACBITS+BOTMAPBTOFRAC)
#define BOTMAPBMASK        (BOTMAPBLOCKSIZE-1)

extern const char* const KEY_JSON_VERTICES;
extern const char* const KEY_JSON_SEGS;
extern const char* const KEY_JSON_LINES;
extern const char* const KEY_JSON_SSECTORS;
extern const char* const KEY_JSON_NODES;
extern const char* const KEY_JSON_METASECTORS;

//typedef std::unordered_set<int> IntSet;
//typedef std::set<int> IntOSet;

//
// BotMap
//
// The final bot map, generated thanks to TempBotMap and glBSP
//
class BotMap : public ZoneObject
{
public:
   //
   // Vertex
   //
   // Simple 2D coordinate
   //
   struct Vertex
   {
      fixed_t x, y;
   } *vertices;
   int numverts;
   
   //
   // Line
   //
   // Line uniting two metasectors
   //
   struct Line
   {
      const Vertex *v[2];  // stard, end
      const MetaSector *msec[2]; // right left
      const line_t* specline;
   } *lines;
   int numlines;
   
   //
   // Seg
   //
   // Segment between two subsectors
   //
   class Subsec;
   class Seg
   {
   public:
      const Vertex *v[2];  // start, end
      fixed_t dx, dy;      // helper deltas
      const Line *ln;      // owner line (if any)
      bool isback;         // whether it's on the back side of line
      Seg *partner;  // partner seg
      
      fixed_t bbox[4];     // bounding box

      v2fixed_t mid;       // middle
      Subsec *owner; // subsector of this seg
      
      PODCollection<int> blocklist; // list of touching map blocks
   };
   Collection<Seg> segs;
   int numsegs;
   
   //
   // Subsec
   //
   // Subsector defined by segments
   //
   struct Neigh
   {
       const Subsec*    ss;
       const Seg*       seg;
   };
   class Subsec
   {
   public:
      Seg *segs;
      const MetaSector *msector;
      int nsegs;
      std::unordered_set<const Mobj *> mobjlist;
      std::unordered_set<const line_t *> linelist;
      v2fixed_t mid;
      // Fast neighbour lookup
      PODCollection<Neigh> neighs;
   };
   Collection<Subsec> ssectors;
   int numssectors;
   
   //
   // Node
   //
   // glBSP node
   //
   struct Node
   {
      fixed_t x, y, dx, dy;
      int child[2];   // right, left
   } *nodes;
   int numnodes;
   
   DLList<MetaSector, &MetaSector::listLink> msecList;
   MetaSector *nullMSec;   // the metasector for walls (exclude from BSP)
   int nummetas;
   
   // use its own blockmap size, because it goes beyond the bounds of the
   // level blockmap
   fixed_t bMapOrgX, bMapOrgY;
   int bMapWidth, bMapHeight;
   Collection<PODCollection<Seg *> > segBlocks;
   fixed_t radius;
   
   std::unordered_map<const Mobj *, PODCollection<Subsec *> >
   mobjSecMap;
   std::unordered_map<const line_t *, PODCollection<Subsec *> >
   lineSecMap;
   
   //
   // Constructor
   //
   BotMap() : vertices(NULL), lines(NULL), ssectors(NULL),
   nodes(NULL),
   numverts(0), numlines(0), numsegs(0), numssectors(0), numnodes(0),
   nullMSec(NULL), nummetas(0),
   bMapOrgX(0), bMapOrgY(0), bMapWidth(0), bMapHeight(0), sectorFlags(nullptr)
   {
      msecList.head = nullptr;
      // set collection prototype
      static Seg protoSeg;
      memset(protoSeg.v, 0, sizeof(protoSeg.v));
      protoSeg.dx = protoSeg.dy = 0;
      protoSeg.ln = NULL;
      protoSeg.isback = false;
      protoSeg.partner = NULL;
      protoSeg.mid.x = protoSeg.mid.y = 0;
      protoSeg.owner = NULL;
      segs.setPrototype(&protoSeg);
      
      static Subsec protoSubsec;
      protoSubsec.segs = NULL;
      protoSubsec.msector = NULL;
      protoSubsec.nsegs = 0;
      ssectors.setPrototype(&protoSubsec);
      
      static PODCollection<Seg *> protoColl;
      segBlocks.setPrototype(&protoColl);
   }
   
   //
   // Destructor
   //
	
   ~BotMap()
   {
      efree(vertices);
      efree(lines);
      efree(nodes);

      efree(sectorFlags);
    
      clearMsecList();
      
   }
   
   void *operator new(size_t size, int tag, BotMap **user);
   void operator delete (void *p);
   void operator delete (void *p, int a, BotMap ** b);

   void createBlockMap();
   
   int pointOnSide(fixed_t x, fixed_t y, const Node &node) const;
   Subsec &pointInSubsector(fixed_t x, fixed_t y) const;
   void getTouchedBlocks(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                         const std::function<void(int)> &func) const;
   void getBoxTouchedBlocks(fixed_t top, fixed_t bottom,
                            fixed_t left, fixed_t right,
                            const std::function<void(int)> &func) const;
   void unsetThingPosition(const Mobj *thing);
   void setThingPosition(const Mobj *thing);
   
   void unsetLinePositions(const line_t &line);
   
   bool canPass(const Subsec &s1, const Subsec &s2, fixed_t height) const;
   bool canPassNow(const Subsec &s1, const Subsec &s2, fixed_t height) const;
   
   static void Build(); // The entry point from P_SetupLevel
   
   static bool demoPlayingFlag;
   // if playing, this flag will be true during P_SetupLevel

   // Common affairs
   std::set<const Mobj*> livingMonsters;    // list of shootable objects

   struct SectorTrait
   {
       int lockID;
       bool isDoor;
   };
   SectorTrait* sectorFlags;

private:
    void getAllLivingMonsters();
    void getDoorSectors();
    static void SpecialIsDoor(int n, SectorTrait& st);
	
	void clearMsecList()
	{
		DLListItem<MetaSector> *item, *next;
		for (item = msecList.head; item != nullptr; item = next)
		{
			next = item->dllNext;
			delete item->dllObject;
		}
	}
};
extern BotMap *botMap;

typedef BotMap::Subsec  BSubsec;
typedef BotMap::Seg     BSeg;
typedef BotMap::Neigh   BNeigh;

#endif /* defined(__EternityEngine__b_botmap__) */

// EOF

