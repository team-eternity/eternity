//
// The Eternity Engine
// Copyright (C) 2018 James Haley et al.
//
// ZDoom
// Copyright (C) 1998-2012 Marisa Heit
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
// Purpose: Code that ties particle effects to map objects,
//          adapted from ZDoom. Thanks to Marisa Heit.
// Authors: James Haley, Ioan Chera
//

#include "z_zone.h"

#include "a_small.h"
#include "autopalette.h"
#include "c_runcmd.h"
#include "d_main.h"
#include "doomstat.h"
#include "doomtype.h"
#include "e_sound.h"
#include "e_ttypes.h"
#include "m_random.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_portalcross.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_portal.h"
#include "r_state.h"
#include "r_things.h"
#include "s_sound.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

// static integers to hold particle color values
static byte grey1, grey2, grey3, grey4, red, green, blue, yellow, black,
            red1, green1, blue1, yellow1, purple, purple1, white,
            rblue1, rblue2, rblue3, rblue4, orange, yorange, dred, grey5,
            maroon1, maroon2, mdred, dred2;

static struct particleColorList {
   byte *color, r, g, b;
} particleColors[] = {
   {&grey1,     85,  85,  85 },
   {&grey2,     171, 171, 171},
   {&grey3,     50,  50,  50 },
   {&grey4,     210, 210, 210},
   {&grey5,     128, 128, 128},
   {&red,       255, 0,   0  },
   {&green,     0,   200, 0  },
   {&blue,      0,   0,   255},
   {&yellow,    255, 255, 0  },
   {&black,     0,   0,   0  },
   {&red1,      255, 127, 127},
   {&green1,    127, 255, 127},
   {&blue1,     127, 127, 255},
   {&yellow1,   255, 255, 180},
   {&purple,    120, 0,   160},
   {&purple1,   200, 30,  255},
   {&white,     255, 255, 255},
   {&rblue1,    81,  81,  255},
   {&rblue2,    0,   0,   227},
   {&rblue3,    0,   0,   130},
   {&rblue4,    0,   0,   80 },
   {&orange,    255, 120, 0  },
   {&yorange,   255, 170, 0  },
   {&dred,      80,  0,   0  },
   {&maroon1,   154, 49,  49 },
   {&maroon2,   125, 24,  24 },
   {&mdred,     165, 0,   0  },
   {&dred2,     115, 0,   0  },
   {nullptr}
};

//
// Begin Quake 2 particle effects data. This code is taken from Quake 2,
// copyright 1997 id Software, Inc. Available under the GNU General
// Public License.
//

#define	BEAMLENGTH       16
#define NUMVERTEXNORMALS 162
typedef float vec3_t[3];

static vec3_t avelocities[NUMVERTEXNORMALS];

