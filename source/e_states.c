// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
// EDF States Module
//
// By James Haley
//
//----------------------------------------------------------------------------

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

#define ITEM_DELTA_NAME      "name"

// forward prototype for action function dispatcher
static int E_ActionFuncCB(cfg_t *cfg, cfg_opt_t *opt, int argc,
                          const char **argv);

//
// Frame Options
//

#define FRAME_FIELDS \
   CFG_STR(ITEM_FRAME_SPRITE,      "BLANK",     CFGF_NONE), \
   CFG_INT_CB(ITEM_FRAME_SPRFRAME, 0,           CFGF_NONE, E_SpriteFrameCB), \
   CFG_BOOL(ITEM_FRAME_FULLBRT,    cfg_false,   CFGF_NONE), \
   CFG_INT(ITEM_FRAME_TICS,        1,           CFGF_NONE), \
   CFG_STRFUNC(ITEM_FRAME_ACTION,  "NULL",      E_ActionFuncCB), \
   CFG_STR(ITEM_FRAME_NEXTFRAME,   "S_NULL",    CFGF_NONE), \
   CFG_STR(ITEM_FRAME_MISC1,       "0",         CFGF_NONE), \
   CFG_STR(ITEM_FRAME_MISC2,       "0",         CFGF_NONE), \
   CFG_STR(ITEM_FRAME_PTCLEVENT,   "pevt_none", CFGF_NONE), \
   CFG_STR(ITEM_FRAME_ARGS,        0,           CFGF_LIST), \
   CFG_INT(ITEM_FRAME_DEHNUM,      -1,          CFGF_NONE), \
   CFG_END()

cfg_opt_t edf_frame_opts[] =
{
   CFG_STR(ITEM_FRAME_CMP, 0, CFGF_NONE),
   FRAME_FIELDS
};

cfg_opt_t edf_fdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   FRAME_FIELDS
};

//
// State Hash Lookup Functions
//

// State hash tables

// State Hashing
#define NUMSTATECHAINS 2003

static ehash_t state_namehash; // hash by name
static ehash_t state_numhash;  // hash by DeHackEd number

// Key retrieval function for hashing by name
E_KEYFUNC(state_t, name)

// Key and link retrieval functions for hashing by number
E_KEYFUNC(state_t, dehnum)
E_LINKFUNC(state_t, numlinks);

//
// E_StateNumForDEHNum
//
// State DeHackEd numbers *were* simply the actual, internal state
// number, but we have to actually store and hash them for EDF to
// remain cross-version compatible. If a state with the requested
// dehnum isn't found, -1 is returned.
//
int E_StateNumForDEHNum(int dehnum)
{
   state_t *st = NULL;
   int ret = -1;

   // 08/31/03: return null state for negative numbers, to
   // please some old, incorrect DeHackEd patches
   if(dehnum < 0)
      ret = NullStateNum;
   else if((st = (state_t *)E_HashObjectForKey(&state_numhash, &dehnum)))
      ret = st->index;

   return ret;
}

//
// E_GetStateNumForDEHNum
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
// E_SafeState
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
// E_StateNumForName
//
// Returns the number of a state given its name. Returns -1
// if the state is not found.
//
int E_StateNumForName(const char *name)
{
   state_t *st = NULL;
   int ret = -1;

   if((st = (state_t *)E_HashObjectForKey(&state_namehash, &name)))
      ret = st->index;

   return ret;
}

//
// E_GetStateNumForName
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
// E_SafeStateName
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

// haleyjd 03/22/06: automatic dehnum allocation
//
// Automatic allocation of dehacked numbers allows states to be used with
// parameterized codepointers without having had a DeHackEd number explicitly
// assigned to them by the EDF author. This was requested by several users
// after v3.33.02.

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_state_dehnum = D_MAXINT;

boolean E_AutoAllocStateDEHNum(int statenum)
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
   E_HashAddObject(&state_numhash, st);

   return true;
}

//
// EDF Processing Routines
//

//
// E_CountUniqueStates
//
// haleyjd 03/31/10: counts the number of states in a cfg which do not overwrite
// any pre-existing states by virtue of having the same name.
//
static unsigned int E_CountUniqueStates(cfg_t *cfg, unsigned int numstates)
{
   unsigned int i;
   unsigned int count = 0;

   // if the state name hash is empty, short-circuit for efficiency
   if(!state_namehash.numitems)
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
// E_ReallocStates
//
// haleyjd 04/04/10: Function to reallocate the states array safely.
//
static void E_ReallocStates(int numnewstates)
{
   static int numstatesalloc = 0;

   // only realloc when needed
   if(!numstatesalloc || (NUMSTATES >= numstatesalloc + numnewstates))
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
      states = realloc(states, numstatesalloc * sizeof(state_t *));

      // set the new state pointers to NULL
      for(i = NUMSTATES; i < numstatesalloc; ++i)
         states[i] = NULL;
   }

   // increment NUMSTATES
   NUMSTATES += numnewstates;
}

//
// E_CollectStates
//
// Pre-creates and hashes by name the states, for purpose of mutual 
// and forward references.
//
void E_CollectStates(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numstates;         // number of states defined by the cfg
   unsigned int numnew;            // number of states that are new
   unsigned int firstnewstate = 0; // index of first new state
   unsigned int curnewstate = 0;   // index of current new state being used
   state_t *statestructs = NULL;
   static boolean firsttime = true;

   // initialize hash tables if needed
   if(!state_namehash.isinit)
   {
      E_StrHashInit(&state_namehash, NUMSTATECHAINS, 
                    E_KEYFUNCNAME(state_t, name), NULL);   
      E_SintHashInit(&state_numhash, NUMSTATECHAINS,
                     E_KEYFUNCNAME(state_t, dehnum), 
                     E_LINKFUNCNAME(state_t, numlinks));
   }

   // get number of states defined by the cfg
   numstates = cfg_size(cfg, EDF_SEC_FRAME);

   // get number of new states in the cfg
   numnew = E_CountUniqueStates(cfg, numstates);

   // echo counts
   E_EDFLogPrintf("\t\t%u states defined (%u new)\n", numstates, numnew);

   if(numnew)
   {
      // allocate state_t structures for the new states
      statestructs = calloc(numnew, sizeof(state_t));

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
               E_HashRemoveObject(&state_numhash, st);

            // assign the new dehnum
            st->dehnum = dehnum;

            // if valid, add it back to the hash with the new id #
            if(st->dehnum >= 0)
               E_HashAddObject(&state_numhash, st);
         }
      }
      else
      {
         // this is a new state
         state_t *st = states[curnewstate++];

         // check name for validity
         if(strlen(name) > 40)
            E_EDFLoggedErr(2, "E_CollectStates: bad frame name '%s'\n", name);

         // initialize name
         strncpy(st->namebuf, name, 41);
         st->name = st->namebuf;

         // add to name hash
         E_HashAddObject(&state_namehash, st);

         // get dehackednum and add state to dehacked hash table if valid
         if((st->dehnum = cfg_getint(statecfg, ITEM_FRAME_DEHNUM)) >= 0)
            E_HashAddObject(&state_numhash, st);
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
// E_CreateArgList
//
// haleyjd 10/22/09: Creates an arglist object for the state, if it does not
// already have one. Otherwise, the existing arguments are disposed of.
//
static void E_CreateArgList(state_t *state)
{
   if(!state->args)
      state->args = calloc(1, sizeof(arglist_t)); // create one
   else
      E_DisposeArgs(state->args);                 // clear it out
}

// frame field parsing routines

//
// E_StateSprite
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
            E_EDFLogPrintf("\t\tWarning: frame '%s': couldn't implicitly "
                           "define sprite '%s'\n", states[i]->name, tempstr);
            sprnum = blankSpriteNum;
         }
         else
            sprnum = E_SpriteNumForName(tempstr);
      }
      states[i]->sprite = sprnum;
   }
}

