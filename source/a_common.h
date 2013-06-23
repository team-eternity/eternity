// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//      Action Pointer Functions
//      that are associated with states/frames.
//
//      Common action functions shared by all games.
//
//-----------------------------------------------------------------------------

#ifndef A_COMMON_H__
#define A_COMMON_H__

// Required for pr_class_t:
#include "m_random.h"

struct actionargs_t;
class  Mobj;

void P_MakeSeeSound(Mobj *actor, pr_class_t rngnum);

void A_Chase(actionargs_t *actionargs);
void A_Die(actionargs_t *actionargs);
void A_Explode(actionargs_t *actionargs);
void A_FaceTarget(actionargs_t *actionargs);
void A_Fall(actionargs_t *actionargs);
void A_Look(actionargs_t *actionargs);
void A_Pain(actionargs_t *actionargs);

#endif

// EOF

