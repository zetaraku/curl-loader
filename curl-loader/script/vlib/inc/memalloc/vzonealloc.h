#ifndef _VZONE_ALLOC_H_
#define _VZONE_ALLOC_H_

#include <util/vslist.h>
#include <util/vutil.h>


typedef struct {
    VSLIST_entry link;     /* link on list of all chunks */
  //V_UINT8		*start_buffer;
    V_UINT8		*alloc_data;  /* current position of allocated blocks < eof_block */
    V_UINT8		*eof_buffer;
} 
    VZONEHEAP_CHUNK;

typedef struct {
    VSLIST_entry link;     /* link on list of all chunks */
    size_t		size;
}
    VZONEHEAP_HDR_BIGALLOC;

typedef struct {
    VCONTEXT	base_interface;	/** exported allocator interface */

    VCONTEXT	*ctx;	/** this allocator interface gets memory for big blocks from it */

    size_t		chunk_size; /** size of chunk */
    size_t		alignment;  /** requested alignment */
    
    VSLIST		chunks;	/** list of all chunks / slabs */	
    VSLIST		bigallocs;	/** list of all large allocations */

    VZONEHEAP_CHUNK *current;

    size_t		user_allocated_count;
    size_t		total_allocated_count; 
} 
    VZONEHEAP;


/**
 * @brief object constructor
 */
V_EXPORT int VZONEHEAP_init(VCONTEXT *ctx, VZONEHEAP *heap, size_t chunksize, size_t alignment);

V_EXPORT int VZONEHEAP_free(VZONEHEAP *heap);

/**
 * @brief return pointer to memory allocation interface
 */
V_INLINE VCONTEXT *VZONEHEAP_get_ctx(VZONEHEAP *alloc) 
{
	return &alloc->base_interface;
}


#endif

