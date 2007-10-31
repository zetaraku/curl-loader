#ifndef _VERROR_H_
#define _VERROR_H_

#ifdef  __cplusplus
extern "C" {
#endif

/** 
 * @brief enumeration of error conditions
 */
typedef enum {
  VCONTEXT_ERROR_OUT_OF_MEMORY,
  VCONTEXT_ERROR_INVALID_HEAP,
  VCONTEXT_ERROR_DOUBLE_FREE,
  VCONTEXT_ERROR_FREE_PTR_NOT_ALLOCATED,
  VCONTEXT_ERROR_REQUEST_TOO_BIG,
  
  VERROR_LAST,	

} VERROR;

/**
 * @brief turn erro code to string message.
 */
V_EXPORT char* VERROR_get_message( int error );

#ifdef  __cplusplus
}
#endif

#endif
