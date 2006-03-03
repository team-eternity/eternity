// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"

#ifdef ACS_INTR

//
// Local Enumerations
//

// script states for acscript sreg
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

// ACS texture position numbers
enum
{
   ACS_TEX_UPPER,
   ACS_TEX_MIDDLE,
   ACS_TEX_LOWER,
};

// opcode IDs
enum
{
   /*   0 */ OP_NOP,
   /*   1 */ OP_TERMINATE,
   /*   2 */ OP_SUSPEND,
   /*   3 */ OP_PUSHNUMBER,
   /*   4 */ OP_LINESPEC1,
   /*   5 */ OP_LINESPEC2,
   /*   6 */ OP_LINESPEC3,
   /*   7 */ OP_LINESPEC4,
   /*   8 */ OP_LINESPEC5,
   /*   9 */ OP_LINESPEC1_IMM,
   /*  10 */ OP_LINESPEC2_IMM,
   /*  11 */ OP_LINESPEC3_IMM,
   /*  12 */ OP_LINESPEC4_IMM,
   /*  13 */ OP_LINESPEC5_IMM,
   /*  14 */ OP_ADD,
   /*  15 */ OP_SUB,
   /*  16 */ OP_MUL,
   /*  17 */ OP_DIV,
   /*  18 */ OP_MOD,
   /*  19 */ OP_EQUAL,
   /*  20 */ OP_NOTEQUAL,
   /*  21 */ OP_LESS,
   /*  22 */ OP_GREATER,
   /*  23 */ OP_LESSOREQUAL,
   /*  24 */ OP_GREATEROREQUAL,
   /*  25 */ OP_ASSIGNSCRIPTVAR,
   /*  26 */ OP_ASSIGNMAPVAR,
   /*  27 */ OP_ASSIGNWORLDVAR,
   /*  28 */ OP_PUSHSCRIPTVAR,
   /*  29 */ OP_PUSHMAPVAR,
   /*  30 */ OP_PUSHWORLDVAR,
   /*  31 */ OP_ADDSCRIPTVAR,
   /*  32 */ OP_ADDMAPVAR,
   /*  33 */ OP_ADDWORLDVAR,
   /*  34 */ OP_SUBSCRIPTVAR,
   /*  35 */ OP_SUBMAPVAR,
   /*  36 */ OP_SUBWORLDVAR,
   /*  37 */ OP_MULSCRIPTVAR,
   /*  38 */ OP_MULMAPVAR,
   /*  39 */ OP_MULWORLDVAR,
   /*  40 */ OP_DIVSCRIPTVAR,
   /*  41 */ OP_DIVMAPVAR,
   /*  42 */ OP_DIVWORLDVAR,
   /*  43 */ OP_MODSCRIPTVAR,
   /*  44 */ OP_MODMAPVAR,
   /*  45 */ OP_MODWORLDVAR,
   /*  46 */ OP_INCSCRIPTVAR,
   /*  47 */ OP_INCMAPVAR,
   /*  48 */ OP_INCWORLDVAR,
   /*  49 */ OP_DECSCRIPTVAR,
   /*  50 */ OP_DECMAPVAR,
   /*  51 */ OP_DECWORLDVAR,
   /*  52 */ OP_BRANCH,
   /*  53 */ OP_BRANCHNOTZERO,
   /*  54 */ OP_DECSTP,
   /*  55 */ OP_DELAY,
   /*  56 */ OP_DELAY_IMM,
   /*  57 */ OP_RANDOM,
   /*  58 */ OP_RANDOM_IMM,
   /*  59 */ OP_THINGCOUNT,
   /*  60 */ OP_THINGCOUNT_IMM,
   /*  61 */ OP_TAGWAIT,
   /*  62 */ OP_TAGWAIT_IMM,
   /*  63 */ OP_POLYWAIT,
   /*  64 */ OP_POLYWAIT_IMM,
   /*  65 */ OP_CHANGEFLOOR,
   /*  66 */ OP_CHANGEFLOOR_IMM,
   /*  67 */ OP_CHANGECEILING,
   /*  68 */ OP_CHANGECEILING_IMM,
   /*  69 */ OP_RESTART,
   /*  70 */ OP_LOGICALAND,
   /*  71 */ OP_LOGICALOR,
   /*  72 */ OP_BITWISEAND,
   /*  73 */ OP_BITWISEOR,
   /*  74 */ OP_BITWISEXOR,
   /*  75 */ OP_LOGICALNOT,
   /*  76 */ OP_SHIFTLEFT,
   /*  77 */ OP_SHIFTRIGHT,
   /*  78 */ OP_NEGATE,
   /*  79 */ OP_BRANCHZERO,
   /*  80 */ OP_LINESIDE,
   /*  81 */ OP_SCRIPTWAIT,
   /*  82 */ OP_SCRIPTWAIT_IMM,
   /*  83 */ OP_CLEARLINESPECIAL,
   /*  84 */ OP_CASEJUMP,
   /*  85 */ OP_STARTPRINT,
   /*  86 */ OP_ENDPRINT,
   /*  87 */ OP_PRINTSTRING,
   /*  88 */ OP_PRINTINT,
   /*  89 */ OP_PRINTCHAR,
   /*  90 */ OP_PLAYERCOUNT,
   /*  91 */ OP_GAMETYPE,
   /*  92 */ OP_GAMESKILL,
   /*  93 */ OP_TIMER,
   /*  94 */ OP_SECTORSOUND,
   /*  95 */ OP_AMBIENTSOUND,
   /*  96 */ OP_SOUNDSEQUENCE,
   /*  97 */ OP_SETLINETEXTURE,
   /*  98 */ OP_SETLINEBLOCKING,
   /*  99 */ OP_SETLINESPECIAL,
   /* 100 */ OP_THINGSOUND,
   /* 101 */ OP_ENDPRINTBOLD,
};


