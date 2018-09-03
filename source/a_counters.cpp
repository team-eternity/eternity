// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Action Pointer Functions
//      that are associated with states/frames.
//
//      Counter-based "frame scripting" action functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_args.h"
#include "doomstat.h"
#include "d_gi.h"
#include "e_args.h"
#include "e_inventory.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "e_weapons.h"
#include "p_mobj.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"

//
// Frame Scripting Codepointers
//
// These implement what is more or less a full frame assembly
// language, whereby you cannot only loop, but branch on hard
// criteria, and write to memory. Not strictly necessary, but will
// make EDF more flexible and reduce the need for action scripting
// for mundane logical control of state transitions (and the
// potential of confusing recursive script executions).
//

// Codepointer comparison types

enum
{
   CPC_LESS,
   CPC_LESSOREQUAL,
   CPC_GREATER,
   CPC_GREATEROREQUAL,
   CPC_EQUAL,
   CPC_NOTEQUAL,
   CPC_BITWISEAND,
   
   CPC_CNTR_LESS,           // alternate counter versions
   CPC_CNTR_LESSOREQUAL,
   CPC_CNTR_GREATER,
   CPC_CNTR_GREATEROREQUAL,
   CPC_CNTR_EQUAL,
   CPC_CNTR_NOTEQUAL,
   CPC_CNTR_BITWISEAND,

   CPC_NUMIMMEDIATE = CPC_BITWISEAND + 1
};

// Codepointer operation types

enum
{
   CPOP_ASSIGN,
   CPOP_ADD,
   CPOP_SUB,
   CPOP_MUL,
   CPOP_DIV,
   CPOP_MOD,
   CPOP_AND,
   CPOP_ANDNOT,
   CPOP_OR,
   CPOP_XOR,
   CPOP_RND,
   CPOP_RNDMOD,
   CPOP_DAMAGE,
   CPOP_SHIFTLEFT,
   CPOP_SHIFTRIGHT,

   // unary operators
   CPOP_ABS,
   CPOP_NEGATE,
   CPOP_NOT,
   CPOP_INVERT,
};

static const char *kwds_CPCOps[] =
{
   "less",                   // 0
   "lessorequal",            // 1
   "greater",                // 2
   "greaterorequal",         // 3
   "equal",                  // 4
   "notequal",               // 5
   "and",                    // 6
   "less_counter",           // 7
   "lessorequal_counter",    // 8
   "greater_counter",        // 9
   "greaterorequal_counter", // 10
   "equal_counter",          // 11
   "notequal_counter",       // 12
   "and_counter",            // 13
};

static argkeywd_t cpckwds =
{
   kwds_CPCOps,
   sizeof(kwds_CPCOps) / sizeof(const char *)
};

//
// A_HealthJump
//
// Parameterized codepointer for branching based on comparisons
// against a thing's health.
//
// args[0] : state number
// args[1] : comparison type
// args[2] : health value OR counter number
//
void A_HealthJump(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   bool branch = false;   
   int statenum, checktype, checkhealth;

   statenum    = E_ArgAsStateNumNI(args, 0, mo);
   checktype   = E_ArgAsKwd(args, 1, &cpckwds, 0);
   checkhealth = E_ArgAsInt(args, 2, 0);

   // validate state
   if(statenum < 0)
      return;
   
   // 08/02/04:
   // support getting check value from a counter
   // if checktype is greater than the last immediate operator,
   // then the checkhealth value is actually a counter number

   if(checktype >= CPC_NUMIMMEDIATE)
   {
      // turn it into the corresponding immediate operation
      checktype -= CPC_NUMIMMEDIATE;

      if(checkhealth < 0 || checkhealth >= NUMMOBJCOUNTERS)
         return; // invalid counter number

      checkhealth = mo->counters[checkhealth];
   }

   switch(checktype)
   {
   case CPC_LESS:
      branch = (mo->health < checkhealth);  break;
   case CPC_LESSOREQUAL:
      branch = (mo->health <= checkhealth); break;
   case CPC_GREATER:
      branch = (mo->health > checkhealth);  break;
   case CPC_GREATEROREQUAL:
      branch = (mo->health >= checkhealth); break;
   case CPC_EQUAL:
      branch = (mo->health == checkhealth); break;
   case CPC_NOTEQUAL:
      branch = (mo->health != checkhealth); break;
   case CPC_BITWISEAND:
      branch = !!(mo->health & checkhealth);  break;
   default:
      break;
   }

   if(branch)
      P_SetMobjState(mo, statenum);
}

