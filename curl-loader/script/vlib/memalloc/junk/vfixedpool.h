#ifndef _V_VIXEDPOOLZ_H_
#define _V_VIXEDPOOLZ_H_

#ifdef  __cplusplus
extern "C" {
#endif


#include <util/vbasedefs.h>
#include <util/vdring.h>


/*
 * Options:
 
 * ??? big block reuse as option, or always on.
 * ??? Initial big block size versus subsequent allocated big blocks.

 * ??? alignment of returned memory as parameter?
 *	    options:
		 - cache line size - 

 * ??? Statistics options
 *		- bytes allocated. bytes in use.
 *		- some measure of fragmentation.

 * ??? Debug options.

 */



/**
 * @brief memory pool where each element maintained by the pool has the same size. (elmsize)
 *
 * The pool allocates big blocks that hold a given fixed number of elements.
 * When the user allocates an element, it is allocated out of such a big block.
 *
 * The memory pool exports the VCONTEXT interface.
 * All allocations of up to elmsize bytes are satisfied.
 * The minimum size of one element is four bytes.
 */
typedef struct  {
	VCONTEXT	base_interface;				/** exported allocator interface */

	VCONTEXT	*ctx;						/** this allocator interface gets memory for big blocks from it */
	size_t		elmsize;					/** size of one element */
	size_t		elmmaxcount_per_block;		/** maximum elements per big memory block */
	size_t		poolsize;					/** size of one memory block (elmsize * elmmaxcount_per_block) */
	size_t		alignment;

	VDRING		root;						/** ring of memory blocks */

	size_t	    poolscount;				    /** number of pools */
	size_t		elmcount;					/** number of elements allocated */

	int			is_grow_heap:1;
}
	VFIXEDPOOL;

/**
 * @brief create a memory pool. Allocates one memory big block in advance.

 * @param ctx		 (in)  allocator object - used to allocate big blox
 * @param pool		 (out) the object
 * @param alignment  (in)  default alignment (if 0 we take V_DEFAULT_STRUCT_ALIGNMENT bytes (platform specific def); if non null this must be power of two)
 * @param elmsize    (in)  size of one element allocated within the heap. 
 * @param elmaxcount (in)  number of elements kept in one big block.
 * @param is_dynamic (in)  if true: if all big blocks are full and further memory is requested then we will allocate further big blocks.

 */
V_EXPORT int  VFIXEDPOOL_init(  VCONTEXT *ctx, 
							    VFIXEDPOOL *pool, 
								size_t alignment,
								size_t elmsize, 
								size_t elmmaxcount, 
								int	   is_dynamic);

/**
 * @brief free the pool of fixed blocks.
 */
V_EXPORT void VFIXEDPOOL_free( VFIXEDPOOL *pool );

/**
 * @brief get allocation interface from fixed pool allocator.
 * User allocates memory via the VCONTEXT allocation interface.
 */
V_INLINE VCONTEXT *VFIXEDPOOL_get_ctx(VFIXEDPOOL * pool)
{
	return &pool->base_interface;
}

/** 
 *	@brief returns number of allocated big memory blocks
 */
V_INLINE size_t	   VFIXEDPOOL_blocks_count(VFIXEDPOOL * pool) 
{
	return pool->poolscount;
}

/**
 * @brief returns number of allocated memory blocks (each of size pool->elmcount)
 */
V_INLINE size_t	   VFIXEDPOOLS_allocated_count(VFIXEDPOOL *pool)
{
	return pool->elmcount;
}




#ifdef  __cplusplus
}
#endif

#endif
