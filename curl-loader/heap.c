/*
 *     heap.c
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
#include <stdlib.h>
#include <string.h>

#include "heap.h"


#define HEAP_PARENT(X) (X == 0 ? 0 : (((X) - 1) / 2))
#define HEAP_LCHILD(X) (((X)+(X))+1)

/* Increase size of heap array */
static int heap_increase (heap*const h);

/* Fetches a free node-id */
static long heap_get_node_id (heap*const h);

/* Puts node to the heap slot and updates id of the node. */
static void heap_put_node_to_slot (heap*const h, size_t slot, hnode*const node);
	
static hnode* heap_remove_node (heap*const h, const size_t slot);

/* Heapifies to the up direction, so called parent path heapification */
static void filter_up (heap*const h, size_t index);

/* Heapifies to the down direction, so called children path heapification */
static void filter_down (heap*const h, size_t index);


int heap_init (heap*const h,
               size_t initial_heap_size,
               size_t increase_step,
               heap_cmp_func comparator,
               node_dump_func dumper,
               size_t nodes_prealloc)
{
  size_t i = 0;
	
  if (!h)
    {
      fprintf(stderr, "%s -wrong input\n", __func__);
      return -1;
    }
	
  /*zero heap */
  memset ((void*)h, 0, sizeof (*h));
  
  /*allocate array of hnode* pointers to serve as heap */
  if (! (h->heap = calloc (initial_heap_size, sizeof (hnode*))) )
    {
      fprintf(stderr, "%s - allocation of heap array failed\n", __func__);
      return -1;
    }
	
  /*allocate array of id-s */
  if (! (h->ids_arr = calloc (initial_heap_size, sizeof (long))) )
    {
      fprintf(stderr, "%s - allocation of ids array failed\n", __func__);
      return -1;
    }
	
  /* mark all node-ids in the ids array as non-valid */
  for (i = 0; i < initial_heap_size; i++)
    {
      h->ids_arr[i] = -1; /* non-valid id is -1 */
    }
	
  h->max_heap_size = initial_heap_size;
  h->heap_increase_step = increase_step;
	
  if (0 == comparator)
    {
      fprintf(stderr, "%s - comparator function should be provided.\n", __func__);
      return -1;      
    }
  else
    {
      h->fcomp = comparator;
    }

  h->fndump = dumper; /* If zero, we do not dump nodes. */

  if (!(h->nodes_allocator = calloc (1, sizeof (allocator))))
    {
      fprintf(stderr, "%s - allocation of nodes allocator failed\n", __func__);
      return -1;
    }
  else
    {
      if (allocator_init (h->nodes_allocator, sizeof (hnode), nodes_prealloc) == -1)
        {
          fprintf(stderr, "%s - allocator_init () -  failed\n",  __func__);
          return -1;
        }
    }
  return 0;
}

void heap_reset (heap*const h)
{
  if (h->heap)
    {
      free (h->heap);
    }

  if (h->ids_arr)
    {
      free (h->ids_arr);
    }

  if (h->nodes_allocator)
    {
      allocator_reset (h->nodes_allocator);

      free (h->nodes_allocator);
    }

  memset (h, 0, sizeof (*h));
}

void heap_dump (heap*const h)
{
  size_t j = 0;
	
  fprintf (stderr, "\tcurr_heap_size=%d\n", h->curr_heap_size);

  for (j = 0; j < h->curr_heap_size; j++)
    {
      fprintf (stderr, "[%d: ", j);
      if (h->fndump)
        {
          h->fndump (h->heap[j]);
        }
      else
        {
          fprintf (stderr, 
                   "%s - error: no dump function provided in allocator_init ().\n",
                   __func__);
        }
      fprintf (stderr, " ]\n");
    }	
}

