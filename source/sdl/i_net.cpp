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

void NetSend(void);
bool NetListen(void);

//
// NETWORKING
//

static Uint16 DOOMPORT = 8626;

static UDPsocket udpsocket;
static UDPpacket *packet;

static IPaddress sendaddress[MAXNETNODES];

// haleyjd: new functions for anarkavre's WinMBF netcode

// haleyjd 06/29/11: Default error-out funcs in case of high-level goofups, as
// happened with the netdemo problem.

static bool I_NetGetError()
{
   I_Error("I_NetGetError: Tried to get a packet without net init!\n");
   return false;
}

static bool I_NetSendError()
{
   I_Error("I_NetSendError: Tried to send a packet without net init!\n");
   return false;
}

static bool (*netget )() = I_NetGetError;
static bool (*netsend)() = I_NetSendError;

inline static void HostToNet16(Sint16 value, byte *area)
{
   // input is host endianness, output is big endian

   area[0] = (byte)((value >> 8) & 0xff);
   area[1] = (byte)( value       & 0xff);
}

inline static Sint16 NetToHost16(const byte *area)
{
   // input is big endian, output is host endianness
   return ((Sint16)area[0] << 8) | area[1];
}

inline static void HostToNet32(Uint32 value, byte *area)
{
   // input is host endianness, output is big endian

   area[0] = (byte)((value >> 24) & 0xff);
   area[1] = (byte)((value >> 16) & 0xff);
   area[2] = (byte)((value >>  8) & 0xff);
   area[3] = (byte)( value        & 0xff);
}

inline static Uint32 NetToHost32(const byte *area)
{
   // input is big endian, output is host endianness

   return ((Uint32)area[0] << 24) |
          ((Uint32)area[1] << 16) |
          ((Uint32)area[2] <<  8) |
                   area[3];
}

// haleyjd: end new functions for anark's netcode

// haleyjd 08/25/11: Moved checksumming to low-level protocol

//
// NetChecksum 
//
static uint32_t NetChecksum(byte *packetdata, int len)
{
   uint32_t c = 0x1234567;
   int i;
   
   len /= sizeof(uint32_t);

   for(i = 0; i < len; ++i)
      c += ((uint32_t *)packetdata)[i] * (i + 1);
   
   return c & NCMD_CHECKSUM;
}


#define NETWRITEBYTE(b) \
   *rover++ = (b); \
   packetsize += 1

#define NETWRITESHORT(s) \
   HostToNet16((s), rover); \
   rover += 2; \
   packetsize += 2

#define NETWRITELONG(dw) \
   HostToNet32((dw), rover); \
   rover += 4; \
   packetsize += 4

//
// PacketSend
//
bool PacketSend(void)
{
   int c;
   int packetsize = 0;

   byte *rover = (byte *)packet->data;

   // reserve 4 bytes for the checksum
   rover += 4;

   NETWRITEBYTE(netbuffer->player);
   NETWRITEBYTE(netbuffer->retransmitfrom);
   NETWRITEBYTE(netbuffer->starttic);
   NETWRITEBYTE(netbuffer->numtics);

   if(!(netbuffer->checksum & NCMD_SETUP))
   {
      for(c = 0; c < netbuffer->numtics; ++c)
      {
         NETWRITEBYTE(netbuffer->d.cmds[c].forwardmove);
         NETWRITEBYTE(netbuffer->d.cmds[c].sidemove);
         NETWRITESHORT(netbuffer->d.cmds[c].angleturn);         
         NETWRITESHORT(netbuffer->d.cmds[c].consistency);         
         NETWRITEBYTE(netbuffer->d.cmds[c].chatchar);
         NETWRITEBYTE(netbuffer->d.cmds[c].buttons);
         NETWRITEBYTE(netbuffer->d.cmds[c].actions);
         NETWRITESHORT(netbuffer->d.cmds[c].look);
         
      }
   }
   else
   {
      for(c = 0; c < GAME_OPTION_SIZE; ++c)
         *rover++ = netbuffer->d.data[c];
      
      packetsize = GAME_OPTION_SIZE;
   }

   // Go back and write the checksum at the beginning
   rover = (byte *)packet->data;
   netbuffer->checksum |= NetChecksum((byte *)packet->data + 4, packetsize);
   NETWRITELONG(netbuffer->checksum);
   
   packet->len     = packetsize;
   packet->address = sendaddress[doomcom->remotenode];

   if(!SDLNet_UDP_Send(udpsocket, -1, packet))
   {
      I_Error("Error sending packet: %s\n", SDLNet_GetError());
      return false;
   }

   return true;
}