static vec3_t bytedirs[NUMVERTEXNORMALS] = 
{
   {-0.525731f, 0.000000f, 0.850651f}, 
   {-0.442863f, 0.238856f, 0.864188f}, 
   {-0.295242f, 0.000000f, 0.955423f}, 
   {-0.309017f, 0.500000f, 0.809017f}, 
   {-0.162460f, 0.262866f, 0.951056f}, 
   {0.000000f, 0.000000f, 1.000000f}, 
   {0.000000f, 0.850651f, 0.525731f}, 
   {-0.147621f, 0.716567f, 0.681718f}, 
   {0.147621f, 0.716567f, 0.681718f}, 
   {0.000000f, 0.525731f, 0.850651f}, 
   {0.309017f, 0.500000f, 0.809017f}, 
   {0.525731f, 0.000000f, 0.850651f}, 
   {0.295242f, 0.000000f, 0.955423f}, 
   {0.442863f, 0.238856f, 0.864188f}, 
   {0.162460f, 0.262866f, 0.951056f}, 
   {-0.681718f, 0.147621f, 0.716567f}, 
   {-0.809017f, 0.309017f, 0.500000f}, 
   {-0.587785f, 0.425325f, 0.688191f}, 
   {-0.850651f, 0.525731f, 0.000000f}, 
   {-0.864188f, 0.442863f, 0.238856f}, 
   {-0.716567f, 0.681718f, 0.147621f}, 
   {-0.688191f, 0.587785f, 0.425325f}, 
   {-0.500000f, 0.809017f, 0.309017f}, 
   {-0.238856f, 0.864188f, 0.442863f}, 
   {-0.425325f, 0.688191f, 0.587785f}, 
   {-0.716567f, 0.681718f, -0.147621f}, 
   {-0.500000f, 0.809017f, -0.309017f}, 
   {-0.525731f, 0.850651f, 0.000000f}, 
   {0.000000f, 0.850651f, -0.525731f}, 
   {-0.238856f, 0.864188f, -0.442863f}, 
   {0.000000f, 0.955423f, -0.295242f}, 
   {-0.262866f, 0.951056f, -0.162460f}, 
   {0.000000f, 1.000000f, 0.000000f}, 
   {0.000000f, 0.955423f, 0.295242f}, 
   {-0.262866f, 0.951056f, 0.162460f}, 
   {0.238856f, 0.864188f, 0.442863f}, 
   {0.262866f, 0.951056f, 0.162460f}, 
   {0.500000f, 0.809017f, 0.309017f}, 
   {0.238856f, 0.864188f, -0.442863f}, 
   {0.262866f, 0.951056f, -0.162460f}, 
   {0.500000f, 0.809017f, -0.309017f}, 
   {0.850651f, 0.525731f, 0.000000f}, 
   {0.716567f, 0.681718f, 0.147621f}, 
   {0.716567f, 0.681718f, -0.147621f}, 
   {0.525731f, 0.850651f, 0.000000f}, 
   {0.425325f, 0.688191f, 0.587785f}, 
   {0.864188f, 0.442863f, 0.238856f}, 
   {0.688191f, 0.587785f, 0.425325f}, 
   {0.809017f, 0.309017f, 0.500000f}, 
   {0.681718f, 0.147621f, 0.716567f}, 
   {0.587785f, 0.425325f, 0.688191f}, 
   {0.955423f, 0.295242f, 0.000000f}, 
   {1.000000f, 0.000000f, 0.000000f}, 
   {0.951056f, 0.162460f, 0.262866f}, 
   {0.850651f, -0.525731f, 0.000000f}, 
   {0.955423f, -0.295242f, 0.000000f}, 
   {0.864188f, -0.442863f, 0.238856f}, 
   {0.951056f, -0.162460f, 0.262866f}, 
   {0.809017f, -0.309017f, 0.500000f}, 
   {0.681718f, -0.147621f, 0.716567f}, 
   {0.850651f, 0.000000f, 0.525731f}, 
   {0.864188f, 0.442863f, -0.238856f}, 
   {0.809017f, 0.309017f, -0.500000f}, 
   {0.951056f, 0.162460f, -0.262866f}, 
   {0.525731f, 0.000000f, -0.850651f}, 
   {0.681718f, 0.147621f, -0.716567f}, 
   {0.681718f, -0.147621f, -0.716567f}, 
   {0.850651f, 0.000000f, -0.525731f}, 
   {0.809017f, -0.309017f, -0.500000f}, 
   {0.864188f, -0.442863f, -0.238856f}, 
   {0.951056f, -0.162460f, -0.262866f}, 
   {0.147621f, 0.716567f, -0.681718f}, 
   {0.309017f, 0.500000f, -0.809017f}, 
   {0.425325f, 0.688191f, -0.587785f}, 
   {0.442863f, 0.238856f, -0.864188f}, 
   {0.587785f, 0.425325f, -0.688191f}, 
   {0.688191f, 0.587785f, -0.425325f}, 
   {-0.147621f, 0.716567f, -0.681718f}, 
   {-0.309017f, 0.500000f, -0.809017f}, 
   {0.000000f, 0.525731f, -0.850651f}, 
   {-0.525731f, 0.000000f, -0.850651f}, 
   {-0.442863f, 0.238856f, -0.864188f}, 
   {-0.295242f, 0.000000f, -0.955423f}, 
   {-0.162460f, 0.262866f, -0.951056f}, 
   {0.000000f, 0.000000f, -1.000000f}, 
   {0.295242f, 0.000000f, -0.955423f}, 
   {0.162460f, 0.262866f, -0.951056f}, 
   {-0.442863f, -0.238856f, -0.864188f}, 
   {-0.309017f, -0.500000f, -0.809017f}, 
   {-0.162460f, -0.262866f, -0.951056f}, 
   {0.000000f, -0.850651f, -0.525731f}, 
   {-0.147621f, -0.716567f, -0.681718f}, 
   {0.147621f, -0.716567f, -0.681718f}, 
   {0.000000f, -0.525731f, -0.850651f}, 
   {0.309017f, -0.500000f, -0.809017f}, 
   {0.442863f, -0.238856f, -0.864188f}, 
   {0.162460f, -0.262866f, -0.951056f}, 
   {0.238856f, -0.864188f, -0.442863f}, 
   {0.500000f, -0.809017f, -0.309017f}, 
   {0.425325f, -0.688191f, -0.587785f}, 
   {0.716567f, -0.681718f, -0.147621f}, 
   {0.688191f, -0.587785f, -0.425325f}, 
   {0.587785f, -0.425325f, -0.688191f}, 
   {0.000000f, -0.955423f, -0.295242f}, 
   {0.000000f, -1.000000f, 0.000000f}, 
   {0.262866f, -0.951056f, -0.162460f}, 
   {0.000000f, -0.850651f, 0.525731f}, 
   {0.000000f, -0.955423f, 0.295242f}, 
   {0.238856f, -0.864188f, 0.442863f}, 
   {0.262866f, -0.951056f, 0.162460f}, 
   {0.500000f, -0.809017f, 0.309017f}, 
   {0.716567f, -0.681718f, 0.147621f}, 
   {0.525731f, -0.850651f, 0.000000f}, 
   {-0.238856f, -0.864188f, -0.442863f}, 
   {-0.500000f, -0.809017f, -0.309017f}, 
   {-0.262866f, -0.951056f, -0.162460f}, 
   {-0.850651f, -0.525731f, 0.000000f}, 
   {-0.716567f, -0.681718f, -0.147621f}, 
   {-0.716567f, -0.681718f, 0.147621f}, 
   {-0.525731f, -0.850651f, 0.000000f}, 
   {-0.500000f, -0.809017f, 0.309017f}, 
   {-0.238856f, -0.864188f, 0.442863f}, 
   {-0.262866f, -0.951056f, 0.162460f}, 
   {-0.864188f, -0.442863f, 0.238856f}, 
   {-0.809017f, -0.309017f, 0.500000f}, 
   {-0.688191f, -0.587785f, 0.425325f}, 
   {-0.681718f, -0.147621f, 0.716567f}, 
   {-0.442863f, -0.238856f, 0.864188f}, 
   {-0.587785f, -0.425325f, 0.688191f}, 
   {-0.309017f, -0.500000f, 0.809017f}, 
   {-0.147621f, -0.716567f, 0.681718f}, 
   {-0.425325f, -0.688191f, 0.587785f}, 
   {-0.162460f, -0.262866f, 0.951056f}, 
   {0.442863f, -0.238856f, 0.864188f}, 
   {0.162460f, -0.262866f, 0.951056f}, 
   {0.309017f, -0.500000f, 0.809017f}, 
   {0.147621f, -0.716567f, 0.681718f}, 
   {0.000000f, -0.525731f, 0.850651f}, 
   {0.425325f, -0.688191f, 0.587785f}, 
   {0.587785f, -0.425325f, 0.688191f}, 
   {0.688191f, -0.587785f, 0.425325f}, 
   {-0.955423f, 0.295242f, 0.000000f}, 
   {-0.951056f, 0.162460f, 0.262866f}, 
   {-1.000000f, 0.000000f, 0.000000f}, 
   {-0.850651f, 0.000000f, 0.525731f}, 
   {-0.955423f, -0.295242f, 0.000000f}, 
   {-0.951056f, -0.162460f, 0.262866f}, 
   {-0.864188f, 0.442863f, -0.238856f}, 
   {-0.951056f, 0.162460f, -0.262866f}, 
   {-0.809017f, 0.309017f, -0.500000f}, 
   {-0.864188f, -0.442863f, -0.238856f}, 
   {-0.951056f, -0.162460f, -0.262866f}, 
   {-0.809017f, -0.309017f, -0.500000f}, 
   {-0.681718f, 0.147621f, -0.716567f}, 
   {-0.681718f, -0.147621f, -0.716567f}, 
   {-0.850651f, 0.000000f, -0.525731f}, 
   {-0.688191f, 0.587785f, -0.425325f}, 
   {-0.587785f, 0.425325f, -0.688191f}, 
   {-0.425325f, 0.688191f, -0.587785f}, 
   {-0.425325f, -0.688191f, -0.587785f}, 
   {-0.587785f, -0.425325f, -0.688191f}, 
   {-0.688191f, -0.587785f, -0.425325f}, 
};

