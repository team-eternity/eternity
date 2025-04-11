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
//-----------------------------------------------------------------------------
//
// Command running functions
//
// Running typed commands, or network/linedef triggers. Sending commands over
// the network. Calls handlers for some commands.
//
// By Simon Howard
//
// NETCODE_FIXME -- CONSOLE_FIXME
// Parts of this module also are involved in the netcode cmd problems.
// Mainly, the buffering issue. Commands need to work without being
// delayed an arbitrary amount of time, that won't work with netgames
// properly. 
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_dwfile.h"
#include "d_io.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_utils.h"
#include "mn_engin.h"
#include "v_misc.h"
#include "w_wad.h"

static void C_EchoValue(command_t *command);
static void C_SetVariable(command_t *command);
static void C_RunAlias(alias_t *alias);
static int  C_Sync(command_t *command);
static void C_ArgvtoArgs();
static bool C_Strcmp(const char *pa, const char *pb);

//=============================================================================
//
// The Console (TM)
//

console_t Console;

//=============================================================================
//
// Parsing/Running Commands
//

static qstring **cmdtokens;
static int numtokens;
static int numtokensalloc;

//
// C_nextCmdToken
//
// haleyjd 08/08/10: Used to remove SMMU limit on console command tokens.
//
static void C_nextCmdToken()
{
   if(numtokens >= numtokensalloc)
   {
      int i;

      // grow by MAXTOKENS at a time (doubling is likely to waste memory)
      numtokensalloc += MAXTOKENS;
      cmdtokens = erealloc(qstring **, cmdtokens, numtokensalloc * sizeof(qstring *));

      for(i = numtokens; i < numtokensalloc; i++)
         cmdtokens[i] = new qstring(128);

      Console.numargvsalloc += MAXTOKENS;
      Console.argv = erealloc(qstring **, Console.argv, Console.numargvsalloc * sizeof(qstring *));

      for(i = numtokens; i < Console.numargvsalloc; i++)
         Console.argv[i] = new qstring(128);
   }
   numtokens++;
}

//
// C_initCmdTokens
//
// haleyjd 07/05/10: create qstrings for the console tokenizer.
//
static void C_initCmdTokens()
{
   static bool cmdtokensinit;

   if(!cmdtokensinit)
   {
      int i;

      // haleyjd: MAXTOKENS is now just the starting size of the array
      cmdtokens = ecalloc(qstring **, MAXTOKENS, sizeof(qstring *));
      numtokensalloc = MAXTOKENS;

      Console.argv = ecalloc(qstring **, MAXTOKENS, sizeof(qstring *));
      Console.numargvsalloc = MAXTOKENS;

      for(i = 0; i < numtokensalloc; i++)
      {
         cmdtokens[i]    = new qstring(128); // local tokens
         Console.argv[i] = new qstring(128); // console argvs
      }

      Console.args.createSize(1024); // congealed args
      
      cmdtokensinit = true;
   }
}

//
// C_clearCmdTokens
//
// haleyjd 07/05/10: clear all console tokens
//
static void C_clearCmdTokens()
{
   int i;

   for(i = 0; i < numtokensalloc; i++)
      cmdtokens[i]->clear();
}

//
// C_GetTokens
//
// Break up the command into tokens.
// haleyjd 07/05/10: rewritten to remove token length limitation
//
static void C_GetTokens(const char *command)
{
   const char *rover;
   bool quotemark = false;
   
   rover = command;
   
   C_initCmdTokens();
   C_clearCmdTokens();

   numtokens = 1;
   
   while(*rover == ' ') 
      rover++;
   
   if(!*rover)     // end of string already
   {
      numtokens = 0;
      return;
   }

   while(*rover)
   {
      if(*rover == '"')
      {
         quotemark = !quotemark;
      }
      else if(*rover == ' ' && !quotemark)   // end of token
      {
         // only if the current one actually contains something
         if (*(cmdtokens[numtokens - 1]->constPtr()))
            C_nextCmdToken();
      }
      else // add char to line
      {
         // 01/12/09: repaired by fraggle to fix mystery sprintf issue on linux
         // 07/05/10: rewritten to use qstrings
         *cmdtokens[numtokens - 1] += *rover;
      }

      rover++;
   }
}

//
// C_RunIndivTextCmd
//
// called once a typed command line has been broken
// into individual commands (multiple commands on one
// line are allowed with the ';' character)
//
static void C_RunIndivTextCmd(const char *cmdname)
{
   command_t *command;
   
   // cut off leading spaces
   while(*cmdname == ' ')
      cmdname++;
   
   // break into tokens
   C_GetTokens(cmdname);
   
   // find the command being run from the first token.
   command = C_GetCmdForName(cmdtokens[0]->constPtr());
   
   if(!numtokens) 
      return; // no command
   
   if(!command) // no _command_ called that
   {
      // alias?
      alias_t *alias;
      if((alias = C_GetAlias(cmdtokens[0]->constPtr())))
      {
         // save the options into cmdoptions
         // QSTR_FIXME: wtf is this doin' anyway? o_O
         cmdoptions = (char *)cmdname + cmdtokens[0]->length();
         C_RunAlias(alias);
      }
      else         // no alias either
         C_Printf("unknown command: '%s'\n", cmdtokens[0]->constPtr());
   }
   else
   {
      // run the command (buffer it)
      C_RunCommand(command, cmdname + cmdtokens[0]->length());
   }
}

