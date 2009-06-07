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
//      Zone Memory Allocation. Neat.
//
// Neat enough to be rewritten by Lee Killough...
//
// Must not have been real neat :)
//
// Made faster and more general, and added wrappers for all of Doom's
// memory allocation functions, including malloc() and similar functions.
// Added line and file numbers, in case of error. Added performance
// statistics and tunables.
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "m_argv.h"

#ifdef DJGPP
#include <dpmi.h>
#endif

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

// Alignment of zone memory (benefit may be negated by HEADER_SIZE, CHUNK_SIZE)
#define CACHE_ALIGN 32

// size of block header
#define HEADER_SIZE 32

// Minimum chunk size at which blocks are allocated
#define CHUNK_SIZE 32

// Minimum size a block must be to become part of a split
#define MIN_BLOCK_SPLIT (1024)

// How much RAM to leave aside for other libraries
#define LEAVE_ASIDE (128*1024)

// Minimum RAM machine is assumed to have
#define MIN_RAM (7*1024*1024)

// haleyjd 11/28/03: Minimum RAM we'd LIKE to have on non-DOS platforms
#define DESIRED_RAM (16*1024*1024)

// Amount to subtract when retrying failed attempts to allocate initial pool
#define RETRY_AMOUNT (256*1024)

// signature for block header
#define ZONEID  0x931d4a11

// Number of mallocs & frees kept in history buffer (must be a power of 2)
#define ZONE_HISTORY 4

// End Tunables

typedef struct memblock {

#ifdef ZONEIDCHECK
  unsigned id;
#endif

  struct memblock *next,*prev;
  size_t size;
  void **user;
  unsigned char tag,vm;

#ifdef INSTRUMENTED
  unsigned short extra;
  const char *file;
  int line;
#endif

} memblock_t;

static memblock_t *rover;                // roving pointer to memory blocks
static memblock_t *zone;                 // pointer to first block
static memblock_t *zonebase;             // pointer to entire zone memory
static size_t     zonebase_size;         // zone memory allocated size
static memblock_t *blockbytag[PU_MAX];   // used for tracking vm blocks

#ifdef INSTRUMENTED

// statistics for evaluating performance
static size_t free_memory;
static size_t active_memory;
static size_t purgable_memory;
static size_t inactive_memory;
static size_t virtual_memory;

int printstats = 0;                    // killough 8/23/98

void Z_PrintStats(void)           // Print allocation statistics
{

  if (printstats)
    {
      unsigned long total_memory = free_memory + active_memory +
	purgable_memory + inactive_memory +
	virtual_memory;
      double s = 100.0 / total_memory;

      doom_printf(
              "%-5lu\t%6.01f%%\tstatic\n"
	      "%-5lu\t%6.01f%%\tpurgable\n"
	      "%-5lu\t%6.01f%%\tfree\n"
	      "%-5lu\t%6.01f%%\tfragmentary\n"
	      "%-5lu\t%6.01f%%\tvirtual\n"
	      "%-5lu\t\ttotal\n",
	      active_memory,
	      active_memory*s,
	      purgable_memory,
	      purgable_memory*s,
	      free_memory,
	      free_memory*s,
	      inactive_memory,
	      inactive_memory*s,
	      virtual_memory,
	      virtual_memory*s,
	      total_memory
	      );
    }
}
#endif

#ifdef INSTRUMENTED

// killough 4/26/98: Add history information

enum {malloc_history, free_history, NUM_HISTORY_TYPES};

static const char *file_history[NUM_HISTORY_TYPES][ZONE_HISTORY];
static int line_history[NUM_HISTORY_TYPES][ZONE_HISTORY];
static int history_index[NUM_HISTORY_TYPES];
static const char *const desc[NUM_HISTORY_TYPES] = {"malloc()'s", "free()'s"};

void Z_DumpHistory(char *buf)
{
   int i,j;
   char s[1024];

   strcat(buf,"\n");
   for(i = 0; i < NUM_HISTORY_TYPES; i++)
   {
      psnprintf(s, sizeof(s), "\nLast several %s:\n\n", desc[i]);
      // FIXME: could overflow
      strcat(buf,s);
      for(j = 0; j < ZONE_HISTORY; j++)
      {
         int k = (history_index[i]-j-1) & (ZONE_HISTORY-1);
         if(file_history[i][k])
         {
            psnprintf(s, sizeof(s), "File: %s, Line: %d\n", 
                      file_history[i][k], line_history[i][k]);
            // FIXME: could overflow
            strcat(buf,s);
         }
      }
   }
}
#else

