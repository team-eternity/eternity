//
// The Eternity Engine
// Copyright (C) 2017 James Haley et al.
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
// Purpose: UMAPINFO lump, invented by Graf Zahl for cross-port support
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "in_lude.h"
#include "metaqstring.h"
#include "w_wad.h"
#include "xl_scripts.h"
#include "xl_umapinfo.h"

// UMAPINFO entry maintenance
static MetaTable umapInfoTable;

//
// Creates a new metatable to hold data for a global UMAPINFO level entry and
// add it to the umapInfoTable
//
static MetaTable *XL_newUMapInfo(const char *levelname)
{
   MetaTable *ret;
   if((ret = umapInfoTable.getObjectKeyAndTypeEx<MetaTable>(levelname)))
      ret->clearTable();
   else
   {
      ret = new MetaTable(levelname);
      umapInfoTable.addObject(ret);
   }
   return ret;
}

//
// The parser
//
class XLUMapInfoParser final : public XLParser
{
   static bool (XLUMapInfoParser::*States[])(XLTokenizer &);

   //
   // The states
   //
   enum
   {
      STATE_EXPECTMAP,        // before a "map" keyword
      STATE_EXPECTMAPNAME,    // before the map lump name
      STATE_EXPECTOPENBRACE,  // before {
      STATE_EXPECTKEY,        // before the key name or }
      STATE_EXPECTEQUAL,      // before =
      STATE_EXPECTVALUE,      // before ""
      STATE_POSTVALUE,        // before , or }
   };

   bool doStateExpectMap(XLTokenizer &);
   bool doStateExpectMapName(XLTokenizer &);
   bool doStateExpectOpenBrace(XLTokenizer &);
   bool doStateExpectKey(XLTokenizer &);
   bool doStateExpectEqual(XLTokenizer &);
   bool doStateExpectValue(XLTokenizer &);
   bool doStatePostValue(XLTokenizer &);

   int state;           // current state
   MetaTable *curInfo;  // level info
   qstring key;         // key string
   qstring value;       // value string

   bool doToken(XLTokenizer &token) override;
   void startLump() override;
   void initTokenizer(XLTokenizer &tokenizer) override;
   void onEOF(bool early) override;

public:
   XLUMapInfoParser() : XLParser("UMAPINFO"), state(STATE_EXPECTMAP),
   curInfo(nullptr)
   {
   }
};

//
// State table
//
bool (XLUMapInfoParser::*XLUMapInfoParser::States[])(XLTokenizer &) =
{
   &XLUMapInfoParser::doStateExpectMap,
   &XLUMapInfoParser::doStateExpectMapName,
   &XLUMapInfoParser::doStateExpectOpenBrace,
   &XLUMapInfoParser::doStateExpectKey,
   &XLUMapInfoParser::doStateExpectEqual,
   &XLUMapInfoParser::doStateExpectValue,
   &XLUMapInfoParser::doStatePostValue,
};

//
// Expect "map"
//
bool XLUMapInfoParser::doStateExpectMap(XLTokenizer &tokenizer)
{
   if(tokenizer.getToken().strCaseCmp("map"))
      return false;
   state = STATE_EXPECTMAPNAME;
   return true;
}

//
// Expect the lump name
//
bool XLUMapInfoParser::doStateExpectMapName(XLTokenizer &tokenizer)
{
   if(tokenizer.getTokenType() != XLTokenizer::TOKEN_KEYWORD)
      return false;
   state = STATE_EXPECTOPENBRACE;
   curInfo = XL_newUMapInfo(tokenizer.getToken().constPtr());
   return true;
}

//
// Expect EOL or {
//
bool XLUMapInfoParser::doStateExpectOpenBrace(XLTokenizer &tokenizer)
{
   if(tokenizer.getTokenType() == XLTokenizer::TOKEN_LINEBREAK)
      return true;   // keep looking
   if(tokenizer.getToken() != "{")
      return false;
   state = STATE_EXPECTKEY;
   return true;
}