enum
{
   CCF_CANNOTSET = 0,    // cannot set anything.
   CCF_CANSETVAR = 0x01, // can set the variable's normal value
   CCF_CANSETDEF = 0x02, // can set the variable's default
   
   CCF_CANSETALL = CCF_CANSETVAR | CCF_CANSETDEF
};

//
// C_CheckFlags
//
// Check the flags of a command to see if it should be run or not.
//
static int C_CheckFlags(command_t *command, const char **errormsg)
{
   int returnval = CCF_CANSETALL;
   
   if(!command->variable || !command->variable->v_default || 
      (command->flags & cf_handlerset))
   {
      // remove default flag if there is no default.
      returnval &= ~CCF_CANSETDEF;
   }

   // check the flags
   *errormsg = nullptr;
   
   if((command->flags & cf_notnet) && (netgame && !demoplayback))
      *errormsg = "not available in netgame";
   if((command->flags & cf_netonly) && !netgame && !demoplayback)
      *errormsg = "only available in netgame";
   if((command->flags & cf_server) && consoleplayer && !demoplayback
      && Console.cmdtype != c_netcmd)
      *errormsg = "for server only";
   if((command->flags & cf_level) && gamestate != GS_LEVEL)
      *errormsg = "can be run in levels only";
   
   // net-sync critical variables are usually critical to demo sync too
   if((command->flags & cf_netvar) && demoplayback)
      *errormsg = "not during demo playback";

   if(*errormsg)
   {
      // haleyjd 08/15/10: allow setting of default values in certain conditions   
      if(demoplayback && (returnval & CCF_CANSETDEF))
      {
         *errormsg = "will take effect after demo ends";
         returnval = CCF_CANSETDEF; // we can still modify the default value.
      }
      else
         returnval = CCF_CANNOTSET; // we can't set anything to this variable now.
   }
   
   return returnval;
}

//
// C_doErrorMsg
//
// haleyjd 08/15/10: I had to separate out printing of errors from C_CheckFlags since
// that function can now be called just for checking a variable's state and not just
// for setting it.
//
static void C_doErrorMsg(command_t *command, const char *errormsg)
{
   if(errormsg)
   {
      C_Printf("%s: %s\n", command->name, errormsg);
      if(menuactive)
         MN_ErrorMsg("%s", errormsg);
   }
}

//
// C_RunCommand
//
// Call with the command to run and the command-line options.
// Buffers the commands which will be run later.
//
void C_RunCommand(command_t *command, const char *options)
{
   // do not run straight away, we might be in the middle of rendering
   C_BufferCommand(Console.cmdtype, command, options, Console.cmdsrc);
   
   Console.cmdtype = c_typed;  // reset to typed command as default
}

//
// C_DoRunCommand
//
// actually run a command. Same as C_RunCommand only instant.
//
static void C_DoRunCommand(command_t *command, const char *options)
{
   int i;
   const char *errormsg = nullptr;
   
   C_GetTokens(options);
   
   for(i = 0; i < numtokensalloc; i++)
      *(Console.argv[i]) = *cmdtokens[i];

   Console.argc = numtokens;
   Console.command = command;
   
   // perform checks
   
   // check through the tokens for variable names
   for(i = 0; i < Console.argc; i++)
   {
      char c = *(Console.argv[i]->constPtr());

      if(c == '%' || c == '$') // variable
      {
         command_t *variable;
         
         variable = C_GetCmdForName(Console.argv[i]->constPtr() + 1);

         if(!variable || !variable->variable)
         {
            C_Printf("unknown variable '%s'\n", Console.argv[i]->constPtr() + 1);
            // clear for next time
            Console.cmdtype = c_typed; 
            Console.cmdsrc  = consoleplayer;
            return;
         }
         
         *Console.argv[i] = (c == '%' ? C_VariableValue(variable->variable) :
                                        C_VariableStringValue(variable->variable));
      }
   }
   
   C_ArgvtoArgs();                 // build Console.args
   
   // actually do this command
   switch(command->type)
   {
   case ct_command:
      // not to be run ?
      if(C_CheckFlags(command, &errormsg) == CCF_CANNOTSET || C_Sync(command))
      {
         C_doErrorMsg(command, errormsg);
         Console.cmdtype = c_typed;
         Console.cmdsrc = consoleplayer; 
         return;
      }
      if(command->handler)
         command->handler();
      else
         C_Printf(FC_ERROR "error: no command handler for %s\n", 
                  command->name);
      break;
      
   case ct_constant:
      C_EchoValue(command);
      break;
      
   case ct_variable:
      C_SetVariable(command);
      break;
      
   default:
      C_Printf(FC_ERROR "unknown command type %i\n", command->type);
      break;
   }
   
   Console.cmdtype = c_typed; 
   Console.cmdsrc  = consoleplayer;   // clear for next time
}

