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
//      DOOM Network game communication and protocol,
//      all OS independend parts.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_englsh.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_net.h"
#include "doomstat.h"
#include "e_player.h"
#include "e_things.h"
#include "f_wipe.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hal/i_timer.h"
#include "m_random.h"
#include "mn_engin.h"
#include "i_net.h"
#include "i_video.h"
#include "p_partcl.h"
#include "p_skin.h"
#include "r_draw.h"
#include "v_misc.h"
#include "v_video.h"
#include "version.h"

doomcom_t  *doomcom;        
doomdata_t *netbuffer; // points inside doomcom

//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// maketic is the tick that hasn't had control made for it yet
// nettics[] has the maketics for all players 
//
// a gametic cannot be run until nettics[] > gametic for all players
//
#define RESENDCOUNT 10
#define PL_DRONE    0x80    /* bit flag in doomdata->player */

static ticcmd_t localcmds[BACKUPTICS];

ticcmd_t    netcmds[MAXPLAYERS][BACKUPTICS];
static int  nettics[MAXNETNODES];
static bool nodeingame[MAXNETNODES];      // set false as nodes leave game
static bool remoteresend[MAXNETNODES];    // set when local needs tics
static int  resendto[MAXNETNODES];        // set when remote needs tics
static int  resendcount[MAXNETNODES];
static int  nodeforplayer[MAXPLAYERS];

int        maketic;
static int skiptics;
int        ticdup;         
static int maxsend;               // BACKUPTICS/(2*ticdup)-1

void D_ProcessEvents(); 
void G_BuildTiccmd(ticcmd_t *cmd, player_t &p, playerinput_t &input, bool handlechatchar);
void D_DoAdvanceDemo();

static bool       reboundpacket;
static doomdata_t reboundstore;

//
// ExpandTics
//
static int ExpandTics(int low)
{
   int delta;
   
   delta = low - (maketic & 0xff);
   
   if(delta >= -64 && delta <= 64)
      return (maketic & ~0xff) + low;
   if(delta > 64)
      return (maketic & ~0xff) - 256 + low;
   if(delta < -64)
      return (maketic & ~0xff) + 256 + low;
   
   I_Error("ExpandTics: strange value %i at maketic %i\n", low, maketic);

   return 0;
}

//
// HSendPacket
//
static void HSendPacket(int node, int flags)
{
   netbuffer->checksum = (uint32_t)flags;
   
   if(!node)
   {
      reboundstore = *netbuffer;
      reboundpacket = true;
      return;
   }

   if(demoplayback || netbot)
      return;

   if(!netgame)
      I_Error("Tried to transmit to another node\n");

   doomcom->command    = CMD_SEND;
   doomcom->remotenode = node;
   
   I_NetCmd();
}

//
// HGetPacket
// Returns false if no packet is waiting
//
static bool HGetPacket()
{       
   if(reboundpacket)
   {
      *netbuffer = reboundstore;
      doomcom->remotenode = 0;
      reboundpacket = false;
      return true;
   }
   
   if(!G_RealNetGame())
      return false;

   doomcom->command = CMD_GET;
   if(!I_NetCmd())
      return false;
   
   if(doomcom->remotenode == -1)
      return false;
   
   // haleyjd 08/25/11: length & checksum not handled here any more
   
   return true;
}

