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

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"
#include "heap.h"

#define DEFAULT_SMOOTH_URL_COMPLETION_TIME 6.0
#define TIME_RECALCULATION_MSG_NUM 100

static int add_loading_clients (batch_context* bctx);
static int add_loading_clients_cont (batch_context* bctx, unsigned long now_time);
static int mget_url_smooth (batch_context* bctx);
static int mperform_smooth (batch_context* bctx, int* still_running);
static int schedule_clients_from_waiting_queue (
	batch_context* bctx, 
	unsigned long now_time);

static int load_next_step (client_context* cctx, unsigned long now_time);

/*
  Next step initialization functions relevant the client state.
*/
static int load_error_state (client_context* cctx, unsigned long *wait_msec);
static int load_init_state (client_context* cctx, unsigned long *wait_msec);
static int load_login_state (client_context* cctx, unsigned long *wait_msec);
static int load_uas_state (client_context* cctx, unsigned long *wait_msec);
static int load_logoff_state (client_context* cctx, unsigned long *wait_msec);
static int load_final_ok_state (client_context* cctx, unsigned long *wait_msec);

typedef int (*load_state_func)  (client_context* cctx, unsigned long *wait_msec);

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
static void advance_cycle_num (client_context* cctx);
static int on_cycling_completed (client_context* cctx, unsigned long *wait_msec);

static int setup_login_logoff (client_context* cctx, const int login);
static int setup_uas (client_context* cctx);

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

	if (!bctx)
	{
		fprintf (stderr, "%s - error: bctx is a NULL pointer.\n", __func__);
		return -1;
	}

	/* Make smooth-mode specific initializations */

	if (! (bctx->waiting_queue = calloc (1, sizeof (heap))))
	{
		fprintf (stderr, "%s - error: failed to allocate queue.\n", __func__);
		return -1;
	}
  
	if (tq_init (
				bctx->waiting_queue,
				bctx->client_num, /* tq size */
				0, /* tq increase step; 0 - means don't increase */
				bctx->client_num /* number of nodes to prealloc */
				) == -1)
	{
		fprintf (stderr, "%s - error: failed to initialize waiting queue.\n", __func__);
		return -1;
	}
  
	bctx->start_time = bctx->last_measure = get_tick_count ();
	bctx->sec_current = 0;
	bctx->active_clients_count = 0;

	if (add_loading_clients (bctx) == -1)
	{
		fprintf (stderr, "%s error: add_loading_clients () failed.\n", __func__);
		return -1;
	}
  
	while (
		bctx->active_clients_count ||
		bctx->do_client_num_gradual_increase ||
		! tq_empty (bctx->waiting_queue)
		)
	{
		if (!bctx->active_clients_count && bctx->do_client_num_gradual_increase)
		{
			if (add_loading_clients (bctx) == -1)
			{
				fprintf (stderr, "%s error: add_loading_clients () failed in while.\n", 
								 __func__) ;
				return -1;
			}
		}

		if (mget_url_smooth (bctx) == -1) 
		{
			fprintf (stderr, "%s error: mget_url () failed.\n", __func__) ;
			return -1;
		}
	}

	dump_final_statistics (cctx_array);

	return 0;
}

/****************************************************************************************
* Function name - add_loading_clients
*
* Description - First initialization of our virtual clients (CURL handles)
*                       setting first url to fetch and scheduling them according to 
*                       clients increment for initial gradual loading.
*
* Input -       *bctx - pointer to the batch of contexts
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int add_loading_clients (batch_context* bctx)
{
	/* 
		 Return, if initial gradual scheduling of all new clients has been accomplished. 
	*/
	if (bctx->client_num <= bctx->clients_initial_running_num)
	{
		bctx->do_client_num_gradual_increase = 0;
		return 0;
	}

	/* Calculate number of the new clients to schedule. */
	const long clients_sched = bctx->clients_initial_inc ? 
		min (bctx->clients_initial_inc, bctx->client_num - bctx->clients_initial_running_num) : 
		bctx->client_num; 

	/* 
		 Disable do_client_num_gradual_increase flag to prevent recursive 
		 calls in load_next_step () 
	*/
	bctx->do_client_num_gradual_increase = 0;

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
		 Re-calculate assisting counters and enable back 
		 do_client_num_gradual_increase flag, if required.
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
* Function name - add_loading_clients_cont
*
* Description - Continue initialization of our virtual clients (CURL handles)
*                       by adding more clients each new second and scheduling 
*                       them according to client's increment number for initial gradual 
*                       loading. Uses add_loading_clients () function.
*
* Input -       *bctx - pointer to the batch of contexts
*                   now_time -  current time in millisecond ticks
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int add_loading_clients_cont (batch_context* bctx, unsigned long now_time)
{
	long sec = (now_time - bctx->start_time)/1000;

	if (sec > bctx->sec_current) /* If a new second, schedule more clients. */
	{
		bctx->sec_current = sec;
		
		/* New second. Calling to schedule more clients. */
		return add_loading_clients (bctx);
	}
	return 0;
}

