/* 
*     loader_storm.c
*
* 2006 Copyright (c) 
* Robert Iakobashvili, <coroberti@gmail.com>
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
*
* Cooked from the CURL-project examples with thanks to the 
* great CURL-project authors and contributors.
*/

#include <unistd.h>

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"

#define POST_LOGIN 1
#define POST_LOGOFF 0

static int posting_credentials_storm (client_context* clients, int in_off);

int mget_url_storm (batch_context* bctx)
{
  CURLM *mhandle = bctx->multiple_handle;
  float max_timeout = bctx->url_ctx_arr[bctx->url_index].url_completion_time;
  int still_running = 0;
  struct timeval timeout;

  while(CURLM_CALL_MULTI_PERFORM== curl_multi_perform (mhandle, &still_running))
    ;

  while (still_running && max_timeout > 0.0) 
    {
      int rc, maxfd;
      fd_set fdread, fdwrite, fdexcep;

      FD_ZERO(&fdread); FD_ZERO(&fdwrite); FD_ZERO(&fdexcep);
      timeout.tv_sec = 0 ;
      timeout.tv_usec = 500000;   

      max_timeout -= ((float)timeout.tv_sec + (float)timeout.tv_usec/1000000.0);
      curl_multi_fdset(mhandle, &fdread, &fdwrite, &fdexcep, &maxfd);

      fprintf (stderr, "%s - Waiting for %d clients with seconds %f.\n", 
               bctx->batch_name, still_running, max_timeout);

      rc = select (maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
      switch(rc) 
        {
        case -1: /* select error */
          break;
        case 0:
        default: /* timeout or readable/writable sockets */
          while (CURLM_CALL_MULTI_PERFORM 
                 == curl_multi_perform(mhandle, &still_running));
          break;
        }
    }
  return 0;
}

/*
  
*/
static int posting_credentials_storm (client_context* clients, int in_off)
{
  batch_context* bctx = clients->bctx;
  int i = 0;

  if ((in_off != POST_LOGIN) && (in_off != POST_LOGOFF))
    return -1;

  /* Somebody removes or at least not adds curl-handles from the multi-handle */
  curl_multi_cleanup(bctx->multiple_handle);

  if (!(bctx->multiple_handle = curl_multi_init()))
    {
      fprintf (stderr, 
               "%s - curl_multi_init() failed to initialize bctx->multiple_handle .\n",
               __func__);
      return -1;
    }
  
  const long url_index = (in_off ==  POST_LOGIN) ? 0 : bctx->urls_num -1;  
  
  /* 
     An average user is used to read and think a bit prior to posting 
     his credentials.
  */
  sleep (bctx->url_ctx_arr[url_index].url_interleave_time);

  /* Add the POST fields */
  for (i = 0 ; i < bctx->client_num; i++)
    { 
      /* Fill POST login fields */
      curl_easy_setopt(bctx->client_handles_array[i], CURLOPT_POSTFIELDS, 
                       in_off ? clients[i].post_data_login : clients[i].post_data_logoff);

      curl_multi_add_handle(bctx->multiple_handle, 
                            bctx->client_handles_array[i]);
    }

  /* Make the actual posting */
  if (mget_url_storm (bctx) == -1) 
    {
      fprintf (stderr, "%s - mget_url_storm() failed for the authentication POST.\n",
               __func__);
      return -1;
    }
  
  return 0;
}

/*
  Simulates user activity upon storm loading mode.
*/
int user_activity_storm (client_context*const cdata)
{
  batch_context* bctx = cdata->bctx;
  long cycle = 0, k = 0;
  long url_index = 0;

  /* 
     Make authentication login, if configured to do it. Such login 
     is done using the first url and only once for each user.
  */
  if (bctx->do_auth && post_login_format[0])
    {
      if (authenticate_clients_storm (cdata) == -1)
        {
          fprintf (stderr, "%s - \"%s\" - authenticate_clients_storm() failed.\n", 
                   __func__, bctx->batch_name);
          return -1;
        }
    }

  /* 
     We are starting from the second url, when the first URL has been used 
     for authentication and commandline option '-a' was not specified.
     When authentication is required and logoff is defined by -w command line
     option, we are using the last url solely for logoff.
  */
  const int do_logoff = (bctx->do_auth && w_logoff_mode) ? 1 : 0;

  const long url_start = (bctx->do_auth && !authentication_url_load) ? 1 : 0;
  const long url_end = do_logoff ? bctx->urls_num - 1 : bctx->urls_num; 
  
  for (cycle = 0; cycle < bctx->repeat_cycles_num ; cycle++)
    {
      for (url_index = url_start; url_index < url_end ; url_index++)
        {
          //fprintf (stderr,"\n\"%s\" - %s - cycle %ld of fetching url %ld .\n\n",
          //         bctx->batch_name, __func__, cycle, url_index);

          /* 
             Remove all CURL handles (clients) from the CURL Multi-handle.
             Reset each CURL handle (client), reset-up the handle with new URL
             params and add it back to the CURL Multi-handle.
          */
          for (k = 0 ; k < bctx->client_num ; k++)
            {
              curl_multi_remove_handle (bctx->multiple_handle, 
                                        bctx->client_handles_array[k]);
  
              single_handle_setup (&cdata[k],
                                   url_index, /* index of url string in array */
                                   cycle, NULL);
            }
            
          /* Fetch the new url by each client of the batch.*/
          if ( mget_url_storm (bctx) == -1)
            {
              fprintf (stderr, 
                       "%s -mget_url_storm failed at cycle %ld of fetching url %ld .\n",
                       __func__, cycle, url_index) ;
              return -1;
            }

          //fprintf (stderr, 
          //         "\n%s - sleeping after cycle %ld of getting url %d.\n\n", 
          //        __func__, cycle, url_index);

          /* Sleep in between the urls, simulating user-activity. */
          sleep (bctx->url_ctx_arr[url_index].url_interleave_time);
        }
 
      /* 
         After completing a cycle - rewind the file. Thus, we are keeping only 
         a limited history run in the batch and the current run.
      */
      if (cycle > 0 && ! (cycle%logfile_rewind_cycles_num))
          rewind (cdata->file_output);
    }

  /* Using the last url solely for logoff and only once for each client. */
  if (do_logoff)
    {

      /* Setup the last url for logoff without any POST-buffer */
      for (k = 0 ; k < bctx->client_num ; k++)
        {
          curl_multi_remove_handle (bctx->multiple_handle, 
                                    bctx->client_handles_array[k]);
          
          single_handle_setup (&cdata[k],
                               bctx->urls_num - 1 , /* index of the last url */
                               cycle, NULL);
        }

      if (w_logoff_mode != LOGOFF_TYPE_POST_ONLY)
        {
          /* GET the last url, if configured to do it.  */
          if (mget_url_storm (bctx) == -1)
            {
              fprintf (stderr, 
                       "%s - \"%s\" - mget_url_storm()- failed for the initial URL.\n", 
                       __func__, bctx->batch_name);
              return -1;
            }
        }

      if (w_logoff_mode != LOGOFF_TYPE_GET_ONLY)
        {
          /* Sets the POSTing logoff buffer to the curl-handlers and 
             POSTs the last url or the url, retrived by the previous GET */
          if (posting_credentials_storm (cdata, POST_LOGOFF) == -1)
            {
              fprintf (stderr, "%s - \"%s\" - posting_credentials_storm()- failed to logoff.\n", 
                       __func__, bctx->batch_name);
              return -1;
            }
        }
    }
  
  fprintf (stderr, "\n%s - cycling done, exiting .\n\n", __func__);
  return 0;
}

int authenticate_clients_storm (client_context* cctx)
{
  batch_context* bctx= cctx->bctx;

  if (z_login_mode == LOGIN_TYPE_GET_AND_POST) /* '-z 3' in commandline skips it */
    {
      /*
        GET the first url to be the POST form. When the first url is redirected 
        and authentication POSTing performed at the redirected url, 
        it can be done as below.
      */
      if (mget_url_storm (bctx) == -1)
        {
          fprintf (stderr, 
                   "%s - \"%s\" - mget_url_storm()- failed for the initial URL.\n", 
                   __func__, bctx->batch_name);
          return -1;
        }
    }
  
  /* Make POSTing of login credentials for each client.*/
  if (posting_credentials_storm (cctx, POST_LOGIN) == -1)
    {
      fprintf (stderr, "%s - \"%s\" - posting_credentials_storm()- failed.\n", 
               __func__, bctx->batch_name);
      return -1;
    }
  return 0;
}
