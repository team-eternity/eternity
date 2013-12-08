// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//      Clipping and Tracer contexts
//
//-----------------------------------------------------------------------------


#ifndef P_MAPCONTEXT_H
#define P_MAPCONTEXT_H


struct line_t;
struct msecnode_t;
struct player_t;
struct sector_t;
class  ClipEngine;
class  TracerEngine;
class  ClipContext;
class  TracerContext;
class  MarkVector;


#include "m_collection.h"
#include "p_mobj.h"

class MapContext
{
   public:
      MapContext();
      virtual ~MapContext();
      
      virtual ClipContext *clipContext(void) { I_Error("Internal error: MapContext::clipContext called on non ClipContext object."); return NULL; }
      virtual TracerContext *tracerContext(void) { I_Error("Internal error: MapContext::tracerContext called on non TracerContext object."); return NULL; }
      virtual void done() = 0;

      MarkVector *getMarkedLines();

   private:
      MarkVector *markedlines;
};


// SoM: This functions as a clipping 'context' now.
class ClipContext : public MapContext
{
   public:
      ClipContext();
      virtual ~ClipContext();
      
      void setEngine(ClipEngine *ce);
      virtual void done();
      
      virtual ClipContext *clipContext(void) { return this; }
      
      
      // SoM 09/07/02: Solution to problem of monsters walking on 3dsides
      // haleyjd: values for tmtouch3dside:
      // 0 == no 3DMidTex involved in clipping
      // 1 == 3DMidTex involved but not responsible for floorz
      // 2 == 3DMidTex responsible for floorz
      int        touch3dside;

      Mobj       *thing;    // current thing being clipped
      fixed_t    x;         // x position, usually where we want to move
      fixed_t    y;         // y position, usually where we want to move
      fixed_t    z;         // z position, usually where we want to move
      fixed_t    height;    // height of the thing

      fixed_t    bbox[4];   // bounding box for thing/line intersection checks
      fixed_t    floorz;    // floor you'd hit if free to fall
      fixed_t    ceilingz;  // ceiling of sector you're in
      fixed_t    dropoffz;  // dropoff on other side of line you're crossing

      fixed_t    secfloorz; // SoM: floorz considering only sector heights
      fixed_t    secceilz;  // SoM: ceilingz considering only sector heights
      
      fixed_t    passfloorz; // SoM 11/6/02: UGHAH
      fixed_t    passceilz;

      int        floorpic;  // haleyjd: for CANTLEAVEFLOORPIC flag

      int        unstuck;   // killough 8/1/98: whether to allow unsticking

      bool       floatok;   // If "floatok" true, move ok if within floorz - ceilingz   
      bool       felldown;  // killough 11/98: if "felldown" true, object was pushed down ledge

      // keep track of the line that lowers the ceiling,
      // so missiles don't explode against sky hack walls
      line_t     *ceilingline;
      line_t     *blockline;   // killough 8/11/98: blocking linedef
      line_t     *floorline;   // killough 8/1/98: Highest touched floor

      // Temporary holder for thing_sectorlist threads
      // haleyjd: this is now *only* used inside P_CreateSecNodeList and callees
      msecnode_t *sector_list;     // phares 3/16/98
      
      Mobj       *BlockingMobj;    // haleyjd 1/17/00: global hit reference
      
      fixed_t    dropoff_deltax;
      fixed_t    dropoff_deltay;
      fixed_t    dropoff_floorz;

      // Spechit stuff
      PODCollection<line_t *> spechit;
      
      // Group the center of the thing is inside
      int centergroup;
      
      // All the portal groups the mobj would be touching in the new position.
      PODCollection<int> adjacent_groups;

      MarkVector *getMarkedGroups();
      MarkVector *getMarkedSectors();
      
   private:
      MarkVector *markedgroups;
      MarkVector *markedsectors;

      ClipEngine *from;

      ClipContext *next;
      friend class ClipEngine;
      friend class PortalClipEngine;
};



// SoM: had to move this here
struct divline_t
{
   fixed_t     x;
   fixed_t     y;
   fixed_t     dx;
   fixed_t     dy;
};

class TracerContext : public MapContext
{
   public:
      TracerContext();
      virtual ~TracerContext();
      
      void setEngine(TracerEngine *te);
      virtual void done();
      
      virtual TracerContext *tracerContext() { return this; }
      
      divline_t   divline;

      // Moved crappy globals here
      fixed_t     z; // replaces shootz
      int         la_damage;
      fixed_t     attackrange;
      fixed_t     aimslope;
      fixed_t     topslope, bottomslope;
      
      fixed_t     originx;
      fixed_t     originy;
      fixed_t     originz;
      
      fixed_t     sin;
      fixed_t     cos;

      // Accumulated travel along the line. Should be the XY distance between (x,y) 
      // and (originx, originy) 
      fixed_t     movefrac;      

      Mobj        *shootthing;
      Mobj        *linetarget;  // who got hit (or NULL)
      
      // From p_pspr.cpp
      fixed_t     bulletslope;      
      
   private:
      TracerEngine *from;
      
      TracerContext *next;
      friend class TracerEngine;
};
#endif