// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "d_io.h"
#include "i_system.h"
#include "w_wad.h"
#include "d_dehtbl.h"
#include "sounds.h"
#include "i_sound.h"
#include "p_mobj.h"
#include "s_sound.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_lib.h"
#include "e_edf.h"
#include "e_sound.h"

//
// Sound keywords
//
#define ITEM_SND_LUMP          "lump"
#define ITEM_SND_PREFIX        "prefix"
#define ITEM_SND_SINGULARITY   "singularity"
#define ITEM_SND_PRIORITY      "priority"
#define ITEM_SND_LINK          "link"
#define ITEM_SND_SKININDEX     "skinindex"
#define ITEM_SND_LINKVOL       "linkvol"
#define ITEM_SND_LINKPITCH     "linkpitch"
#define ITEM_SND_CLIPPING_DIST "clipping_dist"
#define ITEM_SND_CLOSE_DIST    "close_dist"
#define ITEM_SND_DEHNUM        "dehackednum"

#define ITEM_DELTA_NAME "name"

//
// Static sound hash tables
//
#define NUMSFXCHAINS 257
static sfxinfo_t *sfxchains[NUMSFXCHAINS];
static sfxinfo_t *sfx_dehchains[NUMSFXCHAINS];

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
static const char *skinindices[] =
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
};

#define NUM_SKININDICES (sizeof(skinindices) / sizeof(char *))

#define SOUND_OPTIONS \
   CFG_STR(ITEM_SND_LUMP,          NULL,              CFGF_NONE), \
   CFG_BOOL(ITEM_SND_PREFIX,       cfg_true,          CFGF_NONE), \
   CFG_STR(ITEM_SND_SINGULARITY,   "sg_none",         CFGF_NONE), \
   CFG_INT(ITEM_SND_PRIORITY,      64,                CFGF_NONE), \
   CFG_STR(ITEM_SND_LINK,          "none",            CFGF_NONE), \
   CFG_STR(ITEM_SND_SKININDEX,     "sk_none",         CFGF_NONE), \
   CFG_INT(ITEM_SND_LINKVOL,       -1,                CFGF_NONE), \
   CFG_INT(ITEM_SND_LINKPITCH,     -1,                CFGF_NONE), \
   CFG_INT(ITEM_SND_CLIPPING_DIST, S_CLIPPING_DIST_I, CFGF_NONE), \
   CFG_INT(ITEM_SND_CLOSE_DIST,    S_CLOSE_DIST_I,    CFGF_NONE), \
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
   CFG_STR(ITEM_DELTA_NAME, NULL, CFGF_NONE),
   SOUND_OPTIONS
};

//
// E_SoundForName
//
// Returns a sfxinfo_t pointer given the EDF mnemonic for that
// sound. Will return NULL if the requested sound is not found.
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
// This version returns a pointer to S_sfx[0] if the special
// mnemonic "none" is passed in. This allows EDF to get the reserved
// DeHackEd number zero for it. Most other code segments should not
// use this function.
//
sfxinfo_t *E_EDFSoundForName(const char *name)
{
   if(!strcasecmp(name, "none"))
      return &S_sfx[0];

   return E_SoundForName(name);
}

//
// E_SoundForDEHNum
//
// Returns a sfxinfo_t pointer given the DeHackEd number for that
// sound. Will return NULL if the requested sound is not found.
//
sfxinfo_t *E_SoundForDEHNum(int dehnum)
{
   unsigned int hash = dehnum % NUMSFXCHAINS;
   sfxinfo_t  *rover = sfx_dehchains[hash];

   while(rover && rover->dehackednum != dehnum)
      rover = rover->dehnext;

   return rover;
}

//
// E_AddSoundToHash
//
// Adds a new sfxinfo_t structure to the hash table.
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

   if(E_SoundForDEHNum(sfx->dehackednum))
      return;

   if(sfx->dehackednum == 0)
      E_EDFLoggedErr(2, "E_AddSoundToDEHHash: dehackednum zero is reserved!\n");

   sfx->dehnext = sfx_dehchains[hash];
   sfx_dehchains[hash] = sfx;
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

