// Emacs style mode select -*- C++ -*-
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
// Console commands
//
// basic console commands and variables for controlling
// the console itself.
// 
// By Simon Howard
//
// NETCODE_FIXME -- CONSOLE_FIXME
// There's a lot of cruft in here that can be done without.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_io.h"       // SoM 3/12/2002: strncasecmp
#include "gl/gl_vars.h"
#include "m_misc.h"
#include "m_random.h"
#include "v_misc.h"
#include "version.h"

// version hack

char *verdate_hack = (char *)version_date;
char *vername_hack = (char *)version_name;
char *vertime_hack = (char *)version_time;

               /************* constants *************/

// version
CONST_INT(version);
CONSOLE_CONST(version, version);

// version date
CONST_STRING(verdate_hack);
CONSOLE_CONST(ver_date, verdate_hack);

// version time -- haleyjd 02/15/02
CONST_STRING(vertime_hack);
CONSOLE_CONST(ver_time, vertime_hack);

// version name
CONST_STRING(vername_hack);
CONSOLE_CONST(ver_name, vername_hack);

                /************* aliases ***************/
CONSOLE_COMMAND(alias, 0)
{
   alias_t *alias;
   const char *temp;

   // haleyjd 04/14/03: rewritten
   
   if(!Console.argc)
   {
      // list em
      C_Printf(FC_HI"alias list:" FC_NORMAL "\n\n");
      
      alias = aliases.next;
      if(!alias)
      {
         C_Printf("(empty)\n");
      }
      else
      {
         while(alias)
         {
            C_Printf("\"%s\": \"%s\"\n", alias->name, alias->command);
            alias = alias->next;
         }
      }
      return;
   }
  
   if(Console.argc == 1)  // only one, remove alias
   {
      C_RemoveAlias(Console.argv[0]);
      return;
   }
   
   // find it or make a new one
   
   temp = Console.args.bufferAt(Console.argv[0]->length());
   
   // QSTR_FIXME: needs a routine
   while(*temp == ' ')
      temp++;
   
   C_NewAlias(Console.argv[0]->constPtr(), temp);
}

// %opt for aliases
CONST_STRING(cmdoptions);
CONSOLE_CONST(opt, cmdoptions);

// command list
CONSOLE_COMMAND(cmdlist, 0)
{
   command_t *current;
   int charnum = 33;
   int maxchar = 'z';

   // SoM: This could be a little better
   const char *mask = nullptr;
   unsigned int masklen = 0;

   // haleyjd 07/08/04: optional filter parameter -- the provided
   // character will be used to make the loop below run only for one
   // letter
   if(Console.argc == 1)
   {
      unsigned int len = static_cast<unsigned>(Console.argv[0]->length());

      if(len == 1)
         charnum = maxchar = Console.argv[0]->charAt(0);
      else
      {
         charnum = maxchar = Console.argv[0]->charAt(0);
         mask    = Console.argv[0]->constPtr();
         masklen = len;
      }
   }
   
   // list each command from the hash chains
   
   // 5/8/99 change: use hash table and 
   // alphabetical order by first letter
   // haleyjd 07/08/04: fixed to run for last letter

   for(; charnum <= maxchar; ++charnum) // go thru each char in alphabet
   {
      for(command_t *&cmdroot : cmdroots)
      {
         for(current = cmdroot; current; current = current->next)
         {
            if(current->name[0] == charnum &&
               (!mask || !strncasecmp(current->name, mask, masklen)) &&
               !(current->flags & cf_hidden))
            {
               C_Printf("%s\n", current->name);
            }
         }
      }
   }
}

// console height

VARIABLE_INT(c_height,  nullptr,                   20, 200, nullptr);
CONSOLE_VARIABLE(c_height, c_height, 0) {}

// console speed

VARIABLE_INT(c_speed,   nullptr,                   1, 200, nullptr);
CONSOLE_VARIABLE(c_speed, c_speed, 0) {}

// echo string to console

CONSOLE_COMMAND(echo, 0)
{
   C_Puts(Console.args.constPtr());
}

// delay in console

CONSOLE_COMMAND(delay, 0)
{
   C_BufferDelay(Console.cmdtype, Console.argc ? Console.argv[0]->toInt() : 1);
}

// flood the console with crap
// .. such a great and useful command

CONSOLE_COMMAND(flood, 0)
{
  int a;

  for(a = 0; a < 300; a++)
    C_Printf("%c\n", a%64 + 32);
}

CONSOLE_COMMAND(quote, 0) {}

// haleyjd: dumplog command to write out the console to file

CONSOLE_COMMAND(dumplog, 0)
{
   if(!Console.argc)
      C_Printf("usage: dumplog filename\n");
   else
      C_DumpMessages(Console.argv[0]);
}

// haleyjd 09/07/03: true console logging commands

CONSOLE_COMMAND(openlog, 0)
{
   if(!Console.argc)
      C_Printf("usage: openlog filename\n");
   else
      C_OpenConsoleLog(Console.argv[0]);
}

CONSOLE_COMMAND(closelog, 0)
{
   C_CloseConsoleLog();
}


