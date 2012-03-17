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

#include "z_zone.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#endif

#include <fcntl.h>

#include "m_file.h"
#include "m_qstr.h"
#include "w_wad.h"

static int fs_error_code = 0;

static void set_error_code(void)
{
   fs_error_code = errno;
}

#ifndef _WIN32
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
#if !defined(_WIN32) || defined(__MINGW32__)
   return (const char *)strerror(fs_error_code);
#else
   static char error_message[512];

   strerror_s(error_message, 512, fs_error_code);
   return (const char *)error_message;
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
   bool is_folder = false;
   char *dirname = M_Dirname(path);

   if((!dirname) || M_IsFolder(dirname))
      is_folder = true;

   efree(dirname);

   return is_folder;
}

size_t M_PathJoinBuf(const char *d, const char *f, char **buf, size_t s)
{
   bool needs_slash;
   size_t d_length = strlen(d);
   size_t f_length, max_len;

   if((d[d_length]) == '/')
      needs_slash = false;
   else
      needs_slash = true;

   if(!s)
   {
      f_length = strlen(f);

      if(needs_slash)
         max_len = d_length + f_length + 2;
      else
         max_len = d_length + f_length + 1;

      *buf = ecalloc(char *, 1, max_len);
   }
   else
      max_len = s;

   if(needs_slash)
      psnprintf(*buf, max_len, "%s/%s", d, f);
   else
      psnprintf(*buf, max_len, "%s%s", d, f);

   return max_len;
}

