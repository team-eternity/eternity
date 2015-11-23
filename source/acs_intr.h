// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2015 David Hill, James Haley, et al.
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
// Original 100% GPL ACS Interpreter
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef ACS_INTR_H__
#define ACS_INTR_H__

#include "m_dllist.h"
#include "p_tick.h"
#include "r_defs.h"

#include "acsvm/Environment.hpp"
#include "acsvm/Thread.hpp"

class  qstring;
class  Mobj;
struct line_t;
class  WadDirectory;

//
// Defines
//

#define ACS_NUM_STACK      128
#define ACS_NUM_CALLS      16
#define ACS_NUM_PRINTS     4
#define ACS_NUM_LOCALVARS  20
#define ACS_NUM_MAPVARS    32
#define ACS_NUM_MAPARRS    32
#define ACS_NUM_WORLDVARS  64
#define ACS_NUM_WORLDARRS  64
#define ACS_NUM_GLOBALVARS 64
#define ACS_NUM_GLOBALARRS 64
#define ACS_NUM_THINGTYPES 256

// ACS array constants
#define ACS_PAGESIZE 1024
#define ACS_BLOCKSIZE 512
#define ACS_REGIONSIZE 512
#define ACS_ARRDATASIZE 16

// ACS string constants
// padding when header is allocated with payload
#define ACS_STRING_SIZE_PADDED ((sizeof(ACSString) + 7) & ~7)

#define ACS_FUNCARG ACSThinker *thread, uint32_t argc, const int32_t *args, int32_t *&retn

#define ACS_CF_ARGS ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC

#define ACS_CHUNKID(A,B,C,D) (((A) << 0) | ((B) << 8) | ((C) << 16) | ((D) << 24))

//
// Misc. Enums
//

// ACS game mode flags for EDF thingtypes

enum
{
   ACS_MODE_DOOM = 0x00000001,
   ACS_MODE_HTIC = 0x00000002,
   ACS_MODE_ALL  = ACS_MODE_DOOM | ACS_MODE_HTIC,
};

// ACS_Execute flags
enum
{
   ACS_EXECUTE_ALWAYS    = 0x00000001,
   ACS_EXECUTE_IMMEDIATE = 0x00000002,
};

//
// acs_op
//
typedef enum acs_op_e
{
   #define ACS_OP(OP,ARGC) ACS_OP_##OP,
   #include "acs_op.h"
   #undef ACS_OP

   ACS_OPMAX
} acs_op_t;

// CALLFUNC indexes.
enum acs_funcnum_t
{
   #define ACS_FUNC(FUNC) ACS_FUNC_##FUNC,
   #include "acs_op.h"
   #undef ACS_FUNC

   ACS_FUNCMAX
};

// script types
enum acs_stype_t
{
   ACS_STYPE_CLOSED,
   ACS_STYPE_OPEN,

   ACS_STYPEMAX
};

// Tag types.
enum acs_tagtype_t
{
   ACS_TAGTYPE_POLYOBJ,
   ACS_TAGTYPE_SECTOR,
};

// Level Properties.
enum
{
   ACS_LP_ParTime        = 0,
   ACS_LP_ClusterNumber  = 1,
   ACS_LP_LevelNumber    = 2,
   ACS_LP_TotalSecrets   = 3,
   ACS_LP_FoundSecrets   = 4,
   ACS_LP_TotalItems     = 5,
   ACS_LP_FoundItems     = 6,
   ACS_LP_TotalMonsters  = 7,
   ACS_LP_KilledMonsters = 8,
   ACS_LP_SuckTime       = 9,

   ACS_LPMAX
};

