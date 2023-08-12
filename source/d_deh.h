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
//-----------------------------------------------------------------------------
//
// DeHackEd Externals
//
//-----------------------------------------------------------------------------

#ifndef D_DEH_H__
#define D_DEH_H__

#define DEH_KEY_INFIGHTING_GROUP "Infighting group"
#define DEH_KEY_PROJECTILE_GROUP "Projectile group"
#define DEH_KEY_SPLASH_GROUP "Splash group"

// from g_game.c, prefix for savegame name like "boomsav"
extern const char *savegamename;

extern const char *s_QSPROMPT;
extern const char *s_QLPROMPT;

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//
// Ty 03/27/98 - externalized map name arrays - now in d_deh.c
// and converted to arrays of pointers to char *
// See modified HUTITLEx macros
//
extern const char *mapnames[];
extern const char *mapnames2[];
extern const char *mapnamesp[];
extern const char *mapnamest[];
extern const char *mapnamesh[];

extern bool deh_pars;

#endif

//--------------------------------------------------------------------
//
// $Log: d_deh.h,v $
// Revision 1.5  1998/05/04  21:36:33  thldrmn
// commenting, reformatting and savegamename change
//
// Revision 1.4  1998/04/10  06:47:29  killough
// Fix CVS stuff
//   
//
//--------------------------------------------------------------------

