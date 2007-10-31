#ifndef __VMCONST_H__
#define __VMCONST_H__

#include "vmvalue.h"

/* *** constant type definitions *** */

typedef struct tagVM_CONSTANT {
  VM_TYPE type;

  union {
	unsigned long long_value;
	double double_value;
	size_t offset_value;
  } val;

} VM_CONSTANT_VALUE;


#define VM_CONSTANT_long(value)  ((VM_CONSTANT_VALUE *) value)->val.long_value

#define VM_CONSTANT_double(value)  ((VM_CONSTANT_VALUE *) value)->val.double_value

#define VM_CONSTANT_string_offset(value)  ((VM_CONSTANT_VALUE *) value)->val.offset_value

const char * VM_SCALAR_get_const_string(struct tagVSCRIPTVM *vm, VM_CONSTANT_VALUE *val);

size_t VM_SCALAR_get_const_string_length(struct tagVSCRIPTVM *vm, VM_CONSTANT_VALUE *val);

#endif
