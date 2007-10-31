#include <wrapper/vwrapper.h>
#include <string.h>
#include "vtest.h"

#define TEST_FILE "../../testfile.txt"

void VWRAPPER_test_mmap()
{
	VOS_MAPFILE map;
	void *mem;

	map = VOS_MAPFILE_open( TEST_FILE, VOS_MAPPING_READ, 0);
	
	VASSERT( map );
	
	mem = VOS_MAPFILE_get_ptr( map );

	VASSERT( memcmp( mem, "123456", 6) == 0) ;
	
	VASSERT( (size_t) VOS_MAPFILE_get_length( map ) == 6 );

	VOS_MAPFILE_close( map );
	
}

