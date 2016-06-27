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

#include "doomstat.h"
#include "e_exdata.h"
#include "e_udmf.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_auto.h"

//==============================================================================
//
// Collecting and processing
//
//==============================================================================

//
// Reads the raw vertices and obtains final ones
//
void UDMFParser::loadVertices() const
{
   numvertexes = (int)mVertices.getLength();
   vertexes = estructalloctag(vertex_t, numvertexes, PU_LEVEL);
   for(int i = 0; i < numvertexes; i++)
   {
      vertexes[i].x = mVertices[i].x;
      vertexes[i].y = mVertices[i].y;
      // SoM: Cardboard stores float versions of vertices.
      vertexes[i].fx = M_FixedToFloat(vertexes[i].x);
      vertexes[i].fy = M_FixedToFloat(vertexes[i].y);
   }
}

//
// Loads sectors
//
void UDMFParser::loadSectors() const
{
   numsectors = (int)mSectors.getLength();
   sectors = estructalloctag(sector_t, numsectors, PU_LEVEL);
   for(int i = 0; i < numsectors; ++i)
   {
      sector_t *ss = sectors + i;
      if(mNamespace == namespace_Eternity)
      {
         ss->floorheight = mSectors[i].heightfloor;
         ss->ceilingheight = mSectors[i].heightceiling;
      }
      else
      {
         ss->floorheight = mSectors[i].heightfloor << FRACBITS;
         ss->ceilingheight = mSectors[i].heightceiling << FRACBITS;
      }
      ss->floorpic = R_FindFlat(mSectors[i].texturefloor.constPtr());
      P_SetSectorCeilingPic(ss,
                            R_FindFlat(mSectors[i].textureceiling.constPtr()));
      ss->lightlevel = mSectors[i].lightlevel;
      ss->special = mSectors[i].special;
      ss->tag = mSectors[i].identifier;
      P_InitSector(ss);
   }
}

//
// Loads sidedefs
//
void UDMFParser::loadSidedefs() const
{
   numsides = (int)mSidedefs.getLength();
   sides = estructalloctag(side_t, numsides, PU_LEVEL);
}

//
// Loads linedefs. Returns false on error.
//
bool UDMFParser::loadLinedefs()
{
   numlines = (int)mLinedefs.getLength();
   lines = estructalloctag(line_t, numlines, PU_LEVEL);
   for(int i = 0; i < numlines; ++i)
   {
      line_t *ld = lines + i;
      const ULinedef &uld = mLinedefs[i];
      if(uld.blocking)
         ld->flags |= ML_BLOCKING;
      if(uld.blockmonsters)
         ld->flags |= ML_BLOCKMONSTERS;
      if(uld.twosided)
         ld->flags |= ML_TWOSIDED;
      if(uld.dontpegtop)
         ld->flags |= ML_DONTPEGTOP;
      if(uld.dontpegbottom)
         ld->flags |= ML_DONTPEGBOTTOM;
      if(uld.secret)
         ld->flags |= ML_SECRET;
      if(uld.blocksound)
         ld->flags |= ML_SOUNDBLOCK;
      if(uld.dontdraw)
         ld->flags |= ML_DONTDRAW;
      if(uld.mapped)
         ld->flags |= ML_MAPPED;
      if(uld.passuse)
         ld->flags |= ML_PASSUSE;

      // Eternity
      if(uld.midtex3d)
         ld->flags |= ML_3DMIDTEX;

      // TODO: Strife

      if(uld.playercross)
         ld->extflags |= EX_ML_PLAYER | EX_ML_CROSS;
      if(uld.playeruse)
         ld->extflags |= EX_ML_PLAYER | EX_ML_USE;
      if(uld.monstercross)
         ld->extflags |= EX_ML_MONSTER | EX_ML_CROSS;
      if(uld.monsteruse)
         ld->extflags |= EX_ML_MONSTER | EX_ML_USE;
      if(uld.impact)
         ld->extflags |= EX_ML_MISSILE | EX_ML_IMPACT;
      if(uld.playerpush)
         ld->extflags |= EX_ML_PLAYER | EX_ML_PUSH;
      if(uld.monsterpush)
         ld->extflags |= EX_ML_MONSTER | EX_ML_PUSH;
      if(uld.missilecross)
         ld->extflags |= EX_ML_MISSILE | EX_ML_CROSS;
      if(uld.repeatspecial)
         ld->extflags |= EX_ML_REPEAT;

      ld->special = uld.special;
      ld->tag = uld.identifier;
      ld->args[0] = uld.arg[0];
      ld->args[1] = uld.arg[1];
      ld->args[2] = uld.arg[2];
      ld->args[3] = uld.arg[3];
      ld->args[4] = uld.arg[4];
      if(uld.v1 < 0 || uld.v1 >= numvertexes ||
         uld.v2 < 0 || uld.v2 >= numvertexes ||
         uld.sidefront < 0 || uld.sidefront >= numsides ||
         uld.sideback < -1 || uld.sideback >= numsides)
      {
         mLine = uld.errorline;
         mColumn = 1;
         mError = "Vertex or sidedef overflow";
         return false;
      }
      ld->v1 = &vertexes[uld.v1];
      ld->v2 = &vertexes[uld.v2];
      ld->sidenum[0] = uld.sidefront;
      ld->sidenum[1] = uld.sideback;
      P_InitLineDef(ld);
      P_PostProcessLineFlags(ld);

      // more Eternity
      ld->alpha = uld.alpha;
      if(!uld.renderstyle.strCaseCmp("add"))
         ld->extflags |= EX_ML_ADDITIVE;
      if(uld.firstsideonly)
         ld->extflags |= EX_ML_1SONLY;
   }
   return true;
}

