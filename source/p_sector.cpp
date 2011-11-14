// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//   Sector effects.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "m_fixed.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "r_defs.h"

//=============================================================================
//
// SectorThinker class methods
//

IMPLEMENT_THINKER_TYPE(SectorThinker, Thinker)

//
// SectorThinker::serialize
//
void SectorThinker::serialize(SaveArchive &arc)
{
   Thinker::serialize(arc);

   arc << sector;

   // when reloading, attach to sector
   if(arc.isLoading())
   {
      switch(getAttachPoint())
      {
      case ATTACH_FLOOR:
         sector->floordata = this;
         break;
      case ATTACH_CEILING:
         sector->ceilingdata = this;
         break;
      case ATTACH_FLOORCEILING:
         sector->floordata   = this;
         sector->ceilingdata = this;
         break;
      case ATTACH_LIGHT:
         sector->lightingdata = this;
         break;
      default:
         break;
      }
   }
}

// EOF

