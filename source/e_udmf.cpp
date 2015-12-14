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

#include "doomstat.h"
#include "doomtype.h"
#include "e_exdata.h"
#include "e_udmf.h"
#include "ev_specials.h"
#include "m_ctype.h"
#include "m_qstr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_auto.h"

// FIXME: why is this not in a header?
extern const char *level_error;

// UDMF namespace, one of a few choices
UDMFNamespace gUDMFNamespace;

// A convenient buffer to point level_error to
static char gLevelErrorBuffer[256];

// Special metatable keys not used by UDMF. Identified by special characters
// not allowed in TEXTMAP. Since it's internal, it may change with further
// versions so no risk of forward incompatibility

// line number in file associated with the current metatable
static const char kLineNumberKey[] = "#ln";

// Supported namespace strings
static const char kDoomNamespace[] = "doom";
static const char kHereticNamespace[] = "heretic";
static const char kHexenNamespace[] = "hexen";
static const char kStrifeNamespace[] = "doom";
static const char kEternityNamespace[] = "eternity";  // TENTATIVE

//
// MetaBool
//
// Based on MetaObject, needed by UDMF because it has boolean entries
//
class MetaBool : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaBool, MetaObject)

protected:
   bool value;

public:
   MetaBool() : Super(), value(0) {}
   MetaBool(size_t keyIndex, bool b) : Super(keyIndex), value(b) {}
   MetaBool(const char *key, bool b) : Super(key), value(b) {}
   MetaBool(const MetaBool &other) : Super(other), value(other.value) {}

   virtual MetaObject *clone() const { return new MetaBool(*this); }
   virtual const char *toString() const
   {
      return value ? "true" : "false";
   }

   bool getValue() const { return value; }
   void setValue(bool b) { value = b; }

   static void setMetaTable(MetaTable &table, const char *key, bool newValue);
};

IMPLEMENT_RTTI_TYPE(MetaBool)

//
// MetaBool::setMetaTable
//
// Static way to set MetaTable entry
//
void MetaBool::setMetaTable(MetaTable &table, const char *key, bool newValue)
{
   size_t keyIndex = MetaTable::IndexForKey(key);
   MetaObject * obj;

   if(!(obj = table.getObjectKeyAndType(keyIndex, RTTI(MetaBool))))
      table.addObject(new MetaBool(keyIndex, newValue));
   else
      static_cast<MetaBool *>(obj)->value = newValue;
}

//
// E_assignNamespace
//
// Sets the global namespace variable to a value based on text.
// Returns true if namespace is one of the assigned constants
//
static bool E_assignNamespace(const char *value)
{
   if(!strcasecmp(value, kEternityNamespace))
   {
      gUDMFNamespace = unsEternity;
      return true;
   }
   if(!strcasecmp(value, kDoomNamespace))
   {
      gUDMFNamespace = unsDoom;
      return true;
   }
   if(!strcasecmp(value, kHereticNamespace))
   {
      gUDMFNamespace = unsHeretic;
      return true;
   }
   if(!strcasecmp(value, kHexenNamespace))
   {
      gUDMFNamespace = unsHexen;
      return true;
   }
   if(!strcasecmp(value, kStrifeNamespace))
   {
      gUDMFNamespace = unsStrife;
      return true;
   }
   return false;
}

//
// ParsedUDMF::allowNext
//
// Helper function to tell if a token is allowed after another. Comment 
// "tokens" are treated differently in the code.
//
bool ParsedUDMF::allowNext(TokenType first, TokenType second)
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
// E_strtodOrOctal
//
// Helper that interprets either strtod (C++11 variety!) or octal integers
// Octal double not supported
// WARNING: unlike strtod, it does NOT skip whitespaces always
// FIXME: also support negative values
//
static double E_strtodOrOctalOrHex(const char *str, char **endptr)
{
   if(str[0] == '0')
   {
      if(str[1] >= '0' && str[1] <= '9')
         return static_cast<double>(strtol(str, endptr, 8));
      if(str[1] >= 'x' || str[1] == 'X')
         return static_cast<double>(strtol(str, endptr, 16));
   }
   return strtod(str, endptr);
}

//
// ParsedUDMF::sTokenDefs
//
// The token definitions, with corresponding type (to aid iteration), reading
// function and list of allowed nexts (thanks to C++ it's padded with 0)
//
const ParsedUDMF::TokenDef ParsedUDMF::sTokenDefs[] =
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
         E_strtodOrOctalOrHex(*pc, &endResult);
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
// UDMFException
//
// Used to funnel all parse errors into a single place. Uses a message and a
// line number, which can then be accessed directly
//
class UDMFException
{
public:
   const char *message;
   int line;
   const char *locationName;

