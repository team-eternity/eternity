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

#include "../z_zone.h"
#include "../c_io.h"
#include "../c_runcmd.h"
#include "../m_misc.h"

#include "gl_vars.h"

// NB: These variables exist even if EE_FEATURE_OPENGL is disabled.

int  cfg_gl_colordepth;      // colordepth of GL video mode
int  cfg_gl_filter_type;     // texture filtering type
int  cfg_gl_texture_format;  // texture internal format
bool cfg_gl_use_extensions;  // must be true for extensions to be used
bool cfg_gl_arb_pixelbuffer; // enable ARB PBO extension

VARIABLE_INT(cfg_gl_colordepth, NULL, 16, 32, NULL);
CONSOLE_VARIABLE(gl_colordepth, cfg_gl_colordepth, 0) {}

static const char *gl_filter_names[] = { "GL_LINEAR", "GL_NEAREST" };

VARIABLE_INT(cfg_gl_filter_type, NULL, CFG_GL_LINEAR, CFG_GL_NEAREST, gl_filter_names);
CONSOLE_VARIABLE(gl_filter_type, cfg_gl_filter_type, 0) {}

static const char *gl_int_texture_names[] =
{
   "R3_G3_B2",
   "RGB4",
   "RGB5",
   "RGB8",
   "RGB10",
   "RGB12",
   "RGB16",
   "RGBA2",
   "RGBA4",
   "RGB5_A1",
   "RGBA8",
   "RGB10_A2",
   "RGBA12",
   "RGBA16"
};

VARIABLE_INT(cfg_gl_texture_format, NULL, CFG_GL_R3_G3_B2, CFG_GL_RGBA16, gl_int_texture_names);
CONSOLE_VARIABLE(gl_texture_format, cfg_gl_texture_format, 0) {}

VARIABLE_TOGGLE(cfg_gl_use_extensions, NULL, yesno);
CONSOLE_VARIABLE(gl_use_extensions, cfg_gl_use_extensions, 0) {}

VARIABLE_TOGGLE(cfg_gl_arb_pixelbuffer, NULL, yesno);
CONSOLE_VARIABLE(gl_arb_pixelbuffer, cfg_gl_arb_pixelbuffer, 0) {}

//
// GL_AddCommands
//
// Add all GL-related console variables.
//
void GL_AddCommands()
{
   C_AddCommand(gl_colordepth);
   C_AddCommand(gl_filter_type);
   C_AddCommand(gl_texture_format);
   C_AddCommand(gl_use_extensions);
   C_AddCommand(gl_arb_pixelbuffer);
}

// EOF