//
// C_ArgvtoArgs
//
// Take all the argvs and put them all together in the args string
//
// CONSOLE_FIXME: Horrible.
// haleyjd 07/08/09: rewritten to avoid undefined sprintf behavior.
//
static void C_ArgvtoArgs(void)
{
   int i, n;
 
   for(i = 0; i < Console.argc; i++)
   {
      if(!Console.argv[i]->length())       // empty string
      {
         for(n = i; n < Console.argc - 1; n++)
            Console.argv[n+1]->copyInto(*(Console.argv[n]));

         Console.argc--; 
         i--;
      }
   }
   
   Console.args.clear();

   for(i = 0; i < Console.argc; ++i)
   {
      // haleyjd: use qstring to avoid sprintf problems and to be secure
      Console.args << *Console.argv[i] << ' ';
   }
}

//
// C_QuotedArgvToArgs
//
// Return a string of all the argvs linked together, but with each
// argv in quote marks "
//
static const char *C_QuotedArgvToArgs()
{
   int i;
   static qstring returnbuf;

   returnbuf.clearOrCreate(1024);
   
   // haleyjd: use qstring to eliminate undefined sprintf behavior
   for(i = 0; i < Console.argc; ++i)
   {
      returnbuf << '"' << *Console.argv[i] << "\" ";
   }
   
   return returnbuf.constPtr();
}

//
// C_Sync
//
// See if the command needs to be sent to other computers
// to maintain sync and do so if neccesary
//
static int C_Sync(command_t *command)
{
   if(command->flags & cf_netvar)
   {
      // dont get stuck repeatedly sending the same command
      if(Console.cmdtype != c_netcmd)
      {                               // send to sync
         C_SendCmd(CN_BROADCAST, command->netcmd,
                   "%s", C_QuotedArgvToArgs());
         return true;
      }
   }
   
   return false;
}

//
// C_RunTextCmd
//
// Execute a compound command (with or without ;'s)
//
void C_RunTextCmd(const char *command)
{
   bool quotemark = false;  // for " quote marks
   char *sub_command = nullptr;
   const char *rover;

   for(rover = command; *rover; rover++)
   {
      if(*rover == '\"')    // quotemark
      {
         quotemark = !quotemark;
         continue;
      }
      if(*rover == ';' && !quotemark)  // command seperator and not in string 
      {
         // found sub-command
         // use recursion to run the subcommands
         
         // left
         // copy sub command, alloc slightly more than needed
         sub_command = ecalloc(char *, 1, rover-command+3); 
         strncpy(sub_command, command, rover-command);
         sub_command[rover-command] = '\0';   // end string
         
         C_RunTextCmd(sub_command);
         
         // right
         C_RunTextCmd(rover+1);
         
         // leave to the other function calls (above) to run commands
         efree(sub_command);
         return;
      }
   }
   
   // no sub-commands: just one
   // so run it
   
   C_RunIndivTextCmd(command);
}

//
// C_VariableValue
//
// Get the literal value of a variable (ie. "1" not "on")
//
const char *C_VariableValue(variable_t *variable)
{
   static qstring value;
   void *loc;
   const char *dummymsg = nullptr;
   
   value.clearOrCreate(1024);
   
   if(!variable)
      return "";

   // haleyjd 08/15/10: if only the default can be set right now, return the
   // default value rather than the current value.
   if(C_CheckFlags(variable->command, &dummymsg) == CCF_CANSETDEF)
      loc = variable->v_default;
   else
      loc = variable->variable;
   
   switch(variable->type)
   {
   case vt_int:
      value.Printf(0, "%d", *(int *)loc);
      break;

   case vt_toggle:
      // haleyjd 07/05/10
      value.Printf(0, "%d", (int)(*(bool *)loc));
      break;
      
   case vt_string:
      // haleyjd 01/24/03: added null check from prboom
      if(*(char **)variable->variable)
         value = *(char **)loc;
      else
         return "null";
      break;

   case vt_chararray:
      // haleyjd 03/13/06: static string support
      value = (const char *)loc;
      break;

   case vt_float:
      // haleyjd 04/21/10: implemented vt_float
      value.Printf(0, "%+.5f", *(double *)loc);
      break;
      
   default:
      I_Error("C_VariableValue: unknown variable type %d\n", variable->type);
   }
   
   return value.constPtr();
}

