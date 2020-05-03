//
// The Eternity Engine
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
// Purpose: EDF States Module
// Authors: James Haley
//

#include "z_zone.h"
#include "i_system.h"
#include "p_partcl.h"
#include "d_io.h"
#include "d_dehtbl.h"
#include "info.h"
#include "m_qstr.h"
#include "p_pspr.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"

#include "e_lib.h"
#include "e_dstate.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_things.h"
#include "e_sound.h"
#include "e_sprite.h"
#include "e_string.h"
#include "e_states.h"
#include "e_args.h"

// 7/24/05: This is now global, for efficiency's sake

// The "S_NULL" state, which is required, has its number resolved
// in E_CollectStates
int NullStateNum;

// Frame section keywords
#define ITEM_FRAME_DECORATE  "decorate"
#define ITEM_FRAME_SPRITE    "sprite"
#define ITEM_FRAME_SPRFRAME  "spriteframe"
#define ITEM_FRAME_FULLBRT   "fullbright"
#define ITEM_FRAME_TICS      "tics"
#define ITEM_FRAME_ACTION    "action"
#define ITEM_FRAME_NEXTFRAME "nextframe"
#define ITEM_FRAME_MISC1     "misc1"
#define ITEM_FRAME_MISC2     "misc2"
#define ITEM_FRAME_PTCLEVENT "particle_event"
#define ITEM_FRAME_ARGS      "args"
#define ITEM_FRAME_DEHNUM    "dehackednum"
#define ITEM_FRAME_CMP       "cmp"
#define ITEM_FRAME_SKILL5FAST "SKILL5FAST"
#define ITEM_FRAME_INTERPOLATE "INTERPOLATE"

#define ITEM_DELTA_NAME      "name"

#define ITEM_FRAMEBLOCK_FDS    "firststate"
#define ITEM_FRAMEBLOCK_STATES "states"

// forward prototype for action function dispatcher
static int E_ActionFuncCB(cfg_t *cfg, cfg_opt_t *opt, int argc,
                          const char **argv);

//
// Frame Options
//

#define FRAME_FIELDS \
   CFG_STR(ITEM_FRAME_SPRITE,      "BLANK",     CFGF_NONE), \
   CFG_INT_CB(ITEM_FRAME_SPRFRAME, 0,           CFGF_NONE, E_SpriteFrameCB), \
   CFG_BOOL(ITEM_FRAME_FULLBRT,    false,       CFGF_NONE), \
   CFG_INT(ITEM_FRAME_TICS,        1,           CFGF_NONE), \
   CFG_STRFUNC(ITEM_FRAME_ACTION,  "NULL",      E_ActionFuncCB), \
   CFG_STR(ITEM_FRAME_NEXTFRAME,   "S_NULL",    CFGF_NONE), \
   CFG_STR(ITEM_FRAME_MISC1,       "0",         CFGF_NONE), \
   CFG_STR(ITEM_FRAME_MISC2,       "0",         CFGF_NONE), \
   CFG_STR(ITEM_FRAME_PTCLEVENT,   "pevt_none", CFGF_NONE), \
   CFG_STR(ITEM_FRAME_ARGS,        0,           CFGF_LIST), \
   CFG_INT(ITEM_FRAME_DEHNUM,      -1,          CFGF_NONE), \
   CFG_FLAG(ITEM_FRAME_SKILL5FAST, 0,           CFGF_SIGNPREFIX), \
   CFG_FLAG(ITEM_FRAME_INTERPOLATE, 0,          CFGF_SIGNPREFIX), \
   CFG_END()

cfg_opt_t edf_frame_opts[] =
{
   CFG_FLAG(ITEM_FRAME_DECORATE, 0, CFGF_SIGNPREFIX),
   CFG_STR(ITEM_FRAME_CMP, 0, CFGF_NONE),
   FRAME_FIELDS
};

cfg_opt_t edf_fdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   FRAME_FIELDS
};

cfg_opt_t edf_fblock_opts[] =
{
   CFG_STR(ITEM_FRAMEBLOCK_FDS,    nullptr, CFGF_NONE),
   CFG_STR(ITEM_FRAMEBLOCK_STATES, nullptr, CFGF_NONE),
   CFG_END()
};

static const dehflags_t frameFlagSet[] =
{
   { ITEM_FRAME_SKILL5FAST, STATEF_SKILL5FAST },
   { ITEM_FRAME_INTERPOLATE, STATEF_INTERPOLATE },
   { nullptr },
};

//
// State Hash Lookup Functions
//

// State hash tables

// State Hashing
#define NUMSTATECHAINS 2003

// hash by name
static EHashTable<state_t, ENCStringHashKey, 
                  &state_t::name, &state_t::namelinks> state_namehash(NUMSTATECHAINS);

// hash by DeHackEd number
static EHashTable<state_t, EIntHashKey, 
                  &state_t::dehnum, &state_t::numlinks> state_numhash(NUMSTATECHAINS);

