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

/*
  struct batch_context - is used to keep all batch-relevant information for loading.
*/
typedef struct batch_context
{
  char batch_name[BATCH_NAME_SIZE];  /* Some non-empty name of a batch load 
                                        without empty spaces, tabs, etc */
  
  char net_interface[16]; /* Name of the network interface to be used for 
                             loading, e.g. "eth0", "eth1:16" */

  int client_num;  /* Number of clients (each client with its own IP-address) 
                      in the batch */
    
  CURL **client_handles_array; /* Array of client handles of client_num size. 
                                  Each handle means a single curl connection */

  CURLM *multiple_handle; /* Multiple handle for curl. Contains all 
                             curl handles of a batch */
   
  int cidr_netmask;  /* CIDR netmask number from 0 to 32, like 16 or 24, etc */
    
  long ip_addr_min; /* Minimal ip-address of a client in the batch (host order)*/

  long ip_addr_max; /* Maximum ip-address of a client in the batch (host order)*/

  char username[32]; /* Authentication username to which will be appended 
                        client number, e.g. username "robert" to be used for from 
                        "robert1" to "robertN". When not relevant - "NO" */
 
  char password[32];  /* Authentication password to which will be appended 
                         client number, e.g. password "stam" to be used 
                         for from "stam1" to "stamN" When not relevant - "NO" */
  
  int do_auth; /* Flag, whether to do authentication. If zero - don't do, else 
                  do it. When zero, username and password are not relevant. */

  long repeat_cycles_num; /* Number of cycles to repeat the urls downloads and 
                             afterwards sleeping cycles. Zero means run it forever */
   
  int urls_num; /* Number of total urls, should be more or equal to 1 */

  int url_index; /* Index of the parsed url in url_ctx_arr below.
                  Further used by the storm-mode as the current url index. */

  url_context* url_ctx_arr; /* Array of all url contexts */

  char** ip_addr_array; /* Assisting array of pointers to ip-addresses */

  int batch_init_state; /* Parsing state used upon batch initialization */ 

  char error_buffer[CURL_ERROR_SIZE]; /* common buffer for all batch clients */

  
  /*---------------Fields specific for the smooth mode loading --------------------*/

  int curl_handlers_count; /* Counter to used by smooth-loading mode */

  u_long start_time; 
  u_long last_measure;

  stat_point http_delta;
  stat_point http_total;

  stat_point https_delta;
  stat_point https_total;

} batch_context;


#endif /* BATCH_H */
