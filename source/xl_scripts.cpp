// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// Hexen Script Lumps
//
//   Parsing and processing for Hexen's data scripts, including:
//   * SNDINFO
//   * MUSINFO (Risen3D)
//
//   By James Haley
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "e_lib.h"
#include "e_sound.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "p_info.h"
#include "w_wad.h"
#include "xl_scripts.h"

//=============================================================================
//
// XLTokenizer
//
// This class does proper FSA tokenization for Hexen lump parsers.
//

class XLTokenizer
{
public:
   // Tokenizer states
   enum
   {
      STATE_SCAN,    // scanning for a string token
      STATE_INTOKEN, // in a string token
      STATE_COMMENT, // reading out a comment (eat rest of line)
      STATE_DONE     // finished the current token
   };

   // Token types
   enum
   {
      TOKEN_NONE,    // Nothing identified yet
      TOKEN_KEYWORD, // Starts with a $; otherwise, same as a string
      TOKEN_STRING,  // Generic string token; ex: 92 foobar
      TOKEN_EOF,     // End of input
      TOKEN_ERROR    // An unknown token
   };

protected:
   int state;         // state of the scanner
   const char *input; // input string
   int idx;           // current position in input string
   int tokentype;     // type of current token
   qstring token;     // current token value

   void doStateScan();
   void doStateInToken();
   void doStateComment();

   // State table declaration
   static void (XLTokenizer::*States[])();

public:
   // Constructor / Destructor
   XLTokenizer(const char *str)
      : token(32), state(STATE_SCAN), input(str), idx(0), tokentype(TOKEN_NONE)
   {
   }

   int getNextToken();

   // Accessors
   int getTokenType() const { return tokentype; }
   qstring &getToken() { return token; }
};

//
// Tokenizer States
//

// Looking for the start of a new token
void XLTokenizer::doStateScan()
{
   switch(input[idx])
   {
   case ' ':
   case '\t':
   case '\r':
   case '\n':
      // remain in this state
      break;
   case '\0': // end of input
      tokentype = TOKEN_EOF;
      state     = STATE_DONE;
      break;
   case ';': // start of a comment
      state = STATE_COMMENT;
      break;
   default:
      // anything else is the start of a new token
      if(input[idx] == '$') // detect $ keywords
         tokentype = TOKEN_KEYWORD;
      else
         tokentype = TOKEN_STRING;
      state = STATE_INTOKEN;
      token += input[idx];
      break;
   }
}

// Scanning inside a token
void XLTokenizer::doStateInToken()
{
   switch(input[idx])
   {
   case ' ':  // whitespace
   case '\t':
   case '\r':
   case '\n':
      // end of token
      state = STATE_DONE;
      break;
   case '\0':  // end of input -OR- start of a comment
   case ';':
      --idx;   // backup, next call will handle it in STATE_SCAN.
      state = STATE_DONE;
      break;
   default:
      token += input[idx];
      break;
   }
}

// Reading out a single-line comment
void XLTokenizer::doStateComment()
{
   // consume all input to the end of the line
   if(input[idx] == '\n')
      state = STATE_SCAN;
   else if(input[idx] == '\0') // end of input
   {
      tokentype = TOKEN_EOF;
      state     = STATE_DONE;
   }
}

// State table for the tokenizer - static array of method pointers :)
void (XLTokenizer::* XLTokenizer::States[])() =
{
   &XLTokenizer::doStateScan,
   &XLTokenizer::doStateInToken,
   &XLTokenizer::doStateComment
};

//
// XLTokenizer::getNextToken
//
// Call this to retrieve the next token from the input string. The token
// type is returned for convenience. Get the text of the token using the
// GetToken() method.
//
int XLTokenizer::getNextToken()
{
   token.clear();
   state     = STATE_SCAN; // always start out scanning for a new token
   tokentype = TOKEN_NONE; // nothing has been determined yet

   // already at end of input?
   if(input[idx] != '\0')
   {
      while(state != STATE_DONE)
      {
         (this->*States[state])();
         ++idx;
      }
   }
   else
      tokentype = TOKEN_EOF;

   return tokentype;
}

//=============================================================================
//
// XLParser
//
// This is a module-local base class for Hexen lump parsers.
//

class XLParser
{
protected:
   // Data
   const char   *lumpname; // Name of lump handled by this parser
   char         *lumpdata; // Cached lump data
   WadDirectory *waddir;   // Current directory

   // Override me!
   virtual void startLump() {} // called at beginning of a new lump
   virtual void doToken(XLTokenizer &token) {} // called for each token

