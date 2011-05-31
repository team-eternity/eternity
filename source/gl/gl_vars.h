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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   
//  OpenGL Configuration Variables
//  haleyjd 05/30/11
//
//-----------------------------------------------------------------------------

#ifndef GL_VARS_H__
#define GL_VARS_H__

extern int   cfg_gl_colordepth;
extern char *cfg_gl_filter_type;
extern char *cfg_gl_texture_format;
extern bool  cfg_gl_use_extensions;
extern bool  cfg_gl_arb_pixelbuffer;

void GL_AddCommands();

#endif

// EOF

