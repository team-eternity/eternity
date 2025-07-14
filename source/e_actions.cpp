//
// Copyright (C) 2025 James Haley, Max Waine, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: EDF Aeon actions.
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

constexpr const char ITEM_ACT_PLYRONLY[] = "playeronly";
constexpr const char ITEM_ACT_CODE[]     = "code";

// clang-format off

// action
cfg_opt_t edf_action_opts[] =
{
    CFG_INT(ITEM_ACT_PLYRONLY, 0,       CFGF_SIGNPREFIX),
    CFG_STR(ITEM_ACT_CODE,     nullptr, CFGF_NONE),
    CFG_END(),
};

// clang-format on

// If an action starts with A_ strip it, otherwise add it
static inline qstring E_alternateFuncName(const char *name)
{
    qstring      qname(name);
    qstring      qnamespace;
    const size_t sepPos = qname.find("::");

    if(sepPos != qstring::npos)
    {
        // Set qnamespace to be everything before the ::
        qname.copyInto(qnamespace);
        qnamespace.truncate(sepPos);

        // Set qname to be everything after the ::
        qname.erase(0, sepPos + 2);
    }

    if(qname.length() > 2 && !qname.strNCaseCmp("A_", 2))
        qname.erase(0, 2); // Strip A_
    else
        qname = qstring("A_").concat(qname); // Add A_

    if(sepPos != qstring::npos)
        return qnamespace << "::" << qname;
    else
        return qname;
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
    else if(actionargs->actiontype == actionargs_t::MOBJFRAME && actionargs->aeonaction->callType == ACT_MOBJ)
        ctx->SetArgObject(0, actionargs->actor);
    else if(actionargs->actiontype == actionargs_t::WEAPONFRAME || actionargs->actiontype == actionargs_t::ARTIFACT)
    {
        switch(actionargs->aeonaction->callType)
        {
        case ACT_MOBJ:   ctx->SetArgObject(0, actionargs->actor); break;
        case ACT_PLAYER: ctx->SetArgObject(0, actionargs->actor->player); break;
        case ACT_PLAYER_W_PSPRITE:
            ctx->SetArgObject(0, actionargs->actor->player);
            ctx->SetArgObject(1, actionargs->pspr);
            argoffs++;
            break;
        case ACT_ACTIONARGS: ctx->SetArgObject(0, actionargs); break;
        }
    }
    else
        return;

    for(int i = 0; i < actionargs->aeonaction->numArgs; i++)
    {
        char   *argstr;
        fixed_t argfx;

        char   *defaultarg   = nullptr;
        int     defaultint   = 0;
        fixed_t defaultfixed = 0;
        char   *defaultstr   = nullptr;

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
            argfx = E_ArgAsFixed(actionargs->args, i, defaultfixed);
            ctx->SetArgObject(i + argoffs, &argfx);
            break;
        case AAT_STRING:
            if(actionargs->aeonaction->defaultArgs[i])
                defaultarg = static_cast<char *>(actionargs->aeonaction->defaultArgs[i]);
            argstr = const_cast<char *>(E_ArgAsString(actionargs->args, i, defaultarg));
            ctx->SetArgAddress(i + argoffs, argstr);
            break;
        case AAT_SOUND: ctx->SetArgObject(i + argoffs, E_ArgAsSound(actionargs->args, i)); break;
        default:        Aeon::ScriptManager::PopState(); return;
        }
    }

    if(!Aeon::ScriptManager::Execute())
        return;
}

//=============================================================================
//
// Action Definition (Aeon) and Action (Aeon and codepointer) Hash Tables
//

static EHashTable<actiondef_t, ENCStringHashKey, &actiondef_t::name, &actiondef_t::links> e_ActionDefHash;

static EHashTable<action_t, ENCStringHashKey, &action_t::name, &action_t::links> e_ActionHash;

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

    // FIXME: Is this not correct any more?
    if(strlen(name) > 2 && !strncasecmp(name, "A_", 2))
        name += 2;

    if((action = e_ActionHash.objectForKey(name)))
        return action;
    if((actiondef = E_AeonActionForName(name)))
    {
        action             = estructalloc(action_t, 1);
        action->aeonaction = actiondef;
        action->codeptr = action->oldcptr = A_Aeon;
    }
    else if((bexptr = D_GetBexPtr(name)))
    {
        action          = estructalloc(action_t, 1);
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
static void E_registerScriptAction(actiondef_t *action, PODCollection<actiondef_t *> &renamedActions,
                                   const char *funcname, const Collection<qstring> &argTypes,
                                   const Collection<const char *> &defaultArgs, const unsigned int numParams,
                                   const unsigned int nonArgParams, const actioncalltype_e callType)
{
    asIScriptModule *module = Aeon::ScriptManager::Module();

    actionargtype_e args[EMAXARGS];
    void           *defaults[EMAXARGS];

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
                int *temp       = emalloc(int *, sizeof(int));
                *temp           = static_cast<int>(strtol(defaultArgs[i], nullptr, 0));
                defaults[i - 1] = temp;
            }
        }
        else if(argTypes[i] == "fixed_t")
        {
            args[i - 1] = AAT_FIXED;
            if(estrnonempty(defaultArgs[i]))
            {
                fixed_t *temp   = emalloc(fixed_t *, sizeof(fixed_t));
                *temp           = M_DoubleToFixed(strtod(defaultArgs[i], nullptr));
                defaults[i - 1] = temp;
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
                E_EDFLoggedErr(2, "E_registerScriptAction: Bla bla bla TODO \n");
        }
        else
        {
            E_EDFLoggedWarning(2, "E_registerScriptAction: action '%s' has invalid argument type %s\n", action->name,
                               argTypes[i].constPtr());
            return;
        }
    }

    // Populate properties
    memcpy(action->argTypes, args, sizeof(args));
    memcpy(action->defaultArgs, defaults, sizeof(defaults));
    action->numArgs  = numParams - nonArgParams;
    action->callType = callType;

    if(!module->GetFunctionByName(action->name))
        renamedActions.add(action);
}

