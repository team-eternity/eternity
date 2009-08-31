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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Archiving: SaveGame I/O.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "m_random.h"
#include "am_map.h"
#include "p_enemy.h"
#include "p_xenemy.h"
#include "p_hubs.h"
#include "p_skin.h"
#include "p_setup.h"
#include "e_edf.h"
#include "a_small.h"
#include "s_sndseq.h"
#include "d_gi.h"
#include "acs_intr.h"

byte *save_p;

// Pads save_p to a 4-byte boundary
//  so that the load/save works on SGI&Gecko.
// #define PADSAVEP()    do { save_p += (4 - ((int) save_p & 3)) & 3; } while (0)

// sf: uncomment above for sgi and gecko if you want then
// this makes for smaller savegames
#define PADSAVEP()      {}

static unsigned int num_thinkers; // number of thinkers in level being archived

static mobj_t **mobj_p;  // killough 2/14/98: Translation table

// sf: made these into seperate functions
//     for FraggleScript saving object ptrs too

void P_FreeObjTable(void)
{
   free(mobj_p);    // free translation table
}

void P_NumberObjects(void)
{
   thinker_t *th;
   
   num_thinkers = 0; // init to 0
   
   // killough 2/14/98:
   // count the number of thinkers, and mark each one with its index, using
   // the prev field as a placeholder, since it can be restored later.

   // haleyjd 08/01/09: GCC doesn't like writing integer values into pointers,
   // and I always thought it was messy anway. Instead, store the value into
   // the new ordinal member of thinker_t.
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
      if(th->function == P_MobjThinker)
         th->ordinal = ++num_thinkers;
}

void P_DeNumberObjects(void)
{
   thinker_t *th;
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
      th->ordinal = 0;
}

// 
// P_MobjNum
//
// Get the mobj number from the mobj.
// haleyjd 10/03/03: made static
//
static unsigned int P_MobjNum(mobj_t *mo)
{
   unsigned int n = mo ? mo->thinker.ordinal : 0;   // 0 = NULL
   
   // extra check for invalid thingnum (prob. still ptr)
   if(n < 0 || n > num_thinkers) 
      n = 0;
   
   return n;
}

//
// P_MobjForNum
//
// haleyjd 10/03/03: made static
//
static mobj_t *P_MobjForNum(unsigned int n)
{
   return mobj_p[n];
}

//
// P_ArchivePlayers
//
void P_ArchivePlayers(void)
{
   int i;
   
   CheckSaveGame(sizeof(player_t) * MAXPLAYERS); // killough

   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i])
      {
         int      j;
         player_t *dest;

         PADSAVEP();
         dest = (player_t *) save_p;
         memcpy(dest, &players[i], sizeof(player_t));
         save_p += sizeof(player_t);
         for(j = 0; j < NUMPSPRITES; j++)
         {
            if(dest->psprites[j].state)
               dest->psprites[j].state =
               (state_t *)(dest->psprites[j].state->index);
         }
      }
   }
}

//
// P_UnArchivePlayers
//
void P_UnArchivePlayers(void)
{
   int i;
   
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i])
      {
         int j;

         PADSAVEP();
         
         // sf: when loading a hub level using save games,
         //     do not change the player data when crossing
         //     levels: ie. retain the same weapons etc.

         if(!hub_changelevel)
         {
            memcpy(&players[i], save_p, sizeof(player_t));
            for(j = 0; j < NUMPSPRITES; j++)
            {
               if(players[i].psprites[j].state)
                  players[i].psprites[j].state =
                   states[ (int)players[i].psprites[j].state ];
            }
         }
         
         save_p += sizeof(player_t);
         
         // will be set when unarc thinker
         players[i].mo = NULL;
         players[i].attacker = NULL;
         players[i].skin = NULL;
         players[i].pclass = NULL;
         players[i].attackdown = players[i].usedown = false;  // sf
         players[i].cmd.buttons = 0;    // sf
      }
   }
}

// haleyjd 08/30/09: lightning engine variables
extern int NextLightningFlash;
extern int LightningFlash;
extern int LevelSky;
extern int LevelTempSky;

//
// P_ArchiveWorld
//
// Saves dynamic properties of sectors, lines, and sides.
//
void P_ArchiveWorld(void)
{
   int            i;
   const sector_t *sec;
   const line_t   *li;
   const side_t   *si;
   short          *put;
   
   // killough 3/22/98: fix bug caused by hoisting save_p too early
   // killough 10/98: adjust size for changes below
   
   // haleyjd  09/00: we need to save friction & movefactor now too
   // for scripting purposes

   // haleyjd 03/04/07: must save sector colormap indices
   
   size_t size = 
      (sizeof(short)*4 + 
       sizeof(sec->floorheight) + sizeof(sec->ceilingheight) + 
       sizeof(sec->friction) + sizeof(sec->movefactor) + 
       sizeof(sec->topmap) + sizeof(sec->midmap) + sizeof(sec->bottommap) +
       sizeof(sec->flags) + sizeof(sec->intflags) + sizeof(sec->wassecret) +
       sizeof(sec->damage) + sizeof(sec->damageflags) + 
       sizeof(sec->damagemask) + sizeof(sec->damagemod) +
       sizeof(sec->ceilingpic) + sizeof(sec->floorpic))
      * numsectors + sizeof(short)*3*numlines + 4 + 4*sizeof(int);

   for(i = 0; i < numlines; ++i)
   {
      if(lines[i].sidenum[0] != -1)
         size +=
          (sizeof(short)*3 + sizeof si->textureoffset + 
           sizeof si->rowoffset);
      if(lines[i].sidenum[1] != -1)
         size +=
          (sizeof(short)*3 + sizeof si->textureoffset + 
           sizeof si->rowoffset);
   }

   CheckSaveGame(size); // killough
   
   PADSAVEP();                // killough 3/22/98
   
   put = (short *)save_p;

   // do sectors
   for(i = 0, sec = sectors; i < numsectors; ++i, ++sec)
   {
      // killough 10/98: save full floor & ceiling heights, including fraction
      memcpy(put, &sec->floorheight, sizeof(sec->floorheight));
      put = (void *)((char *) put + sizeof(sec->floorheight));
      memcpy(put, &sec->ceilingheight, sizeof(sec->ceilingheight));
      put = (void *)((char *) put + sizeof(sec->ceilingheight));

      // haleyjd: save the friction information too
      memcpy(put, &sec->friction, sizeof(sec->friction));
      put = (void *)((char *) put + sizeof(sec->friction));
      memcpy(put, &sec->movefactor, sizeof(sec->movefactor));
      put = (void *)((char *) put + sizeof(sec->movefactor));

      // haleyjd 03/04/07: save colormap indices
      memcpy(put, &sec->topmap, sizeof(sec->topmap));
      put = (void *)((char *) put + sizeof(sec->topmap));
      memcpy(put, &sec->midmap, sizeof(sec->midmap));
      put = (void *)((char *) put + sizeof(sec->midmap));
      memcpy(put, &sec->bottommap, sizeof(sec->bottommap));
      put = (void *)((char *) put + sizeof(sec->bottommap));

      // haleyjd 12/28/08: save sector flags, wassecret flag
      // haleyjd 08/30/09: intflags
      memcpy(put, &sec->flags, sizeof(sec->flags));
      put = (void *)((char *) put + sizeof(sec->flags));
      memcpy(put, &sec->intflags, sizeof(sec->intflags));
      put = (void *)((char *) put + sizeof(sec->intflags));
      memcpy(put, &sec->wassecret, sizeof(sec->wassecret));
      put = (void *)((char *) put + sizeof(sec->wassecret));

      // haleyjd 03/02/09: save sector damage properties
      memcpy(put, &sec->damage, sizeof(sec->damage));
      put = (void *)((char *) put + sizeof(sec->damage));
      memcpy(put, &sec->damageflags, sizeof(sec->damageflags));
      put = (void *)((char *) put + sizeof(sec->damageflags));
      memcpy(put, &sec->damagemask, sizeof(sec->damagemask));
      put = (void *)((char *) put + sizeof(sec->damagemask));
      memcpy(put, &sec->damagemod, sizeof(sec->damagemod));
      put = (void *)((char *) put + sizeof(sec->damagemod));

      // haleyjd 08/30/09: save floorpic/ceilingpic as ints
      memcpy(put, &sec->floorpic, sizeof(sec->floorpic));
      put = (void *)((char *)put + sizeof(sec->floorpic));
      memcpy(put, &sec->ceilingpic, sizeof(sec->ceilingpic));
      put = (void *)((char *)put + sizeof(sec->ceilingpic));

      *put++ = sec->lightlevel;
      *put++ = sec->oldlightlevel; // haleyjd
      *put++ = sec->special;       // needed?   yes -- transfer types
      *put++ = sec->tag;           // needed?   need them -- killough
   }

   // do lines
   for(i = 0, li = lines; i < numlines; ++i, ++li)
   {
      int j;

      *put++ = li->flags;
      *put++ = li->special;
      *put++ = li->tag;

      for(j = 0; j < 2; j++)
      {
         if(li->sidenum[j] != -1)
         {
            si = &sides[li->sidenum[j]];
            
            // killough 10/98: save full sidedef offsets,
            // preserving fractional scroll offsets
            
            memcpy(put, &si->textureoffset, sizeof(si->textureoffset));
            put = (void *)((char *) put + sizeof(si->textureoffset));
            memcpy(put, &si->rowoffset, sizeof(si->rowoffset));
            put = (void *)((char *) put + sizeof(si->rowoffset));
            
            *put++ = si->toptexture;
            *put++ = si->bottomtexture;
            *put++ = si->midtexture;
         }
      }
   }

   // haleyjd 08/30/09: save state of lightning engine
   memcpy(put, &NextLightningFlash, sizeof(NextLightningFlash));
   put = (void *)((char *)put + sizeof(NextLightningFlash));
   memcpy(put, &LightningFlash, sizeof(LightningFlash));
   put = (void *)((char *)put + sizeof(LightningFlash));
   memcpy(put, &LevelSky, sizeof(LevelSky));
   put = (void *)((char *)put + sizeof(LevelSky));
   memcpy(put, &LevelTempSky, sizeof(LevelTempSky));
   put = (void *)((char *)put + sizeof(LevelTempSky));

   save_p = (byte *)put;
}



