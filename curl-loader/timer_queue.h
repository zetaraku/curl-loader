/*
*     timer_queue.h
*
* 2006 Copyright (c) 
* Robert Iakobashvili, <coroberti@gmail.com>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef TIMER_QUEUE_H
#define TIMER_QUEUE_H

#include <stddef.h>

/*
  Timer queue API.
*/

typedef void timer_queue;

struct timer_node;

/****************************************************************************************
* Function name - tq_init
*
* Description - Performs initialization of an allocated timer queue. Inside sets comparator and
*               dump functions for a timer node objects.
*
* Input -       *tq - pointer to an allocated timer queue, e.g. heap
*               tq_size -  the size of the queue required
*               tq_increase_step -  number of objects to be allocated by each allocation operation
*               nodes_num_prealloc -  number of objects to be pre-allocated at initialization
*
* Return Code/Output - On success - 0, on error -1
****************************************************************************************/
int tq_init (timer_queue*const tq,
             size_t tq_size,
             size_t tq_increase_step,
             size_t nodes_num_prealloc);

/****************************************************************************************
* Function name - tq_release
*
* Description - De-allocates all allocated memory
*
* Input -       *tq - pointer to an allocated timer queue, e.g. heap
*
* Return Code/Output - None
****************************************************************************************/
void tq_release (timer_queue*const tq);


/****************************************************************************************
* Function name - tq_schedule_timer
*
* Description - Schedules timer, using timer-node as assisting structure
*
* Input -       *tq - pointer to an allocated timer queue, e.g. heap
*               *tnode -  pointer to the user-allocated timer node with filled next-timer and, 
*                         optionally, period
*
* Return Code/Output - On success - timer-id to be used in tq_cancel_timer (), on error -1
****************************************************************************************/
long tq_schedule_timer (timer_queue*const tq, struct timer_node* const tnode);

/****************************************************************************************
* Function name - tq_cancel_timer
*
* Description - Cancels timer, using timer-id returned by tq_schedule_timer ()
*
* Input -       *tq - pointer to a timer queue, e.g. heap
*               timer_id -  number returned by tq_schedule_timer ()
*
* Return Code/Output - On success - 0, on error -1
****************************************************************************************/
int tq_cancel_timer (timer_queue*const tq, long timer_id);

/****************************************************************************************
* Function name - tq_cancel_timers
*
* Description - Cancels all timers in timer queue, scheduled for some pointer to context
*
* Input -       *tq - pointer to a timer queue, e.g. heap
*               *tnode -  pointer to timer context (timer-node) to be searched for
*
* Return Code/Output - On success - zero of positive number of cancelled timers, on error -1
****************************************************************************************/
int tq_cancel_timers (timer_queue*const tq, struct timer_node* const tnode);

/****************************************************************************************
* Function name - tq_time_to_nearest_timer
*
* Description - Returns time (msec) to the nearest timer in queue, or -1, when no timers queued
*               Returned time is taked from timer-node next-timer field. 
*
* Input -       *tq - pointer to a timer queue, e.g. heap
*
* Return Code/Output - On success - time in msec , on error -1
****************************************************************************************/
long tq_time_to_nearest_timer (timer_queue*const tq);


/****************************************************************************************
* Function name - tq_time_to_nearest_timer
*
* Description - Removed nearest timer from the queue and fills <tnode> pointer to the timer context
*               Internally performs necessary rearrangements of the queue.
*
* Input -       *tq - pointer to a timer queue, e.g. heap
* Input/Output- **tnode - second pointer to a timer node to be filled 
*
* Return Code/Output - On success - 0, on error -1
****************************************************************************************/
int tq_remove_nearest_timer (timer_queue*const tq, struct timer_node** tnode);


/****************************************************************************************
* Function name - tq_empty
*
* Description - Evaluates, whether a timer queue is empty 
*
* Input -       *tq - pointer to a timer queue, e.g. heap
*
* Return Code/Output - If empty - positive value, if full - 0
****************************************************************************************/
int tq_empty (timer_queue*const tq);
                        

#endif /* TIMER_QUEUE_H */

