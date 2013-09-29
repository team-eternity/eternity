// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 Stephen McGranahan
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Linked portals
//      SoM created 02/13/06
//
//-----------------------------------------------------------------------------

#ifndef P_LINKOFFSET_H__
#define P_LINKOFFSET_H__

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that 
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif

struct linkoffset_t
{
   fixed_t x, y, z;
};

extern linkoffset_t **linktable;
extern linkoffset_t zerolink;


//
// P_GetLinkOffset
//
// This function returns a linkoffset_t object which contains the map-space
// offset to get from the startgroup to the targetgroup. This will always return 
// a linkoffset_t object. In cases of invalid input or no link the offset will be
// (0, 0, 0)
#ifdef RANGECHECK
linkoffset_t *P_GetLinkOffset(int startgroup, int targetgroup);
#else
int P_PortalGroupCount();
extern bool useportalgroups;

inline linkoffset_t *P_GetLinkOffset(int startgroup, int targetgroup)
{
   if (!linktable || !useportalgroups || startgroup < 0 || startgroup >= P_PortalGroupCount() || targetgroup < 0 || targetgroup >= P_PortalGroupCount())
      return &zerolink;
   else
   {
      auto link = linktable[startgroup * P_PortalGroupCount() + targetgroup];
      return link ? link : &zerolink;
   }
}
#endif


//
// P_GetLinkIfExists
//
// Returns a link offset to get from 'fromgroup' to 'togroup' if one exists. 
// Returns NULL otherwise
#ifdef RANGECHECK
linkoffset_t *P_GetLinkIfExists(int fromgroup, int togroup);
#else
inline linkoffset_t *P_GetLinkIfExists(int fromgroup, int togroup)
{
   return (!linktable || !useportalgroups || fromgroup < 0 || fromgroup >= P_PortalGroupCount() || togroup < 0 || togroup >= P_PortalGroupCount())
      ? NULL
      : linktable[fromgroup * P_PortalGroupCount() + togroup];
}
#endif

#endif

// EOF

