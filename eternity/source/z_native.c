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
//
//-----------------------------------------------------------------------------

#ifdef ZONE_NATIVE

#include "z_zone.h"
#include "doomstat.h"
#include "m_argv.h"

// Uncomment this to see real-time memory allocation statistics.
//#define INSTRUMENTED

// Uncomment this to use memory scrambling on allocations and frees.
// haleyjd 01/27/09: made independent of INSTRUMENTED
//#define ZONESCRAMBLE

// Uncomment this to exhaustively run memory checks
// while the game is running (this is EXTREMELY slow).
// haleyjd 01/27/09: made independent of INSTRUMENTED
//#define CHECKHEAP

// Uncomment this to perform id checks on zone blocks,
// to detect corrupted and illegally freed blocks
//#define ZONEIDCHECK

// Uncomment this to dump the heap to file on exit.
//#define DUMPONEXIT

// Uncomment this to log all memory operations to a file
//#define ZONEFILE

// Tunables

// size of block header
#define HEADER_SIZE 32

// Minimum chunk size at which blocks are allocated
#define CHUNK_SIZE 32

// signature for block header
#define ZONEID  0x931d4a11

// End Tunables

typedef struct memblock {

#ifdef ZONEIDCHECK
  unsigned id;
#endif

  struct memblock *next,*prev;
  size_t size;
  void **user;
  unsigned char tag;

#ifdef INSTRUMENTED
  const char *file;
  int line;
#endif

} memblock_t;

static memblock_t *blockbytag[PU_MAX];   // used for tracking vm blocks

#ifdef INSTRUMENTED

// statistics for evaluating performance
static size_t active_memory;
static size_t purgable_memory;

int printstats = 0;                    // killough 8/23/98

void Z_PrintStats(void)           // Print allocation statistics
{
   if(printstats)
   {
      unsigned long total_memory = active_memory +
                                   purgable_memory;
      double s = 100.0 / total_memory;

      doom_printf(
         "%-5lu\t%6.01f%%\tstatic\n"
         "%-5lu\t%6.01f%%\tpurgable\n"
         "%-5lu\t\ttotal\n",
         active_memory,
         active_memory*s,
         purgable_memory,
         purgable_memory*s,
         total_memory
         );
   }
}
#endif

// haleyjd 06/20/09: removed unused, crashy, and non-useful Z_DumpHistory

#ifdef ZONEFILE

// haleyjd 09/16/06: zone logging file

static FILE *zonelog;

static void Z_OpenLogFile(void)
{
   zonelog = fopen("zonelog.txt", "w");
}

static void Z_CloseLogFile(void)
{
   if(zonelog)
   {
      fputs("Closing zone log", zonelog);
      fclose(zonelog);
      zonelog = NULL;
   }
}

static void Z_LogPrintf(const char *msg, ...)
{
   if(zonelog)
   {
      va_list ap;
      va_start(ap, msg);
      vfprintf(zonelog, msg, ap);
      va_end(ap);

      // flush after every message
      fflush(zonelog);
   }
}

static void Z_LogPuts(const char *msg)
{
   if(zonelog)
      fputs(msg, zonelog);
}

#endif


static void Z_Close(void)
{
#ifdef ZONEFILE
   Z_CloseLogFile();
#endif
#ifdef DUMPONEXIT
   Z_PrintZoneHeap();
#endif
}

void Z_Init(void)
{   
   // haleyjd 01/20/04: changed to prboom version:
   if(!(HEADER_SIZE >= sizeof(memblock_t)))
      I_Error("Z_Init: Sanity check failed");
      
   atexit(Z_Close);            // exit handler
   
#ifdef INSTRUMENTED
   active_memory = purgable_memory = 0;
#endif

#ifdef ZONEFILE
   Z_OpenLogFile();
   Z_LogPrintf("Initialized zone heap (using native implementation)\n");
#endif
}

