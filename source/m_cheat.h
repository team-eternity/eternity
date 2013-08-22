// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Cheat code checking.
//
//-----------------------------------------------------------------------------

#ifndef M_CHEAT_H__
#define M_CHEAT_H__

enum 
{ 
   always   = 0,
   not_dm   = 1,
   not_coop = 2,
   not_demo = 4, 
   not_net  = not_dm | not_coop,
   not_sync = not_net | not_demo
}; 

// haleyjd 08/21/13: cheat enumeration
enum cheatnum_e
{
   CHEAT_IDMUS,
   CHEAT_IDCHOPPERS,
   CHEAT_IDDQD,
   CHEAT_IDK,
   CHEAT_IDKFA,
   CHEAT_IDFA,
   CHEAT_IDSPISPOPD,
   CHEAT_IDCLIP,
   CHEAT_IDBEHOLDV,
   CHEAT_IDBEHOLDS,
   CHEAT_IDBEHOLDI,
   CHEAT_IDBEHOLDR,
   CHEAT_IDBEHOLDA,
   CHEAT_IDBEHOLDL,
   CHEAT_IDBEHOLD,
   CHEAT_IDCLEV,
   CHEAT_IDMYPOS,
   CHEAT_COMP,
   CHEAT_KILLEM,
   CHEAT_IDDT,
   CHEAT_HOM,
   CHEAT_KEY,
   CHEAT_KEYR,
   CHEAT_KEYY,
   CHEAT_KEYB,
   CHEAT_KEYRC,
   CHEAT_KEYYC,
   CHEAT_KEYBC,
   CHEAT_KEYRS,
   CHEAT_KEYYS,
   CHEAT_KEYBS,
   CHEAT_WEAP,
   CHEAT_WEAPX,
   CHEAT_AMMO,
   CHEAT_AMMOX,
   CHEAT_TRAN,
   CHEAT_ICE,
   CHEAT_PUSH,
   CHEAT_NUKE,
   CHEAT_HIDEME,
   CHEAT_GHOST,
   CHEAT_INFSHOTS,
   CHEAT_SILENCE,
   CHEAT_IAMTHEONE,

#ifdef INSTRUMENTED
   CHEAT_STAT,
#endif

   CHEAT_END,
   CHEAT_NUMCHEATS
};

// killough 4/16/98: Cheat table structure
struct cheat_s 
{
   const char *cheat;
   const int when;
   void (*const func)(const void *);
   const int arg;
   uint64_t code, mask;
   bool deh_disabled;                // killough 9/12/98
};

extern cheat_s cheat[CHEAT_NUMCHEATS];

bool M_FindCheats(int key);
void M_DoCheat(char *cheatname);

void M_AddNukeSpec(int mobjType, void (*func)(Mobj *)); // haleyjd
void M_CopyNukeSpec(int destType, int srcType);

extern int idmusnum;

#endif

//----------------------------------------------------------------------------
//
// $Log: m_cheat.h,v $
// Revision 1.5  1998/05/03  22:10:56  killough
// Cheat engine, moved from st_stuff
//
// Revision 1.4  1998/05/01  14:38:08  killough
// beautification
//
// Revision 1.3  1998/02/09  03:03:07  killough
// Rendered obsolete by st_stuff.c
//
// Revision 1.2  1998/01/26  19:27:08  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