void Z_DumpHistory(char *buf)
{
}

#endif


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
   
   (free)(zonebase);
   zone = rover = zonebase = NULL;
}

void Z_Init(void)
{
   // haleyjd 05/31/02: implement -heapsize for other platforms
#ifdef DJGPP
   size_t size = _go32_dpmi_remaining_physical_memory();    // Get free RAM
#else
   int p, mb_size;
   size_t size = DESIRED_RAM; // haleyjd 11/28/03: use DESIRED_RAM here

   p = M_CheckParm("-heapsize");
   if(p && ++p < myargc)
   {
      mb_size = (size_t)(atoi(myargv[p]));
      size = mb_size * 1024 * 1024;
   }
#endif
   
   if(size < MIN_RAM)          // If less than MIN_RAM, assume MIN_RAM anyway
      size = MIN_RAM;
   
   size -= LEAVE_ASIDE;        // Leave aside some for other libraries

   // haleyjd 01/20/04: changed to prboom version:
   if(!(HEADER_SIZE >= sizeof(memblock_t) && MIN_RAM > LEAVE_ASIDE))
      I_Error("Z_Init: Sanity check failed");
   
   size = (size+CHUNK_SIZE-1) & ~(CHUNK_SIZE-1);  // round to chunk size
   
   // Allocate the memory
   
   while(!(zonebase = (malloc)(zonebase_size=size + HEADER_SIZE + CACHE_ALIGN)))
   {
      if(size < 
         (MIN_RAM-LEAVE_ASIDE < RETRY_AMOUNT ? RETRY_AMOUNT : MIN_RAM-LEAVE_ASIDE))
      {
         I_Error("Z_Init: failed on allocation of %lu bytes",
                 (unsigned long)zonebase_size);
      }
      else
      {
         size -= RETRY_AMOUNT;
      }
   }

   atexit(Z_Close);            // exit handler

   // Align on cache boundary
   
   zone = (memblock_t *) ((char *) zonebase + CACHE_ALIGN -
                          ((unsigned) zonebase & (CACHE_ALIGN-1)));

   rover = zone;                            // Rover points to base of zone mem
   zone->next = zone->prev = zone;          // Single node
   zone->size = size;                       // All memory in one block
   zone->tag = PU_FREE;                     // A free block
   zone->vm  = 0;

#ifdef ZONEIDCHECK
   zone->id  = 0;
#endif
   
#ifdef INSTRUMENTED
   free_memory = size;
   inactive_memory = zonebase_size - size;
   active_memory = purgable_memory = 0;
#endif

#ifdef ZONEFILE
   Z_OpenLogFile();
   Z_LogPrintf("Initialized zone heap with size of %lu bytes (zonebase = %p)\n",
               zonebase_size, zonebase);
#endif
}


// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.