//
// Static Variables
//

// pointer to the ACS lump, to which jumps are relative
static byte *acsdata;

// string table
static char **acs_stringtbl;

static qstring_t acsPrintBuffer;

// The scripts, as read from the ACS header.
static acscript_t *scripts;
static int        acsNumScripts;

// map variables
static int ACSmapvars[32];

// world variables
static int ACSworldvars[64];

// if true, we can use ACS on this map
static boolean acsLoaded;

// deferred scripts
// TODO


//
// Static Functions
//

//
// ACS_stopScript
//
// Ultimately terminates the script and removes its thinker.
//
static void ACS_stopScript(acsthinker_t *script, acscript_t *acscript)
{
   acscript->sreg = ACS_STATE_STOPPED;
   // TODO: notify waiting scripts that this script has ended
   P_RemoveThinker(&script->thinker);
}

//
// ACS_runOpenScript
//
// Starts an open script (a script indicated to start at the beginning of
// the level).
//
static void ACS_runOpenScript(acscript_t *acs, int iNum)
{
	acsthinker_t *newScript;

	newScript = Z_Malloc(sizeof(acsthinker_t), PU_LEVSPEC, 0);
	memset(script, 0, sizeof(acsthinker_t));
	
   newScript->scriptNum   = acs->number;
   newScript->internalNum = iNum;

   // open scripts wait one second before running
	newScript->delay = TICRATE;

   // set ip to entry point
	newScript->ip = acs->code;

   // set up thinker
   newScript->thinker.function = T_ACSThinker;
	P_AddThinker(&newScript->thinker);

   // mark as running
   acs->sreg = ACS_STATE_RUNNING;
}

//
// ACS_indexForNum
//
// Returns the index of the script in the "scripts" array given its number
// from within the script itself.
//
int ACS_indexForNum(int num)
{
   // TODO: Yet Another Hash Table (TM)

   return 0;
}

//
// ACS_execLineSpec
//
// Executes a line special that has been encoded in the script with
// operands on the stack.
//
static void ACS_execLineSpec(line_t *l, mobj_t *mo, int spec, int side,
                             int argc, int *argv)
{
   int args[5];

   // args are on stack in last to first order
   for(; argc > 0; --argc)
      args[argc-1] = *argv++;

   // FIXME/TODO: must translate line specials & args for Hexen maps

   P_ExecParamLineSpec(l, mo, spec, args, side, SPAC_CROSS, true);
}

