//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//  Remark: this was the only stuff that, according
//   to John Carmack, might have been useful for Quake.
//
//  Rewritten by Lee Killough, though, since it was not efficient enough.
//
// Authors: James Haley, Max Waine
//

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

    PU_SOUND,    // currently unused
    PU_MUSIC,    // currently unused
    PU_RENDERER, // haleyjd 06/29/08: for data allocated via R_Init
    PU_VALLOC,   // haleyjd 04/29/13: belongs to a video/rendering buffer
    PU_AUTO,     // haleyjd 07/08/10: automatic allocation
    PU_LEVEL,    // allocation belongs to level (freed at next level load)

    // cache levels

    PU_CACHE, // block is cached (may be implicitly freed at any time!)

    PU_MAX // Must always be last -- killough
};

static constexpr int PU_PURGELEVEL = PU_CACHE; // First purgable tag's level.

// killough 3/22/98: add file/line info

void Z_Init();

void *Z_SysMalloc(size_t size);
void *Z_SysCalloc(size_t n1, size_t n2);
void *Z_SysRealloc(void *ptr, size_t size);
void  Z_SysFree(void *p);

//
// Context-specific Heap alloc Macros
//
#define zhmalloc(zoneheap, type, n) \
    static_cast<type>((zoneheap).malloc(n, PU_STATIC, 0, __FILE__, __LINE__))

#define zhmalloctag(zoneheap, type, n1, tag, user) \
    static_cast<type>((zoneheap).malloc(n1, tag, user, __FILE__, __LINE__))

#define zhcalloc(zoneheap, type, n1, n2) \
    static_cast<type>((zoneheap).calloc(n1, n2, PU_STATIC, 0, __FILE__, __LINE__))

#define zhcalloctag(zoneheap, type, n1, n2, tag, user) \
    static_cast<type>((zoneheap).calloc(n1, n2, tag, user, __FILE__, __LINE__))

#define zhrealloc(zoneheap, type, p, n) \
    static_cast<type>((zoneheap).realloc(p, n, PU_STATIC, 0, __FILE__, __LINE__))

#define zhrealloctag(zoneheap, type, p, n, tag, user) \
    static_cast<type>((zoneheap).realloc(p, n, tag, user, __FILE__, __LINE__))

#define zhstructalloc(zoneheap, type, n) \
    static_cast<type *>((zoneheap).calloc(n, sizeof(type), PU_STATIC, 0, __FILE__, __LINE__))

#define zhstructalloctag(zoneheap, type, n, tag) \
    static_cast<type *>((zoneheap).calloc(n, sizeof(type), tag, 0, __FILE__, __LINE__))

#define zhstrdup(zoneheap, s) (zoneheap).strdup(s, PU_STATIC, 0, __FILE__, __LINE__)

#define zhfree(zoneheap, p)   (zoneheap).free(p, __FILE__, __LINE__)

// Global Heap alloc Macros

// clang-format off

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

// clang-format on

#define emalloc(type, n)                    zhmalloc(z_globalheap, type, n)
#define emalloctag(type, n1, tag, user)     zhmalloctag(z_globalheap, type, n1, tag, user)
#define ecalloc(type, n1, n2)               zhcalloc(z_globalheap, type, n1, n2)
#define ecalloctag(type, n1, n2, tag, user) zhcalloctag(z_globalheap, type, n1, n2, tag, user)
#define erealloc(type, p, n)                zhrealloc(z_globalheap, type, p, n)
#define erealloctag(type, p, n, tag, user)  zhrealloctag(z_globalheap, type, p, n, tag, user)
#define estructalloc(type, n)               zhstructalloc(z_globalheap, type, n)
#define estructalloctag(type, n, tag)       zhstructalloctag(z_globalheap, type, n, tag)
#define estrdup(s)                          zhstrdup(z_globalheap, s)
#define efree(p)                            zhfree(z_globalheap, p)

//
// Globally useful macros
//

// Get the size of a static array
#define earrlen(a) (sizeof(a) / sizeof(*a))

// Classify a string as either lengthful (non-nullptr, not zero length), or empty
#define estrnonempty(str) ((str) && *(str))
#define estrempty(str)    (!estrnonempty((str)))

