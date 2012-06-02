// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
// Copyright(C) 2012 David Hill
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
// Original 100% GPL ACS Interpreter
//
// By James Haley
//
// Improved by David Hill
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_small.h"
#include "acs_intr.h"
#include "c_io.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_things.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_info.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sndseq.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

//
// Local Enumerations
//

// script states for sreg
enum
{
   ACS_STATE_STOPPED,    // not running
   ACS_STATE_RUNNING,    // currently running
   ACS_STATE_SUSPEND,    // suspended by instruction
   ACS_STATE_WAITTAG,    // waiting on a tag
   ACS_STATE_WAITSCRIPT, // waiting on a script
   ACS_STATE_WAITPOLY,   // waiting on a polyobject
   ACS_STATE_TERMINATE,  // will be stopped on next thinking turn
};

//
// Static Variables
//

// deferred scripts
static DLListItem<deferredacs_t> *acsDeferred;

// haleyjd 06/24/08: level script vm for ACS
static ACSVM acsLevelScriptVM;

static PODCollection<ACSVM *> acsVMs;


//
// Global Variables
//

acs_opdata_t ACSopdata[ACS_OPMAX] =
{
   #define ACS_OP(OP,ARGC) {ACS_OP_##OP, ARGC},
   #include "acs_op.h"
   #undef ACS_OP
};


const char **ACSVM::GlobalStrings = NULL;
unsigned int ACSVM::GlobalNumStrings = 0;

// ACS_thingtypes:
// This array translates from ACS spawn numbers to internal thingtype indices.
// ACS spawn numbers are specified via EDF and are gamemode-dependent. EDF takes
// responsibility for populating this list.

int ACS_thingtypes[ACS_NUM_THINGTYPES];

// world variables
int32_t ACSworldvars[ACS_NUM_WORLDVARS];

// global variables
int32_t ACSglobalvars[ACS_NUM_GLOBALVARS];

//
// Static Functions
//

//
// ACS_addVirtualMachine
//
// haleyjd 06/24/08: keeps track of all ACS virtual machines.
//
static void ACS_addVirtualMachine(ACSVM *vm)
{
   vm->id = acsVMs.getLength();
   acsVMs.add(vm);
}

static bool ACS_addDeferredScriptVM(ACSVM *vm, int scrnum, int mapnum, 
                                    int type, int args[5]);

//
// ACS_addThread
//
// Adds a thinker as a thread on the given script.
//
static void ACS_addThread(ACSThinker *script, acscript_t *acscript)
{
   ACSThinker *next = acscript->threads;

   if((script->nextthread = next))
      next->prevthread = &script->nextthread;
   script->prevthread = &acscript->threads;
   acscript->threads = script;
}

//
// ACS_removeThread
//
// Removes a thinker from the acscript thread list.
//
static void ACS_removeThread(ACSThinker *script)
{
   ACSThinker **prev = script->prevthread;
   ACSThinker  *next = script->nextthread;
   
   if((*prev = next))
      next->prevthread = prev;
}

//
// ACS_scriptFinished
//
// Called when a script stops executing. Looks for other scripts in the
// same VM which are waiting on this one. All threads of the specified
// script must be terminated first.
//
static void ACS_scriptFinished(ACSThinker *script)
{
   unsigned int i;
   ACSVM      *vm       = script->vm;
   acscript_t *acscript = script->acscript;
   ACSThinker *th;

   // first check that all threads of the same script have terminated
   if(acscript->threads)
      return; // nope

   // find scripts waiting on this one (same VM only)
   
   // loop on vm scripts array
   for(i = 0; i < vm->numScripts; ++i)
   {
      // loop on threads list for each script
      for(th = vm->scripts[i].threads; th; th = th->nextthread)
      {
         if(th->sreg  == ACS_STATE_WAITSCRIPT &&
            th->sdata == acscript->number)
         {
            // wake up the script
            th->sreg  = ACS_STATE_RUNNING;
            th->sdata = 0;
         }
      }
   }
}

//
// ACS_stopScript
//
// Ultimately terminates the script and removes its thinker.
//
static void ACS_stopScript(ACSThinker *script, acscript_t *acscript)
{
   ACS_removeThread(script);
   
   // notify waiting scripts that this script has ended
   ACS_scriptFinished(script);

   P_SetTarget<Mobj>(&script->trigger, NULL);

   script->removeThinker();
}

//
// ACS_runOpenScript
//
// Starts an open script (a script indicated to start at the beginning of
// the level).
//
static void ACS_runOpenScript(ACSVM *vm, acscript_t *acs, int iNum, int vmID)
{
   ACSThinker *newScript = new ACSThinker;

   newScript->vmID        = vmID;
   newScript->scriptNum   = acs->number;
   newScript->internalNum = iNum;

   // open scripts wait one second before running
   newScript->delay = TICRATE;

   // set ip to entry point
   newScript->ip = acs->codePtr;

   // copy in some important data
   newScript->code        = acs->codePtr;
   newScript->data        = vm->code;
   newScript->printBuffer = vm->printBuffer;
   newScript->acscript    = acs;
   newScript->vm          = vm;

   // set up thinker
   newScript->addThinker();

   // mark as running
   newScript->sreg = ACS_STATE_RUNNING;
   ACS_addThread(newScript, acs);
}

//
// ACS_indexForNum
//
// Returns the index of the script in the "scripts" array given its number
// from within the script itself. Returns vm->numScripts if no such script
// exists.
//
// Currently uses a linear search, since the set is small and fixed-size.
//
unsigned int ACS_indexForNum(ACSVM *vm, int num)
{
   unsigned int idx;

   for(idx = 0; idx < vm->numScripts; ++idx)
      if(vm->scripts[idx].number == num)
         break;

   return idx;
}

//
// ACS_execLineSpec
//
// Executes a line special that has been encoded in the script with
// immediate operands or on the stack.
//
static void ACS_execLineSpec(line_t *l, Mobj *mo, int16_t spec, int side,
                             int argc, int *argv)
{
   int args[NUMLINEARGS] = { 0, 0, 0, 0, 0 };
   int i = argc;

   // args follow instruction in the code from first to last
   for(; i > 0; --i)
      args[argc-i] = *argv++;

   // translate line specials & args for Hexen maps
   P_ConvertHexenLineSpec(&spec, args);

   P_ExecParamLineSpec(l, mo, spec, args, side, SPAC_CROSS, true);
}

