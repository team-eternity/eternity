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
#include "e_hash.h"
#include "e_udmf.h"
#include "m_collection.h"
#include "m_ctype.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_auto.h"

// FIXME: why is this not in a header?
extern const char *level_error;

// UDMF namespace, one of a few choices
UDMFNamespace gUDMFNamespace;

// A convenient buffer to point level_error to
static char gLevelErrorBuffer[256];

// Supported namespace strings
static const char kDoomNamespace[] = "doom";
static const char kHereticNamespace[] = "heretic";
static const char kHexenNamespace[] = "hexen";
static const char kStrifeNamespace[] = "doom";
static const char kEternityNamespace[] = "eternity";  // TENTATIVE

//
// Unprocessed UDMF structures
//

struct UDMFBinding
{
   const char *name;    // name

   UDMFTokenType type;  // type: number, string or identifier (true/false)
   size_t offset;       // offsetof struct

   // Extra paramter. For numbers, 1=double, 0=int
   // For strings, it's max string length
   // For identifiers (true/false), it's flag bit#
   int extra;

   // hash list link: keep it last so it doesn't have to be mentioned
   DLListItem<UDMFBinding> link;
};

struct UDMFLinedef
{
   int id;
   int v1;
   int v2;
   uint32_t flags;
   int special;
   int arg0;
   int arg1;
   int arg2;
   int arg3;
   int arg4;
   int sidefront;
   int sideback;

   UDMFLinedef &makeDefault()
   {
      id = gUDMFNamespace == unsDoom || gUDMFNamespace == unsHeretic
         || gUDMFNamespace == unsStrife ? 0 : -1;
      v1 = -1;
      v2 = -1;
      flags = 0;
      special = 0;
      arg0 = 0;
      arg1 = 0;
      arg2 = 0;
      arg3 = 0;
      arg4 = 0;
      sidefront = -1;
      sideback = -1;
      return *this;
   }
};

enum UDMFLinedefFlag
{
   UDMFLinedefFlag_Blocking,
   UDMFLinedefFlag_Blockmonsters,
   UDMFLinedefFlag_Twosided,
   UDMFLinedefFlag_Dontpegtop,
   UDMFLinedefFlag_Dontpegbottom,
   UDMFLinedefFlag_Secret,
   UDMFLinedefFlag_Blocksound,
   UDMFLinedefFlag_Dontdraw,
   UDMFLinedefFlag_Mapped,
   UDMFLinedefFlag_Passuse,
   UDMFLinedefFlag_Translucent,
   UDMFLinedefFlag_Jumpover,
   UDMFLinedefFlag_Blockfloaters,
   UDMFLinedefFlag_Playercross,
   UDMFLinedefFlag_Playeruse,
   UDMFLinedefFlag_Monstercross,
   UDMFLinedefFlag_Monsteruse,
   UDMFLinedefFlag_Impact,
   UDMFLinedefFlag_Playerpush,
   UDMFLinedefFlag_Monsterpush,
   UDMFLinedefFlag_Missilecross,
   UDMFLinedefFlag_Repeatspecial,
};

static UDMFBinding kLinedefBindings[] =
{
   { "id", UDMFTokenType_Number, offsetof(UDMFLinedef, id), 0 },
   { "v1", UDMFTokenType_Number, offsetof(UDMFLinedef, v1), 0 },
   { "v2", UDMFTokenType_Number, offsetof(UDMFLinedef, v2), 0 },
   { "blocking", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Blocking },
   { "blockmonsters", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Blockmonsters },
   { "twosided", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Twosided },
   { "dontpegtop", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Dontpegtop },
   { "dontpegbottom", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Dontpegbottom },
   { "secret", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Secret },
   { "blocksound", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Blocksound },
   { "dontdraw", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Dontdraw },
   { "mapped", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Mapped },
   { "passuse", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Passuse },
   { "translucent", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Translucent },
   { "jumpover", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Jumpover },
   { "blockfloaters", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Blockfloaters },
   { "playercross", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Playercross },
   { "playeruse", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Playeruse },
   { "monstercross", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Monstercross },
   { "monsteruse", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Monsteruse },
   { "impact", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Impact },
   { "playerpush", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Playerpush },
   { "monsterpush", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Monsterpush },
   { "missilecross", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Missilecross },
   { "repeatspecial", UDMFTokenType_Identifier, offsetof(UDMFLinedef, flags),
      UDMFLinedefFlag_Repeatspecial },
   { "special", UDMFTokenType_Number, offsetof(UDMFLinedef, special), 0 },
   { "arg0", UDMFTokenType_Number, offsetof(UDMFLinedef, arg0), 0 },
   { "arg1", UDMFTokenType_Number, offsetof(UDMFLinedef, arg1), 0 },
   { "arg2", UDMFTokenType_Number, offsetof(UDMFLinedef, arg2), 0 },
   { "arg3", UDMFTokenType_Number, offsetof(UDMFLinedef, arg3), 0 },
   { "arg4", UDMFTokenType_Number, offsetof(UDMFLinedef, arg4), 0 },
   { "sidefront", UDMFTokenType_Number, offsetof(UDMFLinedef, sidefront), 0 },
   { "sideback", UDMFTokenType_Number, offsetof(UDMFLinedef, sideback), 0 },
};

