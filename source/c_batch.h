//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Batched console commands.
// Authors: James Haley, Charles Gunyon
//

#ifndef C_BATCH__
#define C_BATCH__

void        C_ActivateCommandBatch();
void        C_AddCommandBatch(const char *name, const char *commands);
const char *C_GetCommandBatch(const char *name);
void        C_CommandBatchTicker();
bool        C_CommandIsBatch(const char *name);
void        C_SaveCommandBatches(FILE *file);

#endif