void *(Z_Malloc)(size_t size, int tag, void **user, const char *file, int line)
{
   register memblock_t *block;  
   memblock_t *start;
   
#ifdef INSTRUMENTED
   size_t size_orig = size;
   
   file_history[malloc_history][history_index[malloc_history]] = file;
   line_history[malloc_history][history_index[malloc_history]++] = line;
   history_index[malloc_history] &= ZONE_HISTORY-1;
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

   size = (size+CHUNK_SIZE-1) & ~(CHUNK_SIZE-1);  // round to chunk size
   
   block = rover;
   
   if(block->prev->tag == PU_FREE)
      block = block->prev;
   
   start = block;
   
   // haleyjd 01/01/01 (happy new year!):
   // the first if() inside the loop below contains cph's memory
   // purging efficiency fix  

   do
   {
      // Free purgable blocks; replacement is roughly FIFO
      if(block->tag >= PU_PURGELEVEL)
      {
         start = block->prev;
         Z_Free((char *) block + HEADER_SIZE);

         // cph - If start->next == block, we did not merge with the previous
         //       If !=, we did, so we continue from start.
         //  Important: we've reset start!
         if(start->next == block)
            start = start->next;
         else
            block = start;
      }

      if(block->tag == PU_FREE && block->size >= size)   // First-fit
      {
         size_t extra = block->size - size;
         if(extra >= MIN_BLOCK_SPLIT + HEADER_SIZE)
         {
            memblock_t *newb = 
               (memblock_t *)((char *) block + HEADER_SIZE + size);

            (newb->next = block->next)->prev = newb;
            (newb->prev = block)->next = newb;          // Split up block
            block->size = size;
            newb->size = extra - HEADER_SIZE;
            newb->tag = PU_FREE;
            newb->vm = 0;
            
#ifdef INSTRUMENTED
            inactive_memory += HEADER_SIZE;
            free_memory -= HEADER_SIZE;
#endif
         }

         rover = block->next;           // set roving pointer for next search
         
#ifdef INSTRUMENTED
         inactive_memory += block->extra = block->size - size_orig;
         if(tag >= PU_PURGELEVEL)
            purgable_memory += size_orig;
         else
            active_memory += size_orig;
         free_memory -= block->size;
#endif

allocated:
         
#ifdef INSTRUMENTED
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
         memset(block, gametic & 0xff, size);
#endif
#ifdef ZONEFILE
         Z_LogPrintf("* %p = Z_Malloc(size=%lu, tag=%d, user=%p, source=%s:%d)\n", 
                     block, size, tag, user, file, line);
#endif
         return block;
      }
   }
   while((block = block->next) != start);   // detect cycles as failure

   // We've run out of physical memory, or so we think.
   // Although less efficient, we'll just use ordinary malloc.
   // This will squeeze the remaining juice out of this machine
   // and start cutting into virtual memory if it has it.
   
   while(!(block = (malloc)(size + HEADER_SIZE)))
   {
      if(!blockbytag[PU_CACHE])
         I_Error ("Z_Malloc: Failure trying to allocate %lu bytes\n"
                  "Source: %s:%d", (unsigned long) size, file, line);
      Z_FreeTags(PU_CACHE, PU_CACHE);
   }
   
   if((block->next = blockbytag[tag]))
      block->next->prev = (memblock_t *) &block->next;
   blockbytag[tag] = block;
   block->prev = (memblock_t *) &blockbytag[tag];
   block->vm = 1;

   // haleyjd: cph's virtual memory error fix
#ifdef INSTRUMENTED
   virtual_memory += size + HEADER_SIZE;

   // haleyjd 10/03/06: Big problem: extra wasn't being initialized for vm
   // blocks. This caused the memset used to randomize freed memory when 
   // INSTRUMENTED is defined to stomp all over the C heap.
   block->extra = 0;
#endif
   // cph - the next line was lost in the #ifdef above, and also added an
   // extra HEADER_SIZE to block->size, which was incorrect
   block->size = size;
   goto allocated;
}

