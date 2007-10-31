#ifndef _VOS_WRAPPERS_H_
#define _VOS_WRAPPERS_H_

#ifdef  __cplusplus
|extern "C" {
#endif

#include <util/vbasedefs.h>


/* *** portability layer between POSIX and WIN32 (are there any other systems?) 
	   rant rant: how many portability layers does one programmer 
	   write in the course of his career for stuff like this ?
	   Yawn.
   *** */

/* ***  for memory mapped files *** */

typedef void * VOS_MAPFILE;

typedef enum {
  VOS_MAPPING_READ  = 0x1,
  VOS_MAPPING_WRITE = 0x2,
  VOS_MAPPING_EXEC  = 0x4,

  VOS_MAPPING_INHERIT =0x8,

} VOS_MAPPING_PERMISSIONS;


/* *** map in a whole file into memory, the whole thing *** */
V_EXPORT VOS_MAPFILE VOS_MAPFILE_open(const char *file_name, unsigned int open_flag, const char *share_name);

#if 0
/* *** create anonmymous file mapping *** */
V_EXPORT VOS_MAPFILE VOS_MAPFILE_shared_mem( unsigned int open_flag, const char *share_name, V_UINT64 size);
#endif

/* *** flush changes in memory to backing store *** */
V_EXPORT int  VOS_MAPFILE_flush(VOS_MAPFILE *mapfile);

/* *** close the whole thing *** */
V_EXPORT void VOS_MAPFILE_close(VOS_MAPFILE *mapfile);

/* *** get pointer to start of file mappign *** */
V_EXPORT void *VOS_MAPFILE_get_ptr(VOS_MAPFILE *mapfile);

/* *** get size of mapping *** */
V_EXPORT V_UINT64 VOS_MAPFILE_get_length(VOS_MAPFILE *mapfile);


/* *** delayed loading / shared library *** */

typedef void * VOS_SOLIB;

/* *** load shared library to memory *** */
V_EXPORT VOS_SOLIB * VOS_SOLIB_load(const char *path );

/* *** close/unload library *** */
V_EXPORT void VOS_SOLIB_close(VOS_SOLIB *);

typedef void (*FUN_PTR) (void);

/* *** resolve function by name - get it. *** */
V_EXPORT FUN_PTR VOS_SOLIB_get_proc_address(VOS_SOLIB *, const char *fname);

#ifdef  __cplusplus
}
#endif

#endif
