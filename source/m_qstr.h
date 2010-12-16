// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
// Reallocating string structure
//
// What this "class" guarantees:
// * The string will always be null-terminated
// * Indexing functions always check array bounds
// * Insertion functions always reallocate when needed
//
// Of course, using QStrBuffer can negate these, so avoid it
// except for passing a char * to read op functions.
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef M_QSTR_H__
#define M_QSTR_H__

struct qstring_t
{
   char   *buffer;
   size_t  index;
   size_t  size;
};

#define qstring_npos ((size_t) -1)

//
// Basic Property Getters
//

//
// QStrBuffer
//
// Retrieves a pointer to the internal buffer. This pointer shouldn't be 
// cached, and is not meant for writing into (although it is safe to do so, it
// circumvents the encapsulation and security of this structure).
//
#define QStrBuffer(qstr) ((qstr)->buffer)

//
// QStrConstPtr
//
// Like QStrBuffer, but casts to const to enforce safety.
//
#define QStrConstPtr(qstr) ((const char *)((qstr)->buffer))

//
// QStrLen
//
// Because the validity of "index" is maintained by all insertion and editing
// functions, we can bypass calling strlen.
//
#define QStrLen(qstr) ((qstr)->index)

//
// QStrSize
//
// Returns the amount of size allocated for this qstring (will be >= strlen).
// You are allowed to index into the qstring up to size - 1, although any bytes
// beyond the strlen will be zero.
//
#define QStrSize(qstr) ((qstr)->size)

//
// "Methods"
//

qstring_t  *QStrInitCreate(qstring_t *qstr);
qstring_t  *QStrInitCreateSize(qstring_t *qstr, size_t size);
qstring_t  *QStrCreateSize(qstring_t *qstr, size_t size);
qstring_t  *QStrCreate(qstring_t *qstr);
qstring_t  *QStrGrow(qstring_t *qstr, size_t len);
qstring_t  *QStrClear(qstring_t *qstr);
qstring_t  *QStrClearOrCreate(qstring_t *qstr, size_t size);
void        QStrFree(qstring_t *qstr);
char        QStrCharAt(qstring_t *qstr, size_t idx);
char       *QStrBufferAt(qstring_t *qstr, size_t idx);
qstring_t  *QStrPutc(qstring_t *qstr, char ch);
qstring_t  *QStrDelc(qstring_t *qstr);
qstring_t  *QStrCat(qstring_t *qstr, const char *str);
qstring_t  *QStrQCat(qstring_t *dest, qstring_t *src);
qstring_t  *QStrInsert(qstring_t *dest, const char *insertstr, size_t pos);
int         QStrCmp(qstring_t *qstr, const char *str);
int         QStrNCmp(qstring_t *qstr, const char *str, size_t maxcount);
int         QStrCaseCmp(qstring_t *qstr, const char *str);
int         QStrNCaseCmp(qstring_t *qstr, const char *str, size_t maxcount);
qstring_t  *QStrCopy(qstring_t *qstr, const char *str);
char       *QStrCNCopy(char *dest, const qstring_t *src, size_t size);
qstring_t  *QStrQCopy(qstring_t *dest, const qstring_t *src);
qstring_t  *QStrUpr(qstring_t *qstr);
qstring_t  *QStrLwr(qstring_t *qstr);
size_t      QStrReplace(qstring_t *qstr, const char *filter, char repl);
size_t      QStrReplaceNotOf(qstring_t *qstr, const char *filter, char repl);
char       *QStrCDup(qstring_t *qstr, int tag);
char       *QStrCDupAuto(qstring_t *qstr);
int         QStrAtoi(qstring_t *qstr);
double      QStrToDouble(qstring_t *str, char **endptr);
const char *QStrChr(qstring_t *qstr, char c);
const char *QStrRChr(qstring_t *qstr, char c);
size_t      QStrFindFirstOfChar(qstring_t *qstr, char c);
size_t      QStrFindFirstNotOfChar(qstring_t *qstr, char c);
qstring_t  *QStrLStrip(qstring_t *qstr, char c);
qstring_t  *QStrRStrip(qstring_t *qstr, char c);
qstring_t  *QStrTruncate(qstring_t *qstr, size_t pos);
qstring_t  *QStrMakeQuoted(qstring_t *s);
int         QStrPrintf(qstring_t *qstr, size_t maxlen, const char *fmt, ...);

#endif

// EOF


