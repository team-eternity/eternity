//
// Copyright (C) 2024 James Haley et al.
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

#include <SDL3_net/SDL_net.h>

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

static SDLNet_DatagramSocket *udpsocket;
static byte                  *packetbuf; // SDL3_FIXME: Is this required?

static SDLNet_Address *sendaddress[MAXNETNODES];

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

#define NETWRITEBYTEIF(b, flag) \
   do \
   { \
      if((b)) \
      { \
         NETWRITEBYTE((b)); \
         ticcmdflags |= (flag); \
      } \
   } \
   while(0)

#define NETWRITESHORT(s) \
   HostToNet16((s), rover); \
   rover += 2; \
   packetsize += 2

#define NETWRITESHORTIF(s, flag) \
   do \
   { \
      if((s)) \
      { \
         NETWRITESHORT((s)); \
         ticcmdflags |= (flag); \
      } \
   } \
   while(0)

#define NETWRITELONG(dw) \
   HostToNet32((dw), rover); \
   rover += 4; \
   packetsize += 4

#define NETWRITELONGIF(dw, flag) \
   do \
   { \
      if((dw)) \
      { \
         NETWRITELONG((dw)); \
         ticcmdflags |= (flag); \
      } \
   } \
   while(0)

enum
{
   TCF_FORWARDMOVE = 0x00000001,
   TCF_SIDEMOVE    = 0x00000002,
   TCF_ANGLETURN   = 0x00000004,
   TCF_CHATCHAR    = 0x00000008,
   TCF_BUTTONS     = 0x00000010,
   TCF_ACTIONS     = 0x00000020,
   TCF_LOOK        = 0x00000040,
   TCF_FLY         = 0x00000080,
   TCF_ITEMID      = 0x00000100,
   TCF_WEAPONID    = 0x00000200,
   TCF_SLOTINDEX   = 0x00000400,
};

// DEBUG

void writesendpacket(void *data, int len)
{
   static FILE *sendfile;

   if(!sendfile)
      sendfile = fopen("sendlog.bin", "wb");

   fwrite(data, 1, len, sendfile);
   fflush(sendfile);
}

void writegetpacket(void *data, int len)
{
   static FILE *getfile;

   if(!getfile)
      getfile = fopen("getlog.bin", "wb");

   fwrite(data, 1, len, getfile);
   fflush(getfile);
}


//
// PacketSend
//
bool PacketSend(void)
{
   int c;
   int packetsize = 0;   

   byte *rover = packetbuf;

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
         byte *ticstart = rover, *ticend;
         Sint16 ticcmdflags = 0;         
         
         // reserve 2 bytes for the flags
         rover += 2;

         NETWRITEBYTEIF(netbuffer->d.cmds[c].forwardmove, TCF_FORWARDMOVE);
         NETWRITEBYTEIF(netbuffer->d.cmds[c].sidemove,    TCF_SIDEMOVE);
         NETWRITESHORTIF(netbuffer->d.cmds[c].angleturn,  TCF_ANGLETURN);         
         
         NETWRITESHORT(netbuffer->d.cmds[c].consistency);         

         NETWRITEBYTEIF(netbuffer->d.cmds[c].chatchar,  TCF_CHATCHAR);
         NETWRITEBYTEIF(netbuffer->d.cmds[c].buttons,   TCF_BUTTONS);
         NETWRITEBYTEIF(netbuffer->d.cmds[c].actions,   TCF_ACTIONS);
         NETWRITESHORTIF(netbuffer->d.cmds[c].look,     TCF_LOOK);
         NETWRITEBYTEIF(netbuffer->d.cmds[c].fly,       TCF_FLY);
         NETWRITESHORTIF(netbuffer->d.cmds[c].itemID,   TCF_ITEMID);
         NETWRITESHORTIF(netbuffer->d.cmds[c].weaponID, TCF_WEAPONID);
         NETWRITEBYTEIF(netbuffer->d.cmds[c].chatchar,  TCF_CHATCHAR);

         // go back to ticstart and write in the flags
         ticend = rover;
         rover  = ticstart;
         NETWRITESHORT(ticcmdflags);

         rover = ticend;
      }
   }
   else
   {
      for(c = 0; c < GAME_OPTION_SIZE; ++c)
         *rover++ = netbuffer->d.data[c];
      
      packetsize += GAME_OPTION_SIZE;
   }

   // Go back and write the checksum at the beginning
   rover = packetbuf;
   netbuffer->checksum |= NetChecksum(packetbuf + 4, packetsize);
   NETWRITELONG(netbuffer->checksum);
   
   // DEBUG
   writesendpacket(packetbuf, packetsize);

   if(!SDLNet_SendDatagram(udpsocket, sendaddress[doomcom->remotenode], DOOMPORT, packetbuf, packetsize))
   {
      I_Error("Error sending packet: %s\n", SDL_GetError());
      return false;
   }

   return true;
}

