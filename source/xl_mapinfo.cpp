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
//   Hexen MAPINFO Parser
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_main.h"
#include "e_lib.h"
#include "metaapi.h"
#include "v_misc.h"
#include "w_wad.h"
#include "xl_mapinfo.h"
#include "xl_scripts.h"

//=============================================================================
//
// MAPINFO Data Maintenance
//

static MetaTable defaultMap;
static MetaTable mapInfoTable;

//
// XL_newMapInfo
//
// Create a new XLMapInfo structure, add it to the hash table, and return it.
//
static MetaTable *XL_newMapInfo(const qstring &map, const qstring &name)
{
   MetaTable *ret;

   if((ret = mapInfoTable.getObjectKeyAndTypeEx<MetaTable>(map.constPtr())))
      ret->clearTable();
   else
   {
      ret = new MetaTable(map.constPtr());
      mapInfoTable.addObject(ret);
   }
   
   ret->setString("name", name.constPtr());
   
   return ret;
}

//=============================================================================
//
// MAPINFO Parser
//

struct xlmikeyword_t;

class XLMapInfoParser : public XLParser
{
public:
   static const char *mapKeywords[XL_NUMMAPINFO_FIELDS];

   // global keywords enum
   enum
   {
      KW_GLOBAL_MAP,
      KW_GLOBAL_CD_START,
      KW_GLOBAL_CD_END1,
      KW_GLOBAL_CD_END2,
      KW_GLOBAL_CD_END3,
      KW_GLOBAL_CD_INTRM,
      KW_GLOBAL_CD_TITLE,
      KW_GLOBAL_DEFAULTMAP,
     
      KW_NUMGLOBAL
   };

protected:
   static const char *globalKeywords[KW_NUMGLOBAL];

   // State table declaration
   static bool (XLMapInfoParser::*States[])(XLTokenizer &);

   // parser state enumeration
   enum
   {
      STATE_EXPECTCMD,       // expecting a global command
      STATE_EXPECTGLOBALVAL, // expecting value for a global command
      STATE_EXPECTMAPNAME,   // expecting map number (or name as ZDoom ext.)
      STATE_EXPECTMAPDNAME,  // expecting map display name, for automap etc.
      STATE_EXPECTMAPCMD,    // expecting a map command
      STATE_EXPECTSKYNAME,   // expecting name for a sky texture
      STATE_EXPECTSKYNUM,    // expecting number for sky delta
      STATE_EXPECTVALUE      // expecting value
   };

   // state handlers
   bool doStateExpectCmd(XLTokenizer &);
   bool doStateExpectGlobalVal(XLTokenizer &);
   bool doStateExpectMapName(XLTokenizer &);
   bool doStateExpectMapDName(XLTokenizer &);
   bool doStateExpectMapCmd(XLTokenizer &);
   bool doStateExpectSkyName(XLTokenizer &);
   bool doStateExpectSkyNum(XLTokenizer &);
   bool doStateExpectValue(XLTokenizer &);

   // parser state data
   int            state;
   int            globalKW;
   MetaTable     *curInfo;
   qstring        mapName;
   xlmikeyword_t *kwd;
   bool classicHexenMode;  // use ZDoom's method of determining how to compute some fields

   virtual bool doToken(XLTokenizer &token) override;
   virtual void startLump() override;

public:
   XLMapInfoParser() 
      : XLParser("MAPINFO"), state(STATE_EXPECTCMD), globalKW(KW_NUMGLOBAL),
        curInfo(nullptr), mapName(), kwd(nullptr), classicHexenMode()
   {
   }
};

// Keywords allowed in the global parsing context
const char *XLMapInfoParser::globalKeywords[KW_NUMGLOBAL] =
{
   "map",                   // starts a map definition
   "cd_start_track",        // specifies startup CD track (TODO)
   "cd_end1_track",         // specifies END1 track for Hexen (TODO)
   "cd_end2_track",         // specifies END2 track for Hexen (TODO)
   "cd_end3_track",         // specifies END3 track for Hexen (TODO)
   "cd_intermission_track", // specifies intermission CD track (TODO)
   "cd_title_track",        // specifies titlescreen CD track (TODO)
   "defaultmap",            // GZDoom defaultmap
};