// Difference between two pointers, to avoid spelling static_cast<int> every time
#define eindex static_cast<int>

// Doom-style printf
void doom_printf(E_FORMAT_STRING(const char *), ...) E_PRINTF(1, 2);
void doom_warningf(E_FORMAT_STRING(const char *), ...) E_PRINTF(1, 2);
void doom_warningf_silent(E_FORMAT_STRING(const char *), ...) E_PRINTF(1, 2);

#ifdef INSTRUMENTED
extern int printstats;
#endif

void Z_PrintZoneHeap();

void Z_DumpCore();

struct ZoneHeapMutex;

//
// This class represents an instance of the zone heap. It might be
// single or multiple-consumer. Previously much of this was statics
// in z_native.cpp but thread-local tagged memory demands that what
// was global state is now per-instance.
//
class ZoneHeapBase
{
protected:
    struct memblock_t *m_blockbytag[PU_MAX]; // used for tracking all zone blocks

#ifdef INSTRUMENTED
    size_t m_memorybytag[PU_MAX];
#endif

public:
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

    void print(const char *filename);
    void dumpCore(const char *filename);

#ifdef INSTRUMENTED
    inline size_t memoryForTag(const int tag) { return m_memorybytag[tag]; }
#endif
};

//
// Single-consumer zone heap (thread-local).
//
class ZoneHeap final : public ZoneHeapBase
{
public:
    ~ZoneHeap() { freeTags(PU_STATIC, PU_CACHE, __FILE__, __LINE__); };

    // TODO: Re-enable when C++20 is a thing and kill the zh macros
    // THREAD_FIXME: Guard against any and all PU_STATIC (and maybe PU_PERMANENT) allocations?
    // template <typename T>
    // inline T *malloc(const size_t size, const int tag, void **user = nullptr,
    //   const std::source_location loc = std::source_location::current())
    //{
    //   return static_cast<T *>(ZoneHeapBase::malloc(size, tag, user, loc.file_name(), loc.line()));
    //}
    // template <typename T>
    // inline T *calloc(size_t n, size_t n2, int tag, void **user = nullptr,
    //   const std::source_location loc = std::source_location::current())
    //{
    //   return static_cast<T *>(ZoneHeapBase::calloc(n, n2, tag, user, loc.file_name(), loc.line()));
    //}
    // template <typename T>
    // inline T *realloc(void *p, size_t n, int tag, void **user = nullptr,
    //   const std::source_location loc = std::source_location::current())
    //{
    //   return static_cast<T *>(ZoneHeapBase::realloc(p, n, tag, user, loc.file_name(), loc.line()));
    //}
    // template <typename T>
    // inline T *allocAuto(size_t n, const std::source_location loc = std::source_location::current())
    //{
    //   return static_cast<T *>(ZoneHeapBase::allocAuto(n, loc.file_name(), loc.line()));
    //}
    // template <typename T>
    // inline T *reallocAuto(void *ptr, size_t n, const std::source_location loc = std::source_location::current())
    //{
    //   return static_cast<T *>(ZoneHeapBase::reallocAuto(ptr, n, ptr, loc.file_name(), loc.line()));
    //}
};

//
// Multiple-consumer zone heap (shared).
//
class ZoneHeapThreadSafe final : public ZoneHeapBase
{
private:
    ZoneHeapMutex *m_mutex;

public:
    ZoneHeapThreadSafe();
    ~ZoneHeapThreadSafe();

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
    static void       *newalloc;

    // instance data
    void        *zonealloc; // If non-null, the object is living on the zone heap
    ZoneObject  *zonenext;  // Next object on tag chain
    ZoneObject **zoneprev;  // Previous object on tag chain's next pointer

    void removeFromTagList();
    void addToTagList(int tag);

public:
    ZoneObject();
    virtual ~ZoneObject();
    void *operator new(size_t size);
    void *operator new(size_t size, int tag, void **user = nullptr);
    void  operator delete(void *p);
    void  operator delete(void *p, int, void **);
    void  changeTag(int tag);

    // zone memblock reflection
    int         getZoneTag() const;
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
