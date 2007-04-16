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
#include <netinet/in.h>

#include <curl/curl.h>

#include "timer_queue.h"
#include "url.h"
#include "statistics.h"

#define BATCH_NAME_SIZE 64
#define BATCH_NAME_EXTRA_SIZE 6
#define POST_BUFFER_SIZE 256

#define CUSTOM_HTTP_HDRS_MAX_NUM 16

enum post_str_usertype
{
    POST_STR_USERTYPE_NON_VALID = 0,
    POST_STR_USERTYPE_UNIQUE_USERS_AND_PASSWORDS,
    POST_STR_USERTYPE_UNIQUE_USERS_SAME_PASSWORD,
    POST_STR_USERTYPE_SINGLE_USER,
    POST_STR_USERTYPE_LOAD_USERS_FROM_FILE,
};

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

  /* Logfile <batch-name>.log */
  char batch_logfile[BATCH_NAME_SIZE+BATCH_NAME_EXTRA_SIZE];

  /* Statistics file <batch-name>.txt */
  char batch_statistics[BATCH_NAME_SIZE+BATCH_NAME_EXTRA_SIZE];

  /* Number of clients (each client with its own IP-address) in the batch */
  int client_num;
  
   /* Name of the network interface to be used for loading, e.g. "eth0", "eth1:16" */
  char net_interface[16];

  /* Flag: 0 means IPv4, 1 means IPv6 */ 
  int ipv6;
  
  /* Minimal IPv4-address of a client in the batch (host order). */
  long ip_addr_min;
  /* Maximum IPv4-address of a client in the batch (host order).*/
  long ip_addr_max;

  /* 
     CIDR netmask number from 0 to 128, like 16 or 24, etc. If the input netmask is
     a dotted IPv4 address, we convert it to CIDR by calculating number of 1 bits.
  */
  int cidr_netmask;

  /* "global", "host", "link", for IPV6 only "site" */
  char scope[16];

  /* Minimal IPv6-address of a client in the batch. */
  struct in6_addr ipv6_addr_min;

  /* Miximum IPv6-address of a client in the batch. */
  struct in6_addr ipv6_addr_max;

  
  
   /* 
      Number of cycles to repeat the urls downloads and afterwards sleeping 
      cycles. Zero means run it forever. 
   */
  long cycles_num;

   /* 
      Clients added per second for the loading start phase.
  */
  long clients_initial_inc;

   /* 
      User-agent string to appear in the HTTP 1/1 requests.
  */
  char user_agent[256];

  /* 
      Number of custom  HTTP headers in array.
  */
  size_t custom_http_hdrs_num;

    /* 
      The list of custom  HTTP headers.
  */
  struct curl_slist *custom_http_hdrs;


  /*------------------------ LOGIN SECTION -------------------------------- */

  /* 
     Flag, whether to do authentication. If zero - don't do, else 
     do it. When zero, other fields of the login section are not relevant
     and should be commented out in the configuration file.
  */
  int do_login;

  /* Whether to include login to cycling or not. */
  int login_cycling;

  /* Authentication login_username - depricated, moved to Login URL*/
 
  /* Authentication login_password - depricated, moved to Login URL */
  
  /* 
     Either LOGIN_REQ_TYPE_GET_AND_POST or LOGIN_REQ_TYPE_POST
  */
  size_t login_req_type;

  /* The string to be used as the base for login post message */
  char login_post_str [POST_BUFFER_SIZE + 1];

    /* The type of <login_post_str>. Valid types are: 
       POST_STR_USERTYPE_UNIQUE_USERS_AND_PASSWORDS, - like "user=%s%d&password=%s%d"
       POST_STR_USERTYPE_UNIQUE_USERS_SAME_PASSWORD, like "user=%s%d&password=%s"
       POST_STR_USERTYPE_SINGLE_USER,                            like "user=%s&password=%s"
       POST_STR_USERTYPE_LOAD_USERS_FROM_FILE, like "user=%s&password=%s" and login_credentials_file defined.
    */
  int login_post_str_usertype;

  /* 
     The file with strings like "user:password", where separator may be 
     ':', '@', '/' and ' ' (space) in line with RFC1738. The file may be created
     as a dump of DB tables of users and passwords.
  */
  char* login_credentials_file;

   /* The url object for login. */
  url_context login_url;




  /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */

   /* 
      Whether to proceed with UAS, When zero, other fields of the UAS section 
      are not relevant and should be commented out in the configuration file.
   */
  int do_uas;

  /* Number of total urls, should be more or equal to 1 */
  int uas_urls_num;
  
  /* Array of all UAS url contexts */
  url_context* uas_url_ctx_array;

  /* 
     Index of the parsed url in uas_url_ctx_array below.
     Further used by the storm-mode as the current url index. 
  */
  int url_index;


  /*------------------------LOGOFF SECTION ---------------------------------*/

  /* 
     Flag, whether to do logoff. If zero - don't do, else 
     do it. When zero, other fields of the logoff section are not relevant
     and should be commented out in the configuration file.
  */
  int do_logoff;

  /* Whether to include login to cycling or not. */
  int logoff_cycling;

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




  /*------------------------- ASSISTING SECTION ----------------------------*/

  /* Multiple handle for curl. Contains all curl handles of a batch */
  CURLM *multiple_handle;
  
   /* Assisting array of pointers to ip-addresses */
  char** ip_addr_array;

  /* Current parsing state. Used on reading and parsing conf-file. */ 
  size_t batch_init_state; 

  /* Common error buffer for all batch clients */
  char error_buffer[CURL_ERROR_SIZE];

  /* Array of all client contexts for the batch */
  struct client_context* cctx_array;

  /* Counter used mainly by smooth mode: active clients */
  int active_clients_count;

  /* 
     Whether to do gradual increase of loading clients to prevent
     a simulteneous huge flow of client requests to server.
  */
  int do_client_num_gradual_increase;

  /* 
	 Number of already scheduled clients. Used to schedule new
	 clients in a gradual fashion, when <clients_initial_inc> is positive. 
  */
  int clients_initial_running_num;

  /*  Waiting queue to keep interleev timeouts in smooth mode */
  timer_queue* waiting_queue;


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

  /* Operations statistics */
  op_stat_point op_delta;
  op_stat_point op_total;

} batch_context;




#endif /* BATCH_H */
