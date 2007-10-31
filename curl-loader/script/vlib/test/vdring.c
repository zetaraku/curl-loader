#include <util/vdring.h>
#include <stdio.h>
#include "vtest.h"

typedef struct
{
  VDRING base;
  int num;
}
  INT_DLIST_ENTRY;

static int compare_entry(INT_DLIST_ENTRY *a1, INT_DLIST_ENTRY *a2)
{
	
	if (a1->num < a2->num) {
		return -1;
	}

	if (a1->num > a2->num) {
		return 1;
	}
	return 0;
}

static void *shuffle(int n)
{
	int *arr,i,tmp,pos;
	
	arr = (int *) malloc(n * sizeof(int));
	for(i = 0; i < n; i++) {
		arr[i] = i;
	}

	for(i = 0; i < n; i++) {

		pos = (rand() % (n - 1)) + 1;
		
		tmp = arr[0];
		arr[0] = arr[pos];
		arr[pos] = tmp;

	}
	return arr;

}

void VDRING_test()
{
	VDRING list;
	INT_DLIST_ENTRY *tmp;
	VDRING *pos, *next;
	int i, *arr;


	VDRING_init( &list );

	for(i=0;i<10;i++) {
		tmp = (INT_DLIST_ENTRY *) malloc(sizeof(INT_DLIST_ENTRY));
		tmp->num = (i * 2);
		VDRING_push_back(&list, (VDRING *) tmp);
	}

	VASSERT( VDRING_check( &list ) );

	i = 0;
	VDRING_FOREACH( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i += 2;
	}
	VASSERT( i == 20 );

	i = 18;
	VDRING_FOREACH_REVERSE( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i -= 2;
	}
	VASSERT( i == -2 );

    pos = VDRING_get_first( &list );
	for(i=0;i<10;i++) {
		tmp = (INT_DLIST_ENTRY *) malloc(sizeof(INT_DLIST_ENTRY));
		tmp->num = (i * 2) + 1;

  	    VDRING_insert_after( pos, (VDRING *) tmp);
		pos = VDRING_get_next( &list, pos );
		pos = VDRING_get_next( &list, pos );
	}

	VASSERT( VDRING_check( &list ) );

	i = 0;
	VDRING_FOREACH( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i++;
	}
	VASSERT( i == 20 );

	i = 19;
	VDRING_FOREACH_REVERSE( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i--;
	}
	VASSERT( i == -1 );

	i = 0;
	VDRING_FOREACH_SAVE( pos, next, &list ) {
		if (i & 1) {
			free( VDRING_unlink( pos ) );
		}
		i+= 1;
	}
	VASSERT( i == 20 );
	
	VASSERT( VDRING_check( &list ) );

	i = 0;
	VDRING_FOREACH( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i += 2;
	}
	VASSERT( i == 20);

	VDRING_FOREACH_SAVE( pos, next,  &list ) {
			free( VDRING_unlink( pos ));
	}

	VASSERT( VDRING_check( &list ) );
	VASSERT( VDRING_isempty( &list ) );


	arr = shuffle(100);
	for(i=0; i<100; i++) {
		tmp = (INT_DLIST_ENTRY *) malloc(sizeof(INT_DLIST_ENTRY));
		tmp->num = arr[i];

		VDRING_insert_sorted(&list, (VDRING_COMPARE) compare_entry,(VDRING *) tmp);
	}
	free(arr);

	VASSERT( VDRING_check( &list ) );

	i = 0;
	VDRING_FOREACH( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i += 1;
	}
	VASSERT( i == 100);
		
	VDRING_FOREACH_SAVE( pos, next,  &list ) {
			free( VDRING_unlink(  pos ));
	}

	for(i=0;i<100;i++) {
		tmp = (INT_DLIST_ENTRY *) malloc(sizeof(INT_DLIST_ENTRY));
		tmp->num = i;
		VDRING_push_back(&list, (VDRING *) tmp);
	}
	VASSERT( VDRING_check( &list ) );

	i = 0;
	VDRING_FOREACH( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i += 1;
	}
	VASSERT( i == 100);

	VDRING_FOREACH_SAVE( pos, next,  &list ) {
			free( VDRING_unlink(  pos ));
	}

	for(i=0;i<100;i++) {
		tmp = (INT_DLIST_ENTRY *) malloc(sizeof(INT_DLIST_ENTRY));
		tmp->num = i;
		VDRING_push_front(&list, (VDRING *) tmp);
	}

	VASSERT( VDRING_check( &list ) );

	i = 99;
	VDRING_FOREACH( pos, &list ) {

		VASSERT( ((INT_DLIST_ENTRY *) pos)->num == i );
		i -= 1;
	}
	VASSERT( i == -1);

	VDRING_FOREACH_SAVE( pos, next,  &list ) {
			free( VDRING_unlink(  pos ));
	}

		
}
