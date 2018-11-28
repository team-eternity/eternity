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
// Purpose: Handles WAD file header, directory, lump I/O.
//

#ifdef _MSC_VER
// for Visual C++: 
#include "Win32/i_opndir.h"
#else
// for SANE compilers:
#include <dirent.h>
#endif

#include <algorithm> // ioanch: for sort
#include <memory>

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "d_io.h"  // SoM 3/12/2002: moved unistd stuff into d_io.h

#include "c_io.h"
#include "d_dehtbl.h"
#include "d_files.h"
#include "e_hash.h"
#include "hal/i_directory.h"
#include "m_argv.h"
#include "m_collection.h"
#include "m_dllist.h"
#include "m_hash.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "m_utils.h"
#include "p_skin.h"
#include "s_sound.h"
#include "v_misc.h"
#include "w_formats.h"
#include "w_hacks.h"
#include "w_wad.h"
#include "w_zip.h"
#include "z_auto.h"

//
// GLOBALS
//

// Location of each lump on disk.
WadDirectory wGlobalDir;

int WadDirectory::source;            // haleyjd 03/18/10: next source ID# to use
int WadDirectory::IWADSource   = -1; // sf: the handle of the main iwad
int WadDirectory::ResWADSource = -1; // haleyjd: track handle of first wad added

//
// The structure for source path saving and hash table
//
struct sourcepath_t
{
   int         sourcenum;
   const char *path;
   DLListItem<sourcepath_t> links;
};

static EHashTable<sourcepath_t, EIntHashKey, &sourcepath_t::sourcenum,
                  &sourcepath_t::links> e_SourceNumHash;

static EHashTable<lumpinfo_t, EStringHashKey, &lumpinfo_t::lfn,
                  &lumpinfo_t::lfnlinks> e_LFNHash;

const char *W_PathForSource(int sourcenum)
{
   sourcepath_t *sourcepath = e_SourceNumHash.objectForKey(sourcenum);
   return sourcepath ? sourcepath->path : nullptr;
}

lumpinfo_t *W_NextInLFNHash(lumpinfo_t *lumpinfo)
{
   return e_LFNHash.keyIterator(lumpinfo, lumpinfo->lfn);
}

//
// haleyjd 07/12/07: structure for transparently manipulating lumps of
// different types
//

struct lumptype_t
{
   size_t (*readLump)(lumpinfo_t *, void *);
};

static size_t W_DirectReadLump(lumpinfo_t *, void *);
static size_t W_MemoryReadLump(lumpinfo_t *, void *);
static size_t W_FileReadLump  (lumpinfo_t *, void *);
static size_t W_ZipReadLump   (lumpinfo_t *, void *);

static lumptype_t LumpHandlers[lumpinfo_t::lump_numtypes] =
{
   // direct lump
   {
      W_DirectReadLump,
   },

   // memory lump
   {
      W_MemoryReadLump,
   },

   // directory file lump
   {
      W_FileReadLump,
   },

   // zip file lump
   {
      W_ZipReadLump,
   },
};

//=============================================================================
//
// WadDirectoryPimpl
//

//
// haleyjd 09/01/12: private implementation object for hidden portions of the
// WadDirectory class.
//
class WadDirectoryPimpl : public ZoneObject
{
public:
   // Static collection of source filenames
   static qstring FnPrototype;
   static Collection<qstring> SourceFileNames;

   //
   // Add a source filename
   //
   static void AddFileName(const char *fn)
   {
      SourceFileNames.setPrototype(&FnPrototype);
      SourceFileNames.addNew() << fn;
   }

   //
   // Get a filename for a source number
   //
   static const char *FileNameForSource(size_t source)
   {
      if(source >= SourceFileNames.getLength())
         return nullptr;
      return SourceFileNames[source].constPtr();
   }

   PODCollection<lumpinfo_t *>  infoptrs; // lumpinfo_t allocations
   DLListItem<ZipFile>         *zipFiles; // zip files attached to this waddir

   WadDirectoryPimpl()
      : ZoneObject(), infoptrs(), zipFiles(nullptr)
   {
   }
};

qstring             WadDirectoryPimpl::FnPrototype;
Collection<qstring> WadDirectoryPimpl::SourceFileNames;

//=============================================================================
//
// WadDirectory
//
// LUMP BASED ROUTINES.
//

//
// WadDirectory Constructor
//
WadDirectory::WadDirectory()
 : ZoneObject(), lumpinfo(nullptr), numlumps(0), ispublic(false), type(0)
{
   pImpl = new WadDirectoryPimpl();
   memset(m_namespaces, 0, sizeof(m_namespaces));
}

//
// Destructor
//
WadDirectory::~WadDirectory()
{
   if(pImpl)
   {
      delete pImpl;
      pImpl = nullptr;
   }
}

//
// W_addInfoPtr
//
// haleyjd 06/06/10: I need to track all lumpinfo_t allocations that are
// added to a waddir_t's lumpinfo directory, or else these allocations get
// orphaned when freeing a private wad directory. Oops! ;)
//
void WadDirectory::addInfoPtr(lumpinfo_t *infoptr)
{
   pImpl->infoptrs.add(infoptr);
}

//
// WadDirectory::incrementSource
//
// Add the source filename and increment the static source counter.
//
void WadDirectory::incrementSource(const openwad_t &openData)
{
   // haleyjd: push source filename
   WadDirectoryPimpl::AddFileName(openData.filename);

   // haleyjd: increment source
   ++source;
}

//
// WadDirectory::handleOpenError
//
// Protected method to handle errors during archive file open.
//
void WadDirectory::handleOpenError(openwad_t &openData,
                                   const wfileadd_t &addInfo,
                                   const char *filename) const
{
   if(addInfo.flags & WFA_OPENFAILFATAL)
      I_Error("Error: couldn't open %s\n", filename);
   else
   {
      if(in_textmode)
         printf(" Warning: couldn't open %s\n", filename);
      else
         C_Printf(FC_ERROR "Couldn't open %s\n", filename);
   }

   openData.error = true;
}

//
// WadDirectory::openFile
//
// haleyjd 04/06/11: For normal wad files, the file needs to be found and 
// opened.
//
WadDirectory::openwad_t WadDirectory::openFile(const wfileadd_t &addInfo) const
{
   edefstructvar(openwad_t, openData);
   qstring   filename;
   bool      allowInexact = (addInfo.flags & WFA_ALLOWINEXACTFN) == WFA_ALLOWINEXACTFN;
   
   // Try opening the file
   filename = addInfo.filename;
   if(!(openData.handle = W_TryOpenFile(filename, allowInexact)))
   {
      handleOpenError(openData, addInfo, filename.constPtr());
      return openData;
   }

   // Figure out the file format
   openData.format = W_DetermineFileFormat(openData.handle, 0);

   // Check against format requirements
   if(addInfo.flags & WFA_REQUIREFORMAT && 
      openData.format != addInfo.requiredFmt)
   {
      handleOpenError(openData, addInfo, filename.constPtr());
      return openData;
   }

   // Successfully opened!
   openData.filename = filename.duplicateAuto();
   openData.error    = false;
   return openData;
}

