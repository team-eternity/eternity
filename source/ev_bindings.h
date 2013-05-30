// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//   Generalized line action system - Bindings
//
//-----------------------------------------------------------------------------

#ifndef EV_BINDINGS_H__
#define EV_BINDINGS_H__

extern ev_action_t  NullAction;
extern ev_action_t  BoomGenAction;

extern ev_binding_t DOOMBindings[];
extern ev_binding_t HereticBindings[];
extern ev_binding_t HexenBindings[];

extern size_t DOOMBindingsLen;
extern size_t HereticBindingsLen;
extern size_t HexenBindingsLen;

#endif

// EOF

