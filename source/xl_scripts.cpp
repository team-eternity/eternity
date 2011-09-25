// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
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
//
//   By James Haley
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "e_lib.h"
#include "e_sound.h"
#include "m_misc.h"
#include "m_qstr.h"
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

   void DoStateScan();
   void DoStateInToken();
   void DoStateComment();

   // State table declaration
   static void (XLTokenizer::*states[])();

public:
   // Constructor / Destructor
   XLTokenizer(const char *str) 
      : token(32), state(STATE_SCAN), input(str), idx(0), tokentype(TOKEN_NONE)
   { 
   }

   int GetNextToken();
   
   // Accessors
   int GetTokenType() const { return tokentype; }
   qstring &GetToken() { return token; }
};

//
// Tokenizer States
//

// Looking for the start of a new token
void XLTokenizer::DoStateScan()
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
void XLTokenizer::DoStateInToken() 
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
void XLTokenizer::DoStateComment()
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
void (XLTokenizer::* XLTokenizer::states[])() =
{
   &XLTokenizer::DoStateScan,
   &XLTokenizer::DoStateInToken,
   &XLTokenizer::DoStateComment
};

//
// XLTokenizer::GetNextToken
//
// Call this to retrieve the next token from the input string. The token
// type is returned for convenience. Get the text of the token using the
// GetToken() method.
//
int XLTokenizer::GetNextToken()
{
   token.clear();
   state     = STATE_SCAN; // always start out scanning for a new token
   tokentype = TOKEN_NONE; // nothing has been determined yet

   // already at end of input?
   if(input[idx] != '\0')
   {
      while(state != STATE_DONE)
      {
         (this->*states[state])();
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
   virtual void StartLump() {} // called at beginning of a new lump
   virtual void DoToken(XLTokenizer &token) {} // called for each token

   void ParseLump(WadDirectory &dir, lumpinfo_t *lump);
   void ParseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump);

public:
   // Constructors
   XLParser(const char *pLumpname) : lumpname(pLumpname), lumpdata(NULL) {}

   // Destructor
   virtual ~XLParser() 
   {
      // kill off any lump that might still be cached
      if(lumpdata)
      {
         free(lumpdata);
         lumpdata = NULL;
      }
   }

   void ParseAll(WadDirectory &dir);
   void ParseNew(WadDirectory &dir);

   // Accessors
   const char *GetLumpName() const { return lumpname; }
   void SetLumpName(const char *s) { lumpname = s;    }
};

//
// XLParser::ParseLump
//
// Parses a single lump.
//
void XLParser::ParseLump(WadDirectory &dir, lumpinfo_t *lump) 
{
   // free any previously loaded lump
   if(lumpdata)
   {
      free(lumpdata);
      lumpdata = NULL;
   }

   waddir = &dir;
   StartLump();

   // allocate at lump->size + 1 for null termination
   lumpdata = (char *)(calloc(1, lump->size + 1));
   dir.ReadLump(lump->selfindex, lumpdata);

   XLTokenizer tokenizer = XLTokenizer(lumpdata);

   while(tokenizer.GetNextToken() != XLTokenizer::TOKEN_EOF)
      DoToken(tokenizer);
}

//
// XLParser::ParseLumpRecursive
//
// Runs down the lumpinfo hash chain for the lumpname used by the descendent
// parser and parses each lump in order from oldest to newest.
//
void XLParser::ParseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump)
{
   lumpinfo_t **lumpinfo = dir.GetLumpInfo();

   // Recurse to parse next lump on the chain first
   if(curlump->next != -1)
      ParseLumpRecursive(dir, lumpinfo[curlump->next]);

   // Parse this lump, provided it matches by name and is global
   if(!strncasecmp(curlump->name, lumpname, 8) &&
      curlump->li_namespace == lumpinfo_t::ns_global)
      ParseLump(dir, curlump);
}

//
// XLParser::ParseAll
//
// Call this to kick off a recursive parsing process.
//
void XLParser::ParseAll(WadDirectory &dir)
{
   lumpinfo_t **lumpinfo = dir.GetLumpInfo();
   lumpinfo_t  *root     = dir.GetLumpNameChain(lumpname);
   if(root->index >= 0)
      ParseLumpRecursive(dir, lumpinfo[root->index]);
}