//
// WadDirectory::reAllocLumpInfo
//
// Reallocate the lumpinfo array to contain the indicated number of pointers,
// and, if makelumps is true, will also allocate that number of new lumpinfo_t
// structures and return them.
//
lumpinfo_t *WadDirectory::reAllocLumpInfo(int numnew, int startlump)
{
   lumpinfo_t *newlumps = nullptr;

   numlumps += numnew;

   // Fill in lumpinfo
   lumpinfo = erealloc(lumpinfo_t **, lumpinfo, numlumps * sizeof(lumpinfo_t *));

   // space for new lumps
   newlumps = estructalloc(lumpinfo_t, numlumps - startlump);

   // haleyjd: keep track of this allocation of lumps
   addInfoPtr(newlumps);

   // haleyjd: fill in new pointers here, instead of everywhere this is used.
   for(int i = startlump; i < numlumps; i++)
      lumpinfo[i] = newlumps + (i - startlump);

   return newlumps;
}

//
// WadDirectory::addSingleFile
//
// haleyjd 10/27/12: Load a single file into the wad directory.
//
bool WadDirectory::addSingleFile(openwad_t &openData, const wfileadd_t &addInfo,
                                 int startlump)
{
   edefstructvar(filelump_t, singleinfo);
   lumpinfo_t *lump_p;

   singleinfo.filepos = 0;
   singleinfo.size    = static_cast<int>(M_FileLength(openData.handle));
   M_ExtractFileBase(openData.filename, singleinfo.name);

   lump_p = reAllocLumpInfo(1, startlump);

   lump_p->type   = lumpinfo_t::lump_direct; // haleyjd: lump type
   lump_p->size   = static_cast<size_t>(singleinfo.size);
   lump_p->source = source;                  // haleyjd: source id

   // setup for direct file IO
   lump_p->direct.file     = openData.handle;
   lump_p->direct.position = static_cast<size_t>(singleinfo.filepos);

   lump_p->li_namespace = addInfo.li_namespace; // killough 4/17/98

   strncpy(lump_p->name, singleinfo.name, 8);

   incrementSource(openData);

   return true;
}

//
// WadDirectory::addMemoryWad
//
// haleyjd 10/31/12: Add an id wadlink file stored in memory into the directory.
//
bool WadDirectory::addMemoryWad(openwad_t &openData, const wfileadd_t &addInfo,
                                int startlump)
{
   // haleyjd 04/07/11
   wadinfo_t    header;
   ZAutoBuffer  fileinfo2free; // killough
   filelump_t  *fileinfo; 
   size_t       length;
   size_t       info_offset;
   lumpinfo_t  *lump_p;

   // Read in the header
   memcpy(&header, openData.base, sizeof(header));

   header.numlumps     = SwapLong(header.numlumps);
   header.infotableofs = SwapLong(header.infotableofs);

   // allocate enough fileinfo_t's to hold the wad directory
   length = header.numlumps * sizeof(filelump_t);
  
   fileinfo2free.alloc(length, true);              // killough
   fileinfo = fileinfo2free.getAs<filelump_t *>();

   info_offset = static_cast<size_t>(header.infotableofs);

   // seek to the directory
   if(info_offset + header.numlumps * sizeof(filelump_t)  > openData.size)
   {
      if(addInfo.flags & WFA_OPENFAILFATAL)
         I_Error("Failed reading directory for in-memory file\n");
      else
      {
         if(in_textmode)
            printf("Failed reading directory for in-memory file\n");
         else
            C_Printf(FC_ERROR "Failed reading directory for in-memory file\n");
         return false;
      }
   }

   // read it in.
   byte *directoryBase = static_cast<byte *>(openData.base) + info_offset;
   memcpy(fileinfo, directoryBase, header.numlumps * sizeof(filelump_t));

   // Add lumpinfo_t's for all lumps in the wad file
   lump_p = reAllocLumpInfo(header.numlumps, startlump);

   // Merge into the directory
   for(int i = startlump; i < numlumps; i++, lump_p++, fileinfo++)
   {
      lump_p->type   = lumpinfo_t::lump_memory; // haleyjd
      lump_p->size   = (size_t)(SwapLong(fileinfo->size));
      lump_p->source = source; // haleyjd

      // setup for memory IO
      lump_p->memory.data     = openData.base;
      lump_p->memory.position = (size_t)(SwapLong(fileinfo->filepos));
      
      lump_p->li_namespace = addInfo.li_namespace;     // killough 4/17/98

      strncpy(lump_p->name, fileinfo->name, 8);
   }

   incrementSource(openData);

   return true;
}

//
// WadDirectory::addWadFile
//
// haleyjd 10/28/12: Add an id wadlink file into the directory.
//
bool WadDirectory::addWadFile(openwad_t &openData, const wfileadd_t &addInfo,
                              int startlump)
{
   // haleyjd 04/07/11
   HashData     wadHash  = HashData(HashData::SHA1);
   bool         showHash = false;
   bool         doHacks  = (addInfo.flags & WFA_ALLOWHACKS) == WFA_ALLOWHACKS;
   long         baseoffset = static_cast<long>(addInfo.baseoffset);
   wadinfo_t    header;
   ZAutoBuffer  fileinfo2free; // killough
   filelump_t  *fileinfo; 
   size_t       length;
   long         info_offset;
   lumpinfo_t  *lump_p;

   // check for in-memory wads
   if(addInfo.flags & WFA_INMEMORY)
      return addMemoryWad(openData, addInfo, startlump);

   // haleyjd: seek to baseoffset first when loading a subfile
   if(addInfo.flags & WFA_SUBFILE)
      fseek(openData.handle, baseoffset, SEEK_SET);

   // -nowadhacks disables all wad directory hacks, in case of unforeseen
   // compatibility problems that would last until the next release
   if(M_CheckParm("-nowadhacks"))
      doHacks = false;

   // -showhashes enables display of the computed SHA-1 hash used to apply
   // wad directory hacks. Quite useful for when a new hack needs to be added.
   if(M_CheckParm("-showhashes"))
      showHash = true;

   if(fread(&header, sizeof(header), 1, openData.handle) < 1)
   {
      if(addInfo.flags & WFA_OPENFAILFATAL)
         I_Error("Failed reading header for wad file %s\n", openData.filename);
      else
      {
         fclose(openData.handle);
         if(in_textmode)
            printf("Failed reading header for wad file %s\n", openData.filename);
         else
            C_Printf(FC_ERROR "Failed reading header for wad file %s", openData.filename);

         return false;            
      }
   }

   // Feed the wad header data into the hash computation
   if(doHacks || showHash)
      wadHash.addData((const uint8_t *)&header, (uint32_t)sizeof(header));

   header.numlumps     = SwapLong(header.numlumps);
   header.infotableofs = SwapLong(header.infotableofs);

   // allocate enough fileinfo_t's to hold the wad directory
   length = header.numlumps * sizeof(filelump_t);
  
   fileinfo2free.alloc(length, true);              // killough
   fileinfo = fileinfo2free.getAs<filelump_t *>();

   info_offset = static_cast<long>(header.infotableofs);

   // subfile wads may exist at a positive base offset in the container file
   if(addInfo.flags & WFA_SUBFILE)
      info_offset += baseoffset;

   // seek to the directory
   fseek(openData.handle, info_offset, SEEK_SET);

   // read it in.
   if(fread(fileinfo, length, 1, openData.handle) < 1)
   {
      if(addInfo.flags & WFA_OPENFAILFATAL)
         I_Error("Failed reading directory for wad file %s\n", openData.filename);
      else
      {
         fclose(openData.handle);
         if(in_textmode)
            printf("Failed reading directory for wad file %s\n", openData.filename);
         else
            C_Printf(FC_ERROR "Failed reading directory for wad file %s", openData.filename);
         return false;
      }
   }

   // Feed the wad directory into the hash computation, wrap it up, and if requested,
   // output it to the console.
   if(doHacks || showHash)
   {
      wadHash.addData((const uint8_t *)fileinfo, (uint32_t)length);
      wadHash.wrapUp();
      if(in_textmode && showHash)
         printf("\thash = %s\n", wadHash.digestToString());
      // haleyjd 04/08/11: apply wad directory hacks as needed
      if(doHacks)
         W_CheckDirectoryHacks(wadHash, fileinfo, header.numlumps);
   }

   // update IWAD handle? 
   // haleyjd: Must be a public wad file.
   if(!(addInfo.flags & WFA_PRIVATE) && this->ispublic)
   {
      // haleyjd 06/21/04: track handle of first wad added also
      if(ResWADSource == -1)
         ResWADSource = source;

      // haleyjd 07/13/09: only track the first IWAD found
      // haleyjd 11/03/12: Status as the IWAD is now determined by how the file
      // was added to the game (ie., -iwad or -disk, vs. -file, autoloads, etc.)
      if(IWADSource < 0 && (addInfo.flags & WFA_ISIWADFILE))
         IWADSource = source;
   }

   // Add lumpinfo_t's for all lumps in the wad file
   lump_p = reAllocLumpInfo(header.numlumps, startlump);

   // Merge into the directory
   for(int i = startlump; i < this->numlumps; i++, lump_p++, fileinfo++)
   {
      lump_p->type      = lumpinfo_t::lump_direct; // haleyjd
      lump_p->size      = (size_t)(SwapLong(fileinfo->size));
      lump_p->source    = source; // haleyjd

      // setup for direct IO
      lump_p->direct.file     = openData.handle;
      lump_p->direct.position = (size_t)(SwapLong(fileinfo->filepos));

      // for subfiles, add baseoffset to the lump offset
      if(addInfo.flags & WFA_SUBFILE)
         lump_p->direct.position += static_cast<size_t>(baseoffset);
      
      lump_p->li_namespace = addInfo.li_namespace;     // killough 4/17/98

      strncpy(lump_p->name, fileinfo->name, 8);
   }

   if(ispublic)
      D_NewWadLumps(source);

   incrementSource(openData);

   return true;
}

