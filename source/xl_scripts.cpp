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
#include "w_wad.h"
#include "xl_animdefs.h"
#include "xl_emapinfo.h"
#include "xl_mapinfo.h"
#include "xl_musinfo.h"
#include "xl_scripts.h"
#include "xl_umapinfo.h"

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
   char c = input[idx];

   switch(c)
   {
   case ' ':
   case '\t':
   case '\r':
      // remain in this state
      break; 
   case '\n':
      // if linebreak tokens are enabled, return one now
      if(flags & TF_LINEBREAKS)
      {
         tokentype = TOKEN_LINEBREAK;
         state     = STATE_DONE;
      }
      // otherwise, remain in STATE_SCAN
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
      if(c == '#' && (flags & TF_HASHCOMMENTS)) // possible start of hash comment
      {
         state = STATE_COMMENT;
         break;
      }
      else if(c == '/' && input[idx+1] == '/' && (flags & TF_SLASHCOMMENTS))
      {
         state = STATE_COMMENT;
         break;
      }
      else if(c == '[' && (flags & TF_BRACKETS))
      {
         tokentype = TOKEN_BRACKETSTR;
         state     = STATE_INBRACKETS;
         break;
      }
      else if(c == '$' || flags & TF_STRINGSQUOTED) // detect $ keywords (or without $ if flagged)
         tokentype = TOKEN_KEYWORD;
      else
         tokentype = TOKEN_STRING;
      state = STATE_INTOKEN;
      token += c;
      break;
   }
}

//
// True if it's alphanumeric or _
//
inline static bool XL_isIdentifierChar(char c)
{
   return ectype::isAlnum(c) || c == '_';
}

// Scanning inside a token
void XLTokenizer::doStateInToken() 
{
   char c = input[idx];

   switch(c)
   {
   case '\n':
      if(flags & TF_LINEBREAKS) // if linebreaks are tokens, we need to back up
         --idx;
      // fall through
   case ' ':  // whitespace
   case '\t':
   case '\r':
      // end of token
      state = STATE_DONE;
      break;
   case '\0':  // end of input -OR- start of a comment
   case ';':   
      --idx;   // backup, next call will handle it in STATE_SCAN.
      state = STATE_DONE;
      break;
   default:
      if((c == '#' && flags & TF_HASHCOMMENTS) ||
         (c == '/' && input[idx+1] == '/' && flags & TF_SLASHCOMMENTS) ||
         (flags & TF_OPERATORS && !token.empty() &&
          XL_isIdentifierChar(c) != XL_isIdentifierChar(token[0])) ||
         (c == '"' && tokentype == TOKEN_KEYWORD))
      {
         // hashes may conditionally be supported as comments
         // double slashes may conditionally be supported as comments
         // operators and identifiers are separate
         // starting strings next to keywords should be detected
         --idx;
         state = STATE_DONE;
         break;
      }
      token += c;
      break;
   }
}

// Reading out a bracketed string token
void XLTokenizer::doStateInBrackets()
{
   switch(input[idx])
   {
   case ']':  // end of bracketed token
      state = STATE_DONE;
      break;
   case '\0': // end of input (technically, malformed)
      --idx;
      state = STATE_DONE;
      break;
   default:   // anything else is part of the token
      token += input[idx];
      break;
   }
}

