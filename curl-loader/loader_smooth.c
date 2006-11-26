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
static int load_error_state (client_context* cctx);
static int load_login_state (client_context* cctx);
static int load_uas_state (client_context* cctx);
static int load_logoff_state (client_context* cctx);

static int is_last_cycling_state (client_context* cctx);
static void advance_cycle_num (client_context* cctx);
static int on_cycling_completed (client_context* cctx);

static int setup_login_logoff (client_context* cctx, const int login);
static int setup_uas (client_context* cctx);

/****************************************************************************************
* Function name - user_activity_smooth
*
* Description - Simulates user-activities, like login, uas, logoff, using SMOOTH-MODE
* Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int user_activity_smooth (client_context* cctx)
{
  int j;
  batch_context* bctx = cctx->bctx;

  if (!bctx)
    {
      fprintf (stderr, 
               "%s - error: bctx in the first cctx input is zero.\n", __func__);
      return -1;
    }
  
  bctx->start_time = bctx->last_measure = get_tick_count();

  /* Load the first url for all clients */

  /* 
     TODO. Not all clients should start loading immediately.
     Calls-per-seconds number (CAPS) should be configurable.
  */
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

  dump_final_statistics (cctx);

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
  //CURLM *mhandle =  bctx->multiple_handle;
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
        curl_multi_fdset(bctx->multiple_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
        //        fprintf (stderr, "%s - Waiting for %d clients with seconds %f.\n", 
        //        name, still_running, max_timeout);

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
    
    while(CURLM_CALL_MULTI_PERFORM == 
          curl_multi_perform(mhandle, still_running))
      ;

    unsigned long now = get_tick_count(); 

    if ((now - bctx->last_measure) > snapshot_timeout) 
      {
        dump_intermediate_and_advance_total_statistics (bctx);
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

            if (msg->data.result)
              {
                // fprintf (stderr, "res is %d ", msg->data.result);
                cctx->client_state = CSTATE_ERROR;
                
                /* client tracing function stats all the errors
                stat_err_inc (cctx);
                 fprintf(cctx->file_output, "%ld %s !! ERROR: %d - %s\n", 
                        cctx->cycle_num, cctx->client_name, msg->data.result, 
                        curl_easy_strerror(msg->data.result ));
                */
              }

            cstate client_state =  load_next_step (cctx);
            //fprintf (stderr, "%s - after load_next_step client state %d.\n", __func__, client_state);
            //fprintf (stderr, "%d ", client_state);

            if (client_state == CSTATE_ERROR || client_state == CSTATE_FINISHED_OK) 
              {
                bctx->active_clients_count--; 
              }
          
            if (msg_num <= 0)
              {
                /* If no messages left in the queue - go out */
                break;
              }
          }
      }

    return 0;
}

/****************************************************************************************
* Function name - load_next_step
*
* Description - Called at initialization and further after url-fetch completion indication (that
*                     may be an error status as well). Either sets to client the next url to load, or sets it
*                     at completion state: CSTATE_ERROR or CSTATE_FINISHED_OK.
*
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_next_step (client_context* cctx)
{
  switch (cctx->client_state)
    {
    case CSTATE_ERROR:
      return load_error_state (cctx);
    case CSTATE_INIT:
      return load_init_state (cctx);
    case CSTATE_LOGIN:
       return load_login_state (cctx);
    case CSTATE_UAS_CYCLING:
      return load_uas_state (cctx);
    case CSTATE_LOGOFF:
      return load_logoff_state (cctx);
    case CSTATE_FINISHED_OK:
      return CSTATE_FINISHED_OK;
    }
  
  return CSTATE_ERROR;
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
static int on_cycling_completed (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  /* 
     Go to not-cycling logoff, else to the finish-line. 
  */
  if (bctx->do_logoff && !bctx->logoff_cycling)
    return load_logoff_state (cctx);

  curl_multi_remove_handle (bctx->multiple_handle, cctx->handle);
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
                           0, /* Not applicable for the smooth mode */
                           post_standalone /* If 'true' -POST, else GET */  
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
      CURL* handle = cctx->handle; // bctx->client_handles_array[cctx->client_index];

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

  setup_curl_handle (cctx,
                       &bctx->uas_url_ctx_array[cctx->uas_url_curr_index], /* current url */
                       0, /* Cycle, do we need it? */ 
                       0 /* GET - zero, unless we'll need to make POST here */
                       );

  return cctx->client_state = CSTATE_UAS_CYCLING;
}

