#include <vscript.h>
#include "rtlapi.h"



int std_index(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	char lhs_tmp[100],rhs_tmp[100];
	int  lhs_slen,rhs_slen;
	const char *lhs,*rhs,*res;
	long ret = -1;

	V_UNUSED(method_id);


	
	lhs = VM_SCALAR_to_string(ctx, params->param[0], lhs_tmp, &lhs_slen);
	rhs = VM_SCALAR_to_string(ctx, params->param[1], rhs_tmp, &rhs_slen);

	res = strstr( lhs, rhs );
	if (res) {
		ret = res - lhs;
	}

	params->retval = VM_VALUE_LONG_return(ctx, ret);

	return 0;
}

int std_rindex(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	char lhs_tmp[100],rhs_tmp[100];
	int  lhs_slen,rhs_slen;
	const char *lhs,*rhs;
	long ret = -1;
	const char *p;

	V_UNUSED(method_id);

	lhs = VM_SCALAR_to_string(ctx, params->param[0], lhs_tmp, &lhs_slen);
	rhs = VM_SCALAR_to_string(ctx, params->param[1], rhs_tmp, &rhs_slen);


	for( p = lhs + lhs_slen - rhs_slen; p>=lhs; p --) {
		
		const char *t,*r;

		for(t = p, r = rhs; *r != '\0' && *t++ == *r++; );

		if (*r == '\0') {
			ret = p - lhs;
			break;
		}
	}
	
	params->retval = VM_VALUE_LONG_return(ctx, ret);

	return 0;
}

int std_substr(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	long idx_start,idx_end;
	char lhs_tmp[100];
	int  lhs_slen;
	const char *lhs;

	V_UNUSED(method_id);


	lhs = VM_SCALAR_to_string(ctx, params->param[0], lhs_tmp, &lhs_slen);
	idx_start = VM_SCALAR_to_long(ctx,params->param[1]);
	idx_end = params->param_count >= 3 ? VM_SCALAR_to_long(ctx,params->param[2]) : -1;
	
	if (idx_start < 0) {
		idx_start = 0;
	}
	if (idx_start >= lhs_slen) {
		idx_start = lhs_slen - 1;
	}

	if (idx_end < 0) {
		idx_end = lhs_slen - idx_start;
	} else 
	if ((idx_end + idx_start) >= lhs_slen) {
		idx_end = lhs_slen - idx_start;
	} 

#if 0
	if (idx_start > idx_end) {
		params->retval = VM_VALUE_STRING_return(ctx, "" );
		return 0;
	}
#endif

	params->retval = VM_VALUE_STRING_return_n(ctx, lhs + idx_start, idx_end );


	return 0;
}

 
