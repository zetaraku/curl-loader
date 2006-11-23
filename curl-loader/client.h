/*
*     client.h
*
* 2006 Copyright (c) 
* Robert Iakobashvili, <coroberti@gmail.com>
* Michael Moser,  <moser.michael@gmail.com>                 
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

#ifndef CLIENT_H
#define CLIENT_H

#include <curl/curl.h>

#include "statistics.h"
#include "timer_node.h"

#define CLIENT_NAME_LEN 32

/*
****************   cstate   ************************************
  States of a virtual client. 
*/
typedef enum cstate
  {
    CSTATE_ERROR = -1, /* Completion state */
    CSTATE_INIT, /* state 0, thus, calloc set the state automatically */
  
    /* Operational states */
    CSTATE_LOGIN, /* cycling state, if cycling enabled */
    CSTATE_UAS_CYCLING, /* cycling state */
    CSTATE_LOGOFF, /* cycling state, if cycling enabled */

    CSTATE_FINISHED_OK, /* Completion state */
  } cstate;

/* Forward declarations */
struct batch_context;

/*
****************   client_context   ************************************

  The structure is the placeholder of  a virtual client stateful information.
  
  Client context is passed to curl handle to be returned back to the tracing
  function for output to the logfile.
  Client context is also heavily used for login/logoff, using POST-buffers,
  and for smooth-mode.
*/
typedef struct client_context
{
  /* Thus, we can use it as a timer_node without disclosing all other
     structure of client_context */
  timer_node tn;

   /*
      <Current Cycle Num> SP <Client Seq Num> SP <Client IP-address> 
   */
  char client_name[CLIENT_NAME_LEN];

  /* 
     Library handle, representing all knowledge about the client from the
     side of libcurl.
  */
  CURL* handle;
 
  long cycle_num;  /* Current cycle number. Useful for multiple runs. */                          

  FILE* file_output;  /* The file to be used as output for this particular 
                             client (normally used a file-per-batch logging strategy) */

/* 
   Batch context, which is running the client. Used for getting configs, like 
   urls and writing statistics back. 
*/
  struct batch_context* bctx;

  size_t client_index; /* Index of the client within its batch. */

  size_t uas_url_curr_index; /* Index of the currently used UAS url. */

  cstate client_state; /* Current state of the client. */

  int errors_num; /* Number of errors */ 

  /* Flag controlling sequence of GET-POST logins and logoffs.
     When 0 - do GET, when 1 do POST and set back to 0
  */
  int get_post_count;

  int is_https; /* Used to update statistics in batch. */
 
  /* 
     The buffers for the POST method login and logoff are allocated only, 
     when a batch is configured to run login or logoff, respectively.
  */
  char* post_data_login;
  char* post_data_logoff;

  /* 
     Counter of the headers going in or our.  For the first header in request
     or response, the respective counter is zero, whereas next headers of the same
     request/response are positive numbers.

     Indication of the first header is used to collect statistics.
  */
  int hdrs_req;
  int hdrs_2xx;
  int hdrs_3xx;
  int hdrs_5xx;

  /* 
     Timestamp of a request sent. Used to calculate server 
     application response delay. 
  */
  unsigned long req_timestamp;

  /*
    Client-based statistics. Parallel to updating batch statistics, 
    the client-based statistics is updated.
  */
  stat_point st;

} client_context;

int hdrs_req (client_context* cctx);
void hdrs_req_inc (client_context* cctx);

int hdrs_2xx (client_context* cctx);
void hdrs_2xx_inc (client_context* cctx);

int hdrs_3xx (client_context* cctx);
void hdrs_3xx_inc (client_context* cctx);

int hdrs_5xx (client_context* cctx);
void hdrs_5xx_inc (client_context* cctx);

void hdrs_clear_all (client_context* cctx);
void hdrs_clear_non_req (client_context* cctx);
void hdrs_clear_non_2xx (client_context* cctx);
void hdrs_clear_non_3xx (client_context* cctx);
void hdrs_clear_non_5xx (client_context* cctx);

void stat_data_out_add (client_context* cctx, unsigned long bytes);
void stat_data_in_add (client_context* cctx, unsigned long bytes);
void stat_err_inc (client_context* cctx);
void stat_req_inc (client_context* cctx);
void stat_2xx_inc (client_context* cctx);
void stat_3xx_inc (client_context* cctx);
void stat_5xx_inc (client_context* cctx);

void stat_appl_delay_add (client_context* cctx, unsigned long resp_timestamp);
void stat_appl_delay_2xx_add (client_context* cctx, unsigned long resp_timestamp);

void dump_client (FILE* file, client_context* cctx);

/*
  Flag used to indicate, that no more loading is necessary.
  Time to dump the final statistics, clients table and exit.
  Set by SIGINT.
*/
extern int stop_loading;

#endif /*   CLIENT_H */
