// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 James Haley
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

#include "z_zone.h"
#include "i_system.h"
#include "m_qstr.h"
#include "m_misc.h"   // for M_Strupr/M_Strlwr
#include "d_io.h"     // for strcasecmp

//=============================================================================
//
// Constructors, Destructors, and Reinitializers
//

// 32 bytes is the minimum block size currently used by the zone 
// allocator, so it makes sense to use it as the base default 
// string size too.

#define QSTR_BASESIZE 32

//
// QStrCreateSize
//
// Creates a qstring with a given initial size, which helps prevent
// unnecessary initial reallocations. Resets insertion point to zero.
// This is safe to call on an existing qstring to reinitialize it.
//
qstring_t *QStrCreateSize(qstring_t *qstr, size_t size)
{
   qstr->buffer = (char *)(realloc(qstr->buffer, size));
   qstr->size   = size;
   qstr->index  = 0;
   memset(qstr->buffer, 0, size);

   return qstr;
}

//
// QStrCreate
//
// Gives the qstring a buffer of the default size and initializes
// it to zero. Resets insertion point to zero. This is safe to call
// on an existing qstring to reinitialize it.
//
qstring_t *QStrCreate(qstring_t *qstr)
{
   return QStrCreateSize(qstr, QSTR_BASESIZE);
}

//
// QStrInitCreate
//
// Initializes a qstring struct to all zeroes, then calls
// QStrCreate. This is for safety the first time a qstring
// is created (if qstr->buffer is uninitialized, realloc will
// crash).
//
qstring_t *QStrInitCreate(qstring_t *qstr)
{
   memset(qstr, 0, sizeof(*qstr));

   return QStrCreate(qstr);
}

//
// QStrInitCreateSize
//
// Initialization and creation with size specification.
//
qstring_t *QStrInitCreateSize(qstring_t *qstr, size_t size)
{
   memset(qstr, 0, sizeof(*qstr));

   return QStrCreateSize(qstr, size);
}

//
// QStrFree
//
// Frees the qstring object. It should not be used after this,
// unless QStrCreate is called on it. You don't have to free
// a qstring before recreating it, however, since it uses realloc.
//
void QStrFree(qstring_t *qstr)
{
   if(qstr->buffer)
      free(qstr->buffer);
   qstr->buffer = NULL;
   qstr->index = qstr->size = 0;
}

//=============================================================================
//
// Basic Properties
//

//
// QStrCharAt
//
// Indexing function to access a character in a qstring. This is slower but 
// more secure than using QStrBuffer with array indexing.
//
char QStrCharAt(qstring_t *qstr, size_t idx)
{
   if(idx >= qstr->size)
      I_Error("QStrCharAt: index out of range\n");

   return qstr->buffer[idx];
}

//
// QStrBufferAt
//
// Gets a pointer into the buffer at the given position, if that position would
// be valid. Returns NULL otherwise. The same caveats apply as with QStrBuffer.
//
char *QStrBufferAt(qstring_t *qstr, size_t idx)
{
   return idx < qstr->size ? qstr->buffer + idx : NULL;
}

//=============================================================================
//
// Basic String Ops
//

//
// QStrClear
//
// Sets the entire qstring buffer to zero, and resets the insertion index. 
// Does not reallocate the buffer.
//
qstring_t *QStrClear(qstring_t *qstr)
{
   memset(qstr->buffer, 0, qstr->size);
   qstr->index = 0;

   return qstr;
}

//
// QStrClearOrCreate
//
// Creates a qstring, or clears it if it is already valid.
//
qstring_t *QStrClearOrCreate(qstring_t *qstr, size_t size)
{
   if(!qstr->buffer)
      QStrCreateSize(qstr, size);
   else
      QStrClear(qstr);

   return qstr;
}

//
// QStrGrow
//
// Grows the qstring's buffer by the indicated amount. This is automatically
// called by other qstring methods, so there is generally no need to call it 
// yourself.
//
qstring_t *QStrGrow(qstring_t *qstr, size_t len)
{   
   size_t newsize = qstr->size + len;

   qstr->buffer = (char *)(realloc(qstr->buffer, newsize));
   memset(qstr->buffer + qstr->size, 0, len);
   qstr->size += len;
   
   return qstr;
}

//=============================================================================
// 
// Concatenation and Insertion/Deletion/Copying Functions
//

//
// QStrPutc
//
// Adds a character to the end of the qstring, reallocating via buffer doubling
// if necessary.
//
qstring_t *QStrPutc(qstring_t *qstr, char ch)
{
   if(qstr->index >= qstr->size - 1) // leave room for \0
      QStrGrow(qstr, qstr->size);    // double buffer size

   qstr->buffer[qstr->index++] = ch;

   return qstr;
}

