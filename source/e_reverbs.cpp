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
//----------------------------------------------------------------------------
//
// EDF Reverb Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#define NEED_EDF_DEFINITIONS
#include "Confuse/confuse.h"

#include "e_edf.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_reverbs.h"
#include "m_compare.h"

//=============================================================================
//
// Hash Lookup
//

static EHashTable<ereverb_t, EIntHashKey, &ereverb_t::id, &ereverb_t::links> hash;

//
// E_addReverb
//
// Add a reverb to the hash table.
//
static void E_addReverb(ereverb_t *reverb)
{
   hash.addObject(reverb);
}

//
// E_ReverbForID
//
// Look up a reverb using its two separate id numbers as used in maps and 
// ExtraData.
//
ereverb_t *E_ReverbForID(int id1, int id2)
{
   int id = (id1 << 8) | id2;
   return hash.objectForKey(id);
}

//
// E_ReverbForID
//
// Overload accepting a pre-combined ID value.
//
ereverb_t *E_ReverbForID(int id)
{
   return hash.objectForKey(id);
}

//=============================================================================
// 
// EDF Processing
//

constexpr const char ITEM_TP_ID1[] = "id1";
constexpr const char ITEM_TP_ID2[] = "id2";

constexpr const char ITEM_REVERB_ROOMSIZE[]  = "roomsize";
constexpr const char ITEM_REVERB_DAMPING[]   = "damping";
constexpr const char ITEM_REVERB_WETSCALE[]  = "wetscale";
constexpr const char ITEM_REVERB_DRYSCALE[]  = "dryscale";
constexpr const char ITEM_REVERB_WIDTH[]     = "width";
constexpr const char ITEM_REVERB_PREDELAY[]  = "predelay";
constexpr const char ITEM_REVERB_EQUALIZED[] = "equalized";
constexpr const char ITEM_REVERB_EQLOWFREQ[] = "eq.lowfreq";
constexpr const char ITEM_REVERB_EQHIFREQ[]  = "eq.highfreq";
constexpr const char ITEM_REVERB_EQLOWGAIN[] = "eq.lowgain";
constexpr const char ITEM_REVERB_EQMIDGAIN[] = "eq.midgain";
constexpr const char ITEM_REVERB_EQHIGAIN[]  = "eq.highgain";

// titleprops subsection
static cfg_opt_t titleprops[] =
{
   CFG_INT(ITEM_TP_ID1, 0, CFGF_NONE),
   CFG_INT(ITEM_TP_ID2, 0, CFGF_NONE),
   CFG_END()
};

// EDF reverb options array
cfg_opt_t edf_reverb_opts[] =
{
   CFG_TPROPS(titleprops, CFGF_NOCASE),

   CFG_FLOAT(ITEM_REVERB_ROOMSIZE,  0.5,     CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_DAMPING,   0.5,     CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_WETSCALE,  1.0/3.0, CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_DRYSCALE,  0.0,     CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_WIDTH,     1.0,     CFGF_NONE),

   CFG_INT(ITEM_REVERB_PREDELAY,    0,       CFGF_NONE),
   
   CFG_FLAG(ITEM_REVERB_EQUALIZED,  0,       CFGF_SIGNPREFIX),
   
   CFG_FLOAT(ITEM_REVERB_EQLOWFREQ, 250.0,   CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_EQHIFREQ,  4000.0,  CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_EQLOWGAIN, 1.0,     CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_EQMIDGAIN, 1.0,     CFGF_NONE),
   CFG_FLOAT(ITEM_REVERB_EQHIGAIN,  1.0,     CFGF_NONE),
   
   CFG_END()
};


//
// E_getFloatAndClamp
//
// Get a floating point property from the cfg_t and clamp it into a given
// range inclusive.
//
static void E_getFloatAndClamp(cfg_t *const cfg, const char *name, double &dest,
                               double min, double max, const bool def)
{
   // Test if the item is defined explicitly, or the default should be gotten
   const auto IS_SET = [cfg, def](const char *const s) -> bool {
      return def || cfg_size((cfg), (s)) > 0;
   };

   if(IS_SET(name))
   {
      dest = cfg_getfloat(cfg, name);
      dest = eclamp(dest, min, max);
   }
}

// This built-in default environment reverb is a "non-reverb"; it is not
// marked as being "enabled" which means that the sound engine will skip the
// effect processing pass entirely while it is the active environment. It
// has ID 0 0, which is reserved.
static ereverb_t e_defaultEnvironment;

//
// E_GetDefaultReverb
//
// Get the built-in default no-op reverb environment.
//
ereverb_t *E_GetDefaultReverb()
{
   return &e_defaultEnvironment;
}

