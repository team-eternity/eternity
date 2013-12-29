// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
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

// Supported texture filtering types
enum
{
   CFG_GL_LINEAR,
   CFG_GL_NEAREST,
   CFG_GL_NUMFILTERS
};

// Supported internal texture format values
enum
{
   CFG_GL_R3_G3_B2,
   CFG_GL_RGB4,
   CFG_GL_RGB5,
   CFG_GL_RGB8,
   CFG_GL_RGB10,
   CFG_GL_RGB12,
   CFG_GL_RGB16,
   CFG_GL_RGBA2,
   CFG_GL_RGBA4,
   CFG_GL_RGB5_A1,
   CFG_GL_RGBA8,
   CFG_GL_RGB10_A2,
   CFG_GL_RGBA12,
   CFG_GL_RGBA16,
   CFG_GL_NUMTEXFORMATS
};

extern int  cfg_gl_colordepth;
extern int  cfg_gl_filter_type;
extern int  cfg_gl_texture_format;
extern bool cfg_gl_use_extensions;
extern bool cfg_gl_arb_pixelbuffer;

void GL_AddCommands();

#endif

// EOF

