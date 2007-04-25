/* 
*     loader.c
*
* 2006 - 2007 Copyright (c) 
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

#ifndef LOADER_H
#define LOADER_H

#include <stddef.h>
#include <stdio.h>

#include "timer_queue.h"

#define BATCHES_MAX_NUM 64


#define DEFAULT_SMOOTH_URL_COMPLETION_TIME 6.0
#define TIME_RECALCULATION_CYCLES_NUM 10
#define TIME_RECALCULATION_MSG_NUM 100
#define PERIODIC_TIMERS_NUMBER 2
#define LOGFILE_TEST_TIMER_PERIOD 10 /* once in 10 seconds */


/* forward declarations */
struct url_context;
struct client_context;
struct batch_context;
struct stat_point;
struct timer_node;

/*---------  Common loading functions ----------------*/

int test_environment (struct batch_context* bctx);

/****************************************************************************************
* Function name - setup_curl_handle
*
* Description - Setup for a single curl handle (client): removes a handle from multi-handle, 
*               and inits it, using setup_curl_handle_init () function, and, finally, 
*               adds the handle back to the multi-handle.
*
* Input -       *cctx - pointer to client context, which contains CURL handle pointer;
*               *url_ctx - pointer to url-context, containing all url-related information;
*               cycle_number - current number of loading cycle, passing here for storming mode;
*               post_method - when 'true', POST method is used instead of the default GET
*
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int setup_curl_handle (struct client_context*const cctx, 
                       struct url_context* url_ctx, 
                       long cycle_number,
                       int post_method);

/****************************************************************************************
* Function name - setup_curl_handle_init
*
* Description - Resets client context kept CURL handle and inits it locally and using 
*               setup_curl_handle_appl () function for the application-specific 
*               (HTTP/FTP) initialization.
*
* Input -       *cctx - pointer to client context, which contains CURL handle pointer;
*               *url_ctx - pointer to url-context, containing all url-related information;
*               cycle_number - current number of loading cycle, passing here for storming mode;
*               post_method - when 'true', POST method is used instead of the default GET
*
* Return Code/Output - On Success - 0, on Error -1
***************************************************************************************
*/
int setup_curl_handle_init (
    struct client_context*const cctx,
    struct url_context* url_ctx,
    long cycle_number,
    int post_method);

/*******************************************************************************
* Function name - add_secondary_ip_to_device
*
* Description - Adds a secondary IPv4 address to a valid networking device.
* Input -       *device - network device name as linux sees it, like "eth0"
*               *ip_slash_mask - string in the form of ipv4/mask, 
*                                e.g. "192.168.0.1/24"
*              *scope - network scope of the addresses: "global", "site" (IPv6), "link", "host"
*
* Return Code/Output - On Success - 0, on Error -1
********************************************************************************/
int add_secondary_ip_to_device (const char*const device,
                                const char*const ip_slash_mask, 
                                char* scope);

/*******************************************************************************
* Function name - add_secondary_ip_addrs
*
* Description - Adds all secondary IPv4 addresses from array to network interface
* Input -       *interface - network device name as linux sees it, like "eth0"
*               addr_number - number of addresses to add
*               *addresses - array of strings of ipv4 addresses
*               netmask - CIDR notation netmask
*               scope - address scope, like "global", "host", "link", "site"
* Return Code/Output - On Success - 0, on Error -1
********************************************************************************/
int add_secondary_ip_addrs (const char*const interface, 
                            int addr_number, 
                            const char**const addresses,
                            int netmask,
                            char* scope);

/*******************************************************************************
* Function name - parse_config_file
*
* Description - Parses configuration file and fills batch contexts in array
*
* Output -       *bctx_array - array of batch contexts to be filled on parsing
* Input-         bctx_array_size - number of bctx contexts in <bctx_array>.
*
* Return Code/Output - On Success - number of batches >=1, on Error -1
********************************************************************************/
int parse_config_file (char* const filename, 
                       struct batch_context* bctx_array, 
                       size_t bctx_array_size);

