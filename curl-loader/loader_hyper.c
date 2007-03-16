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

// must be first include
#include "fdsetsize.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <event.h>
#include <string.h>

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"
#include "heap.h"

static timer_node logfile_timer_node; 
static timer_node clients_num_inc_timer_node;

#define DEFAULT_SMOOTH_URL_COMPLETION_TIME 6.0
#define TIME_RECALCULATION_MSG_NUM 100
#define PERIODIC_TIMERS_NUMBER 1
#define SMOOTH_MODE_LOGFILE_TEST_TIMER 10 /* once in 10 seconds */


int client_tracing_function (CURL *handle, 
                                    curl_infotype type, 
                                    unsigned char *data, 
                                    size_t size, 
                                    void *userp);
size_t do_nothing_write_func (void *ptr, 
                                     size_t size, 
                                     size_t nmemb, 
                                     void *stream);
int setup_curl_handle_appl (struct client_context*const cctx,  
                                   url_context* url_ctx,
                                   int post_method);

static int add_loading_clients (batch_context* bctx);
static int client_add_to_load (batch_context* bctx, client_context* cctx);
static int client_remove_from_load (batch_context* bctx, client_context* cctx);
static int handle_logfile_rewinding_timer (timer_node* timer_node, 
                                           void* pvoid_param, 
                                           unsigned long ulong_param);
static int handle_gradual_increase_clients_num_timer (timer_node* timer_node, 
                                                      void* pvoid_param, 
                                                      unsigned long ulong_param);

static int mget_url_smooth (batch_context* bctx);
static int mperform_smooth (batch_context* bctx, int* still_running);
static int dispatch_expired_timers (batch_context* bctx, unsigned long now_time);

static int load_next_step (client_context* cctx, unsigned long now_time);
static int last_cycling_state (batch_context* bctx);
static int first_cycling_state (batch_context* bctx);

/*
  Next step initialization functions relevant the client state.
*/
static int load_error_state (client_context* cctx, unsigned long *wait_msec);
static int load_init_state (client_context* cctx, unsigned long *wait_msec);
static int load_login_state (client_context* cctx, unsigned long *wait_msec);
static int load_uas_state (client_context* cctx, unsigned long *wait_msec);
static int load_logoff_state (client_context* cctx, unsigned long *wait_msec);
static int load_final_ok_state (client_context* cctx, unsigned long *wait_msec);
int handle_cctx_timer_hyper (timer_node* timer_node, 
                       void* pvoid_param,
                       unsigned long ulong_param);

int setup_curl_handle_init_hyper(client_context*const cctx,
                         url_context* url_ctx,
                         long cycle_number,
                         int post_method);

typedef int (*load_state_func)  (client_context* cctx, unsigned long *wait_msec);

/* 
   Table of loading functions in order to call an appropiate for 
   a certain state loading function just by using the state number
   as an index. As we are starting our states from (-1),
   the actual call will be with (state + 1) as an index, used 
   in load_next_step ().
*/
static const load_state_func load_state_func_table [] =
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
static void advance_cycle_num (client_context* cctx);
static int on_cycling_completed (client_context* cctx, unsigned long *wait_msec);

static int setup_login_logoff (client_context* cctx, const int login);
static int setup_uas (client_context* cctx);

int pending_active_and_waiting_clients_num_hyper (batch_context* bctx);;

#if 0
#define PRINTF(args...) fprintf(stdout, ## args);
#else
#define PRINTF(args...)
#endif



typedef struct {

  curl_socket_t sockfd;

  int action;  /*CURL_POLL_IN CURL_POLL_OUT */
  long timeout;

  struct event ev;
  int evset;       /* is ev currently in use ? */

} sock_info;


#define MSG_OUT stdout

int still_running;

static void event_cb(int fd, short kind, void *userp);

/*--------------------------------------------------------------------------*/

struct event timer_event;

#if 0
struct event timer_next_load_event;
int next_load_event_set;
#endif


