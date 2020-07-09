// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// EDF Sound Module
//
// Maintains the globally-used sound hash tables, for lookup by
// assigned mnemonics and DeHackEd numbers. EDF-defined sounds are
// processed and linked into the tables first. Any wad lumps with
// names starting with DS* are later added as the wads that contain
// them are loaded.
//
// Note that wad sounds can't be referred to via DeHackEd, which
// hasn't changed since this functionality was implemented in s_sound.c.
// The functions in s_sound.c for sound hashing now call down to these
// functions.
//
// 05/28/06: Sound Sequences, Ambience
//
// This module also now contains EDF sound sequence code. Sound sequences are
// miniature scripts that determine how sectors and polyobjects play sounds.
// Also included are ambience objects that work with mapthings 14001-14065 to
// provide a generalized ambient sound engine.
//
// By James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "e_sound.h"

#include "d_io.h"
#include "d_dehtbl.h"
#include "i_sound.h"
#include "m_utils.h"
#include "p_mobj.h"
#include "p_skin.h"
#include "s_sndseq.h"
#include "s_sound.h"
#include "w_wad.h"
#include "xl_sndinfo.h"

//
// Sound keywords
//
#define ITEM_SND_LUMP          "lump"
#define ITEM_SND_PREFIX        "prefix"
#define ITEM_SND_SINGULARITY   "singularity"
#define ITEM_SND_PRIORITY      "priority"
#define ITEM_SND_LINK          "link"
#define ITEM_SND_ALIAS         "alias"
#define ITEM_SND_RANDOM        "random"
#define ITEM_SND_SKININDEX     "skinindex"
#define ITEM_SND_LINKVOL       "linkvol"
#define ITEM_SND_LINKPITCH     "linkpitch"
#define ITEM_SND_CLIPPING_DIST "clipping_dist"
#define ITEM_SND_CLOSE_DIST    "close_dist"
#define ITEM_SND_PITCHVAR      "pitchvariance"
#define ITEM_SND_SUBCHANNEL    "subchannel"
#define ITEM_SND_PCSLUMP       "pcslump"
#define ITEM_SND_NOPCSOUND     "nopcsound"
#define ITEM_SND_DEHNUM        "dehackednum"

#define ITEM_DELTA_NAME "name"

//
// Static sound hash tables
//
#define NUMSFXCHAINS 307
static sfxinfo_t             *sfxchains[NUMSFXCHAINS];
static DLListItem<sfxinfo_t> *sfx_dehchains[NUMSFXCHAINS];

//
// Singularity types
//
// This must reflect the enumeration in sounds.h
//
static const char *singularities[] =
{
   "sg_none",
   "sg_itemup",
   "sg_wpnup",
   "sg_oof",
   "sg_getpow",
};

#define NUM_SINGULARITIES (sizeof(singularities) / sizeof(char *))

//
// Skin sound indices
//
// This must reflect the enumeration in p_skin.h, with the
// exception of the addition of "sk_none" to bump up everything
// by one and to provide for a mnemonic for value zero.
//
static const char *skinindices[NUMSKINSOUNDS + 1] =
{
   "sk_none", // Note that sfxinfo stores the true index + 1
   "sk_plpain",
   "sk_pdiehi",
   "sk_oof",
   "sk_slop",
   "sk_punch",
   "sk_radio",
   "sk_pldeth",
   "sk_plfall",
   "sk_plfeet",
   "sk_fallht",
   "sk_plwdth",
   "sk_noway",
   "sk_jump",
};

#define NUM_SKININDICES (sizeof(skinindices) / sizeof(const char *))

//
// Pitch variance types
//
static const char *pitchvars[] =
{
   "none",
   "Doom",
   "DoomSaw",
   "Heretic",
   "HereticAmbient"
};

#define NUM_PITCHVARS (sizeof(pitchvars) / sizeof(const char *))

//
// Subchannel types
//
static const char *subchans[] =
{
   "Auto",
   "Weapon",
   "Voice",
   "Item",
   "Body",
   "SoundSlot5",
   "SoundSlot6",
   "SoundSlot7"
};

#define NUM_SUBCHANS (sizeof(subchans) / sizeof(const char *))

#define SOUND_OPTIONS \
   CFG_STR(ITEM_SND_LUMP,          nullptr,           CFGF_NONE), \
   CFG_BOOL(ITEM_SND_PREFIX,       true,              CFGF_NONE), \
   CFG_STR(ITEM_SND_SINGULARITY,   "sg_none",         CFGF_NONE), \
   CFG_INT(ITEM_SND_PRIORITY,      64,                CFGF_NONE), \
   CFG_STR(ITEM_SND_LINK,          "none",            CFGF_NONE), \
   CFG_STR(ITEM_SND_ALIAS,         "none",            CFGF_NONE), \
   CFG_STR(ITEM_SND_RANDOM,        0,                 CFGF_LIST), \
   CFG_STR(ITEM_SND_SKININDEX,     "sk_none",         CFGF_NONE), \
   CFG_INT(ITEM_SND_LINKVOL,       -1,                CFGF_NONE), \
   CFG_INT(ITEM_SND_LINKPITCH,     -1,                CFGF_NONE), \
   CFG_INT(ITEM_SND_CLIPPING_DIST, S_CLIPPING_DIST_I, CFGF_NONE), \
   CFG_INT(ITEM_SND_CLOSE_DIST,    S_CLOSE_DIST_I,    CFGF_NONE), \
   CFG_STR(ITEM_SND_PITCHVAR,      "none",            CFGF_NONE), \
   CFG_STR(ITEM_SND_SUBCHANNEL,    "Auto",            CFGF_NONE), \
   CFG_STR(ITEM_SND_PCSLUMP,       nullptr,           CFGF_NONE), \
   CFG_BOOL(ITEM_SND_NOPCSOUND,    false,             CFGF_NONE), \
   CFG_INT(ITEM_SND_DEHNUM,        -1,                CFGF_NONE), \
   CFG_END()

//
// Sound cfg options array (used in e_edf.c)
//
cfg_opt_t edf_sound_opts[] =
{
   SOUND_OPTIONS
};

cfg_opt_t edf_sdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, nullptr, CFGF_NONE),
   SOUND_OPTIONS
};

//
// E_SoundForName
//
// Returns a sfxinfo_t pointer given the EDF mnemonic for that
// sound. Will return nullptr if the requested sound is not found.
//
sfxinfo_t *E_SoundForName(const char *name)
{
   unsigned int hash = D_HashTableKey(name) % NUMSFXCHAINS;
   sfxinfo_t  *rover = sfxchains[hash];

   while(rover && strcasecmp(name, rover->mnemonic))
      rover = rover->next;

   return rover;
}

//
// E_EDFSoundForName
//
// This version returns a pointer to NullSound if the special
// mnemonic "none" is passed in. This allows EDF to get the reserved
// DeHackEd number zero for it. Most other code segments should not
// use this function.
//
sfxinfo_t *E_EDFSoundForName(const char *name)
{
   if(!strcasecmp(name, "none"))
      return &NullSound;

   return E_SoundForName(name);
}

//
// E_SoundForDEHNum
//
// Returns a sfxinfo_t pointer given the DeHackEd number for that
// sound. Will return nullptr if the requested sound is not found.
//
sfxinfo_t *E_SoundForDEHNum(int dehnum)
{
   unsigned int hash = dehnum % NUMSFXCHAINS;
   DLListItem<sfxinfo_t> *rover = sfx_dehchains[hash];

   // haleyjd 04/13/08: rewritten for dynamic hash chains
   while(rover && (*rover)->dehackednum != dehnum)
      rover = rover->dllNext;

   return rover ? rover->dllObject : nullptr;
}