//
// End Quake 2 data.
//

static particle_t *JitterParticle(int ttl);
static void P_RunEffect(Mobj *actor, unsigned int effects);
static void P_FlyEffect(Mobj *actor);
static void P_BFGEffect(Mobj *actor);
static void P_DripEffect(Mobj *actor);
static void P_ExplosionParticles(fixed_t, fixed_t, fixed_t, byte, byte);
static void P_RocketExplosion(Mobj *actor);
static void P_BFGExplosion(Mobj *actor);

//
// P_GenVelocities
//
// Populates the avelocities array with randomly created floating
// point values. Derived from Quake 2. Available under the GNU
// General Public License.
//
static void P_GenVelocities(void)
{
   for(vec3_t &avelocity : avelocities) for(float &component : avelocity)
      component = M_Random() * 0.01f;
}

void P_InitParticleEffects(void)
{
   AutoPalette palette(wGlobalDir);
   struct particleColorList *pc = particleColors;

   // match particle colors to best fit and write back to
   // static variables
   while(pc->color)
   {
      *(pc->color) = V_FindBestColor(palette.get(), pc->r, pc->g, pc->b);
      pc++;
   }

   P_GenVelocities();
}

//
// P_UnsetParticlePosition
//
// haleyjd 02/20/04: maintenance of particle sector links,
// necessitated by portals.
//
static void P_UnsetParticlePosition(particle_t *ptcl)
{
   ptcl->seclinks.remove();
   ptcl->subsector = nullptr;
}

//
// P_SetParticlePosition
//
// haleyjd 02/20/04: maintenance of particle sector links,
// necessitated by portals. Maintaining a subsector_t
// field in the particle_t will be useful in the future,
// I am sure.
//
static void P_SetParticlePosition(particle_t *ptcl)
{
   subsector_t *ss = R_PointInSubsector(ptcl->x, ptcl->y);

   ptcl->seclinks.insert(ptcl, &(ss->sector->ptcllist));
   ptcl->subsector = ss;
}

void P_ParticleThinker(void)
{
   int i;
   particle_t *particle, *prev;
   const sector_t *psec;
   fixed_t floorheight;
   
   i = activeParticles;
   prev = nullptr;
   while(i != -1) 
   {
      particle = Particles + i;
      i = particle->next;

      // haleyjd: unlink the particle from the world
      P_UnsetParticlePosition(particle);

      // haleyjd: particles with fall to ground style don't start
      // fading or counting down their TTL until they hit the floor
      if(!(particle->styleflags & PS_FALLTOGROUND))
      {
         // perform fading
         unsigned oldtrans = particle->trans;
         particle->trans -= particle->fade;
         
         // is it time to kill this particle?
         if(oldtrans < particle->trans || --particle->ttl == 0)
         {
            memset(particle, 0, sizeof(particle_t));
            if(prev)
               prev->next = i;
            else
               activeParticles = i;
            particle->next = inactiveParticles;
            inactiveParticles = eindex(particle - Particles);
            continue;
         }
      }

      // Check for wall portals
      if(gMapHasLinePortals && particle->velx | particle->vely)
      {
         v2fixed_t destination = P_LinePortalCrossing(particle->x, particle->y, 
            particle->velx, particle->vely);
         particle->x = destination.x;
         particle->y = destination.y;
      }
      else
      {
         // update and link to new position
         particle->x += particle->velx;
         particle->y += particle->vely;
      }
      particle->z += particle->velz;
      P_SetParticlePosition(particle);
      if(P_IsInVoid(particle->x, particle->y, *particle->subsector))
      {
         particle->ttl = 1;
         particle->trans = 0;
      }

      // apply accelerations
      particle->velx += particle->accx;
      particle->vely += particle->accy;
      particle->velz += particle->accz;

      // handle special movement flags (post-position-set)

      psec = particle->subsector->sector;

      // haleyjd 09/04/05: use deep water floor if it is higher
      // than the real floor.
      floorheight =
         (psec->heightsec != -1 &&
          sectors[psec->heightsec].srf.floor.height > psec->srf.floor.height) ?
          sectors[psec->heightsec].srf.floor.height :
          psec->srf.floor.height;

      // did particle hit ground, but is now no longer on it?
      if(particle->styleflags & PS_HITGROUND && particle->z != floorheight)
         particle->z = floorheight;

      // floor clipping
      if(particle->z < floorheight && psec->srf.floor.pflags & PS_PASSABLE)
      {
         const linkdata_t *ldata = R_FPLink(psec);

         P_UnsetParticlePosition(particle);
         particle->x += ldata->delta.x;
         particle->y += ldata->delta.y;
         particle->z += ldata->delta.z;
         P_SetParticlePosition(particle);
      }
      else if(particle->z < floorheight)
      {
         // particles with fall to ground style start ticking now
         if(particle->styleflags & PS_FALLTOGROUND)
            particle->styleflags &= ~PS_FALLTOGROUND;

         // particles with floor clipping may need to stop
         if(particle->styleflags & PS_FLOORCLIP)
         {
            particle->z = floorheight;
            particle->accz = particle->velz = 0;
            particle->styleflags |= PS_HITGROUND;
            
            // some particles make splashes
            if(particle->styleflags & PS_SPLASH)
               E_PtclTerrainHit(particle);
         }
      }
      else if(particle->z > psec->srf.ceiling.height && psec->srf.ceiling.pflags & PS_PASSABLE)
      {
         const linkdata_t *ldata = R_CPLink(psec);

         P_UnsetParticlePosition(particle);
         particle->x += ldata->delta.x;
         particle->y += ldata->delta.y;
         particle->z += ldata->delta.z;
         P_SetParticlePosition(particle);
      }
      
      prev = particle;
   }
}