   UDMFException(const char *inMessage, int inLine, 
      const char *inLocationName = "line") 
      : message(inMessage), line(inLine), locationName(inLocationName)
   {
   }

   void setLevelError() const;
};

//
// UDMFException::setLevelError
//
// Converts the exception to a P_SetupLevel level_error, so the loading will 
// stop.
//
void UDMFException::setLevelError() const
{
   psnprintf(gLevelErrorBuffer, earrlen(gLevelErrorBuffer), "%s at %s %d", 
         message, locationName, line);

   // Activate the error by setting this pointer
   level_error = gLevelErrorBuffer;
}

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
void ParsedUDMF::tokenize()
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
      for(const TokenDef &def : sTokenDefs)
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
               && !allowNext((mTokens.end() - 1)->type, def.type))
            {
               throw UDMFException("Syntax error", line);
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
         throw UDMFException("Syntax error", line);  // syntax error 
      if(pc == oldpc)
      {
         throw UDMFException("Internal parsing error, please report it to "
            "developers,", line);
      }
   }

   if(mTokens.getLength()) 
   {
      if (!allowEnd((mTokens.end() - 1)->type))
         throw UDMFException("Invalid end of TEXTMAP", line);
      if(mTokens[0].type != TokenType_Identifier)
         throw UDMFException("TEXTMAP must start with an identifier", mTokens[0].line);
   }
   // We now have the list
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
void ParsedUDMF::readTokenizedData()
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
               MetaBool::setMetaTable(*block, identifier.constPtr(), true);
            }
            else if(token.size == 5 && !strncasecmp(token.data, "false", 5))
            {
               MetaBool::setMetaTable(*block, identifier.constPtr(), false);
            }
            else
            {
               // error if not true or false
               throw UDMFException("Invalid keyword; expected 'true' or 'false'", 
                  token.line);
            }
         }
         break;
      case TokenType_Equal:
         if(inField)
            throw UDMFException("Unexpected '='", token.line);
         inField = true;
         break;
      case TokenType_Semicolon:
         // Excessive semicolons are never harmful, so don't return error
         // They already have been filtered by the tokenizer
         inField = false;
         break;
      case TokenType_Number:
         aux.copy(token.data, token.size);   // strtod needs C string
         block->setDouble(identifier.constPtr(), E_strtodOrOctalOrHex(aux.constPtr(), 
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
            GroupedList *list = mBlocks.objectForKey(identifier.constPtr());
            if(!list)
            {
               list = new GroupedList(identifier.constPtr());
               mBlocks.addObject(list);
            }
            list->items.add(table);

            block = list->items.end() - 1;

            // Tag the line number to this block so we can get the error
            // message later
            block->setInt(kLineNumberKey, token.line);
         }
         else
         {
            // don't allow sub-blocks
            throw UDMFException("Blocks cannot be nested", token.line);
         }
         break;
      case TokenType_ClosingBrace:
         if(block == &mGlobalTable) // error if it wasn't started by {
            throw UDMFException("No matching '{' found for '}'", token.line);
         block = &mGlobalTable;
         break;
      }
   }

   // Check validity near the end.
   if(block != &mGlobalTable)
   {
      throw UDMFException("Missing closing brace in TEXTMAP", (mTokens.end() - 1)->line);
   }

   mTokens.makeEmpty(); // clear it after use. It's consumed.
}

//
// E_requireDouble
//
// Returns a double value that accepts no default
//
static double E_requireDouble(MetaTable &block, const char *key)
{
   MetaObject *object = block.getObject(key);

   // dummy num, will be rethrown
   if(!object)
   {
      throw UDMFException("Missing required number field", 
            block.getInt(kLineNumberKey, 0));  
   }
   auto numObject = runtime_cast<MetaDouble *>(object);
   if(!numObject)
   {
      throw UDMFException("Field is not a number", 
            block.getInt(kLineNumberKey, 0));
   }

   return numObject->getValue();
}

//
// E_requireOptDouble
//
// Returns a double value that accepts a default
//
static double E_requireOptDouble(MetaTable &block, const char *key, double def = 0)
{
   MetaObject *object = block.getObject(key);

   // dummy num, will be rethrown
   if(!object)
   {
      return def;
   }
   auto numObject = runtime_cast<MetaDouble *>(object);
   if(!numObject)
   {
      throw UDMFException("Field is not a number", 
            block.getInt(kLineNumberKey, 0));
   }

   return numObject->getValue();
}