//
// WadDirectory::addZipFile
//
// Add a zip file into the directory.
//
bool WadDirectory::addZipFile(openwad_t &openData,
                              const wfileadd_t &addInfo, int startlump)
{
   std::unique_ptr<ZipFile> zip(new ZipFile());
   int         numZipLumps;
   lumpinfo_t *lump_p;

   // Read in the ZIP file's header and directory information
   if(!zip->readFromFile(openData.handle))
   {
      handleOpenError(openData, addInfo, openData.filename);
      return false;
   }

   if(!(numZipLumps = zip->getNumLumps()))
   {
      // load was successful, but this zip file is useless.
      return true;
   }

   // update IWAD handle? 
   if(!(addInfo.flags & WFA_PRIVATE) && this->ispublic)
   {
      if(IWADSource < 0 && (addInfo.flags & WFA_ISIWADFILE))
         IWADSource = source;
   }

   // Allocate lumpinfo_t structures for the zip file's internal file lumps
   lump_p = reAllocLumpInfo(numZipLumps, startlump);

   // Initialize the wad directory copies of the zip lumps
   for(int i = startlump; i < numlumps; i++, lump_p++)
   {
      ZipLump &zipLump = zip->getLump(i - startlump);

      lump_p->type   = lumpinfo_t::lump_zip;
      lump_p->size   = zipLump.size;
      lump_p->source = source;

      // setup for zip file IO
      lump_p->zip.zipLump = &zipLump;

      // Initialize short name, if found appropriate to do so. Lumps that are
      // not given a short name or namespace here will not be hashed by
      // WadDirectory::initLumpHash.
      int li_namespace;
      if((li_namespace = W_NamespaceForFilePath(zipLump.name)) != -1)
      {
         lump_p->li_namespace = li_namespace;
         W_LumpNameFromFilePath(zipLump.name, lump_p->name, li_namespace);
      }

      // Copy lfn
      lump_p->lfn = estrdup(zipLump.name);
   }

   // Hook the ZipFile instance into the WadDirectory's list of zips
   zip->linkTo(&pImpl->zipFiles);

   incrementSource(openData);

   // Check for embedded wad files
   zip->checkForWadFiles(*this);

   zip.release(); // don't destroy the ZipFile
   return true;
}

//
// WadDirectory::addFile
//
// All files are optional, but at least one file must be found (PWAD, if all 
// required lumps are present).
// Files with a .wad extension are wadlink files with multiple lumps.
// Other files are single lumps with the base filename for the lump name.
//
// Reload hack removed by Lee Killough
// killough 1/31/98: static, const
//
bool WadDirectory::addFile(wfileadd_t &addInfo)
{
   // Directory file addition callback type
   typedef bool (WadDirectory::* AddFileCB)(openwad_t &, const wfileadd_t &,
                                            int);
   
   static AddFileCB fileadders[W_FORMAT_MAX] =
   {
      &WadDirectory::addWadFile,             // W_FORMAT_WAD
      &WadDirectory::addZipFile,             // W_FORMAT_ZIP
      &WadDirectory::addSingleFile,          // W_FORMAT_FILE
      &WadDirectory::addDirectoryAsArchive   // W_FORMAT_DIR
   };
   
   edefstructvar(openwad_t, openData);

   // When loading a subfile, the physical file is already open.
   if(addInfo.flags & WFA_SUBFILE)
   {      
      openData.filename = addInfo.filename;
      openData.handle   = addInfo.f;

      // Only WAD files are currently supported as subfiles.
      if(W_DetermineFileFormat(openData.handle, 
            static_cast<long>(addInfo.baseoffset)) != W_FORMAT_WAD)
      {
         addInfo.flags |= WFA_OPENFAILFATAL;
         handleOpenError(openData, addInfo, openData.filename);
         return false; // it'll actually I_Error, but whatever.
      }
      openData.format = W_FORMAT_WAD;
   }
   else if(addInfo.flags & WFA_INMEMORY)
   {
      openData.base     = addInfo.memory;
      openData.size     = addInfo.size;
      openData.filename = "memory";
      openData.format   = W_FORMAT_WAD; // wad handler will deal with this.
   }
   else if(addInfo.flags & WFA_DIRECTORY_ARCHIVE)
   {
      openData.filename = addInfo.filename;
      openData.format = W_FORMAT_DIR;
   }
   else
   {
      // Open the physical archive file and determine its format
      openData = openFile(addInfo);
      if(openData.error)
         return false; 
   }

   // Show adding message if at startup and not a private directory or 
   // in-memory wad
   if(!(addInfo.flags & (WFA_PRIVATE|WFA_INMEMORY)) && 
      this->ispublic && in_textmode)
   {
      printf(" adding %s\n", openData.filename);   // killough 8/8/98
   }

   // Call the appropriate file directory addition routine for this format
#ifdef RANGECHECK
   if(openData.format < 0 || openData.format >= W_FORMAT_MAX)
      I_Error("WadDirectory::addFile: invalid file format %d\n", openData.format);
#endif

   if(!(this->*fileadders[openData.format])(openData, addInfo, numlumps))
   {
      handleOpenError(openData, addInfo, openData.filename);
      return false;
   }
   
   return true; // no error
}