/* Update the event timer after curl_multi library calls */
static void update_timeout(batch_context *g)
{
  long timeout_ms;
  struct timeval timeout;

  curl_multi_timeout(g->multiple_handle, &timeout_ms);
  if(timeout_ms < 0) {
    return;
  }
  timeout.tv_sec = timeout_ms/1000;
  timeout.tv_usec = (timeout_ms%1000)*1000;
  evtimer_add(&timer_event, &timeout);
}



/* Assign information to a sock_info structure */
static void setsock(sock_info*f, curl_socket_t s, CURL*e, int act, batch_context * g)
{
  int kind =
     (act&CURL_POLL_IN?EV_READ:0)|(act&CURL_POLL_OUT?EV_WRITE:0)|EV_PERSIST;

  (void) e;

  f->sockfd = s;
  f->action = act;
  if (f->evset) { 
    event_del(&f->ev); 
  }
  event_set( &f->ev, f->sockfd, kind, event_cb, g);
  f->evset=1;
  event_add(&f->ev, NULL);
}

/* Initialize a new sock_info structure */
static void addsock(curl_socket_t s, CURL *easy, int action, client_context *cctx, sock_info *fdp, batch_context *g) {
  setsock(fdp, s, easy, action, g);
  curl_multi_assign(g->multiple_handle, s, cctx);
}


/* Clean up the sock_info structure */
static void remsock(sock_info *f)
{
  if (!f) { 
    return; 
  }
  if (f->evset) { 
    event_del(&f->ev); 
  }
  f->evset = 0;
}



/*--------------------------------------------------------------------------*/

/* LIBEVENT CALLBACK: Called by libevent when we get action on a multi socket */
static void event_cb(int fd, short kind, void *userp)
{
  batch_context *g = (batch_context *) userp;
  (void) kind;
  (void) fd;
  int st;

  PRINTF("event_cb enter\n");

#if 1
  CURLMcode rc;

  do {
    rc = curl_multi_socket(g->multiple_handle, fd, &st);
  } while (rc == CURLM_CALL_MULTI_PERFORM);
#else
  mperform_smooth (g, &st);
 
#endif



  if(st) {
    update_timeout(g);
  } else {
#if 0
    PRINTF("last transfer done, kill timeout\n");
    if (evtimer_pending(&timer_event, NULL)) {
      evtimer_del(&timer_event);
    }
#endif
  }
  PRINTF("event_cb exit\n");
}

static char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };


/* CURL CALLBACK: socket event of multi user handle */
static int sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
  batch_context* g = (batch_context *) cbp;
  client_context* cctx = (client_context *) sockp;
  sock_info *fdp = (sock_info*) cctx ? cctx->ext_data : 0;

  if (!cctx) {
   curl_easy_getinfo (e, CURLINFO_PRIVATE, &cctx);
    if (cctx) {
	fdp = (sock_info*)cctx->ext_data;
    }
  }

  if (!cctx) {
    PRINTF("no client context\n");
    return 0;
  }

  if (!fdp) {
    PRINTF("no extension data\n");
    return 0;
  }
  
  PRINTF("socket callback: ctx=%p sock=%d what=%s\n",cctx, s , whatstr[what]);

   
  if (what == CURL_POLL_REMOVE) {
    PRINTF("\n");
    remsock(fdp);
  } else {
    if (!fdp->evset) {
      PRINTF("Adding data: %s%s\n",
             what&CURL_POLL_IN?"READ":"",
             what&CURL_POLL_OUT?"WRITE":"" );
      addsock(s, e, what, cctx, fdp, g);
    }
    else {
      PRINTF("Changing action from %d to %d\n", fdp->action, what);
      setsock(fdp, s, e, what, g);
    }
  }
  return 0;
}

/* LIBEVENT CALLBACK: Called by libevent when our timeout expires */
static void timer_cb(int fd, short kind, void *userp)
{
  (void)fd;
  (void)kind;
  batch_context *g = (batch_context *)userp;
  CURLMcode rc;
  int st;

  do {
    rc = curl_multi_socket(g->multiple_handle, CURL_SOCKET_TIMEOUT, &st);
  } while (rc == CURLM_CALL_MULTI_PERFORM);
    
  mperform_smooth (g, &st);

  if ( still_running ) { update_timeout(g); }
}



