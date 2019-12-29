//
// Copyright (C) 2018 James Haley, Max Waine, et al.
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
// Purpose: EDF Aeon Actions
// Authors: Samuel Villarreal, Max Waine
//

#define NEED_EDF_DEFINITIONS

#include "aeon_common.h"
#include "aeon_system.h"

#include "a_args.h"
#include "aeon_math.h"
#include "Confuse/confuse.h"
#include "e_actions.h"
#include "e_args.h"
#include "e_edf.h"
#include "e_hash.h"
#include "i_system.h"
#include "info.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "p_mobj.h"

#define ITEM_ACT_PLYRONLY "playeronly"
#define ITEM_ACT_CODE     "code"

// action
cfg_opt_t edf_action_opts[] =
{
   CFG_INT(ITEM_ACT_PLYRONLY, 0,       CFGF_SIGNPREFIX),
   CFG_STR(ITEM_ACT_CODE,     nullptr, CFGF_NONE),
   CFG_END()
};

// If a string starts with A_ strip it, otherwise add it
static inline qstring E_alternateFuncName(const char *name)
{
   if(strlen(name) > 2 && !strncasecmp(name, "A_", 2))
      return qstring(name + 2); // Strip A_
   else
      return qstring("A_") << name; // Add A_
}

//=============================================================================
//
// The special shell codepointer
//

void A_Aeon(actionargs_t *actionargs)
{
   asIScriptContext *ctx = Aeon::ScriptManager::Context();

   if(!Aeon::ScriptManager::PrepareFunction(actionargs->aeonaction->name))
      return;

   int argoffs = 1;
   if(actionargs->aeonaction->callType == ACT_ACTIONARGS)
      ctx->SetArgObject(0, actionargs);
   else if(actionargs->actiontype == actionargs_t::MOBJFRAME &&
           actionargs->aeonaction->callType == ACT_MOBJ)
      ctx->SetArgObject(0, actionargs->actor);
   else if(actionargs->actiontype == actionargs_t::WEAPONFRAME ||
           actionargs->actiontype == actionargs_t::ARTIFACT)
   {
      switch(actionargs->aeonaction->callType)
      {
      case ACT_MOBJ:
         ctx->SetArgObject(0, actionargs->actor);
         break;
      case ACT_PLAYER:
         ctx->SetArgObject(0, actionargs->actor->player);
         break;
      case ACT_PLAYER_W_PSPRITE:
         ctx->SetArgObject(0, actionargs->actor->player);
         ctx->SetArgObject(1, actionargs->pspr);
         argoffs++;
         break;
      case ACT_ACTIONARGS:
         ctx->SetArgObject(0, actionargs);
         break;
      }
   }
   else
      return;

   for(int i = 0; i < actionargs->aeonaction->numArgs; i++)
   {
      char        *argstr;
      Aeon::Fixed  argfx;

      char        *defaultarg   = nullptr;
      int          defaultint   = 0;
      Aeon::Fixed  defaultfixed = Aeon::Fixed(0);
      char        *defaultstr   = nullptr;

      switch(actionargs->aeonaction->argTypes[i])
      {
      case AAT_INTEGER:
         if(actionargs->aeonaction->defaultArgs[i])
            defaultint = *static_cast<int *>(actionargs->aeonaction->defaultArgs[i]);
         ctx->SetArgDWord(i + argoffs, E_ArgAsInt(actionargs->args, i, defaultint));
         break;
      case AAT_FIXED:
         if(actionargs->aeonaction->defaultArgs[i])
            defaultfixed = *static_cast<fixed_t *>(actionargs->aeonaction->defaultArgs[i]);
         argfx = Aeon::Fixed(E_ArgAsFixed(actionargs->args, i, defaultfixed));
         ctx->SetArgObject(i + argoffs, &argfx);
         break;
      case AAT_STRING:
         if(actionargs->aeonaction->defaultArgs[i])
            defaultarg = static_cast<char *>(actionargs->aeonaction->defaultArgs[i]);
         argstr = const_cast<char *>(E_ArgAsString(actionargs->args, i, defaultarg));
         ctx->SetArgAddress(i + argoffs, argstr);
         break;
      case AAT_SOUND:
         ctx->SetArgObject(i + argoffs, E_ArgAsSound(actionargs->args, i));
         break;
      default:
         Aeon::ScriptManager::PopState();
         return;
      }
   }

   if(!Aeon::ScriptManager::Execute())
      return;
}

