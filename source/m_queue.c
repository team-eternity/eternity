// Emacs style mode select   -*- C -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
   queue->head.next = NULL;
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

   // [CG] Ensure the tail's ->next member is NULL.
   queue->tail->next = NULL;

   // [CG] Update the queue's size.
   queue->size++;
}

//
// M_QueueIsEmpty
//
// [CG] Returns true if the queue is empty, false otherwise.
//
boolean M_QueueIsEmpty(mqueue_t *queue)
{
   if(queue->head.next == NULL)
   {
      return true;
   }
   return false;
}

//
// M_QueuePop
//
// [CG] Removes the oldest element in the queue and returns it.
//
mqueueitem_t* M_QueuePop(mqueue_t *queue)
{
   mqueueitem_t *item;

   if(M_QueueIsEmpty(queue))
   {
      return NULL;
   }

   item = queue->head.next;
   queue->head.next = item->next;
   queue->rover = &(queue->head);

   if(queue->tail == item)
   {
      queue->tail = &(queue->head);
   }

   queue->size--;

   return item;
}

//
// M_QueueIterator
//
// Returns the next item in the queue each time it is called,
// or NULL once the end is reached. The iterator can be reset
// using M_QueueResetIterator.
//
mqueueitem_t *M_QueueIterator(mqueue_t *queue)
{
   if(queue->rover == NULL)
      return NULL;

   return (queue->rover = queue->rover->next);
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
      free(rover);

      rover = next;
   }

   M_QueueInit(queue);
}

// EOF