/****************************************************************************************
* Function name - mget_url_smooth
*
* Description - Performs actual fetching of urls for a whole batch. Starts with initial fetch
*                     by mperform_smooth () and further acts using mperform_smooth () on select events
* Input -       *bctx - pointer to the batch of contexts
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int mget_url_smooth (batch_context* bctx)  		       
{
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
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;

        max_timeout -= ((float)timeout.tv_sec + (float)timeout.tv_usec/1000000.0) ; 
        curl_multi_fdset(bctx->multiple_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
        //fprintf (stderr, "%s - Waiting for %d clients with seconds %f.\n", 
        //name, still_running, max_timeout);

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

/****************************************************************************************
* Function name - mperform_smooth
*
* Description - Uses curl_multi_perform () for initial url-fetch and to react on socket events.
*                     Uses curl_multi_info_read () to test url-fetch completion events and to proceed
*                     with the next step for the client, using load_next_step (). Cares about statistics
*                     at certain timeouts.
*
* Input -       *bctx - pointer to the batch of contexts;
*                     *still_running - pointer to counter of still running clients (CURL handles)
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int mperform_smooth (batch_context* bctx, int* still_running)
{
	CURLM *mhandle =  bctx->multiple_handle;
	unsigned long now_time;
	int cycle_counter = 0;	
	int msg_num = 0;
	CURLMsg *msg;
    
	while(CURLM_CALL_MULTI_PERFORM == 
			curl_multi_perform(mhandle, still_running))
      ;

	now_time = get_tick_count ();
	if ((now_time - bctx->last_measure) > snapshot_timeout) 
	{
		dump_intermediate_and_advance_total_statistics (bctx);
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

	schedule_clients_from_waiting_queue (bctx, now_time);

	return 0;
}

static int
schedule_clients_from_waiting_queue (batch_context* bctx, unsigned long now_time)
{
	timer_queue* tq = bctx->waiting_queue;

	if (!tq)
		return -1;

	if (tq_empty (tq))
		return 0;

	while (! tq_empty (tq))
	{
		long time_nearest = tq_time_to_nearest_timer (tq);

		if (time_nearest <= (long)now_time)
		{
			client_context* cctx = (client_context *) tq_remove_nearest_timer (tq);

			if (!cctx)
			{
				fprintf (stderr, "%s - error: cctx from tq_remove_nearest_timer is NULL.\n", 
								 __func__);
				return -1;
			}
						
			/* Schedule the client immediately */
			if (curl_multi_add_handle (bctx->multiple_handle, cctx->handle) ==  CURLM_OK)
			{
				bctx->active_clients_count++;
				//fprintf (stderr, "%s - client added after wq to work.\n", __func__);
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
*                        indication (that may be an error status as well). Either sets 
*                        to client the next url to load, or sets it at completion state: 
*                        CSTATE_ERROR or CSTATE_FINISHED_OK.
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
	
	/* Remove handle from the multiple handle, if it was added there before. */
	if (cctx->client_state != CSTATE_INIT)
	{
		if (curl_multi_remove_handle (bctx->multiple_handle, cctx->handle) == CURLM_OK)
		{
			if (bctx->active_clients_count > 0)
				bctx->active_clients_count--;
		}
	}

	/* 
		 Initialize virtual client's CURL handle for the next step of loading by calling
		 load_* function relevant for the client state.
	*/
	rval_load = load_state_func_table[cctx->client_state+1](cctx, &interleave_waiting_time);

	/* 
		 If there are still not scheduled clients, thus, continue their gradual
		 scheduling.
	*/
	if (bctx->do_client_num_gradual_increase)
	{
		add_loading_clients_cont (bctx, now_time);
	}
  
	/* 
		 Just return for error or finished states: "finita la comedia".
	*/
	if (rval_load == CSTATE_ERROR || rval_load == CSTATE_FINISHED_OK)
	{
		return rval_load;
	} 

	/* 
		 Schedule virtual clients by adding them to multi-handle, 
		 if the clients are not in error or finished final states.
	*/
	if (! interleave_waiting_time)
	{
		/* Schedule the client immediately */
		if (curl_multi_add_handle (bctx->multiple_handle, cctx->handle) ==  CURLM_OK)
			bctx->active_clients_count++;
	}
	else
	{
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
		//fprintf (stderr, "%s - scheduled client to wq.\n", __func__);
	}

	return rval_load;
}