//
// Various hardcoded particle sounds
//
enum particlesound_e
{
   particlesound_flies,
   particlesound_MAX
};
static const int particlesounds[particlesound_MAX] =
{
   sfx_eefly
};

//
// Maintains a particle's associated ambient sound
//
static void P_maintainParticleAmbientSound(Mobj *actor, particlesound_e psoundid)
{
   int dehnum = particlesounds[psoundid];
   if(S_CheckSoundPlaying(actor, dehnum))
      return;
   S_StartSoundLooped(actor, dehnum);
   actor->intflags |= MIF_MAYPLAYPARTICLESOUNDS;
}

//
// Stops a particle's associated ambient sound. Special psoundid MAX disables ALL sounds.
//
static void P_stopParticleAmbientSounds(Mobj *actor, particlesound_e psoundid)
{
   if(!(actor->intflags & MIF_MAYPLAYPARTICLESOUNDS)) // early out
      return;
   if(psoundid == particlesound_MAX)
   {
      for(int i = 0; i < particlesound_MAX; ++i)
         S_StopSoundId(actor, particlesounds[i]);
      actor->intflags &= ~MIF_MAYPLAYPARTICLESOUNDS;  // guaranteed not to be making noise now
      return;
   }
   S_StopSoundId(actor, particlesounds[psoundid]);
   // Do not clear the flag; other sounds might be playing.
}

void P_RunEffects(void)
{
   int snum = 0;
   Thinker *th = &thinkercap;

   if(camera)
   {
      subsector_t *ss = R_PointInSubsector(camera->x, camera->y);
      snum = eindex(ss->sector - sectors) * numsectors;
   }
   else
   {
      subsector_t *ss = players[displayplayer].mo->subsector;
      snum = eindex(ss->sector - sectors) * numsectors;
   }

   while((th = th->next) != &thinkercap)
   {
      Mobj *mobj;
      if((mobj = thinker_cast<Mobj *>(th)))
      {
         int rnum = snum + eindex(mobj->subsector->sector - sectors);
         if(mobj->effects)
         {
            // run only if possibly visible
            if(!(rejectmatrix[rnum >> 3] & (1 << (rnum & 7))))
               P_RunEffect(mobj, mobj->effects);
            else
               P_stopParticleAmbientSounds(mobj, particlesound_MAX);
         }
         else
            P_stopParticleAmbientSounds(mobj, particlesound_MAX);
      }
   }
}

//
// haleyjd 05/19/02: partially rewrote to not make assumptions
// about struct member order and alignment in memory
//

#define FADEFROMTTL(a) (FRACUNIT/(a))

#define PARTICLE_VELRND ((FRACUNIT / 4096)  * (M_Random() - 128))
#define PARTICLE_ACCRND ((FRACUNIT / 16384) * (M_Random() - 128))

static particle_t *JitterParticle(int ttl)
{
   particle_t *particle = newParticle();
   
   if(particle) 
   {
      // Set initial velocities
      particle->velx = PARTICLE_VELRND;
      particle->vely = PARTICLE_VELRND;
      particle->velz = PARTICLE_VELRND;
      
      // Set initial accelerations
      particle->accx = PARTICLE_ACCRND;
      particle->accy = PARTICLE_ACCRND;
      particle->accz = PARTICLE_ACCRND;
      
      particle->trans = FRACUNIT;	// fully opaque
      particle->ttl = ttl;
      particle->fade = FADEFROMTTL(ttl);
   }
   return particle;
}

static void MakeFountain(Mobj *actor, byte color1, byte color2)
{
   particle_t *particle;
   
   if(!(leveltime & 1))
      return;
   
   particle = JitterParticle(51);
   
   if(particle)
   {
      angle_t an  = M_Random()<<(24-ANGLETOFINESHIFT);
      fixed_t out = FixedMul(actor->radius, M_Random()<<8);
      
      particle->x = actor->x + FixedMul(out, finecosine[an]);
      particle->y = actor->y + FixedMul(out, finesine[an]);
      particle->z = actor->z + actor->height + FRACUNIT;
      P_SetParticlePosition(particle);
      
      if(out < actor->radius/8)
         particle->velz += FRACUNIT*10/3;
      else
         particle->velz += FRACUNIT*3;
      
      particle->accz -= FRACUNIT/11;
      if(M_Random() < 30)
      {
         particle->size = 4;
         particle->color = color2;
      } 
      else 
      {
         particle->size = 6;
         particle->color = color1;
      }

      particle->styleflags = 0;
   }
}

