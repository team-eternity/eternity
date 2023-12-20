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
//      Archiving: SaveGame I/O.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"
#include "am_map.h"
#include "a_common.h"
#include "c_io.h"
#include "d_dehtbl.h"
#include "d_files.h"
#include "d_net.h"
#include "doomstat.h"
#include "e_inventory.h"
#include "e_weapons.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "m_buffer.h"
#include "p_info.h"
#include "p_spec.h"
#include "p_saveg.h"
#include "p_saveid.h"
#include "p_enemy.h"
#include "p_portal.h"
#include "p_hubs.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "s_musinfo.h"
#include "s_sndseq.h"
#include "st_stuff.h"
#include "v_misc.h"
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
// Constructs a SaveArchive object in saving mode.
//
SaveArchive::SaveArchive(OutBuffer *pSaveFile) 
   : savefile(pSaveFile), loadfile(nullptr), read_save_version(0)
{
   if(!pSaveFile)
      I_Error("SaveArchive: created a save file without a valid OutBuffer\n");
}

//
// Constructs a SaveArchive object in loading mode.
//
SaveArchive::SaveArchive(InBuffer *pLoadFile)
   : savefile(nullptr), loadfile(pLoadFile), read_save_version(0)
{
   if(!pLoadFile)
      I_Error("SaveArchive: created a load file without a valid InBuffer\n");
}

//
// Writes/reads strings with a fixed maximum length, which should be 
// null-extended prior to the call.
//
void SaveArchive::archiveCString(char *str, size_t maxLen)
{
   if(savefile)
      savefile->write(str, maxLen);
   else
      loadfile->read(str, maxLen);
}

//
// Writes/reads a C string with prepended length field.
// When reading, the result is returned in a calloc'd buffer.
//
void SaveArchive::archiveLString(char *&str, size_t &len)
{
   if(savefile)
   {
      if(str)
      {
         if(!len)
            len = strlen(str) + 1;

         archiveSize(len);
         savefile->write(str, len);
      }
      else
         savefile->writeUint32(0);
   }
   else
   {
      archiveSize(len);
      if(len != 0)
      {
         str = ecalloc(char *, 1, len);
         loadfile->read(str, len);
      }
      else
         str = nullptr;
   }
}

//
// Attempts to read save version, returning false if it fails
//
bool SaveArchive::readSaveVersion()
{
   char vread[VERSIONSIZE];

   if(!loadfile)
      I_Error("SaveArchive::readSaveVersion: cannot read version if not loading file!\n");

   archiveCString(vread, VERSIONSIZE);

   if(!strncmp(vread, "MBF ", 4))
   {
      const long tempver = strtol(vread + 4, nullptr, 10);
      if(tempver != 401)
      {
         C_Printf(FC_ERROR "Warning: Save too old to be read (pre 4.01.00)!\a");
         return false;
      }

      read_save_version = 0;
   }
   else if(strncmp(vread, "EE", 2))
   {
      C_Printf(FC_ERROR "Warning: unsupported save format!\a"); // blah...
      return false;
   }
   else
      loadfile->readSint32(read_save_version);

   return true;
}

//
// Writes save version
//
void SaveArchive::writeSaveVersion()
{
   if(!savefile)
      I_Error("SaveArchive::writeSaveVersion: cannot save version if not writing file!\n");

   savefile->writeSint32(WRITE_SAVE_VERSION);
}

//
// Writes a C string with prepended length field. Do not call when loading!
// Instead you must call archiveLString.
//
void SaveArchive::writeLString(const char *str, size_t len)
{
   if(savefile)
   {
      if(!len)
         len = strlen(str) + 1;

      archiveSize(len);
      savefile->write(str, len);
   }
   else
      I_Error("SaveArchive::writeLString: cannot deserialize!\n");
}