   void parseLump(WadDirectory &dir, lumpinfo_t *lump);
   void parseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump);

public:
   // Constructors
   XLParser(const char *pLumpname) : lumpname(pLumpname), lumpdata(NULL) {}

   // Destructor
   virtual ~XLParser()
   {
      // kill off any lump that might still be cached
      if(lumpdata)
      {
         efree(lumpdata);
         lumpdata = NULL;
      }
   }

   void parseAll(WadDirectory &dir);
   void parseNew(WadDirectory &dir);

   // Accessors
   const char *getLumpName() const { return lumpname; }
   void setLumpName(const char *s) { lumpname = s;    }
};

//
// XLParser::parseLump
//
// Parses a single lump.
//
void XLParser::parseLump(WadDirectory &dir, lumpinfo_t *lump)
{
   // free any previously loaded lump
   if(lumpdata)
   {
      efree(lumpdata);
      lumpdata = NULL;
   }

   waddir = &dir;
   startLump();

   // allocate at lump->size + 1 for null termination
   lumpdata = ecalloc(char *, 1, lump->size + 1);
   dir.readLump(lump->selfindex, lumpdata);

   XLTokenizer tokenizer = XLTokenizer(lumpdata);

   while(tokenizer.getNextToken() != XLTokenizer::TOKEN_EOF)
      doToken(tokenizer);
}

//
// XLParser::parseLumpRecursive
//
// Runs down the lumpinfo hash chain for the lumpname used by the descendent
// parser and parses each lump in order from oldest to newest.
//
void XLParser::parseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump)
{
   lumpinfo_t **lumpinfo = dir.getLumpInfo();

   // Recurse to parse next lump on the chain first
   if(curlump->next != -1)
      parseLumpRecursive(dir, lumpinfo[curlump->next]);

   // Parse this lump, provided it matches by name and is global
   if(!strncasecmp(curlump->name, lumpname, 8) &&
      curlump->li_namespace == lumpinfo_t::ns_global)
      parseLump(dir, curlump);
}

//
// XLParser::parseAll
//
// Call this to kick off a recursive parsing process.
//
void XLParser::parseAll(WadDirectory &dir)
{
   lumpinfo_t **lumpinfo = dir.getLumpInfo();
   lumpinfo_t  *root     = dir.getLumpNameChain(lumpname);
   if(root->index >= 0)
      parseLumpRecursive(dir, lumpinfo[root->index]);
}

//
// XLParser::parseNew
//
// Call this to only parse a SNDINFO from the given WadDirectory
//
void XLParser::parseNew(WadDirectory &dir)
{
   int lumpnum = dir.checkNumForName(lumpname);
   if(lumpnum >= 0)
      parseLump(dir, dir.getLumpInfo()[lumpnum]);
}

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
     KWD_EDFOVERRIDE,
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
   bool    edfOverRide; // if true, definitions can override EDF sounds
   int     musicmapnum;

   void doStateExpectCmd(XLTokenizer &token);
   void doStateExpectMapNum(XLTokenizer &token);
   void doStateExpectMusLump(XLTokenizer &token);
   void doStateExpectSndLump(XLTokenizer &token);
   void doStateEatToken(XLTokenizer &token);

   // State table declaration
   static void (XLSndInfoParser::*States[])(XLTokenizer &);

   virtual void doToken(XLTokenizer &token);
   virtual void startLump();

public:
   // Constructor
   XLSndInfoParser()
      : XLParser("SNDINFO"), soundname(32), edfOverRide(false), musicmapnum(0)
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
   "$edfoverride",
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
      case KWD_EDFOVERRIDE:
         edfOverRide = true; // set EDF override flag
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
            if(!(sfx->flags & SFXF_EDF) || edfOverRide)
            {
               sfx->flags &= ~SFXF_PREFIX;
               soundname.copyInto(sfx->name, 9);
            }
         } // create a new sound
         else
            E_NewSndInfoSound(soundname.constPtr(), soundlump.constPtr());

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
// XLSndInfoParser::DoToken
//
// Processes a token extracted from the SNDINFO input
//
void XLSndInfoParser::doToken(XLTokenizer &token)
{
   // Call handler method for the current state. Why is this done from
   // a virtual call-down? Because parent classes cannot call child
   // class method pointers! :P
   (this->*States[state])(token);
}

//
// XLSndInfoParser::StartLump
//
// Resets the parser at the beginning of a new SNDINFO lump.
//
void XLSndInfoParser::startLump()
{
   state = STATE_EXPECTCMD; // starting state
   edfOverRide = false;
}