//=============================================================================
//
// Action Definition (Aeon) and Action (Aeon and codepointer) Hash Tables
//

static
   EHashTable<actiondef_t, ENCStringHashKey,
              &actiondef_t::name, &actiondef_t::links> e_ActionDefHash;

static
   EHashTable<action_t, ENCStringHashKey,
              &action_t::name, &action_t::links> e_ActionHash;

actiondef_t *E_AeonActionForName(const char *name)
{
   actiondef_t *ret = e_ActionDefHash.objectForKey(name);

   if(ret)
      return ret;
   else
      return e_ActionDefHash.objectForKey(E_alternateFuncName(name).constPtr());
}

//
// Gets an action, Aeon or codepointer, for a given name
// If it's not already present in the action hash (and should be) then it adds it
//
action_t *E_GetAction(const char *name)
{
   actiondef_t *actiondef;
   deh_bexptr  *bexptr;
   action_t    *action;

   if(strlen(name) > 2 && !strncasecmp(name, "A_", 2))
      name += 2;

   if((action = e_ActionHash.objectForKey(name)))
      return action;
   if((actiondef = E_AeonActionForName(name)))
   {
      action = estructalloc(action_t, 1);
      action->aeonaction = actiondef;
      action->codeptr = action->oldcptr = A_Aeon;
   }
   else if((bexptr = D_GetBexPtr(name)))
   {
      action = estructalloc(action_t, 1);
      action->codeptr = action->oldcptr = bexptr->cptr;
   }
   else
      return nullptr;

   action->name = estrdup(name);
   e_ActionHash.addObject(action);
   return action;

}

//=============================================================================
//
// Processing
//

//
// Register the script action, creating the actiondef_t and adding it to the hash
//
static void E_registerScriptAction(const char *name, const char *funcname,
                                   const Collection<qstring> &argTypes,
                                   const Collection<const char *> &defaultArgs,
                                   const unsigned int numParams,
                                   const unsigned int nonArgParams,
                                   const actioncalltype_e callType)
{
   actionargtype_e  args[EMAXARGS];
   void            *defaults[EMAXARGS];

   for(int i = 0; i < EMAXARGS; i++)
   {
      args[i]     = AAT_INVALID;
      defaults[i] = nullptr;
   }

   for(unsigned int i = nonArgParams; i < numParams; i++)
   {
      if(argTypes[i] == "int")
      {
         args[i - 1] = AAT_INTEGER;
         if(estrnonempty(defaultArgs[i]))
         {
            int *temp       = emalloc(int *, 1);
            *temp           = static_cast<int>(strtol(defaultArgs[i], nullptr, 0));
            defaults[i - 1] = temp;
         }
      }
      else if(argTypes[i] == "fixed_t")
      {
         args[i - 1] = AAT_FIXED;
         if(estrnonempty(defaultArgs[i]))
         {
            fixed_t *temp     = emalloc(fixed_t *, 1);
            *temp             = M_DoubleToFixed(strtod(defaultArgs[i], nullptr));
            defaults[i - 1]   = temp;
         }
      }
      else if(argTypes[i] == "String" || argTypes[i] == "String&")
      {
         args[i - 1] = AAT_STRING;
         if(estrnonempty(defaultArgs[i]))
            defaults[i - 1] = estrdup(defaultArgs[i]);
      }
      else if(argTypes[i] == "EE::Sound@")
      {
         args[i - 1] = AAT_SOUND;
         if(estrnonempty(defaultArgs[i]))
            E_EDFLoggedErr(2, "E_registerScriptAction:Bla bla bla TODO \n");
      }
      else
      {
         E_EDFLoggedWarning(2, "E_registerScriptAction: action '%s' has invalid argument type "
                               "%s\n", name, argTypes[i].constPtr());
         return;
      }
   }

   actiondef_t *info = E_AeonActionForName(funcname);
   memcpy(info->argTypes,    args,     sizeof(args));
   memcpy(info->defaultArgs, defaults, sizeof(defaults));
   info->numArgs  = numParams - nonArgParams;
   info->callType = callType;
}