//
// E_ActionFuncCB
//
// haleyjd 04/03/08: Callback function for the new function-valued string
// option used to specify state action functions. This is called during 
// parsing, not processing, and thus we do not look up/resolve anything
// at this point. We are only interested in populating the cfg's args
// values with the strings passed to this callback as parameters. The value
// of the option has already been set to the name of the codepointer by
// the libConfuse framework.
//
static int E_ActionFuncCB(cfg_t *cfg, cfg_opt_t *opt, int argc, 
                          const char **argv)
{
   if(argc > 0)
      cfg_setlistptr(cfg, "args", argc, (void *)argv);

   return 0; // everything is good
}

//
// E_StateAction
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

enum
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
   if(mobjinfo[thingnum].dehnum >= 0 || E_AutoAllocThingDEHNum(thingnum))
      *target = mobjinfo[thingnum].dehnum;
   else
   {
      E_EDFLogPrintf("\t\tWarning: failed to auto-allocate DeHackEd number "
                     "for thing %s\n", mobjinfo[thingnum].name);
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
      E_EDFLogPrintf("\t\tWarning: failed to auto-allocate DeHackEd number "
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
      E_EDFLogPrintf("\t\tWarning: failed to auto-allocate DeHackEd number "
                     "for sound %s\n", sfx->mnemonic);
      *target = 0;
   }
}

static void E_AssignMiscString(int *target, edf_string_t *str, const char *name)
{
   if(!str || str->numkey < 0)
   {
      E_EDFLogPrintf("\t\tWarning: bad string %s\n", name);
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
   *target = dp - deh_bexptrs;
}

//
// E_ParseMiscField
//
// This function implements the quite powerful prefix:value syntax
// for misc and args fields in frames. Some of the code within may
// be candidate for generalization, since other fields may need
// this syntax in the near future.
//
static void E_ParseMiscField(char *value, int *target)
{
   int i;
   char prefix[16];
   char *colonloc, *strval;
   
   memset(prefix, 0, 16);

   // look for a colon ending a possible prefix
   colonloc = E_ExtractPrefix(value, prefix, 16);
   
   if(colonloc)
   {
      // a colon was found, so identify the prefix
      strval = colonloc + 1;

      i = E_StrToNumLinear(misc_prefixes, NUM_MISC_PREFIXES, prefix);

      switch(i)
      {
      case PREFIX_FRAME:
         {
            int framenum = E_StateNumForName(strval);
            if(framenum < 0)
            {
               E_EDFLogPrintf("\t\tWarning: invalid state '%s' in misc field\n",
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
            if(thingnum == NUMMOBJTYPES)
            {
               E_EDFLogPrintf("\t\tWarning: invalid thing '%s' in misc field\n",
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
               E_EDFLogPrintf("\t\tWarning: invalid sound '%s' in misc field\n", 
                              strval);
               sfx = &NullSound;
            }
            E_AssignMiscSound(target, sfx);
         }
         break;
      case PREFIX_FLAGS:
         *target = deh_ParseFlagsSingle(strval, DEHFLAGS_MODE1);
         break;
      case PREFIX_FLAGS2:
         *target = deh_ParseFlagsSingle(strval, DEHFLAGS_MODE2);
         break;
      case PREFIX_FLAGS3:
         *target = deh_ParseFlagsSingle(strval, DEHFLAGS_MODE3);
         break;
      case PREFIX_FLAGS4:
         *target = deh_ParseFlagsSingle(strval, DEHFLAGS_MODE4);
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
         E_EDFLogPrintf("\t\tWarning: unknown value prefix '%s'\n",
                        prefix);
         *target = 0;
         break;
      }
   }
   else
   {
      char  *endptr;
      int    val;
      double dval;

      // see if it is a number
      if(strchr(value, '.')) // has a decimal point?
      {
         dval = strtod(value, &endptr);

         // convert result to fixed-point
         val = (fixed_t)(dval * FRACUNIT);
      }
      else
      {
         // 11/11/03: use strtol to support hex and oct input
         val = strtol(value, &endptr, 0);
      }

      // haleyjd 04/02/08:
      // no? then try certain namespaces in a predefined order of precedence
      if(*endptr != '\0')
      {
         int temp;
         sfxinfo_t *sfx;
         edf_string_t *str;
         deh_bexptr *dp;
         
         if((temp = E_ThingNumForName(value)) != NUMMOBJTYPES) // thingtype?
            E_AssignMiscThing(target, temp);
         else if((temp = E_StateNumForName(value)) >= 0)       // frame?
            E_AssignMiscState(target, temp);
         else if((sfx = E_EDFSoundForName(value)) != NULL)     // sound?
            E_AssignMiscSound(target, sfx);
         else if((str = E_StringForName(value)) != NULL)       // string?
            E_AssignMiscString(target, str, value);
         else if((dp = D_GetBexPtr(value)) != NULL)            // bexptr???
            E_AssignMiscBexptr(target, dp, value);
      }
      else
         *target = val;
   }
}

//
// E_GetArgument
//
// haleyjd 10/22/09: Skip over any prefix in the argument string.
//
static char *E_GetArgument(char *value)
{
   char prefix[16];
   char *colonloc;
   
   memset(prefix, 0, 16);

   // look for a colon ending a possible prefix
   colonloc = E_ExtractPrefix(value, prefix, 16);

   return colonloc ? colonloc + 1 : value;
}

enum
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
// E_SpecialNextState
//
// 11/07/03:
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
// E_StateNextFrame
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
      char *endptr = NULL;
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
// E_StatePtclEvt
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

// haleyjd 04/03/08: hack for function-style arguments to action function
static boolean in_action;
static boolean early_args_found;
static boolean early_args_end;


//
// E_CmpTokenizer
//
// haleyjd 06/24/04:
// A lexer function for the frame cmp field.
// Used by E_ProcessCmpState below.
//
static char *E_CmpTokenizer(const char *text, int *index, qstring_t *token)
{
   char c;
   int state = 0;

   // if we're already at the end, return NULL
   if(text[*index] == '\0')
      return NULL;

   M_QStrClear(token);

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
            return M_QStrBuffer(token);
         case '(':
            if(in_action)
            {
               early_args_found = true;
               return M_QStrBuffer(token);
            }
            M_QStrPutc(token, c);
            continue;
         case ')':
            if(in_action && early_args_found)
            {
               early_args_end = true;
               continue;
            }
            // fall through
         default:      // everything else == part of value
            M_QStrPutc(token, c);
            continue;
         }
      case 1: // in quoted area (double quotes)
         if(c == '"') // end of quoted area
            state = 0;
         else
            M_QStrPutc(token, c); // everything inside is literal
         continue;
      case 2: // in quoted area (single quotes)
         if(c == '\'') // end of quoted area
            state = 0;
         else
            M_QStrPutc(token, c); // everything inside is literal
         continue;
      default:
         E_EDFLoggedErr(0, "E_CmpTokenizer: internal error - undefined lexer state\n");
      }
   }

   // return final token, next call will return NULL
   return M_QStrBuffer(token);
}

// macros for E_ProcessCmpState:

// NEXTTOKEN: calls E_CmpTokenizer to get the next token

#define NEXTTOKEN() curtoken = E_CmpTokenizer(value, &tok_index, &buffer)

// DEFAULTS: tests if the string value is either NULL or equal to "*"

#define DEFAULTS(value)  (!(value) || (value)[0] == '*')

//
// E_ProcessCmpState
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
   qstring_t buffer;
   char *curtoken = NULL;
   int tok_index = 0, j;

   // first things first, we have to initialize the qstring
   M_QStrInitCreate(&buffer);

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
      if(E_SpriteFrameCB(NULL, NULL, curtoken, &(states[i]->frame)) == -1)
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
      states[i]->tics = strtol(curtoken, NULL, 0);

   // process action
   in_action = true;
   NEXTTOKEN();
   if(DEFAULTS(curtoken))
      states[i]->action = NULL;
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
         
         if(DEFAULTS(curtoken))
            E_AddArgToList(states[i]->args, "");
         else
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
      for(j = 0; j < 5; ++j) // Only 5 args are supported here. Deprecated.
      {
         NEXTTOKEN();

         if(DEFAULTS(curtoken))
            E_AddArgToList(states[i]->args, "");
         else
            E_AddArgToList(states[i]->args, E_GetArgument(curtoken));
      }
   }

   early_args_found = early_args_end = false;

   // free the qstring
   M_QStrFree(&buffer);
}

