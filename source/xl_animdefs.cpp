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
// Purpose: ANIMDEFS lump, from Hexen. Replaces ANIMATED.
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "e_anim.h"
#include "e_switch.h"
#include "m_collection.h"
#include "r_ripple.h"
#include "w_wad.h"
#include "xl_scripts.h"

//
// ANIMDEFS parser
//
class XLAnimDefsParser final : public XLParser
{
   static bool (XLAnimDefsParser::*States[])(XLTokenizer &);

   //
   // The states
   //
   enum
   {
      STATE_EXPECTITEM,    // before "flat", "texture" or "pic"
      STATE_EXPECTANIMATION,  // animation state
      STATE_EXPECTWARP,    // warp
      STATE_EXPECTSWITCH,  // switch state. Use a separate set of states here.
   };

   enum
   {
      ANSTATE_NAME,           // animation name
      ANSTATE_CLAUSE,         // clause: pic or range
      ANSTATE_PICID,          // pic offset or name
      ANSTATE_PICOP,          // pic tics or rand
      ANSTATE_PICTICS,        // pic tics count
      ANSTATE_PICRANDMIN,     // pic random minimum
      ANSTATE_PICRANDMAX,     // pic random maximum
      ANSTATE_RANGENAME,      // range name
      ANSTATE_RANGEOP,        // range tics
      ANSTATE_RANGETICS,      // range tics count
   };

   enum
   {
      WASTATE_TYPE,        // flat or texture
      WASTATE_NAME,        // texture name
   };

   enum
   {
      SWSTATE_SWITCHNAME,
      SWSTATE_SWITCHDIR,
      SWSTATE_ATTRIBNAME,
      SWSTATE_ONNAME,
      SWSTATE_TICS,  // only to be able to parse ZDoom stuff with "tics 0"
      SWSTATE_TICSVAL,
      SWSTATE_SOUNDNAME,
      SWSTATE_OFFATTRIBNAME,
      SWSTATE_OFFSOUNDNAME,
      SWSTATE_OFFNAME,
      SWSTATE_OFFTICS,
      SWSTATE_OFFTICSVAL,
   };

   bool doStateExpectItem(XLTokenizer &);
   bool doStateExpectAnimation(XLTokenizer &);
   bool doStateExpectWarp(XLTokenizer &);
   bool doStateExpectSwitch(XLTokenizer &);

   bool doStateReset(XLTokenizer &token);
   void addSwitchToEDF() const;

   int state;  // current state
   int substate;

   EAnimDef curdef;
   EAnimDef::Pic curpic;
   ESwitchDef curswitch;
   bool switchinvalid;  // only for parsing and validation
   int switchtics;      // same

   int linenum;
   qstring errmsg;

   bool doToken(XLTokenizer &token) override;
   void startLump() override;
   void initTokenizer(XLTokenizer &tokenizer) override;
   void onEOF(bool early) override;
public:
   XLAnimDefsParser() : XLParser("ANIMDEFS"), state(STATE_EXPECTITEM),
   substate(), curdef(), curpic(), curswitch(), switchinvalid(), switchtics(),
   linenum(1)
   {
   }
};

//
// State table
//
bool (XLAnimDefsParser::* XLAnimDefsParser::States[])(XLTokenizer &) =
{
   &XLAnimDefsParser::doStateExpectItem,
   &XLAnimDefsParser::doStateExpectAnimation,
   &XLAnimDefsParser::doStateExpectWarp,
   &XLAnimDefsParser::doStateExpectSwitch,
};

//
// Expect "flat", "texture" or "pic" (only if a flat or texture is already on)
//
bool XLAnimDefsParser::doStateExpectItem(XLTokenizer &token)
{
   const qstring &str = token.getToken();
   if(!str.strCaseCmp("flat"))
   {
      curdef.reset(EAnimDef::type_flat);
      state = STATE_EXPECTANIMATION;
      substate = ANSTATE_NAME;
      return true;
   }
   if(!str.strCaseCmp("texture"))
   {
      curdef.reset(EAnimDef::type_wall);
      state = STATE_EXPECTANIMATION;
      substate = ANSTATE_NAME;
      return true;
   }
   if(!str.strCaseCmp("warp") || !str.strCaseCmp("warp2"))
   {
      curdef.reset();
      curdef.flags |= EAnimDef::SWIRL;
      state = STATE_EXPECTWARP;
      substate = WASTATE_TYPE;
      return true;
   }
   if(!str.strCaseCmp("switch"))
   {
      curswitch.reset();
      curswitch.episode = -1; // mark it as non-changing
      switchinvalid = false;
      switchtics = 0;

      state = STATE_EXPECTSWITCH;
      substate = SWSTATE_SWITCHNAME;
      return true;
   }
   return true;
}