//
// E_requireOptInt
//
// Returns an int value where an integer (no point) is required, but accepting
// a default if not included.
//
static int E_requireOptInt(MetaTable &block, const char *key, int def = 0)
{
   MetaObject *object = block.getObject(key);

   if(!object)
      return def;

   auto numObject = runtime_cast<MetaDouble *>(object);
   if(!numObject)
   {
      throw UDMFException("Field is not an integer", 
            block.getInt(kLineNumberKey, 0));
   }

   double dbl = numObject->getValue();
   if(dbl != floor(dbl) || dbl < -2147483648. || dbl > 2147483647.)
   {
      throw UDMFException("Field is not an integer or is overflowing", 
            block.getInt(kLineNumberKey, 0));
   }

   return static_cast<int>(numObject->getValue());
}

//
// E_requireInt
//
// Returns an int value that accepts no default
//
static int E_requireInt(MetaTable &block, const char *key)
{
   MetaObject *object = block.getObject(key);

   if(!object)
   {
      throw UDMFException("Missing required integer field", 
            block.getInt(kLineNumberKey, 0));  
   }

   auto numObject = runtime_cast<MetaDouble *>(object);
   if(!numObject)
   {
      throw UDMFException("Field is not an integer", 
            block.getInt(kLineNumberKey, 0));
   }

   double dbl = numObject->getValue();
   if(dbl != floor(dbl) || dbl < -2147483648. || dbl > 2147483647.)
   {
      throw UDMFException("Field is not an integer or is overflowing", 
            block.getInt(kLineNumberKey, 0));
   }

   return static_cast<int>(numObject->getValue());
}

//
// E_requireOptBool
//
// Returns a bool value that accepts a default
//
static bool E_requireOptBool(MetaTable &block, const char *key, bool def = false)
{
   MetaObject *object = block.getObject(key);

   if(!object)
      return false;
   
   auto boolObject = runtime_cast<MetaBool *>(object);
   if(!boolObject)
   {
      throw UDMFException("Field is not boolean", 
         block.getInt(kLineNumberKey, 0));
   }

   return boolObject->getValue();
}

//
// E_requireString
//
// Requires a field to be string and exist, returning its value
//
static const char *E_requireString(MetaTable &block, const char *key)
{
   MetaObject *object = block.getObject(key);

   if(!object)
   {
      throw UDMFException("Missing required string field", 
            block.getInt(kLineNumberKey, 0));
   }

   auto strObject = runtime_cast<MetaString *>(object);
   if(!strObject)
   {
      throw UDMFException("Field is not a string", 
            block.getInt(kLineNumberKey, 0));
   }

   return strObject->getValue();
}

//
// E_requireOptString
//
// Requires a field to be string, returning its value or a default
//
static const char *E_requireOptString(MetaTable &block, const char *key, 
                                      const char *def = "-")
{
   MetaObject *object = block.getObject(key);

   if(!object)
   {
      return def;
   }

   auto strObject = runtime_cast<MetaString *>(object);
   if(!strObject)
   {
      throw UDMFException("Field is not a string", 
            block.getInt(kLineNumberKey, 0));
   }

   return strObject->getValue();
}

//
// ParsedUDMF::loadFromText
//
// Parses UDMF from a given text map, filling the blocks hash table.
// Can throw UDMFException
//
void ParsedUDMF::loadFromText(WadDirectory &setupwad, int lump)
{
   ZAutoBuffer buf;

   // FIXME: is it safe to load such a big string in memory?
   setupwad.cacheLumpAuto(lump, buf);
   auto data = buf.getAs<char *>();

   mRawData = data;
   mRawDataSize = setupwad.lumpLength(lump);

   // These can throw
   try
   {
      tokenize();
      readTokenizedData();
      if(!E_assignNamespace(mGlobalTable.getString("namespace", "")))
         throw UDMFException("Invalid or missing namespace", 0); // FIXME: line no.
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
   }
}

