// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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

#ifndef M_QSTR_H__
#define M_QSTR_H__

#include "z_zone.h"

class SaveArchive;

class qstring : public ZoneObject
{
private:
   char    local[16];
   char   *buffer;
   size_t  index;
   size_t  size;

   bool isLocal() const { return (buffer == local); }
   void unLocalize(size_t pSize);

   void moveFrom(qstring &&other) noexcept;

public:
   static const size_t npos;
   static const size_t basesize;

   // Constructors / Destructor
   explicit qstring(size_t startSize = 0) 
      : ZoneObject(), index(0), size(16)
   {
      buffer = local;
      memset(local, 0, sizeof(local));
      if(startSize)
         initCreateSize(startSize);
   }

   qstring(const qstring &other) 
      : ZoneObject(), index(0), size(16)
   {
      buffer = local;
      memset(local, 0, sizeof(local));
      copy(other);
   }

   explicit qstring(const char *cstr)
      : ZoneObject(), index(0), size(16)
   {
      buffer = local;
      memset(local, 0, sizeof(local));
      copy(cstr);
   }

   qstring(qstring &&other) noexcept;

   ~qstring() { freeBuffer(); }

   // Basic Property Getters

   //
   // qstring::getBuffer
   //
   // Retrieves a pointer to the internal buffer. This pointer shouldn't be 
   // cached, and is not meant for writing into (although it is safe to do so, it
   // circumvents the encapsulation and security of this structure).
   //
   char *getBuffer() { return buffer; }

   //
   // qstring::constPtr
   //
   // Like qstring::getBuffer, but casts to const to enforce safety.
   //
   const char *constPtr() const { return buffer; }

   //
   // qstring::length
   //
   // Because the validity of "index" is maintained by all insertion and editing
   // functions, we can bypass calling strlen.
   //
   size_t length() const { return index; }

   //
   // qstring::empty
   //
   // For C++ std::string compatibility.
   //
   bool empty() const { return (index == 0); }

   //
   // qstring::getSize
   //
   // Returns the amount of size allocated for this qstring (will be >= strlen).
   // You are allowed to index into the qstring up to size - 1, although any bytes
   // beyond the strlen will be zero.
   //
   size_t getSize() const { return size; }

   // Initialization and Resizing
   qstring &initCreate();
   qstring &initCreateSize(size_t size);
   qstring &createSize(size_t size);
   qstring &create();
   qstring &grow(size_t len);
   qstring &clear();
   qstring &clearOrCreate(size_t size);
   void     freeBuffer();

   // Indexing operations
   char  charAt(size_t idx) const;
   char *bufferAt(size_t idx);

   unsigned char ucharAt(size_t idx) const { return (unsigned char)charAt(idx); }

   // Concatenation and insertion/deletion
   qstring &Putc(char ch);
   qstring &Delc();
   qstring &concat(const char *str);
   qstring &concat(const qstring &src);
   qstring &insert(const char *insertstr, size_t pos);

   // Comparisons: C and C++ style
   int  strCmp(const char *str) const;
   int  strNCmp(const char *str, size_t maxcount) const;
   int  strCaseCmp(const char *str) const;
   int  strNCaseCmp(const char *str, size_t maxcount) const;
   bool compare(const char *str) const;
   bool compare(const qstring &other) const;

   // Hashing
   static unsigned int HashCodeStatic(const char *str);
   static unsigned int HashCodeCaseStatic(const char *str);

   unsigned int hashCode() const;      // case-insensitive
   unsigned int hashCodeCase() const;  // case-considering

   // Copying and Swapping
   qstring &copy(const char *str);
   qstring &copy(const char *str, size_t count);
   qstring &copy(const qstring &src);
   char    *copyInto(char *dest, size_t size) const;
   qstring &copyInto(qstring &dest) const;
   void     swapWith(qstring &str2);

   // In-Place Case Conversions
   qstring &toUpper();
   qstring &toLower();

   // Substring Replacements
   size_t   replace(const char *filter, char repl);
   size_t   replaceNotOf(const char *filter, char repl);
   
   // File Path Utilities
   qstring &normalizeSlashes();
   qstring &pathConcatenate(const char *addend);
   qstring &pathConcatenate(const qstring &other);
   qstring &addDefaultExtension(const char *ext);
   qstring &removeFileSpec();
   void     extractFileBase(qstring &dest) const;

   // Zone strdup wrappers
   char *duplicate(int tag = PU_STATIC) const;
   char *duplicateAuto() const;

   // Numeric Conversions
   int    toInt() const;
   long   toLong(char **endptr, int radix) const;
   double toDouble(char **endptr) const;

   // Searching/Substring Finding Routines
   const char *strChr(char c) const;
   const char *strRChr(char c) const;
   size_t      findFirstOf(char c) const;
   size_t      findFirstNotOf(char c) const;
   size_t      findLastOf(char c) const;
   const char *findSubStr(const char *substr) const;
   const char *findSubStrNoCase(const char *substr) const;
   size_t      find(const char *s, size_t pos = 0) const;
   bool        endsWith(char c) const;

   // Stripping and Truncation
   qstring &lstrip(char c);
   qstring &rstrip(char c);
   qstring &truncate(size_t pos);
   qstring &erase(size_t pos, size_t n = npos);

   // Special Formatting 
   qstring &makeQuoted();
   int      Printf(size_t maxlen, E_FORMAT_STRING(const char *fmt), ...) E_PRINTF(3, 4);

   // Operators
   bool     operator == (const qstring &other) const;
   bool     operator == (const char    *other) const;
   bool     operator != (const qstring &other) const;
   bool     operator != (const char    *other) const;
   qstring &operator  = (const qstring &other);
   qstring &operator  = (const char    *other);
   qstring &operator  = (qstring      &&other);
   qstring &operator += (const qstring &other);
   qstring &operator += (const char    *other);
   qstring &operator += (char  ch);
   qstring  operator +  (const qstring &other) const;
   qstring  operator +  (const char    *other) const;
   qstring &operator << (const qstring &other);
   qstring &operator << (const char    *other);
   qstring &operator << (char   ch);
   qstring &operator << (int    i);
   qstring &operator << (double d);
   qstring  operator /  (const qstring &other) const;
   qstring  operator /  (const char    *other) const;
   qstring &operator /= (const qstring &other);
   qstring &operator /= (const char    *other);

   friend qstring operator + (const char *a, const qstring &b)
   {
      return qstring(a).concat(b);
   }

   friend qstring operator / (const char *a, const qstring &b)
   {
      return qstring(a).pathConcatenate(b);
   }

   char       &operator [] (size_t idx);
   const char &operator [] (size_t idx) const;

   // Archiving
   void archive(SaveArchive &arc);
};

#endif

// EOF


