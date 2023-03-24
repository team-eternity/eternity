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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//      Remark: this was the only stuff that, according
//       to John Carmack, might have been useful for
//       Quake.
//
// Rewritten by Lee Killough, though, since it was not efficient enough.
//
//-----------------------------------------------------------------------------

#ifndef Z_ZONE_H__
#define Z_ZONE_H__

// haleyjd 05/22/02: keyword defines needed globally
#include "d_keywds.h" 

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#include <errno.h>
#include <float.h>
#include <inttypes.h>
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

// haleyjd: C++ headers
#include <new>
#include <type_traits>

// haleyjd: portable replacement function headers
#include "m_ctype.h"
#include "psnprntf.h"

// ioanch: added debug headers, enabled only in debug builds
#include "m_debug.h"

// ZONE MEMORY

// PU - purge tags.
enum 
{
   PU_FREE,      // block is free
   PU_STATIC,    // block is static (remains until explicitly freed)
   PU_PERMANENT, // haleyjd 06/09/12: block cannot be freed or re-tagged

   // domain-specific allocation lifetimes

   PU_SOUND,     // currently unused
   PU_MUSIC,     // currently unused
   PU_RENDERER,  // haleyjd 06/29/08: for data allocated via R_Init
   PU_VALLOC,    // haleyjd 04/29/13: belongs to a video/rendering buffer
   PU_AUTO,      // haleyjd 07/08/10: automatic allocation
   PU_LEVEL,     // allocation belongs to level (freed at next level load)

   // cache levels

   PU_CACHE,    // block is cached (may be implicitly freed at any time!)

   PU_MAX       // Must always be last -- killough
};

#define PU_PURGELEVEL PU_CACHE        /* First purgable tag's level */

// killough 3/22/98: add file/line info

void Z_Init();

void *Z_SysMalloc(size_t size);
void *Z_SysCalloc(size_t n1, size_t n2);
void *Z_SysRealloc(void *ptr, size_t size);
void  Z_SysFree(void *p);

#define Z_Free(a)          z_globalheap.free       (a,      __FILE__,__LINE__)
#define Z_FreeTags(a,b)    z_globalheap.freeTags   (a,b,    __FILE__,__LINE__)
#define Z_ChangeTag(a,b)   z_globalheap.changeTag  (a,b,    __FILE__,__LINE__)
#define Z_Malloc(a,b,c)    z_globalheap.malloc     (a,b,c,  __FILE__,__LINE__)
#define Z_Strdup(a,b,c)    z_globalheap.strdup     (a,b,c,  __FILE__,__LINE__)
#define Z_Calloc(a,b,c,d)  z_globalheap.calloc     (a,b,c,d,__FILE__,__LINE__)
#define Z_Realloc(a,b,c,d) z_globalheap.realloc    (a,b,c,d,__FILE__,__LINE__)
#define Z_FreeAlloca       z_globalheap.freeAllocAuto
#define Z_Alloca(a)        z_globalheap.allocAuto  (a,      __FILE__,__LINE__)
#define Z_Realloca(a,b)    z_globalheap.reallocAuto(a,b,    __FILE__,__LINE__)
#define Z_Strdupa(a)       z_globalheap.strdupAuto (a,      __FILE__,__LINE__)
#define Z_CheckHeap()      z_globalheap.checkHeap  (        __FILE__,__LINE__)
#define Z_CheckTag(a)      z_globalheap.checkTag   (a,      __FILE__,__LINE__)

#define emalloc(type, n) \
   static_cast<type>(z_globalheap.malloc(n, PU_STATIC, 0, __FILE__, __LINE__))

#define emalloctag(type, n1, tag, user) \
   static_cast<type>(z_globalheap.malloc(n1, tag, user, __FILE__, __LINE__))

#define ecalloc(type, n1, n2) \
   static_cast<type>(z_globalheap.calloc(n1, n2, PU_STATIC, 0, __FILE__, __LINE__))

#define ecalloctag(type, n1, n2, tag, user) \
   static_cast<type>(z_globalheap.calloc(n1, n2, tag, user, __FILE__, __LINE__))

#define erealloc(type, p, n) \
   static_cast<type>(z_globalheap.realloc(p, n, PU_STATIC, 0, __FILE__, __LINE__))

#define erealloctag(type, p, n, tag, user) \
   static_cast<type>(z_globalheap.realloc(p, n, tag, user, __FILE__, __LINE__))

#define estructalloc(type, n) \
   static_cast<type *>(z_globalheap.calloc(n, sizeof(type), PU_STATIC, 0, __FILE__, __LINE__))

#define estructalloctag(type, n, tag) \
   static_cast<type *>(z_globalheap.calloc(n, sizeof(type), tag, 0, __FILE__, __LINE__))

#define estrdup(s) z_globalheap.strdup(s, PU_STATIC, 0, __FILE__, __LINE__)