//
// ACS_execLineSpecImm
//
// Executes a line special that has been encoded in the script with
// immediate operands.
//
static void ACS_execLineSpecImm(line_t *l, mobj_t *mo, int spec, int side,
                                int argc, int *argv)
{
   int args[5];

   // args follow instruction in the code from first to last
   for(; argc > 0; --argc)
      args[5-argc] = LONG(*argv++);

   // FIXME/TODO: must translate line specials & args for Hexen maps

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
   thinker_t *th;
   int count = 0;
   
   // FIXME/TODO: must translate Hexen thing type ids to true types

   // don't bother counting if no valid search is specified
   if(!(type || tid))
      return 0;
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function = P_MobjThinker)
      {
         mobj_t *mo = (mobj_t *)th;

         if((type == 0 || mo->type == type) && (tid == 0 || mo->tid == tid))
         {
            // don't count killable things that are dead
            if(((mo->flags & MF_COUNTKILL) || (mo->flags3 & MF3_KILLABLE)) &&
               mo->health < 0)
               continue;
            ++count;
         }
      }
   }

   return count;
}

//
// ACS_changeFloor
//
// Changes the floor flat of all tagged sectors.
//
static void ACS_changeFloor(const char *name, int tag)
{
   int flatnum;
   int secnum = -1;

   flatnum = R_FlatNumForName(name);

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      sectors[secnum].floorpic = flatnum;
}

//
// ACS_changeCeiling
//
// Changes the ceiling flat of all tagged sectors.
//
static void ACS_changeCeiling(const char *name, int tag)
{
   int flatnum;
   int secnum = -1;

   flatnum = R_FlatNumForName(name);

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      sectors[secnum].ceilingpic = flatnum;
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
// ACS_setLineTexture
//
// Sets the indicated texture on all tagged lines.
//
static void ACS_setLineTexture(const char *texture, int pos, int side, int tag)
{
	line_t *l;
	int linenum, texnum;

	texnum = R_TextureNumForName(texture);
	linenum = -1;

	while((l = P_FindLine(tag, &linenum)) != NULL)
	{
      if(l->sidenum[side] == -1)
         continue;

      switch(pos)
      {
      case ACS_TEX_UPPER:
         sides[l->sidenum[side]].toptexture = texnum;
         break;
      case ACS_TEX_MIDDLE:
         sides[l->sidenum[side]].midtexture = texnum;
         break;
      case ACS_TEX_LOWER:
         sides[l->sidenum[side]].bottomtexture = texnum;
         break;
      }
	}
}

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
      // clear the flag
      l->flags &= ~ML_BLOCKING;

      // if specified, reset the flag
      if(block)
         l->flags |= ML_BLOCKING;
   }
}

//
// ACS_setLineSpecial
//
// Sets all tagged lines' complete parameterized specials.
//
static void ACS_setLineSpecial(int spec, long *args, int tag)
{
   line_t *l;
   int linenum = -1;

   // FIXME/TODO: needs special/args translation for Hexen maps

   while((l = P_FindLine(tag, &linenum)) != NULL)
   {
      l->special = spec;
      memcpy(l->args, args, 5*sizeof(long));
   }
}

//
// Global Functions
//


// Interpreter Macros

#define PUSH(x)   stack[stp++] = (x)
#define POP()     stack[--stp]
#define PEEK()    stack[stp-1]
#define DECSTP()  --stp
#define DECSTP2() stp -= 2
#define DECSTP3() stp -= 3
#define DECSTP4() stp -= 4
#define DECSTP5() stp -= 5

#define IPCURR()  LONG(*ip)
#define IPNEXT()  LONG(*ip++)

// for binary operations: ++ and -- mess these up
#define ST_OP1      stack[stp-2]
#define ST_OP2      stack[stp-1]
#define ST_BINOP(x) stack[stp-2] = (x); --stp


// script states for T_ACSThinker
enum
{
   ACTION_RUN,       // default: keep running opcodes
   ACTION_STOP,      // stop execution for current tic
   ACTION_ENDSCRIPT, // end script execution on cmd or error
};

