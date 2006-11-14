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
static int login_clients_storm (client_context* cctx, int cycle);
static int logoff_clients_storm (client_context*const cctx, int cycle);
static int mget_url_storm (batch_context* bctx, float m_time);

/*
  Simulates user activity upon storm loading mode.
*/
int user_activity_storm (client_context*const cdata)
{
  batch_context* bctx = cdata->bctx;
  long cycle = 0, k = 0;
  long u_index = 0;

  bctx->start_time = bctx->last_measure = get_tick_count();

  /* 
     Make authentication login. If login operation should not be cycled.
     For such cases login is performed only once for each user.
  */
  if (bctx->do_login && !bctx->login_cycling)
    {
      if (login_clients_storm (cdata, 0) == -1)
        {
          fprintf (stderr, "%s - \"%s\" - login_clients_storm() failed.\n", 
                   __func__, bctx->batch_name);
          return -1;
        }

      // First string to contain statistics for non-cycling logins 
      dump_intermediate_and_advance_total_statistics (bctx);
    }
  
  for (cycle = 0; cycle < bctx->cycles_num ; cycle++)
    {
      
      bctx->last_measure = get_tick_count();
      /* 
         Login, when the login operation to be done in cycles. 
       */
      if (bctx->do_login && bctx->login_cycling)
        {
          if (login_clients_storm (cdata, cycle) == -1)
            {
              fprintf (stderr, "%s - login_clients_storm() failed.\n", __func__);
              return -1;
            }
        }

      /* Sleep in between the login urls. */
      sleep (bctx->login_url.url_interleave_time);

      /*
         UAS - user activity simulation, fetching urls from the UAS-array.
      */
      for (u_index = 0; u_index < bctx->uas_urls_num; u_index++)
        {
          // fprintf (stderr,"\n\"%s\" - %s - cycle %ld of fetching url %ld .\n\n",
          //         bctx->batch_name, __func__, cycle, u_index);

          /* 
             Remove all CURL handles (clients) from the CURL multi-handle.
             Reset each CURL handle (client), reset-up the handle with new url
             params and add it back to the CURL multi-handle.
          */
          for (k = 0 ; k < bctx->client_num ; k++)
            {
              if (cdata[k].client_state != CSTATE_ERROR)
                cdata[k].client_state = CSTATE_UAS_CYCLING;
              //fprintf (stderr, "%s - client_num %ld, state %d\n", 
              //        __func__, k, cdata[k].client_state);

              setup_curl_handle (&cdata[k],
                                   &bctx->uas_url_ctx_array[u_index], /* index of url string in array */
                                   cycle,
                                   0);
            }
            
          /* Fetch the new url by each client of the batch.*/
          if ( mget_url_storm (bctx, 
                               bctx->uas_url_ctx_array[u_index].url_completion_time) == -1)
            {
              fprintf (stderr, 
                       "%s -mget_url_storm failed at cycle %ld of fetching url %ld .\n",
                       __func__, cycle, u_index) ;
              return -1;
            }

          //fprintf (stderr, 
          //         "\n%s - sleeping after cycle %ld of getting url %d.\n\n", 
          //        __func__, cycle, u_index);

          /* Sleep in between the urls, simulating user-activity. */
          sleep (bctx->uas_url_ctx_array[u_index].url_interleave_time);
        }

      /* 
         Logoff, when required logoff in cycling. 
      */
      if (bctx->do_logoff && bctx->logoff_cycling)
        {
          if (logoff_clients_storm (cdata, cycle) == -1)
            {
              fprintf (stderr, "%s - logoff_clients_storm() failed .\n", __func__);
              return -1;
            }
        }

      /* Sleep in between the logoff urls. */
      sleep (bctx->logoff_url.url_interleave_time);
 
      /* 
         After completing a cycle - rewind the file. Thus, we are keeping the current run
         and a limited history run in the logfile. 
      */
      if (cycle > 0 && ! (cycle%logfile_rewind_cycles_num))
          rewind (cdata->file_output);
      
      // Bring statistics at the end of each cycle
      dump_intermediate_and_advance_total_statistics (bctx);
    }

  /* 
     Logoff, if not required logoff in cycling 
  */
  if (bctx->do_logoff && ! bctx->logoff_cycling)
    {
      if (logoff_clients_storm (cdata, 0) == -1)
        {
          fprintf (stderr, "%s - logoff_clients_storm() failed .\n", __func__);
          return -1;
        }

      // Last string to contain statistics for non-cycling logoffs 
      dump_intermediate_and_advance_total_statistics (bctx);
    }

  for (k = 0 ; k < bctx->client_num ; k++)
    {
      if (cdata[k].client_state != CSTATE_ERROR)
      {
        cdata[k].client_state = CSTATE_FINISHED_OK;
      }
      //fprintf (stderr, "%s - client_num %ld, state %d\n", 
      //         __func__, k, cdata[k].client_state);
    }

  dump_final_statistics (cdata);
  
  fprintf (stderr, "\n%s - cycling done, exiting .\n\n", __func__);
  return 0;
}