//
// ParsedUDMF::loadVertices
//
// Loads vertices from "vertex" metatable list
//
void ParsedUDMF::loadVertices() const
{
   try
   {
      GroupedList *coll = mBlocks.objectForKey("vertex");
      ::numvertexes = coll ? static_cast<int>(coll->items.getLength()) : 0;
      ::vertexes = estructalloctag(vertex_t, ::numvertexes, PU_LEVEL);

      double dx, dy;
      vertex_t *v;
      for(int i = 0; i < ::numvertexes; ++i)
      {
         MetaTable &table = coll->items[i];

         dx = E_requireDouble(table, "x");
         dy = E_requireDouble(table, "y");

         v = ::vertexes + i;
         v->x = M_DoubleToFixed(dx);
         v->y = M_DoubleToFixed(dy);
         v->fx = static_cast<float>(dx);
         v->fy = static_cast<float>(dy);
      }
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
   }
}

//
// ParsedUDMF::loadSectors
//
// Loads sectors from "sector" metatable list
//
void ParsedUDMF::loadSectors() const
{
   try
   {
      GroupedList *coll = mBlocks.objectForKey("sector");
      ::numsectors = coll ? static_cast<int>(coll->items.getLength()) : 0;
      ::sectors = estructalloctag(sector_t, ::numsectors, PU_LEVEL);

      sector_t *s;
      for(int i = 0; i < ::numsectors; ++i)
      {
         MetaTable &table = coll->items[i];
         s = ::sectors + i;

         s->floorheight = E_requireOptInt(table, "heightfloor") << FRACBITS;
         s->ceilingheight = E_requireOptInt(table, "heightceiling") << FRACBITS;
         s->floorpic = R_FindFlat(E_requireString(table, "texturefloor"));
         P_SetSectorCeilingPic(s, R_FindFlat(E_requireString(table, "textureceiling")));
         s->lightlevel = E_requireOptInt(table, "lightlevel", 160);
         s->special = E_requireOptInt(table, "special");
         s->tag = E_requireOptInt(table, "id");

         P_InitSector(s);
      }
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
   }
}

//
// ParsedUDMF::loadSideDefs
//
// Equivalent of first P_LoadSideDefs call
//
void ParsedUDMF::loadSideDefs() const
{
   GroupedList *coll = mBlocks.objectForKey("sidedef");
   ::numsides = coll ? static_cast<int>(coll->items.getLength()) : 0;
   ::sides = estructalloctag(side_t, ::numsides, PU_LEVEL);
}