//
// Z_Malloc
//
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
void *(Z_Malloc)(size_t size, int tag, void **user, const char *file, int line)
{
   register memblock_t *block;  
   
#ifdef INSTRUMENTED
   size_t size_orig = size;   
#endif

#ifdef CHECKHEAP
   Z_CheckHeap();
#endif

#ifdef ZONEIDCHECK
   if(tag >= PU_PURGELEVEL && !user)
      I_Error("Z_Malloc: an owner is required for purgable blocks\n"
              "Source: %s:%d", file, line);
#endif

   if(!size)
      return user ? *user = NULL : NULL;          // malloc(0) returns NULL
   
   while(!(block = (malloc)(size + HEADER_SIZE)))
   {
      if(!blockbytag[PU_CACHE])
         I_Error ("Z_Malloc: Failure trying to allocate %lu bytes\n"
                  "Source: %s:%d", (unsigned long) size, file, line);
      Z_FreeTags(PU_CACHE, PU_CACHE);
   }
   
   block->size = size;
   
   if((block->next = blockbytag[tag]))
      block->next->prev = (memblock_t *) &block->next;
   blockbytag[tag] = block;
   block->prev = (memblock_t *) &blockbytag[tag];
         
#ifdef INSTRUMENTED
   if(tag >= PU_PURGELEVEL)
      purgable_memory += size_orig;
   else
      active_memory += size_orig;

   block->file = file;
   block->line = line;
#endif
         
#ifdef ZONEIDCHECK
   block->id = ZONEID;         // signature required in block header
#endif
   
   block->tag = tag;           // tag
   block->user = user;         // user
   block = (memblock_t *)((char *) block + HEADER_SIZE);
   if(user)                    // if there is a user
      *user = block;           // set user to point to new block

#ifdef INSTRUMENTED
   Z_PrintStats();           // print memory allocation stats
#endif
#ifdef ZONESCRAMBLE
   // scramble memory -- weed out any bugs
   memset(block, 1 | (gametic & 0xff), size);
#endif
#ifdef ZONEFILE
   Z_LogPrintf("* %p = Z_Malloc(size=%lu, tag=%d, user=%p, source=%s:%d)\n", 
               block, size, tag, user, file, line);
#endif

   return block;
}

void (Z_Free)(void *p, const char *file, int line)
{
#ifdef CHECKHEAP
   Z_CheckHeap();
#endif

   if(p)
   {
      memblock_t *block = (memblock_t *)((char *) p - HEADER_SIZE);

#ifdef ZONEIDCHECK
      if(block->id != ZONEID)
      {
         I_Error("Z_Free: freed a pointer without ZONEID\n"
                 "Source: %s:%d\n"
#ifdef INSTRUMENTED
                 "Source of malloc: %s:%d\n"
                 , file, line, block->file, block->line
#else
                 , file, line
#endif
                );
      }
      block->id = 0;              // Nullify id so another free fails
#endif

      // haleyjd 01/20/09: check invalid tags
      // catches double frees and possible selective heap corruption
      if(block->tag == PU_FREE || block->tag >= PU_MAX)
      {
         I_Error("Z_Free: freed a pointer with invalid tag %d\n"
                 "Source: %s:%d\n"
#ifdef INSTRUMENTED
                 "Source of malloc: %s:%d\n"
                 , block->tag, file, line, block->file, block->line
#else
                 , block->tag, file, line
#endif
                );
      }
      block->tag = PU_FREE;       // Mark block freed

#ifdef ZONESCRAMBLE
      // scramble memory -- weed out any bugs
      memset(p, 1 | (gametic & 0xff), block->size);
#endif

      if(block->user)            // Nullify user if one exists
         *block->user = NULL;

      if((*(memblock_t **) block->prev = block->next))
         block->next->prev = block->prev;

#ifdef INSTRUMENTED
      if(block->tag >= PU_PURGELEVEL)
         purgable_memory -= block->size;
      else
         active_memory -= block->size;
#endif
         
      (free)(block);
         
#ifdef INSTRUMENTED
      Z_PrintStats();           // print memory allocation stats
#endif
#ifdef ZONEFILE
      Z_LogPrintf("* Z_Free(p=%p, file=%s:%d)\n", p, file, line);
#endif
   }
}

