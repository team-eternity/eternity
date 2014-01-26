// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
#include "m_misc.h"
#include "w_wad.h"
#include "xl_mapinfo.h"
#include "xl_musinfo.h"
#include "xl_scripts.h"

//=============================================================================
//
// XLTokenizer
//
// This class does proper FSA tokenization for Hexen lump parsers.
//

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
   case '"': // start of quoted string
      tokentype = TOKEN_STRING;
      state     = STATE_QUOTED;
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

// Reading out a quoted string token
void XLTokenizer::doStateQuoted()
{
   switch(input[idx])
   {
   case '"':  // end of quoted string
      state = STATE_DONE;
      break;
   case '\0': // end of input (technically, this is malformed)
      --idx;
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
   &XLTokenizer::doStateQuoted,
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
// Base class for Hexen lump parsers
//

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

   // check with EDF SHA-1 cache that this lump hasn't already been processed
   if(E_CheckInclude(lumpdata, lump->size))
   {
      XLTokenizer tokenizer = XLTokenizer(lumpdata);

      while(tokenizer.getNextToken() != XLTokenizer::TOKEN_EOF)
      {
         if(!doToken(tokenizer))
            break; // the subclassed parser wants to stop parsing
      }
   }
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
   if(curlump->namehash.next != -1)
      parseLumpRecursive(dir, lumpinfo[curlump->namehash.next]);

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
   if(root->namehash.index >= 0)
      parseLumpRecursive(dir, lumpinfo[root->namehash.index]);
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
// External Interface
//

//
// XL_ParseHexenScripts
//
// Parses Hexen script lumps not handled elsewhere.
//
void XL_ParseHexenScripts()
{
   XL_ParseMapInfo(); // Hexen MAPINFO
   XL_ParseMusInfo(); // Risen3D MUSINFO
}

// EOF

