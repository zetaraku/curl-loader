

// has to be first incldue !!!
// this file has to be included first.
//
#include <bits/types.h>

#undef __FD_SETSIZE

#ifndef CURL_LOADER_FD_SETSIZE 
#error "must define CURL_LOADER_FD_SETSIZE"
#endif

#define __FD_SETSIZE  CURL_LOADER_FD_SETSIZE 


