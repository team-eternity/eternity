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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// Utilities for storing and checking command-line parameters.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_io.h"   // SoM 3/14/2002: strncasecmp


int    myargc;
char **myargv;

//
// M_CheckParm
//
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
//
int M_CheckParm(const char *check)
{
   int i;

   for(i = 1; i < myargc; ++i)
   {
      if(!strcasecmp(check, myargv[i]))
         return i;
   }

   return 0;
}

//
// M_CheckMultiParm
//
// haleyjd 01/17/11: Allows several aliases for a command line parameter to be
// checked in turn.
//
int M_CheckMultiParm(const char **parms, int numargs)
{
   int i = 0;
   const char *parm;

   while((parm = parms[i++]))
   {
      int p = M_CheckParm(parm);

      // it is only found if the expected number of arguments are available
      if(p && p < myargc - numargs)
         return p;
   }

   return 0; // none were found
}

//----------------------------------------------------------------------------
//
// $Log: m_argv.c,v $
// Revision 1.5  1998/05/03  22:51:40  killough
// beautification
//
// Revision 1.4  1998/05/01  14:26:14  killough
// beautification
//
// Revision 1.3  1998/05/01  14:23:29  killough
// beautification
//
// Revision 1.2  1998/01/26  19:23:40  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
