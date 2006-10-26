/* 
*     loader_smooth.c
*
* 2006 Copyright (c)
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
*
* Cooked from the CURL-project examples with thanks to the 
* great CURL-project authors and contributors.
*/

#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

/* Very bad - bloody hacking */
#define HAVE_GETTIMEOFDAY
#include <urldata.h>

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"

static int mget_url_smooth (batch_context* bctx);
static int mperform_smooth (batch_context* bctx, int* still_running);
static int load_next_step (client_context* cctx);
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
  int i;
  unsigned long now;
  batch_context* bctx = cctx->bctx;
  
  bctx->start_time = bctx->last_measure = get_tick_count(); 
  
  while(bctx->curl_handlers_count) 
    {
      if (mget_url_smooth (bctx) == -1) 
        {
          fprintf (stderr, "%s - mget_url () failed.\n", __func__) ;
          return -1;
        }
    }

  now = get_tick_count(); 

  /* Dump final statistics. */
  dump_stat_workhorse (bctx->curl_handlers_count, 
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
      if (cctx[i].client_state == STATE_ERROR)
        {
          fprintf(stderr,"!!!Error!!! client %s failed\n", 
                  cctx[i].client_name/* client buffer of client */);
        }
    }

  return 0;
}

static int mget_url_smooth (batch_context* bctx)  		       
{
    CURLM *mhandle =  bctx->multiple_handle; 
    float max_timeout = bctx->uas_url_ctx_array[0].url_completion_time;
    const char *name = bctx->batch_name; 
 
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
        case -1: // select error
            break;
        case 0:
        default: // timeout or readable/writable sockets
            mperform_smooth (bctx, &still_running);            
	    break;
        }
    }	 
    return 0;
}

static int mperform_smooth (batch_context* bctx, int* still_running)
{
    CURLM *mhandle =  bctx->multiple_handle;     
    
    while(CURLM_CALL_MULTI_PERFORM == 
          curl_multi_perform (mhandle, still_running))
      ;

    unsigned long now = get_tick_count(); 

    if ((now - bctx->last_measure) > snapshot_timeout) 
      {
        // dump statistics.
        dump_stat_workhorse( 
                          bctx->curl_handlers_count, 
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
            /* Very bad style - curl bloody hacking. Some API is appropriate here. */
            struct SessionHandle *shandle = msg->easy_handle;
            client_context *cctx = (client_context *) shandle->set.debugdata;
          
            int client_state =  load_next_step (cctx);
            fprintf (stderr, "%s - after load_next_step client state %d.\n", __func__, client_state);

            if (client_state == STATE_ERROR || client_state == STATE_FINISHED_OK) 
              {
                bctx->curl_handlers_count--; 
              }
          
            if (msg_num <= 0)
              break;
          }
      }

    return 0;
}

static int load_next_step (client_context* cctx)
{
  //char* p_post_buff = NULL;
  batch_context* bctx = cctx->bctx;
  
  curl_multi_remove_handle (bctx->multiple_handle, 
                            bctx->client_handles_array[cctx->client_index]);

  fprintf (stderr, "%s - entered client_state:%d, url:%d.\n", 
           __func__, cctx->client_state, cctx->url_curr_index);

  if (cctx->client_state == STATE_ERROR)
    {
      return cctx->client_state;
    }
  
  if (! cctx->is_https)
    bctx->http_delta.requests += 1;
  else
    bctx->https_delta.requests += 1;
  
  if (cctx->client_state == STATE_LAST_URL)
    {
      return (cctx->client_state = STATE_FINISHED_OK);
    }
  
  /* Advance index of the current url */
  cctx->url_curr_index++;
  
  if (cctx->url_curr_index >= (size_t)(bctx->uas_urls_num - 1)) 
    {
      cctx->cycle_num++;
      
      fprintf (stderr, "%s -cycle_num:%ld cycles_num:%ld\n", 
               __func__, cctx->cycle_num, bctx->cycles_num);
      
      if (cctx->cycle_num >= bctx->cycles_num) 
        {
          if (bctx->do_logoff && cctx->post_data_logoff)
            single_handle_setup (cctx,
                                 URL_INDEX_LOGOFF_URL, /* index of the logoff url */
                                 0, /* the index is not relevant here */ 
                                 1 /* TODO: 1 - means POST, but it could be GET-POST sequence */
                                 );
              
          return (cctx->client_state = STATE_LAST_URL);
        }
      else
        {
          cctx->url_curr_index = 0; /* It was 1, Misha, why not 0 ? */
        }
    }

  fprintf (stderr, "%s - prior to shs  client_state:%d, url:%d.\n",
           __func__, cctx->client_state, cctx->url_curr_index);

  single_handle_setup (cctx, cctx->url_curr_index, -1, 0); /* Last 0 - means no POST-ing */
  
  return cctx->client_state;
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