int heap_prealloc (heap*const h, size_t nodes_prealloc)
{
  if (! h || ! nodes_prealloc)
    {
      fprintf(stderr, "%s - wrong input\n", __func__);
      return -1;
    }

  if (! h->nodes_allocator || ! h->nodes_allocator->increase_step)
    {
      fprintf(stderr, "%s - error: heap_init () should be called first\n", __func__);
      return -1;
    }
  
  if (allocator_alloc (h->nodes_allocator, nodes_prealloc) == -1)
    {
      fprintf(stderr, "%s - allocator_alloc() failed\n", __func__);
      return -1;
    }
	
  return 0;
}

hnode* heap_pop (heap*const h)
{
  if (!h || h->curr_heap_size <= 0)
    {
      fprintf(stderr, "%s - wrong input\n", __func__);
      return 0;
    }
	
  return heap_remove_node (h, 0);
}

hnode*  heap_top_node (heap*const h)
{
  if (!h || h->curr_heap_size <= 0)
    {
      fprintf(stderr, "%s - wrong input\n", __func__);
      return 0;
    }
  return h->heap[0];
}

int heap_empty (heap*const h)
{
  return ! h->curr_heap_size;
}

int heap_increase (heap*const h)
{
  hnode** new_heap = 0, **old_heap = 0;
  long* new_ids = 0, *old_ids = 0;
  int new_size = 0, i = 0;

  if (!h || h->heap_increase_step <= 0)
    {
      fprintf(stderr, "%s - wrong input\n", __func__);
      return -1;
    }
	
  new_size = h->max_heap_size + h->heap_increase_step;
	
  /* Allocate new arrays for heap and ids */
  if ((new_heap = calloc (new_size, sizeof (hnode*))) == 0)
    {
      fprintf(stderr, "%s - allocation of the new heap array failed\n", __func__);
      return -1;
    }
	
  if ((new_ids = calloc (new_size, sizeof (long)) ) == 0)
    {
      fprintf(stderr, "%s - allocation of the new ids array failed\n", __func__);
      return -1;
    }
	
  /* mark all node-ids in the new_ids array as non-valid */
  for (i = 0; i < new_size; i++)
    {
      new_ids[i] = -1;
    }
	
  memcpy (new_heap, h->heap, sizeof (hnode*) * h->max_heap_size);
  memcpy (new_ids, h->ids_arr, sizeof (long) * h->max_heap_size);
	
  /* Rem the old heap and old ids*/
  old_heap = h->heap;
  old_ids = h->ids_arr;
	
  /* Switch the arrays and correct max_curr_heap_size */
  h->heap = new_heap;
  h->ids_arr = new_ids;
  h->max_heap_size = new_size;
	
  /* Release mem of the old arrays*/
  free (old_heap);
  free (old_ids);
  
  return 0;
}

long heap_push (heap* const h, hnode* const nd, int keep_node_id)
{
  long new_node_id = -1;
	
  if (!h || !nd)
    {
      fprintf(stderr, "%s - wrong input\n", __func__);
      return -1;	
    }
	
  if (h->curr_heap_size >= h->max_heap_size)
    {
      if (heap_increase (h) == -1)
        {
          fprintf(stderr, "%s - heap_increase() failed\n", __func__);
          return -1;
        }
    }
	
  if (keep_node_id)
    {
      /* e.g. re-scheduled timers */
      new_node_id = nd->node_id;
    }
  else
    {
      /* get free node-id */
      new_node_id = heap_get_node_id (h);
	
      /* 
         Set node-id to the hnode, it will be further passed from 
         the node to the relevant slot in <ids> array by 
         heap_put_node_to_slot ().
      */
      nd->node_id = new_node_id;
    }
	
  /* write the element into the heap end */
  heap_put_node_to_slot (h, h->curr_heap_size, nd);
  
  /* restore the heap structure */
  filter_up (h, h->curr_heap_size);
  
  /* increase current number of nodes in heap */ 
  h->curr_heap_size++;
  
  return new_node_id;
}

long heap_get_node_id (heap*const h)
{
  while (++h->ids_last < h->max_heap_size && h->ids_arr[h->ids_last] >= 0)
    ;

  if (h->ids_last == h->max_heap_size && h->ids_min_free < h->max_heap_size)
    {
      h->ids_last = h->ids_min_free;
      h->ids_min_free = h->max_heap_size;
    }
  
  return (long) h->ids_last;
}