//
// Loads sidedefs (2)
//
bool UDMFParser::loadSidedefs2()
{
   for(int i = 0; i < numsides; ++i)
   {
      side_t *sd = sides + i;
      const USidedef &usd = mSidedefs[i];

      if(mNamespace == namespace_Eternity)
      {
         sd->textureoffset = usd.offsetx;
         sd->rowoffset = usd.offsety;
      }
      else
      {
         sd->textureoffset = usd.offsetx << FRACBITS;
         sd->rowoffset = usd.offsety << FRACBITS;
      }
      if(usd.sector < 0 || usd.sector >= numsectors)
      {
         mLine = usd.errorline;
         mColumn = 1;
         mError = "Sector overflow";
         return false;
      }
      sd->sector = &sectors[usd.sector];
      P_SetupSidedefTextures(*sd, usd.texturebottom.constPtr(),
                             usd.texturemiddle.constPtr(),
                             usd.texturetop.constPtr());
   }
   return true;
}

//
// Loads things
//
bool UDMFParser::loadThings()
{
   mapthing_t *mapthings;
   numthings = (int)mThings.getLength();
   mapthings = ecalloc(mapthing_t *, numthings, sizeof(mapthing_t));
   for(int i = 0; i < numthings; ++i)
   {
      mapthing_t *ft = &mapthings[i];
      const uthing_t &ut = mThings[i];
      ft->type = ut.type;
      // no Doom thing ban in UDMF
      ft->x = ut.x;
      ft->y = ut.y;
      ft->height = ut.height;
      ft->angle = ut.angle;
      if(ut.skill1 ^ ut.skill2)
         ft->extOptions |= MTF_EX_BABY_TOGGLE;
      if(ut.skill2)
         ft->options |= MTF_EASY;
      if(ut.skill3)
         ft->options |= MTF_NORMAL;
      if(ut.skill4)
         ft->options |= MTF_HARD;
      if(ut.skill4 ^ ut.skill5)
         ft->extOptions |= MTF_EX_NIGHTMARE_TOGGLE;
      if(ut.ambush)
         ft->options |= MTF_AMBUSH;
      if(!ut.single)
         ft->options |= MTF_NOTSINGLE;
      if(!ut.dm)
         ft->options |= MTF_NOTDM;
      if(!ut.coop)
         ft->options |= MTF_NOTCOOP;
      if(ut.friendly)
         ft->options |= MTF_FRIEND;
      if(ut.dormant)
         ft->options |= MTF_DORMANT;
      // TODO: class1, 2, 3
      // TODO: STRIFE
      ft->special = ut.special;
      ft->args[0] = ut.arg[0];
      ft->args[1] = ut.arg[1];
      ft->args[2] = ut.arg[2];
      ft->args[3] = ut.arg[3];
      ft->args[4] = ut.arg[4];

      P_ConvertDoomExtendedSpawnNum(ft);
      P_SpawnMapThing(ft);
   }

   // haleyjd: all player things for players in this game should now be valid
   if(GameType != gt_dm)
   {
      for(int i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i] && !players[i].mo)
         {
            mError = "Missing required player start";
            efree(mapthings);
            return false;
         }
      }
   }

   efree(mapthings);
   return true;
}

