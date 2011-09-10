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
//      Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "d_io.h"  // SoM 3/12/2002: moved unistd stuff into d_io.h
#include <fcntl.h>

#include "c_io.h"
#include "s_sound.h"
#include "p_skin.h"
#include "m_misc.h"
#include "w_wad.h"

//
// GLOBALS
//

// Location of each lump on disk.
waddir_t w_GlobalDir = { NULL, 0, true };

// haleyjd: WAD_TODO: put wad system globals into structure
FILE *iwadhandle;     // sf: the handle of the main iwad
FILE *firstWadHandle; // haleyjd: track handle of first wad added

//
// haleyjd 07/12/07: structure for transparently manipulating lumps of
// different types
//

typedef struct lumptype_s
{
   size_t (*readLump)(lumpinfo_t *, void *, size_t);
} lumptype_t;

static size_t W_DirectReadLump(lumpinfo_t *, void *, size_t);
static size_t W_MemoryReadLump(lumpinfo_t *, void *, size_t);

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
};

//
// LUMP BASED ROUTINES.
//

static int w_sound_update_type;

void D_NewWadLumps(FILE *handle, int sound_update_type);

//
// W_addInfoPtr
//
// haleyjd 06/06/10: I need to track all lumpinfo_t allocations that are
// added to a waddir_t's lumpinfo directory, or else these allocations get
// orphaned when freeing a private wad directory. Oops! ;)
//
static void W_addInfoPtr(waddir_t *dir, lumpinfo_t *infoptr)
{
   // reallocate if necessary
   if(dir->numallocs >= dir->numallocsa)
   {
      dir->numallocsa = dir->numallocsa ? dir->numallocsa * 2 : 32;

      dir->infoptrs =
        (lumpinfo_t **)(realloc(dir->infoptrs, dir->numallocsa * sizeof(lumpinfo_t *)));
   }
   
   // add it
   dir->infoptrs[dir->numallocs] = infoptr;
   dir->numallocs++;
}

static int source; // haleyjd 03/18/10

// [CG] Clears a WAD directory of its data.
void W_ClearWadDir(waddir_t *dir)
{
   unsigned int i, j;
   unsigned int handle_count = 0;
   FILE **handles = NULL;

   if(dir->lumpinfo)
   {
      for(i = 0; i < dir->numlumps_before_coalescing; i++)
      {
         // [CG] Clear the lump's cache.
         if(dir->lumpinfo[i]->cache)
         {
            Z_Free(dir->lumpinfo[i]->cache);
            dir->lumpinfo[i]->cache = NULL;
         }

         // [CG] If this is an in-memory lump, free its in-memory data.
         if(dir->lumpinfo[i]->data)
         {
            Z_Free((void *)dir->lumpinfo[i]->data);
            dir->lumpinfo[i]->data = NULL;
         }

         // [CG] Add the file handle to the list of open file handles if it
         //      isn't in there already.
         if(dir->lumpinfo[i]->file)
         {
            if(handle_count == 0)
            {
               handle_count++;
               handles = realloc(handles, handle_count * sizeof(FILE *));
               handles[handle_count - 1] = dir->lumpinfo[i]->file;
            }
            else
            {
               for(j = 0; j < handle_count; j++)
               {
                  if(handles[j] == dir->lumpinfo[i]->file)
                     break;
               }
               if(j == handle_count)
               {
                  handle_count++;
                  handles = realloc(handles, handle_count * sizeof(FILE *));
                  handles[handle_count - 1] = dir->lumpinfo[i]->file;
               }
            }
         }
      }

      // [CG] Close all the open file handles that were found in the directory.
      for(i = 0; i < handle_count; i++)
         fclose(handles[i]);

      // [CG] Done with the file handle list.
      free(handles);
   }

   // [CG] Reset numlumps.
   dir->numlumps = dir->numlumps_before_coalescing = 0;

   // [CG] This actually frees the lumps themselves as they're allocated in
   //      large chunks, not individually for each lump.
   if(dir->infoptrs)
   {
      for(i = 0; i < dir->numallocs; i++)
         free(dir->infoptrs[i]);
   }

   dir->numallocs = 0;
}