//
// A_CounterJump
//
// Parameterized codepointer for branching based on comparisons
// against a thing's counter values.
//
// args[0] : state number
// args[1] : comparison type
// args[2] : immediate value OR counter number
// args[3] : counter # to use
//
void A_CounterJump(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   bool branch = false;
   int statenum, checktype, value, cnum;
   int *counter;

   statenum  = E_ArgAsStateNumNI(args, 0, mo);
   checktype = E_ArgAsKwd(args, 1, &cpckwds, 0);
   value     = E_ArgAsInt(args, 2, 0);
   cnum      = E_ArgAsInt(args, 3, 0);
   
   // validate state
   if(statenum < 0)
      return;

   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   counter = &(mo->counters[cnum]);

   // 08/02/04:
   // support getting check value from a counter
   // if checktype is greater than the last immediate operator,
   // then the comparison value is actually a counter number

   if(checktype >= CPC_NUMIMMEDIATE)
   {
      // turn it into the corresponding immediate operation
      checktype -= CPC_NUMIMMEDIATE;

      if(value < 0 || value >= NUMMOBJCOUNTERS)
         return; // invalid counter number

      value = mo->counters[value];
   }

   switch(checktype)
   {
   case CPC_LESS:
      branch = (*counter < value);  break;
   case CPC_LESSOREQUAL:
      branch = (*counter <= value); break;
   case CPC_GREATER:
      branch = (*counter > value);  break;
   case CPC_GREATEROREQUAL:
      branch = (*counter >= value); break;
   case CPC_EQUAL:
      branch = (*counter == value); break;
   case CPC_NOTEQUAL:
      branch = (*counter != value); break;
   case CPC_BITWISEAND:
      branch = !!(*counter & value);  break;
   default:
      break;
   }

   if(branch)
      P_SetMobjState(mo, statenum);
}

//
// Parameterized codepointer for branching based on comparisons
// against a thing's counter values.
//
// args[0] : offset || state label
// args[1] : comparison type
// args[2] : immediate value || counter number
// args[3] : counter # to use
//
void A_CounterJumpEx(actionargs_t *actionargs)
{
   Mobj      *mo = actionargs->actor;
   arglist_t *args = actionargs->args;
   bool branch = false;
   int checktype, value, cnum;
   state_t *state;

   state = E_ArgAsStateLabel(mo, args, 0);
   checktype = E_ArgAsKwd(args, 1, &cpckwds, 0);
   value = E_ArgAsInt(args, 2, 0);
   cnum = E_ArgAsInt(args, 3, 0);

   // validate state
   if(!state)
      return;

   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   int &counter = mo->counters[cnum];

   // support getting check value from a counter
   // if checktype is greater than the last immediate operator,
   // then the comparison value is actually a counter number

   if(checktype >= CPC_NUMIMMEDIATE)
   {
      // turn it into the corresponding immediate operation
      checktype -= CPC_NUMIMMEDIATE;

      if(value < 0 || value >= NUMMOBJCOUNTERS)
         return; // invalid counter number

      value = mo->counters[value];
   }

   switch(checktype)
   {
   case CPC_LESS:
      branch = (counter < value);  break;
   case CPC_LESSOREQUAL:
      branch = (counter <= value); break;
   case CPC_GREATER:
      branch = (counter > value);  break;
   case CPC_GREATEROREQUAL:
      branch = (counter >= value); break;
   case CPC_EQUAL:
      branch = (counter == value); break;
   case CPC_NOTEQUAL:
      branch = (counter != value); break;
   case CPC_BITWISEAND:
      branch = !!(counter & value);  break;
   default:
      break;
   }

   if(branch)
      P_SetMobjState(mo, state->index);
}

//
// A_CounterSwitch
//
// This powerful codepointer can branch to one of N states
// depending on the value of the indicated counter, and it
// remains totally safe at all times. If the entire indicated
// frame set is not valid, no actions will be taken.
//
// args[0] : counter # to use
// args[1] : DeHackEd number of first frame in consecutive set
// args[2] : number of frames in consecutive set
//
void A_CounterSwitch(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int cnum, startstate, numstates;
   int *counter;

   cnum       = E_ArgAsInt       (args, 0,  0);
   startstate = E_ArgAsStateNumNI(args, 1, mo);
   numstates  = E_ArgAsInt       (args, 2,  0) - 1;

   // get counter
   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   counter = &(mo->counters[cnum]);

   // verify startstate
   if(startstate < 0)
      return;

   // verify last state is < NUMSTATES
   if(startstate + numstates >= NUMSTATES)
      return;

   // verify counter is in range
   if(*counter < 0 || *counter > numstates)
      return;

   // jump!
   P_SetMobjState(mo, startstate + *counter);
}

