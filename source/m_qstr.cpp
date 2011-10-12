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
// What this class guarantees:
// * The string will always be null-terminated
// * Indexing functions always check array bounds
// * Insertion functions always reallocate when needed
//
// Of course, using getBuffer can negate these, so avoid it
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

const size_t qstring::npos = ((size_t) -1);
const size_t qstring::basesize = 32;

//=============================================================================
//
// Constructors, Destructors, and Reinitializers
//

//
// qstring::createSize
//
// Creates a qstring with a given initial size, which helps prevent
// unnecessary initial reallocations. Resets insertion point to zero.
// This is safe to call on an existing qstring to reinitialize it.
//
qstring &qstring::createSize(size_t pSize)
{
   buffer = erealloc(char *, buffer, pSize);
   size   = pSize;
   index  = 0;
   memset(buffer, 0, size);

   return *this;
}

//
// qstring::create
//
// Gives the qstring a buffer of the default size and initializes
// it to zero. Resets insertion point to zero. This is safe to call
// on an existing qstring to reinitialize it.
//
qstring &qstring::create()
{
   return createSize(basesize);
}

//
// qstring::initCreate
//
// Initializes a qstring struct to all zeroes, then calls
// qstring::create. This is for safety the first time a qstring
// is created (if qstr::buffer is uninitialized, realloc will
// crash).
//
qstring &qstring::initCreate()
{
   buffer = NULL;
   size   = 0;
   index  = 0;

   return create();
}

//
// qstring::initCreateSize
//
// Initialization and creation with size specification.
//
qstring &qstring::initCreateSize(size_t pSize)
{
   buffer = NULL;
   size   = 0;
   index  = 0;

   return createSize(pSize);
}

//
// qstring::freeBuffer
//
// Frees the qstring object. It should not be used after this,
// unless qstring::create is called on it. You don't have to free
// a qstring before recreating it, however, since it uses realloc.
//
void qstring::freeBuffer()
{
   if(buffer)
      efree(buffer);
   buffer = NULL;
   index = size = 0;
}

//=============================================================================
//
// Basic Properties
//

//
// qstring::charAt
//
// Indexing function to access a character in a qstring. This is slower but 
// more secure than using qstring::getBuffer with array indexing.
//
char qstring::charAt(size_t idx) const
{
   if(idx >= size)
      I_Error("qstring::charAt: index out of range\n");

   return buffer[idx];
}

//
// qstring::bufferAt
//
// Gets a pointer into the buffer at the given position, if that position would
// be valid. Returns NULL otherwise. The same caveats apply as with qstring::getBuffer.
//
char *qstring::bufferAt(size_t idx) const
{
   return idx < size ? buffer + idx : NULL;
}

//=============================================================================
//
// Basic String Ops
//

//
// qstring::clear
//
// Sets the entire qstring buffer to zero, and resets the insertion index. 
// Does not reallocate the buffer.
//
qstring &qstring::clear()
{
   if(!buffer)
      initCreate();

   memset(buffer, 0, size);
   index = 0;

   return *this;
}

//
// qstring::clearOrCreate
//
// Creates a qstring, or clears it if it is already valid.
//
qstring &qstring::clearOrCreate(size_t pSize)
{
   if(!buffer)
      createSize(pSize);
   else
      clear();

   return *this;
}

//
// qstring::grow
//
// Grows the qstring's buffer by the indicated amount. This is automatically
// called by other qstring methods, so there is generally no need to call it 
// yourself.
//
qstring& qstring::grow(size_t len)
{   
   if(len > 0)
   {
      size_t newsize = size + len;

      buffer = erealloc(char *, buffer, newsize);
      memset(buffer + size, 0, len);
      size += len;
   }

   return *this;
}

//=============================================================================
// 
// Concatenation and Insertion/Deletion/Copying Functions
//

//
// qstring::Putc
//
// Adds a character to the end of the qstring, reallocating via buffer doubling
// if necessary.
//
qstring &qstring::Putc(char ch)
{
   if(!buffer)
      initCreate();

   if(index >= size - 1) // leave room for \0
      grow(size);        // double buffer size

   buffer[index++] = ch;

   return *this;
}

//
// qstring::operator +=
//
// Overloaded += for characters
//
qstring &qstring::operator += (char ch)
{
   return Putc(ch);
}

//
// qstring::Delc
//
// Deletes a character from the end of the qstring.
//
qstring &qstring::Delc()
{
   if(index > 0)
   {
      index--;
      buffer[index] = '\0';
   }

   return *this;
}