//
// W_AddFile
//
// All files are optional, but at least one file must be found (PWAD, if all 
// required lumps are present).
// Files with a .wad extension are wadlink files with multiple lumps.
// Other files are single lumps with the base filename for the lump name.
//
// Reload hack removed by Lee Killough
// killough 1/31/98: static, const
//
static int W_AddFile(waddir_t *dir, const char *name, int li_namespace)
{
   wadinfo_t   header;
   lumpinfo_t* lump_p;
   unsigned    i;
   FILE        *handle;
   int         length;
   int         startlump;
   filelump_t  *fileinfo, *fileinfo2free = NULL; //killough
   filelump_t  singleinfo;
   char        *filename;
   lumpinfo_t  *newlumps;
   boolean     isWad;     // haleyjd 05/23/04

   // haleyjd 03/18/10: filename fix
   M_StringAlloca(&filename, 2, 1, name, ".wad");
   strcpy(filename, name);

   M_NormalizeSlashes(M_AddDefaultExtension(filename, ".wad"));  // killough 11/98

   // open the file and add to directory
   if((handle = fopen(filename, "rb")) == NULL)
   {
      if(strlen(name) > 4 && !strcasecmp(name + strlen(name) - 4 , ".lmp" ))
         return false; // sf: no errors

      // killough 11/98: allow .lmp extension if none existed before
      M_NormalizeSlashes(M_AddDefaultExtension(strcpy(filename, name), ".lmp"));
      
      if((handle = fopen(filename, "rb")) == NULL)
      {
         if(in_textmode)
            I_Error("Error: couldn't open %s\n", name);  // killough
         else
         {
            C_Printf(FC_ERROR "Couldn't open %s\n", name);
            return true; // error
         }
      }
   }
  
   if(dir->ispublic && in_textmode)
      printf(" adding %s\n", filename);   // killough 8/8/98
   startlump = dir->numlumps;

   // killough:
   if(strlen(filename) <= 4 || strcasecmp(filename+strlen(filename)-4, ".wad" ))
   {
      // single lump file
      isWad = false; // haleyjd 05/23/04
      fileinfo = &singleinfo;
      singleinfo.filepos = 0;
      singleinfo.size = SwapLong(M_FileLength(handle));
      M_ExtractFileBase(filename, singleinfo.name);
      dir->numlumps++;
   }
   else
   {
      // WAD file
      isWad = true; // haleyjd 05/23/04
      
      if(fread(&header, sizeof(header), 1, handle) < 1)
         I_Error("Failed reading header for wad file %s\n", filename);
      
      if(strncmp(header.identification, "IWAD", 4) && 
         strncmp(header.identification, "PWAD", 4))
      {
         I_Error("Wad file %s doesn't have IWAD or PWAD id\n", filename);
      }
      
      header.numlumps     = SwapLong(header.numlumps);
      header.infotableofs = SwapLong(header.infotableofs);
      
      length = header.numlumps * sizeof(filelump_t);
      fileinfo2free = fileinfo = (filelump_t *)(malloc(length)); // killough
      
      fseek(handle, header.infotableofs, SEEK_SET);

      if(fread(fileinfo, length, 1, handle) < 1)
         I_Error("Failed reading directory for wad file %s\n", filename);
      
      dir->numlumps += header.numlumps;
   }
   
   // Fill in lumpinfo
   dir->lumpinfo = (lumpinfo_t **)(realloc(dir->lumpinfo, dir->numlumps * sizeof(lumpinfo_t *)));

   // space for new lumps
   newlumps = (lumpinfo_t *)(malloc((dir->numlumps - startlump) * sizeof(lumpinfo_t)));
   lump_p   = newlumps;

   // haleyjd: keep track of this allocation of lumps
   W_addInfoPtr(dir, newlumps);
   
   // update IWAD handle?   
   if(isWad && dir->ispublic)
   {
      // haleyjd 06/21/04: track handle of first wad added also
      if(!firstWadHandle)
         firstWadHandle = handle;

      // haleyjd 07/13/09: only track the first IWAD found
      if(!iwadhandle && !strncmp(header.identification, "IWAD", 4))
         iwadhandle = handle;
   }
  
   for(i = startlump; i < (unsigned)dir->numlumps; i++, lump_p++, fileinfo++)
   {
      dir->lumpinfo[i] = lump_p;
      lump_p->type     = lumpinfo_t::lump_direct; // haleyjd
      lump_p->file     = handle;
      lump_p->position = (size_t)(SwapLong(fileinfo->filepos));
      lump_p->size     = (size_t)(SwapLong(fileinfo->size));
      lump_p->source   = source; // haleyjd
      
      lump_p->data = lump_p->cache = NULL;         // killough 1/31/98
      lump_p->li_namespace = li_namespace;         // killough 4/17/98
      
      memset(lump_p->name, 0, 9);
      strncpy(lump_p->name, fileinfo->name, 8);
   }

   // haleyjd: increment source
   ++source;
   
   if(fileinfo2free)
      free(fileinfo2free); // killough
   
   if(dir->ispublic)
      D_NewWadLumps(handle, w_sound_update_type);
   
   return false; // no error
}