//
// ACS_countThings
//
// Returns a count of all things that match the supplied criteria.
// If type or tid are zero, that particular criterion is not applied.
//
static int ACS_countThings(int type, int tid)
{
   Thinker *th;
   int count = 0;

   // don't bother counting if no valid search is specified
   if(!(type || tid))
      return 0;
   
   // translate ACS thing type ids to true types
   if(type < 0 || type >= ACS_NUM_THINGTYPES)
      return 0;

   type = ACS_thingtypes[type];
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;
      if((mo = thinker_cast<Mobj *>(th)))
      {
         if((type == 0 || mo->type == type) && (tid == 0 || mo->tid == tid))
         {
            // don't count killable things that are dead
            if(((mo->flags & MF_COUNTKILL) || (mo->flags3 & MF3_KILLABLE)) &&
               mo->health <= 0)
               continue;
            ++count;
         }
      }
   }

   return count;
}

//
// ACS_countPlayers
//
// Returns a count of active players.
//
static int ACS_countPlayers(void)
{
   int i, count = 0;

   for(i = 0; i < MAXPLAYERS; ++i)
      if(playeringame[i])
         ++count;

   return count;
}

//
// ACS_getThingVar
//
static int32_t ACS_getThingVar(Mobj *thing, uint32_t var)
{
   if(!thing) return 0;

   switch(var)
   {
   case ACS_THINGVAR_X: return thing->x;
   case ACS_THINGVAR_Y: return thing->y;
   case ACS_THINGVAR_Z: return thing->z;
   default: return 0;
   }
}

// ZDoom blocking types
enum
{
   BLOCK_NOTHING,
   BLOCK_CREATURES,
   BLOCK_EVERYTHING,
   BLOCK_RAILING,
   BLOCK_PLAYERS,
   BLOCK_MONSTERS_OFF,
   BLOCK_MONSTERS_ON,
};

//
// ACS_setLineBlocking
//
// Toggles the blocking flag on all tagged lines.
//
static void ACS_setLineBlocking(int tag, int block)
{
   line_t *l;
   int linenum = -1;

   while((l = P_FindLine(tag, &linenum)) != NULL)
   {
      switch(block)
      {
      case BLOCK_NOTHING:
         // clear the flags
         l->flags    &= ~ML_BLOCKING;
         l->extflags &= ~EX_ML_BLOCKALL;
         break;
      case BLOCK_CREATURES:
         l->extflags &= ~EX_ML_BLOCKALL;
         l->flags |= ML_BLOCKING;
         break;
      case BLOCK_EVERYTHING: // ZDoom extension - block everything
         l->flags    |= ML_BLOCKING;
         l->extflags |= EX_ML_BLOCKALL;
         break;
      case BLOCK_MONSTERS_OFF:
         l->flags &= ~ML_BLOCKMONSTERS;
         break;
      case BLOCK_MONSTERS_ON:
         l->flags |= ML_BLOCKMONSTERS;
         break;
      default: // Others not implemented yet :P
         break;
      }
   }
}

//
// ACS_setLineSpecial
//
// Sets all tagged lines' complete parameterized specials.
//
static void ACS_setLineSpecial(int16_t spec, int *args, int tag)
{
   line_t *l;
   int linenum = -1;

   // do special/args translation for Hexen maps
   P_ConvertHexenLineSpec(&spec, args);

   while((l = P_FindLine(tag, &linenum)) != NULL)
   {
      l->special = spec;
      memcpy(l->args, args, 5 * sizeof(int));
   }
}

//
// ACS_setThingSpecial
//
// Sets all tagged things' complete parameterized specials.
//
static void ACS_setThingSpecial(int16_t spec, int *args, int tid)
{
   Mobj *mo = NULL;

   // do special/args translation for Hexen maps
   P_ConvertHexenLineSpec(&spec, args);

   while((mo = P_FindMobjFromTID(tid, mo, NULL)))
   {
      //mo->special = spec;
      memcpy(mo->args, args, 5 * sizeof(int));
   }
}

//
// ACS_spawn
//
static bool ACS_spawn(mobjtype_t type, fixed_t x, fixed_t y, fixed_t z,
                      int tid, angle_t angle)
{
   Mobj *mo;
   if(type != -1 && (mo = P_SpawnMobj(x, y, z, type)))
   {
      if(tid) P_AddThingTID(mo, tid);
      mo->angle = angle;
      return true;
   }
   else
      return false;
}

//
// ACS_spawnMobj
//
static void ACS_spawnMobj(const int32_t *args, int32_t *&retn)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   fixed_t x     = args[1];
   fixed_t y     = args[2];
   fixed_t z     = args[3];
   int     tid   = args[4];
   angle_t angle = args[5] << 24;

   *retn++ = ACS_spawn(type, x, y, z, tid, angle);
}

//
// ACS_spawnSpot
//
static void ACS_spawnSpot(const int32_t *args, int32_t *&retn)
{
   mobjtype_t type = E_ThingNumForName(ACSVM::GetString(args[0]));
   int     spotid = args[1];
   int     tid    = args[2];
   angle_t angle  = args[3] << 24;
   Mobj   *spot   = NULL;

   *retn = 0;

   while((spot = P_FindMobjFromTID(spotid, spot, NULL)))
      *retn += ACS_spawn(type, spot->x, spot->y, spot->z, tid, angle);

   ++retn;
}


//
// Global Functions
//


// Interpreter Macros

// This sure would be nice to enable! (But maybe a better check?)
#if defined(__GNUC__)
#define COMPGOTO
#endif

#define PUSH(x)   (*stp++ = (x))
#define POP()     (*--stp)
#define PEEK()    (*(stp-1))
#define DECSTP()  (--stp)
#define INCSTP()  (++stp)

#define IPCURR()  (*ip)
#define IPNEXT()  (*ip++)

#define STACK_AT(x) (*(stp-(x)))

// for binary operations: ++ and -- mess these up
#define ST_BINOP(OP) temp = POP(); STACK_AT(1) = (STACK_AT(1) OP temp)
#define ST_BINOP_EQ(OP) temp = POP(); STACK_AT(1) OP temp

#define DIV_CHECK(VAL) \
   if(!(VAL)) \
   { \
      doom_printf(FC_ERROR "ACS Error: divide by zero\a"); \
      goto action_endscript; \
   } \


#define DIVOP_EQ(VAR, OP) DIV_CHECK(temp = POP()); (VAR) OP temp

#define BRANCH_COUNT() \
   if(++count > 500000) \
   { \
      doom_printf(FC_ERROR "ACS Error: terminated runaway script\a"); \
      goto action_endscript; \
   }

// Uses do...while for convenience of use.
#define BRANCHOP(TARGET) \
   do \
   { \
      ip = this->data + (TARGET); \
      BRANCH_COUNT(); \
   } \
   while(0)