static void P_RunEffect(Mobj *actor, unsigned int effects)
{
   angle_t moveangle = P_PointToAngle(0,0,actor->momx,actor->momy);

   if(effects & FX_FLIES || 
      (effects & FX_FLIESONDEATH && actor->tics == -1 &&
       actor->movecount >= 4*TICRATE))
   {       
      P_FlyEffect(actor);
      P_maintainParticleAmbientSound(actor, particlesound_flies);
   }
   else
      P_stopParticleAmbientSounds(actor, particlesound_flies);
   
   if((effects & FX_ROCKET) && drawrockettrails)
   {
      int i, speed;

      // Rocket trail
      fixed_t backx = 
         actor->x - FixedMul(finecosine[(moveangle)>>ANGLETOFINESHIFT], 
                             actor->radius*2);
      fixed_t backy = 
         actor->y - FixedMul(finesine[(moveangle)>>ANGLETOFINESHIFT], 
                             actor->radius*2);
      fixed_t backz = 
         actor->z - (actor->height>>3) * (actor->momz>>16) + 
         (2*actor->height)/3;
      
      angle_t an = (moveangle + ANG90) >> ANGLETOFINESHIFT;

      particle_t *particle = JitterParticle(3 + (M_Random() & 31));
      if(particle)
      {
         fixed_t pathdist = M_Random()<<8;
         particle->x = backx - FixedMul(actor->momx, pathdist);
         particle->y = backy - FixedMul(actor->momy, pathdist);
         particle->z = backz - FixedMul(actor->momz, pathdist);
         P_SetParticlePosition(particle);

         speed = (M_Random () - 128) * (FRACUNIT/200);
         particle->velx += FixedMul(speed, finecosine[an]);
         particle->vely += FixedMul(speed, finesine[an]);
         particle->velz -= FRACUNIT/36;
         particle->accz -= FRACUNIT/20;
         particle->color = yellow;
         particle->size = 2;
         particle->styleflags = PS_FULLBRIGHT;
      }
      
      for(i = 6; i; --i)
      {
         particle_t *iparticle = JitterParticle(3 + (M_Random() & 31));
         if(iparticle)
         {
            fixed_t pathdist = M_Random() << 8;
            iparticle->x = backx - FixedMul(actor->momx, pathdist);
            iparticle->y = backy - FixedMul(actor->momy, pathdist);
            iparticle->z = backz - FixedMul(actor->momz, pathdist) + 
                             (M_Random() << 10);
            P_SetParticlePosition(iparticle);

            speed = (M_Random() - 128) * (FRACUNIT/200);
            iparticle->velx += FixedMul(speed, finecosine[an]);
            iparticle->vely += FixedMul(speed, finesine[an]);
            iparticle->velz += FRACUNIT/80;
            iparticle->accz += FRACUNIT/40;
            iparticle->color = (M_Random() & 7) ? grey2 : grey1;            
            iparticle->size = 3;
            iparticle->styleflags = 0;
         } 
         else
            break;
      }
   }
   
   if((effects & FX_GRENADE) && drawgrenadetrails)
   {
      // Grenade trail
      
      P_DrawSplash2(6,
         actor->x - FixedMul (finecosine[(moveangle)>>ANGLETOFINESHIFT], actor->radius*2),
         actor->y - FixedMul (finesine[(moveangle)>>ANGLETOFINESHIFT], actor->radius*2),
         actor->z - (actor->height>>3) * (actor->momz>>16) + (2*actor->height)/3,
         moveangle + ANG180, 2, 2);
   }

   if((effects & FX_BFG) && drawbfgcloud)
      P_BFGEffect(actor);

   if((effects & FX_FOUNTAINMASK) && !(actor->flags2 & MF2_DORMANT))
   {
      // Particle fountain -- can be switched on and off via the
      // MF2_DORMANT flag
      
      static const byte *fountainColors[16] = 
      { 
         &black,  &black,
         &red,    &red1,
         &green,  &green1,
         &blue,   &blue1,
         &yellow, &yellow1,
         &purple, &purple1,
         &black,  &grey3,
         &grey4,  &white
      };
      unsigned int color = (effects & FX_FOUNTAINMASK) >> 15;
      MakeFountain(actor, *fountainColors[color], *fountainColors[color+1]);
   }

   if(effects & FX_DRIP)
      P_DripEffect(actor);
}

void P_DrawSplash(int count, fixed_t x, fixed_t y, fixed_t z, 
                  angle_t angle, int kind)
{
   byte color1, color2;
   
   switch(kind) 
   {
   case 1: // Spark
      color1 = orange;
      color2 = yorange;
      break;
   default:
      return;
   }
   
   for(; count; count--)
   {
      angle_t an;
      particle_t *p = JitterParticle(10);
            
      if(!p)
         break;
      
      p->size = 2;
      p->color = M_Random() & 0x80 ? color1 : color2;
      p->styleflags = PS_FULLBRIGHT;
      p->velz -= M_Random() * 512;
      p->accz -= FRACUNIT/8;
      p->accx += (M_Random() - 128) * 8;
      p->accy += (M_Random() - 128) * 8;
      p->z = z - M_Random() * 1024;
      an = (angle + (M_Random() << 21)) >> ANGLETOFINESHIFT;
      p->x = x + (M_Random() & 15)*finecosine[an];
      p->y = y + (M_Random() & 15)*finesine[an];
      P_SetParticlePosition(p);
   }
}

//
// P_BloodDrop
//
// haleyjd 04/01/05: Code originally by SoM that makes a blood drop
// that falls to the floor. Isolated by me into a function, and made
// to use new styleflags that weren't available when this was written.
// This code is under the GPL.
//
static void P_BloodDrop(int count, fixed_t x, fixed_t y, fixed_t z, 
                        angle_t angle, byte color1, byte color2)
{
   for(; count; --count)
   {
      particle_t *p = newParticle();
      angle_t an;
      
      if(!p)
         break;
      
      p->ttl = 96;
      p->fade = FADEFROMTTL(96);
      p->trans = FRACUNIT;
      p->size = 4;
      p->color = M_Random() & 0x80 ? color1 : color2;
      p->velz = 128 * -3000 + M_Random();
      p->accz = -(LevelInfo.gravity*100/256);
      p->styleflags = PS_FLOORCLIP | PS_FALLTOGROUND;
      p->z = z + (M_Random() - 128) * -2400;
      an = (angle + ((M_Random() - 128) << 22)) >> ANGLETOFINESHIFT;
      p->x = x + (M_Random() & 10) * finecosine[an];
      p->y = y + (M_Random() & 10) * finesine[an];
      P_SetParticlePosition(p);
   }
}