//
// P_UnArchiveWorld
//
// Restores dynamic properties of sectors, lines, and sides.
//
void P_UnArchiveWorld(void)
{
   int          i;
   sector_t     *sec;
   line_t       *li;
   const short  *get;
   
   PADSAVEP();                // killough 3/22/98
   
   get = (short *)save_p;

   // do sectors
   for(i = 0, sec = sectors; i < numsectors; i++, sec++)
   {
      // killough 10/98: load full floor & ceiling heights, including fractions      
      memcpy(&sec->floorheight, get, sizeof(sec->floorheight));
      get = (void *)((char *) get + sizeof(sec->floorheight));
      memcpy(&sec->ceilingheight, get, sizeof(sec->ceilingheight));
      get = (void *)((char *) get + sizeof(sec->ceilingheight));

      // haleyjd: retrieve the friction information we now save
      memcpy(&sec->friction, get, sizeof(sec->friction));
      get = (void *)((char *) get + sizeof(sec->friction));
      memcpy(&sec->movefactor, get, sizeof(sec->movefactor));
      get = (void *)((char *) get + sizeof(sec->movefactor));

      // haleyjd 03/04/07: retrieve colormap indices
      memcpy(&sec->topmap, get, sizeof(sec->topmap));
      get = (void *)((char *) get + sizeof(sec->topmap));
      memcpy(&sec->midmap, get, sizeof(sec->midmap));
      get = (void *)((char *) get + sizeof(sec->midmap));
      memcpy(&sec->bottommap, get, sizeof(sec->bottommap));
      get = (void *)((char *) get + sizeof(sec->bottommap));

      // haleyjd 12/28/08: retrieve sector flags, wassecret flag
      // haleyjd 08/30/09: intflags
      memcpy(&sec->flags, get, sizeof(sec->flags));
      get = (void *)((char *) get + sizeof(sec->flags));
      memcpy(&sec->intflags, get, sizeof(sec->intflags));
      get = (void *)((char *) get + sizeof(sec->intflags));
      memcpy(&sec->wassecret, get, sizeof(sec->wassecret));
      get = (void *)((char *) get + sizeof(sec->wassecret));

      // haleyjd 03/02/09: retrieve sector damage info
      memcpy(&sec->damage, get, sizeof(sec->damage));
      get = (void *)((char *) get + sizeof(sec->damage));
      memcpy(&sec->damageflags, get, sizeof(sec->damageflags));
      get = (void *)((char *) get + sizeof(sec->damageflags));
      memcpy(&sec->damagemask, get, sizeof(sec->damagemask));
      get = (void *)((char *) get + sizeof(sec->damagemask));
      memcpy(&sec->damagemod, get, sizeof(sec->damagemod));
      get = (void *)((char *) get + sizeof(sec->damagemod));

      memcpy(&sec->floorpic, get, sizeof(sec->floorpic));
      get = (void *)((char *) get + sizeof(sec->floorpic));
      memcpy(&sec->ceilingpic, get, sizeof(sec->ceilingpic));
      get = (void *)((char *) get + sizeof(sec->ceilingpic));

      sec->lightlevel    = *get++;
      sec->oldlightlevel = *get++; // haleyjd
      sec->special       = *get++;
      sec->tag           = *get++;
      
      // jff 2/22/98 now three thinker fields, not two
      sec->ceilingdata  = NULL;
      sec->floordata    = NULL;
      sec->lightingdata = NULL;
      sec->soundtarget  = NULL;

      // SoM: update the heights
      P_SetFloorHeight(sec, sec->floorheight);
      P_SetCeilingHeight(sec, sec->ceilingheight);
   }

   // do lines
   for(i = 0, li = lines; i < numlines; ++i, ++li)
   {
      int j;

      li->flags = *get++;
      li->special = *get++;
      li->tag = *get++;
      for(j = 0; j < 2; j++)
      {
         if(li->sidenum[j] != -1)
         {
            side_t *si = &sides[li->sidenum[j]];
            
            // killough 10/98: load full sidedef offsets, including fractions
            
            memcpy(&si->textureoffset, get, sizeof(si->textureoffset));
            get = (void *)((char *) get + sizeof(si->textureoffset));
            memcpy(&si->rowoffset, get, sizeof(si->rowoffset));
            get = (void *)((char *) get + sizeof(si->rowoffset));
            
            si->toptexture    = *get++;
            si->bottomtexture = *get++;
            si->midtexture    = *get++;
         }
      }
   }

   // haleyjd 08/30/09: save state of lightning engine
   memcpy(&NextLightningFlash, get, sizeof(NextLightningFlash));
   get = (void *)((char *) get + sizeof(NextLightningFlash));
   memcpy(&LightningFlash, get, sizeof(LightningFlash));
   get = (void *)((char *) get + sizeof(LightningFlash));
   memcpy(&LevelSky, get, sizeof(LevelSky));
   get = (void *)((char *) get + sizeof(LevelSky));
   memcpy(&LevelTempSky, get, sizeof(LevelTempSky));
   get = (void *)((char *) get + sizeof(LevelTempSky));

   save_p = (byte *)get;
}

//
// Thinkers
//

typedef enum {
   tc_end,
   tc_mobj
} thinkerclass_t;

//
// P_ArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs

void P_ArchiveThinkers(void)
{
   thinker_t *th;
   
   CheckSaveGame(sizeof brain);   // killough 3/26/98: Save boss brain state
   memcpy(save_p, &brain, sizeof brain);
   save_p += sizeof brain;

   // check that enough room is available in savegame buffer
   CheckSaveGame(num_thinkers*(sizeof(mobj_t)+4)); // killough 2/14/98

   // save off the current thinkers
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function == P_MobjThinker)
      {
         mobj_t *mobj;
         
         *save_p++ = tc_mobj;
         PADSAVEP();
         mobj = (mobj_t *)save_p;
         memcpy(mobj, th, sizeof(*mobj));
         save_p += sizeof(*mobj);
         mobj->state = (state_t *)(mobj->state->index);

         // killough 2/14/98: convert pointers into indices.
         // Fixes many savegame problems, by properly saving
         // target and tracer fields. Note: we store NULL if
         // the thinker pointed to by these fields is not a
         // mobj thinker.
         
         if(mobj->target)
            mobj->target = (mobj_t *)
              (mobj->target->thinker.function == P_MobjThinker ?
                mobj->target->thinker.ordinal : 0);

         if(mobj->tracer)
         {
            mobj->tracer = (mobj_t *)
               (mobj->tracer->thinker.function == P_MobjThinker ?
                 mobj->tracer->thinker.ordinal : 0);
         }

         // killough 2/14/98: new field: save last known enemy. Prevents
         // monsters from going to sleep after killing monsters and not
         // seeing player anymore.

         if(mobj->lastenemy)
         {
            mobj->lastenemy = (mobj_t *)
              (mobj->lastenemy->thinker.function == P_MobjThinker ?
               mobj->lastenemy->thinker.ordinal : 0);
         }
        
         if(mobj->player)
         {
            mobj->player = (player_t *)((mobj->player-players) + 1);
         }
      }
   }

   // add a terminating marker
   *save_p++ = tc_end;
   
   // killough 9/14/98: save soundtargets
   {
      int i;
      CheckSaveGame(numsectors * sizeof(mobj_t *));       // killough 9/14/98
      for(i = 0; i < numsectors; i++)
      {
         mobj_t *target = sectors[i].soundtarget;
         if(target)
         {
            // haleyjd 11/03/06: We must check for P_MobjThinker here as well,
            // or player corpses waiting for deferred removal will be saved as
            // raw pointer values instead of twizzled numbers, causing a crash
            // on savegame load!
            target = (mobj_t *)
               (target->thinker.function == P_MobjThinker ? 
                target->thinker.ordinal : 0);
         }
         memcpy(save_p, &target, sizeof target);
         save_p += sizeof target;
      }
   }
   
   // killough 2/14/98: restore prev pointers
   // sf: still needed for saving script mobj pointers
   // killough 2/14/98: end changes
}

//
// killough 11/98
//
// Same as P_SetTarget() in p_tick.c, except that the target is nullified
// first, so that no old target's reference count is decreased (when loading
// savegames, old targets are indices, not really pointers to targets).
//
static void P_SetNewTarget(mobj_t **mop, mobj_t *targ)
{
   *mop = NULL;
   P_SetTarget(mop, targ);
}

//
// P_UnArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs
//
void P_UnArchiveThinkers(void)
{
   thinker_t *th;
   size_t    size;        // killough 2/14/98: size of into table
   size_t    idx;         // haleyjd 11/03/06: separate index var
   
   // killough 3/26/98: Load boss brain state
   memcpy(&brain, save_p, sizeof brain);
   save_p += sizeof brain;

   // remove all the current thinkers
   for(th = thinkercap.next; th != &thinkercap; )
   {
      thinker_t *next;
      next = th->next;
      if(th->function == P_MobjThinker)
         P_RemoveMobj((mobj_t *) th);
      else
         Z_Free(th);
      th = next;
   }
   P_InitThinkers();

   // killough 2/14/98: count number of thinkers by skipping through them
   {
      byte *sp = save_p;     // save pointer and skip header
      for(size = 1; *save_p++ == tc_mobj; ++size)  // killough 2/14/98
      {                     // skip all entries, adding up count
         PADSAVEP();
         save_p += sizeof(mobj_t);
      }

      if(*--save_p != tc_end)
         I_Error("Unknown tclass %i in savegame", *save_p);

      // first table entry special: 0 maps to NULL
      *(mobj_p = malloc(size * sizeof *mobj_p)) = 0;   // table of pointers
      save_p = sp;           // restore save pointer
   }

   // read in saved thinkers
   // haleyjd 11/03/06: use idx to save "size" for rangechecking
   for(idx = 1; *save_p++ == tc_mobj; ++idx)    // killough 2/14/98
   {
      mobj_t *mobj = Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL);
      
      // killough 2/14/98 -- insert pointers to thinkers into table, in order:
      mobj_p[idx] = mobj;

      PADSAVEP();
      memcpy(mobj, save_p, sizeof(mobj_t));
      save_p += sizeof(mobj_t);
      mobj->state = states[(int)(mobj->state)];
      
      // haleyjd 07/23/09: this must be before skin setting!
      mobj->info  = &mobjinfo[mobj->type];

      if(mobj->player)
      {
         int playernum = (int)mobj->player - 1;

         (mobj->player = &players[playernum])->mo = mobj;

         // PCLASS_FIXME: Need to save and restore proper player class!
         // Temporary hack.
         players[playernum].pclass = E_PlayerClassForName(GameModeInfo->defPClassName);
         
         // PCLASS_FIXME: Need to save skin and attempt to restore, then fall back
         // to default for player class if non-existant. Note: must be after player
         // class is set.
         P_SetSkin(P_GetDefaultSkin(&players[playernum]), playernum); // haleyjd
      }
      else
      {
         // haleyjd 09/26/04: restore monster skins
         // haleyjd 07/23/09: do not clear player->mo->skin (put into else)
         if(mobj->info->altsprite != NUMSPRITES)
            mobj->skin = P_GetMonsterSkin(mobj->info->altsprite);
         else
            mobj->skin = NULL;
      }

      P_SetThingPosition(mobj);      

      // killough 2/28/98:
      // Fix for falling down into a wall after savegame loaded:
      //      mobj->floorz = mobj->subsector->sector->floorheight;
      //      mobj->ceilingz = mobj->subsector->sector->ceilingheight;
      
      mobj->thinker.function = P_MobjThinker;
      P_AddThinker(&mobj->thinker);

      // haleyjd 02/02/04: possibly add thing to tid hash table
      P_AddThingTID(mobj, mobj->tid);
   }

   // killough 2/14/98: adjust target and tracer fields, plus
   // lastenemy field, to correctly point to mobj thinkers.
   // NULL entries automatically handled by first table entry.
   //
   // killough 11/98: use P_SetNewTarget() to set fields
   // haleyjd 11/03/06: rangecheck all mobj_p indices for security

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      mobj_t *mo = (mobj_t *)th;

      if((size_t)mo->target < size)
         P_SetNewTarget(&mo->target, mobj_p[(size_t)mo->target]);
      else
         mo->target = NULL;

      if((size_t)mo->tracer < size)
         P_SetNewTarget(&mo->tracer, mobj_p[(size_t)mo->tracer]);
      else
         mo->tracer = NULL;

      if((size_t)mo->lastenemy < size)
         P_SetNewTarget(&mo->lastenemy, mobj_p[(size_t)mo->lastenemy]);
      else
         mo->lastenemy = NULL;
   }

   {  // killough 9/14/98: restore soundtargets
      int i;
      for(i = 0; i < numsectors; i++)
      {
         mobj_t *target;
         memcpy(&target, save_p, sizeof target);
         save_p += sizeof target;

         // haleyjd 11/03/06: rangecheck for security
         if((size_t)target < size)
            P_SetNewTarget(&sectors[i].soundtarget, mobj_p[(size_t) target]);
         else
            sectors[i].soundtarget = NULL;
      }
   }

   // killough 3/26/98: Spawn icon landings:
   // haleyjd  3/30/03: call P_InitThingLists
   P_InitThingLists();
}

//
// P_ArchiveSpecials
//
enum {
   tc_ceiling,
   tc_door,
   tc_floor,
   tc_plat,
   tc_flash,
   tc_strobe,
   tc_glow,
   tc_elevator,      // jff 2/22/98 new elevator type thinker
   tc_scroll,        // killough 3/7/98: new scroll effect thinker
   tc_pusher,        // phares 3/22/98:  new push/pull effect thinker
   tc_flicker,       // killough 10/4/98
   tc_polyrotate,    // haleyjd 03/26/06: polyobjects
   tc_polymove,
   tc_polyslidedoor,
   tc_polyswingdoor,
   tc_pillar,        // joek: pillars
   tc_quake,         // haleyjd 02/28/07: earthquake
   tc_lightfade,     // haleyjd 02/28/07: lightfade thinker
   tc_floorwaggle,   // haleyjd 07/05/09: floor waggle
   tc_acs,           // haleyjd 07/06/09: acs
   tc_endspecials
} specials_e;