//
// Fetches a function based on a mnemonic, disregarding the A_
// The A_ is also stripped if necessary
//
static inline asIScriptFunction *E_aeonFuncForMnemonic(const char *mnemonic)
{
   asIScriptFunction *func;
   asIScriptModule *module = Aeon::ScriptManager::Module();

   if(!(func = module->GetFunctionByName(mnemonic)))
   {
      if(!(func = module->GetFunctionByName(E_alternateFuncName(mnemonic).constPtr())))
      {
         E_EDFLoggedErr(2, "E_processAction: Failed to find function '%s' or '%s' "
                           "in the code of action '%s'\n",
                        mnemonic, E_alternateFuncName(mnemonic).constPtr(), mnemonic);
      }
   }
   else if(module->GetFunctionByName(E_alternateFuncName(mnemonic).constPtr()))
   {
      E_EDFLoggedErr(2, "E_processAction: Both functions '%s' and '%s' found "
                        "in the code of action '%s'.\nPlease only define one or the other\n",
                     mnemonic, E_alternateFuncName(mnemonic).constPtr(), mnemonic);
   }

   return func;
}

//
// Return if a codepointer is not allowed to be overriden by Aeon
// (currently only A_Aeon is reserved)
//
static inline bool E_isReservedCodePointer(const char *name)
{
   if(strlen(name) > 2 && !strncasecmp(name, "A_", 2))
      name += 2;

   return !strcasecmp(name, "Aeon");
}

//
// Creates a single Aeon action
//
static void E_createAction(cfg_t *actionsec)
{
   // This is static as E_processAction is called many times
   static asIScriptModule *const module = Aeon::ScriptManager::Module();

   // The function and its constituent components
   const char *name = cfg_title(actionsec);
   const char *code = cfg_getstr(actionsec, ITEM_ACT_CODE);

   if(E_isReservedCodePointer(name))
   {
      E_EDFLoggedErr(2, "E_processAction: Action '%s' is reserved and cannot be "
                        "overriden by EDF\n", name);
   }

   qstring altname = E_alternateFuncName(name);
   if(e_ActionDefHash.objectForKey(altname.constPtr()))
   {
      E_EDFLoggedErr(2, "E_processAction: Action '%s' is already defined via an "
                        "EDF action, so '%s' is not allowed to be. If one is "
                        "defined then the other is also automatically defined\n",
                     altname.constPtr(), name);
   }

   if(!code)
      E_EDFLoggedErr(2, "E_processAction: Code block not supplied for action '%s'\n", name);

   module->AddScriptSection(name, code);

   actiondef_t *info = estructalloc(actiondef_t, 1);
   info->name = estrdup(name);

   e_ActionDefHash.addObject(info);
}

//
// Create all Aeon actions
//
void E_CreateActions(cfg_t *cfg)
{
   int i, numactions;

   E_EDFLogPuts("\t* Preparing Aeon actions\n");

   numactions = cfg_size(cfg, EDF_SEC_ACTION);
   E_EDFLogPrintf("\t\t%d Aeon action(s) created\n", numactions);
   for(i = 0; i < numactions; ++i)
   {
      cfg_t *sec = cfg_getnsec(cfg, EDF_SEC_ACTION, i);
      const char *title = cfg_title(sec);

      E_EDFLogPrintf("\tCreated Aeon action %s\n", title);

      // create action
      E_createAction(sec);
   }
}

