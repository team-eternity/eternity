// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3: 
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

#ifndef W_WAD_H__
#define W_WAD_H__

//
// TYPES
//

struct wadinfo_t
{
  char identification[4];                  // Should be "IWAD" or "PWAD".
  int  numlumps;
  int  infotableofs;
};

struct filelump_t
{
  int  filepos;
  int  size;
  char name[8];
};

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

   // haleyjd 03/27/11: array index into lumpinfo in the parent wad directory,
   // for fast reverse lookup.
   int selfindex;

   // killough 4/17/98: namespace tags, to prevent conflicts between resources
   enum 
   {
      ns_global,
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
      lump_file,    // lump is a directory file; must be opened to use
      lump_numtypes
   }; 
   int type;

   int source; // haleyjd: unique id # for source of this lump
   
   // haleyjd: physical lump data
   
   FILE *file;       // for a direct lump, a pointer to the file it is in
   const void *data; // for a memory lump, a pointer to its static memory buffer
   size_t position;  // for direct and memory lumps, offset into file/buffer
   const char *lfn;  // long file name, where relevant
};

//
// haleyjd 10/09/09: wfileadd_t
//
// This structure is used to pass info down to W_InitMultipleFiles now so that
// certain files being added can be treated specially while being linked into
// the global wad directory.
//
struct wfileadd_t
{
   const char *filename; // name of file
   int li_namespace;     // if not 0, special namespace to add file under
   FILE *f;              // pointer to file handle if this is a subfile
   size_t baseoffset;    // base offset if this is a subfile
   int privatedir;       // if not 0, has a private directory
   bool directory;       // if true, is an on-disk directory
};

//
// haleyjd 06/26/11: Wad lump preprocessing and formatting
//
// Inherit from this interface class to provide verification and preprocessing 
// code to W_CacheLump* routines.
//
class WadLumpLoader
{
public:
   // verifyData should do format checking and return true if the data is valid,
   // and false otherwise. If verifyData returns false, formatData is not called
   // under any circumstance.
   virtual bool verifyData(const void *data, size_t size) const { return true; }

   // formatData should do preprocessing work on the lump. This work will be
   // retained until the wad lump is freed from cache, so it allows such work to
   // be done once only and not every time the lump is retrieved from the wad
   // file. Return false if an error occurs, and true otherwise.
   virtual bool formatData(void *data, size_t size) const { return true; }

   // Error modes enumeration
   enum
   {
      EM_IGNORE, // Ignore any error
      EM_FATAL   // Fatal error on failure
   };

   // getErrorMode tells the wad file code how to respond to a failure in the
   // verification or formatting routines. Return one of the codes above.
   virtual int getErrorMode() const { return EM_IGNORE; }
};

//
// haleyjd 03/01/09: Wad Directory structure
//
// Adding this allows a level of indirection to be added to the wad system,
// letting us have wads that are not part of the master directory.
//
class WadDirectory
{
public:
   // directory types
   enum
   {
      NORMAL,
      MANAGED
   };

   // file add types
   enum
   {
      ADDWADFILE, // Add as an ordinary wad
      ADDSUBFILE, // Add as a subfile wad
      ADDPRIVATE  // Add to a private directory
   };
   
   static int IWADSource;   // source # of the global IWAD file
   static int ResWADSource; // source # of the resource wad (ie. eternity.wad)

protected:
   // openwad structure - this is for return from WadDirectory::OpenFile
   struct openwad_t
   {
      const char *filename; // possibly altered filename
      FILE *handle;         // FILE handle
      bool error;           // true if an error occured
      bool errorRet;        // code to return from AddFile
   };

   static int source;     // unique source ID for each wad file

   lumpinfo_t   **lumpinfo; // array of pointers to lumpinfo structures
   int          numlumps;   // number of lumps
   int          ispublic;   // if false, don't call D_NewWadLumps
   lumpinfo_t   **infoptrs; // 06/06/10: track all allocations
   int          numallocs;  // number of entries in the infoptrs table
   int          numallocsa; // number of entries allocated for the infoptrs table   
   int          type;       // directory type
   void         *data;      // user data (mainly for w_levels code)
   int numlumps_before_coalescing; // [CG] 09/18/11: For clearing.

   // Protected methods
   void InitLumpHash();
   void InitResources();
   void AddInfoPtr(lumpinfo_t *infoptr);
   void CoalesceMarkedResource(const char *start_marker, 
                               const char *end_marker, 
                               int li_namespace);
   openwad_t OpenFile(const char *name, int filetype);
   bool AddFile(const char *name, int li_namespace, int filetype,
                FILE *file = NULL, size_t baseoffset = 0);
   void FreeDirectoryLumps();  // haleyjd 06/27/09
   void FreeDirectoryAllocs(); // haleyjd 06/06/10

   // Utilities
   static int          IsMarker(const char *marker, const char *name);
   static unsigned int LumpNameHash(const char *s);

public:
   // Public methods
   void        InitMultipleFiles(wfileadd_t *files);
   int         CheckNumForName(const char *name, 
                               int li_namespace = lumpinfo_t::ns_global);
   int         CheckNumForNameNSG(const char *name, int li_namespace);
   int         GetNumForName(const char *name);
   lumpinfo_t *GetLumpNameChain(const char *name);
   // sf: add a new wad file after the game has already begun
   int         AddNewFile(const char *filename);
   // haleyjd 06/15/10: special private wad file support
   int         AddNewPrivateFile(const char *filename);
   int         AddDirectory(const char *dirpath);
   int         LumpLength(int lump);
   void        ReadLump(int lump, void *dest, WadLumpLoader *lfmt = NULL);
   int         ReadLumpHeader(int lump, void *dest, size_t size);
   void       *CacheLumpNum(int lump, int tag, WadLumpLoader *lfmt = NULL);
   void       *CacheLumpName(const char *name, int tag, 
                             WadLumpLoader *lfmt = NULL);
   void        Close(); // haleyjd 03/09/11
   void        Clear(); // [CG] 09/18/11

   // Accessors
   int   GetType() const  { return type; }
   void  SetType(int i)   { type = i;    }
   void *GetData() const  { return data; }
   void  SetData(void *d) { data = d;    }
   
   // Read-only properties
   int          GetNumLumps() const { return numlumps; }
   lumpinfo_t **GetLumpInfo() const { return lumpinfo; }
};

extern WadDirectory wGlobalDir; // the global wad directory

int         W_CheckNumForName(const char *name);   // killough 4/17/98
int         W_CheckNumForNameNS(const char *name, int li_namespace);
int         W_GetNumForName(const char* name);

lumpinfo_t *W_GetLumpNameChain(const char *name);

int         W_LumpLength(int lump);
uint32_t    W_LumpCheckSum(int lumpnum);
int         W_ReadLumpHeader(int lump, void *dest, size_t size);

void I_BeginRead(void), I_EndRead(void); // killough 10/98

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
