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
//-----------------------------------------------------------------------------
//
// Description: 
//   SNDINFO Parser
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "e_lib.h"
#include "e_sound.h"
#include "p_info.h"
#include "w_wad.h"
#include "xl_scripts.h"

//=============================================================================
//
// XLSndInfoParser
//
// Parser for Hexen SNDINFO lumps.
//

class XLSndInfoParser : public XLParser
{
protected:
   static const char *sndInfoKwds[]; // see below class.
   
   // keyword enumeration
   enum
   {
     KWD_ALIAS,
     KWD_AMBIENT,     
     KWD_ARCHIVEPATH,
     KWD_ENDIF,
     KWD_IFDOOM,
     KWD_IFHERETIC,
     KWD_IFHEXEN,
     KWD_IFSTRIFE,
     KWD_LIMIT,
     KWD_MAP,
     KWD_MIDIDEVICE,
     KWD_MUSICVOLUME,
     KWD_PITCHSHIFT,
     KWD_PLAYERALIAS,
     KWD_PLAYERCOMPAT,
     KWD_PLAYERSOUND,
     KWD_PLAYERSOUNDDUP,
     KWD_RANDOM,
     KWD_REGISTERED,
     KWD_ROLLOFF,
     KWD_SINGULAR,
     KWD_VOLUME,
     NUMKWDS
   };

   // state enumeration
   enum
   {
      STATE_EXPECTCMD,
      STATE_EXPECTMAPNUM,
      STATE_EXPECTMUSLUMP,
      STATE_EXPECTSNDLUMP,
      STATE_EATTOKEN
   };

   int     state;
   qstring soundname;
   int     musicmapnum;

   void doStateExpectCmd(XLTokenizer &token);
   void doStateExpectMapNum(XLTokenizer &token);
   void doStateExpectMusLump(XLTokenizer &token);
   void doStateExpectSndLump(XLTokenizer &token);
   void doStateEatToken(XLTokenizer &token);

   // State table declaration
   static void (XLSndInfoParser::*States[])(XLTokenizer &);

   virtual bool doToken(XLTokenizer &token);
   virtual void startLump();

public:
   // Constructor
   XLSndInfoParser() 
      : XLParser("SNDINFO"), soundname(32), musicmapnum(0)
   {
   }
};

// Keywords for SNDINFO
// Note that all ZDoom extensions are included, even though they are not 
// supported yet. This is so that they are properly documented and ignored in
// the meanwhile.
const char *XLSndInfoParser::sndInfoKwds[] =
{
   "$alias",
   "$ambient",
   "$archivepath",
   "$endif",
   "$ifdoom",
   "$ifheretic",
   "$ifhexen",
   "$ifstrife",
   "$limit",
   "$map",
   "$mididevice",
   "$musicvolume",
   "$pitchshift",
   "$playeralias",
   "$playercompat",
   "$playersound",
   "$playersounddup",
   "$random",   
   "$registered",
   "$rolloff",
   "$singular",
   "$volume"
};

//
// State Handlers
//
   
// Expecting the start of a SNDINFO command or sound definition
void XLSndInfoParser::doStateExpectCmd(XLTokenizer &token)
{
   int cmdnum;
   qstring &tokenText = token.getToken();

   switch(token.getTokenType())
   {
   case XLTokenizer::TOKEN_KEYWORD: // a $ keyword
      cmdnum = E_StrToNumLinear(sndInfoKwds, NUMKWDS, tokenText.constPtr());
      switch(cmdnum)
      {
      case KWD_ARCHIVEPATH:
         state = STATE_EATTOKEN; // eat the path token
         break;
      case KWD_MAP:
         state = STATE_EXPECTMAPNUM;
         break;
      default: // unknown or inconsequential command (ie. $registered)
         break;
      }
      break;
   case XLTokenizer::TOKEN_STRING:  // a normal string
      soundname = tokenText; // remember the sound name
      state = STATE_EXPECTSNDLUMP;
      break;
   default: // unknown token
      break;
   }
}

