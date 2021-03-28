/* mcpp_lib.h: declarations of libmcpp exported (visible) functions */
#ifndef _MCPP_LIB_H
#define _MCPP_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MCPP_OUT_H
#include    "mcpp_out.h"            /* declaration of OUTDEST   */
#endif

#if _WIN32 || _WIN64 || __CYGWIN__ || __CYGWIN64__ || __MINGW32__   \
            || __MINGW64__
#if     DLL_EXPORT || (__CYGWIN__ && PIC)
#define DLL_DECL    __declspec( dllexport)
#elif   DLL_IMPORT
#define DLL_DECL    __declspec( dllimport)
#else
#define DLL_DECL
#endif
#else
#define DLL_DECL
#endif

#ifdef _MSC_VER
#define MCPP_DECL  __cdecl
#else
#define MCPP_DECL
#endif

extern DLL_DECL int    MCPP_DECL mcpp_lib_main( int argc, char ** argv);
extern DLL_DECL void   MCPP_DECL mcpp_reset_def_out_func( void);
extern DLL_DECL void   MCPP_DECL mcpp_set_out_func(
                                   int (* func_fputc)  ( int c, OUTDEST od),
                                   int (* func_fputs)  ( const char * s, OUTDEST od),
                                   int (* func_fprintf)( OUTDEST od, const char * format, ...));
extern DLL_DECL void   MCPP_DECL mcpp_use_mem_buffers( int tf);
extern DLL_DECL char * MCPP_DECL mcpp_get_mem_buffer( OUTDEST od);

#ifdef __cplusplus
}
#endif

#endif  /* _MCPP_LIB_H  */
