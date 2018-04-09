//
// The Eternity Engine
// Copyright (C) 2016 James Haley et al.
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
// Purpose: WAD I/O functions
//

#ifndef W_WAD_H__
#define W_WAD_H__

#include "z_zone.h"

class  ZAutoBuffer;
class  ZipFile;
struct ZipLump;

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

// A direct lump can be read from its archive with C FILE IO facilities.
struct directlump_t
{
   FILE *file;       // for a direct lump, a pointer to the file it is in
   size_t position;  // for direct and memory lumps, offset into file/buffer
};
  
// A memory lump is loaded in a buffer in RAM and just needs to be memcpy'd.
struct memorylump_t
{
   const void *data; // for a memory lump, a pointer to its static memory buffer
   size_t position;  // for direct and memory lumps, offset into file/buffer
};

// A ZIP lump is managed by a ZipFile instance.
struct ziplump_t
{
   ZipLump *zipLump; // pointer to zip lump instance
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
   struct hash_t
   {
      int index, next;
   };
   hash_t namehash, lfnhash;

   // haleyjd 03/27/11: array index into lumpinfo in the parent wad directory,
   // for fast reverse lookup.
   int selfindex;

   // killough 4/17/98: namespace tags, to prevent conflicts between resources
   enum 
   {
      ns_global,
      ns_MIN_LOCAL,
      ns_sprites = ns_MIN_LOCAL, // we need a "min" reference for iteration.
      ns_flats,
      ns_colormaps,
      ns_translations,
      ns_demos,
      ns_acs,
      ns_pads,
      ns_textures,
      ns_graphics,
      ns_sounds,
      ns_hires,
      ns_max           // keep this last.
   };
   int li_namespace;

   // haleyjd 09/03/12: lump cache formats
   typedef enum
   {
      fmt_default, // always used for the raw untranslated lump data
      fmt_patch,   // converted to a patch
      fmt_maxfmts  // number of formats
   } lumpformat;
   
   void *cache[fmt_maxfmts];  //sf

   // haleyjd: lump type
   enum
   {
      lump_direct,  // lump accessed via stdio (physical file)
      lump_memory,  // lump is a memory buffer
      lump_file,    // lump is a directory file; must be opened to use
      lump_zip,     // lump is inside a zip file
      lump_numtypes
   }; 
   int type;

   int source; // haleyjd: unique id # for source of this lump
   
   // haleyjd: physical lump data (guarded union)
   union
   {
      directlump_t direct;
      memorylump_t memory;
      ziplump_t    zip;
   };

   char *lfn;  // long file name, where relevant   
};

// Flags for wfileadd_t
enum WFileAddFlags
{
   WFA_ALLOWINEXACTFN      = 0x0001, // Filename can be modified with AddDefaultExtension
   WFA_OPENFAILFATAL       = 0x0002, // Failure to open file is fatal
   WFA_PRIVATE             = 0x0004, // Loading into a private directory
   WFA_SUBFILE             = 0x0008, // Loading a physical subfile
   WFA_REQUIREFORMAT       = 0x0010, // A specific archive format is required for this load
   WFA_DIRECTORY_RAW       = 0x0020, // Loading a physical disk directory
   WFA_ALLOWHACKS          = 0x0040, // Allow application of wad directory hacks
   WFA_INMEMORY            = 0x0080, // Archive is in memory
   WFA_ISIWADFILE          = 0x0100, // Archive is the main IWAD file
   WFA_DIRECTORY_ARCHIVE   = 0x0200, // Archive is a directory compressible as PKE
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
   const char *filename; // name of file as specified by end user
   int     li_namespace; // if not 0, special namespace to add file under
   FILE   *f;            // pointer to file handle, IFF this is a subfile
   size_t  baseoffset;   // base offset if this is a subfile
   void   *memory;       // memory buffer, IFF archive is in memory
   size_t  size;         // size of buffer, IFF archive is in memory
   int     requiredFmt;  // required file format, if any (-1 if none)

   unsigned int flags;   // flags
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
      // Error modes enumeration
   typedef enum
   {
      CODE_OK,    // Proceed
      CODE_NOFMT, // OK, but don't call formatData
      CODE_FATAL  // Fatal error
   } Code;

   // verifyData should do format checking and return true if the data is valid,
   // and false otherwise. If verifyData returns anything other than CODE_OK, 
   // formatData is not called under any circumstance.
   virtual Code verifyData(lumpinfo_t *lump) const { return CODE_OK; }

   // formatData should do preprocessing work on the lump. This work will be
   // retained until the wad lump is freed from cache, so it allows such work to
   // be done once only and not every time the lump is referenced/used. 
   virtual Code formatData(lumpinfo_t *lump) const { return CODE_OK; }

   // formatIndex specifies an alternate cache pointer to use for resources
   // converted out of their native lump format by the loader.
   virtual lumpinfo_t::lumpformat formatIndex() const { return lumpinfo_t::fmt_default; }
};