//
// E_AddSoundToHash
//
// Adds a new sfxinfo_t structure to the name hash table.
//
static void E_AddSoundToHash(sfxinfo_t *sfx)
{
   // compute the hash code using the sound mnemonic
   unsigned int hash;

   // make sure it doesn't exist already -- if it does, this
   // insertion must be ignored
   if(E_EDFSoundForName(sfx->mnemonic))
      return;

   hash = D_HashTableKey(sfx->mnemonic) % NUMSFXCHAINS;

   // link it in
   sfx->next = sfxchains[hash];
   sfxchains[hash] = sfx;
}

//
// E_AddSoundToDEHHash
//
// Only used locally. This adds a sound to the DeHackEd number hash
// table, so that both old and new sounds can be referred to by
// use of a number. This avoids major code rewrites and compatibility
// issues. It also naturally extends DeHackEd, too.
//
static void E_AddSoundToDEHHash(sfxinfo_t *sfx)
{
   unsigned int hash = sfx->dehackednum % NUMSFXCHAINS;

#ifdef RANGECHECK
   if(sfx->dehackednum < 0)
      I_Error("E_AddSoundToDEHHash: internal error - dehnum == -1\n");
#endif

   if(sfx->dehackednum == 0)
      E_EDFLoggedErr(2, "E_AddSoundToDEHHash: dehackednum zero is reserved!\n");

   // haleyjd 04/13/08: use M_DLListInsert to support removal & reinsertion
   sfx->numlinks.insert(sfx, &sfx_dehchains[hash]);
}

//
// E_DelSoundFromDEHHash
//
// haleyjd 04/13/08: Made sound dehackednum hash chains dynamically linked
// to fully support DeHackEd number specification in sounds added after
// primary EDF processing.
//
static void E_DelSoundFromDEHHash(sfxinfo_t *sfx)
{
   sfx->numlinks.remove();
}

//
// E_FindSoundForDEH
//
// haleyjd 04/13/08: There's a hackish little code segment in DeHackEd
// that looks for a sound name match to a DeHackEd text string. Now that
// the S_sfx table is gone, we need to do this search here, using the
// mnemonic hash table chains.
//
sfxinfo_t *E_FindSoundForDEH(char *inbuffer, unsigned int fromlen)
{
   // run down all the mnemonic hash chains
   for(sfxinfo_t *cursfx : sfxchains)
   {
      while(cursfx)
      {
         // avoid short prefix erroneous match
         if(strlen(cursfx->mnemonic) == fromlen &&
            !strncasecmp(cursfx->mnemonic, inbuffer, fromlen))
            return cursfx;

         cursfx = cursfx->next;
      }
   }

   // no match
   return nullptr;
}


// haleyjd 03/22/06: automatic dehnum allocation
//
// Automatic allocation of dehacked numbers allows sounds to be used with
// parameterized codepointers without having had a DeHackEd number explicitly
// assigned to them by the EDF author. This was requested by several users
// after v3.33.02.
//

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_sound_dehnum = D_MAXINT;

