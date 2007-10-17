// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
// DESCRIPTION:
//
//  Mid-level network code
//
// NETCODE_FIXME: Stubified during port to Windows. This is where
// mid-level networking code should go. Code here will need to act as
// a go-between for the code in d_net.c and code in an eventual Win32-
// specific (or general BSD) sockets module where UDP and possibly IPX
// are supported. Both zdoom and Quake 2 have sockets code that could
// easily be adapted to work for Eternity. Quake 2 is a preferred 
// reference source because of its GPL license.
//
//-----------------------------------------------------------------------------

#include "SDL_net.h"

#include "../z_zone.h"  /* memory allocation wrappers -- killough */

#include "../doomstat.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../d_event.h"
#include "../d_net.h"
#include "../m_argv.h"

#include "../i_net.h"

void    NetSend(void);
boolean NetListen(void);

//
// NETWORKING
//

void (*netget)(void);
void (*netsend)(void);

// haleyjd: new functions for anarkavre's WinMBF netcode
static int DOOMPORT = 8626;

static UDPsocket udpsocket;
static UDPpacket *packet;

static IPaddress sendaddress[MAXNETNODES];

/*
unsigned short host_to_net16(unsigned int value)
{
   union
   {
      unsigned short s;
      char b[2];
   } data;
   
   SDLNet_Write16(value, data.b);
   
   return data.s;
}

unsigned short net_to_host16(unsigned int value)
{
   unsigned short s = value;
   
   return SDLNet_Read16(&s);
}

unsigned long host_to_net32(unsigned int value)
{
   union
   {
      unsigned long l;
      char b[4];
   } data;
   
   SDLNet_Write32(value, data.b);
   
   return data.l;
}

unsigned long net_to_host32(unsigned int value)
{
   unsigned long l = value;
   
   return SDLNet_Read32(&l);
}
*/

// haleyjd: end new functions for anark's netcode

//
// PacketSend
//
void PacketSend(void)
{
   int c;
   //doomdata_t *sw;
   
   /*
   sw = (doomdata_t *)packet->data;
   
   SDLNet_Write32(netbuffer->checksum, &sw->checksum);
   sw->player         = netbuffer->player;
   sw->retransmitfrom = netbuffer->retransmitfrom;
   sw->starttic       = netbuffer->starttic;
   sw->numtics        = netbuffer->numtics;
   
   for(c = 0; c < netbuffer->numtics; ++c)
   {
      sw->cmds[c].forwardmove = netbuffer->cmds[c].forwardmove;
      sw->cmds[c].sidemove    = netbuffer->cmds[c].sidemove;
      SDLNet_Write16(netbuffer->cmds[c].angleturn,   &(sw->cmds[c].angleturn));
      SDLNet_Write16(netbuffer->cmds[c].consistancy, &(sw->cmds[c].consistancy));
      sw->cmds[c].chatchar    = netbuffer->cmds[c].chatchar;
      sw->cmds[c].buttons     = netbuffer->cmds[c].buttons;
      SDLNet_Write16(netbuffer->cmds[c].look, &(sw->cmds[c].look)); // haleyjd
   }
   */

   byte *rover = (byte *)packet->data;

   SDLNet_Write32(netbuffer->checksum, rover);
   rover += sizeof(netbuffer->checksum);

   *rover++ = netbuffer->player;
   *rover++ = netbuffer->retransmitfrom;
   *rover++ = netbuffer->starttic;
   *rover++ = netbuffer->numtics;

   for(c = 0; c < netbuffer->numtics; ++c)
   {
      *rover++ = netbuffer->cmds[c].forwardmove;
      *rover++ = netbuffer->cmds[c].sidemove;
      SDLNet_Write16(netbuffer->cmds[c].angleturn, rover);
      rover += sizeof(netbuffer->cmds[c].angleturn);
      SDLNet_Write16(netbuffer->cmds[c].consistancy, rover);
      rover += sizeof(netbuffer->cmds[c].consistancy);
      *rover++ = netbuffer->cmds[c].chatchar;
      *rover++ = netbuffer->cmds[c].buttons;
      SDLNet_Write16(netbuffer->cmds[c].look, rover);
      rover += sizeof(netbuffer->cmds[c].look);
   }
   
   packet->len     = doomcom->datalength;
   packet->address = sendaddress[doomcom->remotenode];

   if(!SDLNet_UDP_Send(udpsocket, -1, packet))
      I_Error("Error sending packet: %s", SDLNet_GetError());
}

