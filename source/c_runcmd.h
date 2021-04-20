// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// Console Commands - Macros and Command Execution
//
//----------------------------------------------------------------------------

#ifndef C_RUNCMD_H__
#define C_RUNCMD_H__

#include "m_dllist.h"
#include "m_qstr.h"

// NETCODE_FIXME -- CONSOLE_FIXME -- CONFIG_FIXME: Commands and 
// variables need tweaks and extensions to fully support archiving in
// the configuration and possibly in savegames, and also telling what
// commands and variables are sync-critical. The main addition needed
// is the support of defaults for ALL archived console variables and
// a way to set a variable's value without changing its default.

struct command_t;
struct default_t;
struct variable_t;

//=============================================================================
//
// Macros
//

#define MAXTOKENS 64
#define CMDCHAINS 16

// zdoom _inspired_:

//
// CONSOLE_COMMAND
//
// Create a new console command, eg.
//        CONSOLE_COMMAND(say_something, cf_notnet)
//        {
//              C_Printf("hello!\n");
//        }
//
#define CONSOLE_COMMAND(name, flags)                     \
   static void Handler_ ## name(void);                   \
   static command_t Cmd_ ## name = { # name, ct_command, \
                  flags, nullptr, Handler_ ## name,      \
                  0, nullptr };                          \
   static CCmdRegistry regCmd ## name (&Cmd_ ## name);   \
   static void Handler_ ## name(void)

//
// CONSOLE_VARIABLE
//
// You must define the range of values etc. for the variable using the other 
// macros below. You must also provide a handler function even if it is just {}
//
#define CONSOLE_VARIABLE(name, variable, flags)                \
   static void Handler_ ## name(void);                         \
   static command_t Cmd_ ## name = { # name, ct_variable,      \
                   flags, &var_ ## variable, Handler_ ## name, \
                   0, nullptr };                               \
   static CCmdRegistry regCmd ## name (&Cmd_ ## name);         \
   static void Handler_ ## name(void)

//
// CONSOLE_NETCMD
//
// Same as CONSOLE_COMMAND, but sync-ed across network. When this command is 
// executed, it is run on all computers. You must assign your variable a unique
// netgame variable (list in c_net.h)
//
#define CONSOLE_NETCMD(name, flags, netcmd)              \
   static void Handler_ ## name(void);                   \
   static command_t Cmd_ ## name = { # name, ct_command, \
                  (flags) | cf_netvar, nullptr,          \
                  Handler_ ## name, netcmd, nullptr };   \
   static CCmdRegistry regCmd ## name (&Cmd_ ## name);   \
   static void Handler_ ## name()

//
// CONSOLE_NETVAR
//
// As for CONSOLE_VARIABLE, but for net, see above
//
#define CONSOLE_NETVAR(name, variable, flags, netcmd)      \
   static void Handler_ ## name(void);                     \
   static command_t Cmd_ ## name = { # name, ct_variable,  \
                   cf_netvar | (flags), &var_ ## variable, \
                   Handler_ ## name, netcmd, nullptr };    \
   static CCmdRegistry regCmd ## name (&Cmd_ ## name);     \
   static void Handler_ ## name(void)

//
// CONSOLE_CONST
//
// Create a constant. You must declare the variable holding the constant using
// the variable macros below.
//
#define CONSOLE_CONST(name, variable)                         \
    static command_t Cmd_ ## name = { # name, ct_constant, 0, \
       &var_ ## variable, nullptr, 0, nullptr };              \
    static CCmdRegistry regCmd ## name (&Cmd_ ## name);

        /*********** variable macros *************/

// Each console variable has a corresponding C variable.
// It also has a variable_t which contains data such as,
// a pointer to the variable, the range of values it can
// take, etc. These macros allow you to define variable_t's
// more easily.

//
// VARIABLE
//
// Basic VARIABLE macro. You must specify all the data needed.
//
#define VARIABLE(name, defaultvar, type, min, max, strings)  \
        variable_t var_ ## name = { &name, defaultvar,       \
                  type, min, max, strings, 0, 0, nullptr, nullptr};

