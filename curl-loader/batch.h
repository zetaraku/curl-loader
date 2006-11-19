/*
*     batch.h
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


#ifndef BATCH_H
#define BATCH_H

#include <stddef.h>

#include <curl/curl.h>

#include "url.h"
#include "statistics.h"

#define BATCH_NAME_SIZE 32
#define POST_BUFFER_SIZE 64

struct client_context;

/**********************
  struct batch_context

  Batch is a group of clients with the same characteristics and loading
  behavior.

  The structure is used to keep all batch-relevant configuration and run-time
  information.
*/
typedef struct batch_context
{


  /*------------------------ GENERAL SECTION ------------------------------ */

   /* Some non-empty name of a batch load without empty spaces, tabs, etc */
  char batch_name[BATCH_NAME_SIZE];

  /* Number of clients (each client with its own IP-address) in the batch */
  int client_num;
  
   /* Name of the network interface to be used for loading, e.g. "eth0", "eth1:16" */
  char net_interface[16];

  /* CIDR netmask number from 0 to 32, like 16 or 24, etc. */
  int cidr_netmask;
  
  /* Minimal ip-address of a client in the batch (host order). */
  long ip_addr_min;
  
  /* Maximum ip-address of a client in the batch (host order).*/
  long ip_addr_max;
  
   /* 
      Number of cycles to repeat the urls downloads and afterwards sleeping 
      cycles. Zero means run it forever. 
   */
  long cycles_num;

  

  /*------------------------ LOGIN SECTION -------------------------------- */

  /* 
     Flag, whether to do authentication. If zero - don't do, else 
     do it. When zero, other fields of the login section are not relevant
     and should be commented out in the configuration file.
  */
  int do_login;

  /* Authentication login_username */
  char login_username[32];
 
  /* Authentication login_password */
  char login_password[32];
  
  /* 
     Either LOGIN_REQ_TYPE_GET_AND_POST or LOGIN_REQ_TYPE_POST
  */
  size_t login_req_type;

  /* The string to be used as the base for login post message */
  char login_post_str [POST_BUFFER_SIZE + 1];

   /* The url object for login. */
  url_context login_url;

  /* Whether to include login to cycling or not. */
  int login_cycling;



  /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */

   /* Whether to proceed with UAS, When zero, other fields of the UAS section 
      are not relevant and should be commented out in the configuration file.*/
  int do_uas;

  /* Number of total urls, should be more or equal to 1 */
  int uas_urls_num;
  



  /*------------------------LOGOFF SECTION ---------------------------------*/

  /* 
     Flag, whether to do logoff. If zero - don't do, else 
     do it. When zero, other fields of the logoff section are not relevant
     and should be commented out in the configuration file.
  */
  int do_logoff;

  /* 
     LOGOFF_REQ_TYPE_GET, 
     LOGOFF_REQ_TYPE_GET_AND_POST,
     LOGOFF_REQ_TYPE_POST
  */
  size_t logoff_req_type;

  /* The string to be used as the base for login post message */
  char logoff_post_str[POST_BUFFER_SIZE + 1];

  /* The url object for logoff. */
  url_context logoff_url;

  /* Whether to include login to cycling or not. */
  int logoff_cycling;


  /*------------------------- ASSISTING SECTION ----------------------------*/

   /* 
      Array of client handles of client_num size. Each handle means a single 
      curl connection 
   */
  //CURL **client_handles_array;

  /* Multiple handle for curl. Contains all curl handles of a batch */
  CURLM *multiple_handle;
  
   /* Assisting array of pointers to ip-addresses */
  char** ip_addr_array;

  /* Current parsing state. Used on reading and parsing conf-file. */ 
  size_t batch_init_state; 

  /* Common error buffer for all batch clients */
  char error_buffer[CURL_ERROR_SIZE];

  /* Array of all UAS url contexts */
  url_context* uas_url_ctx_array;

  /* Index of the parsed url in uas_url_ctx_array below.
     Further used by the storm-mode as the current url index. */
  int url_index;

  /* Array of all client contexts for the batch */
  struct client_context* cctx_array;

  /* Counter used mainly by smooth mode */
  int active_clients_count;


   /*--------------- STATISTICS  --------------------------------------------*/

  /* The file to be used for statistics output */
  FILE* statistics_file;

  /* Timestamp, when the loading started */
  unsigned long start_time; 

  /* The last timestamp */
  unsigned long last_measure;

  /* HTTP counters since the last measurements */
  stat_point http_delta;
  /* HTTP counters since the loading started */
  stat_point http_total;

  /* HTTPS counters since the last measurements */
  stat_point https_delta;
  /* HTTPS counters since the loading started */
  stat_point https_total;

} batch_context;


#endif /* BATCH_H */
