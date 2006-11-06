/* 
*     loader.c
*
* 2006 Copyright (c) 
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

#define BATCHES_MAX_NUM 64
#define IPADDR_STR_SIZE 16

#define POST_LOGIN_BUF_SIZE 128
#define POST_LOGOFF_BUF_SIZE 64

/* forward declarations */
struct url_context;
struct client_context;
struct batch_context;
struct stat_point;

/*---------  Common loading functions ----------------*/



/*
  Setup for a single curl handle (client): removes a handle from multi-handle, 
  resets the handle, inits it, and, finally, adds the handle back to the
  multi-handle.

  <ctx> - pointer to the client context;
  <url_index> - either URL_INDEX_LOGIN_URL, URL_INDEX_LOGOFF_URL or
                       some number eq or above URL_INDEX_UAS_URL_START;
  <cycle_number> - used in storming mode;
  <post_method> - when 'true', POST method is used instead of the default GET
*/
int setup_curl_handle (
                         struct client_context*const cctx, 
                         struct url_context* url_ctx, 
                         long cycle_number,
                         int post_method);

int add_secondary_ip_to_device (const char*const device,
                            const char*const ip_slash_mask);

int add_secondary_ip_addrs (
                            const char*const interface, 
                            int addr_number, 
                            const char**const addresses,
                            int netmask);

int parse_config_file (char* const filename, 
                       struct batch_context* bctx_array, 
                       size_t bctx_array_size);

int create_batch_data_array (char*const input_string, 
                             struct batch_context*const bctx);


/* ------------- Storm loading  functions ----------------*/

int user_activity_storm (struct client_context*const cdata);

/*-------------- Smooth loading functions ----------------*/

int user_activity_smooth (struct client_context* cctx);


#endif /* LOADER_H */