//
// This powerful codepointer can branch to one of N states
// depending on the value of the indicated counter, and it
// remains totally safe at all times. If the entire indicated
// frame set is not valid, no actions will be taken.
//
// args[0] : counter # to use
// args[N] : offset || state label
//
void A_CounterSwitchEx(actionargs_t *actionargs)
{
   Mobj      *mo = actionargs->actor;
   arglist_t *args = actionargs->args;
   int cnum, numstates;   
   state_t *state;

   cnum = E_ArgAsInt(args, 0, 0);

   // numstates is the top index
   numstates = args->numargs - 2;

   // get counter
   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   int &counter = mo->counters[cnum];

   // verify counter is in range
   if(counter < 0 || counter > numstates)
      return;

   // validate state
   if((state = E_ArgAsStateLabel(mo, args, 1 + counter)))
      P_SetMobjState(mo, state->index); // jump!
}

static const char *kwds_CPSetOps[] =
{
   "assign",         //  0
   "add",            //  1
   "sub",            //  2
   "mul",            //  3
   "div",            //  4
   "mod",            //  5
   "and",            //  6
   "andnot",         //  7
   "or",             //  8
   "xor",            //  9
   "rand",           //  10
   "randmod",        //  11
   "{DUMMY}",        //  12
   "rshift",         //  13
   "lshift",         //  14
};

static argkeywd_t cpsetkwds =
{
   kwds_CPSetOps, sizeof(kwds_CPSetOps) / sizeof(const char *)
};

//
// A_SetCounter
//
// Sets the value of the indicated counter variable for the thing.
// Can perform numerous operations -- this is more like a virtual
// machine than a codepointer ;)
//
// args[0] : counter # to set
// args[1] : value to utilize
// args[2] : operation to perform
//
void A_SetCounter(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int cnum, value, specialop;
   int *counter;

   cnum      = E_ArgAsInt(args, 0, 0);
   value     = E_ArgAsInt(args, 1, 0);
   specialop = E_ArgAsKwd(args, 2, &cpsetkwds, 0);

   if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
      return; // invalid

   counter = &(mo->counters[cnum]);

   switch(specialop)
   {
   case CPOP_ASSIGN:
      *counter = value; break;
   case CPOP_ADD:
      *counter += value; break;
   case CPOP_SUB:
      *counter -= value; break;
   case CPOP_MUL:
      *counter *= value; break;
   case CPOP_DIV:
      if(value) // don't divide by zero
         *counter /= value;
      break;
   case CPOP_MOD:
      if(value > 0) // only allow modulus by positive values
         *counter %= value;
      break;
   case CPOP_AND:
      *counter &= value; break;
   case CPOP_ANDNOT:
      *counter &= ~value; break; // compound and-not operation
   case CPOP_OR:
      *counter |= value; break;
   case CPOP_XOR:
      *counter ^= value; break;
   case CPOP_RND:
      *counter = P_Random(pr_setcounter); break;
   case CPOP_RNDMOD:
      if(value > 0)
         *counter = P_Random(pr_setcounter) % value;
      break;
   case CPOP_SHIFTLEFT:
      *counter <<= value; break;
   case CPOP_SHIFTRIGHT:
      *counter >>= value; break;
   default:
      break;
   }
}

static const char *kwds_CPOps[] =
{
   "{DUMMY}",        //  0
   "add",            //  1
   "sub",            //  2
   "mul",            //  3
   "div",            //  4
   "mod",            //  5
   "and",            //  6
   "{DUMMY}",        //  7
   "or",             //  8
   "xor",            //  9
   "{DUMMY}",        //  10
   "{DUMMY}",        //  11
   "hitdice",        //  12
   "rshift",         //  13
   "lshift",         //  14
   "abs",            //  15
   "negate",         //  16
   "not",            //  17
   "invert",         //  18
};

