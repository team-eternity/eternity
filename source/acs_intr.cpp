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
#include "g_game.h"
#include "hu_stuff.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_random.h"
#include "m_swap.h"
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
static acsvm_t acsLevelScriptVM;

static acsvm_t **acsVMs;   // virtual machines
static int numACSVMs;      // number of vm's
static int numACSVMsAlloc; // number of vm pointers allocated


//
// Global Variables
//

acs_opdata_t ACSopdata[ACS_OPMAX] =
{
   #define ACS_OP(OP,ARGC) {ACS_OP_##OP, ARGC},
   #include "acs_op.h"
   #undef ACS_OP
};

// ACS_thingtypes:
// This array translates from ACS spawn numbers to internal thingtype indices.
// ACS spawn numbers are specified via EDF and are gamemode-dependent. EDF takes
// responsibility for populating this list.

int ACS_thingtypes[ACS_NUM_THINGTYPES];

// map variables
int ACSmapvars[32];

// world variables
int ACSworldvars[64];

//
// Static Functions
//

//
// ACS_addVirtualMachine
//
// haleyjd 06/24/08: keeps track of all ACS virtual machines.
//
static void ACS_addVirtualMachine(acsvm_t *vm)
{
   if(numACSVMs >= numACSVMsAlloc)
   {
      numACSVMsAlloc = numACSVMsAlloc ? numACSVMsAlloc * 2 : 4;
      acsVMs = erealloc(acsvm_t **, acsVMs, numACSVMsAlloc * sizeof(acsvm_t *));
   }

   acsVMs[numACSVMs] = vm;
   vm->id = numACSVMs++;
}

