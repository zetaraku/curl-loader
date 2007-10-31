#include <util/vstra.h>
#include "vtest.h"

extern VCONTEXT * test_alloc;

void VSTRA_test()
{	
	VSTRA    str,s_copy,s_stack;

	/* init */
	VASSERT( ! VSTRA_init(test_alloc,&str) );

	VASSERT( ! VSTRA_scpy( &str,"abc") );

	VASSERT(  strcmp( VSTRA_cstr(&str), "abc" ) == 0);

	VSTRA_init_stack( &s_stack, 7);


	VASSERT( ! VSTRA_cpy( VNEW_STRING, &s_copy, &str) );

	VASSERT( ! VSTRA_cat( &str, &s_copy) );

	VASSERT(   strcmp( VSTRA_cstr(&str), "abcabc" ) == 0);

	VASSERT( ! VSTRA_cpy( VCOPY_STRING, &s_stack, &s_copy) );

	VASSERT(   strcmp( VSTRA_cstr(&s_copy), VSTRA_cstr(&s_stack) )  == 0);




	/* free */
	VSTRA_free( &str);
	VSTRA_free( &s_copy);
	VSTRA_free( &s_stack);
}