//==============================================================================
//
// TEXTMAP parsing
//
//==============================================================================

//
// Tries to parse a UDMF TEXTMAP document. If it fails, it returns false and
// you can check the error message with error()
//
bool UDMFParser::parse(WadDirectory &setupwad, int lump)
{
   {
      ZAutoBuffer buf;

      setupwad.cacheLumpAuto(lump, buf);
      auto data = buf.getAs<const char *>();

      // store it conveniently
      setData(data, setupwad.lumpLength(lump));
   }

   readresult_e result = readItem();
   if(result == result_Error)
      return false;
   if(result != result_Assignment || mKey.strCaseCmp("namespace") ||
      mValue.type != Token::type_String)
   {
      mError = "TEXTMAP must begin with a namespace assignment";
      return false;
   }

   // Set namespace (we'll think later about checking it
   if(!mValue.text.strCaseCmp("eternity"))
      mNamespace = namespace_Eternity;

   // Gamestuff. Must be null when out of block and only one be set when in
   // block
   ULinedef *linedef = nullptr;
   USidedef *sidedef = nullptr;
   uvertex_t *vertex = nullptr;
   USector *sector = nullptr;
   uthing_t *thing = nullptr;

   while((result = readItem()) != result_Eof)
   {
      if(result == result_Error)
         return false;
      if(result == result_BlockEntry)
      {
         // we're now in some block. Alloc stuff
         if(!mBlockName.strCaseCmp("linedef"))
         {
            linedef = &mLinedefs.addNew();
            linedef->errorline = mLine;
            linedef->renderstyle = "translucent";
         }
         else if(!mBlockName.strCaseCmp("sidedef"))
         {
            sidedef = &mSidedefs.addNew();
            sidedef->texturetop = "-";
            sidedef->texturebottom = "-";
            sidedef->texturemiddle = "-";
            sidedef->errorline = mLine;
         }
         else if(!mBlockName.strCaseCmp("vertex"))
            vertex = &mVertices.addNew();
         else if(!mBlockName.strCaseCmp("sector"))
            sector = &mSectors.addNew();
         else if(!mBlockName.strCaseCmp("thing"))
            thing = &mThings.addNew();
         continue;
      }
      if(result == result_Assignment && mInBlock)
      {
         if(linedef)
         {
            readInt("id", linedef->identifier);
            requireInt("v1", linedef->v1, linedef->v1set);
            requireInt("v2", linedef->v2, linedef->v2set);
            readBool("blocking", linedef->blocking);
            readBool("blockmonsters", linedef->blockmonsters);
            readBool("twosided", linedef->twosided);
            readBool("dontpegtop", linedef->dontpegtop);
            readBool("dontpegbottom", linedef->dontpegbottom);
            readBool("secret", linedef->secret);
            readBool("blocksound", linedef->blocksound);
            readBool("dontdraw", linedef->dontdraw);
            readBool("mapped", linedef->mapped);
            readBool("passuse", linedef->passuse);
            readBool("translucent", linedef->translucent);
            readBool("jumpover", linedef->jumpover);
            readBool("blockfloaters", linedef->blockfloaters);

            if(mNamespace == namespace_Eternity)
            {
               // TODO: these also apply to Hexen
               readBool("playercross", linedef->playercross);
               readBool("playeruse", linedef->playeruse);
               readBool("monstercross", linedef->monstercross);
               readBool("monsteruse", linedef->monsteruse);
               readBool("impact", linedef->impact);
               readBool("playerpush", linedef->playerpush);
               readBool("monsterpush", linedef->monsterpush);
               readBool("missilecross", linedef->missilecross);
               readBool("repeatspecial", linedef->repeatspecial);

               readBool("midtex3d", linedef->midtex3d);
               readBool("firstsideonly", linedef->firstsideonly);
               readFloat("alpha", linedef->alpha);
               readString("renderstyle", linedef->renderstyle);
            }
            readInt("special", linedef->special);
            readInt("arg0", linedef->arg[0]);
            readInt("arg1", linedef->arg[1]);
            readInt("arg2", linedef->arg[2]);
            readInt("arg3", linedef->arg[3]);
            readInt("arg4", linedef->arg[4]);
            requireInt("sidefront", linedef->sidefront, linedef->sfrontset);
            readInt("sideback", linedef->sideback);

            // New fields (Eternity)

         }
         else if(sidedef)
         {
            if(mNamespace == namespace_Eternity)
            {
               readFixed("offsetx", sidedef->offsetx);
               readFixed("offsety", sidedef->offsety);
            }
            else
            {
               readInt("offsetx", sidedef->offsetx);
               readInt("offsety", sidedef->offsety);
            }
            readString("texturetop", sidedef->texturetop);
            readString("texturebottom", sidedef->texturebottom);
            readString("texturemiddle", sidedef->texturemiddle);
            requireInt("sector", sidedef->sector, sidedef->sset);
         }
         else if(vertex)
         {
            requireFixed("x", vertex->x, vertex->xset);
            requireFixed("y", vertex->y, vertex->yset);
         }
         else if(sector)
         {
            if(mNamespace == namespace_Eternity)
            {
               readFixed("heightfloor", sector->heightfloor);
               readFixed("heightceiling", sector->heightceiling);
            }
            else
            {
               readInt("heightfloor", sector->heightfloor);
               readInt("heightceiling", sector->heightceiling);
            }
            requireString("texturefloor", sector->texturefloor,
                          sector->tfloorset);
            requireString("textureceiling", sector->textureceiling,
                          sector->tceilset);
            readInt("lightlevel", sector->lightlevel);
            readInt("special", sector->special);
            readInt("id", sector->identifier);
         }
         else if(thing)
         {
            readInt("id", thing->identifier);
            requireFixed("x", thing->x, thing->xset);
            requireFixed("y", thing->y, thing->yset);
            readFixed("height", thing->height);
            readInt("angle", thing->angle);
            requireInt("type", thing->type, thing->typeset);
            readBool("skill1", thing->skill1);
            readBool("skill2", thing->skill2);
            readBool("skill3", thing->skill3);
            readBool("skill4", thing->skill4);
            readBool("skill5", thing->skill5);
            readBool("ambush", thing->ambush);
            readBool("single", thing->single);
            readBool("dm", thing->dm);
            readBool("coop", thing->coop);
            readBool("friend", thing->friendly);
            readBool("dormant", thing->dormant);
            readBool("class1", thing->class1);
            readBool("class2", thing->class2);
            readBool("class3", thing->class3);
            readBool("standing", thing->standing);
            readBool("strifeally", thing->strifeally);
            readBool("translucent", thing->translucent);
            readBool("invisible", thing->invisible);
            readInt("special", thing->special);
            readInt("arg0", thing->arg[0]);
            readInt("arg1", thing->arg[1]);
            readInt("arg2", thing->arg[2]);
            readInt("arg3", thing->arg[3]);
            readInt("arg4", thing->arg[4]);
         }
         continue;
      }
      if(result == result_BlockExit)
      {
         if(linedef)
         {
            if(!linedef->v1set || !linedef->v2set || !linedef->sfrontset)
            {
               mError = "Incompletely defined linedef";
               return false;
            }
            linedef = nullptr;
         }
         else if(sidedef)
         {
            if(!sidedef->sset)
            {
               mError = "Incompletely defined sidedef";
               return false;
            }
            sidedef = nullptr;
         }
         else if(vertex)
         {
            if(!vertex->xset || !vertex->yset)
            {
               mError = "Incompletely defined vertex";
               return false;
            }
            vertex = nullptr;
         }
         else if(sector)
         {
            if(!sector->tfloorset || !sector->tceilset)
            {
               mError = "Incompletely defined sector";
               return false;
            }
            sector = nullptr;
         }
         else if(thing)
         {
            if(!thing->xset || !thing->yset || !thing->typeset)
            {
               mError = "Incompletely defined thing";
               return false;
            }
            thing = nullptr;
         }
      }
   }

   return true;
}