bool E_AutoAllocSoundDEHNum(sfxinfo_t *sfx)
{
   int dehnum;

#ifdef RANGECHECK
   if(sfx->dehackednum != -1)
      I_Error("E_AutoAllocSoundDEHNum: called for sound with valid dehnum\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_sound_dehnum <= 0)
      return false;

   do
   {
      dehnum = edf_alloc_sound_dehnum--;
   }
   while(dehnum > 0 && E_SoundForDEHNum(dehnum) != nullptr);

   // ran out while searching for an unused number?
   if(dehnum <= 0)
      return false;

   // assign it!
   sfx->dehackednum = dehnum;

   E_AddSoundToDEHHash(sfx);

   return true;
}


//
// E_NewWadSound
//
// Creates a sfxinfo_t structure for a new wad sound and hashes it.
//
sfxinfo_t *E_NewWadSound(const char *name)
{
   sfxinfo_t *sfx;
   char mnemonic[16];

   memset(mnemonic, 0, sizeof(mnemonic));
   strncpy(mnemonic, name+2, 9);

   sfx = E_EDFSoundForName(mnemonic);

   if(!sfx)
   {
      // create a new one and hook into hashchain
      sfx = ecalloc(sfxinfo_t *, 1, sizeof(sfxinfo_t));

      strncpy(sfx->name, name, 9);
      strncpy(sfx->mnemonic, mnemonic, 9);

      sfx->flags         = SFXF_WAD;        // born as implicit wad sound
      sfx->priority      = 64;
      sfx->pitch         = -1;
      sfx->volume        = -1;
      sfx->clipping_dist = S_CLIPPING_DIST;
      sfx->close_dist    = S_CLOSE_DIST;
      sfx->dehackednum   = -1;              // not accessible to DeHackEd

      E_AddSoundToHash(sfx);
   }

   return sfx;
}

//
// E_NewSndInfoSound
//
// haleyjd 03/27/11:
// Creates a sfxinfo_t for a SNDINFO entry.
//
sfxinfo_t *E_NewSndInfoSound(const char *mnemonic, const char *name)
{
   sfxinfo_t *sfx;

   // create a new one and hook into hashchain
   sfx = ecalloc(sfxinfo_t *, 1, sizeof(sfxinfo_t));

   strncpy(sfx->name,     name,      9);
   strncpy(sfx->mnemonic, mnemonic, sizeof(sfx->mnemonic));

   sfx->flags         = SFXF_SNDINFO;    // born via SNDINFO
   sfx->priority      = 64;
   sfx->pitch         = -1;
   sfx->volume        = -1;
   sfx->clipping_dist = S_CLIPPING_DIST;
   sfx->close_dist    = S_CLOSE_DIST;
   sfx->dehackednum   = -1;              // not accessible to DeHackEd

   E_AddSoundToHash(sfx);

   return sfx;
}

//
// E_PreCacheSounds
//
// Runs down the sound mnemonic hash table chains and caches all
// sounds. This is improved from the code that was in SMMU, which
// only precached entries in the S_sfx array. This is called at
// startup when sound precaching is enabled.
//
void E_PreCacheSounds()
{
   // run down all the mnemonic hash chains so that we precache
   // all sounds, not just ones stored in S_sfx
   for(sfxinfo_t *cursfx : sfxchains)
   {
      while(cursfx)
      {
         I_CacheSound(cursfx);
         cursfx = cursfx->next;
      }
   }
}

//
// E_UpdateSoundCache
//
// haleyjd 03/28/10: For loading EDF definitions at runtime, we need to purge
// the sound cache.
//
void E_UpdateSoundCache()
{
   // be sure all sounds are stopped
   S_StopSounds(true);

   for(sfxinfo_t *cursfx : sfxchains)
   {
      while(cursfx)
      {
         if(cursfx->data)
         {
            efree(cursfx->data);
            cursfx->data = nullptr;
         }
         cursfx = cursfx->next;
      }
   }

   // recache sounds if so requested
   if(s_precache)
      E_PreCacheSounds();
}

//
// EDF Processing Functions
//

#define IS_SET(name) (def || cfg_size(section, name) > 0)

//
// E_ProcessSound
//
// Processes an EDF sound definition
//
static void E_ProcessSound(sfxinfo_t *sfx, cfg_t *section, bool def)
{
   bool setLink = false;
   bool explicitLumpName = false;
   int tempint;

   // preconditions:

   // sfx->mnemonic is valid, and this sfxinfo_t has already been
   // added to the sound hash table earlier by E_ProcessSounds

   // process the lump name
   if(IS_SET(ITEM_SND_LUMP))
   {
      // if this is the definition, and the lump name is not
      // defined, duplicate the mnemonic as the sound name
      if(def && cfg_size(section, ITEM_SND_LUMP) == 0)
         strncpy(sfx->name, sfx->mnemonic, 9);
      else
      {
         const char *lumpname = cfg_getstr(section, ITEM_SND_LUMP);

         // alison: set the long file name if applicable
         if(lumpname[0] == '/')
            E_ReplaceString(sfx->lfn, estrdup(&lumpname[1]));
         else
            strncpy(sfx->name, lumpname, 9);

         // mark that the lump name has been listed explicitly
         explicitLumpName = true;
      }
   }

   // process the prefix flag
   if(IS_SET(ITEM_SND_PREFIX))
   {
      // haleyjd 09/23/06: When definitions specify a lump name explicitly and
      // do not specify a value for prefix, the value will be false instead of
      // the normal default of true. This avoids the need to put
      // "prefix = false" in every single unprefixed sound.

      if(def && explicitLumpName && cfg_size(section, ITEM_SND_PREFIX) == 0)
         sfx->flags &= ~SFXF_PREFIX;
      else
      {
         if(cfg_getbool(section, ITEM_SND_PREFIX))
            sfx->flags |= SFXF_PREFIX;
         else
            sfx->flags &= ~SFXF_PREFIX;
      }
   }

   // process the singularity
   if(IS_SET(ITEM_SND_SINGULARITY))
   {
      const char *s = cfg_getstr(section, ITEM_SND_SINGULARITY);

      sfx->singularity = E_StrToNumLinear(singularities, NUM_SINGULARITIES, s);

      if(sfx->singularity == NUM_SINGULARITIES)
         sfx->singularity = 0;
   }

   // process the priority value
   if(IS_SET(ITEM_SND_PRIORITY))
   {
      sfx->priority = cfg_getint(section, ITEM_SND_PRIORITY);

      // haleyjd 09/27/06: force max 255
      // haleyjd 04/27/10: negative priority is now allowed (absolute)
      if(sfx->priority > 255)
         sfx->priority = 255;
   }

   // process the link
   if(IS_SET(ITEM_SND_LINK))
   {
      const char *name = cfg_getstr(section, ITEM_SND_LINK);

      // will be automatically nullified if name is not found
      // (this includes the default value of "none")
      sfx->link = E_SoundForName(name);

      // haleyjd 06/03/06: change defaults for linkvol/linkpitch
      setLink = true;
   }

   // haleyjd 09/24/06: process alias
   if(IS_SET(ITEM_SND_ALIAS))
   {
      const char *name = cfg_getstr(section, ITEM_SND_ALIAS);

      // will be automatically nullified same as above
      sfx->alias = E_SoundForName(name);
   }

   // haleyjd 05/12/09: process random alternatives
   if((tempint = cfg_size(section, ITEM_SND_RANDOM)) > 0)
   {
      int i;

      if(sfx->randomsounds)
         efree(sfx->randomsounds);

      sfx->numrandomsounds = tempint;

      sfx->randomsounds =
         ecalloc(sfxinfo_t **, sfx->numrandomsounds, sizeof(sfxinfo_t *));

      for(i = 0; i < sfx->numrandomsounds; ++i)
         sfx->randomsounds[i] = E_SoundForName(cfg_getnstr(section, ITEM_SND_RANDOM, i));
   }
   else if(def)
   {
      // if defining and a randomsound list is already defined, we need to destroy it,
      // as the new definition has not specified any random sounds.
      if(sfx->randomsounds)
         efree(sfx->randomsounds);

      sfx->randomsounds    = nullptr;
      sfx->numrandomsounds = 0;
   }

   // process the skin index
   if(IS_SET(ITEM_SND_SKININDEX))
   {
      const char *s = cfg_getstr(section, ITEM_SND_SKININDEX);

      sfx->skinsound = E_StrToNumLinear(skinindices, NUM_SKININDICES, s);

      if(sfx->skinsound == NUM_SKININDICES)
         sfx->skinsound = 0;
   }

   // process link volume
   if(IS_SET(ITEM_SND_LINKVOL))
   {
      sfx->volume = cfg_getint(section, ITEM_SND_LINKVOL);

      // haleyjd: test for altered defaults
      // linked sounds need actual valid values for these fields
      if(setLink && sfx->volume < 0)
         sfx->volume = 0;
   }

   // process link pitch
   if(IS_SET(ITEM_SND_LINKPITCH))
   {
      sfx->pitch = cfg_getint(section, ITEM_SND_LINKPITCH);

      // haleyjd: test for altered defaults
      // linked sounds need actual valid values for these fields
      if(setLink && sfx->pitch < 0)
         sfx->pitch = 128;
   }


   // haleyjd 07/13/05: process clipping_dist
   if(IS_SET(ITEM_SND_CLIPPING_DIST))
      sfx->clipping_dist = cfg_getint(section, ITEM_SND_CLIPPING_DIST) << FRACBITS;

   // haleyjd 07/13/05: process close_dist
   if(IS_SET(ITEM_SND_CLOSE_DIST))
      sfx->close_dist = cfg_getint(section, ITEM_SND_CLOSE_DIST) << FRACBITS;

   // haleyjd 09/23/06: process pitch variance type
   if(IS_SET(ITEM_SND_PITCHVAR))
   {
      const char *s = cfg_getstr(section, ITEM_SND_PITCHVAR);

      sfx->pitch_type = E_StrToNumLinear(pitchvars, NUM_PITCHVARS, s);

      if(sfx->pitch_type == NUM_PITCHVARS)
         sfx->pitch_type = sfxinfo_t::pitch_none;
   }

   // haleyjd 06/12/08: process subchannel
   if(IS_SET(ITEM_SND_SUBCHANNEL))
   {
      const char *s = cfg_getstr(section, ITEM_SND_SUBCHANNEL);

      sfx->subchannel = E_StrToNumLinear(subchans, NUM_SUBCHANS, s);

      if(sfx->subchannel == NUM_SUBCHANS)
         sfx->subchannel = CHAN_AUTO;
   }

   // haleyjd 11/07/08: process explicit pc speaker lump name
   if(IS_SET(ITEM_SND_PCSLUMP))
   {
      const char *lumpname = cfg_getstr(section, ITEM_SND_PCSLUMP);

      if(lumpname != nullptr)
      {
         // alison: set the long file name if applicable
         if(lumpname[0] == '/')
            E_ReplaceString(sfx->pcslfn, estrdup(&lumpname[1]));
         else
            strncpy(sfx->pcslump, lumpname, 9);
      }
   }

   // haleyjd 11/08/08: process "nopcsound" flag
   if(IS_SET(ITEM_SND_NOPCSOUND))
   {
      bool nopcsound = cfg_getbool(section, ITEM_SND_NOPCSOUND);

      if(nopcsound)
         sfx->flags |= SFXF_NOPCSOUND;
      else
         sfx->flags &= ~SFXF_NOPCSOUND;
   }
}

//
// E_ProcessSounds
//
// Collects all the sound definitions and builds the sound hash tables.
// 04/13/08: rewritten to be fully dynamic.
//
void E_ProcessSounds(cfg_t *cfg)
{
   unsigned int i, numsfx = cfg_size(cfg, EDF_SEC_SOUND);
   sfxinfo_t *sfx;

   // haleyjd 08/11/13: process SNDINFO lumps here, first, instead of later and
   // having them be concerned with cross-port issues with wads that contain
   // both DECORATE and EDF.
   E_EDFLogPuts("\t\tProcessing Hexen SNDINFO\n");
   XL_ParseSoundInfo();

   E_EDFLogPuts("\t\tHashing sounds\n");

   // now, let's collect the mnemonics (this must be done ahead of time)
   for(i = 0; i < numsfx; i++)
   {
      const char *mnemonic;
      cfg_t *sndsection = cfg_getnsec(cfg, EDF_SEC_SOUND, i);
      int idnum = cfg_getint(sndsection, ITEM_SND_DEHNUM);

      mnemonic = cfg_title(sndsection);

      // if one already exists by this name, use it
      if((sfx = E_SoundForName(mnemonic)))
      {
         // handle dehackednum changes
         if(sfx->dehackednum != idnum)
         {
            // if already in hash, remove it
            if(sfx->dehackednum > 0)
               E_DelSoundFromDEHHash(sfx);

            // set new dehackednum
            sfx->dehackednum = idnum;

            // if > 0, rehash
            if(sfx->dehackednum > 0)
               E_AddSoundToDEHHash(sfx);
         }
      }
      else
      {
         // create a new sound
         sfx = estructalloc(sfxinfo_t, 1);

         // verify the length
         if(strlen(mnemonic) >= sizeof(sfx->mnemonic))
         {
            E_EDFLoggedErr(2, "E_ProcessSounds: invalid sound mnemonic '%s'\n",
                           mnemonic);
         }

         // copy mnemonic
         strncpy(sfx->mnemonic, mnemonic, sizeof(sfx->mnemonic));

         // add this sound to the hash table
         E_AddSoundToHash(sfx);

         // set dehackednum
         sfx->dehackednum = idnum;

         // sfxinfo was born via EDF
         sfx->flags = SFXF_EDF;

         // possibly add to numeric hash
         if(sfx->dehackednum > 0)
            E_AddSoundToDEHHash(sfx);
      }
   }

   E_EDFLogPuts("\t\tProcessing data\n");

   // finally, process the individual sounds
   for(i = 0; i < numsfx; i++)
   {
      cfg_t *section = cfg_getnsec(cfg, EDF_SEC_SOUND, i);
      const char *mnemonic = cfg_title(section);
      sfx = E_SoundForName(mnemonic);

      E_ProcessSound(sfx, section, true);

      E_EDFLogPrintf("\t\tFinished sound %s(#%d)\n", sfx->mnemonic, i);
   }

   E_EDFLogPuts("\t\tFinished sound processing\n");
}

//
// E_ProcessSoundDeltas
//
// Does processing for sounddelta sections, which allow cascading
// editing of existing sounds. The sounddelta shares most of its
// fields and processing code with the sound section.
//
void E_ProcessSoundDeltas(cfg_t *cfg, bool add)
{
   int i, numdeltas;

   if(!add)
      E_EDFLogPuts("\t* Processing sound deltas\n");
   else
      E_EDFLogPuts("\t\tProcessing additive sound deltas\n");

   numdeltas = cfg_size(cfg, EDF_SEC_SDELTA);

   E_EDFLogPrintf("\t\t%d sounddelta(s) defined\n", numdeltas);

   for(i = 0; i < numdeltas; ++i)
   {
      const char *tempstr;
      sfxinfo_t *sfx;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_SDELTA, i);

      // get sound to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessSoundDeltas: sounddelta requires name field\n");

      tempstr = cfg_getstr(deltasec, ITEM_DELTA_NAME);
      sfx = E_SoundForName(tempstr);

      if(!sfx)
      {
         E_EDFLoggedErr(2,
            "E_ProcessSoundDeltas: sound '%s' does not exist\n", tempstr);
      }

      E_ProcessSound(sfx, deltasec, false);

      E_EDFLogPrintf("\t\tApplied sounddelta #%d to sound %s\n", i, tempstr);
   }
}

