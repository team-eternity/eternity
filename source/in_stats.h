// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// 
// Statistics Saving and Loading
//
//-----------------------------------------------------------------------------

#ifndef IN_STATS_H__
#define IN_STATS_H__

#include "m_dllist.h"

struct wbstartstruct_t;

struct in_stat_t
{
   DLListItem<in_stat_t> links; // hash links

   const char *levelkey;   // filename::levelname
   const char *playername; // player who set the record
   int skill;              // skill level of the record
   int recordType;         // type of record
   int value;              // value of the record
   int maxValue;           // maximum, if any for this type
};

class INStatsMgrPimpl;

class INStatsManager
{
private:
   INStatsMgrPimpl *pImpl;

protected:
   // protected constructor for singleton instance
   INStatsManager();
   static INStatsManager singleton;

   void loadStats();

public:
   static void Init();
   static INStatsManager &Get() { return singleton; }

   void recordStats(const wbstartstruct_t *stats);
};

#endif

// EOF

