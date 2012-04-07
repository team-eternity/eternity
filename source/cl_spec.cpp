// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
//   Clientside routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "doomtype.h"
#include "m_dllist.h"
#include "m_fixed.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"

#include "cs_netid.h"
#include "cs_spec.h"
#include "cs_main.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"

bool cl_predicting_sectors = false;
bool cl_setting_sector_positions = false;

static int old_numsectors = 0;
static uint32_t *cl_latest_sector_position_indices = NULL;

struct stored_line_activation_t
{
   DLListItem<stored_line_activation_t> link;
   int32_t type;
   uint32_t command_index;
   actor_status_t actor_status;
   uint32_t line_number;
   int8_t side;
};

DLListItem<stored_line_activation_t> *slas = NULL;

void CL_InitSectorPositions()
{
   int i;
   uint32_t j;

   if(numsectors != old_numsectors)
   {
      cl_latest_sector_position_indices = erealloc(
         uint32_t *,
         cl_latest_sector_position_indices,
         numsectors * sizeof(uint32_t)
      );

      old_numsectors = numsectors;
   }

   for(i = 0; i < numsectors; i++)
   {
      cl_latest_sector_position_indices[i] = cl_commands_sent;
      for(j = 0; j < MAX_POSITIONS; j++)
         CS_SaveSectorPosition(cl_commands_sent + j, i);
   }

   while(slas)
   {
      stored_line_activation_t *sla = slas->dllObject;
      slas->remove();
      efree(sla);
   }
}

void CL_CarrySectorPositions()
{
   int i;
   sector_position_t *old_pos, *new_pos;

   CS_LogSMT("\n");

   for(i = 0; i < numsectors; i++)
   {
      old_pos = CS_GetSectorPosition(i, cl_commands_sent - 1);
      new_pos = CS_GetSectorPosition(i, cl_commands_sent);

      if(new_pos->world_index != cl_commands_sent)
      {
         new_pos->world_index = cl_commands_sent;
         new_pos->ceiling_height = old_pos->ceiling_height;
         new_pos->floor_height = old_pos->floor_height;
      }

   }
}

void CL_SaveSectorPosition(uint32_t index, uint32_t sector_number,
                           sector_position_t *new_position)
{
   sector_position_t *pos = CS_GetSectorPosition(sector_number, index);

   pos->world_index    = index;
   pos->ceiling_height = new_position->ceiling_height;
   pos->floor_height   = new_position->floor_height;

   cl_latest_sector_position_indices[sector_number] = index;
}

void CL_LoadLatestSectorPositions()
{
   for(int i = 0; i < numsectors; i++)
      CS_SetSectorPosition(i, cl_latest_sector_position_indices[i]);
}

void CL_LoadSectorPositionsAt(uint32_t index)
{
   for(int i = 0; i < numsectors; i++)
      CS_SetSectorPosition(i, index);
}

void CL_StoreLineActivation(Mobj *actor, line_t *line, int side,
                            activation_type_e type)
{
   stored_line_activation_t *sla = NULL;

   if(actor->net_id != players[consoleplayer].mo->net_id)
      return;

   sla = estructalloc(stored_line_activation_t, 1);
   sla->type = type;
   sla->command_index = cl_commands_sent;
   sla = estructalloc(stored_line_activation_t, 1);
   P_StoreActorStatus(&sla->actor_status, actor);
   sla->line_number = (line - lines);
   sla->side = side;

   sla->link.insert(sla, &slas);
}

void CL_HandleLineActivation(uint32_t line_number, uint32_t command_index)
{
   // [CG] It's OK not to check for overflow here because this is only called
   //      from CL_HandleLineActivatedMessage which does perform the check.
   line_t *line = lines + line_number;
   stored_line_activation_t *sla = NULL;
   NetIDToObject<SectorThinker> *nito = NULL;
   DLListItem<stored_line_activation_t> *link = slas;
   DLListItem<stored_line_activation_t> *next = NULL;

   actor_status_t actor_status;

   while((nito = CLNetSectorThinkers.iterate(nito)))
   {
      if(nito->object->confirmActivation(line, command_index))
      {
         while(link)
         {
            sla = link->dllObject;

            if((sla->line_number != line_number) ||
               (sla->command_index != command_index))
            {
               link = link->dllNext;
               continue;
            }

            next = link->dllNext;
            link->remove();
            link = next;

            if(players[consoleplayer].mo)
            {
               P_StoreActorStatus(&actor_status, players[consoleplayer].mo);
               P_LoadActorStatus(players[consoleplayer].mo, &sla->actor_status);
            }

            if(sla->type == at_used)
            {
               current_game_type->handleActorUsedSpecialLine(
                  players[consoleplayer].mo, line, sla->side
               );
            }
            else if(sla->type == at_crossed)
            {
               current_game_type->handleActorCrossedSpecialLine(
                  players[consoleplayer].mo, line, sla->side
               );
            }
            else if(sla->type == at_shot)
            {
               current_game_type->handleActorShotSpecialLine(
                  players[consoleplayer].mo, line, sla->side
               );
            }
            else
            {
               doom_printf(
                  "CL_HandleLineActivations: Invalid line activation type "
                  "%d.\n",
                  sla->type
               );
            }

            if(players[consoleplayer].mo)
               P_LoadActorStatus(players[consoleplayer].mo, &actor_status);
         }
      }
   }
}