// SoM: omg why didn't we have this before?
CONSOLE_COMMAND(cvarhelp, 0)
{
   command_t  *current;
   variable_t *var;
   const char *name;

   if(Console.argc != 1)
   {
      C_Printf("Usage: cvarhelp <variablename>\n"
               "Outputs a list of possible values for the given variable\n");
      return;
   }

   name = Console.argv[0]->constPtr();

   // haleyjd 07/05/10: use hashing!
   current = C_GetCmdForName(name);

   if(current && current->type == ct_variable && !(current->flags & cf_hidden))
   {
      var = current->variable;
      default_t *def = current->variable->cfgDefault; // haleyjd 07/05/10: print defaults

      switch(var->type)
      {
      case vt_int:
         if(var->defines && var->min <= var->max)
         {
            C_Printf("Possible values for '%s':\n", name);
            for(int count = var->min; count <= var->max; count++)
            {
               C_Printf("%s\n", var->defines[count - var->min]);
            }

            if(def)
            {
               C_Printf("Default value: %s\n", 
                        var->defines[def->defaultvalue_i - var->min]);
            }
         }
         else
         {
            // haleyjd 04/21/10: respect use of UL
            if(var->min == UL && var->max == UL)
            {
               C_Printf("'%s' accepts any integer value\n", name);
            }
            else if(var->min == UL)
            {
               C_Printf("Value range for '%s':\n Any integer <= %d\n", 
                        name, var->max);
            }
            else if(var->max == UL)
            {
               C_Printf("Value range for '%s':\n Any integer >= %d\n",
                        name, var->min);
            }
            else
            {
               C_Printf("Value range for '%s':\n %d through %d\n", 
                        name, var->min, var->max);
            }

            if(def)
               C_Printf("Default value: %d\n", def->defaultvalue_i);
         }
         break;

      case vt_toggle:
         // haleyjd 07/05/10: for true boolean variables
         if(var->defines)
         {
            C_Printf("Possible values for '%s':\n", name);
            for(int count = var->min; count <= var->max; count++)
               C_Printf(" %s\n", var->defines[count - var->min]);

            if(def)
            {
               C_Printf("Default value: %s\n", 
                        var->defines[def->defaultvalue_b]);
            }
         }
         else
         {
            C_Printf("'%s' is a boolean value (0 or 1)\n", name);
            if(def)
               C_Printf("Default value: %d\n", (int)def->defaultvalue_b);
         }
         break;

      case vt_float:
         // haleyjd 04/21/10: implemented vt_float
         if(var->dmin == UL && var->dmax == UL)
         {
            C_Printf("'%s' accepts any float value\n", name);
         }
         else if(var->dmin == UL)
         {
            C_Printf("Value range for '%s':\n any float <= %f\n", 
                     name, var->dmax);
         }
         else if(var->dmax == UL)
         {
            C_Printf("Value range for '%s':\n any float >= %f\n",
                     name, var->dmin);
         }
         else
         {
            C_Printf("Value range for '%s':\n %f through %f\n", 
                     name, var->dmin, var->dmax);
         }

         if(def)
            C_Printf("Default value: %f\n", def->defaultvalue_f);
         break;

      default:
         if(var->max != UL)
         {
            C_Printf("Value for '%s':\n String of max %d length\n", 
                     name, var->max);
         }
         else
            C_Printf("Value for '%s':\n Unlimited-length string\n", name);

         if(def)
            C_Printf("Default value:\n \"%s\"\n", def->defaultvalue_s);
         break;
      }

      return;
   }

   C_Printf("Variable %s not found\n", name);
}

CONSOLE_COMMAND(c_popup, 0)
{
   C_InstaPopup();
}

//=============================================================================
//
// Add Commands
//

// Static list head pointer for CCmdRegistry instances
DLListItem<CCmdRegistry> *CCmdRegistry::commands;

//
// CCmdRegistry::AddCommands
//
// Static method; called from C_AddCommands at startup. CCmdRegistry instances
// have all added themselves to the class's static instance list (in arbitrary
// order). Run down the list and add every command.
//
void CCmdRegistry::AddCommands()
{
   DLListItem<CCmdRegistry> *item = commands;

   while(item)
   {
      C_AddCommand((*item)->command);
      item = item->dllNext;
   }
}

// Special command adding functions that still need to be called:
extern void G_AddChatMacros();
extern void G_AddAutoloadFiles();
extern void G_AddCompat();
extern void G_CreateAxisActionVars();
extern void P_AddEventVars();

//
// C_AddCommands
//
// Invoked directly from D_DoomSetup as of 08/28/13.
//
void C_AddCommands()
{
   // Add global commands through the registry
   CCmdRegistry::AddCommands();

   // Some commands are created dynamically:
   G_AddChatMacros();
   G_AddAutoloadFiles();
   G_AddCompat();
   G_CreateAxisActionVars();
   P_AddEventVars();
}

#if 0
static cell AMX_NATIVE_CALL sm_version(AMX *amx, cell *params)
{
   return version;
}

AMX_NATIVE_INFO ccmd_Natives[] =
{
   {"_EngineVersion", sm_version },
   { nullptr, nullptr }
};
#endif

// EOF