//
// GetPackets
//
static void GetPackets()
{
   int         netconsole;
   int         netnode;
   ticcmd_t    *src, *dest;
   int         realend;
   int         realstart;
   
   while(HGetPacket())
   {
      if(netbuffer->checksum & NCMD_SETUP)
         continue;           // extra setup packet
      
      netconsole = netbuffer->player & ~PL_DRONE;
      netnode = doomcom->remotenode;
      
      // to save bytes, only the low byte of tic numbers are sent
      // Figure out what the rest of the bytes are
      realstart = ExpandTics(netbuffer->starttic);           
      realend   = realstart + netbuffer->numtics;
      
      // check for exiting the game
      if(netbuffer->checksum & NCMD_EXIT)
      {
         if(!nodeingame[netnode])
            continue;
         nodeingame[netnode] = false;
         playeringame[netconsole] = false;
         doom_printf("%s left the game", players[netconsole].name);
         
         // sf: remove the players mobj
         // spawn teleport flash
         
         if(gamestate == GS_LEVEL)
         {
            Mobj *tflash;

            tflash = P_SpawnMobj(players[netconsole].mo->x,
                                 players[netconsole].mo->y,
                                 players[netconsole].mo->z + 
                                    GameModeInfo->teleFogHeight,
                                 E_SafeThingName(GameModeInfo->teleFogType));
	      
            tflash->momx = players[netconsole].mo->momx;
            tflash->momy = players[netconsole].mo->momy;
            if(drawparticles)
            {
               tflash->flags2 |= MF2_DONTDRAW;
               P_DisconnectEffect(players[netconsole].mo);
            }
            players[netconsole].mo->remove();
         }
         if(demorecording)
            G_CheckDemoStatus();

         continue;
      }

      // check for a remote game kill
      if(netbuffer->checksum & NCMD_KILL)
         I_Error("Killed by network driver\n");
      
      nodeforplayer[netconsole] = netnode;
      
      // check for retransmit request
      if(resendcount[netnode] <= 0  && (netbuffer->checksum & NCMD_RETRANSMIT))
      {
         resendto[netnode] = ExpandTics(netbuffer->retransmitfrom);
         resendcount[netnode] = RESENDCOUNT;
      }
      else
         resendcount[netnode]--;
      
      // check for out of order / duplicated packet           
      if(realend == nettics[netnode])
         continue;
      
      if(realend < nettics[netnode])
         continue;
      
      // check for a missed packet
      if(realstart > nettics[netnode])
      {
         // stop processing until the other system resends the missed tics
         remoteresend[netnode] = true;
         continue;
      }
      
      // update command store from the packet
      int start;
         
      remoteresend[netnode] = false;
         
      start = nettics[netnode] - realstart;               
      src = &netbuffer->d.cmds[start];
         
      while(nettics[netnode] < realend)
      {
         dest = &netcmds[netconsole][nettics[netnode]%BACKUPTICS];
         nettics[netnode]++;
         *dest = *src;
         src++;
      }
   }
}

int gametime;

//
// NetUpdate
//
// Builds ticcmds for console player,
// sends out a packet
//
void NetUpdate()
{
   int nowtime;
   int newtics;
   int realstart;
   int gameticdiv;
   
   // check time
   nowtime = i_haltimer.GetTime() / ticdup;
   newtics = nowtime - gametime;
   gametime = nowtime;
   
   if(newtics <= 0)   // nothing new to update
   {
      GetPackets();
      return;
   }
   
   if(skiptics <= newtics)
   {
      newtics -= skiptics;
      skiptics = 0;
   }
   else
   {
      skiptics -= newtics;
      newtics = 0;
   }
   
   netbuffer->player = consoleplayer;
   
   // build new ticcmds for console player
   gameticdiv = gametic / ticdup;

   for(int i = 0; i < newtics; i++)
   {
      I_StartTic();
      D_ProcessEvents();
      if(maketic - gameticdiv >= BACKUPTICS / 2 - 1)
         break; // can't hold any more
      
      G_BuildTiccmd(&localcmds[maketic%BACKUPTICS], players[consoleplayer], g_input, true);
      ++maketic;
   }
  
   if(singletics)
      return; // singletic update is syncronous
  
   // send the packet to the other nodes
   for(int i = 0; i < doomcom->numnodes; i++)
   {
      if(nodeingame[i])
      {
         netbuffer->starttic = realstart = resendto[i];
         netbuffer->numtics = maketic - realstart;
         if(netbuffer->numtics > BACKUPTICS)
            I_Error("NetUpdate: netbuffer->numtics > BACKUPTICS\n");
         
         resendto[i] = maketic - doomcom->extratics;
         
         for(int j = 0; j < netbuffer->numtics; j++)
            netbuffer->d.cmds[j] = localcmds[(realstart + j) % BACKUPTICS];
         
         if(remoteresend[i])
         {
            netbuffer->retransmitfrom = nettics[i];
            HSendPacket(i, NCMD_RETRANSMIT);
         }
         else
         {
            netbuffer->retransmitfrom = 0;
            HSendPacket(i, 0);
         }
      }
   }
   
   // listen for other packets
   GetPackets();
}

