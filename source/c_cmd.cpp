// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2001 James Haley
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
#include "d_io.h"  // SoM 3/12/2002: strncasecmp
#include "c_io.h"
#include "c_runcmd.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "m_random.h"
#include "version.h"
#include "a_small.h" // haleyjd

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
      C_RemoveAlias(&Console.argv[0]);
      return;
   }
   
   // find it or make a new one
   
   temp = QStrBufferAt(&Console.args, QStrLen(&Console.argv[0]));
   
   // QSTR_FIXME: needs a routine
   while(*temp == ' ')
      temp++;
   
   C_NewAlias(QStrConstPtr(&Console.argv[0]), temp);
}

// %opt for aliases
CONST_STRING(cmdoptions);
CONSOLE_CONST(opt, cmdoptions);

// command list
CONSOLE_COMMAND(cmdlist, 0)
{
   command_t *current;
   int i;
   int charnum = 33;
   int maxchar = 'z';

   // SoM: This could be a little better
   const char *mask = NULL;
   unsigned int masklen = 0;

   // haleyjd 07/08/04: optional filter parameter -- the provided
   // character will be used to make the loop below run only for one
   // letter
   if(Console.argc == 1)
   {
      unsigned int len = QStrLen(&Console.argv[0]);

      if(len == 1)
         charnum = maxchar = QStrCharAt(&Console.argv[0], 0);
      else
      {
         charnum = maxchar = QStrCharAt(&Console.argv[0], 0);
         mask    = QStrConstPtr(&Console.argv[0]);
         masklen = len;
      }
   }
   
   // list each command from the hash chains
   
   // 5/8/99 change: use hash table and 
   // alphabetical order by first letter
   // haleyjd 07/08/04: fixed to run for last letter

   for(; charnum <= maxchar; ++charnum) // go thru each char in alphabet
   {
      for(i = 0; i < CMDCHAINS; ++i)
      {
         for(current = cmdroots[i]; current; current = current->next)
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

VARIABLE_INT(c_height,  NULL,                   20, 200, NULL);
CONSOLE_VARIABLE(c_height, c_height, 0) {}

// console speed

VARIABLE_INT(c_speed,   NULL,                   1, 200, NULL);
CONSOLE_VARIABLE(c_speed, c_speed, 0) {}

// echo string to console

CONSOLE_COMMAND(echo, 0)
{
   C_Puts(QStrConstPtr(&Console.args));
}

// delay in console

CONSOLE_COMMAND(delay, 0)
{
   C_BufferDelay(Console.cmdtype, Console.argc ? QStrAtoi(&Console.argv[0]) : 1);
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
      C_DumpMessages(&Console.argv[0]);
}

// haleyjd 09/07/03: true console logging commands

CONSOLE_COMMAND(openlog, 0)
{
   if(!Console.argc)
      C_Printf("usage: openlog filename\n");
   else
      C_OpenConsoleLog(&Console.argv[0]);
}

CONSOLE_COMMAND(closelog, 0)
{
   C_CloseConsoleLog();
}


// SoM: omg why didn't we have this before?
CONSOLE_COMMAND(cvarhelp, 0)
{
   command_t *current;
   variable_t *var;
   int count;
   const char *name;
   default_t *def;

   if(Console.argc != 1)
   {
      C_Printf("Usage: cvarhelp <variablename>\n"
               "Outputs a list of possible values for the given variable\n");
      return;
   }

   name = QStrConstPtr(&Console.argv[0]);

   // haleyjd 07/05/10: use hashing!
   current = C_GetCmdForName(name);

   if(current && current->type == ct_variable && !(current->flags & cf_hidden))
   {
      var = current->variable;
      def = current->variable->cfgDefault; // haleyjd 07/05/10: print defaults

      switch(var->type)
      {
      case vt_int:
         if(var->defines && var->min <= var->max)
         {
            C_Printf("Possible values for '%s':\n", name);
            for(count = var->min; count <= var->max; count++)
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
            for(count = var->min; count <= var->max; count++)
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

        /******** add commands *******/

// command-adding functions in other modules

extern void       AM_AddCommands(void);        // am_color
extern void    Cheat_AddCommands(void);        // m_cheat
extern void        D_AddCommands(void);        // d_main   -- haleyjd
extern void        E_AddCommands(void);        // e_cmd    -- haleyjd
extern void        G_AddCommands(void);        // g_cmd
extern void   G_Bind_AddCommands(void);        // g_bind   -- haleyjd
extern void      G_DMAddCommands(void);        // g_dmflag -- haleyjd
extern void       HU_AddCommands(void);        // hu_stuff
extern void        I_AddCommands(void);        // i_system
extern void       MN_AddCommands(void);        // mn_menu
extern void      net_AddCommands(void);        // d_net
extern void        P_AddCommands(void);        // p_cmd
extern void P_AddGenLineCommands(void);        // p_genlin -- haleyjd
extern void       PE_AddCommands(void);        // p_enemy  -- haleyjd
extern void        R_AddCommands(void);        // r_main
extern void        S_AddCommands(void);        // s_sound
extern void     S_AddSeqCommands(void);        // s_sndseq -- haleyjd
extern void       ST_AddCommands(void);        // st_stuff
extern void        V_AddCommands(void);        // v_misc
extern void        W_AddCommands(void);        // w_levels -- haleyjd

#ifndef EE_NO_SMALL_SUPPORT
extern void       SM_AddCommands(void);        // a_small  -- haleyjd
#endif

void C_AddCommands()
{
  C_AddCommand(version);
  C_AddCommand(ver_date);
  C_AddCommand(ver_time); // haleyjd
  C_AddCommand(ver_name);
  
  C_AddCommand(c_height);
  C_AddCommand(c_speed);
  C_AddCommand(cmdlist);
  C_AddCommand(delay);
  C_AddCommand(alias);
  C_AddCommand(opt);
  C_AddCommand(echo);
  C_AddCommand(flood);
  C_AddCommand(quote);
  C_AddCommand(dumplog); // haleyjd
  C_AddCommand(openlog);
  C_AddCommand(closelog);

  // SoM: I can never remember the values for a console variable
  C_AddCommand(cvarhelp);
  
  // add commands in other modules
  Cheat_AddCommands();
  G_AddCommands();
  HU_AddCommands();
  I_AddCommands();
  net_AddCommands();
  P_AddCommands();
  R_AddCommands();
  S_AddCommands();
  S_AddSeqCommands();
  ST_AddCommands();
  V_AddCommands();
  MN_AddCommands();
  AM_AddCommands();
  PE_AddCommands();  // haleyjd
  G_Bind_AddCommands();
  
#ifndef EE_NO_SMALL_SUPPORT
  SM_AddCommands();
#endif
  
  G_DMAddCommands();
  E_AddCommands();
  P_AddGenLineCommands();
  W_AddCommands();
  D_AddCommands();
}

#ifndef EE_NO_SMALL_SUPPORT
static cell AMX_NATIVE_CALL sm_version(AMX *amx, cell *params)
{
   return version;
}

AMX_NATIVE_INFO ccmd_Natives[] =
{
   {"_EngineVersion", sm_version },
   { NULL, NULL }
};
#endif

// EOF