//
// C_VariableStringValue
//
// Get the string value (ie. "on" not "1")
//
const char *C_VariableStringValue(variable_t *variable)
{
   static qstring value;
   int stateflags;
   const char *dummymsg = nullptr;
   
   value.clearOrCreate(1024);
   
   if(!variable) 
      return "";

   if(!variable->variable)
      return "null";

   // haleyjd: get the "check" flags
   stateflags = C_CheckFlags(variable->command, &dummymsg);
   
   // does the variable have alternate 'defines' ?
   if((variable->type == vt_int || variable->type == vt_toggle) && variable->defines)
   {
      // print defined value
      // haleyjd 03/17/02: needs rangechecking
      int varValue = 0;
      int valStrIndex = 0;
      void *loc;

      // if this is a variable that can only currently have its default set, use its
      // default value rather than its actual value.
      if(stateflags == CCF_CANSETDEF)
         loc = variable->v_default;
      else
         loc = variable->variable;

      if(variable->type == vt_int)
         varValue = *((int *)loc);
      else if(variable->type == vt_toggle)
         varValue = (int)(*(bool *)loc);

      valStrIndex = varValue - variable->min;

      if(valStrIndex < 0 || valStrIndex > variable->max - variable->min)
         return "";
      else
         value = variable->defines[valStrIndex];
   }
   else
   {
      // print literal value
      value = C_VariableValue(variable);
   }
   
   return value.constPtr();
}


// echo a value eg. ' "alwaysmlook" is "1" '

static void C_EchoValue(command_t *command)
{
   C_Printf("\"%s\" is \"%s\"\n", command->name,
            C_VariableStringValue(command->variable) );
}

// is a string a number?

static bool isnum(const char *text)
{
   // haleyjd 07/05/10: skip over signs here
   if(strlen(text) > 1 && (*text == '-' || *text == '+'))
      ++text;

   for(; *text; text++)
   {
      if(*text > '9' || *text < '0')
         return false;
   }
   return true;
}

//
// C_ValueForDefine
//
// Take a string and see if it matches a define for a variable. Replace with the
// literal value if so.
//
static const char *C_ValueForDefine(variable_t *variable, const char *s, int setflags)
{
   static qstring returnstr;

   returnstr.clearOrCreate(1024);

   // haleyjd 07/05/10: ALWAYS copy param to returnstr, or else strcpy will have
   // undefined behavior in C_SetVariable.
   returnstr = s;

   if(variable->defines)
   {
      for(int count = variable->min; count <= variable->max; count++)
      {
         if(!C_Strcmp(s, variable->defines[count-variable->min]))
         {
            returnstr.Printf(0, "%d", count);
            return returnstr.constPtr();
         }
      }
   }

   // special hacks for menu

   // haleyjd 07/05/10: support * syntax for setting default value
   if(!strcmp(s, "*") && variable->cfgDefault)
   {
      default_t *dp = variable->cfgDefault;

      switch(variable->type)
      {
      case vt_int:
         {
            int i;
            dp->methods->getDefault(dp, &i);
            returnstr.Printf(0, "%d", i);
         }
         break;
      case vt_toggle:
         {
            bool b;
            dp->methods->getDefault(dp, &b);
            returnstr.Printf(0, "%d", !!b);
         }
         break;
      case vt_float:
         {
            double f;
            dp->methods->getDefault(dp, &f);
            returnstr.Printf(0, "%f", f);
         }
         break;
      case vt_string:
         {
            char *def;
            dp->methods->getDefault(dp, &def);
            returnstr = def;
         }
         break;
      case vt_chararray:
         {
            // TODO
         }
         break;
      }

      return returnstr.constPtr();
   }
   
   if(variable->type == vt_int || variable->type == vt_toggle)    // int values only
   {
      int value;
      void *loc;

      // if we can only set the default right now, use the default value.
      if(setflags == CCF_CANSETDEF)
         loc = variable->v_default;
      else
         loc = variable->variable;

      if(variable->type == vt_int)
         value = *(int *)loc;
      else
         value = (int)(*(bool *)loc);

      if(!strcmp(s, "+"))     // increase value
      {
         value += 1;

         if(variable->max != UL && value > variable->max) 
            value = variable->max;
         
         returnstr.Printf(0, "%d", value);
         return returnstr.constPtr();
      }
      if(!strcmp(s, "-"))     // decrease value
      {
         value -= 1;
         
         if(variable->min != UL && value < variable->min)
            value = variable->min;
         
         returnstr.Printf(0, "%d", value);
         return returnstr.constPtr();
      }
      if(!strcmp(s, "/"))     // toggle value
      {
         if(variable->type == vt_int)
            value += 1;
         else
            value = !value;

         if(variable->max != UL && value > variable->max)
            value = variable->min; // wrap around

         returnstr.Printf(0, "%d", value);
         return returnstr.constPtr();
      }
      
      if(!isnum(s))
         return nullptr;
   }
   
   return returnstr.constPtr();
}