#if 0
static void next_load_cb(int fd, short kind, void *userp)
{
  (void)fd;
  (void)kind;
  batch_context *g = (batch_context *)userp;
  PRINTF("next_load_cb\n");
  dispatch_expired_timers (g, get_tick_count());
}
#endif

#if 0 
/* CURLOPT_PROGRESSFUNCTION */
int prog_cb (void *p, double dltotal, double dlnow, double ult, double uln)
{
  (void) ult;
  (void) uln;
  PRINTF("Progress: ctx %p (%g/%g)\n", p, dlnow, dltotal);
  return 0;
}
#endif



/*--------------------------------------------------------------------------*/


/****************************************************************************************
 * Function name - user_activity_smooth
 *
 * Description - Simulates user-activities, like login, uas, logoff, using SMOOTH-MODE
 * Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
 * Return Code/Output - On Success - 0, on Error -1
 ****************************************************************************************/
int user_activity_hyper (client_context* cctx_array)
{
  batch_context* bctx = cctx_array->bctx;
  long logfile_timer_id = -1;
  long clients_num_inc_id = -1;
  sock_info *sinfo;
  int k,rc;
  int st;

  event_init();
  curl_multi_setopt(bctx->multiple_handle, CURLMOPT_SOCKETFUNCTION, sock_cb);
  curl_multi_setopt(bctx->multiple_handle, CURLMOPT_SOCKETDATA, bctx);
  evtimer_set(&timer_event, timer_cb, bctx);
#if 0  
  evtimer_set(&timer_next_load_event, next_load_cb, bctx);
#endif

  still_running = 1; 
 
  for (k = 0 ; k < bctx->client_num ; k++)
    {
      sinfo = malloc(sizeof(sock_info));
      if (!sinfo) {
	return -1;
      }
      memset(sinfo, 0, sizeof(sock_info));
      bctx->cctx_array[k].ext_data = sinfo;
      curl_easy_setopt (bctx->cctx_array[k].handle, CURLOPT_PRIVATE, bctx->cctx_array + k);
      set_timer_handling_func (&bctx->cctx_array[k], handle_cctx_timer_hyper);

    }

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

#if 1
  do {
    rc = curl_multi_socket_all(bctx->multiple_handle, &st);
  } while (CURLM_CALL_MULTI_PERFORM == rc);
#endif

  /* 
     ========= Run the loading machinery ================
  */

  while (still_running) /*(  pending_active_and_waiting_clients_num_hyper (bctx)) ||
         bctx->do_client_num_gradual_increase)*/
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
 * Function name - add_loading_clients
 *
 * Description - Initialization of our virtual clients (CURL handles)
 *               setting first url to fetch and scheduling them according to 
 *               clients increment for gradual loading.
 *
 * Input -       *bctx - pointer to the batch of contexts
 * Return Code/Output - On Success - 0, on error or request to unreg timer - (-1)
 ****************************************************************************************/
static int add_loading_clients (batch_context* bctx)
{
  /* 
     Return, if initial gradual scheduling of all new clients has been accomplished. 
  */
  if (bctx->client_num <= bctx->clients_initial_running_num)
    {
      bctx->do_client_num_gradual_increase = 0;
      return -1;
    }

  /* Calculate number of the new clients to schedule. */
  const long clients_sched = bctx->clients_initial_inc ? 
    min (bctx->clients_initial_inc, bctx->client_num - bctx->clients_initial_running_num) : 
    bctx->client_num; 

  //fprintf (stderr, "%s - adding %ld clients.\n", __func__, clients_sched);

  /* 
     Schedule new clients by initializing thier CURL handle with
     URL, etc. parameters and adding it to MCURL multi-handle.
  */
  long j;
  for (j = bctx->clients_initial_running_num; 
       j < bctx->clients_initial_running_num + clients_sched; 
       j++)
	{
      /* Runs load_init_state () for each newly added client. */
      if (load_next_step (&bctx->cctx_array[j], bctx->start_time) == -1)
        {  
          fprintf(stderr,"%s error: load_next_step() initial failed\n", __func__);
          return -1;
        }
    }

  /* 
     Re-calculate assisting counters and enable do_client_num_gradual_increase 
     flag, if required.
  */
  if (bctx->clients_initial_inc)
    {
      bctx->clients_initial_running_num += clients_sched;
      if (bctx->clients_initial_running_num < bctx->client_num)
        {
          bctx->do_client_num_gradual_increase = 1;
        }
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
  float max_timeout = DEFAULT_SMOOTH_URL_COMPLETION_TIME;
  int st;

  if (bctx->uas_url_ctx_array)
    {
      max_timeout = bctx->uas_url_ctx_array[0].url_completion_time;
    }
 
   event_dispatch();

   mperform_smooth (bctx, &st);


#if 0
  struct timeval timeout;

  mperform_smooth (bctx, &still_running)

  do {
    rc = curl_multi_socket_all((bctx->multiple_handle, &still_running);
  } while (CURLM_CALL_MULTI_PERFORM == rc);
  
  while (still_running && max_timeout > 0.0) 
    {
      int rc, maxfd;
      fd_set fdread, fdwrite, fdexcep;

      FD_ZERO(&fdread); FD_ZERO(&fdwrite); FD_ZERO(&fdexcep);
      timeout.tv_sec = 0;
      timeout.tv_usec = 250000;

      max_timeout -= ((float)timeout.tv_sec + (float)timeout.tv_usec/1000000.0) ; 
      curl_multi_fdset(bctx->multiple_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

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
#endif    
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
static int mperform_smooth (batch_context* bctx, int* still_running)
{
  CURLM *mhandle =  bctx->multiple_handle;
  int cycle_counter = 0;	
  int msg_num = 0;
  const int snapshot_timeout = snapshot_statistics_timeout*1000;
  unsigned long now_time;
  CURLMsg *msg;
    
#if 0    
  while (CURLM_CALL_MULTI_PERFORM == 
        curl_multi_perform(mhandle, still_running))
    ;
#endif
  now_time = get_tick_count ();

  if ((long)(now_time - bctx->last_measure) > snapshot_timeout) 
    {
      dump_snapshot_interval (bctx, now_time);
    }

  dispatch_expired_timers (bctx, now_time);

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
              now_time = get_tick_count ();
            }

          /*cstate client_state =  */
          load_next_step (cctx, now_time);

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
 * Return Code/Output - On Success - 0, on Error -1
 ****************************************************************************************/
static int
dispatch_expired_timers (batch_context* bctx, unsigned long now_time)
{
  timer_queue* tq = bctx->waiting_queue;

  if (!tq)
    return -1;

  if (tq_empty (tq))
    return 1;

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
        }
      else
        break;
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
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
static int load_next_step (client_context* cctx, unsigned long now_time)
{
  batch_context* bctx = cctx->bctx;
  int rval_load = CSTATE_ERROR;
  unsigned long interleave_waiting_time = 0;

  PRINTF("load_next_step %p state %d\n",cctx, cctx->client_state);
	
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
  if (1) //! interleave_waiting_time)
    {
      /* Schedule the client immediately */
      if (client_add_to_load (bctx, cctx) == -1)
        {
          fprintf (stderr, "%s - error: client_add_to_load () failed .\n", __func__);
          return -1;
        }
    }
  else
    {
      struct timeval timeout;
 
      PRINTF("load_next_step: ctx %p schedule next load in %d seconds\n", cctx,(int) interleave_waiting_time/1000);
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

#if 0
      timeout.tv_sec = (tq_time_to_nearest_timer (bctx->waiting_queue) - get_tick_count()) / 1000 + 1;
      timeout.tv_usec = 0;
      evtimer_add(&timer_next_load_event, &timeout);

      PRINTF("load_next_step: next evlib timer %d\n",(int) timeout.tv_sec);
#endif
	
      
      //fprintf (stderr, "%s - scheduled client to wq with wtime %ld\n", 
      //				 __func__, interleave_waiting_time);
    }

  return rval_load;
}



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

  return CSTATE_ERROR;
}

static int first_cycling_state (batch_context* bctx)
{
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

  PRINTF("on_cycling_completed %p\n",cctx);

  still_running = 0;

  /* 
     Go to not-cycling logoff, else to the finish-line. 
  */
  if (bctx->do_logoff && !bctx->logoff_cycling)
    return load_logoff_state (cctx, wait_msec);


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
         - GET, which is by itself enough, e.g. for logoff using cookies;
         - GET, which will be later followed by later POST login/logoff;
         - POST, which is the standalone login/logoff, without any previous GET;
      */
      int post_standalone = 0;

      if ((login && bctx->login_req_type == LOGIN_REQ_TYPE_POST) ||
          (!login && bctx->logoff_req_type == LOGOFF_REQ_TYPE_POST))
        {
          post_standalone = 1;
        }

      if (setup_curl_handle_init_hyper(
                                  cctx,
                                  login ? &bctx->login_url : &bctx->logoff_url,
                                  0, /* Not applicable for smooth mode */
                                  post_standalone /* If 'true' -POST, else GET */
                                  ) == -1)
        {
          fprintf(stderr,"%s error: setup_curl_handle_init - failed\n", __func__);
          return -1;
        }
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
      curl_easy_setopt (handle, CURLOPT_POSTFIELDS, 
                        login ? cctx->post_data_login : cctx->post_data_logoff);
    }

  return cctx->client_state = login ? CSTATE_LOGIN : CSTATE_LOGOFF;
}


int setup_curl_handle_init_hyper(client_context*const cctx,
                         url_context* url_ctx,
                         long cycle_number,
                         int post_method)
{
  batch_context* bctx = cctx->bctx;
  CURL* handle = cctx->handle;
  int rc,st;

  if (!cctx || !url_ctx)
    {
      return -1;
    }
  
  PRINTF("init_hyper %p\n",cctx);
  curl_easy_reset (handle);

  if (bctx->ipv6)
    curl_easy_setopt (handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
      
  /* Bind the handle to a certain IP-address */
  curl_easy_setopt (handle, CURLOPT_INTERFACE, 
                    bctx->ip_addr_array [cctx->client_index]);

  curl_easy_setopt (handle, CURLOPT_NOSIGNAL, 1);
    
  /* Set the url */
  if (url_ctx->url_str && url_ctx->url_str[0])
    {
      curl_easy_setopt (handle, CURLOPT_URL, url_ctx->url_str);
    }
  else
    {
      fprintf (stderr,"%s - error: empty url provided.\n",
                   __func__);
      exit (-1);
    }
  
  /* Set the index to client for the smooth-mode */
  if (url_ctx->url_uas_num >= 0)
    cctx->uas_url_curr_index = url_ctx->url_uas_num;
  
  bctx->url_index = url_ctx->url_uas_num;

  curl_easy_setopt (handle, CURLOPT_DNS_CACHE_TIMEOUT, -1);

  /* Set the connection timeout */
  curl_easy_setopt (handle, CURLOPT_CONNECTTIMEOUT, connect_timeout);

  /* Define the connection re-use policy. When passed 1, dont re-use */
  curl_easy_setopt (handle, CURLOPT_FRESH_CONNECT, reuse_connection_forbidden);

  /* 
     If DNS resolving is necesary, global DNS cache is enough,
     otherwise compile libcurl with ares (cares) library support.
     Attention: DNS global cache is not thread-safe, therefore use
     cares for asynchronous DNS lookups.

     curl_easy_setopt (handle, CURLOPT_DNS_USE_GLOBAL_CACHE, 1); 
  */
     
  curl_easy_setopt (handle, CURLOPT_VERBOSE, 1);
  curl_easy_setopt (handle, CURLOPT_DEBUGFUNCTION, 
                    client_tracing_function);
  curl_easy_setopt (handle, CURLOPT_DEBUGDATA, cctx);

#if 0
  curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, prog_cb);
  curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, cctx);
#endif


  if (!output_to_stdout)
    {
      curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION,
                        do_nothing_write_func);
    }

  curl_easy_setopt (handle, CURLOPT_SSL_VERIFYPEER, 0);
  curl_easy_setopt (handle, CURLOPT_SSL_VERIFYHOST, 0);
    
  /* Set current cycle_number in buffer. */
  if (loading_mode == LOAD_MODE_STORMING)
    {
      cctx->cycle_num = cycle_number;
    }

  /* 
     This is to return cctx pointer as the void* userp to the 
     tracing function. 
  */
  
  /* Set the private pointer to be used by the smooth-mode. */
  curl_easy_setopt (handle, CURLOPT_PRIVATE, cctx);
  curl_easy_setopt (handle, CURLOPT_WRITEDATA, cctx);

  /* Without the buffer set, we do not get any errors in tracing function. */
  curl_easy_setopt (handle, CURLOPT_ERRORBUFFER, bctx->error_buffer);    

  /* 
     Application (url) specific setups, like HTTP-specific, FTP-specific, etc. 
  */
  if (setup_curl_handle_appl (cctx, url_ctx, post_method) == -1)
    {
      fprintf (stderr,
               "%s - error: setup_curl_handle_appl () failed .\n",
               __func__);
      return -1;
    }

#if 1 
  do {
    rc = curl_multi_socket_all(bctx->multiple_handle, &st);
  } while (CURLM_CALL_MULTI_PERFORM == rc);
#endif
     
  return 0;
}
/****************************************************************************************
 * Function name - setup_uas
 *
 * Description - Sets UAS state url
 * Input -       *cctx - pointer to the client context
 *
 * Return Code/Output - CSTATE enumeration with client state
 ****************************************************************************************/
static int setup_uas (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  if (setup_curl_handle_init_hyper (
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
  PRINTF("->STATE: load_init_state %p\n",cctx);

  batch_context* bctx = cctx->bctx;

  *wait_msec = 0;

  if (bctx->do_login) /* Normally, the very first operation is login, but who is normal? */
    {
      return load_login_state (cctx, wait_msec);
    }
  else if (bctx->do_uas) /* Sometimes, no login is defined. Just a traffic-gen */
    {
      return load_uas_state (cctx, wait_msec);
    }
  else if (bctx->do_logoff) /* Logoff only?  If this is what a user wishing ...  */
    {
      return load_logoff_state (cctx, wait_msec);
    }

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

  PRINTF("->STATE: load_login_state %p\n",cctx);

  /*
    Test for login state, if a single login operation has been accomplished. 
    Sometimes, the operation contains two elements: GET and POST.
  */
  if (cctx->client_state == CSTATE_LOGIN)
    {
      if ((bctx->login_req_type != LOGIN_REQ_TYPE_GET_AND_POST) ||
          (bctx->login_req_type == LOGIN_REQ_TYPE_GET_AND_POST && !cctx->get_post_count))
        {
          /* 
             Indeed, accomplished a single login operation. 
          */

          /* Mind the interleave timeout after login */
          *wait_msec = bctx->login_url.url_interleave_time;

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
                  return on_cycling_completed (cctx, wait_msec);
                }
              else
                {
                  /* 
                     Configured to cycle, but the state is the last cycling state 
                     and the only cycling state, therefore, continue login. 
                  */
                  return setup_login_logoff (cctx, 1); /* 1 - means login */
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

  PRINTF("->STATE: load_uas_state %p\n",cctx);


  if (cctx->client_state == CSTATE_UAS_CYCLING)
    {
      /* Mind the interleave timeout after each url, if any. */
      *wait_msec = bctx->uas_url_ctx_array[cctx->uas_url_curr_index].url_interleave_time;

      /* Now, advance the url index */
      cctx->uas_url_curr_index++;

      if (cctx->uas_url_curr_index >= (size_t)(bctx->uas_urls_num))
        {
          /* Finished with all the urls for a single UAS -cycle. */

          cctx->uas_url_curr_index = 0;
 
          if (is_last_cycling_state (cctx))
            {
              /* If UAS is the last cycling state, advance cycle counter. */
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  /* Either logoff or finish_ok. */
                  return on_cycling_completed (cctx, wait_msec);
                }
              else
                {
                  /* Continue cycling - take another cycle */
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx, wait_msec);
                  else
                    return setup_uas (cctx);
                }
            }

          /* 
             We are not the last cycling state. A guess is, that the
             next state is logoff with cycling.
          */
          return load_logoff_state (cctx, wait_msec);
        }
    }
  else
    {
      cctx->uas_url_curr_index = 0;
    }

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
  if (cctx->client_state == CSTATE_LOGOFF)
    {
      if ((bctx->logoff_req_type != LOGOFF_REQ_TYPE_GET_AND_POST) ||
          (bctx->logoff_req_type == LOGOFF_REQ_TYPE_GET_AND_POST && !cctx->get_post_count))
        {
          /* 
             Indeed, we have accomplished a single logoff operation. 
          */

          /* Mind the interleave timeout after login */
          *wait_msec = bctx->logoff_url.url_interleave_time;

          if (is_last_cycling_state (cctx))
            {
              /*
                If logoff cycling ,we are in charge for advancing cycles counter.
              */
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  return on_cycling_completed (cctx, wait_msec); /* Goes to finish-ok */
                }
              else
                {
                  /*
                    Continue cycling - take another cycle
                  */
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx, wait_msec);
                  else if (bctx->do_uas)
                    return load_uas_state (cctx, wait_msec);
                  else /* logoff is the only cycling state? Sounds strange, but allow it */
                    return setup_login_logoff (cctx, 0); /* 0 - means logoff */
                }
            }
 
          /* If not doing logoff cycling, means single logoff done - go to finish-ok */
          return (cctx->client_state = CSTATE_FINISHED_OK);
        }
    }

  /* Non-LOGOFF states are all falling below: */
  return setup_login_logoff (cctx, 0);
}

static int load_final_ok_state (client_context* cctx, unsigned long *wait_msec)
{
  (void) cctx; (void) wait_msec;

  return CSTATE_FINISHED_OK;
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
static int client_add_to_load (batch_context* bctx, client_context* cctx)
{
  PRINTF("client_add_to_load %p\n",cctx);

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
static int client_remove_from_load (batch_context* bctx, client_context* cctx)
{
  PRINTF("client_remove_from_load %p\n",cctx);
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
      fprintf (stderr, "%s - curl_multi_eemove_handle () failed.\n", __func__);
      return -1;
    }

  return 0;	
}

/****************************************************************************************
 * Function name - client_remove_from_load
 *
 * Description - Returns the sum of active and waiting (for load scheduling) clients
 *
 * Input -       *bctx - pointer to the batch context
 *
 * Return Code/Output - Sum of active and waiting (for load scheduling) clients
 ****************************************************************************************/
int pending_active_and_waiting_clients_num_hyper (batch_context* bctx)
{
  return bctx->waiting_queue ? 
    (bctx->active_clients_count + tq_size (bctx->waiting_queue) - 
     PERIODIC_TIMERS_NUMBER - bctx->do_client_num_gradual_increase) :
    bctx->active_clients_count;
}

/****************************************************************************************
 * Function name - handle_cctx_timer_hyper
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
int handle_cctx_timer_hyper (timer_node* timer_node, 
                       void* pvoid_param,
                       unsigned long ulong_param)
{
  PRINTF("handle_cctx_timer_hyper\n");
  
  client_context* cctx = (client_context *) timer_node;
  batch_context* bctx = cctx->bctx;
  (void)pvoid_param;
  (void)ulong_param;

  return client_add_to_load (bctx, cctx);
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
static int handle_logfile_rewinding_timer  (timer_node* timer_node, 
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
static int handle_gradual_increase_clients_num_timer  (timer_node* timer_node, 
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