// Thing properties.
enum
{
   ACS_TP_Health       =  0,
   ACS_TP_Speed        =  1,
   ACS_TP_Damage       =  2,
   ACS_TP_Alpha        =  3,
   ACS_TP_RenderStyle  =  4,
   ACS_TP_SeeSound     =  5,
   ACS_TP_AttackSound  =  6,
   ACS_TP_PainSound    =  7,
   ACS_TP_DeathSound   =  8,
   ACS_TP_ActiveSound  =  9,
   ACS_TP_Ambush       = 10,
   ACS_TP_Invulnerable = 11,
   ACS_TP_JumpZ        = 12,
   ACS_TP_ChaseGoal    = 13,
   ACS_TP_Frightened   = 14,
   ACS_TP_Gravity      = 15,
   ACS_TP_Friendly     = 16,
   ACS_TP_SpawnHealth  = 17,
   ACS_TP_Dropped      = 18,
   ACS_TP_NoTarget     = 19,
   ACS_TP_Species      = 20,
   ACS_TP_NameTag      = 21,
   ACS_TP_Score        = 22,
   ACS_TP_NoTrigger    = 23,
   ACS_TP_DamageFactor = 24,
   ACS_TP_MasterTID    = 25,
   ACS_TP_TargetTID    = 26,
   ACS_TP_TracerTID    = 27,
   ACS_TP_WaterLevel   = 28,
   ACS_TP_ScaleX       = 29,
   ACS_TP_ScaleY       = 30,
   ACS_TP_Dormant      = 31,
   ACS_TP_Mass         = 32,
   ACS_TP_Accuracy     = 33,
   ACS_TP_Stamina      = 34,
   ACS_TP_Height       = 35,
   ACS_TP_Radius       = 36,
   ACS_TP_ReactionTime = 37,
   ACS_TP_MeleeRange   = 38,
   ACS_TP_ViewHeight   = 39,
   ACS_TP_AttackZOff   = 40,
   ACS_TP_StencilColor = 41,
   ACS_TP_Friction     = 42,
   ACS_TP_DamageMult   = 43,

   // Internal properties, not meant for external use.
   ACS_TP_Angle,
   ACS_TP_Armor,
   ACS_TP_CeilTex,
   ACS_TP_CeilZ,
   ACS_TP_FloorTex,
   ACS_TP_FloorZ,
   ACS_TP_Frags,
   ACS_TP_LightLevel,
   ACS_TP_MomX,
   ACS_TP_MomY,
   ACS_TP_MomZ,
   ACS_TP_Pitch,
   ACS_TP_PlayerNumber,
   ACS_TP_SigilPieces,
   ACS_TP_TID,
   ACS_TP_Type,
   ACS_TP_X,
   ACS_TP_Y,
   ACS_TP_Z,

   ACS_TPMAX
};

//
// Structures
//

class ACSScript;
class ACSThinker;
class ACSModule;

typedef void (*acs_func_t)(ACS_FUNCARG);

//
// ACSEnvironment
//
class ACSEnvironment : public ACSVM::Environment
{
protected:
   virtual ACSVM::Thread *allocThread();

   virtual ACSVM::Word callSpecImpl(ACSVM::Thread *thread, ACSVM::Word spec,
                                    const ACSVM::Word *argV, ACSVM::Word argC);

public:
   using ACSVM::Environment::getModuleName;

   ACSEnvironment();

   virtual bool checkTag(ACSVM::Word type, ACSVM::Word tag);

   virtual ACSVM::ModuleName getModuleName(char const *str, size_t len);

   virtual void loadModule(ACSVM::Module *module);

   virtual void refStrings();

   WadDirectory       *dir;
   ACSVM::GlobalScope *global;
   ACSVM::HubScope    *hub;
   ACSVM::MapScope    *map;
   ACSVM::String      *strBEHAVIOR;

   size_t errors;
};

//
// ACSThreadInfo
//
class ACSThreadInfo : public ACSVM::ThreadInfo
{
public:
   ACSThreadInfo() : mo{nullptr}, line{nullptr}, side{0} {}
   ACSThreadInfo(Mobj *mo_, line_t *line_, int side_) :
      mo{mo_}, line{line_}, side{side_} {}

   Mobj   *mo;   // Mobj that activated.
   line_t *line; // Line that activated.
   int     side; // Side of actived line.
};

//
// ACSThread
//
class ACSThread : public ACSVM::Thread
{
public:
   ACSThread(ACSVM::Environment *env_) : ACSVM::Thread{env_} {}

   virtual ACSVM::ThreadInfo const *getInfo() const {return &info;}

   virtual void start(ACSVM::Script *script, ACSVM::MapScope *map,
      const ACSVM::ThreadInfo *info, const ACSVM::Word *argV, ACSVM::Word argC);

