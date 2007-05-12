/* 
 *     loader_fsm.c
 *
 * 2007 Copyright (c) 
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
  */

// must be the first include
#include "fdsetsize.h"

#include <stdlib.h>

#include "loader.h"
#include "client.h"
#include "batch.h"
#include "conf.h"
#include "heap.h"
#include "screen.h"
#include "cl_alloc.h"

static timer_node logfile_timer_node;
static timer_node clients_num_inc_timer_node;
static timer_node screen_input_timer_node;

static long logfile_timer_id = -1;
static long clients_num_inc_id = -1;
static long screen_input_timer_id = -1;

static int load_error_state (client_context* cctx, unsigned long *wait_msec);
static int load_init_state (client_context* cctx, unsigned long *wait_msec);
static int load_urls_state (client_context* cctx, unsigned long *wait_msec);
static int load_final_ok_state (client_context* cctx, unsigned long *wait_msec);

/* 
   Table of loading functions in order to call an appropiate for 
   a certain state loading function just by using the state number
   as an index. As we are starting our states from (-1),
   the actual call will be with (state + 1) as an index, used 
   in load_next_step ().
*/
const load_state_func load_state_func_table [] =
  {
    load_error_state,
    load_init_state,
    load_urls_state,
    load_final_ok_state,
  };

static int pick_up_next_url (client_context* cctx);
static int fetching_first_cycling_url (client_context* cctx);
static void advance_cycle_num (client_context* cctx);
static int setup_url (client_context* cctx);


/*****************************************************************************
 * Function name - alloc_init_timer_waiting_queue
 *
 * Description - Allocates and initializes timer waiting queue
 *
 *Input               size -  maximum possible size of the queue
 *Input/Output   **wq - waiting queue to be allocated, initialized and returned back
 *
 * Return Code/Output - On success -0, on error -1
 ******************************************************************************/
int alloc_init_timer_waiting_queue (size_t size, timer_queue** wq)
{
  timer_queue* tq = NULL;

  *wq = NULL;

  if (! (tq = cl_calloc (1, sizeof (heap))))
    {
      fprintf (stderr, "%s - error: failed to allocate queue.\n", __func__);
      return -1;
    }
  
  if (tq_init (tq,
               size,               /* tq size */
               10,                             /* tq increase step; 0 - means don't increase */
               size /* number of nodes to prealloc */
               ) == -1)
    {
      fprintf (stderr, "%s - error: failed to initialize waiting queue.\n", __func__);
      free (tq);
      return -1;
    }

  *wq = tq;

  return 0;
}

/*****************************************************************************
 * Function name - init_timers_and_add_initial_clients_to_load
 *
 * Description - Really inits timers and adds initial clients to load
 *
 *Input               *bctx - pointer to a batch context
 *                   now-time  - current timestamp
 *
 * Return Code/Output - On success -0, on error -1
 ******************************************************************************/
int init_timers_and_add_initial_clients_to_load (batch_context* bctx,
                                                 unsigned long now_time)
{
    /* 
     Init logfile rewinding timer and schedule it.
  */
  const unsigned long logfile_timer_msec  = 
    1000*LOGFILE_TEST_TIMER_PERIOD;

  logfile_timer_node.next_timer = now_time + logfile_timer_msec;
  logfile_timer_node.period = logfile_timer_msec;
  logfile_timer_node.func_timer = handle_logfile_rewinding_timer;

  if ((logfile_timer_id = tq_schedule_timer (bctx->waiting_queue, 
                                             &logfile_timer_node)) == -1)
    {
      fprintf (stderr, "%s - error: tq_schedule_timer () failed.\n", __func__);
      return -1;
    }

  /* 
     Init screen input testing timer and schedule it.
  */
  screen_input_timer_node.next_timer = now_time + 3000;
  screen_input_timer_node.period = 1000;
  screen_input_timer_node.func_timer = handle_screen_input_timer;

  if ((screen_input_timer_id = tq_schedule_timer (bctx->waiting_queue, 
                                             &screen_input_timer_node)) == -1)
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
  
  return 0;
}

/**************************************************************************
 * Function name - cancel_periodic_timers
 *
 * Description - Cancels scheduled periodic timers
 *
 *Input               *twq - pointer to timer waiting queue
 * Return Code/Output - On success -0, on error -1
 ***************************************************************************/