//
// VARIABLE_INT
//
// Simpler macro for int. You do not need to specify the type.
//
#define VARIABLE_INT(name, defaultvar, min, max, strings)    \
        variable_t var_ ## name = variable_t::makeInt(&name, defaultvar, min, max, strings);

//
// VARIABLE_STRING
//
// Simplified to create strings: 'max' is the maximum string length
//
#define VARIABLE_STRING(name, defaultvar, max)               \
        variable_t var_ ## name = { &name, defaultvar,       \
                  vt_string, 0, max, nullptr, 0, 0, nullptr, nullptr};

//
// VARIABLE_CHARARRAY
//
// haleyjd 03/13/06: support static strings as cvars
//
#define VARIABLE_CHARARRAY(name, defaultvar, max)            \
        variable_t var_ ## name = { name, defaultvar,        \
                  vt_chararray, 0, max, nullptr, 0, 0, nullptr, nullptr};

//
// VARIABLE_BOOLEAN
//
// Boolean. Note that although the name here is boolean, the actual type is 
// int. 
// haleyjd 07/05/10: For real booleans, use vt_toggle type with VARIABLE_TOGGLE
//
#define VARIABLE_BOOLEAN(name, defaultvar, strings)          \
        variable_t var_ ## name = { &name, defaultvar,       \
                  vt_int, 0, 1, strings, 0, 0, nullptr, nullptr };

//
// VARIABLE_TOGGLE
//
// haleyjd: for actual C++ "bool" variables.
//
#define VARIABLE_TOGGLE(name, defaultvar, strings)           \
        variable_t var_ ## name = { &name, defaultvar,       \
                   vt_toggle, 0, 1, strings, 0, 0, nullptr, nullptr };

//
// VARIABLE_FLOAT
//
// haleyjd 04/21/10: support for vt_float
//
#define VARIABLE_FLOAT(name, defaultvar, min, max)           \
        variable_t var_ ## name = { &name, defaultvar,       \
                  vt_float, 0, 0, nullptr, min, max, nullptr, nullptr };

// basic variable_t creators for constants.

#define CONST_INT(name)                                      \
        variable_t var_ ## name = { &name, nullptr,          \
                  vt_int, -1, -1, nullptr, 0, 0, nullptr, nullptr };

#define CONST_STRING(name)                                   \
        variable_t var_ ## name = { &name, nullptr,          \
                  vt_string, -1, -1, nullptr, 0, 0, nullptr, nullptr };

//=============================================================================
//
// Enumerations
//

// cmdtype values
enum
{
  c_typed,        // typed at console
  c_menu,
  c_netcmd,
  c_script,	  // haleyjd: started by command script
  C_CMDTYPES
};

// command type
enum
{
  ct_command,
  ct_variable,
  ct_constant,
  ct_end
};

// command flags
enum
{
  cf_notnet       = 0x001, // not in netgames
  cf_netonly      = 0x002, // only in netgames
  cf_server       = 0x004, // server only 
  cf_handlerset   = 0x008, // if set, the handler sets the variable,
                           // not c_runcmd.c itself
  cf_netvar       = 0x010, // sync with other pcs
  cf_level        = 0x020, // only works in levels
  cf_hidden       = 0x040, // hidden in cmdlist
  cf_buffered     = 0x080, // buffer command: wait til all screen
                           // rendered before running command
  cf_allowblank   = 0x100, // string variable allows empty value
};

// variable types
enum
{
  vt_int,       // normal integer 
  vt_float,     // decimal
  vt_string,    // string
  vt_chararray, // char array -- haleyjd 03/13/06
  vt_toggle     // boolean (for real bool-type variables)
};

//=============================================================================
// 
// Structures
//

struct variable_t
{
   // Static type-safe factories
   template<typename T>
   static variable_t makeInt(T *target, T *defaultTarget, int min, int max,
                             const char **strings)
   {
      static_assert(sizeof(T) == sizeof(int), "Type T must be int-like");
      return { target, defaultTarget, vt_int, min, max, strings, 0, 0, nullptr, nullptr };
   }
   // Stupid boilerplate though
   template<typename T>
   static variable_t makeInt(T *target, std::nullptr_t, int min, int max, const char **strings)
   {
      static_assert(sizeof(T) == sizeof(int), "Type T must be int-like");
      return { target, nullptr, vt_int, min, max, strings, 0, 0, nullptr, nullptr };
   }