void (Z_FreeTags)(int lowtag, int hightag, const char *file, int line)
{
   memblock_t *block;
   
   if(lowtag <= PU_FREE)
      lowtag = PU_FREE+1;

   if(hightag > PU_CACHE)
      hightag = PU_CACHE;
   
   for(; lowtag <= hightag; ++lowtag)
   {
      for(block = blockbytag[lowtag], blockbytag[lowtag] = NULL; block;)
      {
         memblock_t *next = block->next;

#ifdef ZONEIDCHECK
         if(block->id != ZONEID)
            I_Error("Z_FreeTags: Changed a tag without ZONEID\n"
                    "Source: %s:%d"

#ifdef INSTRUMENTED
                    "\nSource of malloc: %s:%d"
                    , file, line, block->file, block->line
#else
                    , file, line
#endif
                    );
#endif

         (Z_Free)((char *)block + HEADER_SIZE, file, line);
         block = next;               // Advance to next block
      }
   }

#ifdef ZONEFILE
   Z_LogPrintf("* Z_FreeTags(lowtag=%d, hightag=%d, file=%s:%d)\n",
               lowtag, hightag, file, line);
#endif
}

void (Z_ChangeTag)(void *ptr, int tag, const char *file, int line)
{
   memblock_t *block = (memblock_t *)((char *) ptr - HEADER_SIZE);
   
#ifdef CHECKHEAP
   Z_CheckHeap();
#endif

#ifdef ZONEIDCHECK
   if(block->id != ZONEID)
      I_Error("Z_ChangeTag: Changed a tag without ZONEID"
              "\nSource: %s:%d"

#ifdef INSTRUMENTED
              "\nSource of malloc: %s:%d"
              , file, line, block->file, block->line
#else
              , file, line
#endif
              );

   if(tag >= PU_PURGELEVEL && !block->user)
      I_Error("Z_ChangeTag: an owner is required for purgable blocks\n"
              "Source: %s:%d"
#ifdef INSTRUMENTED
              "\nSource of malloc: %s:%d"
              , file, line, block->file, block->line
#else
              , file, line
#endif
              );

#endif // ZONEIDCHECK

   if((*(memblock_t **) block->prev = block->next))
      block->next->prev = block->prev;
   if((block->next = blockbytag[tag]))
      block->next->prev = (memblock_t *) &block->next;
   block->prev = (memblock_t *) &blockbytag[tag];
   blockbytag[tag] = block;

#ifdef INSTRUMENTED
   if(block->tag < PU_PURGELEVEL && tag >= PU_PURGELEVEL)
   {
      active_memory -= block->size;
      purgable_memory += block->size;
   }
   else if(block->tag >= PU_PURGELEVEL && tag < PU_PURGELEVEL)
   {
      active_memory += block->size;
      purgable_memory -= block->size;
   }
#endif

   block->tag = tag;

#ifdef ZONEFILE
   Z_LogPrintf("* Z_ChangeTag(p=%p, tag=%d, file=%s:%d)\n",
               ptr, tag, file, line);
#endif
}

//
// Z_Realloc
//
// haleyjd 05/29/08: *Something* is wrong with my Z_Realloc routine, and I
// cannot figure out what! So we're back to using Old Faithful for now.
//
void *(Z_Realloc)(void *ptr, size_t n, int tag, void **user,
                  const char *file, int line)
{
   void *p = (Z_Malloc)(n, tag, user, file, line);
   if(ptr)
   {
      memblock_t *block = (memblock_t *)((char *)ptr - HEADER_SIZE);
      if(p) // haleyjd 09/18/06: allow to return NULL without crashing
         memcpy(p, ptr, n <= block->size ? n : block->size);
      (Z_Free)(ptr, file, line);
      if(user) // in case Z_Free nullified same user
         *user=p;
   }

#ifdef ZONEFILE
   Z_LogPrintf("* %p = Z_Realloc(ptr=%p, n=%lu, tag=%d, user=%p, source=%s:%d)\n", 
               p, ptr, n, tag, user, file, line);
#endif

   return p;
}

//
// Z_Calloc
//
void *(Z_Calloc)(size_t n1, size_t n2, int tag, void **user,
                 const char *file, int line)
{
   return (n1*=n2) ? memset((Z_Malloc)(n1, tag, user, file, line), 0, n1) : NULL;
}

//
// Z_Strdup
//
char *(Z_Strdup)(const char *s, int tag, void **user,
                 const char *file, int line)
{
   return strcpy((Z_Malloc)(strlen(s)+1, tag, user, file, line), s);
}