//
// W_AddSubFile
//
// haleyjd 05/28/10: Adds a wad file that is part of a bigger file.
//
static boolean W_AddSubFile(waddir_t *dir, const char *name, int li_namespace,
                            FILE *handle, size_t baseoffset)
{
   wadinfo_t   header;
   lumpinfo_t* lump_p;
   unsigned    i;
   int         length;
   int         startlump;
   filelump_t  *fileinfo, *fileinfo2free = NULL; //killough
   lumpinfo_t  *newlumps;
       
   if(dir->ispublic && in_textmode)
      printf(" adding %s\n", name);   // killough 8/8/98
   startlump = dir->numlumps;

   // haleyjd: seek to baseoffset first
   fseek(handle, baseoffset, SEEK_SET);
      
   if(fread(&header, sizeof(header), 1, handle) < 1)
      I_Error("Failed reading header for wad file %s\n", name);
      
   if(strncmp(header.identification, "IWAD", 4) && 
      strncmp(header.identification, "PWAD", 4))
   {
      I_Error("Wad file %s doesn't have IWAD or PWAD id\n", name);
   }
      
   header.numlumps     = SwapLong(header.numlumps);
   header.infotableofs = SwapLong(header.infotableofs);
      
   length = header.numlumps * sizeof(filelump_t);
   fileinfo2free = fileinfo = (filelump_t *)(malloc(length));    // killough
      
   fseek(handle, baseoffset + (unsigned int)header.infotableofs, SEEK_SET);

   if(fread(fileinfo, length, 1, handle) < 1)
      I_Error("Failed reading directory for wad file %s\n", name);
      
   dir->numlumps += header.numlumps;
   
   // Fill in lumpinfo
   dir->lumpinfo = (lumpinfo_t **)(realloc(dir->lumpinfo, dir->numlumps * sizeof(lumpinfo_t *)));

   // space for new lumps
   newlumps = (lumpinfo_t *)(malloc((dir->numlumps - startlump) * sizeof(lumpinfo_t)));
   lump_p   = newlumps;

   // haleyjd: keep track of this allocation of lumps
   W_addInfoPtr(dir, newlumps);
   
   // update IWAD handle?   
   if(dir->ispublic)
   {
      // haleyjd 06/21/04: track handle of first wad added also
      if(!firstWadHandle)
         firstWadHandle = handle;

      // haleyjd 07/13/09: only track the first IWAD found
      if(!iwadhandle && !strncmp(header.identification, "IWAD", 4))
         iwadhandle = handle;
   }
  
   for(i = startlump; i < (unsigned)dir->numlumps; i++, lump_p++, fileinfo++)
   {
      dir->lumpinfo[i] = lump_p;
      lump_p->type     = lumpinfo_t::lump_direct; // haleyjd
      lump_p->file     = handle;
      lump_p->position = (size_t)(SwapLong(fileinfo->filepos)) + baseoffset;
      lump_p->size     = (size_t)(SwapLong(fileinfo->size));
      lump_p->source   = source;
      
      lump_p->data = lump_p->cache = NULL;         // killough 1/31/98
      lump_p->li_namespace = li_namespace;         // killough 4/17/98
      
      memset(lump_p->name, 0, 9);
      strncpy(lump_p->name, fileinfo->name, 8);
   }

   // haleyjd: increment source
   ++source;
   
   if(fileinfo2free)
      free(fileinfo2free); // killough
   
   if(dir->ispublic)
      D_NewWadLumps(handle, w_sound_update_type);
   
   return false; // no error
}