static EHashTable<UDMFBinding, ENCStringHashKey,
&UDMFBinding::name, &UDMFBinding::link> gLinedefBindings;

struct UDMFSidedef
{
   char texturetop[9];
   char texturebottom[9];
   char texturemiddle[9];
   int offsetx;
   int offsety;
   int sector;

   UDMFSidedef &makeDefault()
   {
      texturetop[0] = '-';
      texturetop[1] = texturetop[8] = 0;
      texturebottom[0] = '-';
      texturebottom[1] = texturebottom[8] = 0;
      texturemiddle[0] = '-';
      texturemiddle[1] = texturemiddle[8] = 0;
      offsetx = offsety = 0;
      sector = -1;

      return *this;
   }
};

static UDMFBinding kSidedefBindings[] =
{
   { "texturetop", UDMFTokenType_String,
      offsetof(UDMFSidedef, texturetop[0]), 8 },

   { "texturebottom", UDMFTokenType_String,
      offsetof(UDMFSidedef, texturebottom[0]), 8 },

   { "texturemiddle", UDMFTokenType_String,
      offsetof(UDMFSidedef, texturemiddle[0]), 8 },

   { "offsetx", UDMFTokenType_Number,
      offsetof(UDMFSidedef, offsetx), 0 },

   { "offsety", UDMFTokenType_Number,
      offsetof(UDMFSidedef, offsety), 0 },

   { "sector", UDMFTokenType_Number,
      offsetof(UDMFSidedef, sector), 0 },
};

static EHashTable<UDMFBinding, ENCStringHashKey,
&UDMFBinding::name, &UDMFBinding::link> gSidedefBindings;

struct UDMFVertex
{
   double x, y;

   UDMFVertex &makeDefault()
   {
      x = y = 0;
      return *this;
   }
};

static UDMFBinding kVertexBindings[] =
{
   { "x", UDMFTokenType_Number, offsetof(UDMFVertex, x), 1 },
   { "y", UDMFTokenType_Number, offsetof(UDMFVertex, y), 1 },
};

static EHashTable<UDMFBinding, ENCStringHashKey,
&UDMFBinding::name, &UDMFBinding::link> gVertexBindings;

struct UDMFSector
{
   char texturefloor[9], textureceiling[9];
   int heightfloor, heightceiling;
   int lightlevel;
   int special, id;

   UDMFSector &makeDefault()
   {
      texturefloor[0] = textureceiling[0] = '-';
      texturefloor[1] = textureceiling[1] = 0;
      texturefloor[8] = textureceiling[8] = 0;
      heightfloor = heightceiling = 0;
      lightlevel = 160;
      special = id = 0;

      return *this;
   }
};

static UDMFBinding kSectorBindings[] =
{
   { "texturefloor", UDMFTokenType_String,
      offsetof(UDMFSector, texturefloor[0]), 8 },

   { "textureceiling", UDMFTokenType_String,
      offsetof(UDMFSector, textureceiling[0]), 8 },

   { "heightfloor", UDMFTokenType_Number,
      offsetof(UDMFSector, heightfloor), 0 },

   { "heightceiling", UDMFTokenType_Number,
      offsetof(UDMFSector, heightceiling), 0 },

   { "lightlevel", UDMFTokenType_Number,
      offsetof(UDMFSector, lightlevel), 0 },

   { "special", UDMFTokenType_Number,
      offsetof(UDMFSector, special), 0 },

   { "id", UDMFTokenType_Number,
      offsetof(UDMFSector, id), 0 },
};

static EHashTable<UDMFBinding, ENCStringHashKey,
&UDMFBinding::name, &UDMFBinding::link> gSectorBindings;

struct UDMFThing
{
   double x, y, height;
   uint32_t flags;
   int id;
   int angle;
   int type;
   int special;
   int arg0, arg1, arg2, arg3, arg4;

   UDMFThing &makeDefault()
   {
      x = y = height = 0;
      flags = 0;
      id = angle = type = special = arg0 = arg1 = arg2 = arg3 = arg4 = 0;
      return *this;
   }
};

enum UDMFThingFlag
{
   UDMFThingFlag_Skill1,
   UDMFThingFlag_Skill2,
   UDMFThingFlag_Skill3,
   UDMFThingFlag_Skill4,
   UDMFThingFlag_Skill5,
   UDMFThingFlag_Ambush,
   UDMFThingFlag_Single,
   UDMFThingFlag_DM,
   UDMFThingFlag_Coop,
   UDMFThingFlag_Friend,
   UDMFThingFlag_Dormant,
   UDMFThingFlag_Class1,
   UDMFThingFlag_Class2,
   UDMFThingFlag_Class3,
   UDMFThingFlag_Standing,
   UDMFThingFlag_Strifeally,
   UDMFThingFlag_Translucent,
   UDMFThingFlag_Invisible,
};