/*
//
// D_KickPlayer
//
void D_KickPlayer(int playernum)
{
   HSendPacket(nodeforplayer[playernum], NCMD_KILL);
}
*/

//
// CheckAbort
//
static void CheckAbort()
{
   int stoptic;
   
   stoptic = i_haltimer.GetTime() + 2; 
   
   while(i_haltimer.GetTime() < stoptic)
      if(I_CheckAbort())
         I_ExitWithMessage("Network game synchronisation aborted.\n");
}

//
// D_InitPlayers
//
// sf: init players, set names, skins, colours etc
//
// NETCODE_FIXME: Completely wrong. This data should be transmitted
// as cvars or something similar, like in zdoom. This must be removed
// and replaced by something more general which allows players to have
// their proper names, colors, and skins in netgames. This shows how
// ridiculous the current system really is, if nothing else.
//
static void D_InitPlayers()
{
   // haleyjd 04/10/10: brought up out of defunct console net init for now
   players[consoleplayer].colormap = default_colour;
   strncpy(players[consoleplayer].name, default_name, 20);

   for(int i = 0; i < MAXPLAYERS; i++)
   {
      // FIXME / TODO: BRAINDEAD!
      if(i != consoleplayer)
      {
         sprintf(players[i].name, "player %i", i+1);
         players[i].colormap = i % TRANSLATIONCOLOURS;
      }

      // PCLASS_FIXME: subject to the NETCODE_FIXME above; only allows
      // player class to be set to default. Must change!
      players[i].pclass = E_PlayerClassForName(GameModeInfo->defPClassName);
      players[i].skin = P_GetDefaultSkin(&players[i]);
   }
}

//
// D_ArbitrateNetStart
//
static void D_ArbitrateNetStart()
{
   bool gotinfo[MAXNETNODES];
   
   autostart = true;
   memset(gotinfo, 0, sizeof(gotinfo));

   if(doomcom->consoleplayer)
   {
      usermsg("Listening for network start info... (ESC to cancel)");
      while(1)
      {
         C_Update(); // haleyjd 01/14/12: update the screen
         CheckAbort();
         if(!HGetPacket())
            continue;
         if(netbuffer->checksum & NCMD_SETUP)
         {
            bool dm;

            usermsg("Received %d %d\n",
                    netbuffer->retransmitfrom, netbuffer->starttic);
            // FIXME: various insufficient variable sizes
            startskill   = (skill_t)(netbuffer->retransmitfrom & 15);
            dm           = !!((netbuffer->retransmitfrom & 0xc0) >> 6);
            nomonsters   = (netbuffer->retransmitfrom & 0x20) > 0;
            respawnparm  = (netbuffer->retransmitfrom & 0x10) > 0;

            d_startlevel.map = netbuffer->starttic & 0x3f;
            d_startlevel.episode = 1 + (netbuffer->starttic >> 6);

            if(dm)
               DefaultGameType = GameType = gt_dm;

            G_ReadOptions(netbuffer->d.data);
            break;
         }
      }
   }
   else
   {
      int numnodesgotten = 0;
      usermsg("Sending network start info...");

      G_ScrambleRand();
      rngseed = rngseed & 255;

      do
      {
         CheckAbort();
         for(int i = 0; i < doomcom->numnodes; i++)
         {
            netbuffer->retransmitfrom = startskill;
            if(GameType == gt_dm)
               netbuffer->retransmitfrom |= (1 << 6);
            if(nomonsters)
               netbuffer->retransmitfrom |= 0x20;
            if(respawnparm)
               netbuffer->retransmitfrom |= 0x10;
            // FIXME: not large enough for Heretic!
            netbuffer->starttic = (d_startlevel.episode - 1) * 64 + d_startlevel.map;
            netbuffer->player = version;

#ifdef RANGECHECK
            if(GAME_OPTION_SIZE > sizeof(netbuffer->d.data))
               I_Error("D_ArbitrateNetStart: GAME_OPTION_SIZE"
                       " too large w.r.t. BACKUPTICS\n");
#endif

            G_WriteOptions(netbuffer->d.data);    // killough 12/98
            
            // killough 5/2/98: Always write the maximum number of tics.
            netbuffer->numtics = BACKUPTICS;
            
            HSendPacket(i, NCMD_SETUP);
         }

         for(int i = 10; i && HGetPacket(); i--)
         {
            if((netbuffer->player & 0x7f) < MAXNETNODES)
               gotinfo[netbuffer->player & 0x7f] = true;
         }

         numnodesgotten = 1;
         for(int i = 1; i < doomcom->numnodes; i++)
         {
            if(gotinfo[i])
               ++numnodesgotten;
         }
      }
      while(numnodesgotten < doomcom->numnodes);
   }
   
   // NETCODE_FIXME: See note above about D_InitPlayers
   D_InitPlayers();   
}