//
// E_initDefaultEnvironment
//
// haleyjd 01/12/14: One-time setup of the default environment object.
//
static void E_initDefaultEnvironment()
{
   static bool firsttime = true;

   if(!firsttime)
      return;
   firsttime = false;

   E_addReverb(&e_defaultEnvironment);
}

//
// E_processReverb
//
// Process a single EDF reverb definition.
//
static void E_processReverb(cfg_t *const sec)
{
   bool        def    = false;
   const char *title  = cfg_title(sec);
   cfg_t      *tprops = cfg_gettitleprops(sec);

   // Test if the item is defined explicitly, or the default should be gotten
   const auto IS_SET = [sec, &def](const char *const s) -> bool {
      return def || cfg_size((sec), (s)) > 0;
   };

   // get ID components
   int id1 = cfg_getint(tprops, ITEM_TP_ID1);
   int id2 = cfg_getint(tprops, ITEM_TP_ID2);

   // range checking
   if(id1 < 0 || id1 > 255)
   {
      E_EDFLoggedWarning(2, "Invalid id1 %d for reverb %s, ignoring.\n", id1, title);
      return;
   }
   if(id2 < 0 || id2 > 255)
   {
      E_EDFLoggedWarning(2, "Invalid id2 %d for reverb %s, ignoring\n", id2, title);
      return;
   }
   if(id1 == 0 && id2 == 0) // 0 0 is a reserved ID for the default environment
   {
      E_EDFLoggedWarning(2, "ID 0 0 for reverb %s is reserved, ignoring\n", title);
      return;
   }

   // if it already exists, edit it; otherwise, create a new one
   ereverb_t *newReverb;
   if(!(newReverb = E_ReverbForID(id1, id2)))
   {
      newReverb = estructalloc(ereverb_t, 1);
      newReverb->flags |= REVERB_ENABLED;
      newReverb->id = (id1 << 8) | id2;
      E_addReverb(newReverb);
      def = true;
   }

   E_getFloatAndClamp(sec, ITEM_REVERB_ROOMSIZE,  newReverb->roomsize,  0.0, 1.07143, def);
   E_getFloatAndClamp(sec, ITEM_REVERB_DAMPING,   newReverb->dampening, 0.0, 1.0,     def);
   E_getFloatAndClamp(sec, ITEM_REVERB_WETSCALE,  newReverb->wetscale,  0.0, 1.0,     def);
   E_getFloatAndClamp(sec, ITEM_REVERB_DRYSCALE,  newReverb->dryscale,  0.0, 1.0,     def);
   E_getFloatAndClamp(sec, ITEM_REVERB_WIDTH,     newReverb->width,     0.0, 1.0,     def);

   // predelay
   if(IS_SET(ITEM_REVERB_PREDELAY))
   {
      newReverb->predelay = cfg_getint(sec, ITEM_REVERB_PREDELAY);
      newReverb->predelay = eclamp(newReverb->predelay, 0, 250);
   }

   // equalization enabled
   if(IS_SET(ITEM_REVERB_EQUALIZED))
   {
      if(!!cfg_getflag(sec, ITEM_REVERB_EQUALIZED))
         newReverb->flags |= REVERB_EQUALIZED;
      else
         newReverb->flags &= ~REVERB_EQUALIZED;
   }

   E_getFloatAndClamp(sec, ITEM_REVERB_EQLOWFREQ, newReverb->eqlowfreq,  20.0, 20000.0, def);
   E_getFloatAndClamp(sec, ITEM_REVERB_EQHIFREQ,  newReverb->eqhighfreq, 20.0, 20000.0, def);
   E_getFloatAndClamp(sec, ITEM_REVERB_EQLOWGAIN, newReverb->eqlowgain,   0.0,     1.5, def);
   E_getFloatAndClamp(sec, ITEM_REVERB_EQMIDGAIN, newReverb->eqmidgain,   0.0,     1.5, def);
   E_getFloatAndClamp(sec, ITEM_REVERB_EQHIGAIN,  newReverb->eqhighgain,  0.0,     1.5, def);

   E_EDFLogPrintf("\t\t* Finished reverb definition %s (%d %d)\n", title, id1, id2);
}

void E_ProcessReverbs(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Processing reverb definitions\n");
   unsigned int numreverbs = cfg_size(cfg, EDF_SEC_REVERB);

   // add the default environment if it hasn't been added already
   E_initDefaultEnvironment();

   for(unsigned int i = 0; i < numreverbs; i++)
      E_processReverb(cfg_getnsec(cfg, EDF_SEC_REVERB, i));
}

// EOF

