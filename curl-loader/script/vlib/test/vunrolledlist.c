#include <util/vdlistunrolled.h>
#include "vtest.h"

extern VCONTEXT * test_alloc;

void VDLISTUNROLLED_test()
{
	VDLISTUNROLLED list;
	int i;
	VDLISTUNROLLED_position idx;
	int *ptr, idn;
	
	VASSERT( !VDLISTUNROLLED_init( test_alloc, &list, sizeof(int), 10) );
	VASSERT( VDLISTUNROLLED_check(&list) );

	for(i=50;i<100;i++) {
		VDLISTUNROLLED_push_back(&list, &i, sizeof(int) );
	}
	VASSERT( VDLISTUNROLLED_check(&list) );

	idn = 50;
	VDLISTUNROLLED_FOREACH( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == idn);
		idn ++;
	}
	VASSERT( idn == 100 );

	for(i = 49; i >= 0; i--) {
		VDLISTUNROLLED_push_front(&list, &i, sizeof(int) );
	}

	VASSERT( VDLISTUNROLLED_check(&list) );

	/* iterate over list */
	idn = 0;
	VDLISTUNROLLED_FOREACH( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == idn);
		idn ++;
	}
	VASSERT( idn == 100 );
	
	/* iterate over list */
	idn = 99;
	VDLISTUNROLLED_FOREACH_REVERSE( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == idn);
		idn --;
	}
	VASSERT( idn == -1 );

	/* checks */
	VASSERT( VDLISTUNROLLED_check(&list) );
	VASSERT( list.elmcount == 100)
	VASSERT( list.entrycount == 10);

	VDLISTUNROLLED_free(&list, 0, 0);

	VASSERT( list.elmcount == 0)
	VASSERT( list.entrycount == 0);

	VASSERT( VDLISTUNROLLED_check(&list) );

}


#define NTRY 40

void VDLISTUNROLLED_test_insert()
{

	VDLISTUNROLLED list;
	int i,tmp;
	VDLISTUNROLLED_position idx;
	int *ptr, idn;
	

	VASSERT( !VDLISTUNROLLED_init( test_alloc, &list, sizeof(int), 10) );
	VASSERT( VDLISTUNROLLED_check(&list) );

	for(i=0;i< NTRY;i++) {
		tmp = i * 2;
		VDLISTUNROLLED_push_back(&list, &tmp, sizeof(int) );
	}

	VASSERT( VDLISTUNROLLED_check(&list) );


	for( i=0, idx = VDLISTUNROLLED_get_first( &list );i<NTRY;i++) {
		 tmp = (i * 2) + 1;
		 VDLISTUNROLLED_insert_after( &list, idx, &tmp, sizeof(tmp) );
		 idx = VDLISTUNROLLED_next( idx );
		 idx = VDLISTUNROLLED_next( idx );
	}

	VASSERT( VDLISTUNROLLED_check(&list) );

	/* iterate over list */
	idn = 0;
	VDLISTUNROLLED_FOREACH( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == idn);
		idn ++;
	}
	VASSERT( idn == (2 * NTRY) );

	/* iterate over list */
	idn = (2 * NTRY) - 1;
	VDLISTUNROLLED_FOREACH_REVERSE( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == idn);
		idn --;
	}
	VASSERT( idn == -1 );

	VASSERT( VDLISTUNROLLED_check(&list) );

	VASSERT( list.elmcount == (2 * NTRY))
	VASSERT( list.entrycount == ((2 * NTRY) / 10) );

	for( idx = VDLISTUNROLLED_get_first( &list );
		 idx.entry != (VDLISTUNROLLED_entry *) &list.root;) {

		 ptr = (int *) VDLISTUNROLLED_at( &list, idx );

		 VDLISTUNROLLED_unlink( &list, idx );

		 idx = VDLISTUNROLLED_next( idx );
	}

	VASSERT( VDLISTUNROLLED_check(&list) );

	idn = 1;
	VDLISTUNROLLED_FOREACH( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == idn);
		idn += 2;
	}
	VASSERT( idn == (2 * NTRY) + 1 );


	VDLISTUNROLLED_free(&list, 0, 0);

	VASSERT( list.elmcount == 0)
	VASSERT( list.entrycount == 0);

	VASSERT( VDLISTUNROLLED_check(&list) );


	/*--------*/
	for(i=0;i<100;i++) {
		VDLISTUNROLLED_push_back(&list, &i, sizeof(int));
	}
	VASSERT( VDLISTUNROLLED_check( &list ) );

	i = 0;
	VDLISTUNROLLED_FOREACH( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == i );
		i += 1;
	}
	VASSERT( i == 100);

	VDLISTUNROLLED_free(&list, 0, 0);

	for(i=0;i<100;i++) {
		VDLISTUNROLLED_push_front(&list, &i, sizeof(int));
	}

	VASSERT( VDLISTUNROLLED_check( &list ) );

	i = 99;
	VDLISTUNROLLED_FOREACH( idx, &list ) {
		ptr = (int *) VDLISTUNROLLED_at( &list, idx );
		VASSERT( *ptr == i );
		i -= 1;
	}
	VASSERT( i == -1);

	VDLISTUNROLLED_free(&list, 0, 0);

}