//
// P_SmokePuff
//
// haleyjd 09/10/07: SoM's vastly improved smoke puff effect, improved even
// more to replace the god-awful Quake 2-wannabe splash from ZDoom.
// This code is under the GPL.
//
void P_SmokePuff(int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, 
                 int updown)
{
   particle_t *p;
   angle_t an;
   int ttl;
   fixed_t accz;
   bool hitwater = false;
   byte color1, color2;

   // default: grey puff
   color1 = grey1;
   color2 = grey5;
   
   if(!getComp(comp_terrain))
   {
      // 06/21/02: make bullet puff colors responsive to 
      // TerrainTypes -- this is very cool and Quake-2-like ^_^      
      
      ETerrain *terrain = E_GetTerrainTypeForPt(x, y, updown);
      
      if(terrain->usepcolors)
      {
         color1 = terrain->pcolor_1;
         color2 = terrain->pcolor_2;
      }
      
      if(terrain->liquid)
         hitwater = true;
   }

   count += M_Random() & 15; // MOARRRR!

   // handle shooting liquids: make it spray up like in the movies
   if(!updown && hitwater)
   {
      // live longer and accelerate downward faster
      ttl  = 30;
      accz = -FRACUNIT/8;
   }
   else
   {
      ttl  = 15;
      accz = -FRACUNIT/22;
   }

   for(; count; --count)
   {      
      if(!(p = newParticle()))
         break;
      
      p->ttl = ttl;
      p->fade = FADEFROMTTL(ttl);
      p->trans = FRACUNIT;
      p->size = 2 + M_Random() % 5;
      p->color = M_Random() & 0x80 ? color1 : color2;      
      p->velz = M_Random() * 512;
      if(updown == 1) // ceiling shot?
         p->velz = -(p->velz / 4);
      p->accz = accz;
      p->styleflags = 0;
      
      an = (angle + ((M_Random() - 128) << 23)) >> ANGLETOFINESHIFT;
      p->velx = (M_Random() * finecosine[an]) >> 11;
      p->vely = (M_Random() * finesine[an]) >> 11;
      p->accx = p->velx >> 4;
      p->accy = p->vely >> 4;
      
      if(updown == 1) // ceiling shot?
         p->z = z - (M_Random() + 72) * 2000;
      else
         p->z = z + (M_Random() + 72) * 2000;
      an = (angle + ((M_Random() - 128) << 22)) >> ANGLETOFINESHIFT;
      p->x = x + (M_Random() & 14) * finecosine[an];
      p->y = y + (M_Random() & 14) * finesine[an];
      P_SetParticlePosition(p);
   }

   if(!hitwater) // no sparks on liquids
   {
      count = M_Random() & 3;

      for(; count; --count)
      {
         fixed_t pathdist = M_Random() << 8;
         fixed_t speed;
         
         if(!(p = JitterParticle(3 + (M_Random() % 24))))
            break;
         
         p->x = x - pathdist;
         p->y = y - pathdist;
         p->z = z - pathdist;
         P_SetParticlePosition(p);
         
         speed = (M_Random() - 128) * (FRACUNIT / 200);
         an = angle >> ANGLETOFINESHIFT;
         p->velx += FixedMul(speed, finecosine[an]);
         p->vely += FixedMul(speed, finesine[an]);
         if(updown) // on ceiling or wall, fall fast
            p->velz -= FRACUNIT/36;
         else       // on floor, throw it upward a bit
            p->velz += FRACUNIT/2;
         p->accz -= FRACUNIT/20;
         p->color = yellow;
         p->size = 2;
         p->styleflags = PS_FULLBRIGHT;
      }
   }
}

// haleyjd 05/08/03: custom particle blood colors

static struct bloodColor
{
   byte *color1;
   byte *color2;
} mobjBloodColors[NUMBLOODCOLORS] =
{
   { &red,    &dred2 },
   { &grey1,  &grey5 },
   { &green,  &green1 },
   { &blue,   &blue },
   { &yellow, &yellow },
   { &black,  &grey3 },
   { &purple, &purple1 },
   { &grey4,  &white },
   { &orange, &yorange },
};

void P_BloodSpray(Mobj *mo, int count, fixed_t x, fixed_t y, fixed_t z, 
                  angle_t angle)
{
   byte color1, color2;
   particle_t *p;
   angle_t an;
   int bloodcolor = mo->info->bloodcolor;

   // get blood colors
   if(bloodcolor < 0 || bloodcolor >= NUMBLOODCOLORS)
      bloodcolor = 0;

   color1 = *(mobjBloodColors[bloodcolor].color1);
   color2 = *(mobjBloodColors[bloodcolor].color2);

   // haleyjd 04/01/05: at random, throw out drops
   // haleyjd 09/10/07: even if a drop is thrown, do the rest of the effect
   if(M_Random() < 72)
      P_BloodDrop(count, x, y, z, angle, color1, color2);

   // swap colors if reversed
   if(color2 < color1)
   {
      int tempcol = color1;
      color1  = color2;
      color2  = tempcol;
   }

   count += 3*((M_Random() & 31) + 1)/2; // a LOT more blood.

   // haleyjd 07/04/09: randomize z coordinate a bit (128/32 == 4 units)
   z += 3*FRACUNIT + (M_Random() - 128) * FRACUNIT/32;


   for(; count; --count)
   {
      if(!(p = newParticle()))
         break;
      
      p->ttl = 25 + M_Random() % 6;
      p->fade = FADEFROMTTL(p->ttl);
      p->trans = FRACUNIT;
      p->size = 1 + M_Random() % 4;
      
      // if colors are part of same ramp, use all in between
      if(color1 != color2 && abs(color2 - color1) <= 16)
         p->color = M_RangeRandom(color1, color2);
      else
         p->color = M_Random() & 0x80 ? color1 : color2;
      
      p->styleflags = 0;
      
      an      = (angle + ((M_Random() - 128) << 23)) >> ANGLETOFINESHIFT;
      p->velx = (M_Random() * finecosine[an]) / 768;
      p->vely = (M_Random() * finesine[an]) / 768;

      an      = (angle + ((M_Random() - 128) << 22)) >> ANGLETOFINESHIFT;      
      p->x    = x + (M_Random() % 15) * finecosine[an];
      p->y    = y + (M_Random() % 15) * finesine[an];
      p->z    = z + (M_Random() - 128) * -3500;
      p->velz = (M_Random() < 32) ? M_Random() * 140 : M_Random() * -128;
      p->accz = -FRACUNIT/16;
      
      P_SetParticlePosition(p);
   }
}

