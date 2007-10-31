#include <vscript.h>
#include "rtlapi.h"
#include <stdio.h>
#include <vm.h>


/*
 * print to standard output.
 */
int std_print(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	int i;

	V_UNUSED(method_id);

	for( i = 0; i < params->param_count; i++) {

		char sSize[100];
		int  slen;
		const char *ret;
		int   is_trace_on;
		
		is_trace_on = ((VSCRIPTVM *) ctx)->trace_flag;
		
		ret = VM_SCALAR_to_string(ctx, params->param[i], sSize, &slen);
		
		if (is_trace_on) {
			fputs( "\n>", stdout);
		}
		
		fputs( ret , stdout);

		if (is_trace_on) {
			fputs( "\n", stdout);
		}
	}
	
	params->retval = VM_VALUE_LONG_return(ctx, 1);

	return 0;
}