//
// T_ACSThinker
//
// Function for acs thinkers. Runs the script by interpreting its bytecode
// until the script terminates, is suspended, or waits on some condition.
//
void T_ACSThinker(acsthinker_t *script)
{
   // cache vm data in local vars for efficiency
   register int *ip    = script->ip;
   register int stp    = script->stp;
   register int *stack = script->stack;

   acscript_t *acscript = &scripts[script->internalNum];
   int action = ACTION_RUN;
   int opcode, temp, count = 0;

   // should the script terminate?
   if(acscript->sreg == ACS_STATE_TERMINATE)
      ACS_stopScript(script, acscript);

   // is the script running?
   if(acscript->sreg != ACS_STATE_RUNNING)
      return;

   // check for delays
   if(script->delay)
   {
      --script->delay;
      return;
   }
  
   // run opcodes until action != ACTION_RUN
   do
   {
      // count instructions to end unbounded loops
      if(++count >= 500000)
      {
         action = ACTION_ENDSCRIPT;
         break;
      }
      
      opcode = IPNEXT();
      switch(opcode)
      {
      case OP_NOP:
         break;
      case OP_TERMINATE:
         action = ACTION_ENDSCRIPT;
         break;
      case OP_SUSPEND:
         acscript->sreg = ACS_STATE_SUSPEND;
         action = ACTION_STOP;
         break;
      case OP_PUSHNUMBER:
         PUSH(IPNEXT());
         break;
      case OP_LINESPEC1:
         ACS_execLineSpec(script->line, script->trigger, IPNEXT(), 
                          script->lineSide, 1, stack);
         DECSTP();
         break;
      case OP_LINESPEC2:
         ACS_execLineSpec(script->line, script->trigger, IPNEXT(), 
                          script->lineSide, 2, stack);
         DECSTP2();
         break;
      case OP_LINESPEC3:
         ACS_execLineSpec(script->line, script->trigger, IPNEXT(), 
                          script->lineSide, 3, stack);
         DECSTP3();
         break;
      case OP_LINESPEC4:
         ACS_execLineSpec(script->line, script->trigger, IPNEXT(), 
                          script->lineSide, 4, stack);
         DECSTP4();
         break;
      case OP_LINESPEC5:
         ACS_execLineSpec(script->line, script->trigger, IPNEXT(), 
                          script->lineSide, 5, stack);
         DECSTP5();
         break;
      case OP_LINESPEC1_IMM:
         temp = IPNEXT(); // read special
         ACS_execLineSpecImm(script->line, script->trigger, temp, 
                             script->lineSide, 1, ip);
         ++ip; // skip past arg
         break;
      case OP_LINESPEC2_IMM:
         temp = IPNEXT(); // read special
         ACS_execLineSpecImm(script->line, script->trigger, temp,
                             script->lineSide, 2, ip);
         ip += 2; // skip past args
         break;
      case OP_LINESPEC3_IMM:
         temp = IPNEXT(); // read special
         ACS_execLineSpecImm(script->line, script->trigger, temp,
                             script->lineSide, 3, ip);
         ip += 3; // skip past args
         break;
      case OP_LINESPEC4_IMM:
         temp = IPNEXT(); // read special
         ACS_execLineSpecImm(script->line, script->trigger, temp,
                             script->lineSide, 4, ip);
         ip += 4; // skip past args
         break;
      case OP_LINESPEC5_IMM:
         temp = IPNEXT(); // read special
         ACS_execLineSpecImm(script->line, script->trigger, temp,
                             script->lineSide, 5, ip);
         ip += 5; // skip past args
         break;
      case OP_ADD:
         ST_BINOP(ST_OP1 + ST_OP2);
         break;
      case OP_SUB:
         ST_BINOP(ST_OP1 - ST_OP2);
         break;
      case OP_MUL:
         ST_BINOP(ST_OP1 * ST_OP2);
         break;
      case OP_DIV:
         if(!(temp = ST_OP2))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         ST_BINOP(ST_OP1 / temp);
         break;
      case OP_MOD:
         if(!(temp = ST_OP2))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         ST_BINOP(ST_OP1 % temp);
         break;
      case OP_EQUAL:
         ST_BINOP(ST_OP1 == ST_OP2);
         break;
      case OP_NOTEQUAL:
         ST_BINOP(ST_OP1 != ST_OP2);
         break;
      case OP_LESS:
         ST_BINOP(ST_OP1 < ST_OP2);
         break;
      case OP_GREATER:
         ST_BINOP(ST_OP1 > ST_OP2);
         break;
      case OP_LESSOREQUAL:
         ST_BINOP(ST_OP1 <= ST_OP2);
         break;
      case OP_GREATEROREQUAL:
         ST_BINOP(ST_OP1 >= ST_OP2);
         break;
      case OP_ASSIGNSCRIPTVAR:
         script->locals[IPNEXT()] = POP();
         break;
      case OP_ASSIGNMAPVAR:
         ACSmapvars[IPNEXT()] = POP();
         break;
      case OP_ASSIGNWORLDVAR:
         ACSworldvars[IPNEXT()] = POP();
         break;
      case OP_PUSHSCRIPTVAR:
         PUSH(script->locals[IPNEXT()]);
         break;
      case OP_PUSHMAPVAR:
         PUSH(ACSmapvars[IPNEXT()]);
         break;
      case OP_PUSHWORLDVAR:
         PUSH(ACSworldvars[IPNEXT()]);
         break;
      case OP_ADDSCRIPTVAR:
         script->locals[IPNEXT()] += POP();
         break;
      case OP_ADDMAPVAR:
         ACSmapvars[IPNEXT()] += POP();
         break;
      case OP_ADDWORLDVAR:
         ACSworldvars[IPNEXT()] += POP();
         break;
      case OP_SUBSCRIPTVAR:
         script->locals[IPNEXT()] -= POP();
         break;
      case OP_SUBMAPVAR:
         ACSmapvars[IPNEXT()] -= POP();
         break;
      case OP_SUBWORLDVAR:
         ACSworldvars[IPNEXT()] -= POP();
         break;
      case OP_MULSCRIPTVAR:
         script->locals[IPNEXT()] *= POP();
         break;
      case OP_MULMAPVAR:
         ACSmapvars[IPNEXT()] *= POP();
         break;
      case OP_MULWORLDVAR:
         ACSworldvars[IPNEXT()] *= POP();
         break;
      case OP_DIVSCRIPTVAR:
         if(!(temp = POP()))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         script->locals[IPNEXT()] /= temp;
         break;
      case OP_DIVMAPVAR:
         if(!(temp = POP()))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         ACSmapvars[IPNEXT()] /= temp;
         break;
      case OP_DIVWORLDVAR:
         if(!(temp = POP()))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         ACSworldvars[IPNEXT()] /= temp;
         break;
      case OP_MODSCRIPTVAR:
         if(!(temp = POP()))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         script->locals[IPNEXT()] %= temp;
         break;
      case OP_MODMAPVAR:
         if(!(temp = POP()))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         ACSmapvars[IPNEXT()] %= temp;
         break;
      case OP_MODWORLDVAR:
         if(!(temp = POP()))
         {
            action = ACTION_ENDSCRIPT;
            break;
         }
         ACSworldvars[IPNEXT()] %= temp;
         break;
      case OP_INCSCRIPTVAR:
         script->locals[IPNEXT()] += 1;
         break;
      case OP_INCMAPVAR:
         ACSmapvars[IPNEXT()] += 1;
         break;
      case OP_INCWORLDVAR:
         ACSworldvars[IPNEXT()] += 1;
         break;
      case OP_DECSCRIPTVAR:
         script->locals[IPNEXT()] -= 1;
         break;
      case OP_DECMAPVAR:
         ACSmapvars[IPNEXT()] -= 1;
         break;
      case OP_DECWORLDVAR:
         ACSworldvars[IPNEXT()] -= 1;
         break;
      case OP_BRANCH:
         ip = (int *)(acsdata + IPCURR());
         break;
      case OP_BRANCHNOTZERO:
         if((temp = POP()))
            ip = (int *)(acsdata + IPCURR());
         else
            ++ip;
         break;
      case OP_DECSTP:
         DECSTP();
         break;
      case OP_DELAY:
         script->delay = POP();
         action = ACTION_STOP;
         break;
      case OP_DELAY_IMM:
         script->delay = IPNEXT();
         action = ACTION_STOP;
         break;
      case OP_RANDOM:
         {
            int min, max;
            max = POP();
            min = POP();

            PUSH((P_Random(pr_script) % (max - min + 1)) + min);
         }
         break;
      case OP_RANDOM_IMM:
         {
            int min, max;
            min = LONG(*ip++);
            max = LONG(*ip++);

            PUSH((P_Random(pr_script) % (max - min + 1)) + min);
         }
         break;
      case OP_THINGCOUNT:
         temp = POP(); // get tid
         temp = ACS_countThings(POP(), temp);
         PUSH(temp);
         break;
      case OP_THINGCOUNT_IMM:
         temp = IPNEXT(); // get type
         temp = ACS_countThings(temp, IPNEXT());
         PUSH(temp);
         break;
      case OP_TAGWAIT:
         acscript->sreg  = ACS_STATE_WAITTAG;
         acscript->sdata = POP(); // get sector tag
         action = ACTION_STOP;
         break;
      case OP_TAGWAIT_IMM:
         acscript->sreg  = ACS_STATE_WAITTAG;
         acscript->sdata = IPNEXT(); // get sector tag
         action = ACTION_STOP;
         break;
      case OP_POLYWAIT:
         acscript->sreg  = ACS_STATE_WAITPOLY;
         acscript->sdata = POP(); // get poly tag
         action = ACTION_STOP;
         break;
      case OP_POLYWAIT_IMM:
         acscript->sreg  = ACS_STATE_WAITPOLY;
         acscript->sdata = IPNEXT(); // get poly tag
         action = ACTION_STOP;
         break;
      case OP_CHANGEFLOOR:
         temp = POP(); // get flat string index
         ACS_changeFloor(acs_stringtbl[temp], POP()); // get tag
         break;
      case OP_CHANGEFLOOR_IMM:
         temp = *ip++; // get tag
         ACS_changeFloor(acs_stringtbl[IPNEXT()], temp);
         break;
      case OP_CHANGECEILING:
         temp = POP(); // get flat string index
         ACS_changeCeiling(acs_stringtbl[temp], POP()); // get tag
         break;
      case OP_CHANGECEILING_IMM:
         temp = IPNEXT(); // get tag
         ACS_changeCeiling(acs_stringtbl[IPNEXT()], temp);
         break;
      case OP_RESTART:
         ip = acscript->code;
         break;
      case OP_LOGICALAND:
         ST_BINOP(ST_OP1 && ST_OP2);
         break;
      case OP_LOGICALOR:
         ST_BINOP(ST_OP1 || ST_OP2);
         break;
      case OP_BITWISEAND:
         ST_BINOP(ST_OP1 & ST_OP2);
         break;
      case OP_BITWISEOR:
         ST_BINOP(ST_OP1 | ST_OP2);
         break;
      case OP_BITWISEXOR:
         ST_BINOP(ST_OP1 ^ ST_OP2);
         break;
      case OP_LOGICALNOT:
         temp = POP();
         PUSH(!temp);
         break;
      case OP_SHIFTLEFT:
         ST_BINOP(ST_OP1 << ST_OP2);
         break;
      case OP_SHIFTRIGHT:
         ST_BINOP(ST_OP1 >> ST_OP2);
         break;
      case OP_NEGATE:
         temp = POP();
         PUSH(-temp);
         break;
      case OP_BRANCHZERO:
         if(!(temp = POP()))
            ip = (int *)(acsdata + IPCURR());
         else
            ++ip;
         break;
      case OP_LINESIDE:
         PUSH(script->lineSide);
         break;
      case OP_SCRIPTWAIT:
         acscript->sreg  = ACS_STATE_WAITSCRIPT;
         acscript->sdata = POP(); // get script num
         action = ACTION_STOP;
         break;
      case OP_SCRIPTWAIT_IMM:
         acscript->sreg  = ACS_STATE_WAITSCRIPT;
         acscript->sdata = IPNEXT(); // get script num
         action = ACTION_STOP;
         break;
      case OP_CLEARLINESPECIAL:
         if(script->line)
            script->line->special = 0;
         break;
      case OP_CASEJUMP:
         if(PEEK() == IPNEXT()) // compare top of stack against op+1
         {
            DECSTP(); // take the value off the stack
            ip = (int *)(acsdata + IPCURR()); // jump to op+2
         }
         else
            ++ip; // increment past offset at op+2, leave value on stack
         break;
      case OP_STARTPRINT:
         M_QStrClear(&acsPrintBuffer);
         break;
      case OP_ENDPRINT:
         if(script->trigger && script->trigger->player)
            player_printf(script->trigger->player, acsPrintBuffer.buffer);
         else
            player_printf(&players[consoleplayer], acsPrintBuffer.buffer);
         break;
      case OP_PRINTSTRING:
         M_QStrCat(&acsPrintBuffer, acs_stringtbl[POP()]);
         break;
      case OP_PRINTINT:
         {
            char buffer[33];
            M_QStrCat(&acsPrintBuffer, M_Itoa(POP(), buffer, 10));
         }
         break;
      case OP_PRINTCHAR:
         M_QStrPutc(&acsPrintBuffer, (char)POP());
         break;
      case OP_PLAYERCOUNT:
         PUSH(ACS_countPlayers());
         break;
      case OP_GAMETYPE:
         PUSH(GameType);
         break;
      case OP_GAMESKILL:
         PUSH(gameskill);
         break;
      case OP_TIMER:
         PUSH(leveltime);
         break;
      case OP_SECTORSOUND:
         DECSTP2(); // TODO/FIXME: needs hexen sound translation & volume
         break;
      case OP_AMBIENTSOUND:
         DECSTP2(); // TODO/FIXME: needs hexen sound translation & volume
         break;
      case OP_SOUNDSEQUENCE:
         DECSTP();  // TODO/FIXME: needs sound sequences
         break;
      case OP_SETLINETEXTURE:
         {
            int pos, side, tag, strnum;

            strnum = POP();
            pos    = POP();
            side   = POP();
            tag    = POP();

            ACS_setLineTexture(acs_stringtbl[strnum], pos, side, tag);
         }
         break;
      case OP_SETLINEBLOCKING:
         {
            int tag, block;

            block = POP();
            tag   = POP();

            ACS_setLineBlocking(tag, block);
         }
         break;
      case OP_SETLINESPECIAL:
         {
            int tag, spec;
            long args[5];

            for(temp = 5; temp > 0; --temp)
               args[temp-1] = POP();
            spec = POP();
            tag  = POP();

            ACS_setLineSpecial(spec, args, tag);
         }
         break;
      case OP_THINGSOUND:
         DECSTP3(); // TODO/FIXME: needs hexen sound translation & volume
         break;
      case OP_ENDPRINTBOLD:
         HU_CenterMsgTimedColor(acsPrintBuffer.buffer, FC_YELLOW, 20*35);
         break;
      default:
         // unknown opcode, must stop execution
         action = ACTION_ENDSCRIPT;
         break;
      }
   } while(action == ACTION_RUN);

   // check for special actions flagged in loop above
   switch(action)
   {
   case ACTION_ENDSCRIPT:
      // end the script
      ACS_stopScript(script, acscript);
      break;
   default:
      // copy fields back into script
      script->ip  = ip;
      script->stp = stp;
      break;
   }
}