class WadDirectoryPimpl;

//
// haleyjd 03/01/09: Wad Directory structure
//
// Adding this allows a level of indirection to be added to the wad system,
// letting us have wads that are not part of the master directory.
//
class WadDirectory : public ZoneObject
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

   struct namespace_t
   {
      int firstLump; // first lump
      int numLumps;  // number of lumps in namespace
   };

private:
   WadDirectoryPimpl *pImpl; // private implementation object

protected:
   // openwad structure - this is for return from WadDirectory::OpenFile
   struct openwad_t
   {
      const char *filename; // possibly altered filename
      FILE   *handle;       // FILE handle
      void   *base;         // Base for in-memory wads
      size_t  size;         // Size for in-memory wads
      bool    error;        // true if an error occured
      int     format;       // detected file format
   };

   static int source;     // unique source ID for each wad file

   lumpinfo_t **lumpinfo; // array of pointers to lumpinfo structures
   int        numlumps;   // number of lumps
   bool       ispublic;   // if false, don't call D_NewWadLumps
   int        type;       // directory type

   namespace_t m_namespaces[lumpinfo_t::ns_max];

   // Protected methods
   void initLumpHash();
   void initLFNHash();
   void initResources();
   void addInfoPtr(lumpinfo_t *infoptr);
   void coalesceMarkedResources();
   void incrementSource(const openwad_t &openData);
   void handleOpenError(openwad_t &openData, const wfileadd_t &addInfo,
                        const char *filename) const;
   openwad_t openFile(const wfileadd_t &addInfo) const;
   lumpinfo_t *reAllocLumpInfo(int numnew, int startlump);
   bool addSingleFile(openwad_t &openData, const wfileadd_t &addInfo,
                      int startlump);
   bool addMemoryWad(openwad_t &openData, const wfileadd_t &addInfo,
                     int startlump);
   bool addWadFile(openwad_t &openData, const wfileadd_t &addInfo,
                   int startlump);
   bool addZipFile(openwad_t &openData, const wfileadd_t &addInfo, int startlump);
   bool addDirectoryAsArchive(openwad_t &openData, const wfileadd_t &addInfo,
                              int startlump);
   bool addFile(wfileadd_t &addInfo);
   void freeDirectoryLumps();  // haleyjd 06/27/09
   void freeDirectoryAllocs(); // haleyjd 06/06/10

   // Utilities
   static unsigned int LumpNameHash(const char *s);

public:
   WadDirectory();
   ~WadDirectory();

   // Public methods
   void  initMultipleFiles(wfileadd_t *files);
   int   checkNumForName(const char *name,
                         int li_namespace = lumpinfo_t::ns_global) const;
   int   checkNumForNameNSG(const char *name, int li_namespace) const;
   int   getNumForName(const char *name) const;
   int   getNumForNameNSG(const char *name, int ns) const;
   int   checkNumForLFN(const char *lfn,
                        int li_namespace = lumpinfo_t::ns_global) const;
   int   checkNumForLFNNSG(const char *name, int ns) const;

   // sf: add a new wad file after the game has already begun
   bool  addNewFile(const char *filename);
   // haleyjd 06/15/10: special private wad file support
   bool  addNewPrivateFile(const char *filename);
   int   addDirectory(const char *dirpath);
   bool  addInMemoryWad(void *buffer, size_t size);
   int   lumpLength(int lump) const;
   void  readLump(int lump, void *dest,
                  const WadLumpLoader *lfmt = nullptr) const;
   void *cacheLumpNum(int lump, int tag,
                      const WadLumpLoader *lfmt = nullptr) const;
   void *cacheLumpName(const char *name, int tag,
                       const WadLumpLoader *lfmt = nullptr) const;
   void  cacheLumpAuto(int lumpnum, ZAutoBuffer &buffer) const;
   void  cacheLumpAuto(const char *name, ZAutoBuffer &buffer) const;
   bool  writeLump(const char *lumpname, const char *destpath) const;
   void  close(); // haleyjd 03/09/11

   lumpinfo_t *getLumpNameChain(const char *name) const;

   const char *getLumpName(int lumpnum) const;
   const char *getLumpFileName(int lump) const;

   // Accessors
   int   getType() const  { return type; }
   void  setType(int i)   { type = i;    }
   
   // Read-only properties
   int          getNumLumps() const { return numlumps; }
   lumpinfo_t **getLumpInfo() const { return lumpinfo; }

   const namespace_t &getNamespace(int li_namespace) const
   {
      return m_namespaces[li_namespace];
   }
};

extern WadDirectory wGlobalDir; // the global wad directory

int      W_CheckNumForName(const char *name);   // killough 4/17/98
int      W_CheckNumForNameNS(const char *name, int li_namespace);
int      W_GetNumForName(const char* name);

int      W_LumpLength(int lump);
uint32_t W_LumpCheckSum(int lumpnum);

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
