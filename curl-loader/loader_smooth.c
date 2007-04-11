/* 
 *     loader_smooth.c
 *
 * 2006 - 2007 Copyright (c)
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
 *
 * Cooked from the CURL-project examples with thanks to the 
 * great CURL-project authors and contributors.
 */

// must be first include
#include "fdsetsize.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"
#include "heap.h"

static timer_node logfile_timer_node; 
static timer_node clients_num_inc_timer_node;

static int mget_url_smooth (batch_context* bctx);
static int mperform_smooth (batch_context* bctx,
                            unsigned long* now_time,
                            int* still_running);

/****************************************************************************************
 * Function name - user_activity_smooth
 *
 * Description - Simulates user-activities, like login, uas, logoff, using SMOOTH-MODE
 * Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
 * Return Code/Output - On Success - 0, on Error -1
 ****************************************************************************************/
int user_activity_smooth (client_context* cctx_array)
{
  batch_context* bctx = cctx_array->bctx;
  long logfile_timer_id = -1;
  long clients_num_inc_id = -1;

  if (!bctx)
    {
      fprintf (stderr, "%s - error: bctx is a NULL pointer.\n", __func__);
      return -1;
    }

  /* ======== Make the smooth-mode specific allocations and initializations =======*/

  if (! (bctx->waiting_queue = calloc (1, sizeof (heap))))
    {
      fprintf (stderr, "%s - error: failed to allocate queue.\n", __func__);
      return -1;
    }
  
  if (tq_init (bctx->waiting_queue,
               bctx->client_num,               /* tq size */
               10,                             /* tq increase step; 0 - means don't increase */
               bctx->client_num + PERIODIC_TIMERS_NUMBER + 1 /* number of nodes to prealloc */
               ) == -1)
    {
      fprintf (stderr, "%s - error: failed to initialize waiting queue.\n", __func__);
      return -1;
    }

  const unsigned long now_time = get_tick_count ();
  
  /* 
     Init logfile rewinding timer and schedule it.
  */
  const unsigned long logfile_timer_msec  = 1000*SMOOTH_MODE_LOGFILE_TEST_TIMER;

  logfile_timer_node.next_timer = now_time + logfile_timer_msec;
  logfile_timer_node.period = logfile_timer_msec;
  logfile_timer_node.func_timer = handle_logfile_rewinding_timer;

  if ((logfile_timer_id = tq_schedule_timer (bctx->waiting_queue, 
                                             &logfile_timer_node)) == -1)
    {
      fprintf (stderr, "%s - error: tq_schedule_timer () failed.\n", __func__);
      return -1;
    }
  

  bctx->start_time = bctx->last_measure = now_time;
  bctx->active_clients_count = 0;
  

  if (add_loading_clients (bctx) == -1)
    {
      fprintf (stderr, "%s error: add_loading_clients () failed.\n", __func__);
      return -1;
    }

  if (bctx->do_client_num_gradual_increase)
    {
      /* 
         Schedule the gradual loading clients increase timer 
      */
      
      clients_num_inc_timer_node.next_timer = now_time + 1000;
      clients_num_inc_timer_node.period = 1000;
      clients_num_inc_timer_node.func_timer = 
        handle_gradual_increase_clients_num_timer;

      if ((clients_num_inc_id = tq_schedule_timer (
                                                   bctx->waiting_queue, 
                                                   &clients_num_inc_timer_node)) == -1)
        {
          fprintf (stderr, "%s - error: tq_schedule_timer () failed.\n", __func__);
          return -1;
        }
    }

  dump_snapshot_interval (bctx, now_time);

  /* 
     ========= Run the loading machinery ================
  */
  while ((pending_active_and_waiting_clients_num (bctx)) ||
         bctx->do_client_num_gradual_increase)
    {
      if (mget_url_smooth (bctx) == -1)
        {
          fprintf (stderr, "%s error: mget_url () failed.\n", __func__) ;
          return -1;
        }
    }

  dump_final_statistics (cctx_array);

  /* 
     ======= Release resources =========================
  */
  if (bctx->waiting_queue)
    {
        /* Cancel periodic logfile timer */
      if (logfile_timer_id != -1)
        {
          tq_cancel_timer (bctx->waiting_queue, logfile_timer_id);
          tq_cancel_timer (bctx->waiting_queue, clients_num_inc_id);
        }

      tq_release (bctx->waiting_queue);
      free (bctx->waiting_queue);
      bctx->waiting_queue = 0;
    }

  return 0;
}