//
// ACS_Init
//
// Called at startup.
//
void ACS_Init(void)
{
   // initialize the qstring used to construct player messages
   M_QStrInitCreate(&acsPrintBuffer);

   // TODO: init deferred scripts?
}

//
// ACS_NewGame
//
// Called when a new game is started.
//
void ACS_NewGame(void)
{
   // clear out the world variables
   memset(ACSworldvars, 0, sizeof(ACSworldvars));

   // TODO: clear out deferred scripts
}

//
// ACS_InitLevel
//
// Called at level start from P_SetupLevel
//
void ACS_InitLevel(void)
{
   acsLoaded = false;
}

//
// ACS_LoadScript
//
// Called at level start from P_SetupLevel.
//
void ACS_LoadScript(int lump)
{
   long *rover;
   int i, numstrings;

   // zero length lump?
   if(!W_LumpLength(lump))
   {
      acsdata = NULL;
      return;
   }

   // load the lump
   acsdata = W_CacheLumpNum(lump, PU_LEVEL);

   rover = (long *)acsdata;

   // check magic id string: currently supports Hexen format only
   if(LONG(*rover++) != 0x00534341) // "ACS\0"
      return;

   // set rover to information table
   rover = (long *)(acsdata + LONG(*rover));

   // read number of scripts
   acsNumScripts = LONG(*rover++);

   if(!acsNumScripts) // no scripts defined?
      return;

   // allocate scripts array
   scripts = Z_Malloc(numscripts * sizeof(acscript_t), PU_LEVEL, NULL);

   acsLoaded = true;

   // read script information entries
   for(i = 0; i < numscripts; ++i)
   {
      scripts[i].number  = LONG(*rover++); // read script number
      scripts[i].code    = (long *)(acsdata + LONG(*rover++)); // set entry pt
      scripts[i].numArgs = LONG(*rover++);

      // handle open scripts: scripts > 1000 should start at the
      // beginning of the level
      if(scripts[i].number >= 1000)
      {
         scripts[i].number -= 1000;
         ACS_runOpenScript(&scripts[i], i);
      }
      else
         scripts[i].sreg = ACS_STATE_STOPPED;

      scripts[i].sdata = 0;
   }

   // we are now positioned at the string table; read number of strings
   numstrings = LONG(*rover++);

   // allocate string table
   if(numstrings > 0)
   {
      acs_stringtbl = (char **)Z_Malloc(numstrings * sizeof(char *), 
                                        PU_LEVEL, NULL);
      
      // set string pointers
      for(i = 0; i < numstrings; ++i)
         acs_stringtbl[i] = (char *)(acsdata + LONG(*rover++));
   }

   // zero map variables
   memset(ACSmapvars, 0, sizeof(ACSmapvars));
}

