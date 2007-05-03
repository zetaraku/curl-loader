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
static int load_login_state (client_context* cctx, unsigned long *wait_msec);
static int load_uas_state (client_context* cctx, unsigned long *wait_msec);
static int load_logoff_state (client_context* cctx, unsigned long *wait_msec);
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
    load_login_state,
    load_uas_state,
    load_logoff_state,
    load_final_ok_state,
  };

static int is_last_cycling_state (client_context* cctx);
static int is_first_cycling_state (client_context* cctx);
static int last_cycling_state (batch_context* bctx);
static int first_cycling_state (batch_context* bctx);

static void advance_cycle_num (client_context* cctx);
static int on_cycling_completed (client_context* cctx, unsigned long *wait_msec);

static int setup_login_logoff (client_context* cctx, const int login);
static int setup_uas (client_context* cctx);

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
                  cctx->uas_url_curr_index,
                  cctx->preload_uas_url_curr_index);

  if (is_first_cycling_state (cctx))
    {
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
  if (bctx->client_num <= bctx->clients_current_sched_num)
    {
      bctx->do_client_num_gradual_increase = 0;
      return -1; // Returning -1 means - stop the timer
      }
  
  /* Calculate number of the new clients to schedule. */
  if (!bctx->clients_current_sched_num && bctx->client_num_start)
    {
      /* first time scheduling */
      clients_to_sched = bctx->client_num_start;
    }
  else 
    {
      clients_to_sched = bctx->clients_initial_inc ?
        min (bctx->clients_initial_inc, bctx->client_num - bctx->clients_current_sched_num) :
        bctx->client_num;
    }


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

  /* 
     Re-calculate assisting counters and enable do_client_num_gradual_increase 
     flag, if required.
  */

  bctx->clients_current_sched_num += clients_to_sched;

  if (bctx->clients_initial_inc)
    {
      if (bctx->clients_current_sched_num < bctx->client_num)
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
  
  if (bctx->client_num <= bctx->clients_current_sched_num)
    {
      return -1; // No room to add more
    }
  
  /* Calculate number of the new clients to schedule. */
  const long clients_to_sched = min (add_number, 
                                  bctx->client_num - bctx->clients_current_sched_num); 

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
 *               where func_timer () function of client timer-node adds the clients to loading machinery.
 *
 * Input -       *bctx - pointer to the batch of contexts;
 *               now_time -  current time passed in msec
 *
 * Return Code/Output - On Success - 0 or positive number eq to the num of scheduled timers, on Error -1
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
              //fprintf (stderr, 
              //         "%s - error: tq_dispatch_nearest_timer () failed or handle_timer () returns (-1).\n", 
              //         __func__);
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
  /* Remember the previous state and UAS index: fur operational statistics */
  cctx->preload_state = cctx->client_state;
  cctx->preload_uas_url_curr_index = cctx->uas_url_curr_index;

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

/****************************************************************************************
 * Function name - is_last_cycling_state
 *
 * Description -   Figures out, whether the current state of client is the last cycling state.
 *                 Only in the last cycling state the number of cycles is advanced.
 *
 * Input -         *cctx - pointer to the client context
 *
 * Return Code/Output - true, when the last cycling state, else - false
 ****************************************************************************************/
static int is_last_cycling_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  int last_cstate = last_cycling_state (bctx);

  if (last_cstate == CSTATE_ERROR || last_cstate == CSTATE_INIT)
    return 0;

  return (cctx->client_state == last_cstate);
}

static int is_first_cycling_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;
  int first_cstate = first_cycling_state (bctx);

  if (first_cstate == CSTATE_ERROR || first_cstate == CSTATE_INIT)
    return 0;

  return (cctx->client_state == first_cstate);
}

static int last_cycling_state (batch_context* bctx)
{
  /*
  if (bctx->do_logoff && bctx->logoff_cycling)
    {
      return CSTATE_LOGOFF;
    }
  else if (bctx->do_uas)
    {
      return CSTATE_UAS_CYCLING;
    }
  else if (bctx->do_login && bctx->login_cycling)
    {
      return CSTATE_LOGIN;
    }
  */

  return CSTATE_ERROR;
}

static int first_cycling_state (batch_context* bctx)
{
  /*
  if (bctx->do_login && bctx->login_cycling)
    {
      return CSTATE_LOGIN;
    }
  else if (bctx->do_uas)
    {
      return CSTATE_UAS_CYCLING;
    }
  else if (bctx->do_logoff && bctx->logoff_cycling)
    {
      return CSTATE_LOGOFF;
    }
  */

  return CSTATE_ERROR;
}

/****************************************************************************************
 * Function name - advance_cycle_num
 *
 * Description - Advances number of cycles, when the full cycle is done with all url-fetches
 * Input -       *cctx - pointer to the client context
 *
 * Return Code/Output - None
 ****************************************************************************************/
static void advance_cycle_num (client_context* cctx)
{
  cctx->cycle_num++;
}

