//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Console Network support 
//
// Network commands can be sent across netgames using 'C_SendCmd'. The
// command is transferred byte by byte, 1 per tic cmd, using the 
// chatchar variable (previously used for chat messages)
//
// By Simon Howard
//
// NETCODE_FIXME RED LEVEL ALERT!!! -- CONSOLE_FIXME
// The biggest problems are in here by far. While all the netcode needs
// an overhaul, the incredibly terrible way netcmds/net vars are sent
// and the fact that they cannot be recorded in demos is the most
// pressing problem and requires a complete rewrite of more or less
// everything in here. zdoom has an elegant solution to this problem,
// but it requires much more complicated network packet parsing.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "c_net.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "g_game.h"
#include "m_qstr.h"
#include "v_misc.h"

int       incomingdest[MAXPLAYERS];
qstring   incomingmsg[MAXPLAYERS];

command_t *c_netcmds[NUMNETCMDS];

/*
   haleyjd: fixed a bug here

   default_name was being set equal to a string on the C heap,
   and then free()'d in the player name console command handler --
   but of course free() is redefined to Z_Free(), and you can't do
   that -- similar to the bug that was crashing the savegame menu,
   but this one caused a segfault, and only occasionally, because
   of Z_Free's faulty assumption that a pointer will be in valid
   address space to test the zone id, even if its not a ptr to a
   zone block.
*/
char *default_name; // = "player";
int default_colour;

// basic chat char stuff: taken from hu_stuff.c and renamed

#define QUEUESIZE   1024

static char chatchars[QUEUESIZE];
static int  head = 0;
static int  tail = 0;

// haleyjd: this must be called from start-up -- see above.
void C_InitPlayerName(void)
{
   default_name = estrdup("player");
}


//
// C_queueChatChar() (used to be HU_*)
//
// Add an incoming character to the circular chat queue
//
// Passed the character to queue, returns nothing
//

void C_queueChatChar(unsigned char c)
{
   if (((head + 1) & (QUEUESIZE-1)) == tail)
      C_Printf("command unsent\n");
   else
   {
      chatchars[head++] = c;
      head &= QUEUESIZE-1;
   }
}

//
// C_dequeueChatChar() (used to be HU_*)
//
// Remove the earliest added character from the circular chat queue
//
// Passed nothing, returns the character dequeued
//
unsigned char C_dequeueChatChar(void)
{
   char c;

   if (head != tail)
   {
      c = chatchars[tail++];
      tail &= QUEUESIZE-1;
   }
   else
      c = 0;
   return c;
}

void C_SendCmd(int dest, int cmdnum, E_FORMAT_STRING(const char *s), ...)
{
   va_list args;
   char tempstr[500];

   va_start(args, s);  
   pvsnprintf(tempstr, sizeof(tempstr), s, args);
   va_end(args);

   s = tempstr;
  
   if(!netgame || demoplayback)
   {
      Console.cmdsrc = consoleplayer;
      Console.cmdtype = c_netcmd;
      C_RunCommand(c_netcmds[cmdnum], s);
      return;
   }

   C_queueChatChar(0); // flush out previous commands
   C_queueChatChar((unsigned char)(dest+1)); // the chat message destination
   C_queueChatChar((unsigned char)cmdnum);        // command num
   
   while(*s)
   {
      C_queueChatChar(*s);
      s++;
   }
   C_queueChatChar(0);
}

void C_NetInit(void)
{
   int i;

   for(i = 0; i < MAXPLAYERS; ++i)
   {
      incomingdest[i] = -1;
      incomingmsg[i].initCreate();
   }
}

static void C_DealWithChar(unsigned char c, int source);

void C_NetTicker(void)
{
   if(netgame && !demoplayback)      // only deal with chat chars in netgames
   {
      // check for incoming chat chars
      for(int i=0; i<MAXPLAYERS; i++)
      {
         if(!playeringame[i]) 
            continue;
         C_DealWithChar(players[i].cmd.chatchar,i);
      }
   }

   // run buffered commands essential for netgame sync
   C_RunBuffer(c_netcmd);
}

static void C_DealWithChar(unsigned char c, int source)
{
   int netcmdnum;
   
   if(c)
   {
      if(incomingdest[source] == -1)  // first char: the destination
         incomingdest[source] = c-1;
      else
         incomingmsg[source] += c; // append to string
   }
   else
   {
      if(incomingdest[source] != -1)        // end of message
      {
         if((incomingdest[source] == consoleplayer)
            || incomingdest[source] == CN_BROADCAST)
         {
            Console.cmdsrc = source;
            Console.cmdtype = c_netcmd;

            // the first byte is the command num
            netcmdnum = *(incomingmsg[source].constPtr());
            
            if(netcmdnum >= NUMNETCMDS || netcmdnum <= 0)
               C_Printf(FC_ERROR "unknown netcmd: %i\n", netcmdnum);
            else
            {
               // C_Printf("%s, %s", c_netcmds[netcmdnum].name,
               //          incomingmsg[source]+1);
               C_RunCommand(c_netcmds[netcmdnum],
                            incomingmsg[source].constPtr() + 1);
            }
         }
         incomingmsg[source].clear();
         incomingdest[source] = -1;
      }
   }
}

char *G_GetNameForMap(int episode, int map);

void C_SendNetData()
{
   C_SetConsole();

   // display message according to what we're about to do

   C_Printf(consoleplayer ?
            FC_HI "Please Wait" FC_NORMAL " Receiving game data..\n" :
            FC_HI "Please Wait" FC_NORMAL " Sending game data..\n");


   // go thru all hash chains, check for net sync variables
   for(command_t *command : cmdroots)
   {
      while(command)
      {
         if(command->type == ct_variable && command->flags & cf_netvar &&
            (consoleplayer == 0 || !(command->flags & cf_server)))
         {
            C_UpdateVar(command);
         }
         command = command->next;
      }
   }

   demo_insurance = 1;      // always use 1 in multiplayer

   if(consoleplayer == 0)      // if server, send command to warp to map
   {
      char tempstr[100];
      snprintf(tempstr, earrlen(tempstr), "map %s", startlevel);
      C_RunTextCmd(tempstr);
   }
}

//
//      Update a network variable
//
void C_UpdateVar(command_t *command)
{
   char tempstr[100];

   snprintf(tempstr, sizeof(tempstr), "\"%s\"", C_VariableValue(command->variable) );

   C_SendCmd(CN_BROADCAST, command->netcmd, "%s", tempstr);
}

// EOF
