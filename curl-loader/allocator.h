/*
*     allocator.h
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

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

typedef struct linkable
{
  struct linkable* next;
} linkable;

typedef struct allocatable
{
  struct linkable link;

  int mem_block_start;
} allocatable;


typedef struct allocator
{
  /* Freelist free_list_head */
  allocatable* free_list_head;
	
  /* Freelist size. */
  int free_list_size;

  /* Number of objects for each allocation */
  int increase_step;

  /* Number of allocated memory blocks */
  int blocks_alloc_num;
	
  /* Object size */
  int obj_size;
	
  /* Number of allocated objects */
  int obj_alloc_num;
} allocator;

int allocator_init (allocator* allocator, size_t object_size, int num_obj);

void allocator_reset (allocator* allocator);

int allocator_size (allocator* allocator);

int allocator_alloc (allocator* allocator, size_t size_alloc);

int allocator_dealloc (allocator* allocator, size_t size_del);

struct allocatable* allocator_get_obj (allocator* allocator);

int allocator_return_obj (allocator* allocator, allocatable* new_item);



#endif /* ALLOCATOR_H */