//
// PacketGet
//
bool PacketGet(void)
{
   int i, c, packets_read;
   byte *rover;
   
   packets_read = SDLNet_UDP_Recv(udpsocket, packet);
   
   if(packets_read < 0)
      I_Error("Error reading packet: %s\n", SDLNet_GetError());
   
   if(packets_read == 0)
   {
      doomcom->remotenode = -1;
      return true;
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
      return true;
   }
   
   doomcom->remotenode = i;

   if(packet->len < 4)
      return false;
   
   rover = (byte *)packet->data;

   netbuffer->checksum = NetToHost32(rover);
   
   // haleyjd: verify checksum first; if fails, don't even read the rest
   if((netbuffer->checksum & NCMD_CHECKSUM) != NetChecksum((byte *)packet->data + 4, packet->len - 4))
      return false;
   
   rover += sizeof(netbuffer->checksum);
   
   netbuffer->player         = *rover++;
   netbuffer->retransmitfrom = *rover++;
   netbuffer->starttic       = *rover++;
   netbuffer->numtics        = *rover++;
   
   if(!(netbuffer->checksum & NCMD_SETUP))
   {
      for(c = 0; c < netbuffer->numtics; ++c)
      {
         netbuffer->d.cmds[c].forwardmove = *rover++;
         netbuffer->d.cmds[c].sidemove    = *rover++;
         netbuffer->d.cmds[c].angleturn   = NetToHost16(rover);
         rover += sizeof(netbuffer->d.cmds[c].angleturn);
         netbuffer->d.cmds[c].consistency = NetToHost16(rover);
         rover += sizeof(netbuffer->d.cmds[c].consistency);
         netbuffer->d.cmds[c].chatchar    = *rover++;
         netbuffer->d.cmds[c].buttons     = *rover++;
         netbuffer->d.cmds[c].actions     = *rover++;
         netbuffer->d.cmds[c].look        = NetToHost16(rover);
         rover += sizeof(netbuffer->d.cmds[c].look);
      }
   }
   else
   {
      for(c = 0; c < GAME_OPTION_SIZE; ++c)
         netbuffer->d.data[c] = *rover++;
   }

   return true;
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
   int i, p;
   
   doomcom = ecalloc(doomcom_t *, 1, sizeof(*doomcom));
   
   // set up for network
   i = M_CheckParm("-dup");
   if(i && i < myargc - 1)
   {
      doomcom->ticdup = myargv[i + 1][0] - '0';
      if (doomcom->ticdup < 1)
         doomcom->ticdup = 1;
      if (doomcom->ticdup > 9)
         doomcom->ticdup = 9;
   }
   else
      doomcom->ticdup = 1;
	
   if(M_CheckParm("-extratic"))
      doomcom->extratics = 1;
   else
      doomcom->extratics = 0;

   p = M_CheckParm("-port");
   if(p && p < myargc - 1)
   {
      DOOMPORT = atoi(myargv[p + 1]);
      usermsg("Using alternative port %i\n", DOOMPORT);
   }

   // parse network game options,
   //  -net <consoleplayer> <host> <host> ...
   i = M_CheckParm("-net");
   if(!i)
   {
      // single player game
      doomcom->id = DOOMCOM_ID;
      doomcom->numplayers = doomcom->numnodes = 1;
      doomcom->deathmatch = false;
      doomcom->consoleplayer = 0;
      doomcom->extratics = 0;
      doomcom->ticdup = 1;
      
      netgame = false;
      return;      
   }
   
   netsend = PacketSend;
   netget  = PacketGet;
   netgame = true;
   
   doomcom->consoleplayer = myargv[i+1][0]-'1';
   
   doomcom->numnodes = 1;
   
   SDLNet_Init();
   
   atexit(I_QuitNetwork);
   
   i++;
   while(++i < myargc && myargv[i][0] != '-')
   {
      if(SDLNet_ResolveHost(&sendaddress[doomcom->numnodes], myargv[i], DOOMPORT))
         I_Error("Unable to resolve %s\n", myargv[i]);
      
      doomcom->numnodes++;
   }

   doomcom->id = DOOMCOM_ID;
   doomcom->numplayers = doomcom->numnodes;
   
   udpsocket = SDLNet_UDP_Open(DOOMPORT);

   packet = SDLNet_AllocPacket((int)((sizeof(doomdata_t) + 4) & ~4));
}

bool I_NetCmd(void)
{
   if(doomcom->command == CMD_SEND)
   {
      return netsend();
   }
   else if(doomcom->command == CMD_GET)
   {
      return netget();
   }
   else
   {
      I_Error("Bad net cmd: %i\n", doomcom->command);
      return false;
   }
}

//----------------------------------------------------------------------------
//
// $Log: i_net.c,v $
//
//
//----------------------------------------------------------------------------