// Keywords allowed in a map context
const char *XLMapInfoParser::mapKeywords[XL_NUMMAPINFO_FIELDS] =
{
   "sky1",            // first sky texture
   "sky2",            // second sky texture
   "doublesky",       // if enabled, has double skies
   "lightning",       // if enabled, has lightning global effect
   "fadetable",       // sets default global colormap
   "cluster",         // specifies hub number (TODO)
   "warptrans",       // specifies warp cheat translation number (TODO)
   "next",            // next map for normal exits
   "cdtrack",         // specifies CD track # (TODO)
   "secretnext",      // next map for secret exits
   "titlepatch",      // patch to use for name in intermission
   "par",             // par time in seconds
   "music",           // name of music lump
   "nointermission",  // kill intermission after map
   "evenlighting",    // map has no fake contrast
   "noautosequences", // disables auto-assignment of default sound sequences
   "nojump",          // disable jumping
   "nocrouch",        // unused
   "map07special",    // map07 boss special
};

// holds data about how to parse and store a map keyword's data
struct xlmikeyword_t
{
   int index;   // enum value
   int kwtype;  // keyword type for parsing
   int vartype; // type of variable

   const char *key;    // primary value key
   const char *intkey; // key for secondary integer value
};

// map keyword types (determine state transition patterns)
enum
{
   KW_TYPE_SKY,   // is a sky specification (two tokens follow)
   KW_TYPE_VALUE, // a single value follows
   KW_TYPE_FLAG   // is a flag, no value token
};

// keyword value types
enum
{
   KW_VALUE_SKY,  // has a texture name and a scroll delta
   KW_VALUE_INT,  // stores as an integer
   KW_VALUE_QSTR, // stores as a qstring
   KW_VALUE_BOOL, // stores as a boolean   
   KW_VALUE_MAP   // is a map name or number
};

#define XLMI_INTEGER(n)    \
   { n, KW_TYPE_VALUE, KW_VALUE_INT,  XLMapInfoParser::mapKeywords[n], nullptr }
#define XLMI_QSTRING(n)    \
   { n, KW_TYPE_VALUE, KW_VALUE_QSTR, XLMapInfoParser::mapKeywords[n], nullptr }
#define XLMI_BOOLEAN(n)    \
   { n, KW_TYPE_FLAG,  KW_VALUE_BOOL, XLMapInfoParser::mapKeywords[n], nullptr }
#define XLMI_SKYTYPE(n, i) \
   { n, KW_TYPE_SKY,   KW_VALUE_SKY,  XLMapInfoParser::mapKeywords[n], i    }
#define XLMI_MAPNAME(n)    \
   { n, KW_TYPE_VALUE, KW_VALUE_MAP,  XLMapInfoParser::mapKeywords[n], nullptr }

// This table drives parsing while inside a map block.
static xlmikeyword_t mapKeywordParseTable[XL_NUMMAPINFO_FIELDS] =
{
   XLMI_SKYTYPE(XL_MAPINFO_SKY1, "sky1delta"),
   XLMI_SKYTYPE(XL_MAPINFO_SKY2, "sky2delta"),
   XLMI_BOOLEAN(XL_MAPINFO_DOUBLESKY),
   XLMI_BOOLEAN(XL_MAPINFO_LIGHTNING),
   XLMI_QSTRING(XL_MAPINFO_FADETABLE),
   XLMI_INTEGER(XL_MAPINFO_CLUSTER),
   XLMI_INTEGER(XL_MAPINFO_WARPTRANS),
   XLMI_MAPNAME(XL_MAPINFO_NEXT),
   XLMI_INTEGER(XL_MAPINFO_CDTRACK),
   XLMI_MAPNAME(XL_MAPINFO_SECRETNEXT),
   XLMI_QSTRING(XL_MAPINFO_TITLEPATCH),
   XLMI_INTEGER(XL_MAPINFO_PAR),
   XLMI_QSTRING(XL_MAPINFO_MUSIC),
   XLMI_BOOLEAN(XL_MAPINFO_NOINTERMISSION),
   XLMI_BOOLEAN(XL_MAPINFO_EVENLIGHTING),
   XLMI_BOOLEAN(XL_MAPINFO_NOAUTOSEQUENCES),
   XLMI_BOOLEAN(XL_MAPINFO_NOJUMP),
   XLMI_BOOLEAN(XL_MAPINFO_NOCROUCH),
   XLMI_BOOLEAN(XL_MAPINFO_MAP07SPECIAL),
};

