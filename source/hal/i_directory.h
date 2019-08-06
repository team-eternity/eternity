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

//
// Filesystem
//
namespace fsStopgap
{
   //
   // Directory path info
   //
   class directory_entry
   {
   public:
      directory_entry() = default;
      directory_entry(const char *path) : mPath(path)
      {
      }

      bool exists() const;
      bool is_directory() const;

   private:
      qstring mPath; // the indicated path
   };

   //
   // Used for iterating inside
   //
   class directory_iterator
   {
   public:
      directory_iterator(const directory_entry &entry) : mEntry(entry)
      {
      }

      directory_entry begin() const;
      directory_entry end() const;
   private:
      const directory_entry mEntry; // the directory through which to iterate
   };
}

#endif

#endif

// EOF

