#ifndef _XLIB_H_
#define _XLIB_H_

#include <util/varr.h>
#include <util/vbuckethash.h>

struct tagAST_BASE;

/* *** extension methods - internal interface *** */

typedef enum {
  XMETHODLIB_OK,
  XMETHODLIB_METHOD_INTERNAL_ERROR,
  XMETHODLIB_METHOD_ID_REPEATED,
  XMETHODLIB_METHOD_ALREADY_DEFINED,
}
  XMETHODLIB_STATUS;

typedef enum {
  XMETHOD_ASYNCH,
  XMETHOD_CALLBACK,
  XMETHOD_NOTIMPLEMENTED,
}
  XMETHOD_TYPE;

typedef struct tagXMETHODACTION {
	XMETHOD_TYPE action_type;
	int   method_id;
   	void *callback_ptr;
}  XMETHODACTION;

typedef struct tagXMETHODLIB {
 
  int	open_method;	// for cl, for runtime or both.

  VBUCKETHASH hash; // map name to function declaration,
  
  VBUCKETHASH unique_id; // maintains unique external function id.
  
  VARR		  map_id_to_action; // map internal function id to an XMETHODACTION

} XMETHODLIB;

XMETHODLIB *XMETHODLIB_init(int open_mode);

int XMETHODLIB_free(XMETHODLIB *xmethods);

struct  tagAST_FUNCTION_DECL *XMETHODLIB_find(XMETHODLIB *xmethods, struct tagAST_BASE *fcall);

XMETHODACTION * XMETHODLIB_lookup_action(XMETHODLIB *xmethods, size_t idx );

#endif

