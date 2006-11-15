/* 
   ssl_thr_lock.c

   Taken with thanks from CURL project opensslthreadlock.c by 
   Jeremy Brown <jbrown_at_novell.com> 
   and modified by 
   Robert Iakobashvili <coroberti@gmail.com>
*/

#include <pthread.h>

#include "ssl_thr_lock.h"


#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self( )

/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE *mutex_arr= NULL;
 
void locking_function (int mode, int n, const char * file, int line)
{
  (void) file;
  (void) line;
    if (mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_arr[n]);
    else
        MUTEX_UNLOCK(mutex_arr[n]);
}
  
unsigned long id_function(void)
{
    return ((unsigned long)THREAD_ID);
}
  
int thread_openssl_setup(void)
{
    int i;
  
    mutex_arr = (MUTEX_TYPE *) malloc(CRYPTO_num_locks( ) * sizeof(MUTEX_TYPE));

    if (!mutex_arr)
        return -1;

    for (i = 0; i < CRYPTO_num_locks( ); i++)
        MUTEX_SETUP(mutex_arr[i]);

    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);
    return 0;
}
  
int thread_openssl_cleanup(void)
{
    int i;
  
    if (! mutex_arr)
        return -1;

    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);

    for (i = 0; i < CRYPTO_num_locks( ); i++)
        MUTEX_CLEANUP(mutex_arr[i]);

    free(mutex_arr);
    mutex_arr = NULL;

    return 0;
}
