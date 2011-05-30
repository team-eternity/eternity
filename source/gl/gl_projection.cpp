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
//  OpenGL Projection Functions
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#ifdef EE_FEATURE_OPENGL

#include "gl_includes.h"

//
// GL_SetOrthoMode
//
// Changes to a 2D orthogonal projection.
//
void GL_SetOrthoMode(int w, int h)
{
   // Clear model-view matrix
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   // Set projection matrix to a standard orthogonal projection
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0.0, (GLdouble)w, (GLdouble)h, 0.0, -1.0, 1.0);

   // Disable depth buffer test
   glDisable(GL_DEPTH_TEST);
}

#endif

// EOF