//
// ParsedUDMF::loadLineDefs
//
// Equivalent of P_LoadLineDefs
//
void ParsedUDMF::loadLineDefs() 
{
   try
   {
      GroupedList *coll = mBlocks.objectForKey("linedef");
      ::numlines = coll ? static_cast<int>(coll->items.getLength()) : 0;
      ::lines = estructalloctag(line_t, ::numlines, PU_LEVEL);

      line_t *ld;
      int v[2];
      int s[2];
      for(int i = 0; i < ::numlines; ++i)
      {
         MetaTable &table = coll->items[i];
         ld = ::lines + i;

         if(E_requireOptBool(table, "blocking"))
            ld->flags |= ML_BLOCKING;
         if(E_requireOptBool(table, "blockmonsters"))
            ld->flags |= ML_BLOCKMONSTERS;
         if(E_requireOptBool(table, "twosided"))
            ld->flags |= ML_TWOSIDED;
         if(E_requireOptBool(table, "dontpegtop"))
            ld->flags |= ML_DONTPEGTOP;
         if(E_requireOptBool(table, "dontpegbottom"))
            ld->flags |= ML_DONTPEGBOTTOM;
         if(E_requireOptBool(table, "secret"))
            ld->flags |= ML_SECRET;
         if(E_requireOptBool(table, "blocksound"))
            ld->flags |= ML_SOUNDBLOCK;
         if(E_requireOptBool(table, "dontdraw"))
            ld->flags |= ML_DONTDRAW;
         if(E_requireOptBool(table, "mapped"))
            ld->flags |= ML_MAPPED;
         if(E_requireOptBool(table, "passuse"))
            ld->flags |= ML_PASSUSE;

         // STRIFE. NAMESPACE REQUIRED
         if(gUDMFNamespace == unsStrife)
         {
            // TODO: support "translucent", "jumpover" and "blockfloaters".
            // Currently the flags don't seem to exist in Eternity
         }
         if(gUDMFNamespace == unsEternity || gUDMFNamespace == unsHexen)
         {
            // FIXME: not sure if Eternity will use the same fields as Hexen.
            // ExtraData has them set up differently

            // Based on spac_flags_tlate
            if(E_requireOptBool(table, "playercross"))
               ld->extflags |= EX_ML_CROSS | EX_ML_PLAYER;
            if(E_requireOptBool(table, "playeruse"))
               ld->extflags |= EX_ML_USE | EX_ML_PLAYER;
            if(E_requireOptBool(table, "monstercross"))
               ld->extflags |= EX_ML_CROSS | EX_ML_MONSTER;
            if(E_requireOptBool(table, "monsteruse"))
               ld->extflags |= EX_ML_USE | EX_ML_MONSTER;
            if(E_requireOptBool(table, "impact"))
               ld->extflags |= EX_ML_IMPACT | EX_ML_MISSILE;
            if(E_requireOptBool(table, "playerpush"))
               ld->extflags |= EX_ML_PUSH | EX_ML_PLAYER;
            if(E_requireOptBool(table, "monsterpush"))
               ld->extflags |= EX_ML_PUSH | EX_ML_MONSTER;
            if(E_requireOptBool(table, "missilecross"))
               ld->extflags |= EX_ML_CROSS | EX_ML_MISSILE;
            if(E_requireOptBool(table, "repeatspecial"))
               ld->extflags |= EX_ML_REPEAT;
         }
         ld->special = E_requireOptInt(table, "special");
         // Special handling for tag
         if(gUDMFNamespace == unsDoom || gUDMFNamespace == unsHeretic
            || gUDMFNamespace == unsStrife)
         {
            int tag[2];
            tag[0] = E_requireOptInt(table, "id");
            tag[1] = E_requireOptInt(table, "arg0");
            ld->tag = tag[1] ? tag[1] : tag[0]; // support at least one of them
         }
         else if(gUDMFNamespace == unsEternity)
         {
            // Eternity
            // TENTATIVE BEHAVIOUR
            ld->tag = E_requireOptInt(table, "id", -1);

            // TENTATIVE TODO: Eternity namespace special "alpha"
            ld->alpha = static_cast<float>(E_requireOptDouble(table, "alpha", 1.0));

            // TODO: equivalent of ExtraData 1SONLY, ADDITIVE, ZONEBOUNDARY, CLIPMIDTEX
         }
         else if(gUDMFNamespace == unsHexen)
         {
            ld->tag = -1;  // Hexen will always use args
         }
         v[0] = E_requireInt(table, "v1");
         v[1] = E_requireInt(table, "v2");
         if(v[0] < 0 || v[0] >= ::numvertexes 
            || v[1] < 0 || v[1] >= ::numvertexes)
         {
            throw UDMFException("Linedef vertex out of bounds", 
               table.getInt(kLineNumberKey, 0));
         }
         ld->v1 = ::vertexes + v[0];
         ld->v2 = ::vertexes + v[1];
         s[0] = E_requireInt(table, "sidefront");
         s[1] = E_requireOptInt(table, "sideback", -1);
         if(s[0] < 0 || s[0] >= ::numsides || s[1] < -1 || s[1] >= ::numsides)
         {
            throw UDMFException("Linedef sidedef out of bounds", 
               table.getInt(kLineNumberKey, 0));
         }

         ld->sidenum[0] = s[0];
         ld->sidenum[1] = s[1];
         P_InitLineDef(ld);

         // FIXME: should ExtraData records still be able to override UDMF?
         // Currently they aren't

         // Load args if Hexen or EE
         if(gUDMFNamespace == unsEternity || gUDMFNamespace == unsHexen)
         {
            ld->args[0] = E_requireOptInt(table, "args0");
            ld->args[1] = E_requireOptInt(table, "args1");
            ld->args[2] = E_requireOptInt(table, "args2");
            ld->args[3] = E_requireOptInt(table, "args3");
            ld->args[4] = E_requireOptInt(table, "args4");
         }
         
         P_PostProcessLineFlags(ld);
      }
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
   }
}

//
// ParsedUDMF::loadSideDefs2
//
// Equivalent of P_LoadSideDefs2
//
void ParsedUDMF::loadSideDefs2() const
{
   try
   {
      GroupedList *coll = mBlocks.objectForKey("sidedef");

      int i;
      side_t *sd;

      const char *topTexture, *bottomTexture, *midTexture;
      int secnum;

      for(i = 0; i < ::numsides; ++i)
      {
         sd = sides + i;
         MetaTable &table = coll->items[i];
         sd->textureoffset = E_requireOptInt(table, "offsetx") << FRACBITS;
         sd->rowoffset = E_requireOptInt(table, "offsety") << FRACBITS;

         topTexture = E_requireOptString(table, "texturetop", "-");
         bottomTexture = E_requireOptString(table, "texturebottom", "-");
         midTexture = E_requireOptString(table, "texturemiddle", "-");

         secnum = E_requireInt(table, "sector");

         if(secnum < 0 || secnum >= ::numsectors)
         {
            throw UDMFException("Sidedef sector out of bounds", 
                  table.getInt(kLineNumberKey, 0));
         }

         sd->sector = ::sectors + secnum;

         P_SetupSidedefTextures(*sd, bottomTexture, midTexture, topTexture);
      }
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
   }
}

