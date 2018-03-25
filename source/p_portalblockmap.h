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

#ifndef P_PORTALCROSS_H_
#define P_PORTALCROSS_H_

#include "m_collection.h"

struct line_t;
struct linkdata_t;
struct portal_t;
struct sector_t;

//
// Kind of portal block type
//
enum class portalblocktype_e
{
   line,
   sector
};

//
// Portal block entry
//
struct portalblockentry_t
{
   const linkdata_t *ldata;   // the concerned linked portal data.
   portalblocktype_e type;    // whether it is sector or line based.
   union
   {
      struct
      {
         const line_t *line;  // source line. Unique per block.
         int linerefindex;    // index of this entry in the lineref array.
      };
      const sector_t *sector; // source sector. Pair sector/ldata unique per block.
   };   
};

//
// Portal blockmap
//
class PortalBlockmap final
{
public:
   void mapInit();
   void unlinkLine(const line_t &line);
   void linkLine(const line_t &line);

private:
   //
   // Line reference in blockmap
   //
   struct lineref_t
   {
      int mapindex;     // position in blockmap
      int blockindex;   // position in block
   };

   // The blockmap. As big as the map.
   Collection<PODCollection<portalblockentry_t>> mBlocks;

   // line references to their owning blocks. Useful for moving lines. As big as numlines.
   Collection<PODCollection<lineref_t>> mLineRef;

   // Set to true during initialization
   bool mInitializing;

   void checkLinkSector(const sector_t &sector, const portal_t *portal, int mapindex);
};

extern PortalBlockmap gPortalBlockmap;

#endif

// EOF