struct dirfile_t
{
   char   *fullfn;
   bool    isdir;
   size_t  size;
};

//
// WadDirectory::addDirectory
//
// Add an on-disk file directory to the WadDirectory.
//
int WadDirectory::addDirectory(const char *dirpath)
{
   DIR    *dir;
   dirent *ent;
   int     localcount = 0;
   int     totalcount = 0;
   int     startlump;
   int     usinglump  = 0; 
   int     globallump = 0;
   size_t  i, fileslen;

   PODCollection<dirfile_t> files;
   lumpinfo_t *newlumps;
   
   if(!(dir = opendir(dirpath)))
      return 0;

   // count the files in the directory
   while((ent = readdir(dir)))
   {
      edefstructvar(dirfile_t, newfile);
      struct stat sbuf;

      if(!strcmp(ent->d_name, ".")  || !strcmp(ent->d_name, ".."))
         continue;
      
      newfile.fullfn = M_SafeFilePath(dirpath, ent->d_name);

      if(!stat(newfile.fullfn, &sbuf)) // check for existence
      {
         if(S_ISDIR(sbuf.st_mode)) // if it's a directory, mark it
            newfile.isdir = true;
         else
         {
            newfile.isdir = false;
            newfile.size  = (size_t)(sbuf.st_size);
            ++localcount;
         }
      
         files.add(newfile);
      }
   }
   
   closedir(dir);

   fileslen = files.getLength();

   for(i = 0; i < fileslen; i++)
   {
      // recurse into subdirectories first.
      if(files[i].isdir)
         totalcount += addDirectory(files[i].fullfn);
   }

   // No lumps to add for this directory?
   if(!localcount)
      return totalcount;

   // Fill in lumpinfo
   startlump = numlumps;
   numlumps += localcount;
   lumpinfo = erealloc(lumpinfo_t **, lumpinfo, numlumps * sizeof(lumpinfo_t *));

   // create lumpinfo_t structures for the files
   newlumps = estructalloc(lumpinfo_t, localcount);
   
   // keep track of this allocation of lumps
   addInfoPtr(newlumps);

   for(i = 0, globallump = startlump; i < fileslen; i++)
   {
      if(!files[i].isdir)
      {
         lumpinfo_t *lump = &newlumps[usinglump++];
         
         M_ExtractFileBase(files[i].fullfn, lump->name);
         M_Strupr(lump->name);
         lump->li_namespace = lumpinfo_t::ns_global; // TODO
         lump->type         = lumpinfo_t::lump_file;
         lump->lfn          = estrdup(files[i].fullfn);
         lump->source       = source;
         lump->size         = files[i].size;

         lumpinfo[globallump++] = lump;
      }
   }

   // push source filename
   WadDirectoryPimpl::AddFileName(dirpath);

   // increment source
   ++source;

   if(ispublic && in_textmode)
      printf(" adding directory %s\n", dirpath);

   return totalcount + localcount;
}

//
// Used to add archive directory files
//
class ArchiveDirFile
{
public:
   qstring path;        // path from current directory (for reading)
   qstring innerpath;   // path from within the archive dir (for namespace)
   off_t size;          // file size

   ArchiveDirFile() : path(), innerpath(), size(0)
   {
   }

   // for sorting
   bool operator < (const ArchiveDirFile &o) const
   {
      return innerpath.strCaseCmp(o.innerpath.constPtr()) < 0;
   }
};

#define MAX_DEPTH 10 // I seriously doubt anybody needs more than a depth of 10

//
// Helper function to add a path to a collection, from a base.
//
static void W_recurseFiles(Collection<ArchiveDirFile> &paths, const char *base,
                           const char *subpath, Collection<qstring> &prevPaths,
                           int depth)
{
   qstring path(base);
   path.pathConcatenate(subpath);

   DIR *dir = opendir(path.constPtr());
   if(!dir)
      return;

   // Prevent repeated visits to the same paths due to symbolic links and the
   // like, if possible
   qstring real;
   I_GetRealPath(path.constPtr(), real);
   for(const qstring &prevPath : prevPaths)
   {
      if(prevPath == real)
         return;  // avoid duplicate paths
   }

   prevPaths.add(real);

   dirent *ent;
   while((ent = readdir(dir)))
   {
      // Skip UNIX hidden files and directory tree entries
      if(ent->d_name[0] == '.')
         continue;

      path = base;
      path.pathConcatenate(subpath).pathConcatenate(ent->d_name);

      struct stat sbuf;
      if(!stat(path.constPtr(), &sbuf)) // check for existence
      {
         if(S_ISDIR(sbuf.st_mode)) // if it's a directory, recurse in it
         {
            if(depth < MAX_DEPTH)
            {
               // we need to go deeper.
               path = subpath;
               path.pathConcatenate(ent->d_name);
               W_recurseFiles(paths, base, path.constPtr(), prevPaths, depth + 1);
            }
         }
         else
         {
            // Remove the leading path component
            ArchiveDirFile &adf = paths.addNew();

            // Normalize the path
            path.toLower();
            path.replace("\\", '/');

            adf.path = path;
            path = subpath;
            path.pathConcatenate(ent->d_name);

            // Normalize the subpath
            path.toLower();
            path.replace("\\", '/');

            adf.innerpath = path;
            adf.size = sbuf.st_size;
         }
      }
   }
   closedir(dir);
}

//
// Used to add directories arranged the same as PKE/PK3 files. Uses the same
// namespace system. It's different from addDirectory.
//
bool WadDirectory::addDirectoryAsArchive(openwad_t &openData,
                                         const wfileadd_t &addInfo,
                                         int startlump)
{
   // Check if it's a directory
   struct stat sbuf;
   if(stat(openData.filename, &sbuf) || !S_ISDIR(sbuf.st_mode))
   {
      handleOpenError(openData, addInfo, openData.filename);
      return false;
   }

   // read the file
   Collection<ArchiveDirFile> paths;
   ArchiveDirFile proto;
   paths.setPrototype(&proto);

   qstring qtemp(openData.filename);
   qtemp.replace("\\", '/');
   qtemp.toLower();
   sourcepath_t *sourcepath = estructalloc(sourcepath_t, 1);
   sourcepath->sourcenum = source;
   sourcepath->path = qtemp.duplicate();
   e_SourceNumHash.addObject(sourcepath);

   Collection<qstring> prevPaths;
   W_recurseFiles(paths, openData.filename, "", prevPaths, 0);

   if(!paths.isEmpty())
   {
      Collection<qstring> wadPaths; // only lists the wads

      // Sort the list in a case insensitive way, to prevent platform-dependent
      // ordering
      std::sort(paths.begin(), paths.end());

      // we now have the files neatly ordered
      lumpinfo_t *lump_p = reAllocLumpInfo(static_cast<int>(paths.getLength()),
                                           startlump);

      for(int i = startlump; i < numlumps; i++, lump_p++)
      {
         const ArchiveDirFile &adf = paths[i - startlump];

         size_t length = adf.innerpath.length();
         if(length > 4 && adf.size >= 28 &&
            !strcasecmp(adf.innerpath.constPtr() + length - 4, ".wad"))
         {
            // This is a WAD. Put it into the separate list instead of adding its
            // name as a duplicate lump
            wadPaths.add(adf.path);
            continue;
         }

         lump_p->type = lumpinfo_t::lump_file;
         lump_p->lfn = adf.path.duplicate();
         lump_p->size = adf.size;
         lump_p->source = source;
         int li_namespace;
         if((li_namespace = W_NamespaceForFilePath(adf.innerpath.constPtr())) != -1)
         {
            lump_p->li_namespace = li_namespace;
            W_LumpNameFromFilePath(adf.innerpath.constPtr(),
                                   lump_p->name, li_namespace);
         }
      }

      // check for WAD files
      for(const qstring &wadPath : wadPaths)
      {
         edefstructvar(wfileadd_t, addInfo);
         addInfo.filename = wadPath.constPtr();
         addInfo.li_namespace = lumpinfo_t::ns_global;
         addInfo.requiredFmt = -1;
         addFile(addInfo);
      }
   }

   incrementSource(openData);

   return true;
}

