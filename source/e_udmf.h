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

//
// Temporary sector init flags checked only during P_SetupLevel and cleared
// afterwards.
//

enum
{
   // colormap has been set by UDMF or ExtraData
   UDMF_SECTOR_INIT_COLORMAPPED = 1,
};

//
// This holds settings needed during P_SetupLevel and cleared afterwards. Useful
// to avoid populating map item data with unused fields.
//
class UDMFSetupSettings : public ZoneObject
{
   unsigned *mSectorInitFlags;
   void useSectorCount();
public:
   UDMFSetupSettings() : mSectorInitFlags(nullptr)
   {
   }
   ~UDMFSetupSettings()
   {
      efree(mSectorInitFlags);
   }

   //
   // Sector init flag getter and setter
   //
   void setSectorFlag(int index, unsigned flag)
   {
      useSectorCount();
      mSectorInitFlags[index] |= flag;
   }
   bool sectorIsFlagged(int index, unsigned flag) const
   {
      return mSectorInitFlags && !!(mSectorInitFlags[index] & flag);
   }
};

//==============================================================================

class UDMFParser : public ZoneObject
{
public:
   
   UDMFParser() : mLine(1), mColumn(1)
   {
      static ULinedef linedef;
      mLinedefs.setPrototype(&linedef);
      static USector sector;
      mSectors.setPrototype(&sector);
      static USidedef sidedef;
      mSidedefs.setPrototype(&sidedef);
   }

   enum namespace_e
   {
      namespace_Doom,
      namespace_Heretic,
      namespace_Hexen,
      namespace_Strife,
      namespace_Eternity
   };

   void loadVertices() const;
   void loadSectors(UDMFSetupSettings &setupSettings) const;
   void loadSidedefs() const;
   bool loadLinedefs();
   bool loadSidedefs2();
   bool loadThings();

   bool parse(WadDirectory &setupwad, int lump);

   qstring error() const;

   int getMapFormat() const;

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

   // NOTE: some of these are classes because they contain non-POD objects (e.g.
   // qstring

   class ULinedef : public ZoneObject
   {
   public:
      int identifier;
      int v1, v2;

      bool blocking, blockmonsters, twosided, dontpegtop, dontpegbottom, secret,
      blocksound, dontdraw, mapped;

      bool passuse;
      bool translucent, jumpover, blockfloaters;

      bool playercross, playeruse, monstercross, monsteruse, impact, playerpush,
      monsterpush, missilecross, repeatspecial;

      int special, arg[5];
      int sidefront, sideback;

      bool v1set, v2set, sfrontset;
      int errorline;

      // Eternity
      bool midtex3d, firstsideonly, blockeverything, zoneboundary, clipmidtex,
      midtex3dimpassible, lowerportal, upperportal;
      float alpha;
      qstring renderstyle;
      qstring tranmap;

      ULinedef() : identifier(-1), sideback(-1), alpha(1)
      {
      }
   };

   class USidedef
   {
   public:
      int offsetx;
      int offsety;

      qstring texturetop;
      qstring texturebottom;
      qstring texturemiddle;

      int sector;

      bool sset;

      int errorline;

      USidedef() : offsetx(0), offsety(0), sector(0), sset(false), errorline(0)
      {
      }
   };

   struct uvertex_t
   {
      fixed_t x, y;
      bool xset, yset;
   };

   class USector : public ZoneObject
   {
   public:
      int heightfloor;
      int heightceiling;
      fixed_t xpanningfloor;
      fixed_t ypanningfloor;
      fixed_t xpanningceiling;
      fixed_t ypanningceiling;
      double rotationfloor;
      double rotationceiling;

      bool secret;
      int friction;

      int leakiness;
      int damageamount;
      int damageinterval;
      bool damage_endgodmode;
      bool damage_exitlevel;
      bool damageterraineffect;
      qstring damagetype;
      qstring floorterrain;
      qstring ceilingterrain;

      int lightfloor;
      int lightceiling;
      bool lightfloorabsolute;
      bool lightceilingabsolute;

      qstring colormaptop;
      qstring colormapmid;
      qstring colormapbottom;

      qstring texturefloor;
      qstring textureceiling;
      int lightlevel;
      int special;
      int identifier;

      bool tfloorset, tceilset;

      // ED's portalflags.ceiling, and overlayalpha.ceiling
      bool         portal_ceil_disabled;
      bool         portal_ceil_norender;
      bool         portal_ceil_nopass;
      bool         portal_ceil_blocksound;
      bool         portal_ceil_useglobaltex;
      qstring      portal_ceil_overlaytype; // OVERLAY and ADDITIVE consolidated into a single property
      unsigned int portal_ceil_alpha;

      // ED's portalflags.floor, and overlayalpha.floor
      bool         portal_floor_disabled;
      bool         portal_floor_norender;
      bool         portal_floor_nopass;
      bool         portal_floor_blocksound;
      bool         portal_floor_useglobaltex;
      qstring      portal_floor_overlaytype; // OVERLAY and ADDITIVE consolidated into a single property
      unsigned int portal_floor_alpha;

      USector() : friction(-1), damagetype("Unknown"), floorterrain("@flat"), ceilingterrain("@flat"),
         colormaptop("@default"), colormapmid("@default"), colormapbottom("@default"),
         portal_ceil_overlaytype("none"), portal_ceil_alpha(255),
         portal_floor_overlaytype("none"), portal_floor_alpha(255),
         lightlevel(160)
      {
      }
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

   void readFixed(fixed_t &target) const;
   void requireFixed(fixed_t &target, bool &flagtarget) const;
   void requireInt(int &target, bool &flagtarget) const;
   void readString(qstring &target) const;
   void requireString(qstring &target, bool &flagtarget) const;
   void readBool(bool &target) const;
   template<typename T>
   void readNumber(T &target) const;

   readresult_e readItem();

   bool next(Token &token);
   void addPos(int amount);

   bool eof() const { return mPos == mData.length(); }

   qstring mData;
   size_t mPos;
   int mLine; // for locating errors. 1-based
   int mColumn;
   qstring mError;

   qstring mKey;
   Token mValue;
   bool mInBlock;
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