static UDMFBinding kThingBindings[] =
{
   { "id", UDMFTokenType_Number, offsetof(UDMFThing, id), 0 },
   { "x", UDMFTokenType_Number, offsetof(UDMFThing, x), 1 },
   { "y", UDMFTokenType_Number, offsetof(UDMFThing, y), 1 },
   { "height", UDMFTokenType_Number, offsetof(UDMFThing, height), 1 },
   { "angle", UDMFTokenType_Number, offsetof(UDMFThing, angle), 0 },
   { "type", UDMFTokenType_Number, offsetof(UDMFThing, type), 0 },
   { "skill1", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Skill1 },
   { "skill2", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Skill2 },
   { "skill3", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Skill3 },
   { "skill4", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Skill4 },
   { "skill5", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Skill5 },
   { "ambush", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Ambush },
   { "single", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Single },
   { "dm", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_DM },
   { "coop", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Coop },
   { "friend", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Friend },
   { "dormant", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Dormant },
   { "class1", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Class1 },
   { "class2", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Class2 },
   { "class3", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Class3 },
   { "standing", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Standing },
   { "strifeally", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Strifeally },
   { "translucent", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Translucent },
   { "invisible", UDMFTokenType_Identifier, offsetof(UDMFThing, flags),
      UDMFThingFlag_Invisible },
   { "special", UDMFTokenType_Number, offsetof(UDMFThing, special), 0 },
   { "arg0", UDMFTokenType_Number, offsetof(UDMFThing, arg0), 0 },
   { "arg1", UDMFTokenType_Number, offsetof(UDMFThing, arg1), 0 },
   { "arg2", UDMFTokenType_Number, offsetof(UDMFThing, arg2), 0 },
   { "arg3", UDMFTokenType_Number, offsetof(UDMFThing, arg3), 0 },
   { "arg4", UDMFTokenType_Number, offsetof(UDMFThing, arg4), 0 },
};

static EHashTable<UDMFBinding, ENCStringHashKey,
&UDMFBinding::name, &UDMFBinding::link> gThingBindings;

//
// E_assignNamespace
//
// Sets the global namespace variable to a value based on text.
// Returns true if namespace is one of the assigned constants
//
static bool E_assignNamespace(const char *value)
{
   if(!strcasecmp(value, kEternityNamespace))
   {
      gUDMFNamespace = unsEternity;
      return true;
   }
   if(!strcasecmp(value, kDoomNamespace))
   {
      gUDMFNamespace = unsDoom;
      return true;
   }
   if(!strcasecmp(value, kHereticNamespace))
   {
      gUDMFNamespace = unsHeretic;
      return true;
   }
   if(!strcasecmp(value, kHexenNamespace))
   {
      gUDMFNamespace = unsHexen;
      return true;
   }
   if(!strcasecmp(value, kStrifeNamespace))
   {
      gUDMFNamespace = unsStrife;
      return true;
   }
   return false;
}


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
// UDMFException
//
// Used to funnel all parse errors into a single place. Uses a message and a
// line number, which can then be accessed directly
//
class UDMFException
{
private:
   const char *message;
   int line;
   int column;

public:

   UDMFException(const char *inMessage, int inLine, int inColumn) 
      : message(inMessage), line(inLine), column(inColumn)
   {
   }

   void setLevelError() const;
};

//
// UDMFException::setLevelError
//
// Converts the exception to a P_SetupLevel level_error, so the loading will 
// stop.
//
void UDMFException::setLevelError() const
{
   psnprintf(gLevelErrorBuffer, earrlen(gLevelErrorBuffer), "%s at line %d:%d",
         message, line, column);

   // Activate the error by setting this pointer
   level_error = gLevelErrorBuffer;
}

//
// As we see, TEXTMAP consists of global_expr entries. Each global_expr is
// a metatable. 
//

static PODCollection<UDMFLinedef> gLinedefs;
static PODCollection<UDMFSidedef> gSidedefs;
static PODCollection<UDMFVertex> gVertices;
static PODCollection<UDMFSector> gSectors;
static PODCollection<UDMFThing> gThings;

//
// E_LoadUDMFVertices
//
// Loads vertices from "vertex" metatable list
//
void E_LoadUDMFVertices()
{
   ::numvertexes = static_cast<int>(gVertices.getLength());
   ::vertexes = estructalloctag(vertex_t, ::numvertexes, PU_LEVEL);

   vertex_t *v = ::vertexes;
   for(const auto &uvertex : gVertices)
   {
      v->x = M_DoubleToFixed(uvertex.x);
      v->y = M_DoubleToFixed(uvertex.y);
      v->fx = static_cast<float>(uvertex.x);
      v->fy = static_cast<float>(uvertex.y);
      ++v;
   }
}

//
// E_LoadUDMFSectors
//
// Loads sectors from "sector" metatable list
//
void E_LoadUDMFSectors()
{
   ::numsectors = static_cast<int>(gSectors.getLength());
   ::sectors = estructalloctag(sector_t, ::numsectors, PU_LEVEL);

   sector_t *s = ::sectors;
   for(const auto &us : gSectors)
   {
      s->floorheight = us.heightfloor << FRACBITS;
      s->ceilingheight = us.heightceiling << FRACBITS;
      s->floorpic = R_FindFlat(us.texturefloor);
      P_SetSectorCeilingPic(s, R_FindFlat(us.textureceiling));
      s->lightlevel = us.lightlevel;
      s->special = us.special;
      s->tag = us.id;

      P_InitSector(s);

      ++s;
   }
}

//
// E_LoadUDMFSideDefs
//
// Equivalent of first P_LoadSideDefs call
//
void E_LoadUDMFSideDefs()
{
   ::numsides = static_cast<int>(gSidedefs.getLength());
   ::sides = estructalloctag(side_t, ::numsides, PU_LEVEL);
}

