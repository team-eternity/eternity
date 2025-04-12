//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: EDF animation definitions
// Authors: Ioan Chera
//

#include "z_zone.h"
#include "d_dehtbl.h"
#include "e_anim.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_lib.h"
#include "m_qstrkeys.h"
#include "r_ripple.h"

constexpr const char ITEM_ANIM_FLAT[]   = "flat";
constexpr const char ITEM_ANIM_WALL[]   = "wall";
constexpr const char ITEM_ANIM_ENDPIC[] = "lastpic";
constexpr const char ITEM_ANIM_TICS[]   = "tics";
constexpr const char ITEM_ANIM_FLAGS[]  = "flags";

constexpr const char ITEM_ANIM_PIC[]    = "pic";

constexpr const char ITEM_PIC_NAME[]  = "name";
constexpr const char ITEM_PIC_TICS[]  = "tics";
constexpr const char ITEM_PIC_RAND[]  = "random";
constexpr const char ITEM_PIC_FLAGS[] = "flags";

constexpr const char ITEM_RAND_MIN[] = "min";
constexpr const char ITEM_RAND_MAX[] = "max";

constexpr int NUMANIMCHAINS = 67;

//
// Random has two parameters
//
static cfg_opt_t random_opts[] =
{
   CFG_INT("min", 1, CFGF_NONE),
   CFG_INT("max", 1, CFGF_NONE),
   CFG_END()
};

//
// Hexen-style pic entry options
//
static cfg_opt_t picentry_opts[] =
{
   CFG_STR(ITEM_PIC_NAME,    "",          CFGF_NONE  ),
   CFG_INT(ITEM_PIC_TICS,    0,           CFGF_NONE  ),
   CFG_MVPROP(ITEM_PIC_RAND, random_opts, CFGF_NOCASE),
   CFG_STR(ITEM_PIC_FLAGS,   "",          CFGF_NONE  ),
   CFG_END()
};

//
// The EDF animation property
//
cfg_opt_t edf_anim_opts[] =
{
   CFG_STR(ITEM_ANIM_FLAT,   "",            CFGF_NONE),
   CFG_STR(ITEM_ANIM_WALL,   "",            CFGF_NONE),
   CFG_STR(ITEM_ANIM_ENDPIC, "",            CFGF_NONE),
   CFG_INT(ITEM_ANIM_TICS,   8,             CFGF_NONE),
   CFG_STR(ITEM_ANIM_FLAGS,  "",            CFGF_NONE),
   CFG_SEC(ITEM_ANIM_PIC,    picentry_opts, CFGF_MULTI|CFGF_NOCASE),
   CFG_END()
};

//
// The flags
//
static dehflags_t anim_flags[] =
{
   { "SWIRL", EAnimDef::SWIRL },
   { nullptr, 0               }
};

//
// Animation texture flags
//
static dehflagset_t anim_flagset =
{
   anim_flags,
   0,
};

// the animations defined
PODCollection<EAnimDef *> eanimations;

// The collection, for reusable stuff
static EHashTable<EAnimDef, ENCQStrHashKey,
                  &EAnimDef::startpic, &EAnimDef::link> e_anim_namehash(NUMANIMCHAINS);

//
// Resets a pic
//
void EAnimDef::Pic::reset()
{
   name.clear();
   offset  = 0;
   ticsmin = 0;
   ticsmax = 0;
   flags   = 0;
}

//
// Resets the type
//
void EAnimDef::reset(type_t intype)
{
   type     = intype;
   startpic = endpic = "";
   tics     = 0;
   flags    = 0;
   pics.clear();
}