//
// Checks if token is integer.
//
static bool XL_mustBeInt(XLTokenizer &token, int &result)
{
   char *endptr;
   long index = token.getToken().toLong(&endptr, 0);
   if(*endptr)
      return false;
   result = static_cast<int>(index);
   return true;
}

//
// Within a flat or texture block
//
bool XLAnimDefsParser::doStateExpectAnimation(XLTokenizer &token)
{
   const qstring &tok = token.getToken();
   int toknum;
   switch(substate)
   {
      case ANSTATE_NAME:
         curdef.startpic = token.getToken();
         substate = ANSTATE_CLAUSE;
         break;
      case ANSTATE_CLAUSE:
         if(!tok.strCaseCmp("pic"))
         {
            curpic.reset();
            substate = ANSTATE_PICID;
         }
         else if(!tok.strCaseCmp("range"))
            substate = ANSTATE_RANGENAME;
         else
            return doStateReset(token);
         break;
      case ANSTATE_PICID:
         if(XL_mustBeInt(token, toknum))
            curpic.offset = toknum;
         else
            curpic.name = tok;
         substate = ANSTATE_PICOP;
         break;
      case ANSTATE_PICOP:
         if(!tok.strCaseCmp("tics"))
            substate = ANSTATE_PICTICS;
         else if(!tok.strCaseCmp("rand"))
            substate = ANSTATE_PICRANDMIN;
         else
            return doStateReset(token);
         break;
      case ANSTATE_PICTICS:
         if(XL_mustBeInt(token, toknum))
         {
            curpic.ticsmin = toknum;
            curdef.pics.add(curpic);
            substate = ANSTATE_CLAUSE;
         }
         else
            return doStateReset(token);
         break;
      case ANSTATE_PICRANDMIN:
         if(XL_mustBeInt(token, toknum))
         {
            curpic.ticsmin = toknum;
            substate = ANSTATE_PICRANDMAX;
         }
         else
            return doStateReset(token);
         break;
      case ANSTATE_PICRANDMAX:
         if(XL_mustBeInt(token, toknum))
         {
            curpic.ticsmax = toknum;
            curdef.pics.add(curpic);
            substate = ANSTATE_CLAUSE;
         }
         else
            return doStateReset(token);
         break;
      case ANSTATE_RANGENAME:
         curdef.endpic = tok;
         substate = ANSTATE_RANGEOP;
         break;
      case ANSTATE_RANGEOP:
         // TODO: add "rand" support
         if(!tok.strCaseCmp("tics"))
            substate = ANSTATE_RANGETICS;
         else
            return doStateReset(token);
         break;
      case ANSTATE_RANGETICS:
         if(XL_mustBeInt(token, toknum))
         {
            curdef.tics = toknum;
            substate = ANSTATE_CLAUSE;
         }
         else
            return doStateReset(token);
         break;
      default:
         return false;
   }
   return true;
}

//
// Expect warp
//
bool XLAnimDefsParser::doStateExpectWarp(XLTokenizer &token)
{
   const qstring &tok = token.getToken();
   switch(substate)
   {
      case WASTATE_TYPE:
         if(!tok.strCaseCmp("texture"))
            curdef.type = EAnimDef::type_wall;
         else if(!tok.strCaseCmp("flat"))
            curdef.type = EAnimDef::type_flat;
         else
            return doStateReset(token);
         substate = WASTATE_NAME;
         break;
      case WASTATE_NAME:
         curdef.startpic = tok;
         substate = 0;
         E_AddAnimation(curdef);
         state = STATE_EXPECTITEM;
         break;
      default:
         return false;
   }
   return true;
}