// haleyjd 08/30/09: local-origin netcmds need love too
#define cmd_setdefault \
   (Console.cmdtype == c_typed || \
    (Console.cmdtype == c_netcmd && Console.cmdsrc == consoleplayer))

//
// C_SetVariable
//
// Set a variable.
//
static void C_SetVariable(command_t *command)
{
   variable_t* variable;
   int size = 0;
   double fs = 0.0;
   const char *errormsg;
   const char *temp;
   int setflags;
   const char *varerror = nullptr;
   
   // cut off the leading spaces
   
   if(!Console.argc)     // asking for value
   {
      C_EchoValue(command);
      return;
   }

   // find out how we can change it.
   setflags = C_CheckFlags(command, &varerror);
   C_doErrorMsg(command, varerror); // show a message if one was set.

   if(setflags == CCF_CANNOTSET) 
      return; // can't set anything.
   
   // ok, set the value
   variable = command->variable;
   
   temp = C_ValueForDefine(variable, Console.argv[0]->constPtr(), setflags);
   
   if(temp)
      *Console.argv[0] = temp;
   else
   {
      C_Printf("not a possible value for '%s'\n", command->name);
      return;
   }

   switch(variable->type)
   {
   case vt_int:
   case vt_toggle:
      size = Console.argv[0]->toInt();
      break;
      
   case vt_string:
   case vt_chararray:
      size = static_cast<int>(Console.argv[0]->length());
      break;

   case vt_float:
      // haleyjd 04/21/10: vt_float
      fs = Console.argv[0]->toDouble(nullptr);
      break;
      
   default:
      return;
   }

   // check the min/max sizes
   
   errormsg = nullptr;
   
   // haleyjd 03/22/09: allow unlimited bounds
   // haleyjd 04/21/10: implement vt_float
   
   if(variable->type == vt_float)
   {
      // float requires a different check
      if(variable->dmax != UL && fs > variable->dmax)
         errormsg = "value too big";
      if(variable->dmin != UL && fs < variable->dmin)
         errormsg = "value too small";
   }
   else
   {
      if(variable->max != UL && size > variable->max)
         errormsg = "value too big";
      if(variable->min != UL && size < variable->min)
         errormsg = "value too small";
   }
   
   if(errormsg)
   {
      MN_ErrorMsg("%s", errormsg);
      C_Puts(errormsg);
      return;
   }
   
   // netgame sync: send command to other nodes
   if(C_Sync(command))
      return;
   
   // now set it
   // 5/8/99 set default value also
   // 16/9/99 cf_handlerset flag for variables set from
   // the handler instead

   // haleyjd 07/15/09: cmdtype, NOT cmdsrc in tests below!!!
   
   if(!(command->flags & cf_handlerset))
   {
      switch(variable->type)  // implicitly set the variable
      {
      case vt_int:
         if(setflags & CCF_CANSETVAR)
            *(int*)variable->variable = size;
         if((setflags & CCF_CANSETDEF) && cmd_setdefault) // default
            *(int*)variable->v_default = size;
         break;

      case vt_toggle:
         // haleyjd 07/05/10
         if(setflags & CCF_CANSETVAR)
            *(bool *)variable->variable = !!size;
         if((setflags & CCF_CANSETDEF) && cmd_setdefault) // default
            *(bool *)variable->v_default = !!size;
         break;
         
      case vt_string:
         if(setflags & CCF_CANSETVAR)
         {
            efree(*(char**)variable->variable);
            *(char**)variable->variable = Console.argv[0]->duplicate(PU_STATIC);
         }
         if((setflags & CCF_CANSETDEF) && cmd_setdefault)  // default
         {
            efree(*(char**)variable->v_default);
            *(char**)variable->v_default = Console.argv[0]->duplicate(PU_STATIC);
         }
         break;

      case vt_chararray:
         // haleyjd 03/13/06: static strings
         if(setflags & CCF_CANSETVAR)
         {
            memset(variable->variable, 0, variable->max + 1);
            Console.argv[0]->copyInto((char *)variable->variable, variable->max + 1);
         }
         if((setflags & CCF_CANSETDEF) && cmd_setdefault)
         {
            memset(variable->v_default, 0, variable->max+1);
            strcpy((char *)variable->v_default, Console.argv[0]->constPtr());
         }
         break;

      case vt_float:
         // haleyjd 04/21/10: implemented vt_float
         if(setflags & CCF_CANSETVAR)
            *(double *)variable->variable = fs;
         if((setflags & CCF_CANSETDEF) && cmd_setdefault)
            *(double *)variable->v_default = fs;
         break;
         
      default:
         I_Error("C_SetVariable: unknown variable type %d\n", variable->type);
      }
   }
   
   if(command->handler)          // run handler if there is one
      command->handler();
}