extern int viewangleoffset;

//
// D_CheckNetGame
//
// Works out player numbers among the net participants
//
void D_CheckNetGame()
{
   for(int i = 0; i < MAXNETNODES; i++)
   {
      nodeingame[i] = false;
      nettics[i] = 0;
      remoteresend[i] = false;        // set when local needs tics
      resendto[i] = 0;                // which tic to start sending
   }
   
   // I_InitNetwork sets doomcom and netgame
   I_InitNetwork();

   D_InitNetGame();
   
   D_InitPlayers();      
   
   C_NetInit();
   atexit(D_QuitNetGame);       // killough
}

void D_InitNetGame()
{
   if(doomcom->id != DOOMCOM_ID)
      I_Error("Doomcom buffer invalid!\n");
   
   netbuffer = &doomcom->data;
   consoleplayer = displayplayer = doomcom->consoleplayer;
   
   if(netgame)
      D_ArbitrateNetStart();
   
   // read values out of doomcom
   ticdup = doomcom->ticdup;
   maxsend = BACKUPTICS/(2*ticdup)-1;
   if(maxsend<1)
      maxsend = 1;
  
   for(int i = 0; i < doomcom->numplayers; i++)
      playeringame[i] = true;
   for(int i = 0; i < doomcom->numnodes; i++)
      nodeingame[i] = true;
  
   usermsg("player %i of %i (%i nodes)",
           consoleplayer+1, doomcom->numplayers, doomcom->numnodes);
}


//
// D_QuitNetGame
//
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame()
{
   if(!G_RealNetGame() || !usergame || consoleplayer == -1)
      return;
  
   // send a bunch of packets for security
   netbuffer->player = consoleplayer;
   netbuffer->numtics = 0;

   for(int i = 0; i < 4; i++)
   {
      for(int j = 1; j < doomcom->numnodes; j++)
      {
         if(nodeingame[j])
            HSendPacket(j, NCMD_EXIT);
      }
      i_haltimer.Sleep(15);
   }  
}

//
// TryRunTics
//

// sf 18/9/99: split into 2 functions. TryRunTics will run all of the
//             independent tickers, ie. menu, console. RunGameTics runs
//             all of the game tickers and includes adapting, etc.
//             This removes the problem of game slowdown after one pc
//             crashes in netgames, but also ensures the menu and
//             console run at the right speeds during demo timing and
//             changing of the game speed.

//
// NETCODE_FIXME: Possibly undo SMMU changes to main loop functions
// to re-simplify and eliminate possible bugs or redundancies. zdoom
// doesn't do things this way, so I question the necessity of some
// of these subloops.
//

//
// NETCODE_FIXME: Need to handle stuff besides ticcmds too.
//

// haleyjd 01/04/2010
bool d_fastrefresh;
bool d_interpolate;

