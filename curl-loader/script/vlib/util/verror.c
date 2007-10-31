#include <util/vbasedefs.h>
#include <util/verror.h>


static char * memory_error_message[] =
{
	"Out of memory",
	"Invalid heap at pointer location",
	"Double free of pointer",
	"Free of pointer that has not been allocated",
	"Can't allocate memory of requested size",
	"General error",
	0
};

V_EXPORT char* VERROR_get_message( int error )
{
	  if (error < 0 || error >= VERROR_LAST) {
		return "Strange error number";	  
	  }
	  return memory_error_message[ error ];
}


