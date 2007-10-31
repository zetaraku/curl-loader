#include <vscript.h>
#include "rtlapi.h"
#include "vm.h"

 

int std_strace(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	VSCRIPTVM_stack_trace(ctx);

	V_UNUSED(method_id);

	params->retval = VM_VALUE_LONG_return(ctx, 1);
		
	return 0;
}

int std_trace(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	VSCRIPTVM *vm = (VSCRIPTVM *) ctx;
	
	V_UNUSED(method_id);

	params->retval = VM_VALUE_LONG_return(ctx, vm->trace_flag);
	
	vm->trace_flag = 1;
		
	return 0;
}

int std_exit(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	VSCRIPTVM *vm = (VSCRIPTVM *) ctx;

	long val = VM_SCALAR_to_long(ctx, params->param[0]);

	V_UNUSED(method_id);

	vm->exit_status = val;
	vm->exit_status_set = 1;

	return 0;
}
