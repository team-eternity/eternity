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
// Purpose: ANIMDEFS lump, from Hexen. Replaces ANIMATED.
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "e_switch.h"
#include "m_collection.h"
#include "w_wad.h"
#include "xl_animdefs.h"
#include "xl_scripts.h"

Collection<XLAnimDef> xldefs;
PODCollection<xlpicdef_t> xlpics;

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
      STATE_EXPECTDEFNAME, // before initial pic name
      STATE_EXPECTPICNUM,  // before "pic" number
      STATE_EXPECTPICOP,   // before "pic" operator ("tics" or "rand")
      STATE_EXPECTDUR,     // expect duration after "tics" or "rand"
      STATE_EXPECTDUR2,    // second duration, if after rand.
      STATE_EXPECTRANGENAME,
      STATE_EXPECTRANGETICSOP,
      STATE_EXPECTRANGEDUR,
      STATE_EXPECTSWITCH,  // switch state. Use a separate set of states here.
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
   bool doStateExpectDefName(XLTokenizer &);
   bool doStateExpectPicNum(XLTokenizer &);
   bool doStateExpectPicOp(XLTokenizer &);
   bool doStateExpectDur(XLTokenizer &);
   bool doStateExpectDur2(XLTokenizer &);
   bool doStateExpectRangeName(XLTokenizer &);
   bool doStateExpectRangeTicsOp(XLTokenizer &);
   bool doStateExpectRangeDur(XLTokenizer &);
   bool doStateExpectSwitch(XLTokenizer &);

   bool doStateReset(XLTokenizer &token);
   void addSwitchToEDF() const;

   int state;  // current state
   int substate;
   Collection<XLAnimDef> defs;
   PODCollection<xlpicdef_t> pics;

   XLAnimDef *curdef;
   xlpicdef_t *curpic;
   bool inwarp;
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
   &XLAnimDefsParser::doStateExpectDefName,
   &XLAnimDefsParser::doStateExpectPicNum,
   &XLAnimDefsParser::doStateExpectPicOp,
   &XLAnimDefsParser::doStateExpectDur,
   &XLAnimDefsParser::doStateExpectDur2,
   &XLAnimDefsParser::doStateExpectRangeName,
   &XLAnimDefsParser::doStateExpectRangeTicsOp,
   &XLAnimDefsParser::doStateExpectRangeDur,
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
      XLAnimDef def;
      def.type = xlanim_flat;
      def.index = static_cast<int>(pics.getLength());
      defs.add(def);
      curdef = &defs[defs.getLength() - 1];
      state = STATE_EXPECTDEFNAME;
      if(inwarp)
         curdef->rangetics = 65536;
      return true;
   }
   if(!str.strCaseCmp("texture"))
   {
      XLAnimDef def;
      def.type = xlanim_texture;
      def.index = static_cast<int>(pics.getLength());
      defs.add(def);
      curdef = &defs[defs.getLength() - 1];
      state = STATE_EXPECTDEFNAME;
      if(inwarp)
         curdef->rangetics = 65536;
      return true;
   }
   if(!str.strCaseCmp("pic"))
   {
      if(!curdef || inwarp)
      {
         errmsg = "PIC must follow a TEXTURE or FLAT, and not allowed for WARP.";
         return false;  // must have a flat or texture animation prepared
      }
      state = STATE_EXPECTPICNUM;
      curpic = &pics.addNew();
      ++curdef->count;
      return true;
   }
   if(!str.strCaseCmp("range"))
   {
      if(!curdef || inwarp)
      {
         errmsg = "RANGE must follow a TEXTURE or FLAT, and not allowed for WARP.";
         return false;
      }
      state = STATE_EXPECTRANGENAME;
      return true;
   }
   if(!str.strCaseCmp("warp"))
   {
      if(inwarp)
      {
         errmsg = "WARP cannot be nested.";
         return false;
      }
      inwarp = true;
      state = STATE_EXPECTITEM;
      return true;
   }
   if(!str.strCaseCmp("switch"))
   {
      if(inwarp)
      {
         errmsg = "SWITCH not allowed for WARP.";
         return false;
      }
      curdef = nullptr; // nullify it now
      curswitch.reset();
      curswitch.episode = -1; // mark it as non-changing
      switchinvalid = false;
      switchtics = 0;

      state = STATE_EXPECTSWITCH;
      substate = SWSTATE_SWITCHNAME;
      return true;
   }
   errmsg.Printf(256, "Illegal item '%s'.", str.constPtr());
   return false;
}

