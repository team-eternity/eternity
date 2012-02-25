// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// Zipfile routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#endif

#include "m_file.h"
#include "m_zip.h"

// [CG] Because of function pointer semantics, you can't pass a member function
//      as a function pointer unless the call expects a member function from
//      that class.  Therefore whenever ZipFile::addFolderRecursive is called,
//      it sets hack_zf to itself and calls a static non-member function.
//      Lame....
static ZipFile *hack_zf = NULL;

/* change_file_date : change the date/time of a file
    filename : the filename of the file where date/time must be modified
    dosdate : the new date at the MSDos format (4 bytes)
    tmu_date : the SAME new date at the tm_unz format */
static void M_setFileDate(const char *filename, uLong dosdate, tm_unz tmu_date)
{
#ifdef _WIN32
   HANDLE hFile;
   FILETIME ftm, ftLocal, ftCreate, ftLastAcc, ftLastWrite;

   hFile = CreateFileA(
      filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL
   );
   GetFileTime(hFile, &ftCreate, &ftLastAcc, &ftLastWrite);
   DosDateTimeToFileTime((WORD)(dosdate >> 16), (WORD)dosdate, &ftLocal);
   LocalFileTimeToFileTime(&ftLocal, &ftm);
   SetFileTime(hFile, &ftm, &ftLastAcc, &ftm);
   CloseHandle(hFile);
#else
#ifdef unix || __APPLE__
   struct utimbuf ut;
   struct tm newdate;

   if(tmu_date.tm_year > 1900)
      newdate.tm_year = tmu_date.tm_year - 1900;
   else
      newdate.tm_year = tmu_date.tm_year ;

   newdate.tm_mon = tmu_date.tm_mon;
   newdate.tm_mday = tmu_date.tm_mday;
   newdate.tm_hour = tmu_date.tm_hour;
   newdate.tm_min = tmu_date.tm_min;
   newdate.tm_sec = tmu_date.tm_sec;

   newdate.tm_isdst = -1;

   ut.actime = ut.modtime = mktime(&newdate);
   utime(filename,&ut);
#endif
#endif
}

static uLong M_getFileTime(const char *f, tm_zip *tmzip, uLong *dt)
{
   int ret = 0;
#ifdef _WIN32
   FILETIME ftLocal;
   HANDLE hFind;
   WIN32_FIND_DATAA ff32;

   hFind = FindFirstFileA(f, &ff32);
   if (hFind != INVALID_HANDLE_VALUE)
   {
      FileTimeToLocalFileTime(&(ff32.ftLastWriteTime), &ftLocal);
      FileTimeToDosDateTime(&ftLocal, ((LPWORD)dt) + 1, ((LPWORD)dt) + 0);
      FindClose(hFind);
      ret = 1;
   }
#else
#ifdef unix || __APPLE__
   struct stat s;
   struct tm* filedate;
   time_t tm_t = 0;

   if(strcmp(f, "-") != 0)
   {
      char name[MAX_ZIP_MEMBER_NAME_LENGTH + 1];
      int len = strlen(f);
      if(len > MAX_ZIP_MEMBER_NAME_LENGTH)
         len = MAX_ZIP_MEMBER_NAME_LENGTH;

      strncpy(name, f, MAX_ZIP_MEMBER_NAME_LENGTH - 1);
      // strncpy doesnt append the trailing NULL, of the string is too long.
      name[MAX_ZIP_MEMBER_NAME_LENGTH] = '\0';

      if(name[len - 1] == '/')
         name[len - 1] = '\0';
      // not all systems allow stat'ing a file with / appended
      if(stat(name, &s) == 0)
      {
         tm_t = s.st_mtime;
         ret = 1;
      }
   }

   filedate = localtime(&tm_t);

   tmzip->tm_sec  = filedate->tm_sec;
   tmzip->tm_min  = filedate->tm_min;
   tmzip->tm_hour = filedate->tm_hour;
   tmzip->tm_mday = filedate->tm_mday;
   tmzip->tm_mon  = filedate->tm_mon ;
   tmzip->tm_year = filedate->tm_year;
#endif
#endif
   return ret;
}

