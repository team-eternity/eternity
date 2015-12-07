// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright (C) 2015 Ioan Chera et al.
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
//----------------------------------------------------------------------------
//
// Universal Doom Map Format, for Eternity
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomtype.h"
#include "m_collection.h"
#include "m_ctype.h"
#include "m_qstr.h"
#include "metaapi.h"
#include "w_wad.h"
#include "z_auto.h"

//
// Excerpt from the UDMF specifications 1.1 Grammar / Syntax
// http://www.doomworld.com/eternity/engine/stuff/udmf11.txt
//
// translation_unit := global_expr_list
// global_expr_list := global_expr global_expr_list
// global_expr := block | assignment_expr
// block := identifier '{' expr_list '}'
// expr_list := assignment_expr expr_list
// assignment_expr := identifier '=' value ';' | nil
// identifier := [A-Za-z_]+[A-Za-z0-9_]*
// value := integer | float | quoted_string | keyword
// integer := [+-]?[1-9]+[0-9]* | 0[0-9]+ | 0x[0-9A-Fa-f]+
// float := [+-]?[0-9]+'.'[0-9]*([eE][+-]?[0-9]+)?
// quoted_string := "([^"\\]*(\\.[^"\\]*)*)"
// keyword := [^{}();"'\n\t ]+
//

//
// As we see, TEXTMAP consists of global_expr entries. Each global_expr is
// a metatable. 
//

//
// E_advanceNextSymbol
//
// Advances the pointer until first non-whitespace character is encountered.
// - pc: pointer to increase. In-out and used by the caller
// - end: pointer to end of buffer.
// Returns true if it didn't reach the end of buffer.
//
static bool E_advanceNextSymbol(char **pc, const char *end)
{
   while(*pc < end && ectype::isSpace(**pc))
      ++*pc;
   return *pc < end;
}

//
// E_checkIdentifier
//
// Checks that an identifier is matched
//
static bool E_checkIdentifier(char **pc, const char *end, qstring &result)
{
   const char *ret = nullptr;

   if(!E_advanceNextSymbol(pc, end))
      return false;
   if(!ectype::isAlpha(**pc))
      return false;
   ret = *pc;
   ++*pc;
   for(; *pc < end && !ectype::isSpace(**pc); ++*pc)
   {
      if(!ectype::isAlnum(**pc))
         return false;
   }
   result.copy(ret, *pc - ret);

   return true;
}

//
// E_checkValue
//
// Checks that a sequence forms a string, number or miscellaneous keyword
//
static bool E_checkValue(char **pc, const char *end, qstring &result)
{
   if(!E_advanceNextSymbol(pc, end))
      return false;
   if(**pc == '"')
   {
      result << '"'; // include quote to distinguish string
      if(++*pc == end)  // advance one bit
         return false;
      bool escape = false;
      result.clear();
      for(; *pc < end; ++*pc)
      {
         if(!escape)
         {
            if(**pc == '\\')
            {
               result << '\\';   // also include backslash
               escape = true;
            }
            else if(**pc == '"')
               break;
            else
               result << **pc;
         }
         else
         {
            // also add characters such as \ or "
            result << **pc;
            escape = false;
         }
      }
      result << '"';
      return *pc < end; // will return false if string wasn't ended
   }
   else
   {
      // not a string
      result.clear();
      for(; *pc < end; ++*pc)
      {
         switch(**pc)
         {
         case '{':
         case '}':
         case '(':
         case ')':
         case '"':
         case '\'':
            return false;
         case ';':
            goto exitFor;
         default:
            result << **pc;
            break;
         }
         if(ectype::isSpace(**pc))
            break;
      }
   exitFor:
      return *pc < end;
   }
}

//
// E_parseUDMF
//
// Parses UDMF text into a collection of metatables.
// For simplicity, it will preprocess stuff by removing comments first.
//
// Arguments:
// - textMap: collection of elements to populate
// - data: raw textmap data (string)
// - size: size of string
// Returns true on success, false on error
//
static bool E_parseUDMFText(PODCollection<MetaTable *> &textMap, char *data, int size)
{
   char *pc, c;
   const char *const end = data + size;

   int comment = 0;   // 1: singleline, 2: multiline
   bool instring = false;

   //
   // First, walk through the entire TEXTMAP to replace comments with spaces
   //
   for(pc = data; pc < end; ++pc)
   {
      c = *pc;
      if(comment == 1)
      {
         if(c == '\r' || c == '\n')
            comment = 0;
         else 
            *pc = ' ';
         continue;
      }
      if(comment == 2)
      {
         if(c == '*' && pc < end - 1 && *(pc + 1) == '/')
         {
            comment = 0;
            *(pc + 1) = ' ';
         }
         *pc = ' ';
         continue;
      }
      if(!instring)
      {
         if(c == '"')
            instring = true;
         else if(c == '/' && pc < end - 1)
         {
            if(*(pc + 1) == '/')
            {
               comment = 1;
               *pc = ' ';
               *(pc + 1) = ' ';
            }
            else if(*(pc + 1) == '*')
            {
               comment = 2;
               *pc = ' ';
               *(pc + 1) = ' ';
            }
         }
         continue;
      }
      if(instring && c == '"' && pc > data && *(pc - 1) != '\\')
      {
         instring = false;
         continue;
      }
   }

   // Now it has been cleaned up.

   qstring identifier;
   qstring subIdentifier;
   qstring text;  // string value

   //
   // Now do the actual parsing
   //
   for(pc = data; pc < end; ++pc)
   {
      // Expect identifier
      if(!E_checkIdentifier(&pc, end, identifier))
         return false;
      // Expect { or =
      if(!E_advanceNextSymbol(&pc, end))
         return false;
      if(*pc == '=')
      {
         // we have a value
         if(++pc == end)
            return false;
         if(!E_checkValue(&pc, end, text))
            return false;
         if(!E_advanceNextSymbol(&pc, end))
            return false;
         if(*pc != ';')
            return false;
         // TODO: add assignment expression to list
      }
      else if(*pc == '{')
      {
         // we have a block
         if(++pc == end)
            return false;

         do
         {
            if(!E_checkIdentifier(&pc, end, subIdentifier))
               return false;
            if(!E_advanceNextSymbol(&pc, end))
               return false;
            if(*pc != '=')
               return false;
            if(++pc == end)
               return false;
            if(!E_checkValue(&pc, end, text))
               return false;
            if(!E_advanceNextSymbol(&pc, end))
               return false;
            if(*pc != ';')
               return false;
            if(++pc == end)
               return false;
            if(!E_advanceNextSymbol(&pc, end))
               return false;
            // TODO: add kv pair to current block
         } while(*pc != '}');
         
         if(++pc == end)
            return false;
         // TODO: add block
      }
      else
         return false;
   }

   // TODO: whatever finalization is needed

   return true;
}

//
// E_LoadUDMF
//
// Load UDMF TEXTMAP lump.
// - setupwad: current WadDirectory. Probably a global variable in p_setup.cpp
// - lump: index of TEXTMAP lump.
//
void E_LoadUDMF(WadDirectory &setupwad, int lump)
{
   ZAutoBuffer buf;

   // FIXME: is it safe to load such a big string in memory?
   setupwad.cacheLumpAuto(lump, buf);
   auto data = buf.getAs<char *>();

   // Use a collection because order is important, not key uniqueness
   PODCollection<MetaTable *> textMap;
   E_parseUDMFText(textMap, data, setupwad.lumpLength(lump));

   // TODO: process the data
}

// EOF

