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

#include "a_small.h"
#include "acs_intr.h"
#include "am_map.h"
#include "c_io.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_net.h"
#include "doomstat.h"
#include "e_edf.h"
#include "e_player.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "m_buffer.h"
#include "m_random.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "p_enemy.h"
#include "p_xenemy.h"
#include "p_portal.h"
#include "p_hubs.h"
#include "p_skin.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sndseq.h"
#include "st_stuff.h"
#include "v_misc.h"
#include "v_video.h"
#include "version.h"
#include "w_levels.h"
#include "w_wad.h"

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
// SaveArchive::SaveArchive(OutBuffer *)
//
// Constructs a SaveArchive object in saving mode.
//
SaveArchive::SaveArchive(OutBuffer *pSaveFile) 
   : savefile(pSaveFile), loadfile(NULL)
{
   if(!pSaveFile)
      I_Error("SaveArchive: created a save file without a valid OutBuffer\n");
}

//
// SaveArchive::SaveArchive(InBuffer *)
//
// Constructs a SaveArchive object in loading mode.
//
SaveArchive::SaveArchive(InBuffer *pLoadFile)
   : savefile(NULL), loadfile(pLoadFile)
{
   if(!pLoadFile)
      I_Error("SaveArchive: created a load file without a valid InBuffer\n");
}

//
// SaveArchive::ArchiveCString
//
// Writes/reads strings with a fixed maximum length, which should be 
// null-extended prior to the call.
//
void SaveArchive::ArchiveCString(char *str, size_t maxLen)
{
   if(savefile)
      savefile->Write(str, maxLen);
   else
      loadfile->Read(str, maxLen);
}

//
// SaveArchive::ArchiveLString
//
// Writes/reads a C string with prepended length field.
// When reading, the result is returned in a calloc'd buffer.
//
void SaveArchive::ArchiveLString(char *&str, size_t &len)
{
   if(savefile)
   {
      if(str)
      {
         if(!len)
            len = strlen(str) + 1;

         savefile->WriteUint32((uint32_t)len); // FIXME: size_t
         savefile->Write(str, len);
      }
      else
         savefile->WriteUint32(0);
   }
   else
   {
      uint32_t tempLen;
      loadfile->ReadUint32(tempLen);
      len = (size_t)tempLen; // FIXME: size_t
      if(len != 0)
      {
         str = ecalloc(char *, 1, len);
         loadfile->Read(str, len);
      }
      else
         str = NULL;
   }
}

//
// SaveArchive::WriteLString
//
// Writes a C string with prepended length field. Do not call when loading!
// Instead you must call ArchiveLString.
//
void SaveArchive::WriteLString(const char *str, size_t len)
{
   if(savefile)
   {
      if(!len)
         len = strlen(str) + 1;

      savefile->WriteUint32((uint32_t)len); // FIXME: size_t
      savefile->Write(str, len);
   }
   else
      I_Error("SaveArchive::WriteLString: cannot deserialize!\n");
}

//
// IO operators
//