//
// qstring::concat
//
// Concatenates a C string onto the end of a qstring, expanding the buffer if
// necessary.
//
qstring &qstring::concat(const char *str)
{
   unsigned int cursize = size;
   unsigned int newsize = index + strlen(str) + 1;
   
   if(!buffer)
      initCreateSize(newsize);
   
   if(newsize > cursize)
      grow(newsize - cursize);

   strcat(buffer, str);

   index = strlen(buffer);

   return *this;
}

//
// qstring::concat
//
// Concatenates a qstring to a qstring. Convenience routine.
//
qstring &qstring::concat(const qstring &src)
{
   return concat(src.buffer ? src.buffer : "");
}

//
// qstring::operator +=
//
// Overloaded += for const char *
//
qstring &qstring::operator += (const char *other)
{
   return concat(other);
}

//
// qstring::operator +=
//
// Overloaded += for qstring &
//
qstring &qstring::operator += (const qstring &other)
{
   return concat(other);
}

//
// qstring::insert
//
// Inserts a string at a given position in the qstring. If the
// position is outside the range of the string, an error will occur.
//
qstring &qstring::insert(const char *insertstr, size_t pos)
{
   char *insertpoint;
   size_t charstomove;
   size_t insertstrlen = strlen(insertstr);
   size_t totalsize = index + insertstrlen + 1;
   
   if(!buffer)
      initCreateSize(totalsize);
   
   // pos must be between 0 and dest->index - 1
   if(pos >= index)
      I_Error("qstring::insert: position out of range\n");

   // grow the buffer to hold the resulting string if necessary
   if(totalsize > size)
      grow(totalsize - size);

   insertpoint = buffer + pos;
   charstomove = index  - pos;

   // use memmove for absolute safety
   memmove(insertpoint + insertstrlen, insertpoint, charstomove);
   memmove(insertpoint, insertstr, insertstrlen);

   index = strlen(buffer);

   return *this;
}

//
// qstring::copy
//
// Copies a C string into the qstring. The qstring is cleared first,
// and then set to the contents of *str.
//
qstring &qstring::copy(const char *str)
{
   clear();
   
   return concat(str);
}

//
// qstring::copy
//
// Overloaded for qstring &
//
qstring &qstring::copy(const qstring &src)
{
   clear();

   return concat(src);
}

//
// qstring::operator =
//
// Assignment from a qstring &
//
qstring &qstring::operator = (const qstring &other)
{
   return copy(other);
}

//
// qstring::operator =
//
// Assignment from a const char *
qstring &qstring::operator = (const char *other)
{
   return copy(other);
}


//
// qstring::copyInto
//
// Copies the qstring into a C string buffer.
//
char *qstring::copyInto(char *dest, size_t pSize) const
{
   return strncpy(dest, buffer ? buffer : "", pSize);
}

//
// qstring::copyInto
//
// Copies one qstring into another.
//
qstring &qstring::copyInto(qstring &dest) const
{
   dest.clear();
   
   return dest.concat(*this);
}

//
// qstring::swapWith
//
// Exchanges the contents of two qstrings.
//
void qstring::swapWith(qstring &str2)
{
   qstring temp;
   
   temp.buffer = this->buffer; // make a shallow copy
   temp.size   = this->size;
   temp.index  = this->index;

   this->buffer = str2.buffer;
   this->size   = str2.size;
   this->index  = str2.index;

   str2.buffer = temp.buffer;
   str2.size   = temp.size;
   str2.index  = temp.index;
}

//
// qstring::LStrip
//
// Removes occurrences of a specified character at the beginning of a qstring.
//
qstring &qstring::LStrip(char c)
{
   size_t i   = 0;
   size_t len = index;
   
   if(!buffer)
      initCreate();
   
   while(buffer[i] != '\0' && buffer[i] == c)
      ++i;

   if(i)
   {
      if((len -= i) == 0)
         clear();
      else
      {
         memmove(buffer, buffer + i, len);
         memset(buffer + len, 0, size - len);
         index -= i;
      }
   }

   return *this;
}

//
// qstring::RStrip
//
// Removes occurrences of a specified character at the end of a qstring.
//
qstring &qstring::RStrip(char c)
{
   if(!buffer)
      initCreate();

   while(index && buffer[index - 1] == c)
      Delc();

   return *this;
}

