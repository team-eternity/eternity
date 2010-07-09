// Emacs style mode select   -*- C -*-
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

typedef struct qstring_s
{
   char *buffer;
   unsigned int index;
   unsigned int size;
} qstring_t;

qstring_t    *QStrInitCreate(qstring_t *qstr);
qstring_t    *QStrCreateSize(qstring_t *qstr, unsigned int size);
qstring_t    *QStrCreate(qstring_t *qstr);
unsigned int  QStrLen(qstring_t *qstr);
unsigned int  QStrSize(qstring_t *qstr);
char         *QStrBuffer(qstring_t *qstr);
qstring_t    *QStrGrow(qstring_t *qstr, unsigned int len);
qstring_t    *QStrClear(qstring_t *qstr);
void          QStrFree(qstring_t *qstr);
char          QStrCharAt(qstring_t *qstr, unsigned int idx);
qstring_t    *QStrPutc(qstring_t *qstr, char ch);
qstring_t    *QStrDelc(qstring_t *qstr);
qstring_t    *QStrCat(qstring_t *qstr, const char *str);
int           QStrCmp(qstring_t *qstr, const char *str);
int           QStrCaseCmp(qstring_t *qstr, const char *str);
qstring_t    *QStrCpy(qstring_t *qstr, const char *str);
qstring_t    *QStrCopyTo(qstring_t *dest, const qstring_t *src);
qstring_t    *QStrUpr(qstring_t *qstr);
qstring_t    *QStrLwr(qstring_t *qstr);
unsigned int  QStrReplace(qstring_t *qstr, const char *filter, char repl);
unsigned int  QStrReplaceNotOf(qstring_t *qstr, const char *filter, char repl);
char         *QStrCDup(qstring_t *qstr, int tag);
int           QStrAtoi(qstring_t *qstr);
const char   *QStrChr(qstring_t *qstr, char c);
const char   *QStrRChr(qstring_t *qstr, char c);
qstring_t    *QStrLStrip(qstring_t *qstr, char c);
qstring_t    *QStrRStrip(qstring_t *qstr, char c);
int           QStrPrintf(qstring_t *qstr, unsigned int maxlen, const char *fmt, ...);

#endif

// EOF