//
// WadDirectory::addInMemoryWad
//
// Externally trigger addition of a wad file that is loaded completely
// into memory.
//
bool WadDirectory::addInMemoryWad(void *buffer, size_t size)
{
   wfileadd_t addInfo;

   memset(&addInfo, 0, sizeof(addInfo));

   addInfo.memory = buffer;
   addInfo.size   = size;
   addInfo.flags  = WFA_OPENFAILFATAL | WFA_INMEMORY;

   if(!ispublic)
      addInfo.flags |= WFA_PRIVATE;

   return addFile(addInfo);
}

// jff 1/23/98 Create routines to reorder the master directory
// putting all flats into one marked block, and all sprites into another.
// This will allow loading of sprites and flats from a PWAD with no
// other changes to code, particularly fast hashes of the lumps.
//
// killough 1/24/98 modified routines to be a little faster and smaller

static int W_IsMarker(const char *marker, const char *name)
{
   return !strncasecmp(name, marker, 8) ||
      (*name == *marker && !strncasecmp(name+1, marker, 7));
}

// namespace data
struct nsdata_t
{
   const char *startMarker;
   const char *endMarker;
   int li_namespace;
};

// namespaces
static nsdata_t wadNameSpaces[lumpinfo_t::ns_max] =
{
   { NULL,       NULL,     lumpinfo_t::ns_global       },
   { "S_START",  "S_END",  lumpinfo_t::ns_sprites      },
   { "F_START",  "F_END",  lumpinfo_t::ns_flats        },
   { "C_START",  "C_END",  lumpinfo_t::ns_colormaps    },
   { "T_START",  "T_END",  lumpinfo_t::ns_translations },
   { NULL,       NULL,     lumpinfo_t::ns_demos        },
   { "A_START",  "A_END",  lumpinfo_t::ns_acs          },
   { NULL,       NULL,     lumpinfo_t::ns_pads         },
   { "TX_START", "TX_END", lumpinfo_t::ns_textures     },
   { NULL,       NULL,     lumpinfo_t::ns_graphics     },
   { NULL,       NULL,     lumpinfo_t::ns_sounds       },
   { "HI_START", "HI_END", lumpinfo_t::ns_hires        }, // TODO: Implement
};

//
// WadNamespace
//
// This class is a utility used only during coalesceMarkedResources, to build
// the namespace_t structures for the WadDirectory instance.
//
class WadNamespace
{
protected:
   bool inMarkers;                     // currently between markers?
   nsdata_t *nsdata;
   PODCollection<lumpinfo_t *> marked; // lumps in namespace
   int numMarked;                      // number added

   // Returns true if the lump is a start marker for this namespace
   bool lumpIsStartMarker(lumpinfo_t *lump) const
   {
      return (nsdata->startMarker &&
              W_IsMarker(nsdata->startMarker, lump->name));
   }

   // Returns false if the lump is an end marker for this namespace
   bool lumpIsEndMarker(lumpinfo_t *lump) const
   {
      return (nsdata->endMarker &&
              W_IsMarker(nsdata->endMarker, lump->name));
   }

public:
   WadNamespace() 
      : inMarkers(false), nsdata(nullptr), marked(), numMarked(0)
   {
   }

   // Initialize from static data
   void setNSData(nsdata_t *pNsData) { nsdata = pNsData; }

   // Add a lump into this namespace
   void addLump(lumpinfo_t *lump) 
   {
      marked.add(lump); 
      lump->li_namespace = nsdata->li_namespace;
      ++numMarked;
   }

   // Check if a lump belongs in this namespace
   void checkLump(lumpinfo_t *lump)
   {
      if(lumpIsStartMarker(lump))
         inMarkers = true;
      else if(lumpIsEndMarker(lump))
         inMarkers = false;
      else if(inMarkers || lump->li_namespace == nsdata->li_namespace)
      {
         // we are either between markers, or dealing with a pre-marked lump;
         // special cases:
         switch(nsdata->li_namespace)
         {
         case lumpinfo_t::ns_sprites:
            // sf 10/26/99: ignore sprite lumps smaller than 8 bytes (the
            // smallest possible) in size -- this was used by some dmadds
            // wads as an 'empty' graphics resource
            if(lump->size > 8)
               addLump(lump);
            break;
         case lumpinfo_t::ns_flats:
            // SoM: Ignore marker lumps inside F_START and F_END
            if(lump->size > 0)
               addLump(lump);
            break;
         case lumpinfo_t::ns_hires:
            // MaxW: Ignore these, as they're to be implemented later and cause issues otherwise
            lump->li_namespace = nsdata->li_namespace;
            break;
         default:
            addLump(lump);
            break;
         }
      }
   }

   // Get the number of lumps marked as belonging
   int getNumMarked() const { return numMarked; }

   // Merge this namespace back into the master directory
   int mergeToDirectory(lumpinfo_t **lumpinfo, int index)
   {
      int i;

      // append the marked list to the current end of the directory
      for(i = 0; i < numMarked; i++)
         lumpinfo[index + i] = marked[i];

      return index + i;
   }
};

//
// WadDirectory::coalesceMarkedResources
//
// killough 4/17/98: add namespace tags
//
void WadDirectory::coalesceMarkedResources()
{
   WadNamespace namespaces[lumpinfo_t::ns_max];
   int i, ns;

   for(i = 0; i < lumpinfo_t::ns_max; i++)
      namespaces[i].setNSData(&wadNameSpaces[i]);

   // scan the entire wad directory; save off lumps that belong in each
   // namespace
   for(ns = lumpinfo_t::ns_MIN_LOCAL; ns < lumpinfo_t::ns_max; ns++)
   {
      for(int lumpnum = 0; lumpnum < numlumps; lumpnum++)
         namespaces[ns].checkLump(lumpinfo[lumpnum]);
   }

   // now add any remaining unmarked lumps to ns_global
   for(i = 0; i < numlumps; i++)
      namespaces[lumpinfo_t::ns_global].checkLump(lumpinfo[i]);

   // re-allocate lumpinfo if necessary
   int totalLumps = 0;
   for(ns = 0; ns < lumpinfo_t::ns_max; ns++)
   {
      int numMarked = namespaces[ns].getNumMarked();
      m_namespaces[ns].numLumps = numMarked;
      totalLumps += numMarked;
   }

   if(totalLumps != numlumps)
   {
      lumpinfo = erealloc(lumpinfo_t **, lumpinfo, totalLumps * sizeof(*lumpinfo));
      numlumps = totalLumps;
   }

   // re-order the directory so that all namespaces are contiguous
   int curLump = 0;
   for(ns = 0; ns < lumpinfo_t::ns_max; ns++)
   {
      m_namespaces[ns].firstLump = curLump;
      curLump = namespaces[ns].mergeToDirectory(lumpinfo, curLump);
   }
}

