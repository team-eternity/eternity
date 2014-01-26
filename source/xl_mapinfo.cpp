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
#include "e_hash.h"
#include "e_lib.h"
#include "m_qstrkeys.h"
#include "v_misc.h"
#include "w_wad.h"
#include "xl_mapinfo.h"
#include "xl_scripts.h"

//=============================================================================
//
// XLMapInfo Maintenance
//

// XLMapInfo hash
static EHashTable<XLMapInfo, ENCQStrHashKey, &XLMapInfo::map, &XLMapInfo::links>
   mapInfoTable(191);

//
// XL_newMapInfo
//
// Create a new XLMapInfo structure, add it to the hash table, and return it.
//
static XLMapInfo *XL_newMapInfo(const qstring &map, const qstring &name)
{
   XLMapInfo *ret = new XLMapInfo();

   ret->map  = map;
   ret->name = name;

   // mark all fields as unmodified
   for(int i = 0; i < XL_NUMMAPINFO_FIELDS; i++)
      ret->setfields[i] = false;

   // add it to the hash
   mapInfoTable.addObject(ret);

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
   XLMapInfo     *curInfo;
   qstring        mapName;
   xlmikeyword_t *kwd;

   virtual bool doToken(XLTokenizer &token);
   virtual void startLump();

public:
   XLMapInfoParser() 
      : XLParser("MAPINFO"), state(STATE_EXPECTCMD), globalKW(KW_NUMGLOBAL),
        curInfo(NULL), mapName(), kwd(NULL)
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
};

// holds data about how to parse and store a map keyword's data
struct xlmikeyword_t
{
   int index;   // enum value
   int kwtype;  // keyword type for parsing
   int vartype; // type of variable