//
// ACS_AddDeferredScript
//
// Adds a deferred script that will be executed when the indicated
// gamemap is reached. Currently supports maps of MAPxy name structure.
//
void ACS_AddDeferredScript(int scrnum, int mapnum, int args[3])
{
   // TODO
}

//
// ACS_RunDeferredScripts
//
// Runs scripts that have been deferred for the current map
//
void ACS_RunDeferredScripts(void)
{
   // TODO
}

//
// ACS_StartScript
//
// Standard method for starting an ACS script.
//
boolean ACS_StartScript(int scrnum, int map, int *args, 
                        mobj_t *mo, line_t *line, int side)
{
   acscript_t   *scrData;
   acsthinker_t *newScript;
   int i, internalNum;

   // ACS must be active on the current map or we do nothing
   if(!acsLoaded)
      return false;

	if(map > 0 && map != gamemap)
	{
      // TODO: add to deferred scripts instead of running
      return true;
	}

   if((internalNum = ACS_indexForNum(scrnum)) == acsNumScripts)
	{
      // tink!
      C_Printf(FC_ERROR "ACS_StartScript: no such script %d\a\n", scrnum);
      return false;
	}

   scrData = &scripts[internalNum];

   // if the script is suspended, restart it
	if(scrData->sreg == ACS_STATE_SUSPEND)
	{ 
		scrData->sreg = ACS_STATE_RUNNING;
		return true;
	}

   // if the script is already running, we can't do anything
	if(scrData->sreg != ACS_STATE_STOPPED)
		return false;

   // setup the new script thinker
   newScript = Z_Malloc(sizeof(acsthinker_t), PU_LEVSPEC, NULL);	
   memset(newScript, 0, sizeof(acsthinker_t));

   newScript->scriptNum   = scrnum;
   newScript->internalNum = internalNum;
   newScript->ip          = scrData->code;
   newScript->line        = line;
   newScript->lineSide    = side;
   P_SetTarget(&newScript->trigger, mo);

   // copy arguments into first N local variables
   for(i = 0; i < scrData->numArgs; ++i)
      newScript->locals[i] = args[i];

   // attach the thinker
   newScript->thinker.function = T_ACSThinker;
   P_AddThinker(&newScript->thinker);
	
   // mark as running
   scrData->sreg  = ACS_STATE_RUNNING;
   scrData->sdata = 0;
	
   return true;
}

#endif

// EOF