static bool ACS_addDeferredScriptVM(acsvm_t *vm, int scrnum, int mapnum, 
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
   acsvm_t *vm          = script->vm;
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
static void ACS_runOpenScript(acsvm_t *vm, acscript_t *acs, int iNum, int vmID)
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
   newScript->stringtable = vm->strings;
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
unsigned int ACS_indexForNum(acsvm_t *vm, int num)
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

// ZDoom blocking types
enum
{
   BLOCK_NOTHING,
   BLOCK_CREATURES,
   BLOCK_EVERYTHING,
   BLOCK_RAILING,
   BLOCK_PLAYERS
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

#define IPCURR()  (*ip)
#define IPNEXT()  (*ip++)

#define STACK_AT(x) (*(stp-(x)))

// for binary operations: ++ and -- mess these up
#define ST_BINOP(OP) temp = POP(); STACK_AT(1) = (STACK_AT(1) OP temp)
#define ST_BINOP_EQ(OP) temp = POP(); STACK_AT(1) OP temp

#define DIVOP_EQ(VAR, OP) \
   if(!(temp = POP())) \
   { \
      doom_printf(FC_ERROR "ACS Error: divide by zero\a"); \
      action = ACTION_ENDSCRIPT; \
      goto action_end; \
   } \
   (VAR) OP temp

#define BRANCH_COUNT() \
   if(++count > 500000) \
   { \
      doom_printf(FC_ERROR "ACS Error: terminated runaway script\a"); \
      action = ACTION_ENDSCRIPT; \
      goto action_end; \
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

// script states for T_ACSThinker
enum
{
   ACTION_RUN,       // default: keep running opcodes
   ACTION_STOP,      // stop execution for current tic
   ACTION_ENDSCRIPT, // end script execution on cmd or error
};

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
   int action = ACTION_RUN, count = 0;
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
      ACSmapvars[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_WORLDVAR):
      ACSworldvars[IPNEXT()] = POP();
      NEXTOP();

      // GET
   OPCODE(GET_IMM):
      PUSH(IPNEXT());
      NEXTOP();
   OPCODE(GET_LOCALVAR):
      PUSH(this->locals[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_MAPVAR):
      PUSH(ACSmapvars[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_WORLDVAR):
      PUSH(ACSworldvars[IPNEXT()]);
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
      ACSmapvars[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_WORLDVAR):
      ACSworldvars[IPNEXT()] += POP();
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
      --ACSmapvars[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_WORLDVAR):
      --ACSworldvars[IPNEXT()];
      NEXTOP();

      // DIV
   OPCODE(DIV_STACK):
      DIVOP_EQ(STACK_AT(1), /=);
      NEXTOP();
   OPCODE(DIV_LOCALVAR):
      DIVOP_EQ(this->locals[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_MAPVAR):
      DIVOP_EQ(ACSmapvars[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_WORLDVAR):
      DIVOP_EQ(ACSworldvars[IPNEXT()], /=);
      NEXTOP();

      // INC
   OPCODE(INC_LOCALVAR):
      ++this->locals[IPNEXT()];
      NEXTOP();
   OPCODE(INC_MAPVAR):
      ++ACSmapvars[IPNEXT()];
      NEXTOP();
   OPCODE(INC_WORLDVAR):
      ++ACSworldvars[IPNEXT()];
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
      DIVOP_EQ(ACSmapvars[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_WORLDVAR):
      DIVOP_EQ(ACSworldvars[IPNEXT()], %=);
      NEXTOP();

      // MUL
   OPCODE(MUL_STACK):
      ST_BINOP_EQ(*=);
      NEXTOP();
   OPCODE(MUL_LOCALVAR):
      this->locals[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_MAPVAR):
      ACSmapvars[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_WORLDVAR):
      ACSworldvars[IPNEXT()] *= POP();
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
      ACSmapvars[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_WORLDVAR):
      ACSworldvars[IPNEXT()] -= POP();
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
   OPCODE(STACK_DROP):
      DECSTP();
      NEXTOP();

      // Script Control
   OPCODE(SCRIPT_RESTART):
      ip = this->code;
      BRANCH_COUNT();
      NEXTOP();
   OPCODE(SCRIPT_SUSPEND):
      this->sreg = ACS_STATE_SUSPEND;
      action = ACTION_STOP;
      goto action_end;
   OPCODE(SCRIPT_TERMINATE):
      action = ACTION_ENDSCRIPT;
      goto action_end;

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
   OPCODE(PRINTSTRING):
      this->printBuffer->concat(this->stringtable[POP()]);
      NEXTOP();
   OPCODE(PRINTINT):
      {
         char buffer[33];
         this->printBuffer->concat(M_Itoa(POP(), buffer, 10));
      }
      NEXTOP();
   OPCODE(PRINTCHAR):
      *this->printBuffer += (char)POP();
      NEXTOP();

      // Miscellaneous

   OPCODE(DELAY):
      this->delay = POP();
      action = ACTION_STOP;
      goto action_end;
   OPCODE(DELAY_IMM):
      this->delay = IPNEXT();
      action = ACTION_STOP;
      goto action_end;

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
      action = ACTION_STOP;
      goto action_end;
   OPCODE(TAGWAIT_IMM):
      this->sreg  = ACS_STATE_WAITTAG;
      this->sdata = IPNEXT(); // get sector tag
      action = ACTION_STOP;
      goto action_end;

   OPCODE(POLYWAIT):
      this->sreg  = ACS_STATE_WAITPOLY;
      this->sdata = POP(); // get poly tag
      action = ACTION_STOP;
      goto action_end;
   OPCODE(POLYWAIT_IMM):
      this->sreg  = ACS_STATE_WAITPOLY;
      this->sdata = IPNEXT(); // get poly tag
      action = ACTION_STOP;
      goto action_end;

   OPCODE(CHANGEFLOOR):
      temp = POP(); // get flat string index
      P_ChangeFloorTex(this->stringtable[temp], POP()); // get tag
      NEXTOP();
   OPCODE(CHANGEFLOOR_IMM):
      temp = *ip++; // get tag
      P_ChangeFloorTex(this->stringtable[IPNEXT()], temp);
      NEXTOP();

   OPCODE(CHANGECEILING):
      temp = POP(); // get flat string index
      P_ChangeCeilingTex(this->stringtable[temp], POP()); // get tag
      NEXTOP();
   OPCODE(CHANGECEILING_IMM):
      temp = IPNEXT(); // get tag
      P_ChangeCeilingTex(this->stringtable[IPNEXT()], temp);
      NEXTOP();

   OPCODE(LINESIDE):
      PUSH(this->lineSide);
      NEXTOP();

   OPCODE(SCRIPTWAIT):
      this->sreg  = ACS_STATE_WAITSCRIPT;
      this->sdata = POP(); // get script num
      action = ACTION_STOP;
      goto action_end;
   OPCODE(SCRIPTWAIT_IMM):
      this->sreg  = ACS_STATE_WAITSCRIPT;
      this->sdata = IPNEXT(); // get script num
      action = ACTION_STOP;
      goto action_end;

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

         S_StartSoundNameAtVolume(src, this->stringtable[strnum], vol, 
                                  ATTN_NORMAL, CHAN_AUTO);
      }
      NEXTOP();

   OPCODE(AMBIENTSOUND):
      {
         int vol    = POP();
         int strnum = POP();

         S_StartSoundNameAtVolume(NULL, this->stringtable[strnum], vol, 
                                  ATTN_NORMAL, CHAN_AUTO);
      }
      NEXTOP();

   OPCODE(SOUNDSEQUENCE):
      {
         sector_t *sec;
         int strnum = POP();

         if(this->line && (sec = this->line->frontsector))
         {
            S_StartSectorSequenceName(sec, this->stringtable[strnum], 
                                      SEQ_ORIGIN_SECTOR_F);
         }
         /*
         FIXME
         else
         {
            S_StartSequenceName(NULL, vm->stringtable[strnum], 
                                SEQ_ORIGIN_OTHER, -1);
         }
         */
      }
      NEXTOP();

   OPCODE(SETLINETEXTURE):
      {
         int strnum = POP();
         int pos    = POP();
         int side   = POP();
         int tag    = POP();

         P_ChangeLineTex(this->stringtable[strnum], pos, side, tag, false);
      }
      NEXTOP();

   OPCODE(SETLINEBLOCKING):
      {
         int block = POP();
         int tag   = POP();

         ACS_setLineBlocking(tag, block);
      }
      NEXTOP();

   OPCODE(SETLINESPECIAL):
      {
         int tag;
         int16_t spec;
         int args[NUMLINEARGS];

         for(temp = 5; temp > 0; --temp)
            args[temp-1] = POP();
         spec = POP();
         tag  = POP();

         ACS_setLineSpecial(spec, args, tag);
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
            S_StartSoundNameAtVolume(mo, this->stringtable[strnum], vol,
                                     ATTN_NORMAL, CHAN_AUTO);
         }
      }
      NEXTOP();

#ifndef COMPGOTO
   default:
      // unknown opcode, must stop execution
      doom_printf(FC_ERROR "ACS Error: unknown opcode %d\a", opcode);
      action = ACTION_ENDSCRIPT;
      goto action_end;
#endif
   }

action_end:
   // check for special actions flagged in loop above
   switch(action)
   {
   case ACTION_ENDSCRIPT:
      // end the script
      ACS_stopScript(this, this->acscript);
      break;
   default:
      // copy fields back into script
      this->ip  = ip;
      this->stp = stp - this->stack;
      break;
   }
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
   P_ArchiveArray<int>(arc, locals, ACS_NUMLOCALS);

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
// ACS_Init
//
// Called at startup.
//
void ACS_Init(void)
{
   // initialize the qstring used to construct player messages
   acsLevelScriptVM.printBuffer = new qstring(qstring::basesize);

   // add levelscript vm as vm #0
   ACS_addVirtualMachine(&acsLevelScriptVM);
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
   acsLevelScriptVM.loaded = false;
}

//
// ACS_LoadScript
//
void ACS_LoadScript(acsvm_t *vm, int lump)
{
   byte *data;

   // zero length or too-short lump?
   if(W_LumpLength(lump) < 4)
   {
      vm->code = NULL;
      return;
   }

   // load the lump
   data = (byte *)(wGlobalDir.cacheLumpNum(lump, PU_LEVEL));

   switch(SwapLong(*(int32_t *)data))
   {
   case 0x00534341: // TODO: macro
      ACS_LoadScriptACS0(vm, lump, data);
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
// ACS_LoadLevelScript
//
// Loads the level scripts and initializes the levelscript virtual machine.
// Called from P_SetupLevel.
//
void ACS_LoadLevelScript(int lump)
{
   ACS_LoadScript(&acsLevelScriptVM, lump);
   
   // zero map variables
   memset(ACSmapvars, 0, sizeof(ACSmapvars));
}

//
// ACS_addDeferredScriptVM
//
// Adds a deferred script that will be executed when the indicated
// gamemap is reached. Currently supports maps of MAPxy name structure.
//
static bool ACS_addDeferredScriptVM(acsvm_t *vm, int scrnum, int mapnum, 
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
      acsvm_t *vm;
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
bool ACS_StartScriptVM(acsvm_t *vm, int scrnum, int map, int *args, 
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
   newScript->stringtable = vm->strings;
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
bool ACS_TerminateScriptVM(acsvm_t *vm, int scrnum, int mapnum)
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
bool ACS_SuspendScriptVM(acsvm_t *vm, int scrnum, int mapnum)
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
// ACS_PrepareForLoad
//
// Gets the ACS system ready for a savegame load.
//
void ACS_PrepareForLoad(void)
{
   int i;
   unsigned int j;

   for(i = 0; i < numACSVMs; ++i)
   {
      for(j = 0; j < acsVMs[i]->numScripts; ++j)
      {
         // any OPEN script threads created during ACS_LoadLevelScript
         // have been destroyed, so clear out the threads list of all 
         // scripts
         acsVMs[i]->scripts[j].threads = NULL;
      }
   }

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
   th->stringtable = th->vm->strings;
   th->printBuffer = th->vm->printBuffer;

   // note: line and trigger pointers are restored in p_saveg.c

   // restore ip to be relative to new codebase (saved value is offset)
   th->ip = (int *)(th->code + ipOffset);

   // add the thread
   ACS_addThread(th, th->acscript);
}

// EOF

