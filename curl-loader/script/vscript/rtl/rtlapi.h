#ifndef __RTLAPI_H__
#define __RTLAPI_H__

/*
 * print to standard output.
 */
int std_push(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_pop(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_print(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_length(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_keys(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_values(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_join(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_index(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_rindex(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_substr(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params); 

int std_strace(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_trace(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_abs(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_atan2(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_cos(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_exp(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_log(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_sin(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_sqrt(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_oct(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);
	
int std_hex(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);
	
int std_int(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);
	
int std_rand(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);
	
int std_srand(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

int std_exit(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params);

#endif


