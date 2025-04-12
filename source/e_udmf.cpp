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
// Authors: Ioan Chera, Max Waine, anotak
//

#include "z_zone.h"

#include "doomstat.h"
#include "e_exdata.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_sound.h"
#include "e_ttypes.h"
#include "e_udmf.h"
#include "m_compare.h"
#include "p_scroll.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_main.h" // Needed for PI
#include "r_portal.h" // Needed for portalflags
#include "r_state.h"
#include "v_misc.h"
#include "w_wad.h"
#include "z_auto.h"

// SOME CONSTANTS
static const char DEFAULT_default[] = "@default";
static const char DEFAULT_flat[] = "@flat";

static const char RENDERSTYLE_translucent[] = "translucent";
static const char RENDERSTYLE_add[] = "add";

static const char *udmfscrolltypes[NUMSCROLLTYPES] =
{
   "none",
   "visual",
   "physical",
   "both"
};

static constexpr const char *udmfskewtypes[NUMSKEWTYPES] =
{
   "none",
   "front_floor",
   "front_ceiling",
   "back_floor",
   "back_ceiling",
};

//
// Initializes the internal structure with the sector count
//
void UDMFSetupSettings::useSectorCount()
{
   if(mSectorInitData)
      return;
   mSectorInitData = estructalloc(sectorinfo_t, ::numsectors);
}