   ACSThreadInfo info;
};

//
// ACSArray
//
// Stores an "array" with a logical size of 2^32, but is actually only
// allocated as needed.
//
class ACSArray
{
private:
   typedef int32_t val_t;
   typedef val_t  page_t[ACS_PAGESIZE];
   typedef page_t *block_t[ACS_BLOCKSIZE];
   typedef block_t *region_t[ACS_REGIONSIZE];
   typedef region_t *arrdata_t[ACS_ARRDATASIZE];

   void archiveArrdata(SaveArchive &arc, arrdata_t *arrdata);
   void archiveRegion(SaveArchive &arc, region_t *region);
   void archiveBlock(SaveArchive &arc, block_t *block);
   void archivePage(SaveArchive &arc, page_t *page);
   void archiveVal(SaveArchive &arc, val_t *val);

   arrdata_t &getArrdata() {return arrdata;}
   region_t &getRegion(uint32_t addr);
   block_t &getBlock(uint32_t addr);
   page_t &getPage(uint32_t addr);
   val_t &getVal(uint32_t addr)
   {
      return getPage(addr / ACS_PAGESIZE)[addr % ACS_PAGESIZE];
   }

   arrdata_t arrdata;

public:
   ACSArray() {memset(arrdata, 0, sizeof(arrdata));}
   ~ACSArray() {clear();}

   void clear();

   val_t &at(uint32_t addr) {return getVal(addr);}

   val_t &operator [] (uint32_t addr) {return getVal(addr);}

   void archive(SaveArchive &arc);

   bool copyString(uint32_t offset, uint32_t length, uint32_t strnum, uint32_t stroff);
   void print(qstring *printBuffer, uint32_t offset, uint32_t length = 0xFFFFFFFF);
};

//
// ACSString
//
// Stores information about a VM string.
//
class ACSString
{
public:
   struct Data
   {
      const char *s; // the actual string
      uint32_t    l; // allocated size
   } data;

   // metadata
   ACSScript  *script; // script this string is a name of, if any
   uint32_t    length; // null-terminated length
   uint32_t    number; // index into global array

   DLListItem<ACSString> dataLinks;
};

//
// acs_opdata
//
struct acs_opdata_t
{
   acs_op_t op;
   int args;
};

//
// ACSFunc
//
// Like ACSScript below, but for user-defined functions.
//
class ACSFunc
{
public:
   uint32_t number;  // number in GlobalFuncs array
   uint8_t  numArgs; // argument count
   uint8_t  numVars; // variable count
   uint8_t  numRetn; // return count
   ACSModule *vm;      // VM the function is from

   union
   {
      uint32_t codeIndex;
      int32_t *codePtr;
   };
};

//
// ACSJump
//
union ACSJump
{
   uint32_t codeIndex;
   int32_t *codePtr;
};

//
// ACSScript
//
// An actual script entity as read from the ACS lump's entry table,
// augmented with basic runtime data.
//
class ACSScript
{
public:
   const char *name;    // script's name, if any
   int32_t     number;  // the script's number; negative means named
   uint32_t    numArgs; // expected arguments
   uint16_t    numVars; // local variable count
   uint16_t    flags;   // script flags
   acs_stype_t type;    // script type
   ACSModule  *vm;      // VM the script is from
   ACSThinker *threads;

   union
   {
      uint32_t codeIndex;
      int32_t *codePtr;
   };

   DLListItem<ACSScript> nameLinks;
   DLListItem<ACSScript> numberLinks;
};

//
// acs_call_t
//
// Stores a call-frame for ACS execution.
//
struct acs_call_t
{
   int32_t *ip;
   uint32_t numLocals;
   ACSModule *vm;
};

//
// ACSThinker
//
// A thinker which runs a script.
//
class ACSThinker : public Thinker
{
   DECLARE_THINKER_TYPE(ACSThinker, Thinker)

protected:
   // Data Members
   unsigned int triggerSwizzle; // Holds the swizzled target during loading

