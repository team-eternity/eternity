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

#include "e_hash.h"
#include "metaapi.h"
#include "m_collection.h"

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

//
// ParsedUDMF
//
// All that has to be done for UDMF parsing
//
class ParsedUDMF : public ZoneObject
{
public:
   // Doesn't own the data, in this current implementation
   ParsedUDMF() : mRawData(nullptr), mRawDataSize(0)
   {
      mBlocks.initialize(13);
   }

   ~ParsedUDMF()
   {
      // clear the hash table
      GroupedList *gl;
      while((gl = mBlocks.tableIterator(static_cast<GroupedList *>(nullptr))))
      {
         mBlocks.removeObject(gl);
         delete gl;
      }
   }

   // these can set level_error and return quickly
   void loadFromText(WadDirectory &setupwad, int lump);
   void loadVertices() const;
   void loadSectors() const;
   void loadSideDefs() const;
   void loadLineDefs();
   void loadSideDefs2() const;
   void loadThings() const;

private:

   //
   // TokenType
   //
   // Type of UDMF token.
   //
   enum TokenType
   {
      TokenType_Identifier,
      TokenType_Equal,
      TokenType_Semicolon,
      TokenType_Number,
      TokenType_String,
   #if 0 // currently disabled because it's too strange
      TokenType_Keyword,
   #endif
      TokenType_OpeningBrace,
      TokenType_ClosingBrace,
      TokenType_SingleComment,
      TokenType_MultiComment,
   };

   //
   // TokenDef
   //
   // Token definition. Used for the constants below
   //
   struct TokenDef
   {
      TokenType type;
   
      // Returns true if this token type is detected, and pc is advanced right
      // after the token. End is the buffer length, and pc is input as the 
      // current position
      bool (*detect)(const char **pc, const char *end, int *line);
   };

   //
   // Token
   //
   // UDMF parsed token instance. To be stored in a list
   //
   struct Token
   {
      TokenType type;
      const char *data;    // address in TEXTMAP buffer
      size_t size;         // length of data
      int line;            // line in TEXTMAP where it appears (for debugging)
   };

   //
   // GroupedList
   //
   // Holds a Collection and hash table handles. Useful to have separate
   // collections (referenced from an EHashTable) of same-named blocks, to be
   // used later from P_SetupLevel.
   //
   class GroupedList : public ZoneObject
   {
   public:
      Collection<MetaTable> items;     // The list of blocks
      DLListItem<GroupedList> link;
      const char *key;                 // the common name of the blocks

      GroupedList(const char *inKey) : link(), key(inKey)
      {
      }
   };

   static const TokenDef sTokenDefs[];

   // Raw data (not owned and subject to deletion once parsing is over)
   const char *mRawData;
   size_t mRawDataSize;

   // Semi-raw list of tokens (syntactically checked on the fly, though)
   PODCollection<Token> mTokens;

   // Contains top-level assignments which probably won't be ordered
   MetaTable mGlobalTable;

   // Collections of map items (to be copied into global arrays)
   EHashTable<GroupedList, ENCStringHashKey, 
      &GroupedList::key, &GroupedList::link> mBlocks;

   static bool allowNext(TokenType first, TokenType second);
   inline static bool allowEnd(TokenType token)
   {
      return token == TokenType_Semicolon || token == TokenType_ClosingBrace;
   }

   void tokenize();
   void readTokenizedData();
   
};

extern UDMFNamespace gUDMFNamespace;

#endif

// EOF