//
// W_AddPrivateFile
//
// haleyjd 06/14/10: Adds a file to a strictly private wad directory.
//
static boolean W_AddPrivateFile(waddir_t *dir, const char *filename)
{
   wadinfo_t   header;
   lumpinfo_t* lump_p;
   unsigned    i;
   int         length;
   int         startlump;
   filelump_t  *fileinfo, *fileinfo2free = NULL; //killough
   lumpinfo_t  *newlumps;
   FILE        *handle;
   
   // open the file and add to directory
   if((handle = fopen(filename, "rb")) == NULL)
   {
      C_Printf(FC_ERROR "Couldn't open %s\n", filename);
      return false;
   }
       
   startlump = dir->numlumps;
      
   if(fread(&header, sizeof(header), 1, handle) < 1)
   {
      fclose(handle);
      C_Printf(FC_ERROR "Failed reading header for wad file %s\n", filename);
      return false;
   }
      
   if(strncmp(header.identification, "IWAD", 4) && 
      strncmp(header.identification, "PWAD", 4))
   {
      fclose(handle);
      C_Printf(FC_ERROR "Wad file %s doesn't have IWAD or PWAD id\n", filename);
      return false;
   }
      
   header.numlumps     = SwapLong(header.numlumps);
   header.infotableofs = SwapLong(header.infotableofs);
      
   length = header.numlumps * sizeof(filelump_t);
   fileinfo2free = fileinfo = (filelump_t *)(malloc(length));    // killough
      
   fseek(handle, (unsigned int)header.infotableofs, SEEK_SET);

   if(fread(fileinfo, length, 1, handle) < 1)
   {
      fclose(handle);
      free(fileinfo2free);
      C_Printf(FC_ERROR "Failed reading directory for wad file %s\n", filename);
      return false;
   }
      
   dir->numlumps += header.numlumps;
   
   // Fill in lumpinfo
   dir->lumpinfo = (lumpinfo_t **)(realloc(dir->lumpinfo, dir->numlumps * sizeof(lumpinfo_t *)));

   // space for new lumps
   newlumps = (lumpinfo_t *)(malloc((dir->numlumps - startlump) * sizeof(lumpinfo_t)));
   lump_p   = newlumps;

   // haleyjd: keep track of this allocation of lumps
   W_addInfoPtr(dir, newlumps);
     
   for(i = startlump; i < (unsigned)dir->numlumps; i++, lump_p++, fileinfo++)
   {
      dir->lumpinfo[i] = lump_p;
      lump_p->type     = lumpinfo_t::lump_direct; // haleyjd
      lump_p->file     = handle;
      lump_p->position = (size_t)(SwapLong(fileinfo->filepos));
      lump_p->size     = (size_t)(SwapLong(fileinfo->size));
      lump_p->source   = source;
      
      lump_p->data = lump_p->cache = NULL;          // killough 1/31/98
      lump_p->li_namespace = lumpinfo_t::ns_global; // killough 4/17/98
      
      memset(lump_p->name, 0, 9);
      strncpy(lump_p->name, fileinfo->name, 8);
   }

   // haleyjd: increment source
   ++source;
   
   if(fileinfo2free)
      free(fileinfo2free); // killough
      
   return true; // no error
}

// jff 1/23/98 Create routines to reorder the master directory
// putting all flats into one marked block, and all sprites into another.
// This will allow loading of sprites and flats from a PWAD with no
// other changes to code, particularly fast hashes of the lumps.
//
// killough 1/24/98 modified routines to be a little faster and smaller

