/* 
   ssl_thr_lock.h

   Taken with thanks from CURL project opensslthreadlock.c by 
   Jeremy Brown <jbrown_at_novell.com> 
   and modified by 
   Robert Iakobashvili <coroberti@gmail.com>
*/

#ifndef SSL_THR_LOCK_H
#define SSL_THR_LOCK_H

#include <openssl/crypto.h>

void locking_function (int mode, int n, const char * file, int line);
u_long id_function(void);

int thread_openssl_setup(void);
int thread_openssl_cleanup(void);


#endif /* SSL_THR_LOCK_H */
