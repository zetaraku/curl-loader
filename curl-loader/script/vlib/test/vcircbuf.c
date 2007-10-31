#include <util/vcircbuf.h>
#include <stdio.h>
#include "vtest.h"

#define TEST_SIZE 100

extern VCONTEXT * test_alloc;

void VCIRCBUF_test()
{
	VCIRCBUF circbuf;
	int      data, i;


	VASSERT( !VCIRCBUF_init( test_alloc, &circbuf, sizeof(int), TEST_SIZE, V_FALSE, 0, 0, 0) );

    VASSERT( VCIRCBUF_isempty(&circbuf) );

	for(i=0;i<TEST_SIZE;i++) {
		data = i;
		VASSERT( !VCIRCBUF_push( &circbuf, &data, sizeof(data) ) );

		if (i < (TEST_SIZE-1)) {
			VASSERT( !VCIRCBUF_isfull( &circbuf) );
		} else {
			VASSERT( VCIRCBUF_isfull( &circbuf) );
		}
	}

	VASSERT( VCIRCBUF_size(&circbuf) == TEST_SIZE);

	for(i=0;i<TEST_SIZE;i++) {
		VASSERT( !VCIRCBUF_pop( &circbuf, &data, sizeof(data) ) );
		VASSERT(  data == i);
		
		if (i < (TEST_SIZE-1)) {
			VASSERT(  ! VCIRCBUF_isempty( &circbuf) );
		} else {
			VASSERT(  VCIRCBUF_isempty( &circbuf) );
		}
	}

	VCIRCBUF_free(&circbuf);

}