   // Methods
   void Think();

public:
   ACSThinker() : result(0), calls(NULL), callPtr(NULL), numCalls(0),
                  printStack(NULL), printPtr(NULL), numPrints(0), printBuffer(NULL)
   {
   }

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual void deSwizzle();

   // Invokes bytecode execution.
   void exec() { Think(); }

   void pushPrint();
   void popPrint();

   // Data Members
   // thread links
   ACSThinker **prevthread;
   ACSThinker  *nextthread;

   // virtual machine data
   int32_t     result;      // user-defined result of the thread
   int32_t    *ip;          // instruction pointer
   int32_t    *stack;       // value stack
   uint32_t    stackPtr;    // stack pointer
   uint32_t    numStack;    // stack size
   int32_t    *localvar;    // local variable storage area
   uint32_t    numLocalvar; // local variable allocation size
   int32_t    *locals;      // local variables and arguments
   uint32_t    numLocals;   // number of local variables
   int32_t     sreg;        // state register
   int32_t     sdata;       // special data for state
   acs_call_t *calls;       // call frames
   acs_call_t *callPtr;     // current call frame
   uint32_t    numCalls;    // number of call frames
   qstring   **printStack;  // print buffers
   qstring   **printPtr;    // current print buffer
   uint32_t    numPrints;   // number of print buffers
   qstring    *printBuffer; // print buffer

   // info copied from acscript and acsvm
   ACSScript *script;       // the script being executed
   ACSModule *vm;           // the current execution environment

   // misc
   int32_t delay;           // counter for script delays
   Mobj   *trigger;         // thing that activated
   line_t *line;            // line that activated
   int     lineSide;        // line side of activation
};

//
// ACSDeferred
//
// This class keeps track of ACS actions to be taken on other maps.
//
class ACSDeferred : public ZoneObject
{
private:
   // deferred action types
   enum
   {
      EXECUTE_NUMBER,
      EXECUTE_NAME,
      SUSPEND_NUMBER,
      SUSPEND_NAME,
      TERMINATE_NUMBER,
      TERMINATE_NAME,
   };

   ACSDeferred() : type(-1), name(NULL), argv(NULL)
   {
      link.insert(this, &list);
   }

   ACSDeferred(int pType, int32_t pNumber, char *pName, int pMapnum,
               int pFlags, int32_t *pArgv, uint32_t pArgc)
      : type(pType), number(pNumber), name(pName), mapnum(pMapnum),
        flags(pFlags), argv(pArgv), argc(pArgc)
   {
      link.insert(this, &list);
   }

   ~ACSDeferred();

   void archive(SaveArchive &arc);
   void execute();

   int      type;   // type of action to perform
   int32_t  number; // ACS script number
   char    *name;   // ACS script name
   int      mapnum; // target map number
   int      flags;  // execute flags
   int32_t *argv;   // argument vector
   uint32_t argc;   // argument count

   DLListItem<ACSDeferred> link;         // list links
   static DLListItem<ACSDeferred> *list; // deferred actions list

   static bool IsDeferredNumber(int32_t number, int mapnum);
   static bool IsDeferredName(const char *name, int mapnum);

public:
   static bool DeferExecuteNumber(int32_t number, int mapnum, int flags,
                                  const int32_t *argv, uint32_t argc);
   static bool DeferExecuteName(const char *name, int mapnum, int flags,
                                const int32_t *argv, uint32_t argc);

   static bool DeferSuspendNumber(int32_t number, int mapnum);
   static bool DeferSuspendName(const char *name, int mapnum);

   static bool DeferTerminateNumber(int32_t number, int mapnum);
   static bool DeferTerminateName(const char *name, int mapnum);

   static void ArchiveAll(SaveArchive &arc);
   static void ExecuteAll();
   static void ClearAll();
};

//
// ACSModule
//
// haleyjd 06/24/08: I am rewriting the interpreter to be modular in hopes of
// eventual support for ZDoomish features such as ACS libraries.
//
class ACSModule : public ZoneObject
{
public:
   ACSModule();
   ~ACSModule();

   // Used by the loader to sync the global strings to the VM's strings.
   void addStrings();

   ACSFunc *findFunction(const char *name);

   int32_t *findMapVar(const char *name);
   ACSArray *findMapArr(const char *name);