boolean E_AutoAllocSoundDEHNum(sfxinfo_t *sfx)
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
   } while(dehnum > 0 && E_SoundForDEHNum(dehnum) != NULL);

   // ran out while searching for an unused number?
   if(dehnum <= 0)
      return false;

   // assign it!
   E_AddSoundToDEHHash(sfx);

   return true;
}


//
// E_NewWadSound
//
// Creates a sfxinfo_t structure for a new wad sound and hashes it.
//
void E_NewWadSound(const char *name)
{
   sfxinfo_t *sfx;
   char mnemonic[9];

   memset(mnemonic, 0, sizeof(mnemonic));
   strncpy(mnemonic, name+2, 9);

   sfx = E_EDFSoundForName(mnemonic);
   
   if(!sfx)
   {
      // create a new one and hook into hashchain
      sfx = Z_Malloc(sizeof(sfxinfo_t), PU_STATIC, 0);

      memset(sfx, 0, sizeof(sfxinfo_t));
      
      strncpy(sfx->name, name, 9);
      strncpy(sfx->mnemonic, mnemonic, 9);
      sfx->prefix = false;  // do not add another DS prefix      
      
      sfx->singularity = sg_none;
      sfx->priority = 64;
      sfx->link = NULL;
      sfx->pitch = sfx->volume = -1;
      sfx->clipping_dist = S_CLIPPING_DIST;
      sfx->close_dist = S_CLOSE_DIST;
      sfx->skinsound = 0;
      sfx->data = NULL;
      sfx->dehackednum = -1; // not accessible to DeHackEd
      
      E_AddSoundToHash(sfx);

      return;
   }
}