//
// ParsedUDMF::loadThings
//
// Load things, like P_LoadThings
//
void ParsedUDMF::loadThings() const
{
   mapthing_t *mapthings = nullptr;
   try
   {
      GroupedList *coll = mBlocks.objectForKey("thing");
      ::numthings = coll ? static_cast<int>(coll->items.getLength()) : 0;

      mapthings = ecalloc(mapthing_t *, numthings, sizeof(mapthing_t));
      mapthing_t *ft;

      bool young, nightmare;

      for(int i = 0; i < ::numthings; ++i)
      {
         MetaTable &table = coll->items[i];
         ft = mapthings + i;

         ft->type = E_requireInt(table, "type");
         if(P_CheckThingDoomBan(ft->type))
            continue;

         // FIXME: due to current layout, FRACTIONARY PARTS ARE LOST. Probably
         // not recommended anyway, because of platform strtod inaccuracies.
         ft->x = static_cast<int16_t>(floor(E_requireDouble(table, "x") + 0.5));
         ft->y = static_cast<int16_t>(floor(E_requireDouble(table, "y") + 0.5));
         ft->angle = static_cast<int16_t>(E_requireOptInt(table, "angle"));

         // FIXME: handle young and nightmare skills in separate booleans.
         // Thankfully the skill levels are only checked in P_SpawnMapThing, not
         // elsewhere in mapthing_t. BUT THIS IS A BIG ***HACK***
         young = E_requireOptBool(table, "skill1");
         if(E_requireOptBool(table, "skill2"))
            ft->options |= MTF_EASY;
         if(E_requireOptBool(table, "skill3"))
            ft->options |= MTF_NORMAL;
         if(E_requireOptBool(table, "skill4"))
            ft->options |= MTF_HARD;
         nightmare = E_requireOptBool(table, "skill5");

         if(E_requireOptBool(table, "ambush"))
            ft->options |= MTF_AMBUSH;
         
         // negative
         if(!E_requireOptBool(table, "single"))
            ft->options |= MTF_NOTSINGLE;
         if(!E_requireOptBool(table, "dm"))
            ft->options |= MTF_NOTDM;
         if(!E_requireOptBool(table, "coop"))
            ft->options |= MTF_NOTCOOP;

         if(gUDMFNamespace == unsDoom || gUDMFNamespace == unsEternity)
         {
            if(E_requireOptBool(table, "friend"))
               ft->options |= MTF_FRIEND;
         }

         if(gUDMFNamespace == unsHexen || gUDMFNamespace == unsEternity)
         {
            if(E_requireOptBool(table, "dormant"))
               ft->options |= MTF_DORMANT;
            ft->tid = static_cast<int16_t>(E_requireOptInt(table, "id"));
            ft->height = static_cast<int16_t>(E_requireOptInt(table, "height"));
            ft->args[0] = E_requireOptInt(table, "arg0");
            ft->args[1] = E_requireOptInt(table, "arg1");
            ft->args[2] = E_requireOptInt(table, "arg2");
            ft->args[3] = E_requireOptInt(table, "arg3");
            ft->args[4] = E_requireOptInt(table, "arg4");
         }

         // TODO: Hexen classes!

         // TODO: Strife standing
         // TODO: Strife ally
         // TODO: Strife shadow
         // TODO: Strife invisible

         if(gUDMFNamespace == unsHeretic)
            P_ConvertHereticThing(ft);
         
         P_ConvertDoomExtendedSpawnNum(ft);

         // use the extra parameters here
         P_SpawnMapThing(ft, young ? 1 : 0, nightmare ? 1 : 0);
      }

      // do the player start check like in P_LoadThings
      if(GameType != gt_dm)
      {
         for(int i = 0; i < MAXPLAYERS; i++)
         {
            if(playeringame[i] && !players[i].mo)
               level_error = "Missing required player start";
         }
      } 
   
      efree(mapthings);
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
      efree(mapthings);
   }
}

// EOF