//
// Processes an animation
//
static void E_processAnimation(cfg_t *cfg)
{
   EAnimDef::type_t type;
   const char      *firstpic;
   bool             modified = true;
   if(cfg_size(cfg, ITEM_ANIM_FLAT) >= 1)
   {
      type = EAnimDef::type_flat;
      firstpic = cfg_getstr(cfg, ITEM_ANIM_FLAT);
   }
   else if(cfg_size(cfg, ITEM_ANIM_WALL) >= 1)
   {
      type = EAnimDef::type_wall;
      firstpic = cfg_getstr(cfg, ITEM_ANIM_WALL);
   }
   else
   {
      E_EDFLoggedWarning(2, "Missing 'flat' or 'wall' in EDF animdef.\n");
      return;
   }
   EAnimDef *def = nullptr;
   do // repeat until you find one that's also the same kind
   {
      def = e_anim_namehash.keyIterator(def, firstpic);
   } while(def && def->type != type);
   if(!def)
   {
      def           = new EAnimDef;
      def->startpic = firstpic;
      def->type     = type;
      e_anim_namehash.addObject(def);
      eanimations.add(def);
      modified = false;
   }

   if(cfg_size(cfg, ITEM_ANIM_FLAGS) > 0)
      def->flags = E_ParseFlags(cfg_getstr(cfg, ITEM_ANIM_FLAGS), &anim_flagset);

   unsigned seqsize = cfg_size(cfg, ITEM_ANIM_PIC);
   if(seqsize >= 1)
   {
      // clear incompatible alternative
      def->endpic.clear();
      def->tics = 0;
      def->pics.clear();   // prepare to overwrite it
      for(unsigned i = 0; i < seqsize; ++i)
      {
         cfg_t      *pic     = cfg_getnsec(cfg, ITEM_ANIM_PIC, i);
         const char *picname = cfg_getstr(pic, ITEM_PIC_NAME);
         if(estrempty(picname))
         {
            E_EDFLoggedWarning(2, "Invalid pic in animation %s\n", firstpic);
            continue;   // invalid
         }
         char *endptr = nullptr;
         long offset  = strtol(picname, &endptr, 0);
         EAnimDef::Pic &newpic = def->pics.addNew();
         if(*endptr)
         {
            newpic.name = picname;
            offset      = 0;   // offset not valid if it's not a valid number
         }
         else
            newpic.name = "";
         newpic.offset = static_cast<int>(offset);
         if(cfg_size(pic, ITEM_PIC_RAND) >= 1)
         {
            cfg_t *rtime   = cfg_getmvprop(pic, ITEM_PIC_RAND);
            newpic.ticsmin = cfg_getint(rtime, ITEM_RAND_MIN);
            newpic.ticsmax = cfg_getint(rtime, ITEM_RAND_MAX);
         }
         else if(cfg_size(pic, ITEM_PIC_TICS))
         {
            newpic.ticsmin = cfg_getint(pic, ITEM_PIC_TICS);
            newpic.ticsmax = 0;  // less than min means fixed time
         }
         else  // otherwise get the base definition if available
            newpic.ticsmin = cfg_getint(cfg, ITEM_PIC_TICS);
         newpic.flags = E_ParseFlags(cfg_getstr(pic, ITEM_PIC_FLAGS),
                                     &anim_flagset);
      }
      return;
   }
   // Doom style
   if(cfg_size(cfg, ITEM_ANIM_ENDPIC) >= 1)
   {
      // clear hexen definition if we see doom-style replacement
      def->pics.clear();
      def->endpic = cfg_getstr(cfg, ITEM_ANIM_ENDPIC);
   }
   if(!modified || cfg_size(cfg, ITEM_ANIM_TICS) >= 1)
      def->tics = cfg_getint(cfg, ITEM_ANIM_TICS);
}

//
// Processes all animations from EDF
//
void E_ProcessAnimations(cfg_t *cfg)
{
   unsigned numanims = cfg_size(cfg, EDF_SEC_ANIMATION);
   E_EDFLogPrintf("\t* Processing animations\n"
                  "\t\t%u animation(s) defined\n", numanims);
   for(unsigned i = 0; i < numanims; ++i)
      E_processAnimation(cfg_getnsec(cfg, EDF_SEC_ANIMATION, i));
}

//
// Adds an animation to EDF if defined from external sources
//
void E_AddAnimation(const EAnimDef &extdef)
{
   if(extdef.startpic.empty())
      return;
   const char *title = extdef.startpic.constPtr();
   EAnimDef   *def   = nullptr;
   do
   {
      def = e_anim_namehash.keyIterator(def, title);
   } while(def && def->type != extdef.type);
   if(def)
      return;

   def = new EAnimDef(extdef);
   e_anim_namehash.addObject(def);
   eanimations.add(def);
      E_EDFLogPrintf("\t\tDefined animation %s from ANIMDEFS\n", title);
}

//
// Returns true if given startpic is for a Hexen animation
//
bool E_IsHexenAnimation(const char *startpic, EAnimDef::type_t type)
{
   EAnimDef *def = nullptr;
   do
   {
      def = e_anim_namehash.keyIterator(def, startpic);
   } while(def && def->type != type);
   return def && def->pics.getLength() >= 1;
}

// EOF
