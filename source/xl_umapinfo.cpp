//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: UMAPINFO lump, invented by Graf Zahl for cross-port support.
// Authors: Ioan Chera
//

#include <assert.h>
#include "z_zone.h"

#include "d_gi.h"
#include "d_main.h"
#include "e_fonts.h"
#include "e_hash.h"
#include "in_lude.h"
#include "m_utils.h"
#include "metaqstring.h"
#include "mn_engin.h"
#include "st_stuff.h"
#include "v_patchfmt.h"
#include "w_wad.h"
#include "xl_scripts.h"
#include "xl_umapinfo.h"
#include "mn_emenu.h"
#include "v_font.h"

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

// In-order map names
static PODCollection<const char *> orderedLevels;

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
      orderedLevels.add(ret->getKey());
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
      usermsg("UMAPINFO: unknown property '%s' in map '%s'", newkey.constPtr(), curInfo->getKey());

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
   if(!rule)
   {
      state = STATE_POSTVALUE;
      return true;
   }

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
            if(type == XLTokenizer::TOKEN_KEYWORD && !value.strCaseCmp("clear"))
            {
               curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
               gotClear = true;
            }
            else
            {
               subtable = new MetaTable(key.constPtr());
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
            {
               curInfo->addInt(key.constPtr(), XL_UMAPINFO_SPECVAL_CLEAR);
               gotClear = true;
            }
            else
            {
               subtable = new MetaTable(key.constPtr());
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
   const propertyrule_t *rule = rules.objectForKey(key.constPtr());

   if(rule && (rule->type == PropertyType::addBossActionOrClear ||
               rule->type == PropertyType::addEpisodeOrClear) && !gotClear &&
      valueIndex != 2)
   {
      I_Error("UMAPINFO: expected 3 arguments for '%s' in map '%s'\n", key.constPtr(),
              curInfo->getKey());
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
                           XLTokenizer::TF_STRINGSQUOTED | XLTokenizer::TF_SLASHCOMMENTS | XLTokenizer::TF_BLOCKCOMMENTS);
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
   addRule("intertext", PropertyType::setMultiStringOrClear);
   addRule("intertextsecret", PropertyType::setMultiStringOrClear);
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

//
// Constructs the episodes from UMAPINFO, if available
//
void XL_BuildUMapInfoEpisodes()
{
   // The EDF episode menu has priority
   if(mn_episode_override)
      return;

   PODCollection<menuitem_t> prefixItems; // the visual items before the actual items, if any
   PODCollection<menuitem_t> items;
   int prefixGaps = 0;   // number of prefix lines with blanks

   const menu_t *base = GameModeInfo->episodeMenu;
   edefstructvar(menu_t, newmenu);
   bool finishedPrefix = false;
   if(base)
   {
      newmenu = *base;  // copy the vanilla menu properties
      for(const menuitem_t *item = newmenu.menuitems; item->type != it_end; ++item)
      {
         if(item->type != it_runcmd)
         {
            if(!finishedPrefix)
            {
               prefixItems.add(*item);
               if(item->type == it_gap)
                  ++prefixGaps;
            }
            continue;
         }
         finishedPrefix = true;
         items.add(*item);
      }
      newmenu.flags |= mf_bigfont;
   }
   else
   {
      // Shouldn't really go here
      // Based on the Doom menu
      newmenu.x = 48;
      newmenu.y = 63;
      newmenu.flags = mf_skullmenu | mf_bigfont;
   }

   struct episodeinfo_t
   {
      const char *patch;
      const char *name;
      // NOTE: key currently unused
   };

   PODCollection<episodeinfo_t> epinfos;  // REVERSE LIST

   bool changed = false;

   for(const char *levelname : orderedLevels)
   {
      const MetaTable *level = XL_UMapInfoForMapName(levelname);
      assert(level);

      // Locate the last "clear" entry, if any. Use it to clear the items list.
      int mint = level->getInt("episode", XL_UMAPINFO_SPECVAL_NOT_SET);
      if(mint == XL_UMAPINFO_SPECVAL_CLEAR)
      {
         items.makeEmpty();   // empty out the episode list
         changed = true;
      }

      // Now get the episode list.
      epinfos.makeEmpty();
      // use MetaObject because we may hit a MetaInteger and know to stop
      MetaObject *epentry = nullptr;
      while((epentry = level->getNextObject(epentry, "episode")))
      {
         auto eptable = runtime_cast<MetaTable *>(epentry);
         if(!eptable)   // hit a "clear" entry
            break;
         edefstructvar(episodeinfo_t, epinfo);
         epinfo.patch = eptable->getString("patch", nullptr);
         assert(epinfo.patch);
         epinfo.name = eptable->getString("name", nullptr);
         assert(epinfo.name);
         epinfos.add(epinfo);
      }

      // Visit the list in reverse
      for(int i = (int)epinfos.getLength() - 1; i >= 0; --i)
      {
         const episodeinfo_t &epinfo = epinfos[i];

         qstring ccmd("mn_start_mapname ");
         ccmd += levelname;

         edefstructvar(menuitem_t, newitem);
         newitem.type = it_runcmd;
         newitem.description = epinfo.name;
         newitem.data = ccmd.duplicate(PU_STATIC);
         newitem.patch = epinfo.patch;
         items.add(newitem);
         changed = true;
      }
   }
   // Now we have the items
   // Cut them off to 8 (UMAPINFO limit)
   if(items.getLength() > 8)
   {
      items.resize(8);
      changed = true;
   }

   // Scan for missing patches and remove them
   // NOTE: this is a deviation from the UMAPINFO specs, by only invalidating the missing patches
   // on an element-by-element basis, not every item.
   for(menuitem_t &item : items)
      if(item.patch && W_CheckNumForName(item.patch) == -1)
         item.patch = nullptr;

   // Check if the menu needs adjustment (DOOM mf_emulated menus are fine, they have fixed height)
   if(newmenu.flags & mf_bigfont)
   {
      const vfont_t *gapfont = E_FontForName(mn_fontname);  // the gap uses the basic font height
      int rowheight = -1;  // by default invalid
      if(newmenu.flags & mf_emulated)
         rowheight = EMULATED_ITEM_SIZE;
      else
      {
         const vfont_t *font = E_FontForName(mn_bigfontname);
         if(font)
            rowheight = font->cy;
      }
      if(rowheight > 0 && gapfont)
      {
         int prefixHeight = rowheight * ((int)prefixItems.getLength() - prefixGaps) +
               gapfont->cy * prefixGaps;
         int contentHeight = rowheight * ((int)items.getLength());
         int bottom = newmenu.y + prefixHeight + contentHeight;
         if(bottom > SCREENHEIGHT)
         {
            // Center to fit it to screen
            newmenu.y = (SCREENHEIGHT - prefixHeight - contentHeight) / 2;
            if(newmenu.y < 0)
            {
               // Still not fitting? Remove the title
               newmenu.selected -= (int)prefixItems.getLength();
               prefixItems.makeEmpty();
               newmenu.y = (SCREENHEIGHT - contentHeight) / 2;
               if(newmenu.y < 0)
               {
                  // Still not fitting? Give up, but ensure valid origin
                  newmenu.y = 0;
               }
            }
         }
         else if(GameModeInfo->StatusBar == &DoomStatusBar && bottom > SCREENHEIGHT - 32)
         {
            static int shiftedTop;
            int oldmenuy = newmenu.y;
            newmenu.y = SCREENHEIGHT - 32 - prefixHeight - contentHeight;  // touch the status bar
            if(newmenu.y < 0)
               newmenu.y = 0;
            shiftedTop = 38 - (oldmenuy - newmenu.y);

            // HACK: replace the drawer function to something equivalent but dynamically offset.
            extern menu_t menu_episode, menu_episodeDoom2Stub;
            if(GameModeInfo->episodeMenu == &menu_episode ||
               GameModeInfo->episodeMenu == &menu_episodeDoom2Stub)
            {
               newmenu.drawer = []()
               {
                  V_DrawPatch(54, shiftedTop, &subscreen43,
                              PatchLoader::CacheName(wGlobalDir, "M_EPISOD", PU_CACHE));
               };
            }
         }
      }
   }

   // the stock menu didn't get changed, so don't override!
   // Also do not perform any change if the menu is merely cleared. We never want an empty menu.
   if(!changed || items.isEmpty())
      return;

   // Now we have the episode!
   mn_episode_override = estructalloc(menu_t, 1);
   *mn_episode_override = newmenu;  // copy the properties
   mn_episode_override->menuitems = estructalloc(menuitem_t,
                                                 prefixItems.getLength() + items.getLength() + 1);

   for(size_t i = 0; i < prefixItems.getLength(); ++i)
      mn_episode_override->menuitems[i] = prefixItems[i];
   for(size_t i = 0; i < items.getLength(); ++i)
      mn_episode_override->menuitems[prefixItems.getLength() + i] = items[i];
   mn_episode_override->menuitems[prefixItems.getLength() + items.getLength()].type = it_end;
}

// EOF

