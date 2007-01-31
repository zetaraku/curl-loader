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

/*
  Inside sets fcomp and fdump ptfs
*/
int tq_init (timer_queue*const tq,
             size_t tq_size,
             size_t tq_increase_step,
             size_t nodes_num_prealloc);

void tq_release (timer_queue*const tq);

/*
  tnode contains information such as next-timer and period
  returns the timer-id to be used in cancel timer.
*/
long tq_schedule_timer (timer_queue*const tq, struct timer_node* const tnode);

int tq_cancel_timer (timer_queue*const tq, long timer_id);

int tq_cancel_timers (timer_queue*const tq, struct timer_node* const tnode);

long tq_time_to_nearest_timer (timer_queue*const tq);

/*
	Pop the root node out of the heap and return it to allocator.
	Returns pointer to the timer context.
*/
void* tq_remove_nearest_timer (timer_queue*const tq);

int tq_empty (timer_queue*const tq);
                        

#endif /* TIMER_QUEUE_H */