//
// QStrDelc
//
// Deletes a character from the end of the qstring.
//
qstring_t *QStrDelc(qstring_t *qstr)
{
   if(qstr->index > 0)
      qstr->index--;

   qstr->buffer[qstr->index] = '\0';

   return qstr;
}

//
// QStrCat
//
// Concatenates a C string onto the end of a qstring, expanding the buffer if
// necessary.
//
qstring_t *QStrCat(qstring_t *qstr, const char *str)
{
   unsigned int cursize = qstr->size;
   unsigned int newsize = qstr->index + strlen(str) + 1;

   if(newsize >= cursize)
      QStrGrow(qstr, newsize - cursize);

   strcat(qstr->buffer, str);

   qstr->index = strlen(qstr->buffer);

   return qstr;
}

//
// QStrQCat
//
// Concatenates a qstring to a qstring. Convenience routine.
//
qstring_t *QStrQCat(qstring_t *dest, qstring_t *src)
{
   return QStrCat(dest, src->buffer);
}

//
// QStrInsert
//
// Inserts a string at a given position in the qstring. If the
// position is outside the range of the string, an error will occur.
//
qstring_t *QStrInsert(qstring_t *dest, const char *insertstr, size_t pos)
{
   char *insertpoint;
   size_t charstomove;
   size_t insertstrlen = strlen(insertstr);
   size_t totalsize = dest->index + insertstrlen + 1;

   // pos must be between 0 and dest->index - 1
   if(pos >= dest->index)
      I_Error("QStrInsert: position out of range\n");

   // grow the buffer to hold the resulting string if necessary
   if(totalsize >= dest->size)
      QStrGrow(dest, totalsize - dest->size);

   insertpoint = dest->buffer + pos;
   charstomove = dest->index  - pos;

   // use memmove for absolute safety
   memmove(insertpoint + insertstrlen, insertpoint, charstomove);
   memmove(insertpoint, insertstr, insertstrlen);

   dest->index = strlen(dest->buffer);

   return dest;
}

//
// QStrCopy
//
// Copies a C string into the qstring. The qstring is cleared first,
// and then set to the contents of *str.
//
qstring_t *QStrCopy(qstring_t *qstr, const char *str)
{
   QStrClear(qstr);
   
   return QStrCat(qstr, str);
}

//
// QStrCNCopy
//
// Copies the qstring into a C string buffer.
//
char *QStrCNCopy(char *dest, const qstring_t *src, size_t size)
{
   return strncpy(dest, src->buffer, size);
}

//
// QStrQCopy
//
// Copies one qstring into another.
//
qstring_t *QStrQCopy(qstring_t *dest, const qstring_t *src)
{
   QStrClear(dest);
   
   return QStrCat(dest, src->buffer);
}

//
// QStrSwap
//
// Exchanges the contents of two qstrings.
//
void QStrSwap(qstring_t *str1, qstring_t *str2)
{
   qstring_t temp = *str1; // make a shallow copy of string 1

   *str1 = *str2;
   *str2 = temp;
}

//
// QStrLStrip
//
// Removes occurrences of a specified character at the beginning of a qstring.
//
qstring_t *QStrLStrip(qstring_t *qstr, char c)
{
   size_t i = 0;
   size_t len = qstr->index;

   while(qstr->buffer[i] != '\0' && qstr->buffer[i] == c)
      ++i;

   if(i)
   {
      if((len -= i) == 0)
         QStrClear(qstr);
      else
      {
         memmove(qstr->buffer, qstr->buffer + i, len);
         memset(qstr->buffer + len, 0, qstr->size - len);
         qstr->index -= i;
      }
   }

   return qstr;
}

//
// QStrRStrip
//
// Removes occurrences of a specified character at the end of a qstring.
//
qstring_t *QStrRStrip(qstring_t *qstr, char c)
{
   while(qstr->index && qstr->buffer[qstr->index - 1] == c)
      QStrDelc(qstr);

   return qstr;
}

//
// QStrTruncate
//
// Truncates the qstring to the indicated position.
//
qstring_t *QStrTruncate(qstring_t *qstr, size_t pos)
{
   // pos must be between 0 and qstr->index - 1
   if(pos >= qstr->index)
      I_Error("QStrTruncate: position out of range\n");

   memset(qstr->buffer + pos, 0, qstr->index - pos);
   qstr->index = pos;

   return qstr;
}

//=============================================================================
//
// Comparison Functions
//

