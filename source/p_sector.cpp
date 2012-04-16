// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//   Sector effects.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "e_things.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "r_defs.h"

#include "cs_netid.h"
#include "cl_main.h"
#include "cl_spec.h"
#include "sv_main.h"

//=============================================================================
//
// SectorThinker class methods
//

IMPLEMENT_THINKER_TYPE(SectorThinker)
IMPLEMENT_THINKER_TYPE(SectorMovementThinker)

SectorThinker::SectorThinker() : Thinker()
{
   if(CS_CLIENT)
   {
      activation_status = ACTIVATION_UNCONFIRMED;
      activation_world_index = 0;
      if(cl_commands_sent)
         activation_command_index = cl_commands_sent - 1;
      else
         activation_command_index = 0;
      activated_clientside = true;
   }
   else
   {
      activation_status = ACTIVATION_AFFIRMATIVE;
      activation_world_index = 0;
      activation_command_index = 0;
      activated_clientside = false;
   }

   pre_activation_sector = NULL;
   pre_activation_line = NULL;

   predicting = false;
   repredicting = false;
   prediction_index = 0;
   stored_statuses = NULL;

   net_id = 0;
   removal_index = 0;
   inactive = 0;
   line = NULL;
   sector = NULL;

}

SectorThinker::SectorThinker(sector_t *new_sector) : Thinker()
{
   if(CS_CLIENT)
   {
      activation_status = ACTIVATION_UNCONFIRMED;
      activation_world_index = 0;
      if(cl_commands_sent)
         activation_command_index = cl_commands_sent - 1;
      else
         activation_command_index = 0;
      activated_clientside = true;
   }
   else
   {
      activation_status = ACTIVATION_AFFIRMATIVE;
      activation_world_index = 0;
      activation_command_index = 0;
      activated_clientside = false;
   }

   pre_activation_sector = NULL;
   pre_activation_line = NULL;

   predicting = false;
   repredicting = false;
   prediction_index = 0;
   stored_statuses = NULL;

   net_id = 0;
   removal_index = 0;
   inactive = 0;
   line = NULL;
   sector = NULL;

   setSector(new_sector);
}

SectorThinker::SectorThinker(sector_t *new_sector, line_t *new_line) : Thinker()
{
   if(CS_CLIENT)
   {
      activation_status = ACTIVATION_UNCONFIRMED;
      activation_world_index = 0;
      if(cl_commands_sent)
         activation_command_index = cl_commands_sent - 1;
      else
         activation_command_index = 0;
      activated_clientside = true;
   }
   else
   {
      activation_status = ACTIVATION_AFFIRMATIVE;
      activation_world_index = 0;
      activation_command_index = 0;
      activated_clientside = false;
   }

   pre_activation_sector = NULL;
   pre_activation_line = NULL;

   predicting = false;
   repredicting = false;
   prediction_index = 0;
   stored_statuses = NULL;

   net_id = 0;
   removal_index = 0;
   inactive = 0;
   line = NULL;
   sector = NULL;

   setLine(new_line);
   setSector(new_sector);
}

SectorThinker::~SectorThinker()
{
   cs_stored_sector_thinker_data_t *sstd = NULL;

   if(pre_activation_sector)
      efree(pre_activation_sector);

   if(pre_activation_line)
      efree(pre_activation_line);

   while(stored_statuses)
   {
      sstd = stored_statuses->next;
      efree(stored_statuses);
      stored_statuses = sstd;
      if(stored_statuses)
         stored_statuses->prev = NULL;
   }
}

//
// SectorThinker::serialize
//
void SectorThinker::serialize(SaveArchive &arc)
{
   int32_t astatus = activation_status;

   Super::serialize(arc);

   arc << net_id;

   arc << sector;
   arc << line;

   // when reloading, attach to sector
   if(arc.isLoading())
   {
      activation_status = (activation_status_e)astatus;

      switch(getAttachPoint())
      {
      case ATTACH_FLOOR:
         sector->floordata = this;
         break;
      case ATTACH_CEILING:
         sector->ceilingdata = this;
         break;
      case ATTACH_FLOORCEILING:
         sector->floordata   = this;
         sector->ceilingdata = this;
         break;
      case ATTACH_LIGHT:
         sector->lightingdata = this;
         break;
      default:
         break;
      }

      // [CG] Register Net ID.
      NetSectorThinkers.add(this);
   }
}