//
// XLParser::ParseNew
//
// Call this to only parse a SNDINFO from the given WadDirectory
//
void XLParser::ParseNew(WadDirectory &dir)
{
   int lumpnum = dir.CheckNumForName(lumpname);
   if(lumpnum >= 0)
      ParseLump(dir, dir.GetLumpInfo()[lumpnum]);
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
     KWD_RANDOM,
     KWD_ALIAS,
     KWD_LIMIT,
     KWD_SINGULAR,
     KWD_PITCHSHIFT,
     KWD_VOLUME,
     KWD_ROLLOFF,
     KWD_PLAYERSOUND,
     KWD_PLAYERSOUNDDUP,
     KWD_PLAYERALIAS,
     KWD_PLAYERCOMPAT,
     KWD_AMBIENT,
     KWD_IFDOOM,
     KWD_IFHERETIC,
     KWD_IFHEXEN,
     KWD_IFSTRIFE,
     KWD_ENDIF,
     KWD_MAP,
     KWD_MUSICVOLUME,
     KWD_REGISTERED,
     KWD_ARCHIVEPATH,
     KWD_MIDIDEVICE,
     KWD_EDFOVERRIDE,
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

   int state;
   qstring soundname;
   bool edfOverRide; // if true, definitions can override EDF sounds

   void DoStateExpectCmd(XLTokenizer &token);
   void DoStateExpectMapNum(XLTokenizer &token);
   void DoStateExpectMusLump(XLTokenizer &token);
   void DoStateExpectSndLump(XLTokenizer &token);
   void DoStateEatToken(XLTokenizer &token);

   // State table declaration
   static void (XLSndInfoParser::*states[])(XLTokenizer &);

   virtual void DoToken(XLTokenizer &token);
   virtual void StartLump();

public:
   // Constructor
   XLSndInfoParser() 
      : XLParser("SNDINFO"), soundname(32), edfOverRide(false)
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
void XLSndInfoParser::DoStateExpectCmd(XLTokenizer &token)
{
   int cmdnum;
   qstring &tokenText = token.GetToken();

   switch(token.GetTokenType())
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
void XLSndInfoParser::DoStateExpectMapNum(XLTokenizer &token)
{
   // TODO
   state = STATE_EXPECTMUSLUMP;
}

// Expecting the music lump name after a map number
void XLSndInfoParser::DoStateExpectMusLump(XLTokenizer &token)
{
   // TODO
   state = STATE_EXPECTCMD;
}

// Expecting the lump name after a sound definition
void XLSndInfoParser::DoStateExpectSndLump(XLTokenizer &token)
{
   if(token.GetTokenType() != XLTokenizer::TOKEN_STRING)
   {
      // Not a string? We are probably in an error state.
      // Get out with an immediate call to the expect command state
      state = STATE_EXPECTCMD;
      DoStateExpectCmd(token);
   }
   else
   {
      qstring &soundlump = token.GetToken();

      // Lump must exist, otherwise we create erroneous sounds if there are
      // unknown keywords in the lump. Thanks to ZDoom for defining such a 
      // clean, context-free, grammar-based language with delimiters :>
      if(soundlump.length() <= 8 &&
         waddir->CheckNumForName(soundlump.constPtr()) != -1)
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
      }
      else // Otherwise we might be off due to unknown tokens; return to ExpectCmd
      {
         state = STATE_EXPECTCMD;
         DoStateExpectCmd(token);
         return;
      }

      // Return to expecting a command
      state = STATE_EXPECTCMD;
   }
}

// Throw away a token unconditionally
void XLSndInfoParser::DoStateEatToken(XLTokenizer &token)
{
   state = STATE_EXPECTCMD; // return to expecting a command
}

// State table for SNDINFO parser
void (XLSndInfoParser::* XLSndInfoParser::states[])(XLTokenizer &) =
{
   &XLSndInfoParser::DoStateExpectCmd,
   &XLSndInfoParser::DoStateExpectMapNum,
   &XLSndInfoParser::DoStateExpectMusLump,
   &XLSndInfoParser::DoStateExpectSndLump,
   &XLSndInfoParser::DoStateEatToken
};

//
// XLSndInfoParser::DoToken
//
// Processes a token extracted from the SNDINFO input
//
void XLSndInfoParser::DoToken(XLTokenizer &token)
{
   // Call handler method for the current state. Why is this done from
   // a virtual call-down? Because parent classes cannot call child
   // class method pointers! :P
   (this->*states[state])(token);
}

//
// XLSndInfoParser::StartLump
//
// Resets the parser at the beginning of a new SNDINFO lump.
//
void XLSndInfoParser::StartLump()
{
   state = STATE_EXPECTCMD; // starting state
   edfOverRide = false;
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

   sndInfoParser.ParseAll(wGlobalDir);
}

// EOF

