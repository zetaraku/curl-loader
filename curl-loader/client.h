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
  Client states for smooth mode.
  TODO: Transit to more general states below.
*/
enum
  {
    STATE_RUNNING = 0, /* Thus, calloc set the state automatically */
    STATE_LAST_URL,
    STATE_FINISHED_OK,
    STATE_PLACE_FILLING,
    STATE_ERROR, // 4
  };

enum cstate
  {
    CSTATE_INIT = 0, /* Thus, calloc set the state automatically */
  
    /* Operational states */
    CSTATE_LOGIN,
    CSTATE_UAS,
    CSTATE_LOGOFF,

    /* Completion states */
    CSTATE_ERROR, // also 4 as STATE_ERROR
    CSTATE_FINISHED_OK,
  };

/* Forward declarations */
struct batch_context;

/******** struct client_context  ***********
  Client context is passed to curl handle and returned back to the tracing
  function and used for output to the logfile.
  Client context is also heavily used for login/logoff, using POST-buffers,
  and for smooth-mode.
*/
typedef struct client_context
{
  char client_name[CLIENT_NAME_LEN]; /* Current cycle number SP client sequence
                                        number SP IP-address */
 
  long cycle_num;  /* Current cycle number. Useful for multiple runs. */                          

  FILE* file_output;  /* The file to be used for output for this particular 
                             client (normally used a file-per-batch logging strategy) */

  struct batch_context* bctx; /* Batch context, which is running the client.
                               Used for getting configs, like urls and 
                               writing back statistics. */

  size_t client_index; /* Index of the client within its batch. */

  size_t url_curr_index; /* Index of the currently used url. */

  int client_state; /* Current state of the client. */

  int is_https; /* Used to update statistics in batch. */
 
  /* 
     The buffers for the POST method login and logoff 
     are allocated only, when a batch is configured to run authentication.
  */
  char* post_data_login;
  char* post_data_logoff;

} client_context;

#endif /*   CLIENT_H */