// Reading out a quoted string token
void XLTokenizer::doStateQuoted()
{
   char c;
   int i;

   switch(input[idx])
   {
   case '"':  // end of quoted string
      state = STATE_DONE;
      break;
   case '\0': // end of input (technically, this is malformed)
      --idx;
      state = STATE_DONE;
      break;
   case '\\':
      if(!(flags & TF_ESCAPESTRINGS))
      {
         token += input[idx];
         break;
      }
      // Increase IDX, check against '\0' for guarding.
      if(input[++idx] == '\0')
      {
         --idx;
         state = STATE_DONE;
         break;
      }
      switch(input[idx])   // these correspond to the C escape sequences
      {
         case 'a':
            token += '\a';
            break;
         case 'b':
            token += '\b';
            break;
         case 'f':
            token += '\f';
            break;
         case 'n':
            token += '\n';
            break;
         case 't':
            token += '\t';
            break;
         case 'r':
            token += '\r';
            break;
         case 'v':
            token += '\v';
            break;
         case '?':
            token += '\?';
            break;
         case '\n':  // for escaping newlines
            break;
         case 'x':
         case 'X':
            c = 0;
            for(i = 0; i < 2; ++i)
            {
               idx++;
               if(input[idx] >= '0' && input[idx] <= '9')
                  c = (c << 4) + input[idx] - '0';
               else if(input[idx] >= 'a' && input[idx] <= 'f')
                  c = (c << 4) + 10 + input[idx] - 'a';
               else if(input[idx] >= 'A' && input[idx] <= 'F')
                  c = (c << 4) + 10 + input[idx] - 'A';
               else
               {
                  idx--;
                  break;
               }
            }
            token += c;
            break;
         case '0':
         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
            c = input[idx] - '0';
            for(i = 0; i < 2; ++i)
            {
               idx++;
               if(input[idx] >= '0' && input[idx] <= '7')
                  c = (c << 3) + input[idx] - '0';
               else
               {
                  idx--;
                  break;
               }
            }
            token += c;
            break;
         default:
            token += input[idx];
            break;
      }
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
   {
      // if linebreak tokens are enabled, send one now
      if(flags & TF_LINEBREAKS)
      {
         tokentype = TOKEN_LINEBREAK;
         state     = STATE_DONE;
      }
      else
         state = STATE_SCAN;
   }
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
   &XLTokenizer::doStateInBrackets,
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
void XLParser::parseLump(WadDirectory &dir, lumpinfo_t *lump, bool global) 
{
   // free any previously loaded lump
   if(lumpdata)
   {
      efree(lumpdata);
      lumpdata = nullptr;
   }

   // can't parse empty lumps
   if(!lump->size)
      return;

   waddir = &dir;
   startLump();

   // allocate at lump->size + 2 for null termination and look-ahead padding
   lumpdata = ecalloc(char *, 1, lump->size + 2);
   dir.readLump(lump->selfindex, lumpdata);

   // check with EDF SHA-1 cache that this lump hasn't already been processed,
   // unless the data source is marked as non-global (which means, parse it every time)
   if(!global || E_CheckInclude(lumpdata, lump->size))
   {
      XLTokenizer tokenizer(lumpdata);
      bool early = false;

      // allow subclasses to alter properties of the tokenizer now before parsing begins
      initTokenizer(tokenizer);

      while(tokenizer.getNextToken() != XLTokenizer::TOKEN_EOF)
      {
         if(!doToken(tokenizer))
         {
            early = true;
            break; // the subclassed parser wants to stop parsing
         }
      }

      // allow subclasses to handle EOF
      onEOF(early);
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
   if(curlump->next != -1)
      parseLumpRecursive(dir, lumpinfo[curlump->next]);

   // Parse this lump, provided it matches by name and is global
   if(!strncasecmp(curlump->name, lumpname, 8) &&
      curlump->li_namespace == lumpinfo_t::ns_global)
      parseLump(dir, curlump, true);
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
      parseLump(dir, dir.getLumpInfo()[lumpnum], true);
}

//
// Builds the intermission-needed set of mapinfos from the available XL lumps
//
static void XL_buildInterMapInfo()
{
   // First, visit UMAPINFO
   XL_BuildInterUMapInfo();
   // Then, override with EMAPINFO
   XL_BuildInterEMapInfo();

   // Episode menu from UMAPINFO
   XL_BuildUMapInfoEpisodes();

   // FIXME: MAPINFO is meant only for Hexen, which doesn't have Doom-style in-
   // termission anyway. But maybe we should use its fields.
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
   XL_ParseEMapInfo(); // Eternity: EMAPINFO
   XL_ParseMapInfo();  // Hexen:    MAPINFO
   XL_ParseMusInfo();  // Risen3D:  MUSINFO
   XL_ParseAnimDefs();  // Hexen: ANIMDEFS

   XL_ParseUMapInfo();  // Universal MAPINFO new format

   XL_buildInterMapInfo();
}

// EOF