// Expecting the map number after a $map command
void XLSndInfoParser::doStateExpectMapNum(XLTokenizer &token)
{
   if(token.getTokenType() != XLTokenizer::TOKEN_STRING)
   {
      state = STATE_EXPECTCMD;
      doStateExpectCmd(token);
   }
   else
   {
      musicmapnum = token.getToken().toInt();

      // Expect music lump name next.
      state = STATE_EXPECTMUSLUMP;
   }
}

// Expecting the music lump name after a map number
void XLSndInfoParser::doStateExpectMusLump(XLTokenizer &token)
{
   if(token.getTokenType() != XLTokenizer::TOKEN_STRING)
   {
      state = STATE_EXPECTCMD;
      doStateExpectCmd(token);
   }
   else
   {
      qstring &muslump = token.getToken();

      // Lump must exist
      if(muslump.length() <= 8 &&
         waddir->checkNumForName(muslump.constPtr()) != -1)
      {
         P_AddSndInfoMusic(musicmapnum, muslump.constPtr());

         // Return to expecting a command
         state = STATE_EXPECTCMD;
      }
      else // Otherwise we might be off due to unknown tokens; return to ExpectCmd
      {
         state = STATE_EXPECTCMD;
         doStateExpectCmd(token);
      }
   }
}

// Expecting the lump name after a sound definition
void XLSndInfoParser::doStateExpectSndLump(XLTokenizer &token)
{
   if(token.getTokenType() != XLTokenizer::TOKEN_STRING)
   {
      // Not a string? We are probably in an error state.
      // Get out with an immediate call to the expect command state
      state = STATE_EXPECTCMD;
      doStateExpectCmd(token);
   }
   else
   {
      qstring &soundlump = token.getToken();

      // Lump must exist, otherwise we create erroneous sounds if there are
      // unknown keywords in the lump. Thanks to ZDoom for defining such a 
      // clean, context-free, grammar-based language with delimiters :>
      if(soundlump.length() <= 8 &&
         waddir->checkNumForName(soundlump.constPtr()) != -1)
      {
         sfxinfo_t *sfx;

         if((sfx = E_SoundForName(soundname.constPtr()))) // defined already?
         {
            sfx->flags &= ~SFXF_PREFIX;
            soundname.copyInto(sfx->name, 9);
         } 
         else
         {
            // create a new sound
            E_NewSndInfoSound(soundname.constPtr(), soundlump.constPtr());
         }

         // Return to expecting a command
         state = STATE_EXPECTCMD;
      }
      else // Otherwise we might be off due to unknown tokens; return to ExpectCmd
      {
         state = STATE_EXPECTCMD;
         doStateExpectCmd(token);
      }
   }
}

// Throw away a token unconditionally
void XLSndInfoParser::doStateEatToken(XLTokenizer &token)
{
   state = STATE_EXPECTCMD; // return to expecting a command
}

// State table for SNDINFO parser
void (XLSndInfoParser::* XLSndInfoParser::States[])(XLTokenizer &) =
{
   &XLSndInfoParser::doStateExpectCmd,
   &XLSndInfoParser::doStateExpectMapNum,
   &XLSndInfoParser::doStateExpectMusLump,
   &XLSndInfoParser::doStateExpectSndLump,
   &XLSndInfoParser::doStateEatToken
};

//
// XLSndInfoParser::doToken
//
// Processes a token extracted from the SNDINFO input
//
bool XLSndInfoParser::doToken(XLTokenizer &token)
{
   // Call handler method for the current state. Why is this done from
   // a virtual call-down? Because parent classes cannot call child
   // class method pointers! :P
   (this->*States[state])(token);
   return true;
}

//
// XLSndInfoParser::startLump
//
// Resets the parser at the beginning of a new SNDINFO lump.
//
void XLSndInfoParser::startLump()
{
   state = STATE_EXPECTCMD; // starting state
}

//=============================================================================
//
// External Interface
//

//
// XL_ParseSoundInfo
//
// Parse SNDINFO scripts
//
void XL_ParseSoundInfo()
{
   XLSndInfoParser sndInfoParser;

   sndInfoParser.parseAll(wGlobalDir);
}

// EOF