//
// State DeHackEd numbers *were* simply the actual, internal state
// number, but we have to actually store and hash them for EDF to
// remain cross-version compatible. If a state with the requested
// dehnum isn't found, -1 is returned.
//
int E_StateNumForDEHNum(int dehnum)
{
   state_t *st = nullptr;
   int ret = -1;

   // 08/31/03: return null state for negative numbers, to
   // please some old, incorrect DeHackEd patches
   if(dehnum < 0)
      ret = NullStateNum;
   else if((st = state_numhash.objectForKey(dehnum)))
      ret = st->index;

   return ret;
}

//
// As above, but causes a fatal error if the state isn't found,
// rather than returning -1. This keeps error checking code
// from being necessitated all over the source code.
//
int E_GetStateNumForDEHNum(int dehnum)
{
   int statenum = E_StateNumForDEHNum(dehnum);

   if(statenum < 0)
      I_Error("E_GetStateNumForDEHNum: invalid deh num %d\n", dehnum);

   return statenum;
}

//
// As above, but returns index of S_NULL state if the requested
// one was not found.
//
int E_SafeState(int dehnum)
{
   int statenum = E_StateNumForDEHNum(dehnum);

   if(statenum < 0)
      statenum = NullStateNum;

   return statenum;
}

//
// Returns the number of a state given its name. Returns -1
// if the state is not found.
//
int E_StateNumForName(const char *name)
{
   state_t *st = nullptr;
   int ret = -1;

   if((st = state_namehash.objectForKey(name)))
      ret = st->index;

   return ret;
}

//
// As above, but causes a fatal error if the state doesn't exist.
//
int E_GetStateNumForName(const char *name)
{
   int statenum = E_StateNumForName(name);

   if(statenum < 0)
      I_Error("E_GetStateNumForName: bad frame %s\n", name);

   return statenum;
}

//
// As above, but returns index of S_NULL if result of a name lookup
// is not found.
//
int E_SafeStateName(const char *name)
{
   int statenum = E_StateNumForName(name);

   if(statenum < 0)
      statenum = NullStateNum;

   return statenum;
}

//
// Allows lookup of what may either be an EDF global state name, DECORATE state 
// label relative to a particular mobjinfo, or a state DeHackEd number.
//
int E_SafeStateNameOrLabel(const mobjinfo_t *mi, const char *name)
{
   char *pos = nullptr;
   long  num = strtol(name, &pos, 0);

   // Not a number? It is a state name.
   if(estrnonempty(pos))
   {
      int      statenum;
      const state_t *state = nullptr;
      
      // Try global resolution first.
      if((statenum = E_StateNumForName(name)) < 0)
      {
         // Try DECORATE state label resolution.
         if((state = E_GetJumpInfo(mi, name)))
            statenum = state->index;
      }

      return statenum;
   }
   else
      return E_SafeState((int)num); // DeHackEd number
}

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_state_dehnum = D_MAXINT;

