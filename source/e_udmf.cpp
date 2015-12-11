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

#include "z_zone.h"

#include "doomtype.h"
#include "m_collection.h"
#include "m_ctype.h"
#include "m_qstr.h"
#include "metaapi.h"
#include "p_setup.h"
#include "w_wad.h"
#include "z_auto.h"

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
   // after the token. End is the buffer length, and pc is input as the current
   // position
   bool (*detect)(const char **pc, const char *end, int *line);
};

//
// E_allowNext
//
// Helper function to tell if a token is allowed after another. Comment 
// "tokens" are treated differently in the code.
//
static bool E_allowNext(TokenType first, TokenType second)
{
   switch(first)
   {
   case TokenType_Identifier:
      return second == TokenType_Equal || second == TokenType_OpeningBrace
         || second == TokenType_Semicolon;
   case TokenType_Equal:
      return second == TokenType_Identifier || second == TokenType_Number 
         || second == TokenType_String;
   case TokenType_Semicolon:
   case TokenType_OpeningBrace:
      return second == TokenType_Identifier || second == TokenType_ClosingBrace
         || second == TokenType_Semicolon;
   case TokenType_Number:
   case TokenType_String:
      return second == TokenType_Semicolon;
   case TokenType_ClosingBrace:
      return second == TokenType_Identifier || second == TokenType_Semicolon;
   }
   return false;
}

//
// E_allowEnd
//
// True if element is allowed to be the last in the file
//
inline static bool E_allowEnd(TokenType token)
{
   return token == TokenType_Semicolon || token == TokenType_ClosingBrace;
}

//
// E_advanceWhitespace
//
// Helper function to advance (ignoring) whitespace
//
inline static bool E_advanceWhitespace(const char **pc, const char *end, int *line)
{
   while(ectype::isSpace(**pc) && *pc < end)
   {
      if(**pc == '\n')
         ++*line; // keep track of enters
      ++*pc;
   }
   return *pc < end;
}

//
// E_advanceString
//
// Token detection function written separately because VC2012 crashes if I use
// lambda
//
static bool E_advanceString(const char **pc, const char *end, int *line)
{
   const char *orgPc = *pc;
   if(**pc != '"')
      return false;
         
   int orgLine = *line;
   bool escape = false;
   for(++*pc; *pc < end; ++*pc)
   {
      if(**pc == '\n')
         ++*line;
      if(!escape)
      {
         if(**pc == '\\')
            escape = true;
         else if(**pc == '"')
         {
            ++*pc;
            return true;
         }
      }
      else
         escape = false;
   }

   *pc = orgPc;   // if it returns false, do NOT move the pointer
   *line = orgLine;
   return false;  // ending " not found, treat this as error
}

