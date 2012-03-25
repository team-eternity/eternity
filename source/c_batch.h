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
//   Batched console commands.
//
//----------------------------------------------------------------------------

#ifndef C_BATCH__
#define C_BATCH__

#include "m_dllist.h"
#include "m_qstr.h"

class CommandBatch : public ZoneObject
{
private:
   char *name;
   char *commands;
   char *current_command;
   qstring command_buffer;
   unsigned int delay;
   bool finished;

   unsigned int parseWait(const char *token);

public:
   DLListItem<CommandBatch> links;
   DLListItem<CommandBatch> finished_links;
   const char *key;

   CommandBatch(const char *new_name, const char *new_commands);
   ~CommandBatch();

   const char* getName() const;
   const char* getCommands() const;
   bool isFinished();
   void run();
};

void          C_ActivateCommandBatch();
void          C_AddCommandBatch(const char *name, const char *commands);
const char*   C_GetCommandBatch(const char *name);
void          C_CommandBatchTicker();
CommandBatch* C_CommandBatchIterator(CommandBatch *batch);

#endif