//=============================================================================
//
// Sound Sequences
//
// haleyjd 05/28/06
//

#define ITEM_SEQ_ID     "id"
#define ITEM_SEQ_CMDS   "cmds"
#define ITEM_SEQ_HCMDS  "commands"
#define ITEM_SEQ_TYPE   "type"
#define ITEM_SEQ_STOP   "stopsound"
#define ITEM_SEQ_ATTN   "attenuation"
#define ITEM_SEQ_VOL    "volume"
#define ITEM_SEQ_MNVOL  "minvolume"
#define ITEM_SEQ_NSCO   "nostopcutoff"
#define ITEM_SEQ_RNDVOL "randomplayvol"
#define ITEM_SEQ_DOOR   "doorsequence"
#define ITEM_SEQ_PLAT   "platsequence"
#define ITEM_SEQ_FLOOR  "floorsequence"
#define ITEM_SEQ_CEIL   "ceilingsequence"
#define ITEM_SEQ_REVERB "reverb"

// attenuation types -- also used by ambience
static const char *attenuation_types[] =
{
   "normal",
   "idle",
   "static",
   "none"
};

#define NUM_ATTENUATION_TYPES (sizeof(attenuation_types) / sizeof(char *))

// sequence types
static const char *seq_types[] =
{
   "sector",      // a general sector type
   "door",        // specifically a door type
   "plat",        // specifically a plat type
   "environment", // environment
};

#define NUM_SEQ_TYPES (sizeof(seq_types) / sizeof(char *))

// sequence command strings
static const char *sndseq_cmdstrs[] =
{
   "play",
   "playuntildone",
   "playtime",
   "playrepeat",
   "playloop",
   "playabsvol",
   "playrelvol",
   "relvolume",
   "delay",
   "delayrand",
   "restart",
   "end",

   // these are supported as properties as well
   "stopsound",
   "attenuation",
   "volume",
   "nostopcutoff",
};

enum
{
   SEQ_TXTCMD_PLAY,
   SEQ_TXTCMD_PLAYUNTILDONE,
   SEQ_TXTCMD_PLAYTIME,
   SEQ_TXTCMD_PLAYREPEAT,
   SEQ_TXTCMD_PLAYLOOP,
   SEQ_TXTCMD_PLAYABSVOL,
   SEQ_TXTCMD_PLAYRELVOL,
   SEQ_TXTCMD_RELVOLUME,
   SEQ_TXTCMD_DELAY,
   SEQ_TXTCMD_DELAYRAND,
   SEQ_TXTCMD_RESTART,
   SEQ_TXTCMD_END,

   SEQ_TXTCMD_STOPSOUND,
   SEQ_TXTCMD_ATTENUATION,
   SEQ_TXTCMD_VOLUME,
   SEQ_TXTCMD_NOSTOPCUTOFF,
   SEQ_NUM_TXTCMDS,
};

cfg_opt_t edf_sndseq_opts[] =
{
   CFG_INT(ITEM_SEQ_ID,      -1,        CFGF_NONE),
   CFG_STR(ITEM_SEQ_CMDS,    nullptr,   CFGF_LIST|CFGF_STRSPACE),
   CFG_STR(ITEM_SEQ_HCMDS,   nullptr,   CFGF_NONE),
   CFG_STR(ITEM_SEQ_TYPE,    "sector",  CFGF_NONE),
   CFG_STR(ITEM_SEQ_STOP,    "none",    CFGF_NONE),
   CFG_STR(ITEM_SEQ_ATTN,    "normal",  CFGF_NONE),
   CFG_INT(ITEM_SEQ_VOL,     127,       CFGF_NONE),
   CFG_INT(ITEM_SEQ_MNVOL,   -1,        CFGF_NONE),
   CFG_STR(ITEM_SEQ_DOOR,    nullptr,   CFGF_NONE),
   CFG_STR(ITEM_SEQ_PLAT,    nullptr,   CFGF_NONE),
   CFG_STR(ITEM_SEQ_FLOOR,   nullptr,   CFGF_NONE),
   CFG_STR(ITEM_SEQ_CEIL,    nullptr,   CFGF_NONE),

   CFG_BOOL(ITEM_SEQ_NSCO,   false,     CFGF_NONE),
   CFG_BOOL(ITEM_SEQ_RNDVOL, false,     CFGF_NONE),
   CFG_FLAG(ITEM_SEQ_REVERB, 1,         CFGF_SIGNPREFIX),

   CFG_END()
};