static argkeywd_t cpopkwds =
{
   kwds_CPOps, sizeof(kwds_CPOps) / sizeof(const char *)
};

//
// A_CounterOp
//
// Sets the value of the indicated counter variable for the thing
// using two (possibly the same) counters as operands.
//
// args[0] : counter operand #1
// args[1] : counter operand #2
// args[2] : counter destination
// args[3] : operation to perform
//
void A_CounterOp(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int c_oper1_num, c_oper2_num, c_dest_num, specialop;
   int *c_oper1, *c_oper2, *c_dest;

   c_oper1_num = E_ArgAsInt(args, 0, 0);
   c_oper2_num = E_ArgAsInt(args, 1, 0);
   c_dest_num  = E_ArgAsInt(args, 2, 0);   
   specialop   = E_ArgAsKwd(args, 3, &cpopkwds, 0);

   if(c_oper1_num < 0 || c_oper1_num >= NUMMOBJCOUNTERS)
      return; // invalid

   c_oper1 = &(mo->counters[c_oper1_num]);

   if(c_oper2_num < 0 || c_oper2_num >= NUMMOBJCOUNTERS)
      return; // invalid

   c_oper2 = &(mo->counters[c_oper2_num]);

   if(c_dest_num < 0 || c_dest_num >= NUMMOBJCOUNTERS)
      return; // invalid

   c_dest = &(mo->counters[c_dest_num]);

   switch(specialop)
   {
   case CPOP_ADD:
      *c_dest = *c_oper1 + *c_oper2; break;
   case CPOP_SUB:
      *c_dest = *c_oper1 - *c_oper2; break;
   case CPOP_MUL:
      *c_dest = *c_oper1 * *c_oper2; break;
   case CPOP_DIV:
      if(c_oper2) // don't divide by zero
         *c_dest = *c_oper1 / *c_oper2;
      break;
   case CPOP_MOD:
      if(*c_oper2 > 0) // only allow modulus by positive values
         *c_dest = *c_oper1 % *c_oper2;
      break;
   case CPOP_AND:
      *c_dest = *c_oper1 & *c_oper2; break;
   case CPOP_OR:
      *c_dest = *c_oper1 | *c_oper2; break;
   case CPOP_XOR:
      *c_dest = *c_oper1 ^ *c_oper2; break;
   case CPOP_DAMAGE:
      // do a HITDICE-style calculation
      if(*c_oper2 > 0) // the modulus must be positive
         *c_dest = *c_oper1 * ((P_Random(pr_setcounter) % *c_oper2) + 1);
      break;
   case CPOP_SHIFTLEFT:
      *c_dest = *c_oper1 << *c_oper2; break;
   case CPOP_SHIFTRIGHT:
      *c_dest = *c_oper1 >> *c_oper2; break;

      // unary operations (c_oper2 is unused for these)
   case CPOP_ABS:
      *c_dest = abs(*c_oper1); break;
   case CPOP_NEGATE:
      *c_dest = -(*c_oper1); break;
   case CPOP_NOT:
      *c_dest = !(*c_oper1); break;
   case CPOP_INVERT:
      *c_dest = ~(*c_oper1); break;
   default:
      break;
   }
}

//
// A_CopyCounter
//
// Copies the value of one counter into another.
//
// args[0] : source counter #
// args[1] : destination counter #
//
void A_CopyCounter(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int cnum1, cnum2;
   int *src, *dest;

   cnum1 = E_ArgAsInt(args, 0, 0);
   cnum2 = E_ArgAsInt(args, 1, 0);

   if(cnum1 < 0 || cnum1 >= NUMMOBJCOUNTERS)
      return; // invalid

   src = &(mo->counters[cnum1]);

   if(cnum2 < 0 || cnum2 >= NUMMOBJCOUNTERS)
      return; // invalid

   dest = &(mo->counters[cnum2]);

   *dest = *src;
}

//
// Weapon Frame Scripting
//
// These are versions of the above, crafted especially to use the new
// weapon counters.
// haleyjd 03/31/06
//

static const char *kwds_A_WeaponCtrJump[] =
{
   "less",                   // 0 
   "lessorequal",            // 1 
   "greater",                // 2 
   "greaterorequal",         // 3 
   "equal",                  // 4 
   "notequal",               // 5 
   "and",                    // 6 
   "less_counter",           // 7 
   "lessorequal_counter",    // 8 
   "greater_counter",        // 9 
   "greaterorequal_counter", // 10
   "equal_counter",          // 11
   "notequal_counter",       // 12
   "and_counter",            // 13
};

