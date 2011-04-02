// Emacs style mode select -*- C++ -*-
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

   // Parser States

   void DoStateScan() // looking for the start of a new token
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

   void DoStateInToken() // scanning inside a token
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

   void DoStateComment() // reading out a single-line comment
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

   // State table declaration
   static void (XLTokenizer::*states[])();

public:
   // Constructor / Destructor
   XLTokenizer(const char *str) 
      : token(), state(STATE_SCAN), input(str), idx(0), tokentype(TOKEN_NONE)
   { 
   }

   //
   // XLTokenizer::GetNextToken
   //
   // Call this to retrieve the next token from the input string. The token
   // type is returned for convenience. Get the text of the token using the
   // GetToken() method.
   //
   int GetNextToken()
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
   
   // Accessors
   int GetTokenType() const { return tokentype; }
   qstring &GetToken() { return token; }
};

// State table for the tokenizer - static array of method pointers :)
void (XLTokenizer::* XLTokenizer::states[])() =
{
   &XLTokenizer::DoStateScan,
   &XLTokenizer::DoStateInToken,
   &XLTokenizer::DoStateComment
};

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
   const char  *lumpname;    // Name of lump handled by this parser
   char        *lumpdata;    // Cached lump data

   // Override me!
   virtual void ParseLine(char *line) {} // called for every line of data
   virtual void StartLump() {}           // called at beginning of a new lump

   //
   // XLParser::ParseLump
   //
   // Parses a single lump.
   //
   void ParseLump(WadDirectory &dir, lumpinfo_t *lump) 
   {
      char *rover, *line; 

      // free any previously loaded lump
      if(lumpdata)
      {
         free(lumpdata);
         lumpdata = NULL;
      }

      StartLump();

      // allocate at lump->size + 1 for null termination
      rover = lumpdata = (char *)(calloc(1, lump->size + 1));
      dir.ReadLump(lump->selfindex, lumpdata);

      // parse each line of the lump independently
      while((line = E_GetHeredocLine(&rover)))
         ParseLine(line);
   }

   //
   // XLParser::ParseLumpRecursive
   //
   // Runs down the lumpinfo hash chain for the lumpname used by the descendent
   // parser and parses each lump in order from oldest to newest.
   //
   void ParseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump)
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

public:
   // Constructors
   XLParser(const char *p_lumpname) : lumpname(p_lumpname), lumpdata(NULL) {}

   // Destructors
   virtual ~XLParser() 
   {
      // kill off any lump that might still be cached
      if(lumpdata)
      {
         free(lumpdata);
         lumpdata = NULL;
      }
   }

   //
   // XLParser::ParseAll
   //
   // Call this to kick off a recursive parsing process.
   //
   void ParseAll(WadDirectory &dir)
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
   void ParseNew(WadDirectory &dir)
   {
      int lumpnum = dir.CheckNumForName(lumpname);
      if(lumpnum >= 0)
         ParseLump(dir, dir.GetLumpInfo()[lumpnum]);
   }

   // Accessors
   const char *GetLumpName() const { return lumpname; }
   void SetLumpName(const char *s) { lumpname = s;    }
};

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

   boolean edfOverRide; // if true, definitions can override EDF sounds

   //
   // XLSndInfoParser::HandleMusic
   //
   // Creates or edits a music definition
   //
   void HandleMusic(tempcmd_t &cmd)
   {
      // need two argument tokens on the line
      if(!cmd.strs[1] || !cmd.strs[2])
         return;
   }

   //
   // XLSndInfoParser::HandleSound
   //
   // Creates or edits a sound definition
   //
   void HandleSound(tempcmd_t &cmd)
   {
      sfxinfo_t *sfx;

      if(!cmd.strs[1]) // need one argument token
         return;

      if((sfx = E_SoundForName(cmd.strs[0]))) // defined already?
      {
         if(!(sfx->flags & SFXF_EDF) || edfOverRide)
         {
            sfx->flags &= ~SFXF_PREFIX;
            strncpy(sfx->name, cmd.strs[1], 9);
         }
      }
      else // create a new sound
         E_NewSndInfoSound(cmd.strs[0], cmd.strs[1]);
   }

   //
   // XLSndInfoParser::ParseLine
   //
   // Processes individual lines of SNDINFO data
   //
   virtual void ParseLine(char *line)
   {
      tempcmd_t cmd = E_ParseTextLine(line);

      if(!cmd.strs[0] || cmd.strs[0][0] == ';') // empty line or comment
         return;

      if(cmd.strs[0][0] == '$') // keyword?
      {
         int kwd = E_StrToNumLinear(sndInfoKwds, NUMKWDS, cmd.strs[0]);
         if(kwd == NUMKWDS) // unknown keyword?
            return;

         switch(kwd)
         {
         case KWD_MAP: // music definition
            HandleMusic(cmd);
            break;
         case KWD_EDFOVERRIDE:
            edfOverRide = true; // enable overriding EDF sounds
            break;
         default: // a no-op, or something unhandled for the time being
            break;
         }
      }
      else // anything else is a sound definition
         HandleSound(cmd);
   }

   //
   // XLSndInfoParser::StartLump
   //
   // Resets the parser at the beginning of a new SNDINFO lump.
   //
   virtual void StartLump()
   {
      edfOverRide = false;
   }

public:
   // Constructor
   XLSndInfoParser() : XLParser("SNDINFO"), edfOverRide(false) {}
};

// Keywords for SNDINFO
// Note that all ZDoom extensions are included, even though they are not 
// supported yet. This is so that they are properly documented and ignored in
// the meanwhile.
const char *XLSndInfoParser::sndInfoKwds[] =
{
   "$random",
   "$alias",
   "$limit",
   "$singular",
   "$pitchshift",
   "$volume",
   "$rolloff",
   "$playersound",
   "$playersounddup",
   "$playeralias",
   "$playercompat",
   "$ambient",
   "$ifdoom",
   "$ifheretic",
   "$ifhexen",
   "$ifstrife",
   "$endif",
   "$map",
   "$musicvolume",
   "$registered",
   "$archivepath",
   "$mididevice",
   "$edfoverride"
};

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