int cancel_periodic_timers (timer_queue* twq)
{
  if (logfile_timer_id != -1)
    {
      tq_cancel_timer (twq, logfile_timer_id);
      logfile_timer_id = -1;
    }

  if (clients_num_inc_id != -1)
    {
      tq_cancel_timer (twq, clients_num_inc_id);
      clients_num_inc_id = -1;
    }

  if (screen_input_timer_id != -1)
    {
      tq_cancel_timer (twq, screen_input_timer_id);
      screen_input_timer_id = -1;
    }

  return 0;
}


/****************************************************************************************
 * Function name - load_next_step
 *
 * Description - Called at initialization and further after url-fetch completion 
 *               indication (that may be an error status as well). Either sets 
 *               to client the next url to load, or marks the being at completion state: 
 *               CSTATE_ERROR or CSTATE_FINISHED_OK.
 *
 * Input -       *cctx - pointer to the client context
 *               now_time -  current timestamp in msec
 *Input/Output   sched_now - when true, the client is scheduled right now without timer queue.
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
int load_next_step (client_context* cctx,
                    unsigned long now_time,
                    int* sched_now)
{
  batch_context* bctx = cctx->bctx;
  int rval_load = CSTATE_ERROR;
  unsigned long interleave_waiting_time = 0;

  *sched_now = 0;
	
  /* Remove handle from the multiple handle, if it was added there before. */
  if (cctx->client_state != CSTATE_INIT)
    {
      if (client_remove_from_load (bctx, cctx) == -1)
        {
          fprintf (stderr, "%s - client_remove_from_load () failed.\n", __func__);
          return -1;
        }
    }
 
  /* 
     When load_error_state () gets client (in CSTATE_ERROR) and 
     <recoverable_error_state> is true (the default), it recovers the 
     client and sets the first cycling state to it. However, operational
     statistics should record it as a failed operation in op_stat_update.
     Therefore, remembering here possible error state.
  */
  int recoverable_error_state = cctx->client_state;

  /* 
     Initialize virtual client's CURL handle for the next step of loading by calling
     load_<state-name>_state() function relevant for a particular client state.
  */
  rval_load = load_state_func_table[cctx->client_state+1](cctx, &interleave_waiting_time);


  /* Update operational statistics */
  op_stat_update (
                  &bctx->op_delta, 
                  (recoverable_error_state == CSTATE_ERROR) ? recoverable_error_state : rval_load, 
                  cctx->preload_state,
                  cctx->url_curr_index,
                  cctx->preload_url_curr_index);

  if (fetching_first_cycling_url (cctx))
    {
      /* Update CAPS numbers */
      op_stat_call_init_count_inc (&bctx->op_delta);
    }

  /*
     Coming to the error or the finished states, just return without more 
     scheduling the client any more.
  */
  if (rval_load == CSTATE_ERROR || rval_load == CSTATE_FINISHED_OK)
    {
      return rval_load;
    }

  /* 
     Schedule virtual clients by adding them to multi-handle, 
     if the clients are not in error or finished final states.
  */
  if (!interleave_waiting_time)
    {
      /* Schedule the client immediately */
      if (client_add_to_load (bctx, cctx) == -1)
        {
          fprintf (stderr, "%s - error: client_add_to_load () failed .\n", __func__);
          return -1;
        }
      else
        {
            *sched_now = 1;
        }
    }
  else
    {
        //PRINTF("load_next_step: ctx %p schedule next load in %d seconds\n", 
        //     cctx,(int) interleave_waiting_time/1000);
      /* 
         Postpone client scheduling for the interleave_waiting_time msec by 
         placing it to the timer queue. 
      */
      cctx->tn.next_timer = now_time + interleave_waiting_time;
		
      if (tq_schedule_timer (bctx->waiting_queue, (struct timer_node *) cctx) == -1)
        {
          fprintf (stderr, "%s - error: tq_schedule_timer () failed.\n", __func__);
          return -1;
        }

      //fprintf (stderr, "%s - scheduled client to wq with wtime %ld\n", 
      //				 __func__, interleave_waiting_time);
    }

  return rval_load;
}