#define NUM_EDFSEQ_CHAINS 127
static ESoundSeq_t              *edf_seq_chains[NUM_EDFSEQ_CHAINS];
static DLListItem<ESoundSeq_t> *edf_seq_numchains[NUM_EDFSEQ_CHAINS];

// need a separate hash for environmental sequences
#define NUM_EDFSEQ_ENVCHAINS 31
static DLListItem<ESoundSeq_t> *edf_seq_envchains[NUM_EDFSEQ_ENVCHAINS];

// translator tables for specific types

#define NUM_SEQ_TRANSLATE 64

static ESoundSeq_t *edf_door_sequences[NUM_SEQ_TRANSLATE];
static ESoundSeq_t *edf_plat_sequences[NUM_SEQ_TRANSLATE];

static int e_sequence_count; // 04/13/08: keep count of sequences

//
// E_AddSequenceToNameHash
//
// Adds an EDF sound sequence to the mnemonic hash table.
//
static void E_AddSequenceToNameHash(ESoundSeq_t *seq)
{
   unsigned int keyval = D_HashTableKey(seq->name) % NUM_EDFSEQ_CHAINS;

   seq->namenext = edf_seq_chains[keyval];
   edf_seq_chains[keyval] = seq;

   ++e_sequence_count;
}

//
// E_AddSequenceToNumHash
//
// Adds an EDF sound sequence object to the numeric hash table. More than
// one object with the same numeric id can exist, but E_SequenceForNum will
// only ever find the first such object, which is always the last such
// object added to the hash table.
//
// Note that sequences of type SEQ_ENVIRONMENT have their own separate hash.
//
static void E_AddSequenceToNumHash(ESoundSeq_t *seq)
{
   unsigned int idx;

   if(seq->type != SEQ_ENVIRONMENT)
   {
      idx = seq->index % NUM_EDFSEQ_CHAINS;

      seq->numlinks.insert(seq, &edf_seq_numchains[idx]);

      // possibly add to the specific type translation tables
      if(seq->index < NUM_SEQ_TRANSLATE)
      {
         switch(seq->type)
         {
         case SEQ_DOORTYPE:
            edf_door_sequences[seq->index] = seq;
            break;
         case SEQ_PLATTYPE:
            edf_plat_sequences[seq->index] = seq;
            break;
         default:
            break;
         }
      }
   }
   else // environment sequences are hashed separately
   {
      idx = seq->index % NUM_EDFSEQ_ENVCHAINS;

      seq->numlinks.insert(seq, &edf_seq_envchains[idx]);
   }
}

//
// E_DelSequenceFromNumHash
//
// Removes a specific EDF sound sequence object from the numeric hash
// table. This must be called on an object that's already linked before
// relinking it.
//
static void E_DelSequenceFromNumHash(ESoundSeq_t *seq)
{
   seq->numlinks.remove();
}

//
// E_SequenceForName
//
// Returns an EDF sound sequence with the given name. If none exists,
// nullptr will be returned.
//
ESoundSeq_t *E_SequenceForName(const char *name)
{
   unsigned int key = D_HashTableKey(name) % NUM_EDFSEQ_CHAINS;
   ESoundSeq_t *seq = edf_seq_chains[key];

   while(seq && strncasecmp(seq->name, name, sizeof(seq->name)))
      seq = seq->namenext;

   return seq;
}

//
// E_SequenceForNum
//
// Returns an EDF sound sequence with the given numeric id. If none exists,
// nullptr will be returned.
//
ESoundSeq_t *E_SequenceForNum(int id)
{
   unsigned int key = id % NUM_EDFSEQ_CHAINS;
   DLListItem<ESoundSeq_t> *link = edf_seq_numchains[key];

   while(link && (*link)->index != id)
      link = link->dllNext;

   return link ? link->dllObject : nullptr;
}

//
// E_SequenceForNumType
//
// Looks at the specific type translation tables before resorting to the normal
// hash table.
//
ESoundSeq_t *E_SequenceForNumType(int id, int type)
{
   ESoundSeq_t *ret = nullptr;

   if(id < NUM_SEQ_TRANSLATE)
   {
      switch(type)
      {
      case SEQ_DOOR:
         ret = edf_door_sequences[id];
         break;
      case SEQ_PLAT:
      case SEQ_FLOOR:
      case SEQ_CEILING:
         ret = edf_plat_sequences[id];
         break;
      default:
         break;
      }
   }

   return ret ? ret : E_SequenceForNum(id);
}

//
// E_EnvironmentSequence
//
// Returns the environmental sound sequence with the given numeric id.
// If none exists, nullptr will be returned.
//
ESoundSeq_t *E_EnvironmentSequence(int id)
{
   unsigned int key = id % NUM_EDFSEQ_ENVCHAINS;
   DLListItem<ESoundSeq_t> *link = edf_seq_envchains[key];

   while(link && (*link)->index != id)
      link = link->dllNext;

   return link ? link->dllObject : nullptr;
}

//
// E_SeqGetSound
//
// A safe wrapper around E_SoundForName that returns nullptr for the
// nullptr pointer. This saves me a truckload of hassle below.
//
inline static sfxinfo_t *E_SeqGetSound(const char *soundname)
{
   return soundname ? E_SoundForName(soundname) : nullptr;
}

//
// E_SeqGetNumber
//
// A safe wrapper around strtol that returns 0 for the nullptr string.
//
inline static int E_SeqGetNumber(const char *numstr)
{
   return numstr ? static_cast<int>(strtol(numstr, nullptr, 0)) : 0;
}

//
// E_SeqGetAttn
//
// A safe wrapper on E_StrToNumLinear for attenuation types.
//
static int E_SeqGetAttn(const char *attnstr)
{
   int attn;

   if(attnstr)
   {
      attn = E_StrToNumLinear(attenuation_types, NUM_ATTENUATION_TYPES,
                              attnstr);
      if(attn == NUM_ATTENUATION_TYPES)
         attn = ATTN_NORMAL;
   }
   else
      attn = ATTN_NORMAL;

   return attn;
}