#undef NEXTTOKEN
#undef DEFAULTS

// IS_SET: this macro tests whether or not a particular field should
// be set. When applying deltas, we should not retrieve defaults.

#undef  IS_SET
#define IS_SET(name) (def || cfg_size(framesec, (name)) > 0)

//
// E_ProcessState
//
// Generalized code to process the data for a single state
// structure. Doubles as code for frame and framedelta.
//
static void E_ProcessState(int i, cfg_t *framesec, boolean def)
{
   int j;
   int tempint;
   char *tempstr;

   // 11/14/03:
   // In definitions only, see if the cmp field is defined. If so,
   // we go into it with E_ProcessCmpState above, and ignore most
   // other fields in the frame block.
   if(def)
   {
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
      if(cfg_getbool(framesec, ITEM_FRAME_FULLBRT) == cfg_true)
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

   // process particle event
   if(IS_SET(ITEM_FRAME_PTCLEVENT))
   {
      tempstr = cfg_getstr(framesec, ITEM_FRAME_PTCLEVENT);

      E_StatePtclEvt(tempstr, i);
   }

   // 03/30/05: the following fields are now also allowed in cmp frames
hitcmp:
      
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
}

//
// E_ProcessStates
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

#if 1
   {
      extern void TestDSParser(void); // stupid GCC...
      
      // TEMPORARY DEBUG TEST CODE
      TestDSParser();
   }
#endif
}

//
// E_ProcessStateDeltas
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

//==============================================================================
//
// DECORATE State Management
//
// haleyjd 04/03/10: This code manages the creation of DECORATE state sets and
// is used by the parser below.
//

// enumeration for estatebuf types
enum
{
   BUF_LABEL,
   BUF_STATE,
   BUF_KEYWORD,
   BUF_GOTO
};

//
// estatebuf
//
// This is a temporary structure used to cache data about DECORATE state
// sequences.
//
typedef struct estatebuf_s
{
   mdllistitem_t links; // linked list links
   int type;            // type of entry (label, state, etc.)
   int linenum;         // origin line number
   char *name;          // name when needed

   // goto-specific info
   char *gotodest;      // goto destination
   char *gotooffset;    // goto offset
} estatebuf_t;

static estatebuf_t *statebuffer; // temporary data cache
static estatebuf_t *neweststate; // newest buffered state

//
// E_AddBufferedState
//
// Caches data in the statebuffer list.
//
static void E_AddBufferedState(int type, const char *name, int linenum)
{
   estatebuf_t *newbuf = calloc(1, sizeof(estatebuf_t));

   newbuf->type = type;

   if(name)
      newbuf->name = strdup(name);

   newbuf->linenum = linenum;

   // use tail insertion to keep objects in order generated
   M_DLListInsert(&newbuf->links,
                  neweststate ? &(neweststate->links.next) : 
                                 (mdllistitem_t **)&statebuffer);

   neweststate = newbuf;
}

//
// Parser state globals
//
static int numdeclabels; // number of labels counted
static int numdecstates; // number of states counted
static int numkeywords;  // number of keywords counted
static int numgotos;     // number of goto's counted

