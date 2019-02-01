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
   UDMF_SECTOR_INIT_COLOR_TOP = 1,
   UDMF_SECTOR_INIT_COLOR_MIDDLE = 2,
   UDMF_SECTOR_INIT_COLOR_BOTTOM = 4,
};

//
// Data for UDMF attaching
//
struct udmfattach_t
{
   int floorid;
   int ceilingid;
   int attachfloor;
   int attachceiling;
};

//
// This holds settings needed during P_SetupLevel and cleared afterwards. Useful
// to avoid populating map item data with unused fields.
//
class UDMFSetupSettings : public ZoneObject
{
   struct sectorinfo_t
   {
      unsigned flags;
      int portalceiling;
      int portalfloor;
      udmfattach_t attach;
   };

   struct lineinfo_t
   {
      int portal;
   };

   struct vertexinfo_t
   {
      fixed_t zfloor, zceiling;
   };

   sectorinfo_t *mSectorInitData;
   lineinfo_t *mLineInitData;
   vertexinfo_t *mVertexInitData;

   void useSectorCount();
   void useLineCount();
   void useVertexCount();
public:
   bool hasSectorData() const { return !!mSectorInitData; }
   bool hasLineData() const { return !!mLineInitData; }
   bool hasVertexData() const { return !!mVertexInitData; }

   UDMFSetupSettings() : mSectorInitData(nullptr), mLineInitData(nullptr), mVertexInitData(nullptr)
   {
   }
   ~UDMFSetupSettings()
   {
      efree(mSectorInitData);
      efree(mLineInitData);
      efree(mVertexInitData);
   }

   //
   // Sector init flag getter and setter
   //
   void setSectorFlag(int index, unsigned flag)
   {
      useSectorCount();
      mSectorInitData[index].flags |= flag;
   }
   bool sectorIsFlagged(int index, unsigned flag) const
   {
      return mSectorInitData && !!(mSectorInitData[index].flags & flag);
   }
   void setSectorPortals(int index, int portalceiling, int portalfloor)
   {
      useSectorCount();
      mSectorInitData[index].portalceiling = portalceiling;
      mSectorInitData[index].portalfloor = portalfloor;
   }
   void getSectorPortals(int index, int &portalceiling, int &portalfloor) const
   {
      if(mSectorInitData)
      {
         portalceiling = mSectorInitData[index].portalceiling;
         portalfloor = mSectorInitData[index].portalfloor;
         return;
      }
      // no data
      portalceiling = 0;
      portalfloor = 0;
   }

   void setLinePortal(int index, int portal)
   {
      useLineCount();
      mLineInitData[index].portal = portal;
   }
   int getLinePortal(int index) const
   {
      if(mLineInitData)
         return mLineInitData[index].portal;
      return 0;
   }

   //
   // Attaching. This merely gets a reference you can modify
   //
   udmfattach_t &getAttachInfo(int index)
   {
      useSectorCount();
      return mSectorInitData[index].attach;
   }

   //
   // Vertex stuff
   //
   void setVertexSlope(int index, fixed_t zfloor, fixed_t zceiling)
   {
      useVertexCount();
      mVertexInitData[index].zfloor = zfloor;
      mVertexInitData[index].zceiling = zceiling;
   }
   void getVertexSlope(int index, fixed_t &zfloor, fixed_t &zceiling) const
   {
      if(mVertexInitData)
      {
         zfloor = mVertexInitData[index].zfloor;
         zceiling = mVertexInitData[index].zceiling;
         return;
      }
      zfloor = 0;
      zceiling = 0;
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

   void loadVertices(UDMFSetupSettings &setupSettings) const;
   void loadSectors(UDMFSetupSettings &setupSettings) const;
   void loadSidedefs() const;
   bool loadLinedefs(UDMFSetupSettings &setupSettings);
   bool loadSidedefs2();
   bool loadThings();

   bool checkForCompatibilityFlag(qstring nstext);
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
      int identifier;   // tag
      int v1, v2;       // vertices

      // Classic
      bool blocking, blockmonsters, twosided, dontpegtop, dontpegbottom, secret,
      blocksound, dontdraw, mapped;

      // Inherited from games
      bool passuse;                                // Boom
      bool translucent, jumpover, blockfloaters;   // Strife

      // Activation specials
      bool playercross, playeruse, monstercross, monsteruse, impact, playerpush,
      monsterpush, missilecross, repeatspecial, polycross;

      int special, arg[5];       // linedef special and args
      int sidefront, sideback;   // sidedef references

      // auxiliary fields
      bool v1set, v2set, sfrontset; // (mandatory field internal flags)
      int errorline;                // parsing error (not a property)

      // Eternity
      bool midtex3d;             // 3dmidtex
      bool firstsideonly;        // special only activateable from front side
      bool blockeverything;      // zdoomish blocks-everything
      bool zoneboundary;         // zdoomish audio reverb boundary
      bool clipmidtex;           // zdoomish cut rendering of middle texture
      bool midtex3dimpassible;   // zdoomish trick to make projectiles pass
      bool lowerportal;          // lower part acts as a portal extension
      bool upperportal;          // upper part acts as a portal extension
      int portal;
      float alpha;               // opacity ratio
      qstring renderstyle;       // zdoomish renderstyle (add, translucent)
      qstring tranmap;           // boomish translucency lump
      

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

      // Eternity
      fixed_t zfloor;
      fixed_t zceiling;
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
      double xscalefloor;
      double yscalefloor;
      double xscaleceiling;
      double yscaleceiling;
      double rotationfloor;
      double rotationceiling;

      // Brand new UDMF scroller properties
      double scroll_ceil_x;
      double scroll_ceil_y;
      qstring scroll_ceil_type;

      double scroll_floor_x;
      double scroll_floor_y;
      qstring scroll_floor_type;

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

      int floorid, ceilingid;
      int attachfloor, attachceiling;

      qstring soundsequence;

      bool tfloorset, tceilset;

      // ED's portalflags.ceiling, and overlayalpha.ceiling
      bool         portal_ceil_disabled;
      bool         portal_ceil_norender;
      bool         portal_ceil_nopass;
      bool         portal_ceil_blocksound;
      bool         portal_ceil_useglobaltex;
      qstring      portal_ceil_overlaytype; // OVERLAY and ADDITIVE consolidated into a single property
      bool         portal_ceil_attached;
      double       alphaceiling;

      // ED's portalflags.floor, and overlayalpha.floor
      bool         portal_floor_disabled;
      bool         portal_floor_norender;
      bool         portal_floor_nopass;
      bool         portal_floor_blocksound;
      bool         portal_floor_useglobaltex;
      qstring      portal_floor_overlaytype; // OVERLAY and ADDITIVE consolidated into a single property
      bool         portal_floor_attached;
      double       alphafloor;

      int          portalceiling;   // floor portal id
      int          portalfloor;     // floor portal id

      USector() : xscalefloor(1.0), yscalefloor(1.0), xscaleceiling(1.0), yscaleceiling(1.0),
         scroll_ceil_type("none"), scroll_floor_type("none"), friction(-1), damageinterval(32),
         damagetype("Unknown"), floorterrain("@flat"), ceilingterrain("@flat"),
         colormaptop("@default"), colormapmid("@default"), colormapbottom("@default"),
         lightlevel(160),
         portal_ceil_overlaytype("none"), alphaceiling(1.0),
         portal_floor_overlaytype("none"), alphafloor(1.0)
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

      // new stuff
      double health;

      bool xset, yset, typeset;
   };

   void setData(const char *data, size_t size);
   void reset();

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
   void addPos(size_t amount);

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

