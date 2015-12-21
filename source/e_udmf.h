// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright (C) 2015 Ioan Chera et al.
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
//----------------------------------------------------------------------------
//
// Universal Doom Map Format, for Eternity
//
//----------------------------------------------------------------------------

#ifndef E_ARGS_H_
#define E_ARGS_H_

class WadDirectory;

//
// UDMFNamespace
//
// UDMF namespace selection
//
enum UDMFNamespace
{
   unsDoom,
   unsHeretic,
   unsHexen,
   unsStrife,
   unsEternity,
};

//
// PostEditingLumpNums
//
// This contains the post-editing lump addresses, needed because they're not in
// fixed position in UDMF.
//
struct MapGenLumpAddresses
{
   int segs;
   int ssectors;
   int nodes;
   int reject;
   int blockmap;
   int behavior;

   //
   // completeForDoom
   //
   // True if all but the behaviour lump are found. The behaviour lump is not
   // required for Doom.
   //
   bool completeForDoom() const
   {
      return segs >= 0 && ssectors >= 0 && nodes >= 0
         && reject >= 0 && blockmap >= 0;
   }
};

extern UDMFNamespace gUDMFNamespace;

///////////////////////////////////////////////////////////////////////////////

//
// UDMFTokenType
//
// For parsing. Used by entities here and in the cpp
//
enum UDMFTokenType
{
   UDMFTokenType_Identifier,
   UDMFTokenType_Operator,
   UDMFTokenType_String,
   UDMFTokenType_Number,
};

//
// UDMFParser
//
// The class that reads from the TEXTMAP lump and populates the collections
// accordingly. Has a destructor that makes empty these collections.
//
class UDMFParser : public ZoneObject
{
public:
   void start(WadDirectory &setupwad, int lump);
   ~UDMFParser();

private:

   //
   // Token
   //
   // For parsing
   //
   struct Token
   {
      char *data;
      size_t size;
      UDMFTokenType type;
      double number;
   };

   //
   // Tokenizer
   //
   // Does the actual extraction
   //
   class Tokenizer : public ZoneObject
   {
   public:
      Tokenizer() : mData(nullptr), mEnd(nullptr), mLineStart(nullptr), 
         mLine(1)
      {
      }

      Tokenizer(char *data, size_t size)
      {
         setData(data, size);
      }

      //
      // setData
      //
      // Sets the data (doesn't own it)
      //
      void setData(char *data, size_t size)
      {
         mData = data;
         mEnd = data + size;
         mLineStart = data;
         mLine = 1;
      }

      //
      // nextLine
      //
      // Call this whenever the cursor encounters a newline. It will increment
      // the line number and set the line start to the character after the
      // newline.
      //
      void nextLine()
      {
         ++mLine;
         mLineStart = mData + 1;
      }

      bool readToken(Token &token);
      
      void raise(const char *message) const;

   private:
      char *mData;            // pointer to cursor in document
      const char *mEnd;       // end of document
      const char *mLineStart; // pointer to start of current line
      int mLine;              // line number (1-based)
   };

   void handleGlobalAssignment() const;
   void handleNewBlock();
   void handleLocalAssignment() const;

   Tokenizer mTokenizer;

   const char *mGlobalKey;
   Token mGlobalValue;

   const char *mLocalKey;
   Token mLocalValue;

   byte *mCurrentNewItem;
   const void *mCurrentBinding;
};

void E_LoadUDMFLineDefs();
void E_LoadUDMFSideDefs();
void E_LoadUDMFSideDefs2();
void E_LoadUDMFVertices();
void E_LoadUDMFSectors();
void E_LoadUDMFThings();

#endif

// EOF