#ifdef COMPGOTO
#define OPCODE(OP) acs_op_##OP
#define NEXTOP() goto *ops[IPNEXT()]
#else
#define OPCODE(OP) case ACS_OP_##OP
#define NEXTOP() break
#endif

IMPLEMENT_THINKER_TYPE(ACSThinker)

//
// T_ACSThinker
//
// Function for acs thinkers. Runs the script by interpreting its bytecode
// until the script terminates, is suspended, or waits on some condition.
//
void ACSThinker::Think()
{
#ifdef COMPGOTO
   static const void *const ops[ACS_OPMAX] =
   {
      #define ACS_OP(OP,ARGC) &&acs_op_##OP,
      #include "acs_op.h"
      #undef ACS_OP
   };
#endif

   // cache vm data in local vars for efficiency
   register int32_t *ip  = this->ip;
   register int32_t *stp = this->stack + this->stp;
   int count = 0;
   int32_t opcode, temp;

   // should the script terminate?
   if(this->sreg == ACS_STATE_TERMINATE)
      ACS_stopScript(this, this->acscript);

   // is the script running?
   if(this->sreg != ACS_STATE_RUNNING)
      return;

   // check for delays
   if(this->delay)
   {
      --this->delay;
      return;
   }

   // run opcodes until a terminating instruction is reached
#ifdef COMPGOTO
   NEXTOP();
#else
   for(;;) switch(IPNEXT())
#endif
   {
   OPCODE(NOP):
      NEXTOP();

   OPCODE(KILL):
      doom_printf(FC_ERROR "ACS Error: KILL at %d\a", (int)(ip - data - 1));
      goto action_endscript;

      // Special Commands
   OPCODE(LINESPEC):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      stp -= temp; // consume args
      ACS_execLineSpec(this->line, this->trigger, (int16_t)opcode,
                       this->lineSide, temp, stp);
      NEXTOP();
   OPCODE(LINESPEC_IMM):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      ACS_execLineSpec(this->line, this->trigger, (int16_t)opcode,
                       this->lineSide, temp, ip);
      ip += temp; // consume args
      NEXTOP();

      // SET
   OPCODE(SET_LOCALVAR):
      this->locals[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_MAPVAR):
      *vm->mapvtab[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_WORLDVAR):
      ACSworldvars[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_GLOBALVAR):
      ACSglobalvars[IPNEXT()] = POP();
      NEXTOP();

      // GET
   OPCODE(GET_IMM):
      PUSH(IPNEXT());
      NEXTOP();
   OPCODE(GET_LOCALVAR):
      PUSH(this->locals[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_MAPVAR):
      PUSH(*vm->mapvtab[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_WORLDVAR):
      PUSH(ACSworldvars[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_GLOBALVAR):
      PUSH(ACSglobalvars[IPNEXT()]);
      NEXTOP();

   OPCODE(GET_THINGVAR):
      {
         Mobj *mo = P_FindMobjFromTID(STACK_AT(1), NULL, trigger);
         STACK_AT(1) = ACS_getThingVar(mo, IPNEXT());
      }
      NEXTOP();

      // GETARR
   OPCODE(GETARR_IMM):
      for(temp = IPNEXT(); temp--;) PUSH(IPNEXT());
      NEXTOP();

      // Binary Ops
      // ADD
   OPCODE(ADD_STACK):
      ST_BINOP_EQ(+=);
      NEXTOP();
   OPCODE(ADD_LOCALVAR):
      this->locals[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_MAPVAR):
      *vm->mapvtab[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_WORLDVAR):
      ACSworldvars[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_GLOBALVAR):
      ACSglobalvars[IPNEXT()] += POP();
      NEXTOP();

      // AND
   OPCODE(AND_STACK):
      ST_BINOP_EQ(&=);
      NEXTOP();

      // CMP
   OPCODE(CMP_EQ):
      ST_BINOP(==);
      NEXTOP();
   OPCODE(CMP_NE):
      ST_BINOP(!=);
      NEXTOP();
   OPCODE(CMP_LT):
      ST_BINOP(<);
      NEXTOP();
   OPCODE(CMP_GT):
      ST_BINOP(>);
      NEXTOP();
   OPCODE(CMP_LE):
      ST_BINOP(<=);
      NEXTOP();
   OPCODE(CMP_GE):
      ST_BINOP(>=);
      NEXTOP();

      // DEC
   OPCODE(DEC_LOCALVAR):
      --this->locals[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_MAPVAR):
      --*vm->mapvtab[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_WORLDVAR):
      --ACSworldvars[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_GLOBALVAR):
      --ACSglobalvars[IPNEXT()];
      NEXTOP();

      // DIV
   OPCODE(DIV_STACK):
      DIVOP_EQ(STACK_AT(1), /=);
      NEXTOP();
   OPCODE(DIV_LOCALVAR):
      DIVOP_EQ(this->locals[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_MAPVAR):
      DIVOP_EQ(*vm->mapvtab[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_WORLDVAR):
      DIVOP_EQ(ACSworldvars[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_GLOBALVAR):
      DIVOP_EQ(ACSglobalvars[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIVX_STACK):
      DIV_CHECK(temp = POP()); STACK_AT(1) = FixedDiv(STACK_AT(1), temp);
      NEXTOP();

      // INC
   OPCODE(INC_LOCALVAR):
      ++this->locals[IPNEXT()];
      NEXTOP();
   OPCODE(INC_MAPVAR):
      ++*vm->mapvtab[IPNEXT()];
      NEXTOP();
   OPCODE(INC_WORLDVAR):
      ++ACSworldvars[IPNEXT()];
      NEXTOP();
   OPCODE(INC_GLOBALVAR):
      ++ACSglobalvars[IPNEXT()];
      NEXTOP();

      // IOR
   OPCODE(IOR_STACK):
      ST_BINOP_EQ(|=);
      NEXTOP();

      // LSH
   OPCODE(LSH_STACK):
      ST_BINOP_EQ(<<=);
      NEXTOP();

      // MOD
   OPCODE(MOD_STACK):
      DIVOP_EQ(STACK_AT(1), %=);
      NEXTOP();
   OPCODE(MOD_LOCALVAR):
      DIVOP_EQ(this->locals[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_MAPVAR):
      DIVOP_EQ(*vm->mapvtab[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_WORLDVAR):
      DIVOP_EQ(ACSworldvars[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_GLOBALVAR):
      DIVOP_EQ(ACSglobalvars[IPNEXT()], %=);
      NEXTOP();

      // MUL
   OPCODE(MUL_STACK):
      ST_BINOP_EQ(*=);
      NEXTOP();
   OPCODE(MUL_LOCALVAR):
      this->locals[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_MAPVAR):
      *vm->mapvtab[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_WORLDVAR):
      ACSworldvars[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_GLOBALVAR):
      ACSglobalvars[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MULX_STACK):
      temp = POP(); STACK_AT(1) = FixedMul(STACK_AT(1), temp);
      NEXTOP();

      // RSH
   OPCODE(RSH_STACK):
      ST_BINOP_EQ(>>=);
      NEXTOP();

      // SUB
   OPCODE(SUB_STACK):
      ST_BINOP_EQ(-=);
      NEXTOP();
   OPCODE(SUB_LOCALVAR):
      this->locals[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_MAPVAR):
      *vm->mapvtab[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_WORLDVAR):
      ACSworldvars[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_GLOBALVAR):
      ACSglobalvars[IPNEXT()] -= POP();
      NEXTOP();

      // XOR
   OPCODE(XOR_STACK):
      ST_BINOP_EQ(^=);
      NEXTOP();

      // Unary Ops
   OPCODE(NEGATE_STACK):
      STACK_AT(1) = -STACK_AT(1);
      NEXTOP();

      // Logical Ops
   OPCODE(LOGAND_STACK):
      ST_BINOP(&&);
      NEXTOP();
   OPCODE(LOGIOR_STACK):
      ST_BINOP(||);
      NEXTOP();
   OPCODE(LOGNOT_STACK):
      STACK_AT(1) = !STACK_AT(1);
      NEXTOP();

      // Trigonometry Ops
   OPCODE(TRIG_COS):
      STACK_AT(1) = finecosine[(angle_t)(STACK_AT(1) << FRACBITS) >> ANGLETOFINESHIFT];
      NEXTOP();
   OPCODE(TRIG_SIN):
      STACK_AT(1) = finesine[(angle_t)(STACK_AT(1) << FRACBITS) >> ANGLETOFINESHIFT];
      NEXTOP();
   OPCODE(TRIG_VECTORANGLE):
      DECSTP();
      STACK_AT(1) = P_PointToAngle(0, 0, STACK_AT(1), STACK_AT(0));
      NEXTOP();

      // Branching
   OPCODE(BRANCH_CASE):
      if(PEEK() == IPNEXT()) // compare top of stack against op+1
      {
         DECSTP(); // take the value off the stack
         BRANCHOP(IPCURR()); // jump to op+2
      }
      else
         ++ip; // increment past offset at op+2, leave value on stack
      NEXTOP();
   OPCODE(BRANCH_IMM):
      BRANCHOP(IPCURR());
      NEXTOP();
   OPCODE(BRANCH_NOTZERO):
      if(POP())
         BRANCHOP(IPCURR());
      else
         ++ip;
      NEXTOP();
   OPCODE(BRANCH_ZERO):
      if(!POP())
         BRANCHOP(IPCURR());
      else
         ++ip;
      NEXTOP();

      // Stack Control
   OPCODE(STACK_COPY):
      STACK_AT(0) = STACK_AT(1);
      INCSTP();
      NEXTOP();
   OPCODE(STACK_DROP):
      DECSTP();
      NEXTOP();
   OPCODE(STACK_SWAP):
      temp = STACK_AT(1);
      STACK_AT(1) = STACK_AT(2);
      STACK_AT(2) = temp;
      NEXTOP();

      // Script Control
   OPCODE(SCRIPT_RESTART):
      ip = this->code;
      BRANCH_COUNT();
      NEXTOP();
   OPCODE(SCRIPT_SUSPEND):
      this->sreg = ACS_STATE_SUSPEND;
      goto action_stop;
   OPCODE(SCRIPT_TERMINATE):
      goto action_endscript;

      // Printing
   OPCODE(STARTPRINT):
      this->printBuffer->clear();
      NEXTOP();
   OPCODE(ENDPRINT):
      if(this->trigger && this->trigger->player)
         player_printf(this->trigger->player, this->printBuffer->constPtr());
      else
         player_printf(&players[consoleplayer], this->printBuffer->constPtr());
      NEXTOP();
   OPCODE(ENDPRINTBOLD):
      HU_CenterMsgTimedColor(this->printBuffer->constPtr(), FC_GOLD, 20*35);
      NEXTOP();
   OPCODE(PRINTCHAR):
      *this->printBuffer += (char)POP();
      NEXTOP();
   OPCODE(PRINTFIXED):
      {
         // %E worst case: -1.52587e-05 == 12 + NUL
         // %F worst case: -0.000106811 == 12 + NUL
         // %F worst case: -32768.9     ==  8 + NUL
         // %G is probably maximally P+6+1.
         char buffer[13];
         sprintf(buffer, "%G", M_FixedToDouble(POP()));
         this->printBuffer->concat(buffer);
      }
      NEXTOP();
   OPCODE(PRINTINT):
      {
         // %i worst case: -2147483648 == 11 + NUL
         char buffer[12];
         this->printBuffer->concat(M_Itoa(POP(), buffer, 10));
      }
      NEXTOP();
   OPCODE(PRINTNAME):
      temp = POP();
      switch(temp)
      {
      case 0:
         this->printBuffer->concat(players[consoleplayer].name);
         break;

      default:
         if(temp > 0 && temp <= MAXPLAYERS)
            this->printBuffer->concat(players[temp - 1].name);
         break;
      }
      NEXTOP();
   OPCODE(PRINTSTRING):
      this->printBuffer->concat(ACSVM::GetString(POP()));
      NEXTOP();

      // Miscellaneous

   OPCODE(ACTIVATORARMOR):
      PUSH(trigger && trigger->player ? trigger->player->armorpoints : 0);
      NEXTOP();
   OPCODE(ACTIVATORFRAGS):
      PUSH(trigger && trigger->player ? trigger->player->totalfrags : 0);
      NEXTOP();
   OPCODE(ACTIVATORHEALTH):
      PUSH(trigger ? trigger->health : 0);
      NEXTOP();
   OPCODE(ACTIVATORSOUND):
      // If trigger is null, turn into ambient sound as in ZDoom.
      stp -= 2; //                                       sound    vol
      S_StartSoundNameAtVolume(trigger, ACSVM::GetString(stp[0]), stp[1],
                               ATTN_NORMAL, CHAN_AUTO);
      NEXTOP();

   OPCODE(AMBIENTSOUND):
      stp -= 2; //                                    sound    vol
      S_StartSoundNameAtVolume(NULL, ACSVM::GetString(stp[0]), stp[1],
                               ATTN_NORMAL, CHAN_AUTO);
      NEXTOP();
   OPCODE(AMBIENTSOUNDLOCAL):
      stp -= 2;
      if(trigger == players[displayplayer].mo) //        sound    vol
         S_StartSoundNameAtVolume(NULL, ACSVM::GetString(stp[0]), stp[1],
                                  ATTN_NORMAL, CHAN_AUTO);
      NEXTOP();

   OPCODE(SETGRAVITY):
      LevelInfo.gravity = POP() / 800;
      NEXTOP();
   OPCODE(SETGRAVITY_IMM):
      LevelInfo.gravity = IPNEXT() / 800;
      NEXTOP();

   OPCODE(SETLINEBLOCKING):
      stp -= 2;
      ACS_setLineBlocking(stp[0], stp[1]);
      NEXTOP();
   OPCODE(SETLINEMONSTERBLOCKING):
      stp -= 2;
      ACS_setLineBlocking(stp[0], BLOCK_MONSTERS_OFF + !!stp[1]);
      NEXTOP();
   OPCODE(SETLINESPECIAL):
      stp -= 7; //       tag     args   special
      ACS_setLineSpecial(stp[1], stp+2, stp[0]);
      NEXTOP();
   OPCODE(SETLINETEXTURE):
      stp -= 4; //                     texture  pos     side    tag
      P_ChangeLineTex(ACSVM::GetString(stp[3]), stp[2], stp[1], stp[0], false);
      NEXTOP();

   OPCODE(SETMUSIC):
      stp -= 3;
      S_ChangeMusicName(ACSVM::GetString(stp[0]), 1);
      NEXTOP();
   OPCODE(SETMUSIC_IMM):
      S_ChangeMusicName(ACSVM::GetString(ip[0]), 1);
      ip += 3;
      NEXTOP();
  OPCODE(SETMUSICLOCAL):
      stp -= 3;
      if(trigger == players[consoleplayer].mo)
         S_ChangeMusicName(ACSVM::GetString(stp[0]), 1);
      NEXTOP();
   OPCODE(SETMUSICLOCAL_IMM):
      if(trigger == players[consoleplayer].mo)
         S_ChangeMusicName(ACSVM::GetString(ip[0]), 1);
      ip += 3;
      NEXTOP();

   OPCODE(SETTHINGSPECIAL):
      stp -= 7; //       tag     args   special
      ACS_setThingSpecial(stp[1], stp+2, stp[0]);
      NEXTOP();

   OPCODE(SPAWN):
      stp -= 6;
      ACS_spawnMobj(stp, stp);
      NEXTOP();
   OPCODE(SPAWN_IMM):
      ACS_spawnMobj(ip, stp);
      ip += 6;
      NEXTOP();

   OPCODE(SPAWNSPOT):
      stp -= 4;
      ACS_spawnSpot(stp, stp);
      NEXTOP();
   OPCODE(SPAWNSPOT_IMM):
      ACS_spawnSpot(ip, stp);
      ip += 4;
      NEXTOP();

   OPCODE(TAGSTRING):
      if((uint32_t)PEEK() < vm->strings) PEEK() += vm->strings;
      NEXTOP();

   OPCODE(DELAY):
      this->delay = POP();
      goto action_stop;
   OPCODE(DELAY_IMM):
      this->delay = IPNEXT();
      goto action_stop;

   OPCODE(RANDOM):
      {
         int min, max;
         max = POP();
         min = POP();

         PUSH(P_RangeRandom(pr_script, min, max));
      }
      NEXTOP();
   OPCODE(RANDOM_IMM):
      {
         int min, max;
         min = IPNEXT();
         max = IPNEXT();

         PUSH(P_RangeRandom(pr_script, min, max));
      }
      NEXTOP();

   OPCODE(THINGCOUNT):
      temp = POP(); // get tid
      temp = ACS_countThings(POP(), temp);
      PUSH(temp);
      NEXTOP();
   OPCODE(THINGCOUNT_IMM):
      temp = IPNEXT(); // get type
      temp = ACS_countThings(temp, IPNEXT());
      PUSH(temp);
      NEXTOP();

   OPCODE(TAGWAIT):
      this->sreg  = ACS_STATE_WAITTAG;
      this->sdata = POP(); // get sector tag
      goto action_stop;
   OPCODE(TAGWAIT_IMM):
      this->sreg  = ACS_STATE_WAITTAG;
      this->sdata = IPNEXT(); // get sector tag
      goto action_stop;

   OPCODE(POLYWAIT):
      this->sreg  = ACS_STATE_WAITPOLY;
      this->sdata = POP(); // get poly tag
      goto action_stop;
   OPCODE(POLYWAIT_IMM):
      this->sreg  = ACS_STATE_WAITPOLY;
      this->sdata = IPNEXT(); // get poly tag
      goto action_stop;

   OPCODE(CHANGEFLOOR):
      temp = POP(); // get flat string index
      P_ChangeFloorTex(ACSVM::GetString(temp), POP()); // get tag
      NEXTOP();
   OPCODE(CHANGEFLOOR_IMM):
      temp = *ip++; // get tag
      P_ChangeFloorTex(ACSVM::GetString(IPNEXT()), temp);
      NEXTOP();

   OPCODE(CHANGECEILING):
      temp = POP(); // get flat string index
      P_ChangeCeilingTex(ACSVM::GetString(temp), POP()); // get tag
      NEXTOP();
   OPCODE(CHANGECEILING_IMM):
      temp = IPNEXT(); // get tag
      P_ChangeCeilingTex(ACSVM::GetString(IPNEXT()), temp);
      NEXTOP();

   OPCODE(LINESIDE):
      PUSH(this->lineSide);
      NEXTOP();

   OPCODE(SCRIPTWAIT):
      this->sreg  = ACS_STATE_WAITSCRIPT;
      this->sdata = POP(); // get script num
      goto action_stop;
   OPCODE(SCRIPTWAIT_IMM):
      this->sreg  = ACS_STATE_WAITSCRIPT;
      this->sdata = IPNEXT(); // get script num
      goto action_stop;

   OPCODE(CLEARLINESPECIAL):
      if(this->line)
         this->line->special = 0;
      NEXTOP();

   OPCODE(PLAYERCOUNT):
      PUSH(ACS_countPlayers());
      NEXTOP();

   OPCODE(GAMETYPE):
      PUSH(GameType);
      NEXTOP();

   OPCODE(GAMESKILL):
      PUSH(gameskill);
      NEXTOP();

   OPCODE(TIMER):
      PUSH(leveltime);
      NEXTOP();

   OPCODE(SECTORSOUND):
      {
         PointThinker *src;
         int vol    = POP();
         int strnum = POP();

         // if script started from a line, use the frontsector's sound origin
         if(this->line)
            src = &(this->line->frontsector->soundorg);
         else
            src = NULL;

         S_StartSoundNameAtVolume(src, ACSVM::GetString(strnum), vol,
                                  ATTN_NORMAL, CHAN_AUTO);
      }
      NEXTOP();

   OPCODE(SOUNDSEQUENCE):
      {
         sector_t *sec;
         int strnum = POP();

         if(this->line && (sec = this->line->frontsector))
         {
            S_StartSectorSequenceName(sec, ACSVM::GetString(strnum),
                                      SEQ_ORIGIN_SECTOR_F);
         }
         /*
         FIXME
         else
         {
            S_StartSequenceName(NULL, ACSVM::GetString(strnum),
                                SEQ_ORIGIN_OTHER, -1);
         }
         */
      }
      NEXTOP();

   OPCODE(THINGSOUND):
      {
         int vol    = POP();
         int strnum = POP();
         int tid    = POP();
         Mobj *mo   = NULL;

         while((mo = P_FindMobjFromTID(tid, mo, NULL)))
         {
            S_StartSoundNameAtVolume(mo, ACSVM::GetString(strnum), vol,
                                     ATTN_NORMAL, CHAN_AUTO);
         }
      }
      NEXTOP();

#ifndef COMPGOTO
   default:
      // unknown opcode, must stop execution
      doom_printf(FC_ERROR "ACS Error: unknown opcode %d\a", opcode);
      goto action_endscript;
#endif
   }

action_endscript:
   // end the script
   ACS_stopScript(this, this->acscript);
   return;

action_stop:
   // copy fields back into script
   this->ip  = ip;
   this->stp = stp - this->stack;
   return;
}

//
// ACSThinker::serialize
//
// Saves/loads a ACSThinker.
//
void ACSThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   // Basic properties
   arc << vmID << scriptNum << internalNum << stp << sreg << sdata 
       << delay << lineSide;

   // Arrays
   P_ArchiveArray<int>(arc, stack,  ACS_STACK_LEN);
   P_ArchiveArray<int>(arc, locals, ACS_NUM_LOCALVARS);

   // Pointers
   if(arc.isSaving())
   {
      unsigned int tempIp, tempLine, tempTrigger;

      // Save instruction pointer, line pointer, and trigger
      tempIp      = ip - code;
      tempLine    = line ? line - lines + 1 : 0;
      tempTrigger = P_NumForThinker(trigger);
      
      arc << tempIp << tempLine << tempTrigger;
   }
   else
   {
      unsigned int ipnum, linenum;
      
      // Restore line pointer; IP is restored in ACS_RestartSavedScript,
      // and trigger must be restored in deswizzle() after all thinkers
      // have been spawned.
      arc << ipnum << linenum << triggerSwizzle;

      line = linenum ? &lines[linenum - 1] : NULL;

      // This will restore all the various data structure pointers such as
      // prev/next thread, stringtable, code/data, printBuffer, vm, etc.
      ACS_RestartSavedScript(this, ipnum);
   }
}

//
// ACSThinker::deSwizzle
//
// Fixes up the trigger reference in a ACSThinker.
//
void ACSThinker::deSwizzle()
{
   Mobj *mo = thinker_cast<Mobj *>(P_ThinkerForNum(triggerSwizzle));
   P_SetNewTarget(&trigger, mo);
   triggerSwizzle = 0;
}

//
// ACSVM::ACSVM
//
ACSVM::ACSVM(int tag) : ZoneObject()
{
   printBuffer = new qstring(qstring::basesize);
   reset();
}

//
// ACSVM::~ACSVM
//
ACSVM::~ACSVM()
{
   delete printBuffer;
}

//
// ACSVM::reset
//
void ACSVM::reset()
{
   for(int i = ACS_NUM_MAPVARS; i--;)
      mapvtab[i] = &mapvars[i];

   memset(mapvars, 0, sizeof(mapvars));

   numCode    = 0;
   numStrings = 0;
   numScripts = 0;
   loaded     = false;
   lump       = -1;
}

//
// ACS_Init
//
// Called at startup.
//
void ACS_Init(void)
{
}

//
// ACS_NewGame
//
// Called when a new game is started.
//
void ACS_NewGame(void)
{
   DLListItem<deferredacs_t> *cur, *next;

   // clear out the world variables
   memset(ACSworldvars, 0, sizeof(ACSworldvars));

   // clear out the global variables
   memset(ACSglobalvars, 0, sizeof(ACSglobalvars));

   // clear out deferred scripts
   cur = acsDeferred;

   while(cur)
   {
      next = cur->dllNext;
      cur->remove();      
      efree(cur->dllObject);
      cur = next;
   }

   acsDeferred = NULL;
}

//
// ACS_InitLevel
//
// Called at level start from P_SetupLevel
//
void ACS_InitLevel(void)
{
   acsLevelScriptVM.reset();

   ACSVM::GlobalStrings = NULL;
   ACSVM::GlobalNumStrings = 0;
}

//
// ACS_LoadScript
//
void ACS_LoadScript(ACSVM *vm, WadDirectory *dir, int lump)
{
   byte *data;

   ACS_addVirtualMachine(vm);
   vm->strings = ACSVM::GlobalNumStrings;
   vm->lump    = lump;

   // zero length or too-short lump?
   if(dir->lumpLength(lump) < 4)
   {
      vm->code = NULL;
      return;
   }

   // load the lump
   data = (byte *)(dir->cacheLumpNum(lump, PU_LEVEL));

   switch(SwapLong(*(int32_t *)data))
   {
   case 0x00534341: // TODO: macro
      ACS_LoadScriptACS0(vm, dir, lump, data);
      break;
   }

   // haleyjd 06/30/09: open scripts must be started *here*, not above.
   for(acscript_t *end = vm->scripts+vm->numScripts,
       *itr = vm->scripts; itr != end; ++itr)
   {
      if(itr->type)
         ACS_runOpenScript(vm, itr, itr - vm->scripts, vm->id);
   }
}

//
// ACS_loadScripts
//
// Reads lump names out of the lump and loads them as scripts.
//
static void ACS_loadScripts(WadDirectory *dir, int lump)
{
   ACSVM *vm;
   const char *itr, *end;
   char lumpname[9], *nameItr, *const nameEnd = lumpname+8;

   itr = (const char *)(dir->cacheLumpNum(lump, PU_CACHE));
   end = itr + dir->lumpLength(lump);

   for(;;)
   {
      // Discard any whitespace.
      while(itr != end && isspace(*itr)) ++itr;

      if(itr == end) break;

      // Read a name.
      nameItr = lumpname;
      while(itr != end && nameItr != nameEnd && !isspace(*itr))
         *nameItr++ = *itr++;
      *nameItr = '\0';

      // Discard excess letters.
      while(itr != end && !isspace(*itr)) ++itr;

      if((lump = dir->checkNumForName(lumpname, lumpinfo_t::ns_acs)) == -1)
         continue;

      // If the lump has already been loaded, don't reload it.
      for(ACSVM **itr = acsVMs.begin(), **end = acsVMs.end(); itr != end; ++itr)
      {
         if ((*itr)->lump == lump)
            goto vm_loaded;
      }

      vm = new ACSVM(PU_LEVEL);

      ACS_LoadScript(vm, dir, lump);

   vm_loaded:;
   }
}

//
// ACS_LoadLevelScript
//
// Loads the level scripts and initializes the levelscript virtual machine.
// Called from P_SetupLevel.
//
void ACS_LoadLevelScript(WadDirectory *dir, int lump)
{
   acsVMs.makeEmpty();

   // load the level script, if any
   if(lump != -1)
      ACS_LoadScript(&acsLevelScriptVM, dir, lump);

   // The rest of the function is LOADACS handling.

   lumpinfo_t **lumpinfo = dir->getLumpInfo();

   lump = dir->getLumpNameChain("LOADACS")->index;
   while(lump != -1)
   {
      if(!strncasecmp(lumpinfo[lump]->name, "LOADACS", 7) &&
         lumpinfo[lump]->li_namespace == lumpinfo_t::ns_global)
      {
         ACS_loadScripts(dir, lump);
      }

      lump = lumpinfo[lump]->next;
   }

}

//
// ACS_addDeferredScriptVM
//
// Adds a deferred script that will be executed when the indicated
// gamemap is reached. Currently supports maps of MAPxy name structure.
//
static bool ACS_addDeferredScriptVM(ACSVM *vm, int scrnum, int mapnum, 
                                    int type, int args[NUMLINEARGS])
{
   DLListItem<deferredacs_t> *cur = acsDeferred;
   deferredacs_t *newdacs;

   // check to make sure the script isn't already scheduled
   while(cur)
   {
      deferredacs_t *dacs = cur->dllObject;

      if(dacs->targetMap == mapnum && dacs->scriptNum == scrnum)
         return false;

      cur = cur->dllNext;
   }

   // allocate a new deferredacs_t
   newdacs = estructalloc(deferredacs_t, 1);

   // set script number
   newdacs->scriptNum = scrnum; 

   // set vm id #
   newdacs->vmID = vm->id;

   // set action type
   newdacs->type = type;

   // set args
   memcpy(newdacs->args, args, 5 * sizeof(int));

   // record target map number
   newdacs->targetMap = mapnum;

   // add it to the linked list
   newdacs->link.insert(newdacs, &acsDeferred);

   return true;
}

//
// ACS_RunDeferredScripts
//
// Runs scripts that have been deferred for the current map
//
void ACS_RunDeferredScripts(void)
{
   DLListItem<deferredacs_t> *cur = acsDeferred, *next;
   ACSThinker *newScript = NULL;
   ACSThinker *rover = NULL;
   unsigned int internalNum;

   while(cur)
   {
      ACSVM *vm;
      deferredacs_t *dacs = cur->dllObject;

      next = cur->dllNext; 

      vm = acsVMs[dacs->vmID];

      if(dacs->targetMap == gamemap)
      {
         switch(dacs->type)
         {
         case ACS_DEFERRED_EXECUTE:
            ACS_StartScriptVM(vm, dacs->scriptNum, 0, dacs->args, NULL, NULL, 0, 
                              &newScript, false);
            if(newScript)
               newScript->delay = TICRATE;
            break;
         case ACS_DEFERRED_SUSPEND:
            if((internalNum = ACS_indexForNum(vm, dacs->scriptNum)) != vm->numScripts)
            {
               acscript_t *script = &(vm->scripts[internalNum]);
               rover = script->threads;

               while(rover)
               {
                  if(rover->sreg != ACS_STATE_STOPPED &&
                     rover->sreg != ACS_STATE_TERMINATE)
                     rover->sreg = ACS_STATE_SUSPEND;

                  rover = rover->nextthread;
               }
            }
            break;
         case ACS_DEFERRED_TERMINATE:
            if((internalNum = ACS_indexForNum(vm, dacs->scriptNum)) != vm->numScripts)
            {
               acscript_t *script = &(vm->scripts[internalNum]);
               rover = script->threads;

               while(rover)
               {
                  if(rover->sreg != ACS_STATE_STOPPED)
                     rover->sreg = ACS_STATE_TERMINATE;

                  rover = rover->nextthread;
               }
            }
            break;
         }

         // unhook and delete this deferred script
         cur->remove();
         efree(dacs);
      }

      cur = next;
   }
}

//
// ACS_StartScriptVM
//
// Standard method for starting an ACS script.
//
bool ACS_StartScriptVM(ACSVM *vm, int scrnum, int map, int *args,
                       Mobj *mo, line_t *line, int side,
                       ACSThinker **scr, bool always)
{
   acscript_t   *scrData;
   ACSThinker *newScript, *rover;
   bool foundScripts = false;
   unsigned int internalNum;
   int i;

   // ACS must be active on the current map or we do nothing
   if(!vm->loaded)
      return false;

   // should the script be deferred to a different map?
   if(map > 0 && map != gamemap)
      return ACS_addDeferredScriptVM(vm, scrnum, map, ACS_DEFERRED_EXECUTE, args);

   // look for the script
   if((internalNum = ACS_indexForNum(vm, scrnum)) == vm->numScripts)
   {
      // tink!
      doom_printf(FC_ERROR "ACS_StartScript: no such script %d\a\n", scrnum);
      return false;
   }

   scrData = &(vm->scripts[internalNum]);

   rover = scrData->threads;

   while(rover)
   {
      // if the script is suspended, restart it
      if(rover->sreg == ACS_STATE_SUSPEND)
      {
         foundScripts = true;
         rover->sreg = ACS_STATE_RUNNING;
      }

      rover = rover->nextthread;
   }

   // if not an always-execute action and we restarted a stopped script,
   // then we return true now.
   // otherwise, return false for failure.
   if(!always && scrData->threads)
      return foundScripts;

   // setup the new script thinker
   newScript = new ACSThinker;

   newScript->scriptNum   = scrnum;
   newScript->internalNum = internalNum;
   newScript->ip          = scrData->codePtr;
   newScript->line        = line;
   newScript->lineSide    = side;
   P_SetTarget<Mobj>(&newScript->trigger, mo);

   // copy in some important data
   newScript->code        = scrData->codePtr;
   newScript->data        = vm->code;
   newScript->printBuffer = vm->printBuffer;
   newScript->acscript    = scrData;
   newScript->vm          = vm;

   // copy arguments into first N local variables
   for(i = 0; i < scrData->numArgs; ++i)
      newScript->locals[i] = args[i];

   // attach the thinker
   newScript->addThinker();

   // add as a thread of the acscript
   ACS_addThread(newScript, scrData);
	
   // mark as running
   newScript->sreg  = ACS_STATE_RUNNING;
   newScript->sdata = 0;

   // return pointer to new script in *scr if not null
   if(scr)
      *scr = newScript;
	
   return true;
}

//
// ACS_StartScript
//
// Convenience routine; starts a script in the levelscript vm.
//
bool ACS_StartScript(int scrnum, int map, int *args, 
                     Mobj *mo, line_t *line, int side,
                     ACSThinker **scr, bool always)
{
   return ACS_StartScriptVM(&acsLevelScriptVM, scrnum, map, args, mo,
                            line, side, scr, always);
}

//
// ACS_TerminateScriptVM
//
// Attempts to terminate the given script. If the mapnum doesn't match the
// current gamemap, the action will be deferred.
//
bool ACS_TerminateScriptVM(ACSVM *vm, int scrnum, int mapnum)
{
   bool ret = false;
   int foo[NUMLINEARGS] = { 0, 0, 0, 0, 0 };

   // ACS must be active on the current map or we do nothing
   if(!vm->loaded)
      return false;

   if(mapnum > 0 && mapnum == gamemap)
   {
      unsigned int internalNum;

      if((internalNum = ACS_indexForNum(vm, scrnum)) != vm->numScripts)
      {
         acscript_t *script = &(vm->scripts[internalNum]);
         ACSThinker *rover = script->threads;

         while(rover)
         {
            if(rover->sreg != ACS_STATE_STOPPED)
            {
               rover->sreg = ACS_STATE_TERMINATE;
               ret = true;
            }
            rover = rover->nextthread;
         }
      }
   }
   else
      ret = ACS_addDeferredScriptVM(vm, scrnum, mapnum, ACS_DEFERRED_TERMINATE, foo);

   return ret;
}

//
// ACS_TerminateScript
//
// Convenience routine; terminates a level script.
//
bool ACS_TerminateScript(int scrnum, int mapnum)
{
   return ACS_TerminateScriptVM(&acsLevelScriptVM, scrnum, mapnum);
}

//
// ACS_SuspendScriptVM
//
// Attempts to suspend the given script. If the mapnum doesn't match the
// current gamemap, the action will be deferred.
//
bool ACS_SuspendScriptVM(ACSVM *vm, int scrnum, int mapnum)
{
   int foo[NUMLINEARGS] = { 0, 0, 0, 0, 0 };
   bool ret = false;

   // ACS must be active on the current map or we do nothing
   if(!vm->loaded)
      return false;

   if(mapnum > 0 && mapnum == gamemap)
   {
      unsigned int internalNum;

      if((internalNum = ACS_indexForNum(vm, scrnum)) != vm->numScripts)
      {
         acscript_t *script = &(vm->scripts[internalNum]);
         ACSThinker *rover = script->threads;

         while(rover)
         {
            if(rover->sreg != ACS_STATE_STOPPED &&
               rover->sreg != ACS_STATE_TERMINATE)
            {
               rover->sreg = ACS_STATE_SUSPEND;
               ret = true;
            }
         }
      }
   }
   else
      ret = ACS_addDeferredScriptVM(vm, scrnum, mapnum, ACS_DEFERRED_SUSPEND, foo);

   return ret;
}

//
// ACS_SuspendScript
//
// Convenience routine; suspends a level script.
//
bool ACS_SuspendScript(int scrnum, int mapnum)
{
   return ACS_SuspendScriptVM(&acsLevelScriptVM, scrnum, mapnum);
}

//=============================================================================
//
// Save/Load Code
//

//
// SaveArchive << deferredacs_t
//
static SaveArchive &operator << (SaveArchive &arc, deferredacs_t &dacs)
{
   arc << dacs.scriptNum << dacs.vmID << dacs.targetMap << dacs.type;
   P_ArchiveArray(arc, dacs.args, NUMLINEARGS);
   return arc;
}

//
// ACS_Archive
//
void ACS_Archive(SaveArchive &arc)
{
   DLListItem<deferredacs_t> *dacs;
   uint32_t size;

   // any OPEN script threads created during ACS_LoadLevelScript have
   // been destroyed, so clear out the threads list of all scripts
   if(arc.isLoading())
   {
      for(ACSVM **vm = acsVMs.begin(), **vmEnd = acsVMs.end(); vm != vmEnd; ++vm)
      {
         for(acscript_t *s = (*vm)->scripts,
             *sEnd = s + (*vm)->numScripts; s != sEnd; ++s)
         {
            s->threads = NULL;
         }
      }
   }

   // Archive map variables.
   for(ACSVM **vm = acsVMs.begin(), **vmEnd = acsVMs.end(); vm != vmEnd; ++vm)
   {
      P_ArchiveArray(arc, (*vm)->mapvars, ACS_NUM_MAPVARS);
   }

   // Archive world variables. (TODO: not load on hub transfer?)
   P_ArchiveArray(arc, ACSworldvars, ACS_NUM_WORLDVARS);

   // Archive global variables.
   P_ArchiveArray(arc, ACSglobalvars, ACS_NUM_GLOBALVARS);

   // Archive deferred scripts. (TODO: not load on hub transfer?)
   if(arc.isSaving())
   {
      for(size = 0, dacs = acsDeferred; dacs; dacs = dacs->dllNext)
         ++size;
   }

   arc << size;

   if(arc.isLoading())
   {
      while(size--)
      {
         deferredacs_t *dacsItem;
         dacsItem = estructalloc(deferredacs_t, 1);
         dacsItem->link.insert(dacsItem, &acsDeferred);
      }
   }

   for(dacs = acsDeferred; dacs; dacs = dacs->dllNext)
      arc << *dacs->dllObject;
}

//
// ACS_RestartSavedScript
//
// Fixes up an ACSThinker loaded from a savegame.
//
void ACS_RestartSavedScript(ACSThinker *th, unsigned int ipOffset)
{
   // nullify list links for safety
   th->prevthread = NULL;
   th->nextthread = NULL;

   // reinitialize pointers
   th->vm          = acsVMs[th->vmID];
   th->acscript    = &(th->vm->scripts[th->internalNum]);
   th->code        = th->acscript->codePtr;
   th->data        = th->vm->code;
   th->printBuffer = th->vm->printBuffer;

   // note: line and trigger pointers are restored in p_saveg.c

   // restore ip to be relative to new codebase (saved value is offset)
   th->ip = th->code + ipOffset;

   // add the thread
   ACS_addThread(th, th->acscript);
}

// EOF

