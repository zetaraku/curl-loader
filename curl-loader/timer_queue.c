/*
*     timer_queue.c
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

#include <stdio.h>
#include <string.h>

#include "timer_queue.h"
#include "heap.h"
#include "timer_node.h"

#define TQ_RESOLUTION 9 /* 9 msec */

/* 	
   Prototype of the function to be used to compare heap-kept objects
   for the sake of sorting in the heap. Sorting is necessary to implement 
   sorted queue container.
*/
int timer_node_comparator (hnode* const left, hnode* right)
{
    return (int) (((timer_node *) left->ctx)->next_timer < ((timer_node *) right->ctx)->next_timer);
}

/* 	
   Prototype of the function to dump the nodes. Each type of nodes
   has its own context to dump.
*/
void timer_node_dump (hnode* const h)
{
    fprintf (stderr, "n_timer=%ld ", ((timer_node *) h->ctx)->next_timer);
}

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
             size_t tq_initial_size,
             size_t tq_increase_step,
             size_t nodes_num_prealloc)
{
    if (!tq || !tq_initial_size)
    {
        fprintf (stderr, "%s - error: wrong input.\n", __func__);
        return -1;
    }

    return heap_init ((heap *const) tq,
                      tq_initial_size,
                      tq_increase_step,
                      timer_node_comparator,
                      timer_node_dump,
                      nodes_num_prealloc);
}

/****************************************************************************************
* Function name - tq_release
*
* Description - De-allocates all allocated memory
*
* Input -       *tq - pointer to an allocated timer queue, e.g. heap
*
* Return Code/Output - None
****************************************************************************************/
void tq_release (timer_queue*const tq)
{
    return heap_reset ((heap*const) tq);
}

/****************************************************************************************
* Function name - tq_schedule_timer
*
* Description - Schedules timer, using timer-node information
*
* Input -       *tq - pointer to an allocated timer queue, e.g. heap
*               *tnode -  pointer to the user-allocated timer node with filled next-timer and, 
*                         optionally, period
*
* Return Code/Output - On success - timer-id to be used in tq_cancel_timer (), on error -1
****************************************************************************************/
long tq_schedule_timer (timer_queue*const tq, 
                        timer_node* const tnode)
{
    if (!tq || !tnode)
    {
        fprintf (stderr, "%s - error: wrong input.\n", __func__);
        return -1;
    }
    
    if (tnode->next_timer < 0 || tnode->period < 0 || 
				(tnode->period && tnode->period < TQ_RESOLUTION)
			)
    {
        fprintf (stderr, 
                 "%s - error: tnode fields outside of valid range: next_timer (%ld), period (%ld).\n",
                 __func__, tnode->next_timer, tnode->period);
        return -1;
    }

    heap * h = (heap *) tq;
    hnode* new_hnode = (hnode *) mpool_take_obj (h->nodes_mpool);

    if (!new_hnode)
    {
        fprintf (stderr, 
                 "%s - error: allocation of a new hnode from pool failed.\n",
                 __func__);
        return -1;
    }
    else
    {
        new_hnode->ctx = tnode;
    }
    
    /* 	
       Push the new timer node to the heap. Zero passed as an indication, 
       that it is a new timer rather than re-scheduling of a periodic timer.	
    */ 
    return heap_push (tq, new_hnode, 0);
}

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
int tq_cancel_timer (timer_queue*const tq, long timer_id)
{
    heap* h = (heap *) tq;

    if (!tq || timer_id < 0 || (size_t) timer_id > h->max_heap_size)
    {
        fprintf (stderr, "%s - error: wrong input.\n", __func__);
        return -1;
    }

    long node_slot = h->ids_arr[timer_id];

    if (node_slot < 0)
    {
        fprintf (stderr, 
                 "%s - error: the timer-id is not valid any more (cancelled, expired?).\n", 
                 __func__);
        return -1;
    }

    if (timer_id != h->heap[node_slot]->node_id)
    {
        fprintf (stderr, "%s - error: internal timer mismatch.\n", __func__);
        return -1;
    }
    
    hnode* node = heap_remove_node (h, node_slot);

    if (!node)
    {
        fprintf (stderr, "%s - error: hnode removal failed.\n", __func__);
        return -1;
    }
    else
    {
	   node_reset (node);

        mpool_return_obj (h->nodes_mpool, (allocatable *) node);
    }
    
    return 0;
}

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
int tq_cancel_timers (timer_queue*const tq, struct timer_node* const tnode)
{
    hnode* node = 0;
    size_t index = 0;
    int counter = 0;
    heap* h = (heap *) tq;

    if (!tq || !tnode)
    {
        fprintf (stderr, "%s - error: wrong input.\n", __func__);
        return -1;
    }

    for (index = 0; index < h->curr_heap_size;)
    {
        if (h->heap[index]->ctx == tnode)
        {
            node = heap_remove_node (h, index);

            if (node)
            {
			 node_reset (node);

			 if (mpool_return_obj (h->nodes_mpool, (allocatable *) node) == -1)
			 {
				 fprintf (stderr, "%s - error: mpool_return_obj () failed.\n", __func__);
				 return -1;
			 }
                counter++;
            }
            else
            {
                fprintf (stderr, "%s - error: node removal failed.\n", __func__);
                return -1;
            }
        }
        else
        {
            ++index;
        }
    }
    
    return counter;
}

/****************************************************************************************
* Function name - tq_time_to_nearest_timer
*
* Description - Returns time (msec) to the nearest timer in queue, or -1, when no timers queued
*               Returned time is taked from timer-node field next-timer 
*
* Input -       *tq - pointer to a timer queue, e.g. heap
*
* Return Code/Output - On success - time in msec , on error -1
****************************************************************************************/
long tq_time_to_nearest_timer (timer_queue*const tq)
{
    heap* h = (heap *) tq;

    if (! h->curr_heap_size)
        return -1;

    return ((timer_node *) h->heap[0]->ctx)->next_timer;
}

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
int tq_remove_nearest_timer (timer_queue*const tq, timer_node** tnode)
{
	heap* h = (heap *) tq;
	hnode* node = heap_pop ((heap *const) tq);

	if (!node)
		return -1;

	*tnode = node->ctx;

	node_reset (node);

	if (mpool_return_obj (h->nodes_mpool, (allocatable *)node) == -1)
	{
		return -1;
	}

	return 0;
}

/****************************************************************************************
* Function name - tq_empty
*
* Description -   Evaluates, whether a timer queue is empty 
*
* Input -         *tq - pointer to a timer queue, e.g. heap
*
* Return Code/Output - If empty - positive value, if full - 0
****************************************************************************************/
int tq_empty (timer_queue*const tq)
{
	return heap_empty ((heap *const) tq);
}