//
// Archives a string that will be cached. Any equivalent values will only be referenced by an integer ID
//
void SaveArchive::archiveCachedString(qstring &string)
{
   if(isSaving())
   {
      // look up strings by content to get their ID
      CachedString *cache = mStringTable.objectForKey(string.constPtr());
      if(!cache)
      {
         cache = new CachedString;
         mCacheStringHolder.add(cache);

         cache->identifier = mNextCachedStringID;
         cache->string = string;
         ++mNextCachedStringID;
         mStringTable.addObject(cache);

         // Write both the new ID and the content
         auto localID = static_cast<int32_t>(cache->identifier);
         *this << localID;
         qstring localString = string;
         localString.archive(*this);
      }
      else
      {
         // We have it cached already
         auto localID = static_cast<int32_t>(cache->identifier);
         *this << localID;
      }
   }
   else
   {
      // Loading
      int32_t id;
      *this << id;
      CachedString *cache = mIdTable.objectForKey(id);   // look up strings by ID
      if(!cache)
      {
         cache = new CachedString;
         mCacheStringHolder.add(cache);

         cache->identifier = id;
         cache->string.archive(*this);
         mIdTable.addObject(cache);

         string = cache->string;
      }
      else
      {
         string = cache->string;
      }
   }
}

#define SAVE_MIN_SIZEOF_SIZE_T 4
#define SAVE_MAX_SIZEOF_SIZE_T 8

//
// Archive a size_t
//
void SaveArchive::archiveSize(size_t &value)
{
   static_assert(sizeof(size_t) == SAVE_MIN_SIZEOF_SIZE_T || 
                 sizeof(size_t) == SAVE_MAX_SIZEOF_SIZE_T, "Unsupported sizeof(size_t)");

   uint64_t uv = uint64_t(value);
   if(savefile)
      savefile->writeUint64(uv);
   else
   {
      loadfile->readUint64(uv);
#if SIZE_MAX < UINT64_MAX
      if(uv > SIZE_MAX)
         I_Error("Cannot load save game: size_t value out of range on this platform\n");
#endif
      value = size_t(uv);
   }
}

//
// IO operators
//

