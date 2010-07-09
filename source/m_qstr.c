// Emacs style mode select   -*- C -*-
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
qstring_t *QStrCreateSize(qstring_t *qstr, unsigned int size)
{
   qstr->buffer = realloc(qstr->buffer, size);
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
// QStrBuffer
//
// Retrieves a pointer to the internal buffer. This pointer shouldn't be 
// cached, and is not meant for writing into (although it is safe to do so, it
// circumvents the encapsulation and security of this structure).
//
char *QStrBuffer(qstring_t *qstr)
{
   return qstr->buffer;
}

//
// QStrCharAt
//
// Indexing function to access a character in a qstring. This is slower but 
// more secure than using QStrBuffer with array indexing.
//
char QStrCharAt(qstring_t *qstr, unsigned int idx)
{
   if(idx >= qstr->size)
      I_Error("QStrCharAt: index out of range\n");

   return qstr->buffer[idx];
}

//
// QStrLen
//
// Calls strlen on the internal buffer. Convenience method.
//
unsigned int QStrLen(qstring_t *qstr)
{
   return strlen(qstr->buffer);
}

//
// QStrSize
//
// Returns the amount of size allocated for this qstring (will be >= strlen).
// You are allowed to index into the qstring up to size - 1, although any bytes
// beyond the strlen will be zero.
//
unsigned int QStrSize(qstring_t *qstr)
{
   return qstr->size;
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
// QStrGrow
//
// Grows the qstring's buffer by the indicated amount. This is automatically
// called by other qstring methods, so there is generally no need to call it 
// yourself.
//
qstring_t *QStrGrow(qstring_t *qstr, unsigned int len)
{   
   int newsize = qstr->size + len;

   qstr->buffer = realloc(qstr->buffer, newsize);
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
      QStrGrow(qstr, qstr->size);  // double buffer size

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
   unsigned int newsize = strlen(qstr->buffer) + strlen(str) + 1;

   if(newsize > cursize)
      QStrGrow(qstr, newsize - cursize);

   strcat(qstr->buffer, str);

   qstr->index = newsize - 1;

   return qstr;
}

//
// QStrCpy
//
// Copies a C string into the qstring. The qstring is cleared first,
// and then set to the contents of *str.
//
qstring_t *QStrCpy(qstring_t *qstr, const char *str)
{
   QStrClear(qstr);
   
   return QStrCat(qstr, str);
}

//
// QStrCopyTo
//
// Copies one qstring into another.
//
qstring_t *QStrCopyTo(qstring_t *dest, const qstring_t *src)
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
   unsigned int i = 0;
   size_t len = strlen(qstr->buffer);

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
// QStrCaseCmp
//
// Case-insensitive string compare.
//
int QStrCaseCmp(qstring_t *qstr, const char *str)
{
   return strcasecmp(qstr->buffer, str);
}

//=============================================================================
//
// Search Functions
//

//
// QStrChr
//
// Calls strchr on the qstring ("find first of").
//
const char *QStrChr(qstring_t *qstr, char c)
{
   return strchr(qstr->buffer, c);
}

//
// QStrRChr
//
// Calls strrchr on the qstring ("find last of")
//
const char *QStrRChr(qstring_t *qstr, char c)
{
   return strrchr(qstr->buffer, c);
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
static unsigned int QStrReplaceInternal(qstring_t *qstr, char repl)
{
   unsigned int repcount = 0;
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
unsigned int QStrReplace(qstring_t *qstr, const char *filter, char repl)
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
unsigned int QStrReplaceNotOf(qstring_t *qstr, const char *filter, char repl)
{
   const unsigned char *fptr = (unsigned char *)filter;

   memset(qstr_repltable, 1, sizeof(qstr_repltable));

   // first scan the filter string and build the replacement filter table
   while(*fptr)
      qstr_repltable[*fptr++] = 0;

   return QStrReplaceInternal(qstr, repl);
}

//=============================================================================
// 
// Formatting
//

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
int QStrPrintf(qstring_t *qstr, unsigned int maxlen, const char *fmt, ...)
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
      unsigned int charcount = strlen(fmt) + 1; // start at strlen of format string

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