/****************************************************************************************
 * Function name - mget_url_smooth
 *
 * Description - Performs actual fetching of urls for a whole batch. Starts with initial fetch
 *               by mperform_smooth () and further acts using mperform_smooth () on select events
 *
 * Input -       *bctx - pointer to the batch of contexts
 *
 * Return Code/Output - On Success - 0, on Error -1
 ****************************************************************************************/
static int mget_url_smooth (batch_context* bctx)  		       
{
  int max_timeout_msec = 1000;
    //    DEFAULT_SMOOTH_URL_COMPLETION_TIME*1000;
  unsigned long now_time = get_tick_count ();
  int cycle_counter = 0;
    
  //if (bctx->uas_url_ctx_array)
  //  {
  //   max_timeout_msec = bctx->uas_url_ctx_array[0].url_completion_time*1000;
  // }
 
  int still_running = 0;
  struct timeval timeout;

  mperform_smooth (bctx, &now_time, &still_running);

  while (max_timeout_msec > 0) 
    {
      int rc, maxfd;
      fd_set fdread, fdwrite, fdexcep;

      FD_ZERO(&fdread); FD_ZERO(&fdwrite); FD_ZERO(&fdexcep);
      timeout.tv_sec = 0;
      timeout.tv_usec = 250000;

      max_timeout_msec -= timeout.tv_sec*1000 + timeout.tv_usec * 0.001;

      curl_multi_fdset(bctx->multiple_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

      //fprintf (stderr, "%s - Waiting for %d clients with seconds %f.\n", 
      //name, still_running, max_timeout);

      rc = select (maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

      switch(rc)
        {
        case -1: /* select error */
          break;

        case 0: /* timeout */
          now_time = get_tick_count ();
        default: /* timeout or readable/writable sockets */
          mperform_smooth (bctx, &now_time, &still_running);            
          break;
        }
          
      if (! (++cycle_counter % TIME_RECALCULATION_CYCLES_NUM))
        {
          now_time = get_tick_count ();
          dispatch_expired_timers (bctx, now_time);
        }

      if (!rc)
        {
          dispatch_expired_timers (bctx, now_time);
        }
    } 
  return 0;
}

/****************************************************************************************
 * Function name - mperform_smooth
 *
 * Description - Uses curl_multi_perform () for initial url-fetch and to react on socket events.
 *               Uses curl_multi_info_read () to test url-fetch completion events and to proceed
 *               with the next step for the client, using load_next_step (). Cares about statistics
 *               at certain timeouts.
 *
 * Input -       *bctx - pointer to the batch of contexts;
 *               *still_running - pointer to counter of still running clients (CURL handles)
 *               
 * Return Code/Output - On Success - 0, on Error -1
 ****************************************************************************************/
static int mperform_smooth (batch_context* bctx, 
                            unsigned long* now_time, 
                            int* still_running)
{
  CURLM *mhandle =  bctx->multiple_handle;
  int cycle_counter = 0;	
  int msg_num = 0;
  const int snapshot_timeout = snapshot_statistics_timeout*1000;
  CURLMsg *msg;
  int sched_now = 0; 
    
  while (CURLM_CALL_MULTI_PERFORM == 
        curl_multi_perform(mhandle, still_running))
    ;

  if ((long)(*now_time - bctx->last_measure) > snapshot_timeout) 
    {
      dump_snapshot_interval (bctx, *now_time);
    }

  while( (msg = curl_multi_info_read (mhandle, &msg_num)) != 0)
    {
      if (msg->msg == CURLMSG_DONE)
        {
          /* TODO: CURLMsg returns 'result' field as curl return code. We may wish to use it. */

          CURL *handle = msg->easy_handle;
          client_context *cctx = NULL;

          curl_easy_getinfo (handle, CURLINFO_PRIVATE, &cctx);

          if (!cctx)
            {
              fprintf (stderr, "%s - error: cctx is a NULL pointer.\n", __func__);
              return -1;
            }

          if (msg->data.result)
            {
              // fprintf (stderr, "res is %d ", msg->data.result);
              cctx->client_state = CSTATE_ERROR;
                
              // fprintf(cctx->file_output, "%ld %s !! ERROR: %d - %s\n", cctx->cycle_num, 
              // cctx->client_name, msg->data.result, curl_easy_strerror(msg->data.result ));
            }

          if (! (++cycle_counter % TIME_RECALCULATION_MSG_NUM))
            {
              *now_time = get_tick_count ();
            }

          /*cstate client_state =  */
          load_next_step (cctx, *now_time, &sched_now);

          //fprintf (stderr, "%s - after load_next_step client state %d.\n", __func__, client_state);

          if (msg_num <= 0)
            {
              break;  /* If no messages left in the queue - go out */
            }

          cycle_counter++;
        }
    }

  return 0;
}

