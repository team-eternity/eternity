//
// Copyright (C) 2013 Simon Howard et al.
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
// Hubs.
//
// Header file.
//
// By Simon Howard
//
//---------------------------------------------------------------------------

#ifndef __P_HUBS_H__
#define __P_HUBS_H__

void P_HubChangeLevel(char *levelname);
void P_InitHubs();
void P_ClearHubs();

void P_SavePlayerPosition(player_t *player, int sectag);
void P_RestorePlayerPosition();

void P_HubReborn();

extern bool hub_changelevel;

#endif

// EOF
