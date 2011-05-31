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

// NB: These variables exist even if EE_FEATURE_OPENGL is disabled.

int   cfg_gl_colordepth;      // colordepth of GL video mode
char *cfg_gl_filter_type;     // texture filtering type
char *cfg_gl_texture_format;  // texture internal format
bool  cfg_gl_use_extensions;  // must be true for extensions to be used
bool  cfg_gl_arb_pixelbuffer; // enable ARB PBO extension

VARIABLE_INT(cfg_gl_colordepth, NULL, 16, 32, NULL);
CONSOLE_VARIABLE(gl_colordepth, cfg_gl_colordepth, cf_handlerset)
{
   int bpp;

   if(Console.argc < 1)
      return;

   bpp = Console.argv[0]->toInt();

   switch(cfg_gl_colordepth)
   {
   case 16: // these values are fine
   case 32:
      cfg_gl_colordepth = bpp;
      break;
   default:
      C_Printf("Not a valid colordepth value (16, 32)");
   }
}

VARIABLE_STRING(cfg_gl_filter_type, NULL, UL);
CONSOLE_VARIABLE(gl_filter_type, cfg_gl_filter_type, cf_handlerset)
{
   if(Console.argc < 1)
      return;

   if(*Console.argv[0] != "GL_LINEAR" &&
      *Console.argv[0] != "GL_NEAREST")
   {
      C_Printf("Not a valid filter type (GL_LINEAR, GL_NEAREST)");
      return;
   }

   if(cfg_gl_filter_type)
      free(cfg_gl_filter_type);

   cfg_gl_filter_type = Console.argv[0]->duplicate(PU_STATIC);
}

// TODO: cfg_gl_texture_format

VARIABLE_TOGGLE(cfg_gl_use_extensions, NULL, yesno);
CONSOLE_VARIABLE(gl_use_extensions, cfg_gl_use_extensions, 0) {}

VARIABLE_TOGGLE(cfg_gl_arb_pixelbuffer, NULL, yesno);
CONSOLE_VARIABLE(gl_arb_pixelbuffer, cfg_gl_arb_pixelbuffer, 0) {}

void GL_AddCommands()
{
   C_AddCommand(gl_colordepth);
   C_AddCommand(gl_filter_type);
   C_AddCommand(gl_use_extensions);
   C_AddCommand(gl_arb_pixelbuffer);
}

// EOF