   // Converts a local index to a global index, or returns the original number.
   uint32_t getStringIndex(uint32_t strnum)
   {
      return strnum < numStrings ? strings[strnum] : strnum;
   }

   // Resets the VM to an uninitialized state. Only useful for static VMs.
   void reset();

   // bytecode info
   int32_t     *code;                  // ACS code; jumps are relative to this
   unsigned int numCode;
   uint32_t    *strings;               // indexes into global table
   unsigned int numStrings;
   ACSScript   *scripts;               // the scripts
   unsigned int numScripts;
   uint32_t    *scriptNames;           // script names
   unsigned int numScriptNames;
   bool         loaded;                // for static VMs, if it's valid or not
   uint32_t     id;                    // vm id number
   int          lump;                  // lump bytecode was loaded from

   // interpreter info
   ACSFunc **funcptrs;                 // pointers into vm funcdat
   ACSFunc  *funcs;                    // functions local to this vm
   uint32_t  numFuncs;                 // number of functions
   ACSJump  *jumps;                    // dynamic jump targets
   uint32_t  numJumps;                 // number of dynamic jump targets
   int32_t  *mapvtab[ACS_NUM_MAPVARS]; // pointers into vm mapvars
   int32_t   mapvars[ACS_NUM_MAPVARS]; // map variables local to this vm
   ACSArray *mapatab[ACS_NUM_MAPARRS]; // pointers into vm maparrs
   ACSArray  maparrs[ACS_NUM_MAPARRS]; // map arrays local to this vm

   // loader info
   uint32_t    *exports;                  // exported variables
   unsigned int numExports;               // number of exports
   const char **imports;                  // imported lump names
   ACSModule  **importVMs;                // imported VMs
   unsigned int numImports;               // number of imports
   uint32_t    *funcNames;                // function names
   unsigned int numFuncNames;             // number of function names
   const char  *mapvnam[ACS_NUM_MAPVARS]; // map variable names
   const char  *mapanam[ACS_NUM_MAPARRS]; // map array names
   uint32_t     mapalen[ACS_NUM_MAPARRS]; // map array lengths
   bool         mapahas[ACS_NUM_MAPARRS]; // if true, index is declared as array

   // global bytecode info
   static ACSString  **GlobalStrings;        // string table
   static unsigned int GlobalNumStrings;     // used count
   static unsigned int GlobalAllocStrings;   // allocated count
   static unsigned int GlobalNumStringsBase; // beyond lay DynaStrings
   static const char *GetString(uint32_t strnum)
   {
      return strnum < GlobalNumStrings ? GlobalStrings[strnum]->data.s : "";
   }
   static uint32_t GetStringLength(uint32_t strnum)
   {
      return strnum < GlobalNumStrings ? GlobalStrings[strnum]->length : 0;
   }
   static ACSString *GetStringData(uint32_t strnum)
   {
      return strnum < GlobalNumStrings ? GlobalStrings[strnum] : NULL;
   }

   static uint32_t AddString(const char *s) {return AddString(s, strlen(s));}
   static uint32_t AddString(const char *s, uint32_t l);

   static void ArchiveStrings(SaveArchive &arc);

   static ACSFunc    **GlobalFuncs;
   static unsigned int GlobalNumFuncs;
   static ACSFunc *FindFunction(uint32_t funcnum)
   {
      return funcnum < GlobalNumFuncs ? GlobalFuncs[funcnum] : NULL;
   }

   static ACSScript *FindScriptByNumber(int32_t scrnum);
   static ACSScript *FindScriptByName(const char *name);
   static ACSScript *FindScriptByString(uint32_t strnum)
   {
      return strnum < GlobalNumStrings ? GlobalStrings[strnum]->script : NULL;
   }
};


// Global function prototypes

// Environment control.
void ACS_Init();
void ACS_NewGame();
void ACS_InitLevel();
void ACS_LoadLevelScript(WadDirectory *dir, int lump);
void ACS_Exec();

// Script control.
bool ACS_ExecuteScriptI(uint32_t name, uint32_t mapnum, const uint32_t *argv,
                        uint32_t argc, Mobj *mo, line_t *line, int side);
