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

   static EHashTable<actionrecord_t, ENCStringHashKey,
                     &actionrecord_t::name, &actionrecord_t::links> e_InvalidActionHash;

   static void executeActionMobj(Mobj *mo, const qstring &name, const CScriptArray *argv)
   {
      const action_t *action  = E_GetAction(name.constPtr());
      const int argc          = argv ? emin<int>(argv->GetSize(), EMAXARGS) : 0;
      arglist_t arglist       = { {}, {}, argc };
      actionargs_t actionargs;

      // Log if the desired action doesn't exist, if not already present in the hash table
      if(!action)
      {
         if(!e_InvalidActionHash.objectForKey(name.constPtr()))
         {
            doom_printf("Aeon: EE::Mobj::executeAction: Action '%s' not found\a\n",
                        name.constPtr());
            actionrecord_t *record = estructalloc(actionrecord_t, 1);
            record->name = name.duplicate();
            e_InvalidActionHash.addObject(record);
         }
         return;
      }

      // actionargs HAS to be assigned here
      actionargs = { actionargs_t::MOBJFRAME, mo, nullptr, &arglist, action->aeonaction };
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

      // Assignment functions
      static qstring &AssignString(const qstring &in, qstring *self)
      {
         return in.copyInto(*self);
      }
      static qstring &AssignInt(const int val, qstring *self)
      {
         self->clear();
         *self << val;
         return *self;
      }
      static qstring &AssignDouble(const double val, qstring *self)
      {
         self->clear();
         *self << val;
         return *self;
      }
      static qstring &AssignFixed(const fixed_t val, qstring *self)
      {
         char buf[19]; // minus, 5 digits max, dot, 11 digits max, null terminator
         psnprintf(buf, sizeof(buf), "%.11f", M_FixedToDouble(val));

         *self = buf;
         return *self;
      }
   };

   #define QSTRXFORM(m)  WRAP_MFN_PR(qstring, m, (const qstring &), qstring &)

   #define ASSIGNSIG(arg) "actionarg_t &opAssign(" arg ")"

   static const aeonfuncreg_t actionargFuncs[] =
   {
      { ASSIGNSIG("const actionarg_t &in"), QSTRXFORM(operator =)                  },
      { ASSIGNSIG("const String &in"),      WRAP_OBJ_LAST(ActionArg::AssignString) },
      { ASSIGNSIG("const int"),             WRAP_OBJ_LAST(ActionArg::AssignInt)    },
      { ASSIGNSIG("const double"),          WRAP_OBJ_LAST(ActionArg::AssignDouble) },
      { ASSIGNSIG("const fixed_t"),         WRAP_OBJ_LAST(ActionArg::AssignFixed)  }
   };

   #define EXECSIG(name) "void " name "(const String &name," \
                                       "const array<EE::actionarg_t> @args = null)"

   static const aeonbehaviorreg_t actionargBehaviors[] =
   {
      { asBEHAVE_CONSTRUCT, "void f()",          WRAP_OBJ_LAST(ActionArg::Construct)      },
      { asBEHAVE_CONSTRUCT, "void f(const int)", WRAP_OBJ_LAST(ActionArg::IntConstructor) },
      { asBEHAVE_DESTRUCT,  "void f()",          WRAP_OBJ_LAST(ActionArg::Destruct)       },
   };


   void ScriptObjAction::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      // Register actionarg_t, which is just a qstring that stuff automatically converts to
      e->RegisterObjectType("actionarg_t", sizeof(qstring), asOBJ_VALUE | asOBJ_APP_CLASS_CD);

      for(const aeonbehaviorreg_t &behavior : actionargBehaviors)
         e->RegisterObjectBehaviour("actionarg_t", behavior.behavior, behavior.declaration, behavior.funcPointer, asCALL_GENERIC);

      for(const aeonfuncreg_t &fn : actionargFuncs)
         e->RegisterObjectMethod("actionarg_t", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->RegisterObjectMethod(
         "Mobj", EXECSIG("executeAction"),
         WRAP_OBJ_FIRST(executeActionMobj), asCALL_GENERIC
      );

      //e->RegisterObjectMethod(
      //   "Player", EXECSIG("executeAction"),
      //   WRAP_OBJ_FIRST(executeActionPlayer), asCALL_GENERIC
      //);

      e->SetDefaultNamespace("");
   }
}

// EOF