//
// QStrCmp
//
// String compare.
//
int QStrCmp(qstring_t *qstr, const char *str)
{
   return strcmp(qstr->buffer, str);
}

//
// QStrNCmp
//
// String compare with length limitation.
//
int QStrNCmp(qstring_t *qstr, const char *str, size_t maxcount)
{
   return strncmp(qstr->buffer, str, maxcount);
}

//
// QStrCaseCmp
//
// Case-insensitive string compare.
//
int QStrCaseCmp(qstring_t *qstr, const char *str)
{
   return strcasecmp(qstr->buffer, str);
}

//
// QStrNCaseCmp
//
// Case-insensitive compare with length limitation.
//
int QStrNCaseCmp(qstring_t *qstr, const char *str, size_t maxcount)
{
   return strncasecmp(qstr->buffer, str, maxcount);
}

//=============================================================================
//
// Search Functions
//

//
// QStrChr
//
// Calls strchr on the qstring ("find first of", C-style).
//
const char *QStrChr(qstring_t *qstr, char c)
{
   return strchr(qstr->buffer, c);
}

//
// QStrRChr
//
// Calls strrchr on the qstring ("find last of", C-style)
//
const char *QStrRChr(qstring_t *qstr, char c)
{
   return strrchr(qstr->buffer, c);
}

//
// QStrFindFirstOfChar
//
// Finds the first occurance of a character in the qstring and returns its 
// position. Returns qstring_npos if not found.
//
size_t QStrFindFirstOfChar(qstring_t *qstr, char c)
{
   const char *rover = qstr->buffer;
   boolean found = false;

   while(*rover)
   {
      if(*rover == c)
      {
         found = true;
         break;
      }
      ++rover;
   }

   return found ? rover - qstr->buffer : qstring_npos;
}

//
// QStrFindFirstNotOfChar
//
// Finds the first occurance of a character in the qstring which does not
// match the provided character. Returns qstring_npos if not found.
//
size_t QStrFindFirstNotOfChar(qstring_t *qstr, char c)
{
   const char *rover = qstr->buffer;
   boolean found = false;

   while(*rover)
   {
      if(*rover != c)
      {
         found = true;
         break;
      }
      ++rover;
   }

   return found ? rover - qstr->buffer : qstring_npos;
}

//=============================================================================
//
// Conversion Functions
//

//
// Integers
//

//
// QStrAtoi
//
// Returns the qstring converted to an integer via atoi.
//
int QStrAtoi(qstring_t *qstr)
{
   return atoi(qstr->buffer);
}

//
// Floating Point
//

//
// QStrToDouble
//
// Calls strtod on the qstring.

double QStrToDouble(qstring_t *str, char **endptr)
{
   return strtod(str->buffer, endptr);
}

//
// Other String Types
//

//
// QStrCDup
//
// Creates a C string duplicate of a qstring's contents.
//
char *QStrCDup(qstring_t *qstr, int tag)
{
   return (char *)Z_Strdup(qstr->buffer, tag, NULL);
}

//
// QStrCDupAuto
//
// Creates an automatic allocation (disposable) copy of the qstring's
// contents.
//
char *QStrCDupAuto(qstring_t *qstr)
{
   return (char *)Z_Strdupa(qstr->buffer);
}

//=============================================================================
//
// Case Handling
//

//
// QStrLwr
//
// Converts the string to lowercase.
//
qstring_t *QStrLwr(qstring_t *qstr)
{
   M_Strlwr(qstr->buffer);
   return qstr;
}

//
// QStrUpr
//
// Converts the string to uppercase.
//
qstring_t *QStrUpr(qstring_t *qstr)
{
   M_Strupr(qstr->buffer);
   return qstr;
}

//=============================================================================
//
// Replacement Operations
//

static byte qstr_repltable[256];

//
// QStrReplaceInternal
//
// Static routine for replacement functions.
//
static size_t QStrReplaceInternal(qstring_t *qstr, char repl)
{
   size_t repcount = 0;
   unsigned char *rptr = (unsigned char *)(qstr->buffer);

   // now scan through the qstring buffer and replace any characters that
   // match characters in the filter table.
   while(*rptr)
   {
      if(qstr_repltable[*rptr])
      {
         *rptr = (unsigned char)repl;
         ++repcount; // count characters replaced
      }
      ++rptr;
   }

   return repcount;
}

//
// QStrReplace
//
// Replaces characters in the qstring that match any character in the filter
// string with the character specified by the final parameter.
//
size_t QStrReplace(qstring_t *qstr, const char *filter, char repl)
{
   const unsigned char *fptr = (unsigned char *)filter;

   memset(qstr_repltable, 0, sizeof(qstr_repltable));

   // first scan the filter string and build the replacement filter table
   while(*fptr)
      qstr_repltable[*fptr++] = 1;

   return QStrReplaceInternal(qstr, repl);
}