int  frametics[4];
int  frameon;
int  frameskip[4];
int  oldnettics;
int  opensocket_count = 0;
bool opensocket;

extern bool advancedemo;

//
// RunGameTics
//
// Run new game tics.
//
static bool RunGameTics()
{
   static int  oldentertic;
   int         lowtic;
   int         entertic;
   int         realtics;
   int         availabletics;
   int         counts;
   int         numplaying;

   // get real tics            
   entertic = i_haltimer.GetTime() / ticdup;
   realtics = entertic - oldentertic;
   oldentertic = entertic;
  
   // get available tics
   NetUpdate();
      
   lowtic = D_MAXINT;
   numplaying = 0;
   for(int i = 0; i < doomcom->numnodes; i++)
   {
      if(nodeingame[i])
      {
         numplaying++;
         if(nettics[i] < lowtic)
            lowtic = nettics[i];
      }
   }
   availabletics = lowtic - gametic/ticdup;
   
   // decide how many tics to run
   if(realtics < availabletics-1)
      counts = realtics+1;
   else if(realtics < availabletics)
      counts = realtics;
   else
      counts = availabletics;
  
   // haleyjd 09/07/10: enhanced d_fastrefresh w/early return when no tics to run
   if(counts <= 0 && d_fastrefresh && !singletics && !timingdemo) // 10/03/10: not in timedemos!
      return false;

   if(counts < 1)
      counts = 1;
   
   frameon++;
   
   if(!demoplayback && !netbot)
   {   
      int pnum;

      // ideally nettics[0] should be 1 - 3 tics above lowtic
      // if we are consistantly slower, speed up time
      for(pnum = 0; pnum < MAXPLAYERS; pnum++)
      {
         if(playeringame[pnum])
            break;
      }

      // the key player does not adapt
      if(consoleplayer != pnum)
      {
         if(nettics[0] <= nettics[nodeforplayer[pnum]])
         {
            gametime--;
         }
         frameskip[frameon&3] = (oldnettics > nettics[nodeforplayer[pnum]]);
         oldnettics = nettics[0];
         if(frameskip[0] && frameskip[1] && frameskip[2] && frameskip[3])
         {
            skiptics = 1;
         }
      }
   }
   
   // singletic update ?
   // sf: moved here from d_main.c
   // as it seemed more appropriate
   // also to keep the menu/console
   // tickers running at normal speed

   // NETCODE_FIXME: fraggle change #1

   if(singletics)
   {
      I_StartTic();
      D_ProcessEvents();
      G_BuildTiccmd(&netcmds[consoleplayer][maketic%BACKUPTICS], players[consoleplayer], g_input, true);
      if(advancedemo)
         D_DoAdvanceDemo();
      G_Ticker();
      gametic++;
      maketic++;
      return true;
   }

   // sf: reorganised to stop doom locking up

   // NETCODE_FIXME: fraggle change #2
   
   if(lowtic < gametic/ticdup + counts)         // no more loops
   {
      NetUpdate();

      opensocket_count += realtics;
      opensocket = opensocket_count >= 20;    // if no tics to run
      // for 20 tics then declare an open socket
      
      if(opensocket)  // dont let the pc freeze
      {
         I_StartTic();           // allow keyboard input
         D_ProcessEvents();
      }
      
      // Sleep until a tic is available, so we don't hog the CPU.
      i_haltimer.Sleep(1);

      return false;
   }
   
   opensocket_count = 0;
   opensocket = 0;

   // run the count * ticdup tics
   while(counts--)
   {
      for(int i = 0; i < ticdup; i++)
      {
         if(gametic/ticdup > lowtic)
            I_Error("gametic>lowtic\n");
         if(advancedemo)
            D_DoAdvanceDemo();
         i_haltimer.SaveMS();
         G_Ticker();
         gametic++;
         
         // modify command for duplicated tics

         if(i != ticdup-1)
         {
            int buf = (gametic / ticdup) % BACKUPTICS; 

            for(int j = 0; j < MAXPLAYERS; j++)
            {
               auto cmd = &netcmds[j][buf];
               cmd->chatchar = 0;
               if(cmd->buttons & BT_SPECIAL)
                  cmd->buttons = 0;
            }
         }
      }

      NetUpdate();   // check for new console commands
   }

   return true;
}