/****************************************************************************************
 * Function name - on_cycling_completed
 *
 * Description - Either goes to the logoff state (logoff-no-cycling) of to CSTATE_FINISHED_OK
 * Input -       *cctx - pointer to the client context
 *
 * Return Code/Output - CSTATE enumeration with client state
 ****************************************************************************************/
static int on_cycling_completed (client_context* cctx, unsigned long *wait_msec)
{
  batch_context* bctx = cctx->bctx;

  /* 
     Go to the finish-line. 
  */
  return (cctx->client_state = CSTATE_FINISHED_OK);
}

/****************************************************************************************
 * Function name - setup_login_logoff
 *
 * Description - Sets up login or logoff url, depending on flag <login>
 * Input -       *cctx - pointer to the client context
 *               login - when true - login state, when false logoff state is set
 *
 * Return Code/Output - CSTATE enumeration with client state
 ****************************************************************************************/
static int setup_login_logoff (client_context* cctx, const int login)
{
  batch_context* bctx = cctx->bctx;
  int posting_after_get = 0;

  /*
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
  */
  
  if (!posting_after_get)
    {
      /* 
         Three possible cases are treated here:
         - GET, which is by itself enough, e.g. for logoff using cookies;
         - GET, which will be later followed by later POST login/logoff;
         - POST, which is the standalone login/logoff, without any previous GET;
      */
      int post_standalone = 0;

      /*
      if ((login && bctx->login_req_type == LOGIN_REQ_TYPE_POST) ||
          (!login && bctx->logoff_req_type == LOGOFF_REQ_TYPE_POST))
        {
          post_standalone = 1;
        }
      

      if (setup_curl_handle_init (
                                  cctx,
                                  login ? &bctx->login_url : &bctx->logoff_url,
                                  0, // Not applicable for smooth mode
                                    post_standalone // If 'true' -POST, else GET
                                  ) == -1)
        {
          fprintf(stderr,"%s error: setup_curl_handle_init - failed\n", __func__);
          return -1;
        }
      */
    }
  else
    {
      /* 
         The only case here, is when doing POST after GET. 
         We should preserve the url kept in CURL handle after GET,
         which may be the result of redirection/s,  but switch to POST 
         request method using client-specific POST login/logoff fields. 
      */
      CURL* handle = cctx->handle;

      /* 
         Just add POSTFIELDS. Note, that it should be done on CURL handle 
         outside (removed) from MCURL handle. Add it back afterwords.
      */
      curl_easy_setopt (handle, CURLOPT_POSTFIELDS, cctx->post_data);
    }

  return cctx->client_state = login ? CSTATE_LOGIN : CSTATE_LOGOFF;
}

/****************************************************************************************
 * Function name - setup_uas
 *
 * Description -   Sets UAS state url
 * Input -         *cctx - pointer to the client context
 *
 * Return Code/Output - CSTATE enumeration with client state
 ****************************************************************************************/
static int setup_uas (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  if (setup_curl_handle_init (
                              cctx,
                              &bctx->uas_url_ctx_array[cctx->uas_url_curr_index], /* current url */
                              0, /* Cycle, do we need it? */ 
                              0 /* GET - zero, unless we'll need to make POST here */
                              ) == -1)
    {
      fprintf(stderr,"%s error: setup_curl_handle_init - failed\n", __func__);
      return -1;
    }
  
  return cctx->client_state = CSTATE_UAS_CYCLING;
}

/****************************************************************************************
 * Function name - load_init_state
 *
 * Description - Called by load_next_step () for setting up of the very first url to fetch
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
static int load_init_state (client_context* cctx, unsigned long *wait_msec)
{
  batch_context* bctx = cctx->bctx;

  *wait_msec = 0;

  /*
  if (bctx->do_login)
    {
      return load_login_state (cctx, wait_msec);
    }
  else if (bctx->do_uas) 
    {
      return load_uas_state (cctx, wait_msec);
    }
  else if (bctx->do_logoff) 
    {
      return load_logoff_state (cctx, wait_msec);
    }
*/
  return (cctx->client_state = CSTATE_ERROR);
}


/****************************************************************************************
 * Function name - load_error_state
 *
 * Description - Called by load_next_step () for the client in CSTATE_ERROR. If the global
 *               flag <error_recovery_client> is not false, re-schedules the client for 
 *               next cycle of loading.
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
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
          /* first cycling state */
          int first_cstate = first_cycling_state (bctx);

          if (first_cstate <= 0) /* if CSTATE_ERROR or CSTATE_INIT */
            return (cctx->client_state = CSTATE_ERROR);

          /* Load the first cycling state url */
          return load_state_func_table[first_cstate + 1](cctx, wait_msec);
        }
    }
 
  /* Thus, the client will not be scheduled for load any more. */
  return (cctx->client_state = CSTATE_ERROR);
}


