/* 
 *     environment.c
 *
 * 2006-2007 Copyright (c) 
 * Robert Iakobashvili, <coroberti@gmail.com>
 * Michael Moser, <moser.michael@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// must be the first include
#include "fdsetsize.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "batch.h"
#include "conf.h"

#define OPEN_FDS_SUGGESTION 19999


int test_environment (batch_context* bctx)
{
  struct rlimit file_limit;
  int ret;

    //fprintf(stderr, " __FD_SETSIZE %d  FD_SETSIZE %d __NFDBITS %d  \n",
    //      __FD_SETSIZE,  FD_SETSIZE, __NFDBITS );
  
  ret = getrlimit(RLIMIT_NOFILE, &file_limit);

  if (!ret && file_limit.rlim_cur <= (unsigned int) bctx->client_num)
    {
      fprintf(stderr,
              "%s - ERROR: the current limit of open descriptors for the shell (%d) \n"
              "is below the number of clients in the batch (%d).\n"
              "Please, increase the limit, e.g. by running #ulimit -n %d\n",
              __func__, (int)(file_limit.rlim_cur), bctx->client_num, OPEN_FDS_SUGGESTION);
      return -1;
    }

  if (!ret && file_limit.rlim_cur <= (unsigned int) (3*bctx->client_num))
  {
      fprintf(stderr,
              "%s - NOTE: the current limit of open descriptors for the shell (%d) is higher \n"
              "than number of clients in the batch (%d).\n"
              "Still, due to time-waiting state of TCP sockets, the number of the sockets may be not enough.\n"
              "Consider, increasing the limit, e.g. by running #ulimit -n %d\n",
              __func__, (int)(file_limit.rlim_cur), bctx->client_num, OPEN_FDS_SUGGESTION);

      if (file_limit.rlim_cur > 5000)
      {
          fprintf(stderr, "and/or chaging temporarily TCP stack defaults by running:\n"
                  "#echo 1 > /proc/sys/net/ipv4/tcp_tw_recycle and/or\n"
                  "#echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");
      }

      fprintf (stderr, " Please, press ENTER to continue.\n");
             
      getchar ();

      return 0;
  }

  if (loading_mode != LOAD_MODE_HYPER)
  {
      if (!ret && file_limit.rlim_cur > CURL_LOADER_FD_SETSIZE)
      {
          fprintf(stderr, 
                  "%s - ERROR: The limit of open descriptors in your shell is \n"
                  "larger than allowed for modes using poll/select (to prevent fd_set smashing).\n"
                  "The limit for non-Hyper modes is %d fdescriptors, whereas the current shell limit is %d\n"
                  "Edit CURL_LOADER_FD_SETSIZE value in Makefile and recompile the program.\n"
                  "Alternatively, decrease the shell limit by e.g. #ulimit -n %d  or "
                  "use epoll-based hyper-mode (add \"-m 0\" to your command line).\n",
                  __func__ , CURL_LOADER_FD_SETSIZE, (int) file_limit.rlim_cur,
                  CURL_LOADER_FD_SETSIZE - 1);   
          return -1;
      }
  }

  return 0;
}
