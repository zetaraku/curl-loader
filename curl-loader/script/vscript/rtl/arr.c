#include <vscript.h>
#include "rtlapi.h"

int std_join(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	VM_VALUE_ARRAY *arr;
	VM_VALUE_STRING *ret; 
	int i;

	V_UNUSED(method_id);

	arr = (VM_VALUE_ARRAY *) params->param[ 0 ];

	ret = VM_VALUE_STRING_init(VM_CTX( ctx ), 10); 
	if (!ret) {
		return -1;
	}

	for( i =0; i<arr->count; i++) {
		char tmp[100];
		int  slen;
		const char *sval;

		VM_OBJ_HEADER *obj = arr->val[ i ];

		sval = VM_SCALAR_to_string( ctx, obj, tmp, &slen);

		if (VM_VALUE_STRING_add_cstr( VM_CTX(ctx), ret, sval, slen )) {
			return -1;
		}
	}

	params->retval = &ret->base;

	return 0;
}

int std_push(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	VM_VALUE_ARRAY *arr;
	VM_OBJ_HEADER *val;

	V_UNUSED(method_id);

	arr = (VM_VALUE_ARRAY *) params->param[ 0 ];

	val = params->param[ 1 ];

	arr->val[ arr->count ++  ] = val;
	
	VM_OBJ_HEADER_add_ref( val );

	params->retval = VM_VALUE_LONG_return( ctx, arr->count );

	return 0;
}


int std_pop(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	VM_VALUE_ARRAY *arr;
	VM_OBJ_HEADER *ret;

	V_UNUSED(method_id);

	arr = (VM_VALUE_ARRAY *) params->param[ 0 ];

	if (arr->count == 0) {

		params->retval = VM_VALUE_STRING_return( ctx, "" );
		return 0;
	}

	ret = arr->val[ --arr->count ];
	
	VM_OBJ_HEADER_add_ref( ret );

	params->retval = ret;

	return 0;
}