/****************************************************************************************
 * Function name - add_loading_clients
 *
 * Description - Initialization of our virtual clients (CURL handles)
 *               setting first url to fetch and scheduling them according to 
 *               clients increment for gradual loading.
 *
 * Input -       *bctx - pointer to the batch of contexts
 * Return Code/Output - On Success - 0, on error or request to unreg timer - (-1)
 ****************************************************************************************/
int add_loading_clients (batch_context* bctx)
{
  long clients_to_sched = 0;
  int scheduled_now = 0;
  
  /* 
     Return, if initial gradual scheduling of all new clients has been stopped
  */
  if (bctx->stop_client_num_gradual_increase)
    {
      return 0; // Returning 0 means do not stop the timer
    }
  
  /* 
     Return, if initial gradual scheduling of all new clients has been accomplished. 
  */
  if (bctx->client_num_max <= bctx->clients_current_sched_num)
    {
      bctx->do_client_num_gradual_increase = 0;
      return -1; // Returning (-1) means - stop the timer
      }
  
  /* Calculate number of the new clients to schedule. */
  if (!bctx->clients_current_sched_num && bctx->client_num_start)
    {
      /* first time scheduling - zero bctx->clients_current_sched_num */
      clients_to_sched = bctx->client_num_start;
    }
  else 
    {
      clients_to_sched = bctx->clients_initial_inc ?
        min (bctx->clients_initial_inc, bctx->client_num_max - bctx->clients_current_sched_num) :
        bctx->client_num_max;
    }


  //fprintf (stderr, "%s - adding %ld clients.\n", __func__, clients_to_sched);

  /* 
     Schedule new clients by initializing their CURL handle with
     URL, etc. parameters and adding it to MCURL multi-handle.
  */
  long j;
  for (j = bctx->clients_current_sched_num; 
       j < bctx->clients_current_sched_num + clients_to_sched; 
       j++)
	{
      /* 
         Runs load_init_state () for each newly added client. 
      */
      if (load_next_step (&bctx->cctx_array[j], 
                          bctx->start_time,
                          &scheduled_now) == -1)
        {  
          fprintf(stderr,"%s error: load_next_step() initial failed\n", __func__);
          return -1;
        }
    }

  /* 
     Re-calculate assisting counters and enable do_client_num_gradual_increase 
     flag, if required.
  */

  bctx->clients_current_sched_num += clients_to_sched;

  if (bctx->clients_initial_inc)
    {
      if (bctx->clients_current_sched_num < bctx->client_num_max)
        {
          bctx->do_client_num_gradual_increase = 1;
        }
    }
	
  return 0;
}

/****************************************************************************************
 * Function name - add_loading_clients_num
 *
 * Description - Adding a number of clients to load
 *
 * Input -       *bctx - pointer to the batch of contexts
 *                  add_number - number of clients to add to load
 * Return Code/Output - On Success - 0, on error  (-1)
 ****************************************************************************************/
int add_loading_clients_num (batch_context* bctx, int add_number)
{
  int scheduled_now = 0;

  if (add_number <= 0)
    {
      return -1;
    }
  
  if (bctx->client_num_max <= bctx->clients_current_sched_num)
    {
      return -1; // No room to add more
    }
  
  /* Calculate number of the new clients to schedule. */
  const long clients_to_sched = min (add_number, 
                                  bctx->client_num_max - bctx->clients_current_sched_num); 

  //fprintf (stderr, "%s - adding %ld clients.\n", __func__, clients_to_sched);

  /* 
     Schedule new clients by initializing thier CURL handle with
     URL, etc. parameters and adding it to MCURL multi-handle.
  */
  long j;
  for (j = bctx->clients_current_sched_num; 
       j < bctx->clients_current_sched_num + clients_to_sched; 
       j++)
	{
      /* Runs load_init_state () for each newly added client. */
      if (load_next_step (&bctx->cctx_array[j], 
                          bctx->start_time,
                          &scheduled_now) == -1)
        {  
          fprintf(stderr,"%s error: load_next_step() initial failed\n", __func__);
          return -1;
      }
    }
  
  bctx->clients_current_sched_num += clients_to_sched;
	
  return 0;
}


