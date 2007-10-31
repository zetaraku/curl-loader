#include <util/vdring.h>



V_EXPORT void VDRING_foreach( VDRING *lst, VDRING_VISITOR_V eval, void *data, int save_from_del)
{
    VDRING *cur, *next;

	if (!eval) {
	  return;
	}

	if (save_from_del) {
		VDRING_FOREACH_SAVE( cur, next, lst) {
	   		eval( cur, data );
		}
	} else {
		VDRING_FOREACH(  cur, lst ) {
			eval( cur, data );
		}
	}
		
}


V_EXPORT void VDRING_foreach_reverse( VDRING *lst, VDRING_VISITOR_V eval, void *data, int save_from_delete)
{
	VDRING *cur, *next;

	if (!eval) {
	  return ;
	}

	if ( save_from_delete ) {
		VDRING_FOREACH_REVERSE_SAVE( cur, next, lst ) {

	   		eval( cur, data );
		}
	} else {
		VDRING_FOREACH_REVERSE( cur, lst ) {

	   		eval( cur, data );
		}
	}
}




V_EXPORT void VDRING_insert_sorted( VDRING *list, VDRING_COMPARE compare, VDRING *newentry) 
{
	VDRING *cur;
	
	VDRING_FOREACH(  cur, list ) {
		if (compare(cur,newentry) > 0) {
			VDRING_insert_before(cur,newentry);
			return;
		}
	}

	VDRING_push_back( list, newentry );
}

