// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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

#ifndef M_FILE_H__
#define M_FILE_H__

#include "z_zone.h"

#ifndef _WIN32
#include <unistd.h>
#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif
#include <ftw.h> // [CG] Quite possibly the best header.
#endif

#include "doomtype.h"
#include "i_system.h"

#ifdef _WIN32
typedef bool(*file_walker)(const char *path);
#else
typedef  int(*file_walker)(const char *path, const struct stat *stat_result,
                           int flags, struct FTW *walker);
#endif

const char* M_GetFileSystemErrorMessage(void);
bool        M_PathExists(const char *path);
bool        M_DirnameIsFolder(const char *path);
size_t      M_PathJoinBuf(const char *d, const char *f, char **buf, size_t s);
char*       M_PathJoin(const char *one, const char *two);
bool        M_IsFile(const char *path);
bool        M_IsFileInFolder(const char *folder, const char *file);
bool        M_IsFolder(const char *path);
bool        M_IsAbsolutePath(const char *path);
const char* M_StripAbsolutePath(const char *path);
bool        M_CreateFile(const char *path);
bool        M_CreateFolder(const char *path);
bool        M_DeleteFile(const char *path);
bool        M_DeleteFileInFolder(const char *folder, const char *file);
bool        M_DeleteFolder(const char *path);
bool        M_WalkFiles(const char *path, file_walker walker);
bool        M_DeleteFolderAndContents(const char *path);
char*       M_GetCurrentFolder(void);
bool        M_SetCurrentFolder(const char *path);
char*       M_Dirname(const char *path);
const char* M_Basename(const char *path);
bool        M_RenamePath(const char *oldpath, const char *newpath);
uint32_t    M_FileLength(FILE *f);
FILE*       M_OpenFile(const char *path, const char *mode);
size_t      M_ReadFromFile(void *ptr, size_t size, size_t count, FILE *f);
size_t      M_WriteToFile(const void *ptr, size_t size, size_t count, FILE *f);
long        M_GetFilePosition(FILE *f);
bool        M_SeekFile(FILE *f, long int offset, int origin);
bool        M_FlushFile(FILE *f);
bool        M_CloseFile(FILE *f);
void        M_ReportFileSystemError(void);

#endif