/****************************************************************************************
* Function name - load_init_state
*
* Description - Called by load_next_step () to set the very first url to fetch, which depends on
*                     user-defined batch configuration.
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_init_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  if (bctx->do_login) /* Normally first operation is login, but who is normal? */
      return load_login_state (cctx);
  else if (bctx->do_uas) /* Sometimes, no login is defined. Just a traffic-gen */
      return load_uas_state (cctx);
  else if (bctx->do_logoff) /* Logoff only?  If this is what a user wishing ...  */
      return load_logoff_state (cctx);

  return (cctx->client_state = CSTATE_ERROR);
}

/****************************************************************************************
* Function name - load_error_state
*
* Description - Called by load_next_step () for the client in CSTATE_ERROR. If the global
*                     flag <error_recovery_client> is not false, re-schedules the client for next cycle 
*                     of loading.
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_error_state (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;

  if (error_recovery_client)
    {
      advance_cycle_num (cctx);

      if (cctx->cycle_num >= bctx->cycles_num)
        {
          curl_multi_remove_handle (bctx->multiple_handle, cctx->handle);
          return (cctx->client_state = CSTATE_ERROR);
        }
      else
        {
          if (bctx->do_login && bctx->login_cycling)
            return load_login_state (cctx);
          else if (bctx->do_uas)
            return load_uas_state (cctx);
          else if (bctx->do_logoff && bctx->logoff_cycling)
            return load_logoff_state (cctx);
        }
    }
 
  /* Thus, the client will not be scheduled for load any more. */
  curl_multi_remove_handle (bctx->multiple_handle, cctx->handle);
  return (cctx->client_state = CSTATE_ERROR);
}

/****************************************************************************************
* Function name - load_login_state
*
* Description - Called by load_next_step () for the client in CSTATE_LOGIN state to schedule
*                     the next loading url.
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
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
                  return setup_login_logoff (cctx, 1); /* 1 - means login */
                }
            }
 
          if (bctx->do_uas)
            return load_uas_state (cctx);
          else if (bctx->do_logoff)
            return load_logoff_state (cctx);
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
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
static int load_uas_state (client_context* cctx)
{ 
  batch_context* bctx = cctx->bctx;

  if (cctx->client_state == CSTATE_UAS_CYCLING)
    {
      cctx->uas_url_curr_index++;

       if (cctx->uas_url_curr_index >= (size_t)(bctx->uas_urls_num))
        {
          /* Accomplished all the urls for a single UAS -cycle. */

          cctx->uas_url_curr_index = 0;
 
          if (is_last_cycling_state (cctx))
            {
              /* If UAS is the last cycling state, advance cycle counter. */
              advance_cycle_num (cctx);

              if (cctx->cycle_num >= bctx->cycles_num)
                {
                  /* Either logoff or finish_ok. */
                  return on_cycling_completed (cctx);
                }
              else
                {
                  /* Continue cycling - take another cycle */
                  if (bctx->do_login && bctx->login_cycling)
                    return load_login_state  (cctx);
                  else
                    return setup_uas (cctx);
                }
            }

          /* 
             We are not the last cycling state. A guess is, that the
             next state is logoff with cycling.
          */
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

/****************************************************************************************
* Function name - load_logoff_state
*
* Description - Called by load_next_step () for the client in CSTATE_LOGOFF state to 
*                     schedule the next loading url.
* Input -       *cctx - pointer to the client context
*
* Return Code/Output - CSTATE enumeration with the state of loading
****************************************************************************************/
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
                  return on_cycling_completed (cctx); /* Goes to finish-ok */
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