static argkeywd_t weapctrkwds =
{
   kwds_A_WeaponCtrJump, sizeof(kwds_A_WeaponCtrJump) / sizeof(const char *)
};

static const char *kwds_pspr_choice[] =
{
   "weapon",                // 0
   "flash",                 // 1
};

static argkeywd_t psprkwds =
{
   kwds_pspr_choice, sizeof(kwds_pspr_choice) / sizeof(const char *)
};

//
// A_WeaponCtrJump
//
// Parameterized codepointer for branching based on comparisons
// against a weapon's counter values.
//
// args[0] : state number
// args[1] : comparison type
// args[2] : immediate value OR counter number
// args[3] : counter # to use
// args[4] : psprite to affect (weapon or flash)
//
void A_WeaponCtrJump(actionargs_t *actionargs)
{
   bool branch = false;
   int statenum, checktype, cnum, psprnum;
   int value, *counter;
   player_t  *player;
   pspdef_t  *pspr;
   arglist_t *args = actionargs->args;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   statenum  = E_ArgAsStateNumNI(args, 0, player);
   checktype = E_ArgAsKwd(args, 1, &weapctrkwds, 0);
   value     = E_ArgAsInt(args, 2, 0);
   cnum      = E_ArgAsInt(args, 3, 0);
   psprnum   = E_ArgAsKwd(args, 4, &psprkwds, 0);
   
   // validate state
   if(statenum < 0)
      return;

   // validate psprite number
   if(psprnum < 0 || psprnum >= NUMPSPRITES)
      return;

   switch(cnum)
   {
   case 0:
   case 1:
   case 2:
      counter = E_GetIndexedWepCtrForPlayer(player, cnum);
      break;
   default:
      return;
   }

   // 08/02/04:
   // support getting check value from a counter
   // if checktype is greater than the last immediate operator,
   // then the comparison value is actually a counter number

   if(checktype >= CPC_NUMIMMEDIATE)
   {
      // turn it into the corresponding immediate operation
      checktype -= CPC_NUMIMMEDIATE;

      switch(value)
      {
      case 0:
      case 1:
      case 2:
         value = *E_GetIndexedWepCtrForPlayer(player, value);
         break;
      default:
         return; // invalid counter number
      }
   }

   switch(checktype)
   {
   case CPC_LESS:
      branch = (*counter < value); break;
   case CPC_LESSOREQUAL:
      branch = (*counter <= value); break;
   case CPC_GREATER:
      branch = (*counter > value); break;
   case CPC_GREATEROREQUAL:
      branch = (*counter >= value); break;
   case CPC_EQUAL:
      branch = (*counter == value); break;
   case CPC_NOTEQUAL:
      branch = (*counter != value); break;
   case CPC_BITWISEAND:
      branch = !!(*counter & value); break;
   default:
      break;
   }

   if(branch)
      P_SetPsprite(player, psprnum, statenum);
}

//
// Parameterized codepointer for branching based on comparisons
// against a weapon's counter values.
//
// args[0] : offset || state label
// args[1] : comparison type
// args[2] : immediate value OR counter number
// args[3] : counter # to use
// args[4] : psprite to affect (weapon or flash)
//
void A_WeaponCtrJumpEx(actionargs_t *actionargs)
{
   bool branch = false;
   int checktype, cnum, psprnum;
   int value, *counter;
   player_t  *player;
   pspdef_t  *pspr;
   state_t *state;
   Mobj *mo = actionargs->actor;
   arglist_t *args = actionargs->args;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   state     = E_ArgAsStateLabel(player, args, 0);
   checktype = E_ArgAsKwd(args, 1, &weapctrkwds, 0);
   value     = E_ArgAsInt(args, 2, 0);
   cnum      = E_ArgAsInt(args, 3, 0);
   psprnum   = E_ArgAsKwd(args, 4, &psprkwds, 0);
   
   // validate state
   if(!state)
      return;

   // validate psprite number
   if(psprnum < 0 || psprnum >= NUMPSPRITES)
      return;

   switch(cnum)
   {
   case 0:
   case 1:
   case 2:
      counter = E_GetIndexedWepCtrForPlayer(player, cnum);
      break;
   default:
      return;
   }

   // 08/02/04:
   // support getting check value from a counter
   // if checktype is greater than the last immediate operator,
   // then the comparison value is actually a counter number

   if(checktype >= CPC_NUMIMMEDIATE)
   {
      // turn it into the corresponding immediate operation
      checktype -= CPC_NUMIMMEDIATE;

      switch(value)
      {
      case 0:
      case 1:
      case 2:
         value = *E_GetIndexedWepCtrForPlayer(player, value);
         break;
      default:
         return; // invalid counter number
      }
   }

   switch(checktype)
   {
   case CPC_LESS:
      branch = (*counter < value); break;
   case CPC_LESSOREQUAL:
      branch = (*counter <= value); break;
   case CPC_GREATER:
      branch = (*counter > value); break;
   case CPC_GREATEROREQUAL:
      branch = (*counter >= value); break;
   case CPC_EQUAL:
      branch = (*counter == value); break;
   case CPC_NOTEQUAL:
      branch = (*counter != value); break;
   case CPC_BITWISEAND:
      branch = !!(*counter & value); break;
   default:
      break;
   }

   if(branch)
      P_SetPsprite(player, psprnum, state->index);
}