//
// Populates a single Aeon action
//
static void E_populateAction(const char *const name)
{
   // This is static as E_whatever is called many times
   static asIScriptEngine *const e = Aeon::ScriptManager::Engine();

   // The various type infos of permitted first params (or second for EE::Psprite)
   static const int mobjTypeID    = e->GetTypeIdByDecl("EE::Mobj @");
   static const int playerTypeID  = e->GetTypeIdByDecl("EE::Player @");
   static const int psprTypeID    = e->GetTypeIdByDecl("EE::Psprite @");
   //static const int actArgsTypeID = e->GetTypeIdByDecl("EE::ActionArgs @");

   asIScriptFunction *func;
   int                nonArgParams = 1; // No. of parameters that aren't provided as EDF args
   actioncalltype_e   callType;

   // Get the Aeon function for a given mnemonic (like D_GetBexPtr)
   func = E_aeonFuncForMnemonic(name);

   // Verify that the first parameter is a Mobj or player_t handle
   int         typeID     = 0;
   const char *defaultArg = nullptr;
   if(func->GetParam(0, &typeID, nullptr, nullptr, &defaultArg) < 0)
      E_EDFLoggedErr(2, "E_processAction: No parameters defined for action '%s'\n", name);
   else if(estrnonempty(defaultArg))
      E_EDFLoggedErr(2, "E_processAction: Default argument TODO: Rest of error '%s'\n", name);

   typeID &= ~asTYPEID_HANDLETOCONST;

   const unsigned int paramCount = func->GetParamCount();
   if(typeID == mobjTypeID)
      callType = ACT_MOBJ;
   else if(typeID == playerTypeID)
   {
      if(func->GetParam(1, &typeID, nullptr, nullptr, &defaultArg) >= 0 &&
         typeID == psprTypeID)
      {
         callType = ACT_PLAYER_W_PSPRITE;
         nonArgParams = 2;
      }
      else
         callType = ACT_PLAYER;
   }
   //else if(typeID == actArgsTypeID)
   //{
   //   // If you're using raw actionargs_t then you don't need any more parameters
   //   if(paramCount > 1)
   //   {
   //      E_EDFLoggedWarning(2, "E_processAction: Too many arguments declared in action '%s'. "
   //                            "No arguments past the first are permitted in functions with
   //                            "'EE::ActionArgs @' as their first argument.\n", name);
   //      return;
   //   }
   //   E_registerScriptAction(name, func->GetName(), Collection<qstring>(),
   //                          Collection<const char *>(), 1, 1, ACT_ACTIONARGS);
   //   return;
   //}
   else
   {
      E_EDFLoggedErr(2, "E_processAction: First parameter of action '%s' must be of type "
                        "'EE::Mobj @', 'EE::Player @', or 'EE::ActionArgs'\n", name);
   }

   if(paramCount - nonArgParams > EMAXARGS)
   {
      E_EDFLoggedWarning(2, "E_processAction: Too many arguments declared in action '%s'. "
                            "Declared: %d, Allowed: %d\n",
                         name, paramCount, EMAXARGS + nonArgParams);
      return;
   }

   Collection<qstring>      argNameTypes;
   Collection<const char *> argDefaults;
   qstring     strTemp;
   const char *argTemp;
   // This loop is effectively kexScriptManager::GetArgTypesFromFunction from Powerslave EX
   for(unsigned int i = 0; i < paramCount; i++)
   {
      int idx;

      strTemp = func->GetVarDecl(i);

      // Remove everything except for the type: no identifier,
      // no const, just the type (and if it's a handle)
      if((idx = strTemp.find("@const")) != -1)
          strTemp.erase(idx + 1, 4 + 1);
      if((idx = strTemp.find("const")) != -1)
         strTemp.erase(idx, strTemp.find(" ") + 1 - idx);
      strTemp.erase(strTemp.find(" "), strTemp.length() - strTemp.find(" "));

      argNameTypes.add(qstring(strTemp));

      func->GetParam(i, nullptr, nullptr, nullptr, &argTemp);
      argDefaults.add(argTemp);
   }

   E_registerScriptAction(name, func->GetName(), argNameTypes, argDefaults,
                          paramCount,  nonArgParams, callType);
}

//
// Populate all Aeon actions
//
void E_PopulateActions()
{
   E_EDFLogPuts("\t* Populating Aeon actions\n");

   actiondef_t *def = nullptr;
   while((def = e_ActionDefHash.tableIterator(def)))
   {
      E_EDFLogPrintf("\Populated Aeon action %s\n", def->name);

      // populate action properties
      E_populateAction(def->name);
   }
}

// EOF