//
// Expect key or }
//
bool XLUMapInfoParser::doStateExpectKey(XLTokenizer &tokenizer)
{
   if(tokenizer.getToken() == "}")
   {
      state = STATE_EXPECTMAP;
      return true;
   }
   key = tokenizer.getToken();
   value = "";
   state = STATE_EXPECTEQUAL;
   return true;
}

//
// Expect equal
//
bool XLUMapInfoParser::doStateExpectEqual(XLTokenizer &tokenizer)
{
   if(tokenizer.getToken() == "=")
   {
      state = STATE_EXPECTVALUE;
      return true;
   }
   return false;
}

//
// Expect value after equal.
//
bool XLUMapInfoParser::doStateExpectValue(XLTokenizer &tokenizer)
{
   value << tokenizer.getToken();

   if(tokenizer.getTokenType() == XLTokenizer::TOKEN_STRING)
      curInfo->addString(key.constPtr(), value.constPtr());
   else if(tokenizer.getTokenType() == XLTokenizer::TOKEN_KEYWORD)
   {
      char *endptr = nullptr;
      long val = value.toLong(&endptr, 10);
      if(!*endptr)
         curInfo->addInt(key.constPtr(), (int)val);
      else if(!value.strCaseCmp("clear"))
         curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
      else if(!value.strCaseCmp("false"))
         curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_FALSE);
      else if(!value.strCaseCmp("true"))
         curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_TRUE);
      else
         return false;  // FAILURE
   }

   state = STATE_POSTVALUE;
   return true;
}

//
// Expect comma or }
//
bool XLUMapInfoParser::doStatePostValue(XLTokenizer &tokenizer)
{
   if(tokenizer.getToken() == ",")
   {
      state = STATE_EXPECTVALUE;
      return true;
   }
   if(tokenizer.getToken() == "}")
   {
      state = STATE_EXPECTMAP;
      return true;
   }
   if(tokenizer.getTokenType() == XLTokenizer::TOKEN_KEYWORD)
   {
      // Otherwise it's a new key
      return doStateExpectKey(tokenizer);
   }
   return false;
}

//
// Does a token
//
bool XLUMapInfoParser::doToken(XLTokenizer &token)
{
   return (this->*States[state])(token);
}

//
// Start the lump
//
void XLUMapInfoParser::startLump()
{
   state = STATE_EXPECTMAP;
   curInfo = nullptr;
   key = "";
   value = "";
}

//
// Initializes the tokenizer
//
void XLUMapInfoParser::initTokenizer(XLTokenizer &tokenizer)
{
   tokenizer.setTokenFlags(XLTokenizer::TF_OPERATORS | XLTokenizer::TF_ESCAPESTRINGS |
                           XLTokenizer::TF_STRINGSQUOTED);
}

//
// On end of file
//
void XLUMapInfoParser::onEOF(bool early)
{
}

//
// Gets the metatable from a UMAPINFO level
//
MetaTable *XL_UMapInfoForMapName(const char *mapname)
{
   return umapInfoTable.getObjectKeyAndTypeEx<MetaTable>(mapname);
}

//
// Parses the UMAPINFO lump
//
void XL_ParseUMapInfo()
{
   XLUMapInfoParser parser;
   parser.parseAll(wGlobalDir);
}

//
// Builds the intermission information from UMAPINFO
//
void XL_BuildInterUMapInfo()
{
   MetaTable *level = nullptr;
   while((level = umapInfoTable.getNextTypeEx(level)))
   {
      intermapinfo_t &info = IN_GetMapInfo(level->getKey());

      auto str = level->getObjectKeyAndTypeEx<MetaString>("levelname");
      if(str)
         info.levelname = str->getValue();

      str = level->getObjectKeyAndTypeEx<MetaString>("levelpic");
      if(str)
         info.levelpic = str->getValue();

      str = level->getObjectKeyAndTypeEx<MetaString>("enterpic");
      if(str)
         info.enterpic = str->getValue();

      str = level->getObjectKeyAndTypeEx<MetaString>("exitpic");
      if(str)
         info.exitpic = str->getValue();
   }
}

// EOF