SaveArchive &SaveArchive::operator << (int64_t &x)
{
   if(savefile)
      savefile->writeSint64(x);
   else
      loadfile->readSint64(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint64_t &x)
{
   if(savefile)
      savefile->writeUint64(x);
   else
      loadfile->readUint64(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (int32_t &x)
{
   if(savefile)
      savefile->writeSint32(x);
   else
      loadfile->readSint32(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint32_t &x)
{
   if(savefile)
      savefile->writeUint32(x);
   else
      loadfile->readUint32(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (int16_t &x)
{
   if(savefile)
      savefile->writeSint16(x);
   else
      loadfile->readSint16(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint16_t &x)
{
   if(savefile)
      savefile->writeUint16(x);
   else
      loadfile->readUint16(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (int8_t &x)
{
   if(savefile)
      savefile->writeSint8(x);
   else
      loadfile->readSint8(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (uint8_t &x)
{
   if(savefile)
      savefile->writeUint8(x);
   else
      loadfile->readUint8(x);

   return *this;
}

SaveArchive &SaveArchive::operator << (bool &x)
{
   if(savefile)
      savefile->writeUint8((uint8_t)x);
   else
   {
      uint8_t temp;
      loadfile->readUint8(temp);
      x = !!temp;
   }

   return *this;
}

SaveArchive &SaveArchive::operator << (float &x)
{
   // FIXME/TODO: Fix non-portable IO.
   if(savefile)
      savefile->write(&x, sizeof(x));
   else
      loadfile->read(&x, sizeof(x));

   return *this;
}

SaveArchive &SaveArchive::operator << (double &x)
{
   // FIXME/TODO: Fix non-portable IO.
   if(savefile)
      savefile->write(&x, sizeof(x));
   else
      loadfile->read(&x, sizeof(x));

   return *this;
}

// Swizzle a serialized sector_t pointer.
SaveArchive &SaveArchive::operator << (sector_t *&s)
{
   int32_t sectornum;

   if(savefile)
   {
      sectornum = int32_t(s - sectors);
      savefile->writeSint32(sectornum);
   }
   else
   {
      loadfile->readSint32(sectornum);
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
         linenum = int32_t(ln - lines);
      savefile->writeSint32(linenum);
   }
   else
   {
      loadfile->readSint32(linenum);
      if(linenum == -1) // Some line pointers can be nullptr
         ln = nullptr;
      else if(linenum < 0 || linenum >= numlinesPlusExtra)
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
   *this << st.damage << st.damageflags << st.leakiness << st.damagemask
         << st.damagemod << st.flags << st.newspecial;
   
   return *this;
}

// Serialize a mapthing_t structure
SaveArchive &SaveArchive::operator << (mapthing_t &mt)
{
   // ioanch 20151218: add extended options
   *this << mt.angle << mt.height << mt.next << mt.options << mt.extOptions
         << mt.recordnum << mt.special << mt.tid << mt.type
         << mt.x << mt.y << mt.healthModifier;

   P_ArchiveArray<int>(*this, mt.args, NUMMTARGS);

   return *this;
}

// Serialize an inventory_t structure
SaveArchive &SaveArchive::operator << (inventoryslot_t &slot)
{
   *this << slot.amount << slot.item;
   return *this;
}

// Serializes a vector
SaveArchive &SaveArchive::operator << (v2fixed_t &vec)
{
   *this << vec.x << vec.y;
   return *this;
}

// Serialize a z plane reference
SaveArchive &SaveArchive::operator << (zrefs_t &zref)
{
   *this << zref.floor << zref.ceiling << zref.dropoff << zref.secfloor << zref.secceil
         << zref.passfloor << zref.passceil;
   if(saveVersion() >= 15)
   {
      int32_t val;
      if(isSaving())
         val = zref.sector.floor ? eindex(zref.sector.floor - ::sectors) : -1;
      *this << val;
      if(isLoading())
         zref.sector.floor = val >= 0 ? sectors + val : nullptr;
   }
   if(saveVersion() >= 19)
   {
      int32_t val;
      if(isSaving())
         val = zref.sector.ceiling ? eindex(zref.sector.ceiling - ::sectors) : -1;
      *this << val;
      if(isLoading())
         zref.sector.ceiling = val >= 0 ? sectors + val : nullptr;
   }
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

static void P_FreeThinkerTable()
{
   efree(thinker_p);    // free translation table
}

static void P_NumberThinkers()
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

static void P_DeNumberThinkers()
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
   return th ? th->getOrdinal() : 0; // 0 == nullptr
}

//
// P_ThinkerForNum
//
Thinker *P_ThinkerForNum(unsigned int n)
{
   return n <= num_thinkers ? thinker_p[n] : nullptr;
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
   arc << pspr.playpos.x << pspr.playpos.y << pspr.tics << pspr.trans;

   if(arc.saveVersion() >= 5)
      arc << pspr.renderpos.x << pspr.renderpos.y;

   if(arc.isSaving())
      Archive_PSpriteState_Save(arc, pspr.state);
   else
   {
      pspr.state = Archive_PSpriteState_Load(arc);
      pspr.backupPosition();
   }
}

//
// Recursively save weapon counters
//
static void P_saveWeaponCounters(SaveArchive &arc, WeaponCounterNode *node)
{
   if(arc.isSaving())
   {
      if(node->left)
         P_saveWeaponCounters(arc, node->left);
      if(node->right)
         P_saveWeaponCounters(arc, node->right);
      arc.writeLString(E_WeaponForID(node->key)->name);
      for(int &counter : *node->object)
         arc << counter;
   }
}

//
// Load all the weapon counters if there are any
// TODO: This function is kinda ugly, probably could be rewritten
//
static void P_loadWeaponCounters(SaveArchive &arc, player_t &p)
{
   int numCounters;

   delete p.weaponctrs;
   p.weaponctrs = new WeaponCounterTree();
   arc << numCounters;
   if(numCounters)
   {
      WeaponCounter *weaponCounters = estructalloc(WeaponCounter, numCounters);
      for(int i = 0; i < numCounters; i++)
      {
         size_t len;
         char *className = nullptr;

         arc.archiveLString(className, len);
         weaponinfo_t *wp = E_WeaponForName(className);
         if(!wp)
            I_Error("P_loadWeaponCounters: weapon '%s' not found\n", className);
         WeaponCounter &wc = weaponCounters[i];
         for(int &counter : wc)
            arc << counter;
         p.weaponctrs->insert(wp->id, &wc);
      }
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
             << p.health       << p.armorpoints     << p.armorfactor
             << p.armordivisor << p.totalfrags      << p.extralight
             << p.cheats       << p.refire          << p.killcount
             << p.itemcount    << p.secretcount     << p.didsecret
             << p.damagecount  << p.bonuscount      << p.fixedcolormap
             << p.colormap     << p.quake           << p.jumptime
             << p.inv_ptr;

         int inventorySize;
         if(arc.isSaving())
         {
            int numCounters, slotIndex;
            size_t noLen = 0;

            inventorySize = E_GetInventoryAllocSize();
            arc << inventorySize;

            // Save ready and pending weapon via string
            if(p.readyweapon)
               arc.writeLString(p.readyweapon->name);
            else
               arc.archiveSize(noLen);
            if(p.pendingweapon)
               arc.writeLString(p.pendingweapon->name);
            else
               arc.archiveSize(noLen);

            slotIndex = p.readyweaponslot != nullptr ? p.readyweaponslot->slotindex : 0;
            arc << slotIndex;
            slotIndex = p.pendingweaponslot != nullptr ? p.pendingweaponslot->slotindex : 0;
            arc << slotIndex;

            // Save numcounters, then counters if there's a need to
            numCounters = p.weaponctrs->numNodes();
            arc << numCounters;
            if(numCounters)
               P_saveWeaponCounters(arc, p.weaponctrs->root); // Recursively save
         }
         else
         {
            int slotIndex;
            char *className = nullptr;
            size_t len;

            arc << inventorySize;
            if(inventorySize != E_GetInventoryAllocSize())
               I_Error("P_ArchivePlayers: inventory size mismatch\n");

            // Load ready and pending weapon via string
            arc.archiveLString(className, len);
            if(estrnonempty(className) && !(p.readyweapon = E_WeaponForName(className)))
               I_Error("P_ArchivePlayers: readyweapon '%s' not found\n", className);
            arc.archiveLString(className, len);
            if(estrnonempty(className) && !(p.pendingweapon = E_WeaponForName(className)))
               I_Error("P_ArchivePlayers: pendingweapon '%s' not found\n", className);

            arc << slotIndex;
            p.readyweaponslot = E_FindEntryForWeaponInSlotIndex(&p, p.readyweapon, slotIndex);
            arc << slotIndex;
            if(p.pendingweapon != nullptr)
               p.pendingweaponslot = E_FindEntryForWeaponInSlotIndex(&p, p.pendingweapon, slotIndex);

            // Load counters if there's a need to
            P_loadWeaponCounters(arc, p);
         }
         P_ArchiveArray<inventoryslot_t>(arc, p.inventory, inventorySize);

         for(powerduration_t &power : p.powers)
         {
            arc << power.tics;
            if(arc.saveVersion() >= 14)
               arc << power.infinite;
         }

         for(j = 0; j < MAXPLAYERS; j++)
            arc << p.frags[j];

         for(pspdef_t &psprite : p.psprites)
            P_ArchivePSprite(arc, psprite);

         arc.archiveCString(p.name, 20);

         if(arc.isLoading())
         {
            // will be set when unarc thinker
            p.mo          = nullptr;
            p.attacker    = nullptr;
            p.skin        = nullptr;
            p.pclass      = nullptr;
            p.attackdown  = AT_NONE; // sf, MaxW
            p.usedown     = false;   // sf
            p.cmd.buttons = 0;       // sf
            p.prevviewz   = p.viewz;
            p.prevpitch   = p.pitch;
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
      surface_t &floor = sec->srf.floor;
      surface_t &ceiling = sec->srf.ceiling;

      arc << floor.height << ceiling.height << sec->friction << sec->movefactor;
      Archive_Colormap(arc, sec->topmap);
      Archive_Colormap(arc, sec->midmap);
      Archive_Colormap(arc, sec->bottommap);
      arc << sec->flags;
      if(arc.saveVersion() <= 17 && arc.isLoading())
      {
         // Adjust for the MONSTERDEATH flag inserted in the middle
         sec->flags = (sec->flags &            (SECF_MONSTERDEATH - 1)) |
                     ((sec->flags & ~((unsigned)SECF_MONSTERDEATH - 1)) << 1);
      }
      else if(arc.saveVersion() <= 11 && arc.isLoading())
      {
         // Adjust for the INSTANTDEATH flag inserted in the middle
         sec->flags = (sec->flags &            (SECF_INSTANTDEATH - 1)) |
                     ((sec->flags & ~((unsigned)SECF_INSTANTDEATH - 1)) << 1);
      }
      arc << sec->intflags
          << sec->damage << sec->damageflags << sec->leakiness << sec->damagemask
          << sec->damagemod;
      Archive_Flat(arc, floor.pic);
      Archive_Flat(arc, ceiling.pic);
      arc << sec->lightlevel << sec->oldlightlevel << floor.lightdelta << ceiling.lightdelta
          << sec->special << sec->tag; // needed?   yes -- transfer types -- killough
      if(arc.saveVersion() >= 10)
      {
         // surf.data handled elsewhere
         // surf.scale, .baseangle, .lightsec, .(attached), .pflags, .portal invariant
         // surf.heightf derivative, same with .slope Z.
         // surf.terrain invariant

         // nexttag, firsttag invariant because tag is also invariant
         // soundtarget handled elsewhere
         // blockbox, soundorg, csoundorg invariant
         // validcount, thinglist, spriteproj dynamic
         // heightsec, sky invariant
         // touching_thinglist dynamic
         // linecount, lines, groupid, hticPushType, hticPushAngle, hticPushForce invariant
         arc << floor.offset << ceiling.offset << floor.angle << ceiling.angle
             << sec->soundtraversed << sec->stairlock << sec->prevsec << sec->nextsec;

         arc << sec->sndSeqID;   // currently the ID is always stable, no need for name
      }

      if(arc.isLoading())
      {
         // jff 2/22/98 now three thinker fields, not two
         ceiling.data = nullptr;
         floor.data = nullptr;
         sec->soundtarget  = nullptr;

         // SoM: update the heights
         P_SetSectorHeight(*sec, surf_floor, floor.height);
         P_SetSectorHeight(*sec, surf_ceil, ceiling.height);
      }
   }

   // do lines
   int numlinesSave = arc.saveVersion() >= 11 ? numlinesPlusExtra : numlines;
   for(i = 0, li = lines; i < numlinesSave; ++i, ++li)
   {
      int j;

      arc << li->flags << li->special << li->tag
          << li->args[0] << li->args[1] << li->args[2] << li->args[3] << li->args[4];

      if((arc.saveVersion() >= 1))
         arc << li->extflags;

      if(arc.saveVersion() >= 10)
         arc << li->intflags;

      for(j = 0; j < 2; j++)
      {
         if(li->sidenum[j] != -1)
         {
            si = &sides[li->sidenum[j]];
            
            // killough 10/98: save full sidedef offsets,
            // preserving fractional scroll offsets
            
            arc << si->offset_base_x << si->offset_base_y;
            Archive_Texture(arc, si->toptexture);
            Archive_Texture(arc, si->bottomtexture);
            Archive_Texture(arc, si->midtexture);

            if(arc.saveVersion() >= 16)
            {
               arc << si->offset_top_x    << si->offset_top_y
                   << si->offset_bottom_x << si->offset_bottom_y
                   << si->offset_mid_x    << si->offset_mid_y;
            }
         }
      }
   }

   // do sides
   if(arc.saveVersion() >= 8)
      for(i = 0, si = sides; i < numsides; ++i, ++si)
         arc << si->intflags;

   // killough 3/26/98: Save boss brain state
   arc << brain.easy;

   // haleyjd 08/30/09: save state of lightning engine
   arc << NextLightningFlash << LightningFlash;
   Archive_Texture(arc, LevelSky);
   Archive_Texture(arc, LevelTempSky);

   // ioanch: musinfo stuff
   S_MusInfoArchive(arc);
}

//
// Dynamically alterable EMAPINFO stuff
//
static void P_ArchiveLevelInfo(SaveArchive &arc)
{
   arc << LevelInfo.airControl << LevelInfo.airFriction << LevelInfo.gravity <<
          LevelInfo.skyDelta << LevelInfo.sky2Delta;
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
            sectors[i].soundtarget = nullptr;
      }
   }
}

static void P_archiveSectorActions(SaveArchive &arc)
{
   if(arc.saveVersion() < 6)
      return;

   if(arc.isSaving())
   {
      for(int i = 0; i < numsectors; i++)
      {
         sector_t &sec = sectors[i];
         unsigned int numActions = sec.actions ? sec.actions->dllData + 1 : 0;
         DLListItem<sectoraction_t> *action = sec.actions;
         arc << numActions;

         for(unsigned int j = 0; j < numActions; j++)
         {
            unsigned int ordinal = action->dllObject->mo->getOrdinal();
            arc << ordinal;
            action = action->dllNext;
         }
      }
   }
   else
   {
      for(int i = 0; i < numsectors; i++)
      {
         sector_t &sec = sectors[i];
         unsigned int mapNumActions = sec.actions ? sec.actions->dllData + 1 : 0;
         unsigned int numActions;
         DLListItem<sectoraction_t> *action = sec.actions;
         arc << numActions;

         if(numActions != mapNumActions)
            I_Error("P_archiveSectorActions: sector action count mismatch\n");

         for(unsigned int j = 0; j < numActions; j++)
         {
            unsigned int ordinal;
            arc << ordinal;
            action->dllObject->mo = thinker_cast<Mobj *>(P_ThinkerForNum(ordinal));
            action = action->dllNext;
         }
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
static void P_RemoveAllThinkers()
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
         th->remove();
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
      arc.writeLString(tc_end);
   }
   else
   {
      char *className = nullptr;
      size_t len;
      unsigned int idx = 1; // Start at index 1, as 0 means nullptr
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
         arc.archiveLString(className, len);

         // Find the ThinkerType matching this name
         if(!(thinkerType = RTTIObject::FindTypeCls<Thinker>(className)))
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
   // Do sector actions
   P_archiveSectorActions(arc);
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
   *mop = nullptr;
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
   // Protection for existing affected savegames...
   if(arc.saveVersion() <= 12)
      P_ArchiveArray<unsigned int>(arc, rng.seed, pr_mbf21);
   else
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

   if(arc.isSaving())
   {
      if(markpointnum)
         for(int i = 0; i < markpointnum; i++)
            arc << markpoints[i].x << markpoints[i].y << markpoints[i].groupid;
   }
   else
   {
      if(automapactive)
         AM_Start();

      if(markpointnum)
      {
         while(markpointnum >= markpointnum_max)
         {
            markpointnum_max = markpointnum_max ? markpointnum_max * 2 : 16;
            markpoints = erealloc(markpoint_t *, markpoints,
               sizeof *markpoints * markpointnum_max);
         }

         for(int i = 0; i < markpointnum; i++)
            arc << markpoints[i].x << markpoints[i].y << markpoints[i].groupid;
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
      po->thinker = nullptr;

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
      char *className = nullptr;
      size_t len = 0;
      
      arc.archiveLString(className, len);

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

//============================================================================
//
// haleyjd 10/17/06: Sound Sequences
//

static void P_ArchiveSndSeq(SaveArchive &arc, SndSeq_t *seq)
{
   unsigned int twizzle;

   // save name of EDF sequence
   arc.archiveCString(seq->sequence->name, 33);

   // twizzle command pointer
   twizzle = static_cast<unsigned>(seq->cmdPtr - seq->sequence->commands);
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
   arc.archiveCString(name, 33);

   if(!(newSeq->sequence = E_SequenceForName(name)))
   {
      I_Error("P_UnArchiveSndSeq: unknown EDF sound sequence %s archived\n", 
              name);
   }

   newSeq->currentSound = nullptr; // not currently playing a sound

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

static void P_ArchiveSoundSequences(SaveArchive &arc)
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
      P_ArchiveSndSeq(arc, *item);
      item = item->dllNext;
   }

   // save enviro sequence
   if(EnviroSequence)
      P_ArchiveSndSeq(arc, EnviroSequence);
}

static void P_UnArchiveSoundSequences(SaveArchive &arc)
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

static void P_ArchiveButtons(SaveArchive &arc)
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
      Archive_Texture(arc, buttonlist[i].btexture);
      arc << buttonlist[i].btimer
          << buttonlist[i].dopopout << buttonlist[i].line
          << buttonlist[i].side     << buttonlist[i].where
          << buttonlist[i].switchindex;
   }
}

//=============================================================================
//
// haleyjd 07/06/09: ACS Save/Load
//

static void P_ArchiveACS(SaveArchive &arc)
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

   if(!savefile.createFile(filename, 512*1024, OutBuffer::NENDIAN))
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
      arc.archiveCString(description, SAVESTRINGSIZE);
      
      // killough 2/22/98: "proprietary" version string :-)
      memset(name2, 0, sizeof(name2));
      snprintf(name2, sizeof(name2), VERSIONID);
   
      arc.archiveCString(name2, VERSIONSIZE);

      arc.writeSaveVersion();
   
      // killough 2/14/98: save old compatibility flag:
      // haleyjd 06/16/10: save "inmasterlevels" state
      int tempskill = (int)gameskill;
      
      arc << compatibility << tempskill << inmanageddir;
      arc << vanilla_mode;
   
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
         // save length of managed directory filename string and
         // managed directory filename string
         arc.writeLString(fn);
      }
      else
      {
         // just save 0; there is no name to save
         size_t len = 0;
         arc.archiveSize(len);
      }

      // killough 3/16/98, 12/98: store lump name checksum
      uint64_t checksum    = G_Signature(g_dir);
      int      numwadfiles = D_GetNumWadFiles();

      arc << checksum;
      arc << numwadfiles;
      // killough 3/16/98: store pwad filenames in savegame
      for(wfileadd_t *file = wadfiles; file->filename; ++file)
      {
         const char *fn = file->filename;
         arc.writeLString(fn, 0);
      }

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
      savefile.write(options, sizeof(options));
   
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
      P_ArchiveLevelInfo(arc);
      P_ArchivePolyObjects(arc); // haleyjd 03/27/06
      P_ArchiveThinkers(arc);
      P_ArchiveRNG(arc);    // killough 1/18/98: save RNG information
      P_ArchiveMap(arc);    // killough 1/22/98: save automap information
      P_ArchiveSoundSequences(arc);
      P_ArchiveButtons(arc);
      P_ArchiveACS(arc);            // davidph 05/30/12
      P_ArchiveHereticWeapons(arc);

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
      savefile.close();
      remove(filename);
      return;
   }

   // Close the save file
   savefile.close();

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
   InBuffer loadfile;
   SaveArchive arc(&loadfile);

   if(!loadfile.openFile(filename, InBuffer::NENDIAN))
   {
      C_Printf(FC_ERROR "Failed to load savegame %s\n", filename);
      C_SetConsole();
      return;
   }

   // Enable buffered IO exceptions
   loadfile.setThrowing(true);

   try
   {
      WadDirectory *tmp_g_dir = g_dir;
      WadDirectory *tmp_d_dir = d_dir;

      int     tmp_gamemap         = gamemap;
      int     tmp_gameepisode     = gameepisode;
      int     tmp_compatibility   = compatibility;
      skill_t tmp_gameskill       = gameskill;
      int     tmp_inmanageddir    = inmanageddir;
      bool    tmp_vanilla_mode    = vanilla_mode;
      int     tmp_demo_version    = demo_version;
      int     tmp_demo_subversion = demo_subversion;
      char    tmp_gamemapname[9]  = {};
      strncpy(tmp_gamemapname, gamemapname, 9);

      // skip description
      char throwaway[SAVESTRINGSIZE];

      arc.archiveCString(throwaway, SAVESTRINGSIZE);

      if(!arc.readSaveVersion())
         return;

      // killough 2/14/98: load compatibility mode
      // haleyjd 06/16/10: reload "inmasterlevels" state
      int tempskill;
      arc << compatibility << tempskill << inmanageddir;
      gameskill = (skill_t)tempskill;

      arc << vanilla_mode;  // -vanilla setting
      if(vanilla_mode)      // use UDoom version (no point for longtics now).
      {
         // All the other settings (save longtics) are stored in the save
         demo_version    = 109;
         demo_subversion = 0;
      }
      else
      {
         demo_version    = version;    // killough 7/19/98: use this version's id
         demo_subversion = subversion; // haleyjd 06/17/01
      }

      // sf: use string rather than episode, map
      for(i = 0; i < 8; i++)
      {
         int8_t lvc;
         arc << lvc;
         gamemapname[i] = (char)lvc;
      }
      gamemapname[8] = '\0'; // ending nullptr

      G_SetGameMap(); // get gameepisode, map

      // start out g_dir pointing at wGlobalDir again
      g_dir = &wGlobalDir;

      // haleyjd 06/16/10: if the level was saved in a map loaded under a managed
      // directory, we need to restore the managed directory to g_dir when loading
      // the game here. When this is the case, the file name of the managed directory
      // has been saved into the save game.
      size_t len;
      arc.archiveSize(len);

      if(len)
      {
         WadDirectory *dir;

         // read a name of len bytes 
         char *fn = ecalloc(char *, 1, len);
         arc.archiveCString(fn, len);

         // Try to get an existing managed wad first. If none such exists, try
         // adding it now. If that doesn't work, the normal error message appears
         // for a missing wad.
         // Note: set d_dir as well, so G_InitNew won't overwrite with wGlobalDir!
         if((dir = W_GetManagedWad(fn)) || (dir = W_AddManagedWad(fn)))
            g_dir = d_dir = dir;

         // done with temporary file name
         efree(fn);

         // 11/04/12: Since we loaded a managed directory wad, initialize the
         // mission. This will take care of any special data loading 
         // requirements, such as metadata for NR4TL.
         W_InitManagedMission(inmanageddir);
      }

      // killough 3/16/98, 12/98: check lump name checksum
      if(arc.saveVersion() >= 4)
      {
         uint64_t checksum, rchecksum;
         int      numwadfiles;
         qstring  msg{ "Possibly Incompatible Savegame.\nWads expected:\n\n" };

         checksum = G_Signature(g_dir);

         arc << rchecksum;
         arc << numwadfiles;

         for(int i = 0; i < numwadfiles; i++)
         {
            qstring fn;
            char   *wad = nullptr;
            size_t  len = 0;

            arc.archiveLString(wad, len);
            qstring(wad).extractFileBase(fn);
            msg << fn.constPtr() << '\n';

            efree(wad);
         }

         msg << "\nAre you sure?";

         if(checksum != rchecksum && !forced_loadgame)
         {
            // If we don't restore some state things will go very awry
            g_dir           = tmp_g_dir;
            d_dir           = tmp_d_dir;
            gamemap         = tmp_gamemap;
            gameepisode     = tmp_gameepisode;
            compatibility   = tmp_compatibility;
            gameskill       = tmp_gameskill;
            inmanageddir    = tmp_inmanageddir;
            vanilla_mode    = tmp_vanilla_mode;
            demo_version    = tmp_demo_version;
            demo_subversion = tmp_demo_subversion;
            strncpy(gamemapname, tmp_gamemapname, 9);

            C_Puts(msg.constPtr());
            G_LoadGameErr(msg.constPtr());
            loadfile.close();

            return;
         }
      }

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
      loadfile.read(options, sizeof(options));

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
      P_ArchiveLevelInfo(arc);
      P_ArchivePolyObjects(arc);    // haleyjd 03/27/06
      P_ArchiveThinkers(arc);
      P_ArchiveRNG(arc);            // killough 1/18/98: load RNG information
      P_ArchiveMap(arc);            // killough 1/22/98: load automap information
      P_UnArchiveSoundSequences(arc);
      P_ArchiveButtons(arc);
      P_ArchiveACS(arc);            // davidph 05/30/12
      P_ArchiveHereticWeapons(arc);

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

   loadfile.close();

   if (setsizeneeded)
      R_ExecuteSetViewSize();
   
   // draw the pattern into the back screen
   R_FillBackScreen(scaledwindow);

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