//
// E_GenerateSeqOp
//
// Generate a sound sequence opcode.
//
static void E_GenerateSeqOp(ESoundSeq_t *newSeq, tempcmd_t &tempcmd,
                            seqcmd_t *tempcmdbuf, unsigned int &allocused)
{
   int cmdindex;

   // translate to command index
   cmdindex = E_StrToNumLinear(sndseq_cmdstrs, SEQ_NUM_TXTCMDS,
                               tempcmd.strs[0]);

   // generate opcodes and their arguments in the temporary buffer
   switch(cmdindex)
   {
   case SEQ_TXTCMD_PLAY:
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAY;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_PLAYUNTILDONE:
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAY;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      tempcmdbuf[allocused++].data = SEQ_CMD_WAITSOUND;
      break;
   case SEQ_TXTCMD_PLAYTIME:
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAY;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      tempcmdbuf[allocused++].data = SEQ_CMD_DELAY;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[2]);
      break;
   case SEQ_TXTCMD_PLAYREPEAT:
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAYREPEAT;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_PLAYLOOP:
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAYLOOP;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[2]);
      break;
   case SEQ_TXTCMD_PLAYABSVOL:
      tempcmdbuf[allocused++].data = SEQ_CMD_SETVOLUME;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[2]);
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAY;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_PLAYRELVOL:
      tempcmdbuf[allocused++].data = SEQ_CMD_SETVOLUMEREL;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[2]);
      tempcmdbuf[allocused++].data = SEQ_CMD_PLAY;
      tempcmdbuf[allocused++].sfx  = E_SeqGetSound(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_DELAY:
      tempcmdbuf[allocused++].data = SEQ_CMD_DELAY;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_DELAYRAND:
      tempcmdbuf[allocused++].data = SEQ_CMD_DELAYRANDOM;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[1]);
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[2]);
      break;
   case SEQ_TXTCMD_VOLUME:
      tempcmdbuf[allocused++].data = SEQ_CMD_SETVOLUME;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_RELVOLUME:
      tempcmdbuf[allocused++].data = SEQ_CMD_SETVOLUMEREL;
      tempcmdbuf[allocused++].data = E_SeqGetNumber(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_ATTENUATION:
      tempcmdbuf[allocused++].data = SEQ_CMD_SETATTENUATION;
      tempcmdbuf[allocused++].data = E_SeqGetAttn(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_STOPSOUND:
      // this doesn't go into the command stream; rather, it changes the
      // stopsound property of the sound sequence object
      newSeq->stopsound = E_SeqGetSound(tempcmd.strs[1]);
      break;
   case SEQ_TXTCMD_NOSTOPCUTOFF:
      // as above, but this sets the nostopcutoff property
      newSeq->nostopcutoff = true;
      break;
   case SEQ_TXTCMD_RESTART:
      tempcmdbuf[allocused++].data = SEQ_CMD_RESTART;
      break;
   case SEQ_TXTCMD_END:
      // do nothing, an end command will be generated below
      break;
   default:
      // invalid opcode :P
      E_EDFLoggedWarning(2, "Warning: invalid cmd '%s' in sequence, ignored\n",
                         tempcmd.strs[0]);
      break;
   }
}

//
// E_ParseSeqCmds
//
// Yay, more complicated stuff like the cmp frame parser!
// Seriously, this function compiles sound sequence command strings into a
// compact array of binary data (bytecode, if you will), and stores it into
// the ESoundSeq_t structure.
//
// Individual commands are tokenized by E_ParseSeqCmdStr above. Only the first
// three whitespace-delimited tokens on each line are considered, the rest is
// thrown away as garbage. This makes it freeform but still somewhat strict.
// Everything is error-tolerant. Missing tokens are NULLified or zeroed out.
// Bad commands have no effect. Bad sound names end up nullptr also.
//
// Note that the commands are compiled into a temporary buffer that is allocated
// at the upper bound of the possible code size -- no command compiles to more
// than four bytecodes (one or two opcodes and two arguments). At the end, the
// temporary buffer is copied into one of the actually used size and the temp
// buffer is destroyed.
//
static void E_ParseSeqCmds(cfg_t *cfg, ESoundSeq_t *newSeq)
{
   unsigned int i, numcmds;              // loop stuff
   unsigned int cmdalloc, allocused = 0; // space allocated, space actually used
   seqcmd_t *tempcmdbuf;                 // temporary command buffer

   numcmds = cfg_size(cfg, ITEM_SEQ_CMDS);

   // allocate the upper bound of command space in the temp buffer:
   // * add 1 to numcmds for possible missing end command
   // * multiply by 4 because no txt command compiles to more than 4 ops
   cmdalloc = (numcmds + 1) * 4 * sizeof(seqcmd_t);

   tempcmdbuf = ecalloc(seqcmd_t *, 1, cmdalloc);

   for(i = 0; i < numcmds; i++)
   {
      tempcmd_t tempcmd;
      char *tempstr = estrdup(cfg_getnstr(cfg, ITEM_SEQ_CMDS, i));

      tempcmd = E_ParseTextLine(tempstr); // parse the command

      // figure out the command (first token on the line)
      if(!tempcmd.strs[0])
         E_EDFLoggedWarning(2, "Warning: invalid command in sequence, ignored\n");
      else
         E_GenerateSeqOp(newSeq, tempcmd, tempcmdbuf, allocused);

      efree(tempstr); // free temporary copy of command
   } // end for

   // now, generate an end command -- doing it this way, we make sure the
   // sequence is terminated whether or not the user provides an end command,
   // which is good since it's really unnecessary in EDF due to syntax
   tempcmdbuf[allocused++].data = SEQ_CMD_END;

   // now, allocate the buffer in the ESoundSeq_t object at the size actually
   // used by the compiled sound sequence commands
   cmdalloc = allocused * sizeof(seqcmd_t);

   newSeq->commands = emalloc(seqcmd_t *, cmdalloc);
   memcpy(newSeq->commands, tempcmdbuf, cmdalloc);

   // free the temp buffer
   efree(tempcmdbuf);
}

//
// E_ParseSeqCmdsFromHereDoc
//
// haleyjd 01/10/11: Support for heredoc-embedded sequences.
//
static void E_ParseSeqCmdsFromHereDoc(const char *heredoc, ESoundSeq_t *newSeq)
{
   int numcmds;
   unsigned int cmdalloc, allocused = 0; // space allocated, space actually used
   seqcmd_t *tempcmdbuf;                 // temporary command buffer
   char *str   = Z_Strdupa(heredoc);     // make a temp mutable copy of the string
   char *rover = str;                    // position in buffer
   char *line  = nullptr;                   // start of current line in buffer

   // The number of commands should be equal to the number of lines in the
   // heredoc string, plus a possible one extra for an implicit end command.
   numcmds = M_CountNumLines(str);

   // allocate the upper bound of command space in the temp buffer:
   // * add 1 to numcmds for possible missing end command
   // * multiply by 4 because no txt command compiles to more than 4 ops
   cmdalloc = ((unsigned int)numcmds + 1) * 4 * sizeof(seqcmd_t);

   tempcmdbuf = ecalloc(seqcmd_t *, 1, cmdalloc);

   while((line = E_GetHeredocLine(&rover)))
   {
      tempcmd_t tempcmd;

      // ignore empty lines
      if(!strlen(line))
         continue;

      tempcmd = E_ParseTextLine(line); // parse the command

      // figure out the command (first token on the line)
      if(!tempcmd.strs[0])
         E_EDFLoggedWarning(2, "Warning: invalid command in sequence, ignored\n");
      else
         E_GenerateSeqOp(newSeq, tempcmd, tempcmdbuf, allocused);
   }

   // now, generate an end command -- doing it this way, we make sure the
   // sequence is terminated whether or not the user provides an end command,
   // which is good since it's really unnecessary in EDF due to syntax
   tempcmdbuf[allocused++].data = SEQ_CMD_END;

   // now, allocate the buffer in the ESoundSeq_t object at the size actually
   // used by the compiled sound sequence commands
   cmdalloc = allocused * sizeof(seqcmd_t);

   newSeq->commands = emalloc(seqcmd_t *, cmdalloc);
   memcpy(newSeq->commands, tempcmdbuf, cmdalloc);

   // free the temp buffer
   efree(tempcmdbuf);
}

