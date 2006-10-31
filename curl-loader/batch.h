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

#include <curl/curl.h>

#define BATCH_NAME_SIZE 32
#define POST_BUFFER_SIZE 64

/*
  Statistics used by the smooth mode.
*/
typedef struct stat_point
{
  unsigned long long data_in;
  unsigned long long data_out;
  unsigned long long requests;
  unsigned long long errors;
} stat_point;

void stat_point_add (stat_point* left, stat_point* right);
void stat_point_reset (stat_point* point);



typedef struct url_context
{
  char* url_str; /* String of url */

  float url_completion_time;  /* Maximum time given to all clients in the 
                                  batch to accomplish fetching. */

  int url_interleave_time; /* Client sleeping intervals between fetching 
                               the urls. */
} url_context;



/**********************
  struct batch_context

  Batch is a group of clients with the same characteristics and loading
  behavior.

  The structure is used to keep all batch-relevant information for loading.
*/
typedef struct batch_context
{


  /*------------------------ GENERAL SECTION ------------------------------ */

  char batch_name[BATCH_NAME_SIZE];  /* Some non-empty name of a batch load 
                                        without empty spaces, tabs, etc */

  int client_num;  /* Number of clients (each client with its own IP-address) 
                      in the batch */
  
  char net_interface[16]; /* Name of the network interface to be used for 
                             loading, e.g. "eth0", "eth1:16" */

  int cidr_netmask;  /* CIDR netmask number from 0 to 32, like 16 or 24, etc. */
  
  long ip_addr_min; /* Minimal ip-address of a client in the batch (host order). */
  
  long ip_addr_max; /* Maximum ip-address of a client in the batch (host order).*/
  
  long cycles_num; /* Number of cycles to repeat the urls downloads and 
                      afterwards sleeping cycles. Zero means run it forever */

  

  /*------------------------ LOGIN SECTION -------------------------------- */

  int do_login; /* Flag, whether to do authentication. If zero - don't do, else 
                  do it. When zero, username, password, etc are not relevant. */

  char login_username[32]; /* Authentication login_username */
 
  char login_password[32];  /* Authentication login_password */
  
  size_t login_req_type; /* either 
                            LOGIN_REQ_TYPE_GET_AND_POST (1) or 
                            LOGIN_REQ_TYPE_POST (2) 
                         */

  /* The string to be used as the base for post message */
  char login_post_str [POST_BUFFER_SIZE + 1];

  url_context login_url; /* The url for login. */

  int login_cycling; /* Whether to include login to cycling or not. */



  /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */


  int do_uas ; /* Whether to proceed with UAS */

  int uas_urls_num; /* Number of total urls, should be more or equal to 1 */
  



  /*------------------------LOGOFF SECTION ---------------------------------*/

  int do_logoff;

  size_t logoff_req_type; /* 
                             LOGOFF_REQ_TYPE_GET (1), 
                             LOGOFF_REQ_TYPE_GET_AND_POST (2),
                             LOGOFF_REQ_TYPE_POST (3) 
                          */

  char logoff_post_str[POST_BUFFER_SIZE + 1];

  url_context logoff_url;

  int logoff_cycling;


  /*------------------------- ASSISTING SECTION ----------------------------*/

  CURL **client_handles_array; /* Array of client handles of client_num size. 
                                  Each handle means a single curl connection */

  CURLM *multiple_handle; /* Multiple handle for curl. Contains all 
                             curl handles of a batch */
  
  char** ip_addr_array; /* Assisting array of pointers to ip-addresses */

  size_t batch_init_state; /* Current parsing state. Used on reading and 
                              parsing conf-file. */ 

  char error_buffer[CURL_ERROR_SIZE]; /* common buffer for all batch clients */

  url_context* uas_url_ctx_array; /* Array of all url contexts */

  int url_index; /* Index of the parsed url in uas_url_ctx_array below.
                  Further used by the storm-mode as the current url index. */ 



  /*---------------SMOOTH-mode specific  --------------------*/

  int curl_handlers_count; /* Counter used by smooth-loading mode */

  u_long start_time; 
  u_long last_measure;

  stat_point http_delta;
  stat_point http_total;

  stat_point https_delta;
  stat_point https_total;

} batch_context;


#endif /* BATCH_H */