//
// E_LoadUDMFLineDefs
//
// Equivalent of P_LoadLineDefs
//
void E_LoadUDMFLineDefs()
{
   ::numlines = static_cast<int>(gLinedefs.getLength());
   ::lines = estructalloctag(line_t, ::numlines, PU_LEVEL);

   line_t *ld = ::lines;
   int v[2];
   int s[2];
   for(const auto &uld : gLinedefs)
   {
         
      if(uld.flags & 1 << UDMFLinedefFlag_Blocking)
         ld->flags |= ML_BLOCKING;
      if(uld.flags & 1 << UDMFLinedefFlag_Blockmonsters)
         ld->flags |= ML_BLOCKMONSTERS;
      if(uld.flags & 1 << UDMFLinedefFlag_Twosided)
         ld->flags |= ML_TWOSIDED;
      if(uld.flags & 1 << UDMFLinedefFlag_Dontpegtop)
         ld->flags |= ML_DONTPEGTOP;
      if(uld.flags & 1 << UDMFLinedefFlag_Dontpegbottom)
         ld->flags |= ML_DONTPEGBOTTOM;
      if(uld.flags & 1 << UDMFLinedefFlag_Secret)
         ld->flags |= ML_SECRET;
      if(uld.flags & 1 << UDMFLinedefFlag_Blocksound)
         ld->flags |= ML_SOUNDBLOCK;
      if(uld.flags & 1 << UDMFLinedefFlag_Dontdraw)
         ld->flags |= ML_DONTDRAW;
      if(uld.flags & 1 << UDMFLinedefFlag_Mapped)
         ld->flags |= ML_MAPPED;
      if(uld.flags & 1 << UDMFLinedefFlag_Passuse)
         ld->flags |= ML_PASSUSE;

      // STRIFE. NAMESPACE REQUIRED
      if(gUDMFNamespace == unsStrife)
      {
         // TODO: support "translucent", "jumpover" and "blockfloaters".
         // Currently the flags don't seem to exist in Eternity
      }
      if(gUDMFNamespace == unsEternity || gUDMFNamespace == unsHexen)
      {
         // FIXME: not sure if Eternity will use the same fields as Hexen.
         // ExtraData has them set up differently

         // Based on spac_flags_tlate
         if(uld.flags & 1 << UDMFLinedefFlag_Playercross)
            ld->extflags |= EX_ML_CROSS | EX_ML_PLAYER;
         if(uld.flags & 1 << UDMFLinedefFlag_Playeruse)
            ld->extflags |= EX_ML_USE | EX_ML_PLAYER;
         if(uld.flags & 1 << UDMFLinedefFlag_Monstercross)
            ld->extflags |= EX_ML_CROSS | EX_ML_MONSTER;
         if(uld.flags & 1 << UDMFLinedefFlag_Monsteruse)
            ld->extflags |= EX_ML_USE | EX_ML_MONSTER;
         if(uld.flags & 1 << UDMFLinedefFlag_Impact)
            ld->extflags |= EX_ML_IMPACT | EX_ML_MISSILE;
         if(uld.flags & 1 << UDMFLinedefFlag_Playerpush)
            ld->extflags |= EX_ML_PUSH | EX_ML_PLAYER;
         if(uld.flags & 1 << UDMFLinedefFlag_Monsterpush)
            ld->extflags |= EX_ML_PUSH | EX_ML_MONSTER;
         if(uld.flags & 1 << UDMFLinedefFlag_Missilecross)
            ld->extflags |= EX_ML_CROSS | EX_ML_MISSILE;
         if(uld.flags & 1 << UDMFLinedefFlag_Repeatspecial)
            ld->extflags |= EX_ML_REPEAT;
      }
      ld->special = uld.special;
      // Special handling for tag
      if(gUDMFNamespace == unsDoom || gUDMFNamespace == unsHeretic
         || gUDMFNamespace == unsStrife)
      {
         // FIXME: this is obviously not complete. The classic specials need to
         // be updated to use either args[0] or tag.
         ld->tag = uld.id;
         ld->args[0] = uld.arg0;
      }
      else if(gUDMFNamespace == unsEternity)
      {
         // Eternity
         // TENTATIVE BEHAVIOUR
         ld->tag = uld.id; 

         // TENTATIVE TODO: Eternity namespace special "alpha"
         //ld->alpha = static_cast<float>(E_requireOptDouble(table, "alpha", 1.0));

         // TODO: equivalent of ExtraData 1SONLY, ADDITIVE, ZONEBOUNDARY, CLIPMIDTEX
      }
      else if(gUDMFNamespace == unsHexen)
      {
         ld->tag = -1;  // Hexen will always use args
      }
      v[0] = uld.v1;
      v[1] = uld.v2;
      if(v[0] < 0 || v[0] >= ::numvertexes 
         || v[1] < 0 || v[1] >= ::numvertexes)
      {
         psnprintf(gLevelErrorBuffer, sizeof(gLevelErrorBuffer), 
            "Linedef %d vertex out of bounds", (int)(ld - ::lines));
         level_error = gLevelErrorBuffer;
         return;
      }
      ld->v1 = ::vertexes + v[0];
      ld->v2 = ::vertexes + v[1];
      s[0] = uld.sidefront;
      s[1] = uld.sideback;
      if(s[0] < 0 || s[0] >= ::numsides || s[1] < -1 || s[1] >= ::numsides)
      {
         psnprintf(gLevelErrorBuffer, sizeof(gLevelErrorBuffer), 
            "Linedef %d sidedef out of bounds", (int)(ld - ::lines));
         level_error = gLevelErrorBuffer;
         return;
      }

      ld->sidenum[0] = s[0];
      ld->sidenum[1] = s[1];
      P_InitLineDef(ld);

      // FIXME: should ExtraData records still be able to override UDMF?
      // Currently they aren't

      // Load args if Hexen or EE
      if(gUDMFNamespace == unsEternity || gUDMFNamespace == unsHexen)
      {
         ld->args[0] = uld.arg0;
         ld->args[1] = uld.arg1;
         ld->args[2] = uld.arg2;
         ld->args[3] = uld.arg3;
         ld->args[4] = uld.arg4;
      }
         
      P_PostProcessLineFlags(ld);
      ++ld;
   }
}