/****************************************************************************************
 * Function name - dispatch_expired_timers
 *
 * Description - Fetches from the waiting timer queue timers and dispatches them
 *               by calling timer-node specific handle_timeout () method. Among other expired timers
 *               dispatches waiting clients (kept in timer-queue to respect url interleave timeouts),
 *               where func_timer () function of client timer-node adds the clients to our 
 *               loading machinery.
 *
 * Input -       *bctx - pointer to the batch of contexts;
 *               now_time -  current time passed in msec
 *
 * Return Code/Output - On Success - 0 or positive number eq to the num of scheduled timers, 
 *                                   on Error -1
 ****************************************************************************************/
int
dispatch_expired_timers (batch_context* bctx, unsigned long now_time)
{
  timer_queue* tq = bctx->waiting_queue;
  int count =0;

  if (!tq)
    return -1;

  if (tq_empty (tq))
    return 0;

  while (! tq_empty (tq))
    {
      unsigned long time_nearest = tq_time_to_nearest_timer (tq);

      if (time_nearest <= now_time)
        {
          if (tq_dispatch_nearest_timer (tq, bctx, now_time) == -1)
            {
              // fprintf (stderr, "%s - error: tq_dispatch_nearest_timer () failed "
              // "or handle_timer () returns (-1).\n", __func__);
              return -1;
            }
          else
          {
              count++;
          }
        }
      else
        break;
    }

  return count;
}

/****************************************************************************************
 * Function name - client_add_to_load
 *
 * Description - Adds client context to the batch context multiple handle for loading
 *
 * Input -       *bctx - pointer to the batch context
 *               *cctx - pointer to the client context
 *
 * Return Code/Output - On success -0, on error - (-1)
 ****************************************************************************************/
int client_add_to_load (batch_context* bctx, client_context* cctx)
{
  /* Remember the previous state and url index: fur operational statistics */
  cctx->preload_state = cctx->client_state;
  cctx->preload_url_curr_index = cctx->url_curr_index;

  /* Schedule the client immediately */
  if (curl_multi_add_handle (bctx->multiple_handle, cctx->handle) ==  CURLM_OK)
    {
      bctx->active_clients_count++;
      //fprintf (stderr, "%s - client added.\n", __func__);
    }
  else
    {
      fprintf (stderr, "%s - curl_multi_add_handle () failed.\n", __func__);
      return -1;
    }

  return 0;
}

/****************************************************************************************
 * Function name - client_remove_from_load
 *
 * Description - Removes client context to from the kept in batch context multiple handle,
 * 		 thus, removing the client from the loading machinery
 *
 * Input -       *bctx - pointer to the batch context
 *               *cctx - pointer to the client context
 *
 * Return Code/Output - On success -0, on error - (-1)
 ****************************************************************************************/
int client_remove_from_load (batch_context* bctx, client_context* cctx)
{
  if (curl_multi_remove_handle (bctx->multiple_handle, cctx->handle) == CURLM_OK)
    {
      if (bctx->active_clients_count > 0)
        {
          bctx->active_clients_count--;
        }
      //fprintf (stderr, "%s - client removed.\n", __func__);
    }
  else
    {
      fprintf (stderr, "%s - curl_multi_remove_handle () failed.\n", __func__);
      return -1;
    }

  return 0;	
}


/****************************************************************************************
 * Function name - handle_gradual_increase_clients_num_timer
 *
 * Description - Handling of one second timer to increase gradually number of 
 *               loading clients.
 *
 * Input -       *timer_node - pointer to timer_node structure
 *               *pvoid_param - pointer to some extra data; here batch context
 *               *ulong_param - some extra data.
 *
 * Return Code/Output - On success -0, on error - (-1)
 ****************************************************************************************/
int handle_gradual_increase_clients_num_timer  (timer_node* timer_node, 
                                                void* pvoid_param, 
                                                unsigned long ulong_param)
{
  batch_context* bctx = (batch_context *) pvoid_param;
  (void) timer_node;
  (void) ulong_param;

  if (add_loading_clients (bctx) == -1)
    {
      //fprintf (stderr, "%s add_loading_clients () returns -1.\n", __func__);
      return -1;
    }

  //fprintf (stderr, "%s - runs.\n", __func__);
  return 0;
}

/****************************************************************************************
 * Function name - handle_logfile_rewinding_timer
 *
 * Description -   Handling of logfile controlling periodic timer
 *
 * Input -        *timer_node - pointer to timer node structure
 *                *pvoid_param - pointer to some extra data; here batch context
 *                *ulong_param - some extra data.
 *
 * Return Code/Output - On success -0, on error - (-1)
 ****************************************************************************************/
