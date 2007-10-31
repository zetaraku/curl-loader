#include <wrapper/vwrapper.h>
#include <windows.h>


V_EXPORT VOS_SOLIB * VOS_SOLIB_load(const char *path )
{
	HANDLE hLib = LoadLibrary( path );
	return (VOS_SOLIB *) hLib;

}


V_EXPORT void VOS_SOLIB_close(VOS_SOLIB *hLib)
{
	FreeLibrary( (HINSTANCE) hLib );
}


typedef void (*FUN_PTR) (void);

V_EXPORT FUN_PTR VOS_SOLIB_get_proc_address(VOS_SOLIB *hLib, const char *fname)
{
	return (FUN_PTR) GetProcAddress( (HANDLE) hLib, fname );
}