//
// Automatic allocation of dehacked numbers allows states to be used with
// parameterized codepointers without having had a DeHackEd number explicitly
// assigned to them by the EDF author. This was requested by several users
// after v3.33.02.
//
bool E_AutoAllocStateDEHNum(int statenum)
{
   int dehnum;
   state_t *st = states[statenum];

#ifdef RANGECHECK
   if(st->dehnum != -1)
      I_Error("E_AutoAllocStateDEHNum: called for state with valid dehnum\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_state_dehnum < 0)
      return false;

   do
   {
      dehnum = edf_alloc_state_dehnum--;
   } 
   while(dehnum >= 0 && E_StateNumForDEHNum(dehnum) >= 0);

   // ran out while looking for an unused number?
   if(dehnum < 0)
      return false;

   // assign it!
   st->dehnum = dehnum;
   state_numhash.addObject(st);

   return true;
}

//
// EDF Processing Routines
//

//
// Counts the number of states in a cfg which do not overwrite
// any pre-existing states by virtue of having the same name.
//
static unsigned int E_CountUniqueStates(cfg_t *cfg, unsigned int numstates)
{
   unsigned int i;
   unsigned int count = 0;

   // if the state name hash is empty, short-circuit for efficiency
   if(!state_namehash.getNumItems())
      return numstates;

   for(i = 0; i < numstates; ++i)
   {
      cfg_t *statecfg  = cfg_getnsec(cfg, EDF_SEC_FRAME, i);
      const char *name = cfg_title(statecfg);

      // if not in the name table, count it
      if(E_StateNumForName(name) < 0)
         ++count;
   }

   return count;
}

//
// Function to reallocate the states array safely.
//
void E_ReallocStates(int numnewstates)
{
   static int numstatesalloc = 0;

   // only realloc when needed
   if(!numstatesalloc || (NUMSTATES < numstatesalloc + numnewstates))
   {
      int i;

      // First time, just allocate the requested number of states.
      // Afterward:
      // * If the number of states requested is small, add 2 times as many
      //   requested, plus a small constant amount.
      // * If the number is large, just add that number.

      if(!numstatesalloc)
         numstatesalloc = numnewstates;
      else if(numnewstates <= 50)
         numstatesalloc += numnewstates * 2 + 32;
      else
         numstatesalloc += numnewstates;

      // reallocate states[]
      states = erealloc(state_t **, states, numstatesalloc * sizeof(state_t *));

      // set the new state pointers to NULL
      for(i = NUMSTATES; i < numstatesalloc; ++i)
         states[i] = nullptr;
   }

   // increment NUMSTATES
   NUMSTATES += numnewstates;
}

//
// Pre-creates and hashes by name the states, for purpose of mutual 
// and forward references.
//
void E_CollectStates(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numstates;         // number of states defined by the cfg
   unsigned int numnew;            // number of states that are new
   unsigned int curnewstate = 0;   // index of current new state being used
   state_t *statestructs = nullptr;
   static bool firsttime = true;

   // get number of states defined by the cfg
   numstates = cfg_size(cfg, EDF_SEC_FRAME);

   // get number of new states in the cfg
   numnew = E_CountUniqueStates(cfg, numstates);

   // echo counts
   E_EDFLogPrintf("\t\t%u states defined (%u new)\n", numstates, numnew);

   if(numnew)
   {
      unsigned firstnewstate = 0;   // index of first new state
      // allocate state_t structures for the new states
      statestructs = estructalloc(state_t, numnew);

      // add space to the states array
      curnewstate = firstnewstate = NUMSTATES;

      E_ReallocStates((int)numnew);
      
      // set pointers in states[] to the proper structures
      for(i = firstnewstate; i < (unsigned int)NUMSTATES; ++i)
      {
         states[i] = &statestructs[i - firstnewstate];

         // haleyjd 06/15/09: set state index
         states[i]->index = i;
      }
   }

   // build hash tables
   E_EDFLogPuts("\t\tBuilding state hash tables\n");

   // cycle through the states defined in the cfg
   for(i = 0; i < numstates; ++i)
   {
      cfg_t *statecfg  = cfg_getnsec(cfg, EDF_SEC_FRAME, i);
      const char *name = cfg_title(statecfg);
      int statenum;

      if((statenum = E_StateNumForName(name)) >= 0)
      {
         int dehnum;

         // a state already exists by this name
         state_t *st = states[statenum];

         // get dehackednum of libConfuse definition
         dehnum = cfg_getint(statecfg, ITEM_FRAME_DEHNUM);

         // if not equal to current state dehnum...
         if(dehnum != st->dehnum)
         {
            // if state has a valid dehnum, remove it from the deh hash
            if(st->dehnum >= 0)
               state_numhash.removeObject(st);

            // assign the new dehnum
            st->dehnum = dehnum;

            // if valid, add it back to the hash with the new id #
            if(st->dehnum >= 0)
               state_numhash.addObject(st);
         }
      }
      else
      {
         // this is a new state
         state_t *st = states[curnewstate++];

         // initialize name
         st->name = estrdup(name);

         // add to name hash
         state_namehash.addObject(st);

         // get dehackednum and add state to dehacked hash table if valid
         if((st->dehnum = cfg_getint(statecfg, ITEM_FRAME_DEHNUM)) >= 0)
            state_numhash.addObject(st);
      }
   }

   // first-time-only events
   if(firsttime)
   {
      // check that at least one frame was defined
      if(!NUMSTATES)
         E_EDFLoggedErr(2, "E_CollectStates: no frames defined.\n");

      // verify the existence of the S_NULL frame
      NullStateNum = E_StateNumForName("S_NULL");
      if(NullStateNum < 0)
         E_EDFLoggedErr(2, "E_CollectStates: 'S_NULL' frame must be defined.\n");

      firsttime = false;
   }
}

//
// Creates an arglist object for the state, if it does not already have one. 
// Otherwise, the existing arguments are disposed of.
//
void E_CreateArgList(state_t *state)
{
   if(!state->args)
      state->args = estructalloc(arglist_t, 1); // create one
   else
      E_DisposeArgs(state->args);               // clear it out
}

// frame field parsing routines

//
// Isolated code to process the frame sprite field.
//
static void E_StateSprite(const char *tempstr, int i)
{
   // check for special 'BLANK' identifier
   if(!strcasecmp(tempstr, "BLANK"))
      states[i]->sprite = blankSpriteNum;
   else
   {
      int sprnum = E_SpriteNumForName(tempstr);

      if(sprnum == -1)
      {
         // haleyjd 03/24/10: add implicitly-defined sprite
         if(!E_ProcessSingleSprite(tempstr))
         {
            E_EDFLoggedWarning(2, 
                               "Warning: frame '%s': couldn't implicitly "
                               "define sprite '%s'\n", 
                               states[i]->name, tempstr);
            sprnum = blankSpriteNum;
         }
         else
            sprnum = E_SpriteNumForName(tempstr);
      }
      states[i]->sprite = sprnum;
   }
}

//
// Callback function for the new function-valued string option used to 
// specify state action functions. This is called during parsing, not 
// processing, and thus we do not look up/resolve anything at this point.
// We are only interested in populating the cfg's args values with the 
// strings passed to this callback as parameters. The value of the option has
// already been set to the name of the codepointer by the libConfuse framework.
//
static int E_ActionFuncCB(cfg_t *cfg, cfg_opt_t *opt, int argc, 
                          const char **argv)
{
   if(argc > 0)
      cfg_setlistptr(cfg, "args", argc, (const void *)argv);

   return 0; // everything is good
}

//
// Isolated code to process the frame action field.
//
static void E_StateAction(const char *tempstr, int i)
{
   deh_bexptr *dp = D_GetBexPtr(tempstr);
   
   if(!dp)
   {
      E_EDFLoggedErr(2, "E_ProcessState: frame '%s': bad action '%s'\n",
                     states[i]->name, tempstr);
   }

   states[i]->action = states[i]->oldaction = dp->cptr;
}

enum prefixkwd_e
{
   PREFIX_FRAME,
   PREFIX_THING,
   PREFIX_SOUND,
   PREFIX_FLAGS,
   PREFIX_FLAGS2,
   PREFIX_FLAGS3,
   PREFIX_FLAGS4,
   PREFIX_BEXPTR,
   PREFIX_STRING,
   NUM_MISC_PREFIXES
};

static const char *misc_prefixes[NUM_MISC_PREFIXES] =
{
   "frame",  "thing",  "sound", "flags", "flags2", "flags3", "flags4",
   "bexptr", "string"
};

static void E_AssignMiscThing(int *target, int thingnum)
{
   // 09/19/03: add check for no dehacked number
   // 03/22/06: auto-allocate dehacked numbers where possible
   if(mobjinfo[thingnum]->dehnum >= 0 || E_AutoAllocThingDEHNum(thingnum))
      *target = mobjinfo[thingnum]->dehnum;
   else
   {
      E_EDFLoggedWarning(2, 
                         "Warning: failed to auto-allocate DeHackEd number "
                         "for thing %s\n", mobjinfo[thingnum]->name);
      *target = UnknownThingType;
   }
}

static void E_AssignMiscState(int *target, int framenum)
{
   // 09/19/03: add check for no dehacked number
   // 03/22/06: auto-allocate dehacked numbers where possible
   if(states[framenum]->dehnum >= 0 || E_AutoAllocStateDEHNum(framenum))
      *target = states[framenum]->dehnum;
   else
   {
      E_EDFLoggedWarning(2,
                         "Warning: failed to auto-allocate DeHackEd number "
                         "for frame %s\n", states[framenum]->name);
      *target = NullStateNum;
   }
}

static void E_AssignMiscSound(int *target, sfxinfo_t *sfx)
{
   // 01/04/09: check for NULL just in case
   if(!sfx)
      sfx = &NullSound;

   // 03/22/06: auto-allocate dehacked numbers where possible
   if(sfx->dehackednum >= 0 || E_AutoAllocSoundDEHNum(sfx))
      *target = sfx->dehackednum;
   else
   {
      E_EDFLoggedWarning(2, 
                         "Warning: failed to auto-allocate DeHackEd number "
                         "for sound %s\n", sfx->mnemonic);
      *target = 0;
   }
}

static void E_AssignMiscString(int *target, edf_string_t *str, const char *name)
{
   if(!str || str->numkey < 0)
   {
      E_EDFLoggedWarning(2, "Warning: bad string %s\n", name);
      *target = 0;
   }
   else
      *target = str->numkey;
}

static void E_AssignMiscBexptr(int *target, deh_bexptr *dp, const char *name)
{
   if(!dp)
      E_EDFLoggedErr(2, "E_ParseMiscField: bad bexptr '%s'\n", name);
   
   // get the index of this deh_bexptr in the master
   // deh_bexptrs array, and store it in the arg field
   *target = eindex(dp - deh_bexptrs);
}

//
// This function implements the quite powerful prefix:value syntax
// for misc and args fields in frames. Some of the code within may
// be candidate for generalization, since other fields may need
// this syntax in the near future.
//
static void E_ParseMiscField(const char *value, int *target)
{
   char prefix[16];
   const char *colonloc;
   
   memset(prefix, 0, 16);

   // look for a colon ending a possible prefix
   colonloc = E_ExtractPrefix(value, prefix, 16);
   
   if(colonloc)
   {
      // a colon was found, so identify the prefix
      const char *strval = colonloc + 1;

      int i = E_StrToNumLinear(misc_prefixes, NUM_MISC_PREFIXES, prefix);

      switch(i)
      {
      case PREFIX_FRAME:
         {
            int framenum = E_StateNumForName(strval);
            if(framenum < 0)
            {
               E_EDFLoggedWarning(2, "tWarning: invalid state '%s' in misc field\n",
                                  strval);
               *target = NullStateNum;
            }
            else
               E_AssignMiscState(target, framenum);
         }
         break;
      case PREFIX_THING:
         {
            int thingnum = E_ThingNumForName(strval);
            if(thingnum == -1)
            {
               E_EDFLoggedWarning(2, "Warning: invalid thing '%s' in misc field\n",
                                  strval);
               *target = UnknownThingType;
            }
            else
               E_AssignMiscThing(target, thingnum);
         }
         break;
      case PREFIX_SOUND:
         {
            sfxinfo_t *sfx = E_EDFSoundForName(strval);
            if(!sfx)
            {
               // haleyjd 05/31/06: relaxed to warning
               E_EDFLoggedWarning(2, "Warning: invalid sound '%s' in misc field\n", 
                                  strval);
               sfx = &NullSound;
            }
            E_AssignMiscSound(target, sfx);
         }
         break;
      case PREFIX_FLAGS:
         *target = (int)deh_ParseFlagsSingle(strval, DEHFLAGS_MODE1);
         break;
      case PREFIX_FLAGS2:
         *target = (int)deh_ParseFlagsSingle(strval, DEHFLAGS_MODE2);
         break;
      case PREFIX_FLAGS3:
         *target = (int)deh_ParseFlagsSingle(strval, DEHFLAGS_MODE3);
         break;
      case PREFIX_FLAGS4:
         *target = (int)deh_ParseFlagsSingle(strval, DEHFLAGS_MODE4);
         break;
      case PREFIX_BEXPTR:
         {
            deh_bexptr *dp = D_GetBexPtr(strval);
            E_AssignMiscBexptr(target, dp, strval);
         }
         break;
      case PREFIX_STRING:
         {
            edf_string_t *str = E_StringForName(strval);
            E_AssignMiscString(target, str, strval);
         }
         break;
      default:
         E_EDFLoggedWarning(2, "Warning: unknown value prefix '%s'\n",
                            prefix);
         *target = 0;
         break;
      }
   }
   else
   {
      char  *endptr;
      int    val;

      // see if it is a number
      if(strchr(value, '.')) // has a decimal point?
      {
         double dval = strtod(value, &endptr);

         // convert result to fixed-point
         val = (fixed_t)(dval * FRACUNIT);
      }
      else
      {
         // 11/11/03: use strtol to support hex and oct input
         val = static_cast<int>(strtol(value, &endptr, 0));
      }

      // haleyjd 04/02/08:
      // no? then try certain namespaces in a predefined order of precedence
      if(*endptr != '\0')
      {
         int temp;
         sfxinfo_t *sfx;
         edf_string_t *str;
         deh_bexptr *dp;
         
         if((temp = E_ThingNumForName(value)) != -1)           // thingtype?
            E_AssignMiscThing(target, temp);
         else if((temp = E_StateNumForName(value)) >= 0)       // frame?
            E_AssignMiscState(target, temp);
         else if((sfx = E_EDFSoundForName(value)) != nullptr)  // sound?
            E_AssignMiscSound(target, sfx);
         else if((str = E_StringForName(value)) != nullptr)    // string?
            E_AssignMiscString(target, str, value);
         else if((dp = D_GetBexPtr(value)) != nullptr)         // bexptr???
            E_AssignMiscBexptr(target, dp, value);
      }
      else
         *target = val;
   }
}

//
// Skip over any prefix in the argument string.
//
static const char *E_GetArgument(const char *value)
{
   char prefix[16];
   const char *colonloc;
   
   memset(prefix, 0, 16);

   // look for a colon ending a possible prefix
   colonloc = E_ExtractPrefix(value, prefix, 16);

   return colonloc ? colonloc + 1 : value;
}

enum nspeckwd_e
{
   NSPEC_NEXT,
   NSPEC_PREV,
   NSPEC_THIS,
   NSPEC_NULL,
   NUM_NSPEC_KEYWDS
};

static const char *nspec_keywds[NUM_NSPEC_KEYWDS] =
{
   "next", "prev", "this", "null"
};

//
// Returns a frame number for a special nextframe value.
//
static int E_SpecialNextState(const char *string, int framenum)
{
   int i, nextnum = 0;
   const char *value = string + 1;

   i = E_StrToNumLinear(nspec_keywds, NUM_NSPEC_KEYWDS, value);

   switch(i)
   {
   case NSPEC_NEXT:
      if(framenum == NUMSTATES - 1) // can't do it
      {
         E_EDFLoggedErr(2, "E_SpecialNextState: invalid frame #%d\n",
                        NUMSTATES);
      }
      nextnum = framenum + 1;
      break;
   case NSPEC_PREV:
      if(framenum == 0) // can't do it
         E_EDFLoggedErr(2, "E_SpecialNextState: invalid frame -1\n");
      nextnum = framenum - 1;
      break;
   case NSPEC_THIS:
      nextnum = framenum;
      break;
   case NSPEC_NULL:
      nextnum = NullStateNum;
      break;
   default: // ???
      E_EDFLoggedErr(2, "E_SpecialNextState: invalid specifier '%s'\n",
                     value);
   }

   return nextnum;
}

//
// Isolated code to process the frame nextframe field.
//
static void E_StateNextFrame(const char *tempstr, int i)
{
   int tempint = 0;

   // 11/07/03: allow special values in the nextframe field
   if(tempstr[0] == '@')
   {
      tempint = E_SpecialNextState(tempstr, i);
   }
   else if((tempint = E_StateNumForName(tempstr)) < 0)
   {
      char *endptr = nullptr;
      int result;

      result = (int)strtol(tempstr, &endptr, 0);
      if(*endptr == '\0')
      {
         // check for DeHackEd num specification;
         // the resulting value must be a valid frame deh number
         tempint = E_GetStateNumForDEHNum(result);
      }
      else      
      {
         // error
         E_EDFLoggedErr(2, 
            "E_ProcessState: frame '%s': bad nextframe '%s'\n",
            states[i]->name, tempstr);

      }
   }

   states[i]->nextstate = tempint;
}

//
// Isolated code to process the frame particle event field.
//
static void E_StatePtclEvt(const char *tempstr, int i)
{
   int tempint = 0;

   while(tempint != P_EVENT_NUMEVENTS &&
         strcasecmp(tempstr, particleEvents[tempint].name))
   {
      ++tempint;
   }
   if(tempint == P_EVENT_NUMEVENTS)
   {
      E_EDFLoggedErr(2, 
         "E_ProcessState: frame '%s': bad ptclevent '%s'\n",
         states[i]->name, tempstr);
   }

   states[i]->particle_evt = tempint;
}

// Hack for function-style arguments to action function
static bool in_action;
static bool early_args_found;
static bool early_args_end;

//
// A lexer function for the frame cmp field.
// Used by E_ProcessCmpState below.
//
static char *E_CmpTokenizer(const char *text, int *index, qstring *token)
{
   char c;
   int state = 0;

   // if we're already at the end, return NULL
   if(text[*index] == '\0')
      return nullptr;

   token->clear();

   while((c = text[*index]) != '\0')
   {
      *index += 1;
      switch(state)
      {
      case 0: // default state
         switch(c)
         {
         case ' ':
         case '\t':
            continue;  // skip whitespace
         case '"':
            state = 1; // enter quoted part
            continue;
         case '\'':
            state = 2; // enter quoted part (single quote support)
            continue;
         case '|':     // end of current token
         case ',':     // 03/01/05: added by user request
            return token->getBuffer();
         case '(':
            if(in_action)
            {
               early_args_found = true;
               return token->getBuffer();
            }
            *token += c;
            continue;
         case ')':
            if(in_action && early_args_found)
            {
               early_args_end = true;
               continue;
            }
            // fall through
         default:      // everything else == part of value
            *token += c;
            continue;
         }
      case 1: // in quoted area (double quotes)
         if(c == '"') // end of quoted area
            state = 0;
         else
            *token += c; // everything inside is literal
         continue;
      case 2: // in quoted area (single quotes)
         if(c == '\'') // end of quoted area
            state = 0;
         else
            *token += c; // everything inside is literal
         continue;
      default:
         E_EDFLoggedErr(0, "E_CmpTokenizer: internal error - undefined lexer state\n");
      }
   }

   // return final token, next call will return null
   return token->getBuffer();
}

// macros for E_ProcessCmpState:

// NEXTTOKEN: calls E_CmpTokenizer to get the next token

#define NEXTTOKEN() curtoken = E_CmpTokenizer(value, &tok_index, &buffer)

// DEFAULTS: tests if the string value is either null or equal to "*"

#define DEFAULTS(value)  (!(value) || (value)[0] == '*')

//
// Code to process a compressed state definition. Compressed state
// definitions are just a string with each frame field in a set order,
// delimited by pipes. This is very similar to DDF's frame specification,
// and has been requested by multiple users.
//
// Compressed format:
// "sprite|spriteframe|fullbright|tics|action|nextframe|ptcl|misc|args"
//
// haleyjd 04/03/08: An alternate syntax is also now supported:
// "sprite|spriteframe|fullbright|tics|action(args,...)|nextframe|ptcl|misc"
//
// This new syntax for argument specification is preferred, and the old
// one is now deprecated.
//
// Fields at the end can be left off. "*" in a field means to use
// the normal default value.
//
// haleyjd 06/24/04: rewritten to use a finite-state-automaton lexer, 
// making the format MUCH more flexible than it was under the former 
// strtok system. The E_CmpTokenizer function above performs the 
// lexing, returning each token in the qstring provided to it.
//
static void E_ProcessCmpState(const char *value, int i)
{
   qstring buffer;
   char *curtoken = nullptr;
   int tok_index = 0;

   // initialize tokenizer variables
   in_action = false;
   early_args_found = false;
   early_args_end = false;

   // process sprite
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->sprite = blankSpriteNum;
   else
      E_StateSprite(curtoken, i);

   // process spriteframe
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->frame = 0;
   else
   {
      // call the value-parsing callback explicitly
      if(E_SpriteFrameCB(nullptr, nullptr, curtoken, &(states[i]->frame)) == -1)
      {
         E_EDFLoggedErr(2, 
            "E_ProcessCmpState: frame '%s': bad spriteframe '%s'\n",
            states[i]->name, curtoken);
      }

      // haleyjd 09/22/07: if blank sprite, force to frame 0
      if(states[i]->sprite == blankSpriteNum)
         states[i]->frame = 0;
   }

   // process fullbright
   NEXTTOKEN();
   if(DEFAULTS(curtoken) == 0)
   {
      if(curtoken[0] == 't' || curtoken[0] == 'T')
         states[i]->frame |= FF_FULLBRIGHT;
   }

   // process tics
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->tics = 1;
   else
      states[i]->tics = static_cast<int>(strtol(curtoken, nullptr, 0));

   // process action
   in_action = true;
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->action = nullptr;
   else
      E_StateAction(curtoken, i);

   // haleyjd 04/03/08: check for early args found by tokenizer
   if(early_args_found)
   {
      // give the frame an arg list, or clear it out if it has one already
      E_CreateArgList(states[i]);

      // process args
      while(!early_args_end)
      {
         NEXTTOKEN();

         if(!DEFAULTS(curtoken))
            E_AddArgToList(states[i]->args, E_GetArgument(curtoken));
      }
   }

   in_action = false;

   // process nextframe
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->nextstate = NullStateNum;
   else
      E_StateNextFrame(curtoken, i);

   // process particle event
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->particle_evt = 0;
   else
      E_StatePtclEvt(curtoken, i);

   // process misc1, misc2
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->misc1 = 0;
   else
      E_ParseMiscField(curtoken, &(states[i]->misc1));

   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->misc2 = 0;
   else
      E_ParseMiscField(curtoken, &(states[i]->misc2));

   // NOTE: Argument specification at the end of cmp frames is deprecated!
   // Do not use this syntax any more. It will not be extended to support
   // more than 5 arguments.
   // Use DECORATE-style specification inside parentheses after the action
   // function name instead.

   if(!early_args_found) // do not do if early args specified
   {
      // give the frame an args list, or clear out its existing one
      E_CreateArgList(states[i]);

      // process args
      for(int j = 0; j < 5; ++j) // Only 5 args are supported here. Deprecated.
      {
         NEXTTOKEN();

         if(!DEFAULTS(curtoken))
            E_AddArgToList(states[i]->args, E_GetArgument(curtoken));
      }
   }

   early_args_found = early_args_end = false;
}

#undef NEXTTOKEN
#undef DEFAULTS

// IS_SET: this macro tests whether or not a particular field should
// be set. When applying deltas, we should not retrieve defaults.

#undef  IS_SET
#define IS_SET(name) (def || cfg_size(framesec, (name)) > 0)

//
// Generalized code to process the data for a single state
// structure. Doubles as code for frame and framedelta.
//
static void E_ProcessState(int i, cfg_t *framesec, bool def)
{
   int j;
   int tempint;
   const char *tempstr;

   // 11/14/03:
   // In definitions only, see if the cmp field is defined. If so,
   // we go into it with E_ProcessCmpState above, and ignore most
   // other fields in the frame block.
   // 01/01/12:
   // In definitions only, see if this state is reserved for definition
   // in a DECORATE state block by a thingtype.
   if(def)
   {
      int decoratestate = cfg_getflag(framesec, ITEM_FRAME_DECORATE);

      if(decoratestate)
      {
         states[i]->flags |= STATEFI_DECORATE;
         goto hitdecorate; // skip most processing
      }
      else
         states[i]->flags &= ~STATEFI_DECORATE;

      if(cfg_size(framesec, ITEM_FRAME_CMP) > 0)
      {
         tempstr = cfg_getstr(framesec, ITEM_FRAME_CMP);
         
         E_ProcessCmpState(tempstr, i);
         def = false; // process remainder as if a frame delta
         goto hitcmp;
      }
   }

   // process sprite
   if(IS_SET(ITEM_FRAME_SPRITE))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_SPRITE);

      E_StateSprite(tempstr, i);
   }

   // process spriteframe
   if(IS_SET(ITEM_FRAME_SPRFRAME))
      states[i]->frame = cfg_getint(framesec, ITEM_FRAME_SPRFRAME);

   // haleyjd 09/22/07: if sprite == blankSpriteNum, force to frame 0
   if(states[i]->sprite == blankSpriteNum)
      states[i]->frame = 0;

   // check for fullbright
   if(IS_SET(ITEM_FRAME_FULLBRT))
   {
      if(cfg_getbool(framesec, ITEM_FRAME_FULLBRT))
         states[i]->frame |= FF_FULLBRIGHT;
   }

   // process tics
   if(IS_SET(ITEM_FRAME_TICS))
      states[i]->tics = cfg_getint(framesec, ITEM_FRAME_TICS);

   // resolve codepointer
   if(IS_SET(ITEM_FRAME_ACTION))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_ACTION);

      E_StateAction(tempstr, i);
   }

   // process nextframe
   if(IS_SET(ITEM_FRAME_NEXTFRAME))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_NEXTFRAME);
      
      E_StateNextFrame(tempstr, i);
   }

   // 03/30/05: the following fields are now also allowed in cmp frames
