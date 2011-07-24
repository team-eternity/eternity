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

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_states.h"
#include "e_things.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_map3d.h"
#include "p_mapcontext.h"
#include "p_portalclip.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_traceengine.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_segs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"
#include "v_video.h"



// ----------------------------------------------------------------------------
// PortalClip

ClipContext*  PortalClipEngine::getContext()
{
   if(unused == NULL) {
      ClipContext *ret = new ClipContext();
      ret->setEngine(this);
      return ret;
   }
   
   ClipContext *res = unused;
   
   unused = unused->next;
   res->next = NULL;
   return res;
}


void PortalClipEngine::freeContext(ClipContext *cc)
{
   cc->next = unused;
   unused = cc;
}