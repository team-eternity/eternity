//
// The Eternity Engine
// Copyright (C) 2025 David Hill et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Directory manipulation.
// Authors: James Haley, David Hill, Ioan Chera, Max Waine, Joshua T. Woodie
//

#include "i_platform.h"

#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
#include <locale>
#include <filesystem>
namespace fs = std::filesystem;
#include <string>
#endif

#include "../z_zone.h"

#include "i_directory.h"

#include "../m_qstr.h"

//
// All this for PATH_MAX
//
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX || EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX || \
    EE_CURRENT_PLATFORM == EE_PLATFORM_FREEBSD
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
#ifdef BUILD_FLATPAK
    return "/app/share/eternity/base";
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
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
    fs::path pathobj(fs::u8path(path));
    pathobj = fs::canonical(pathobj);

    // Has to be converted since fs::value_type is wchar_t on Windows
    std::wstring wpath(pathobj.c_str());
    char        *ret = ecalloc(char *, wpath.length() + 1, 1);
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, ret, static_cast<int>(wpath.length()), nullptr, nullptr);
    real = ret;
    efree(ret);

    // wstring_convert became deprecated and didn't get replaced
    // std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    // std::string spath(convertor.to_bytes(wpath));
    // real = spath.c_str();

#elif EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX || EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX || \
    EE_CURRENT_PLATFORM == EE_PLATFORM_FREEBSD

    char result[PATH_MAX + 1];
    if(realpath(path, result))
        real = result;
    else
        real = path; // failure

#else
#warning "Unknown platform; this will merely copy \"path\" to \"real\""
    real = path;
#endif
}

FILE *I_fopen(const char *path, const char *mode)
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
    fs::path     pathObject = fs::u8path(path);
    std::wstring wmode;
    for(const char *m = mode; *m; ++m)
        wmode.push_back(static_cast<wchar_t>(*m));
    return _wfopen(pathObject.wstring().c_str(), wmode.c_str());
#else
    return fopen(path, mode);
#endif
}

int I_stat(const char *fileName, struct stat *statobj)
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
    fs::path fileNameObject = fs::u8path(fileName);
    return _wstat(fileNameObject.wstring().c_str(), reinterpret_cast<struct _stat *>(statobj));
#else
    return stat(fileName, statobj);
#endif
}

// EOF

