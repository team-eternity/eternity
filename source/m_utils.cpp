//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Utility functions
// Authors: James Haley et al.
//

#include <assert.h>
#include "z_zone.h"
#include "hal/i_platform.h"
#include "doomtype.h"

#include "d_main.h"   // for usermsg
#include "i_system.h" // for I_Error
#include "m_qstr.h"   // for qstring

//=============================================================================
//
// File IO Routines
//

//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
bool M_WriteFile(char const *name, void *source, size_t length)
{
    FILE *fp;
    bool  result;

    errno = 0;

    if(!(fp = fopen(name, "wb"))) // Try opening file
        return 0;                 // Could not open file for writing

    result = (fwrite(source, 1, length, fp) == length); // Write data
    fclose(fp);

    if(!result) // Remove partially written file
        remove(name);

    return result;
}

//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
int M_ReadFile(char const *name, byte **buffer)
{
    FILE *fp;

    errno = 0;

    if((fp = fopen(name, "rb")))
    {
        size_t length;

        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        *buffer = ecalloc(byte *, 1, length);

        if(fread(*buffer, 1, length, fp) == length)
        {
            fclose(fp);
            return static_cast<int>(length);
        }
        fclose(fp);
    }

    // sf: do not quit on file not found
    //  I_Error("Couldn't read file %s: %s", name,
    //	  errno ? strerror(errno) : "(Unknown Error)");

    return -1;
}

// Returns the path to a temporary file of the given name, stored
// inside the system temporary directory.
//
// The returned value must be freed with Z_Free after use.
char *M_TempFile(const char *s)
{
    const char *tempdir;

#ifdef _WIN32

    // Check the TEMP environment variable to find the location.

    tempdir = getenv("TEMP");

    if(tempdir == NULL)
    {
        tempdir = ".";
    }
#else
    // In Unix, just use /tmp.

    tempdir = "/tmp";
#endif

    qstring temp(tempdir);
    temp.pathConcatenate(s);

    return temp.duplicate();
}

//
// Gets the length of a file given its handle.
//
long M_FileLength(FILE *f)
{
    long curpos, len;

    curpos = ftell(f);
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, curpos, SEEK_SET);

    return len;
}

//
// haleyjd 09/02/12: Like M_ReadFile, but assumes the contents are a string
// and therefore null terminates the buffer.
//
char *M_LoadStringFromFile(const char *filename)
{
    FILE  *f   = nullptr;
    char  *buf = nullptr;
    size_t len = 0;

    if(!(f = fopen(filename, "rb")))
        return nullptr;

    // allocate at length + 1 for null termination
    len = static_cast<size_t>(M_FileLength(f));
    buf = ecalloc(char *, 1, len + 1);
    if(fread(buf, 1, len, f) != len)
        usermsg("M_LoadStringFromFile: warning: could not read file %s\n", filename);
    fclose(f);

    return buf;
}

//=============================================================================
//
// Portable non-standard libc functions and misc string operations
//

//
// strnlen not available on all platforms.. maybe autoconf it?
//
size_t M_Strnlen(const char *s, size_t count)
{
    const char *p = s;
    while(*p && count-- > 0)
        p++;

    return p - s;
}

//
// haleyjd: portable strupr function
//
char *M_Strupr(char *string)
{
    char *s = string;

    while(*s)
    {
        int c = ectype::toUpper(*s);
        *s++  = c;
    }

    return string;
}

//
// haleyjd: portable strlwr function
//
char *M_Strlwr(char *string)
{
    char *s = string;

    while(*s)
    {
        int c = ectype::toLower(*s);
        *s++  = c;
    }

    return string;
}

