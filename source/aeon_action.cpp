//
// The Eternity Engine
// Copyright(C) 2018 James Haley, Max Waine, et al.
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
// Purpose: Aeon bindings for interop with ACS
// Authors: Max Waine
//

#include "scriptarray.h"

#include "aeon_action.h"
#include "aeon_common.h"
#include "aeon_math.h"
#include "aeon_string.h"
#include "aeon_system.h"
#include "a_args.h"
#include "e_actions.h"
#include "e_hash.h"
#include "m_compare.h"
#include "m_qstr.h"
#include "m_utils.h"
#include "p_mobj.h"

#include "g_game.h"

namespace Aeon
{
   // This structure provides a record of unfound actions that Aeon has tried to call
   struct actionrecord_t
   {
      const char *name;
      DLListItem<actionrecord_t> links;
   };

   static
      EHashTable<actionrecord_t, ENCStringHashKey,
                 &actionrecord_t::name, &actionrecord_t::links> e_InvalidActionHash;

   static void ExecuteActionMobj(Mobj &mo, const qstring &name, const CScriptArray *argv)
   {
      action_t *action        = E_GetAction(name.constPtr());
      const int argc          = argv ? emin(static_cast<int>(argv->GetSize()), EMAXARGS) : 0;
      arglist_t arglist       = { {}, {}, argc };
      actionargs_t actionargs = { actionargs_t::MOBJFRAME, &mo, nullptr, &arglist };

      // Log if the provided action is invalid
      if(!action)
      {
         if(!e_InvalidActionHash.objectForKey(name.constPtr()))
         {
            doom_printf("Aeon: EE::ExecuteAction: Action '%s' not found\a\n",
                        name.constPtr());
            actionrecord_t *record = estructalloc(actionrecord_t, 1);
            record->name = name.duplicate();
            e_InvalidActionHash.addObject(record);
         }
         return;
      }

      for(int i = 0; i < argc; i++)
         arglist.args[i] = static_cast<qstring *>(const_cast<void *>(argv->At(i)))->getBuffer();

      action->codeptr(&actionargs);
   }

   //
   // It's just a qstring, but we pretend it isn't
   //
   class ActionArg
   {
   public:
      // CPP_FIXME: temporary placement construction for "action args"
      static void Construct(qstring *self)
      {
         ::new(self)qstring();
      }
      static void IntConstructor(const int val, qstring *self)
      {
         ::new(self)qstring();
         *self << val;
      }
      static void Destruct(qstring *self)
      {
         self->~qstring();
      }
   };

   static qstring &AssignStringToActionArg(const qstring &in, qstring *self)
   {
      return in.copyInto(*self);
   }

   static qstring &AssignIntToActionArg(const int val, qstring *self)
   {
      self->clear();
      *self << val;
      return *self;
   }

   #define EXECSIG(name, activatee) "void " name "(" activatee "," \
                                                  "const String &name,"   \
                                                  "const array<EE::actionarg_t> @args = null)"
   static const aeonfuncreg_t actionFuncs[] =
   {
      { EXECSIG("ExecuteAction", "Mobj &mo"),       WRAP_FN(ExecuteActionMobj)   },
    //{ EXECSIG("ExecuteAction", "Player &player"), WRAP_FN(ExecuteActionPlayer) },
   };

   void ScriptObjAction::Init()
   {
      asIScriptEngine *e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      // register type
      int ret = e->RegisterObjectType("actionarg_t", sizeof(qstring), asOBJ_VALUE | asOBJ_APP_CLASS_CDAK);

      e->RegisterObjectBehaviour("actionarg_t", asBEHAVE_CONSTRUCT, "void f()",
                                 WRAP_OBJ_LAST(ActionArg::Construct), asCALL_GENERIC);
      e->RegisterObjectBehaviour("actionarg_t", asBEHAVE_CONSTRUCT, "void f(const int)",
                                 WRAP_OBJ_LAST(ActionArg::IntConstructor), asCALL_GENERIC);
      e->RegisterObjectBehaviour("actionarg_t", asBEHAVE_DESTRUCT, "void f()",
                                 WRAP_OBJ_LAST(ActionArg::Destruct), asCALL_GENERIC);

      e->RegisterObjectMethod("actionarg_t", "actionarg_t &opAssign(const actionarg_t &in)",
                              WRAP_MFN_PR(qstring, operator =, (const qstring &), qstring &),
                              asCALL_GENERIC);
      e->RegisterObjectMethod("actionarg_t", "actionarg_t &opAssign(const String &in)",
                              WRAP_OBJ_LAST(AssignStringToActionArg), asCALL_GENERIC);
      e->RegisterObjectMethod("actionarg_t", "actionarg_t &opAssign(const int)",
                              WRAP_OBJ_LAST(AssignIntToActionArg), asCALL_GENERIC);

      for(const aeonfuncreg_t  &fn : actionFuncs)
         e->RegisterGlobalFunction(fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->SetDefaultNamespace("");
   }
}

// EOF