//
// SectorThinker::reTriggerVerticalDoor()
//
// Returns true if a vertical door should be re-triggered.
//
bool SectorThinker::reTriggerVerticalDoor(bool player)
{
   return false;
}

//
// SectorThinker::netSerialize()
//
// Serializes the current status into a net-safe struct.
//
void SectorThinker::netSerialize(cs_sector_thinker_data_t *data)
{
   dumpStatusData(data);
}

//
// SectorThinker::insertStatus()
//
// Inserts a status into this sector thinker's status queue.
//
void SectorThinker::insertStatus(uint32_t index, cs_sector_thinker_data_t *data)
{
   cs_stored_sector_thinker_data_t *sstd = NULL;
   cs_stored_sector_thinker_data_t *nsstd = NULL;
   cs_stored_sector_thinker_data_t *psstd = NULL;

   if(!CS_CLIENT) // [CG] C/S clients only.
      return;

   if(!stored_statuses)
   {
      CS_LogSMT("%u/%u: insertStatus: Inserting 1st status at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         index
      );

      stored_statuses = estructalloc(cs_stored_sector_thinker_data_t, 1);
      stored_statuses->index = index;
      copyStatusData(&stored_statuses->data, data);
      stored_statuses->prev = NULL;
      stored_statuses->next = NULL;
      return;
   }

   // [CG] Search for a status with the target index.  All statuses after the
   //      target index will be overwritten anyway, so once it's found, break
   //      and chop it off.
   psstd = sstd = stored_statuses;
   while(sstd)
   {
      if(sstd->index >= index)
         break;
      psstd = sstd;
      sstd = sstd->next;
   }

   // [CG] If we got to the end of the list without finding the target index
   //      and the last status' index wasn't (index - 1), we're missing
   //      statuses and that is very bad.  We should never be inserting gaps
   //      into the status list.  Error out in this case.
   if(psstd->index != (index - 1))
   {
      I_Error(
         "%u/%u: SMT %u missing status at %u (activated at %u) (%u/%u)!\n",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         index - 1,
         activation_command_index,
         psstd ? psstd->index : 0,
         sstd ? sstd->index : 0
      );
   }

   // [CG] If a status currently exists for this index, check its data.  If the
   //      data is equal, there's no need to do anything, so return.  Otherwise
   //      set its data and free all subsequent statuses.
   if(sstd)
   {
      if(statusesEqual(&sstd->data, data))
      {
         CS_LogSMT("%u/%u: insertStatus: Statuses equal at %u.\n",
            cl_latest_world_index,
            cl_current_world_index,
            index
         );
         return;
      }

      CS_LogSMT("%u/%u: insertStatus: Setting status at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         index
      );

      copyStatusData(&sstd->data, data);
      nsstd = sstd->next;
      sstd->next = NULL;
      sstd = nsstd;

      while(sstd)
      {
         CS_LogSMT("%u/%u: insertStatus: Removing old status at %u.\n",
            cl_latest_world_index,
            cl_current_world_index,
            sstd->index
         );
         nsstd = sstd->next;
         efree(sstd);
         sstd = nsstd;
      }
   }
   else
   {
      CS_LogSMT("%u/%u: insertStatus: Appending new status at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         index
      );
      // [CG] Otherwise just append the new status to the list.
      sstd = estructalloc(cs_stored_sector_thinker_data_t, 1);
      sstd->index = index;
      copyStatusData(&sstd->data, data);
      sstd->prev = psstd;
      sstd->next = NULL;
      psstd->next = sstd;
   }
}

//
// SectorThinker::logStatus()
//
// Logs the passed status data to the c/s sector thinker log file.  Used for
// debugging and overridden by derived classes.
//
void SectorThinker::logStatus(cs_sector_thinker_data_t *data) {}

//
// SectorThinker::statusChanged()
//
// Returns true if the sector thinker's status has changed since it was last
// saved (stored in the protected current_status member).
//
bool SectorThinker::statusChanged()
{
   cs_sector_thinker_data_t data;

   dumpStatusData(&data);

   if(statusesEqual(&data, &current_status))
      return false;

   return true;
}

//
// SectorThinker::startThinking()
//
// Checks if a sector movement thinker should think, and if so sets it up for
// thinking.
//
bool SectorThinker::startThinking()
{
   dumpStatusData(&current_status);

   if(CS_CLIENT && inactive && (inactive <= prediction_index))
      return false;

   return true;
}

