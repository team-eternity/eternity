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

#include "aeon_system.h"

#include "a_args.h"
#include "aeon_fixed.h"
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
static inline qstring AlternateFuncName(const char *name)
{
   if(strlen(name) > 2 && !strncasecmp(name, "A_", 2))
      return qstring(name + 2); // Strip A_
   else
      return qstring("A_") << name; // Add A_
}

// Fetches a function based on a mnemonic, disregarding the A_
static asIScriptFunction *E_aeonFuncForMnemonic(const char *mnemonic)
{
   asIScriptFunction *func;
   asIScriptModule *module = AeonScriptManager::Module();

   if(!(func = module->GetFunctionByName(mnemonic)))
   {
      if(!(func = module->GetFunctionByName(AlternateFuncName(mnemonic).constPtr())))
      {
         E_EDFLoggedErr(2, "E_processAction: Failed to find function '%s' or '%s' "
                           "in the code of action '%s'\n",
                        mnemonic, AlternateFuncName(mnemonic).constPtr(), mnemonic);
      }
   }
   else if(module->GetFunctionByName(AlternateFuncName(mnemonic).constPtr()))
   {
      E_EDFLoggedErr(2, "E_processAction: Both functions '%s' and '%s' found "
                        "in the code of action '%s'.\nPlease only define one or the other\n",
                     mnemonic, AlternateFuncName(mnemonic).constPtr(), mnemonic);
   }


   return func;
}

// Stripped down version of E_aeonFuncForMnemonic, made for usage at runtime (instead of
// during initialisation), as by now we know that the name resolves to a valid function
static asIScriptFunction *E_aeonFuncForMnemonicRuntime(const char *mnemonic)
{
   asIScriptFunction *func;
   asIScriptModule *module = AeonScriptManager::Module();

   if(!(func = module->GetFunctionByName(mnemonic)))
      return module->GetFunctionByName(AlternateFuncName(mnemonic).constPtr());

   return func;
}

//=============================================================================
//
// The special shell codepointer
//

void A_Aeon(actionargs_t *actionargs)
{
   if(!actionargs->aeonaction)
      I_Error("A_Aeon: Not bound to Aeon function (don't call A_Aeon from states directly)\n");

   if(!AeonScriptManager::PrepareFunction(E_aeonFuncForMnemonic(actionargs->aeonaction->name)))
      return;
   if(actionargs->actiontype == actionargs_t::MOBJFRAME)
      AeonScriptManager::Context()->SetArgObject(0, actionargs->actor);
   else
   {
      if(!actionargs->actor->player)
         return;
   }

   for(int i = 0; i < actionargs->aeonaction->numArgs; i++)
   {
      fixed_t argfixed;
      char *argstr;

      switch(actionargs->aeonaction->argTypes[i])
      {
      case AAT_INTEGER:
         AeonScriptManager::Context()->SetArgDWord(i+1, E_ArgAsInt(actionargs->args, i, 0));
         break;
      case AAT_FIXED:
         argfixed = E_ArgAsFixed(actionargs->args, i, 0);
         AeonScriptManager::Context()->SetArgObject(i+1, &AeonFixed(argfixed));
         break;
      case AAT_STRING:
         argstr = const_cast<char *>(E_ArgAsString(actionargs->args, i, nullptr));
         AeonScriptManager::Context()->SetArgAddress(i+1, argstr);
         break;
      default:
         AeonScriptManager::PopState();
         return;
        }
   }

   if(!AeonScriptManager::Execute())
      return;
}

//=============================================================================
//
// Action Definition Hash Table
//

static
   EHashTable<actiondef_t, ENCStringHashKey, &actiondef_t::name, &actiondef_t::links>
   e_ActionDefHash;

actiondef_t *E_AeonActionForName(const char *name)
{
   return e_ActionDefHash.objectForKey(name);
}

action_t *E_GetAction(const char *name)
{
   actiondef_t *actiondef;
   deh_bexptr  *bexptr;

   if(strlen(name) > 2 && !strncasecmp(name, "A_", 2))
      name += 2;

   if((actiondef = E_AeonActionForName(name)))
   {
      action_t *action = estructalloc(action_t, 1);
      action->aeonaction = actiondef;
      action->codeptr = action->oldcptr = A_Aeon;

      return action;
   }
   else if((bexptr = D_GetBexPtr(name)))
   {
      action_t *action = estructalloc(action_t, 1);
      action->codeptr = action->oldcptr = bexptr->cptr;

      return action;
   }
   else
      return nullptr;
}

