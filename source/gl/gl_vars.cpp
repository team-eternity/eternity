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

#include "../z_zone.h"
#include "../c_io.h"
#include "../c_runcmd.h"

#include "gl_vars.h"

// NB: These variables exist even if EE_FEATURE_OPENGL is disabled.

int  cfg_gl_colordepth;      // colordepth of GL video mode
int  cfg_gl_filter_type;     // texture filtering type
int  cfg_gl_texture_format;  // texture internal format
bool cfg_gl_use_extensions;  // must be true for extensions to be used
bool cfg_gl_arb_pixelbuffer; // enable ARB PBO extension

VARIABLE_INT(cfg_gl_colordepth, nullptr, 16, 32, nullptr);
CONSOLE_VARIABLE(gl_colordepth, cfg_gl_colordepth, 0) {}

static const char *gl_filter_names[] = { "GL_LINEAR", "GL_NEAREST" };

VARIABLE_INT(cfg_gl_filter_type, nullptr, CFG_GL_LINEAR, CFG_GL_NEAREST, gl_filter_names);
CONSOLE_VARIABLE(gl_filter_type, cfg_gl_filter_type, 0) {}

VARIABLE_TOGGLE(cfg_gl_use_extensions, nullptr, yesno);
CONSOLE_VARIABLE(gl_use_extensions, cfg_gl_use_extensions, 0) {}

VARIABLE_TOGGLE(cfg_gl_arb_pixelbuffer, nullptr, yesno);
CONSOLE_VARIABLE(gl_arb_pixelbuffer, cfg_gl_arb_pixelbuffer, 0) {}

// EOF

