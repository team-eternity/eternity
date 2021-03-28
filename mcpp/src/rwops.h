/*

   RWOps - File IO Abstraction Support

   jhaley 20120705

*/

#ifndef RWOPS_H__
#define RWOPS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef unsigned char rwbyte;

#ifdef _MSC_VER
#define RWOPS_DECL  __cdecl
#else
#define RWOPS_DECL
#endif

typedef struct RWops_s RWops_t;

typedef long   (*rwseek_t) (RWops_t *, long , int);
typedef size_t (*rwread_t) (RWops_t *, void *, size_t, size_t);
typedef char  *(*rwgets_t) (char *, int, RWops_t *);
typedef size_t (*rwwrite_t)(RWops_t *, const void *, size_t, size_t);
typedef int    (*rwclose_t)(RWops_t *);
typedef int    (*rwerror_t)(RWops_t *);

typedef void *(RWOPS_DECL *rwopen_t)  (const char *, const char *);
typedef void  (RWOPS_DECL *rwcclose_t)(void *mem);

struct RWops_s
{
   FILE *fp;         // file pointer, when using stdio
   int staticHandle; // 1 if fp is a static file handle

   rwbyte *base; // pointers when using memory
   rwbyte *here;
   rwbyte *stop;

   // "Methods"
   rwseek_t  seek;
   rwread_t  read;
   rwgets_t  gets;
   rwwrite_t write;
   rwclose_t close;
   rwerror_t error;
};

#if _WIN32 || _WIN64 || __CYGWIN__ || __CYGWIN64__ || __MINGW32__   \
            || __MINGW64__
#if     DLL_EXPORT || (__CYGWIN__ && PIC)
#define RWDLL_DECL    __declspec( dllexport)
#elif   DLL_IMPORT
#define RWDLL_DECL    __declspec( dllimport)
#else
#define RWDLL_DECL
#endif
#else
#define RWDLL_DECL
#endif

// DLL exports

// Call both of these before using mcpp with memory file IO
RWDLL_DECL void RWOPS_DECL mcpp_setopencallback(rwopen_t oc);
RWDLL_DECL void RWOPS_DECL mcpp_setclosecallback(rwcclose_t cc);

// Call one of these from inside your open callback and return the value.
RWDLL_DECL void * RWOPS_DECL mcpp_openmemory(void *mem, size_t size);
RWDLL_DECL void * RWOPS_DECL mcpp_openfile(const char *file, const char *mode);

// Internal fopen
RWops_t *mcpp_fopen(const char *file, const char *mode);
RWops_t *mcpp_openstdin(void);

#define rw_fseek(fp, pos, whence) (fp)->seek((fp), (pos), (whence))
#define rw_ftell(fp)              (fp)->seek((fp), 0,     SEEK_CUR) 
#define rw_fread(fp, ptr, m, n)   (fp)->read((fp), (ptr), (m), (n))
#define rw_fgets(s, i, fp)        (fp)->gets((s), (i), (fp))
#define rw_fwrite(fp, ptr, m, n)  (fp)->write((fp), (ptr), (m), (n))
#define rw_fclose(fp)             (fp)->close(fp)
#define rw_ferror(fp)             (fp)->error(fp)

#ifdef __cplusplus
}
#endif

#endif

// EOF

