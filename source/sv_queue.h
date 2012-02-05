// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//   Serverside player queue routines.
//
//----------------------------------------------------------------------------

void         SV_UpdateQueueLevels();
unsigned int SV_GetNewQueuePosition();
void         SV_AssignQueuePosition(int playernum, unsigned int position);
void         SV_AdvanceQueue(unsigned int queue_position);
void         SV_PutPlayerInQueue(int playernum);
void         SV_PutPlayerAtQueueEnd(int playernum);
void         SV_RemovePlayerFromQueue(int playernum);
void         SV_MarkQueuePlayersAFK();

