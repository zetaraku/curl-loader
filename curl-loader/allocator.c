/*
*     allocator.c
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
#include <asm/page.h>

#define OS_FREE_LIST_CHUNK_SIZE (PAGE_SIZE*9/10)


#include <allocator.h>

void allocatable_set_next (allocatable* item, allocatable* next_item)
{
  item->link.next = (linkable *)next_item;
}

allocatable* allocatable_get_next (allocatable* item)
{
  return (allocatable *) item->link.next;
}

int allocator_add (allocator* allocator, allocatable* new_item)
{
  /* Put to free_list_head */
  
  if (allocator->free_list_head)
    {
      allocatable_set_next (new_item, allocator->free_list_head);
   }
	
  allocator->free_list_head = new_item;
  allocator->free_list_size++;
  
  return 0;
}

allocatable* allocator_remove (allocator* allocator)
{
  allocatable *temp = allocator->free_list_head;

   if (temp)
   {
     allocator->free_list_head =  allocatable_get_next (allocator->free_list_head);
     allocator->free_list_size--;

     // set next pointer to 0
     allocatable_set_next (temp, 0);
   }
   return temp;
}

allocatable* allocator_get_obj (allocator* allocator)
{
  if (! allocator)
    {
      fprintf (stderr, "%s - wrong input\n", __func__);
      return 0;
    }

  allocatable* obj = 0;
	
  if (! (obj = allocator_remove (allocator)))
    {
      /*Allocator is empty. Allocating from the OS 
       */
      if (allocator_alloc (allocator, allocator->increase_step) == -1)
        {
          fprintf (stderr, "%s - allocator_alloc () - failed\n", __func__);
          return 0;
        }
      else
        {
          if (! (obj = allocator_remove (allocator)))
            {
              /* Rare scenario. */
              fprintf (stderr, "%s - allocator_remove () - failed, still no objects\n", __func__);
              return 0;
            }
        }
    }
  
  return obj;
}


int allocator_return_obj (allocator* allocator, allocatable* new_item)
{
  if (! allocator || ! new_item)
    {
      fprintf (stderr, "%s - wrong input\n", __func__);
      return -1;
    }
	
  return allocator_add (allocator, new_item);
}

int
allocator_init (allocator* allocator,
	size_t object_size, 
	int number_objects)
{
  if (! allocator || ! object_size || number_objects < 0)
   {
     fprintf (stderr, "%s - wrong input\n", __func__);
     return -1;
   }

  if (object_size > OS_FREE_LIST_CHUNK_SIZE)
    {
      fprintf (stderr, "%s - size of object more the PAGE_SIZE\n", __func__);
      return -1;
    }

  allocator->increase_step = allocator->free_list_size = 0;
  allocator->obj_size = object_size;
  
  /* Preventing fragmentation*/
  allocator->increase_step = OS_FREE_LIST_CHUNK_SIZE / allocator->obj_size;
	
  if (allocator_alloc (allocator, number_objects) == -1)
    {
      fprintf (stderr, "%s - size of object more the PAGE_SIZE\n", __func__);
      return -1;
    }

  return 0;
}

void allocator_reset (allocator* allocator)
{
  if (allocator->obj_alloc_num != allocator->free_list_size)
    {
      fprintf (stderr, "%s - all objects must be returned\n", __func__);
      return;
    }

  if (! allocator->blocks_alloc_num)
    {
      fprintf (stderr, "%s - there are no allocated memory blocks\n", __func__);
      return;
    }

  allocatable** memblock_array = 0;

  if (! (memblock_array = 
         calloc (allocator->blocks_alloc_num, sizeof (allocatable*))))
    {
      fprintf (stderr, "%s - calloc () of blocks_array failed\n", __func__);
      return;
    }
	
  int memblock_array_index = 0;

  /*
    Iterate through all objects to fetch starts of memblocks and 
    write them to the memblock_array
  */
  allocatable* item = 0;
  for (item = allocator->free_list_head; item ; item = allocatable_get_next (item))
    {
      if (item->mem_block_start)
        {
          memblock_array[memblock_array_index++] = item;
        }
    }

  // now free () all the blocks
  while (--memblock_array_index >= 0)
    {
      free (memblock_array[memblock_array_index]);
    }

  memset (allocator, 0, sizeof (*allocator));
}

int allocator_size (allocator* allocator)
{
  return allocator->free_list_size;
}

int allocator_alloc (allocator* allocator, size_t size_alloc)
{
  if (! allocator || ! size_alloc)
    {
      fprintf (stderr, "%s - wrong input.\n", __func__);
      return -1;
    }

  if (allocator->increase_step <= 0)
    {
      fprintf (stderr, "%s - allocator not initialized\n", __func__);
      return -1;
    }

  // number of allocations, each of about a PAGE_SIZE
  int num_alloc_step = size_alloc / allocator->increase_step;
  
  // minimum 1 allocation step should be done
  num_alloc_step = num_alloc_step ? num_alloc_step : 1;
  
  // number of allocated by this function call objects
  int obj_allocated = 0;
  
  // u_char is to enable ANSI-C pointer arithmetics
  unsigned char* chunk;
  
  while (num_alloc_step--)
    {
      chunk = 0;

      if (! (chunk =  calloc (allocator->increase_step, allocator->obj_size)))
        {
          fprintf (stderr, "%s - calloc () failed\n", __func__);
          break;
        }
      else
        {
          allocator->blocks_alloc_num++;
          
          // add to allocator successfully allocated allocator->increase_step number of objects
          //
          int i;
          
          for ( i = 0; i < allocator->increase_step; i++)
            {
              allocatable* item = (allocatable*)(chunk + i*allocator->obj_size);
              
              // The first block (i == 0) we mark as 1, which mean allowed to call free (), others -0
              //
              item->mem_block_start =  i ? 0 : 1;
              allocator_add (allocator, item);
            }
          
          obj_allocated += allocator->increase_step;
        }
    }
	
  if (!obj_allocated)
    {
      fprintf (stderr, "%s - failed to allocate objects\n", __func__);
      return -1;
    }
  else
    {
      allocator->obj_alloc_num += obj_allocated;
    }

  return 0;
}

int allocator_dealloc (allocator* allocator, size_t size_del)
{
  if (! allocator || size_del <= 0)
    {
      fprintf (stderr, "%s - wrong input\n", __func__);
      return -1;
    }

  for (; allocator->free_list_head && size_del > 0; size_del--)
    {
      allocatable* item_to_free = allocator_remove (allocator);
      free (item_to_free);
    }

  return 0;
}