//
// E_ProcessSndSeq
//
// Processes a single EDF sound sequence.
//
static void E_ProcessSndSeq(cfg_t *cfg, unsigned int i)
{
   const char *name, *tempstr;
   ESoundSeq_t *newSeq;
   int idnum, type;

   // get name of sequence
   name = cfg_title(cfg);

   // get numeric id and type now
   idnum = cfg_getint(cfg, ITEM_SEQ_ID);

   tempstr = cfg_getstr(cfg, ITEM_SEQ_TYPE);
   type = E_StrToNumLinear(seq_types, NUM_SEQ_TYPES, tempstr);
   if(type == NUM_SEQ_TYPES)
   {
      E_EDFLoggedWarning(2, "Warning: invalid sequence type '%s'\n", tempstr);
      type = SEQ_SECTOR; // default is sector-type if invalid
   }

   // if one exists by this name already, reuse it
   if((newSeq = E_SequenceForName(name)))
   {
      // Modify numeric id and rehash object if necessary
      if(idnum != newSeq->index || type != newSeq->type)
      {
         // If old key is >= 0, must remove from hash first
         if(newSeq->index >= 0)
            E_DelSequenceFromNumHash(newSeq);

         // Set new key and type
         newSeq->index = idnum;
         newSeq->type  = type;

         // If new key >= 0, add back to hash
         if(newSeq->index >= 0)
            E_AddSequenceToNumHash(newSeq);
      }
   }
   else
   {
      // Create a new sound sequence object
      newSeq = estructalloc(ESoundSeq_t, 1);

      // verify length
      if(strlen(name) >= sizeof(newSeq->name))
         E_EDFLoggedErr(2, "E_ProcessSndSeq: invalid mnemonic '%s'\n", name);

      // copy keys into sequence object
      strncpy(newSeq->name, name, sizeof(newSeq->name));

      newSeq->index = idnum;
      newSeq->type  = type;

      // add to hash tables

      E_AddSequenceToNameHash(newSeq);

      // numeric key is not required
      if(idnum >= 0)
         E_AddSequenceToNumHash(newSeq);
   }

   // process stopsound -- invalid sounds are not an error, and mean "none"
   tempstr = cfg_getstr(cfg, ITEM_SEQ_STOP);
   newSeq->stopsound = E_SoundForName(tempstr);

   // process attenuation
   tempstr = cfg_getstr(cfg, ITEM_SEQ_ATTN);
   newSeq->attenuation =
      E_StrToNumLinear(attenuation_types, NUM_ATTENUATION_TYPES, tempstr);
   if(newSeq->attenuation == NUM_ATTENUATION_TYPES)
   {
      E_EDFLoggedWarning(2, "Warning: sequence %d uses unknown attn type '%s'\n",
                         newSeq->index, tempstr);
      newSeq->attenuation = ATTN_NORMAL; // normal attenuation is fine
   }

   // process volume
   newSeq->volume = cfg_getint(cfg, ITEM_SEQ_VOL);
   // rangecheck
   if(newSeq->volume < 0)
      newSeq->volume = 0;
   else if(newSeq->volume > 127)
      newSeq->volume = 127;

   // process minvolume
   newSeq->minvolume = cfg_getint(cfg, ITEM_SEQ_MNVOL);
   // if != -1 and not same as volume, volume is randomized
   if(newSeq->minvolume != -1 && newSeq->minvolume != newSeq->volume)
   {
      newSeq->randvol = true;
      // rangecheck minvolume (max is volume - 1)
      if(newSeq->minvolume > newSeq->volume)
         newSeq->minvolume = newSeq->volume - 1;
      if(newSeq->minvolume < 0)
         newSeq->minvolume = 0;
   }

   // process nostopcutoff
   newSeq->nostopcutoff = cfg_getbool(cfg, ITEM_SEQ_NSCO);

   // haleyjd 01/12/11: support for proper Heretic randomization behavior
   newSeq->randomplayvol = cfg_getbool(cfg, ITEM_SEQ_RNDVOL);

   // 01/16/14: reverb flag
   newSeq->reverb = !!cfg_getflag(cfg, ITEM_SEQ_REVERB);

   // process command list

   // if a command list already exists, destroy it first
   if(newSeq->commands)
      efree(newSeq->commands);

   // haleyjd 01/11/11: I have added support for heredoc-based sound sequences,
   // which are exclusive of and preferred to the old syntax. This allows Hexen-
   // compatible SNDSEQ data to be pasted directly into EDF. It should also come
   // in handy later when supporting actual SNDSEQ lumps.

   if(cfg_size(cfg, ITEM_SEQ_HCMDS) > 0)
      E_ParseSeqCmdsFromHereDoc(cfg_getstr(cfg, ITEM_SEQ_HCMDS), newSeq);
   else
      E_ParseSeqCmds(cfg, newSeq);
}

//
// E_ResolveNames
//
// This function resolves the fields in the sound sequences which
// can reference other sequences. It's necessary to do this in a second
// pass due to the ability of sound sequences to be overwritten.
//
static void E_ResolveNames(cfg_t *cfg, unsigned int i)
{
   const char  *tempstr;
   const char  *name = cfg_title(cfg);
   ESoundSeq_t *seq  = E_SequenceForName(name);

   if(!seq)
   {
      E_EDFLoggedErr(2, "E_ResolveNames: internal error: no such sequence %s\n",
                     name);
   }

   // Note it is not an error for any of these to be either unspecified or
   // invalid. In either case, the corresponding redirection is nullified.

   // process door redirection
   if((tempstr = cfg_getstr(cfg, ITEM_SEQ_DOOR)))
      seq->doorseq = E_SequenceForName(tempstr);

   // process plat redirection
   if((tempstr = cfg_getstr(cfg, ITEM_SEQ_PLAT)))
      seq->platseq = E_SequenceForName(tempstr);

   // process floor redirection
   if((tempstr = cfg_getstr(cfg, ITEM_SEQ_FLOOR)))
      seq->floorseq = E_SequenceForName(tempstr);

   // process ceiling redirection
   if((tempstr = cfg_getstr(cfg, ITEM_SEQ_CEIL)))
      seq->ceilseq = E_SequenceForName(tempstr);

   E_EDFLogPrintf("\t\tFinished sound sequence %s (#%d)\n", name, i);
}

// enviro seq manager stuff

#define ITEM_SEQMGR_MINSTARTWAIT "minstartwait"
#define ITEM_SEQMGR_MAXSTARTWAIT "maxstartwait"
#define ITEM_SEQMGR_MINWAIT      "minwait"
#define ITEM_SEQMGR_MAXWAIT      "maxwait"

cfg_opt_t edf_seqmgr_opts[] =
{
   CFG_INT(ITEM_SEQMGR_MINSTARTWAIT, (10*TICRATE),       CFGF_NONE),
   CFG_INT(ITEM_SEQMGR_MAXSTARTWAIT, (10*TICRATE + 31),  CFGF_NONE),
   CFG_INT(ITEM_SEQMGR_MINWAIT,      ( 6*TICRATE),       CFGF_NONE),
   CFG_INT(ITEM_SEQMGR_MAXWAIT,      ( 6*TICRATE + 255), CFGF_NONE),
   CFG_END()
};

//
// E_ProcessEnviroMgr
//
// Processes the optional block for customizing the environment sequence manager
//
static void E_ProcessEnviroMgr(cfg_t *cfg)
{
   unsigned int nummgr;

   E_EDFLogPuts("\t* Processing enviroment sequence manager\n");

   nummgr = cfg_size(cfg, EDF_SEC_ENVIROMGR);

   if(nummgr)
   {
      cfg_t *eseq = cfg_getsec(cfg, EDF_SEC_ENVIROMGR);

      EnviroSeqManager.minStartWait =
         cfg_getint(eseq, ITEM_SEQMGR_MINSTARTWAIT);
      EnviroSeqManager.maxStartWait =
         cfg_getint(eseq, ITEM_SEQMGR_MAXSTARTWAIT);

      // range check
      if(EnviroSeqManager.minStartWait < 0)
         EnviroSeqManager.minStartWait = 0;
      if(EnviroSeqManager.maxStartWait < 1)
         EnviroSeqManager.maxStartWait = 1;

      if(EnviroSeqManager.maxStartWait <= EnviroSeqManager.minStartWait)
         EnviroSeqManager.maxStartWait = EnviroSeqManager.minStartWait + 1;

      EnviroSeqManager.minEnviroWait = cfg_getint(eseq, ITEM_SEQMGR_MINWAIT);
      EnviroSeqManager.maxEnviroWait = cfg_getint(eseq, ITEM_SEQMGR_MAXWAIT);

      if(EnviroSeqManager.minEnviroWait < 1)
         EnviroSeqManager.minEnviroWait = 1;
      if(EnviroSeqManager.maxEnviroWait < 2)
         EnviroSeqManager.maxEnviroWait = 2;
      if(EnviroSeqManager.maxEnviroWait <= EnviroSeqManager.minEnviroWait)
         EnviroSeqManager.maxEnviroWait = EnviroSeqManager.minEnviroWait + 1;
   }
}