//
// XLMapInfoParser::doToken
//
// Dispatch the proper state handler for the current token.
//
bool XLMapInfoParser::doToken(XLTokenizer &token)
{
   return (this->*States[state])(token);
}

//
// XLMapInfoParser::startLump
//
// Start parsing a new MAPINFO lump.
//
void XLMapInfoParser::startLump()
{
   state    = STATE_EXPECTCMD; // scanning for global command
   globalKW = KW_NUMGLOBAL;    // no current global keyword
   curInfo  = nullptr;            // not in a current info definition
   kwd      = nullptr;            // no current keyword data
   classicHexenMode = false;
   mapName.clear();
}

// Expecting a command at the global level
bool XLMapInfoParser::doStateExpectCmd(XLTokenizer &token)
{
   auto &tokenVal = token.getToken();
   int   kwNum    = E_StrToNumLinear(globalKeywords, KW_NUMGLOBAL, tokenVal.constPtr());

   if(kwNum == KW_NUMGLOBAL)
   {
      usermsg("MAPINFO: unknown item '%s'; stopping parsing.", tokenVal.constPtr());
      return false; // error, stop parsing.
   }

   switch(kwNum)
   {
   case KW_GLOBAL_MAP:
      state = STATE_EXPECTMAPNAME; // map name or number should be next.
      break;
   case KW_GLOBAL_DEFAULTMAP:
      defaultMap.clearTable();   // must clear previous defaultmap data
      curInfo = &defaultMap;
      state = STATE_EXPECTMAPCMD;
      break;
   default:
      globalKW = kwNum;
      state = STATE_EXPECTGLOBALVAL; // should have a value for global variable
      break;
   }

   return true;
}

bool XLMapInfoParser::doStateExpectGlobalVal(XLTokenizer &token)
{
   // TODO: store the global value somewhere. For now, none of them are
   // supported because they all relate to CD music tracks.
   
   state = STATE_EXPECTCMD; // return to scanning for a global command
   return true;
}

//
// MakeMAPxy
//
// Utility to convert Hexen's map numbers into MAPxy map name strings.
// Returns true if it was indeed just a number
//
static bool MakeMAPxy(qstring &mapName)
{
   char *endptr = nullptr;
   auto  num    = mapName.toLong(&endptr, 10);

   if(*endptr == '\0' && num >= 1 && num <= 99)
   {
      mapName.Printf(9, "MAP%02ld", num);
      return true;
   }
   return false;
}

// Expecting the map lump name or number after "map"
bool XLMapInfoParser::doStateExpectMapName(XLTokenizer &token)
{
   mapName = token.getToken();
   if(MakeMAPxy(mapName))
      classicHexenMode = true;   // WARNING: GZDoom keeps this on true for the rest of the lump.
   state = STATE_EXPECTMAPDNAME;
   return true;
}

// Expecting the display or "pretty" name for the map
bool XLMapInfoParser::doStateExpectMapDName(XLTokenizer &token)
{
   // we have enough information now to create a new XLMapInfo
   curInfo = XL_newMapInfo(mapName, token.getToken());

   // Copy now whatever we have in defaultmap
   curInfo->copyTableFrom(&defaultMap);

   state = STATE_EXPECTMAPCMD;
   return true;
}

// Expecting a map-level command
bool XLMapInfoParser::doStateExpectMapCmd(XLTokenizer &token)
{
   auto &tokenVal = token.getToken();
   int   mapKwd   = E_StrToNumLinear(mapKeywords, XL_NUMMAPINFO_FIELDS, 
                                     tokenVal.constPtr());

   if(mapKwd == XL_NUMMAPINFO_FIELDS) // unknown keyword, error
   {
      // see if it is a new global keyword
      return doStateExpectCmd(token);
   }

   // get the parsing information for this map keyword
   kwd = &mapKeywordParseTable[mapKwd];

   switch(kwd->kwtype)
   {
   case KW_TYPE_SKY:
      state = STATE_EXPECTSKYNAME; // sky name should be next token
      break;
   case KW_TYPE_FLAG:              // flags have no value; presence means set it now
      curInfo->setInt(kwd->key, 1);
      state = STATE_EXPECTMAPCMD;
      break;      
   case KW_TYPE_VALUE:             // next token should be value
      state = STATE_EXPECTVALUE;
      break;
   }

   return true;
}