//==============================================================================
//
// DECORATE State Parser
//
// haleyjd 06/16/09: EDF now supports DECORATE-format state definitions inside
// thingtype "states" fields, which accept heredoc strings. The processing
// of those strings is done here, where they are turned into states.
//

/*
  Grammar

  <labeledunit> := <labelblock><frameblock><labeledunit> | nil
    <labelblock> := <label><eol><labelblock>
      <label> := [A-Za-z0-9_]+('.'[A-Za-z0-9_]+)?':'
      <eol>   := '\n'
    <frameblock> := <frame><frameblock> | <frame>
      <frame> := <keyword><eol> | <frame_token_list><eol>
        <keyword> := "stop" | "wait" | "loop" | "goto" <jumplabel>
          <jumplabel> := <jlabel> | <jlabel> '+' number
            <jlabel> := [A-Za-z0-9_:]+('.'[A-Za-z0-9_]+)?
        <frame_token_list> := <sprite><frameletters><tics><action>
                            | <sprite><frameletters><tics><bright><action>
          <sprite> := [A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]
          <frameletters> := [A-Z\[\\\]]+
          <tics> := [0-9]+
          <bright> := "bright"
          <action> := <name>
                    | <name> '(' <arglist> ')'
            <name> := [A-Za-z0-9_]+
            <arglist> := <arg> ',' <arglist> | <arg> | nil
              <arg> := "string" | number
*/

/*
   State Transition Diagram
   ----------------------------------
   NEEDLABEL: 
      <label> : NEEDLABELORKWORSTATE
      EOL     : loop
   NEEDLABELORKWORSTATE:
      <label>   : NEEDLABELORKWORSTATE
      "goto"    : NEEDGOTOLABEL
      <keyword> : NEEDKWEOL
      <text>    : NEEDSTATEFRAMES
      EOL       : loop ***
   NEEDGOTOLABEL:
      <label> : NEEDGOTOEOLORPLUS
   NEEDGOTOEOLORPLUS:
      '+' : NEEDGOTOOFFSET
      EOL : NEEDLABEL
   NEEDGOTOOFFSET:
      <number> : NEEDKWEOL
   NEEDKWEOL:
      EOL : NEEDLABEL
   NEEDSTATEFRAMES:
      <text> : NEEDSTATETICS
   NEEDSTATETICS:
      <number> : NEEDSTATEBRIGHTORACTION
   NEEDSTATEBRIGHTORACTION:
      "bright" : NEEDSTATEACTION
      <text>   : NEEDSTATEEOLORPAREN
   NEEDSTATEACTION:
      <text> : NEEDSTATEEOLORPAREN
   NEEDSTATEEOLORPAREN:
      EOL : NEEDLABELORKWORSTATE
      '(' : NEEDSTATEARGORPAREN
   NEEDSTATEARGORPAREN:
      <number> | <string> : NEEDSTATECOMMAORPAREN
      ')' : NEEDSTATEEOL
   NEEDSTATECOMMAORPAREN:
      ',' : NEEDSTATEARG
      ')' : NEEDSTATEEOL
   NEEDSTATEARG:
      <number> | <string> : NEEDSTATECOMMAORPAREN
   NEEDSTATEEOL:
      EOL : NEEDLABELORKWORSTATE

  *** EOL is allowed in PSTATE_NEEDLABELORKWORSTATE in a manner more flexible than
      that specified by the above grammar for matters of convenience.
*/

// parser state enumeration
enum
{
   PSTATE_NEEDLABEL,             // starting state, first token must be a label
   PSTATE_NEEDLABELORKWORSTATE,  // after label, need label or keyword or state
   PSTATE_NEEDGOTOLABEL,         // after goto, need goto label
   PSTATE_NEEDGOTOEOLORPLUS,     // after goto label, need EOL or '+'
   PSTATE_NEEDGOTOOFFSET,        // after '+', need goto offset number
   PSTATE_NEEDKWEOL,             // need kw EOL
   PSTATE_NEEDSTATEFRAMES,       // after state sprite, need frames
   PSTATE_NEEDSTATETICS,         // after state frames, need tics
   PSTATE_NEEDBRIGHTORACTION,    // after tics, need "bright" or action name
   PSTATE_NEEDSTATEACTION,       // after "bright", need action name
   PSTATE_NEEDSTATEEOLORPAREN,   // after action name, need EOL or '('
   PSTATE_NEEDSTATEARGORPAREN,   // after '(', need argument or ')'
   PSTATE_NEEDSTATECOMMAORPAREN, // after argument, need ',' or ')'
   PSTATE_NEEDSTATEARG,          // after ',', need argument (loop state)
   PSTATE_NEEDSTATEEOL,          // after ')', need EOL
   PSTATE_NUMSTATES
};

// parser state structure
typedef struct pstate_s
{
   int state; // state of the parser, as defined by the above enumeration
   qstring_t *linebuffer;  // qstring to use as line buffer
   qstring_t *tokenbuffer; // qstring to use as token buffer
   int index;              // current index into line buffer for tokenization
   int linenum;            // line number (relative to start of state block)
   boolean needline;       // if true, feed a line from the input
   boolean principals;     // parsing for principals only

   int tokentype;    // current token type, once decided upon
   int tokenerror;   // current token error code
} pstate_t;

// tokenization

// token types
enum
{
   TOKEN_LABEL,   // [A-Za-z0-9_]+('.'[A-Za-z0-9_]+)?':'
   TOKEN_KEYWORD, // loop, stop, wait, goto
   TOKEN_PLUS,    // '+'
   TOKEN_LPAREN,  // '('
   TOKEN_COMMA,   // ','
   TOKEN_RPAREN,  // ')'
   TOKEN_TEXT,    // anything else (numbers, strings, words, etc.)
   TOKEN_EOL,     // end of line
   TOKEN_ERROR,   // an error token
   TOKEN_NUMTOKENS
};

// token type names for display in error messages
static const char *ds_token_names[TOKEN_NUMTOKENS] =
{
   "label",
   "keyword",
   "character",
   "character",
   "character",
   "character",
   "text token",
   "end of line",
   "error"        // not actually displayed; see below
};

// token errors
enum
{
   TERR_NONE,           // not an error
   TERR_BADSTRING,      // malformed string constant
   TERR_UNEXPECTEDCHAR, // weird character
   TERR_NUMERRORS
};

// token error strings
static const char *ds_token_errors[TERR_NUMERRORS] =
{
   "not an error",
   "unterminated string constant",
   "unexpected character"
};

