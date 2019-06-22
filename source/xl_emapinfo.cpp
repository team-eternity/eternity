// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2014 James Haley et al.
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
// Description: 
//   SMMU/Eternity EMAPINFO Parser
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "e_lib.h"
#include "in_lude.h"
#include "m_qstr.h"
#include "metaapi.h"
#include "v_misc.h"
#include "w_wad.h"
#include "xl_scripts.h"

//=============================================================================
//
// EMAPINFO Entry Maintenance
//

static MetaTable emapInfoTable;

//
// XL_newEMapInfo
//
// Create a new metatable to hold data for a global EMAPINFO level entry, and
// add it to the emapInfoTable.
//
static MetaTable *XL_newEMapInfo(const char *levelname)
{
   MetaTable *ret;

   if((ret = emapInfoTable.getObjectKeyAndTypeEx<MetaTable>(levelname)))
      ret->clearTable();
   else
   {
      ret = new MetaTable(levelname);
      emapInfoTable.addObject(ret);
   }

   return ret;
}

//
// XL_newLevelInfo
//
// Create a temporary MetaTable to hold level header MapInfo data, for SMMU
// compatibility.
//
static MetaTable *XL_newLevelInfo()
{
   return new MetaTable("level info");
}

//=============================================================================
//
// EMAPINFO Parser
//

class XLEMapInfoParser : public XLParser
{
protected:

   // State table declaration
   static bool (XLEMapInfoParser::*States[])(XLTokenizer &);

   // parser state enumeration
   enum
   {
      STATE_EXPECTHEADER,  // need a bracketed header
      STATE_EXPECTKEYWORD, // expecting a mapinfo field keyword
      STATE_EXPECTEQUAL,   // expecting optional = or start of next token
      STATE_EXPECTVALUE,   // expecting value for mapinfo keyword
      STATE_EXPECTEOL,     // expecting end-of-line token
      STATE_SKIPSECTION    // scan until next header
   };

   // state handlers
   bool doStateExpectHeader(XLTokenizer &);
   bool doStateExpectKeyword(XLTokenizer &);
   bool doStateExpectEqual(XLTokenizer &);
   bool doStateExpectValue(XLTokenizer &);
   bool doStateExpectEOL(XLTokenizer &);
   bool doStateSkipSection(XLTokenizer &);

   // parser state data
   int        state;
   int        nextstate;
   bool       isGlobal;
   MetaTable *curInfo;
   qstring    key;
   qstring    value;
   bool       allowMultiValue;

   virtual bool doToken(XLTokenizer &token);
   virtual void startLump();
   virtual void initTokenizer(XLTokenizer &tokenizer);
   virtual void onEOF(bool early);

public:
   XLEMapInfoParser() 
      : XLParser("EMAPINFO"), state(STATE_EXPECTHEADER), nextstate(-1),
        isGlobal(true), curInfo(NULL), key(), value(), allowMultiValue(false)
   {
   }

   void setGlobalMode(bool global) { isGlobal = global; }
   
   MetaTable *getCurrentInfo() const { return curInfo; }
};

// State Table
bool (XLEMapInfoParser::* XLEMapInfoParser::States[])(XLTokenizer &) =
{
   &XLEMapInfoParser::doStateExpectHeader,
   &XLEMapInfoParser::doStateExpectKeyword,
   &XLEMapInfoParser::doStateExpectEqual,
   &XLEMapInfoParser::doStateExpectValue,
   &XLEMapInfoParser::doStateExpectEOL,
   &XLEMapInfoParser::doStateSkipSection
};

// Dispatch token to appropriate state handler
bool XLEMapInfoParser::doToken(XLTokenizer &token)
{
   return (this->*States[state])(token);
}

// Reinitialize parser at beginning of lump parsing
void XLEMapInfoParser::startLump()
{
   state     = STATE_EXPECTHEADER;
   nextstate = -1;
   curInfo   = NULL;
   key       = "";
   value     = "";
   allowMultiValue = false;
}