  void *variable;        // NB: for strings, this is char ** not char *
  void *v_default;       // the default 
  int type;              // vt_?? variable type: int, string
  int min;               // minimum value or string length
  int max;               // maximum value/length
  const char **defines;  // strings representing the value: eg "on" not "1"
  double dmin;           // haleyjd 04/21/10: min for double vars
  double dmax;           //                   max for double vars
  
  default_t *cfgDefault; // haleyjd 07/04/10: pointer to config default
  command_t *command;           // haleyjd 08/15/10: parent command
};

struct command_t
{
  const char *name;
  int type;              // ct_?? command type
  int flags;             // cf_??
  variable_t *variable;
  void (*handler)(void); // handler
  int netcmd;            // network command number
  command_t *next;       // for hashing
};

struct alias_t
{
  char *name;
  char *command;
  
  alias_t *next; // haleyjd 04/14/03
};

//=============================================================================
//
// Console Command Registering Class
//
// haleyjd 3/10/13: replaces need to call C_AddCommand for every command and
// variable manually. An instance is created by every console command and cvar
// macro pointing to the corresponding command_t structure.
//

class CCmdRegistry
{
protected:
   DLListItem<CCmdRegistry> links; // list links
   command_t *command;             // command to register
   
   static DLListItem<CCmdRegistry> *commands; // static list of commands

public:
   //
   // Constructor
   //
   // Register the command on the global list of commands. The list is walked
   // during program init (after all instances have constructed).
   //
   explicit CCmdRegistry(command_t *pCmd)
   {
      command = pCmd;
      links.insert(this, &commands);
   }

   static void AddCommands();
};

//=============================================================================
//
// Prototypes and Externs
//

//
// haleyjd 05/20/10
//
// Console state is now stored in the console_t structure.
// 
struct console_t
{
   int prev_height;    // previous height for interpolation
   int current_height; // current height of console
   int current_target; // target height of console
   bool showprompt;    // toggles input prompt on or off
   bool enabled;       // enabled state of console
   int cmdsrc;         // player source of current command being run
   int cmdtype;        // source type of command (console, menu, etc)
   command_t *command; // current command being run
   int  argc;          // number of argv's
   qstring   args;     // args as single string   
   qstring **argv;     // argument values to current command
   int numargvsalloc;  // number of arguments available to command parsing
};

extern console_t Console; // the one and only Console object

/***** command running ****/

void C_RunCommand(command_t *command, const char *options);
void C_RunTextCmd(const char *cmdname);

const char *C_VariableValue(variable_t *command);
const char *C_VariableStringValue(variable_t *command);

// haleyjd: the SMMU v3.30 script-running functions
// (with my fixes :P)

class DWFILE;
void C_RunScript(DWFILE *);
void C_RunScriptFromFile(const char *filename);

void C_RunCmdLineScripts(void);

/**** tab completion ****/

void C_InitTab(void);
qstring &C_NextTab(qstring &key);
qstring &C_PrevTab(qstring &key);

/**** aliases ****/

extern alias_t aliases; // haleyjd 04/14/03: changed to linked list
extern char *cmdoptions;

alias_t *C_NewAlias(const char *aliasname, const char *command);
void     C_RemoveAlias(qstring *aliasname);
alias_t *C_GetAlias(const char *name);

/**** command buffers ****/

void C_BufferCommand(int cmdtype, command_t *command,
                     const char *options, int cmdsrc);
void C_RunBuffers();
void C_RunBuffer(int cmtype);
void C_BufferDelay(int, int);
void C_ClearBuffer(int);

/**** hashing ****/

extern command_t *cmdroots[CMDCHAINS];   // the commands in hash chains

void C_AddCommand(command_t *command);
void C_AddCommandList(command_t *list);
void C_AddCommands();
command_t *C_GetCmdForName(const char *cmdname);

/***** define strings for variables *****/

extern const char *yesno[];
extern const char *onoff[];
extern const char *colournames[];
extern const char *textcolours[];
extern const char *skills[];

#endif

// EOF
