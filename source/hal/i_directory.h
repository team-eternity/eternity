// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 David Hill et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//    Directory Manipulation
//
//-----------------------------------------------------------------------------

#ifndef I_DIRECTORY_H__
#define I_DIRECTORY_H__

#include "i_platform.h"
#include "../m_qstr.h"

class qstring;


bool I_CreateDirectory(qstring const &path);

const char *I_PlatformInstallDirectory();

void I_GetRealPath(const char *path, qstring &real);

#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
// TEMPORARY until we have std::filesystem in macos

#include <string>
#include <system_error>
#include "../m_collection.h"

//
// Filesystem. NOTE: everything is strictly for support with current use.
//
namespace fsStopgap
{
   //
   // Any path
   //
   class path : public ZoneObject
   {
      friend class directory_entry;
      friend class directory_iterator;
   public:
      path() = default;
      explicit path(const char *value) : mValue(value)
      {
      }
      path(const path &path) = default;

      path filename() const;
      path extension() const;
      std::string generic_u8string() const
      {
         return mValue.constPtr();
      }

      path operator / (const char *sub) const;
      bool empty() const
      {
         return mValue.empty();
      }

      bool operator == (const char *str) const
      {
         return mValue == str;
      }
   private:
      qstring mValue;
   };

   //
   // Directory path info
   //
   class directory_entry : public ZoneObject
   {
      friend class directory_iterator;
   public:
      directory_entry() = default;
      directory_entry(const char *path) : mPath(path)
      {
      }
      directory_entry(const path &path) : mPath(path)
      {
      }

      bool exists() const;
      bool is_directory() const;
      off_t file_size() const;

      const path &path() const
      {
         return mPath;
      }

   private:
      class path mPath; // the indicated path
   };

   //
   // Used for iterating inside
   //
   class directory_iterator : public ZoneObject
   {
   public:
      directory_iterator(const directory_entry &entry)
      {
         construct(entry);
      }
      directory_iterator(const path &path)
      {
         construct(path);
      }

      directory_entry *begin() const
      {
         return mEntries.begin();
      }
      directory_entry *end() const
      {
         return mEntries.end();
      }
   private:
      void construct(const directory_entry &entry);
      PODCollection<directory_entry> mEntries;
   };

   //
   // Quick shortcut
   //
   inline static bool is_directory(const char *path, std::error_code &ec)
   {
      return directory_entry(path).is_directory();
   }
   inline static bool exists(const path &path)
   {
      return directory_entry(path).exists();
   }
}

#endif

#endif

// EOF

