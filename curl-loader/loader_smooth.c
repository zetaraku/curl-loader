/* 
*     loader_smooth.c
*
* 2006 Copyright (c)
* Michael Moser, <moser.michael@gmail.com>
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

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"

#define DEFAULT_SMOOTH_URL_COMPLETION_TIME 6.0

static int mget_url_smooth (batch_context* bctx);
static int mperform_smooth (batch_context* bctx, int* still_running);
static int load_next_step (client_context* cctx);
static int load_init_state (client_context* cctx);
static int load_login_state (client_context* cctx);
static int load_uas_state (client_context* cctx);
static int load_logoff_state (client_context* cctx);

static int is_last_cycling_state (client_context* cctx);
static void advance_cycle_num (client_context* cctx);
static int on_cycling_completed  (client_context* cctx);

static int setup_login_logoff (client_context* cctx, const int login);
static int setup_uas (client_context* cctx);

static unsigned long get_tick_count ();
static void dump_statistics (
                             unsigned long period,  
                             stat_point *http, 
                             stat_point *https);
static void dump_stat_workhorse (int clients, 
                                 unsigned long period,  
                                 stat_point *http, 
                                 stat_point *https);

int user_activity_smooth (client_context* cctx)
{
  int j, i;
  unsigned long now;
  batch_context* bctx = cctx->bctx;
  
  bctx->start_time = bctx->last_measure = get_tick_count();

  for (j = 0 ; j < bctx->client_num; j++)
    if (load_next_step (&cctx[j]) == -1)
      {
        fprintf(stderr,"%s error: load_next_step() initial failed\n", __func__);
        return -1;
      }
  
  while(bctx->active_clients_count) 
    {
      if (mget_url_smooth (bctx) == -1) 
        {
          fprintf (stderr, "%s error: mget_url () failed.\n", __func__) ;
          return -1;
        }
    }

  now = get_tick_count(); 

  /* Dump final statistics. */
  dump_stat_workhorse (bctx->active_clients_count, 
                       now - bctx->last_measure, 
                       &bctx->http_delta,  
                       &bctx->https_delta);

  stat_point_add (&bctx->http_total, &bctx->http_delta);
  stat_point_add (&bctx->https_total, &bctx->https_delta);  
    
  fprintf(stderr,"===========================================\n");
  fprintf(stderr,"End of test:\n"); 
  fprintf(stderr,"===========================================\n");
  
  now = get_tick_count();
  
  dump_statistics ((now - bctx->start_time)/ 1000, 
                   &bctx->http_total,  
                   &bctx->https_total); 
 
   for (i = 0 ; i < bctx->client_num; i++)
    {
      if (cctx[i].client_state == CSTATE_ERROR)
        {
          fprintf(stderr,"%s - error client %s failed\n", 
                  __func__, cctx[i].client_name);
        }
    }

  return 0;
}

static int mget_url_smooth (batch_context* bctx)  		       
{
    CURLM *mhandle =  bctx->multiple_handle;
    const char *name = bctx->batch_name;
    float max_timeout = DEFAULT_SMOOTH_URL_COMPLETION_TIME;
    
    if (bctx->uas_url_ctx_array)
      {
      max_timeout = bctx->uas_url_ctx_array[0].url_completion_time;
      }
 
    int still_running = 0;
    struct timeval timeout;

    mperform_smooth (bctx, &still_running);

    while (still_running && max_timeout > 0.0) 
    {
        int rc, maxfd;
        fd_set fdread, fdwrite, fdexcep;

        FD_ZERO(&fdread); FD_ZERO(&fdwrite); FD_ZERO(&fdexcep);
        timeout.tv_sec = 0 ;
        timeout.tv_usec = 500000;   

        max_timeout -= ((float)timeout.tv_sec + (float)timeout.tv_usec/1000000.0) ; 
        curl_multi_fdset(mhandle, &fdread, &fdwrite, &fdexcep, &maxfd);
        fprintf (stderr, "%s - Waiting for %d clients with seconds %f.\n", 
                 name, still_running, max_timeout);

        rc = select (maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout) ;
        switch(rc)
        {
        case -1: /* select error */
            break;
        case 0:
        default: /* timeout or readable/writable sockets */
            mperform_smooth (bctx, &still_running);            
	    break;
        }
    }	 
    return 0;
}

