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

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "d_dehtbl.h"
#include "d_main.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "m_buffer.h"
#include "m_random.h"
#include "am_map.h"
#include "p_enemy.h"
#include "p_xenemy.h"
#include "p_portal.h"
#include "p_hubs.h"
#include "p_skin.h"
#include "p_setup.h"
#include "e_edf.h"
#include "a_small.h"
#include "s_sndseq.h"
#include "d_gi.h"
#include "acs_intr.h"
#include "e_player.h"
#include "version.h"
#include "w_wad.h"
#include "w_levels.h"

byte *save_p;


// Pads save_p to a 4-byte boundary
//  so that the load/save works on SGI&Gecko.
// #define PADSAVEP()    do { save_p += (4 - ((int) save_p & 3)) & 3; } while (0)

// sf: uncomment above for sgi and gecko if you want then
// this makes for smaller savegames
#define PADSAVEP()      {}

//=============================================================================
//
// Basic IO
//

//
// CSaveArchive::CSaveArchive(COutBuffer *)
//
// Constructs a CSaveArchive object in saving mode.
//
CSaveArchive::CSaveArchive(COutBuffer *pSaveFile) 
   : savefile(pSaveFile), loadfile(NULL)
{
   if(!pSaveFile)
      I_Error("CSaveArchive: created a save file without a valid COutBuffer\n");
}

//
// CSaveArchive::CSaveArchive(CInBuffer *)
//
// Constructs a CSaveArchive object in loading mode.
//
CSaveArchive::CSaveArchive(CInBuffer *pLoadFile)
   : savefile(NULL), loadfile(pLoadFile)
{
   if(!pLoadFile)
      I_Error("CSaveArchive: created a load file without a valid CInBuffer\n");
}

//
// CSaveArchive::ArchiveCString
//
// Writes/reads strings with a fixed maximum length, which should be 
// null-extended prior to the call.
//
void CSaveArchive::ArchiveCString(char *str, size_t maxLen)
{
   if(savefile)
      savefile->Write(str, maxLen);
   else
      loadfile->Read(str, maxLen);
}

//
// CSaveArchive::ArchiveLString
//
// Writes/reads a C string with prepended length field.
// When reading, the result is returned in a calloc'd buffer.
//
void CSaveArchive::ArchiveLString(char *&str, size_t &len)
{
   if(savefile)
   {
      if(!len)
         len = strlen(str) + 1;

      savefile->WriteUint32((uint32_t)len); // FIXME: size_t
      savefile->Write(str, len);
   }
   else
   {
      uint32_t tempLen;
      loadfile->ReadUint32(tempLen);
      len = (size_t)tempLen; // FIXME: size_t
      if(len != 0)
      {
         str = (char *)(calloc(1, len));
         loadfile->Read(str, len);
      }
      else
         str = NULL;
   }
}

//
// CSaveArchive::WriteLString
//
// Writes a C string with prepended length field. Do not call when loading!
// Instead you must call ArchiveLString.
//
void CSaveArchive::WriteLString(const char *str, size_t len)
{
   if(savefile)
   {
      if(!len)
         len = strlen(str) + 1;

      savefile->WriteUint32((uint32_t)len); // FIXME: size_t
      savefile->Write(str, len);
   }
   else
      I_Error("CSaveArchive::WriteLString: cannot deserialize!\n");
}

//
// IO operators
//

CSaveArchive &CSaveArchive::operator << (int32_t &x)
{
   if(savefile)
      savefile->WriteSint32(x);
   else
      loadfile->ReadSint32(x);

   return *this;
}

CSaveArchive &CSaveArchive::operator << (uint32_t &x)
{
   if(savefile)
      savefile->WriteUint32(x);
   else
      loadfile->ReadUint32(x);

   return *this;
}

CSaveArchive &CSaveArchive::operator << (int16_t &x)
{
   if(savefile)
      savefile->WriteSint16(x);
   else
      loadfile->ReadSint16(x);

   return *this;
}

CSaveArchive &CSaveArchive::operator << (uint16_t &x)
{
   if(savefile)
      savefile->WriteUint16(x);
   else
      loadfile->ReadUint16(x);

   return *this;
}

