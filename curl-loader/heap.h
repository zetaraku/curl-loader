/*
*     heap.h
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

#ifndef HEAP_H
#define HEAP_H

#include "mpool.h"

typedef struct hnode
{
  /* Base for the "allocatable" property. */
  allocatable alloc;	
  
  /* The unique id of the node. */
  long node_id;

  /* Pointer to the user-data context to keep with the node. */
  void* ctx;

} hnode;

/* 	
   Prototype of the function to be used to compare heap-kept objects
   for the sake of sorting in the heap. Sorting is necessary to implement 
   sorted queue container.
*/
typedef int (*heap_cmp_func) (hnode* const, hnode* const);

/* 	
   Prototype of the function to dump the nodes. Each type of nodes
   has its own context to dump.
*/
typedef void (*node_dump_func) (hnode* const);


typedef struct heap
{
  /* Array of pointers to the nodes. */
  hnode** heap;
	
  /* Maximum size of the heap. */
  size_t max_heap_size;
	
  /* Current size of the heap. */
  size_t curr_heap_size;
	
  /* 
     Really heap rising step.
  */
  size_t heap_increase_step;
	
  /* 
     Array of node ids to serve for mapping between node-id and place in heap,
     containing node with the id.
		
     When ids_arr[i] is -1, it is free and may be used for a newcoming node. 
     But, when ids_arr[i] >=0, it does contain some valid node-id corresponding
     to a valid node in the <heap> array.	
  */
  long* ids_arr;
	
  /* The last provided slot from <ids_arr> (node-id) */
  size_t ids_last;
	
  /* The lowest freed node-id */
  size_t ids_min_free;
	
  /* 
     Keeping <ids_min_free> in order to restore the value for a
     node to be re-scheduled.
  */
  size_t ids_min_free_restore;

  /* Comparator for nodes. */
  heap_cmp_func fcomp;

  /* Dumper for nodes. */
  node_dump_func fndump;

  /* Mpool of nodes */
  struct mpool* nodes_mpool;
	
} heap;



int heap_init (heap*const h,
               size_t initial_heap_size,
               size_t increase_step,
               heap_cmp_func comparator,
               node_dump_func dumper,
               size_t nodes_prealloc);

void heap_reset (heap*const h);

void heap_dump (heap*const h);

int heap_prealloc (heap*const h, size_t nodes_prealloc);

hnode* heap_pop (heap*const h);

/*
  Pushes a new node to the heap. If successful, returns node-id (>=0).
  Returns -1 on errors.		
*/
long heap_push (heap* const h, hnode* const nd, int keep_node_id);

hnode* heap_top_node (heap*const h);

int heap_empty (heap*const h);

#endif /* HEAP_H */
