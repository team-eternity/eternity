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

#ifndef E_ARGS_H_
#define E_ARGS_H_

#include "m_collection.h"
#include "m_fixed.h"
#include "m_qstr.h"

class WadDirectory;

//
// This contains the post-editing lump addresses, needed because they're not in
// fixed position in UDMF.
//
struct maplumpindex_t
{
   int segs;
   int ssectors;
   int nodes;
   int reject;
   int blockmap;
   int behavior;
};

//==============================================================================

class UDMFParser : public ZoneObject
{
public:
   
   UDMFParser()
   {
      static ULinedef linedef;
      mLinedefs.setPrototype(&linedef);
      static USector sector;
      mSectors.setPrototype(&sector);
      static USidedef sidedef;
      mSidedefs.setPrototype(&sidedef);
   }

   void loadVertices() const;
   void loadSectors() const;
   void loadSidedefs() const;
   bool loadLinedefs();
   bool loadSidedefs2();
   bool loadThings();

   bool parse(WadDirectory &setupwad, int lump);

   qstring error() const;
   int line() const { return mLine; }
   int column() const { return mColumn; }

private:

   class Token
   {
   public:
      enum type_e
      {
         type_Keyword,
         type_Number,
         type_String,
         type_Symbol
      };

      type_e type;
      double number;
      qstring text;
      char symbol;

      Token()
      {
         clear();
      }

      void clear()
      {
         type = type_Keyword;
         number = 0;
         text.clear();
         symbol = 0;
      }
   };

   enum readresult_e
   {
      result_Assignment,
      result_BlockEntry,
      result_BlockExit,
      result_Eof,
      result_Error
   };

   enum namespace_e
   {
      namespace_Doom,
      namespace_Eternity,
   };

   // NOTE: some of these are classes because they contain non-POD objects (e.g.
   // qstring

   class ULinedef : public ZoneObject
   {
   public:
      int identifier = -1;
      int v1, v2;

      bool blocking, blockmonsters, twosided, dontpegtop, dontpegbottom, secret,
      blocksound, dontdraw, mapped;

      bool passuse;
      bool translucent, jumpover, blockfloaters;

      bool playercross, playeruse, monstercross, monsteruse, impact, playerpush,
      monsterpush, missilecross, repeatspecial;

      int special, arg[5];
      int sidefront, sideback = -1;

      bool v1set, v2set, sfrontset;
      int errorline;

      // Eternity
      bool midtex3d, firstsideonly, blockeverything, zoneboundary, clipmidtex;
      float alpha = 1;
      qstring renderstyle;
      qstring tranmap;
   };

   class USidedef
   {
   public:
      int offsetx = 0;
      int offsety = 0;

      qstring texturetop;
      qstring texturebottom;
      qstring texturemiddle;

      int sector = 0;

      bool sset = false;

      int errorline = 0;
   };

   struct uvertex_t
   {
      fixed_t x, y;
      bool xset, yset;
   };

   class USector
   {
   public:
      int heightfloor = 0;
      int heightceiling = 0;
      fixed_t xpanningfloor = 0;
      fixed_t ypanningfloor = 0;
      fixed_t xpanningceiling = 0;
      fixed_t ypanningceiling = 0;
      double rotationfloor = 0.0;
      double rotationceiling = 0.0;

      // TODO: Implement functionality for sliding scale of leakiness from 0-256
      byte leakiness = 0;
      int damageamount = 0;
      int damageinterval = 0;
      bool damage_endgodmode = false;
      bool damage_exitlevel = false;
      bool damage_terrainhit = false;
      qstring floorterrain = qstring("@flat");
      qstring ceilingterrain = qstring("@flat");

      qstring texturefloor;
      qstring textureceiling;
      int lightlevel = 160;
      int special = 0;
      int identifier = 0;

      bool tfloorset = false, tceilset = false;
   };

   struct uthing_t
   {
      int identifier;
      fixed_t x, y;
      fixed_t height;
      int angle;
      int type;
      bool skill1, skill2, skill3, skill4, skill5, ambush, single, dm, coop;
      bool friendly;
      bool dormant, class1, class2, class3;
      bool standing, strifeally, translucent, invisible;
      int special, arg[5];

      bool xset, yset, typeset;
   };

   void setData(const char *data, size_t size);

   void readFixed(const char *key, fixed_t &target) const;
   void requireFixed(const char *key, fixed_t &target, bool &flagtarget) const;
   void requireInt(const char *key, int &target, bool &flagtarget) const;
   void readString(const char *key, qstring &target) const;
   void requireString(const char *key, qstring &target, bool &flagtarget) const;
   void readBool(const char *key, bool &target) const;
   template<typename T>
   void readNumber(const char *key, T &target) const;

   readresult_e readItem();

   bool next(Token &token);
   void addPos(int amount);

   bool eof() const { return mPos == mData.length(); }

   qstring mData;
   size_t mPos = 0;
   int mLine = 1; // for locating errors. 1-based
   int mColumn = 1;
   qstring mError;

   qstring mKey;
   Token mValue;
   bool mInBlock = false;
   qstring mBlockName;

   // Game stuff
   namespace_e mNamespace;
   Collection<ULinedef> mLinedefs;
   Collection<USidedef> mSidedefs;
   PODCollection<uvertex_t> mVertices;
   Collection<USector> mSectors;
   PODCollection<uthing_t> mThings;
};

#endif

// EOF

