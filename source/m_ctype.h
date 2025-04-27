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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Replacement for standard ctype.h header.
// Authors: James Haley
//

#ifndef M_CTYPE_H__
#define M_CTYPE_H__

//
// Namespace for Eternity Engine character type functions.
//
namespace ectype
{
// Return 1 if c is a control character, 0 otherwise.
inline int isCntrl(int c)
{
    return (c >= 0x00 && c <= 0x1f);
}

// Return 1 if c is a blank character, 0 otherwise.
inline int isBlank(int c)
{
    return (c == '\t' || c == ' ');
}

// Return 1 if c is a whitespace character, 0 otherwise.
inline int isSpace(int c)
{
    return ((c >= '\t' && c <= '\r') || c == ' ');
}

// Return 1 if c is an uppercase alphabetic character, 0 otherwise.
inline int isUpper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

// Return 1 if c is a lowercase alphabetic character, 0 otherwise.
inline int isLower(int c)
{
    return (c >= 'a' && c <= 'z');
}

// Return 1 if c is an alphabetic character, 0 otherwise.
inline int isAlpha(int c)
{
    return (isUpper(c) || isLower(c));
}

// Return 1 if c is a base 10 numeral, 0 otherwise.
inline int isDigit(int c)
{
    return (c >= '0' && c <= '9');
}

// Return 1 if c is a base 16 numeral, 0 otherwise.
inline int isXDigit(int c)
{
    return (isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'));
}

// Return 1 if c is either a base 10 digit or an alphabetic character.
// Return 0 otherwise.
inline int isAlnum(int c)
{
    return (isAlpha(c) || isDigit(c));
}

// Return 1 if c is a punctuation mark, 0 otherwise.
inline int isPunct(int c)
{
    return ((c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~'));
}

// Return 1 if c is a character with a graphical glyph.
// Return 0 otherwise.
inline int isGraph(int c)
{
    return (c >= '!' && c <= '~');
}

// Return 1 if c is a printable character, 0 otherwise.
inline int isPrint(int c)
{
    return (c >= ' ' && c <= '~');
}

// Eternity-specific; return 1 if c is an extended ASCII character,
// and 0 otherwise.
inline int isExtended(int c)
{
    return (c < 0 || c > 0x7f);
}

// Convert lowercase alphabetics to uppercase; leave other inputs alone.
inline int toUpper(int c)
{
    return (isLower(c) ? c - 'a' + 'A' : c);
}

// Convert uppercase alphabetics to lowercase; leave other inputs alone.
inline int toLower(int c)
{
    return (isUpper(c) ? c - 'A' + 'a' : c);
}
} // namespace ectype

#endif

// EOF