static void E_registerScriptAction(const char *name, Collection<qstring> &argTypes,
                                   const unsigned int numArgs)
{
   actiondef_t *info;
   actionargtype_e args[EMAXARGS];
   for(actionargtype_e &arg : args)
      arg = AAT_INVALID;

   for(unsigned int i = 1; i < numArgs; i++)
   {
      if(argTypes[i] == "int")
         args[i - 1] = AAT_INTEGER;
      else if(argTypes[i] == "eFixed")
         args[i - 1] = AAT_FIXED;
      else if(argTypes[i] == "eString" || argTypes[i] == "eString&")
         args[i - 1] = AAT_STRING;
      else
      {
         E_EDFLoggedWarning(2, "E_registerScriptAction: action '%s' has invalid argument type "
                               "%s\n", name, argTypes[i].constPtr());
      }
   }

   info = estructalloc(actiondef_t, 1);
   info->name = estrdup(name);
   memcpy(info->argTypes, args, sizeof(args));
   info->numArgs = numArgs - 1;

   e_ActionDefHash.addObject(info);
}

static void E_processAction(cfg_t *actionsec)
{
   asIScriptFunction *func;
   asIScriptModule *module = AeonScriptManager::Module();
   static const asITypeInfo *mobjtypeinfo = AeonScriptManager::Engine()->GetTypeInfoByName("eMobj");
   const char *name = cfg_title(actionsec);
   const char *code = cfg_getstr(actionsec, ITEM_ACT_CODE);

   if(!code)
      E_EDFLoggedErr(2, "E_processAction: Code block not supplied for action '%s'\n", name);

   module->AddScriptSection("section", code);
   module->Build();

   func = E_aeonFuncForMnemonic(name);

   int typeID = 0;
   if(func->GetParam(0, &typeID) < 0)
      E_EDFLoggedErr(2, "E_processAction: No parameters defined for action '%s'\n", name);
   if(typeID != (mobjtypeinfo->GetTypeId() | asTYPEID_OBJHANDLE))
   {
      E_EDFLoggedErr(2, "E_processAction: First parameter of action '%s' must be of type "
                        "'eMobj @'\n", name);
   }

   // paramCount is off-by-one as the first param doesn't matter
   const unsigned int paramCount = func->GetParamCount();
   if(paramCount > EMAXARGS)
   {
      E_EDFLoggedWarning(2, "E_processAction: Too many arguments declared in action '%s'. "
                            "Declared: %d, Allowed: %d\n", name, paramCount, EMAXARGS + 1);
      return;
   }

   Collection<qstring> argNameTypes;
   qstring strTemp;
   // This loop is effectively kexScriptManager::GetArgTypesFromFunction from Powerslave EX
   for(unsigned int i = 0; i < paramCount; i++)
   {
      int idx;

      strTemp = func->GetVarDecl(i);
      idx = strTemp.find("const");

      // erase
      if(idx != -1)
         strTemp.erase(idx, strTemp.find(" ") + 1 - idx);
      // strTemp.Remove(idx, strTemp.find(" ")+1);

      strTemp.erase(strTemp.find(" "), strTemp.length() - strTemp.find(" "));
      // strTemp.Remove(strTemp.find(" "), strTemp.length());
      argNameTypes.add(qstring(strTemp));
   }

   E_registerScriptAction(name, argNameTypes, paramCount);
}

//
// Process all actions
//
void E_ProcessActions(cfg_t *cfg)
{
   int i, numpickups;

   E_EDFLogPuts("\t* Processing Aeon actions\n");

   // load pickupfx
   numpickups = cfg_size(cfg, EDF_SEC_ACTION);
   E_EDFLogPrintf("\t\t%d Aeon action(s) defined\n", numpickups);
   for(i = 0; i < numpickups; ++i)
   {
      cfg_t *sec = cfg_getnsec(cfg, EDF_SEC_ACTION, i);
      const char *title = cfg_title(sec);

      E_EDFLogPrintf("\tCreated Aeon action %s\n", title);

      // process action properties
      E_processAction(sec);
   }
}

// EOF