//
// Expect an animation's first pic name
//
bool XLAnimDefsParser::doStateExpectDefName(XLTokenizer &token)
{
   curdef->picname = token.getToken();
   state = STATE_EXPECTITEM;
   if(inwarp)
   {
      inwarp = false;
      curdef->rangename = curdef->picname;
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
// Expect a pic index
//
bool XLAnimDefsParser::doStateExpectPicNum(XLTokenizer &token)
{
   int index;
   if(!XL_mustBeInt(token, index))
   {
      errmsg = "PIC must have a number.";
      return false;
   }
   curpic->offset = static_cast<int>(index);
   state = STATE_EXPECTPICOP;
   return true;
}

//
// Expect tics or rand
//
bool XLAnimDefsParser::doStateExpectPicOp(XLTokenizer &token)
{
   const qstring &str = token.getToken();
   if(!str.strCaseCmp("tics"))
   {
      curpic->isRandom = false;
      state = STATE_EXPECTDUR;
      return true;
   }
   if(!str.strCaseCmp("rand"))
   {
      curpic->isRandom = true;
      state = STATE_EXPECTDUR;
      return true;
   }
   errmsg = "Expected TICS or RAND after PIC index.";
   return false;
}

//
// Expect duration
//
bool XLAnimDefsParser::doStateExpectDur(XLTokenizer &token)
{
   int tics;
   if(!XL_mustBeInt(token, tics))
   {
      errmsg = "Expected number after TICS or RAND.";
      return false;
   }
   if(!curpic->isRandom)
   {
      curpic->tics = tics;
      state = STATE_EXPECTITEM;
      return true;
   }
   curpic->ticsmin = tics;
   state = STATE_EXPECTDUR2;
   return true;
}

//
// Expect second duration for rand
//
bool XLAnimDefsParser::doStateExpectDur2(XLTokenizer &token)
{
   int tics;
   if(!XL_mustBeInt(token, tics))
   {
      errmsg = "Expected two numbers after RAND.";
      return false;
   }
   curpic->ticsmax = tics;
   state = STATE_EXPECTITEM;
   return true;
}

//
// Expect range name
//
bool XLAnimDefsParser::doStateExpectRangeName(XLTokenizer &token)
{
   curdef->rangename = token.getToken();
   state = STATE_EXPECTRANGETICSOP;
   return true;
}

//
// Expect "Tics"
//
bool XLAnimDefsParser::doStateExpectRangeTicsOp(XLTokenizer &token)
{
   if(token.getToken().strCaseCmp("tics"))
   {
      errmsg = "Expected TICS in RANGE section.";
      return false;
   }
   state = STATE_EXPECTRANGEDUR;
   return true;
}

//
// Expect range duration
//
bool XLAnimDefsParser::doStateExpectRangeDur(XLTokenizer &token)
{
   int tics;
   if(!XL_mustBeInt(token, tics))
   {
      errmsg = "Expected number after TICS.";
      return false;
   }
   curdef->rangetics = tics;
   state = STATE_EXPECTITEM;
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
   defs.assign(xldefs);
   pics.assign(xlpics);
   curdef = nullptr;
   curpic = nullptr;
   inwarp = false;
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
   if(!early)
   {
      xldefs.assign(defs);
      xlpics.assign(pics);
   }
   else
   {
      I_Error("ANIMDEFS error at line %d: %s\n", linenum, errmsg.constPtr());
   }
   if(state == STATE_EXPECTSWITCH)
      addSwitchToEDF();
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