//
// Quick error message
//
qstring UDMFParser::error() const
{
   qstring message("TEXTMAP error at ");
   message << (int)mLine << ':' << (int)mColumn << " - " << mError;
   return message;
}

//
// Loads a new TEXTMAP and clears all variables
//
void UDMFParser::setData(const char *data, size_t size)
{
   mData.copy(data, size);
   mPos = 0;
   mLine = 1;
   mColumn = 1;
   mError.clear();

   mKey.clear();
   mValue.clear();
   mInBlock = false;
   mBlockName.clear();

   // Game stuff
   mNamespace = namespace_Doom;  // default to Doom
   mLinedefs.makeEmpty();
   mSidedefs.makeEmpty();
   mVertices.makeEmpty();
   mSectors.makeEmpty();
   mThings.makeEmpty();
}

//
// Passes a float
//
void UDMFParser::readFixed(const char *key, fixed_t &target) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_Number)
      target = M_DoubleToFixed(mValue.number);
}

//
// Passes a float to an object and flags a required element
//
void UDMFParser::requireFixed(const char *key, fixed_t &target,
                              bool &flagtarget) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_Number)
   {
      target = M_DoubleToFixed(mValue.number);
      flagtarget = true;
   }
}

//
// Passes an int
//
void UDMFParser::readInt(const char *key, int &target) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_Number)
      target = static_cast<int>(mValue.number);
}

