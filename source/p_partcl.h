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
//-----------------------------------------------------------------------------
//
// Particle effects header
//
//-----------------------------------------------------------------------------

#ifndef P_PARTCL_H__
#define P_PARTCL_H__

// Required for: DLListItem, fixed_t, angle_t
#include "m_dllist.h"
#include "m_fixed.h"
#include "tables.h"

class  Mobj;
struct subsector_t;

// haleyjd: particle variables and structures

// particle style flags -- 07/03/03
#define PS_FULLBRIGHT   0x0001
#define PS_FLOORCLIP    0x0002
#define PS_FALLTOGROUND 0x0004
#define PS_HITGROUND    0x0008
#define PS_SPLASH       0x0010 

struct particle_t
{
   // haleyjd 02/20/04: particles now need sector links
   // haleyjd 08/05/05: use generalized dbl-linked list code
   DLListItem<particle_t> seclinks;         // sector links
   subsector_t *subsector;

   fixed_t x, y, z;
   fixed_t velx, vely, velz;
   fixed_t accx, accy, accz;
   unsigned int trans;
   unsigned int fade;
   byte	ttl;
   byte	size;
   byte color;
   int  next;
   int  styleflags; // haleyjd 07/03/03
};

extern int activeParticles;
extern int inactiveParticles;
extern particle_t *Particles;
extern int particle_trans;

#define FX_ROCKET		0x00000001
#define FX_GRENADE		0x00000002
#define FX_FLIES                0x00000004
#define FX_BFG                  0x00000008
#define FX_FLIESONDEATH         0x00000010
#define FX_DRIP                 0x00000020

#define FX_FOUNTAINMASK		0x00070000
#define FX_FOUNTAINSHIFT	16
#define FX_REDFOUNTAIN		0x00010000
#define FX_GREENFOUNTAIN	0x00020000
#define FX_BLUEFOUNTAIN		0x00030000
#define FX_YELLOWFOUNTAIN	0x00040000
#define FX_PURPLEFOUNTAIN	0x00050000
#define FX_BLACKFOUNTAIN	0x00060000
#define FX_WHITEFOUNTAIN	0x00070000

enum
{
   MBC_DEFAULTRED,
   MBC_GREY,
   MBC_GREEN,
   MBC_BLUE,
   MBC_YELLOW,
   MBC_BLACK,
   MBC_PURPLE,
   MBC_WHITE,
   MBC_ORANGE,
   NUMBLOODCOLORS
};

#define MBC_BLOODMASK 32768

// haleyjd 05/20/02: particle events
enum
{
   P_EVENT_NONE,
   P_EVENT_ROCKET_EXPLODE,
   P_EVENT_BFG_EXPLODE,
   P_EVENT_NUMEVENTS,
};

struct particle_event_t
{
   void (*func)(Mobj *);
   char name[16];
   int  enabled;
};

extern particle_event_t particleEvents[P_EVENT_NUMEVENTS];

void P_ParticleThinker(void);
void P_InitParticleEffects(void);
void P_RunEffects(void);

void P_SmokePuff(int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int updown);

void P_DrawSplash(int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int kind);
void P_DrawSplash2(int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int updown, 
                   int kind);
void P_BloodSpray(Mobj *mo, int count, fixed_t x, fixed_t y, fixed_t z, 
                  angle_t angle);
void P_DisconnectEffect(Mobj *actor);

// event functions

void P_RunEvent(Mobj *actor);
void P_AddEventVars();

#endif

// EOF