//
// A_WeaponCtrSwitch
//
// This powerful codepointer can branch to one of N states
// depending on the value of the indicated counter, and it
// remains totally safe at all times. If the entire indicated
// frame set is not valid, no actions will be taken.
//
// args[0] : counter # to use
// args[1] : DeHackEd number of first frame in consecutive set
// args[2] : number of frames in consecutive set
// args[3] : psprite to affect (weapon or flash)
//
void A_WeaponCtrSwitch(actionargs_t *actionargs)
{
   int cnum, startstate, numstates, psprnum;
   int       *counter;
   player_t  *player;
   pspdef_t  *pspr;
   arglist_t *args = actionargs->args;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   cnum       = E_ArgAsInt(args, 0, 0);
   startstate = E_ArgAsStateNumNI(args, 1, player);
   numstates  = E_ArgAsInt(args, 2, 0) - 1;
   psprnum    = E_ArgAsKwd(args, 3, &psprkwds, 0);

   // validate psprite number
   if(psprnum < 0 || psprnum >= NUMPSPRITES)
      return;

   // get counter
   switch(cnum)
   {
   case 0:
   case 1:
   case 2:
      counter = E_GetIndexedWepCtrForPlayer(player, cnum);
      break;
   default:
      return;
   }

   // verify startstate
   if(startstate < 0)
      return;

   // verify last state is < NUMSTATES
   if(startstate + numstates >= NUMSTATES)
      return;

   // verify counter is in range
   if(*counter < 0 || *counter > numstates)
      return;

   // jump!
   P_SetPsprite(player, psprnum, startstate + *counter);
}

static const char *kwds_A_WeaponSetCtr[] =
{
   "assign",           //  0
   "add",              //  1
   "sub",              //  2
   "mul",              //  3
   "div",              //  4
   "mod",              //  5
   "and",              //  6
   "andnot",           //  7
   "or",               //  8
   "xor",              //  9
   "rand",             //  10
   "randmod",          //  11
   "{DUMMY}",          //  12
   "rshift",           //  13
   "lshift",           //  14
};

static argkeywd_t weapsetkwds =
{
   kwds_A_WeaponSetCtr, sizeof(kwds_A_WeaponSetCtr) / sizeof(const char *)
};