hitcmp:
   // args field parsing (even more complicated, but similar)
   // Note: deltas can only set the entire args list at once, not
   // just parts of it.
   if(IS_SET(ITEM_FRAME_ARGS))
   {
      tempint = cfg_size(framesec, ITEM_FRAME_ARGS);

      // create an arg list for the state, or clear out the existing one
      E_CreateArgList(states[i]);

      for(j = 0; j < tempint; ++j)
      {
         tempstr = cfg_getnstr(framesec, ITEM_FRAME_ARGS, j);
         
         E_AddArgToList(states[i]->args, E_GetArgument(tempstr));
      }
   }

   // Set flags
   E_SetFlagsFromPrefixCfg(framesec, states[i]->flags, frameFlagSet);

   // 01/01/12: the following fields are allowed when processing a 
   // DECORATE-reserved state
hitdecorate:
   // misc field parsing (complicated)

   if(IS_SET(ITEM_FRAME_MISC1))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_MISC1);
      E_ParseMiscField(tempstr, &(states[i]->misc1));
   }

   if(IS_SET(ITEM_FRAME_MISC2))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_MISC2);
      E_ParseMiscField(tempstr, &(states[i]->misc2));
   }

   // process particle event
   if(IS_SET(ITEM_FRAME_PTCLEVENT))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_PTCLEVENT);

      E_StatePtclEvt(tempstr, i);
   }
}