void P_DrawSplash2(int count, fixed_t x, fixed_t y, fixed_t z, 
                   angle_t angle, int updown, int kind)
{   
   byte color1, color2;
   int zvel, zspread, zadd;
   
   switch(kind)
   {
   case 2:		// Smoke
      color1 = grey3;
      color2 = grey1;
      break;
   default:
      return;
   }
   
   zvel = -128;
   zspread = (updown ? -6000 : 6000);
   zadd = ((updown == 2) ? -128 : 0);
   
   for(; count; count--)
   {
      particle_t *p = newParticle();
      angle_t an;
      
      if(!p)
         break;
      
      p->ttl = 12;
      p->fade = FADEFROMTTL(12);
      p->trans = FRACUNIT;
      p->styleflags = 0;
      p->size = 2 + M_Random() % 5;
      p->color = M_Random() & 0x80 ? color1 : color2;
      p->velz = M_Random() * zvel;
      p->accz = -FRACUNIT/22;
      if(kind)
      {
         an = (angle + ((M_Random() - 128) << 23)) >> ANGLETOFINESHIFT;
         p->velx = (M_Random() * finecosine[an]) >> 11;
         p->vely = (M_Random() * finesine[an]) >> 11;
         p->accx = p->velx >> 4;
         p->accy = p->vely >> 4;
      }
      p->z = z + (M_Random() + zadd) * zspread;
      an = (angle + ((M_Random() - 128) << 22)) >> ANGLETOFINESHIFT;
      p->x = x + (M_Random() & 31) * finecosine[an];
      p->y = y + (M_Random() & 31) * finesine[an];
      P_SetParticlePosition(p);
   }
}

void P_DisconnectEffect(Mobj *actor)
{
   int i;
   
   for(i = 64; i; i--)
   {
      particle_t *p = JitterParticle (TICRATE*2);
      
      if(!p)
         break;
      
      p->x = actor->x + 
             ((M_Random()-128)<<9) * (actor->radius>>FRACBITS);
      p->y = actor->y + 
             ((M_Random()-128)<<9) * (actor->radius>>FRACBITS);
      p->z = actor->z + (M_Random()<<8) * (actor->height>>FRACBITS);
      P_SetParticlePosition(p);

      p->accz -= FRACUNIT/4096;
      p->color = M_Random() < 128 ? maroon1 : maroon2;
      p->size = 4;
      p->styleflags = PS_FULLBRIGHT;
   }
}

//
// P_FlyEffect
//
// Derived from Quake 2. Available under the GNU General Public License.
//
static void P_FlyEffect(Mobj *actor)
{
   int i, count;
   particle_t *p;
   float angle;
   float sp, sy, cp, cy;
   vec3_t forward;
   float dist = 64;
   float ltime;

   ltime = (float)leveltime / 50.0f;

   // 07/13/05: ramp flies up over time for flies-on-death effect
   if(actor->effects & FX_FLIESONDEATH)
      count = (actor->movecount - 4*TICRATE) * 162 / (20 * TICRATE);
   else
      count = 162;

   if(count < 1)
      count = 1;   
   if(count > 162)
      count = 162;
   
   for(i = 0; i < count; i += 2)
   {
      if(!(p = newParticle()))
         break;

      angle = ltime * avelocities[i][0];
      sy = (float)sin(angle);
      cy = (float)cos(angle);
      angle = ltime * avelocities[i][1];
      sp = (float)sin(angle);
      cp = (float)cos(angle);
	
      forward[0] = cp*cy;
      forward[1] = cp*sy;
      forward[2] = -sp;

      dist = (float)sin(ltime + i)*64;
      p->x = actor->x + (int)((bytedirs[i][0]*dist + forward[0]*BEAMLENGTH)*FRACUNIT);
      p->y = actor->y + (int)((bytedirs[i][1]*dist + forward[1]*BEAMLENGTH)*FRACUNIT);
      p->z = actor->z + (int)((bytedirs[i][2]*dist + forward[2]*BEAMLENGTH)*FRACUNIT);
      P_SetParticlePosition(p);

      p->velx = p->vely = p->velz = 0;
      p->accx = p->accy = p->accz = 0;

      p->color = black;

      p->size = 4; // ???
      p->ttl = 1;
      p->trans = FRACUNIT;
      p->styleflags = 0;
   }
}

//
// P_BFGEffect
//
// Derived from Quake 2. Available under the GNU General Public License.
//
static void P_BFGEffect(Mobj *actor)
{
   int i;
   particle_t *p;
   float angle;
   float sp, sy, cp, cy;
   vec3_t forward;
   float dist = 64;
   float ltime;
	
   ltime = (float)leveltime / 30.0f;
   for(i = 0; i < NUMVERTEXNORMALS; i++)
   {
      if(!(p = newParticle()))
         break;

      angle = ltime * avelocities[i][0];
      sy = (float)sin(angle);
      cy = (float)cos(angle);
      angle = ltime * avelocities[i][1];
      sp = (float)sin(angle);
      cp = (float)cos(angle);
	
      forward[0] = cp*cy;
      forward[1] = cp*sy;
      forward[2] = -sp;
      
      dist = (float)sin(ltime + i)*64;
      p->x = actor->x + (int)((bytedirs[i][0]*dist + forward[0]*BEAMLENGTH)*FRACUNIT);
      p->y = actor->y + (int)((bytedirs[i][1]*dist + forward[1]*BEAMLENGTH)*FRACUNIT);
      p->z = actor->z + (15*FRACUNIT) + (int)((bytedirs[i][2]*dist + forward[2]*BEAMLENGTH)*FRACUNIT);
      P_SetParticlePosition(p);

      p->velx = p->vely = p->velz = 0;
      p->accx = p->accy = p->accz = 0;

      p->color = green;

      p->size = 4;
      p->ttl = 1;
      p->trans = 2*FRACUNIT/3;
      p->styleflags = PS_FULLBRIGHT;
   }
}

//
// P_DripEffect
//
// haleyjd 09/05/05: Effect for parameterized particle drip object.
//
// Parameters:
// args[0] = color (palette index)
// args[1] = size
// args[2] = frequency
// args[3] = make splash?
// args[4] = fullbright?
//
static void P_DripEffect(Mobj *actor)
{
   bool makesplash = !!actor->args[3];
   bool fullbright = !!actor->args[4];
   particle_t *p;

   // do not cause a division by zero crash or
   // allow a negative frequency
   if(actor->args[2] <= 0)
      return;

   if(leveltime % actor->args[2])
      return;

   if(!(p = newParticle()))
      return;
      
   p->ttl   = 18;
   p->trans = 9*FRACUNIT/16;
   p->fade  = p->trans / p->ttl;
   
   p->color = (byte)(actor->args[0]);
   p->size  = (byte)(actor->args[1]);
   
   p->velz = 128 * -3000;
   p->accz = -LevelInfo.gravity;
   p->styleflags = PS_FLOORCLIP | PS_FALLTOGROUND;
   if(makesplash)
      p->styleflags |= PS_SPLASH;
   if(fullbright)
      p->styleflags |= PS_FULLBRIGHT;
   p->x = actor->x;
   p->y = actor->y;
   p->z = actor->subsector->sector->srf.ceiling.height;
   P_SetParticlePosition(p);
}

