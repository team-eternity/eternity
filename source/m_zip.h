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

#ifndef M_ZIP_H__
#define M_ZIP_H__

#ifdef ZLIB_WINAPI
#undef ZLIB_WINAPI
#endif

#include <zip.h>
#include <unzip.h>

#define ZIP_CHUNK_SIZE 16384
#define MAX_ZIP_MEMBER_NAME_LENGTH 512

class ZipFile : public ZoneObject
{
private:
   const static int mode_none    = 0;
   const static int mode_writing = 1;
   const static int mode_reading = 2;

   const static int create_new   = APPEND_STATUS_CREATE;
   const static int create_after = APPEND_STATUS_CREATEAFTER;
   const static int add_in_zip   = APPEND_STATUS_ADDINZIP;

   const static int no_error                   =  0;
   const static int fs_error                   =  1;
   const static int errno_error                =  2;
   const static int zip_error                  =  3;
   const static int unzip_error                =  4;
   const static int already_open               =  5;
   const static int already_exists             =  6;
   const static int does_not_exist             =  7;
   const static int is_not_file                =  8;
   const static int not_open_for_reading       =  9;
   const static int not_open_for_writing       = 10;
   const static int destination_does_not_exist = 11;
   const static int destination_is_not_folder  = 12;
   const static int new_file_does_not_exist    = 13;
   const static int new_file_is_not_file       = 14;
   const static int new_file_already_exists    = 15;
   const static int file_not_found             = 16;
   const static int invalid_compression_level  = 17;

   const static int default_compression_level  = 4;

   char         *path;
   int           mode;
   int           internal_error;
   int           internal_zip_error;
   int           internal_unzip_error;
   unzFile       uf;
   zipFile       zf;
   char         *data_buffer;
   char         *input_filename_buffer;
   char         *output_filename_buffer;
   unsigned int  iterator_index;
   int           current_compression_level;
   const char   *current_recursive_folder;

   void setError(int error_code);
   void setZipError(int zip_error_code);
   void setUnzipError(int unzip_error_code);
   bool extractCurrentFileTo(const char *out_path);

public:
   ZipFile(const char *new_path)
      : ZoneObject(), mode(mode_none), internal_error(0),
        internal_zip_error(0), internal_unzip_error(0), iterator_index(0),
        current_compression_level(default_compression_level),
        current_recursive_folder(NULL)
   {
      path = estrdup(new_path);
      data_buffer = emalloc(char *, ZIP_CHUNK_SIZE);
      input_filename_buffer = emalloc(char *, MAX_ZIP_MEMBER_NAME_LENGTH);
      output_filename_buffer = emalloc(char *, MAX_ZIP_MEMBER_NAME_LENGTH);
   }

   ~ZipFile()
   {
      close();

      if(path)
         efree(path);

      if(data_buffer)
         efree(data_buffer);

      if(input_filename_buffer)
         efree(input_filename_buffer);

      if(output_filename_buffer)
         efree(output_filename_buffer);
   }

   bool        openForReading();
   bool        createForWriting();
   bool        openForWriting();
   bool        addFile(const char *file_path);
   bool        addFile(const char *file_path, int compression_level);
   bool        addFolderRecursive(const char *folder_path);
   bool        addFolderRecursive(const char *folder_path,
                                  int compression_level);
   bool        extractAllTo(const char *out_path);
   bool        extractFileTo(const char *member_name, const char *out_path);
   bool        close();
   bool        iterateFilenames(char **buf);
   void        resetFilenameIterator();
   bool        hasError();
   void        clearError();
   const char *getError();
};

#endif