//
// SectorThinker::finishThinking()
//
// Runs after a sector movement thinker finishes thinking.
//
void SectorThinker::finishThinking()
{
   if((CS_CLIENT) && (!repredicting))
      savePredictedStatus(prediction_index);

   if(CS_SERVER && net_id && statusChanged())
      SV_BroadcastSectorThinkerStatus(this);

   prediction_index = 0;
}

//
// SectorThinker::savePredictedStatus()
//
// Stores a new predicted status.
//
void SectorThinker::savePredictedStatus(uint32_t index)
{
   cs_sector_thinker_data_t data;

   dumpStatusData(&data);
   insertStatus(index, &data);
}

//
// SectorThinker::saveInitialSpawnStatus()
//
// Stores initial spawn status if activated clientside.
//
void SectorThinker::saveInitialSpawnStatus(uint32_t index,
                                           cs_sector_thinker_data_t *data)
{
   cs_stored_sector_thinker_data_t *sstd = stored_statuses;
   cs_stored_sector_thinker_data_t *nsstd = NULL;

   // [CG] Clear out the status list.
   while(sstd)
   {
      CS_LogSMT("%u/%u: saveInitialSpawnStatus: Removing old status at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         sstd->index
      );
      nsstd = sstd->next;
      efree(sstd);
      sstd = nsstd;
      if(sstd)
         sstd->prev = NULL;
   }

   stored_statuses = estructalloc(cs_stored_sector_thinker_data_t, 1);
   stored_statuses->index = index;
   copyStatusData(&stored_statuses->data, data);
   stored_statuses->prev = NULL;
   stored_statuses->next = NULL;
}

//
// SectorThinker::wasActivatedClientside()
//
// Returns true if this sector thinker was activated clientside.
//
bool SectorThinker::wasActivatedClientside()
{
   if(activated_clientside)
      return true;

   return false;
}

//
// SectorThinker::clearOldUpdates()
//
// Clears outdated updates from the queue.
//
void SectorThinker::clearOldUpdates(uint32_t index)
{
   cs_stored_sector_thinker_data_t *sstd = stored_statuses;
   cs_stored_sector_thinker_data_t *nsstd = NULL;

   if(!CS_CLIENT) // [CG] C/S clients only.
      return;

   while((sstd) && (sstd->index < index))
   {
      CS_LogSMT("%u/%u: clearOldUpdates: Removing old status at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         sstd->index
      );

      nsstd = sstd->next;
      efree(sstd);
      sstd = nsstd;
      if(sstd)
         sstd->prev = NULL;
   }
}

//
// SectorThinker::loadStatusFor()
//
// Loads the sector movement thinker's status for a given index.
//
void SectorThinker::loadStatusFor(uint32_t index)
{
   cs_stored_sector_thinker_data_t *sstd = NULL;

   if(!CS_CLIENT) // [CG] C/S clients only.
      return;

   if(!stored_statuses)
   {
      if(!activated_clientside)
         I_Error("SMT %u has no stored statuses.\n", net_id);

      savePredictedStatus(activation_command_index - 1);
      return;
   }

   for(sstd = stored_statuses; sstd; sstd = sstd->next)
   {
      if(sstd->index == (index - 1))
      {
         CS_LogSMT("%u/%u: Loading status at %u.\n",
            cl_latest_world_index,
            cl_current_world_index,
            sstd->index
         );
         loadStatusData(&sstd->data);
         break;
      }

      if(sstd->index > (index - 1))
      {
         I_Error(
            "%u/%u: SMT %u missing status at %u (activated at %u) "
            "(%u)!\n",
            cl_latest_world_index,
            cl_current_world_index,
            net_id,
            index - 1,
            activation_command_index,
            sstd ? sstd->index : 0
         );
      }
   }
}

//
// SectorThinker::rePredict()
//
// Re-Predicts sector movement clientside.
//
void SectorThinker::rePredict(uint32_t index)
{
   if(CS_CLIENT)
   {
      fixed_t oldch = sector->ceilingheight;
      fixed_t oldfh = sector->floorheight;
      cs_sector_thinker_data_t data;

      if(index < activation_command_index)
         return;

      loadStatusFor(index);
      dumpStatusData(&data);
      logStatus(&data);

      prediction_index = index;
      repredicting = true;
      cl_predicting_sectors = true;
      Think();
      savePredictedStatus(index);
      repredicting = false;
      cl_predicting_sectors = false;

      CS_LogSMT(
         "%u/%u: Re-Predicted SMT %u at %u (%u): %d/%d => %d/%d, status:\n",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         index,
         cl_commands_sent - 1,
         oldch >> FRACBITS,
         oldfh >> FRACBITS,
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );
   }
}