//
// E_LoadUDMFSideDefs2
//
// Equivalent of P_LoadSideDefs2
//
void E_LoadUDMFSideDefs2()
{
   side_t *sd;

   const char *topTexture, *bottomTexture, *midTexture;
   int secnum;

   sd = ::sides;
   for(const auto &usd : gSidedefs)
   {
      sd->textureoffset = usd.offsetx << FRACBITS;
      sd->rowoffset = usd.offsety << FRACBITS;

      topTexture = usd.texturetop;
      bottomTexture = usd.texturebottom;
      midTexture = usd.texturemiddle;

      secnum = usd.sector;

      if(secnum < 0 || secnum >= ::numsectors)
      {
         psnprintf(gLevelErrorBuffer, sizeof(gLevelErrorBuffer), 
            "Sidedef %d sector out of bounds", (int)(sd - ::sides));
         level_error = gLevelErrorBuffer;
         return;
      }

      sd->sector = ::sectors + secnum;

      P_SetupSidedefTextures(*sd, bottomTexture, midTexture, topTexture);

      ++sd;
   }
}

//
// E_LoadUDMFThings
//
// Load things, like P_LoadThings
//
void E_LoadUDMFThings()
{
   mapthing_t *mapthings = nullptr;
   ::numthings = static_cast<int>(gThings.getLength());

   mapthings = ecalloc(mapthing_t *, numthings, sizeof(mapthing_t));
   mapthing_t *ft = mapthings;

   bool skill[5] = { false };

   for(const auto &ut : gThings)
   {
      ft->type = ut.type;
      if(P_CheckThingDoomBan(ft->type))
      {
         ++ft;
         continue;
      }

      ft->x = M_DoubleToFixed(ut.x);
      ft->y = M_DoubleToFixed(ut.y);
      ft->height = M_DoubleToFixed(ut.height);
         
      ft->angle = static_cast<int16_t>(ut.angle);

      skill[0] = !!(ut.flags & 1 << UDMFThingFlag_Skill1);
      skill[1] = !!(ut.flags & 1 << UDMFThingFlag_Skill2);
      skill[2] = !!(ut.flags & 1 << UDMFThingFlag_Skill3);
      skill[3] = !!(ut.flags & 1 << UDMFThingFlag_Skill4);
      skill[4] = !!(ut.flags & 1 << UDMFThingFlag_Skill5);

      if(skill[1])
         ft->options |= MTF_EASY;
      if(skill[0] != skill[1])
         ft->extOptions |= MTF_EX_BABY_TOGGLE;
      if(skill[2])
         ft->options |= MTF_NORMAL;
      if(skill[3])
         ft->options |= MTF_HARD;
      if(skill[4] != skill[3])
         ft->extOptions |= MTF_EX_NIGHTMARE_TOGGLE;

      if(ut.flags & 1 << UDMFThingFlag_Ambush)
         ft->options |= MTF_AMBUSH;
         
      // negative
      if(!(ut.flags & 1 << UDMFThingFlag_Single))
         ft->options |= MTF_NOTSINGLE;
      if(!(ut.flags & 1 << UDMFThingFlag_DM))
         ft->options |= MTF_NOTDM;
      if(!(ut.flags & 1 << UDMFThingFlag_Coop))
         ft->options |= MTF_NOTCOOP;

      if(gUDMFNamespace == unsDoom || gUDMFNamespace == unsEternity)
      {
         if(ut.flags & 1 << UDMFThingFlag_Friend)
            ft->options |= MTF_FRIEND;
      }

      if(gUDMFNamespace == unsHexen || gUDMFNamespace == unsEternity)
      {
         if(ut.flags & 1 << UDMFThingFlag_Dormant)
            ft->options |= MTF_DORMANT;
         ft->tid = static_cast<int16_t>(ut.id);
            
         ft->args[0] = ut.arg0;;
         ft->args[1] = ut.arg1;
         ft->args[2] = ut.arg2;
         ft->args[3] = ut.arg3;
         ft->args[4] = ut.arg4;
      }

      // TODO: Hexen classes!

      // TODO: Strife standing
      // TODO: Strife ally
      // TODO: Strife shadow
      // TODO: Strife invisible

      if(gUDMFNamespace == unsHeretic)
         P_ConvertHereticThing(ft);
         
      P_ConvertDoomExtendedSpawnNum(ft);

      // use the extra parameters here
      P_SpawnMapThing(ft);
      ++ft;
   }

   // do the player start check like in P_LoadThings
   if(GameType != gt_dm)
   {
      for(int i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i] && !players[i].mo)
            level_error = "Missing required player start";
      }
   } 
   
   efree(mapthings);
   
}