#ifdef _WIN32
static bool M_addFileToZipFile(const char *path)
{
   hack_zf->addFile(path);
   return true;
}
#else
static int M_addFileToZipFile(const char *path, const struct stat *stat_result,
                              int flags, struct FTW *walker)
{
   if(flags == FTW_DNR || flags == FTW_NS)
      errno = EACCES;

   if((flags == FTW_F || flags == FTW_SL) && (!hack_zf->addFile(path)))
      return -1;

   return 0;
}
#endif

void ZipFile::setError(int error_code)
{
   internal_error = error_code;
   if(error_code == errno_error)
      M_ReportFileSystemError();
}

void ZipFile::setZipError(int zip_error_code)
{
   if(zip_error_code != ZIP_ERRNO)
   {
      internal_error = zip_error;
      internal_zip_error = zip_error_code;
   }
   else
      setError(errno_error);
}

void ZipFile::setUnzipError(int unzip_error_code)
{
   if(unzip_error_code != UNZ_ERRNO)
   {
      internal_error = unzip_error;
      internal_unzip_error = unzip_error_code;
   }
   else
      setError(errno_error);
}

bool ZipFile::extractCurrentFileTo(const char *out_path)
{
   FILE *file;
   int error, bytes_read;
   unsigned int chunk_size = ZIP_CHUNK_SIZE;
   const char *basename;
   char *unarchived_pathname;
   unz_file_info64 ui;

   // [CG] Some checks are skipped here because it's assumed they're run in
   //      methods that call this.

   error = unzGetCurrentFileInfo64(
      uf, &ui, input_filename_buffer, MAX_ZIP_MEMBER_NAME_LENGTH,
      NULL, 0, NULL, 0
   );

   if(error != UNZ_OK)
   {
      setUnzipError(error);
      return false;
   }

   unarchived_pathname = input_filename_buffer;
   while(((*unarchived_pathname) == '/') || ((*unarchived_pathname) == '\\'))
      unarchived_pathname++;

   M_PathJoinBuf(
      out_path,
      unarchived_pathname,
      &output_filename_buffer,
      MAX_ZIP_MEMBER_NAME_LENGTH
   );

   if((basename = M_Basename(output_filename_buffer)))
   {
      if((*basename) == '\0')
      {
         M_CreateFolder(output_filename_buffer);
         return true;
      }
   }

   if(M_PathExists(output_filename_buffer))
   {
      setError(new_file_already_exists);
      return false;
   }

   if(!(file = M_OpenFile(output_filename_buffer, "wb")))
   {
      setError(fs_error);
      return false;
   }

   do
   {
      if((bytes_read = unzReadCurrentFile(uf, data_buffer, chunk_size)) < 0)
      {
         setUnzipError(bytes_read);
         M_CloseFile(file);
         return false;
      }

      if(M_WriteToFile(data_buffer, 1, bytes_read, file) != (size_t)bytes_read)
      {
         setError(fs_error);
         M_CloseFile(file);
         return false;
      }

   } while(bytes_read);

   M_setFileDate(unarchived_pathname, ui.dosDate, ui.tmu_date);

   if((error = unzCloseCurrentFile(uf)) != UNZ_OK)
   {
      setUnzipError(error);
      return false;
   }

   if(!M_CloseFile(file))
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool ZipFile::openForReading()
{
   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(!(uf = unzOpen64(path)))
   {
      setError(errno_error);
      return false;
   }

   return true;
}

bool ZipFile::createForWriting()
{
   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(M_PathExists(path))
   {
      setError(already_exists);
      return false;
   }

   if(!(zf = zipOpen64(path, create_new)))
   {
      setError(errno_error);
      return false;
   }

   return true;
}

bool ZipFile::openForWriting()
{
   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(!M_PathExists(path))
   {
      setError(does_not_exist);
      return false;
   }

   if(!M_IsFile(path))
   {
      setError(is_not_file);
      return false;
   }

   if(!(zf = zipOpen64(path, add_in_zip)))
   {
      setError(errno_error);
      return false;
   }

   return true;
}

bool ZipFile::addFile(const char *file_path)
{
   FILE *file;
   int error, bread;
   zip_fileinfo zi;
   const char *archived_pathname = file_path;

   if(mode != mode_writing)
   {
      setError(not_open_for_writing);
      return false;
   }

   if(!M_PathExists(file_path))
   {
      setError(new_file_does_not_exist);
      return false;
   }

   if(!M_IsFile(file_path))
   {
      setError(new_file_is_not_file);
      return false;
   }

   zi.tmz_date.tm_year = 0;
   zi.tmz_date.tm_mon  = 0;
   zi.tmz_date.tm_mday = 0;
   zi.tmz_date.tm_hour = 0;
   zi.tmz_date.tm_min  = 0;
   zi.tmz_date.tm_sec  = 0;
   zi.dosDate = 0;
   zi.internal_fa = 0;
   zi.external_fa = 0;

   M_getFileTime(file_path, &zi.tmz_date, &zi.dosDate);

   // [CG] Files with absolute paths are dangerous, so strip leading slashes.
   while(((*archived_pathname) == '\\') || ((*archived_pathname == '/')))
      archived_pathname++;

   error = zipOpenNewFileInZip64(
      zf,
      archived_pathname,
      &zi,
      NULL, 0, NULL, 0, NULL,
      Z_DEFLATED,
      current_compression_level,
      1
   );

   if(error != ZIP_OK)
   {
      setZipError(error);
      return false;
   }

   if(!(file = M_OpenFile(archived_pathname, "rb")))
   {
      setError(fs_error);
      return false;
   }

   while(!feof(file))
   {
      bread = (int)M_ReadFromFile(data_buffer, 1, ZIP_CHUNK_SIZE, file);
      if(bread > 0)
      {
         if(ferror(file))
         {
            setError(fs_error);
            M_CloseFile(file);
            return false;
         }

         if((error = zipWriteInFileInZip(zf, data_buffer, bread)) != ZIP_OK)
         {
            setZipError(error);
            M_CloseFile(file);
            return false;
         }
      }
   }

   if((error = zipCloseFileInZip(zf)) != ZIP_OK)
   {
      setZipError(error);
      return false;
   }

   if(!M_CloseFile(file))
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool ZipFile::addFile(const char *file_path, int compression_level)
{
   if((compression_level < 0) || (compression_level > 9))
   {
      setError(invalid_compression_level);
      return false;
   }

   current_compression_level = compression_level;
   return addFile(file_path);
}

bool ZipFile::addFolderRecursive(const char *folder_path)
{
   hack_zf = this;
   return M_WalkFiles(folder_path, M_addFileToZipFile);
}

bool ZipFile::addFolderRecursive(const char *folder_path,
                                 int compression_level)
{
   if((compression_level < 0) || (compression_level > 9))
   {
      setError(invalid_compression_level);
      return false;
   }

   current_compression_level = compression_level;
   return addFolderRecursive(folder_path);
}

bool ZipFile::extractAllTo(const char *out_path)
{
   int error;
   unz_global_info64 gi;
   uLong i;

   if(mode != mode_reading)
   {
      setError(not_open_for_reading);
      return false;
   }

   if(!M_PathExists(out_path))
   {
      setError(destination_does_not_exist);
      return false;
   }

   if(!M_IsFolder(out_path))
   {
      setError(destination_is_not_folder);
      return false;
   }

   if((error = unzGetGlobalInfo64(uf, &gi)) != UNZ_OK)
   {
      setUnzipError(error);
      return false;
   }

   if((error = unzGoToFirstFile(uf)) != UNZ_OK)
   {
      setUnzipError(error);
      return false;
   }

   for(i = 0; i < gi.number_entry; i++)
   {
      if(!extractCurrentFileTo(out_path))
         return false;

      if((error = unzGoToNextFile(uf)) != UNZ_OK)
      {
         setUnzipError(error);
         return false;
      }
   }

   return true;
}

bool ZipFile::extractFileTo(const char *member_name, const char *out_path)
{
   int error;

   if(mode != mode_reading)
   {
      setError(not_open_for_reading);
      return false;
   }

   if((error = unzLocateFile(uf, member_name, 1)) != UNZ_OK)
   {
      if(error == UNZ_END_OF_LIST_OF_FILE)
         setError(file_not_found);
      else
         setUnzipError(error);
      return false;
   }

   return extractCurrentFileTo(out_path);
}

bool ZipFile::close()
{
   int error;

   if(mode == mode_reading)
   {
      if((error = unzClose(uf)) != UNZ_OK)
      {
         setUnzipError(error);
         return false;
      }
   }
   else if(mode == mode_writing)
   {
      if((error = zipClose(zf, NULL)) != ZIP_OK)
      {
         setZipError(error);
         return false;
      }
   }

   return true;
}

bool ZipFile::iterateFilenames(char **buf)
{
   int error;
   unsigned int i;
   unz_global_info64 gi;
   unz_file_info64 ui;

   if(mode != mode_reading)
   {
      iterator_index = 0;
      setError(not_open_for_reading);
      return false;
   }

   if(iterator_index == 0)
   {
      if((error = unzGoToFirstFile(uf)) != UNZ_OK)
      {
         setUnzipError(error);
         return false;
      }
   }

   if((error = unzGetGlobalInfo64(uf, &gi)) != UNZ_OK)
   {
      iterator_index = 0;
      setUnzipError(error);
      return false;
   }

   if(iterator_index >= gi.number_entry)
   {
      iterator_index = 0;
      return false;
   }

   for(i = 0; i < iterator_index; i++)
   {
      if((error = unzGoToNextFile(uf)) != UNZ_OK)
      {
         iterator_index = 0;
         setUnzipError(error);
         return false;
      }
   }

   if((error = unzGetCurrentFileInfo64(
       uf, &ui, input_filename_buffer, MAX_ZIP_MEMBER_NAME_LENGTH,
       NULL, 0, NULL, 0)) != UNZ_OK)
   {
      iterator_index = 0;
      setUnzipError(error);
      return false;
   }

   *buf = input_filename_buffer;

   return true;
}

void ZipFile::resetFilenameIterator()
{
   iterator_index = 0;
}

bool ZipFile::hasError()
{
   if(internal_error != no_error)
      return true;

   return false;
}

void ZipFile::clearError()
{
   internal_error = internal_zip_error = internal_unzip_error = no_error;
}

const char* ZipFile::getError()
{
   if((internal_error == fs_error) || (internal_error == errno_error))
      return M_GetFileSystemErrorMessage();

   if(internal_error == zip_error)
   {
      if(zip_error == ZIP_BADZIPFILE)
         return "ZIP file is corrupt";

      if(zip_error == ZIP_INTERNALERROR)
         return "internal ZIP error";
   }

   if(internal_error == unzip_error)
   {
      if(unzip_error == UNZ_END_OF_LIST_OF_FILE)
         return "file not found in archive";

      if(unzip_error == UNZ_BADZIPFILE)
         return "ZIP file is corrupt";

      if(unzip_error == UNZ_INTERNALERROR)
         return "internal ZIP error";

      if(unzip_error == UNZ_CRCERROR)
         return "CRC check failed";
   }

   if(internal_error == already_open)
      return "ZIP file is already open";

   if(internal_error == already_exists)
      return "ZIP file already exists";

   if(internal_error == does_not_exist)
      return "ZIP file does not exist";

   if(internal_error == is_not_file)
      return "ZIP file is not a file";

   if(internal_error == not_open_for_reading)
      return "ZIP file is not open for reading";

   if(internal_error == not_open_for_writing)
      return "ZIP file is not open for writing";

   if(internal_error == destination_does_not_exist)
      return "destination does not exist";

   if(internal_error == destination_is_not_folder)
      return "destination is not a folder";

   if(internal_error == new_file_does_not_exist)
      return "new file does not exist";

   if(internal_error == new_file_is_not_file)
      return "new file is not a file";

   if(internal_error == new_file_already_exists)
      return "new file already exists";

   if(internal_error == file_not_found)
      return "file not found";

   if(internal_error == invalid_compression_level)
      return "invalid compression level";

   return "no error";
}