static int IsMarker(const char *marker, const char *name)
{
   return !strncasecmp(name, marker, 8) ||
      (*name == *marker && !strncasecmp(name+1, marker, 7));
}

//
// W_CoalesceMarkedResource
//
// killough 4/17/98: add namespace tags
//
static void W_CoalesceMarkedResource(waddir_t *dir, const char *start_marker,
                                     const char *end_marker, int li_namespace)
{
   lumpinfo_t **marked = (lumpinfo_t **)(calloc(sizeof(*marked), dir->numlumps));
   size_t i, num_marked = 0, num_unmarked = 0;
   int is_marked = 0, mark_end = 0;
   lumpinfo_t *lump;
  
   dir->numlumps_before_coalescing = dir->numlumps;

   for(i = 0; i < (unsigned)dir->numlumps; i++)
   {
      lump = dir->lumpinfo[i];
      
      // If this is the first start marker, add start marker to marked lumps
      if(IsMarker(start_marker, lump->name))       // start marker found
      {
         if(!num_marked)
         {
            marked[0] = lump;
            marked[0]->li_namespace = lumpinfo_t::ns_global; // killough 4/17/98
            num_marked = 1;
         }
         is_marked = 1;                            // start marking lumps
      }
      else if(IsMarker(end_marker, lump->name))    // end marker found
      {
         mark_end = 1;                           // add end marker below
         is_marked = 0;                          // stop marking lumps
      }
      else if(is_marked || lump->li_namespace == li_namespace)
      {
         // if we are marking lumps,
         // move lump to marked list
         // sf: check for namespace already set

         // sf 26/10/99:
         // ignore sprite lumps smaller than 8 bytes (the smallest possible)
         // in size -- this was used by some dmadds wads
         // as an 'empty' graphics resource
         
         // SoM: Ignore marker lumps inside F_START and F_END
         if((li_namespace == lumpinfo_t::ns_sprites && lump->size > 8) ||
            (li_namespace == lumpinfo_t::ns_flats && lump->size > 0) ||
            (li_namespace != lumpinfo_t::ns_sprites && li_namespace != lumpinfo_t::ns_flats))
         {
            marked[num_marked] = lump;
            marked[num_marked]->li_namespace = li_namespace;
            num_marked++;
         }
      }
      else
      {
         dir->lumpinfo[num_unmarked] = lump;       // else move down THIS list
         num_unmarked++;
      }
   }
   
   // Append marked list to end of unmarked list
   memcpy(dir->lumpinfo + num_unmarked, marked, num_marked * sizeof(lumpinfo_t *));

   free(marked);                                   // free marked list
   
   dir->numlumps = num_unmarked + num_marked;      // new total number of lumps
   
   if(mark_end)                                    // add end marker
   {
      lumpinfo_t *newlump = (lumpinfo_t *)(calloc(1, sizeof(lumpinfo_t)));
      W_addInfoPtr(dir, newlump); // haleyjd: track it
      dir->lumpinfo[dir->numlumps] = newlump;
      dir->lumpinfo[dir->numlumps]->size = 0;  // killough 3/20/98: force size to be 0
      dir->lumpinfo[dir->numlumps]->li_namespace = lumpinfo_t::ns_global;   // killough 4/17/98
      strncpy(dir->lumpinfo[dir->numlumps]->name, end_marker, 8);
      dir->numlumps++;
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
unsigned int W_LumpNameHash(const char *s)
{
  unsigned int hash;
  (void) ((hash =        toupper(s[0]), s[1]) &&
          (hash = hash*3+toupper(s[1]), s[2]) &&
          (hash = hash*2+toupper(s[2]), s[3]) &&
          (hash = hash*2+toupper(s[3]), s[4]) &&
          (hash = hash*2+toupper(s[4]), s[5]) &&
          (hash = hash*2+toupper(s[5]), s[6]) &&
          (hash = hash*2+toupper(s[6]),
           hash = hash*2+toupper(s[7]))
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
int W_CheckNumForNameInDir(waddir_t *dir, const char *name, int li_namespace)
{
   // Hash function maps the name to one of possibly numlump chains.
   // It has been tuned so that the average chain length never exceeds 2.
   
   register int i = dir->lumpinfo[W_LumpNameHash(name) % 
                                  (unsigned)dir->numlumps]->index;

   // We search along the chain until end, looking for case-insensitive
   // matches which also match a namespace tag. Separate hash tables are
   // not used for each namespace, because the performance benefit is not
   // worth the overhead, considering namespace collisions are rare in
   // Doom wads.

   while(i >= 0 && (strncasecmp(dir->lumpinfo[i]->name, name, 8) ||
         dir->lumpinfo[i]->li_namespace != li_namespace))
      i = dir->lumpinfo[i]->next;

   // Return the matching lump, or -1 if none found.   
   return i;
}

//
// W_CheckNumForName
//
// haleyjd: Now a global directory convenience routine.
//
int W_CheckNumForName(register const char *name)
{
   return W_CheckNumForNameInDir(&w_GlobalDir, name, lumpinfo_t::ns_global);
}

//
// W_CheckNumForNameNS
//
// haleyjd: Separated from W_CheckNumForName. Looks in a specific namespace.
//
int W_CheckNumForNameNS(register const char *name, register int li_namespace)
{
   return W_CheckNumForNameInDir(&w_GlobalDir, name, li_namespace);
}

//
// W_CheckNumForNameNSG
//
// haleyjd 02/15/10: Looks in specified namespace and if not found, then looks
// in the global namespace.
//
int W_CheckNumForNameNSG(const char *name, int ns)
{
   int num = -1;
   int curnamespace = ns;

   do
   {
      num = W_CheckNumForNameInDir(&w_GlobalDir, name, curnamespace);
   }
   while(num < 0 && curnamespace == ns ? curnamespace = lumpinfo_t::ns_global, 1 : 0);

   return num;
}

//
// W_GetLumpNameChainInDir
//
// haleyjd 03/18/10: routine for getting the lumpinfo hash chain for lumps of a
// given name, to replace code segments doing this in several different places.
//
lumpinfo_t *W_GetLumpNameChainInDir(waddir_t *dir, const char *name)
{
   return dir->lumpinfo[W_LumpNameHash(name) % (unsigned int)dir->numlumps];
}

//
// W_GetLumpNameChain
//
// haleyjd 03/18/10: convenience routine to do the above on the global wad
// directory.
//
lumpinfo_t *W_GetLumpNameChain(const char *name)
{
   return W_GetLumpNameChainInDir(&w_GlobalDir, name);
}


//
// W_InitLumpHash
//
// killough 1/31/98: Initialize lump hash table
//
void W_InitLumpHash(waddir_t *dir)
{
   int i;
   
   for(i = 0; i < dir->numlumps; i++)
      dir->lumpinfo[i]->index = -1;                  // mark slots empty

   // Insert nodes to the beginning of each chain, in first-to-last
   // lump order, so that the last lump of a given name appears first
   // in any chain, observing pwad ordering rules. killough

   for(i = 0; i < dir->numlumps; i++)
   {                                           // hash function:
      unsigned int j;

      j = W_LumpNameHash(dir->lumpinfo[i]->name) % (unsigned int)dir->numlumps;
      dir->lumpinfo[i]->next = dir->lumpinfo[j]->index;     // Prepend to list
      dir->lumpinfo[j]->index = i;
   }
}

// End of lump hashing -- killough 1/31/98

//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName(const char* name)     // killough -- const added
{
   int i = W_CheckNumForName(name);
   if(i == -1)
      I_Error("W_GetNumForName: %.8s not found!\n", name); // killough .8 added
   return i;
}

//
// W_InitResources
//
static void W_InitResources(waddir_t *dir) // sf
{
   //jff 1/23/98
   // get all the sprites and flats into one marked block each
   // killough 1/24/98: change interface to use M_START/M_END explicitly
   // killough 4/17/98: Add namespace tags to each entry
   
   W_CoalesceMarkedResource(dir, "S_START", "S_END", lumpinfo_t::ns_sprites);
   
   W_CoalesceMarkedResource(dir, "F_START", "F_END", lumpinfo_t::ns_flats);

   // killough 4/4/98: add colormap markers
   W_CoalesceMarkedResource(dir, "C_START", "C_END", lumpinfo_t::ns_colormaps);
   
   // haleyjd 01/12/04: add translation markers
   W_CoalesceMarkedResource(dir, "T_START", "T_END", lumpinfo_t::ns_translations);

   // set up caching
   // sf: caching now done in the lumpinfo_t's
   
   // killough 1/31/98: initialize lump hash table
   W_InitLumpHash(dir);
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
void W_InitMultipleFiles(waddir_t *dir, wfileadd_t *files)
{
   wfileadd_t *curfile;
   dir->numlumps = 0;
   
   dir->lumpinfo = NULL;

   // haleyjd 09/13/03: set new sound lump parsing to deferred
   w_sound_update_type = 0;

   curfile = files;
   
   // open all the files, load headers, and count lumps
   while(curfile->filename)
   {
      // haleyjd 07/11/09: ignore empty filenames
      if((curfile->filename)[0])
      {
         // haleyjd 05/28/10: if there is a non-zero baseoffset, then this is a
         // subfile wad, or a wad contained within a larger file in other words.
         // Open them with a special routine; they can be treated uniformly with
         // other wad files afterward.
         if(curfile->baseoffset)
         {
            W_AddSubFile(dir, curfile->filename, curfile->li_namespace,
                         curfile->f, curfile->baseoffset);
         }
         else
            W_AddFile(dir, curfile->filename, curfile->li_namespace);
      }

      ++curfile;
   }
   
   if(!dir->numlumps)
      I_Error("W_InitMultipleFiles: no files found\n");
   
   W_InitResources(dir);
}

int W_AddNewFile(waddir_t *dir, const char *filename)
{
   // haleyjd 09/13/03: use S_UpdateSound here
   w_sound_update_type = 1;
   if(W_AddFile(dir, filename, lumpinfo_t::ns_global)) 
      return true;
   W_InitResources(dir);         // reinit lump lookups etc
   return false;
}

int W_AddNewPrivateFile(waddir_t *dir, const char *filename)
{
   if(!W_AddPrivateFile(dir, filename))
      return false;

   // there is no resource coalescence on this particular brand of private
   // wad file, so just call W_InitLumpHash.
   W_InitLumpHash(dir);

   return true;
}

//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
int W_LumpLengthInDir(waddir_t *dir, int lump)
{
   if(lump < 0 || lump >= dir->numlumps)
      I_Error("W_LumpLength: %i >= numlumps\n", lump);
   return dir->lumpinfo[lump]->size;
}

int W_LumpLength(int lump)
{
   return W_LumpLengthInDir(&w_GlobalDir, lump);
}

//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
void W_ReadLumpInDir(waddir_t *dir, int lump, void *dest)
{
   size_t c;
   lumpinfo_t *l;
   
   if(lump < 0 || lump >= dir->numlumps)
      I_Error("W_ReadLump: %d >= numlumps\n", lump);

   l = dir->lumpinfo[lump];

   // killough 1/31/98: Reload hack (-wart) removed

   c = LumpHandlers[l->type].readLump(l, dest, l->size);
   if(c < l->size)
   {
      I_Error("W_ReadLump: only read %d of %d on lump %d\n", 
              (int)c, (int)l->size, lump);
   }
}

void W_ReadLump(int lump, void *dest)
{
   W_ReadLumpInDir(&w_GlobalDir, lump, dest);
}

//
// W_ReadLumpHeader
//
// haleyjd 08/30/02: Inspired by DOOM Legacy, this function is for when you
// just need a small piece of data from the beginning of a lump. There's hardly
// any use reading in the whole thing in that case.
//
int W_ReadLumpHeaderInDir(waddir_t *dir, int lump, void *dest, size_t size)
{
   lumpinfo_t *l;
   void *data;
   
   if(lump < 0 || lump >= dir->numlumps)
      I_Error("W_ReadLumpHeader: %d >= numlumps\n", lump);

   l = dir->lumpinfo[lump];

   if(l->size < size || l->size == 0)
      return 0;

   data = W_CacheLumpNumInDir(dir, lump, PU_CACHE);

   memcpy(dest, data, size);
   
   return size;
}

int W_ReadLumpHeader(int lump, void *dest, size_t size)
{
   return W_ReadLumpHeaderInDir(&w_GlobalDir, lump, dest, size);
}

//
// W_CacheLumpNum
//
// killough 4/25/98: simplified
//
void *W_CacheLumpNumInDir(waddir_t *dir, int lump, int tag)
{
   // haleyjd 08/14/02: again, should not be RANGECHECK only
   if(lump < 0 || lump >= dir->numlumps)
      I_Error("W_CacheLumpNum: %i >= numlumps\n", lump);
   
   if(!(dir->lumpinfo[lump]->cache))      // read the lump in
   {
      W_ReadLumpInDir(dir, lump, Z_Calloc(
         1, W_LumpLengthInDir(dir, lump), tag, &(dir->lumpinfo[lump]->cache)
      ));
   }
   else
   {
      // haleyjd: do not lower cache level and cause static users to lose their 
      // data unexpectedly (ie, do not change PU_STATIC into PU_CACHE -- that 
      // must be done using Z_ChangeTag explicitly)
      
      int oldtag = Z_CheckTag(dir->lumpinfo[lump]->cache);

      if(tag < oldtag) 
         Z_ChangeTag(dir->lumpinfo[lump]->cache, tag);
   }
   
   return dir->lumpinfo[lump]->cache;
}

void *W_CacheLumpNum(int lump, int tag)
{
   return W_CacheLumpNumInDir(&w_GlobalDir, lump, tag);
}

// W_CacheLumpName macroized in w_wad.h -- killough

// Predefined lumps removed -- sf

//
// W_LumpCheckSum
//
// sf
//
int W_LumpCheckSum(int lumpnum)
{
   int   i, lumplength;
   char *lump;
   int   checksum = 0;
   
   lump = (char *)(W_CacheLumpNum(lumpnum, PU_CACHE));
   lumplength = W_LumpLength(lumpnum);
   
   for(i = 0; i < lumplength; i++)
      checksum += lump[i] * i;
   
   return checksum;
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
void W_FreeDirectoryLumps(waddir_t *waddir)
{
   int i;
   lumpinfo_t **li = waddir->lumpinfo;

   for(i = 0; i < waddir->numlumps; ++i)
   {
      if(li[i]->cache)
      {
         Z_Free(li[i]->cache);
         li[i]->cache = NULL;
      }
   }
}

//
// W_FreeDirectoryAllocs
//
// haleyjd 06/06/10
// Frees all lumpinfo_t's allocated for a wad directory.
//
void W_FreeDirectoryAllocs(waddir_t *dir)
{
   int i;

   if(!dir->infoptrs)
      return;

   // free each lumpinfo_t allocation
   for(i = 0; i < dir->numallocs; ++i)
      free(dir->infoptrs[i]);

   // free the allocation tracking table
   free(dir->infoptrs);
   dir->infoptrs = NULL;
   dir->numallocs = dir->numallocsa = 0;
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

static size_t W_DirectReadLump(lumpinfo_t *l, void *dest, size_t size)
{
   size_t ret;

   // killough 10/98: Add flashing disk indicator
   I_BeginRead();
   fseek(l->file, l->position, SEEK_SET);
   ret = fread(dest, 1, size, l->file);
   I_EndRead();

   return ret;
}

//
// Memory lumps -- lumps that are held in a static memory buffer
//

static size_t W_MemoryReadLump(lumpinfo_t *l, void *dest, size_t size)
{
   // killough 1/31/98: predefined lump data
   memcpy(dest, (byte *)(l->data) + l->position, size);

   return size;
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
