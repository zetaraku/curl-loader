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
struct client_context;
struct batch_context;
struct stat_point;

/* Common loading functions */
int single_handle_setup (struct client_context*const c_ctx, size_t url_num, 
                         long cycle_number, char* post_buff);

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


/* Smooth load functions. */
int mget_url_storm (struct batch_context* bctx);
int user_activity_storm (struct client_context*const cdata);
int authenticate_clients_storm (struct client_context* cctx);


/* Smooth load functions. */
int mget_url_smooth (struct batch_context* bctx);
int mperform_smooth (struct batch_context* bctx, int* still_running);
int user_activity_smooth (struct client_context* cctx);

int load_next_step (struct client_context* cctx);
unsigned long get_tick_count (void);
void dump_statistics (unsigned long period,  struct stat_point *http, 
                      struct stat_point *https);
void dump_stat_workhorse (int clients, unsigned long period,  
                         struct stat_point *http, struct stat_point *https);

#endif /* LOADER_H */
