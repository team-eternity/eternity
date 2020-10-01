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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Directory Manipulation
//
//-----------------------------------------------------------------------------

#if _MSC_VER >= 1914
#include <locale>
#include <filesystem>
namespace fs = std::filesystem;
#include <string>
#endif

#include "../z_zone.h"

#include "i_directory.h"

#include "i_platform.h"
#include "../m_qstr.h"

//
// All this for PATH_MAX
//
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX \
 || EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX \
 || EE_CURRENT_PLATFORM == EE_PLATFORM_FREEBSD
#include <limits.h>
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
#include <windows.h>
#endif

//=============================================================================
//
// Global Functions
//

//
// I_CreateDirectory
//
bool I_CreateDirectory(const qstring &path)
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
   if(!mkdir(path.constPtr(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
      return true;
#endif

   return false;
}

//
// I_PlatformInstallDirectory
//
const char *I_PlatformInstallDirectory()
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
   struct stat sbuf;

   // Prefer /usr/local, but fall back to just /usr.
   if(!stat("/usr/local/share/eternity/base", &sbuf) && S_ISDIR(sbuf.st_mode))
      return "/usr/local/share/eternity/base";
   else
      return "/usr/share/eternity/base";
#endif

   return nullptr;
}

//
// Clears all symbolic links from a path (which may be relative) and returns the
// real path in "real"
//
void I_GetRealPath(const char *path, qstring &real)
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
   fs::path pathobj(path);
   pathobj = fs::canonical(pathobj);

   // Has to be converted since fs::value_type is wchar_t on Windows
   std::wstring wpath(pathobj.c_str());
   char *ret = ecalloc(char *, wpath.length() + 1, 1);
   WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, ret,
                       static_cast<int>(wpath.length()), nullptr, nullptr);
   real = ret;
   efree(ret);

   // wstring_convert became deprecated and didn't get replaced
   //std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
   //std::string spath(convertor.to_bytes(wpath));
   //real = spath.c_str();


#elif EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX \
   || EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX \
   || EE_CURRENT_PLATFORM == EE_PLATFORM_FREEBSD

   char result[PATH_MAX + 1];
   if(realpath(path, result))
      real = result;
   else
      real = path;   // failure

#else
#warning Unknown platform; this will merely copy "path" to "real"
   real = path;
#endif
}

#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
//==============================================================================
//
// MacOS FileSystem stopgap
//

#include <dirent.h>
#include <sys/types.h>

//
// Get the last file component
//
fsStopgap::path fsStopgap::path::filename() const
{
   qstring basename;
   mValue.extractFileBase(basename);
   return path(basename.constPtr());
}

fsStopgap::path fsStopgap::path::extension() const
{
   const char *start = strrchr(mValue.constPtr(), '.');
   return start ? path(start) : path();
}

//
// Fancy path concatenation
//
fsStopgap::path fsStopgap::path::operator / (const char *sub) const
{
   qstring newpath(mValue);
   newpath += '/';
   newpath += sub;
   return path(newpath.constPtr());
}

//
// True if path exists
//
bool fsStopgap::directory_entry::exists() const
{
   struct stat st = {};
   return !stat(mPath.mValue.constPtr(), &st);
}

//
// True if it's a directory
//
bool fsStopgap::directory_entry::is_directory() const
{
   struct stat st = {};
   int n = stat(mPath.mValue.constPtr(), &st);
   if(n)
      return false;
   return S_ISDIR(st.st_mode);
}

//
// Return file size
//
off_t fsStopgap::directory_entry::file_size() const
{
   struct stat st = {};
   int n = stat(mPath.mValue.constPtr(), &st);
   if(n)
      return 0;
   return st.st_size;
}

//
// Explores all items from folder
//
void fsStopgap::directory_iterator::construct(const directory_entry &entry)
{
   DIR *dir = opendir(entry.mPath.mValue.constPtr());
   if(!dir)
      return;  // failed
   dirent *en;
   while((en = readdir(dir)))
   {
      if(!strcmp(en->d_name, ".") || !strcmp(en->d_name, ".."))
         continue;
      qstring fullPath = entry.mPath.mValue;
      fullPath += '/';
      fullPath += en->d_name;
      mEntries.add(fullPath.constPtr());
   }
   closedir(dir);
}

#endif

// EOF