//
// qstring::truncate
//
// Truncates the qstring to the indicated position.
//
qstring &qstring::truncate(size_t pos)
{
   if(!buffer)
      initCreate();

   // pos must be between 0 and qstr->index - 1
   if(pos >= index)
      I_Error("qstring::truncate: position out of range\n");

   memset(buffer + pos, 0, index - pos);
   index = pos;

   return *this;
}

//=============================================================================
//
// Comparison Functions
//

//
// qstring::strCmp
//
// C-style string comparison/collation ordering.
//
int qstring::strCmp(const char *str) const
{
   return strcmp(buffer ? buffer : "", str);
}

//
// qstring::strNCmp
//
// C-style string compare with length limitation.
//
int qstring::strNCmp(const char *str, size_t maxcount) const
{
   return strncmp(buffer ? buffer : "", str, maxcount);
}

//
// qstring::strCaseCmp
//
// Case-insensitive C-style string compare.
//
int qstring::strCaseCmp(const char *str) const
{
   return strcasecmp(buffer ? buffer : "", str);
}

//
// qstring::strNCaseCmp
//
// Case-insensitive C-style compare with length limitation.
//
int qstring::strNCaseCmp(const char *str, size_t maxcount) const
{
   return strncasecmp(buffer ? buffer : "", str, maxcount);
}

//
// qstring::compare
// 
// C++ style comparison. True return value means it is equal to the argument.
//
bool qstring::compare(const char *str) const
{
   return !strcmp(buffer ? buffer : "", str);
}

//
// qstring::compare
// 
// Overload for qstrings.
//
bool qstring::compare(const qstring &other) const
{
   return !strcmp(buffer ? buffer : "", other.buffer);
}

//
// qstring::operator ==
//
// Overloaded comparison operator for const char *
//
bool qstring::operator == (const char *other) const
{
   return !strcmp(buffer ? buffer : "", other);
}

//
// qstring::operator ==
//
// Overloaded comparison operator for qstring &
//
bool qstring::operator == (const qstring &other) const
{
   return !strcmp(buffer ? buffer : "", other.buffer);
}

//
// qstring::operator !=
//
// Overloaded comparison operator for const char *
//
bool qstring::operator != (const char *other) const
{
   return strcmp(buffer ? buffer : "", other) != 0;
}

//
// qstring::operator !=
//
// Overloaded comparison operator for qstring &
//
bool qstring::operator != (const qstring &other) const
{
   return strcmp(buffer ? buffer : "", other.buffer) != 0;
}

//=============================================================================
//
// Search Functions
//

//
// qstring::strChr
//
// Calls strchr on the qstring ("find first of", C-style).
//
const char *qstring::strChr(char c) const
{
   return buffer ? strchr(buffer, c) : NULL;
}

//
// qstring::strRChr
//
// Calls strrchr on the qstring ("find last of", C-style)
//
const char *qstring::strRChr(char c) const
{
   return buffer ? strrchr(buffer, c) : NULL;
}

//
// qstring::findFirstOf
//
// Finds the first occurance of a character in the qstring and returns its 
// position. Returns qstring_npos if not found.
//
size_t qstring::findFirstOf(char c) const
{
   const char *rover;
   bool found = false;
   
   if(!buffer)
      return npos;
   
   rover = buffer;
   while(*rover)
   {
      if(*rover == c)
      {
         found = true;
         break;
      }
      ++rover;
   }

   return found ? rover - buffer : npos;
}

//
// qstring::findFirstNotOf
//
// Finds the first occurance of a character in the qstring which does not
// match the provided character. Returns qstring_npos if not found.
//
size_t qstring::findFirstNotOf(char c) const
{
   const char *rover;
   bool found = false;

   if(!buffer)
      return npos;

   rover = buffer;
   while(*rover)
   {
      if(*rover != c)
      {
         found = true;
         break;
      }
      ++rover;
   }

   return found ? rover - buffer : npos;
}

//
// qstring::findSubStr
//
// Calls strstr on the qstring. If the passed-in string is found, then the
// return value points to the location of the first instance of that substring.
//
const char *qstring::findSubStr(const char * substr) const
{
   return strstr(buffer ? buffer : "", substr);
}

//=============================================================================
//
// Conversion Functions
//

//
// Integers
//

//
// qstring::toInt
//
// Returns the qstring converted to an integer via atoi.
//
int qstring::toInt() const
{
   return buffer ? atoi(buffer) : 0;
}

//
// qstring::toLong
//
// Returns the qstring converted to a long integer via strtol.
//
long qstring::toLong(char **endptr, int radix) 
{
   if(!buffer)
      initCreate();

   return strtol(buffer, endptr, radix);
}