CSaveArchive &CSaveArchive::operator << (int8_t &x)
{
   if(savefile)
      savefile->WriteSint8(x);
   else
      loadfile->ReadSint8(x);

   return *this;
}

CSaveArchive &CSaveArchive::operator << (uint8_t &x)
{
   if(savefile)
      savefile->WriteUint8(x);
   else
      loadfile->ReadUint8(x);

   return *this;
}

CSaveArchive &CSaveArchive::operator << (boolean &x)
{
   if(savefile)
      savefile->WriteUint8((uint8_t)x);
   else
   {
      uint8_t temp;
      loadfile->ReadUint8(temp);
      x = !!temp;
   }

   return *this;
}

CSaveArchive &CSaveArchive::operator << (sector_t *&s)
{
   int sectornum;

   if(savefile)
   {
      sectornum = s - sectors;
      savefile->WriteSint32(sectornum);
   }
   else
   {
      loadfile->ReadSint32(sectornum);
      if(sectornum < 0 || sectornum >= numsectors)
      {
         I_FatalError(I_ERR_KILL, "CSaveArchive: sector num %d out of range\n",
                      sectornum);
      }
      s = &sectors[sectornum];
   }

   return *this;
}

CSaveArchive &CSaveArchive::operator << (line_t *&ln)
{
   int linenum = -1;

   if(savefile)
   {
      if(ln)
         linenum = ln - lines;
      savefile->WriteSint32(linenum);
   }
   else
   {
      loadfile->ReadSint32(linenum);
      if(linenum == -1) // Some line pointers can be NULL
         ln = NULL;
      else if(linenum < 0 || linenum >= numlines)
      {
         I_FatalError(I_ERR_KILL, "CSaveArchive: line num %d out of range\n",
                      linenum);
      }
      else
         ln = &lines[linenum];
   }

   return *this;
}

CSaveArchive &CSaveArchive::operator << (spectransfer_t &st)
{
   *this << st.damage << st.damageflags << st.damagemask << st.damagemod
         << st.flags << st.newspecial;
   
   return *this;
}

//=============================================================================
//
// Thinker Enumeration
//

static unsigned int num_thinkers; // number of thinkers in level being archived

static CThinker **thinker_p;  // killough 2/14/98: Translation table

// sf: made these into separate functions
//     for FraggleScript saving object ptrs too

void P_FreeThinkerTable(void)
{
   free(thinker_p);    // free translation table
}

void P_NumberThinkers(void)
{
   CThinker *th;
   
   num_thinkers = 0; // init to 0
   
   // killough 2/14/98:
   // count the number of thinkers, and mark each one with its index, using
   // the prev field as a placeholder, since it can be restored later.

   // haleyjd 11/26/10: Replaced with virtual enumeration facility
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      th->setOrdinal(num_thinkers + 1);
      if(th->getOrdinal() == num_thinkers + 1) // if accepted, increment
         ++num_thinkers;
   }
}

void P_DeNumberThinkers(void)
{
   CThinker *th;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
      th->setOrdinal(0);
}

// 
// P_ThinkerNum
//
// Get the mobj number from the mobj.
// haleyjd 10/03/03: made static
//
static unsigned int P_ThinkerNum(CThinker *th)
{
   unsigned int n = th ? th->getOrdinal() : 0; // 0 = NULL
   
   // extra check for invalid thingnum (prob. still ptr)
   if(n > num_thinkers) 
      n = 0;
   
   return n;
}

//
// P_ThinkerForNum
//
// haleyjd 10/03/03: made static
//
static CThinker *P_ThinkerForNum(unsigned int n)
{
   return thinker_p[n];
}

//=============================================================================
//
// Game World Saving and Loading
//

//
// P_ArchivePSprite
//
// haleyjd 12/06/10: Archiving for psprites
//
static void P_ArchivePSprite(CSaveArchive &arc, pspdef_t &pspr)
{
   arc << pspr.sx << pspr.sy << pspr.tics << pspr.trans;

   if(arc.isSaving())
   {
      int statenum = pspr.state ? pspr.state->index : -1;
      arc << statenum;
   }
   else
   {
      int statenum = -1;
      arc << statenum;
      if(statenum != -1) // TODO: bounds-check!
         pspr.state = states[statenum];
      else
         pspr.state = NULL;
   }
}

