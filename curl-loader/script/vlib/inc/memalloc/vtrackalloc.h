#ifndef _VTRACKALLOC_H_
#define _VTRACKALLOC_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <util/vutil.h>
#include <util/vdring.h>


/* incidentally: for 32 bit this is 8 byte long == sizeof(double)
   for 64 bytes even more so , important to keep alignment of sizeof(double)
 */
typedef struct {
   VDRING link;

} VTRACKALLOC_ARENA;

/** 
 *	@brief allocator for blocks of tracking allocator
 */
typedef struct  {

	VCONTEXT	base_interface;				/** exported allocator interface */

	VCONTEXT	*ctx;	/** this allocator interface gets memory for big blocks from it */

	VDRING		links;

}
	VTRACKALLOC;

/** 
 * @brief iniitialise a tracking allocator 
 */
V_EXPORT int VTRACKALLOC_init(VCONTEXT *ctx,VTRACKALLOC *alloc);


/**
 * @brief return pointer to memory allocation interface
 */
V_INLINE VCONTEXT *VTRACKALLOC_get_ctx(VTRACKALLOC *alloc) 
{
	return &alloc->base_interface;
}

/**
 * @brief destroy the allocator and free all memory
 */
V_EXPORT void VTRACKALLOC_free( VTRACKALLOC *alloc );

/**
 * @brief check consistency of allocator
 */
V_EXPORT int VTRACKALLOC_check( VTRACKALLOC *alloc );



#ifdef  __cplusplus
}
#endif


#endif