//
// Within a switch block
//
bool XLAnimDefsParser::doStateExpectSwitch(XLTokenizer &token)
{
   switch(substate)
   {
      case SWSTATE_SWITCHNAME:
         curswitch.offpic = token.getToken();
         substate = SWSTATE_SWITCHDIR;
         break;
      case SWSTATE_SWITCHDIR:
         if(!token.getToken().strCaseCmp("on"))
            substate = SWSTATE_ATTRIBNAME;
         else if(!token.getToken().strCaseCmp("off"))
            substate = SWSTATE_OFFATTRIBNAME;
         else
            return doStateReset(token);
         break;
      case SWSTATE_ATTRIBNAME:
         if(!token.getToken().strCaseCmp("pic"))
            substate = SWSTATE_ONNAME;
         else if(!token.getToken().strCaseCmp("sound"))
            substate = SWSTATE_SOUNDNAME;
         else
         {
            substate = SWSTATE_SWITCHDIR;
            return doToken(token);
         }
         break;
      case SWSTATE_ONNAME:
         if(!curswitch.onpic.empty() && switchtics > 0)
            switchinvalid = true; // animations not yet supported
         else
            curswitch.onpic = token.getToken();
         substate = SWSTATE_TICS;
         break;
      case SWSTATE_TICS:
         if(!token.getToken().strCaseCmp("tics"))
            substate = SWSTATE_TICSVAL;
         else
            return doStateReset(token);
         break;
      case SWSTATE_TICSVAL:
         substate = SWSTATE_ATTRIBNAME;
         switchtics = static_cast<int>(strtol(token.getToken().constPtr(),
                                              nullptr, 0));
         break;
      case SWSTATE_SOUNDNAME:
         curswitch.onsound = token.getToken();
         substate = SWSTATE_ATTRIBNAME;
         break;
      case SWSTATE_OFFATTRIBNAME:
         if(!token.getToken().strCaseCmp("pic"))
            substate = SWSTATE_OFFNAME;
         else if(!token.getToken().strCaseCmp("sound"))
            substate = SWSTATE_OFFSOUNDNAME;
         else
         {
            substate = SWSTATE_SWITCHDIR;
            return doToken(token);
         }
         break;
      case SWSTATE_OFFNAME:
         // Off name must currently match the switch name
         if(token.getToken().strCaseCmp(curswitch.offpic.constPtr()))
            switchinvalid = true;
         substate = SWSTATE_OFFTICS;
         break;
      case SWSTATE_OFFTICS:
         if(!token.getToken().strCaseCmp("tics"))
            substate = SWSTATE_OFFTICSVAL;
         else
            return doStateReset(token);
         break;
      case SWSTATE_OFFTICSVAL:
         substate = SWSTATE_OFFATTRIBNAME;
         break;
      case SWSTATE_OFFSOUNDNAME:
         curswitch.offsound = token.getToken();
         substate = SWSTATE_OFFATTRIBNAME;
         break;
      default:
         errmsg = "Illegal switch state.";
         return false;
   }
   return true;
}

//
// Resets the state and handles the token as a new top object
//
bool XLAnimDefsParser::doStateReset(XLTokenizer &token)
{
   substate = 0;
   if(state == STATE_EXPECTSWITCH)
      addSwitchToEDF();
   if(state == STATE_EXPECTANIMATION)
      E_AddAnimation(curdef);
   state = STATE_EXPECTITEM;
   return doToken(token);
}

//
// Add switch to EDF if current state is switch
//
void XLAnimDefsParser::addSwitchToEDF() const
{
   if(!switchinvalid)
      E_AddSwitchDef(curswitch);
}

//
// Parse next token. Goes into one of the pointed functions.
//
bool XLAnimDefsParser::doToken(XLTokenizer &token)
{
   if(token.getTokenType() == XLTokenizer::TOKEN_LINEBREAK)
   {
      linenum++;
      return true;
   }
   return (this->*States[state])(token);
}

//
// Called at beginning of parsing
//
void XLAnimDefsParser::startLump()
{
   state = STATE_EXPECTITEM;
   substate = 0;
   curdef.reset();
   curpic.reset();
   curswitch.reset();
   switchinvalid = false;
   switchtics = 0;
   linenum = 1;
}

//
// Init tokenizer
//
void XLAnimDefsParser::initTokenizer(XLTokenizer &tokenizer)
{
   tokenizer.setTokenFlags(XLTokenizer::TF_LINEBREAKS);
}

//
// On EOF
//
void XLAnimDefsParser::onEOF(bool early)
{
   if(state == STATE_EXPECTSWITCH)
      addSwitchToEDF();
   else if(state == STATE_EXPECTANIMATION)
      E_AddAnimation(curdef);
}

//
// Parses the lump
//
void XL_ParseAnimDefs()
{
   XLAnimDefsParser parser;
   parser.parseAll(wGlobalDir);
}

// EOF