//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
// T_MoveElevator, (plat_t: sector_t *), - active list -- jff 2/22/98
// T_Scroll                                            -- killough 3/7/98
// T_Pusher                                            -- phares 3/22/98
// T_FireFlicker                                       -- killough 10/4/98
// T_PolyObjRotate                                     -- haleyjd 03/26/06:
// T_PolyObjMove                                        -     
// T_PolyDoorSlide                                      - polyobjects
// T_PolyDoorSwing                                      -     
// T_MovePillar                                        -- joek: pillars
// T_QuakeThinker                                      -- haleyjd: quakes
// T_LightFade                                         -- haleyjd: param light
// T_FloorWaggle                                       -- haleyjd: floor waggle
// T_ACSThinker                                        -- haleyjd: ACS
//

void P_ArchiveSpecials(void)
{
   thinker_t *th;
   size_t    size = 0;          // killough
   
   // save off the current thinkers (memory size calculation -- killough)
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(!th->function)
      {
         platlist_t *pl;
         ceilinglist_t *cl;     //jff 2/22/98 need this for ceilings too now
         for(pl = activeplats; pl; pl = pl->next)
         {
            if(pl->plat == (plat_t *)th)   // killough 2/14/98
            {
               size += 4+sizeof(plat_t);
               goto end;
            }
         }
         for(cl = activeceilings; cl; cl = cl->next) // search for activeceiling
         {
            if(cl->ceiling == (ceiling_t *)th)   //jff 2/22/98
            {
               size += 4+sizeof(ceiling_t);
               goto end;
            }
         }
      end:;
      }
      else
      {
         size +=
            th->function == T_MoveCeiling   ? 4 + sizeof(ceiling_t)       :
            th->function == T_VerticalDoor  ? 4 + sizeof(vldoor_t)        :
            th->function == T_MoveFloor     ? 4 + sizeof(floormove_t)     :
            th->function == T_PlatRaise     ? 4 + sizeof(plat_t)          :
            th->function == T_LightFlash    ? 4 + sizeof(lightflash_t)    :
            th->function == T_StrobeFlash   ? 4 + sizeof(strobe_t)        :
            th->function == T_Glow          ? 4 + sizeof(glow_t)          :
            th->function == T_MoveElevator  ? 4 + sizeof(elevator_t)      :
            th->function == T_Scroll        ? 4 + sizeof(scroll_t)        :
            th->function == T_Pusher        ? 4 + sizeof(pusher_t)        :
            th->function == T_FireFlicker   ? 4 + sizeof(fireflicker_t)   :
            th->function == T_PolyObjRotate ? 4 + sizeof(polyrotate_t)    :
            th->function == T_PolyObjMove   ? 4 + sizeof(polymove_t)      :
            th->function == T_PolyDoorSlide ? 4 + sizeof(polyslidedoor_t) :
            th->function == T_PolyDoorSwing ? 4 + sizeof(polyswingdoor_t) :
            th->function == T_MovePillar    ? 4 + sizeof(pillar_t)        :
            th->function == T_QuakeThinker  ? 4 + sizeof(quakethinker_t)  :
            th->function == T_LightFade     ? 4 + sizeof(lightfade_t)     :
            th->function == T_FloorWaggle   ? 4 + sizeof(floorwaggle_t)   :
            th->function == T_ACSThinker    ? 4 + sizeof(acsthinker_t)    :
            0;
      }
   }

   CheckSaveGame(size);          // killough

   // save off the current thinkers
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(!th->function)
      {
         platlist_t *pl;
         ceilinglist_t *cl;    //jff 2/22/98 add iter variable for ceilings

         // killough 2/8/98: fix plat original height bug.
         // Since acv==NULL, this could be a plat in stasis.
         // so check the active plats list, and save this
         // plat (jff: or ceiling) even if it is in stasis.

         for(pl = activeplats; pl; pl = pl->next)
         {
            if(pl->plat == (plat_t *)th)      // killough 2/14/98
               goto plat;
         }

         for(cl = activeceilings; cl; cl = cl->next)
         {
            if(cl->ceiling == (ceiling_t *)th)      //jff 2/22/98
               goto ceiling;
         }
         
         continue;
      }

      if(th->function == T_MoveCeiling)
      {
         ceiling_t *ceiling;
      ceiling:                               // killough 2/14/98
         *save_p++ = tc_ceiling;
         PADSAVEP();
         ceiling = (ceiling_t *)save_p;
         memcpy(ceiling, th, sizeof(*ceiling));
         save_p += sizeof(*ceiling);
         ceiling->sector = (sector_t *)(ceiling->sector - sectors);
         continue;
      }

      if(th->function == T_VerticalDoor)
      {
         vldoor_t *door;
         *save_p++ = tc_door;
         PADSAVEP();
         door = (vldoor_t *) save_p;
         memcpy(door, th, sizeof *door);
         save_p += sizeof(*door);
         door->sector = (sector_t *)(door->sector - sectors);
         //jff 1/31/98 archive line remembered by door as well
         door->line = (line_t *) (door->line ? door->line-lines : -1);
         continue;
      }

      if(th->function == T_MoveFloor)
      {
         floormove_t *floor;
         *save_p++ = tc_floor;
         PADSAVEP();
         floor = (floormove_t *)save_p;
         memcpy(floor, th, sizeof(*floor));
         save_p += sizeof(*floor);
         floor->sector = (sector_t *)(floor->sector - sectors);
         continue;
      }

      if(th->function == T_PlatRaise)
      {
         plat_t *plat;
      plat:   // killough 2/14/98: added fix for original plat height above
         *save_p++ = tc_plat;
         PADSAVEP();
         plat = (plat_t *)save_p;
         memcpy(plat, th, sizeof(*plat));
         save_p += sizeof(*plat);
         plat->sector = (sector_t *)(plat->sector - sectors);
         continue;
      }

      if(th->function == T_LightFlash)
      {
         lightflash_t *flash;
         *save_p++ = tc_flash;
         PADSAVEP();
         flash = (lightflash_t *)save_p;
         memcpy(flash, th, sizeof(*flash));
         save_p += sizeof(*flash);
         flash->sector = (sector_t *)(flash->sector - sectors);
         continue;
      }

      if(th->function == T_StrobeFlash)
      {
         strobe_t *strobe;
         *save_p++ = tc_strobe;
         PADSAVEP();
         strobe = (strobe_t *)save_p;
         memcpy(strobe, th, sizeof(*strobe));
         save_p += sizeof(*strobe);
         strobe->sector = (sector_t *)(strobe->sector - sectors);
         continue;
      }

      if(th->function == T_Glow)
      {
         glow_t *glow;
         *save_p++ = tc_glow;
         PADSAVEP();
         glow = (glow_t *)save_p;
         memcpy(glow, th, sizeof(*glow));
         save_p += sizeof(*glow);
         glow->sector = (sector_t *)(glow->sector - sectors);
         continue;
      }

      // killough 10/4/98: save flickers
      if(th->function == T_FireFlicker)
      {
         fireflicker_t *flicker;
         *save_p++ = tc_flicker;
         PADSAVEP();
         flicker = (fireflicker_t *)save_p;
         memcpy(flicker, th, sizeof(*flicker));
         save_p += sizeof(*flicker);
         flicker->sector = (sector_t *)(flicker->sector - sectors);
         continue;
      }

      //jff 2/22/98 new case for elevators
      if(th->function == T_MoveElevator)
      {
         elevator_t *elevator;         //jff 2/22/98
         *save_p++ = tc_elevator;
         PADSAVEP();
         elevator = (elevator_t *)save_p;
         memcpy(elevator, th, sizeof(*elevator));
         save_p += sizeof(*elevator);
         elevator->sector = (sector_t *)(elevator->sector - sectors);
         continue;
      }

      // killough 3/7/98: Scroll effect thinkers
      if(th->function == T_Scroll)
      {
         *save_p++ = tc_scroll;
         memcpy(save_p, th, sizeof(scroll_t));
         save_p += sizeof(scroll_t);
         continue;
      }

      // phares 3/22/98: Push/Pull effect thinkers

      if(th->function == T_Pusher)
      {
         *save_p++ = tc_pusher;
         memcpy(save_p, th, sizeof(pusher_t));
         save_p += sizeof(pusher_t);
         continue;
      }

      // haleyjd 03/26/06: PolyObject thinkers
      if(th->function == T_PolyObjRotate)
      {
         *save_p++ = tc_polyrotate;
         memcpy(save_p, th, sizeof(polyrotate_t));
         save_p += sizeof(polyrotate_t);
         continue;
      }

      if(th->function == T_PolyObjMove)
      {
         *save_p++ = tc_polymove;
         memcpy(save_p, th, sizeof(polymove_t));
         save_p += sizeof(polymove_t);
         continue;
      }

      if(th->function == T_PolyDoorSlide)
      {
         *save_p++ = tc_polyslidedoor;
         memcpy(save_p, th, sizeof(polyslidedoor_t));
         save_p += sizeof(polyslidedoor_t);
         continue;
      }

      if(th->function == T_PolyDoorSwing)
      {
         *save_p++ = tc_polyswingdoor;
         memcpy(save_p, th, sizeof(polyswingdoor_t));
         save_p += sizeof(polyswingdoor_t);
         continue;
      }

      if(th->function == T_MovePillar)
      {
      	 pillar_t *pillar;
         *save_p++ = tc_pillar;     
         pillar = (pillar_t *)save_p;
         memcpy(save_p, th, sizeof(pillar_t));
         save_p += sizeof(pillar_t);
         pillar->sector = (sector_t *)(pillar->sector - sectors);
         continue;
      }

      if(th->function == T_QuakeThinker)
      {
         *save_p++ = tc_quake;
         memcpy(save_p, th, sizeof(quakethinker_t));
         save_p += sizeof(quakethinker_t);
         continue;
      }

      if(th->function == T_LightFade)
      {
         lightfade_t *fade;
         *save_p++ = tc_lightfade;
         fade = (lightfade_t *)save_p;
         memcpy(save_p, th, sizeof(lightfade_t));
         save_p += sizeof(lightfade_t);
         fade->sector = (sector_t *)(fade->sector - sectors);
         continue;
      }

      if(th->function == T_FloorWaggle)
      {
         floorwaggle_t *waggle;
         *save_p++ = tc_floorwaggle;
         waggle = (floorwaggle_t *)save_p;
         memcpy(save_p, th, sizeof(floorwaggle_t));
         save_p += sizeof(floorwaggle_t);
         waggle->sector = (sector_t *)(waggle->sector - sectors);
         continue;
      }

      if(th->function == T_ACSThinker)
      {
         acsthinker_t *acs;
         *save_p++ = tc_acs;
         acs = (acsthinker_t *)save_p;
         memcpy(save_p, th, sizeof(acsthinker_t));
         save_p += sizeof(acsthinker_t);
         acs->ip      = (int *)(acs->ip - acs->code);
         acs->line    = acs->line ? (line_t *)(acs->line - lines + 1) : NULL;
         acs->trigger = (mobj_t *)P_MobjNum(acs->trigger);
         continue;
      }
   }
   
   // add a terminating marker
   *save_p++ = tc_endspecials;
}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials(void)
{
   byte tclass;
   
   // read in saved thinkers
   while((tclass = *save_p++) != tc_endspecials)  // killough 2/14/98
   {
      switch(tclass)
      {
      case tc_ceiling:
         PADSAVEP();
         {
            ceiling_t *ceiling = 
               Z_Malloc(sizeof(*ceiling), PU_LEVEL, NULL);
            memcpy(ceiling, save_p, sizeof(*ceiling));
            save_p += sizeof(*ceiling);
            ceiling->sector = &sectors[(int)ceiling->sector];
            ceiling->sector->ceilingdata = ceiling; //jff 2/22/98
            
            if(ceiling->thinker.function)
               ceiling->thinker.function = T_MoveCeiling;
            
            P_AddThinker(&ceiling->thinker);
            P_AddActiveCeiling(ceiling);
            break;
         }

      case tc_door:
         PADSAVEP();
         {
            vldoor_t *door = Z_Malloc(sizeof(*door), PU_LEVEL, NULL);
            memcpy(door, save_p, sizeof(*door));
            save_p += sizeof(*door);
            door->sector = &sectors[(int)door->sector];
            
            //jff 1/31/98 unarchive line remembered by door as well
            door->line = (int)door->line!=-1? &lines[(int)door->line] : NULL;
            
            door->sector->ceilingdata = door;       //jff 2/22/98
            door->thinker.function = T_VerticalDoor;
            P_AddThinker(&door->thinker);
            break;
         }

      case tc_floor:
         PADSAVEP();
         {
            floormove_t *floor = 
               Z_Malloc(sizeof(*floor), PU_LEVEL, NULL);
            memcpy(floor, save_p, sizeof(*floor));
            save_p += sizeof(*floor);
            floor->sector = &sectors[(int)floor->sector];
            floor->sector->floordata = floor; //jff 2/22/98
            floor->thinker.function = T_MoveFloor;
            P_AddThinker(&floor->thinker);
            break;
         }

      case tc_plat:
         PADSAVEP();
         {
            plat_t *plat = Z_Malloc(sizeof(*plat), PU_LEVEL, NULL);
            memcpy(plat, save_p, sizeof(*plat));
            save_p += sizeof(*plat);
            plat->sector = &sectors[(int)plat->sector];
            plat->sector->floordata = plat; //jff 2/22/98
            
            if(plat->thinker.function)
               plat->thinker.function = T_PlatRaise;
            
            P_AddThinker(&plat->thinker);
            P_AddActivePlat(plat);
            break;
         }

      case tc_flash:
         PADSAVEP();
         {
            lightflash_t *flash = 
               Z_Malloc(sizeof(*flash), PU_LEVEL, NULL);
            memcpy(flash, save_p, sizeof(*flash));
            save_p += sizeof(*flash);
            flash->sector = &sectors[(int)flash->sector];
            flash->thinker.function = T_LightFlash;
            P_AddThinker(&flash->thinker);
            break;
         }

      case tc_strobe:
         PADSAVEP();
         {
            strobe_t *strobe = 
               Z_Malloc(sizeof(*strobe), PU_LEVEL, NULL);
            memcpy(strobe, save_p, sizeof(*strobe));
            save_p += sizeof(*strobe);
            strobe->sector = &sectors[(int)strobe->sector];
            strobe->thinker.function = T_StrobeFlash;
            P_AddThinker(&strobe->thinker);
            break;
         }

      case tc_glow:
         PADSAVEP();
         {
            glow_t *glow = Z_Malloc(sizeof(*glow), PU_LEVEL, NULL);
            memcpy(glow, save_p, sizeof(*glow));
            save_p += sizeof(*glow);
            glow->sector = &sectors[(int)glow->sector];
            glow->thinker.function = T_Glow;
            P_AddThinker(&glow->thinker);
            break;
         }

      case tc_flicker:           // killough 10/4/98
         PADSAVEP();
         {
            fireflicker_t *flicker = 
               Z_Malloc(sizeof(*flicker), PU_LEVEL, NULL);
            memcpy(flicker, save_p, sizeof(*flicker));
            save_p += sizeof(*flicker);
            flicker->sector = &sectors[(int)flicker->sector];
            flicker->thinker.function = T_FireFlicker;
            P_AddThinker(&flicker->thinker);
            break;
         }

         //jff 2/22/98 new case for elevators
      case tc_elevator:
         PADSAVEP();
         {
            elevator_t *elevator = 
               Z_Malloc(sizeof(*elevator), PU_LEVEL, NULL);
            memcpy(elevator, save_p, sizeof(*elevator));
            save_p += sizeof(*elevator);
            elevator->sector = &sectors[(int)elevator->sector];
            elevator->sector->floordata = elevator; //jff 2/22/98
            elevator->sector->ceilingdata = elevator; //jff 2/22/98
            elevator->thinker.function = T_MoveElevator;
            P_AddThinker(&elevator->thinker);
            break;
         }

      case tc_scroll:       // killough 3/7/98: scroll effect thinkers
         {
            scroll_t *scroll = 
               Z_Malloc(sizeof(scroll_t), PU_LEVEL, NULL);
            memcpy(scroll, save_p, sizeof(scroll_t));
            save_p += sizeof(scroll_t);
            scroll->thinker.function = T_Scroll;
            P_AddThinker(&scroll->thinker);
            break;
         }

      case tc_pusher:   // phares 3/22/98: new Push/Pull effect thinkers
         {
            pusher_t *pusher = 
               Z_Malloc(sizeof(pusher_t), PU_LEVEL, NULL);
            memcpy(pusher, save_p, sizeof(pusher_t));
            save_p += sizeof(pusher_t);
            pusher->thinker.function = T_Pusher;
            pusher->source = P_GetPushThing(pusher->affectee);
            P_AddThinker(&pusher->thinker);
            break;
         }

         // haleyjd 03/26/06: PolyObjects

      case tc_polyrotate: 
         {
            polyrotate_t *polyrot =
               Z_Malloc(sizeof(polyrotate_t), PU_LEVEL, NULL);
            memcpy(polyrot, save_p, sizeof(polyrotate_t));
            save_p += sizeof(polyrotate_t);
            polyrot->thinker.function = T_PolyObjRotate;
            P_AddThinker(&polyrot->thinker);
            break;
         }

      case tc_polymove:
         {
            polymove_t *polymove =
               Z_Malloc(sizeof(polymove_t), PU_LEVEL, NULL);
            memcpy(polymove, save_p, sizeof(polymove_t));
            save_p += sizeof(polymove_t);
            polymove->thinker.function = T_PolyObjMove;
            P_AddThinker(&polymove->thinker);
            break;
         }

      case tc_polyslidedoor:
         {
            polyslidedoor_t *psldoor =
               Z_Malloc(sizeof(polyslidedoor_t), PU_LEVEL, NULL);
            memcpy(psldoor, save_p, sizeof(polyslidedoor_t));
            save_p += sizeof(polyslidedoor_t);
            psldoor->thinker.function = T_PolyDoorSlide;
            P_AddThinker(&psldoor->thinker);
            break;
         }

      case tc_polyswingdoor:
         {
            polyswingdoor_t *pswdoor =
               Z_Malloc(sizeof(polyswingdoor_t), PU_LEVEL, NULL);
            memcpy(pswdoor, save_p, sizeof(polyswingdoor_t));
            save_p += sizeof(polyswingdoor_t);
            pswdoor->thinker.function = T_PolyDoorSwing;
            P_AddThinker(&pswdoor->thinker);
            break;
         }
      
      case tc_pillar:
         {
            pillar_t *pillar = Z_Malloc(sizeof(pillar_t), PU_LEVEL, NULL);
            memcpy(pillar, save_p, sizeof(pillar_t));
            save_p += sizeof(pillar_t);
            pillar->sector = &sectors[(int)pillar->sector];
            pillar->sector->floordata = pillar; 
            pillar->sector->ceilingdata = pillar;
            pillar->thinker.function = T_MovePillar;
            P_AddThinker(&pillar->thinker);
            break;
         }

      case tc_quake:
         {
            quakethinker_t *quake = Z_Malloc(sizeof(quakethinker_t), PU_LEVEL, NULL);
            memcpy(quake, save_p, sizeof(quakethinker_t));
            save_p += sizeof(quakethinker_t);
            quake->origin.thinker.function = T_QuakeThinker;
            P_AddThinker(&(quake->origin.thinker));
            break;
         }

      case tc_lightfade:
         {
            lightfade_t *fade = Z_Malloc(sizeof(lightfade_t), PU_LEVEL, NULL);
            memcpy(fade, save_p, sizeof(lightfade_t));
            save_p += sizeof(lightfade_t);
            fade->thinker.function = T_LightFade;
            fade->sector = &sectors[(int)fade->sector];
            P_AddThinker(&fade->thinker);
            break;
         }
      
      case tc_floorwaggle:
         {
            floorwaggle_t *waggle = Z_Malloc(sizeof(floorwaggle_t), PU_LEVEL, NULL);
            memcpy(waggle, save_p, sizeof(floorwaggle_t));
            save_p += sizeof(floorwaggle_t);
            waggle->thinker.function = T_FloorWaggle;
            waggle->sector = &sectors[(int)waggle->sector];
            P_AddThinker(&waggle->thinker);
            break;
         }

      case tc_acs:
         {
            mobj_t *mo;
            acsthinker_t *acs = Z_Malloc(sizeof(acsthinker_t), PU_LEVEL, NULL);
            memcpy(acs, save_p, sizeof(acsthinker_t));
            save_p += sizeof(acsthinker_t);
            acs->thinker.function = T_ACSThinker;
            acs->line = acs->line ? &lines[(size_t)acs->line - 1] : NULL;
            mo = P_MobjForNum((int)acs->trigger);
            P_SetNewTarget(&acs->trigger, mo);
            P_AddThinker(&acs->thinker);
            ACS_RestartSavedScript(acs);
            break;
         }

      default:
         I_Error("P_UnArchiveSpecials: Unknown tclass %i in savegame", tclass);
      }
   }
}