bool ACS_ExecuteScriptIAlways(uint32_t name, uint32_t mapnum, const uint32_t *argv,
                              uint32_t argc, Mobj *mo, line_t *line, int side);
uint32_t ACS_ExecuteScriptIResult(uint32_t name, const uint32_t *argv,
                                 uint32_t argc, Mobj *mo, line_t *line, int side);
bool ACS_ExecuteScriptS(const char *name, uint32_t mapnum, const uint32_t *argv,
                        uint32_t argc, Mobj *mo, line_t *line, int side);
bool ACS_ExecuteScriptSAlways(const char *name, uint32_t mapnum, const uint32_t *argv,
                              uint32_t argc, Mobj *mo, line_t *line, int side);
uint32_t ACS_ExecuteScriptSResult(const char *name, const uint32_t *argv,
                                 uint32_t argc, Mobj *mo, line_t *line, int side);
bool ACS_SuspendScriptI(uint32_t name, uint32_t mapnum);
bool ACS_SuspendScriptS(const char *name, uint32_t mapnum);
bool ACS_TerminateScriptI(uint32_t name, uint32_t mapnum);
bool ACS_TerminateScriptS(const char *name, uint32_t mapnum);

// Utilities.
uint32_t ACS_GetLevelProp(uint32_t prop);
bool     ACS_ChkThingProp(Mobj *mo, uint32_t prop, uint32_t val);
uint32_t ACS_GetThingProp(Mobj *mo, uint32_t prop);
void     ACS_SetThingProp(Mobj *mo, uint32_t prop, uint32_t val);

