/*
*     mpool.h
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

#ifndef MPOOL_H
#define MPOOL_H

typedef struct linkable
{
  struct linkable* next;
} linkable;

typedef struct allocatable
{
  struct linkable link;

  int mem_block_start;
} allocatable;


typedef struct mpool
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
} mpool;

int mpool_init (mpool* mpool, size_t object_size, int num_obj);

void mpool_free (mpool* mpool);

int mpool_size (mpool* mpool);

int mpool_allocate (mpool* mpool, size_t size_alloc);

int mpool_mem_release (mpool* mpool, size_t size_del);

struct allocatable* mpool_take_obj (mpool* mpool);

int mpool_return_obj (mpool* mpool, allocatable* new_item);



#endif /* MPOOL_H */