//
// haleyjd 12/06/06: Zone alloca functions
//

typedef struct alloca_header_s
{
   struct alloca_header_s *next;
} alloca_header_t;

static alloca_header_t *alloca_root;

//
// Z_FreeAlloca
//
// haleyjd 12/06/06: Frees all blocks allocated with Z_Alloca.
//
void Z_FreeAlloca(void)
{
   alloca_header_t *hdr = alloca_root, *next;

#ifdef ZONEFILE
   Z_LogPuts("* Freeing alloca blocks\n");
#endif

   while(hdr)
   {
      next = hdr->next;

      Z_Free(hdr);

      hdr = next;
   }

   alloca_root = NULL;
}

//
// Z_Alloca
//
// haleyjd 12/06/06:
// Implements a portable garbage-collected alloca on the zone heap.
//
void *(Z_Alloca)(size_t n, const char *file, int line)
{
   alloca_header_t *hdr;
   void *ptr;

   if(n == 0)
      return NULL;

   // add an alloca_header_t to the requested allocation size
   ptr = (Z_Calloc)(n + sizeof(alloca_header_t), 1, PU_STATIC, NULL, file, line);

#ifdef ZONEFILE
   Z_LogPrintf("* %p = Z_Alloca(n = %lu, file = %s, line = %d)\n", ptr, n, file, line);
#endif

   // add to linked list
   hdr = (alloca_header_t *)ptr;
   hdr->next = alloca_root;
   alloca_root = hdr;

   // return a pointer to the actual allocation
   return (void *)((char *)ptr + sizeof(alloca_header_t));
}

//
// Z_Strdupa
//
// haleyjd 05/07/08: strdup that uses alloca, for convenience.
//
char *(Z_Strdupa)(const char *s, const char *file, int line)
{      
   return strcpy((Z_Alloca)(strlen(s)+1, file, line), s);
}


void (Z_CheckHeap)(const char *file, int line)
{
   memblock_t *block;
   int lowtag;

   for(lowtag = PU_FREE; lowtag < PU_MAX; ++lowtag)
   {
      for(block = blockbytag[lowtag]; block; block = block->next)
      {
#ifdef ZONEIDCHECK
         if(block->id != ZONEID)
         {
            I_Error("Z_CheckHeap: Block found without ZONEID\n"
                    "Source: %s:%d"
#ifdef INSTRUMENTED
                    "\nSource of malloc: %s:%d"
                    , file, line, block->file, block->line
#else
                    , file, line
#endif
                    );
         }
#endif
      }
   }

#ifdef ZONEFILE
#ifndef CHECKHEAP
   Z_LogPrintf("* Z_CheckHeap(file=%s:%d)\n", file, line);
#endif
#endif
}

//
// Z_CheckTag
//
// haleyjd: a function to return the allocation tag of a block.
// This is needed by W_CacheLumpNum so that it does not
// inadvertently lower the cache level of lump allocations and
// cause code which expects them to be static to lose them
//
int (Z_CheckTag)(void *ptr, const char *file, int line)
{
   memblock_t *block = (memblock_t *)((char *) ptr - HEADER_SIZE);
   
#ifdef CHECKHEAP
   Z_CheckHeap();
#endif

#ifdef ZONEIDCHECK
   if(block->id != ZONEID)
   {
      I_Error("Z_CheckTag: block doesn't have ZONEID"
              "\nSource: %s:%d"

#ifdef INSTRUMENTED
              "\nSource of malloc: %s:%d"
              , file, line, block->file, block->line
#else
              , file, line
#endif
              );
   }
#endif // ZONEIDCHECK
   
   return block->tag;
}

