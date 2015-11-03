//-----------------------------------------------------------------------------
//
// Copyright (C) 2015 David Hill
//
// See COPYING for license information.
//
//-----------------------------------------------------------------------------
//
// Deferred Action classes.
//
//-----------------------------------------------------------------------------

#include "Action.hpp"

#include "Environ.hpp"


//----------------------------------------------------------------------------|
// Extern Functions                                                           |
//

namespace ACSVM
{
   //
   // ScriptAction constructor
   //
   ScriptAction::ScriptAction(ScopeID id_, ScriptName name_, Action action_, Vector<Word> &&argV_) :
      action{action_},
      argV  {std::move(argV_)},
      id    {id_},
      link  {this},
      name  {name_}
   {
   }

   //
   // ScriptAction destructor
   //
   ScriptAction::~ScriptAction()
   {
   }

   //
   // ScriptAction::lockStrings
   //
   void ScriptAction::lockStrings(Environment *env) const
   {
      if(name.s) ++name.s->lock;

      for(auto &arg : argV)
         ++env->getString(arg)->lock;
   }

   //
   // ScriptAction::refStrings
   //
   void ScriptAction::refStrings(Environment *env) const
   {
      if(name.s) name.s->ref = true;

      for(auto &arg : argV)
         env->getString(arg)->ref = true;
   }

   //
   // ScriptAction::unlockStrings
   //
   void ScriptAction::unlockStrings(Environment *env) const
   {
      if(name.s) --name.s->lock;

      for(auto &arg : argV)
         --env->getString(arg)->lock;
   }
}

// EOF

