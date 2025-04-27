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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: General queue code.
// Authors: James Haley, Charles Gunyon
//

#ifndef M_QUEUE_H
#define M_QUEUE_H

struct mqueueitem_t
{
    struct mqueueitem_t *next;
};

struct mqueue_t
{
    mqueueitem_t  head;
    mqueueitem_t *tail;
    mqueueitem_t *rover;
    unsigned int  size;
};

void          M_QueueInit(mqueue_t *queue);
void          M_QueueInsert(mqueueitem_t *item, mqueue_t *queue);
bool          M_QueueIsEmpty(mqueue_t *queue);
mqueueitem_t *M_QueuePop(mqueue_t *queue);
mqueueitem_t *M_QueueIterator(mqueue_t *queue);
mqueueitem_t *M_QueuePeek(mqueue_t *queue);
void          M_QueueResetIterator(mqueue_t *queue);
void          M_QueueFree(mqueue_t *queue);

#endif

// EOF