void CL_ActivateAllSectorThinkers(uint32_t index)
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
   {
      if(nito->object->inactive >= index)
         nito->object->inactive = 0;
   }
}

void CL_SavePredictedSectorPositions(uint32_t index)
{
   int i;

   for(i = 0; i < numsectors; i++)
      CS_SaveSectorPosition(index, i);
}

static SectorThinker* CL_findSectorThinker(sector_t *sector, line_t *line,
                                           uint8_t ceiling_or_floor)
{
   SectorThinker *thinker = NULL;

   if(ceiling_or_floor == 0)
      thinker = (SectorThinker *)sector->ceilingdata;
   else if(ceiling_or_floor == 1)
      thinker = (SectorThinker *)sector->floordata;
   else
   {
      doom_printf(
         "CL_findSectorThinker: Bad ceiling_or_floor value %u, "
         "returning NULL.\n",
         ceiling_or_floor
      );
   }

   if(thinker && thinker->line && line)
   {
      if(thinker->line != line)
         return NULL;
   }

   return thinker;
}

void CL_SpawnPlatform(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   PlatThinker *platform = NULL;
   uint8_t cof = message->ceiling_or_floor;

   static char seqname[15];

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   platform = (PlatThinker *)CL_findSectorThinker(sector, line, cof);
   if(!platform)
   {
      CS_LogSMT(
         "%u/%u: Spawning new platform.\n",
         cl_latest_world_index,
         cl_current_world_index
      );
      platform = new PlatThinker;
      platform->sector = sector;
      platform->addThinker();
      platform->saveInitialSpawnStatus(message->command_index, data);

      P_AddActivePlat(platform);
      strncpy(
         seqname, spawn_data->platform_spawn_data.seqname, sizeof(seqname) - 1
      );
      seqname[sizeof(seqname) - 1] = '\0';
      P_PlatSequence(platform, (const char *)seqname);
   }
   else
   {
      CLNetSectorThinkers.remove(platform);
      platform->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(platform);
}

void CL_SpawnVerticalDoor(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   VerticalDoorThinker *door = NULL;
   uint8_t cof = message->ceiling_or_floor;

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   door = (VerticalDoorThinker *)CL_findSectorThinker(sector, line, cof);
   if(!door)
   {
      VerticalDoorThinker *door = new VerticalDoorThinker;
      door->addThinker();
      door->sector = sector;
      door->saveInitialSpawnStatus(message->command_index, data);

      if(spawn_data->door_spawn_data.make_sound)
      {
         P_DoorSequence(
            door,
            spawn_data->door_spawn_data.raise,
            spawn_data->door_spawn_data.turbo,
            spawn_data->door_spawn_data.bounced
         );
      }
   }
   else
   {
      CLNetSectorThinkers.remove(door);
      door->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(door);
}

void CL_SpawnCeiling(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   CeilingThinker *ceiling = NULL;
   uint8_t cof = message->ceiling_or_floor;

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   ceiling = (CeilingThinker *)CL_findSectorThinker(sector, line, cof);
   if(!ceiling)
   {
      ceiling = new CeilingThinker;
      ceiling->addThinker();
      ceiling->sector = sector;
      ceiling->saveInitialSpawnStatus(message->command_index, data);

      P_AddActiveCeiling(ceiling);
      P_CeilingSequence(ceiling, spawn_data->ceiling_spawn_data.noise);
   }
   else
   {
      CLNetSectorThinkers.remove(ceiling);
      ceiling->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(ceiling);
}

void CL_SpawnFloor(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   FloorMoveThinker *floor = NULL;
   uint8_t cof = message->ceiling_or_floor;

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   floor = (FloorMoveThinker *)CL_findSectorThinker(sector, line, cof);
   if(!floor)
   {
      floor = new FloorMoveThinker;
      floor->addThinker();
      floor->sector = sector;
      floor->saveInitialSpawnStatus(message->command_index, data);

      if(spawn_data->floor_spawn_data.make_sound)
         P_FloorSequence(floor);
   }
   else
   {
      CLNetSectorThinkers.remove(floor);
      floor->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(floor);
}

void CL_SpawnElevator(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   ElevatorThinker *elevator = NULL;
   uint8_t cof = message->ceiling_or_floor;

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   elevator = (ElevatorThinker *)CL_findSectorThinker(sector, line, cof);
   if(!elevator)
   {
      elevator = new ElevatorThinker;
      elevator->addThinker();
      elevator->sector = sector;
      elevator->saveInitialSpawnStatus(message->command_index, data);

      if(spawn_data->floor_spawn_data.make_sound)
         P_FloorSequence(elevator);
   }
   else
   {
      CLNetSectorThinkers.remove(elevator);
      elevator->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(elevator);
}

void CL_SpawnPillar(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   PillarThinker *pillar = NULL;
   uint8_t cof = message->ceiling_or_floor;

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   pillar = (PillarThinker *)CL_findSectorThinker(sector, line, cof);
   if(!pillar)
   {
      PillarThinker *pillar = new PillarThinker;

      pillar->addThinker();
      pillar->sector = sector;
      pillar->saveInitialSpawnStatus(message->command_index, data);

      if(spawn_data->floor_spawn_data.make_sound)
         P_FloorSequence(pillar);
   }
   else
   {
      CLNetSectorThinkers.remove(pillar);
      pillar->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(pillar);
}

void CL_SpawnFloorWaggle(nm_sectorthinkerspawned_t *message)
{
   cs_sector_thinker_data_t *data = &message->data;
   cs_sector_thinker_spawn_data_t *spawn_data = &message->spawn_data;
   sector_t *sector = &sectors[message->sector_number];
   line_t *line = NULL;
   FloorWaggleThinker *floor_waggle = NULL;
   uint8_t cof = message->ceiling_or_floor;

   if(message->line_number >= 0)
      line = &lines[message->line_number];

   floor_waggle = (FloorWaggleThinker *)CL_findSectorThinker(sector, line, cof);
   if(!floor_waggle)
   {
      FloorWaggleThinker *floor_waggle = new FloorWaggleThinker;

      floor_waggle->addThinker();
      floor_waggle->sector = sector;
      floor_waggle->saveInitialSpawnStatus(message->command_index, data);

      if(spawn_data->floor_spawn_data.make_sound)
         P_FloorSequence(floor_waggle);
   }
   else
   {
      CLNetSectorThinkers.remove(floor_waggle);
      floor_waggle->saveInitialSpawnStatus(message->command_index, data);
   }

   NetSectorThinkers.add(floor_waggle);
}

void CL_UpdatePlatform(uint32_t index, cs_sector_thinker_data_t *data)
{
   PlatThinker *platform = NULL;
   SectorThinker *thinker = NULL;
   cs_platform_data_t *dt = &data->platform_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such platform %u.", dt->net_id);
   else if(!(platform = dynamic_cast<PlatThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a platform.", dt->net_id);
   else
      platform->insertStatus(index, data);
}

void CL_UpdateVerticalDoor(uint32_t index, cs_sector_thinker_data_t *data)
{
   VerticalDoorThinker *door = NULL;
   SectorThinker *thinker = NULL;
   cs_door_data_t *dt = &data->door_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such door %u.", dt->net_id);
   else if(!(door = dynamic_cast<VerticalDoorThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a door.", dt->net_id);
   else
      door->insertStatus(index, data);
}

void CL_UpdateCeiling(uint32_t index, cs_sector_thinker_data_t *data)
{
   CeilingThinker *ceiling = NULL;
   SectorThinker *thinker = NULL;
   cs_ceiling_data_t *dt = &data->ceiling_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such ceiling %u.", dt->net_id);
   else if(!(ceiling = dynamic_cast<CeilingThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a ceiling.", dt->net_id);
   else
      ceiling->insertStatus(index, data);
}

void CL_UpdateFloor(uint32_t index, cs_sector_thinker_data_t *data)
{
   FloorMoveThinker *floor = NULL;
   SectorThinker *thinker = NULL;
   cs_floor_data_t *dt = &data->floor_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such floor %u.", dt->net_id);
   else if(!(floor = dynamic_cast<FloorMoveThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a floor.", dt->net_id);
   else
      floor->insertStatus(index, data);
}

void CL_UpdateElevator(uint32_t index, cs_sector_thinker_data_t *data)
{
   ElevatorThinker *elevator = NULL;
   SectorThinker *thinker = NULL;
   cs_elevator_data_t *dt = &data->elevator_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such elevator %u.", dt->net_id);
   else if(!(elevator = dynamic_cast<ElevatorThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a elevator.", dt->net_id);
   else
      elevator->insertStatus(index, data);
}

void CL_UpdatePillar(uint32_t index, cs_sector_thinker_data_t *data)
{
   PillarThinker *pillar = NULL;
   SectorThinker *thinker = NULL;
   cs_pillar_data_t *dt = &data->pillar_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such pillar %u.", dt->net_id);
   else if(!(pillar = dynamic_cast<PillarThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a pillar.", dt->net_id);
   else
      pillar->insertStatus(index, data);
}

void CL_UpdateFloorWaggle(uint32_t index, cs_sector_thinker_data_t *data)
{
   FloorWaggleThinker *floorwaggle = NULL;
   SectorThinker *thinker = NULL;
   cs_floorwaggle_data_t *dt = &data->floorwaggle_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such floorwaggle %u.", dt->net_id);
   else if(!(floorwaggle = dynamic_cast<FloorWaggleThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a floor waggle.", dt->net_id);
   else
      floorwaggle->insertStatus(index, data);
}

