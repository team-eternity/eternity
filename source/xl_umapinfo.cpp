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

#include <assert.h>
#include "z_zone.h"

#include "e_hash.h"
#include "in_lude.h"
#include "metaqstring.h"
#include "w_wad.h"
#include "xl_scripts.h"
#include "xl_umapinfo.h"

//
// UMAPINFO property data type
//
enum class PropertyType
{
   setString,  // assign a string (replacing anything before it)
   setStringOrClear, // assign a string or accept 'clear'
   setMultiStringOrClear,  // assign one or more strings, or 'clear'
   setInteger, // assign an integer number (replacing anything before it)
   setBoolean, // true or false
   setTrue, // only accept 'true'
   addEpisodeOrClear, // accept three strings, or a single 'clear'
   addBossActionOrClear,   // accept <identifier>, <int>, <int> or 'clear'
};

//
// UMAPINFO property rule
//
struct propertyrule_t
{
   // hash table items
   const char *name;
   DLListItem<propertyrule_t> link;

   PropertyType type;
};

static EHashTable<propertyrule_t, ENCStringHashKey, &propertyrule_t::name, &propertyrule_t::link> rules;

// UMAPINFO entry maintenance
static MetaTable umapInfoTable;

//
// Creates a new metatable to hold data for a global UMAPINFO level entry and
// add it to the umapInfoTable
//
static MetaTable *XL_newUMapInfo(const char *levelname)
{
   MetaTable *ret;
   if((ret = umapInfoTable.getMetaTable(levelname, nullptr)))
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
   int valueIndex;      // index of value in a multi-parameter option
   bool gotClear;       // whether the value got 'clear'

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
   {
      I_Error("UMAPINFO: expected 'map' at the top level\n");
      return false;
   }
   state = STATE_EXPECTMAPNAME;
   return true;
}

//
// Expect the lump name
//
bool XLUMapInfoParser::doStateExpectMapName(XLTokenizer &tokenizer)
{
   if(tokenizer.getTokenType() != XLTokenizer::TOKEN_KEYWORD)
   {
      I_Error("UMAPINFO: expected map name after 'map' keyword\n");
      return false;
   }

   // NOTE: do not restrict to certain names, unlike the base specs

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
   {
      I_Error("UMAPINFO: expected '{' after map '%s'\n", curInfo->getKey());
      return false;
   }
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

   // Check the key
   const qstring &newkey = tokenizer.getToken();
   if(!rules.objectForKey(newkey.constPtr()))
   {
      // Don't allow unrecognized properties
      I_Error("UMAPINFO: unexpected property '%s' in map '%s'\n", newkey.constPtr(),
              curInfo->getKey());
   }

   key = newkey;
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
      valueIndex = 0;
      gotClear = false;
      state = STATE_EXPECTVALUE;
      return true;
   }
   I_Error("UMAPINFO: expected '=' after key '%s' in map '%s'\n", key.constPtr(),
           curInfo->getKey());
   return false;
}