// Setup tokenizer state before parsing begins
void XLEMapInfoParser::initTokenizer(XLTokenizer &tokenizer)
{
   tokenizer.setTokenFlags(
      XLTokenizer::TF_LINEBREAKS   |  // linebreaks are treated as significant
      XLTokenizer::TF_BRACKETS     |  // support [keyword] tokens
      XLTokenizer::TF_HASHCOMMENTS |  // # can start a comment as well as ;
      XLTokenizer::TF_SLASHCOMMENTS); // support C++-style comments too.
}

// Handle EOF
void XLEMapInfoParser::onEOF(bool early)
{
   if(state == STATE_EXPECTVALUE)
   {
      // if we ended in STATE_EXPECTVALUE and there is a currently valid
      // info, key, and value, we need to push it.
      if(curInfo && key.length() && value.length())
         curInfo->setString(key.constPtr(), value.constPtr());
   }
}

// Expecting a bracketed header to start a new definition
bool XLEMapInfoParser::doStateExpectHeader(XLTokenizer &tokenizer)
{
   // if not a bracketed token, keep scanning
   if(tokenizer.getTokenType() != XLTokenizer::TOKEN_BRACKETSTR)
      return true;

   if(!isGlobal)
   {
      // if not in global mode, the only recognized header is "level info"
      if(!tokenizer.getToken().strCaseCmp("level info"))
      {
         // in level header mode, all [level info] blocks accumulate into a
         // single definition
         if(!curInfo)
            curInfo = XL_newLevelInfo();
         nextstate = STATE_EXPECTKEYWORD;
      }
      else
         nextstate = STATE_SKIPSECTION;

      state = STATE_EXPECTEOL;
   }
   else
   {
      // in EMAPINFO, only map names are allowed; each populates its own
      // MetaTable object, and the newest definition for a given map is the
      // one which entirely wins (any data from a previous section with the
      // same name will be obliterated by a later definition).
      curInfo   = XL_newEMapInfo(tokenizer.getToken().constPtr());
      nextstate = STATE_EXPECTKEYWORD;
      state     = STATE_EXPECTEOL;
   }

   return true;
}

enum multivalkw_e
{
   MVKW_LEVELACTION,
   MVKW_NUMKEYWORDS
};

// Keywords which allow multiple values
static const char *multiValKeywords[MVKW_NUMKEYWORDS] =
{
   "levelaction"
};

// Expecting a MapInfo keyword
bool XLEMapInfoParser::doStateExpectKeyword(XLTokenizer &tokenizer)
{
   int tokenType = tokenizer.getTokenType();
   
   switch(tokenType)
   {
   case XLTokenizer::TOKEN_BRACKETSTR:
      // if we find a bracketed string instead, it's actually the start of a new
      // section; process it through the header state handler immediately
      return doStateExpectHeader(tokenizer);

   case XLTokenizer::TOKEN_KEYWORD:
   case XLTokenizer::TOKEN_STRING:
      // record as the current key and expect potential = to follow
      key   = tokenizer.getToken();
      value = "";
      state = STATE_EXPECTEQUAL;
      if(E_StrToNumLinear(multiValKeywords, MVKW_NUMKEYWORDS, key.constPtr()) != MVKW_NUMKEYWORDS)
         allowMultiValue = true;
      else
         allowMultiValue = false;
      break;

   default:
      // if we see anything else, keep scanning
      break;
   }

   return true;
}

// Expecting optional = sign, or the start of the value
bool XLEMapInfoParser::doStateExpectEqual(XLTokenizer &tokenizer)
{
   if(tokenizer.getToken() == "=")
   {
      // found an optional =, expect value.
      state = STATE_EXPECTVALUE;
      return true;
   }
   else
   {
      // otherwise, this is the value token; process it immediately
      return doStateExpectValue(tokenizer);
   }
}

// Expecting value
bool XLEMapInfoParser::doStateExpectValue(XLTokenizer &tokenizer)
{
   int tokenType = tokenizer.getTokenType();

   // remain in this state until a TOKEN_LINEBREAK is encountered
   if(tokenType == XLTokenizer::TOKEN_LINEBREAK)
   {
      // push the key/value pair
      if(key.length() && value.length())
      {
         if(allowMultiValue)
            curInfo->addString(key.constPtr(), value.constPtr());
         else
            curInfo->setString(key.constPtr(), value.constPtr());
      }
      state = STATE_EXPECTKEYWORD;
      key   = "";
      value = "";
   }
   else
   {
      // add token into the accumulating value string; subsequent tokens will
      // be separated by a single whitespace in the output
      if(value.length())
         value << " ";
      value << tokenizer.getToken();
      state = STATE_EXPECTVALUE; // ensure we come back here
   }

   return true;
}