/*******************************************************************************
* Function name - rewind_logfile_above_maxsize
*
* Description - Rewinds logfile, when file reaches maximum allowed size
* Input-        filepointer - pointer to the open logfile
* Return Code/Output - On success - 0, on errors -1
********************************************************************************/
int rewind_logfile_above_maxsize (FILE* filepointer);


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
int handle_gradual_increase_clients_num_timer  (struct timer_node* timer_node, 
                                                void* pvoid_param, 
                                                unsigned long ulong_param);

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
int handle_logfile_rewinding_timer  (struct timer_node* timer_node, 
                                     void* pvoid_param, 
                                     unsigned long ulong_param);

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
int handle_screen_input_timer  (struct timer_node* timer_node, 
                                void* pvoid_param, 
                                unsigned long ulong_param);

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
int handle_cctx_timer (struct timer_node* timer_node, 
                       void* pvoid_param,
                       unsigned long ulong_param);

/****************************************************************************************
 * Function name - pending_active_and_waiting_clients_num
 *
 * Description - Returns the sum of active and waiting (for load scheduling) clients
 *
 * Input -       *bctx - pointer to the batch context
 *
 * Return Code/Output - Sum of active and waiting (for load scheduling) clients
 ****************************************************************************************/
int pending_active_and_waiting_clients_num (struct batch_context* bctx);


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
int client_remove_from_load (struct batch_context* bctx, struct client_context* cctx);


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
int client_add_to_load (struct batch_context* bctx, struct client_context* cctx);



/****************************************************************************************
 * Function name - load_next_step
 *
 * Description - Called at initialization and further after url-fetch completion 
 *               indication (that may be an error status as well). Either sets 
 *               to client the next url to load, or marks the being at completion state: 
 *               CSTATE_ERROR or CSTATE_FINISHED_OK.
 *
 * Input -       *cctx - pointer to the client context
 *                   now_time -  current timestamp in msec
 *
 *Input/Output -  sched_now - when true, the client is scheduled right now without timer queue.
 *
 * Return Code/Output - CSTATE enumeration with the state of loading
 ****************************************************************************************/
int load_next_step (struct client_context* cctx,
                    unsigned long now_time,
                    int* sched_now);


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
int dispatch_expired_timers (struct batch_context* bctx, unsigned long now_time);

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
int add_loading_clients (struct batch_context* bctx);

/****************************************************************************************
 * Function name - add_loading_clients_num
 *
 * Description - Adding a number of clients to load
 *
 * Input -       *bctx - pointer to the batch of contexts
 *                  add_number - number of clients to add to load
 * Return Code/Output - On Success - 0, on error  - (-1)
 ****************************************************************************************/
int add_loading_clients_num (struct batch_context* bctx, int add_number);

typedef int (*load_state_func)  (struct client_context* cctx, unsigned long *wait_msec);

/* 
   Table of loading functions in order to call an appropiate for 
   a certain state loading function just by using the state number
   as an index. As we are starting our states from (-1),
   the actual call will be with (state + 1) as an index, used 
   in load_next_step ().
*/
extern const load_state_func load_state_func_table [];

/****************************************************************************************
 * Function name - alloc_init_timer_waiting_queue
 *
 * Description - Allocates and initializes timer waiting queue
 *
 *Input               size -  maximum possible size of the queue
 *Input/Output   **wq - waiting queue to be allocated, initialized and returned back
 *
 * Return Code/Output - On success -0, on error -1
 ****************************************************************************************/
int alloc_init_timer_waiting_queue (size_t size, timer_queue** wq);

/***************************************************************************
 * Function name - cancel_periodic_timers
 *
 * Description - Cancels scheduled periodic timers
 *
 *Input               *twq - pointer to timer waiting queue
 * Return Code/Output - On success -0, on error -1
 ****************************************************************************/
int cancel_periodic_timers (timer_queue* twq);

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
int init_timers_and_add_initial_clients_to_load (struct batch_context* bctx,
                                                 unsigned long now_time);


/* ------------- Hyper-mode loading  function ----------------*/

/****************************************************************************************
* Function name - user_activity_hyper
*
* Description - Simulates user-activities, like login, uas, logoff, using HYPER-MODE
* Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int user_activity_hyper (struct client_context*const cctx_array);

/* ------------- Storm-mode loading  function ----------------*/

/****************************************************************************************
* Function name - user_activity_storm
*
* Description - Simulates user-activities, like login, uas, logoff, using STORM-MODE
* Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int user_activity_storm (struct client_context*const cctx_array);

/*-------------- Smooth-mode loading function ----------------*/

/****************************************************************************************
* Function name - user_activity_smooth
*
* Description - Simulates user-activities, like login, uas, logoff, using SMOOTH-MODE
* Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int user_activity_smooth (struct client_context*const cctx_array);

int handle_cctx_timer (struct timer_node*, void*, unsigned long);

extern int stop_loading;


#endif /* LOADER_H */