// killough 2/16/98: save/restore random number generator state information

void P_ArchiveRNG(void)
{
   CheckSaveGame(sizeof rng);
   memcpy(save_p, &rng, sizeof rng);
   save_p += sizeof rng;
}

void P_UnArchiveRNG(void)
{
   memcpy(&rng, save_p, sizeof rng);
   save_p += sizeof rng;
}

// killough 2/22/98: Save/restore automap state
void P_ArchiveMap(void)
{
   CheckSaveGame(sizeof followplayer + sizeof markpointnum +
                 markpointnum * sizeof *markpoints +
                 sizeof automapactive);

   memcpy(save_p, &automapactive, sizeof automapactive);
   save_p += sizeof automapactive;
   memcpy(save_p, &followplayer, sizeof followplayer);
   save_p += sizeof followplayer;
   memcpy(save_p, &automap_grid, sizeof automap_grid);
   save_p += sizeof automap_grid;
   memcpy(save_p, &markpointnum, sizeof markpointnum);
   save_p += sizeof markpointnum;

   if(markpointnum)
   {
      memcpy(save_p, markpoints, sizeof *markpoints * markpointnum);
      save_p += markpointnum * sizeof *markpoints;
   }
}

void P_UnArchiveMap(void)
{
   if(!hub_changelevel) 
      memcpy(&automapactive, save_p, sizeof automapactive);
   save_p += sizeof automapactive;
   if(!hub_changelevel) 
      memcpy(&followplayer, save_p, sizeof followplayer);
   save_p += sizeof followplayer;
   if(!hub_changelevel) 
      memcpy(&automap_grid, save_p, sizeof automap_grid);
   save_p += sizeof automap_grid;

   if(automapactive)
      AM_Start();

   memcpy(&markpointnum, save_p, sizeof markpointnum);
   save_p += sizeof markpointnum;

   if(markpointnum)
   {
      while(markpointnum >= markpointnum_max)
      {
         markpoints = realloc(markpoints, sizeof *markpoints *
            (markpointnum_max = markpointnum_max ? markpointnum_max*2 : 16));
      }
      memcpy(markpoints, save_p, markpointnum * sizeof *markpoints);
      save_p += markpointnum * sizeof *markpoints;
   }
}