// Expecting the end of the line
bool XLEMapInfoParser::doStateExpectEOL(XLTokenizer &tokenizer)
{
   if(tokenizer.getTokenType() != XLTokenizer::TOKEN_LINEBREAK)
      return false; // error
   
   state = nextstate;
   return true;
}

// Skip forward until the next section header token
bool XLEMapInfoParser::doStateSkipSection(XLTokenizer &tokenizer)
{
   // ignore all tokens until another section header appears
   if(tokenizer.getTokenType() != XLTokenizer::TOKEN_BRACKETSTR)
      return true;

   // process this token immediately through doStateExpectHeader
   return doStateExpectHeader(tokenizer);
}

//=============================================================================
//
// External Interface
//

//
// XL_EMapInfoForMapName
//
// Retrieve an EMAPINFO definition by map name.
//
const MetaTable *XL_EMapInfoForMapName(const char *mapname)
{
   return emapInfoTable.getObjectKeyAndTypeEx<MetaTable>(mapname);
}

//
// XL_EMapInfoForMapNum
//
// Retrieve and EMAPINFO definition by episode and map. Send in episode
// 0 for MAPxy maps.
//
MetaTable *XL_EMapInfoForMapNum(int episode, int map)
{
   qstring mapname;

   if(episode > 0)
      mapname.Printf(9, "E%dM%d", episode, map);
   else
      mapname.Printf(9, "MAP%02d", map);

   return emapInfoTable.getObjectKeyAndTypeEx<MetaTable>(mapname.constPtr());
}

//
// XL_ParseEMapInfo
//
// Parse all EMAPINFO lumps in the global wad directory that haven't been 
// parsed already.
//
void XL_ParseEMapInfo()
{
   XLEMapInfoParser parser;

   parser.parseAll(wGlobalDir);
}

//
// XL_ParseLevelInfo
//
// Given a WadDirectory and lump number, parse that specific lump for 
// [level info] definitions. All will accumulate into a single MetaTable, if
// any definitions were present, and the object will be returned. Ownership
// of the MetaTable is assumed by the caller.
//
MetaTable *XL_ParseLevelInfo(WadDirectory *dir, int lumpnum)
{
   XLEMapInfoParser parser;

   parser.setGlobalMode(false); // put into [level info] parsing mode
   parser.parseLump(*dir, dir->getLumpInfo()[lumpnum], false);

   return parser.getCurrentInfo();
}

//
// Builds the intermission info from EMAPINFO
//
void XL_BuildInterEMapInfo()
{
   MetaTable *level = nullptr;
   while((level = emapInfoTable.getNextTypeEx(level)))
   {
      intermapinfo_t &info = IN_GetMapInfo(level->getKey());

      auto str = level->getString("inter-levelname", "");
      if(*str)
         info.levelname = str;

      str = level->getString("levelpic", "");
      if(*str)
         info.levelpic = str;

      // TODO: enterpic not implemented yet

      str = level->getString("interpic", "");
      if(*str)
         info.exitpic = str;
   }
}

//=============================================================================
//
// Console Commands
//

//
// xl_dumpmapinfo
//
// Display information on a single MAPINFO entry by map name.
//
CONSOLE_COMMAND(xl_dumpemapinfo, 0)
{
   if(Console.argc < 1)
   {
      C_Printf("Usage: xl_dumpemapinfo mapname\n");
      return;
   }

   const MetaTable *mapInfo = NULL;
   if((mapInfo = XL_EMapInfoForMapName(Console.argv[0]->constPtr())))
   {
      const MetaObject *obj = NULL;

      C_Printf("EMAPINFO Entry for %s:\n", mapInfo->getKey());
      while((obj = mapInfo->tableIterator(obj)))
         C_Printf(FC_HI "%s: " FC_NORMAL "%s\n", obj->getKey(), obj->toString());
   }
   else
      C_Printf(FC_ERROR "No EMAPINFO defined for %s\n", Console.argv[0]->constPtr());
}


// EOF