void TryRunTics()
{
   static int oldentertic;
   int entertic, realtics;
   int game_advanced = 0;

   // Loop until we have done some kind of useful work.  If no
   // game tics are run, RunGameTics() will send the game to
   // sleep in 1ms increments until we advance.

   do
   {
      entertic = i_haltimer.GetTime() / ticdup;
      realtics = entertic - oldentertic;
      oldentertic = entertic;

      // sf: run the menu and console regardless of 
      // game time. to prevent lockups

      // NETCODE_FIXME: fraggle change #3

      I_StartTic();        // run these here now to get keyboard
      D_ProcessEvents();   // input for console/menu

      for(int i = 0; i < realtics; i++)   // run tics
      {
         // all independent tickers here
         // ioanch: skip some tickers during windowless fast demos. It has been
         // noticed by profiling that these functions add significant time with-
         // out being gameplay relevant.
         if(!D_noWindow() || !fastdemo)
            MN_Ticker();
         C_Ticker();
         if(!D_noWindow() || !fastdemo)
            V_FPSTicker();
      }

      // run the game tickers
      game_advanced = RunGameTics();
   } 
   while(!d_fastrefresh && realtics <= 0 && !game_advanced);
}

/////////////////////////////////////////////////////
//
// Console Commands
//

/*
CONSOLE_COMMAND(kick, cf_server)
{
   if(!Console.argc)
   {
      C_Printf("usage: kick <playernum>\n"
               " use playerinfo to find playernum\n");
      return;
   }
   D_KickPlayer(atoi(Console.argv[0]));
}
*/

CONSOLE_COMMAND(playerinfo, 0)
{
   int i;
   
   for(i = 0; i < MAXPLAYERS; ++i)
      if(playeringame[i])
         C_Printf("%i: %s\n",i, players[i].name);
}

/*
//
// NETCODE_FIXME: See notes above about kicking out instead of 
// dropping to console.
//
CONSOLE_COMMAND(disconnect, cf_netonly)
{
   D_QuitNetGame();
   C_SetConsole();
}
*/
 
VARIABLE_TOGGLE(d_fastrefresh, NULL, onoff);
CONSOLE_VARIABLE(d_fastrefresh, d_fastrefresh, 0) {}

VARIABLE_TOGGLE(d_interpolate, NULL, onoff);
CONSOLE_VARIABLE(d_interpolate, d_interpolate, 0) {}

//----------------------------------------------------------------------------
//
// $Log: d_net.c,v $
// Revision 1.12  1998/05/21  12:12:09  jim
// Removed conditional from net code
//
// Revision 1.11  1998/05/16  09:40:57  jim
// formatted net files, installed temp switch for testing Stan/Lee's version
//
// Revision 1.10  1998/05/12  12:46:08  phares
// Removed OVER_UNDER code
//
// Revision 1.9  1998/05/05  16:28:58  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.8  1998/05/03  23:40:34  killough
// Fix net consistency problems, using G_WriteOptions/G_Readoptions
//
// Revision 1.7  1998/04/15  14:41:36  stan
// Now encode startepisode-1 in netbuffer->starttic because episode 4
// caused an overflow in the byte used for starttic.  Don't know how it
// ever worked!
//
// Revision 1.6  1998/04/13  10:40:48  stan
// Now synch up all items identified by Lee Killough as essential to
// game synch (including bobbing, recoil, rngseed).  Commented out
// code in g_game.c so rndseed is always set even in netgame.
//
// Revision 1.5  1998/02/15  02:47:36  phares
// User-defined keys
//
// Revision 1.4  1998/01/26  19:23:07  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  05:47:28  killough
// add missing right brace
//
// Revision 1.2  1998/01/19  16:38:10  rand
// Added dummy comments to be reomved later
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