void heap_put_node_to_slot (heap*const h, size_t slot, hnode*const nd)
{
  /* Insert node into the specified slot of the heap */
  h->heap[slot] = nd;
  
  /* Update slot in the parallel ids-array */
  h->ids_arr[nd->node_id] = slot;
}

hnode* heap_remove_node (heap*const h, const size_t slot)
{
  hnode* mved_end_node = 0;
  size_t parent_slot = 0;
  hnode* removed_node = h->heap[slot];
  size_t removed_node_id;

  if (!removed_node)
    {
      fprintf(stderr, "%s - null removed node.\n", __func__);
      return 0;
    }
	
  removed_node_id = removed_node->node_id;

  /* Decrement the heap size */
  h->curr_heap_size--;

  /* Reheapify only, if we're not deleting the last entry. */
  if (slot < h->curr_heap_size)
    {
      mved_end_node = h->heap[h->curr_heap_size];
      
      /* 
         Move the end node to the location being removed.  Update
         slot in the parallel <ids> array.
      */
      heap_put_node_to_slot (h, slot, mved_end_node);

      /* 	
         If the mved_end_node "node-value" < than the value its 
         parent, we move it up the heap.
      */
      parent_slot = HEAP_PARENT (slot);
		
      if ((*h->fcomp) (mved_end_node, h->heap[parent_slot])) // <
        {
          filter_up (h, slot);
        }
      else
        {
          filter_down (h, slot);
        }
    }
	
  /* free the node from the ids array */
  h->ids_arr[removed_node_id] = -1;
	
  if (removed_node_id < h->ids_min_free && removed_node_id <= h->ids_last)
    {
      h->ids_min_free_restore = h->ids_min_free;
      h->ids_min_free = removed_node_id;
    }

  return removed_node;
}

void filter_up (heap*const h, size_t index)
{
  int curr_pos = index;
  int parent_pos = HEAP_PARENT(index);
  hnode* target = h->heap [index];

  /* traverse path of parents up to the root */
  while (curr_pos > 0)
    {
      /* compare target and parent value */
      if ((*h->fcomp) (h->heap[parent_pos], target)) /* less   < */
        {
          break;
        }
      else
        {
          /* move data from parent position to current position.*/
          heap_put_node_to_slot (h, curr_pos, h->heap[parent_pos]);
			
          /* update current position to parent */
          curr_pos = parent_pos;
			
          /* compute next parent */
          parent_pos = (curr_pos - 1)/2;
        }
    }
	
  heap_put_node_to_slot (h, curr_pos, target);
}

void filter_down (heap*const h, size_t index)
{
  void* target = h->heap [index];
  size_t curr_pos = index;

  /*
    Compute the left child index and go down along path of children,
     stopping at the end of list, or when find a place for target 
  */
  size_t child_pos = HEAP_LCHILD(index);
	
  while (child_pos < h->curr_heap_size)
    {
      /* 
         Index of the right child is child_pos+1. Compare the two children and 
         change child_pos, if comparison is true.
      */
      if ((child_pos+1 < h->curr_heap_size) &&
          (*h->fcomp) (h->heap[child_pos+1], h->heap[child_pos])) /* less  < */
        {
          child_pos = child_pos + 1;
        }

      /* Compare selected child to target */
      if ((*h->fcomp) (target, h->heap[child_pos])) /* less < */
        {
          /* Target belongs at curr_pos */
          break;
        }
      else
        {
          /* 
             Move selected child to the parent position. 
             The position of the selected child starts to be 
             empty and available. 
          */
          heap_put_node_to_slot (h, curr_pos, h->heap[child_pos]);
          
          /* Update index and continue */
          curr_pos = child_pos;
          child_pos = HEAP_LCHILD(curr_pos);
        }
    }
	
  /* Place target to the available vacated position */
  heap_put_node_to_slot (h, curr_pos, target);
}
