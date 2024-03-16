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
   // DOOM Cheats
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
   CHEAT_IDDT,
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
   CHEAT_IAMTHEONE,
   CHEAT_KILLEM,

   // Heretic Cheats
   CHEAT_HTICGOD,
   CHEAT_HTICHEALTH,
   CHEAT_HTICIDDQD,
   CHEAT_HTICKEYS,
   CHEAT_HTICKILL,
   CHEAT_HTICNOCLIP,
   CHEAT_HTICWARP,
   CHEAT_HTICMAP,
   CHEAT_HTICPOWERV,
   CHEAT_HTICPOWERG,
   CHEAT_HTICPOWERA,
   CHEAT_HTICPOWERT,
   CHEAT_HTICPOWERF,
   CHEAT_HTICPOWERR,
   CHEAT_HTICPOWER,
   CHEAT_HTICGIMME,
   CHEAT_HTICRAMBO,

   // Shared Cheats
   CHEAT_COMP,
   CHEAT_HOM,
   CHEAT_TRAN,
   CHEAT_ICE,
   CHEAT_PUSH,
   CHEAT_NUKE,
   CHEAT_HIDEME,
   CHEAT_GHOST,
   CHEAT_INFSHOTS,
   CHEAT_SILENCE,

#ifdef INSTRUMENTED
   // Debug Cheats
   CHEAT_STAT,
#endif

   CHEAT_END,
   CHEAT_NUMCHEATS
};

// killough 4/16/98: Cheat table structure
struct cheat_s 
{
   const char *cheat;
   const int gametype;
   const int when;
   void (*const func)(const void *);
   const int arg;
   uint64_t code, mask;
   bool deh_disabled;                // killough 9/12/98
};

extern cheat_s cheat[CHEAT_NUMCHEATS];

bool M_FindCheats(int key);
void M_DoCheat(const char *cheatname);
int M_NukeMonsters();

//
// Shared both by cheats and by vanilla Heretic demos
//
static constexpr char const *hartiNames[] =
{
   "ArtiInvulnerability",
   "ArtiInvisibility",
   "ArtiHealth",
   "ArtiSuperHealth",
   "ArtiTomeOfPower",
   "ArtiTorch",
   "ArtiTimeBomb",
   "ArtiEgg",
   "ArtiFly",
   "ArtiTeleport"
};

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