SaveArchive &SaveArchive::operator << (int32_t &x)
{
   if(savefile)
      savefile->WriteSint32(x);
   else
      loadfile->ReadSint32(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint32_t &x)
{
   if(savefile)
      savefile->WriteUint32(x);
   else
      loadfile->ReadUint32(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (int16_t &x)
{
   if(savefile)
      savefile->WriteSint16(x);
   else
      loadfile->ReadSint16(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint16_t &x)
{
   if(savefile)
      savefile->WriteUint16(x);
   else
      loadfile->ReadUint16(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (int8_t &x)
{
   if(savefile)
      savefile->WriteSint8(x);
   else
      loadfile->ReadSint8(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint8_t &x)
{
   if(savefile)
      savefile->WriteUint8(x);
   else
      loadfile->ReadUint8(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (bool &x)
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

SaveArchive &SaveArchive::operator << (float &x)
{
   // FIXME/TODO: Fix non-portable IO.
   if(savefile)
      savefile->Write(&x, sizeof(x));
   else
      loadfile->Read(&x, sizeof(x));

   return *this;
}

SaveArchive &SaveArchive::operator << (double &x)
{
   // FIXME/TODO: Fix non-portable IO.
   if(savefile)
      savefile->Write(&x, sizeof(x));
   else
      loadfile->Read(&x, sizeof(x));

   return *this;
}

// Swizzle a serialized sector_t pointer.
SaveArchive &SaveArchive::operator << (sector_t *&s)
{
   int32_t sectornum;

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
         I_Error("SaveArchive: sector num %d out of range\n",
                 sectornum);
      }
      s = &sectors[sectornum];
   }

   return *this;
}

// Serialize a swizzled line_t pointer.
SaveArchive &SaveArchive::operator << (line_t *&ln)
{
   int32_t linenum = -1;

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
         I_Error("SaveArchive: line num %d out of range\n", linenum);
      }
      else
         ln = &lines[linenum];
   }

   return *this;
}

// Serialize a spectransfer_t structure (contained in many thinkers)
SaveArchive &SaveArchive::operator << (spectransfer_t &st)
{
   *this << st.damage << st.damageflags << st.damagemask << st.damagemod
         << st.flags << st.newspecial;
   
   return *this;
}

// Serialize a mapthing_t structure
SaveArchive &SaveArchive::operator << (mapthing_t &mt)
{
   *this << mt.angle << mt.height << mt.next << mt.options
         << mt.recordnum << mt.special << mt.tid << mt.type
         << mt.x << mt.y;

   P_ArchiveArray<int>(*this, mt.args, NUMMTARGS);

   return *this;
}

//=============================================================================
//
// Thinker Enumeration
//

static unsigned int num_thinkers; // number of thinkers in level being archived

static Thinker **thinker_p;  // killough 2/14/98: Translation table

// sf: made these into separate functions
//     for FraggleScript saving object ptrs too

static void P_FreeThinkerTable(void)
{
   efree(thinker_p);    // free translation table
}

static void P_NumberThinkers(void)
{
   Thinker *th;
   
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

static void P_DeNumberThinkers(void)
{
   Thinker *th;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
      th->setOrdinal(0);
}

// 
// P_NumForThinker
//
// Get the mobj number from the mobj.
//
unsigned int P_NumForThinker(Thinker *th)
{
   return th ? th->getOrdinal() : 0; // 0 == NULL
}

//
// P_ThinkerForNum
//
Thinker *P_ThinkerForNum(unsigned int n)
{
   return n <= num_thinkers ? thinker_p[n] : NULL;
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
static void P_ArchivePSprite(SaveArchive &arc, pspdef_t &pspr)
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
static void P_ArchivePlayers(SaveArchive &arc)
{
   for(int i = 0; i < MAXPLAYERS; i++)
   {
      // TODO: hub logic
      if(playeringame[i])
      {
         int j;
         player_t &p = players[i];

         arc << p.playerstate  << p.cmd.actions     << p.cmd.angleturn 
             << p.cmd.chatchar << p.cmd.consistency << p.cmd.forwardmove
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
static void P_ArchiveWorld(SaveArchive &arc)
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

      if(arc.isLoading())
      {
         // jff 2/22/98 now three thinker fields, not two
         sec->ceilingdata  = NULL;
         sec->floordata    = NULL;
         sec->lightingdata = NULL;
         sec->soundtarget  = NULL;

         // SoM: update the heights
         P_SetFloorHeight(sec, sec->floorheight);
         P_SetCeilingHeight(sec, sec->ceilingheight);
      }
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
// Sound Targets
//
static void P_ArchiveSoundTargets(SaveArchive &arc)
{
   int i;

   // killough 9/14/98: save soundtargets
   if(arc.isSaving())
   {
      for(i = 0; i < numsectors; i++)
      {
         Mobj *target = sectors[i].soundtarget;
         unsigned int ordinal = 0;
         if(target)
         {
            // haleyjd 11/03/06: We must check for P_MobjThinker here as well,
            // or player corpses waiting for deferred removal will be saved as
            // raw pointer values instead of twizzled numbers, causing a crash
            // on savegame load!
            ordinal = target->getOrdinal();
         }
         arc << ordinal;
      }
   }
   else
   {
      for(i = 0; i < numsectors; i++)
      {
         Mobj *target;
         unsigned int ordinal = 0;
         arc << ordinal;

         if((target = thinker_cast<Mobj *>(P_ThinkerForNum(ordinal))))
            P_SetNewTarget(&sectors[i].soundtarget, target);
         else
            sectors[i].soundtarget = NULL;
      }
   }
}

//
// Thinkers
//

#define tc_end "TC_END"

//
// P_RemoveAllThinkers
//
static void P_RemoveAllThinkers(void)
{
   Thinker *th;

   // FIXME/TODO: This leaks all mobjs til the next level by calling
   // Thinker::InitThinkers. This should really be handled more 
   // uniformly with a virtual method.

   // remove all the current thinkers
   for(th = thinkercap.next; th != &thinkercap; )
   {
      Thinker *next = th->next;

      if(th->isInstanceOf(RTTI(Mobj)))
         th->removeThinker();
      else
         delete th;
      
      th = next;
   }

   // Clear out the list
   Thinker::InitThinkers();
}

//
// P_ArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs
//
static void P_ArchiveThinkers(SaveArchive &arc)
{
   Thinker *th;

   // first, save or load count of enumerated thinkers
   arc << num_thinkers;

   if(arc.isSaving())
   {
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
      char *className = NULL;
      size_t len;
      unsigned int idx = 1; // Start at index 1, as 0 means NULL
      Thinker::Type *thinkerType;
      Thinker     *newThinker;

      // allocate thinker table
      thinker_p = ecalloc(Thinker **, num_thinkers+1, sizeof(Thinker *));

      // clear out the thinker list
      P_RemoveAllThinkers();

      while(1)
      {
         if(className)
            efree(className);

         // Get the next class name
         arc.ArchiveLString(className, len);

         // Find the ThinkerType matching this name
         if(!(thinkerType = RTTIObject::Type::FindType<Thinker::Type>(className)))
         {
            if(!strcmp(className, tc_end))
               break; // Reached end of thinker list
            else 
               I_Error("Unknown tclass %s in savegame\n", className);
         }

         // Too many thinkers?!
         if(idx > num_thinkers) 
            I_Error("P_ArchiveThinkers: too many thinkers in savegame\n");

         // Create a thinker of the appropriate type and load it
         newThinker = thinkerType->newObject();
         newThinker->serialize(arc);

         // Put it in the table
         thinker_p[idx++] = newThinker;

         // Add it
         newThinker->addThinker();
      }

      // Now, call deswizzle to fix up mutual references between thinkers, such
      // as mobj targets/tracers and ACS triggers.
      for(th = thinkercap.next; th != &thinkercap; th = th->next)
         th->deSwizzle();

      // killough 3/26/98: Spawn icon landings:
      // haleyjd  3/30/03: call P_InitThingLists
      P_InitThingLists();
   }

   // Do sound targets
   P_ArchiveSoundTargets(arc);
}

//
// killough 11/98
//
// Same as P_SetTarget() in p_tick.c, except that the target is nullified
// first, so that no old target's reference count is decreased (when loading
// savegames, old targets are indices, not really pointers to targets).
//
void P_SetNewTarget(Mobj **mop, Mobj *targ)
{
   *mop = NULL;
   P_SetTarget<Mobj>(mop, targ);
}

//
// P_ArchiveRNG
//
// killough 2/16/98: save/restore random number generator state information
//
static void P_ArchiveRNG(SaveArchive &arc)
{
   arc << rng.rndindex << rng.prndindex;

   P_ArchiveArray<unsigned int>(arc, rng.seed, NUMPRCLASS);
}

//
// P_ArchiveMap
//
// killough 2/22/98: Save/restore automap state
//
static void P_ArchiveMap(SaveArchive &arc)
{
   arc << automapactive << followplayer << automap_grid << markpointnum;

   if(markpointnum)
   {
      if(arc.isSaving())
      {
         for(int i = 0; i < markpointnum; i++)
            arc << markpoints[i].x << markpoints[i].y;
      }
      else
      {
         if(automapactive)
            AM_Start();

         while(markpointnum >= markpointnum_max)
         {
            markpointnum_max = markpointnum_max ? markpointnum_max * 2 : 16;
            markpoints = erealloc(mpoint_t *, markpoints, 
                                  sizeof *markpoints * markpointnum_max);
         }

         for(int i = 0; i < markpointnum; i++)
            arc << markpoints[i].x << markpoints[i].y;
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
// 
// haleyjd 03/26/06: PolyObject saving code
//

static void P_ArchivePolyObj(SaveArchive &arc, polyobj_t *po)
{
   int id = 0;
   angle_t angle = 0;
   PointThinker pt;

   // nullify all polyobject thinker pointers;
   // the thinkers themselves will fight over who gets the field
   // when they first start to run.
   if(arc.isLoading())
      po->thinker = NULL;

   if(arc.isSaving())
   {
      id         = po->id;
      angle      = po->angle;
      pt.x       = po->spawnSpot.x;
      pt.y       = po->spawnSpot.y;
      pt.z       = po->spawnSpot.z;
      pt.groupid = po->spawnSpot.groupid;
   }
   arc << id << angle;

   // Thinker::serialize won't read its own name so we need to do that here.
   if(arc.isLoading())
   {
      char *className = NULL;
      size_t len = 0;
      
      arc.ArchiveLString(className, len);

      if(!className || strncmp(className, "PointThinker", len))
         I_Error("P_ArchivePolyObj: no PointThinker for polyobject");
   }

   pt.serialize(arc);

   // if the object is bad or isn't in the id hash, we can do nothing more
   // with it, so return now
   if(arc.isLoading())
   {
      if((po->flags & POF_ISBAD) || po != Polyobj_GetForNum(po->id))
         return;

      // rotate and translate polyobject
      Polyobj_MoveOnLoad(po, angle, pt.x, pt.y);
   }
}

static void P_ArchivePolyObjects(SaveArchive &arc)
{
   // save number of polyobjects
   if(arc.isSaving())
      arc << numPolyObjects;
   else
   {
      int numSavedPolys = 0;

      arc << numSavedPolys;

      if(numSavedPolys != numPolyObjects)
         I_Error("P_UnArchivePolyObjects: polyobj count inconsistency\n");
   }

   for(int i = 0; i < numPolyObjects; ++i)
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

#if 0
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
static void P_ArchiveCallbacks(SaveArchive &arc)
{

   int callback_count = 0;
   
   if(arc.isSaving())
   {
      sc_callback_t *list = SM_GetCallbackList();
      sc_callback_t *rover;

      for(rover = list->next; rover != list; rover = rover->next)
         ++callback_count;

      // save number of callbacks
      arc << callback_count;

      // save off the callbacks
      for(rover = list->next; rover != list; rover = rover->next)
      {
         int8_t vm = rover->vm;

         arc << rover->flags << rover->scriptNum << vm
             << rover->wait_data << rover->wait_type;
      }
   }
   else
   {
      // kill any existing callbacks
      SM_RemoveCallbacks(-1);

      // get number of callbacks
      arc << callback_count;

      // read until the end marker is hit
      for(int i = 0; i < callback_count; i++)
      {
         sc_callback_t *newCallback = estructalloc(sc_callback_t, 1);
         int8_t vm;

         arc << newCallback->flags << newCallback->scriptNum << vm 
             << newCallback->wait_data << newCallback->wait_type;

         newCallback->vm = vm;

         // nullify pointers for maximum safety
         newCallback->next = newCallback->prev = NULL;

         // put this callback into the callback list
         SM_LinkCallback(newCallback);
      }
   }
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
void P_ArchiveScripts(SaveArchive &arc)
{
   bool haveGameScript = false, haveLevelScript = false;

   if(arc.isSaving())
   {
      haveGameScript  = gameScriptLoaded;
      haveLevelScript = levelScriptLoaded;
   }

   arc << haveGameScript;
   arc << haveLevelScript;

   // check for presence consistency
   if(arc.isLoading() &&
      ((haveGameScript  != gameScriptLoaded) ||
       (haveLevelScript != levelScriptLoaded)))
   {
      I_Error("P_UnArchiveScripts: vm presence inconsistency\n");
   }

   // save gamescript
   if(haveGameScript)
      P_ArchiveSmallAMX(arc, &GameScript.smallAMX);

   // save levelscript
   if(haveLevelScript)
      P_ArchiveSmallAMX(arc, &LevelScript.smallAMX);

   // save callbacks
   P_ArchiveCallbacks(arc);

   // TODO: execute load game event callbacks?
}
#endif

//============================================================================
//
// haleyjd 10/17/06: Sound Sequences
//

static void P_ArchiveSndSeq(SaveArchive &arc, SndSeq_t *seq)
{
   unsigned int twizzle;

   // save name of EDF sequence
   arc.ArchiveCString(seq->sequence->name, 33);

   // twizzle command pointer
   twizzle = seq->cmdPtr - seq->sequence->commands;
   arc << twizzle;

   // save origin type
   arc << seq->originType;

   // depending on origin type, either save the origin index (sector or polyobj
   // number), or an Mobj number. This differentiation is necessary because 
   // degenMobj are not covered by mobj numbering.
   switch(seq->originType)
   {
   case SEQ_ORIGIN_SECTOR_F:
   case SEQ_ORIGIN_SECTOR_C:
   case SEQ_ORIGIN_POLYOBJ:
      arc << seq->originIdx;
      break;
   case SEQ_ORIGIN_OTHER:
      twizzle = P_NumForThinker(seq->origin); // it better be a normal Mobj here!
      arc << twizzle;
      break;
   default:
      I_Error("P_ArchiveSndSeq: unknown sequence origin type %d\n", 
              seq->originType);
   }

   // save basic data
   arc << seq->delayCounter << seq->volume << seq->attenuation << seq->flags;
}

static void P_UnArchiveSndSeq(SaveArchive &arc)
{
   SndSeq_t  *newSeq;
   sector_t  *s;
   polyobj_t *po;
   Mobj      *mo;
   int twizzle = 0;
   char name[33];

   // allocate a new sound sequence
   newSeq = estructalloctag(SndSeq_t, 1, PU_LEVEL);

   // get corresponding EDF sequence
   arc.ArchiveCString(name, 33);

   if(!(newSeq->sequence = E_SequenceForName(name)))
   {
      I_Error("P_UnArchiveSndSeq: unknown EDF sound sequence %s archived\n", 
              name);
   }

   newSeq->currentSound = NULL; // not currently playing a sound

   // reset command pointer
   arc << twizzle;
   newSeq->cmdPtr = newSeq->sequence->commands + twizzle;

   // get origin type
   arc << newSeq->originType;

   // get twizzled origin index
   arc << twizzle;

   // restore pointer to origin
   switch(newSeq->originType)
   {
   case SEQ_ORIGIN_SECTOR_F:
      s = sectors + twizzle;
      newSeq->originIdx = twizzle;
      newSeq->origin = &s->soundorg;
      break;
   case SEQ_ORIGIN_SECTOR_C:
      s = sectors + twizzle;
      newSeq->originIdx = twizzle;
      newSeq->origin = &s->csoundorg;
      break;
   case SEQ_ORIGIN_POLYOBJ:
      if(!(po = Polyobj_GetForNum(twizzle)))
      {
         I_Error("P_UnArchiveSndSeq: origin at unknown polyobject %d\n", 
                 twizzle);
      }
      newSeq->originIdx = po->id;
      newSeq->origin = &po->spawnSpot;
      break;
   case SEQ_ORIGIN_OTHER:
      mo = thinker_cast<Mobj *>(P_ThinkerForNum((unsigned int)twizzle));
      newSeq->originIdx = -1;
      newSeq->origin = mo;
      break;
   default:
      I_Error("P_UnArchiveSndSeq: corrupted savegame (originType = %d)\n",
              newSeq->originType);
   }

   // restore delay counter, volume, attenuation, and flags
   arc << newSeq->delayCounter << newSeq->volume << newSeq->attenuation
       << newSeq->flags;

   // remove looping flag if present
   newSeq->flags &= ~SEQ_FLAG_LOOPING;

   // let the sound sequence code take care of putting this sequence into its 
   // proper place, as that's a complicated action that requires use of data
   // static to s_sndseq.c
   S_SetSequenceStatus(newSeq);
}

void P_ArchiveSoundSequences(SaveArchive &arc)
{
   DLListItem<SndSeq_t> *item = SoundSequences;
   int count = 0;

   // count active sound sequences (+1 if there's a running enviroseq)
   while(item)
   {
      ++count;
      item = item->dllNext;
   }

   if(EnviroSequence)
      ++count;

   // save count
   arc << count;

   // save all the normal sequences
   item = SoundSequences;
   while(item)
   {
      P_ArchiveSndSeq(arc, item->dllObject);
      item = item->dllNext;
   }

   // save enviro sequence
   if(EnviroSequence)
      P_ArchiveSndSeq(arc, EnviroSequence);
}

void P_UnArchiveSoundSequences(SaveArchive &arc)
{
   int i, count = 0;

   // stop any sequences currently playing
   S_SequenceGameLoad();

   // get sequence count
   arc << count;

   // unarchive all sequences; the sound sequence code takes care of
   // distinguishing any special sequences (such as environmental) for us.
   for(i = 0; i < count; ++i)
      P_UnArchiveSndSeq(arc);
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

void P_ArchiveButtons(SaveArchive &arc)
{
   int numsaved = 0;

   // first, save or load the number of buttons
   if(arc.isSaving())
      numsaved = numbuttonsalloc;
   
   arc << numsaved;

   // When loading, if not equal, we need to realloc buttonlist
   if(arc.isLoading() && numsaved > 0 && numsaved != numbuttonsalloc)
   {
      buttonlist = erealloc(button_t *, buttonlist, numsaved * sizeof(button_t));
      numbuttonsalloc = numsaved;
   }

   // Save the button_t's directly. They no longer contain any pointers due to
   // my recent rewrite of the button code.
   for(int i = 0; i < numsaved; i++)
   {
      arc << buttonlist[i].btexture << buttonlist[i].btimer
          << buttonlist[i].dopopout << buttonlist[i].line
          << buttonlist[i].side     << buttonlist[i].where;
   }
}

//=============================================================================
//
// haleyjd 07/06/09: ACS Save/Load
//

void P_ArchiveACS(SaveArchive &arc)
{
   ACS_Archive(arc);
}

//============================================================================
//
// Saving - Main Routine
//

#define SAVESTRINGSIZE 24

void P_SaveCurrentLevel(char *filename, char *description)
{
   int i;
   char name2[VERSIONSIZE];
   const char *fn;
   OutBuffer savefile;
   SaveArchive arc(&savefile);

   if(!savefile.CreateFile(filename, 512*1024, OutBuffer::NENDIAN))
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
      int tempskill = (int)gameskill;
      
      arc << compatibility << tempskill << inmasterlevels;
   
      // sf: use string rather than episode, map
      for(i = 0; i < 8; i++)
      {
         int8_t lvc = levelmapname[i];
         arc << lvc;
      }

      // haleyjd 06/16/10: support for saving/loading levels in managed wad
      // directories.

      if((fn = W_GetManagedDirFN(g_dir))) // returns null if g_dir == &w_GlobalDir
      {
         int len = 0;

         // save length of managed directory filename string and
         // managed directory filename string
         arc.WriteLString(fn, len);
      }
      else
      {
         // just save 0; there is no name to save
         int len = 0;
         arc << len;
      }
  
      // killough 3/16/98, 12/98: store lump name checksum
      // FIXME/TODO: Will be simple with future save format
      /*
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
      */
  
      for(i = 0; i < MAXPLAYERS; i++)
         arc << playeringame[i];

      for(; i < MIN_MAXPLAYERS; i++)         // killough 2/28/98
      {
         bool dummy = 0;
         arc << dummy;
      }

      // jff 3/17/98 save idmus state
      int tempGameType = (int)GameType;
      arc << idmusnum << tempGameType;

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
   
      P_NumberThinkers();    // turn ptrs to numbers

      P_ArchivePlayers(arc);
      P_ArchiveWorld(arc);
      P_ArchivePolyObjects(arc); // haleyjd 03/27/06
      P_ArchiveThinkers(arc);
      P_ArchiveRNG(arc);    // killough 1/18/98: save RNG information
      P_ArchiveMap(arc);    // killough 1/22/98: save automap information
      P_ArchiveSoundSequences(arc);
      P_ArchiveButtons(arc);
      P_ArchiveACS(arc);            // davidph 05/30/12

      P_DeNumberThinkers();

      uint8_t cmarker = 0xE6; // consistency marker
      arc << cmarker; 
   }
   catch(BufferedIOException)
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

//============================================================================
// 
// Loading -- Main Routine
//

void P_LoadGame(const char *filename)
{
   int i;
   char vcheck[VERSIONSIZE], vread[VERSIONSIZE];
   //uint64_t checksum, rchecksum;
   int len;
   InBuffer loadfile;
   SaveArchive arc(&loadfile);

   if(!loadfile.OpenFile(filename, 512*1024, OutBuffer::NENDIAN))
   {
      C_Printf(FC_ERROR "Failed to load savegame %s\n", filename);
      C_SetConsole();
      return;
   }

   // Enable buffered IO exceptions
   loadfile.setThrowing(true);

   try
   {
      // skip description
      char throwaway[SAVESTRINGSIZE];

      arc.ArchiveCString(throwaway, SAVESTRINGSIZE);
      
      // killough 2/22/98: "proprietary" version string :-)
      sprintf(vcheck, VERSIONID, version);

      arc.ArchiveCString(vread, VERSIONSIZE);
   
      // killough 2/22/98: Friendly savegame version difference message
      // FIXME/TODO: restore proper version verification
      if(strncmp(vread, vcheck, VERSIONSIZE))
         C_Printf(FC_ERROR "Warning: save version mismatch!\a"); // blah...

      // killough 2/14/98: load compatibility mode
      // haleyjd 06/16/10: reload "inmasterlevels" state
      int tempskill;
      arc << compatibility << tempskill << inmasterlevels;

      gameskill = (skill_t)tempskill;
      
      demo_version    = version;    // killough 7/19/98: use this version's id
      demo_subversion = subversion; // haleyjd 06/17/01   
  
      // sf: use string rather than episode, map
      for(i = 0; i < 8; i++)
      {
         int8_t lvc;
         arc << lvc;
         gamemapname[i] = (char)lvc;
      }
      gamemapname[8] = '\0'; // ending NULL

      G_SetGameMap(); // get gameepisode, map

      // start out g_dir pointing at wGlobalDir again
      g_dir = &wGlobalDir;

      // haleyjd 06/16/10: if the level was saved in a map loaded under a managed
      // directory, we need to restore the managed directory to g_dir when loading
      // the game here. When this is the case, the file name of the managed directory
      // has been saved into the save game.
      arc << len;

      if(len)
      {
         WadDirectory *dir;

         // read a name of len bytes 
         char *fn = ecalloc(char *, 1, len);
         arc.ArchiveCString(fn, (size_t)len);

         // Try to get an existing managed wad first. If none such exists, try
         // adding it now. If that doesn't work, the normal error message appears
         // for a missing wad.
         // Note: set d_dir as well, so G_InitNew won't overwrite with wGlobalDir!
         if((dir = W_GetManagedWad(fn)) || (dir = W_AddManagedWad(fn)))
            g_dir = d_dir = dir;

         // done with temporary file name
         efree(fn);
      }
   
      // killough 3/16/98, 12/98: check lump name checksum
      // FIXME/TODO: advanced savegame verification is needed
      /*
      checksum = G_Signature(g_dir);

      loadfile.Read(&rchecksum, sizeof(rchecksum));

      if(memcmp(&checksum, &rchecksum, sizeof checksum))
      {
         char *msg = ecalloc(char *, 1, strlen((const char *)(save_p + sizeof checksum)) + 128);
         strcpy(msg,"Incompatible Savegame!!!\n");
         if(save_p[sizeof checksum])
            strcat(strcat(msg,"Wads expected:\n\n"), (char *)(save_p + sizeof checksum));
         strcat(msg, "\nAre you sure?");
         C_Puts(msg);
         G_LoadGameErr(msg);
         efree(msg);
         return;
      }
      */

      for(i = 0; i < MAXPLAYERS; ++i)
         arc << playeringame[i];

      for(; i < MIN_MAXPLAYERS; i++) // killough 2/28/98
      {
         bool dummy = 0;
         arc << dummy;
      }

      // jff 3/17/98 restore idmus music
      // jff 3/18/98 account for unsigned byte
      // killough 11/98: simplify
      // haleyjd 04/14/03: game type
      // note: don't set DefaultGameType from save games
      int tempGameType;
      arc << idmusnum << tempGameType;

      GameType = (gametype_t)tempGameType;

      /* cph 2001/05/23 - Must read options before we set up the level */
      byte options[GAME_OPTION_SIZE];
      loadfile.Read(options, sizeof(options));

      G_ReadOptions(options);
 
      // load a base level
      // sf: in hubs, use g_doloadlevel instead of g_initnew
      if(hub_changelevel)
         G_DoLoadLevel();
      else
         G_InitNew(gameskill, gamemapname);

      // killough 3/1/98: Read game options
      // killough 11/98: move down to here
   
      // cph - MBF needs to reread the savegame options because 
      // G_InitNew rereads the WAD options. The demo playback code does 
      // this too.
      G_ReadOptions(options);

      // get the times
      arc << leveltime;

      // killough 11/98: load revenant tracer state
      uint8_t tracerState;
      arc << tracerState;
      basetic = gametic - tracerState;

      // haleyjd 04/14/03: load dmflags
      arc << dmflags;

      // dearchive all the modifications
      P_ArchivePlayers(arc);
      P_ArchiveWorld(arc);
      P_ArchivePolyObjects(arc);    // haleyjd 03/27/06
      P_ArchiveThinkers(arc);
      P_ArchiveRNG(arc);            // killough 1/18/98: load RNG information
      P_ArchiveMap(arc);            // killough 1/22/98: load automap information
      P_UnArchiveSoundSequences(arc);
      P_ArchiveButtons(arc);
      P_ArchiveACS(arc);            // davidph 05/30/12

      P_FreeThinkerTable();

      uint8_t cmarker;
      arc << cmarker;
      if(cmarker != 0xE6)
         I_Error("Bad savegame: last byte is 0x%x\n", cmarker);

      // haleyjd: move up Z_CheckHeap to before Z_Free (safer)
      Z_CheckHeap(); 
   }
   catch(...)
   {
      // FIXME/TODO: I hate fatal errors, don't know what to do right now.
      I_Error("P_LoadGame: Savegame read error\n");
   }

   loadfile.Close();

   if (setsizeneeded)
      R_ExecuteSetViewSize();
   
   // draw the pattern into the back screen
   R_FillBackScreen();

   // haleyjd 02/09/10: wake up status bar again
   ST_Start();

   // killough 12/98: support -recordfrom and -loadgame -playdemo
   if(!command_loadgame)
      singledemo = false;         // Clear singledemo flag if loading from menu
   else if(singledemo)
   {
      gameaction = ga_loadgame; // Mark that we're loading a game before demo
      G_DoPlayDemo();           // This will detect it and won't reinit level
   }
   else       // Loading games from menu isn't allowed during demo recordings,
      if(demorecording) // So this can only possibly be a -recordfrom command.
         G_BeginRecording(); // Start the -recordfrom, since the game was loaded.

   // sf: if loading a hub level, restore position relative to sector
   //  for 'seamless' travel between levels
   if(hub_changelevel) 
      P_RestorePlayerPosition();

   // haleyjd 01/07/07: run deferred ACS scripts
   ACS_RunDeferredScripts();
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

