//
// The Eternity Engine
// Copyright (C) 2019 James Haley et al.
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
// Purpose: TEXTURES lump, from ZDoom
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "d_main.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "m_vector.h"
#include "w_wad.h"
#include "xl_scripts.h"
#include "xl_textures.h"

//
// TEXTURES top item
//
struct TopItem
{
   //
   // Texture patch
   //
   struct Patch
   {
      qstring name;
      int x;
      int y;
   };

   bool optional;
   qstring name;
   int width;
   int height;
   Collection<Patch> patches;
};

//
// The parser
//
class XLTextureParser final : public XLParser
{
   static bool (XLTextureParser::*States[])();

   //
   // The states
   //
   enum
   {
      STATE_EXPECTTOPITEM, // (type) [optional] <name>, <width>, <height>
      STATE_EXPECTTOPBRACE,
      STATE_EXPECTPROPERTY,   // Some properties here. Or patch
      STATE_EXPECTPATCHBRACE,
      STATE_EXPECTPATCHPROPERTY,
   };

   bool doStateExpectTopItem();
   bool doStateExpectTopBrace();
   bool doStateExpectProperty();
   bool doStateExpectPatchBrace();
   bool doStateExpectPatchProperty();

   int state;  // current state
   Collection<qstring> tokens;   // collected tokens so far

   bool doToken(XLTokenizer &token) override;
   void startLump() override;
   void initTokenizer(XLTokenizer &tokenizer) override;
   void onEOF(bool early) override;

   int mLine;
   int mLumpIndex = 0;
   qstring mError;

   TopItem mCurItem;
   TopItem::Patch mPatch;

public:
   XLTextureParser() : XLParser("TEXTURES"), state(STATE_EXPECTTOPITEM)
   {
   }

   Collection<TopItem> mTopItems;
};

Collection<TopItem> xlTopItems;

//
// State table
//

bool (XLTextureParser::*XLTextureParser::States[])() =
{
   &XLTextureParser::doStateExpectTopItem,
   &XLTextureParser::doStateExpectTopBrace,
   &XLTextureParser::doStateExpectProperty,
   &XLTextureParser::doStateExpectPatchBrace,
   &XLTextureParser::doStateExpectPatchProperty,
};

//
// Top item
//
bool XLTextureParser::doStateExpectTopItem()
{
   size_t count = tokens.getLength();
   if(!count)
      return true;   // do nothing if given nothing
   switch(count)
   {
      case 1:
         // NOTE: currently only these identifiers are supported
         if(!tokens.back().strCaseCmp("texture") || !tokens.back().strCaseCmp("walltexture"))
         {
            mCurItem = TopItem();
            return true;
         }
         mError = "Unexpected top level item of type '";
         mError << tokens.back();
         mError << "'.";
         return false;
      case 2:
         // At 2 it's not clear yet if we have optional
         return true;
      case 3:
         if(tokens.back() != ",")
         {
            if(tokens[count - 2].strCaseCmp("optional"))
            {
               mError = "Only the 'optional' modifier can be added before the name.";
               return false;
            }
            mCurItem.optional = true;
            mCurItem.name = tokens.back();
         }
         else
            mCurItem.name = tokens[count - 2];
         return true;
   }
   size_t fullcount = mCurItem.optional ? count : count + 1;
   switch(fullcount)
   {
      case 4:
      case 6:
         if(tokens.back() != ",")
         {
            mError = "Missing ',' in definition.";
            return false;
         }
         return true;
      case 5:
         mCurItem.width = tokens.back().toInt();
         return true;
      case 7:
         mCurItem.height = tokens.back().toInt();
         tokens.makeEmpty();
         state = STATE_EXPECTTOPBRACE;
         return true;
   }
   return false;
}

//
// Expect optional top brace
//
bool XLTextureParser::doStateExpectTopBrace()
{
   if(tokens.back() == "{")
   {
      tokens.makeEmpty();
      state = STATE_EXPECTPROPERTY;
      return true;
   }
   state = STATE_EXPECTTOPITEM;
   mTopItems.add(mCurItem);
   return doStateExpectTopItem();
}

//
// Expect property
//
bool XLTextureParser::doStateExpectProperty()
{
   // NOTE: currently only "patch" is supported
   size_t count = tokens.getLength();
   if(!count)
      return true;   // do nothing if given nothing
   switch(count)
   {
      case 1:
         if(!tokens.back().strCaseCmp("patch"))
         {
            mPatch = TopItem::Patch();
            return true;
         }
         if(tokens.back() == "}")
         {
            state = STATE_EXPECTTOPITEM;
            mTopItems.add(mCurItem);
            tokens.makeEmpty();
            return true;
         }
         mError = "Unsupported property '";
         mError << tokens.back();
         mError << "'.";
         return false;
      case 2:
         mPatch.name = tokens.back();
         return true;
      case 3:
      case 5:
         if(tokens.back() != ",")
         {
            mError = "Missing ',' in definition.";
            return false;
         }
         return true;
      case 4:
         mPatch.x = tokens.back().toInt();
         return true;
      case 6:
         mPatch.y = tokens.back().toInt();
         mCurItem.patches.add(mPatch);
         tokens.makeEmpty();
         return true;
      default:
         break;
   }
   return false;
}

//
// Patch brace {
//
bool XLTextureParser::doStateExpectPatchBrace()
{
   return false;
}

//
// Patch property or }
//
bool XLTextureParser::doStateExpectPatchProperty()
{
   return false;
}

//
// Does a token
//
bool XLTextureParser::doToken(XLTokenizer &token)
{
   if(token.getTokenType() == XLTokenizer::TOKEN_LINEBREAK)
   {
      ++mLine;
      return true;
   }
   tokens.add(token.getToken());
   return (this->*States[state])();
}

//
// Start the lump
//
void XLTextureParser::startLump()
{
   state = STATE_EXPECTTOPITEM;
   mCurItem = TopItem();
   mLine = 1;
   mLumpIndex++;
}

//
// Initializes the tokenizer
//
void XLTextureParser::initTokenizer(XLTokenizer &tokenizer)
{
   tokenizer.setTokenFlags(XLTokenizer::TF_LINEBREAKS |
                           XLTokenizer::TF_OPERATORS |
                           XLTokenizer::TF_SLASHCOMMENTS);  // use linebreaks to count lines
}

//
// Nothing special on premature EOF
//
void XLTextureParser::onEOF(bool early)
{
   if(early)
   {
      usermsg("XL_ParseTextures: Error in TEXTURES (lump number %d) at line %d: %s", mLumpIndex, mLine,
              mError.constPtr());
   }
}

//
// Parse all TEXTURES lumps
//
void XL_ParseTextures()
{
   XLTextureParser parser;
   parser.parseAll(wGlobalDir);
   xlTopItems = std::move(parser.mTopItems);
}

// EOF