//
// E_ProcessSndSeqs
//
// Processes all EDF sound sequences.
//
void E_ProcessSndSeqs(cfg_t *cfg)
{
   unsigned int i, numsequences;

   E_EDFLogPuts("\t* Processing sound sequences\n");

   numsequences = cfg_size(cfg, EDF_SEC_SNDSEQ);

   E_EDFLogPrintf("\t\t%d sound sequence(s) defined\n", numsequences);

   // do primary processing
   for(i = 0; i < numsequences; ++i)
      E_ProcessSndSeq(cfg_getnsec(cfg, EDF_SEC_SNDSEQ, i), i);

   // resolve mutually referencing fields
   for(i = 0; i < numsequences; ++i)
      E_ResolveNames(cfg_getnsec(cfg, EDF_SEC_SNDSEQ, i), i);

   // process the environment sequence manager
   E_ProcessEnviroMgr(cfg);
}

//=============================================================================
//
// Ambience
//
// haleyjd 05/30/06
//

#define ITEM_AMB_SOUND       "sound"
#define ITEM_AMB_INDEX       "index"
#define ITEM_AMB_VOLUME      "volume"
#define ITEM_AMB_ATTENUATION "attenuation"
#define ITEM_AMB_TYPE        "type"
#define ITEM_AMB_PERIOD      "period"
#define ITEM_AMB_MINPERIOD   "minperiod"
#define ITEM_AMB_MAXPERIOD   "maxperiod"
#define ITEM_AMB_REVERB      "reverb"

static const char *ambience_types[] =
{
   "continuous",
   "periodic",
   "random",
};

#define NUM_AMBIENCE_TYPES (sizeof(ambience_types) / sizeof(char *))

cfg_opt_t edf_ambience_opts[] =
{
   CFG_STR(ITEM_AMB_SOUND,       "none",       CFGF_NONE),
   CFG_INT(ITEM_AMB_INDEX,       0,            CFGF_NONE),
   CFG_INT(ITEM_AMB_VOLUME,      127,          CFGF_NONE),
   CFG_STR(ITEM_AMB_ATTENUATION, "normal",     CFGF_NONE),
   CFG_STR(ITEM_AMB_TYPE,        "continuous", CFGF_NONE),
   CFG_INT(ITEM_AMB_PERIOD,      35,           CFGF_NONE),
   CFG_INT(ITEM_AMB_MINPERIOD,   35,           CFGF_NONE),
   CFG_INT(ITEM_AMB_MAXPERIOD,   35,           CFGF_NONE),

   CFG_FLAG(ITEM_AMB_REVERB,     1,            CFGF_SIGNPREFIX),

   CFG_END()
};

// ambience hash table
#define NUMAMBIENCECHAINS 67
static EAmbience_t *ambience_chains[NUMAMBIENCECHAINS];

//
// E_AmbienceForNum
//
// Given an ambience index, returns the ambience object.
// Returns nullptr if no such ambience object exists.
//
EAmbience_t *E_AmbienceForNum(int num)
{
   int key = num % NUMAMBIENCECHAINS;
   EAmbience_t *cur = ambience_chains[key];

   while(cur && cur->index != num)
      cur = cur->next;

   return cur;
}

//
// E_AddAmbienceToHash
//
// Adds an ambience object to the hash table.
//
static void E_AddAmbienceToHash(EAmbience_t *amb)
{
   int key = amb->index % NUMAMBIENCECHAINS;

   amb->next = ambience_chains[key];

   ambience_chains[key] = amb;
}

//
// E_ProcessAmbienceSec
//
// Processes a single EDF ambience section.
//
static void E_ProcessAmbienceSec(cfg_t *cfg, unsigned int i)
{
   EAmbience_t *newAmb;
   const char *tempstr;
   int index;

   // issue a warning if index is undefined
   if(cfg_size(cfg, ITEM_AMB_INDEX) == 0)
   {
      E_EDFLoggedWarning(2, "Warning: ambience %d defines no index, "
                            "ambience index 0 may be overwritten.\n", i);
   }

   // get index
   index = cfg_getint(cfg, ITEM_AMB_INDEX);

   // if one already exists, use it, else create a new one
   if(!(newAmb = E_AmbienceForNum(index)))
   {
      newAmb = estructalloc(EAmbience_t, 1);

      // add to hash table
      newAmb->index = index;
      E_AddAmbienceToHash(newAmb);
   }

   // process type -- must be valid
   tempstr = cfg_getstr(cfg, ITEM_AMB_TYPE);
   newAmb->type = E_StrToNumLinear(ambience_types, NUM_AMBIENCE_TYPES, tempstr);
   if(newAmb->type == NUM_AMBIENCE_TYPES)
   {
      E_EDFLoggedWarning(2, "Warning: ambience %d uses bad type '%s'\n",
                         newAmb->index, tempstr);
      newAmb->type = 0; // use continuous as a default
   }

   // process sound -- note: may end up nullptr, this is not an error
   tempstr = cfg_getstr(cfg, ITEM_AMB_SOUND);
   newAmb->sound = E_SoundForName(tempstr);
   if(!newAmb->sound)
   {
      // issue a warning just in case this is a mistake
      E_EDFLoggedWarning(2, "Warning: ambience %d references bad sound '%s'\n",
                         newAmb->index, tempstr);
   }

   // process volume
   newAmb->volume = cfg_getint(cfg, ITEM_AMB_VOLUME);
   // rangecheck
   if(newAmb->volume < 0)
      newAmb->volume = 0;
   else if(newAmb->volume > 127)
      newAmb->volume = 127;

   // process attenuation
   tempstr = cfg_getstr(cfg, ITEM_AMB_ATTENUATION);
   newAmb->attenuation =
      E_StrToNumLinear(attenuation_types, NUM_ATTENUATION_TYPES, tempstr);
   if(newAmb->attenuation == NUM_ATTENUATION_TYPES)
   {
      E_EDFLoggedWarning(2, "Warning: ambience %d uses unknown attn type '%s'\n",
                         newAmb->index, tempstr);
      newAmb->attenuation = ATTN_NORMAL; // normal attenuation is fine
   }

   // process period variables
   newAmb->period    = cfg_getint(cfg, ITEM_AMB_PERIOD);
   newAmb->minperiod = cfg_getint(cfg, ITEM_AMB_MINPERIOD);
   newAmb->maxperiod = cfg_getint(cfg, ITEM_AMB_MAXPERIOD);

   // 01/16/14: reverb flag
   newAmb->reverb = !!cfg_getflag(cfg, ITEM_AMB_REVERB);

   E_EDFLogPrintf("\t\tFinished ambience #%d (index %d)\n", i, newAmb->index);
}

//
// E_ProcessAmbience
//
// Processes all EDF ambience sections.
//
void E_ProcessAmbience(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Processing ambience\n");

   unsigned int numambience = cfg_size(cfg, EDF_SEC_AMBIENCE);

   E_EDFLogPrintf("\t\t%d ambience section(s) defined\n", numambience);

   for(unsigned int i = 0; i < numambience; i++)
      E_ProcessAmbienceSec(cfg_getnsec(cfg, EDF_SEC_AMBIENCE, i), i);
}

// EOF


