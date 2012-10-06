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
//   Wad directory hacks for prominent broken wad files
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "m_hash.h"
#include "w_wad.h"

typedef void (*hackhandler_t)(filelump_t *, int);

//
// w_dirhack_t structure
//
// This structure stores a SHA-1 hash in string form and a routine which can
// handle the hack needed to repair that wad file's directory.
//
struct w_dirhack_t
{
   const char *sha1;
   hackhandler_t handler;
};

//
// Hack Handler Functions
//

//
// W_doGothic2Hack
//
// Gothic DM 2 has a strangely constructed run of flats which break the
// BOOM namespace coalescence algorithm. Fortunately, it's easily fixed.
//
static void W_doGothic2Hack(filelump_t *fileinfo, int numlumps)
{
   filelump_t *lump = NULL;
   int i;

   // scan backward and look for ADEL_Y08
   for(i = numlumps - 1; i >= 0; --i)
   {
      if(!strncasecmp(fileinfo[i].name, "ADEL_Y08", 8))
      {
         lump = &fileinfo[i];
         break;
      }
   }

   if(lump)
   {
      // Move ADEL_Y08 to the end of the wad directory, overwriting the
      // erroneous FF_START lump that is there.
      filelump_t *lastlump = &fileinfo[numlumps - 1];
      lastlump->filepos = lump->filepos;
      lastlump->size    = lump->size;
      strncpy(lastlump->name, lump->name, 8);

      // Now, replace the original ADEL_Y08 with an FF_START.
      lump->filepos = 0;
      lump->size    = 0;
      strncpy(lump->name, "FF_START", 8);
   }
}

//
// Wad Directory Hacks
//
// Some very prominent and historically significant wads such as GothicDM2
// are incompatible with BOOM's wad file handling, such as because of the
// namespace coalescence algorithm. This is an expandable list of hacks that
// can be applied based on a match to the SHA-1 hash computed from the wad
// file's header and directory during WadDirectory::AddFile.
//
static w_dirhack_t w_dirhacks[] =
{
   { "9a296941da455d0009ee3988b55d50ea363a4a84", W_doGothic2Hack }, // gothic2.wad
   
   // Terminator, must be last
   { NULL, NULL }
};

//
// W_CheckDirectoryHacks
//
// Scans the w_dirhacks array for a SHA-1 match with the passed-in checksum
// and calls the handler for that hack if one exists.
//
void W_CheckDirectoryHacks(const HashData &hash, filelump_t *fileinfo, int numlumps)
{
   HashData localHash = HashData(HashData::SHA1);
   w_dirhack_t *curhack = w_dirhacks;

   while(curhack->sha1)
   {
      localHash.stringToDigest(curhack->sha1);

      if(localHash == hash) 
      {
         // found a match
         curhack->handler(fileinfo, numlumps);
         return;
      }
      ++curhack;
   }
}

// EOF

