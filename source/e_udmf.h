//
// The Eternity Engine
// Copyright (C) 2025 Ioan Chera et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Universal Doom Map Format, for Eternity.
// Authors: Ioan Chera, Max Waine
//

#ifndef E_UDMF_H_
#define E_UDMF_H_

#include <optional>
#include "m_collection.h"
#include "m_fixed.h"
#include "m_qstr.h"

struct keytoken_t;
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
    UDMF_SECTOR_INIT_COLOR_TOP    = 1,
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
    struct planeeq_t
    {
        double a, b, c, d; // plane coefficients: ax + by + cz + d = 0
    };

    struct sectorinfo_t
    {
        unsigned     flags;
        int          portalceiling;
        int          portalfloor;
        udmfattach_t attach;

        Surfaces<planeeq_t> plane;
    };

    struct lineinfo_t
    {
        int portal;
    };

    struct vertexinfo_t
    {
        std::optional<double> zfloor;
        std::optional<double> zceiling;
    };

    sectorinfo_t *mSectorInitData = nullptr;
    lineinfo_t   *mLineInitData   = nullptr;
    vertexinfo_t *mVertexInitData = nullptr;

    void useSectorCount();
    void useLineCount();
    void useVertexCount();

public:
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
        mSectorInitData[index].portalfloor   = portalfloor;
    }
    void getSectorPortals(int index, int &portalceiling, int &portalfloor) const
    {
        if(mSectorInitData)
        {
            portalceiling = mSectorInitData[index].portalceiling;
            portalfloor   = mSectorInitData[index].portalfloor;
            return;
        }
        // no data
        portalceiling = 0;
        portalfloor   = 0;
    }
    void setSectorPlane(int index, surf_e surf, double a, double b, double c, double d)
    {
        useSectorCount();
        mSectorInitData[index].plane[surf].a = a;
        mSectorInitData[index].plane[surf].b = b;
        mSectorInitData[index].plane[surf].c = c;
        mSectorInitData[index].plane[surf].d = d;
    }
    void getSectorPlane(int index, surf_e surf, double &a, double &b, double &c, double &d) const
    {
        if(!mSectorInitData)
        {
            a = 0;
            b = 0;
            c = 0;
            d = 0;
            return;
        }
        a = mSectorInitData[index].plane[surf].a;
        b = mSectorInitData[index].plane[surf].b;
        c = mSectorInitData[index].plane[surf].c;
        d = mSectorInitData[index].plane[surf].d;
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

    void setVertexSlopeZ(int index, surf_e surf, fixed_t value)
    {
        useVertexCount();
        if(surf == surf_floor)
            mVertexInitData[index].zfloor = value;
        else
            mVertexInitData[index].zceiling = value;
    }

    const double *getVertexSlopeZ(int index, surf_e surf) const
    {
        if(!mVertexInitData)
            return nullptr;
        std::optional<double> vertexinfo_t::*field =
            surf == surf_floor ? &vertexinfo_t::zfloor : &vertexinfo_t::zceiling;
        return (mVertexInitData[index].*field).has_value() ? &(mVertexInitData[index].*field).value() : nullptr;
    }

    bool hasVertexData() const { return !!mVertexInitData; }
    bool hasSectorData() const { return !!mSectorInitData; }
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

        type_e  type;
        double  number;
        qstring text;
        char    symbol;

        Token() { clear(); }

        void clear()
        {
            type   = type_Keyword;
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
        int identifier; // tag
        int v1, v2;     // vertices

        // Classic
        bool blocking, blockmonsters, twosided, dontpegtop, dontpegbottom, secret, blocksound, dontdraw, mapped;

        // Inherited from games
        bool passuse;                              // Boom
        bool translucent, jumpover, blockfloaters; // Strife

        // Activation specials
        bool playercross, playeruse, monstercross, monsteruse, impact, monstershoot, playerpush, monsterpush,
            missilecross, repeatspecial, polycross;

        int special, arg[5];     // linedef special and args
        int sidefront, sideback; // sidedef references

        // auxiliary fields
        bool v1set, v2set, sfrontset; // (mandatory field internal flags)
        int  errorline;               // parsing error (not a property)

        // Eternity
        bool    midtex3d;           // 3dmidtex
        bool    firstsideonly;      // special only activateable from front side
        bool    blockeverything;    // zdoomish blocks-everything
        bool    zoneboundary;       // zdoomish audio reverb boundary
        bool    clipmidtex;         // zdoomish cut rendering of middle texture
        bool    wrapmidtex;         // zdoomish wrapping of 2s midtex up/down
        bool    midtex3dimpassible; // zdoomish trick to make projectiles pass
        bool    lowerportal;        // lower part acts as a portal extension
        bool    upperportal;        // upper part acts as a portal extension
        int     portal;
        float   alpha;       // opacity ratio
        qstring renderstyle; // zdoomish renderstyle (add, translucent)
        qstring tranmap;     // boomish translucency lump

        // MBF21
        bool blocklandmonsters; // ML_BLOCKLANDMONSTERS
        bool blockplayers;      // ML_BLOCKPLAYERS

        ULinedef() : identifier(-1), sideback(-1), alpha(1) {}
    };

    class USidedef
    {
    public:
        fixed_t offsetx, offsety;
        fixed_t offsetx_top, offsety_top, offsetx_mid, offsety_mid, offsetx_bottom, offsety_bottom;

        fixed_t scalex_top, scaley_top, scalex_mid, scaley_mid, scalex_bottom, scaley_bottom;

        qstring texturetop;
        qstring texturebottom;
        qstring texturemiddle;

        qstring skew_bottom_type, skew_middle_type, skew_top_type;

        int  light, light_top, light_mid, light_bottom;
        bool lightabsolute, lightabsolute_top, lightabsolute_mid, lightabsolute_bottom;

        int sector;

        bool sset;

        int errorline;

        USidedef()
            : offsetx(0), offsety(0), offsetx_top(0), offsety_top(0), offsetx_mid(0), offsety_mid(0), offsetx_bottom(0),
              offsety_bottom(0), scalex_top(FRACUNIT), scaley_top(FRACUNIT), scalex_mid(FRACUNIT), scaley_mid(FRACUNIT),
              scalex_bottom(FRACUNIT), scaley_bottom(FRACUNIT), sector(0), sset(false), errorline(0)
        {}
    };

    struct uvertex_t
    {
        fixed_t x, y;
        bool    xset, yset;
        double  zfloor, zceiling;
        bool    zfloorset, zceilingset;
    };

    class USector : public ZoneObject
    {
    public:
        int     heightfloor     = 0;
        int     heightceiling   = 0;
        fixed_t xpanningfloor   = 0;
        fixed_t ypanningfloor   = 0;
        fixed_t xpanningceiling = 0;
        fixed_t ypanningceiling = 0;
        double  xscalefloor     = 1.0;
        double  yscalefloor     = 1.0;
        double  xscaleceiling   = 1.0;
        double  yscaleceiling   = 1.0;
        double  rotationfloor   = 0.0;
        double  rotationceiling = 0.0;

        // Brand new UDMF scroller properties
        double  scroll_ceil_x    = 0.0;
        double  scroll_ceil_y    = 0.0;
        qstring scroll_ceil_type = qstring("none");

        double  scroll_floor_x    = 0.0;
        double  scroll_floor_y    = 0.0;
        qstring scroll_floor_type = qstring("none");

        bool secret   = false;
        int  friction = -1;

        int     leakiness           = 0;
        int     damageamount        = 0;
        int     damageinterval      = 32;
        bool    damage_endgodmode   = false;
        bool    damage_exitlevel    = false;
        bool    damageterraineffect = false;
        qstring damagetype          = qstring("Unknown");
        qstring floorterrain        = qstring("@flat");
        qstring ceilingterrain      = qstring("@flat");

        int  lightfloor           = 0;
        int  lightceiling         = 0;
        bool lightfloorabsolute   = false;
        bool lightceilingabsolute = false;
        bool phasedlight          = false;
        bool lightsequence        = false;
        bool lightseqalt          = false;

        qstring colormaptop    = qstring("@default");
        qstring colormapmid    = qstring("@default");
        qstring colormapbottom = qstring("@default");

        qstring texturefloor;
        qstring textureceiling;
        int     lightlevel = 160;
        int     special    = 0;
        int     identifier = 0;

        int floorid = 0, ceilingid = 0;
        int attachfloor = 0, attachceiling = 0;

        qstring soundsequence;

        bool tfloorset = false, tceilset = false;

        // ED's portalflags.ceiling, and overlayalpha.ceiling
        bool portal_ceil_disabled     = false;
        bool portal_ceil_norender     = false;
        bool portal_ceil_nopass       = false;
        bool portal_ceil_blocksound   = false;
        bool portal_ceil_useglobaltex = false;
        // OVERLAY and ADDITIVE consolidated into a single property
        qstring portal_ceil_overlaytype = qstring("none");
        bool    portal_ceil_attached    = false;
        double  alphaceiling            = 1.0;

        // ED's portalflags.floor, and overlayalpha.floor
        bool portal_floor_disabled     = false;
        bool portal_floor_norender     = false;
        bool portal_floor_nopass       = false;
        bool portal_floor_blocksound   = false;
        bool portal_floor_useglobaltex = false;
        // OVERLAY and ADDITIVE consolidated into a single property
        qstring portal_floor_overlaytype = qstring("none");
        bool    portal_floor_attached    = false;
        double  alphafloor               = 1.0;

        int portalceiling = 0; // floor portal id
        int portalfloor   = 0; // floor portal id

        double ceilingplane_a = 0.0;
        double ceilingplane_b = 0.0;
        double ceilingplane_c = 0.0;
        double ceilingplane_d = 0.0;
        double floorplane_a   = 0.0;
        double floorplane_b   = 0.0;
        double floorplane_c   = 0.0;
        double floorplane_d   = 0.0;
    };

    struct uthing_t
    {
        int     identifier;
        fixed_t x, y;
        fixed_t height;
        int     angle;
        int     type;
        bool    skill1, skill2, skill3, skill4, skill5, ambush, single, dm, coop;
        bool    friendly;
        bool    dormant, class1, class2, class3;
        bool    standing, strifeally, translucent, invisible;
        int     special, arg[5];

        // new stuff
        double health;

        bool xset, yset, typeset;
    };

    void readLinedefToken(ULinedef *linedef, const keytoken_t *kt) const;
    void readSidedefToken(USidedef *sidedef, const keytoken_t *kt) const;
    void readVertexToken(uvertex_t *vertex, const keytoken_t *kt) const;
    void readSectorToken(USector *sector, const keytoken_t *kt) const;
    void readThingToken(uthing_t *thing, const keytoken_t *kt) const;

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
    size_t  mPos;
    int     mLine; // for locating errors. 1-based
    int     mColumn;
    qstring mError;

    qstring mKey;
    Token   mValue;
    bool    mInBlock;
    qstring mBlockName;

    // Game stuff
    namespace_e              mNamespace;
    Collection<ULinedef>     mLinedefs;
    Collection<USidedef>     mSidedefs;
    PODCollection<uvertex_t> mVertices;
    Collection<USector>      mSectors;
    PODCollection<uthing_t>  mThings;
};

#endif

// EOF

