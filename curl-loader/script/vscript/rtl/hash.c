#include <vscript.h>
#include "rtlapi.h"


static int extract_from_hash(int op, void *ctx, VSCRIPT_XMETHOD_FRAME *params)
{
	VM_BASE_TYPE  var_type = VM_TYPE_get_base_type( params->param[0] );
	VM_VALUE_HASH *hash;
	VM_VALUE_ARRAY *ret;
	VM_VALUE_HASH_IMP *imp;
	size_t i, j, k = 0, added_cnt;

	if (var_type != VM_HASH ) {	
		return -1;
	}
	
	hash = (VM_VALUE_HASH *) params->param[ 0 ];

	ret = VM_VALUE_ARRAY_init( VM_CTX(ctx), hash->imp.elmcount );
	if (!ret) {
		return 1;
	}

	imp = &hash->imp;
	added_cnt = 0;

	for(i = 0; i<imp->buckets_count; i++) {
		VM_VALUE_HASH_BUCKET *cur;

		for(cur = imp->buckets[ i ] ; cur ; cur = cur->next) {
 
			for(j = 0; j < VM_VALUE_HASH_ENTRIES_PER_BUCKET; j++ ) {
				if (cur->entry[ j ].hash) {
					
					if (op) {
						VM_OBJ_HEADER_add_ref( cur->entry[ j ].key );
						ret->val[ k ++ ] =  cur->entry[ j ].key;
					} else {
						VM_OBJ_HEADER_add_ref( cur->entry[ j ].value );
						ret->val[ k ++ ] =  cur->entry[ j ].value;
					}
					added_cnt ++;
				}
			}
		}

	} 

	ret->count = added_cnt;
	params->retval = &ret->base;

	return 0;
}

int std_keys(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	V_UNUSED(method_id);

	return extract_from_hash(1, ctx, params);
}


int std_values(void *ctx, int method_id, VSCRIPT_XMETHOD_FRAME *params)
{
	V_UNUSED(method_id);
	
	return extract_from_hash(0, ctx, params);
}