//
// Floating Point
//

//
// qstring::toDouble
//
// Calls strtod on the qstring.

double qstring::toDouble(char **endptr)
{
   if(!buffer)
      initCreate();

   return strtod(buffer, endptr);
}

//
// Other String Types
//

//
// qstring::duplicate
//
// Creates a C string duplicate of a qstring's contents.
//
char *qstring::duplicate(int tag) const
{
   return (char *)Z_Strdup(buffer ? buffer : "", tag, NULL);
}

//
// qstring::duplicateAuto
//
// Creates an automatic allocation (disposable) copy of the qstring's
// contents.
//
char *qstring::duplicateAuto() const
{
   return (char *)Z_Strdupa(buffer ? buffer : "");
}

//=============================================================================
//
// Case Handling
//

//
// qstring::toLower
//
// Converts the string to lowercase.
//
qstring &qstring::toLower()
{
   if(!buffer)
      initCreate();

   M_Strlwr(buffer);
   return *this;
}

//
// qstring::toUpper
//
// Converts the string to uppercase.
//
qstring &qstring::toUpper()
{
   if(!buffer)
      initCreate();

   M_Strupr(buffer);
   return *this;
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
static size_t QStrReplaceInternal(qstring *qstr, char repl)
{
   size_t repcount = 0;
   unsigned char *rptr = (unsigned char *)(qstr->getBuffer());

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
// qstring::replace
//
// Replaces characters in the qstring that match any character in the filter
// string with the character specified by the final parameter.
//
size_t qstring::replace(const char *filter, char repl)
{
   const unsigned char *fptr = (unsigned char *)filter;
   
   if(!buffer)
      initCreate();
   
   memset(qstr_repltable, 0, sizeof(qstr_repltable));

   // first scan the filter string and build the replacement filter table
   while(*fptr)
      qstr_repltable[*fptr++] = 1;

   return QStrReplaceInternal(this, repl);
}

//
// qstring::replaceNotOf
//
// As above, but replaces all characters NOT in the filter string.
//
size_t qstring::replaceNotOf(const char *filter, char repl)
{
   const unsigned char *fptr = (unsigned char *)filter;
   
   if(!buffer)
      initCreate();
   
   memset(qstr_repltable, 1, sizeof(qstr_repltable));

   // first scan the filter string and build the replacement filter table
   while(*fptr)
      qstr_repltable[*fptr++] = 0;

   return QStrReplaceInternal(this, repl);
}

//
// qstring::normalizeSlashes
//
// Calls M_NormalizeSlashes on a qstring, which replaces \ characters with /
// and eliminates any duplicate slashes. This isn't simply a convenience
// method, as the qstring structure requires a fix-up after this function is
// used on it, in order to keep the string length correct.
//
qstring &qstring::normalizeSlashes()
{
   if(!buffer)
      initCreate();

   M_NormalizeSlashes(buffer);
   index = strlen(buffer);

   return *this;
}

//=============================================================================
// 
// Formatting
//

//
// qstring::makeQuoted
// 
// Adds quotation marks to the qstring.
//
qstring &qstring::makeQuoted()
{
   // if the string is empty, make it "", else add quotes around the contents
   if(index == 0)
      return concat("\"\"");
   else
   {
      insert("\"", 0);
      return Putc('\"');
   }
}

//
// qstring::Printf
//
// Performs formatted printing into a qstring. If maxlen is > 0, the qstring
// will be reallocated to a minimum of that size for the formatted printing.
// Otherwise, the qstring will be allocated to a worst-case size for the given
// format string, and in this case, the format string MAY NOT contain any
// padding directives, as they will be ignored, and the resulting output may
// then be truncated to qstr->size - 1.
//
int qstring::Printf(size_t maxlen, const char *fmt, ...)
{
   va_list va2;
   int returnval;

   if(maxlen)
   {
      // maxlen is specified. Check it against the qstring's current size.
      if(maxlen > size)
         createSize(maxlen);
      else
         clear();
   }
   else
   {
      // determine a worst-case size by parsing the fmt string
      va_list va1;                     // args
      char c;                          // current character
      const char *s = fmt;             // pointer into format string
      bool pctstate = false;           // seen a percentage?
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

      if(charcount > size)
         createSize(charcount);
      else
         clear();
   }

   va_start(va2, fmt);
   returnval = pvsnprintf(buffer, size, fmt, va2);
   va_end(va2);

   index = strlen(buffer);

   return returnval;
}

// EOF