//=============================================================================
//
// Risen3D MUSINFO
//

class XLMusInfoParser : public XLParser
{
protected:
   // state enumeration
   enum
   {
      STATE_EXPECTMAP,
      STATE_EXPECTMAPNUM,
      STATE_EXPECTMAPNUM2,
      STATE_EXPECTMUSLUMP
   };

   int     state;
   qstring mapname;
   qstring lumpname;
   int     mapnum;

   void doStateExpectMap(XLTokenizer &token);
   void doStateExpectMapNum(XLTokenizer &token);
   void doStateExpectMapNum2(XLTokenizer &token);
   void doStateExpectMusLump(XLTokenizer &token);

   void pushMusInfoDef();

   // State table declaration
   static void (XLMusInfoParser::*States[])(XLTokenizer &);

   virtual void doToken(XLTokenizer &token);
   virtual void startLump();

public:
   // Constructor
   XLMusInfoParser()
      : XLParser("MUSINFO"), state(STATE_EXPECTMAP), mapname(), mapnum(0)
   {
   }
};

//
// XLMusInfoParser::DoStateExpectMap
//
// Expecting a map name.
//
void XLMusInfoParser::doStateExpectMap(XLTokenizer &token)
{
   qstring &tokenText = token.getToken();

   switch(token.getTokenType())
   {
   case XLTokenizer::TOKEN_STRING:  // a normal string
      mapname = tokenText;          // remember the map name
      state = STATE_EXPECTMAPNUM;
      break;
   default:
      break;
   }
}

//
// XLMusInfoParser::doStateExpectMapNum
//
// Expecting a map number.
//
void XLMusInfoParser::doStateExpectMapNum(XLTokenizer &token)
{
   mapnum = token.getToken().toInt();
   state = STATE_EXPECTMUSLUMP;
}

//
// XLMusInfoParser::pushMusInfoDef
//
// Called when enough information is available to push a MUSINFO definition.
//
void XLMusInfoParser::pushMusInfoDef()
{
   if(mapnum >= 0 &&
      waddir->checkNumForName(mapname.constPtr())  >= 0 &&
      waddir->checkNumForName(lumpname.constPtr()) >= 0)
   {
      P_AddMusInfoMusic(mapname.constPtr(), mapnum, lumpname.constPtr());
   }
   mapnum = -1;
   lumpname.clear();
}

//
// XLMusInfoParser::doStateExpectMapNum2
//
// Expecting a map number, or the next map name.
//
void XLMusInfoParser::doStateExpectMapNum2(XLTokenizer &token)
{
   char *end = NULL;
   qstring &tokenText = token.getToken();
   long num = tokenText.toLong(&end, 10);

   if(end && *end != '\0') // not a number?
   {
      // push out any current definition
      pushMusInfoDef();

      // return to STATE_EXPECTMAP immediately
      state = STATE_EXPECTMAP;
      doStateExpectMap(token);
   }
   else
   {
      mapnum = (int)num;
      state = STATE_EXPECTMUSLUMP;
   }
}

//
// XLMusInfoParser::doStateExpectMusLump
//
// Expecting a music lump.
//
void XLMusInfoParser::doStateExpectMusLump(XLTokenizer &token)
{
   lumpname = token.getToken();
   pushMusInfoDef(); // push out the complete definition

   // expecting either another mapnum, or a new mapname token
   state = STATE_EXPECTMAPNUM2;
}

// State table for MUSINFO parser
void (XLMusInfoParser::* XLMusInfoParser::States[])(XLTokenizer &) =
{
   &XLMusInfoParser::doStateExpectMap,
   &XLMusInfoParser::doStateExpectMapNum,
   &XLMusInfoParser::doStateExpectMapNum2,
   &XLMusInfoParser::doStateExpectMusLump
};

//
// XLMusInfoParser::doToken
//
// Dispatch a token via this class's state table.
//
void XLMusInfoParser::doToken(XLTokenizer &token)
{
   (this->*States[state])(token);
}

//
// XLMusInfoParser::startLump
//
void XLMusInfoParser::startLump()
{
   // clear all data
   state = STATE_EXPECTMAP;
   mapname.clear();
   lumpname.clear();
   mapnum = -1;
}

//=============================================================================
//
// External Interface
//

//
// XL_ParseHexenScripts
//
// Parses all Hexen script lumps.
//
void XL_ParseHexenScripts()
{
   XLSndInfoParser sndInfoParser;
   XLMusInfoParser musInfoParser;

   sndInfoParser.parseAll(wGlobalDir);
   musInfoParser.parseAll(wGlobalDir);
}

// EOF