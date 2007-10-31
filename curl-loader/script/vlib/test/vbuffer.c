#include <util/vbuffer.h>
#include <stdio.h>
#include "vtest.h"


#define TEST_SIZE 10

extern VCONTEXT * test_alloc;

void VBUFFER_test()
{
	VBUFFER buf;
	size_t  head_area_size = 3;
	char sbuf[3];
	
	memset(sbuf,1,sizeof(sbuf));

	VASSERT( ! VBUFFER_init( test_alloc, &buf, TEST_SIZE, head_area_size, 0 ) );



	VASSERT( VBUFFER_check( &buf ) );


	VBUFFER_free( &buf );

}