//
// UDMFBlockBinding
//
// Binding for block definition
//
struct UDMFBlockBinding
{
   const char *name;

   EHashTable<UDMFBinding, ENCStringHashKey,
   &UDMFBinding::name, &UDMFBinding::link> *fieldBindings;
   byte *(*createItem)();

   DLListItem<UDMFBlockBinding> link;
};

static UDMFBlockBinding kBlockBindings[] =
{
   { "linedef", &gLinedefBindings, []() {
      return reinterpret_cast<byte *>(&gLinedefs.addNew().makeDefault());
   } },
   { "sidedef", &gSidedefBindings, []() {
      return reinterpret_cast<byte *>(&gSidedefs.addNew().makeDefault());
   } },
   { "vertex", &gVertexBindings, []() {
      return reinterpret_cast<byte *>(&gVertices.addNew().makeDefault());
   } },
   { "sector", &gSectorBindings, []() {
      return reinterpret_cast<byte *>(&gSectors.addNew().makeDefault());
   } },
   { "thing", &gThingBindings, []() {
      return reinterpret_cast<byte *>(&gThings.addNew().makeDefault());
   } },
};

static EHashTable<UDMFBlockBinding, ENCStringHashKey,
&UDMFBlockBinding::name, &UDMFBlockBinding::link> gBlockBindings;

//
// E_initHashTables
//
// Initializes hash tables from this file, if not already
//
static void E_initHashTables()
{
   static bool initialized;
   if(initialized)
      return;
   initialized = true;

   for(auto &item : kThingBindings)
      gThingBindings.addObject(&item);
   for(auto &item : kLinedefBindings)
      gLinedefBindings.addObject(&item);
   for(auto &item : kSidedefBindings)
      gSidedefBindings.addObject(&item);
   for(auto &item : kVertexBindings)
      gVertexBindings.addObject(&item);
   for(auto &item : kSectorBindings)
      gSectorBindings.addObject(&item);
   for(auto &item : kBlockBindings)
      gBlockBindings.addObject(&item);
}

//
// UDMFParser::Tokenizer::readToken
//
// Returns next token (or false if end of TEXTMAP)
//
bool UDMFParser::Tokenizer::readToken(Token &token)
{
   char *numberParseOut = nullptr;

   while(mData < mEnd)
   {
      while(mData < mEnd && ectype::isSpace(*mData))
      {
         if(*mData == '\n')
            nextLine();
         ++mData;
      }
      if(mData + 1 < mEnd && *mData == '/' && *(mData + 1) == '/')
      {
         mData += 2;
         while(mData < mEnd && *mData != '\n')
            ++mData;
         if(mData < mEnd && *mData == '\n')
         {
            nextLine();
            ++mData;
         }
      }
      if(mData + 1 < mEnd && *mData == '/' && *(mData + 1) == '*')
      {
         mData += 2;
         while(mData < mEnd && (*mData != '*' || mData + 1 >= mEnd 
            || *(mData + 1) != '/'))
         {
            if(*mData == '\n')
               nextLine();
            ++mData;
         }
         if(mData + 1 < mEnd && *mData == '*' && *(mData + 1) == '/')
            mData += 2;
      }
      if(mData < mEnd)
      {
         if(ectype::isAlpha(*mData) || *mData == '_')
         {
            // Found an identifier token
            token.data = mData;
            token.number = 0;
            
            ++mData;
            while(mData < mEnd && (ectype::isAlnum(*mData) || *mData == '_'))
               ++mData;

            token.type = UDMFTokenType_Identifier;
            token.size = mData - token.data;
            return true;
         }
         if(*mData == '"')
         {
            // Also modify it in-place to be easily stored internally
            int displace = 0;
            ++mData;
            token.data = mData;
            token.number = 0;
            while(mData < mEnd && (*mData != '"' || *(mData - 1) == '\\'))
            {
               if(*mData == '\n')
                  nextLine();
               if(displace)
                  *(mData - displace) = *mData;
               if(*mData == '\\' && *(mData - 1) != '\\')
                  ++displace;
               ++mData;
            }
            if(mData >= mEnd)
               return false;   // invalid string won't be treated
            *(mData - displace) = 0;
            token.size = mData - token.data;
            ++mData;
            token.type = UDMFTokenType_String;
            return true;
         }
         if(*mData == '.' && mData + 1 < mEnd && ectype::isDigit(*(mData + 1)))
         {
            token.data = mData;
            token.number = strtod(mData, &numberParseOut);
            mData = numberParseOut;
            token.type = UDMFTokenType_Number;
            token.size = mData - token.data;
            return true;
         }
         if(*mData == '0' && mData + 1 < mEnd && ectype::isDigit(*(mData + 1)))
         {
            token.data = mData;
            token.number = static_cast<double>(
               strtol(mData, &numberParseOut, 8));
            mData = numberParseOut;
            token.type = UDMFTokenType_Number;
            token.size = mData - token.data;
            return true;
         }
         if(*mData == '0' && mData + 1 < mEnd 
            && (*(mData + 1) == 'x' || *(mData + 1) == 'X'))
         {
            token.data = mData;
            token.number = static_cast<double>(
               strtol(mData, &numberParseOut, 16));
            mData = numberParseOut;
            token.type = UDMFTokenType_Number;
            token.size = mData - token.data;
            return true;
         }
         if(ectype::isDigit(*mData))
         {
            token.data = mData;
            token.number = strtod(mData, &numberParseOut);
            mData = numberParseOut;
            token.type = UDMFTokenType_Number;
            token.size = mData - token.data;
            return true;
         }
         if(!ectype::isSpace(*mData))
         {
            token.data = mData;
            token.number = 0;
            ++mData;
            token.type = UDMFTokenType_Operator;
            token.size = 1;
            return true;
         }
      }
   }

   return false;
}

