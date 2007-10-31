#ifndef _VPLATFORM_H_X_
#define _VPLATFORM_H_X_

/* *** platform dependent definitions *** */

/** 
 * @brief Platform specific constants
 *
 *   V_PTRSIZE  size of pointer in current architecture.
 *
 *	 V_DEFAULT_STRUCT_ALIGNMENT default recomended structure member alignment/
 *
 *	 V_PAGE_SIZE size of one memory page on current platform (unit of paging)
 *
 *   V_L2_CACHE_LINE_SIZE size of one processor L2 cache line
 */


/* *** Windows platform with visual C *** */
#ifdef _MSC_VER

/* 
 * surpressed Visual C 6 warnings (error level mode 4)
 * 4154: nonstandard extension used : zero-sized array in struct/union
 * 4200: nonstandard extension used : zero-sized array in struct/union
 * 4127: conditional expression is constant
 */
#pragma warning( disable : 4514 4200 4127 )

#ifdef _WIN32

typedef V_UINT32			V_POINTER_SIZE_T;

#define V_DEFAULT_STRUCT_ALIGNMENT	(sizeof(void *))
#define V_PAGE_SIZE			(4096)
#define V_L2_CACHE_LINE_SIZE		(32)

#else
#error "Undefined platform"
#endif

#elif linux

#if i386

typedef V_UINT32			V_POINTER_SIZE_T;

#define V_DEFAULT_STRUCT_ALIGNMENT	(sizeof(void *))
#define V_PAGE_SIZE			(4096)
#define V_L2_CACHE_LINE_SIZE		(32)

#else
#error "Undefined platform"
#endif

#else
#error "Undefined platform"
#endif



#endif