//
// P_ArchivePlayers
//
static void P_ArchivePlayers(CSaveArchive &arc)
{
   int i;
   
   for(i = 0; i < MAXPLAYERS; i++)
   {
      // TODO: hub logic
      if(playeringame[i])
      {
         int j;
         player_t &p = players[i];

         arc << p.playerstate  << p.cmd.actions     << p.cmd.angleturn 
             << p.cmd.chatchar << p.cmd.consistancy << p.cmd.forwardmove 
             << p.cmd.look     << p.cmd.sidemove    << p.viewz
             << p.viewheight   << p.deltaviewheight << p.bob
             << p.pitch        << p.momx            << p.momy
             << p.health       << p.armorpoints     << p.armortype
             << p.hereticarmor << p.backpack        << p.totalfrags
             << p.readyweapon  << p.pendingweapon   << p.extralight
             << p.cheats       << p.refire          << p.killcount
             << p.itemcount    << p.secretcount     << p.didsecret
             << p.damagecount  << p.bonuscount      << p.fixedcolormap
             << p.colormap     << p.curpsprite      << p.quake
             << p.jumptime;
         
         for(j = 0; j < NUMPOWERS; j++)
            arc << p.powers[j];

         for(j = 0; j < NUMCARDS; j++)
            arc << p.cards[j];

         for(j = 0; j < MAXPLAYERS; j++)
            arc << p.frags[j];

         for(j = 0; j < NUMWEAPONS; j++)
            arc << p.weaponowned[j];

         for(j = 0; j < NUMAMMO; j++)
            arc << p.ammo[j] << p.maxammo[j];

         for(j = 0; j < NUMWEAPONS; j++)
         {
            for(int k = 0; k < 3; k++)
               arc << p.weaponctrs[j][k];
         }

         for(j = 0; j < NUMPSPRITES; j++)
            P_ArchivePSprite(arc, p.psprites[j]);

         arc.ArchiveCString(p.name, 20);

         if(arc.isLoading())
         {
            // will be set when unarc thinker
            p.mo          = NULL;
            p.attacker    = NULL;
            p.skin        = NULL;
            p.pclass      = NULL;
            p.attackdown  = false; // sf
            p.usedown     = false; // sf
            p.cmd.buttons = 0;     // sf
         }
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
static void P_ArchiveWorld(CSaveArchive &arc)
{
   int       i;
   sector_t *sec;
   line_t   *li;
   side_t   *si;
  
   // do sectors
   for(i = 0, sec = sectors; i < numsectors; ++i, ++sec)
   {
      // killough 10/98: save full floor & ceiling heights, including fraction
      // haleyjd: save the friction information too
      // haleyjd 03/04/07: save colormap indices
      // haleyjd 12/28/08: save sector flags
      // haleyjd 08/30/09: intflags
      // haleyjd 03/02/09: save sector damage properties
      // haleyjd 08/30/09: save floorpic/ceilingpic as ints

      arc << sec->floorheight << sec->ceilingheight 
          << sec->friction << sec->movefactor  
          << sec->topmap << sec->midmap << sec->bottommap
          << sec->flags << sec->intflags 
          << sec->damage << sec->damageflags << sec->damagemask << sec->damagemod
          << sec->floorpic << sec->ceilingpic
          << sec->lightlevel << sec->oldlightlevel 
          << sec->special << sec->tag; // needed?   yes -- transfer types -- killough
   }

   // do lines
   for(i = 0, li = lines; i < numlines; ++i, ++li)
   {
      int j;

      arc << li->flags << li->special << li->tag;

      for(j = 0; j < 2; j++)
      {
         if(li->sidenum[j] != -1)
         {
            si = &sides[li->sidenum[j]];
            
            // killough 10/98: save full sidedef offsets,
            // preserving fractional scroll offsets
            
            arc << si->textureoffset << si->rowoffset
                << si->toptexture << si->bottomtexture << si->midtexture;
         }
      }
   }

   // killough 3/26/98: Save boss brain state
   arc << brain.easy;

   // haleyjd 08/30/09: save state of lightning engine
   arc << NextLightningFlash << LightningFlash << LevelSky << LevelTempSky;
}

//
// Thinkers
//

#define tc_end "TC_END"

//
// P_ArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs
//
static void P_ArchiveThinkers(CSaveArchive &arc)
{
   if(arc.isSaving())
   {
      CThinker *th;

      // first, save count of enumerated thinkers
      arc << num_thinkers;
      
      // save off the current thinkers
      for(th = thinkercap.next; th != &thinkercap; th = th->next)
      {
         if(th->shouldSerialize())
            th->serialize(arc);
      }

      // add a terminating marker
      arc.WriteLString(tc_end);
   }
   else
   {
      char *className = "";
      size_t len;
      unsigned int idx = 1; // Start at index 1, as 0 means NULL
      CThinkerType *thinkerType;
      CThinker     *newThinker, *th;

      // get number of thinkers
      arc << num_thinkers;

      // allocate thinker table
      thinker_p = (CThinker **)(calloc(num_thinkers+1, sizeof(CThinker *)));

      while(1)
      {
         if(className != "")
            free(className);

         // Get the next class name
         arc.ArchiveLString(className, len);

         // Find the CThinkerType matching this name
         if(!(thinkerType = CThinkerType::FindType(className)))
         {
            if(!strcmp(className, tc_end))
               break; // Reached end of thinker list
            else 
               I_FatalError(I_ERR_KILL, "Unknown tclass %s in savegame\n", className);
         }

         // Create a thinker of the appropriate type and load it
         newThinker = thinkerType->newThinker();
         newThinker->serialize(*this);

         // Put it in the table
         thinker_p[idx++] = newThinker;

         // Add it
         newThinker->addThinker();
      }

      // Now, call deswizzle to fix up mutual references between thinkers, such
      // as mobj targets/tracers and ACS triggers.
      for(th = thinkercap.next; th != &thinkercap; th = th->next)
         th->deswizzle();
   }

   /*
   CThinker *th;
   int i;

   // save off the current thinkers
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(thinker_cast<mobj_t *>(th))
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
            mobj->target = (mobj_t *)(mobj->target->getEnumeration());

         if(mobj->tracer)
            mobj->tracer = (mobj_t *)(mobj->tracer->getEnumeration());

         // killough 2/14/98: new field: save last known enemy. Prevents
         // monsters from going to sleep after killing monsters and not
         // seeing player anymore.
         if(mobj->lastenemy)
            mobj->lastenemy = (mobj_t *)(mobj->lastenemy->getEnumeration());
        
         if(mobj->player)
            mobj->player = (player_t *)((mobj->player-players) + 1);
      }
   }

   // add a terminating marker
   savefile.WriteUint8(tc_end);
   
   // killough 9/14/98: save soundtargets
   for(i = 0; i < numsectors; i++)
   {
      mobj_t *target = sectors[i].soundtarget;
      unsigned int ordinal = 0;
      if(target)
      {
         // haleyjd 11/03/06: We must check for P_MobjThinker here as well,
         // or player corpses waiting for deferred removal will be saved as
         // raw pointer values instead of twizzled numbers, causing a crash
         // on savegame load!
         ordinal = target->getEnumeration();
      }
      arc << ordinal;
   }
   
   // killough 2/14/98: restore prev pointers
   // sf: still needed for saving script mobj pointers
   // killough 2/14/98: end changes
   */
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
   P_SetTarget<mobj_t>(mop, targ);
}

//
// P_UnArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs
//
void P_UnArchiveThinkers(void)
{
   CThinker *th;
   size_t    size;        // killough 2/14/98: size of into table
   size_t    idx;         // haleyjd 11/03/06: separate index var

   // remove all the current thinkers
   for(th = thinkercap.next; th != &thinkercap; )
   {
      CThinker *next;
      next = th->next;
      if(th->function == P_MobjThinker)
         P_RemoveMobj((mobj_t *) th);
      else
         Z_Free(th);
      th = next;
   }
   CThinker::InitThinkers();

   // killough 2/14/98: count number of thinkers by skipping through them
   {
      byte *sp = save_p;     // save pointer and skip header
      for(size = 1; *save_p++ == tc_mobj; ++size)  // killough 2/14/98
      {                     // skip all entries, adding up count
         PADSAVEP();
         save_p += sizeof(mobj_t);
      }

      if(*--save_p != tc_end)
         I_FatalError(I_ERR_KILL, "Unknown tclass %i in savegame", *save_p);

      // first table entry special: 0 maps to NULL
      *(mobj_p = (mobj_t **)(malloc(size * sizeof *mobj_p))) = 0;   // table of pointers
      save_p = sp;           // restore save pointer
   }

   // read in saved thinkers
   // haleyjd 11/03/06: use idx to save "size" for rangechecking
   for(idx = 1; *save_p++ == tc_mobj; ++idx)    // killough 2/14/98
   {
      mobj_t *mobj = (mobj_t *)(Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL));
      
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
         if(mobj->info->altsprite != -1)
            mobj->skin = P_GetMonsterSkin(mobj->info->altsprite);
         else
            mobj->skin = NULL;
      }

      P_SetThingPosition(mobj);      

      // killough 2/28/98:
      // Fix for falling down into a wall after savegame loaded:
      //      mobj->floorz = mobj->subsector->sector->floorheight;
      //      mobj->ceilingz = mobj->subsector->sector->ceilingheight;
      
      mobj->addThinker();

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
enum 
{
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
// T_MoveCeiling, (CCeiling: sector_t * swizzle), - active list
// T_VerticalDoor, (CVerticalDoor: sector_t * swizzle),
// T_MoveFloor, (CFloorMove: sector_t * swizzle),
// T_LightFlash, (CLightFlash: sector_t * swizzle),
// T_StrobeFlash, (CStrobeThinker: sector_t *),
// T_Glow, (CGlowThinker: sector_t *),
// T_PlatRaise, (CPlat: sector_t *), - active list
// T_MoveElevator, (CPlat: sector_t *), - active list -- jff 2/22/98
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

void P_ArchiveSpecials(COutBuffer &savefile)
{
   CThinker *th;
   
   // save off the current thinkers
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
   }
   
   // add a terminating marker
   savefile.WriteUint8(tc_endspecials);
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
      default:
         // haleyjd: savegame read errors can cause heap corruption, so trigger
         // a fatal error exit.
         I_FatalError(I_ERR_KILL,
                      "P_UnArchiveSpecials: Unknown tclass %i in savegame", 
                      tclass);
      }
   }
}

// killough 2/16/98: save/restore random number generator state information

void P_ArchiveRNG(void)
{
   memcpy(save_p, &rng, sizeof rng);
   save_p += sizeof rng;
}

void P_UnArchiveRNG(void)
{
   memcpy(&rng, save_p, sizeof rng);
   save_p += sizeof rng;
}

//
// P_ArchiveMap
//
// killough 2/22/98: Save/restore automap state
//
static void P_ArchiveMap(CSaveArchive &arc)
{
   arc << automapactive << followplayer << automap_grid << markpointnum;

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
         markpoints = (mpoint_t *)(realloc(markpoints, sizeof *markpoints *
            (markpointnum_max = markpointnum_max ? markpointnum_max*2 : 16)));
      }
      memcpy(markpoints, save_p, markpointnum * sizeof *markpoints);
      save_p += markpointnum * sizeof *markpoints;
   }
}