//
// SectorThinker::Predict()
//
// Predicts sector movement clientside.
//
void SectorThinker::Predict(uint32_t index)
{
   if(CS_CLIENT)
   {
      fixed_t oldch = sector->ceilingheight;
      fixed_t oldfh = sector->floorheight;
      cs_sector_thinker_data_t data;

      if(index < activation_command_index)
         return;

      prediction_index = index;
      predicting = true;
      cl_predicting_sectors = true;
      loadStatusFor(index);
      dumpStatusData(&data);
      Think();
      savePredictedStatus(index);
      CS_LogSMT(
         "%u/%u: Predicted SMT %u at %u (%u): %d/%d => %d/%d, status:\n",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         index,
         cl_commands_sent - 1,
         oldch >> FRACBITS,
         oldfh >> FRACBITS,
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );
      logStatus(&data);
      predicting = false;
      cl_predicting_sectors = false;
   }
}

//
// SectorThinker::setInactive()
//
// Sets the sector movement thinker as inactive at the current prediction
// index.
//
void SectorThinker::setInactive()
{
   if(CS_CLIENT)
   {
      inactive = prediction_index;
      CS_LogSMT(
         "%u/%u: Set SMT %u inactive at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         prediction_index
      );
   }
}

//
// SectorThinker::isPredicting()
//
// Returns true if the sector movement thinker is currently predicting.
//
bool SectorThinker::isPredicting()
{
   if(predicting)
      return true;

   return false;
}

//
// SectorThinker::isRePredicting()
//
// Returns true if the sector movement thinker is currently predicting.
//
bool SectorThinker::isRePredicting()
{
   if(repredicting)
      return true;

   return false;
}

void SectorThinker::setActivationIndexCutoff(uint32_t command_index,
                                             uint32_t world_index)
{
   if(!CS_CLIENT)
      return;

   if(activation_world_index)
      return;

   if(!line)
      return;

   if(command_index < activation_command_index)
      return;

   activation_world_index = world_index;
}

bool SectorThinker::confirmActivation(line_t *ln, uint32_t command_index)
{
   if(!CS_CLIENT)
      return false;

   if(!line)
      return false;

   if(activation_status != ACTIVATION_UNCONFIRMED)
      return false;

   if(line != ln)
      return false;

   if(command_index != activation_command_index)
      return false;

   activation_status = ACTIVATION_AFFIRMATIVE;
   return true;
}

void SectorThinker::checkActivationExpired()
{
   if(!CS_CLIENT)
      return;

   if(!line)
      return;

   if(activation_status == ACTIVATION_AFFIRMATIVE)
      return;

   if(activation_status == ACTIVATION_NEGATIVE)
      return;

   if(!activation_world_index)
      return;

   if(cl_latest_world_index >= activation_world_index)
      activation_status = ACTIVATION_NEGATIVE;
}

bool SectorThinker::activationExpired()
{
   if(activation_status == ACTIVATION_NEGATIVE)
      return true;

   return false;
}

void SectorThinker::Reset()
{
   if(sector && pre_activation_sector)
      memcpy(sector, pre_activation_sector, sizeof(sector_t));

   if(line && pre_activation_line)
      memcpy(line, pre_activation_line, sizeof(line_t));
}

void SectorThinker::setSector(sector_t *new_sector)
{
   sector = new_sector;

   if(pre_activation_sector)
   {
      efree(pre_activation_sector);
      pre_activation_sector = NULL;
   }

   if(sector)
   {
      pre_activation_sector = estructalloc(sector_t, 1);
      memcpy(pre_activation_sector, sector, sizeof(sector_t));
   }
}

void SectorThinker::setLine(line_t *new_line)
{
   line = new_line;

   if(pre_activation_line)
   {
      efree(pre_activation_line);
      pre_activation_line = NULL;
   }

   if(line)
   {
      pre_activation_line = estructalloc(line_t, 1);
      memcpy(pre_activation_line, line, sizeof(line_t));
   }
}

//=============================================================================
//
// Sector Actions
//

//
// P_NewSectorActionFromMobj
//
// Adds the Mobj's special to the sector
//
void P_NewSectorActionFromMobj(Mobj *actor)
{
// TODO
#if 0
   sectoraction_t *newAction = estructalloc(sectoraction_t, 1);

   if(actor->type == E_ThingNumForName("EESectorActionEnter"))
   {
      // TODO
   }
#endif
}

// EOF