// tokenizer states
enum
{
   TSTATE_SCAN,    // scanning for start of a token
   TSTATE_SLASH,   // scanning after a '/'
   TSTATE_COMMENT, // consume up to next "\n"
   TSTATE_TEXT,    // scanning in a label, keyword, or text token
   TSTATE_COLON,   // scanning after ':'
   TSTATE_LABEL,   // scanning after '.' in a label
   TSTATE_STRING,  // scanning in a string literal
   TSTATE_DONE     // finished; return token to parser
};

// tokenizer state structure
typedef struct tkstate_s
{
   int state;        // current state of tokenizer
   int i;            // index into string
   int tokentype;    // token type, once decided upon
   int tokenerror;   // token error code
   qstring_t *line;  // line text
   qstring_t *token; // token text
} tkstate_t;

//=============================================================================
//
// Tokenizer Callbacks
//

//
// DoTokenStateScan
//
// Scanning for the start of a token.
//
static void DoTokenStateScan(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;
   qstring_t *token = tks->token;

   // allow A-Za-z0-9, underscore, and leading - for numbers
   if(isalnum(str[i]) || str[i] == '_' || str[i] == '-')
   {
      // start a text token - we'll determine the more specific type, if any,
      // later.
      M_QStrPutc(token, str[i]);
      tks->tokentype = TOKEN_TEXT;
      tks->state     = TSTATE_TEXT;
   }
   else
   {
      switch(str[i])
      {
      case ' ':
      case '\t': // whitespace
         break;  // keep scanning
      case '\0': // end of line, which is significant in DECORATE
         tks->tokentype = TOKEN_EOL;
         tks->state     = TSTATE_DONE;
         break;
      case '"':  // quoted string
         tks->tokentype = TOKEN_TEXT;
         tks->state     = TSTATE_STRING;
         break;
      case '+':  // plus - used in relative goto statement
         M_QStrPutc(token, '+');
         tks->tokentype = TOKEN_PLUS;
         tks->state     = TSTATE_DONE;
         break;
      case '(':  // lparen - for action argument lists
         M_QStrPutc(token, '(');
         tks->tokentype = TOKEN_LPAREN;
         tks->state     = TSTATE_DONE;
         break;
      case ',':  // comma - for action argument lists
         M_QStrPutc(token, ',');
         tks->tokentype = TOKEN_COMMA;
         tks->state     = TSTATE_DONE;
         break;
      case ')':  // rparen - for action argument lists
         M_QStrPutc(token, ')');
         tks->tokentype = TOKEN_RPAREN;
         tks->state     = TSTATE_DONE;
         break;
      case '/':  // forward slash - start of single-line comment?
         tks->state     = TSTATE_SLASH;
         break;
      default: // whatever it is, we don't care for it.
         M_QStrPutc(token, str[i]);
         tks->tokentype  = TOKEN_ERROR;
         tks->tokenerror = TERR_UNEXPECTEDCHAR;
         tks->state      = TSTATE_DONE;
         break;
      }
   }
}

//
// DoTokenStateSlash
//
static void DoTokenStateSlash(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;
   qstring_t *token = tks->token;

   if(str[i] == '/')
      tks->state = TSTATE_COMMENT;
   else
   {
      M_QStrPutc(token, str[i]);
      tks->tokentype  = TOKEN_ERROR;
      tks->tokenerror = TERR_UNEXPECTEDCHAR;
      tks->state      = TSTATE_DONE;
   }
}

//
// DoTokenStateComment
//
static void DoTokenStateComment(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;

   // eol?
   if(str[i] == '\0')
   {
      tks->tokentype = TOKEN_EOL;
      tks->state     = TSTATE_DONE;
   }
   // else keep consuming characters
}

// there are only four keywords, so hashing is pointless.
static const char *decorate_kwds[] =
{
   "loop",
   "wait",
   "stop",
   "goto",
};

#define NUMDECKWDS (sizeof(decorate_kwds) / sizeof(const char *))

//
// isDecorateKeyword
//
// Returns true if the string matches a DECORATE state keyword, and false
// otherwise.
//
static boolean isDecorateKeyword(const char *text)
{
   return (E_StrToNumLinear(decorate_kwds, NUMDECKWDS, text) != NUMDECKWDS);
}

//
// DoTokenStateText
//
// At this point we are in a label, a keyword, or a plain text token,
// but we don't know which yet.
//
static void DoTokenStateText(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;
   qstring_t *token = tks->token;

   if(isalnum(str[i]) || str[i] == '_')
   {
      // continuing in label, keyword, or text
      M_QStrPutc(token, str[i]);
   }
   else if(str[i] == '.')
   {
      char *endpos = NULL;
      long foo;

      // we see a '.' which could either be the decimal point in a float
      // value, or the dot name separator in a label. If the token is only
      // numbers up to this point, we will consider it a number, but the
      // parser can sort this out if the wrong token type appears when it
      // is expecting TOKEN_LABEL.
      
      foo = strtol(token->buffer, &endpos, 10);
      
      if(*endpos != '\0')
      {      
         // it's not just a number, so assume it's a label
         tks->tokentype = TOKEN_LABEL;
         tks->state     = TSTATE_LABEL;
      }
      // otherwise, proceed as normal
   }
   else if(str[i] == ':')
   {
      // colon either means:
      // A. this is a label, and we are at the end of it.
      // B. this is a namespace declarator in a goto label - next char is ':'
      // We must use another state to determine which is which.
      tks->state     = TSTATE_COLON;
   }
   else // anything else ends this token, and steps back
   {
      // see if it is a keyword
      if(isDecorateKeyword(token->buffer))
         tks->tokentype = TOKEN_KEYWORD;
      tks->state = TSTATE_DONE;
      --tks->i; // the char we're on is the start of a new token, so back up.
   }
}

//
// DoTokenStateColon
//
// When a ':' is found in a text token we must defer the decision on whether
// to call that the end of the token or not until we see the next character.
//
static void DoTokenStateColon(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;
   qstring_t *token = tks->token;

   if(str[i] == ':')
   {
      // Two colons in a row means we've found the namespace declarator.
      // Add two colons to the token buffer, and return to TSTATE_TEXT to
      // parse out the rest of the token.
      M_QStrCat(token, "::");
      tks->state = TSTATE_TEXT;
   }
   else // anything else means this was a label
   {
      tks->tokentype = TOKEN_LABEL;
      tks->state     = TSTATE_DONE;
      --tks->i; // the char we're on is the start of a new token, so back up
   }
}