//
// PacketGet
//
bool PacketGet(void)
{
   SDLNet_Datagram *packet = nullptr;
   uint32_t checksum;
   int i, c;
   byte *rover;
   
   if(!SDLNet_ReceiveDatagram(udpsocket, &packet))
      I_Error("Error reading packet: %s\n", SDL_GetError());
   
   if(packet == nullptr)
   {
      doomcom->remotenode = -1;
      return true;
   }

   writegetpacket(packet->buf, packet->buflen);
   
   for(i = 0; i < doomcom->numnodes; ++i)
   {
      // SDL3_FIXME: Verify correctness
      if(SDLNet_CompareAddresses(packet->addr, sendaddress[i])) // &&
         // packet->address.port == sendaddress[i].port)
         break;
   }
   
   if(i == doomcom->numnodes)
   {
      doomcom->remotenode = -1;
      return true;
   }
   
   doomcom->remotenode = i;

   if(packet->buflen < 4)
      return false;
   
   rover = (byte *)packet->buf;

   checksum = NetToHost32(rover);
   
   // haleyjd: verify checksum first; if fails, don't even read the rest
   if((checksum & NCMD_CHECKSUM) != NetChecksum((byte *)packet->buf + 4, packet->buflen - 4))
      return false;
   
   netbuffer->checksum = checksum;
   rover += 4;
   
   netbuffer->player         = *rover++;
   netbuffer->retransmitfrom = *rover++;
   netbuffer->starttic       = *rover++;
   netbuffer->numtics        = *rover++;
   
   if(!(netbuffer->checksum & NCMD_SETUP))
   {
      for(c = 0; c < netbuffer->numtics; ++c)
      {
         Sint16 ticcmdflags;

         ticcmdflags = NetToHost16(rover);
         rover += 2;

         memset(&(netbuffer->d.cmds[c]), 0, sizeof(ticcmd_t));

         if(ticcmdflags & TCF_FORWARDMOVE)
            netbuffer->d.cmds[c].forwardmove = *rover++;
         if(ticcmdflags & TCF_SIDEMOVE)
            netbuffer->d.cmds[c].sidemove = *rover++;
         if(ticcmdflags & TCF_ANGLETURN)
         {
            netbuffer->d.cmds[c].angleturn = NetToHost16(rover);
            rover += 2;
         }
         
         netbuffer->d.cmds[c].consistency = NetToHost16(rover);
         rover += 2;
         
         if(ticcmdflags & TCF_CHATCHAR)
            netbuffer->d.cmds[c].chatchar = *rover++;
         if(ticcmdflags & TCF_BUTTONS)
            netbuffer->d.cmds[c].buttons = *rover++;
         if(ticcmdflags & TCF_ACTIONS)
            netbuffer->d.cmds[c].actions = *rover++;
         if(ticcmdflags & TCF_LOOK)
         {
            netbuffer->d.cmds[c].look = NetToHost16(rover);
            rover += 2;
         }
         if(ticcmdflags & TCF_FLY)
            netbuffer->d.cmds[c].fly = *rover++;
         if(ticcmdflags & TCF_ITEMID)
         {
            netbuffer->d.cmds[c].itemID = NetToHost16(rover);
            rover += 2;
         }
         if(ticcmdflags & TCF_WEAPONID)
         {
            netbuffer->d.cmds[c].weaponID = NetToHost16(rover);
            rover += 2;
         }
         if(ticcmdflags & TCF_SLOTINDEX)
         {
            netbuffer->d.cmds[c].slotIndex = *rover++;
         }
      }
   }
   else
   {
      for(c = 0; c < GAME_OPTION_SIZE; ++c)
         netbuffer->d.data[c] = *rover++;
   }

   SDLNet_DestroyDatagram(packet);

   return true;
}

//
// I_QuitNetwork
//
// haleyjd: from anarkavre's WinMBF netcode
// MaxW: Updated to SDL 3
//
void I_QuitNetwork(void)
{
   for(SDLNet_Address *&curraddress : sendaddress)
   {
      if(curraddress)
      {
         SDLNet_UnrefAddress(curraddress);
         curraddress = nullptr;
      }
   }

   if(packetbuf)
   {
      efree(packetbuf);
      packetbuf = nullptr;
   }
   
   if(udpsocket)
   {
      SDLNet_DestroyDatagramSocket(udpsocket);
      udpsocket = nullptr;
   }
   
   SDLNet_Quit();
}

//
// I_InitNetwork
//
void I_InitNetwork(void)
{
   int i, p;
   
   doomcom = estructalloc(doomcom_t, 1);
   
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

   if(i + 2 >= myargc)
      I_Error("I_InitNetwork: insufficient parameters to -net\n");
   
   netsend = PacketSend;
   netget  = PacketGet;
   netgame = true;
   
   doomcom->consoleplayer = myargv[i+1][0]-'1';
   
   doomcom->numnodes = 1;
   
   SDLNet_Init();
   
   I_AtExit(I_QuitNetwork);
   
   i++;
   while(++i < myargc && myargv[i][0] != '-')
   {
      sendaddress[doomcom->numnodes] = SDLNet_ResolveHostname(myargv[i]);
      if(sendaddress[doomcom->numnodes] == nullptr)
         I_Error("Catastrophic failure trying to resolve %s (%s)\n", myargv[i], SDL_GetError());
      
      doomcom->numnodes++;
   }

   int numresolved = 1;
   while(numresolved < doomcom->numnodes)
   {
      for(int node = 1; node++; node < doomcom->numnodes)
      {
         const int resolveResult = SDLNet_WaitUntilResolved(sendaddress[doomcom->numnodes], 0);
         if(resolveResult != -1)
            I_Error("Unable to resolve %s (%s)\n", myargv[i], SDL_GetError());
         else if(resolveResult)
            numresolved++;

      }

      SDL_Delay(10);
   }

   doomcom->id = DOOMCOM_ID;
   doomcom->numplayers = doomcom->numnodes;
   
   udpsocket = SDLNet_CreateDatagramSocket(nullptr, DOOMPORT);

   packetbuf = ecalloc(byte *, (sizeof(doomdata_t) + 31) & ~31, 1);
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

// EOF