int handle_logfile_rewinding_timer  (timer_node* timer_node, 
                                     void* pvoid_param, 
                                     unsigned long ulong_param)
{
  batch_context* bctx = (batch_context *) pvoid_param;
  (void) timer_node;
  (void) ulong_param;

  if (rewind_logfile_above_maxsize (bctx->cctx_array->file_output) == -1)
    {
      fprintf (stderr, "%s - rewind_logfile_above_maxsize() failed .\n", __func__);
      return -1;
    }
  
  //fprintf (stderr, "%s - runs.\n", __func__);
  return 0;
}

/****************************************************************************************
 * Function name - handle_screen_input_timer
 *
 * Description -   Handling of screen imput
 *
 * Input -        *timer_node - pointer to timer node structure
 *                *pvoid_param - pointer to some extra data; here batch context
 *                *ulong_param - some extra data.
 *
 * Return Code/Output - On success -0, on error - (-1)
 ****************************************************************************************/
int handle_screen_input_timer  (timer_node* timer_node, 
                                     void* pvoid_param, 
                                     unsigned long ulong_param)
{
  batch_context* bctx = (batch_context *) pvoid_param;
  (void) timer_node;
  (void) ulong_param;

  screen_test_keyboard_input (bctx);

  //fprintf (stderr, "%s - runs.\n", __func__);
  return 0;
}

/****************************************************************************************
 * Function name - handle_cctx_timer
 *
 * Description - Handling of timer for a client waiting in the waiting queue to 
 *               respect url interleave timeout. Schedules the client to perform 
 *               the next loading operation.
 *
 * Input -       *timer_node - pointer to timer node structure
 *               *pvoid_param - pointer to some extra data; here batch context
 *               *ulong_param - some extra data.
 *
 * Return Code/Output - On success -0, on error - (-1)
 ****************************************************************************************/
int handle_cctx_timer (timer_node* timer_node, 
                       void* pvoid_param,
                       unsigned long ulong_param)
{
  client_context* cctx = (client_context *) timer_node;
  batch_context* bctx = cctx->bctx;
  (void)pvoid_param;
  (void)ulong_param;

  return client_add_to_load (bctx, cctx);
}

/****************************************************************************************
 * Function name - pending_active_and_waiting_clients_num
 *
 * Description - Returns the sum of active and waiting (for load scheduling) clients
 *
 * Input -       *bctx - pointer to the batch context
 *
 * Return Code/Output - Sum of active and waiting (for load scheduling) clients
 ****************************************************************************************/
int pending_active_and_waiting_clients_num (batch_context* bctx)
{
  return bctx->waiting_queue ? 
    (bctx->active_clients_count + tq_size (bctx->waiting_queue) - 
     PERIODIC_TIMERS_NUMBER - bctx->do_client_num_gradual_increase) :
    bctx->active_clients_count;
}


/*================= STATIC FUNCTIONS =================== */

static int fetching_first_cycling_url (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  return (int)cctx->url_curr_index == bctx->first_cycling_url;  
}

/*******************************************************************************
 * Function name - advance_cycle_num
 *
 * Description - Advances number of cycles, when the full cycle is done with all url-fetches
 * Input -       *cctx - pointer to the client context
 *
 * Return Code/Output - None
 ********************************************************************************/
static void advance_cycle_num (client_context* cctx)
{
      cctx->cycle_num++;
}

/*****************************************************************************
 * Function name - setup_url
 *
 * Description -   Sets URL-state url
 * Input -         *cctx - pointer to the client context
 *
 * Return Code/Output - CSTATE enumeration with client state
 ******************************************************************************/
