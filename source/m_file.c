// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
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

#ifdef WIN32
DWORD fs_error_code = 0;
#else
int fs_error_code = 0;
#endif

static char *error_messages[FS_ERROR_MAX] = {
   "filesystem entry already exists.",
   "filesystem path not found."
};

static void set_error_code(void)
{
#ifdef WIN32
   fs_error_code = GetLastError();
#else
   fs_error_code = errno;
   // [CG] strerror() doesn't allocate a string, so there's no need to free
   //      error_message.
#endif
}

#ifndef WIN32
static int remover(const char *path, const struct stat *stat_result,
                   int flags, struct FTW *walker)
{
   if(flags == FTW_DNR || flags == FTW_NS)
   {
      errno = EACCES;
   }

   if(flags == FTW_D || flags == FTW_DP)
   {
      if(rmdir((char *)path) == -1)
      {
         return -1;
      }
   }
   else if(flags == FTW_F || flags == FTW_SL)
   {
      if(unlink((char *)path) == -1)
      {
         return -1;
      }
   }

   return 0;
}
#endif

char* M_GetFileSystemErrorMessage(void)
{
#ifdef WIN32
   static LPVOID error_message_buffer = 0;
   static LPVOID error_message = 0;
   static boolean should_free = false;

   if(should_free)
   {
      if(error_message_buffer != 0)
      {
         LocalFree(error_message_buffer);
      }
      if(error_message != 0)
      {
         LocalFree(error_message);
      }
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
      fs_error_code,
      error_message_buffer
   );
   /*
   StringCchPrintf(
      (LPTSTR)error_message,
      LocalSize(error_message) / sizeof(TCHAR),
      TEXT("error %d: %s"),
      fs_error_code,
      error_message_buffer
   );
   */
   should_free = true;
   return (char *)error_message;
#else
   return strerror(fs_error_code);
#endif
}

boolean M_PathExists(char *path)
{
   struct stat stat_result;

   if(stat(path, &stat_result) != -1)
   {
      return true;
   }
   return false;
}

boolean M_IsFile(char *path)
{
   struct stat stat_result;

   if(stat(path, &stat_result) == -1)
   {
      return false;
   }

   if(stat_result.st_mode & S_IFREG)
   {
      return true;
   }
   return false;
}

boolean M_IsFolder(char *path)
{
   struct stat stat_result;

   if(stat(path, &stat_result) == -1)
   {
      return false;
   }

   if(stat_result.st_mode & S_IFDIR)
   {
      return true;
   }

   return false;
}

boolean M_IsAbsolutePath(char *path)
{
   size_t path_size = strlen(path);

#ifdef WIN32
   if(path_size > 3)
   {
      // [CG] Normal C: type path.
      if(strncmp(path + 1, ":\\", 2) == 0)
      {
         return true;
      }

      // [CG] UNC path.
      if(strncmp(path, "\\\\", 2) == 0)
      {
         return true;
      }

      // [CG] Long UNC path.
      if(strncmp(path, "\\\\?\\", 4) == 0)
      {
         return true;
      }
   }
#else
   if(path_size && strncmp(path, "/", 1) == 0)
   {
      return true;
   }
#endif
   return false;
}

// [CG] Creates a folder.  On *NIX systems the folder is given 0700
//      permissions.
boolean M_CreateFolder(char *path)
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
boolean M_CreateFile(char *path)
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

boolean M_DeleteFile(char *path)
{
#ifdef WIN32
   if(!DeleteFile(path))
#else
   if(unlink(path) == -1)
#endif
   {
      set_error_code();
      return false;
   }
   return true;
}

boolean M_DeleteFolder(char *path)
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

boolean M_DeleteFolderAndContents(char *path)
{
#ifdef WIN32
   WIN32_FIND_DATA fdata;
   HANDLE folder_handle;
   size_t path_length = strlen(path);
   size_t entry_length;
   char *folder_path = NULL;
   char *star_path = NULL;
   char *entry_path = NULL;


   // [CG] Check that the folder path has the minimum reasonable length.
   if(path_length < 4)
   {
      I_Error("M_DeleteFolderAndContents: Invalid path: %s.\n", path);
   }

   // [CG] Check that the folder path ends in a backslash, if not add it.  Then
   //      add an asterisk (apparently Windows needs this).
   if((*(path + path_length)) - 1 != '\\')
   {
      folder_path = calloc(path_length + 2, sizeof(char));
      star_path = calloc(path_length + 3, sizeof(char));
      sprintf(folder_path, "%s\\", path);
   }
   else
   {
      folder_path = calloc(path_length + 1, sizeof(char));
      star_path = calloc(path_length + 2, sizeof(char));
      strncpy(folder_path, path, path_length);
   }
   sprintf(star_path, "%s*", folder_path);

   folder_handle = FindFirstFile(folder_path, &fdata);

   while(FindNextFile(folder_handle, &fdata))
   {
      if(strncmp(fdata.cFileName, "..", 2))
      {
         // [CG] Skip the "previous folder" entry.
         continue;
      }
      entry_length = strlen(fdata.cFileName);
      entry_path = realloc(entry_path, path_length + entry_length + 1);
      memset(entry_path, 0, path_length + entry_length + 1);
      strncat(entry_path, folder_path, path_length);
      strncat(entry_path, fdata.cFileName, entry_length);
      if(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
         if(!M_DeleteFolderAndContents(path))
         {
            // [CG] M_DeleteFolderAndContents set an error, so quit.
            return false;
         };
      }
      else if(!DeleteFile(entry_path))
      {
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
      {
         return false;
      }
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

char* M_GetCurrentFolder(void)
{
#ifdef WIN32
   LPTSTR buf;
   DWORD buf_size;

   buf_size = GetCurrentDirectory(0, buf);

   if(buf_size == 0)
   {
      return NULL;
   }

   buf = (LPTSTR)calloc(buf_size, sizeof(TCHAR));
   buf_size = GetCurrentDirectory(buf_size, buf);
   if(buf_size == 0)
   {
      return NULL;
   }
   return buf;
#else
   long size;
   char *buf;

   size = pathconf(".", _PC_PATH_MAX);

   if((buf = (char *)malloc((size_t)size)) == NULL)
   {
      return NULL;
   }
   return getcwd(buf, (size_t)size);
#endif
}

boolean M_SetCurrentFolder(char *path)
{
#ifdef WIN32
   if(!SetCurrentDirectory(path))
   {
      return false;
   }
#else
   if(chdir((const char*)path) == -1)
   {
      return false;
   }
#endif
   return true;
}

char* M_Basename(char *path)
{
   char *basename;
#ifdef WIN32
   char path_sep = '\\';
#else
   char path_sep = '/';
#endif

   if((basename = (char *)strrchr((const char *)path, path_sep)) == NULL)
      return path;
   return basename + 1;
}

