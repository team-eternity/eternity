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
//      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//      Remark: this was the only stuff that, according
//       to John Carmack, might have been useful for
//       Quake.
//
// Rewritten by Lee Killough, though, since it was not efficient enough.
//
//---------------------------------------------------------------------

#ifndef __Z_ZONE__
#define __Z_ZONE__

#include "d_keywds.h" // haleyjd 05/22/02

// Remove all definitions before including system definitions

#undef malloc
#undef free
#undef realloc
#undef calloc
#undef strdup

// Include system definitions so that prototypes become
// active before macro replacements below are in effect.

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef LINUX
// Linux needs strings.h too, for strcasecmp etc.
#include <strings.h>
#endif
#include <sys/stat.h>
#ifndef LINUX
// NON-Linux platforms need sys/types.h
#include <sys/types.h>
#endif
#include <time.h>

// haleyjd: inttypes.h - on Windows, this comes from the source\Win32 directory
#include <inttypes.h>

// haleyjd: C++ headers
#include <new>

// haleyjd: portable replacement function headers
#include "psnprntf.h"

// ZONE MEMORY

// PU - purge tags.
enum 
{
   PU_FREE,     // block is free
   PU_STATIC,   // block is static (remains until explicitly freed)

   // domain-specific allocation lifetimes

   PU_SOUND,    // currently unused
   PU_MUSIC,    // currently unused
   PU_RENDERER, // haleyjd 06/29/08: for data allocated via R_Init
   PU_AUTO,     // haleyjd 07/08/10: automatic allocation
   PU_LEVEL,    // allocation belongs to level (freed at next level load)
   PU_OBJECT,   // haleyjd 04/01/11: for ZoneObject

   // cache levels

   PU_CACHE,    // block is cached (may be implicitly freed at any time!)

   PU_MAX       // Must always be last -- killough
};

#define PU_PURGELEVEL PU_CACHE        /* First purgable tag's level */

void *(Z_Malloc)(size_t size, int tag, void **ptr, const char *, int);
void  (Z_Free)(void *ptr, const char *, int);
void  (Z_FreeTags)(int lowtag, int hightag, const char *, int);
void  (Z_ChangeTag)(void *ptr, int tag, const char *, int);
void   Z_Init(void);
void *(Z_Calloc)(size_t n, size_t n2, int tag, void **user, const char *, int);
void *(Z_Realloc)(void *p, size_t n, int tag, void **user, const char *, int);
char *(Z_Strdup)(const char *s, int tag, void **user, const char *, int);
void   Z_FreeAlloca(void);
void *(Z_Alloca)(size_t n, const char *file, int line);
void *(Z_Realloca)(void *ptr, size_t n, const char *file, int line);
char *(Z_Strdupa)(const char *s, const char *file, int line);
void  (Z_CheckHeap)(const char *,int);   // killough 3/22/98: add file/line info
int   (Z_CheckTag)(void *,const char *,int);

void *Z_SysMalloc(size_t size);
void *Z_SysCalloc(size_t n1, size_t n2);
void *Z_SysRealloc(void *ptr, size_t size);
void  Z_SysFree(void *p);

#define Z_Free(a)          (Z_Free)     (a,      __FILE__,__LINE__)
#define Z_FreeTags(a,b)    (Z_FreeTags) (a,b,    __FILE__,__LINE__)
#define Z_ChangeTag(a,b)   (Z_ChangeTag)(a,b,    __FILE__,__LINE__)
#define Z_Malloc(a,b,c)    (Z_Malloc)   (a,b,c,  __FILE__,__LINE__)
#define Z_Strdup(a,b,c)    (Z_Strdup)   (a,b,c,  __FILE__,__LINE__)
#define Z_Calloc(a,b,c,d)  (Z_Calloc)   (a,b,c,d,__FILE__,__LINE__)
#define Z_Realloc(a,b,c,d) (Z_Realloc)  (a,b,c,d,__FILE__,__LINE__)
#define Z_Alloca(a)        (Z_Alloca)   (a,      __FILE__,__LINE__)
#define Z_Realloca(a,b)    (Z_Realloca) (a,b,    __FILE__,__LINE__)
#define Z_Strdupa(a)       (Z_Strdupa)  (a,      __FILE__,__LINE__)
#define Z_CheckHeap()      (Z_CheckHeap)(        __FILE__,__LINE__)
#define Z_CheckTag(a)      (Z_CheckTag) (a,      __FILE__,__LINE__)

// haleyjd 10/29/06: undefine any existing macros by these names
#undef malloc
#undef free
#undef realloc
#undef calloc
#undef strdup

#define malloc(n)          (Z_Malloc) (n,    PU_STATIC,0,__FILE__,__LINE__)
#define free(p)            (Z_Free)   (p,                __FILE__,__LINE__)
#define realloc(p,n)       (Z_Realloc)(p,n,  PU_STATIC,0,__FILE__,__LINE__)
#define calloc(n1,n2)      (Z_Calloc) (n1,n2,PU_STATIC,0,__FILE__,__LINE__)
#define strdup(s)          (Z_Strdup) (s,    PU_STATIC,0,__FILE__,__LINE__)

template<typename T> inline T ecalloc_impl(size_t n1, size_t n2, const char *file, int line)
{
   return static_cast<T>((Z_Calloc)(n1, n2, PU_STATIC, 0, file, line));
}

#define ecalloc(type, n1, n2) ecalloc_impl<type>(n1, n2, __FILE__, __LINE__)

// Doom-style printf
void doom_printf(const char *, ...) __attribute__((format(printf,1,2)));

#ifdef INSTRUMENTED
extern size_t memorybytag[PU_MAX]; // haleyjd  04/01/11
extern int printstats;             // killough 08/23/98
#endif

void Z_PrintZoneHeap(void);

void Z_DumpCore(void);

//
// ZoneObject Class
//
// This class serves as a base class for C++ objects that want to support
// allocation on the zone heap. The actual zone memory is always stored at
// PU_OBJECT allocation level, and the tag semantics are handled inside this
// base class.
//
class ZoneObject
{
private:
   // static data
   static ZoneObject *objectbytag[PU_MAX];
   static void *newalloc;

   // instance data
   void *zonealloc;       // If non-null, the object is living on the zone heap
   int   zonetag;         // If zonealloc is valid, the object's zone tag
   ZoneObject  *zonenext; // Next object on tag chain
   ZoneObject **zoneprev; // Previous object on tag chain's next pointer

public:
   ZoneObject();
   virtual ~ZoneObject();
   void *operator new (size_t size);
   void  operator delete (void *p);
   void  ChangeTag(int tag);

   // zone memblock reflection
   size_t getZoneSize() const;
   const void *getBlockPtr() const { return zonealloc; }

   static void FreeTags(int lowtag, int hightag);
};

#endif

//----------------------------------------------------------------------------
//
// $Log: z_zone.h,v $
// Revision 1.7  1998/05/08  20:32:12  killough
// fix __attribute__ redefinition
//
// Revision 1.6  1998/05/03  22:38:11  killough
// Remove unnecessary #include
//
// Revision 1.5  1998/04/27  01:49:42  killough
// Add history of malloc/free and scrambler (INSTRUMENTED only)
//
// Revision 1.4  1998/03/23  03:43:54  killough
// Make Z_CheckHeap() more diagnostic
//
// Revision 1.3  1998/02/02  13:28:06  killough
// Add doom_printf
//
// Revision 1.2  1998/01/26  19:28:04  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