//
// PacketGet
//
void PacketGet(void)
{
   int i, c, packets_read;
   //doomdata_t *sw;
   byte *rover;
   
   packets_read = SDLNet_UDP_Recv(udpsocket, packet);
   
   if(packets_read < 0)
      I_Error("Error reading packet: %s", SDLNet_GetError());
   
   if(packets_read == 0)
   {
      doomcom->remotenode = -1;
      return;
   }
   
   for(i = 0; i < doomcom->numnodes; ++i)
   {
      if(packet->address.host == sendaddress[i].host && 
         packet->address.port == sendaddress[i].port)
         break;
   }
   
   if(i == doomcom->numnodes)
   {
      doomcom->remotenode = -1;
      return;
   }
   
   doomcom->remotenode = i;
   doomcom->datalength = packet->len;
   
   //sw = (doomdata_t *)packet->data;
   rover = (byte *)packet->data;
   
   netbuffer->checksum = SDLNet_Read32(rover);
   rover += sizeof(netbuffer->checksum);
   netbuffer->player         = *rover++;
   netbuffer->retransmitfrom = *rover++;
   netbuffer->starttic       = *rover++;
   netbuffer->numtics        = *rover++;
   
   for(c = 0; c < netbuffer->numtics; ++c)
   {
      netbuffer->cmds[c].forwardmove = *rover++;
      netbuffer->cmds[c].sidemove    = *rover++;
      netbuffer->cmds[c].angleturn   = SDLNet_Read16(rover);
      rover += sizeof(netbuffer->cmds[c].angleturn);
      netbuffer->cmds[c].consistancy = SDLNet_Read16(rover);
      rover += sizeof(netbuffer->cmds[c].consistancy);
      netbuffer->cmds[c].chatchar    = *rover++;
      netbuffer->cmds[c].buttons     = *rover++;
      netbuffer->cmds[c].look        = SDLNet_Read16(rover);
      rover += sizeof(netbuffer->cmds[c].look);
   }
}

//
// I_QuitNetwork
//
// haleyjd: from anarkavre's WinMBF netcode
//
void I_QuitNetwork(void)
{
   if(packet)
   {
      SDLNet_FreePacket(packet);
      packet = NULL;
   }
   
   if(udpsocket)
   {
      SDLNet_UDP_Close(udpsocket);
      udpsocket = NULL;
   }
   
   SDLNet_Quit();
}

//
// I_InitNetwork
//
void I_InitNetwork(void)
{
   /*   
   // set up the singleplayer doomcom
   
   singleplayer.id = DOOMCOM_ID;
   singleplayer.numplayers = singleplayer.numnodes = 1;
   singleplayer.deathmatch = false;
   singleplayer.consoleplayer = 0;
   singleplayer.extratics=0;
   singleplayer.ticdup=1;
   
   // set up for network
   
   // parse network game options,
   //  -net <consoleplayer> <host> <host> ...
   // single player game
   doomcom = &singleplayer;
   netgame = false;
   return;
   */
   int i, p;
   
   doomcom = calloc(1, sizeof(*doomcom));
   
   // set up for network
   i = M_CheckParm("-dup");
   if(i && i < myargc-1)
   {
      doomcom->ticdup = myargv[i+1][0]-'0';
      if (doomcom->ticdup < 1)
         doomcom->ticdup = 1;
      if (doomcom->ticdup > 9)
         doomcom->ticdup = 9;
   }
   else
      doomcom-> ticdup = 1;
	
   if(M_CheckParm ("-extratic"))
      doomcom-> extratics = 1;
   else
      doomcom-> extratics = 0;

   p = M_CheckParm ("-port");
   if(p && p<myargc-1)
   {
      DOOMPORT = atoi (myargv[p+1]);
      usermsg("Using alternative port %i\n", DOOMPORT);
   }

   // parse network game options,
   //  -net <consoleplayer> <host> <host> ...
   i = M_CheckParm("-net");
   if(!i)
   {
      // single player game
      /*
      netgame = false;
      doomcom->id = DOOMCOM_ID;
      doomcom->numplayers = doomcom->numnodes = 1;
      doomcom->deathmatch = false;
      doomcom->consoleplayer = 0;

      return;
      */
      doomcom->id = DOOMCOM_ID;
      doomcom->numplayers = doomcom->numnodes = 1;
      doomcom->deathmatch = false;
      doomcom->consoleplayer = 0;
      doomcom->extratics=0;
      doomcom->ticdup=1;
      
      // set up for network
      
      // parse network game options,
      //  -net <consoleplayer> <host> <host> ...
      // single player game
      //doomcom = &singleplayer;
      netgame = false;
      return;      
   }
   
   netsend = PacketSend;
   netget = PacketGet;
   netgame = true;
   
   doomcom->consoleplayer = myargv[i+1][0]-'1';
   
   doomcom->numnodes = 1;
   
   SDLNet_Init();
   
   atexit(I_QuitNetwork);
   
   i++;
   while (++i < myargc && myargv[i][0] != '-')
   {
      if (SDLNet_ResolveHost(&sendaddress[doomcom->numnodes], myargv[i], DOOMPORT))
         I_Error("Unable to resolve %s", myargv[i]);
      
      doomcom->numnodes++;
   }

   doomcom->id = DOOMCOM_ID;
   doomcom->numplayers = doomcom->numnodes;
   
   udpsocket = SDLNet_UDP_Open(DOOMPORT);
   
   packet = SDLNet_AllocPacket(5000);
}

void I_NetCmd(void)
{
   if(doomcom->command == CMD_SEND)
   {
      netsend();
   }
   else if(doomcom->command == CMD_GET)
   {
      netget();
   }
   else
      I_Error("Bad net cmd: %i\n",doomcom->command);
}

//----------------------------------------------------------------------------
//
// $Log: i_net.c,v $
//
//
//----------------------------------------------------------------------------