#define efree(p)   z_globalheap.free(p, __FILE__, __LINE__)

//
// Globally useful macros
//

// Get the size of a static array
#define earrlen(a) (sizeof(a) / sizeof(*a))

// Define a struct var and ensure it is fully initialized
#define edefstructvar(type, name)  \
   type name;                      \
   memset(&name, 0, sizeof(name))

// Classify a string as either lengthful (non-nullptr, not zero length), or empty
#define estrnonempty(str) ((str) && *(str))
#define estrempty(str)    (!estrnonempty((str)))

// Difference between two pointers, to avoid spelling static_cast<int> every time
#define eindex static_cast<int>

// Doom-style printf
void doom_printf(E_FORMAT_STRING(const char *), ...) E_PRINTF(1, 2);
void doom_warningf(E_FORMAT_STRING(const char *), ...) E_PRINTF(1, 2);

#ifdef INSTRUMENTED
extern size_t memorybytag[PU_MAX]; // haleyjd  04/01/11
extern int printstats;             // killough 08/23/98
#endif

void Z_PrintZoneHeap();

void Z_DumpCore();

struct ZoneHeapPimpl;

//
// This class represents an instance of the zone heap. It might be
// single or multiple-consumer. Previously much of this was statics
// in z_native.cpp but thread-local tagged memory demands that what
// was global state is now per-instance.
//
class ZoneHeapBase
{
protected:
   struct memblock_t* m_blockbytag[PU_MAX]; // used for tracking all zone blocks

   friend struct ZoneHeapPimpl;
   ZoneHeapPimpl *pImpl;

public:
   ZoneHeapBase();
   ~ZoneHeapBase();

   virtual void *malloc(size_t size, int tag, void **ptr, const char *, int);
   virtual void  free(void *ptr, const char *, int);
   virtual void  freeTags(int lowtag, int hightag, const char *, int);
   virtual void  changeTag(void *ptr, int tag, const char *, int);
   virtual void *calloc(size_t n, size_t n2, int tag, void **user, const char *, int);
   virtual void *realloc(void *p, size_t n, int tag, void **user, const char *, int);
   virtual char *strdup(const char *s, int tag, void **user, const char *, int);
   virtual void  freeAllocAuto();
   virtual void *allocAuto(size_t n, const char *file, int line); // Not alloca because defines
   virtual void *reallocAuto(void *ptr, size_t n, const char *file, int line);
   virtual char *strdupAuto(const char *s, const char *file, int line);
   virtual void  checkHeap(const char *, int);
   virtual int   checkTag(void *, const char *, int);
};

//
// Single-consumer zone heap (thread-local).
//
class ZoneHeap final : public ZoneHeapBase { };

//
// Multiple-consumer zone heap (shared).
//
class ZoneHeapThreadSafe final : public ZoneHeapBase
{
public:
   virtual void *malloc(size_t size, int tag, void **ptr, const char *, int) override;
   virtual void  free(void *ptr, const char *, int) override;
   virtual void  freeTags(int lowtag, int hightag, const char *, int) override;
   virtual void  changeTag(void *ptr, int tag, const char *, int) override;
   virtual void *calloc(size_t n, size_t n2, int tag, void **user, const char *, int) override;
   virtual void *realloc(void *p, size_t n, int tag, void **user, const char *, int) override;
   virtual char *strdup(const char *s, int tag, void **user, const char *, int) override;
   virtual void  freeAllocAuto() override;
   virtual void *allocAuto(size_t n, const char *file, int line) override; // Not alloca because defines
   virtual void *reallocAuto(void *ptr, size_t n, const char *file, int line) override;
   virtual char *strdupAuto(const char *s, const char *file, int line) override;
   virtual void  checkHeap(const char *, int) override;
   virtual int   checkTag(void *, const char *, int) override;
};

//
// This class serves as a base class for C++ objects that want to support
// allocation on the zone heap. 
//
class ZoneObject
{
private:
   // static data
   static ZoneObject *objectbytag[PU_MAX];
   static void *newalloc;

   // instance data
   void        *zonealloc; // If non-null, the object is living on the zone heap
   ZoneObject  *zonenext;  // Next object on tag chain
   ZoneObject **zoneprev;  // Previous object on tag chain's next pointer

   void removeFromTagList();
   void addToTagList(int tag);

public:
   ZoneObject();
   virtual ~ZoneObject();
   void *operator new (size_t size);
   void *operator new (size_t size, int tag, void **user = nullptr);
   void  operator delete (void *p);
   void  operator delete (void *p, int, void **);
   void  changeTag(int tag);

   // zone memblock reflection
   int         getZoneTag()  const;
   size_t      getZoneSize() const;
   const void *getBlockPtr() const { return zonealloc; }

   static void FreeTags(int lowtag, int hightag);
};

// Has to be inline otherwise initialisation order causes stuff to use z_globalheap before its mutex exists
inline ZoneHeapThreadSafe z_globalheap;

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