//
// haleyjd 05/20/02: frame-based particle events system
//
// A field, particle_evt, has been added to the state_t structure
// to provide a numerical indicator of what type of effect a frame
// should trigger. All code related to particle events is available
// under the GNU General Public License.
//

particle_event_t particleEvents[P_EVENT_NUMEVENTS] =
{
   { nullptr,           "pevt_none"    },       // P_EVENT_NONE
   { P_RocketExplosion, "pevt_rexpl"   },       // P_EVENT_ROCKET_EXPLODE
   { P_BFGExplosion,    "pevt_bfgexpl" },       // P_EVENT_BFG_EXPLODE
};

//
// P_RunEvent
//
// Called from P_SetMobjState, immediately after the action function
// for the actor's current state has been executed.
//
void P_RunEvent(Mobj *actor)
{
   int effectNum;

   if(!drawparticles)
      return;

   // haleyjd: 
   // if actor->state is nullptr, the thing has been removed, or
   // if MIF_NOPTCLEVTS is set, don't run events for this thing
   if(!actor || !actor->state || (actor->intflags & MIF_NOPTCLEVTS))
      return;
   
   effectNum = actor->state->particle_evt;

   if(effectNum < 0 || effectNum >= P_EVENT_NUMEVENTS)
   {
      doom_printf(FC_ERROR "P_RunEvent: Particle event no. out of range");
      return;
   }

   if(effectNum != P_EVENT_NONE)
   {
      if(particleEvents[effectNum].enabled)
         particleEvents[effectNum].func(actor);
   }
}

//
// P_ExplosionParticles
//
// Causes an explosion in a customizable color. Derived from Quake 2's
// rocket/BFG burst code. Available under the GNU General Public
// License.
//
static void P_ExplosionParticles(fixed_t x, fixed_t y, fixed_t z, 
                                 byte color1, byte color2)
{
   int i, rnd;

   for(i = 0; i < 256; i++)
   {
      particle_t *p = newParticle();

      if(!p)
         break;

      p->ttl = 26;
      p->fade = FADEFROMTTL(26);
      p->trans = FRACUNIT;

      // 2^11 = 2048, 2^12 = 4096
      p->x = x + (((M_Random() % 32) - 16)*4096);
      p->y = y + (((M_Random() % 32) - 16)*4096);
      p->z = z + (((M_Random() % 32) - 16)*4096);
      P_SetParticlePosition(p);

      // note: was (rand() % 384) - 192 in Q2, but DOOM's RNG
      // only outputs numbers from 0 to 255, so it has to be
      // corrected to unbias it and get output from approx.
      // -192 to 191
      rnd = M_Random();
      p->velx = (rnd - 192 + (rnd/2))*2048;
      rnd = M_Random();
      p->vely = (rnd - 192 + (rnd/2))*2048;
      rnd = M_Random();
      p->velz = (rnd - 192 + (rnd/2))*2048;

      p->accx = p->accy = p->accz = 0;

      p->size = (M_Random() < 48) ? 6 : 4;

      p->color = (M_Random() & 0x80) ? color2 : color1;

      p->styleflags = PS_FULLBRIGHT;
   }
}

static void P_RocketExplosion(Mobj *actor)
{
   P_ExplosionParticles(actor->x, actor->y, actor->z, orange, yorange);
}

static void P_BFGExplosion(Mobj *actor)
{
   P_ExplosionParticles(actor->x, actor->y, actor->z, green, green);
}

// Generate console variables for the enabled flags on each event
void P_AddEventVars()
{
   for(int i = 1; i < P_EVENT_NUMEVENTS; i++)
   {
      variable_t *variable;
      command_t  *command;

      variable = emalloctag(variable_t *, sizeof(variable_t), PU_STATIC, nullptr);
      variable->variable  = &(particleEvents[i].enabled);
      variable->v_default = nullptr;
      variable->type      = vt_int;
      variable->min       = 0;
      variable->max       = 1;
      variable->defines   = onoff;

      command = emalloctag(command_t *, sizeof(command_t), PU_STATIC, nullptr);
      command->name     = particleEvents[i].name;
      command->type     = ct_variable;
      command->flags    = 0;
      command->variable = variable;
      command->handler  = nullptr;
      command->netcmd   = 0;

      C_AddCommand(command);
   }
}

#if 0
//
// Script functions
//

static cell AMX_NATIVE_CALL sm_ptclexplosionpos(AMX *amx, cell *params)
{
   fixed_t x, y, z;
   byte col1, col2;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   x    = (fixed_t)params[1];
   y    = (fixed_t)params[2];
   z    = (fixed_t)params[3];
   col1 = (byte)params[4];
   col2 = (byte)params[5];

   P_ExplosionParticles(x, y, z, col1, col2);

   return 0;
}

static cell AMX_NATIVE_CALL sm_ptclexplosionthing(AMX *amx, cell *params)
{
   int tid;
   byte col1, col2;
   Mobj *mo = nullptr;
   SmallContext_t *ctx = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid  = (int) params[1];
   col1 = (byte)params[2];
   col2 = (byte)params[3];

   while((mo = P_FindMobjFromTID(tid, mo, ctx->invocationData.trigger)))
      P_ExplosionParticles(mo->x, mo->y, mo->z, col1, col2);

   return 0;
}

AMX_NATIVE_INFO ptcl_Natives[] =
{
   { "_PtclExplosionPos",   sm_ptclexplosionpos   },
   { "_PtclExplosionThing", sm_ptclexplosionthing },
   { nullptr, nullptr }
};
#endif

// EOF

