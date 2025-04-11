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
//--------------------------------------------------------------------------
//
// General queue code
//
// By James Haley
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "m_queue.h"

//
// M_QueueInit
//
// Sets up a queue. Can be called again to reset a used queue
// structure.
//
void M_QueueInit(mqueue_t *queue)
{
   queue->head.next = nullptr;
   queue->tail = &(queue->head);
   queue->rover = &(queue->head);
   queue->size = 0;
}

//
// M_QueueInsert
//
// Inserts the given item into the queue.
//
void M_QueueInsert(mqueueitem_t *item, mqueue_t *queue)
{
   // link in at the tail (this works even for the first node!)
   queue->tail = queue->tail->next = item;

   // [CG] Ensure the tail's ->next member is nullptr.
   queue->tail->next = nullptr;

   // [CG] Update the queue's size.
   queue->size++;
}

//
// M_QueueIsEmpty
//
// Returns true if the queue is empty, false otherwise.
//
bool M_QueueIsEmpty(mqueue_t *queue)
{
   if(queue->head.next == nullptr)
      return true;

   return false;
}

//
// M_QueuePop
//
// Removes the oldest element in the queue and returns it.
//
mqueueitem_t* M_QueuePop(mqueue_t *queue)
{
   mqueueitem_t *item;

   if(M_QueueIsEmpty(queue))
      return nullptr;

   item = queue->head.next;
   queue->head.next = item->next;
   queue->rover = &(queue->head);

   if(queue->tail == item)
      queue->tail = &(queue->head);

   queue->size--;

   return item;
}

//
// M_QueueIterator
//
// Returns the next item in the queue each time it is called,
// or nullptr once the end is reached. The iterator can be reset
// using M_QueueResetIterator.
//
mqueueitem_t* M_QueueIterator(mqueue_t *queue)
{
   if(queue->rover == nullptr)
      return nullptr;
      
   return (queue->rover = queue->rover->next);
}

//
// M_QueuePeek
//
// Returns the first element of the queue.
//
mqueueitem_t* M_QueuePeek(mqueue_t *queue)
{
   return queue->head.next;
}

//
// M_QueueResetIterator
//
// Returns the queue iterator to the beginning.
//
void M_QueueResetIterator(mqueue_t *queue)
{
   queue->rover = &(queue->head);
}

//
// M_QueueFree
//
// Frees all the elements in the queue
//
void M_QueueFree(mqueue_t *queue)
{
   mqueueitem_t *rover = queue->head.next;

   while(rover)
   {
      mqueueitem_t *next = rover->next;
      efree(rover);

      rover = next;
   }

   M_QueueInit(queue);
}

// EOF