void UDMFSetupSettings::useLineCount()
{
   if(mLineInitData)
      return;
   mLineInitData = estructalloc(lineinfo_t, ::numlines);
}

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
void UDMFParser::loadSectors(UDMFSetupSettings &setupSettings) const
{
   numsectors = (int)mSectors.getLength();
   sectors = estructalloctag(sector_t, numsectors, PU_LEVEL);

   for(int i = 0; i < numsectors; ++i)
   {
      sector_t *ss = sectors + i;
      const USector &us = mSectors[i];

      if(mNamespace == namespace_Eternity)
      {
         // These two pass the fixed_t value now
         ss->srf.floor.height = us.heightfloor;
         ss->srf.ceiling.height = us.heightceiling;

         // New to Eternity
         ss->srf.floor.offset = { us.xpanningfloor, us.ypanningfloor };
         ss->srf.ceiling.offset = { us.xpanningceiling, us.ypanningceiling };
         ss->srf.floor.baseangle = static_cast<float>
            (E_NormalizeFlatAngle(us.rotationfloor) *  PI / 180.0f);
         ss->srf.ceiling.baseangle = static_cast<float>
            (E_NormalizeFlatAngle(us.rotationceiling) *  PI / 180.0f);

         int scrolltype = E_StrToNumLinear(udmfscrolltypes, NUMSCROLLTYPES,
                                           us.scroll_floor_type.constPtr());
         if(scrolltype != NUMSCROLLTYPES && (us.scroll_floor_x || us.scroll_floor_y))
            P_SpawnFloorUDMF(i, scrolltype, us.scroll_floor_x, us.scroll_floor_y);

         scrolltype = E_StrToNumLinear(udmfscrolltypes, NUMSCROLLTYPES,
                                       us.scroll_ceil_type.constPtr());
         if(scrolltype != NUMSCROLLTYPES && (us.scroll_ceil_x || us.scroll_ceil_y))
            P_SpawnCeilingUDMF(i, scrolltype, us.scroll_ceil_x, us.scroll_ceil_y);

         // Flags
         ss->flags |= us.secret ? SECF_SECRET : 0;

         // Friction: set the parameter directly from UDMF
         if(us.friction >= 0)   // default: -1
         {
            int friction, movefactor;
            P_CalcFriction(us.friction, friction, movefactor);

            ss->flags |= SECF_FRICTION;   // add the flag too
            ss->friction = friction;
            ss->movefactor = movefactor;
         }

         // Damage
         ss->damage = us.damageamount;
         ss->damagemask = us.damageinterval;
         ss->damagemod = E_DamageTypeNumForName(us.damagetype.constPtr());
         // If the following flags are true for the current sector, then set the
         // appropriate damageflags to true, otherwise don't set them.
         ss->damageflags |= us.damage_endgodmode ? SDMG_ENDGODMODE : 0;
         ss->damageflags |= us.damage_exitlevel ? SDMG_EXITLEVEL : 0;
         ss->damageflags |= us.damageterraineffect ? SDMG_TERRAINHIT : 0;
         ss->leakiness = eclamp(us.leakiness, 0, 256);

         // Terrain types
         if(us.floorterrain.strCaseCmp(DEFAULT_flat))
            ss->srf.floor.terrain = E_TerrainForName(us.floorterrain.constPtr());
         if (us.ceilingterrain.strCaseCmp(DEFAULT_flat))
            ss->srf.ceiling.terrain = E_TerrainForName(us.ceilingterrain.constPtr());

         // Lights
         ss->srf.floor.lightdelta = static_cast<int16_t>(us.lightfloor);
         ss->srf.ceiling.lightdelta = static_cast<int16_t>(us.lightceiling);
         ss->flags |=
         (us.lightfloorabsolute ? SECF_FLOORLIGHTABSOLUTE : 0) |
         (us.lightceilingabsolute ? SECF_CEILLIGHTABSOLUTE : 0) |
         (us.phasedlight ? SECF_PHASEDLIGHT : 0) |
         (us.lightsequence ? SECF_LIGHTSEQUENCE : 0) |
         (us.lightseqalt ? SECF_LIGHTSEQALT : 0);

         // sector colormaps
         ss->topmap = ss->midmap = ss->bottommap = -1; // mark as not specified

         setupSettings.setSectorPortals(i, us.portalceiling, us.portalfloor);
         setupSettings.getAttachInfo(i) = udmfattach_t{ us.floorid, 
            us.ceilingid, us.attachfloor, us.attachceiling };
      }
      else
      {
         ss->srf.floor.height = us.heightfloor << FRACBITS;
         ss->srf.ceiling.height = us.heightceiling << FRACBITS;
      }
      ss->srf.floor.pic = R_FindFlat(us.texturefloor.constPtr());
      P_SetSectorCeilingPic(ss,
                            R_FindFlat(us.textureceiling.constPtr()));
      ss->lightlevel = us.lightlevel;
      ss->special = us.special;
      ss->tag = us.identifier;
      P_InitSector(ss);

      //
      // HERE GO THE PROPERTIES THAT MUST TAKE EFFECT AFTER P_InitSector
      //
      if(mNamespace == namespace_Eternity)
      {
         if(us.colormaptop.strCaseCmp(DEFAULT_default))
         {
            ss->topmap    = R_ColormapNumForName(us.colormaptop.constPtr());
            setupSettings.setSectorFlag(i, UDMF_SECTOR_INIT_COLOR_TOP);
         }
         if(us.colormapmid.strCaseCmp(DEFAULT_default))
         {
            ss->midmap    = R_ColormapNumForName(us.colormapmid.constPtr());
            setupSettings.setSectorFlag(i, UDMF_SECTOR_INIT_COLOR_MIDDLE);
         }
         if(us.colormapbottom.strCaseCmp(DEFAULT_default))
         {
            ss->bottommap = R_ColormapNumForName(us.colormapbottom.constPtr());
            setupSettings.setSectorFlag(i, UDMF_SECTOR_INIT_COLOR_BOTTOM);
         }

         auto checkBadCMap = [ss](int *cmap)
         {
            if(*cmap < 0)
            {
               *cmap = 0;
               doom_printf(FC_ERROR "Invalid colormap for sector %d", eindex(ss - ::sectors));
            }
         };

         checkBadCMap(&ss->topmap);
         checkBadCMap(&ss->midmap);
         checkBadCMap(&ss->bottommap);

         // Portal fields
         // Floors
         int balpha = us.alphafloor >= 1.0 ? 255 : us.alphafloor <= 0 ? 
            0 : int(round(255 * us.alphafloor));
         balpha = eclamp(balpha, 0, 255);
         ss->srf.floor.pflags |= balpha << PO_OPACITYSHIFT;
         ss->srf.floor.pflags |= us.portal_floor_blocksound ? PF_BLOCKSOUND : 0;
         ss->srf.floor.pflags |= us.portal_floor_disabled ? PF_DISABLED : 0;
         ss->srf.floor.pflags |= us.portal_floor_nopass ? PF_NOPASS : 0;
         ss->srf.floor.pflags |= us.portal_floor_norender ? PF_NORENDER : 0;
         if(!us.portal_floor_overlaytype.strCaseCmp(RENDERSTYLE_translucent))
            ss->srf.floor.pflags |= PS_OVERLAY;
         else if(!us.portal_floor_overlaytype.strCaseCmp(RENDERSTYLE_add))
            ss->srf.floor.pflags |= PS_OBLENDFLAGS; // PS_OBLENDFLAGS is PS_OVERLAY | PS_ADDITIVE
         ss->srf.floor.pflags |= us.portal_floor_useglobaltex ? PS_USEGLOBALTEX : 0;
         ss->srf.floor.pflags |= us.portal_floor_attached ? PF_ATTACHEDPORTAL : 0;

         // Ceilings
         balpha = us.alphaceiling >= 1.0 ? 255 : us.alphaceiling <= 0 ? 
            0 : int(round(255 * us.alphaceiling));
         balpha = eclamp(balpha, 0, 255);
         ss->srf.ceiling.pflags |= balpha << PO_OPACITYSHIFT;
         ss->srf.ceiling.pflags |= us.portal_ceil_blocksound ? PF_BLOCKSOUND : 0;
         ss->srf.ceiling.pflags |= us.portal_ceil_disabled ? PF_DISABLED : 0;
         ss->srf.ceiling.pflags |= us.portal_ceil_nopass ? PF_NOPASS : 0;
         ss->srf.ceiling.pflags |= us.portal_ceil_norender ? PF_NORENDER : 0;
         if(!us.portal_ceil_overlaytype.strCaseCmp(RENDERSTYLE_translucent))
            ss->srf.ceiling.pflags |= PS_OVERLAY;
         else if(!us.portal_ceil_overlaytype.strCaseCmp(RENDERSTYLE_add))
            ss->srf.ceiling.pflags |= PS_OBLENDFLAGS; // PS_OBLENDFLAGS is PS_OVERLAY | PS_ADDITIVE
         ss->srf.ceiling.pflags |= us.portal_ceil_useglobaltex ? PS_USEGLOBALTEX : 0;
         ss->srf.ceiling.pflags |= us.portal_ceil_attached ? PF_ATTACHEDPORTAL : 0;

         ss->srf.floor.scale.x = static_cast<float>(us.xscalefloor);
         ss->srf.floor.scale.y = static_cast<float>(us.yscalefloor);
         ss->srf.ceiling.scale.x = static_cast<float>(us.xscaleceiling);
         ss->srf.ceiling.scale.y = static_cast<float>(us.yscaleceiling);

         // Sound sequences
         if(!us.soundsequence.empty())
         {
            char *endptr = nullptr;
            long number = strtol(us.soundsequence.constPtr(), &endptr, 10);
            if(endptr == us.soundsequence.constPtr())
            {
               // We got a string then
               const ESoundSeq_t *seq = E_SequenceForName(us.soundsequence.constPtr());
               if(seq)
                  ss->sndSeqID = seq->index;
            }
            else
            {
               // We got ourselves a number
               ss->sndSeqID = static_cast<int>(number);
            }
         }
      }
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
bool UDMFParser::loadLinedefs(UDMFSetupSettings &setupSettings)
{
   numlines = (int)mLinedefs.getLength();
   numlinesPlusExtra = numlines + NUM_LINES_EXTRA;
   lines = estructalloctag(line_t, numlinesPlusExtra, PU_LEVEL);
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

      if(mNamespace == namespace_Doom || mNamespace == namespace_Eternity)
      {
         if(uld.passuse)
            ld->flags |= ML_PASSUSE;
      }

      // Eternity
      if(mNamespace == namespace_Eternity)
      {
         if(uld.midtex3d)
            ld->flags |= ML_3DMIDTEX;
         if(uld.blocklandmonsters)
            ld->flags |= ML_BLOCKLANDMONSTERS;
         if(uld.blockplayers)
            ld->flags |= ML_BLOCKPLAYERS;
         if(uld.firstsideonly)
            ld->extflags |= EX_ML_1SONLY;
         if(uld.blockeverything)
            ld->extflags |= EX_ML_BLOCKALL;
         if(uld.zoneboundary)
            ld->extflags |= EX_ML_ZONEBOUNDARY;
         if(uld.clipmidtex)
            ld->extflags |= EX_ML_CLIPMIDTEX;
         if(uld.midtex3dimpassible)
            ld->extflags |= EX_ML_3DMTPASSPROJ;
         if(uld.lowerportal)
            ld->extflags |= EX_ML_LOWERPORTAL;
         if(uld.upperportal)
            ld->extflags |= EX_ML_UPPERPORTAL;
         setupSettings.setLinePortal(i, uld.portal);
      }

      // TODO: Strife

      if(mNamespace == namespace_Hexen || mNamespace == namespace_Eternity)
      {
         if(uld.playercross)
            ld->extflags |= EX_ML_PLAYER | EX_ML_CROSS;
         if(uld.playeruse)
            ld->extflags |= EX_ML_PLAYER | EX_ML_USE;
         if(uld.monstercross)
            ld->extflags |= EX_ML_MONSTER | EX_ML_CROSS;
         if(uld.monsteruse)
            ld->extflags |= EX_ML_MONSTER | EX_ML_USE;
         if(uld.impact)
            ld->extflags |= EX_ML_PLAYER | EX_ML_IMPACT;
         if(uld.monstershoot)
            ld->extflags |= EX_ML_MONSTER | EX_ML_IMPACT;
         if(uld.playerpush)
            ld->extflags |= EX_ML_PLAYER | EX_ML_PUSH;
         if(uld.monsterpush)
            ld->extflags |= EX_ML_MONSTER | EX_ML_PUSH;
         if(uld.missilecross)
            ld->extflags |= EX_ML_MISSILE | EX_ML_CROSS;
         if(uld.repeatspecial)
            ld->extflags |= EX_ML_REPEAT;
         if(uld.polycross)
            ld->extflags |= EX_ML_POLYOBJECT | EX_ML_CROSS;
      }

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
      if(mNamespace == namespace_Eternity)
      {
         ld->alpha = uld.alpha;
         if(!uld.renderstyle.strCaseCmp(RENDERSTYLE_add))
            ld->extflags |= EX_ML_ADDITIVE;
         if(!uld.tranmap.empty())
         {
            if(uld.tranmap != "TRANMAP")
            {
               int special = W_CheckNumForName(uld.tranmap.constPtr());
               if(special < 0 || W_LumpLength(special) != 65536)
                  ld->tranlump = 0;
               else
               {
                  ld->tranlump = special + 1;
                  wGlobalDir.cacheLumpNum(special, PU_CACHE);
               }
            }
            else
               ld->tranlump = 0;
         }
      }
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
         sd->offset_base_x = usd.offsetx;
         sd->offset_base_y = usd.offsety;

         sd->offset_bottom_x = usd.offsetx_bottom;
         sd->offset_bottom_y = usd.offsety_bottom;
         sd->offset_mid_x    = usd.offsetx_mid;
         sd->offset_mid_y    = usd.offsety_mid;
         sd->offset_top_x    = usd.offsetx_top;
         sd->offset_top_y    = usd.offsety_top;

         sd->light_base           = usd.light;
         sd->light_top            = usd.light_top;
         sd->light_mid            = usd.light_mid;
         sd->light_bottom         = usd.light_bottom;
         sd->flags |= (usd.lightabsolute        ? SDF_LIGHT_BASE_ABSOLUTE   : 0);
         sd->flags |= (usd.lightabsolute_top    ? SDF_LIGHT_TOP_ABSOLUTE    : 0);
         sd->flags |= (usd.lightabsolute_mid    ? SDF_LIGHT_MID_ABSOLUTE    : 0);
         sd->flags |= (usd.lightabsolute_bottom ? SDF_LIGHT_BOTTOM_ABSOLUTE : 0);

         const int skewTopType    = E_StrToNumLinear(udmfskewtypes, NUMSKEWTYPES, usd.skew_top_type.constPtr());
         const int skewBottomType = E_StrToNumLinear(udmfskewtypes, NUMSKEWTYPES, usd.skew_bottom_type.constPtr());
         const int skewMiddleType = E_StrToNumLinear(udmfskewtypes, NUMSKEWTYPES, usd.skew_middle_type.constPtr());
         sd->intflags |= ((skewTopType    == NUMSKEWTYPES ? 0 : skewTopType)    << SDI_SKEW_TOP_SHIFT);
         sd->intflags |= ((skewBottomType == NUMSKEWTYPES ? 0 : skewBottomType) << SDI_SKEW_BOTTOM_SHIFT);
         sd->intflags |= ((skewMiddleType == NUMSKEWTYPES ? 0 : skewMiddleType) << SDI_SKEW_MIDDLE_SHIFT);

         // TODO: Remove later probably
         if(usd.skew_top_type.length() && skewTopType == NUMSKEWTYPES)
         {
            if(usd.skew_top_type == "front")
               sd->intflags |= SKEW_FRONT_CEILING << SDI_SKEW_TOP_SHIFT;
            else if(usd.skew_top_type == "back")
               sd->intflags |= SKEW_BACK_CEILING << SDI_SKEW_TOP_SHIFT;
         }
         if(usd.skew_bottom_type.length() && skewBottomType == NUMSKEWTYPES)
         {
            if(usd.skew_bottom_type == "front")
               sd->intflags |= SKEW_FRONT_FLOOR << SDI_SKEW_BOTTOM_SHIFT;
            else if(usd.skew_bottom_type == "back")
               sd->intflags |= SKEW_BACK_FLOOR << SDI_SKEW_BOTTOM_SHIFT;
         }
      }
      else
      {
         sd->offset_base_x = usd.offsetx << FRACBITS;
         sd->offset_base_y = usd.offsety << FRACBITS;
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
      ft->tid = ut.identifier;
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
      if(ut.friendly && (mNamespace == namespace_Doom || mNamespace == namespace_Eternity))
         ft->options |= MTF_FRIEND;
      if(ut.dormant && (mNamespace == namespace_Hexen || mNamespace == namespace_Eternity))
         ft->options |= MTF_DORMANT;
      if(ut.standing)
         ft->extOptions |= MTF_EX_STAND;
      // TODO: class1, 2, 3
      // TODO: STRIFE
      if(mNamespace == namespace_Hexen || mNamespace == namespace_Eternity)
      {
         ft->special = ut.special;
         ft->args[0] = ut.arg[0];
         ft->args[1] = ut.arg[1];
         ft->args[2] = ut.arg[2];
         ft->args[3] = ut.arg[3];
         ft->args[4] = ut.arg[4];
      }

      if(mNamespace == namespace_Eternity)
      {
         ft->healthModifier = M_DoubleToFixed(ut.health);
      }

      // haleyjd 10/05/05: convert heretic things
      if(mNamespace == namespace_Heretic)
         P_ConvertHereticThing(ft);

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
// A means to map a string to a numeric token, so it can be easily passed into
// switch/case
//

enum token_e
{
   t_alpha,
   t_alphaceiling,
   t_alphafloor,
   t_ambush,
   t_angle,
   t_arg0,
   t_arg1,
   t_arg2,
   t_arg3,
   t_arg4,
   t_attachceiling,
   t_attachfloor,
   t_blockeverything,
   t_blockfloaters,
   t_blocking,
   t_blocklandmonsters,
   t_blockmonsters,
   t_blockplayers,
   t_blocksound,
   t_ceilingid,
   t_ceilingterrain,
   t_class1,
   t_class2,
   t_class3,
   t_clipmidtex,
   t_colormapbottom,
   t_colormapmid,
   t_colormaptop,
   t_coop,
   t_copyceilingportal,
   t_copyfloorportal,
   t_damage_endgodmode,
   t_damage_exitlevel,
   t_damageamount,
   t_damageinterval,
   t_damageterraineffect,
   t_damagetype,
   t_dm,
   t_dontdraw,
   t_dontpegbottom,
   t_dontpegtop,
   t_dormant,
   t_firstsideonly,
   t_floorid,
   t_floorterrain,
   t_friction,
   t_friend,
   t_health,
   t_height,
   t_heightceiling,
   t_heightfloor,
   t_id,
   t_impact,
   t_invisible,
   t_jumpover,
   t_leakiness,
   t_light,
   t_light_top,
   t_light_mid,
   t_light_bottom,
   t_lightabsolute,
   t_lightabsolute_top,
   t_lightabsolute_mid,
   t_lightabsolute_bottom,
   t_lightceiling,
   t_lightceilingabsolute,
   t_lightfloor,
   t_lightfloorabsolute,
   t_lightlevel,
   t_lightseqalt,
   t_lightsequence,
   t_lowerportal,
   t_mapped,
   t_midtex3d,
   t_midtex3dimpassible,
   t_missilecross,
   t_monstercross,
   t_monsterpush,
   t_monstershoot,
   t_monsteruse,
   t_offsetx,
   t_offsety,
   t_offsetx_bottom,
   t_offsety_bottom,
   t_offsetx_mid,
   t_offsety_mid,
   t_offsetx_top,
   t_offsety_top,
   t_phasedlight,
   t_polycross,
   t_portal,
   t_portalceiling,
   t_portal_ceil_attached,
   t_portal_ceil_blocksound,
   t_portal_ceil_disabled,
   t_portal_ceil_nopass,
   t_portal_ceil_norender,
   t_portal_ceil_overlaytype,
   t_portal_ceil_useglobaltex,
   t_portalfloor,
   t_portal_floor_attached,
   t_portal_floor_blocksound,
   t_portal_floor_disabled,
   t_portal_floor_nopass,
   t_portal_floor_norender,
   t_portal_floor_overlaytype,
   t_portal_floor_useglobaltex,
   t_passuse,
   t_playercross,
   t_playerpush,
   t_playeruse,
   t_renderstyle,
   t_repeatspecial,
   t_rotationceiling,
   t_rotationfloor,
   t_scroll_ceil_x,
   t_scroll_ceil_y,
   t_scroll_ceil_type,
   t_scroll_floor_x,
   t_scroll_floor_y,
   t_scroll_floor_type,
   t_secret,
   t_sector,
   t_sideback,
   t_sidefront,
   t_single,
   t_skew_bottom_type,
   t_skew_middle_type,
   t_skew_top_type,
   t_skill1,
   t_skill2,
   t_skill3,
   t_skill4,
   t_skill5,
   t_soundsequence,
   t_special,
   t_standing,
   t_strifeally,
   t_texturebottom,
   t_textureceiling,
   t_texturefloor,
   t_texturemiddle,
   t_texturetop,
   t_tranmap,
   t_translucent,
   t_twosided,
   t_type,
   t_upperportal,
   t_v1,
   t_v2,
   t_x,
   t_xpanningceiling,
   t_xpanningfloor,
   t_xscaleceiling,
   t_xscalefloor,
   t_y,
   t_ypanningceiling,
   t_ypanningfloor,
   t_yscaleceiling,
   t_yscalefloor,
   t_zoneboundary,
};

struct keytoken_t
{
   const char *string;
   DLListItem<keytoken_t> link;
   token_e token;
};

#define TOKEN(a) { #a, DLListItem<keytoken_t>(), t_##a }

static keytoken_t gTokenList[] =
{
   TOKEN(alpha),
   TOKEN(alphaceiling),
   TOKEN(alphafloor),
   TOKEN(ambush),
   TOKEN(angle),
   TOKEN(arg0),
   TOKEN(arg1),
   TOKEN(arg2),
   TOKEN(arg3),
   TOKEN(arg4),
   TOKEN(attachceiling),
   TOKEN(attachfloor),
   TOKEN(blockeverything),
   TOKEN(blockfloaters),
   TOKEN(blocking),
   TOKEN(blocklandmonsters),
   TOKEN(blockmonsters),
   TOKEN(blockplayers),
   TOKEN(blocksound),
   TOKEN(ceilingid),
   TOKEN(ceilingterrain),
   TOKEN(class1),
   TOKEN(class2),
   TOKEN(class3),
   TOKEN(clipmidtex),
   TOKEN(colormapbottom),
   TOKEN(colormapmid),
   TOKEN(colormaptop),
   TOKEN(coop),
   TOKEN(damage_endgodmode),
   TOKEN(damage_exitlevel),
   TOKEN(damageamount),
   TOKEN(damageinterval),
   TOKEN(damageterraineffect),
   TOKEN(damagetype),
   TOKEN(dm),
   TOKEN(dontdraw),
   TOKEN(dontpegbottom),
   TOKEN(dontpegtop),
   TOKEN(dormant),
   TOKEN(firstsideonly),
   TOKEN(floorid),
   TOKEN(floorterrain),
   TOKEN(friction),
   TOKEN(friend),
   TOKEN(health),
   TOKEN(height),
   TOKEN(heightceiling),
   TOKEN(heightfloor),
   TOKEN(id),
   TOKEN(impact),
   TOKEN(invisible),
   TOKEN(jumpover),
   TOKEN(leakiness),
   TOKEN(light),
   TOKEN(light_top),
   TOKEN(light_mid),
   TOKEN(light_bottom),
   TOKEN(lightabsolute),
   TOKEN(lightabsolute_top),
   TOKEN(lightabsolute_mid),
   TOKEN(lightabsolute_bottom),
   TOKEN(lightceiling),
   TOKEN(lightceilingabsolute),
   TOKEN(lightfloor),
   TOKEN(lightfloorabsolute),
   TOKEN(lightlevel),
   TOKEN(lightseqalt),
   TOKEN(lightsequence),
   TOKEN(lowerportal),
   TOKEN(mapped),
   TOKEN(midtex3d),
   TOKEN(midtex3dimpassible),
   TOKEN(missilecross),
   TOKEN(monstercross),
   TOKEN(monsterpush),
   TOKEN(monstershoot),
   TOKEN(monsteruse),
   TOKEN(offsetx),
   TOKEN(offsety),
   TOKEN(offsetx_bottom),
   TOKEN(offsety_bottom),
   TOKEN(offsetx_mid),
   TOKEN(offsety_mid),
   TOKEN(offsetx_top),
   TOKEN(offsety_top),
   TOKEN(polycross),
   TOKEN(portal),
   TOKEN(portalceiling),
   TOKEN(portal_ceil_attached),
   TOKEN(portal_ceil_blocksound),
   TOKEN(portal_ceil_disabled),
   TOKEN(portal_ceil_nopass),
   TOKEN(portal_ceil_norender),
   TOKEN(portal_ceil_overlaytype),
   TOKEN(portal_ceil_useglobaltex),
   TOKEN(portalfloor),
   TOKEN(portal_floor_attached),
   TOKEN(portal_floor_blocksound),
   TOKEN(portal_floor_disabled),
   TOKEN(portal_floor_nopass),
   TOKEN(portal_floor_norender),
   TOKEN(portal_floor_overlaytype),
   TOKEN(portal_floor_useglobaltex),
   TOKEN(passuse),
   TOKEN(phasedlight),
   TOKEN(playercross),
   TOKEN(playerpush),
   TOKEN(playeruse),
   TOKEN(renderstyle),
   TOKEN(repeatspecial),
   TOKEN(rotationceiling),
   TOKEN(rotationfloor),
   TOKEN(scroll_ceil_x),
   TOKEN(scroll_ceil_y),
   TOKEN(scroll_ceil_type),
   TOKEN(scroll_floor_x),
   TOKEN(scroll_floor_y),
   TOKEN(scroll_floor_type),
   TOKEN(secret),
   TOKEN(sector),
   TOKEN(sideback),
   TOKEN(sidefront),
   TOKEN(single),
   TOKEN(skew_bottom_type),
   TOKEN(skew_middle_type),
   TOKEN(skew_top_type),
   TOKEN(skill1),
   TOKEN(skill2),
   TOKEN(skill3),
   TOKEN(skill4),
   TOKEN(skill5),
   TOKEN(soundsequence),
   TOKEN(special),
   TOKEN(standing),
   TOKEN(strifeally),
   TOKEN(texturebottom),
   TOKEN(textureceiling),
   TOKEN(texturefloor),
   TOKEN(texturemiddle),
   TOKEN(texturetop),
   TOKEN(tranmap),
   TOKEN(translucent),
   TOKEN(twosided),
   TOKEN(type),
   TOKEN(upperportal),
   TOKEN(v1),
   TOKEN(v2),
   TOKEN(x),
   TOKEN(xpanningceiling),
   TOKEN(xpanningfloor),
   TOKEN(xscaleceiling),
   TOKEN(xscalefloor),
   TOKEN(y),
   TOKEN(ypanningceiling),
   TOKEN(ypanningfloor),
   TOKEN(yscaleceiling),
   TOKEN(yscalefloor),
   TOKEN(zoneboundary),
};

static EHashTable<keytoken_t, ENCStringHashKey, &keytoken_t::string, &keytoken_t::link> gTokenTable;

static void registerAllKeys()
{
   static bool called = false;
   if(called)
      return;
   for(size_t i = 0; i < earrlen(gTokenList); ++i)
      gTokenTable.addObject(gTokenList[i]);
   called = true;
}

//
// Looks for "ee_compat = true;" in the TEXTMAP in order to accept unknown name-
// spaces as Eternity-compatible. Useful to support arbitrary namespaces which
// look like Eternity but weren't made only for it. The resulting behaviour
// is like Eternity. Thanks to anotak for this feature.
//
bool UDMFParser::checkForCompatibilityFlag(qstring nstext)
{
   // ano - read over the file looking for `ee_compat="true"`
   readresult_e result;
   bool eecompatfound = false;
   // nstext needs to be copied, as contents of it gets overwritten before it's used.
   qstring nstextcopy(nstext);

   while((result = readItem()) != result_Eof)
   {
      if(result == result_Error)
      {
         mError = "UDMF error while checking unsupported namespace '";
         mError << nstextcopy;
         mError << "'";
         return false;
      }

      if(result == result_Assignment &&
         !mInBlock &&
         mKey.strCaseCmp("ee_compat") == 0 &&
         mValue.type == Token::type_Keyword &&
         ectype::toUpper(mValue.text[0]) == 'T')
      {
         eecompatfound = true;
         break; // while ((result = readItem()) != result_Eof)
      }
   } // while

   reset(); // make sure to reset the cursor after we find the field

   if(!eecompatfound)
   {
      mError = "Unsupported namespace '";
      mError << nstextcopy;
      mError << "'";
      return false;
   }

   mNamespace = namespace_Eternity;
   return true;
}

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

   // Set namespace
   if(!mValue.text.strCaseCmp("eternity"))
      mNamespace = namespace_Eternity;
   else if(!mValue.text.strCaseCmp("heretic"))
      mNamespace = namespace_Heretic;
   else if(!mValue.text.strCaseCmp("hexen"))
      mNamespace = namespace_Hexen;
   else if(!mValue.text.strCaseCmp("strife"))
      mNamespace = namespace_Strife;
   else if(!mValue.text.strCaseCmp("doom"))
      mNamespace = namespace_Doom;
   else if(!checkForCompatibilityFlag(mValue.text))
      return false;

   // Gamestuff. Must be null when out of block and only one be set when in
   // block
   ULinedef *linedef = nullptr;
   USidedef *sidedef = nullptr;
   uvertex_t *vertex = nullptr;
   USector *sector = nullptr;
   uthing_t *thing = nullptr;

   registerAllKeys();   // now it's the time

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
            linedef->renderstyle = RENDERSTYLE_translucent;
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
         {
            thing = &mThings.addNew();
            thing->health = 1.0;
         }
         continue;
      }
      if(result == result_Assignment && mInBlock)
      {

#define READ_NUMBER(obj, field) case t_##field: readNumber(obj->field); break
#define READ_BOOL(obj, field) case t_##field: readBool(obj->field); break
#define READ_STRING(obj, field) case t_##field: readString(obj->field); break
#define READ_FIXED(obj, field) case t_##field: readFixed(obj->field); break
#define REQUIRE_INT(obj, field, flag) case t_##field: requireInt(obj->field, obj->flag); break
#define REQUIRE_STRING(obj, field, flag) case t_##field: requireString(obj->field, obj->flag); break
#define REQUIRE_FIXED(obj, field, flag) case t_##field: requireFixed(obj->field, obj->flag); break

#define READ_ETERNITY_FIXED_ELSE_NUMBER(obj, field) case t_##field:\
   if(mNamespace == namespace_Eternity) \
         readFixed(obj->field); \
      else \
         readNumber(obj->field); \
   break

         const keytoken_t *kt = gTokenTable.objectForKey(mKey.constPtr());
         if(kt)
         {
            if(linedef)
            {
               switch(kt->token)
               {
                  case t_id: readNumber(linedef->identifier); break;
                  REQUIRE_INT(linedef, v1, v1set);
                  REQUIRE_INT(linedef, v2, v2set);
                  REQUIRE_INT(linedef, sidefront, sfrontset);
                  READ_NUMBER(linedef, sideback);
                  READ_BOOL(linedef, blocking);
                  READ_BOOL(linedef, blocklandmonsters);
                  READ_BOOL(linedef, blockmonsters);
                  READ_BOOL(linedef, blockplayers);
                  READ_BOOL(linedef, twosided);
                  READ_BOOL(linedef, dontpegtop);
                  READ_BOOL(linedef, dontpegbottom);
                  READ_BOOL(linedef, secret);
                  READ_BOOL(linedef, blocksound);
                  READ_BOOL(linedef, dontdraw);
                  READ_BOOL(linedef, mapped);
                  READ_BOOL(linedef, passuse);
                  READ_BOOL(linedef, translucent);
                  READ_BOOL(linedef, jumpover);
                  READ_BOOL(linedef, blockfloaters);
                  READ_NUMBER(linedef, special);
                  case t_arg0: readNumber(linedef->arg[0]); break;
                  case t_arg1: readNumber(linedef->arg[1]); break;
                  case t_arg2: readNumber(linedef->arg[2]); break;
                  case t_arg3: readNumber(linedef->arg[3]); break;
                  case t_arg4: readNumber(linedef->arg[4]); break;
                  READ_BOOL(linedef, playercross);
                  READ_BOOL(linedef, playeruse);
                  READ_BOOL(linedef, monstercross);
                  READ_BOOL(linedef, monsteruse);
                  READ_BOOL(linedef, impact);
                  READ_BOOL(linedef, monstershoot);
                  READ_BOOL(linedef, playerpush);
                  READ_BOOL(linedef, monsterpush);
                  READ_BOOL(linedef, missilecross);
                  READ_BOOL(linedef, repeatspecial);
                  READ_BOOL(linedef, polycross);

                  READ_BOOL(linedef, midtex3d);
                  READ_BOOL(linedef, midtex3dimpassible);
                  READ_BOOL(linedef, firstsideonly);
                  READ_BOOL(linedef, blockeverything);
                  READ_BOOL(linedef, zoneboundary);
                  READ_BOOL(linedef, clipmidtex);
                  READ_BOOL(linedef, lowerportal);
                  READ_BOOL(linedef, upperportal);
                  READ_NUMBER(linedef, portal);
                  READ_NUMBER(linedef, alpha);
                  READ_STRING(linedef, renderstyle);
                  READ_STRING(linedef, tranmap);
                  default:
                     break;
               }

            }
            else if(sidedef)
            {
               switch(kt->token)
               {
                  READ_ETERNITY_FIXED_ELSE_NUMBER(sidedef, offsetx);
                  READ_ETERNITY_FIXED_ELSE_NUMBER(sidedef, offsety);
                  READ_STRING(sidedef, texturetop);
                  READ_STRING(sidedef, texturebottom);
                  READ_STRING(sidedef, texturemiddle);
                  REQUIRE_INT(sidedef, sector, sset);
                  default:
                     break;
               }

               if(mNamespace == namespace_Eternity)
               {
                  switch(kt->token)
                  {
                     READ_FIXED(sidedef, offsetx_bottom);
                     READ_FIXED(sidedef, offsety_bottom);
                     READ_FIXED(sidedef, offsetx_mid);
                     READ_FIXED(sidedef, offsety_mid);
                     READ_FIXED(sidedef, offsetx_top);
                     READ_FIXED(sidedef, offsety_top);

                     READ_NUMBER(sidedef, light);
                     READ_NUMBER(sidedef, light_top);
                     READ_NUMBER(sidedef, light_mid);
                     READ_NUMBER(sidedef, light_bottom);
                     READ_BOOL(sidedef, lightabsolute);
                     READ_BOOL(sidedef, lightabsolute_top);
                     READ_BOOL(sidedef, lightabsolute_mid);
                     READ_BOOL(sidedef, lightabsolute_bottom);

                     READ_STRING(sidedef, skew_bottom_type);
                     READ_STRING(sidedef, skew_middle_type);
                     READ_STRING(sidedef, skew_top_type);
                  default:
                     break;
                  }
               }
            }
            else if(vertex)
            {
               if(kt->token == t_x)
                  requireFixed(vertex->x, vertex->xset);
               else if(kt->token == t_y)
                  requireFixed(vertex->y, vertex->yset);
            }
            else if(sector)
            {
               switch(kt->token)
               {
                  REQUIRE_STRING(sector, texturefloor, tfloorset);
                  REQUIRE_STRING(sector, textureceiling, tceilset);
                  READ_NUMBER(sector, lightlevel);
                  READ_NUMBER(sector, special);
                  case t_id:
                     readNumber(sector->identifier);
                     break;
                  READ_ETERNITY_FIXED_ELSE_NUMBER(sector, heightfloor);
                  READ_ETERNITY_FIXED_ELSE_NUMBER(sector, heightceiling);
                  default:
                     break;
               }
               if(mNamespace == namespace_Eternity)
               {
                  switch(kt->token)
                  {
                     READ_FIXED(sector, xpanningfloor);
                     READ_FIXED(sector, ypanningfloor);
                     READ_FIXED(sector, xpanningceiling);
                     READ_FIXED(sector, ypanningceiling);
                     READ_NUMBER(sector, xscaleceiling);
                     READ_NUMBER(sector, xscalefloor);
                     READ_NUMBER(sector, yscaleceiling);
                     READ_NUMBER(sector, yscalefloor);
                     READ_NUMBER(sector, rotationfloor);
                     READ_NUMBER(sector, rotationceiling);

                     READ_NUMBER(sector, scroll_ceil_x);
                     READ_NUMBER(sector, scroll_ceil_y);
                     READ_STRING(sector, scroll_ceil_type);

                     READ_NUMBER(sector, scroll_floor_x);
                     READ_NUMBER(sector, scroll_floor_y);
                     READ_STRING(sector, scroll_floor_type);

                     READ_BOOL(sector, secret);
                     READ_NUMBER(sector, friction);

                     READ_NUMBER(sector, lightfloor);
                     READ_NUMBER(sector, lightceiling);
                     READ_BOOL(sector, lightfloorabsolute);
                     READ_BOOL(sector, lightceilingabsolute);
                     READ_BOOL(sector, phasedlight);
                     READ_BOOL(sector, lightsequence);
                     READ_BOOL(sector, lightseqalt);

                     READ_STRING(sector, colormaptop);
                     READ_STRING(sector, colormapmid);
                     READ_STRING(sector, colormapbottom);

                     READ_NUMBER(sector, leakiness);
                     READ_NUMBER(sector, damageamount);
                     READ_NUMBER(sector, damageinterval);
                     READ_BOOL(sector, damage_endgodmode);
                     READ_BOOL(sector, damage_exitlevel);
                     READ_BOOL(sector, damageterraineffect);
                     READ_STRING(sector, damagetype);

                     READ_STRING(sector, floorterrain);
                     READ_STRING(sector, ceilingterrain);

                     READ_NUMBER(sector, floorid);
                     READ_NUMBER(sector, ceilingid);
                     READ_NUMBER(sector, attachfloor);
                     READ_NUMBER(sector, attachceiling);

                     READ_STRING(sector, soundsequence);

                     READ_STRING(sector, portal_floor_overlaytype);
                     READ_NUMBER(sector, alphafloor);
                     READ_BOOL(sector, portal_floor_blocksound);
                     READ_BOOL(sector, portal_floor_disabled);
                     READ_BOOL(sector, portal_floor_nopass);
                     READ_BOOL(sector, portal_floor_norender);
                     READ_BOOL(sector, portal_floor_useglobaltex);
                     READ_BOOL(sector, portal_floor_attached);

                     READ_STRING(sector, portal_ceil_overlaytype);
                     READ_NUMBER(sector, alphaceiling);
                     READ_BOOL(sector, portal_ceil_blocksound);
                     READ_BOOL(sector, portal_ceil_disabled);
                     READ_BOOL(sector, portal_ceil_nopass);
                     READ_BOOL(sector, portal_ceil_norender);
                     READ_BOOL(sector, portal_ceil_useglobaltex);
                     READ_BOOL(sector, portal_ceil_attached);

                     READ_NUMBER(sector, portalceiling);
                     READ_NUMBER(sector, portalfloor);
                     default:
                        break;
                  }
               }
            }
            else if(thing)
            {
               switch(kt->token)
               {
                  case t_id: readNumber(thing->identifier); break;
                  REQUIRE_FIXED(thing, x, xset);
                  REQUIRE_FIXED(thing, y, yset);
                  READ_FIXED(thing, height);
                  READ_NUMBER(thing, angle);
                  REQUIRE_INT(thing, type, typeset);
                  READ_BOOL(thing, skill1);
                  READ_BOOL(thing, skill2);
                  READ_BOOL(thing, skill3);
                  READ_BOOL(thing, skill4);
                  READ_BOOL(thing, skill5);
                  READ_BOOL(thing, ambush);
                  READ_BOOL(thing, single);
                  READ_BOOL(thing, dm);
                  READ_BOOL(thing, coop);
                  case t_friend:
                        readBool(thing->friendly);
                     break;
                  READ_BOOL(thing, dormant);
                  READ_BOOL(thing, class1);
                  READ_BOOL(thing, class2);
                  READ_BOOL(thing, class3);
                  READ_BOOL(thing, standing);
                  READ_BOOL(thing, strifeally);
                  READ_BOOL(thing, translucent);
                  READ_BOOL(thing, invisible);
                  READ_NUMBER(thing, special);
                  case t_arg0:
                        readNumber(thing->arg[0]);
                     break;
                  case t_arg1:
                        readNumber(thing->arg[1]);
                     break;
                  case t_arg2:
                        readNumber(thing->arg[2]);
                     break;
                  case t_arg3:
                        readNumber(thing->arg[3]);
                     break;
                  case t_arg4:
                        readNumber(thing->arg[4]);
                     break;
                  default:
                     break;
               }
               if(mNamespace == namespace_Eternity)
               {
                  switch(kt->token)
                  {
                     READ_NUMBER(thing, health);
                     default:
                        break;
                  }
               }
            }
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
// Returns the level info map format from the namespace, chiefly for linedef
// specials
//
int UDMFParser::getMapFormat() const
{
   switch(mNamespace)
   {
      case namespace_Doom:
      case namespace_Heretic:
      case namespace_Strife:
         // Just use the normal doom linedefs
         return LEVEL_FORMAT_DOOM;
      case namespace_Eternity:
         return LEVEL_FORMAT_UDMF_ETERNITY;
      case namespace_Hexen:
         return LEVEL_FORMAT_HEXEN;
      default:
         return LEVEL_FORMAT_INVALID; // Unsupported namespace
   }
}

//
// Loads a new TEXTMAP and clears all variables
//
void UDMFParser::setData(const char *data, size_t size)
{
   mData.copy(data, size);
   reset();
}

//
// Resets variables to defaults
//
void UDMFParser::reset()
{
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
// Passes a fixed_t
//
void UDMFParser::readFixed(fixed_t &target) const
{
   if(mValue.type == Token::type_Number)
      target = M_DoubleToFixed(mValue.number);
}

//
// Passes a float to an object and flags a required element
//
void UDMFParser::requireFixed(fixed_t &target,
                              bool &flagtarget) const
{
   if(mValue.type == Token::type_Number)
   {
      target = M_DoubleToFixed(mValue.number);
      flagtarget = true;
   }
}

//
// Requires an int
//
void UDMFParser::requireInt(int &target, bool &flagtarget) const
{
   if(mValue.type == Token::type_Number)
   {
      target = static_cast<int>(mValue.number);
      flagtarget = true;
   }
}

//
// Reads a string
//
void UDMFParser::readString(qstring &target) const
{
   if(mValue.type == Token::type_String)
      target = mValue.text;
}

//
// Passes a string
//
void UDMFParser::requireString(qstring &target, bool &flagtarget) const
{
   if(mValue.type == Token::type_String)
   {
      target = mValue.text;
      flagtarget = true;
   }
}

//
// Passes a boolean
//
void UDMFParser::readBool(bool &target) const
{
   if(mValue.type == Token::type_Keyword)
   {
      target = ectype::toUpper(mValue.text[0]) == 'T';
   }
}

//
// Passes a number (float/double/int)
//
template<typename T>
void UDMFParser::readNumber(T &target) const
{
   if(mValue.type == Token::type_Number)
   {
      target = static_cast<T>(mValue.number);
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
         while(mPos + 1 < size && (mData[mPos] != '*' || mData[mPos + 1] != '/'))
            addPos(1);
         if(mPos + 1 >= size)
         {
            addPos(1);
            return false;
         }
         if(mData[mPos] == '*' && mData[mPos + 1] == '/')
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
void UDMFParser::addPos(size_t amount)
{
   for(size_t i = 0; i < amount; i++)
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