//=============================================================================
//
// Tab Completion
//

static qstring origkey;
static bool gotkey;
static command_t **tabs;
static int numtabsalloc; // haleyjd 07/25/10
static int numtabs = 0;
static int thistab = -1;

//
// CheckTabs
//
// haleyjd 07/25/10: Removed dangerous unchecked limit on tab completion.
//
static void CheckTabs()
{
   if(numtabs >= numtabsalloc)
   {
      numtabsalloc = numtabsalloc ? 2 * numtabsalloc : 128;
      tabs = erealloc(command_t **, tabs, numtabsalloc * sizeof(command_t *));
   }
}

//
// GetTabs
//
// given a key (eg. "r_sw"), will look through all the commands in the hash
// chains and gather all the commands which begin with this into a list 'tabs'
//
static void GetTabs(qstring &qkey)
{
   size_t pos, keylen;

   numtabs = 0;

   origkey.clearOrCreate(128);

   // remember input
   origkey = qkey;

   // find the first non-space character; if none, we can't do this
   if((pos = qkey.findFirstNotOf(' ')) == qstring::npos)
      return;

   // save the input from the first non-space character, and lowercase it
   origkey = qkey.bufferAt(pos);
   origkey.toLower();

   gotkey = true;

   keylen = origkey.length();

   // check each hash chain in turn

   for(command_t *browser : cmdroots)
   {
      // go through each link in this chain
      for(; browser; browser = browser->next)
      {
         if(!(browser->flags & cf_hidden) && // ignore hidden ones
            !origkey.strNCmp(browser->name, keylen))
         {
            // found a new tab
            CheckTabs();
            tabs[numtabs] = browser;
            numtabs++;
         }
      }
   }
}

//
// C_InitTab
//
// Reset the tab list 
//
void C_InitTab()
{
   numtabs = 0;

   origkey.clearOrCreate(100);

   gotkey = false;
   thistab = -1;
}

//
// C_NextTab
//
// Called when tab pressed. Get the next tab from the list.
//
qstring &C_NextTab(qstring &key)
{
   static qstring returnstr;
 
   returnstr.clearOrCreate(128);
   
   // get tabs if not done already
    if(!gotkey)
      GetTabs(key);
   
   // select next tab
   thistab = thistab == -1 ? 0 : thistab + 1;  

   if(thistab >= numtabs)
   {
      thistab = -1;
      return origkey;
   }
   else
   {   
      returnstr << tabs[thistab]->name << ' ';

      return returnstr;
   }
}

//
// C_PrevTab
//
// Called when shift-tab pressed. Get the previous tab from the list.
//
qstring &C_PrevTab(qstring &key)
{
   static qstring returnstr;

   returnstr.clearOrCreate(128);
   
   // get tabs if neccesary
   if(!gotkey)
      GetTabs(key);
   
   // select prev.
   thistab = thistab == -1 ? numtabs - 1 : thistab - 1;
   
   // check invalid
   if(thistab < 0)
   {
      thistab = -1;
      return origkey;
   }
   else
   {
      returnstr << tabs[thistab]->name << ' ';
      return returnstr;
   }
}

//=============================================================================
//
// Aliases
//

// haleyjd 04/14/03: removed limit and rewrote all code
alias_t aliases;
char *cmdoptions;       // command line options for aliases

//
// C_GetAlias
//
// Get an alias from a name
//
alias_t *C_GetAlias(const char *name)
{
   alias_t *alias = aliases.next;

   while(alias)
   {
      if(!strcmp(name, alias->name))
         return alias;
      alias = alias->next;
   }

   return nullptr;
}

//
// C_NewAlias
//
// Create a new alias, or use one that already exists
//
alias_t *C_NewAlias(const char *aliasname, const char *command)
{
   alias_t *alias;

   // search for an existing alias with this name
   if((alias = C_GetAlias(aliasname)))
   {
      efree(alias->command);
      alias->command = estrdup(command);
   }
   else
   {
      // create a new alias
      alias = estructalloc(alias_t, 1);
      alias->name = estrdup(aliasname);
      alias->command = estrdup(command);
      alias->next = aliases.next;
      aliases.next = alias;
   }

   return alias;
}

//
// C_RemoveAlias
//
// Remove an alias
//
void C_RemoveAlias(qstring *aliasname)
{
   alias_t *prev  = &aliases;
   alias_t *rover = aliases.next;
   alias_t *alias = nullptr;

   while(rover)
   {
      if(*aliasname == rover->name)
      {
         alias = rover;
         break;
      }

      prev = rover;
      rover = rover->next;
   }

   if(!alias)
   {
      C_Printf("unknown alias \"%s\"\n", aliasname->constPtr());
      return;
   }
   
   C_Printf("removing alias \"%s\"\n", aliasname->constPtr());
   
   // free alias data
   efree(alias->name);
   efree(alias->command);

   // unlink alias
   prev->next  = alias->next;
   alias->next = nullptr;

   // free the alias
   efree(alias);
}