//
// W_LumpNameHash
//
// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
// by Lee Killough
//
unsigned int WadDirectory::LumpNameHash(const char *s)
{
   using namespace ectype;
   unsigned int hash;

   (void) ((hash =        toUpper(s[0]), s[1]) &&
           (hash = hash*3+toUpper(s[1]), s[2]) &&
           (hash = hash*2+toUpper(s[2]), s[3]) &&
           (hash = hash*2+toUpper(s[3]), s[4]) &&
           (hash = hash*2+toUpper(s[4]), s[5]) &&
           (hash = hash*2+toUpper(s[5]), s[6]) &&
           (hash = hash*2+toUpper(s[6]),
            hash = hash*2+toUpper(s[7]))
           );
   return hash;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases Doom performance over 300%. This is the
// single most important optimization of the original Doom sources, because
// lump name lookup is used so often, and the original Doom used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
// killough 4/17/98: add namespace parameter to prevent collisions
// between different resources such as flats, sprites, colormaps
//
// haleyjd 03/01/09: added InDir version.
//
int WadDirectory::checkNumForName(const char *name, int li_namespace) const
{
   // Hash function maps the name to one of possibly numlump chains.
   // It has been tuned so that the average chain length never exceeds 2.

   unsigned int hashkey = LumpNameHash(name) % (unsigned int)numlumps;
   int i = lumpinfo[hashkey]->index;

   // We search along the chain until end, looking for case-insensitive
   // matches which also match a namespace tag. Separate hash tables are
   // not used for each namespace, because the performance benefit is not
   // worth the overhead, considering namespace collisions are rare in
   // Doom wads.

   while(i >= 0 && (strncasecmp(lumpinfo[i]->name, name, 8) ||
         lumpinfo[i]->li_namespace != li_namespace))
      i = lumpinfo[i]->next;

   // Return the matching lump, or -1 if none found.
   return i;
}

//
// W_CheckNumForName
//
// haleyjd: Now a global directory convenience routine.
//
int W_CheckNumForName(const char *name)
{
   return wGlobalDir.checkNumForName(name);
}

//
// W_CheckNumForNameNS
//
// haleyjd: Separated from W_CheckNumForName. Looks in a specific namespace.
//
int W_CheckNumForNameNS(const char *name, int li_namespace)
{
   return wGlobalDir.checkNumForName(name, li_namespace);
}

//
// WadDirectory::checkNumForNameNSG
//
// haleyjd 12/24/13: Looks in both a specified namespace and the global
// namespace for a lump. If the namespace lump is found and is from a newer
// source than any global lump, it will be returned. Otherwise, if the
// global lump exists, it will be returned.
//
int WadDirectory::checkNumForNameNSG(const char *name, int ns) const
{
   int num = -1;
   int inNS, inGlobal;
   lumpinfo_t *nsLump = NULL, *globalLump = NULL;

   if((inNS = checkNumForName(name, ns)) >= 0)
      nsLump = lumpinfo[inNS];
   if((inGlobal = checkNumForName(name, lumpinfo_t::ns_global)) >= 0)
      globalLump = lumpinfo[inGlobal];

   if(!nsLump)
      num = inGlobal;
   else if(!globalLump)
      num = inNS;
   else
      num = nsLump->source >= globalLump->source ? inNS : inGlobal;

   return num;
}

//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
int WadDirectory::getNumForName(const char* name) const     // killough -- const added
{
   int i = checkNumForName(name);
   if(i == -1)
      I_Error("WadDirectory::getNumForName: %.8s not found!\n", name); // killough .8 added
   return i;
}

//
// WadDirectory::getNumForNameNSG
//
int WadDirectory::getNumForNameNSG(const char *name, int ns) const
{
   int i = checkNumForNameNSG(name, ns);
   if(i == -1)
      I_Error("WadDirectory::getNumForNameNSG: %.8s not found!\n", name);
   return i;
}

int W_GetNumForName(const char *name)
{
   return wGlobalDir.getNumForName(name);
}

//
// WadDirectory::checkNumForLFN
//
// haleyjd 04/21/13: Support for looking up lumps by their long file name.
//
int WadDirectory::checkNumForLFN(const char *lfn, int li_namespace) const
{
   lumpinfo_t *currinfo = e_LFNHash.keyIterator(nullptr, lfn);

   while(currinfo != nullptr && currinfo->li_namespace != li_namespace)
       currinfo = e_LFNHash.keyIterator(currinfo, lfn);

   return currinfo ? currinfo->index : -1;
}

//
// WadDirectory::checkNumForLFNNSG
//
// As above but falling back to ns_global if not initially found in
// preferred namespace.
//
int WadDirectory::checkNumForLFNNSG(const char *name, int ns) const
{
   int num = -1;
   int inNS, inGlobal;
   lumpinfo_t *nsLump = NULL, *globalLump = NULL;

   if((inNS = checkNumForLFN(name, ns)) >= 0)
      nsLump = lumpinfo[inNS];
   if((inGlobal = checkNumForLFN(name, lumpinfo_t::ns_global)) >= 0)
      globalLump = lumpinfo[inGlobal];

   if(!nsLump)
      num = inGlobal;
   else if(!globalLump)
      num = inNS;
   else
      num = nsLump->source >= globalLump->source ? inNS : inGlobal;

   return num;
}

//
// WadDirectory::getLumpNameChain
//
// haleyjd 03/18/10: routine for getting the lumpinfo hash chain for lumps of a
// given name, to replace code segments doing this in several different places.
//
lumpinfo_t *WadDirectory::getLumpNameChain(const char *name) const
{
   return lumpinfo[LumpNameHash(name) % (unsigned int)numlumps];
}

//
// As above, but for long file names.
//
lumpinfo_t *WadDirectory::getLumpLFNChain(const char *name) const
{
   return e_LFNHash.objectForKey(name);
}

//
// WadDirectory::getLumpName
//
// haleyjd 02/04/14: Sometimes we need to go the opposite direction conveniently.
//
const char *WadDirectory::getLumpName(int lumpnum) const
{
   if(lumpnum < 0 || lumpnum >= numlumps)
      I_Error("WadDirectory::getLumpName: bad lump number %d\n", lumpnum);

   return lumpinfo[lumpnum]->name;
}

//
// WadDirectory::getLumpFileName
//
// Get the filename of the file from which a particular lump came.
//
const char *WadDirectory::getLumpFileName(int lump) const
{
   if(lump < 0 || lump >= numlumps)
      return nullptr;

   size_t lumpIdx = static_cast<size_t>(lump);
   return WadDirectoryPimpl::FileNameForSource(lumpinfo[lumpIdx]->source);
}

//
// W_InitLumpHash
//
// killough 1/31/98: Initialize lump hash table
//
void WadDirectory::initLumpHashes()
{
   int i;

   for(i = 0; i < numlumps; i++)
   {
      lumpinfo[i]->index     = -1; // mark slots empty
      lumpinfo[i]->selfindex =  i; // haleyjd: record position in array
   }

   // Insert nodes to the beginning of each chain, in first-to-last
   // lump order, so that the last lump of a given name appears first
   // in any chain, observing pwad ordering rules. killough

   for(i = 0; i < numlumps; i++)
   {                                           // hash function:
      // haleyjd 10/28/12: if lump name is empty, do not add it into the hash.
      if(lumpinfo[i]->name[0])
      {
         const unsigned int j = LumpNameHash(lumpinfo[i]->name) % (unsigned int)numlumps;
         lumpinfo[i]->next    = lumpinfo[j]->index; // Prepend to list
         lumpinfo[j]->index   = i;
      }

      // haleyjd 04/21/12: add to lfn hash also if has a valid LFN
      if(lumpinfo[i]->lfn && *lumpinfo[i]->lfn)
         e_LFNHash.addObject(lumpinfo[i]);
   }
}

// End of lump hashing -- killough 1/31/98

//
// W_InitResources
//
void WadDirectory::initResources() // sf
{
   // jff 1/23/98
   // get all the sprites and flats into one marked block each
   coalesceMarkedResources();

   // set up caching
   // sf: caching now done in the lumpinfo_t's

   // killough 1/31/98: initialize lump hash table
   initLumpHashes();
}

//
// W_InitMultipleFiles
//
// Pass a null terminated list of files to use.
// All files are optional, but at least one file must be found.
// Files with a .wad extension are idlink files with multiple lumps.
// Other files are single lumps with the base filename for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file overrides earlier ones.
//
void WadDirectory::initMultipleFiles(wfileadd_t *files)
{
   wfileadd_t *curfile;

   // Basic initialization
   numlumps = 0;
   lumpinfo = nullptr;
   ispublic = true;   // Is a public wad directory
   type     = NORMAL; // Not a managed directory

   curfile = files;
   
   // open all the files, load headers, and count lumps
   while(curfile->filename)
   {
      // haleyjd 07/11/09: ignore empty filenames
      if((curfile->filename)[0])
      {
         if(curfile->flags & WFA_DIRECTORY_RAW)
            addDirectory(curfile->filename);
         else
            addFile(*curfile);
      }

      ++curfile;
   }
   
   if(!numlumps)
      I_Error("WadDirectory::InitMultipleFiles: no files found\n");
   
   initResources();
}

bool WadDirectory::addNewFile(const char *filename)
{
   wfileadd_t newfile;

   newfile.filename     = filename;
   newfile.f            = nullptr;
   newfile.baseoffset   = 0;
   newfile.li_namespace = lumpinfo_t::ns_global;
   newfile.requiredFmt  = -1;
   newfile.flags        = WFA_ALLOWINEXACTFN | WFA_ALLOWHACKS;

   if(!addFile(newfile))
      return false;

   initResources();         // reinit lump lookups etc
   return true;
}

bool WadDirectory::addNewPrivateFile(const char *filename)
{
   wfileadd_t newfile;

   newfile.filename     = filename;
   newfile.f            = nullptr;
   newfile.baseoffset   = 0;
   newfile.li_namespace = lumpinfo_t::ns_global;
   newfile.requiredFmt  = -1;
   newfile.flags        = WFA_PRIVATE;

   if(!addFile(newfile))
      return false;

   // there is no resource coalescence on this particular brand of private
   // wad file, so just call W_InitLumpHash.
   initLumpHashes();

   return true;
}

//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
int WadDirectory::lumpLength(int lump) const
{
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::LumpLength: %i >= numlumps\n", lump);
   return static_cast<int>(lumpinfo[lump]->size);
}

int W_LumpLength(int lump)
{
   return wGlobalDir.lumpLength(lump);
}

//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
void WadDirectory::readLump(int lump, void *dest,
                            const WadLumpLoader *lfmt) const
{
   size_t c;
   lumpinfo_t *lptr;
   
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::ReadLump: %d >= numlumps\n", lump);

   lptr = lumpinfo[lump];

   // don't read from zero-size lump, or into a null buffer
   if(!lptr->size || !dest)
      return;

   // killough 1/31/98: Reload hack (-wart) removed

   c = LumpHandlers[lptr->type].readLump(lptr, dest);
   if(c < lptr->size)
   {
      I_Error("WadDirectory::readLump: only read %d of %d on lump %d\n", 
              (int)c, (int)lptr->size, lump);
   }

   // haleyjd 06/26/11: Apply lump formatting/preprocessing if provided
   if(lfmt)
   {
      WadLumpLoader::Code code = lfmt->verifyData(lptr);

      switch(code)
      {
      case WadLumpLoader::CODE_OK:
         // When OK is returned, do formatting
         code = lfmt->formatData(lptr);
         break;
      default:
         break;
      }
 
      // Does the formatter want us to bomb out in response to an error?
      if(code == WadLumpLoader::CODE_FATAL) 
         I_Error("WadDirectory::readLump: lump %s is malformed\n", lptr->name);
   }
}

//
// W_CacheLumpNum
//
// killough 4/25/98: simplified
//
void *WadDirectory::cacheLumpNum(int lump, int tag,
                                 const WadLumpLoader *lfmt) const
{
   lumpinfo_t::lumpformat fmt = lumpinfo_t::fmt_default;

   if(lfmt)
      fmt = lfmt->formatIndex();

   // haleyjd 08/14/02: again, should not be RANGECHECK only
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::CacheLumpNum: %i >= numlumps\n", lump);
   
   if(!(lumpinfo[lump]->cache[fmt]))      // read the lump in
   {
      readLump(lump, 
               Z_Malloc(lumpLength(lump), tag, &(lumpinfo[lump]->cache[fmt])), 
               lfmt);
   }
   else
   {
      // haleyjd: do not lower cache level and cause static users to lose their 
      // data unexpectedly (ie, do not change PU_STATIC into PU_CACHE -- that 
      // must be done using Z_ChangeTag explicitly)
      
      int oldtag = Z_CheckTag(lumpinfo[lump]->cache[fmt]);

      if(tag < oldtag) 
         Z_ChangeTag(lumpinfo[lump]->cache[fmt], tag);
   }
   
   return lumpinfo[lump]->cache[fmt];
}

//
// W_CacheLumpName
//
// haleyjd 06/26/11: Restored to status as a method of WadDirectory instead of
// being a macro, as it needs to take an optional parameter now.
//
void *WadDirectory::cacheLumpName(const char *name, int tag,
                                  const WadLumpLoader *lfmt) const
{
   return cacheLumpNum(getNumForName(name), tag, lfmt);
}

//
// WadDirectory::cacheLumpAuto
//
// Cache a copy of a lump into a ZAutoBuffer, by lump num.
//
void WadDirectory::cacheLumpAuto(int lumpnum, ZAutoBuffer &buffer) const
{
   if(lumpnum < 0 || lumpnum >= numlumps)
      I_Error("WadDirectory::cacheLumpAuto: %i >= numlumps\n", lumpnum);

   size_t size = lumpinfo[lumpnum]->size;

   buffer.alloc(size, false);
   readLump(lumpnum, buffer.get());
}

//
// WadDirectory::cacheLumpAuto
//
// Cache a copy of a lump into a ZAutoBuffer, by lump name.
//
void WadDirectory::cacheLumpAuto(const char *name, ZAutoBuffer &buffer) const
{
   cacheLumpAuto(getNumForName(name), buffer);
}

//
// WadDirectory::writeLump
//
// Write out a lump to a physical file.
//
bool WadDirectory::writeLump(const char *lumpname, const char *destpath) const
{
   int    lumpnum;
   size_t size;
   
   if((lumpnum = checkNumForName(lumpname)) >= 0 && 
      (size    = lumpinfo[lumpnum]->size  ) >  0)
   {
      ZAutoBuffer lumpData(size, false);
      readLump(lumpnum, lumpData.get());
      return M_WriteFile(destpath, lumpData.get(), size);
   }
   else
      return false;
}

// Predefined lumps removed -- sf

//
// W_LumpCheckSum
//
// sf
// haleyjd 08/27/11: Rewritten to use CRC32 hash algorithm
//
uint32_t W_LumpCheckSum(int lumpnum)
{
   uint8_t  *lump    = (uint8_t *)(wGlobalDir.cacheLumpNum(lumpnum, PU_CACHE));
   uint32_t  lumplen = (uint32_t )(wGlobalDir.lumpLength(lumpnum));

   return HashData(HashData::CRC32, lump, lumplen).getDigestPart(0);
}

//
// W_FreeDirectoryLumps
//
// haleyjd 06/26/09
// Frees all the lumps cached in a private directory.
//
// Note that it's necessary to use Z_Free and not Z_ChangeTag 
// on all resources loaded from a private wad directory if the 
// directory is destroyed. Otherwise the zone heap will maintain
// dangling pointers into the freed wad directory, and heap 
// corruption would occur at a seemingly random time after an 
// arbitrary Z_Malloc call freed the cached resources.
//
void WadDirectory::freeDirectoryLumps()
{
   int i, j;
   lumpinfo_t **li = lumpinfo;

   for(i = 0; i < numlumps; i++)
   {
      for(j = 0; j != lumpinfo_t::fmt_maxfmts; j++)
      {
         if(li[i]->cache[j])
         {
            Z_Free(li[i]->cache[j]);
            li[i]->cache[j] = nullptr;
         }
      }

      // free long filenames
      if(li[i]->lfn)
         efree(li[i]->lfn);
   }
}

//
// W_FreeDirectoryAllocs
//
// haleyjd 06/06/10
// Frees all lumpinfo_t's allocated for a wad directory.
//
void WadDirectory::freeDirectoryAllocs()
{
   size_t i, len = pImpl->infoptrs.getLength();

   // free each lumpinfo_t allocation
   for(i = 0; i < len; i++)
      efree(pImpl->infoptrs[i]);

   pImpl->infoptrs.clear();
}

//
// WadDirectory::close
//
// haleyjd 03/09/11: Abstracted out of I_Pick_CloseWad and W_delManagedDir.
//
void WadDirectory::close()
{
   // close the wad file if it is open; public directories can't be closed
   if(lumpinfo && !ispublic)
   {
      // free all resources loaded from the wad
      freeDirectoryLumps();

      if(lumpinfo[0]->type == lumpinfo_t::lump_direct && lumpinfo[0]->direct.file)
         fclose(lumpinfo[0]->direct.file);

      // free all lumpinfo_t's allocated for the wad
      freeDirectoryAllocs();

      // free the private wad directory
      Z_Free(lumpinfo);

      lumpinfo = nullptr;
   }
}

//=============================================================================
//
// haleyjd 07/12/07: Implementor functions for individual lump types
//

//
// Direct lumps -- lumps that consist of an entire physical file, or a part
// of one. This includes normal files and wad lumps; to the code, it makes no
// difference.
//

static size_t W_DirectReadLump(lumpinfo_t *l, void *dest)
{
   size_t size = l->size;
   size_t ret;
   directlump_t &direct = l->direct;

   // killough 10/98: Add flashing disk indicator
   fseek(direct.file, static_cast<long>(direct.position), SEEK_SET);
   ret = fread(dest, 1, size, direct.file);

   return ret;
}

//
// Memory lumps -- lumps that are held in a static memory buffer
//

static size_t W_MemoryReadLump(lumpinfo_t *l, void *dest)
{
   size_t size = l->size;
   memorylump_t &memory = l->memory;

   // killough 1/31/98: predefined lump data
   memcpy(dest, (byte *)(memory.data) + memory.position, size);

   return size;
}

//
// Directory file lumps -- lumps that are physical files on disk that are
// not kept open except when being read.
//

static size_t W_FileReadLump(lumpinfo_t *l, void *dest)
{
   FILE *f;
   size_t size     = l->size;
   size_t sizeread = 0;

   if((f = fopen(l->lfn, "rb")))
   {
      sizeread = fread(dest, 1, size, f);
      fclose(f);
   }
   
   return sizeread;
}

//
// ZIP lumps -- files embedded inside a ZIP archive. The ZipFile
// and ZipLump classes take care of all the specifics.
//

static size_t W_ZipReadLump(lumpinfo_t *l, void *dest)
{
   l->zip.zipLump->read(dest);

   // if I_Error wasn't invoked, we can assume the full read was
   // successful.
   return l->size;
}

//----------------------------------------------------------------------------
//
// $Log: w_wad.c,v $
// Revision 1.20  1998/05/06  11:32:00  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.19  1998/05/03  22:43:09  killough
// beautification, header #includes
//
// Revision 1.18  1998/05/01  14:53:59  killough
// beautification
//
// Revision 1.17  1998/04/27  02:06:41  killough
// Program beautification
//
// Revision 1.16  1998/04/17  10:34:53  killough
// Tag lumps with namespace tags to resolve collisions
//
// Revision 1.15  1998/04/06  04:43:59  killough
// Add C_START/C_END support, remove non-standard C code
//
// Revision 1.14  1998/03/23  03:42:59  killough
// Fix drive-letter bug and force marker lumps to 0-size
//
// Revision 1.12  1998/02/23  04:59:18  killough
// Move TRANMAP init code to r_data.c
//
// Revision 1.11  1998/02/20  23:32:30  phares
// Added external tranmap
//
// Revision 1.10  1998/02/20  22:53:25  phares
// Moved TRANMAP initialization to w_wad.c
//
// Revision 1.9  1998/02/17  06:25:07  killough
// Make numlumps static add #ifdef RANGECHECK for perf
//
// Revision 1.8  1998/02/09  03:20:16  killough
// Fix garbage printed in lump error message
//
// Revision 1.7  1998/02/02  13:21:04  killough
// improve hashing, add predef lumps, fix err handling
//
// Revision 1.6  1998/01/26  19:25:10  phares
// First rev with no ^Ms
//
// Revision 1.5  1998/01/26  06:30:50  killough
// Rewrite merge routine to use simpler, robust algorithm
//
// Revision 1.3  1998/01/23  20:28:11  jim
// Basic sprite/flat functionality in PWAD added
//
// Revision 1.2  1998/01/22  05:55:58  killough
// Improve hashing algorithm
//
//----------------------------------------------------------------------------