//
// UDMFParser::Tokenizer::raise
//
// Convenient exception thrower that uses an internal variable
//
void UDMFParser::Tokenizer::raise(const char *message) const
{
   // FIXME: integer coordinates in file, a sub 2 GB TEXTMAP is assumed
   throw UDMFException(message, mLine, 
      static_cast<int>(mData - mLineStart + 1));
}

//
// UDMFParser::handleGlobalAssignment
//
// Called when a global assignment is detected. Only namespace is known now
// so don't use complex data structures
//
void UDMFParser::handleGlobalAssignment() const
{
   if(!strcasecmp(mGlobalKey, "namespace"))
   {
      if(mGlobalValue.type != UDMFTokenType_String)
         mTokenizer.raise("Namespace value must be a string");
      E_assignNamespace(mGlobalValue.data);
   }
}

//
// UDMFParser::handleNewBlock
//
// Called when a block is detected
//
void UDMFParser::handleNewBlock() 
{
   mCurrentNewItem = nullptr;
   const UDMFBlockBinding *binding = gBlockBindings.objectForKey(mGlobalKey);
   if(binding)
   {
      mCurrentNewItem = binding->createItem();
      mCurrentBinding = binding->fieldBindings;
   }
   
}

//
// UDMFParser::handleLocalAssignment
//
// Called when a local (in-block) assignment is encountered
//
void UDMFParser::handleLocalAssignment() const
{
   if(!mCurrentNewItem)
      return;

   auto bindings = static_cast<const EHashTable<UDMFBinding, ENCStringHashKey, 
      &UDMFBinding::name, &UDMFBinding::link> *>(mCurrentBinding);

   const UDMFBinding *binding = bindings->objectForKey(mLocalKey);
   if(binding && binding->type == mLocalValue.type)
   {
      switch(mLocalValue.type)
      {
      case UDMFTokenType_Identifier: // true or false
         if(*mLocalValue.data == 't' || *mLocalValue.data == 'T')
         {
            *reinterpret_cast<uint32_t *>(mCurrentNewItem + binding->offset) 
               |= 1 << binding->extra;
         }
         break;
      case UDMFTokenType_Number:
         if(binding->extra)   // double
         {
            *reinterpret_cast<double *>(mCurrentNewItem + binding->offset) 
               = mLocalValue.number;
         }
         else
         {
            // int
            *reinterpret_cast<int *>(mCurrentNewItem + binding->offset) 
               = static_cast<int>(mLocalValue.number);
         }
         break;
      case UDMFTokenType_String:
         strncpy(reinterpret_cast<char *>(mCurrentNewItem + binding->offset), 
            mLocalValue.data, binding->extra);
         break;
      default:
         break;
      }
   }

   
}