///////////////////////////////////////////////////////////////////////////////
// 
// haleyjd 03/26/06: PolyObject saving code
//

static void P_ArchivePolyObj(CSaveArchive &arc, polyobj_t *po)
{
   // nullify all polyobject thinker pointers;
   // the thinkers themselves will fight over who gets the field
   // when they first start to run.
   if(arc.isLoading())
      po->thinker = NULL;

   arc << po->id << po->angle;

   // TODO: CPointThinker
   memcpy(save_p, &po->spawnSpot, sizeof(po->spawnSpot));
   save_p += sizeof(po->spawnSpot);

   // if the object is bad or isn't in the id hash, we can do nothing more
   // with it, so return now
   if(arc.isLoading())
   {
      if((po->flags & POF_ISBAD) || po != Polyobj_GetForNum(po->id))
         return;

      // rotate and translate polyobject
      Polyobj_MoveOnLoad(po, angle, spawnSpot.x, spawnSpot.y);
   }
}

static void P_ArchivePolyObjects(CSaveArchive &arc)
{
   int i;

   // save number of polyobjects
   if(arc.isSaving())
      arc << numPolyObjects;
   else
   {
      int numSavedPolys = 0;

      arc << numSavedPolys;

      if(numSavedPolys != numPolyObjects)
      {
         I_FatalError(I_ERR_KILL,
            "P_UnArchivePolyObjects: polyobj count inconsistency\n");
      }
   }

   for(i = 0; i < numPolyObjects; ++i)
      P_ArchivePolyObj(arc, &PolyObjects[i]);
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
#ifndef EE_NO_SMALL_SUPPORT
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
#endif

//
// P_UnArchiveSmallAMX
//
// Restores a Small AMX's data segment to the archived state.
// The existing data segment will be checked for size consistency
// with the archived one, and it'll bomb out if there's a conflict.
// This will avoid most problems with maps that have had their
// scripts recompiled since last being used.
//
#ifndef EE_NO_SMALL_SUPPORT
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
   {
      I_FatalError(I_ERR_KILL,
         "P_UnArchiveSmallAMX: data segment consistency error\n");
   }

   // copy the archived data segment into the VM
   memcpy(data, save_p, arch_amx_size);
   save_p += arch_amx_size;
}
#endif

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
#ifndef EE_NO_SMALL_SUPPORT
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
#endif
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
#ifndef EE_NO_SMALL_SUPPORT
   // kill any existing callbacks
   SM_RemoveCallbacks(-1);

   // read until the end marker is hit
   while(*save_p != SC_VM_END)
   {
      sc_callback_t *newCallback = (sc_callback_t *)(malloc(sizeof(sc_callback_t)));

      memcpy(newCallback, save_p, sizeof(sc_callback_t));

      // nullify pointers for maximum safety
      newCallback->next = newCallback->prev = NULL;

      // put this callback into the callback list
      SM_LinkCallback(newCallback);

      save_p += sizeof(sc_callback_t);
   }
   
   // move past the last sentinel byte
   ++save_p;