//
// Resolves and loads all information for the state_t structures.
//
void E_ProcessStates(cfg_t *cfg)
{
   unsigned int i, numstates;

   E_EDFLogPuts("\t* Processing frame data\n");

   numstates = cfg_size(cfg, EDF_SEC_FRAME);

   for(i = 0; i < numstates; ++i)
   {
      cfg_t *framesec  = cfg_getnsec(cfg, EDF_SEC_FRAME, i);
      const char *name = cfg_title(framesec);
      int statenum     = E_StateNumForName(name);

      E_ProcessState(statenum, framesec, true);

      E_EDFLogPrintf("\t\tFinished frame %s (#%d)\n", 
                     states[statenum]->name, statenum);
   }
}

//
// Does processing for framedelta sections, which allow cascading
// editing of existing frames. The framedelta shares most of its
// fields and processing code with the frame section.
//
void E_ProcessStateDeltas(cfg_t *cfg)
{
   int i, numdeltas;

   E_EDFLogPuts("\t* Processing frame deltas\n");

   numdeltas = cfg_size(cfg, EDF_SEC_FRMDELTA);

   E_EDFLogPrintf("\t\t%d framedelta(s) defined\n", numdeltas);

   for(i = 0; i < numdeltas; ++i)
   {
      const char *tempstr;
      int stateNum;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_FRMDELTA, i);

      // get state to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessFrameDeltas: framedelta requires name field\n");

      tempstr = cfg_getstr(deltasec, ITEM_DELTA_NAME);
      stateNum = E_GetStateNumForName(tempstr);

      E_ProcessState(stateNum, deltasec, false);

      E_EDFLogPrintf("\t\tApplied framedelta #%d to %s(#%d)\n",
                     i, states[stateNum]->name, stateNum);
   }
}