static int setup_url (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;
  url_context* url = &bctx->url_ctx_array[cctx->url_curr_index];

  if (! url->url_use_current)
  {
      /* 
         Setup a new url. Internally it initializes client POST-ing buffer,
         when url req_type is POST.
       */
      if (setup_curl_handle_init (cctx, url) == -1)
      {
          fprintf(stderr,"%s error: setup_curl_handle_init - failed\n", 
                  __func__);
          return -1;
      }
  }
  else
    {
      /*
        We should preserve the url kept in CURL handle after GET,
        which may be the result of redirection/s,  but switch to POST 
        request method using client-specific POST login/logoff fields. 
        
        Initializes client POST-ing buffer and adds POSTFIELDS to the 
        CURL handle. Note, that it should be done on CURL handle 
        outside (removed) from MCURL handle. Add it back afterwords.
      */
      if (set_client_url_post_data (cctx, url) == -1)
        {
          fprintf(stderr,"%s error: set_client_url_post_data() - failed\n", 
                  __func__);
          return -1;
        }
    }
  return cctx->client_state = CSTATE_URLS;
}

/*****************************************************************************
 * Function name - load_init_state
 *
 * Description - Called by load_next_step () for setting up of the very first url to fetch
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 *******************************************************************************/
static int load_init_state (client_context* cctx, unsigned long *wait_msec)
{
  *wait_msec = 0;

  return load_urls_state (cctx, wait_msec);
}

/*******************************************************************************
 * Function name - load_error_state
 *
 * Description - Called by load_next_step () for the client in CSTATE_ERROR. 
 *               If the global flag <error_recovery_client> is not false, re-schedules 
 *               the client for next cycle of loading.
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ********************************************************************************/
static int load_error_state (client_context* cctx, unsigned long *wait_msec)
{
  batch_context* bctx = cctx->bctx;

  if (error_recovery_client)
    {
      advance_cycle_num (cctx);
		
      if (cctx->cycle_num >= bctx->cycles_num)
        {
          return (cctx->client_state = CSTATE_ERROR);
        }
      else
        {
            /* Load the first active url - start from the very beginning. */
          return load_state_func_table[CSTATE_URLS](cctx, wait_msec);
        }
    }
 
  /* Thus, the client will not be scheduled for load any more. */
  return (cctx->client_state = CSTATE_ERROR);
}

static int pick_up_next_url (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  if (bctx->cycling_completed)
    {
      if (cctx->url_curr_index == (size_t)(bctx->urls_num - 1))
        {
          return -1; // finita la-comedia
        }
      // take the next url
      return ++cctx->url_curr_index;
    }

  const int fc_url = bctx->first_cycling_url;
  const int lc_url = bctx->last_cycling_url;
  
  if (! bctx->cycling_completed && ((int)cctx->url_curr_index < lc_url))
    {
      return ++cctx->url_curr_index;
    }
  
  /*
    At the last cycling url.
  */
  if (! bctx->cycling_completed && cctx->url_curr_index == (size_t)lc_url)
    {
      // Finished with all the urls for a single cycle
      advance_cycle_num (cctx);
          
      if (cctx->cycle_num == bctx->cycles_num)
        {
          bctx->cycling_completed = 1; // Cycling completed

          // If there are non-cycling urls to fetch - continue
          if (cctx->url_curr_index == (size_t)(bctx->urls_num - 1))
            {
              return -1; // finita la-comedia
            }
          // take the next url
          return ++cctx->url_curr_index;
        }
      else
        {
          // If there are more cycles to do continue cycling
          return cctx->url_curr_index = fc_url;
        }
    }
  return -1;
}

static int load_urls_state (client_context* cctx, unsigned long *wait_msec)
{ 
  batch_context* bctx = cctx->bctx;
  int url_next = -1;
  
  if (cctx->client_state == CSTATE_URLS)
    {
      // Mind the after url sleeping timeout, if any.
      *wait_msec = bctx->url_ctx_array[cctx->url_curr_index].timer_after_url_sleep;
      
      // Now, pick-up the next url index
      if ((url_next = pick_up_next_url (cctx)) < 0)
        {
          return (cctx->client_state = CSTATE_FINISHED_OK);
        }
      
      // Set the new url index
      cctx->url_curr_index =  (size_t) url_next;
      
      // Setup the new url.
      setup_url (cctx);

      // Remain in URL state
      return cctx->client_state;
    }
  else
    {
      // Coming from any other states, start from the first url.
      cctx->url_curr_index = 0;
    }

  // Non-URL states are all falling below
  return setup_url (cctx);
}

static int load_final_ok_state (client_context* cctx, unsigned long *wait_msec)
{
  (void) cctx; (void) wait_msec;

  return CSTATE_FINISHED_OK;
}
