// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//   C/S Network ID assignment.
//
//-----------------------------------------------------------------------------

#ifndef __CS_NETID_H__
#define __CS_NETID_H__

#include "e_hash.h"
#include "m_dllist.h"
#include "p_mobj.h"
#include "p_spec.h"

#define NETID_MAX D_MAXINT
#define NETID_MAX_LOAD_FACTOR 0.7

// [CG] Just guessing at the proper amount, will probably need tuning.
#define NETID_INIT_CHAINS 8192

// [CG] Because there's a few of these and so much of the code is boilerplate,
//      I've used the preprocessor to handle a lot of the work.

#define DECLARE_NETID_LOOKUP(type, name, funcname) \
   typedef struct name##_netid_s\
   {\
      mdllistitem_t link;\
      type *name;\
   } name##_netid_t;\
   extern ehash_t *name##_by_netid;\
   void CS_Obtain##funcname##NetID(type *name);\
   void CS_Release##funcname##NetID(type *name);\
   void CS_Register##funcname##NetID(type *name);\
   void CS_Reset##funcname##NetIDs(void);\
   type* CS_Get##funcname##FromNetID(unsigned int net_id);\
   name##_netid_t* CS_Iterate##funcname##s(name##_netid_t *node)

DECLARE_NETID_LOOKUP(mobj_t, actor, Actor);
DECLARE_NETID_LOOKUP(ceiling_t, ceiling, Ceiling);
DECLARE_NETID_LOOKUP(vldoor_t, door, Door);
DECLARE_NETID_LOOKUP(floormove_t, floor, Floor);
DECLARE_NETID_LOOKUP(elevator_t, elevator, Elevator);
DECLARE_NETID_LOOKUP(pillar_t, pillar, Pillar);
DECLARE_NETID_LOOKUP(floorwaggle_t, floorwaggle, FloorWaggle);
DECLARE_NETID_LOOKUP(plat_t, platform, Platform);

void CS_InitNetIDs(void);
void CS_ResetNetIDs(void);

#endif