// run an alias

static void C_RunAlias(alias_t *alias)
{
   // store command line for use in macro
   while(*cmdoptions == ' ')
      cmdoptions++;

   C_RunTextCmd(alias->command);   // run the command
}

//=============================================================================
//
// Command Bufferring
//

// new ticcmds can be built at any time including during the
// rendering process. The commands need to be buffered
// and run by the tickers instead of directly from the
// responders
//
// a seperate buffered command list is kept for each type
// of command (typed, script, etc)

// 15/11/99 cleaned up: now uses linked lists of bufferedcmd's
//              rather than a static array: no limit on
//              buffered commands and nicer code

struct bufferedcmd
{
   command_t *command;     // the command
   char *options;          // command line options
   int cmdsrc;             // source player
   bufferedcmd *next;      // next in list
};

struct cmdbuffer
{
   // nullptr once list empty
   bufferedcmd *cmdbuffer;
   int timer;   // tic timer to temporarily freeze executing of cmds
};

cmdbuffer buffers[C_CMDTYPES];

static void C_RunBufferedCommand(bufferedcmd *bufcmd)
{
   // run command
   // restore variables
   
   Console.cmdsrc = bufcmd->cmdsrc;

   C_DoRunCommand(bufcmd->command, bufcmd->options);
}

void C_BufferCommand(int cmtype, command_t *command, const char *options,
                     int cmdsrc)
{
   bufferedcmd *bufcmd;
   bufferedcmd *newbuf;
   
   // create bufferedcmd
   newbuf = estructalloc(bufferedcmd, 1);
   
   // add to new bufferedcmd
   newbuf->command = command;
   newbuf->options = estrdup(options);
   newbuf->cmdsrc = cmdsrc;
   newbuf->next = nullptr;            // is always at end of chain
   
   // no need to be buffered: run it now
   if(!(command->flags & cf_buffered) && buffers[cmtype].timer == 0)
   {
      Console.cmdtype = cmtype;
      C_RunBufferedCommand(newbuf);
      
      efree(newbuf->options);
      efree(newbuf);
      return;
   }
   
   // add to list now
   
   // select according to cmdtype
   bufcmd = buffers[cmtype].cmdbuffer;
   
   // list empty?
   if(!bufcmd)              // hook in
      buffers[cmtype].cmdbuffer = newbuf;
   else
   {
      // go to end of list
      for(; bufcmd->next; bufcmd = bufcmd->next);
      
      // point last to newbuf
      bufcmd->next = newbuf;
   }
}

void C_RunBuffer(int cmtype)
{
   bufferedcmd *bufcmd, *next;
   
   bufcmd = buffers[cmtype].cmdbuffer;
   
   while(bufcmd)
   {
      // check for delay timer
      if(buffers[cmtype].timer)
      {
         buffers[cmtype].timer--;
         break;
      }
      
      Console.cmdtype = cmtype;
      C_RunBufferedCommand(bufcmd);
      
      // save next before freeing
      
      next = bufcmd->next;
      
      // free bufferedcmd
      
      efree(bufcmd->options);
      efree(bufcmd);
      
      // go to next in list
      bufcmd = buffers[cmtype].cmdbuffer = next;
   }
}

void C_RunBuffers()
{
   int i;
   
   // run all buffers
   
   for(i = 0; i < C_CMDTYPES; i++)
      C_RunBuffer(i);
}

void C_BufferDelay(int cmdtype, int delay)
{
   buffers[cmdtype].timer += delay;
}

void C_ClearBuffer(int cmdtype)
{
   buffers[cmdtype].timer = 0;                     // clear timer
   buffers[cmdtype].cmdbuffer = nullptr;           // empty
}

        // compare regardless of font colour
static bool C_Strcmp(const char *pa, const char *pb)
{
   const unsigned char *a = (const unsigned char *)pa;
   const unsigned char *b = (const unsigned char *)pb;

   while(*a || *b)
   {
      // remove colour dependency
      if(*a >= 128)   // skip colour
      {
         a++; continue;
      }
      if(*b >= 128)
      {
         b++; continue;
      }
      // regardless of case also
      if(ectype::toUpper(*a) != ectype::toUpper(*b))
         return true;
      a++; b++;
   }
   
   return false;       // no difference in them
}

//=============================================================================
//
// Command hashing
//
// Faster look up of console commands

command_t *cmdroots[CMDCHAINS];