void Z_PrintZoneHeap(void)
{
   memblock_t *block;
   int lowtag;
   FILE *outfile;

   const char *fmtstr =
#if defined(ZONEIDCHECK) && defined(INSTRUMENTED)
      "%p: { %8X : %p : %p : %8u : %p : %d : %s : %d }\n"
#elif defined(INSTRUMENTED)
      "%p: { %p : %p : %8u : %p : %d : %s : %d }\n"
#elif defined(ZONEIDCHECK)
      "%p: { %8X : %p : %p : %8u : %p : %d }\n"
#else
      "%p: { %p : %p : %8u : %p : %d }\n"
#endif
      ;

   outfile = fopen("heap.txt", "w");
   if(!outfile)
      return;

   for(lowtag = PU_FREE; lowtag < PU_MAX; ++lowtag)
   {
      for(block = blockbytag[lowtag]; block; block = block->next)
      {
         fprintf(outfile, fmtstr, block,
#if defined(ZONEIDCHECK)
                 block->id, 
#endif
                 block->next, block->prev, block->size,
                 block->user, block->tag
#if defined(INSTRUMENTED)
                 , block->file, block->line
#endif
                 );
         // warnings
#if defined(ZONEIDCHECK)
         if(block->tag != PU_FREE && block->id != ZONEID)
            fputs("\tWARNING: block does not have ZONEID\n", outfile);
#endif
         if(!block->user && block->tag >= PU_PURGELEVEL)
            fputs("\tWARNING: purgable block with no user\n", outfile);
         if(block->tag >= PU_MAX)
            fputs("\tWARNING: invalid cache level\n", outfile);
         if(block->next->prev != block || block->prev->next != block)
            fputs("\tWARNING: block pointer inconsistency\n", outfile);
         
         fflush(outfile);
      }
   }

   fclose(outfile);
}

//
// Z_DumpCore
//
// haleyjd 03/18/07: Write the zone heap to file
//
void Z_DumpCore(void)
{
   // FIXME
}

//
// Z_SysMalloc
//
// Similar to I_AllocLow in the original source, this function gives explicit
// access to the C heap. There are allocations which are a detriment to the zone
// system, such as large video buffers, which should be handled through this
// function instead.
//
// Care must be taken, of course, to not mix zone and C heap allocations.
//
void *Z_SysMalloc(size_t size)
{
   void *ret;
   
   if(!(ret = (malloc)(size)))
      I_Error("Z_SysMalloc: failed on allocation of %u bytes\n", size);

   return ret;
}

//
// Z_SysCalloc
//
// Convenience routine to match above.
//
void *Z_SysCalloc(size_t n1, size_t n2)
{
   void *ret;

   if(!(ret = (calloc)(n1, n2)))
      I_Error("Z_SysCalloc: failed on allocation of %u bytes\n", n1*n2);

   return ret;
}

//
// Z_SysRealloc
//
// Turns out I need this in the sound code to avoid possible multithreaded
// race conditions.
//
void *Z_SysRealloc(void *ptr, size_t size)
{
   void *ret;

   if(!(ret = (realloc)(ptr, size)))
      I_Error("Z_SysRealloc: failed on allocation of %u bytes\n", size);

   return ret;
}

//
// Z_SysFree
//
// For use with Z_SysAlloc.
//
void Z_SysFree(void *p)
{
   if(p)
      (free)(p);
}

#endif // ZONE_NATIVE

//-----------------------------------------------------------------------------
//
// $Log: z_zone.c,v $
// Revision 1.13  1998/05/12  06:11:55  killough
// Improve memory-related error messages
//
// Revision 1.12  1998/05/03  22:37:45  killough
// beautification
//
// Revision 1.11  1998/04/27  01:49:39  killough
// Add history of malloc/free and scrambler (INSTRUMENTED only)
//
// Revision 1.10  1998/03/28  18:10:33  killough
// Add memory scrambler for debugging
//
// Revision 1.9  1998/03/23  03:43:56  killough
// Make Z_CheckHeap() more diagnostic
//
// Revision 1.8  1998/03/02  11:40:02  killough
// Put #ifdef CHECKHEAP around slow heap checks (debug)
//
// Revision 1.7  1998/02/02  13:27:45  killough
// Additional debug info turned on with #defines
//
// Revision 1.6  1998/01/26  19:25:15  phares
// First rev with no ^Ms
//
// Revision 1.5  1998/01/26  07:15:43  phares
// Added rcsid
//
// Revision 1.4  1998/01/26  06:12:30  killough
// Fix memory usage problems and improve debug stat display
//
// Revision 1.3  1998/01/22  05:57:20  killough
// Allow use of virtual memory when physical memory runs out
//
// ???
//
//-----------------------------------------------------------------------------