//
// Expect value after equal.
//
bool XLUMapInfoParser::doStateExpectValue(XLTokenizer &tokenizer)
{
   value = tokenizer.getToken();
   int type = tokenizer.getTokenType();

   const propertyrule_t *rule = rules.objectForKey(key.constPtr());
   I_Assert(rule, "Key '%s' unexpectedly passed\n", key.constPtr());

   //
   // Helper to check if it's a number
   //
   auto isNumber = [this, type]()
   {
      if(type != XLTokenizer::TOKEN_KEYWORD)
         return false;
      char *endptr;
      strtol(value.constPtr(), &endptr, 10);
      return !*endptr;
   };

   // Check the data type
   bool error = false;
   switch(rule->type)
   {
      case PropertyType::setString:
         error = valueIndex >= 1 || type != XLTokenizer::TOKEN_STRING;
         break;
      case PropertyType::setStringOrClear:
         error = valueIndex >= 1 || (type != XLTokenizer::TOKEN_STRING &&
                                     value.strCaseCmp("clear"));
         break;
      case PropertyType::setMultiStringOrClear:
         error = (gotClear && valueIndex >= 1) || (valueIndex >= 1 && type !=
                                                   XLTokenizer::TOKEN_STRING) ||
         (valueIndex == 0 && type != XLTokenizer::TOKEN_STRING && value.strCaseCmp("clear"));
         break;
      case PropertyType::setInteger:
         error = valueIndex >= 1 || !isNumber();
         break;
      case PropertyType::setBoolean:
         error = valueIndex >= 1 || type != XLTokenizer::TOKEN_KEYWORD ||
         (value.strCaseCmp("true") && value.strCaseCmp("false"));
         break;
      case PropertyType::setTrue:
         error = valueIndex >= 1 || type != XLTokenizer::TOKEN_KEYWORD || value.strCaseCmp("true");
         break;
      case PropertyType::addEpisodeOrClear:
         error = (gotClear && valueIndex >= 1) ||
         (valueIndex >= 1 && type != XLTokenizer::TOKEN_STRING) || valueIndex >= 3 ||
         (valueIndex == 0 && type != XLTokenizer::TOKEN_STRING && value.strCaseCmp("clear"));
         break;
      case PropertyType::addBossActionOrClear:
         error = (gotClear && valueIndex >= 1) || type != XLTokenizer::TOKEN_KEYWORD ||
         valueIndex >= 3 || (valueIndex >= 1 && !isNumber());
         break;
   }
   if(error)
   {
      I_Error("UMAPINFO: invalid value for property '%s' for map '%s'\n", key.constPtr(),
              curInfo->getKey());
   }

   // Now that validation is out of our way, do the assignment
   MetaTable *subtable;
   switch(rule->type)
   {
      case PropertyType::setString:
         curInfo->setString(key.constPtr(), value.constPtr());
         break;
      case PropertyType::setStringOrClear:
         if(type == XLTokenizer::TOKEN_KEYWORD)
         {
            curInfo->removeString(key.constPtr());
            curInfo->setInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
            gotClear = true;
         }
         else
         {
            curInfo->removeInt(key.constPtr());
            curInfo->setString(key.constPtr(), value.constPtr());
         }
         break;
      case PropertyType::setMultiStringOrClear:
         if(type == XLTokenizer::TOKEN_KEYWORD)
         {
            curInfo->removeAndDeleteAllObjects(key.constPtr());
            curInfo->setInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
            gotClear = true;
         }
         else
         {
            if(!valueIndex)
               curInfo->removeAndDeleteAllObjects(key.constPtr());
            curInfo->addString(key.constPtr(), value.constPtr());
         }
         break;
      case PropertyType::setInteger:
         curInfo->setInt(key.constPtr(), value.toInt());
         break;
      case PropertyType::setBoolean:
         curInfo->setInt(key.constPtr(), !value.strCaseCmp("false") ? XL_UMAPINFO_SPECVAL_FALSE :
                         XL_UMAPINFO_SPECVAL_TRUE);
         break;
      case PropertyType::setTrue:
         curInfo->setInt(key.constPtr(), XL_UMAPINFO_SPECVAL_TRUE);
         break;
      case PropertyType::addEpisodeOrClear:
         if(!valueIndex)
         {
            if(!value.strCaseCmp("clear"))
               curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
            else
            {
               subtable = new MetaTable;
               subtable->setString("patch", value.constPtr());
               curInfo->addMetaTable(key.constPtr(), subtable);
            }
         }
         else
         {
            subtable = curInfo->getMetaTable(key.constPtr(), nullptr);
            I_Assert(subtable, "Expected to have a table here");
            if(valueIndex == 1)
               subtable->setString("name", value.constPtr());
            else if(valueIndex == 2)
               subtable->setString("key", value.constPtr());
         }
         break;
      case PropertyType::addBossActionOrClear:
         if(!valueIndex)
         {
            if(!value.strCaseCmp("clear"))
               curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
            else
            {
               subtable = new MetaTable;
               subtable->setString("thingtype", value.constPtr());
               curInfo->addMetaTable(key.constPtr(), subtable);
            }
         }
         else
         {
            subtable = curInfo->getMetaTable(key.constPtr(), nullptr);
            I_Assert(subtable, "Expected to have a table here");
            if(valueIndex == 1)
               subtable->setInt("linespecial", value.toInt());
            else if(valueIndex == 2)
               subtable->setInt("tag", value.toInt());
         }
         break;
   }

   state = STATE_POSTVALUE;
   return true;
}

//
// Expect comma or }
//
bool XLUMapInfoParser::doStatePostValue(XLTokenizer &tokenizer)
{
   // NOTE: the error checking will happen in the next value, if a comma is added
   if(tokenizer.getToken() == ",")
   {
      ++valueIndex;
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
   I_Error("UMAPINFO: unexpected token '%s' in map '%s'\n",
           tokenizer.getToken().constPtr(), curInfo->getKey());
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
                           XLTokenizer::TF_STRINGSQUOTED | XLTokenizer::TF_SLASHCOMMENTS);
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
   return umapInfoTable.getMetaTable(mapname, nullptr);
}

//
// Initialize the rules
//
static void XL_initRules()
{
   if(rules.getNumItems())
      return;

   auto addRule = [](const char *name, PropertyType type)
   {
      propertyrule_t *rule;
      rule = estructalloc(propertyrule_t, 1);
      rule->name = name;
      rule->type = type;
      rules.addObject(rule);
   };

   addRule("levelname", PropertyType::setString);
   addRule("label", PropertyType::setStringOrClear);
   addRule("levelpic", PropertyType::setString);
   addRule("next", PropertyType::setString);
   addRule("nextsecret", PropertyType::setString);
   addRule("skytexture", PropertyType::setString);
   addRule("music", PropertyType::setString);
   addRule("exitpic", PropertyType::setString);
   addRule("enterpic", PropertyType::setString);
   addRule("partime", PropertyType::setInteger);
   addRule("endgame", PropertyType::setBoolean);
   addRule("endpic", PropertyType::setString);
   addRule("endbunny", PropertyType::setTrue);
   addRule("endcast", PropertyType::setTrue);
   addRule("nointermission", PropertyType::setBoolean);
   addRule("intertext", PropertyType::setStringOrClear);
   addRule("intertextsecret", PropertyType::setStringOrClear);
   addRule("interbackdrop", PropertyType::setString);
   addRule("intermusic", PropertyType::setString);
   addRule("episode", PropertyType::addEpisodeOrClear);
   addRule("bossaction", PropertyType::addBossActionOrClear);
}

//
// Parses the UMAPINFO lump
//
void XL_ParseUMapInfo()
{
   XL_initRules();

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

      const char *str = level->getString("levelname", nullptr);
      if(str)
         info.levelname = str;

      str = level->getString("levelpic", nullptr);
      if(str)
         info.levelpic = str;

      str = level->getString("enterpic", nullptr);
      if(str)
         info.enterpic = str;

      str = level->getString("exitpic", nullptr);
      if(str)
         info.exitpic = str;
   }
}

// EOF

