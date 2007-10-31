#include <util/varr.h>
#include "vtest.h"


extern VCONTEXT * test_alloc;

void VARR_test()
{
	VARR arr;
	int  elm;

	VASSERT( ! VARR_init( test_alloc, &arr, sizeof(int), 0 ) );

	elm = 1;
	VASSERT( ! VARR_push_back( &arr, &elm, sizeof(elm) ) );

	elm = 3;
	VASSERT( ! VARR_push_back( &arr, &elm, sizeof(elm) ) );

	elm = 4;
	VASSERT( ! VARR_push_back( &arr, &elm, sizeof(elm) ) );

	elm = 2;
	VASSERT( ! VARR_insert_at( &arr, 1, &elm, sizeof(elm) ) );


	VASSERT( VARR_maxsize(&arr) == 6);

	VASSERT (  VARR_size(&arr) == 4 );
	VASSERT ( *( (int *) VARR_at( &arr, 0)) == 1 );
	VASSERT ( *( (int *) VARR_at( &arr, 1)) == 2 );
	VASSERT ( *( (int *) VARR_at( &arr, 2)) == 3 );
	VASSERT ( *( (int *) VARR_at( &arr, 3)) == 4 );

	
	VASSERT( ! VARR_delete_at( &arr, 1 ) );
	VASSERT( ! VARR_pop_back( &arr, &elm, sizeof(int) ) );

	VASSERT (  VARR_size(&arr) == 2 );
	VASSERT ( *( (int *) VARR_at( &arr, 0)) == 1 );
	VASSERT ( *( (int *) VARR_at( &arr, 1)) == 3 );


	VARR_free( &arr );


}

