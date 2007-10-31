#include <wrapper/vwrapper.h>
#include <dlfcn.h>

V_EXPORT VOS_SOLIB * VOS_SOLIB_load(const char *path )
{
	void *hLib = dlopen( path, RTLD_NOW );
	return (VOS_SOLIB *) hLib;

}


V_EXPORT void VOS_SOLIB_close(VOS_SOLIB *hLib)
{
	dlclose( (void *) hLib );
}


//typedef void (*FUN_PTR) (void);

V_EXPORT FUN_PTR VOS_SOLIB_get_proc_address(VOS_SOLIB *hLib, const char *fname)
{
	return (FUN_PTR) dlsym( (void *) hLib, fname );
}