//
// UDMFParser::start
//
// Called when UDMF parsing is decided.
//
void UDMFParser::start(WadDirectory &setupwad, int lump)
{
   enum Expect
   {
      Expect_GlobalIdentifier,
      Expect_GlobalEqualBrace,
      Expect_GlobalValue,
      Expect_GlobalNumber,
      Expect_GlobalSemicolon,
      Expect_LocalIdentifierBrace,
      Expect_LocalEqual,
      Expect_LocalValue,
      Expect_LocalNumber,
      Expect_LocalSemicolon,
   } expect = Expect_GlobalIdentifier;

   ZAutoBuffer buf;

   setupwad.cacheLumpAuto(lump, buf);
   auto data = buf.getAs<char *>();

   mTokenizer.setData(data, setupwad.lumpLength(lump));

   // These are global because I don't really want them to be destroyed on 
   // each level.
   E_initHashTables();
   gLinedefs.makeEmpty();
   gSidedefs.makeEmpty();
   gVertices.makeEmpty();
   gSectors.makeEmpty();
   gThings.makeEmpty();

   Token token, globalField, localField;
   int sign = 1;

   try
   {
      while(mTokenizer.readToken(token))
      {
         switch(expect)
         {
         case Expect_GlobalIdentifier:
            if(token.type != UDMFTokenType_Identifier)
               mTokenizer.raise("Expected global field");

            globalField = token;
            expect = Expect_GlobalEqualBrace;
            break;
         case Expect_GlobalEqualBrace:
            if(token.type != UDMFTokenType_Operator
               || (*token.data != '{' && *token.data != '='))
            {
               mTokenizer.raise("Expected '=' or '{' after identifier");
            }
            // make C string from global field
            *(globalField.data + globalField.size) = 0;
            mGlobalKey = globalField.data;

            if(*token.data == '=')
            {
               expect = Expect_GlobalValue;
               sign = 1;
            }
            else
            {
               handleNewBlock();
               expect = Expect_LocalIdentifierBrace;
            }
            break;
         case Expect_GlobalValue:
            mGlobalValue = token;
            if(token.type == UDMFTokenType_Identifier)
            {
               if((token.size == 4 && strncasecmp(token.data, "true", 4))
                  || (token.size == 5 && strncasecmp(token.data, "false", 5))
                  || (token.size != 4 && token.size != 5))
               {
                  mTokenizer.raise("Found keyword but expected 'true' or "
                     "'false' after '='");
               }
               
               handleGlobalAssignment();
               expect = Expect_GlobalSemicolon;
            }
            else if(token.type == UDMFTokenType_Operator)
            {
               if(*token.data == '-')
                  sign = -sign;
               else if(*token.data != '+')
                  mTokenizer.raise("Invalid character after '='");

               expect = Expect_GlobalNumber;
            }
            else if(token.type == UDMFTokenType_Number)
            {
               mGlobalValue.number *= sign;

               handleGlobalAssignment();
               expect = Expect_GlobalSemicolon;
            }
            else
            {
               // string
               handleGlobalAssignment();
               expect = Expect_GlobalSemicolon;
            }
            break;
         case Expect_GlobalNumber:
            mGlobalValue = token;

            if(token.type != UDMFTokenType_Number)
            {
               mTokenizer.raise("Expected number after sign operator");
            }
            mGlobalValue.number *= sign;
            handleGlobalAssignment();

            expect = Expect_GlobalSemicolon;
            break;
         case Expect_GlobalSemicolon:
            if(token.type != UDMFTokenType_Operator || *token.data != ';')
            {
               mTokenizer.raise("Expected ';' after value");
            }
            expect = Expect_GlobalIdentifier;
            break;

         case Expect_LocalIdentifierBrace:
            if(token.type == UDMFTokenType_Identifier)
            {
               localField = token;
               expect = Expect_LocalEqual;
            }
            else if(token.type == UDMFTokenType_Operator && *token.data == '}')
               expect = Expect_GlobalIdentifier;
            else
               mTokenizer.raise("Expected identifier or '}'");
            break;
         case Expect_LocalEqual:
            if(token.type != UDMFTokenType_Operator || *token.data != '=')
               mTokenizer.raise("Expected '=' after local field");
            *(localField.data + localField.size) = 0;
            mLocalKey = localField.data;
            sign = 1;
            expect = Expect_LocalValue;
            break;
         case Expect_LocalValue:
            mLocalValue = token;
            if(token.type == UDMFTokenType_Identifier)
            {
               if((token.size == 4 && strncasecmp(token.data, "true", 4))
                  || (token.size == 5 && strncasecmp(token.data, "false", 5))
                  || (token.size != 4 && token.size != 5))
               {
                  mTokenizer.raise("Found keyword but expected 'true' or "
                     "'false' after '='");
               }
               handleLocalAssignment();
               expect = Expect_LocalSemicolon;
            }
            else if(token.type == UDMFTokenType_Operator)
            {
               if(*token.data == '-')
                  sign = -sign;
               else if(*token.data != '+')
                  mTokenizer.raise("Invalid character after '='");
               expect = Expect_LocalNumber;
            }
            else if(token.type == UDMFTokenType_Number)
            {
               mLocalValue.number *= sign;
               handleLocalAssignment();
               expect = Expect_LocalSemicolon;
            }
            else
            {
               handleLocalAssignment();
               expect = Expect_LocalSemicolon;
            }
            break;
         case Expect_LocalNumber:
            mLocalValue = token;
            if(token.type != UDMFTokenType_Number)
            {
               mTokenizer.raise("Expected number after sign operator");
            }
            mLocalValue.number *= sign;
            handleLocalAssignment();
            expect = Expect_LocalSemicolon;
            break;
         case Expect_LocalSemicolon:
            if(token.type != UDMFTokenType_Operator || *token.data != ';')
               break;
            expect = Expect_LocalIdentifierBrace;
            break;
         }
      }
   }
   catch(const UDMFException &e)
   {
      e.setLevelError();
   }

}

//
// Clear all lists now (called from the same function that does the parsing
// and the loading of map objects)
//
UDMFParser::~UDMFParser()
{
   // Some primitive anti-leak protection
   if(gLinedefs.getLength() > 10000)
      gLinedefs.clear();
   else
      gLinedefs.makeEmpty();
   if(gSidedefs.getLength() > 10000)
      gSidedefs.clear();
   else 
      gSidedefs.makeEmpty();
   if(gVertices.getLength() > 10000)
      gVertices.clear();
   else
      gVertices.makeEmpty();
   if(gSectors.getLength() > 10000)
      gSectors.clear();
   else
      gSectors.makeEmpty();
   if(gThings.getLength() > 10000)
      gThings.clear();
   else 
      gThings.makeEmpty();
}

// EOF