//
// Fetches a function based on a mnemonic,working
// regardless of whether or not A_ prefixes are or aren't present
//
static inline asIScriptFunction *E_aeonFuncForMnemonic(const char *mnemonic)
{
    asIScriptFunction *func;
    asIScriptModule   *module = Aeon::ScriptManager::Module();

    if(!(func = module->GetFunctionByName(mnemonic)))
    {
        if(!(func = module->GetFunctionByName(E_alternateFuncName(mnemonic).constPtr())))
        {
            E_EDFLoggedErr(2, "E_processAction: Failed to find function '%s' or '%s' in the code of action '%s'\n",
                           mnemonic, E_alternateFuncName(mnemonic).constPtr(), mnemonic);
        }
    }
    else if(module->GetFunctionByName(E_alternateFuncName(mnemonic).constPtr()))
    {
        E_EDFLoggedErr(2,
                       "E_processAction: Both functions '%s' and '%s' found in the code of action '%s'.\nPlease only "
                       "define one or the other\n",
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
// Verify the appropriateness of an action name
//
static void E_verifyActionName(const char *name)
{
    if(E_isReservedCodePointer(name))
    {
        E_EDFLoggedErr(2, "E_processAction: Action '%s' is reserved and cannot be overriden by EDF\n", name);
    }

    // Doing "::Foo" is an acceptable error,
    // but if users do "Foo::" or "Foo::A_" they deserve what they get
    qstring      qname(name);
    const size_t sepPos = qname.find("::");
    if(sepPos == 0)
        E_EDFLoggedErr(2, "E_processAction: Action '%s' has no namespace before the '::'\n", name);
    else if(sepPos != qstring::npos)
    {
        if(sepPos + 2 >= qname.length())
            E_EDFLoggedErr(2, "E_processAction: Action '%s' has no action name after the '::'\n", name);

        qname.erase(0, sepPos + 2);
        if(qname.length() == 2 && !qname.strCaseCmp("A_"))
        {
            E_EDFLoggedErr(2, "E_processAction: Action '%s' doesn't have a valid action name after the '::'.\n", name);
        }
    }
}

//
// Create a single Aeon action that comes from a plain AngelScript file
//
void E_DefineAction(const char *name)
{
    E_verifyActionName(name);

    actiondef_t *info = estructalloc(actiondef_t, 1);
    info->name        = estrdup(name);

    e_ActionDefHash.addObject(info);
}

//
// Creates a single Aeon action
//
static void E_createAction(cfg_t *actionsec)
{
    // This is static as E_createAction is called many times
    static asIScriptModule *const module = Aeon::ScriptManager::Module();

    // The function and its constituent components
    const char *name = cfg_title(actionsec);
    const char *code = cfg_getstr(actionsec, ITEM_ACT_CODE);

    E_verifyActionName(name);

    qstring altname = E_alternateFuncName(name);
    if(e_ActionDefHash.objectForKey(altname.constPtr()))
    {
        E_EDFLoggedErr(2,
                       "E_processAction: Action '%s' is already defined via an "
                       "EDF action, so '%s' is not allowed to be. If one is "
                       "defined then the other is also automatically defined\n",
                       altname.constPtr(), name);
    }

    if(!code)
        E_EDFLoggedErr(2, "E_processAction: Code block not supplied for action '%s'\n", name);

    module->AddScriptSection(name, code);

    actiondef_t *info = estructalloc(actiondef_t, 1);
    info->name        = estrdup(name);

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
        cfg_t      *sec   = cfg_getnsec(cfg, EDF_SEC_ACTION, i);
        const char *title = cfg_title(sec);

        E_EDFLogPrintf("\tCreated Aeon action %s\n", title);

        // create action
        E_createAction(sec);
    }
}

//
// Populates a single Aeon action
//
static void E_populateAction(actiondef_t *action, PODCollection<actiondef_t *> &renamedActions)
{
    // This is static as E_populateAction is called many times
    static asIScriptEngine *const e = Aeon::ScriptManager::Engine();

    // The various type infos of permitted first params (or second for EE::Psprite)
    static const int mobjTypeID   = e->GetTypeIdByDecl("EE::Mobj @");
    static const int playerTypeID = e->GetTypeIdByDecl("EE::Player @");
    static const int psprTypeID   = e->GetTypeIdByDecl("EE::Psprite @");
    // static const int actArgsTypeID = e->GetTypeIdByDecl("EE::ActionArgs @");

    asIScriptFunction *func;
    int                nonArgParams = 1; // No. of parameters that aren't provided as EDF args
    actioncalltype_e   callType;

    // Get the Aeon function for a given mnemonic (like D_GetBexPtr)
    func = E_aeonFuncForMnemonic(action->name);

    // Verify that the first parameter is a Mobj or player_t handle
    int         typeID     = 0;
    const char *defaultArg = nullptr;
    if(func->GetParam(0, &typeID, nullptr, nullptr, &defaultArg) < 0)
        E_EDFLoggedErr(2, "E_processAction: No parameters defined for action '%s'\n", action->name);
    else if(estrnonempty(defaultArg))
        E_EDFLoggedErr(2, "E_processAction: Default argument TODO: Rest of error '%s'\n", action->name);

    typeID &= ~asTYPEID_HANDLETOCONST;

    const unsigned int paramCount = func->GetParamCount();
    if(typeID == mobjTypeID)
        callType = ACT_MOBJ;
    else if(typeID == playerTypeID)
    {
        if(func->GetParam(1, &typeID, nullptr, nullptr, &defaultArg) >= 0 && typeID == psprTypeID)
        {
            callType     = ACT_PLAYER_W_PSPRITE;
            nonArgParams = 2;
        }
        else
            callType = ACT_PLAYER;
    }
    // else if(typeID == actArgsTypeID)
    //{
    //    // If you're using raw actionargs_t then you don't need any more parameters
    //    if(paramCount > 1)
    //    {
    //       E_EDFLoggedWarning(2, "E_processAction: Too many arguments declared in action '%s'. "
    //                             "No arguments past the first are permitted in functions with
    //                             "'EE::ActionArgs @' as their first argument.\n", name);
    //       return;
    //    }
    //    E_registerScriptAction(name, func->GetName(), Collection<qstring>(),
    //                           Collection<const char *>(), 1, 1, ACT_ACTIONARGS);
    //    return;
    // }
    else
    {
        E_EDFLoggedErr(2,
                       "E_processAction: First parameter of action '%s' must be of type 'EE::Mobj @', 'EE::Player @', "
                       "or 'EE::ActionArgs'\n",
                       action->name);
    }

    if(paramCount - nonArgParams > EMAXARGS)
    {
        E_EDFLoggedWarning(2,
                           "E_processAction: Too many arguments declared in action '%s'. Declared: %d, Allowed: %d\n",
                           action->name, paramCount, EMAXARGS + nonArgParams);
        return;
    }

    Collection<qstring>      argNameTypes;
    Collection<const char *> argDefaults;
    const char              *argTemp;
    // This loop is effectively kexScriptManager::GetArgTypesFromFunction from Powerslave EX
    for(unsigned int i = 0; i < paramCount; i++)
    {
        size_t  idx;
        qstring strTemp{ func->GetVarDecl(i) };

        // Remove everything except for the type: no identifier,
        // no const, just the type (and if it's a handle)
        if(idx = strTemp.find("@const"); idx != -1)
            strTemp.erase(idx + 1, 4 + 1);
        if(idx = strTemp.find("const"); idx != -1)
            strTemp.erase(idx, strTemp.find(" ") + 1 - idx);
        strTemp.erase(strTemp.find(" "), strTemp.length() - strTemp.find(" "));

        argNameTypes.add(std::move(strTemp));

        func->GetParam(i, nullptr, nullptr, nullptr, &argTemp);
        argDefaults.add(argTemp);
    }

    E_registerScriptAction(action, renamedActions, func->GetName(), argNameTypes, argDefaults, paramCount, nonArgParams,
                           callType);
}

//
// Populate all Aeon actions
//
void E_PopulateActions()
{
    E_EDFLogPuts("\t* Populating Aeon actions\n");

    PODCollection<actiondef_t *> renamedActions;

    actiondef_t *action = nullptr;
    while((action = e_ActionDefHash.tableIterator(action)))
    {
        E_EDFLogPrintf("\tPopulated Aeon action %s\n", action->name);

        // populate action properties
        E_populateAction(action, renamedActions);
    }

    for(actiondef_t *renamedAction : renamedActions)
    {
        e_ActionDefHash.removeObject(renamedAction);
        const char *temp = E_alternateFuncName(renamedAction->name).duplicate();
        efree(const_cast<char *>(renamedAction->name));
        renamedAction->name = temp;
        e_ActionDefHash.addObject(renamedAction);
    }
}

// EOF

