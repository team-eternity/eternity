// Emacs style mode select   -*- C -*- 
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
//      WAD I/O functions.
//
//-----------------------------------------------------------------------------

#ifndef __W_WAD__
#define __W_WAD__

#include <stdio.h>

//
// TYPES
//

typedef struct wadinfo_s
{
  char identification[4];                  // Should be "IWAD" or "PWAD".
  int  numlumps;
  int  infotableofs;
} wadinfo_t;

typedef struct filelump_s
{
  int  filepos;
  int  size;
  char name[8];
} filelump_t;

//
// WADFILE I/O related stuff.
//

// haleyjd 07/12/07: altered lumpinfo_t for separation of logical and physical
// lump fields.

struct lumpinfo_t
{
   // haleyjd: logical lump data
   char   name[9];
   size_t size;
   
   // killough 1/31/98: hash table fields, used for ultra-fast hash table lookup
   int index, next;

   // killough 4/17/98: namespace tags, to prevent conflicts between resources
   enum 
   {
      ns_global = 0,
      ns_sprites,
      ns_flats,
      ns_colormaps,
      ns_translations,
      ns_fonts,
      ns_demos
   };
   int li_namespace;
   
   void *cache;  //sf

   // haleyjd: lump type
   enum
   {
      lump_direct,  // lump accessed via stdio (physical file)
      lump_memory,  // lump is a memory buffer
      lump_numtypes
   }; 
   int type;

   int source; // haleyjd: unique id # for source of this lump
   
   // haleyjd: physical lump data
   
   FILE *file;       // for a direct lump, a pointer to the file it is in
   const void *data; // for a memory lump, a pointer to its static memory buffer
   size_t position;  // for direct and memory lumps, offset into file/buffer

};

// directory types
enum
{
   WADDIR_NORMAL,
   WADDIR_MANAGED
};

//
// haleyjd 03/01/09: Wad Directory structure
//
// Adding this allows a level of indirection to be added to the wad system,
// letting us have wads that are not part of the master directory.
//
typedef struct waddir_s
{
   lumpinfo_t **lumpinfo; // array of pointers to lumpinfo structures
   int        numlumps;   // number of lumps
   int        ispublic;   // if false, don't call D_NewWadLumps
   lumpinfo_t **infoptrs; // 06/06/10: track all allocations
   int        numallocs;  // number of entries in the infoptrs table
   int        numallocsa; // number of entries allocated for the infoptrs table   
   int        type;       // directory type
   void       *data;      // user data (mainly for w_levels code)
   int        numlumps_before_coalescing; // [CG] Used when freeing.
} waddir_t;

//
// haleyjd 10/09/09: wfileadd_t
//
// This structure is used to pass info down to W_InitMultipleFiles now so that
// certain files being added can be treated specially while being linked into
// the global wad directory.
//
typedef struct wfileadd_s
{
   const char *filename; // name of file
   int li_namespace;     // if not 0, special namespace to add file under
   FILE *f;              // pointer to file handle if this is a subfile
   size_t baseoffset;    // base offset if this is a subfile
   int privatedir;       // if not 0, has a private directory
} wfileadd_t;

extern waddir_t w_GlobalDir; // the global wad directory

void        W_ClearWadDir(waddir_t *dir); // [CG] Added.
void        W_InitMultipleFiles(waddir_t *dir, wfileadd_t *files);
int         W_CheckNumForNameInDir(waddir_t *dir, const char *name, int);
int         W_CheckNumForName(const char *name);   // killough 4/17/98
int         W_CheckNumForNameNS(const char *name, int li_namespace);
int         W_CheckNumForNameNSG(const char *name, int ns);
int         W_GetNumForName(const char* name);
lumpinfo_t *W_GetLumpNameChainInDir(waddir_t *dir, const char *name);
lumpinfo_t *W_GetLumpNameChain(const char *name);
int         W_LumpLengthInDir(waddir_t *dir, int lump);
int         W_LumpLength(int lump);
void        W_ReadLumpInDir(waddir_t *dir, int lump, void *dest);
void        W_ReadLump(int lump, void *dest);
void       *W_CacheLumpNumInDir(waddir_t *dir, int lump, int tag);
void       *W_CacheLumpNum(int lump, int tag);
int         W_LumpCheckSum(int lumpnum);
int         W_ReadLumpHeader(int lump, void *dest, size_t size);
void        W_FreeDirectoryLumps(waddir_t *waddir); // haleyjd 06/27/09

// sf: add a new wad file after the game has already begun
int W_AddNewFile(waddir_t *dir, const char *filename);

// haleyjd 06/15/10: special private wad file support
int W_AddNewPrivateFile(waddir_t *dir, const char *filename);

#define W_CacheLumpName(name,tag) W_CacheLumpNum (W_GetNumForName(name),(tag))

unsigned int W_LumpNameHash(const char *s);           // killough 1/31/98

void W_FreeDirectoryLumps(waddir_t *waddir); // haleyjd 06/27/09
void W_FreeDirectoryAllocs(waddir_t *dir);   // haleyjd 06/06/10

void I_BeginRead(void), I_EndRead(void); // killough 10/98

extern FILE *iwadhandle;
extern FILE *firstWadHandle; // haleyjd 06/21/04

#endif

//----------------------------------------------------------------------------
//
// $Log: w_wad.h,v $
// Revision 1.10  1998/05/06  11:32:05  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.9  1998/05/03  22:43:45  killough
// remove unnecessary #includes
//
// Revision 1.8  1998/05/01  14:55:54  killough
// beautification
//
// Revision 1.7  1998/04/27  02:05:30  killough
// Program beautification
//
// Revision 1.6  1998/04/19  01:14:36  killough
// Reinstate separate namespaces
//
// Revision 1.5  1998/04/17  16:52:21  killough
// back out namespace changes temporarily
//
// Revision 1.4  1998/04/17  10:33:50  killough
// Macroize W_CheckNumForName(), add namespace parameter to functional version
//
// Revision 1.3  1998/02/02  13:35:13  killough
// Improve lump hashing, add predefine lumps
//
// Revision 1.2  1998/01/26  19:28:01  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
