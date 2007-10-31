#include <memalloc/vfixedsize.h>
#include <memalloc/vfixedsize.h>
#include <memalloc/vzonealloc.h>
#include <memalloc/vtrackalloc.h>
#include "vtest.h"

void VMEMORY_test_page_alloc()
{
}


#define TEST_LOOPS				(4)
#define ELEMENTS_PER_BIGBLOCK	(1000)
#define TEST_ELEMENT_SIZE		(sizeof(int))

void alloc_test(VFIXEDHEAP *alloc, size_t num_to_alloc) 
{
	VCONTEXT *ctx = VFIXEDHEAP_get_ctx(alloc);
	int *alloc_ptr[ ELEMENTS_PER_BIGBLOCK * 2 ], *ptr;
	size_t i;

	for( i=0; i<num_to_alloc; i++ ) {
		ptr = alloc_ptr[i] = V_MALLOC( ctx, TEST_ELEMENT_SIZE );
		*ptr = i;
	}
	
	VASSERT( VFIXEDHEAP_check( alloc ) );

	/* assert number of bigblocks */

	for(i=1;i<num_to_alloc; i++ ) {
		void *tmp;
		size_t idx;

		idx = rand() % num_to_alloc;

		tmp = alloc_ptr[ 0  ];
		alloc_ptr[ 0 ] = alloc_ptr[ idx ];
		alloc_ptr[ idx ] = alloc_ptr[ 0 ];
	}


	for(i=0;i<num_to_alloc; i++ ) {
		V_FREE( ctx, alloc_ptr[ i ] );
	}
	
	/* assert that number of bigblocks is empty */

	VASSERT( VFIXEDHEAP_check( alloc ) );

	VCONTEXT_reset( ctx  );

	VASSERT( VFIXEDHEAP_check( alloc ) );
}


void VMEMORY_test_fixed_alloc()
{
	int test;
	VFIXEDHEAP alloc;


	VASSERT( ! VFIXEDHEAP_init( 0, &alloc, TEST_ELEMENT_SIZE, sizeof(int), ELEMENTS_PER_BIGBLOCK) );

	for(test = 0; test < TEST_LOOPS; test ++ ) {
		
	    alloc_test(&alloc, 10);
		
	    alloc_test(&alloc, ELEMENTS_PER_BIGBLOCK / 2);	

		alloc_test(&alloc, ELEMENTS_PER_BIGBLOCK * 2);
	}

	VFIXEDHEAP_free	( &alloc );
}

void VMEMORY_zone_alloc()
{
	VZONEHEAP zone_heap;
	VCONTEXT *ctx;
	int i;

	VASSERT( ! VZONEHEAP_init(0, &zone_heap, 30, 8) );

	ctx = VZONEHEAP_get_ctx( &zone_heap);
	
	for(i =0 ; i<2000; i++ )
	{
		int sz; 
		void *mem;
		
		sz = (i % 50) + 1;
		mem = ctx->malloc( ctx, sz );

		VASSERT( mem  );

		memset(mem, 0, sz );

		VASSERT ( ( ((V_POINTER_SIZE_T) mem) & 7 ) == 0 );
	}

	VASSERT( ! VZONEHEAP_free( &zone_heap ) );

}


void VMEMORY_track_alloc()
{
	VTRACKALLOC alloc;
	int i;
	unsigned char *mem;
	VCONTEXT *ctx;

	VASSERT( ! VTRACKALLOC_init( 0, &alloc) ); 
	
	ctx = VTRACKALLOC_get_ctx( &alloc );
	
	for(i =0 ; i<2000; i++ )
	{
		int sz = (rand() % 2000) + 10;
		mem = ctx->malloc( ctx, sz + 10 );
		
		VASSERT( mem );

		memset( mem, 0xCA , sz + 10 );

		if ( i & 8 ) {

			mem = ctx->realloc( ctx, mem, sz + 30 );
			
			VASSERT( mem );

			memset( mem, 0xCB , sz + 10 );
		}

		if ( i & 1) {
			ctx->free( ctx, mem );
		}
	
	}


	VTRACKALLOC_free( &alloc );

}