#endif
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
#ifndef EE_NO_SMALL_SUPPORT
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
#endif
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
#ifndef EE_NO_SMALL_SUPPORT
   boolean hadGameScript, hadLevelScript;

   // get saved presence flags
   hadGameScript  = *save_p++;
   hadLevelScript = *save_p++;

   // check for presence consistency
   if((hadGameScript && !gameScriptLoaded) ||
      (hadLevelScript && !levelScriptLoaded))
   {
      I_FatalError(I_ERR_KILL,
         "P_UnArchiveScripts: vm presence inconsistency\n");
   }

   // restore gamescript
   if(hadGameScript)
      P_UnArchiveSmallAMX(&GameScript.smallAMX);

   // restore levelscript
   if(hadLevelScript)
      P_UnArchiveSmallAMX(&LevelScript.smallAMX);

   // restore callbacks
   P_UnArchiveCallbacks();

   // TODO: execute load game event callbacks?
#endif
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
   newSeq = (SndSeq_t *)(Z_Malloc(sizeof(SndSeq_t), PU_LEVEL, NULL));

   // get corresponding EDF sequence
   memcpy(name, save_p, 33);
   save_p += 33;

   if(!(newSeq->sequence = E_SequenceForName(name)))
   {
      I_FatalError(I_ERR_KILL,
         "P_UnArchiveSndSeq: unknown EDF sound sequence %s archived\n", 
         name);
   }

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
      {
         I_FatalError(I_ERR_KILL,
            "P_UnArchiveSndSeq: origin at unknown polyobject %d\n", 
            twizzle);
      }
      newSeq->originIdx = po->id;
      newSeq->origin = (mobj_t *)&po->spawnSpot;
      break;
   case SEQ_ORIGIN_OTHER:
      mo = P_MobjForNum((unsigned int)twizzle);
      newSeq->originIdx = -1;
      newSeq->origin = mo;
      break;
   default:
      I_FatalError(I_ERR_KILL,
         "P_UnArchiveSndSeq: corrupted savegame (originType = %d)\n",
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
      buttonlist = (button_t *)(realloc(buttonlist, numsaved * sizeof(button_t)));
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

//============================================================================
//
// Saving - Main Routine
//

#define SAVESTRINGSIZE 24

void P_SaveCurrentLevel(char *filename, char *description)
{
   int  length, i;
   char name2[VERSIONSIZE];
   const char *fn;
   COutBuffer savefile;
   CSaveArchive arc(&savefile);

   if(!savefile.CreateFile(filename, 512*1024, COutBuffer::NENDIAN))
   {
      const char *str =
         errno ? strerror(errno) : FC_ERROR "Could not save game: Error unknown";
      doom_printf("%s", str);
      return;
   }

   // Enable buffered IO exceptions
   savefile.setThrowing(true);

   try
   {
      arc.ArchiveCString(description, SAVESTRINGSIZE);
      
      // killough 2/22/98: "proprietary" version string :-)
      memset(name2, 0, sizeof(name2));
      sprintf(name2, VERSIONID, version);
   
      arc.ArchiveCString(name2, VERSIONSIZE);
   
      // killough 2/14/98: save old compatibility flag:
      // haleyjd 06/16/10: save "inmasterlevels" state
      arc << compatibility << gameskill << inmasterlevels;
   
      // sf: use string rather than episode, map
      for(i = 0; i < 8; i++)
         arc << levelmapname[i];

      // haleyjd 06/16/10: support for saving/loading levels in managed wad
      // directories.

      if((fn = W_GetManagedDirFN(g_dir))) // returns null if g_dir == &w_GlobalDir
      {
         int len = 0;

         // save length of managed directory filename string and
         // managed directory filename string
         arc.ArchiveLString(fn, len);
      }
      else
      {
         // just save 0; there is no name to save
         int len = 0;
         arc << len;
      }
  
      // killough 3/16/98, 12/98: store lump name checksum
      uint64_t checksum = G_Signature(g_dir);
      savefile.Write(&checksum, sizeof(checksum));

      // killough 3/16/98: store pwad filenames in savegame      
      for(wfileadd_t *file = wadfiles; file->filename; ++file)
      {
         const char *fn = file->filename;
         savefile.Write(fn, strlen(fn));
         savefile.WriteUint8((uint8_t)'\n');
      }
      savefile.WriteUint8(0);
  
      for(i = 0; i < MAXPLAYERS; i++)
         arc << playeringame[i];

      for(; i < MIN_MAXPLAYERS; i++)         // killough 2/28/98
      {
         int dummy = 0;
         arc << dummy;
      }

      // jff 3/17/98 save idmus state
      arc << idmusnum << GameType;

      byte options[GAME_OPTION_SIZE];
      G_WriteOptions(options);    // killough 3/1/98: save game options
      savefile.Write(options, sizeof(options));
   
      //killough 11/98: save entire word
      arc << leveltime;
   
      // killough 11/98: save revenant tracer state
      uint8_t tracerState = (uint8_t)((gametic-basetic) & 255);
      arc << tracerState;

      arc << dmflags;
   
      // killough 3/22/98: add Z_CheckHeap after each call to ensure consistency
      // haleyjd 07/06/09: just Z_CheckHeap after the end. This stuff works by now.
   
      P_NumberObjects();    // turn ptrs to numbers

      P_ArchivePlayers(arc);
      P_ArchiveWorld(arc);
      P_ArchivePolyObjects(arc); // haleyjd 03/27/06
      P_ArchiveThinkers(arc);
      P_ArchiveSpecials();
      P_ArchiveRNG();    // killough 1/18/98: save RNG information
      P_ArchiveMap(arc);    // killough 1/22/98: save automap information
      P_ArchiveScripts();   // sf: archive scripts
      P_ArchiveSoundSequences();
      P_ArchiveButtons();

      P_DeNumberObjects();

      uint8_t cmarker = 0xE6; // consistancy marker
      arc << cmarker; 
   }
   catch(CBufferedIOException)
   {
      // An IO error occurred while trying to save.
      const char *str =
         errno ? strerror(errno) : FC_ERROR "Could not save game: Error unknown";
      doom_printf("%s", str);

      // Close the file and remove it
      savefile.setThrowing(false);
      savefile.Close();
      remove(filename);
      return;
   }

   // Close the save file
   savefile.Close();

   // Check the heap.
   Z_CheckHeap();

   if(!hub_changelevel) // sf: no 'game saved' message for hubs
      doom_printf("%s", DEH_String("GGSAVED"));  // Ty 03/27/98 - externalized
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