/****************************************************************************************
 * Function name - load_login_state
 *
 * Description - Called by load_next_step () for the client in CSTATE_LOGIN 
 *               state to schedule the next loading url.
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
static int load_login_state (client_context* cctx, unsigned long *wait_msec)
{
  batch_context* bctx = cctx->bctx;

  /*
  //
    //Test for login state, if a single login operation has been accomplished. 
    //Sometimes, the operation contains two elements: GET and POST.
  //
  if (cctx->client_state == CSTATE_LOGIN)
    {
      if ((bctx->login_req_type != LOGIN_REQ_TYPE_GET_AND_POST) ||
          (bctx->login_req_type == LOGIN_REQ_TYPE_GET_AND_POST && !cctx->get_post_count))
        {
           
             //Indeed, accomplished a single login operation. 
          

          // Mind the interleave timeout after login
          *wait_msec = bctx->login_url.url_interleave_time;

          if (is_last_cycling_state (cctx))
            {
              
                //If we are login cycling and the last/only cycling state,
                //we are in charge for advancing cycles counter.
              
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  // Either jump to logoff or to finish_ok. 
                  return on_cycling_completed (cctx, wait_msec);
                }
              else
                {
                  
                     //Configured to cycle, but the state is the last cycling state 
                     //and the only cycling state, therefore, continue login. 
                  return setup_login_logoff (cctx, 1); // 1 - means login
                }
            }
 
          if (bctx->do_uas)
            return load_uas_state (cctx, wait_msec);
          else if (bctx->do_logoff)
            return load_logoff_state (cctx, wait_msec);
          else
            return (cctx->client_state = CSTATE_FINISHED_OK);
        }
    }
  */
  /* Non-LOGIN states are all falling below: */
  return setup_login_logoff (cctx, 1);
}

/****************************************************************************************
 * Function name - load_uas_state
 *
 * Description - Called by load_next_step () for the client in CSTATE_UAS state to 
 * 		 schedule the next loading url.
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
static int load_uas_state (client_context* cctx, unsigned long *wait_msec)
{ 
  batch_context* bctx = cctx->bctx;
  /*

  if (cctx->client_state == CSTATE_UAS_CYCLING)
    {
      // Mind the interleave timeout after each url, if any.
      *wait_msec = bctx->uas_url_ctx_array[cctx->uas_url_curr_index].url_interleave_time;

      // Now, advance the url index
      cctx->uas_url_curr_index++;

      if (cctx->uas_url_curr_index >= (size_t)(bctx->uas_urls_num))
        {
          // Finished with all the urls for a single UAS -cycle.

          cctx->uas_url_curr_index = 0;
 
          if (is_last_cycling_state (cctx))
            {
              // If UAS is the last cycling state, advance cycle counter. 
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  // Either logoff or finish_ok. 
                  return on_cycling_completed (cctx, wait_msec);
                }
              else
                {
                  // Continue cycling - take another cycle
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx, wait_msec);
                  else
                    return setup_uas (cctx);
                }
            }

             //We are not the last cycling state. A guess is, that the
             //next state is logoff with cycling.
          return load_logoff_state (cctx, wait_msec);
        }
    }
  else
    {
      cctx->uas_url_curr_index = 0;
    }

*/

  /* Non-UAS states are all falling below: */
  return setup_uas (cctx);
}

/****************************************************************************************
 * Function name - load_logoff_state
 *
 * Description - Called by load_next_step () for the client in CSTATE_LOGOFF state to 
 *               schedule the next loading url.
 *
 * Input -       *cctx - pointer to the client context
 *               *wait_msec - pointer to time to wait till next scheduling (interleave time).
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
static int load_logoff_state (client_context* cctx, unsigned long *wait_msec)
{
  batch_context* bctx = cctx->bctx;

  /*
    Test for logoff state, if a single login operation has been accomplished. 
    Sometimes, the operation contains two elements: GET and POST.
  */
  /*
  if (cctx->client_state == CSTATE_LOGOFF)
    {
      if ((bctx->logoff_req_type != LOGOFF_REQ_TYPE_GET_AND_POST) ||
          (bctx->logoff_req_type == LOGOFF_REQ_TYPE_GET_AND_POST && !cctx->get_post_count))
        {
          
             //Indeed, we have accomplished a single logoff operation. 
         

          //  Mind the interleave timeout after login
          *wait_msec = bctx->logoff_url.url_interleave_time;

          if (is_last_cycling_state (cctx))
            {
             
              // If logoff cycling ,we are in charge for advancing cycles counter.
              
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  return on_cycling_completed (cctx, wait_msec); // Goes to finish-ok 
                }
              else
                {
                  
                    //Continue cycling - take another cycle
                  
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx, wait_msec);
                  else if (bctx->do_uas)
                    return load_uas_state (cctx, wait_msec);
                  else // logoff is the only cycling state? Sounds strange, but allow it
                    return setup_login_logoff (cctx, 0); //0 - means logoff
                }
            }
 
          // If not doing logoff cycling, means single logoff done - go to finish-ok
          return (cctx->client_state = CSTATE_FINISHED_OK);
        }
    }
*/

  /* Non-LOGOFF states are all falling below: */
  return setup_login_logoff (cctx, 0);
}

static int load_final_ok_state (client_context* cctx, unsigned long *wait_msec)
{
  (void) cctx; (void) wait_msec;

  return CSTATE_FINISHED_OK;
}
