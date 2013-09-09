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
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

// The First Commandment of EE: Thou shalt include z_zone.h. Thus sayeth the LORD.
#include "z_zone.h"

#include "doomstat.h"
#include "m_bbox.h"
#include "m_collection.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_portalclip.h"
#include "p_portal.h"
#include "p_spec.h"
#include "p_traceengine.h"
#include "r_defs.h"
#include "r_state.h"



// --------------------------------------------------------------------------------------
// PortalClip
static int groupcells;





bool PortalClipEngine::changeSector(sector_t *sector, int crunch, ClipContext *cc)
{
   return false;
}

bool PortalClipEngine::checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil, ClipContext *cc)
{
   return false;
}

bool PortalClipEngine::checkSides(Mobj *actor, int x, int y, ClipContext *cc)
{
   return false;
}

int  PortalClipEngine::getMoveFactor(Mobj *mo, int *friction)
{
   return FRACUNIT;
}

int  PortalClipEngine::getFriction(const Mobj *mo, int *factor)
{
   return FRACUNIT;
}

void PortalClipEngine::applyTorque(Mobj *mo, ClipContext *cc)
{
}

void PortalClipEngine::radiusAttack(Mobj *spot, Mobj *source, int damage, int distance, int mod, unsigned int flags, ClipContext *cc)
{
}

fixed_t PortalClipEngine::avoidDropoff(Mobj *actor, ClipContext *cc)
{
   return 0;
}
      
// Utility functions
ClipContext*  PortalClipEngine::getContext()
{
   ClipContext *ret;

   if(unused == NULL) {
      ret = new ClipContext();
      ret->setEngine(this);
      ret->markedgroups = ret->markedsectors = NULL;
   }
   else {  
      ret = unused;
      unused = unused->next;
      ret->next = NULL;
   }

   if(ret->markedgroups == NULL)
      ret->markedgroups = new (PU_LEVEL) MarkVector(P_PortalGroupCount());
   if(ret->markedsectors == NULL)
      ret->markedsectors = new (PU_LEVEL) MarkVector(numsectors);

   return ret;
}


void PortalClipEngine::freeContext(ClipContext *cc)
{
   cc->next = unused;
   unused = cc;
}


//
// mapLoaded
// 
void PortalClipEngine::mapLoaded()
{
   ClipContext *cc = unused;
   while(cc)
   {
      cc->markedgroups = NULL;
      cc->markedsectors = NULL;
      cc = cc->next;
   }
}



// --------------------------------------------------------------------------------------
// MarkVector

MarkVector::MarkVector(size_t numItems) : ZoneObject()
{
   arraySize = ((numItems + 7) & ~7) / 8;
   markArray = ecalloctag(byte *, arraySize, sizeof(byte), PU_STATIC, NULL);
}


MarkVector::~MarkVector() 
{
   efree(markArray);
   arraySize = 0;
}


void MarkVector::clearMarks()
{
   memset(markArray, 0, sizeof(byte) * arraySize);
}


void MarkVector::mark(size_t itemIndex)
{
   markArray[itemIndex >> 3] |= 1 << (itemIndex & 7);
}