// Expecting the name of a sky texture
bool XLMapInfoParser::doStateExpectSkyName(XLTokenizer &token)
{
   curInfo->setString(kwd->key, token.getToken().constPtr());
   state = STATE_EXPECTSKYNUM;
   return true;
}

// Expecting the scroll delta to apply to that sky texture
bool XLMapInfoParser::doStateExpectSkyNum(XLTokenizer &token)
{
   char *endptr = nullptr;
   double val = token.getToken().toDouble(&endptr);

   if(*endptr) // not a number, it's optional, so try something else
      return doStateExpectMapCmd(token);

   // Must apply GZDoom's scale if so far the format is modern
   if(!classicHexenMode)
      val *= 256;
   curInfo->setDouble(kwd->intkey, val);
   state = STATE_EXPECTMAPCMD; // go back to scanning for a map command
   return true;
}

// Expecting a value for a single-value MAPINFO keyword
bool XLMapInfoParser::doStateExpectValue(XLTokenizer &token)
{
   auto &tokenVal = token.getToken();
   
   switch(kwd->vartype)
   {
   case KW_VALUE_INT: // integer value
      curInfo->setInt(kwd->key, tokenVal.toInt());
      break;
   case KW_VALUE_QSTR: // string
      curInfo->setString(kwd->key, tokenVal.constPtr());
      break;
   case KW_VALUE_MAP:
      {
         qstring temp = tokenVal;
         MakeMAPxy(temp);
         curInfo->setString(kwd->key, temp.constPtr());
      }
      break;
   }
   state = STATE_EXPECTMAPCMD; // go back to scanning for map commands
   return true;
}

// State Table
bool (XLMapInfoParser::* XLMapInfoParser::States[])(XLTokenizer &) =
{
   &XLMapInfoParser::doStateExpectCmd,
   &XLMapInfoParser::doStateExpectGlobalVal,
   &XLMapInfoParser::doStateExpectMapName,
   &XLMapInfoParser::doStateExpectMapDName,
   &XLMapInfoParser::doStateExpectMapCmd,
   &XLMapInfoParser::doStateExpectSkyName,
   &XLMapInfoParser::doStateExpectSkyNum,
   &XLMapInfoParser::doStateExpectValue
};

//=============================================================================
//
// External Interface
//

//
// XL_MapInfoForMapName
//
// Lookup an XLMapInfo object by mapname. Returns the active object for the
// map if one exists, and nullptr otherwise.
//
MetaTable *XL_MapInfoForMapName(const char *name)
{
   return mapInfoTable.getObjectKeyAndTypeEx<MetaTable>(name);
}

//
// XL_MapInfoForMapNum
//
// Lookup an XLMapInfo object by map number, assuming the map name in MAPINFO
// is an ExMy or MAPxy lump. Send in 0 for episode if should be MAPxy.
//
MetaTable *XL_MapInfoForMapNum(int episode, int map)
{
   qstring mapname;

   if(episode > 0)
      mapname.Printf(9, "E%dM%d", episode, map);
   else
      mapname.Printf(9, "MAP%02d", map);

   return mapInfoTable.getObjectKeyAndTypeEx<MetaTable>(mapname.constPtr());
}

//
// XL_ParseMapInfo
//
// Parse all MAPINFO lumps.
//
void XL_ParseMapInfo()
{
   XLMapInfoParser parser;

   parser.parseAll(wGlobalDir);
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
CONSOLE_COMMAND(xl_dumpmapinfo, 0)
{
   if(Console.argc < 1)
   {
      C_Printf("Usage: xl_dumpmapinfo mapname\n");
      return;
   }

   MetaTable *mapInfo = nullptr;
   if((mapInfo = XL_MapInfoForMapName(Console.argv[0]->constPtr())))
   {
      const MetaObject *obj = nullptr;

      C_Printf("MAPINFO Entry for %s:\n", mapInfo->getKey());
      while((obj = mapInfo->tableIterator(obj)))
         C_Printf(FC_HI "%s: " FC_NORMAL "%s\n", obj->getKey(), obj->toString());
   }
   else
      C_Printf(FC_ERROR "No MAPINFO defined for %s\n", Console.argv[0]->constPtr());
}

// EOF