void C_AddCommand(command_t *command)
{
   unsigned int hash = D_HashTableKey(command->name) % CMDCHAINS;
   
   command->next = cmdroots[hash]; // hook it in at the start of
   cmdroots[hash] = command;       // the table
   
   // save the netcmd link
   if(command->flags & cf_netvar && command->netcmd == 0)
      C_Printf(FC_ERROR "C_AddCommand: cf_netvar without a netcmd (%s)\n", command->name);
   
   if(command->netcmd >= NUMNETCMDS || command->netcmd < 0)
      I_Error("Illegal netcmd index %d (must be positive and less than %d)\n",
              command->netcmd, NUMNETCMDS);
   c_netcmds[command->netcmd] = command;

   if(command->type == ct_variable || command->type == ct_constant)
   {
      // haleyjd 07/04/10: find default in config for cvars that have one
      command->variable->cfgDefault = M_FindDefaultForCVar(command->variable);

      // haleyjd 08/15/10: set variable's pointer to command
      command->variable->command = command;
   }
}

// add a list of commands terminated by one of type ct_end

void C_AddCommandList(command_t *list)
{
   for(; list->type != ct_end; list++)
      C_AddCommand(list);
}

// get a command from a string if possible
        
command_t *C_GetCmdForName(const char *cmdname)
{
   command_t *current;
   unsigned int hash;
   
   while(*cmdname == ' ')
      cmdname++;

   // start hashing
   
   hash = D_HashTableKey(cmdname) % CMDCHAINS;
   
   current = cmdroots[hash];
   while(current)
   {
      if(!strcasecmp(cmdname, current->name))
         return current;
      current = current->next;        // try next in chain
   }
   
   return nullptr;
}

/*
   haleyjd: Command Scripts
   From SMMU v3.30, these allow running command scripts from either
   files or buffered lumps -- very cool IMHO.
*/

// states for console script lexer
enum
{
   CSC_NONE,
   CSC_COMMENT,
   CSC_SLASH,
   CSC_COMMAND,
};

//
// haleyjd 02/12/04: rewritten to use DWFILE and qstring, and to
// formalize into a finite state automaton lexer. Removes several 
// bugs and problems, including static line length limit and failure 
// to run a command on the last line of the data.
//
void C_RunScript(DWFILE *dwfile)
{
   qstring qstr;
   int state = CSC_NONE;
   int c;

   // parse script
   while((c = dwfile->getChar()) != EOF)
   {
      // turn \r into \n for simplicity
      if(c == '\r')
         c = '\n';

      switch(state)
      {
      case CSC_COMMENT:
         if(c == '\n')        // end the comment now
            state = CSC_NONE;
         continue;

      case CSC_SLASH:
         if(c == '/')         // start the comment now
            state = CSC_COMMENT;
         else
            state = CSC_NONE; // ??? -- malformatted
         continue;

      case CSC_COMMAND:
         if(c == '\n' || c == '\f') // end of line - run command
         {
            Console.cmdtype = c_script;
            C_RunTextCmd(qstr.constPtr());
            C_RunBuffer(c_script);  // force to run now
            state = CSC_NONE;
         }
         else
            qstr += static_cast<char>(c);
         continue;
      }

      // CSC_NONE processing
      switch(c)
      {
      case ' ':  // ignore leading whitespace
      case '\t':
      case '\f':
      case '\n':
         continue;

      case '/': // possibly starting a comment
         state = CSC_SLASH;
         continue;

      default:  // anything else starts a command
         qstr.clear() << static_cast<char>(c);
         state = CSC_COMMAND;
         continue;
      }
   }

   if(state == CSC_COMMAND) // EOF on command line - run final command
   {
      Console.cmdtype = c_script;
      C_RunTextCmd(qstr.constPtr());
      C_RunBuffer(c_script);  // force to run now
   }
}

//
// C_RunScriptFromFile
//
// Opens the indicated file and runs it as a console script.
// haleyjd 02/12/04: rewritten to use new C_RunScript above.
//
void C_RunScriptFromFile(const char *filename)
{
   DWFILE dwfile;

   dwfile.openFile(filename, "r");

   if(!dwfile.isOpen())
   {
      C_Printf(FC_ERROR "Couldn't exec script '%s'\n", filename);
   }
   else
   {
      C_Printf("Executing script '%s'\n", filename);
      C_RunScript(&dwfile);
   }
}

//
// C_RunCmdLineScripts
//
// Called at startup to look for scripts to run specified via the -exec command
// line parameter.
//
void C_RunCmdLineScripts()
{
   int p;

   if((p = M_CheckParm("-exec")))
   {
      // the parms after p are console script names,
      // until end of parms or another - preceded parm
      
      bool file = true;
      
      while(++p < myargc)
      {
         if(*myargv[p] == '-')
            file = !strcasecmp(myargv[p], "-exec"); // allow multiple -exec
         else if(file)
         {
            qstring filename(myargv[p]);
            filename.addDefaultExtension(".csc").normalizeSlashes();
            C_RunScriptFromFile(filename.constPtr());
         }
      }
   }
}

// EOF