//
// QStrReplaceNotOf
//
// As above, but replaces all characters NOT in the filter string.
//
size_t QStrReplaceNotOf(qstring_t *qstr, const char *filter, char repl)
{
   const unsigned char *fptr = (unsigned char *)filter;

   memset(qstr_repltable, 1, sizeof(qstr_repltable));

   // first scan the filter string and build the replacement filter table
   while(*fptr)
      qstr_repltable[*fptr++] = 0;

   return QStrReplaceInternal(qstr, repl);
}

//
// QStrNormalizeSlashes
//
// Calls M_NormalizeSlashes on a qstring, which replaces \ characters with /
// and eliminates any duplicate slashes. This isn't simply a convenience
// method, as the qstring structure requires a fix-up after this function is
// used on it, in order to keep the string length correct.
//
qstring_t *QStrNormalizeSlashes(qstring_t *qstr)
{
   M_NormalizeSlashes(qstr->buffer);
   qstr->index = strlen(qstr->buffer);

   return qstr;
}

//=============================================================================
// 
// Formatting
//

//
// QStrMakeQuoted
// 
// Adds quotation marks to the qstring.
//
qstring_t *QStrMakeQuoted(qstring_t *s)
{
   // if the string is empty, make it "", else add quotes around the contents
   if(s->index == 0)
      return QStrCat(s, "\"\"");
   else
      return QStrPutc(QStrInsert(s, "\"", 0), '\"');
}

//
// QStrPrintf
//
// Performs formatted printing into a qstring. If maxlen is > 0, the qstring
// will be reallocated to a minimum of that size for the formatted printing.
// Otherwise, the qstring will be allocated to a worst-case size for the given
// format string, and in this case, the format string MAY NOT contain any
// padding directives, as they will be ignored, and the resulting output may
// then be truncated to qstr->size - 1.
//
int QStrPrintf(qstring_t *qstr, size_t maxlen, const char *fmt, ...)
{
   va_list va2;
   int returnval;

   if(maxlen)
   {
      // maxlen is specified. Check it against the qstring's current size.
      if(maxlen > qstr->size)
         QStrCreateSize(qstr, maxlen);
      else
         QStrClear(qstr);
   }
   else
   {
      // determine a worst-case size by parsing the fmt string
      va_list va1;                     // args
      char c;                          // current character
      const char *s = fmt;             // pointer into format string
      boolean pctstate = false;        // seen a percentage?
      int dummyint;                    // dummy vars just to get args
      double dummydbl;
      const char *dummystr;
      void *dummyptr;
      size_t charcount = strlen(fmt) + 1; // start at strlen of format string

      va_start(va1, fmt);
      while((c = *s++))
      {
         if(pctstate)
         {
            switch(c)
            {
            case 'x': // Integer formats
            case 'X':
               charcount += 2; // for 0x
               // fall-through
            case 'd':
            case 'i':
            case 'o':
            case 'u':
               // highest 32-bit octal is 11, plus 1 for possible sign
               dummyint = va_arg(va1, int);
               charcount += 12; 
               pctstate = false;
               break;
            case 'p': // Pointer
               dummyptr = va_arg(va1, void *);
               charcount += 8 * sizeof(void *) / 4 + 2;
               pctstate = false;
               break;
            case 'e': // Float formats
            case 'E':
            case 'f':
            case 'g':
            case 'G':
               // extremely excessive, but it's possible according to fcvt
               dummydbl = va_arg(va1, double);
               charcount += 626; 
               pctstate = false;
               break;
            case 'c': // Character
               c = va_arg(va1, int);
               pctstate = false;
               break;
            case 's': // String
               dummystr = va_arg(va1, const char *);
               charcount += strlen(dummystr);
               pctstate = false;
               break;
            case '%': // Just a percent sign
               pctstate = false;
               break;
            default:
               // subtract width specifiers, signs, etc.
               charcount -= 1;
               break;
            }
         }
         else if(c == '%')
         {
            pctstate = true;
            charcount -= 1;
         }
      }
      va_end(va1);

      if(charcount > qstr->size)
         QStrCreateSize(qstr, charcount);
      else
         QStrClear(qstr);
   }

   va_start(va2, fmt);
   returnval = pvsnprintf(qstr->buffer, qstr->size, fmt, va2);
   va_end(va2);

   qstr->index = strlen(qstr->buffer);

   return returnval;
}

// EOF