//
// DoTokenStateLabel
//
// Scans inside a label after encountering a dot, which is used for custom
// death and pain state specification.
//
static void DoTokenStateLabel(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;
   qstring_t *token = tks->token;

   if(isalnum(str[i]) || str[i] == '_')
   {
      // continuing in label
      M_QStrPutc(token, str[i]);
   }
   else if(str[i] == ':')
   {
      // colon at the end means this is a label, and we are at the end of it.
      tks->state = TSTATE_DONE;
   }
   else
   {
      // anything else means we may have been mistaken about this being a 
      // label after all, or this is a label in the context of a goto statement;
      // in that case, we mark the token as text type and the parser can sort 
      // out the damage by watching out for it.
      tks->tokentype = TOKEN_TEXT;
      tks->state     = TSTATE_DONE;
      --tks->i; // back up one char since we ate something unrelated
   }
}

//
// DoTokenStateString
//
// Parsing inside a string literal.
//
static void DoTokenStateString(tkstate_t *tks)
{
   const char *str  = tks->line->buffer;
   int i            = tks->i;
   qstring_t *token = tks->token;

   // note: DECORATE does not support escapes in strings?!?
   switch(str[i])
   {
   case '"': // end of string
      tks->tokentype = TOKEN_TEXT;
      tks->state     = TSTATE_DONE;
      break;
   case '\0': // EOL - error
      tks->tokentype  = TOKEN_ERROR;
      tks->tokenerror = TERR_BADSTRING;
      tks->state      = TSTATE_DONE;
      break;
   default:
      // add character and continue scanning
      M_QStrPutc(token, str[i]);
      break;
   }
}

// tokenizer callback type
typedef void (*tksfunc_t)(tkstate_t *);

// tokenizer state function table
static tksfunc_t tstatefuncs[] =
{
   DoTokenStateScan,    // scanning for start of a token
   DoTokenStateSlash,   // scanning inside a single-line comment?
   DoTokenStateComment, // scanning inside a comment; consume to next EOL
   DoTokenStateText,    // scanning inside a label, keyword, or text
   DoTokenStateColon,   // scanning after a ':' encountered in a text token
   DoTokenStateLabel,   // scanning inside a label after a dot
   DoTokenStateString,  // scanning inside a string literal
};

//
// E_GetDSToken
//
// Gets the next token from the DECORATE states string.
//
static int E_GetDSToken(pstate_t *ps)
{
   tkstate_t tks;

   // set up tokenizer state - transfer in parser state details
   tks.state      = TSTATE_SCAN;
   tks.tokentype  = -1;
   tks.tokenerror = TERR_NONE;
   tks.i          = ps->index;
   tks.line       = ps->linebuffer;
   tks.token      = ps->tokenbuffer;

   M_QStrClear(tks.token);

   while(tks.state != TSTATE_DONE)
   {
#ifdef RANGECHECK
      if(tks.state < 0 || tks.state >= TSTATE_DONE)
         I_Error("E_GetDSToken: Internal error: undefined state\n");
#endif

      tstatefuncs[tks.state](&tks);

      // step to next character
      ++tks.i;
   }

   // write back info to parser state
   ps->index      = tks.i;
   ps->tokentype  = tks.tokentype;
   ps->tokenerror = tks.tokenerror;

   return tks.tokentype;
}

//=============================================================================
//
// Parser Callbacks
//

// utilities

// for unnecessarily precise grammatical correctness ;)
#define hasvowel(str) \
   (*str == 'a' || *str == 'e' || *str == 'i' || *str == 'o' || *str == 'u')

//
// PSExpectedErr
//
// Called to issue a verbose log message indicating an error consisting of an
// unexpected token type.
//
static void PSExpectedErr(pstate_t *ps, const char *expected)
{
   const char *tokenname = ds_token_names[ps->tokentype];

   // if error is due to a tokenizer error, get the more specific error message
   // pertaining to that malformed token
   if(ps->tokenerror)
      tokenname = ds_token_errors[ps->tokenerror];

   E_EDFLoggedErr(3, "Error on line %d of DECORATE state block:\n"
                     "\t\t\tExpected %s %s but found %s %s with value '%s'\n",
                     ps->linenum, 
                     hasvowel(expected)  ? "an" : "a", expected, 
                     hasvowel(tokenname) ? "an" : "a", tokenname, 
                     ps->tokentype != TOKEN_EOL ? ps->tokenbuffer->buffer : "\\n");
}

//
// DoPSNeedLabel
//
// Initial state of the parser; we expect a state label, but we'll accept
// EOL, which means to loop.
//
static void DoPSNeedLabel(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // loop until something meaningful appears
      break;

   case TOKEN_LABEL:
      // record label to associate with next state generated
      if(ps->principals) 
      {
         // count a label
         numdeclabels++; 
         // make a label object
         E_AddBufferedState(BUF_LABEL, ps->tokenbuffer->buffer, ps->linenum);
      }
      else
      {
         // TODO
      }
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;

   default:
      // anything else is an error
      PSExpectedErr(ps, "label");
      break;
   }
}

//
// DoPSNeedLabelOrKWOrState
//
// Main looping state. This is gone to after finding the first label,
// as well as returned to after processing a state.
//
static void DoPSNeedLabelOrKWOrState(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // loop until something meaningful appears
      break;

   case TOKEN_LABEL:
      // record label to associate with next state generated.
      // remain in the current state
      if(ps->principals)
      {
         // count a label
         numdeclabels++;
         // make a label object
         E_AddBufferedState(BUF_LABEL, ps->tokenbuffer->buffer, ps->linenum);
      }
      else
      {
         // TODO
      }
      break;

   case TOKEN_KEYWORD:
      // TODO: generate appropriate state for keyword
      if(!M_QStrCaseCmp(ps->tokenbuffer, "goto"))
      {
         // handle goto specifics
         if(ps->principals)
         {
            // count a goto
            numgotos++;
            // make a goto object
            E_AddBufferedState(BUF_GOTO, NULL, ps->linenum);
         }
         else
         {
            // TODO
         }
         ps->state = PSTATE_NEEDGOTOLABEL;
      }
      else
      {
         // handle other keyword specifics
         if(ps->principals)
         {
            // count a keyword
            numkeywords++;
            // make a keyword object
            E_AddBufferedState(BUF_KEYWORD, ps->tokenbuffer->buffer, ps->linenum);
         }
         else
         {
            // TODO
         }
         ps->state = PSTATE_NEEDKWEOL;
      }
      break;

   case TOKEN_TEXT:
      // verify sprite name; generate new state
      if(ps->principals)
      {
         // make a new sprite if this one doesn't already exist
         if(E_SpriteNumForName(ps->tokenbuffer->buffer) == -1)
            E_ProcessSingleSprite(ps->tokenbuffer->buffer);

         // do not count or create a state yet; this will be done when we parse
         // the frame token, whose strlen is the number of states to count
      }
      else
      {
         // TODO
      }
      ps->state = PSTATE_NEEDSTATEFRAMES;
      break;

   default:
      // anything else is an error; eat rest of the line
      PSExpectedErr(ps, "label or keyword or sprite");
      break;
   }
}

