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
//   Wad directory hacks for prominent broken wad files
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_iwad.h"
#include "m_hash.h"
#include "w_levels.h"
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
   filelump_t *lump = nullptr;
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
// W_doNerveHack
//
// If NERVE.WAD (No Rest for the Living) is added on the command line, rather
// than through the managed directory system, I still want to load its metadata.
//
static void W_doNerveHack(filelump_t *fileinfo, int numlumps)
{
   static bool firsttime = true;

   if(firsttime) // don't do this more than once, in case of runtime loading
   {
      // When loaded this way, the metadata definitions apply to global gameplay
      D_DeferredMissionMetaData("ENRVMETA", MD_NONE);
      firsttime = false;
   }
}

//
// W_doPS3MLHack
//
// If the PlayStation 3 masterlevels.wad is loaded, load metadata for it.
//
static void W_doPS3MLHack(filelump_t *fileinfo, int numlumps)
{
   static bool firsttime = true;

   if(firsttime)
   {
      D_DeferredMissionMetaData("EMLSMETA", MD_NONE);
      firsttime = false;
   }
}

//
// W_doOtakonHack
//
// Hack for otakugfx.wad
//
static void W_doOtakonHack(filelump_t *fileinfo, int numlumps)
{
   // ensure the expected directory size
   if(numlumps != 268)
      return;

   filelump_t *templumps = estructalloc(filelump_t, 72);
   int curnum = 0;

   // back up lumps that are in the wrong namespace
   for(int i = 109; i < 176; i++, curnum++)
      templumps[curnum] = fileinfo[i]; // STFST01-DSSAWHIT
   for(int i = 251; i < 255; i++, curnum++)
      templumps[curnum] = fileinfo[i]; // D_DDTBLU-D_COUNTD
   templumps[curnum] = fileinfo[256];  // D_STALKS

   // move up sprites into the cleared out space
   curnum = 109;
   for(int i = 176; i < 251; i++, curnum++)
      fileinfo[curnum] = fileinfo[i];  // CHGFA0-CSAWA0
   fileinfo[curnum++] = fileinfo[255]; // SGN2A0
   fileinfo[curnum++] = fileinfo[257]; // S_END

   // fill the saved off lumps back in at the end
   for(int i = 0; i < 72; i++, curnum++)
      fileinfo[curnum] = templumps[i];

   efree(templumps);
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
   { "5b9a4f587edbd08031d9bbf7dd80dec25433e827", W_doPS3MLHack   }, // masterlevels.wad
   { "fe650cc58c8f12a3642b6f5ef2b3368630a4aaa6", W_doNerveHack   }, // nerve.wad
   { "9f823104462d9575750bf0ba6a4a3a6df0f766e3", W_doOtakonHack  }, // otakugfx.wad

   // Terminator, must be last
   { nullptr, nullptr }
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