//
// Requires an int
//
void UDMFParser::requireInt(const char *key, int &target,
                              bool &flagtarget) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_Number)
   {
      target = static_cast<int>(mValue.number);
      flagtarget = true;
   }
}

//
// Reads a string
//
void UDMFParser::readString(const char *key, qstring &target) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_String)
      target = mValue.text;
}

//
// Passes a string
//
void UDMFParser::requireString(const char *key, qstring &target,
                               bool &flagtarget) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_String)
   {
      target = mValue.text;
      flagtarget = true;
   }
}

//
// Passes a boolean
//
void UDMFParser::readBool(const char *key, bool &target) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_Keyword)
   {
      target = ectype::toUpper(mValue.text[0]) == 'T';
   }
}

//
// Passes a float
//
void UDMFParser::readFloat(const char *key, float &target) const
{
   if(!mKey.strCaseCmp(key) && mValue.type == Token::type_Number)
   {
      target = static_cast<float>(mValue.number);
   }
}

//
// Reads a line or block item. Returns false on error
//
UDMFParser::readresult_e UDMFParser::readItem()
{
   Token token;
   if(!next(token))
      return result_Eof;

   if(token.type == Token::type_Symbol && token.symbol == '}')
   {
      if(mInBlock)
      {
         mInBlock = false;
         mKey = mBlockName; // preserve the name into "key"
         mBlockName.clear();
         return result_BlockExit;
      }
      // not in block: error
      mError = "Unexpected '}'";
      return result_Error;
   }

   if(token.type != Token::type_Keyword)
   {
      mError = "Expected a keyword";
      return result_Error;
   }
   mKey = token.text;

   if(!next(token) || token.type != Token::type_Symbol ||
      (token.symbol != '=' && token.symbol != '{'))
   {
      mError = "Expected '=' or '{'";
      return result_Error;
   }

   if(token.symbol == '=')
   {
      // assignment
      if(!next(token) || (token.type != Token::type_Keyword &&
         token.type != Token::type_String && token.type != Token::type_Number))
      {
         mError = "Expected a number, string or true/false";
         return result_Error;
      }

      if(token.type == Token::type_Keyword && token.text.strCaseCmp("true") &&
         token.text.strCaseCmp("false"))
      {
         mError = "Identifier can only be true or false";
         return result_Error;
      }
      mValue = token;

      if(!next(token) || token.type != Token::type_Symbol ||
         token.symbol != ';')
      {
         mError = "Expected ; after assignment";
         return result_Error;
      }

      return result_Assignment;
   }
   else  // {
   {
      // block
      if(!mInBlock)
      {
         mInBlock = true;
         mBlockName = mKey;
         return result_BlockEntry;
      }
      else
      {
         mError = "Blocks cannot be nested";
         return result_Error;
      }
   }
}