///////////////////////////////////////////////////////////////////////////////
// 
// haleyjd 03/26/06: PolyObject saving code
//

static void P_ArchivePolyObj(polyobj_t *po)
{
   size_t poSize = sizeof(po->id) + sizeof(po->angle) + sizeof(po->spawnSpot);

   CheckSaveGame(poSize);

   memcpy(save_p, &po->id, sizeof(po->id));
   save_p += sizeof(po->id);

   memcpy(save_p, &po->angle, sizeof(po->angle));
   save_p += sizeof(po->angle);

   memcpy(save_p, &po->spawnSpot, sizeof(po->spawnSpot));
   save_p += sizeof(po->spawnSpot);
}

static void P_UnArchivePolyObj(polyobj_t *po)
{
   int id;
   unsigned int angle;
   degenmobj_t spawnSpot;

   // nullify all polyobject thinker pointers;
   // the thinkers themselves will fight over who gets the field
   // when they first start to run.
   po->thinker = NULL;

   memcpy(&id, save_p, sizeof(id));
   save_p += sizeof(id);

   memcpy(&angle, save_p, sizeof(angle));
   save_p += sizeof(angle);

   memcpy(&spawnSpot, save_p, sizeof(spawnSpot));
   save_p += sizeof(spawnSpot);

   // if the object is bad or isn't in the id hash, we can do nothing more
   // with it, so return now
   if(po->isBad || po != Polyobj_GetForNum(id))
      return;

   // rotate and translate polyobject
   Polyobj_MoveOnLoad(po, angle, spawnSpot.x, spawnSpot.y);
}