//
// kTokenDefs
//
// The token definitions, with corresponding type (to aid iteration), reading
// function and list of allowed nexts (thanks to C++ it's padded with 0)
//
static const TokenDef kTokenDefs[] =
{
   { 
      TokenType_Identifier, [](const char **pc, const char *end, int *line) {
         if(!ectype::isAlpha(**pc) && **pc != '_')
            return false;

         for(++*pc; *pc < end; ++*pc)
            if(!ectype::isAlnum(**pc) && **pc != '_')
               break;
         return true;
      }
   },

   {
      TokenType_Equal, [](const char **pc, const char *end, int *line) {
         if(**pc == '=')
         {
            ++*pc;
            return true;
         }
         return false;
      }
   },

   {
      TokenType_Semicolon, [](const char **pc, const char *end, int *line) {
         if(**pc == ';')
         {
            ++*pc;
            return true;
         }
         return false;
      }
   },

   {
      TokenType_OpeningBrace, [](const char **pc, const char *end, int *line) {
         if(**pc == '{')
         {
            ++*pc;
            return true;
         }
         return false;
      }
   },

   {
      TokenType_ClosingBrace, [](const char **pc, const char *end, int *line) {
         if(**pc == '}')
         {
            ++*pc;
            return true;
         }
         return false;
      }
   },

   {
      TokenType_Number, [](const char **pc, const char *end, int *line) {
         char *endResult;
         strtod(*pc, &endResult);
         if(*pc == endResult)
            return false;
         *pc = endResult;
         return true;
      }
   },

   { TokenType_String, E_advanceString },

   {
      TokenType_SingleComment, [](const char **pc, const char *end, int *line) {
         if(**pc != '/' || (*pc + 1) == end || *(*pc + 1) != '/')
            return false;

         for(++*pc; *pc < end; ++*pc)
         {
            if(**pc == '\n')
            {
               ++*pc;
               return true;
            }
         }
         return true;
      }
   },
   {
      TokenType_MultiComment, [](const char **pc, const char *end, int *line) {
         if(**pc != '/' || (*pc + 1) == end || *(*pc + 1) != '*')
            return false;

         for(*pc += 2; *pc < end; ++*pc)
         {
            if(**pc == '*' && *pc + 1 < end && *(*pc + 1) == '/')
            {
               *pc += 2;
               return true;
            }
         }
         return true;
      }
   }
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
// Excerpt from the UDMF specifications 1.1 Grammar / Syntax
// http://www.doomworld.com/eternity/engine/stuff/udmf11.txt
//
// translation_unit := global_expr_list
// global_expr_list := global_expr global_expr_list
// global_expr := block | assignment_expr
// block := identifier '{' expr_list '}'
// expr_list := assignment_expr expr_list
// assignment_expr := identifier '=' value ';' | nil
// identifier := [A-Za-z_]+[A-Za-z0-9_]*
// value := integer | float | quoted_string | keyword
// integer := [+-]?[1-9]+[0-9]* | 0[0-9]+ | 0x[0-9A-Fa-f]+
// float := [+-]?[0-9]+'.'[0-9]*([eE][+-]?[0-9]+)?
// quoted_string := "([^"\\]*(\\.[^"\\]*)*)"
// keyword := [^{}();"'\n\t ]+
//

//
// Error
//
// Parse error
//
class Error
{
public:
   const char *message;
   int line;

   Error(const char *inMessage, int inLine) 
      : message(inMessage), line(inLine)
   {
   }
};

//
// OK
//
// Return a non-error
//
inline static Error OK()
{
   return Error(nullptr, 0);
}

//
// ParsedUDMF
//
// All that has to be done for UDMF parsing
//
class ParsedUDMF : public ZoneObject
{
public:
   // Doesn't own the data, in this current implementation
   ParsedUDMF(const char *rawData, size_t rawDataSize) : mRawData(rawData), 
      mRawDataSize(rawDataSize)
   {
      static MetaTable proto;
      mBlocks.setPrototype(&proto);
   }

   Error tokenize();
   Error readTokenizedData();

private:
   // Raw data (not owned)
   const char *mRawData;
   size_t mRawDataSize;

   // Semi-raw list of tokens (syntactically checked on the fly, though)
   PODCollection<Token> mTokens;

   // Contains top-level assignments which probably won't be ordered
   MetaTable mGlobalTable;

   // Ordered list of map objects
   Collection<MetaTable> mBlocks;
};

//
// As we see, TEXTMAP consists of global_expr entries. Each global_expr is
// a metatable. 
//

//
// ParsedUDMF::parse
//
// Parses UDMF text into a collection of metatables.
// For simplicity, it will preprocess stuff by removing comments first.
//
// Arguments:
// - textMap: collection of elements to populate
// - data: raw textmap data (string)
// - size: size of string
// Returns true on success, false on error
//
Error ParsedUDMF::tokenize()
{
   const char *pc, *oldpc;
   const char *end = mRawData + mRawDataSize;

   Token token;
   const char *prepos;

   bool found;
   int line = 1;
   
   // it should really advance from E_advanceWhitespace and token detection. If
   // not, then it will throw an error for safety.
   for(pc = mRawData; pc < end; )
   {
      oldpc = pc;
      if(!E_advanceWhitespace(&pc, end, &line))
         break;

      found = false;
      for(const TokenDef &def : kTokenDefs)
      {
         prepos = pc;
         if(def.detect(&pc, end, &line))
         {
            found = true;
            // Completely skip comments
            if(def.type == TokenType_SingleComment 
               || def.type == TokenType_MultiComment)
            {
               continue;
            }

            // If this is not going to be the first token, check if it's
            // syntactically correct
            if(mTokens.getLength() 
               && !E_allowNext((mTokens.end() - 1)->type, def.type))
            {
               return Error("Syntax error", line);
            }
            token.type = def.type;
            token.data = prepos;
            token.size = pc - prepos;
            token.line = line;
            mTokens.add(token);
            
            break;
         }
      }
      if(!found)
         return Error("Syntax error", line);  // syntax error 
      if(pc == oldpc)
      {
         return Error("Internal parsing error, please report it to developers,", 
               line);
      }
   }

   if(mTokens.getLength()) 
   {
      if (!E_allowEnd((mTokens.end() - 1)->type))
         return Error("Invalid end of TEXTMAP", line);
      if(mTokens[0].type != TokenType_Identifier)
         return Error("TEXTMAP must start with an identifier", mTokens[0].line);
   }
   // We now have the list
   return OK();
}

//
// E_decodeString
//
// Helper function to de-escape a string
//
static void E_decodeString(const char *str, size_t len, qstring &dest)
{
   char c;
   bool escaped = false;
   dest.clear();
   for(size_t i = 1; i < len; ++i)
   {
      c = str[i];
      if(!escaped)
      {
         if(c == '"')
            return;
         if(c == '\\')
            escaped = true;
         else
            dest << c;
      }
      else
      {
         dest << c;
         escaped = false;
      }
   }
}

//
// ParsedUDMF::readTokenizedData
//
// Reads the tokenized data (already syntax-validated) into usable metaobjects
// Clears the token list afterwards.
//
Error ParsedUDMF::readTokenizedData()
{

   bool haveIdentifier = false;
   qstring identifier;
   bool inField = false;
   MetaTable *block = &mGlobalTable;

   qstring aux;
   for(const Token &token : mTokens)
   {
      switch(token.type)
      {
      case TokenType_Identifier:
         if(!inField)
            identifier.copy(token.data, token.size);
         else
         {
            if(token.size == 4 && !strncasecmp(token.data, "true", 4))
            {
               // FIXME: currently just use int to represent booleans
               block->setInt(identifier.constPtr(), 1);
            }
            else if(token.size == 5 && !strncasecmp(token.data, "false", 5))
            {
               block->setInt(identifier.constPtr(), 0);
            }
            else
            {
               // error if not true or false
               return Error("Invalid keyword; expected 'true' or 'false'", 
                  token.line);
            }
         }
         break;
      case TokenType_Equal:
         if(inField)
            return Error("Unexpected '='", token.line);
         inField = true;
         break;
      case TokenType_Semicolon:
         // Excessive semicolons are never harmful, so don't return error
         // They already have been filtered by the tokenizer
         inField = false;
         break;
      case TokenType_Number:
         aux.copy(token.data, token.size);   // strtod needs C string
         block->setDouble(identifier.constPtr(), strtod(aux.constPtr(), 
            nullptr));
         break;
      case TokenType_String:
         E_decodeString(token.data, token.size, aux);
         block->setString(identifier.constPtr(), aux.constPtr());
         break;
      case TokenType_OpeningBrace:
         if(block == &mGlobalTable)
         {
            MetaTable table(identifier.constPtr());
            mBlocks.add(table);
            block = mBlocks.end() - 1;
         }
         else
         {
            // don't allow sub-blocks
            return Error("Blocks cannot be nested", token.line);
         }
         break;
      case TokenType_ClosingBrace:
         if(block == &mGlobalTable) // error if it wasn't started by {
            return Error("No matching '{' found for '}'", token.line);
         block = &mGlobalTable;
         break;
      }
   }

   // Check validity near the end.
   if(block != &mGlobalTable)
   {
      return Error("Missing closing brace in TEXTMAP", (mTokens.end() - 1)->line);
   }

   mTokens.makeEmpty();

   return OK();
}

//
// E_LoadUDMF
//
// Load UDMF TEXTMAP lump.
// - setupwad: current WadDirectory. Probably a global variable in p_setup.cpp
// - lump: index of TEXTMAP lump.
//
void E_LoadUDMF(WadDirectory &setupwad, int lump, const char *mapname)
{
   ZAutoBuffer buf;

   // FIXME: is it safe to load such a big string in memory?
   setupwad.cacheLumpAuto(lump, buf);
   auto data = buf.getAs<char *>();

   // Use a collection because order is important, not key uniqueness
   ParsedUDMF udmf(data, setupwad.lumpLength(lump));
   Error error = udmf.tokenize();
   if(error.message)
   {
      qstring complete_message;
      complete_message.copy(error.message) << " at line " << error.line;
      P_SetupLevelError(complete_message.constPtr(), mapname); 
      return;
   }
   error = udmf.readTokenizedData();
   if(error.message)
   {
      qstring complete_message;
      complete_message.copy(error.message) << " at line " << error.line;
      P_SetupLevelError(complete_message.constPtr(), mapname); 
      return;
   }  

   // TODO: explore blocks
}

// EOF