//
// Gets the next token from mData. Returns false if EOF. It will not return
// false if there's something to return
//
bool UDMFParser::next(Token &token)
{
   // Skip all leading whitespace
   bool checkWhite = true;
   size_t size = mData.length();

   while(checkWhite)
   {
      checkWhite = false;  // reset it unless someone else restores it

      while(mPos != size && ectype::isSpace(mData[mPos]))
         addPos(1);
      if(mPos == size)
         return false;

      // Skip comments
      if(mData[mPos] == '/' && mPos + 1 < size && mData[mPos + 1] == '/')
      {
         addPos(2);
         // one line comment
         while(mPos != size && mData[mPos] != '\n')
            addPos(1);
         if(mPos == size)
            return false;

         // If here, we hit an "enter"
         addPos(1);
         if(mPos == size)
            return false;

         checkWhite = true;   // look again for whitespaces if reached here
      }

      if(mData[mPos] == '/' && mPos + 1 < size && mData[mPos + 1] == '*')
      {
         addPos(2);
         while(mPos + 1 < size && mData[mPos] != '*' && mData[mPos + 1] != '/')
            addPos(1);
         if(mPos + 1 >= size)
         {
            addPos(1);
            return false;
         }
         if(mData[mPos] == '*' && mData[mPos] == '/')
            addPos(2);
         if(mPos == size)
            return false;
         checkWhite = true;
      }
   }

   // now we're clear from whitespaces and comments

   // Check for number
   char *result = nullptr;
   double number = strtod(&mData[mPos], &result);
   if(result > &mData[mPos])  // we have something
   {
      // copy it
      token.type = Token::type_Number;
      token.number = number;
      addPos(result - mData.constPtr() - mPos);
      return true;
   }

   // Check for string
   if(mData[mPos] == '"')
   {
      addPos(1);

      // we entered a string
      // find the next string
      token.type = Token::type_String;

      // we must escape things here
      token.text.clear();
      bool escape = false;
      while(mPos != size)
      {
         if(!escape)
         {
            if(mData[mPos] == '\\')
               escape = true;
            else if(mData[mPos] == '"')
            {
               addPos(1);     // skip the quote
               return true;   // we're done
            }
            else
               token.text.Putc(mData[mPos]);
         }
         else
         {
            token.text.Putc(mData[mPos]);
            escape = false;
         }
         addPos(1);
      }
      return true;
   }

   // keyword: start with a letter or _
   if(ectype::isAlpha(mData[mPos]) || mData[mPos] == '_')
   {
      token.type = Token::type_Keyword;
      token.text.clear();
      while(mPos != size &&
            (ectype::isAlnum(mData[mPos]) || mData[mPos] == '_'))
      {
         token.text.Putc(mData[mPos]);
         addPos(1);
      }
      return true;
   }

   // symbol. Just put one character
   token.type = Token::type_Symbol;
   token.symbol = mData[mPos];
   addPos(1);

   return true;
}

//
// Increases position by given amount. Updates line and column accordingly.
//
void UDMFParser::addPos(int amount)
{
   for(int i = 0; i < amount; ++i)
   {
      if(mPos == mData.length())
         return;
      if(mData[mPos] == '\n')
      {
         mColumn = 1;
         mLine++;
      }
      else
         mColumn++;
      mPos++;
   }
}

// EOF

