
#include <util/varr.h>


static int VARR_grow_default(VARR *arr,size_t request_size)
{
	size_t sz;
	void *rt;

	if (!arr->ctx) {
		return -1;
	}
	sz = VRESIZE_request(arr->sizepolicy, arr, request_size);//arr->elmmaxcount+1);
	if (!sz) {
		return -1;
	}

	rt = V_REALLOC( arr->ctx,  arr->buffer, sz * arr->elmsize);
	if (!rt) {
		return -1;
	}
	arr->buffer = rt;
	arr->elmmaxcount = sz;
	return 0;
}





V_EXPORT int VARR_insert_at( VARR *arr, size_t index, void *elm, size_t elmsize)
{	
	if (elmsize != arr->elmsize) {
		return -1;
	}

 
	if (index >= arr->elmmaxcount) {
		if (VARR_grow_default(arr,index+1)) {
			return -1;
		}
	}


	if (arr->elmcount == arr->elmmaxcount) {
		if (VARR_grow_default(arr,arr->elmmaxcount+1)) {
			return -1;
		}
	}


	if (index < arr->elmcount) {
		memmove( arr->buffer + (index + 1) * elmsize, 
				 arr->buffer + index * elmsize, (arr->elmcount - index) * elmsize);

	} 

	if (index <= arr->elmcount) {
		arr->elmcount++;
	} else {
		memset( arr->buffer + arr->elmcount * arr->elmsize, 
				0, 
				arr->elmsize * (index - arr->elmcount) );		
		arr->elmcount = index + 1;
	}

	memcpy(arr->buffer + (index * elmsize), elm, elmsize);


	return 0;
}

V_EXPORT int VARR_set_at( VARR *arr, size_t index, void *elm, size_t elmsize)
{	
	if (elmsize != arr->elmsize) {
		return -1;
	}

 
	if (index >= arr->elmmaxcount) {
		if (VARR_grow_default(arr,index+1)) {
			return -1;
		}
	}


	memcpy(arr->buffer + (index * elmsize), elm, elmsize);
	if (index >= arr->elmcount) {
		memset( arr->buffer + arr->elmcount * arr->elmsize, 
				0, 
				arr->elmsize * (index - arr->elmcount) );	
		arr->elmcount = index + 1;

	}

	return 0;
}