void P_ArchivePolyObjects(void)
{
   int i;

   CheckSaveGame(sizeof(numPolyObjects));

   // save number of polyobjects
   memcpy(save_p, &numPolyObjects, sizeof(numPolyObjects));
   save_p += sizeof(numPolyObjects);

   for(i = 0; i < numPolyObjects; ++i)
      P_ArchivePolyObj(&PolyObjects[i]);
}

void P_UnArchivePolyObjects(void)
{
   int i, numSavedPolys;

   memcpy(&numSavedPolys, save_p, sizeof(numSavedPolys));
   save_p += sizeof(numSavedPolys);

   if(numSavedPolys != numPolyObjects)
      I_Error("P_UnArchivePolyObjects: polyobj count inconsistency\n");

   for(i = 0; i < numSavedPolys; ++i)
      P_UnArchivePolyObj(&PolyObjects[i]);
}

/*******************************
                SCRIPT SAVING
 *******************************/

// This comment saved for nostalgia purposes:

// haleyjd 11/23/00: Here I sit at 4:07 am on Thanksgiving of the
// year 2000 attempting to finish up a rewrite of the FraggleScript
// higher architecture. ::sighs::
//

// haleyjd 05/24/04: Small savegame support!

//
// P_ArchiveSmallAMX
//
// Saves the size and contents of a Small AMX's data segment.
//
static void P_ArchiveSmallAMX(AMX *amx)
{
   long amx_size = 0;
   byte *data;

   // get both size of and pointer to data segment
   data = SM_GetAMXDataSegment(amx, &amx_size);

   // check for enough room to save both the size and 
   // the whole data segment
   CheckSaveGame(sizeof(long) + amx_size);

   // write the size
   memcpy(save_p, &amx_size, sizeof(long));
   save_p += sizeof(long);

   // write the data segment
   memcpy(save_p, data, amx_size);
   save_p += amx_size;
}

//
// P_UnArchiveSmallAMX
//
// Restores a Small AMX's data segment to the archived state.
// The existing data segment will be checked for size consistency
// with the archived one, and it'll bomb out if there's a conflict.
// This will avoid most problems with maps that have had their
// scripts recompiled since last being used.
//
static void P_UnArchiveSmallAMX(AMX *amx)
{
   long cur_amx_size, arch_amx_size;
   byte *data;

   // get pointer to AMX data segment and current data segment size
   data = SM_GetAMXDataSegment(amx, &cur_amx_size);

   // read the archived size
   memcpy(&arch_amx_size, save_p, sizeof(long));
   save_p += sizeof(long);

   // make sure the archived data segment is consistent with the
   // existing one (which was loaded by P_SetupLevel, or always
   // exists in the case of a gamescript)

   if(arch_amx_size != cur_amx_size)
      I_Error("P_UnArchiveSmallAMX: data segment consistency error\n");

   // copy the archived data segment into the VM
   memcpy(data, save_p, arch_amx_size);
   save_p += arch_amx_size;
}

//
// P_ArchiveCallbacks
//
// Archives the Small callback list, which is maintained in
// a_small.c -- the entire callback structures are saved, but
// the next and prev pointer values will not be used on restore.
// The order of callbacks is insignificant, and therefore they
// will simply be relinked at runtime in their archived order.
// The list of callbacks is terminated with a single byte value
// equal to SC_VM_END.
//
// FIXME: Order may be important after all, at least for the
// -recordfrom command.
//
static void P_ArchiveCallbacks(void)
{
   int callback_count = 0;
   sc_callback_t *list = SM_GetCallbackList();
   sc_callback_t *rover;

   for(rover = list->next; rover != list; rover = rover->next)
      ++callback_count;

   // check for enough room for all the callbacks plus an end marker
   CheckSaveGame(callback_count * sizeof(sc_callback_t) + sizeof(char));

   // save off the callbacks
   for(rover = list->next; rover != list; rover = rover->next)
   {
      memcpy(save_p, rover, sizeof(sc_callback_t));
      save_p += sizeof(sc_callback_t);
   }

   // save an end marker
   *save_p++ = SC_VM_END;
}

//
// P_UnArchiveCallbacks
//
// Kills any existing Small callbacks, then unarchives and links
// in any saved callbacks.
//
// FIXME: restore callbacks to the order they were saved in.
// Why? -recordfrom command.
//
static void P_UnArchiveCallbacks(void)
{
   // kill any existing callbacks
   SM_RemoveCallbacks(-1);

   // read until the end marker is hit
   while(*save_p != SC_VM_END)
   {
      sc_callback_t *newCallback = malloc(sizeof(sc_callback_t));

      memcpy(newCallback, save_p, sizeof(sc_callback_t));

      // nullify pointers for maximum safety
      newCallback->next = newCallback->prev = NULL;

      // put this callback into the callback list
      SM_LinkCallback(newCallback);

      save_p += sizeof(sc_callback_t);
   }
   
   // move past the last sentinel byte
   ++save_p;
}

//=============================================================================
//
// Script saving functions
//

//
// P_ArchiveScripts
//
// Saves the presence of the gamescript and levelscript, then
// saves them if they exist.  Any scheduled callbacks are then
// saved.
//
void P_ArchiveScripts(void)
{
   CheckSaveGame(2 * sizeof(unsigned char));

   // save gamescript/levelscript presence flags
   *save_p++ = (unsigned char)gameScriptLoaded;
   *save_p++ = (unsigned char)levelScriptLoaded;

   // save gamescript
   if(gameScriptLoaded)
      P_ArchiveSmallAMX(&GameScript.smallAMX);

   // save levelscript
   if(levelScriptLoaded)
      P_ArchiveSmallAMX(&LevelScript.smallAMX);

   // save callbacks
   P_ArchiveCallbacks();
}

//
// P_UnArchiveScripts
//
// Unarchives any saved gamescript or levelscript. If one was
// saved, but the corresponding script VM doesn't currently exist,
// there's a script state consistency problem, and the game will
// bomb out.  Any archived callbacks are then restored.
//
void P_UnArchiveScripts(void)
{
   boolean hadGameScript, hadLevelScript;

   // get saved presence flags
   hadGameScript  = *save_p++;
   hadLevelScript = *save_p++;

   // check for presence consistency
   if((hadGameScript && !gameScriptLoaded) ||
      (hadLevelScript && !levelScriptLoaded))
      I_Error("P_UnArchiveScripts: vm presence inconsistency\n");

   // restore gamescript
   if(hadGameScript)
      P_UnArchiveSmallAMX(&GameScript.smallAMX);

   // restore levelscript
   if(hadLevelScript)
      P_UnArchiveSmallAMX(&LevelScript.smallAMX);

   // restore callbacks
   P_UnArchiveCallbacks();

   // TODO: execute load game event callbacks?
}

//============================================================================
//
// haleyjd 10/17/06: Sound Sequences
//

static void P_ArchiveSndSeq(SndSeq_t *seq)
{
   int twizzle;

   CheckSaveGame(33 + 7*sizeof(int));

   // save name of EDF sequence
   memcpy(save_p, seq->sequence->name, 33);
   save_p += 33;

   // twizzle command pointer
   twizzle = seq->cmdPtr - seq->sequence->commands;
   memcpy(save_p, &twizzle, sizeof(int));
   save_p += sizeof(int);

   // save origin type
   memcpy(save_p, &seq->originType, sizeof(int));
   save_p += sizeof(int);

   // depending on origin type, either save the origin index (sector or polyobj
   // number), or an mobj_t number. This differentiation is necessary because 
   // degenmobj_t are not covered by mobj numbering.
   switch(seq->originType)
   {
   case SEQ_ORIGIN_SECTOR_F:
   case SEQ_ORIGIN_SECTOR_C:
   case SEQ_ORIGIN_POLYOBJ:
      memcpy(save_p, &seq->originIdx, sizeof(int));
      break;
   case SEQ_ORIGIN_OTHER:
      twizzle = P_MobjNum(seq->origin); // it better be a normal mobj_t here!
      memcpy(save_p, &twizzle, sizeof(int));
      break;
   default:
      I_Error("P_ArchiveSndSeq: unknown sequence origin type %d\n", 
              seq->originType);
   }

   save_p += sizeof(int);

   // save delay counter
   memcpy(save_p, &seq->delayCounter, sizeof(int));
   save_p += sizeof(int);

   // save current volume level
   memcpy(save_p, &seq->volume, sizeof(int));
   save_p += sizeof(int);

   // save current attenuation parameter
   memcpy(save_p, &seq->attenuation, sizeof(int));
   save_p += sizeof(int);

   // save flags
   memcpy(save_p, &seq->flags, sizeof(int));
   save_p += sizeof(int);
}