static int mperform_smooth (batch_context* bctx, int* still_running)
{
    CURLM *mhandle =  bctx->multiple_handle;     
    
    while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(mhandle,still_running))
      ;

    unsigned long now = get_tick_count(); 

    if ((now - bctx->last_measure) > snapshot_timeout) 
      {
        // dump statistics.
        dump_stat_workhorse( 
                          bctx->active_clients_count, 
                          now - bctx->last_measure, 
                          &bctx->http_delta,  
                          &bctx->https_delta);
	  
        stat_point_add (&bctx->http_total, &bctx->http_delta);
        stat_point_add (&bctx->https_total, &bctx->https_delta); 
        stat_point_reset (&bctx->http_delta); 
        stat_point_reset (&bctx->https_delta); 
        bctx->last_measure = get_tick_count(); 
      }
 
    int msg_num;	
    CURLMsg *msg;
	  
    while( (msg = curl_multi_info_read (mhandle, &msg_num)) != 0)
      {
        if (msg->msg == CURLMSG_DONE)
          {
              /* TODO: CURLMsg returns 'result' field as curl return code. We may wish to use it. */

            /* Finally API instead of bloody hacking */

            CURL *handle = msg->easy_handle;
            client_context *cctx = NULL;

            curl_easy_getinfo (handle, CURLINFO_PRIVATE, &cctx);

            int client_state =  load_next_step (cctx);
            //fprintf (stderr, "%s - after load_next_step client state %d.\n", __func__, client_state);

            if (client_state == CSTATE_ERROR || client_state == CSTATE_FINISHED_OK) 
              {
                bctx->active_clients_count--; 
              }
          
            if (msg_num <= 0)
              break;
          }
      }

    return 0;
}

static int load_next_step (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  //fprintf (stderr, "%s - entered client_state:%d, url:%d.\n", 
  //          __func__, cctx->client_state, cctx->uas_url_curr_index);

  if (cctx->client_state != CSTATE_ERROR &&
      cctx->client_state != CSTATE_INIT)
    {
      cctx->is_https ? bctx->https_delta.requests++ : bctx->http_delta.requests++;
      //fprintf (stderr, "Inc to %lld\n", bctx->http_delta.requests); 
    }

  switch (cctx->client_state)
    {
    case CSTATE_ERROR:
      return cctx->client_state;
    case CSTATE_INIT:
      return load_init_state (cctx);
    case CSTATE_LOGIN:
       return load_login_state (cctx);
    case CSTATE_UAS_CYCLING:
      return load_uas_state (cctx);
    case CSTATE_LOGOFF:
      return load_logoff_state (cctx);
    }
  
  return CSTATE_ERROR;
}

static int load_init_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  if (bctx->do_login) // normally first operation is login
      return load_login_state (cctx);
  else if (bctx->do_uas) // sometimes, no login defined at all
      return load_uas_state (cctx);
  else if (bctx->do_logoff) // Logoff load only?  If user wants it, lets do it.
      return load_logoff_state (cctx);

  return (cctx->client_state = CSTATE_ERROR);
}

static int is_last_cycling_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  switch (cctx->client_state)
    {
    case CSTATE_LOGIN:
      return (bctx->login_cycling && !bctx->do_uas && !(bctx->do_logoff && bctx->logoff_cycling)) ? 1 : 0;
    case CSTATE_UAS_CYCLING:
      return (!(bctx->do_logoff && bctx->logoff_cycling)) ? 1 : 0;
    case CSTATE_LOGOFF: // logoff cycling, if exists, is the last state of a cycle
      return bctx->logoff_cycling ? 1 : 0;
    }
  return 0;
}

static void advance_cycle_num (client_context* cctx)
{
  cctx->cycle_num++;
  
  //fprintf (stderr, "%s -cycle_num:%ld cycles_num:%ld\n", 
  //         __func__, cctx->cycle_num);
}