char* M_PathJoin(const char *one, const char *two)
{
   char *buf = NULL;

   M_PathJoinBuf(one, two, &buf, 0);

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

bool M_IsRootFolder(const char *path)
{
   size_t path_size;

   if((*path) == '/')
      return true;

   path_size = strlen(path);

   if(!path_size)
      return false;

#ifdef _WIN32
   qstring buf;

   buf = path;

   if(buf.length() < 2)
      return false;

   buf.normalizeSlashes();

   if((buf.length() == 2) && (isalpha(buf.charAt(0))) && (buf.charAt(1) == ':'))
      return true;
   else if((buf.length() == 3) && (strncmp(buf.constPtr() + 1, ":/", 2) == 0))
      return true;
   else if((buf.length() == 2) && (strncmp(buf.constPtr(), "//", 2) == 0))
      return true;
   else if((buf.length() == 4) && (strncmp(buf.constPtr(), "//?/", 4) == 0))
      return true;
#endif
   return false;
}

bool M_IsAbsolutePath(const char *path)
{
   size_t path_size;

   if((*path) == '/')
      return true;

   path_size = strlen(path);

   if(!path_size)
      return false;

#ifdef _WIN32
   qstring buf;

   buf = path;

   if(buf.length() < 3)
      return false;

   buf.normalizeSlashes();

   if(strncmp(buf.constPtr() + 1, ":/", 2) == 0) // [CG] Normal C: type path.
      return true;
   else if(strncmp(buf.constPtr(), "//", 2) == 0) // [CG] UNC path.
      return true;
   else if(strncmp(buf.constPtr(), "//?/", 4) == 0) // [CG] Long UNC path.
      return true;
#endif
   return false;
}

const char* M_StripAbsolutePath(const char *path)
{
   const char *relative_path = path;

   if(!M_IsAbsolutePath(path))
      return path;

#ifdef _WIN32
   qstring buf;

   buf = path;

   if(buf.length() < 3)
      return path;

   buf.normalizeSlashes();

   if(strncmp(buf.constPtr() + 1, ":/", 2) == 0) // [CG] Normal C: type path.
      relative_path = path + 2;
   else if(strncmp(buf.constPtr(), "//?/", 4) == 0) // [CG] Long UNC path.
      relative_path = path + 3;

   while(((*relative_path) == '\\') || ((*relative_path) == '/'))
#else
   while((*relative_path) == '/')
#endif
      relative_path++;

   return relative_path;
}

// [CG] Creates a folder.  On *NIX systems the folder is given 0700
//      permissions.
bool M_CreateFolder(const char *path)
{
#ifdef _WIN32
   if(_mkdir(path) == -1)
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
#ifdef _WIN32
   int fd = _open(path, _O_RDWR | _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
#else
   int fd = open(path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif

   if(fd == -1)
   {
      set_error_code();
      return false;
   }
#if !defined(_WIN32) || defined(__MINGW32__)
   close(fd);
#else
   _close(fd);
#endif
   return true;
}

bool M_DeletePath(const char *path)
{
   if(!M_PathExists(path))
      return false;

   if(M_IsFile(path))
      return M_DeleteFile(path);
   else if(M_IsFolder(path))
      return M_DeleteFolder(path);
   else
      return false;
}

bool M_DeleteFile(const char *path)
{
   if(remove(path) != 0)
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
#ifdef _WIN32
   int result;
   DWORD attr = GetFileAttributes(path);

   if(attr & FILE_ATTRIBUTE_READONLY)
   {
      attr &= ~FILE_ATTRIBUTE_READONLY;
      if((result = SetFileAttributes(path, attr)) == 0)
         return false;
   }

   if(_rmdir(path) == -1)
#else
   if(rmdir(path) == -1)
#endif
   {
      set_error_code();
      return false;
   }
   return true;
}

bool M_IterateFiles(const char *path, file_iterator iterator)
{
   if(!M_IsFolder(path))
      return false;

#ifdef _WIN32
   WIN32_FIND_DATA fdata;
   HANDLE folder_handle;
   qstring path_buf, star_buf, entry_buf;

   path_buf = path;

   // [CG] Check that the folder path has the minimum reasonable length.
   if(path_buf.length() < 4)
      I_Error("M_WalkFiles: Invalid path: %s.\n", path);

   // [CG] Check that the folder path ends in a backslash, if not add it.  Then
   //      add an asterisk (apparently Windows needs this).
   path_buf.normalizeSlashes();
   star_buf.Printf(0, "%s/*", path_buf.constPtr());
   star_buf.normalizeSlashes();

   folder_handle = FindFirstFile(star_buf.getBuffer(), &fdata);

   while(FindNextFile(folder_handle, &fdata))
   {
      size_t entry_length = strlen(fdata.cFileName);

      // [CG] Skip the "current folder" and "previous folder" entries.
      if((entry_length == 1) && (!strcmp(fdata.cFileName, ".")))
         continue;
      else if((entry_length == 2) && (!strcmp(fdata.cFileName, "..")))
         continue;

      entry_buf.Printf(0, "%s/%s", path_buf.constPtr(), fdata.cFileName);
      entry_buf.normalizeSlashes();

      if(!iterator(entry_buf.constPtr()))
      {
         set_error_code();
         FindClose(folder_handle);
         return false;
      }
   }

   FindClose(folder_handle);

   // [CG] FindNextFile returns false if there is an error, but running out of
   //      contents is considered an error so we need to check for that code to
   //      determine if we successfully walked all the contents.
   if(GetLastError() != ERROR_NO_MORE_FILES)
   {
      set_error_code();
      return false;
   }
#else
   DIR *d;
   dirent *e;
   qstring path_buf, entry_buf;

   path_buf = path;
   path_buf.normalizeSlashes();
   
   if(!(d = opendir(path)))
   {
      set_error_code();
      return false;
   }

   while(d)
   {
      if(!(e = readdir(d)))
      {
         closedir(d);
         set_error_code();
         return false;
      }

      entry_buf.Printf(0, "%s/%s", path_buf.constPtr(), e->d_name);
      entry_buf.normalizeSlashes();

      if(!iterator(entry_buf.constPtr()))
      {
         set_error_code();
         closedir(d);
         return false;
      }
   }
#endif
   return true;
}

bool M_WalkFiles(const char *path, file_walker walker)
{
   if(!M_IsFolder(path))
      return false;

#ifdef _WIN32
   WIN32_FIND_DATA fdata;
   HANDLE folder_handle;
   qstring path_buf, star_buf, entry_buf;

   path_buf = path;

   // [CG] Check that the folder path has the minimum reasonable length.
   if(path_buf.length() < 4)
      I_Error("M_WalkFiles: Invalid path: %s.\n", path);

   // [CG] Check that the folder path ends in a backslash, if not add it.  Then
   //      add an asterisk (apparently Windows needs this).
   path_buf.normalizeSlashes();
   star_buf.Printf(0, "%s/*", path_buf.constPtr());
   star_buf.normalizeSlashes();

   folder_handle = FindFirstFile(star_buf.getBuffer(), &fdata);

   while(FindNextFile(folder_handle, &fdata))
   {
      size_t entry_length = strlen(fdata.cFileName);

      // [CG] Skip the "current folder" and "previous folder" entries.
      if((entry_length == 1) && (!strcmp(fdata.cFileName, ".")))
         continue;
      else if((entry_length == 2) && (!strcmp(fdata.cFileName, "..")))
         continue;

      entry_buf.Printf(0, "%s/%s", path_buf.constPtr(), fdata.cFileName);
      entry_buf.normalizeSlashes();

      if(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
         if(!M_WalkFiles(entry_buf.constPtr(), walker))
            return false;
      }
      else if(!walker(entry_buf.constPtr()))
      {
         FindClose(folder_handle);
         set_error_code();
         return false;
      }
   }

   FindClose(folder_handle);

   // [CG] FindNextFile returns false if there is an error, but running out of
   //      contents is considered an error so we need to check for that code to
   //      determine if we successfully walked all the contents.
   if(GetLastError() == ERROR_NO_MORE_FILES)
   {
      if(!walker(path))
         return false;
   }
   else
   {
      set_error_code();
      return false;
   }
#else
   if(nftw(path, walker, 64, FTW_CHDIR | FTW_DEPTH | FTW_PHYS) == -1)
   {
      set_error_code();
      return false;
   }
#endif
   return true;
}

bool M_DeleteFolderAndContents(const char *path)
{
#ifdef _WIN32
   return M_WalkFiles(path, M_DeletePath);
#else
   return M_WalkFiles(path, remover);
#endif
}

char* M_GetCurrentFolder(void)
{
   char *output = NULL;
#ifdef _WIN32
   char *temp = NULL;

   if(!(temp = _getcwd(temp, 1)))
   {
      set_error_code();
      return false;
   }
   output = estrdup(temp);
   Z_SysFree(temp);
#else
   size_t size;

   size = (size_t)pathconf(".", _PC_PATH_MAX);
   output = emalloc(char *, size);
   if(!(output = getcwd(output, size)))
   {
      set_error_code();
      return false;
   }
#endif
   return output;
}

bool M_SetCurrentFolder(const char *path)
{
#if !defined(_WIN32) || defined(__MINGW32__)
   if(chdir(path) == -1)
#else
   if(_chdir(path) == -1)
#endif
      return false;
   return true;
}

char* M_Dirname(const char *path)
{
   qstring buf;
   size_t dirname_size;

   buf = path;
   buf.normalizeSlashes();

   // [CG] Check for absolute root paths first, these have no dirname and
   //      therefore return NULL.
   switch(buf.length())
   {
      case 1:
         return NULL;
         break;
      case 2:
         if(buf.charAt(1) == '/')
            return NULL;
         break;
#ifdef _WIN32
      case 3:
         if(strncmp(buf.constPtr() + 1, ":/", 2) == 0)
            return NULL;
         break;
      case 4:
         if(buf == "//?/")
            return NULL;
         break;
#endif
      default:
         break;
   }

   // [CG] Remove trailing slashes
   for(dirname_size = buf.length() - 1;
       (dirname_size > 0 && buf.charAt(dirname_size) == '/');
       dirname_size--);

   // [CG] Find the position of the last slash.
   while((dirname_size > 0) && (buf.charAt(dirname_size) != '/'))
       dirname_size--;

   // [CG] If there were no more slashes then only a basename was given, so
   //      return NULL (basenames have no dirname by definition).
   if(!dirname_size)
      return estrdup("/");

   // [CG] Truncate buffer to last slash index.
   buf.truncate(dirname_size);

   return buf.duplicate(PU_STATIC);
}

const char* M_Basename(const char *path)
{
   const char *basename;

   if((basename = strrchr(path, '/')) == NULL)
      if((basename = strrchr(path, '\\')) == NULL)
         return path;

   return basename + 1;
}

bool M_RenamePath(const char *oldpath, const char *newpath)
{
   if(rename(oldpath, newpath))
   {
      set_error_code();
      return false;
   }
   return true;
}

//
// M_WriteFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
bool M_WriteFile(char const *name, void *source, unsigned int length)
{
   FILE *fp;
   bool result;

   if(!(fp = fopen(name, "wb")))         // Try opening file
   {
      set_error_code();
      return false;                      // Could not open file for writing
   }

   I_BeginRead();                       // Disk icon on
   result = (fwrite(source, 1, length, fp) == length);   // Write data
   fclose(fp);
   I_EndRead();                         // Disk icon off

   if(!result)                          // Remove partially written file
   {
      set_error_code();
      remove(name);
      return false;
   }

   return true;
}

//
// M_ReadFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
int M_ReadFile(char const *name, byte **buffer)
{
   FILE *fp;
   size_t length;
   int out = -1;

   if(!(fp = fopen(name, "rb")))
   {
      set_error_code();
      return false;
   }

   I_BeginRead();
   fseek(fp, 0, SEEK_END);
   length = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   *buffer = ecalloc(byte *, 1, length);

   if(fread(*buffer, 1, length, fp) != length)
      set_error_code();
   else
      out = length;

   fclose(fp);
   I_EndRead();
   // sf: do not quit on file not found
   //  I_Error("Couldn't read file %s: %s", name,
   //	  errno ? strerror(errno) : "(Unknown Error)");
   return out;
}

//
// M_FileLength
//
// Gets the length of a file given its handle.
// haleyjd 03/09/06: made global
// haleyjd 01/04/10: use fseek/ftell
//
uint32_t M_FileLength(FILE *f)
{
   long curpos, len;

   curpos = ftell(f);
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, curpos, SEEK_SET);

   return (uint32_t)len;
}

FILE* M_OpenFile(const char *path, const char *mode)
{
   FILE *f = fopen(path, mode);

   if(!f)
      set_error_code();

   return f;
}

size_t M_ReadFromFile(void *ptr, size_t size, size_t count, FILE *f)
{
   size_t result;

   I_BeginRead();
   result = fread(ptr, size, count, f);
   I_EndRead();

   if(result != count)
   {
      if(!feof(f))
         set_error_code();
   }

   return result;
}

size_t M_WriteToFile(const void *ptr, size_t size, size_t count, FILE *f)
{
   size_t result;

   I_BeginRead();
   result = fwrite(ptr, size, count, f);
   I_EndRead();

   if(result != count)
      set_error_code();

   return result;
}

long M_GetFilePosition(FILE *f)
{
   long result = ftell(f);

   if((result = ftell(f)) == -1)
      set_error_code();

   return result;
}

bool M_SeekFile(FILE *f, long int offset, int origin)
{
   if(fseek(f, offset, origin) != 0)
   {
      set_error_code();
      return false;
   }

   return true;
}

bool M_FlushFile(FILE *f)
{
   if(fflush(f) != 0)
   {
      set_error_code();
      return false;
   }

   return true;
}

bool M_CloseFile(FILE *f)
{
   if(fclose(f) != 0)
   {
      set_error_code();
      return false;
   }

   return true;
}

void M_ReportFileSystemError(void)
{
   set_error_code();
}

