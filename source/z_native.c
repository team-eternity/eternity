// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
// Native implementation of the zone API. This doesn't have a lot of advantage
// over the zone heap during normal play, but it shines when the game is
// under stress, whereas the zone heap chokes doing an O(N) search over the
// block list and wasting time dumping purgables, causing unnecessary disk IO.
//
// This code must be enabled by globally defining ZONE_NATIVE. When running 
// with this heap, there is no limitation to the amount of memory allocated
// except what the system will provide.
//
// Limitations:
// Instrumentation is limited to tracking of static and purgable memory.
// Heap check is limited to a zone ID check.
// Core dump function is not supported.
//
//-----------------------------------------------------------------------------

#ifdef ZONE_NATIVE

#include "z_zone.h"
#include "doomstat.h"
#include "m_argv.h"

//=============================================================================
//
// Macros
//

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

// Uncomment this to enable more verbose - but dangerous - error messages
// that could cause crashes
//#define ZONEVERBOSE

//=============================================================================
//
// Tunables
//

// haleyjd 03/08/10: dynamically calculated header size
static size_t header_size;

// signature for block header
#define ZONEID  0x931d4a11

// End Tunables

//=============================================================================
//
// Memblock Structure
// 

typedef struct memblock
{
#ifdef ZONEIDCHECK
  unsigned int id;
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

//=============================================================================
//
// Debug Macros
//
// haleyjd 11/18/09
// These help clean up the #ifdef hell.
//

// Instrumentation macros
#ifdef INSTRUMENTED
#define INSTRUMENT(a) a
#define INSTRUMENT_IF(opt, a, b) if((opt)) (a); else (b)
#else
#define INSTRUMENT(a)
#define INSTRUMENT_IF(opt, a, b)
#endif

// ID Check macros
#ifdef ZONEIDCHECK

#define IDCHECK(a) a
#define IDBOOL(a) (a)

//
// Z_IDCheckNB
// 
// Performs a fatal error condition check contingent on the definition
// of ZONEIDCHECK, in any context where a memblock pointer is not available.
//
static void Z_IDCheckNB(boolean err, const char *errmsg,
                        const char *file, int line)
{
   if(err)
      I_FatalError(I_ERR_KILL, "%s\nSource: %s:%d\n", errmsg, file, line);
}

//
// Z_IDCheck
//
// Performs a fatal error condition check contingent on the definition
// of ZONEIDCHECK, and accepts a memblock pointer for provision of additional
// malloc source information available when INSTRUMENTED is also defined.
//
static void Z_IDCheck(boolean err, const char *errmsg, 
                      memblock_t *block, const char *file, int line)
{
   if(err)
   {
      I_FatalError(I_ERR_KILL,
                   "%s\nSource: %s:%d\nSource of malloc: %s:%d",
                   errmsg, file, line,
#if defined(ZONEVERBOSE) && defined(INSTRUMENTED)
                   block->file, block->line
#else
                   "(not available)", 0
#endif
                  );
   }
}
#else

#define IDCHECK(a)
#define IDBOOL(a) false
#define Z_IDCheckNB(err, errmsg, file, line)
#define Z_IDCheck(err, errmsg, block, file, line)

#endif

// Heap checking macro
#ifdef CHECKHEAP
#define DEBUG_CHECKHEAP() Z_CheckHeap()
#else
#define DEBUG_CHECKHEAP()
#endif

// Zone scrambling macro
#ifdef ZONESCRAMBLE
#define SCRAMBLER(b, s) memset((b), 1 | (gametic & 0xff), (s))
#else
#define SCRAMBLER(b, s)
#endif

//=============================================================================
//
// Instrumentation Statistics
//

// statistics for evaluating performance
INSTRUMENT(static size_t active_memory);
INSTRUMENT(static size_t purgable_memory);
INSTRUMENT(int printstats = 0);            // killough 8/23/98

void Z_PrintStats(void)           // Print allocation statistics
{
#ifdef INSTRUMENTED
   if(printstats)
   {
      unsigned int total_memory = active_memory + purgable_memory;
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
#endif
}

// haleyjd 06/20/09: removed unused, crashy, and non-useful Z_DumpHistory

//=============================================================================
//
// Zone Log File
//
// haleyjd 09/16/06
//

#ifdef ZONEFILE
static FILE *zonelog;
#endif

static void Z_OpenLogFile(void)
{
#ifdef ZONEFILE
   zonelog = fopen("zonelog.txt", "w");
#endif
}

static void Z_CloseLogFile(void)
{
#ifdef ZONEFILE
   if(zonelog)
   {
      fputs("Closing zone log", zonelog);
      fclose(zonelog);
      zonelog = NULL;
   }
#endif
}

static void Z_LogPrintf(const char *msg, ...)
{
#ifdef ZONEFILE
   if(zonelog)
   {
      va_list ap;
      va_start(ap, msg);
      vfprintf(zonelog, msg, ap);
      va_end(ap);

      // flush after every message
      fflush(zonelog);
   }
#endif
}

static void Z_LogPuts(const char *msg)
{
#ifdef ZONEFILE
   if(zonelog)
      fputs(msg, zonelog);
#endif
}

//=============================================================================
//
// Initialization and Shutdown
//

static void Z_Close(void)
{
   Z_CloseLogFile();

#ifdef DUMPONEXIT
   Z_PrintZoneHeap();
#endif
}

void Z_Init(void)
{   
   // haleyjd 03/08/10: dynamically calculate block header size;
   // round sizeof(memblock_t) up to nearest 16-byte boundary. This should work
   // just about everywhere, and keeps the assumption of a 32-byte header on 
   // 32-bit. 64-bit will use a 64-byte header.
   header_size = (sizeof(memblock_t) + 15) & ~15;
      
   atexit(Z_Close);            // exit handler
   
   INSTRUMENT(active_memory = purgable_memory = 0);

   Z_OpenLogFile();
   Z_LogPrintf("Initialized zone heap (using native implementation)\n");
}

//=============================================================================
//
// Core Memory Management Routines
//

//
// Z_Malloc
//
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
void *(Z_Malloc)(size_t size, int tag, void **user, const char *file, int line)
{
   register memblock_t *block;  
   
   INSTRUMENT(size_t size_orig = size);   

   DEBUG_CHECKHEAP();

   Z_IDCheckNB(IDBOOL(tag >= PU_PURGELEVEL && !user),
               "Z_Malloc: an owner is required for purgable blocks", 
               file, line);

   if(!size)
      return user ? *user = NULL : NULL;          // malloc(0) returns NULL
   
   while(!(block = (malloc)(size + header_size)))
   {
      if(!blockbytag[PU_CACHE])
      {
         I_FatalError(I_ERR_KILL,
                      "Z_Malloc: Failure trying to allocate %u bytes\n"
                      "Source: %s:%d", (unsigned int)size, file, line);
      }
      Z_FreeTags(PU_CACHE, PU_CACHE);
   }
   
   block->size = size;
   
   if((block->next = blockbytag[tag]))
      block->next->prev = (memblock_t *) &block->next;
   blockbytag[tag] = block;
   block->prev = (memblock_t *) &blockbytag[tag];
           
   INSTRUMENT_IF(tag >= PU_PURGELEVEL, 
                 purgable_memory += size_orig,
                 active_memory += size_orig);
   INSTRUMENT(block->file = file);
   INSTRUMENT(block->line = line);
         
   IDCHECK(block->id = ZONEID); // signature required in block header
   
   block->tag = tag;           // tag
   block->user = user;         // user
   block = (memblock_t *)((byte *) block + header_size);
   if(user)                    // if there is a user
      *user = block;           // set user to point to new block

   Z_PrintStats();           // print memory allocation stats

   // scramble memory -- weed out any bugs
   SCRAMBLER(block, size);

   Z_LogPrintf("* %p = Z_Malloc(size=%lu, tag=%d, user=%p, source=%s:%d)\n", 
               block, size, tag, user, file, line);

   return block;
}

//
// Z_Free
//
void (Z_Free)(void *p, const char *file, int line)
{
   DEBUG_CHECKHEAP();

   if(p)
   {
      memblock_t *block = (memblock_t *)((byte *) p - header_size);

      Z_IDCheck(IDBOOL(block->id != ZONEID),
                "Z_Free: freed a pointer without ZONEID", block, file, line);
      
      IDCHECK(block->id = 0); // Nullify id so another free fails

      // haleyjd 01/20/09: check invalid tags
      // catches double frees and possible selective heap corruption
      if(block->tag == PU_FREE || block->tag >= PU_MAX)
      {
         I_FatalError(I_ERR_KILL,
                      "Z_Free: freed a pointer with invalid tag %d\n"
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

      // scramble memory -- weed out any bugs
      SCRAMBLER(p, block->size);

      if(block->user)            // Nullify user if one exists
         *block->user = NULL;

      if((*(memblock_t **) block->prev = block->next))
         block->next->prev = block->prev;

      INSTRUMENT_IF(block->tag >= PU_PURGELEVEL,
         purgable_memory -= block->size,
         active_memory -= block->size);
         
      (free)(block);
         
      Z_PrintStats();           // print memory allocation stats

      Z_LogPrintf("* Z_Free(p=%p, file=%s:%d)\n", p, file, line);
   }
}

//
// Z_FreeTags
//
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

         Z_IDCheck(IDBOOL(block->id != ZONEID),
                   "Z_FreeTags: Changed a tag without ZONEID", 
                   block, file, line);

         (Z_Free)((byte *)block + header_size, file, line);
         block = next;               // Advance to next block
      }
   }

   Z_LogPrintf("* Z_FreeTags(lowtag=%d, hightag=%d, file=%s:%d)\n",
               lowtag, hightag, file, line);
}

//
// Z_ChangeTag
//
void (Z_ChangeTag)(void *ptr, int tag, const char *file, int line)
{
   memblock_t *block = (memblock_t *)((byte *) ptr - header_size);
   
   DEBUG_CHECKHEAP();

   Z_IDCheck(IDBOOL(block->id != ZONEID),
             "Z_ChangeTag: Changed a tag without ZONEID", block, file, line);

   Z_IDCheck(IDBOOL(tag >= PU_PURGELEVEL && !block->user),
             "Z_ChangeTag: an owner is required for purgable blocks",
             block, file, line);

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

   Z_LogPrintf("* Z_ChangeTag(p=%p, tag=%d, file=%s:%d)\n",
               ptr, tag, file, line);
}

//
// Z_Realloc
//
// For the native heap, this can easily behave as a real realloc, and not
// just an ignorant copy-and-free.
//
void *(Z_Realloc)(void *ptr, size_t n, int tag, void **user,
                  const char *file, int line)
{
   void *p;
   memblock_t *block;

   // haleyjd: no reallocation under purgable tags
   if(tag >= PU_PURGELEVEL)
      I_FatalError(I_ERR_KILL, "Z_Realloc: Reallocated a purgable tag\n");

   // if not allocated at all, defer to Z_Malloc
   if(!ptr)
      return (Z_Malloc)(n, tag, user, file, line);

   // size == 0 is a special case that cannot be handled below
   if(n == 0)
   {
      (Z_Free)(ptr, file, line);
      return NULL;
   }

   DEBUG_CHECKHEAP();

   block = (memblock_t *)((byte *)ptr - header_size);

   Z_IDCheck(IDBOOL(block->id != ZONEID),
             "Z_Realloc: Reallocated a block without ZONEID\n", 
             block, file, line);

   // haleyjd: no reallocation of purgable blocks
   if(block->tag >= PU_PURGELEVEL)
      I_FatalError(I_ERR_KILL, "Z_Realloc: Reallocated a purgable block\n");

   // nullify current user, if any
   if(block->user)
      *(block->user) = NULL;

   // detach from list before reallocation
   if((*(memblock_t **) block->prev = block->next))
      block->next->prev = block->prev;

   block->next = NULL;
   block->prev = NULL;

   INSTRUMENT(active_memory -= block->size);

   while(!(block = (realloc)(block, n + header_size)))
   {
      if(!blockbytag[PU_CACHE])
      {
         I_FatalError(I_ERR_KILL, 
                      "Z_Realloc: Failure trying to allocate %u bytes\n"
                      "Source: %s:%d\n", (unsigned int)n, file, line);
      }
      Z_FreeTags(PU_CACHE, PU_CACHE);
   }

   block->size = n;
   block->tag  = tag;

   p = (byte *)block + header_size;

   // set new user, if any
   block->user = user;
   if(user)
      *user = p;

   // reattach to list at possibly new address, new tag
   if((block->next = blockbytag[tag]))
      block->next->prev = (memblock_t *) &block->next;
   blockbytag[tag] = block;
   block->prev = (memblock_t *) &blockbytag[tag];

   INSTRUMENT(active_memory += block->size);
   INSTRUMENT(block->file = file);
   INSTRUMENT(block->line = line);

   Z_PrintStats();           // print memory allocation stats

   Z_LogPrintf("* %p = Z_Realloc(ptr=%p, n=%lu, tag=%d, user=%p, source=%s:%d)\n", 
               p, ptr, n, tag, user, file, line);

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

//=============================================================================
//
// Zone Alloca
//
// haleyjd 12/06/06
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

   Z_LogPuts("* Freeing alloca blocks\n");

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

   Z_LogPrintf("* %p = Z_Alloca(n = %lu, file = %s, line = %d)\n", ptr, n, file, line);

   // add to linked list
   hdr = (alloca_header_t *)ptr;
   hdr->next = alloca_root;
   alloca_root = hdr;

   // return a pointer to the actual allocation
   return (void *)((byte *)ptr + sizeof(alloca_header_t));
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

//=============================================================================
//
// Heap Verification
//

//
// Z_CheckHeap
//
void (Z_CheckHeap)(const char *file, int line)
{
#ifdef ZONEIDCHECK
   memblock_t *block;
   int lowtag;

   for(lowtag = PU_FREE; lowtag < PU_MAX; ++lowtag)
   {
      for(block = blockbytag[lowtag]; block; block = block->next)
      {
         Z_IDCheck(IDBOOL(block->id != ZONEID),
                   "Z_CheckHeap: Block found without ZONEID", 
                   block, file, line);
      }
   }
#endif

#ifndef CHECKHEAP
   Z_LogPrintf("* Z_CheckHeap(file=%s:%d)\n", file, line);
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
   memblock_t *block = (memblock_t *)((byte *) ptr - header_size);

   DEBUG_CHECKHEAP();

   Z_IDCheck(IDBOOL(block->id != ZONEID),
             "Z_CheckTag: block doesn't have ZONEID", block, file, line);
   
   return block->tag;
}

//
// Z_PrintZoneHeap
//
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

//=============================================================================
//
// System Allocator Functions
//
// Guaranteed access to system malloc and free, regardless of the heap in use.
//
// Note: these are now redundant with the native heap in place.
// Eliminate if z_zone is permanently removed.
//

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
   {
      I_FatalError(I_ERR_KILL,
                   "Z_SysMalloc: failed on allocation of %u bytes\n", 
                   (unsigned int)size);
   }

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
   {
      I_FatalError(I_ERR_KILL,
                   "Z_SysCalloc: failed on allocation of %u bytes\n", 
                   (unsigned int)n1*n2);
   }

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
   {
      I_FatalError(I_ERR_KILL,
                   "Z_SysRealloc: failed on allocation of %u bytes\n", 
                   (unsigned int)size);
   }

   return ret;
}

//
// Z_SysFree
//
// For use with Z_SysMalloc.
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