static int on_cycling_completed  (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  /* Go to not-cycling logoff, if exists, or to finish. */
  if (bctx->do_logoff && !bctx->logoff_cycling)
    return load_logoff_state (cctx);

  return CSTATE_FINISHED_OK;
}

static int setup_login_logoff (client_context* cctx, const int login)
{
  batch_context* bctx = cctx->bctx;
  int posting_after_get = 0;

  if ( (login && bctx->login_req_type == LOGIN_REQ_TYPE_GET_AND_POST)  ||
       (!login && bctx->logoff_req_type == LOGOFF_REQ_TYPE_GET_AND_POST)
       )
    {  
      if (cctx->get_post_count == 0)
        cctx->get_post_count = 1;
      else if (cctx->get_post_count == 1)
        {
          posting_after_get = 1;
          cctx->get_post_count = 0;
        }
    }
  
  if (!posting_after_get)
    {
      /* 
         Three possible cases are treated here:
         - GET, which is by itself enough for logoff (thanks to cookies)
         - GET, which will be later followed by later POST login/logoff;
         - POST, which is the standalone login/logoff, without any previous GET;
      */
      int post_standalone = 0;
      if ((login && bctx->login_req_type == LOGIN_REQ_TYPE_POST) ||
          (!login && bctx->logoff_req_type == LOGOFF_REQ_TYPE_POST))
        {
          post_standalone = 1;
        }

      setup_curl_handle (cctx,
                           login ? &bctx->login_url : &bctx->logoff_url,
                           0, /* not applicable for the smooth mode */
                           post_standalone /* if 'true' -POST, else GET */  
                           );
    }
  else
    {
      /* 
         The only case here, is when doing POST after GET. 
         We should preserve the url kept in CURL handle after GET,
         which may be the result of redirection/s,  but use HTTP POST 
         request method with post login/logoff fields. 
      */
      CURL* handle = bctx->client_handles_array[cctx->client_index];

      curl_multi_remove_handle (bctx->multiple_handle, handle);
      
      /* 
         Just add POSTFIELDS.
         Note, that it should be done on CURL handle outside (removed) 
         from MCURL. Add it back afterwords.
      */
      curl_easy_setopt(handle, CURLOPT_POSTFIELDS, 
                       login ? cctx->post_data_login : cctx->post_data_logoff);

      curl_multi_add_handle(bctx->multiple_handle, handle);
    }

  return cctx->client_state = login ? CSTATE_LOGIN : CSTATE_LOGOFF;
}

static int setup_uas (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  setup_curl_handle (cctx,
                       &bctx->uas_url_ctx_array[cctx->uas_url_curr_index], /* current url */
                       0, /* cycle, do we need it */ 
                       0 /* GET - zero, unless we'll need to make POST here */
                       );

  return cctx->client_state = CSTATE_UAS_CYCLING;
}

static int load_login_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  /*
    Test for login state, if a single login operation has been accomplished. 
    Sometimes, the operation contains two elements: GET and POST.
  */
  if (cctx->client_state == CSTATE_LOGIN)
    {
      if ((bctx->login_req_type != LOGIN_REQ_TYPE_GET_AND_POST) ||
          (bctx->login_req_type == LOGIN_REQ_TYPE_GET_AND_POST && !cctx->get_post_count))
        {
          /* Accomplished a single login operation. */

          if (is_last_cycling_state (cctx))
            {
              /*
                If we are login cycling and the last/only cycling state,
                we are in charge for advancing cycles counter.
              */
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  /* Either jump to logoff or to finish_ok. */
                  return on_cycling_completed (cctx);
                }
              else
                {
                  /* Configured to cycle, but the state is the last cycling state 
                     and also the only cycling state, therefore, continue login. */
                  return setup_login_logoff (cctx, 1); // one means login
                }
            }
 
          if (bctx->do_uas)
            return load_uas_state (cctx);
          else if (bctx->do_logoff)
            return load_logoff_state (cctx);
          else
            return CSTATE_FINISHED_OK;
        }
    }

  /* Non-LOGIN states are all falling below: */
  return setup_login_logoff (cctx, 1);
}