//
// E_PreCacheSounds
//
// Runs down the sound mnemonic hash table chains and caches all 
// sounds. This is improved from the code that was in SMMU, which 
// only precached entries in the S_sfx array. This is called at 
// startup when sound precaching is enabled.
//
void E_PreCacheSounds(void)
{
   int i;
   sfxinfo_t *cursfx;

   // run down all the mnemonic hash chains so that we precache 
   // all sounds, not just ones stored in S_sfx

   for(i = 0; i < NUMSFXCHAINS; ++i)
   {
      cursfx = sfxchains[i];

      while(cursfx)
      {
         I_CacheSound(cursfx);
         cursfx = cursfx->next;
      }
   }
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
static void E_ProcessSound(sfxinfo_t *sfx, cfg_t *section, boolean def)
{
   boolean setLink = false;

   // preconditions: 
   
   // sfx->mnemonic is valid, and this sfxinfo_t has already been 
   // added to the sound hash table earlier by E_ProcessSounds

   // process the lump name
   if(IS_SET(ITEM_SND_LUMP))
   {
      const char *lumpname;

      // if this is the definition, and the lump name is not
      // defined, duplicate the mnemonic as the sound name
      if(def && cfg_size(section, ITEM_SND_LUMP) == 0)
         strncpy(sfx->name, sfx->mnemonic, 9);
      else
      {
         lumpname = cfg_getstr(section, ITEM_SND_LUMP);

         strncpy(sfx->name, lumpname, 9);
      }
   }

   // process the prefix flag
   if(IS_SET(ITEM_SND_PREFIX))
      sfx->prefix = cfg_getbool(section, ITEM_SND_PREFIX);

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
      sfx->priority = cfg_getint(section, ITEM_SND_PRIORITY);

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

   // process dehackednum -- not in deltas!
   if(def)
   {
      sfx->dehackednum = cfg_getint(section, ITEM_SND_DEHNUM);

      if(sfx->dehackednum != -1)
         E_AddSoundToDEHHash(sfx); // add to DeHackEd num hash table
   }
}

//
// E_ProcessSounds
//
// Collects all the sound definitions and builds the sound hash
// tables.
//
void E_ProcessSounds(cfg_t *cfg)
{
   int i;

   E_EDFLogPuts("\t\tHashing sounds\n");

   // initialize S_sfx[0]
   strcpy(S_sfx[0].name, "none");
   strcpy(S_sfx[0].mnemonic, "none");

   // now, let's collect the mnemonics (this must be done ahead of time)
   for(i = 1; i < NUMSFX; ++i)
   {
      const char *mnemonic;
      cfg_t *sndsection = cfg_getnsec(cfg, EDF_SEC_SOUND, i - 1);

      mnemonic = cfg_title(sndsection);

      // verify the length -- haleyjd 06/03/06: doubled length limit
      if(strlen(mnemonic) > 32)
      {
         E_EDFLoggedErr(2, 
            "E_ProcessSounds: invalid sound mnemonic '%s'\n", mnemonic);
      }

      // copy it to the sound
      strncpy(S_sfx[i].mnemonic, mnemonic, 33);

      // add this sound to the hash table
      E_AddSoundToHash(&S_sfx[i]);
   }

   E_EDFLogPuts("\t\tProcessing data\n");

   // finally, process the individual sounds
   for(i = 1; i < NUMSFX; ++i)
   {
      cfg_t *section = cfg_getnsec(cfg, EDF_SEC_SOUND, i - 1);

      E_ProcessSound(&S_sfx[i], section, true);

      E_EDFLogPrintf("\t\tFinished sound %s(#%d)\n", S_sfx[i].mnemonic, i);
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
void E_ProcessSoundDeltas(cfg_t *cfg)
{
   int i, numdeltas;

   E_EDFLogPuts("\t* Processing sound deltas\n");

   numdeltas = cfg_size(cfg, EDF_SEC_SDELTA);

   E_EDFLogPrintf("\t\t%d sounddelta(s) defined\n", numdeltas);

   for(i = 0; i < numdeltas; i++)
   {
      const char *tempstr;
      sfxinfo_t *sfx;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_SDELTA, i);

      // get thingtype to edit
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

#define ITEM_SEQ_ID    "id"
#define ITEM_SEQ_CMDS  "cmds"
#define ITEM_SEQ_TYPE  "type"
#define ITEM_SEQ_STOP  "stopsound"
#define ITEM_SEQ_ATTN  "attenuation"
#define ITEM_SEQ_VOL   "volume"
#define ITEM_SEQ_NSCO  "nostopcutoff"
#define ITEM_SEQ_DOOR  "doorsequence"
#define ITEM_SEQ_PLAT  "platsequence"
#define ITEM_SEQ_FLOOR "floorsequence"
#define ITEM_SEQ_CEIL  "ceilingsequence"

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
   "sector",
   "environment",
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
   "delay",
   "delayrand",
   "end",

   // these are supported as properties as well
   "stopsound",
   "attenuation",
   "volume",
   "nostopcutoff",
};

#define NUM_SEQ_CMDS (sizeof(sndseq_cmdstrs) / sizeof(char *))

cfg_opt_t edf_seq_opts[] =
{
   CFG_INT(ITEM_SEQ_ID,    -1,        CFGF_NONE),
   CFG_STR(ITEM_SEQ_CMDS,  0,         CFGF_LIST),
   CFG_STR(ITEM_SEQ_TYPE,  "sector",  CFGF_NONE),
   CFG_STR(ITEM_SEQ_STOP,  "none",    CFGF_NONE),
   CFG_STR(ITEM_SEQ_ATTN,  "normal",  CFGF_NONE),
   CFG_INT(ITEM_SEQ_VOL,   127,       CFGF_NONE),
   CFG_BOOL(ITEM_SEQ_NSCO, cfg_false, CFGF_NONE),
   CFG_STR(ITEM_SEQ_DOOR,  NULL,      CFGF_NONE),
   CFG_STR(ITEM_SEQ_PLAT,  NULL,      CFGF_NONE),
   CFG_STR(ITEM_SEQ_FLOOR, NULL,      CFGF_NONE),
   CFG_STR(ITEM_SEQ_CEIL,  NULL,      CFGF_NONE),
   CFG_END()
};

#define NUM_EDFSEQ_CHAINS 127
static ESoundSeq_t *edf_seq_chains[NUM_EDFSEQ_CHAINS];
static ESoundSeq_t *edf_seq_numchains[NUM_EDFSEQ_CHAINS];

// need a separate hash for environmental sequences
#define NUM_EDFSEQ_ENVCHAINS 31
static ESoundSeq_t *edf_seq_envchains[NUM_EDFSEQ_ENVCHAINS];

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
   
   if(seq->type == SEQ_SECTOR)
   {
      idx = seq->index % NUM_EDFSEQ_CHAINS;
      
      M_DLListInsert((mdllistitem_t *)seq, 
                     (mdllistitem_t **)(&edf_seq_numchains[idx]));
   }
   else // environment sequences are hashed separately
   {
      idx = seq->index % NUM_EDFSEQ_ENVCHAINS;

      M_DLListInsert((mdllistitem_t *)seq,
                     (mdllistitem_t **)(&edf_seq_envchains[idx]));
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
   M_DLListRemove((mdllistitem_t *)seq);
}

//
// E_SequenceForName
//
// Returns an EDF sound sequence with the given name. If none exists,
// NULL will be returned.
//
ESoundSeq_t *E_SequenceForName(const char *name)
{
   unsigned int key = D_HashTableKey(name) % NUM_EDFSEQ_CHAINS;
   ESoundSeq_t *seq = edf_seq_chains[key];

   while(seq && strncasecmp(seq->name, name, 33))
      seq = seq->namenext;

   return seq;
}

//
// E_SequenceForNum
//
// Returns an EDF sound sequence with the given numeric id. If none exists,
// NULL will be returned.
//
ESoundSeq_t *E_SequenceForNum(unsigned int id)
{
   unsigned int key = id % NUM_EDFSEQ_CHAINS;
   ESoundSeq_t *seq = edf_seq_numchains[key];

   while(seq && seq->index != id)
      seq = (ESoundSeq_t *)(seq->numlinks.next);

   return seq;
}

//
// E_EnvironmentSequence
//
// Returns the environmental sound sequence with the given numeric id.
// If none exists, NULL will be returned.
//
ESoundSeq_t *E_EnvironmentSequence(unsigned int id)
{
   unsigned int key = id % NUM_EDFSEQ_ENVCHAINS;
   ESoundSeq_t *seq = edf_seq_envchains[key];

   while(seq && seq->index != id)
      seq = (ESoundSeq_t *)(seq->numlinks.next);

   return seq;
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

   // verify length
   if(strlen(name) > 32)
      E_EDFLoggedErr(2, "E_ProcessSndSeq: invalid mnemonic '%s'\n", name);

   // get numeric id and type now
   idnum = cfg_getint(cfg, ITEM_SEQ_ID);

   tempstr = cfg_getstr(cfg, ITEM_SEQ_TYPE);
   type = E_StrToNumLinear(seq_types, NUM_SEQ_TYPES, tempstr);
   if(type == NUM_SEQ_TYPES)
   {
      E_EDFLogPrintf("\t\tWarning: invalid sequence type '%s'\n", tempstr);
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
      newSeq = malloc(sizeof(ESoundSeq_t));

      // init to zero
      memset(newSeq, 0, sizeof(*newSeq));
      
      // copy keys into sequence object
      strncpy(newSeq->name, name, 33);
      
      newSeq->index = idnum;
      newSeq->type  = type;
            
      // add to hash tables
      
      E_AddSequenceToNameHash(newSeq);
      
      // numeric key is not required
      if(idnum >= 0)
         E_AddSequenceToNumHash(newSeq);
   }

   /*
   CFG_STR(ITEM_SEQ_CMDS,  0,         CFGF_LIST),
   */

   // process stopsound -- invalid sounds are not an error, and mean "none"
   tempstr = cfg_getstr(cfg, ITEM_SEQ_STOP);
   newSeq->stopsound = E_SoundForName(tempstr);

   // process attenuation
   tempstr = cfg_getstr(cfg, ITEM_SEQ_ATTN);
   newSeq->attenuation =
      E_StrToNumLinear(attenuation_types, NUM_ATTENUATION_TYPES, tempstr);
   if(newSeq->attenuation == NUM_ATTENUATION_TYPES)
   {
      E_EDFLogPrintf("\t\tWarning: sequence %d uses unknown attn type '%s'\n",
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

   // process nostopcutoff
   newSeq->nostopcutoff = (cfg_getbool(cfg, ITEM_SEQ_NSCO) == cfg_true);

   // process command list

   // if a command list already exists, destroy it
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
   CFG_END()
};

// ambience hash table
#define NUMAMBIENCECHAINS 67
static EAmbience_t *ambience_chains[NUMAMBIENCECHAINS];

//
// E_AmbienceForNum
//
// Given an ambience index, returns the ambience object.
// Returns NULL if no such ambience object exists.
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
   char *tempstr;
   int index;

   // issue a warning if index is undefined
   if(cfg_size(cfg, ITEM_AMB_INDEX) == 0)
   {
      E_EDFLogPrintf("\t\tWarning: ambience %d defines no index, "
                     "ambience index 0 may be overwritten.\n", i);
   }

   // get index
   index = cfg_getint(cfg, ITEM_AMB_INDEX);

   // if one already exists, use it, else create a new one
   if(!(newAmb = E_AmbienceForNum(index)))
   {
      newAmb = malloc(sizeof(EAmbience_t));

      // add to hash table
      newAmb->index = index;
      E_AddAmbienceToHash(newAmb);
   }

   // process type -- must be valid
   tempstr = cfg_getstr(cfg, ITEM_AMB_TYPE);
   newAmb->type = E_StrToNumLinear(ambience_types, NUM_AMBIENCE_TYPES, tempstr);
   if(newAmb->type == NUM_AMBIENCE_TYPES)
   {
      E_EDFLogPrintf("\t\tWarning: ambience %d uses bad type '%s'\n",
                     newAmb->index, tempstr);
      newAmb->type = 0; // use continuous as a default
   }

   // process sound -- note: may end up NULL, this is not an error
   tempstr = cfg_getstr(cfg, ITEM_AMB_SOUND);
   newAmb->sound = E_SoundForName(tempstr);
   if(!newAmb->sound)
   {
      // issue a warning just in case this is a mistake
      E_EDFLogPrintf("\t\tWarning: ambience %d references bad sound '%s'\n",
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
      E_EDFLogPrintf("\t\tWarning: ambience %d uses unknown attn type '%s'\n",
                     newAmb->index, tempstr);
      newAmb->attenuation = ATTN_NORMAL; // normal attenuation is fine
   }

   // process period variables
   newAmb->period    = cfg_getint(cfg, ITEM_AMB_PERIOD);
   newAmb->minperiod = cfg_getint(cfg, ITEM_AMB_MINPERIOD);
   newAmb->maxperiod = cfg_getint(cfg, ITEM_AMB_MAXPERIOD);

   E_EDFLogPrintf("\t\tFinished ambience #%d (index %d)\n", i, newAmb->index);
}

//
// E_ProcessAmbience
//
// Processes all EDF ambience sections.
//
void E_ProcessAmbience(cfg_t *cfg)
{
   unsigned int i, numambience;

   E_EDFLogPuts("\t* Processing ambience\n");

   numambience = cfg_size(cfg, EDF_SEC_AMBIENCE);

   E_EDFLogPrintf("\t\t%d ambience section(s) defined\n", numambience);

   for(i = 0; i < numambience; ++i)
      E_ProcessAmbienceSec(cfg_getnsec(cfg, EDF_SEC_AMBIENCE, i), i);
}

// EOF