static void P_UnArchiveSndSeq(void)
{
   SndSeq_t  *newSeq;
   sector_t  *s;
   polyobj_t *po;
   mobj_t    *mo;
   int twizzle;
   char name[33];

   // allocate a new sound sequence
   newSeq = Z_Malloc(sizeof(SndSeq_t), PU_LEVEL, NULL);

   // get corresponding EDF sequence
   memcpy(name, save_p, 33);
   save_p += 33;

   if(!(newSeq->sequence = E_SequenceForName(name)))
      I_Error("P_UnArchiveSndSeq: unknown EDF sound sequence %s archived\n", 
              name);

   newSeq->currentSound = NULL; // not currently playing a sound

   // reset command pointer
   memcpy(&twizzle, save_p, sizeof(int));
   save_p += sizeof(int);
   newSeq->cmdPtr = newSeq->sequence->commands + twizzle;

   // get origin type
   memcpy(&newSeq->originType, save_p, sizeof(int));
   save_p += sizeof(int);

   // get twizzled origin index
   memcpy(&twizzle, save_p, sizeof(int));
   save_p += sizeof(int);

   // restore pointer to origin
   switch(newSeq->originType)
   {
   case SEQ_ORIGIN_SECTOR_F:
      s = sectors + twizzle;
      newSeq->originIdx = twizzle;
      newSeq->origin = (mobj_t *)&s->soundorg;
      break;
   case SEQ_ORIGIN_SECTOR_C:
      s = sectors + twizzle;
      newSeq->originIdx = twizzle;
      newSeq->origin = (mobj_t *)&s->csoundorg;
      break;
   case SEQ_ORIGIN_POLYOBJ:
      if(!(po = Polyobj_GetForNum(twizzle)))
         I_Error("P_UnArchiveSndSeq: origin at unknown polyobject %d\n", 
                 twizzle);
      newSeq->originIdx = po->id;
      newSeq->origin = (mobj_t *)&po->spawnSpot;
      break;
   case SEQ_ORIGIN_OTHER:
      mo = P_MobjForNum((unsigned int)twizzle);
      newSeq->originIdx = -1;
      newSeq->origin = mo;
      break;
   default:
      I_Error("P_UnArchiveSndSeq: corrupted savegame (originType = %d)\n",
              newSeq->originType);
   }

   // restore delay counter
   memcpy(&newSeq->delayCounter, save_p, sizeof(int));
   save_p += sizeof(int);

   // restore current volume
   memcpy(&newSeq->volume, save_p, sizeof(int));
   save_p += sizeof(int);

   // restore current attenuation
   memcpy(&newSeq->attenuation, save_p, sizeof(int));
   save_p += sizeof(int);

   // restore flags and remove looping flag if present
   memcpy(&newSeq->flags, save_p, sizeof(int));
   save_p += sizeof(int);

   newSeq->flags &= ~SEQ_FLAG_LOOPING;

   // let the sound sequence code take care of putting this sequence into its 
   // proper place, as that's a complicated action that requires use of data
   // static to s_sndseq.c
   S_SetSequenceStatus(newSeq);
}

void P_ArchiveSoundSequences(void)
{
   SndSeq_t *seq = SoundSequences;
   int count = 0;

   CheckSaveGame(sizeof(int));

   // count active sound sequences (+1 if there's a running enviroseq)
   while(seq)
   {
      ++count;
      seq = (SndSeq_t *)(seq->link.next);
   }

   if(EnviroSequence)
      ++count;

   // save count
   memcpy(save_p, &count, sizeof(int));
   save_p += sizeof(int);

   // save all the normal sequences
   seq = SoundSequences;
   while(seq)
   {
      P_ArchiveSndSeq(seq);
      seq = (SndSeq_t *)(seq->link.next);
   }

   // save enviro sequence
   if(EnviroSequence)
      P_ArchiveSndSeq(EnviroSequence);
}

void P_UnArchiveSoundSequences(void)
{
   int i, count;

   // stop any sequences currently playing
   S_SequenceGameLoad();

   // get sequence count
   memcpy(&count, save_p, sizeof(int));
   save_p += sizeof(int);

   // unarchive all sequences; the sound sequence code takes care of
   // distinguishing any special sequences (such as environmental) for us.
   for(i = 0; i < count; ++i)
      P_UnArchiveSndSeq();
}

//============================================================================
//
// haleyjd 04/17/08: Button saving
//
// Since Doom 0.99, buttons have never been saved in savegames, presumably
// because id didn't know how to twizzle the line and sector pointers they
// stored in the structure (but maybe they just forgot about it altogether).
// This meant that switches scheduled to pop out at the time the game is saved
// never did so after loading the save. No longer!
//

void P_ArchiveButtons(void)
{
   CheckSaveGame(sizeof(int) + numbuttonsalloc * sizeof(button_t));

   // first, save number of buttons
   memcpy(save_p, &numbuttonsalloc, sizeof(int));
   save_p += sizeof(int);

   // Save the button_t's directly. They no longer contain any pointers due to
   // my recent rewrite of the button code.
   if(numbuttonsalloc)
   {
      memcpy(save_p, buttonlist, numbuttonsalloc * sizeof(button_t));
      save_p += numbuttonsalloc * sizeof(button_t);
   }
}

void P_UnArchiveButtons(void)
{
   int numsaved;

   // get number allocated when the game was saved
   memcpy(&numsaved, save_p, sizeof(int));
   save_p += sizeof(int);

   // if not equal, we need to realloc buttonlist
   if(numsaved != numbuttonsalloc)
   {
      buttonlist = realloc(buttonlist, numsaved * sizeof(button_t));
      numbuttonsalloc = numsaved;
   }

   // copy the buttons from the save
   memcpy(buttonlist, save_p, numsaved * sizeof(button_t));
   save_p += numsaved * sizeof(button_t);
}

//=============================================================================
//
// haleyjd 07/06/09: ACS Save/Load
//

void P_ArchiveACS(void)
{
   // save map vars
   // save world vars
   // TODO: save deferred scripts
}

void P_UnArchiveACS(void)
{
   // load map vars
   // load world vars (TODO: not on hub transfer)
   // TODO: load deferred scripts (TODO: not on hub transfer)
}

//----------------------------------------------------------------------------
//
// $Log: p_saveg.c,v $
// Revision 1.17  1998/05/03  23:10:22  killough
// beautification
//
// Revision 1.16  1998/04/19  01:16:06  killough
// Fix boss brain spawn crashes after loadgames
//
// Revision 1.15  1998/03/28  18:02:17  killough
// Fix boss spawner savegame crash bug
//
// Revision 1.14  1998/03/23  15:24:36  phares
// Changed pushers to linedef control
//
// Revision 1.13  1998/03/23  03:29:54  killough
// Fix savegame crash caused in P_ArchiveWorld
//
// Revision 1.12  1998/03/20  00:30:12  phares
// Changed friction to linedef control
//
// Revision 1.11  1998/03/09  07:20:23  killough
// Add generalized scrollers
//
// Revision 1.10  1998/03/02  12:07:18  killough
// fix stuck-in wall loadgame bug, automap status
//
// Revision 1.9  1998/02/24  08:46:31  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.8  1998/02/23  04:49:42  killough
// Add automap marks and properties to saved state
//
// Revision 1.7  1998/02/23  01:02:13  jim
// fixed elevator size, comments
//
// Revision 1.4  1998/02/17  05:43:33  killough
// Fix savegame crashes and monster sleepiness
// Save new RNG info
// Fix original plats height bug
//
// Revision 1.3  1998/02/02  22:17:55  jim
// Extended linedef types
//
// Revision 1.2  1998/01/26  19:24:21  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