/****************************************************************************************
* Function name - is_last_cycling_state
*
* Description - Figures out, whether the current state of client is the last cycling state.
*                     Only in the last cycling state the number of cycles is advanced.
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - true, when the last cycling state, else - false
****************************************************************************************/
static int is_last_cycling_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  switch (cctx->client_state)
    {
    case CSTATE_LOGIN:
      return (bctx->login_cycling && !bctx->do_uas && 
              !(bctx->do_logoff && bctx->logoff_cycling)) ? 1 : 0;
    case CSTATE_UAS_CYCLING:
      return (!(bctx->do_logoff && bctx->logoff_cycling)) ? 1 : 0;
    case CSTATE_LOGOFF: /* Logoff cycling, if exists, 
                           supposed to be the last state of a cycle. */
      return bctx->logoff_cycling ? 1 : 0;

    default:
      return 0;
    }
  return 0;
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
* Description - Either goes to logoff state (logoff-no-cycling) of to CSTATE_FINISHED_OK
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - CSTATE enumeration with client state
****************************************************************************************/
static int on_cycling_completed (client_context* cctx, unsigned long *wait_msec)
{
	batch_context* bctx = cctx->bctx;

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
*                     login - when true - login state, when false logoff state is set
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
         - GET, which is by itself enough for logoff (thanks to cookies);
         - GET, which will be later followed by later POST login/logoff;
         - POST, which is the standalone login/logoff, without any previous GET;
      */
      int post_standalone = 0;

      if ((login && bctx->login_req_type == LOGIN_REQ_TYPE_POST) ||
          (!login && bctx->logoff_req_type == LOGOFF_REQ_TYPE_POST))
        {
          post_standalone = 1;
        }

      if (setup_curl_handle_init (
				 cctx,
				 login ? &bctx->login_url : &bctx->logoff_url,
				 0, /* Not applicable for the smooth mode */
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
         which may be the result of redirection/s,  but use HTTP POST 
         request method with post login/logoff fields. 
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
*                     *wait_msec - pointer to time to wait till next scheduling (interleave time).
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_init_state (client_context* cctx, unsigned long *wait_msec)
{
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
*                     flag <error_recovery_client> is not false, re-schedules the client for next cycle 
*                     of loading.
* Input -       *cctx - pointer to the client context
*                     *wait_msec - pointer to time to wait till next scheduling (interleave time).
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
			if (bctx->do_login && bctx->login_cycling)
			{
				return load_login_state (cctx, wait_msec);
			}
			else if (bctx->do_uas)
			{
				return load_uas_state (cctx, wait_msec);
			}
			else if (bctx->do_logoff && bctx->logoff_cycling)
			{
				return load_logoff_state (cctx, wait_msec);
			}
		}
	}
 
  /* Thus, the client will not be scheduled for load any more. */
  return (cctx->client_state = CSTATE_ERROR);
}

/****************************************************************************************
* Function name - load_login_state
*
* Description - Called by load_next_step () for the client in CSTATE_LOGIN 
*                        state to schedule the next loading url.
*
* Input -       *cctx - pointer to the client context
*                     *wait_msec - pointer to time to wait till next scheduling (interleave time).
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_login_state (client_context* cctx, unsigned long *wait_msec)
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
* Description - Called by load_next_step () for the client in CSTATE_UAS state to schedule the
*                     next loading url.
* Input -       *cctx - pointer to the client context
*                     *wait_msec - pointer to time to wait till next scheduling (interleave time).
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_uas_state (client_context* cctx, unsigned long *wait_msec)
{ 
  batch_context* bctx = cctx->bctx;

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
*                     schedule the next loading url.
* Input -       *cctx - pointer to the client context
*                     *wait_msec - pointer to time to wait till next scheduling (interleave time).
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