static int mget_url_storm (batch_context* bctx, float m_time)
{
  struct timeval timeout;
  CURLM *mhandle = bctx->multiple_handle;
  int still_running = 0;
  float max_timeout = m_time; //bctx->uas_url_ctx_array[bctx->url_index].url_completion_time;

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

      //fprintf (stderr, "%s - Waiting for %d clients with seconds %f.\n", 
      //         bctx->batch_name, still_running, max_timeout);

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

  curl_multi_cleanup(bctx->multiple_handle);

  if (!(bctx->multiple_handle = curl_multi_init()))
    {
      fprintf (stderr, 
               "%s - curl_multi_init() failed to initialize bctx->multiple_handle .\n",
               __func__);
      return -1;
    }

  const float max_completion_time = (in_off == POST_LOGIN) ? 
    bctx->login_url.url_completion_time : 
    bctx->logoff_url.url_completion_time;

  const long sleep_time = (in_off == POST_LOGIN) ? 
    bctx->login_url.url_interleave_time : 
    bctx->logoff_url.url_interleave_time;

  /* 
     An average user is used to read and think a bit 
     prior to posting his credentials.
  */
  sleep (sleep_time);

  /* Add the POST fields */
  for (i = 0 ; i < bctx->client_num; i++)
    { 
      /* 
         Fill POST login or logoff fields. Note, that it should be done on CURL handle
         removed from MCURL.
       */
      curl_easy_setopt(bctx->client_handles_array[i], CURLOPT_POSTFIELDS, 
                       in_off ? clients[i].post_data_login : clients[i].post_data_logoff);

      curl_multi_add_handle(bctx->multiple_handle, 
                            bctx->client_handles_array[i]);
    }

  /* Make the actual posting */
  if (mget_url_storm (bctx, max_completion_time) == -1) 
    {
      fprintf (stderr, "%s - mget_url_storm() failed for the authentication POST.\n",
               __func__);
      return -1;
    }
  
  return 0;
}

static int login_clients_storm (client_context* cdata, int cycle)
{
  int k = 0;
  batch_context* bctx= cdata->bctx;

  /* Setup client handles for GET-method. */
  for (k = 0 ; k < bctx->client_num ; k++)
    {
      cdata[k].client_state = CSTATE_LOGIN;
      //fprintf (stderr, "%s - client_num %d, state %d\n", 
      //         __func__, k, CSTATE_LOGIN);

      setup_curl_handle (&cdata[k], /* pointer to client context */
                           &bctx->login_url, /* login url */
                           cycle, /* zero cycle */
                           0 /*without POST buffers as a more general case*/  
                           );
    }

  if (bctx->login_req_type == LOGIN_REQ_TYPE_GET_AND_POST)
    {
      /*
        GET the first url to be the POST form. When the first url is redirected 
        and authentication POSTing performed at the redirected url, 
        it can be done as below.
      */

      if (mget_url_storm (bctx, bctx->login_url.url_completion_time) == -1)
        {
          fprintf (stderr, 
                   "%s - \"%s\" - error: mget_url_storm()- failed for the login URL.\n", 
                   __func__, bctx->batch_name);
          return -1;
        }
    }
  
  /* Make POSTing of login credentials for each client. Pass POST-buffers. */
  if (posting_credentials_storm (cdata, POST_LOGIN) == -1)
    {
      fprintf (stderr, "%s - \"%s\" - error: posting_credentials_storm()- failed.\n", 
               __func__, bctx->batch_name);
      return -1;
    }
  return 0;
}

static int logoff_clients_storm (client_context*const cdata, int cycle)
{
  int k = 0;
  batch_context* bctx = cdata->bctx;

  /* 
     Setup the last url for logoff without any POST-buffer 
  */
  for (k = 0 ; k < bctx->client_num ; k++)
    {
      if (cdata[k].client_state != CSTATE_ERROR) 
        cdata[k].client_state = CSTATE_LOGOFF;
      //fprintf (stderr, "%s - client_num %d, state %d\n", 
      //        __func__, k, cdata[k].client_state);

      setup_curl_handle (&cdata[k],
                           &bctx->logoff_url, 
                           cycle, /* Cycle number does not matter here */
                           0 /* General case, without POST */
                           );
    }

  if (bctx->logoff_req_type != LOGOFF_REQ_TYPE_POST)
    {
      /* 
         GET the logoff url, if configured to do it.  
      */
      if (mget_url_storm (bctx, bctx->logoff_url.url_completion_time) == -1)
        {
          fprintf (stderr, 
                   "%s - \"%s\" - error: mget_url_storm()- failed for the logoff URL.\n", 
                   __func__, bctx->batch_name);
          return -1;
        }
    }

  if (bctx->logoff_req_type != LOGOFF_REQ_TYPE_GET)
    {
      /*
        POST the logoff url, if configured to do it.

        Sets the POSTing logoff buffer to the curl-handlers and 
        POSTs the last url or the url, retrived by the previous GET 
      */
      if (posting_credentials_storm (cdata, POST_LOGOFF) == -1)
        {
          fprintf (stderr, "%s - \"%s\" - posting_credentials_storm()- failed to logoff.\n", 
                   __func__, bctx->batch_name);
          return -1;
        }
    }
  return 0;
}
