//-----------------------------------------------------------------------------
//
// Copyright (C) 2015 David Hill
//
// See COPYING for license information.
//
//-----------------------------------------------------------------------------
//
// Scope classes.
//
//-----------------------------------------------------------------------------

#ifndef ACSVM__Scope_H__
#define ACSVM__Scope_H__

#include "Array.hpp"
#include "List.hpp"


//----------------------------------------------------------------------------|
// Types                                                                      |
//

namespace ACSVM
{
   //
   // GlobalScope
   //
   class GlobalScope
   {
   public:
      static constexpr std::size_t ArrC = 256;
      static constexpr std::size_t RegC = 256;


      GlobalScope(GlobalScope const &) = delete;
      GlobalScope(Environment *env, Word id);
      ~GlobalScope();

      void exec();

      HubScope *getHubScope(Word id);

      bool hasActiveThread();

      void lockStrings() const;

      void loadState(std::istream &in);

      void refStrings() const;

      void reset();

      void saveState(std::ostream &out) const;

      void unlockStrings() const;

      Environment *const env;
      Word         const id;

      Array arrV[ArrC];
      Word  regV[RegC];

      ListLink<GlobalScope>  hashLink;
      ListLink<ScriptAction> scriptAction;

      bool active;

   private:
      struct PrivData;

      PrivData *pd;
   };

   //
   // HubScope
   //
   class HubScope
   {
   public:
      static constexpr std::size_t ArrC = 256;
      static constexpr std::size_t RegC = 256;


      HubScope(HubScope const &) = delete;
      HubScope(GlobalScope *global, Word id);
      ~HubScope();

      void exec();

      MapScope *getMapScope(Word id);

      bool hasActiveThread();

      void lockStrings() const;

      void loadState(std::istream &in);

      void refStrings() const;

      void reset();

      void saveState(std::ostream &out) const;

      void unlockStrings() const;

      Environment *const env;
      GlobalScope *const global;
      Word         const id;

      Array arrV[ArrC];
      Word  regV[RegC];

      ListLink<HubScope>     hashLink;
      ListLink<ScriptAction> scriptAction;

      bool active;

   private:
      struct PrivData;

      PrivData *pd;
   };

   //
   // MapScope
   //
   class MapScope
   {
   public:
      MapScope(MapScope const &) = delete;
      MapScope(HubScope *hub, Word id);
      ~MapScope();

      void addModule(Module *module);

      //This must be called after all modules have been added.
      void addModuleFinish();

      void exec();

      Script *findScript(ScriptName name);
      Script *findScript(String *name);
      Script *findScript(Word name);

      ModuleScope *getModuleScope(Module *module);

      String *getString(Word idx) const;

      bool hasActiveThread();

      bool isScriptActive(Script *script);

      void loadState(std::istream &in);

      void lockStrings() const;

      void refStrings() const;

      void reset();

      void saveState(std::ostream &out) const;

      void scriptPause(Script *script);
      void scriptStart(Script *script, ThreadInfo const *info, Word const *argV, Word argC);
      void scriptStartForced(Script *script, ThreadInfo const *info, Word const *argV, Word argC);
      Word scriptStartResult(Script *script, ThreadInfo const *info, Word const *argV, Word argC);
      void scriptStartType(ScriptType type, ThreadInfo const *info, Word const *argV, Word argC);
      void scriptStop(Script *script);

      void unlockStrings() const;

      Environment *const env;
      HubScope    *const hub;
      Word         const id;

      ListLink<MapScope>     hashLink;
      ListLink<ScriptAction> scriptAction;
      ListLink<Thread>       threadActive;

      // Used for untagged string lookup.
      Module *module0;

      bool active;
      bool clampCallSpec;

   protected:
      void freeThread(Thread *thread);

   private:
      struct PrivData;

      void loadModules(std::istream &in);
      void loadThreads(std::istream &in);

      void saveModules(std::ostream &out) const;
      void saveThreads(std::ostream &out) const;

      PrivData *pd;
   };

   //
   // ModuleScope
   //
   class ModuleScope
   {
   public:
      static constexpr std::size_t ArrC = 256;
      static constexpr std::size_t RegC = 256;


      ModuleScope(ModuleScope const &) = delete;
      ModuleScope(MapScope *map, Module *module);
      ~ModuleScope();

      void import();

      void loadState(std::istream &in);

      void lockStrings() const;

      void refStrings() const;

      void saveState(std::ostream &out) const;

      void unlockStrings() const;

      Environment *const env;
      MapScope    *const map;
      Module      *const module;

      Array *arrV[ArrC];
      Word  *regV[RegC];

   private:
      Array selfArrV[ArrC];
      Word  selfRegV[RegC];
   };
}

#endif//ACSVM__Scope_H__