static int load_uas_state (client_context* cctx)
{ 
  batch_context* bctx = cctx->bctx;

  if (cctx->client_state == CSTATE_UAS_CYCLING)
    {
      cctx->uas_url_curr_index++;

       if (cctx->uas_url_curr_index >= (size_t)(bctx->uas_urls_num))
        {
          /* Accomplished all urls for a single UAS -cycle. */

          cctx->uas_url_curr_index = 0;
 
          if (is_last_cycling_state (cctx))
            {
              /* If UAS is the last cycling state, advance cycle counter. */
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  // Either logoff or finish_ok.
                  return on_cycling_completed (cctx);
                }
              else
                {
                  // continue cycling - take another cycle
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx);
                  else
                    return setup_uas (cctx);
                }
            }

          // We are not the last cycling state. The guess is, that the
          // next state is logoff with cycling.
          return load_logoff_state (cctx);
        }
    }
  else
    {
      cctx->uas_url_curr_index = 0;
    }

  /* Non-UAS states are all falling below: */
  return setup_uas (cctx);
}

static int load_logoff_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  /*
    Test for logoff state, if a single login operation has been accomplished. 
    Sometimes, the operation contains two elements: GET and POST.
  */
  if (cctx->client_state == CSTATE_LOGOFF)
    {
      if ((bctx->logoff_req_type != LOGOFF_REQ_TYPE_GET_AND_POST) ||
          (bctx->logoff_req_type == LOGOFF_REQ_TYPE_GET_AND_POST && !cctx->get_post_count))
        {
          /* 
             Indeed, we have accomplished a single logoff operation. 
          */
          if (is_last_cycling_state (cctx))
            {
              /*
                If logoff cycling ,we are in charge for advancing cycles counter.
              */
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  return on_cycling_completed (cctx); // Goes to finish-ok
                }
              else
                {
                  /*
                    Continue cycling - take another cycle
                  */
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx);
                  else if (bctx->do_uas)
                    return load_uas_state (cctx);
                  else /* logoff is the only cycling state - strange, but allow it */
                    return setup_login_logoff (cctx, 0); // zero means logoff
                }
            }
 
          /* If not doing logoff cycling, means single logoff done - go to finish-ok */
            return CSTATE_FINISHED_OK;
        }
    }

  /* Non-LOGOFF states are all falling below: */
  return setup_login_logoff (cctx, 0);
}

static unsigned long get_tick_count ()
{
  struct timeval  tval;

  if (!gettimeofday (&tval, NULL) == -1)
    {
      fprintf(stderr, "%s - gettimeofday () failed with errno %d.\n", 
              __func__, errno);
      exit (1);
    }
  return tval.tv_sec * 1000 + (tval.tv_usec / 1000);
}

static void dump_statistics (
                             unsigned long period,  
                             stat_point *http, 
                             stat_point *https)
{
  if (period == 0)
    {
      fprintf(stderr,
              "%s - less than 1 second duration test without statistics.\n",
              __func__);
      return;
    } 
  
  fprintf(stderr,
	      "Test took %d seconds\n", (int) period);
  fprintf(stderr,
          "HTTP(Err: %d In/s: %d Out/s: %d Req/s: %d) HTTPS(Err: %d In/s: %d Out/s: %d Req/s: %d)\n",    
	      (int)  http->errors, (int) (http->data_in/period), (int) (http->data_out/period),
	      (int) (http->requests/period),
	      (int)  https->errors, (int) (https->data_in/period),
	      (int) (https->data_out/period), (int) (https->requests/period)
          );
}

static void dump_stat_workhorse (int clients, 
                                 unsigned long period,  
                                 stat_point *http, 
                                 stat_point *https)
{
  period /= 1000;
  if (period == 0) 
    {
      period = 1;
    }

  fprintf(stderr,
	      "Clients: %d Time %d sec HTTP(Err: %d In/s: %d Out/s: %d Req: %d) HTTPS(Err: %d In/s: %d Out/s: %d Req: %d)\n",
	      (int) clients, (int) period, 
	      (int) http->errors, (int) (http->data_in/period),
	      (int) (http->data_out/period), (int) http->requests,
	      (int) https->errors, (int) (https->data_in/period),
	      (int) (https->data_out/period), (int) https->requests);
}