//
// A_WeaponSetCtr
//
// Sets the value of the indicated counter variable for the thing.
// Can perform numerous operations -- this is more like a virtual
// machine than a codepointer ;)
//
// args[0] : counter # to set
// args[1] : value to utilize
// args[2] : operation to perform
//
void A_WeaponSetCtr(actionargs_t *actionargs)
{
   int cnum;
   int value;
   int specialop;
   int       *counter;
   player_t  *player;
   pspdef_t  *pspr;
   arglist_t *args = actionargs->args;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   cnum      = E_ArgAsInt(args, 0, 0);
   value     = E_ArgAsInt(args, 1, 0);
   specialop = E_ArgAsKwd(args, 2, &weapsetkwds, 0);

   switch(cnum)
   {
   case 0:
   case 1:
   case 2:
      counter = E_GetIndexedWepCtrForPlayer(player, cnum);
      break;
   default:
      return;
   }

   switch(specialop)
   {
   case CPOP_ASSIGN:
      *counter = value; break;
   case CPOP_ADD:
      *counter += value; break;
   case CPOP_SUB:
      *counter -= value; break;
   case CPOP_MUL:
      *counter *= value; break;
   case CPOP_DIV:
      if(value) // don't divide by zero
         *counter /= value;
      break;
   case CPOP_MOD:
      if(value > 0) // only allow modulus by positive values
         *counter %= value;
      break;
   case CPOP_AND:
      *counter &= value; break;
   case CPOP_ANDNOT:
      *counter &= ~value; break; // compound and-not operation
   case CPOP_OR:
      *counter |= value; break;
   case CPOP_XOR:
      *counter ^= value; break;
   case CPOP_RND:
      *counter = P_Random(pr_weapsetctr); break;
   case CPOP_RNDMOD:
      if(value > 0)
         *counter = P_Random(pr_weapsetctr) % value; break;
   case CPOP_SHIFTLEFT:
      *counter <<= value; break;
   case CPOP_SHIFTRIGHT:
      *counter >>= value; break;
   default:
      break;
   }
}

static const char *kwds_A_WeaponCtrOp[] =
{
   "{DUMMY}",           // 0
   "add",               // 1 
   "sub",               // 2 
   "mul",               // 3 
   "div",               // 4 
   "mod",               // 5 
   "and",               // 6
   "{DUMMY}",           // 7
   "or",                // 8 
   "xor",               // 9
   "{DUMMY}",           // 10
   "{DUMMY}",           // 11
   "hitdice",           // 12
   "rshift",            // 13
   "lshift",            // 14
   "abs",               // 15
   "negate",            // 16
   "not",               // 17
   "invert",            // 18
};

static argkeywd_t weapctropkwds =
{
   kwds_A_WeaponCtrOp, sizeof(kwds_A_WeaponCtrOp) / sizeof(const char *)
};

//
// A_WeaponCtrOp
//
// Sets the value of the indicated counter variable for the weapon
// using two (possibly the same) counters as operands.
//
// args[0] : counter operand #1
// args[1] : counter operand #2
// args[2] : counter destination
// args[3] : operation to perform
//
void A_WeaponCtrOp(actionargs_t *actionargs)
{
   player_t  *player;
   pspdef_t  *pspr;
   arglist_t *args = actionargs->args;
   int c_oper1_num;
   int c_oper2_num;
   int c_dest_num;
   int specialop;

   int *c_oper1, *c_oper2, *c_dest;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   c_oper1_num = E_ArgAsInt(args, 0, 0);
   c_oper2_num = E_ArgAsInt(args, 1, 0);
   c_dest_num  = E_ArgAsInt(args, 2, 0);
   specialop   = E_ArgAsKwd(args, 3, &weapctropkwds, 0);

   switch(c_oper1_num)
   {
   case 0:
   case 1:
   case 2:
      c_oper1 = E_GetIndexedWepCtrForPlayer(player, c_oper1_num);
      break;
   default:
      return;
   }

   switch(c_oper2_num)
   {
   case 0:
   case 1:
   case 2:
      c_oper2 = E_GetIndexedWepCtrForPlayer(player, c_oper2_num);
      break;
   default:
      return;
   }

   switch(c_dest_num)
   {
   case 0:
   case 1:
   case 2:
      c_dest = E_GetIndexedWepCtrForPlayer(player, c_dest_num);
      break;
   default:
      return;
   }

   switch(specialop)
   {
   case CPOP_ADD:
      *c_dest = *c_oper1 + *c_oper2; break;
   case CPOP_SUB:
      *c_dest = *c_oper1 - *c_oper2; break;
   case CPOP_MUL:
      *c_dest = *c_oper1 * *c_oper2; break;
   case CPOP_DIV:
      if(c_oper2) // don't divide by zero
         *c_dest = *c_oper1 / *c_oper2;
      break;
   case CPOP_MOD:
      if(*c_oper2 > 0) // only allow modulus by positive values
         *c_dest = *c_oper1 % *c_oper2;
      break;
   case CPOP_AND:
      *c_dest = *c_oper1 & *c_oper2; break;
   case CPOP_OR:
      *c_dest = *c_oper1 | *c_oper2; break;
   case CPOP_XOR:
      *c_dest = *c_oper1 ^ *c_oper2; break;
   case CPOP_DAMAGE:
      // do a HITDICE-style calculation
      if(*c_oper2 > 0) // the modulus must be positive
         *c_dest = *c_oper1 * ((P_Random(pr_weapsetctr) % *c_oper2) + 1);
      break;
   case CPOP_SHIFTLEFT:
      *c_dest = *c_oper1 << *c_oper2; break;
   case CPOP_SHIFTRIGHT:
      *c_dest = *c_oper1 >> *c_oper2; break;

      // unary operations (c_oper2 is unused for these)
   case CPOP_ABS:
      *c_dest = abs(*c_oper1); break;
   case CPOP_NEGATE:
      *c_dest = -(*c_oper1); break;
   case CPOP_NOT:
      *c_dest = !(*c_oper1); break;
   case CPOP_INVERT:
      *c_dest = ~(*c_oper1); break;
   default:
      break;
   }
}