void (Z_Free)(void *p, const char *file, int line)
{
#ifdef CHECKHEAP
   Z_CheckHeap();
#endif
#ifdef INSTRUMENTED
   file_history[free_history][history_index[free_history]] = file;
   line_history[free_history][history_index[free_history]++] = line;
   history_index[free_history] &= ZONE_HISTORY-1;
#endif

   if(p)
   {
      memblock_t *other, *block = (memblock_t *)((char *) p - HEADER_SIZE);

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

#ifdef ZONESCRAMBLE
      // scramble memory -- weed out any bugs
      memset(p, gametic & 0xff, block->size);
#endif

      if(block->user)            // Nullify user if one exists
         *block->user = NULL;

      if(block->vm)
      {
         if((*(memblock_t **) block->prev = block->next))
            block->next->prev = block->prev;
         
#ifdef INSTRUMENTED
         virtual_memory -= block->size;
#endif
         (free)(block);
      }
      else
      {

#ifdef INSTRUMENTED
         free_memory += block->size;
         inactive_memory -= block->extra;
         if(block->tag >= PU_PURGELEVEL)
            purgable_memory -= block->size - block->extra;
         else
            active_memory -= block->size - block->extra;
#endif

         block->tag = PU_FREE;       // Mark block freed
         
         if(block != zone)
         {
            other = block->prev;        // Possibly merge with previous block
            if(other->tag == PU_FREE)
            {
               if(rover == block)  // Move back rover if it points at block
                  rover = other;
               (other->next = block->next)->prev = other;
               other->size += block->size + HEADER_SIZE;
               block = other;

#ifdef INSTRUMENTED
               inactive_memory -= HEADER_SIZE;
               free_memory += HEADER_SIZE;
#endif
            }
         }

         other = block->next;        // Possibly merge with next block
         if(other->tag == PU_FREE && other != zone)
         {
            if(rover == other) // Move back rover if it points at next block
               rover = block;
            (block->next = other->next)->prev = block;
            block->size += other->size + HEADER_SIZE;
            
#ifdef INSTRUMENTED
            inactive_memory -= HEADER_SIZE;
            free_memory += HEADER_SIZE;
#endif
         }
      }

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
   memblock_t *block = zone;
   
   if(lowtag <= PU_FREE)
      lowtag = PU_FREE+1;

   // haleyjd: code inside this do loop has been updated with
   //          cph's fix for memory wastage
   
   do               // Scan through list, searching for tags in range
   {
      if(block->tag >= lowtag && block->tag <= hightag)
      {
         memblock_t *prev = block->prev, *cur = block;
         (Z_Free)((char *) block + HEADER_SIZE, file, line);
         /* cph - be more careful here, we were skipping blocks!
          * If the current block was not merged with the previous, 
          *  cur is still a valid pointer, prev->next == cur, and cur is 
          *  already free so skip to the next.
          * If the current block was merged with the previous, 
          *  the next block to analyse is prev->next.
          * Note that the while() below does the actual step forward
          */
         block = (prev->next == cur) ? cur : prev;
      }
   }
   while((block = block->next) != zone);

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

         block->id = 0;              // Nullify id so another free fails
#endif

#ifdef INSTRUMENTED
         virtual_memory -= block->size;
#endif

         if(block->user)            // Nullify user if one exists
            *block->user = NULL;

         (free)(block);              // Free the block

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

   if(block->vm)
   {
      if((*(memblock_t **) block->prev = block->next))
         block->next->prev = block->prev;
      if((block->next = blockbytag[tag]))
         block->next->prev = (memblock_t *) &block->next;
      block->prev = (memblock_t *) &blockbytag[tag];
      blockbytag[tag] = block;
   }
   else
   {
#ifdef INSTRUMENTED
      if(block->tag < PU_PURGELEVEL && tag >= PU_PURGELEVEL)
      {
         active_memory -= block->size - block->extra;
         purgable_memory += block->size - block->extra;
      }
      else if(block->tag >= PU_PURGELEVEL && tag < PU_PURGELEVEL)
      {
         active_memory += block->size - block->extra;
         purgable_memory -= block->size - block->extra;
      }
#endif
   }
   block->tag = tag;

#ifdef ZONEFILE
   Z_LogPrintf("* Z_ChangeTag(p=%p, tag=%d, file=%s:%d)\n",
               ptr, tag, file, line);
#endif
}

/*
//
// Z_ReallocOld
//
// haleyjd 09/18/06: This is the original Z_Realloc routine. It is still called
// from the new one below for several cases where this is the appropriate action
// to be taken.
//
static void *Z_ReallocOld(void *ptr, size_t n, int tag, void **user,
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
   return p;
}

//
// Z_Realloc
//
// haleyjd 09/18/06: Rewritten to be an actual realloc routine. The 
// various cases are as follows:
//
// 1. If the block is NULL, is in virtual memory, or we're trying to set it to
//    zero-byte size, we use Z_ReallocOld above.
// 2. If the block is smaller than the new size, we need to expand it. If the
//    next block on the zone heap is free, check to see if it together with the
//    current block is large enough. If so, merge the blocks. Now test to make
//    sure the internal fragmentation does not exceed the split limit. If it
//    does, resplit the blocks at the new boundary. If the next block wasn't
//    free, we have to call Z_ReallocOld to move the entire block elsewhere.
// 3. If the block is larger than the new size, we can shrink it, but we only
//    need to shrink it if the wasted space is larger than the split limit.
//    If so, the block is split at its new boundary. If the next block on the
//    zone heap is free, it is then necessary to merge the new free block with
//    the next block on the heap. In the event the block is not shrunk, only the
//    INSTRUMENTED data needs to be updated to reflect the new internal fragmen-
//    tation.
// 4. If the block is already the same size as "n", we don't need to do anything
//    aside from adjusting the INSTRUMENTED block data for debugging purposes.
//
void *(Z_Realloc)(void *ptr, size_t n, int tag, void **user,
                  const char *file, int line)
{
   memblock_t *block, *other = NULL;
   size_t curr_size = 0;

   // is ptr null or requested size 0? if so, go to "old" realloc
   if(ptr == NULL || n == 0)
      return Z_ReallocOld(ptr, n, tag, user, file, line);

   // get current size of block
   block = (memblock_t *)((char *)ptr - HEADER_SIZE);

#ifdef ZONEIDCHECK
   if(block->id != ZONEID)
      I_Error("Z_Realloc: Tried to realloc a block without ZONEID"
              "\nSource: %s:%d"
      
#ifdef INSTRUMENTED
              "\nSource of malloc: %s:%d"
              , file, line, block->file, block->line
#else
              , file, line
#endif
      );
#endif

   if(block->vm) // vm block? go to old realloc (can't mess with it)
      return Z_ReallocOld(ptr, n, tag, user, file, line);

   other = block->next; // save pointer to next block
   curr_size = block->size;
   
   // round new size to CHUNK_SIZE
   n = (n + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);
   
   if(n > curr_size) // is new allocation size larger than current?
   {
      size_t extra;

      // haleyjd 10/03/06: free adjacent purgable blocks
      while(other != zone && other != block &&
            (other->tag == PU_FREE || 
             (other->tag >= PU_PURGELEVEL && !other->vm)))
      {
         if(other->tag >= PU_PURGELEVEL)
         {
            (Z_Free)((char *)other + HEADER_SIZE, file, line);
            
            // reset pointer to next block
            other = block->next;
         }

         // use current size of block; note it may have increased if it was
         // merged with an adjacent free block

         // if we've freed enough, stop
         if(curr_size + other->size + HEADER_SIZE >= n)
            break;

         // move to next block
         other = other->next;
      }

      // reset pointer
      other = block->next;

      // check to see if it can fit if we merge with the next block
      if(other != zone && other->tag == PU_FREE &&
         curr_size + other->size + HEADER_SIZE >= n)
      {
         // merge the blocks
         if(rover == other)
            rover = block;
         (block->next = other->next)->prev = block;
         block->size += other->size + HEADER_SIZE;
            
#ifdef INSTRUMENTED
         // lost a block...
         inactive_memory -= HEADER_SIZE;
         // lost a free block...
         free_memory -= other->size;
         // increased active or purgable
         if(block->tag >= PU_PURGELEVEL)
            purgable_memory += other->size + HEADER_SIZE;
         else
            active_memory += other->size + HEADER_SIZE;
#endif

         // check to see if there's enough extra to warrant splitting off
         // a new free block
         extra = block->size - n;

         if(extra >= MIN_BLOCK_SPLIT + HEADER_SIZE)
         {
            memblock_t *newb =
               (memblock_t *)((char *)block + HEADER_SIZE + n);

            (newb->next = block->next)->prev = newb;
            (newb->prev = block)->next = newb;
            block->size = n;
            newb->size = extra - HEADER_SIZE;
            newb->tag = PU_FREE;
            newb->vm = 0;

            if(rover == block)
               rover = newb;

#ifdef INSTRUMENTED
            // added a block...
            inactive_memory += HEADER_SIZE;
            // added a free block...
            free_memory += newb->size;
            // decreased active or purgable
            if(block->tag >= PU_PURGELEVEL)
               purgable_memory -= newb->size + HEADER_SIZE;
            else
               active_memory -= newb->size + HEADER_SIZE;
#endif
         }

#ifdef INSTRUMENTED
         // subtract old internal fragmentation and add new
         inactive_memory -= block->extra;
         inactive_memory += (block->extra = block->size - n);
#endif
      }
      else // else, do old realloc (make new, copy old, free old)
         return Z_ReallocOld(ptr, n, tag, user, file, line);
   }
   else if(n < curr_size) // is new allocation size smaller than current?
   {
      // check to see if there's enough extra to warrant splitting off
      // a new free block
      size_t extra = curr_size - n;

      if(extra >= MIN_BLOCK_SPLIT + HEADER_SIZE)
      {
         memblock_t *newb = 
            (memblock_t *)((char *)block + HEADER_SIZE + n);
         
         (newb->next = block->next)->prev = newb;
         (newb->prev = block)->next = newb;
         block->size = n;
         newb->size = extra - HEADER_SIZE;
         newb->tag = PU_FREE;
         newb->vm = 0;
         
#ifdef INSTRUMENTED
         // added a block...
         inactive_memory += HEADER_SIZE;
         // added a free block...
         free_memory += newb->size;
         // decreased purgable or active
         if(block->tag >= PU_PURGELEVEL)
            purgable_memory -= newb->size + HEADER_SIZE;
         else
            active_memory -= newb->size + HEADER_SIZE;
#endif

         // may need to merge new block with next block
         if(other && other->tag == PU_FREE && other != zone)
         {
            if(rover == other) // Move back rover if it points at next block
               rover = newb;
            (newb->next = other->next)->prev = newb;
            newb->size += other->size + HEADER_SIZE;
            
#ifdef INSTRUMENTED
            // deleted a block...
            inactive_memory -= HEADER_SIZE;
            // space between blocks is now free
            free_memory += HEADER_SIZE;
#endif
         }
      }
      // else, leave block the same size

#ifdef INSTRUMENTED
      // subtract old internal fragmentation and add new
      inactive_memory -= block->extra;
      inactive_memory += (block->extra = block->size - n);
#endif
   }
   // else new allocation size is same as current, don't change it

   // modify the block
#ifdef INSTRUMENTED
   block->file = file;
   block->line = line;
#endif

   // reset ptr for consistency
   ptr = (void *)((char *)block + HEADER_SIZE);

   if(block->user != user)
   {
      if(block->user)           // nullify old user if any
         *(block->user) = NULL;
      block->user = user;       // set block's new user
      if(user)                  // if non-null, set user to allocation 
         *user = ptr;
   }
   
   // let Z_ChangeTag handle changing the tag
   if(block->tag != tag)
      (Z_ChangeTag)(ptr, tag, file, line);
   
#ifdef INSTRUMENTED
   Z_PrintStats();           // print memory allocation stats
#endif
#ifdef ZONEFILE
   Z_LogPrintf("* Z_Realloc(ptr=%p, n=%lu, tag=%d, user=%p, source=%s:%d)\n", 
               ptr, n, tag, user, file, line);
#endif

   return ptr;
}
*/

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
   memblock_t *block = zone;   // Start at base of zone mem
   do                          // Consistency check (last node treated special)
   {
      if((block->next != zone &&
          (memblock_t *)((char *) block+HEADER_SIZE+block->size) != block->next) ||
         block->next->prev != block || block->prev->next != block)
      {
         I_Error("Z_CheckHeap: Block size does not touch the next block\n"
                 "Source: %s:%d"
#ifdef INSTRUMENTED
                 "\nSource of offending block: %s:%d"
                 , file, line, block->file, block->line
#else
                 , file, line
#endif
                );
      }
   }
   while((block = block->next) != zone);

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
   FILE *outfile;
   memblock_t *block = zone;

   const char *fmtstr =
#if defined(ZONEIDCHECK) && defined(INSTRUMENTED)
      "%p: { %8X : %p : %p : %8u : %p : %d : %d : %s : %d }\n"
#elif defined(INSTRUMENTED)
      "%p: { %p : %p : %8u : %p : %d : %d : %s : %d }\n"
#elif defined(ZONEIDCHECK)
      "%p: { %8X : %p : %p : %8u : %p : %d : %d }\n"
#else
      "%p: { %p : %p : %8u : %p : %d : %d }\n"
#endif
      ;

   const char *freestr =
#if defined(ZONEIDCHECK)
      "%p: { %8X : %p : %p : %8u }\n"
#else
      "%p: { %p : %p : %8u }\n"
#endif
      ;

   outfile = fopen("heap.txt", "w");
   if(!outfile)
      return;

   do
   {
      if(block->tag != PU_FREE)
      {
         fprintf(outfile, fmtstr, block,
#if defined(ZONEIDCHECK)
                 block->id, 
#endif
                 block->next, block->prev, block->size,
                 block->user, block->tag, block->vm 
#if defined(INSTRUMENTED)
                 , block->file, block->line
#endif
                 );
      }
      else
      {
         fprintf(outfile, freestr, block,
#if defined(ZONEIDCHECK)
                 block->id, 
#endif
                 block->next, block->prev, block->size);
      }

      // warnings
#if defined(ZONEIDCHECK)
      if(block->tag != PU_FREE && block->id != ZONEID)
         fputs("\tWARNING: block does not have ZONEID\n", outfile);
#endif
      if(!block->user && block->tag >= PU_PURGELEVEL)
         fputs("\tWARNING: purgable block with no user\n", outfile);
      if(block->tag >= PU_MAX)
         fputs("\tWARNING: invalid cache level\n", outfile);
      if(!block->vm && block->next != zone &&
         (memblock_t *)((char *)block + HEADER_SIZE + block->size) != block->next)
         fputs("\tWARNING: block size doesn't touch next block\n", outfile);
      if(block->next->prev != block || block->prev->next != block)
         fputs("\tWARNING: block pointer inconsistency\n", outfile);

      fflush(outfile);
   }
   while((block = block->next) != zone);

   fclose(outfile);
}

//
// Z_DumpCore
//
// haleyjd 03/18/07: Write the zone heap to file
//
void Z_DumpCore(void)
{
   FILE *outfile;

   if(!(outfile = fopen("zone.bin", "wb")))
      return;

   fwrite(zonebase, 1, zonebase_size, outfile);
   fclose(outfile);
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