//
// haleyjd: portable itoa, from DJGPP libc
// Copyright (C) 2001 DJ Delorie, see COPYING.DJ for details
//
char *M_Itoa(int value, char *string, int radix)
{
#ifdef EE_HAVE_ITOA
    return ITOA_NAME(value, string, radix);
#else
    char         tmp[33];
    char        *tp = tmp;
    int          i;
    unsigned int v;
    int          sign;
    char        *sp;

    if(radix > 36 || radix <= 1)
    {
        errno = EDOM;
        return 0;
    }

    sign = (radix == 10 && value < 0);

    if(sign)
        v = -value;
    else
        v = (unsigned int)value;

    while(v || tp == tmp)
    {
        i = v % radix;
        v = v / radix;

        if(i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    if(string == 0)
        string = (char *)(malloc)((tp - tmp) + sign + 1);
    sp = string;

    if(sign)
        *sp++ = '-';

    while(tp > tmp)
        *sp++ = *--tp;
    *sp = 0;

    return string;
#endif
}

//
// Counts the number of lines in a string. If the string length is greater than
// 0, we consider the string to have at least one line.
//
int M_CountNumLines(const char *str)
{
    const char *rover    = str;
    int         numlines = 0;

    if(strlen(str))
    {
        numlines = 1;

        char c;
        while((c = *rover++))
        {
            if(c == '\n')
                ++numlines;
        }
    }

    return numlines;
}

//=============================================================================
//
// Filename and Path Routines
//

//
// haleyjd: General file path extraction. Strips a path+filename down to only
// the path component.
//
void M_GetFilePath(const char *fn, char *base, size_t len)
{
    bool  found_slash = false;
    char *p;

    memset(base, 0, len);

    p = base + len - 1;

    strncpy(base, fn, len);

    while(p >= base)
    {
        if(*p == '/' || *p == '\\')
        {
            found_slash = true; // mark that the path ended with a slash
            *p          = '\0';
            break;
        }
        *p = '\0';
        p--;
    }

    // haleyjd: in the case that no slash was ever found, yet the
    // path string is empty, we are dealing with a file local to the
    // working directory. The proper path to return for such a string is
    // not "", but ".", since the format strings add a slash now. When
    // the string is empty but a slash WAS found, we really do want to
    // return the empty string, since the path is relative to the root.
    if(!found_slash && *base == '\0')
        *base = '.';
}

//
// Extract an up-to-eight-character filename out of a full file path
// (relative or absolute), removing the path components and extensions.
// This is not a general filename extraction routine and should only
// be used for generating WAD lump names from files.
//
// haleyjd 04/17/11: Added support for truncation of LFNs courtesy of
// Choco Doom. Thanks, fraggle ;)
//
void M_ExtractFileBase(const char *path, char *dest)
{
    const char *src = path + strlen(path) - 1;
    int         length;

    // back up until a \ or the start
    while(src != path && src[-1] != ':' // killough 3/22/98: allow c:filename
          && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    // copy up to eight characters
    // FIXME: insecure, does not ensure null termination of output string!
    memset(dest, 0, 8);
    length = 0;

    while(*src && *src != '.')
    {
        if(length >= 8)
            break; // stop at 8

        dest[length++] = ectype::toUpper(*src++);
    }
}

//
// 1/18/98 killough: adds a default extension to a path
// Note: Backslashes are treated specially, for MS-DOS.
//
// Warning: the string passed here *MUST* have room for an
// extension to be added to it! -haleyjd
//
char *M_AddDefaultExtension(char *path, const char *ext)
{
    char *p = path;
    while(*p++)
        ;
    while(p-- > path && *p != '/' && *p != '\\')
        if(*p == '.')
            return path;
    if(*ext != '.')
        strcat(path, ".");
    return strcat(path, ext);
}

//
// Remove trailing slashes, translate backslashes to slashes
// The string to normalize is passed and returned in str
//
void M_NormalizeSlashes(char *str)
{
    char *p;
    char  useSlash     = '/';  // The slash type to use for normalization.
    char  replaceSlash = '\\'; // The kind of slash to replace.
    bool  isUNC        = false;

    if(ee_current_platform == EE_PLATFORM_WINDOWS)
    {
        // This is an UNC path; it should use backslashes.
        // NB: We check for both in the event one was changed earlier by mistake.
        if(strlen(str) > 2 && ((str[0] == '\\' || str[0] == '/') && str[0] == str[1]))
        {
            useSlash     = '\\';
            replaceSlash = '/';
            isUNC        = true;
        }
    }

    // Convert all replaceSlashes to useSlashes
    for(p = str; *p; p++)
    {
        if(*p == replaceSlash)
            *p = useSlash;
    }

    // Remove trailing slashes
    while(p > str && *--p == useSlash)
        *p = 0;

    // Collapse multiple slashes
    for(p = str + (isUNC ? 2 : 0); (*str++ = *p);)
        if(*p++ == useSlash)
            while(*p == useSlash)
                p++;
}

//
// haleyjd: This routine takes any number of strings and a number of extra
// characters, calculates their combined length, and calls Z_Alloca to create
// a temporary buffer of that size. This is extremely useful for allocation of
// file paths, and is used extensively in d_main.c.  The pointer returned is
// to a temporary Z_Alloca buffer, which lives until the next main loop
// iteration, so don't cache it. Note that this idiom is not possible with the
// normal non-standard alloca function, which allocates stack space.
//
int M_StringAlloca(char **str, int numstrs, size_t extra, const char *str1, ...)
{
    size_t len = extra;

    if(numstrs < 1)
        I_Error("M_StringAlloca: invalid input\n");

    len += strlen(str1);

    --numstrs;

    if(numstrs != 0)
    {
        va_list args;
        va_start(args, str1);

        while(numstrs != 0)
        {
            const char *argstr = va_arg(args, const char *);

            len += strlen(argstr);

            --numstrs;
        }

        va_end(args);
    }

    ++len;

    *str = static_cast<char *>(Z_Alloca(len));

    return static_cast<int>(len);
}

//
// haleyjd 09/10/11: back-adapted from Chocolate Strife to provide secure
// file path concatenation with automatic normalization on alloca-provided
// buffers.
//
char *M_SafeFilePath(const char *pbasepath, const char *newcomponent)
{
    int   newstrlen = 0;
    char *newstr    = nullptr;

    newstrlen = M_StringAlloca(&newstr, 3, 1, pbasepath, "/", newcomponent);
    psnprintf(newstr, newstrlen, "%s/%s", pbasepath, newcomponent);
    M_NormalizeSlashes(newstr);

    return newstr;
}

//
// Modulo which adjusts to have positive result always. Needed because we use it throughout the code
// for hash tables
//
int M_PositiveModulo(int op1, int op2)
{
    int result = op1 % op2;
    return result < 0 ? result + abs(op2) : result;
}

//
// Check if the map name follows the ExMy pattern (Doom 1, Heretic)
//
bool M_IsExMy(const char *name, int *episode, int *map)
{
    assert(name);
    int state = 0;
    int val = 0, mal = 0;
    for(int i = 0; i < 8; ++i)
    {
        char c = name[i];
        switch(state)
        {
        case 0: // expect E
            if(c != 'E')
                return false;
            state = 1;
            break;
        case 1: // expect digit
            if(c < '0' || c > '9')
                return false;
            state = 2;
            val   = c - '0';
            break;
        case 2: // expect digit or M
            if(c == 'M')
            {
                state = 3;
                mal   = 0;
                break;
            }
            if(c < '0' || c > '9')
                return false;
            val = 10 * val + c - '0';
            break;
        case 3: // expect level digit
            if(c < '0' || c > '9')
                return false;
            state = 4;
            mal   = c - '0';
            break;
        case 4: // more digits or exit
            if(c == '\0')
                break;
            if(c < '0' || c > '9')
                return false;
            mal = 10 * mal + c - '0';
            break;
        }
        if(!c)
            break;
    }
    if(state == 4)
    {
        if(episode)
            *episode = val;
        if(map)
            *map = mal;
        return true;
    }
    return false;
}

//
// Check if Doom 2, Hexen or Strife map format
//
bool M_IsMAPxy(const char *name, int *map)
{
    assert(name);
    if(name[0] != 'M' || name[1] != 'A' || name[2] != 'P' || !isnumchar(name[3]) || !isnumchar(name[4]))
    {
        return false;
    }
    for(int i = 5; i < 8; ++i)
    {
        if(!name[i])
            break;
        if(name[i] < '0' || name[i] > '9')
            return false;
    }
    if(map)
        *map = atoi(name + 3);
    return true;
}

// EOF