//
// A_WeaponCopyCtr
//
// Copies the value of one counter into another.
//
// args[0] : source counter #
// args[1] : destination counter #
//
void A_WeaponCopyCtr(actionargs_t *actionargs)
{
   int cnum1, cnum2;
   int *src, *dest;
   player_t  *player;
   pspdef_t  *pspr;
   arglist_t *args = actionargs->args;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   cnum1 = E_ArgAsInt(args, 0, 0);
   cnum2 = E_ArgAsInt(args, 1, 0);

   switch(cnum1)
   {
   case 0:
   case 1:
   case 2:
      src = E_GetIndexedWepCtrForPlayer(player, cnum1);
      break;
   default:
      return;
   }

   switch(cnum2)
   {
   case 0:
   case 1:
   case 2:
      dest = E_GetIndexedWepCtrForPlayer(player, cnum2);
      break;
   default:
      return;
   }

   *dest = *src;
}

//
// A_CheckReloadEx
//
// An extended version of A_JumpIfAmmo that can compare against flexible values
// rather than assuming weapon's ammo per shot is the value to compare against.
// Args:
//    args[0] : state to jump to if test fails
//    args[1] : test to perform
//    args[2] : immediate value OR counter number to compare against
//    args[3] : psprite to affect (weapon or flash)
//
// Note: uses the exact same keyword set as A_WeaponCtrJump. Not redefined.
//
void A_CheckReloadEx(actionargs_t *actionargs)
{
   bool branch = false;
   int statenum, checktype, psprnum, value, ammo;
   player_t        *player;
   pspdef_t        *pspr;
   arglist_t       *args = actionargs->args;
   weaponinfo_t    *w;

   if(!(player = actionargs->actor->player))
      return;

   if(!(pspr = actionargs->pspr))
      return;

   w = player->readyweapon;
   if(!w->ammo) // no-ammo weapon?
      return;

   ammo      = E_GetItemOwnedAmount(player, w->ammo);
   statenum  = E_ArgAsStateNumNI(args, 0, player);
   checktype = E_ArgAsKwd(args, 1, &weapctrkwds, 0);
   value     = E_ArgAsInt(args, 2, 0);
   psprnum   = E_ArgAsKwd(args, 3, &psprkwds, 0);

   // validate state number
   if(statenum < 0)
      return;

   // validate psprite number
   if(psprnum < 0 || psprnum >= NUMPSPRITES)
      return;

   // 08/02/04:
   // support getting check value from a counter
   // if checktype is greater than the last immediate operator,
   // then the comparison value is actually a counter number

   if(checktype >= CPC_NUMIMMEDIATE)
   {
      // turn it into the corresponding immediate operation
      checktype -= CPC_NUMIMMEDIATE;

      switch(value)
      {
      case 0:
      case 1:
      case 2:
         value = *E_GetIndexedWepCtrForPlayer(player, value);
         break;
      default:
         return; // invalid counter number
      }
   }

   switch(checktype)
   {
   case CPC_LESS:
      branch = (ammo < value); break;
   case CPC_LESSOREQUAL:
      branch = (ammo <= value); break;
   case CPC_GREATER:
      branch = (ammo > value); break;
   case CPC_GREATEROREQUAL:
      branch = (ammo >= value); break;
   case CPC_EQUAL:
      branch = (ammo == value); break;
   case CPC_NOTEQUAL:
      branch = (ammo != value); break;
   case CPC_BITWISEAND:
      branch = !!(ammo & value); break;
   default:
      break;
   }

   if(branch)
      P_SetPsprite(player, psprnum, statenum);      
}

// EOF

