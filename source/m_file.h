// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//    Platform-independent filesystem operations.
//
//-----------------------------------------------------------------------------

#ifndef __M_FILE_H__
#define __M_FILE_H__

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif
#include <ftw.h> // [CG] Quite possibly the best header.
#endif

#include "doomtype.h"

// [CG] There are pre-defined error strings for errors that we want to handle
//      at some point, but because Windows and POSIX systems don't share error
//      code names or numbers there has to be some translation.  So this
//      enumeration both maps error codes to pre-defined error strings, and
//      maps expected Windows or POSIX error codes to internal EE error codes.
enum
{
#ifdef WIN32
   FS_ERROR_ALREADY_EXISTS = ERROR_ALREADY_EXISTS,
   FS_ERROR_NOT_FOUND = ERROR_NOT_FOUND,
#else
   FS_ERROR_ALREADY_EXISTS = EEXIST,
   FS_ERROR_NOT_FOUND = ENOENT,
#endif
   FS_ERROR_MAX,
};

#ifdef WIN32
extern DWORD fs_error_code;
#else
extern int fs_error_code;
#endif

const char* M_GetFileSystemErrorMessage(void);
bool        M_PathExists(const char *path);
bool        M_IsFile(const char *path);
bool        M_IsFolder(const char *path);
bool        M_IsAbsolutePath(const char *path);
bool        M_CreateFolder(const char *path);
bool        M_CreateFile(const char *path);
bool        M_DeleteFolder(const char *path);
bool        M_DeleteFile(const char *path);
bool        M_DeleteFolderAndContents(const char *path);
const char* M_GetCurrentFolder(void);
bool        M_SetCurrentFolder(const char *path);
const char* M_Basename(const char *path);

#endif

