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

#include "m_file.h"
#include "z_zone.h"

#ifdef WIN32
DWORD fs_error_code = 0;
#else
int fs_error_code = 0;
#endif

static const char *error_messages[FS_ERROR_MAX] = {
   "filesystem entry already exists.",
   "filesystem path not found."
};

static void set_windows_error_code(void)
{
#ifdef WIN32
   fs_error_code = GetLastError();
#endif
}

static void set_sys_error_code(void)
{
   fs_error_code = errno;
}

static void set_error_code(void)
{
#ifdef WIN32
   set_windows_error_code();
#else
   set_sys_error_code();
#endif
}

#ifndef WIN32
static int remover(const char *path, const struct stat *stat_result,
                   int flags, struct FTW *walker)
{
   if(flags == FTW_DNR || flags == FTW_NS)
      errno = EACCES;

   if(flags == FTW_D || flags == FTW_DP)
   {
      if(rmdir((char *)path) == -1)
         return -1;
   }
   else if(flags == FTW_F || flags == FTW_SL)
   {
      if(remove((char *)path) == -1)
         return -1;
   }

   return 0;
}
#endif

const char* M_GetFileSystemErrorMessage(void)
{
#ifdef WIN32
   static LPVOID error_message_buffer = 0;
   static LPVOID error_message = 0;
   static bool should_free = false;

   if(should_free)
   {
      if(error_message_buffer != 0)
         LocalFree(error_message_buffer);
      if(error_message != 0)
         LocalFree(error_message);
      should_free = false;
   }
#endif
   if(fs_error_code == FS_ERROR_ALREADY_EXISTS ||
      fs_error_code == FS_ERROR_NOT_FOUND)
   {
      return error_messages[fs_error_code];
   }
#ifdef WIN32
   FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM     |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      fs_error_code,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &error_message_buffer,
      0,
      NULL
   );
   error_message = (LPVOID)LocalAlloc(
      LMEM_ZEROINIT,
      (strlen((LPCTSTR)error_message_buffer) + 40) * sizeof(TCHAR)
   );
   sprintf(
      (char *)error_message,
      "error %d: %s\n",
      (int)fs_error_code,
      (char *)error_message_buffer
   );
   should_free = true;
   return (const char *)error_message;
#else
   return (const char *)strerror(fs_error_code);
#endif
}

bool M_PathExists(const char *path)
{
   struct stat stat_result;

   if(stat(path, &stat_result) != -1)
      return true;
   return false;
}

bool M_DirnameIsFolder(const char *path)
{
#ifdef WIN32
   char path_sep = '\\';
#else
   char path_sep = '/';
#endif
   char *buffer = NULL;
   char *basename = NULL;
   size_t dirname_size;
   bool exists = false;

   if(!M_IsAbsolutePath(path))
      return false;

   if(!(basename = strrchr((char *)path, path_sep)))
      return false;

   if(path == basename)
      return true;

   dirname_size = (basename - path);
   buffer = ecalloc(char *, 1, dirname_size + 1);
   strncpy(buffer, path, dirname_size);
   if(M_IsFolder((const char *)buffer))
      exists = true;

   efree(buffer);

   return exists;
}

char* M_PathJoin(const char *one, const char *two)
{
   size_t one_length = strlen(one);
   size_t two_length = strlen(two);
   char *buf = ecalloc(char *, 1, one_length + two_length + 2);

   if((one[one_length]) == '/' || (one[one_length]) == '\\')
      sprintf(buf, "%s%s", one, two);
   else
      sprintf(buf, "%s/%s", one, two);

   return buf;
}

bool M_IsFile(const char *path)
{
   struct stat stat_result;

   if(stat(path, &stat_result) == -1)
      return false;

   if(stat_result.st_mode & S_IFREG)
      return true;

   return false;
}

bool M_IsFileInFolder(const char *folder, const char *file)
{
   bool ret;
   char *full_path = M_PathJoin(folder, file);

   ret = M_IsFile(full_path);
   efree(full_path);

   return ret;
}

bool M_IsFolder(const char *path)
{
   struct stat stat_result;

   if(stat(path, &stat_result) == -1)
      return false;

   if(stat_result.st_mode & S_IFDIR)
      return true;

   return false;
}

bool M_IsAbsolutePath(const char *path)
{
   size_t path_size = strlen(path);

#ifdef WIN32
   if(path_size > 3)
   {
      // [CG] Normal C: type path.
      if(strncmp(path + 1, ":\\", 2) == 0)
         return true;

      // [CG] UNC path.
      if(strncmp(path, "\\\\", 2) == 0)
         return true;

      // [CG] Long UNC path.
      if(strncmp(path, "\\\\?\\", 4) == 0)
         return true;
   }
#else
   if(path_size && strncmp(path, "/", 1) == 0)
      return true;
#endif
   return false;
}

// [CG] Creates a folder.  On *NIX systems the folder is given 0700
//      permissions.
bool M_CreateFolder(const char *path)
{
#ifdef WIN32
   if(!CreateDirectory(path, NULL))
#else
   if(mkdir(path, S_IRWXU) == -1)
#endif
   {
      set_error_code();
      return false;
   }
   return true;
}

