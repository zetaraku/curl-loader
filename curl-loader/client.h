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

#define CLIENT_NAME_LEN 32

/*
****************   cstate   ************************************
  States of a virtual client. 
*/
enum cstate
  {
    CSTATE_ERROR = -1, /* Completion state */
    CSTATE_INIT = 0, /* state 0, thus, calloc set the state automatically */
  
    /* Operational states */
    CSTATE_LOGIN, /* cycling state, if cycling enabled */
    CSTATE_UAS_CYCLING, /* cycling state */
    CSTATE_LOGOFF, /* cycling state, if cycling enabled */

    CSTATE_FINISHED_OK, /* Completion state */
  };

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
   /*
      <Current Cycle Num> SP <Client Seq Num> SP <Client IP-address> 
   */
  char client_name[CLIENT_NAME_LEN];
 
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

  int client_state; /* Current state of the client. */

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

} client_context;

#endif /*   CLIENT_H */