//
// DoPSNeedGotoLabel
//
// Expecting the label for a "goto" keyword
//
static void DoPSNeedGotoLabel(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype == TOKEN_TEXT)
   {
      // record text into goto destination record for later
      // resolution pass.
      if(ps->principals)
      {
         if(neweststate->type != BUF_GOTO)
            I_Error("DoPSNeedGotoLabel: internal error - last state != GOTO\n");

         neweststate->gotodest = M_QStrCDup(ps->tokenbuffer, PU_STATIC);
      }
      else
      {
         // TODO
      }
      ps->state = PSTATE_NEEDGOTOEOLORPLUS;
   }
   else
   {
      PSExpectedErr(ps, "goto label");
      ps->state = PSTATE_NEEDLABEL;          // return to initial state
   }
}

//
// DoPSNeedGotoEOLOrPlus
//
// Expecting EOL or '+' after a goto destination label.
//
static void DoPSNeedGotoEOLOrPlus(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // TODO: finalize goto destination record if necessary
      ps->state = PSTATE_NEEDLABEL;
      break;

   case TOKEN_PLUS:
      // TODO: modify goto destination record to accept offset
      ps->state = PSTATE_NEEDGOTOOFFSET;
      break;

   default:
      // anything else is an error
      PSExpectedErr(ps, "end of line or +");
      ps->state = PSTATE_NEEDLABEL;
      break;
   }
}

//
// DoPSNeedGotoOffset
//
// Expecting a numeric offset value after the '+' in a goto statement.
//
static void DoPSNeedGotoOffset(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype == TOKEN_TEXT)
   {
      // record offset into goto destination record
      if(ps->principals)
      {
         if(neweststate->type != BUF_GOTO)
            I_Error("DoPSNeedGotoOffset: internal error - last state != GOTO\n");

         neweststate->gotooffset = M_QStrCDup(ps->tokenbuffer, PU_STATIC);
      }
      else
      {
         // TODO
      }
      // TODO: finalize destination record if necessary
      ps->state = PSTATE_NEEDKWEOL;
   }
   else
   {
      // anything else is an error
      PSExpectedErr(ps, "goto offset");
      ps->state = PSTATE_NEEDLABEL;
   }
}

//
// DoPSNeedKWEOL
//
// Expecting the end of a line after a state transition keyword.
// The only thing which can come next is a new label.
//
static void DoPSNeedKWEOL(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_EOL)
      PSExpectedErr(ps, "end of line");

   // return to initial state
   ps->state = PSTATE_NEEDLABEL;
}

//
// DoPSNeedStateFrames
//
// Expecting a frames string for the current state.
// Frames strings are a series of A-Z and [\] chars.
// If this string is more than one character long, we generate additional
// states up to the number of characters in the string, and the rest of the
// properties parsed for the first state apply to those states. The states
// created this way have their next state fields automatically created to be 
// consecutive.
//
static void DoPSNeedStateFrames(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "frame string");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {
      if(ps->principals)
      {
         // During parsing for principals, we determine how many states should
         // be created, and add that many state objects. Each state object created
         // here will have the same line number as this line in the DECORATE state
         // block, and when parsing for values, we will apply the values to all
         // states from the current one to the last one found having the same line
         // number.
         unsigned int i;
         unsigned int numstatestomake = M_QStrLen(ps->tokenbuffer);

         for(i = 0; i < numstatestomake; ++i)
            E_AddBufferedState(BUF_STATE, NULL, ps->linenum);
      }
      else
      {
         // TODO
      }
      ps->state = PSTATE_NEEDSTATETICS;
   }
}

//
// DoPSNeedStateTics
//
// Expecting a tics amount for the current state.
//
static void DoPSNeedStateTics(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "tics value");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {
      // TODO: verify is properly ranged number
      
      // TODO: record into current range of states
      
      ps->state = PSTATE_NEEDBRIGHTORACTION;
   }
}

//
// DoPSNeedBrightOrAction
//
// Expecting either "bright", which modifies the current state block to use
// fullbright frames, or the action name.
//
static void DoPSNeedBrightOrAction(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "bright keyword or action name");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   if(!M_QStrCaseCmp(ps->tokenbuffer, "bright"))
   {
      // TODO: apply fullbright to all states in the current range
      ps->state = PSTATE_NEEDSTATEACTION;
   }
   else
   {
      // TODO: verify is valid codepointer name and apply action to all
      // states in the current range.
      ps->state = PSTATE_NEEDSTATEEOLORPAREN;
   }
}

//
// DoPSNeedStateAction
//
// Expecting an action name after having dealt with the "bright" command.
//
static void DoPSNeedStateAction(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "action name");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {
      // TODO: verify is valid codepointer name and apply action to all
      // states in the current range.
      ps->state = PSTATE_NEEDSTATEEOLORPAREN;
   }
}

//
// DoPSNeedStateEOLOrParen
//
// After parsing the action name, expecting an EOL or '('.
//
static void DoPSNeedStateEOLOrParen(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // TODO: finalize state range
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   case TOKEN_LPAREN:
      // TODO: prepare for args parsing if necessary
      ps->state = PSTATE_NEEDSTATEARGORPAREN;
      break;
   default:
      // anything else is an error
      PSExpectedErr(ps, "end of line or (");
      // TODO: finalize state range
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   }
}

//
// DoPSNeedStateArgOrParen
//
// After '(' we expect an argument or ')' (which is pointless to have but 
// is still allowed).
//
static void DoPSNeedStateArgOrParen(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_TEXT:
      // TODO: parse and populate argument in state range, increment
      // argument count.
      ps->state = PSTATE_NEEDSTATECOMMAORPAREN;
      break;
   case TOKEN_RPAREN:
      ps->state = PSTATE_NEEDSTATEEOL;
      break;
   default:
      // error
      PSExpectedErr(ps, "argument or )");
      // TODO: finalize state range
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   }
}