// [CG] Creates a file.  On *NIX systems the folder is given 0600 permissions.
//      If the file already exists, error codes are set.
bool M_CreateFile(const char *path)
{
#ifdef WIN32
   HANDLE fd = CreateFile(
      path,
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ,
      NULL,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL,
      NULL
   );

   if(fd == INVALID_HANDLE_VALUE)
   {
      set_error_code();
      return false;
   }
   CloseHandle(fd);
#else
   int fd = open(path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

   if(fd == -1)
   {
      set_error_code();
      return false;
   }
   close(fd);
#endif
   return true;
}

bool M_DeleteFile(const char *path)
{
#ifdef WIN32
   if(DeleteFile(path) == 0)
#else
   if(remove(path) != 0)
#endif
   {
      set_error_code();
      return false;
   }
   return true;
}

bool M_DeleteFileInFolder(const char *folder, const char *file)
{
   bool ret;
   char *full_path = M_PathJoin(folder, file);

   ret = M_DeleteFile(full_path);
   efree(full_path);

   return ret;
}

bool M_DeleteFolder(const char *path)
{
#ifdef WIN32
   if(!RemoveDirectory(path))
#else
   if(rmdir(path) == -1)
#endif
   {
      set_error_code();
      return false;
   }
   return true;
}

bool M_DeleteFolderAndContents(const char *path)
{
#ifdef WIN32
   WIN32_FIND_DATA fdata;
   HANDLE folder_handle;
   size_t path_length = strlen(path);
   size_t entry_length, full_length;
   char *folder_path = NULL;
   char *star_path = NULL;
   char *entry_path = NULL;


   // [CG] Check that the folder path has the minimum reasonable length.
   if(path_length < 4)
      I_Error("M_DeleteFolderAndContents: Invalid path: %s.\n", path);

   // [CG] Check that the folder path ends in a backslash, if not add it.  Then
   //      add an asterisk (apparently Windows needs this).
   if((*(path + path_length)) - 1 != '\\')
   {
      folder_path = ecalloc(char *, path_length + 2, sizeof(char));
      star_path = ecalloc(char *, path_length + 3, sizeof(char));
      sprintf(folder_path, "%s\\", path);
   }
   else
   {
      folder_path = ecalloc(char *, path_length + 1, sizeof(char));
      star_path = ecalloc(char *, path_length + 2, sizeof(char));
      strncpy(folder_path, path, path_length);
   }
   sprintf(star_path, "%s*", folder_path);

   folder_handle = FindFirstFile(star_path, &fdata);

   while(FindNextFile(folder_handle, &fdata))
   {
      // [CG] Skip the "previous folder" entry.
      if(strncmp(fdata.cFileName, "..", 2))
         continue;

      entry_length = strlen(fdata.cFileName);
      full_length = path_length + entry_length + 1;
      entry_path = erealloc(char *, entry_path, full_length);
      memset(entry_path, 0, full_length);
      strncat(entry_path, folder_path, path_length);
      strncat(entry_path, fdata.cFileName, entry_length);
      if(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
         if(!M_DeleteFolderAndContents(entry_path))
         {
            efree(folder_path);
            efree(star_path);
            efree(entry_path);
            return false;
         }
      }
      else if(!DeleteFile(entry_path))
      {
         efree(folder_path);
         efree(star_path);
         efree(entry_path);
         set_error_code();
         return false;
      }
   }

   // [CG] FindNextFile returns false if there is an error, but running out of
   //      contents is considered an error so we need to check for that code to
   //      determine if we successfully deleted all the contents.
   if(GetLastError() == ERROR_NO_MORE_FILES)
   {
      if(!M_DeleteFolder(path))
         return false;
   }
   else
   {
      set_error_code();
      return false;
   }
#else
   if(nftw(path, remover, 64, FTW_CHDIR | FTW_DEPTH | FTW_PHYS) == -1)
   {
      set_error_code();
      return false;
   }
#endif
   return true;
}

const char* M_GetCurrentFolder(void)
{
#ifdef WIN32
   LPTSTR buf = NULL;
   DWORD buf_size;

   buf_size = GetCurrentDirectory(0, buf);

   if(buf_size == 0)
      return NULL;

   buf = ecalloc(LPTSTR, buf_size, sizeof(TCHAR));
   buf_size = GetCurrentDirectory(buf_size, buf);
   if(buf_size == 0)
      return NULL;

   return (const char *)buf;
#else
   size_t size;
   char *buf;

   size = (size_t)pathconf(".", _PC_PATH_MAX);

   if((buf = emalloc(char *, size)) == NULL)
      return NULL;

   return (const char *)getcwd(buf, size);
#endif
}

bool M_SetCurrentFolder(const char *path)
{
#ifdef WIN32
   if(!SetCurrentDirectory(path))
      return false;
#else
   if(chdir(path) == -1)
      return false;
#endif
   return true;
}

const char* M_Basename(const char *path)
{
   const char *basename;
#ifdef WIN32
   char path_sep = '\\';
#else
   char path_sep = '/';
#endif

   if((basename = strrchr(path, path_sep)) == NULL)
      return path;

   return basename + 1;
}

bool M_RenamePath(const char *oldpath, const char *newpath)
{
   if(rename(oldpath, newpath))
   {
      set_sys_error_code();
      return false;
   }
   return true;
}