// CallFuncs.
bool ACS_CF_ActivatorArmor(ACS_CF_ARGS);
bool ACS_CF_ActivatorFrags(ACS_CF_ARGS);
bool ACS_CF_ActivatorHealth(ACS_CF_ARGS);
bool ACS_CF_ActivatorSigil(ACS_CF_ARGS);
bool ACS_CF_ActivatorSound(ACS_CF_ARGS);
bool ACS_CF_ActivatorTID(ACS_CF_ARGS);
bool ACS_CF_AmbientSound(ACS_CF_ARGS);
bool ACS_CF_AmbientSoundLoc(ACS_CF_ARGS);
bool ACS_CF_ATan2(ACS_CF_ARGS);
bool ACS_CF_ChangeCeil(ACS_CF_ARGS);
bool ACS_CF_ChangeFloor(ACS_CF_ARGS);
bool ACS_CF_CheckSight(ACS_CF_ARGS);
bool ACS_CF_ChkThingCeilTex(ACS_CF_ARGS);
bool ACS_CF_ChkThingFlag(ACS_CF_ARGS);
bool ACS_CF_ChkThingFloorTex(ACS_CF_ARGS);
bool ACS_CF_ChkThingProp(ACS_CF_ARGS);
bool ACS_CF_ChkThingType(ACS_CF_ARGS);
bool ACS_CF_ClassifyThing(ACS_CF_ARGS);
bool ACS_CF_ClrLineSpec(ACS_CF_ARGS);
bool ACS_CF_Cos(ACS_CF_ARGS);
bool ACS_CF_EndLog(ACS_CF_ARGS);
bool ACS_CF_EndPrint(ACS_CF_ARGS);
bool ACS_CF_EndPrintBold(ACS_CF_ARGS);
bool ACS_CF_GameSkill(ACS_CF_ARGS);
bool ACS_CF_GameType(ACS_CF_ARGS);
bool ACS_CF_GetCVar(ACS_CF_ARGS);
bool ACS_CF_GetCVarStr(ACS_CF_ARGS);
bool ACS_CF_GetLevelProp(ACS_CF_ARGS);
bool ACS_CF_GetPlayerInput(ACS_CF_ARGS);
bool ACS_CF_GetPolyobjX(ACS_CF_ARGS);
bool ACS_CF_GetPolyobjY(ACS_CF_ARGS);
bool ACS_CF_GetScreenH(ACS_CF_ARGS);
bool ACS_CF_GetScreenW(ACS_CF_ARGS);
bool ACS_CF_GetSectorCeilZ(ACS_CF_ARGS);
bool ACS_CF_GetSectorFloorZ(ACS_CF_ARGS);
bool ACS_CF_GetSectorLight(ACS_CF_ARGS);
bool ACS_CF_GetThingAngle(ACS_CF_ARGS);
bool ACS_CF_GetThingCeilZ(ACS_CF_ARGS);
bool ACS_CF_GetThingFloorZ(ACS_CF_ARGS);
bool ACS_CF_GetThingLight(ACS_CF_ARGS);
bool ACS_CF_GetThingMomX(ACS_CF_ARGS);
bool ACS_CF_GetThingMomY(ACS_CF_ARGS);
bool ACS_CF_GetThingMomZ(ACS_CF_ARGS);
bool ACS_CF_GetThingPitch(ACS_CF_ARGS);
bool ACS_CF_GetThingProp(ACS_CF_ARGS);
bool ACS_CF_GetThingX(ACS_CF_ARGS);
bool ACS_CF_GetThingY(ACS_CF_ARGS);
bool ACS_CF_GetThingZ(ACS_CF_ARGS);
bool ACS_CF_Hypot(ACS_CF_ARGS);
bool ACS_CF_IsTIDUsed(ACS_CF_ARGS);
bool ACS_CF_LineOffsetY(ACS_CF_ARGS);
bool ACS_CF_LineSide(ACS_CF_ARGS);
bool ACS_CF_PlaySound(ACS_CF_ARGS);
bool ACS_CF_PlayThingSound(ACS_CF_ARGS);
bool ACS_CF_PlayerCount(ACS_CF_ARGS);
bool ACS_CF_PlayerNumber(ACS_CF_ARGS);
bool ACS_CF_PrintName(ACS_CF_ARGS);
bool ACS_CF_RadiusQuake(ACS_CF_ARGS);
bool ACS_CF_Random(ACS_CF_ARGS);
bool ACS_CF_ReplaceTex(ACS_CF_ARGS);
bool ACS_CF_SectorDamage(ACS_CF_ARGS);
bool ACS_CF_SectorSound(ACS_CF_ARGS);
bool ACS_CF_SetActivator(ACS_CF_ARGS);
bool ACS_CF_SetActivatorToTarget(ACS_CF_ARGS);
bool ACS_CF_SetGravity(ACS_CF_ARGS);
bool ACS_CF_SetLineBlock(ACS_CF_ARGS);
bool ACS_CF_SetLineBlockMon(ACS_CF_ARGS);
bool ACS_CF_SetLineSpec(ACS_CF_ARGS);
bool ACS_CF_SetLineTex(ACS_CF_ARGS);
bool ACS_CF_SetMusic(ACS_CF_ARGS);
bool ACS_CF_SetMusicLoc(ACS_CF_ARGS);
bool ACS_CF_SetSkyDelta(ACS_CF_ARGS);
bool ACS_CF_SetThingAngle(ACS_CF_ARGS);
bool ACS_CF_SetThingAngleRet(ACS_CF_ARGS);
bool ACS_CF_SetThingMom(ACS_CF_ARGS);
bool ACS_CF_SetThingPitch(ACS_CF_ARGS);
bool ACS_CF_SetThingPitchRet(ACS_CF_ARGS);
bool ACS_CF_SetThingPos(ACS_CF_ARGS);
bool ACS_CF_SetThingProp(ACS_CF_ARGS);
bool ACS_CF_SetThingSpec(ACS_CF_ARGS);
bool ACS_CF_SetThingState(ACS_CF_ARGS);
bool ACS_CF_Sin(ACS_CF_ARGS);
bool ACS_CF_SinglePlayer(ACS_CF_ARGS);
bool ACS_CF_SoundSeq(ACS_CF_ARGS);
bool ACS_CF_SpawnMissile(ACS_CF_ARGS);
bool ACS_CF_SpawnPoint(ACS_CF_ARGS);
bool ACS_CF_SpawnPointF(ACS_CF_ARGS);
bool ACS_CF_SpawnSpot(ACS_CF_ARGS);
bool ACS_CF_SpawnSpotF(ACS_CF_ARGS);
bool ACS_CF_SpawnSpotAng(ACS_CF_ARGS);
bool ACS_CF_SpawnSpotAngF(ACS_CF_ARGS);
bool ACS_CF_Sqrt(ACS_CF_ARGS);
bool ACS_CF_SqrtFixed(ACS_CF_ARGS);
bool ACS_CF_StopSound(ACS_CF_ARGS);
bool ACS_CF_ThingCount(ACS_CF_ARGS);
bool ACS_CF_ThingCountStr(ACS_CF_ARGS);
bool ACS_CF_ThingCountSec(ACS_CF_ARGS);
bool ACS_CF_ThingCountSecStr(ACS_CF_ARGS);
bool ACS_CF_ThingDamage(ACS_CF_ARGS);
bool ACS_CF_ThingMissile(ACS_CF_ARGS);
bool ACS_CF_ThingSound(ACS_CF_ARGS);
bool ACS_CF_ThingSoundSeq(ACS_CF_ARGS);
bool ACS_CF_Timer(ACS_CF_ARGS);
bool ACS_CF_UniqueTID(ACS_CF_ARGS);
bool ACS_CF_WaitPolyObj(ACS_CF_ARGS);
bool ACS_CF_WaitSector(ACS_CF_ARGS);