//
// DoPSNeedStateCommaOrParen
//
// Expecting ',' or ')' after an action argument.
//
static void DoPSNeedStateCommaOrParen(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_COMMA:
      ps->state = PSTATE_NEEDSTATEARG;
      break;
   case TOKEN_RPAREN:
      ps->state = PSTATE_NEEDSTATEEOL;
      break;
   default:
      // error
      PSExpectedErr(ps, ", or )");
      // TODO: finalize state range
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   }
}

//
// DoPSNeedStateArg
//
// Expecting an argument, after having seen ','.
// This state exists because ')' is not allowed here.
//
static void DoPSNeedStateArg(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "argument");
      // TODO: finalize state range
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {
      // TODO: parse and populate argument in state range, increment
      // argument count.
      ps->state = PSTATE_NEEDSTATECOMMAORPAREN;
   }
}

//
// DoPSNeedStateEOL
//
// Expecting end of line after completing state parsing.
//
static void DoPSNeedStateEOL(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_EOL)
      PSExpectedErr(ps, "end of line");

   // TODO: finalize state range
   ps->state = PSTATE_NEEDLABELORKWORSTATE;
}

// parser callback type
typedef void (*psfunc_t)(pstate_t *);

// parser state function table
static psfunc_t pstatefuncs[] =
{
   DoPSNeedLabel,
   DoPSNeedLabelOrKWOrState,
   DoPSNeedGotoLabel,
   DoPSNeedGotoEOLOrPlus,
   DoPSNeedGotoOffset,
   DoPSNeedKWEOL,
   DoPSNeedStateFrames,
   DoPSNeedStateTics,
   DoPSNeedBrightOrAction,
   DoPSNeedStateAction,
   DoPSNeedStateEOLOrParen,
   DoPSNeedStateArgOrParen,
   DoPSNeedStateCommaOrParen,
   DoPSNeedStateArg,
   DoPSNeedStateEOL,
};

//
// E_GetDSLine
//
// Gets the next line of DECORATE state input.
//
boolean E_GetDSLine(const char **src, pstate_t *ps)
{
   boolean isdone = false;
   const char *srctxt = *src;

   M_QStrClear(ps->linebuffer);
   ps->index = 0;

   if(!*srctxt) // at end?
      isdone = true;
   else
   {
      char c;

      while((c = *srctxt++))
      {
         if(c == '\n')
            break;
         
         M_QStrPutc(ps->linebuffer, c);
      }

      // track line numbers
      ps->linenum++;
   }

   *src = srctxt;

   return isdone;
}

//
// E_ParseDecorateInternal
//
// Main driver routine for parsing of DECORATE state blocks.
// Can be called either to collect principals or to run the final collection
// of data.
//
static void E_ParseDecorateInternal(const char *input, boolean principals)
{
   pstate_t ps;
   qstring_t linebuffer;
   qstring_t tokenbuffer;
   const char *inputstr = input;

   // create line and token buffers
   M_QStrInitCreate(&linebuffer);
   M_QStrInitCreate(&tokenbuffer);

   // initialize pstate structure
   ps.index       = 0;
   ps.linenum     = 0;
   ps.tokentype   = 0;
   ps.tokenerror  = TERR_NONE;   
   ps.linebuffer  = &linebuffer;
   ps.tokenbuffer = &tokenbuffer;
   ps.principals  = principals;

   // set initial state
   ps.state = PSTATE_NEEDLABEL;

   // need a line to start
   ps.needline = true;

   while(1)
   {
      // need a new line of input?
      if(ps.needline)
      {
         if(E_GetDSLine(&inputstr, &ps))
            break; // ran out of lines

         ps.needline = false;
      }

#ifdef RANGECHECK
      if(ps.state < 0 || ps.state >= PSTATE_NUMSTATES)
         I_Error("E_ParseDecorateInternal: internal error - undefined state\n");
#endif

      pstatefuncs[ps.state](&ps);

      // if last token processed was an EOL, we need a new line of input
      if(ps.tokentype == TOKEN_EOL)
         ps.needline = true;
   }

   // destroy qstrings
   M_QStrFree(&linebuffer);
   M_QStrFree(&tokenbuffer);
}

//
// E_DecoratePrincipals
//
// Counts and allocates labels, states, etc.
//
void E_DecoratePrincipals(const char *input)
{
   E_ParseDecorateInternal(input, true);
}

//
// E_ParseDecorateStates
//
// Main driver routine for parsing of DECORATE state blocks.
//
void E_ParseDecorateStates(const char *input)
{
   // init variables
   statebuffer = neweststate = NULL;
   numdeclabels = numdecstates = numkeywords = numgotos = 0;

   // parse for principals
   E_DecoratePrincipals(input);
}

#if 1
//=============================================================================
// 
// TEMPORARY DEBUG TEST CODE
//
// This code will dry-run the DECORATE state parser.
//

static const char *teststr = 
"Spawn:\n"
"   SPR1 ABCD 10 bright A_Foobar(0, 1, 2)\n"
"   loop\n"
"See:\n"
"   SPR2 ABCDE 20 A_WalkAround\n"
"   SPR2 F     5  A_DoNothing() \n"
"   stop\n"
"\n"
"  CustomLabel:\n"
"     goto See\n"
"  CustomLabel2: \n"
"     goto See+6\n";

/*
static const char *teststr =
"Spawn:\n"
"   goto Super::Spawn\n";
*/

void TestDSParser(void)
{
   E_ParseDecorateStates(teststr);

   // DEBUG: output buffered state objects
   if(statebuffer)
   {
      estatebuf_t *curstate = statebuffer;
      static const char *bufstatetypes[] =
      {
         "label", "state", "keyword", "goto"
      };

      while(curstate)
      {
         printf("%s (line %d): ", bufstatetypes[curstate->type], curstate->linenum);

         switch(curstate->type)
         {
         case BUF_LABEL:
            printf("%s\n", curstate->name);
            break;
         case BUF_STATE:
            printf("(nothing)\n");
            break;
         case BUF_KEYWORD:
            printf("%s\n", curstate->name);
            break;
         case BUF_GOTO:
            printf("%s + %s\n", curstate->gotodest, 
                   curstate->gotooffset ? curstate->gotooffset : "0");
            break;
         }
         curstate = (estatebuf_t *)(curstate->links.next);
      }
   }
}

#endif

// EOF