   qstring XLMapInfo::*qstrval; // qstring value pointer
   int     XLMapInfo::*intval;  // integer value pointer
   bool    XLMapInfo::*boolval; // boolean value pointer
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

#define XLMI_INTEGER(n, i) \
   { XL_MAPINFO_ ## n, KW_TYPE_VALUE, KW_VALUE_INT,  NULL, i,    NULL }
#define XLMI_QSTRING(n, s) \
   { XL_MAPINFO_ ## n, KW_TYPE_VALUE, KW_VALUE_QSTR, s,    NULL, NULL }
#define XLMI_BOOLEAN(n, b) \
   { XL_MAPINFO_ ## n, KW_TYPE_FLAG,  KW_VALUE_BOOL, NULL, NULL, b    }
#define XLMI_SKYTYPE(n, s, i) \
   { XL_MAPINFO_ ## n, KW_TYPE_SKY,   KW_VALUE_SKY,  s,    i,    NULL }
#define XLMI_MAPNAME(n, s) \
   { XL_MAPINFO_ ## n, KW_TYPE_VALUE, KW_VALUE_MAP,  s,    NULL, NULL }

// This table drives parsing while inside a map block.
static xlmikeyword_t mapKeywordParseTable[XL_NUMMAPINFO_FIELDS] =
{
   XLMI_SKYTYPE(SKY1,            &XLMapInfo::sky1, &XLMapInfo::sky1delta),
   XLMI_SKYTYPE(SKY2,            &XLMapInfo::sky2, &XLMapInfo::sky2delta),
   XLMI_BOOLEAN(DOUBLESKY,       &XLMapInfo::doublesky),
   XLMI_BOOLEAN(LIGHTNING,       &XLMapInfo::lightning),
   XLMI_QSTRING(FADETABLE,       &XLMapInfo::fadetable),
   XLMI_INTEGER(CLUSTER,         &XLMapInfo::cluster),
   XLMI_INTEGER(WARPTRANS,       &XLMapInfo::warptrans),
   XLMI_MAPNAME(NEXT,            &XLMapInfo::next),
   XLMI_INTEGER(CDTRACK,         &XLMapInfo::cdtrack),
   XLMI_MAPNAME(SECRETNEXT,      &XLMapInfo::secretnext),
   XLMI_QSTRING(TITLEPATCH,      &XLMapInfo::titlepatch),
   XLMI_INTEGER(PAR,             &XLMapInfo::par),
   XLMI_QSTRING(MUSIC,           &XLMapInfo::music),
   XLMI_BOOLEAN(NOINTERMISSION,  &XLMapInfo::nointermission),
   XLMI_BOOLEAN(EVENLIGHTING,    &XLMapInfo::evenlighting),
   XLMI_BOOLEAN(NOAUTOSEQUENCES, &XLMapInfo::noautosequences)
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
   curInfo  = NULL;            // not in a current info definition
   kwd      = NULL;            // no current keyword data
   mapName.clear();
}

// Expecting a command at the global level
bool XLMapInfoParser::doStateExpectCmd(XLTokenizer &token)
{
   auto &tokenVal = token.getToken();
   int   kwNum    = E_StrToNumLinear(globalKeywords, KW_NUMGLOBAL, tokenVal.constPtr());

   if(kwNum == KW_NUMGLOBAL)
      return false; // error, stop parsing.

   switch(kwNum)
   {
   case KW_GLOBAL_MAP:
      state = STATE_EXPECTMAPNAME; // map name or number should be next.
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
//
static void MakeMAPxy(qstring &mapName)
{
   char *endptr = NULL;
   auto  num    = mapName.toLong(&endptr, 10);

   if(*endptr == '\0' && num >= 1 && num <= 99)
      mapName.Printf(9, "MAP%02d", num);
}

// Expecting the map lump name or number after "map"
bool XLMapInfoParser::doStateExpectMapName(XLTokenizer &token)
{
   mapName = token.getToken();
   MakeMAPxy(mapName);
   state = STATE_EXPECTMAPDNAME;
   return true;
}

// Expecting the display or "pretty" name for the map
bool XLMapInfoParser::doStateExpectMapDName(XLTokenizer &token)
{
   // we have enough information now to create a new XLMapInfo
   curInfo = XL_newMapInfo(mapName, token.getToken());

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
      curInfo->*(kwd->boolval) = true;
      curInfo->setfields[kwd->index] = true;
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
   curInfo->*(kwd->qstrval) = token.getToken();
   curInfo->setfields[kwd->index] = true;
   state = STATE_EXPECTSKYNUM;
   return true;
}

// Expecting the scroll delta to apply to that sky texture
bool XLMapInfoParser::doStateExpectSkyNum(XLTokenizer &token)
{
   curInfo->*(kwd->intval) = token.getToken().toInt();
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
      curInfo->*(kwd->intval) = tokenVal.toInt();
      break;
   case KW_VALUE_QSTR: // string
      curInfo->*(kwd->qstrval) = tokenVal;
      break;
   case KW_VALUE_MAP: // could be int, or full map name
      curInfo->*(kwd->qstrval) = tokenVal;
      MakeMAPxy(curInfo->*(kwd->qstrval));
      break;
   default: // not handled here...
      break;
   }
   curInfo->setfields[kwd->index] = true;

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
// map if one exists, and NULL otherwise.
//
XLMapInfo *XL_MapInfoForMapName(const char *name)
{
   return mapInfoTable.objectForKey(name);
}

//
// XL_MapInfoForMapNum
//
// Lookup an XLMapInfo object by map number, assuming the map name in MAPINFO
// is an ExMy or MAPxy lump. Send in 0 for episode if should be MAPxy.
//
XLMapInfo *XL_MapInfoForMapNum(int episode, int map)
{
   qstring mapname;

   if(episode > 0)
      mapname.Printf(9, "E%dM%d", episode, map);
   else
      mapname.Printf(9, "MAP%02d", map);

   return mapInfoTable.objectForKey(mapname.constPtr());
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

   XLMapInfo *mapInfo = NULL;
   if((mapInfo = XL_MapInfoForMapName(Console.argv[0]->constPtr())))
   {
      C_Printf("MAPINFO Entry for %s:\n"
               FC_HI "name: " FC_NORMAL "%s\n",
               mapInfo->map.constPtr(), mapInfo->name.constPtr());

      for(int i = 0; i < XL_NUMMAPINFO_FIELDS; i++)
      {
         auto &kwd       = mapKeywordParseTable[i];
         auto  fieldname = XLMapInfoParser::mapKeywords[kwd.index];

         if(mapInfo->setfields[kwd.index])
         {
            qstring val;

            switch(kwd.vartype)
            {
            case KW_VALUE_SKY:
               val << mapInfo->*(kwd.qstrval) << ", " << mapInfo->*(kwd.intval);
               break;
            case KW_VALUE_INT:
               val << mapInfo->*(kwd.intval);
               break;
            case KW_VALUE_MAP:
            case KW_VALUE_QSTR:
               val << mapInfo->*(kwd.qstrval);
               break;
            case KW_VALUE_BOOL:
               val << mapInfo->*(kwd.boolval);
               break;
            }

            C_Printf(FC_HI "%s: " FC_NORMAL "%s\n", fieldname, val.constPtr());
         }
      }
   }
   else
      C_Printf(FC_ERROR "No MAPINFO defined for %s\n", Console.argv[0]->constPtr());
}

// EOF