//=============================================================================
//
// Frame block processing
//

//
// Resolve a single external goto target for a global frameblock. If the goto
// target is invalid in any way, the goto target is set to NullStateNum.
//
static void E_processFrameBlockGoto(const egoto_t &gotoDef)
{
   int statenum = NullStateNum;
   int calcstatenum;

   if((calcstatenum = E_StateNumForName(gotoDef.label)) >= 0)
   {
      // get state and compute goto offset
      state_t *state = states[calcstatenum];
      calcstatenum = state->index + gotoDef.offset;

      if(calcstatenum >= 0 && calcstatenum < NUMSTATES)
      {
         // success, this is a valid state
         statenum = calcstatenum;
      }
      else
         E_EDFLoggedWarning(2, "Invalid goto offset %d for state '%s'\n", gotoDef.offset, state->name);
   }
   else
      E_EDFLoggedWarning(2, "Invalid goto target '%s' in frameblock\n", gotoDef.label);

   // resolve the goto
   *(gotoDef.nextstate) = statenum;
}

//
// Perform DECORATE state parsing and data population for an EDF global frameblock.
//
static void E_processFrameBlock(cfg_t *sec, unsigned int index)
{
   const char *firststate = cfg_getstr(sec, ITEM_FRAMEBLOCK_FDS);
   int firststatenum;

   if(!firststate || (firststatenum = E_StateNumForName(firststate)) < 0)
   {
      E_EDFLoggedWarning(2, "E_processFrameBlock: firststate is required, block %u discarded\n", index);
      return;
   }

   const char *states = cfg_getstr(sec, ITEM_FRAMEBLOCK_STATES);
   if(!states)
   {
      E_EDFLoggedWarning(2, "E_processFrameBlock: states block required, block %u discarded\n", index);
      return;
   }

   edecstateout_t *dso; 
   if((dso = E_ParseDecorateStates(states, firststate)))
   {
      // warn if there are killstates, as these have no meaning
      if(dso->numkillstates)
         E_EDFLoggedWarning(2, "E_processFrameBlock: killstates ignored in block %u\n", index);

      // fixup external gotos; in the case of global DECORATE state blocks, these must be
      // the names of global EDF states
      if(dso->numgotos)
      {
         for(int i = 0; i < dso->numgotos; i++)
            E_processFrameBlockGoto(dso->gotos[i]);
      }

      // done with DSO
      E_FreeDSO(dso);
   }
   else
      E_EDFLoggedWarning(2, "E_processFrameBlock: could not parse DECORATE in frameblock %u\n", index);
}

//
// Processing for loose DECORATE-format state blocks which can be used to 
// populate predefined EDF states.
//
void E_ProcessFrameBlocks(cfg_t *cfg)
{
   unsigned int i, numblocks;

   E_EDFLogPuts("\t* Processing frameblock data\n");

   numblocks = cfg_size(cfg, EDF_SEC_FRAMEBLOCK);

   E_EDFLogPrintf("\t\t%u frameblock(s) defined\n", numblocks);

   for(i = 0; i < numblocks; i++)
   {
      E_processFrameBlock(cfg_getnsec(cfg, EDF_SEC_FRAMEBLOCK, i), i);
      E_EDFLogPrintf("\t\t* Processed frameblock %u\n", i);
   }
}


// EOF