ACSModule *ACS_LoadScript(WadDirectory *dir, int lump);
void ACS_LoadScript(ACSModule *vm, WadDirectory *dir, int lump);
void ACS_LoadScriptACS0(ACSModule *vm, WadDirectory *dir, int lump, byte *data);
void ACS_LoadScriptACSE(ACSModule *vm, WadDirectory *dir, int lump, byte *data,
                        uint32_t tableOffset = 4);
void ACS_LoadScriptACSe(ACSModule *vm, WadDirectory *dir, int lump, byte *data,
                        uint32_t tableOffset = 4);
void ACS_LoadScriptCodeACS0(ACSModule *vm, byte *data, uint32_t lumpLength, bool compressed);
uint32_t ACS_LoadStringACS0(const byte *begin, const byte *end);
void ACS_RunDeferredScripts();
bool ACS_ExecuteScript(ACSScript *script, int flags, const int32_t *argv,
                       uint32_t argc, Mobj *trigger, line_t *line, int lineSide,
                       ACSThinker **thread = NULL);
bool ACS_ExecuteScriptNumber(int32_t number, int mapnum, int flags,
                             const int32_t *argv, uint32_t argc, Mobj *trigger,
                             line_t *line, int lineSide, ACSThinker **thread = NULL);
bool ACS_ExecuteScriptName(const char *name, int mapnum, int flags,
                           const int32_t *argv, uint32_t argc, Mobj *trigger,
                           line_t *line, int lineSide, ACSThinker **thread = NULL);
bool ACS_ExecuteScriptString(uint32_t strnum, int mapnum, int flags,
                             const int32_t *argv, uint32_t argc, Mobj *trigger,
                             line_t *line, int lineSide, ACSThinker **thread = NULL);
bool ACS_TerminateScript(ACSScript *script);
bool ACS_TerminateScriptNumber(int32_t number, int mapnum);
bool ACS_TerminateScriptName(const char *name, int mapnum);
bool ACS_TerminateScriptString(uint32_t strnum, int mapnum);
bool ACS_SuspendScript(ACSScript *script);
bool ACS_SuspendScriptNumber(int32_t number, int mapnum);
bool ACS_SuspendScriptName(const char *name, int mapnum);
bool ACS_SuspendScriptString(uint32_t strnum, int mapnum);
void ACS_Archive(SaveArchive &arc);

// extern vars.

extern ACSEnvironment ACSenv;

extern acs_func_t ACSfunc[ACS_FUNCMAX];
extern acs_opdata_t ACSopdata[ACS_OPMAX];

extern int ACS_thingtypes[ACS_NUM_THINGTYPES];

extern int32_t ACSworldvars[ACS_NUM_WORLDVARS];
extern ACSArray ACSworldarrs[ACS_NUM_WORLDARRS];
extern int32_t ACSglobalvars[ACS_NUM_GLOBALVARS];
extern ACSArray ACSglobalarrs[ACS_NUM_GLOBALARRS];

#endif

// EOF

