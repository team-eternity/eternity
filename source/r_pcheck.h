// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley, Stephen McGranahan, et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Inline linked portal predicates.
//      SoM created 12/23/07
//
//-----------------------------------------------------------------------------

#ifndef R_PORTALCHECK_H__
#define R_PORTALCHECK_H__

#include "r_portal.h"
#include "r_defs.h"

extern int demo_version;

//
// R_FPCam
//
// haleyjd 3/17/08: Convenience routine to clean some shit up.
//
inline static linkdata_t *R_FPLink(const sector_t *s)
{
   return &(s->srf.floor.portal->data.link);
}

//
// R_CPCam
//
// ditto
//
inline static linkdata_t *R_CPLink(const sector_t *s)
{
   return &(s->srf.ceiling.portal->data.link);
}


#endif

//----------------------------------------------------------------------------
//
// $Log: r_pcheck.h,v $
//
//----------------------------------------------------------------------------
