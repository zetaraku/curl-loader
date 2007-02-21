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
#include <stdio.h>

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
*
* Return Code/Output - On Success - 0, on Error -1
********************************************************************************/
int add_secondary_ip_to_device (const char*const device,
                                const char*const ip_slash_mask);

/*******************************************************************************
* Function name - add_secondary_ip_addrs
*
* Description - Adds all secondary IPv4 addresses from array to network interface
* Input -       *interface - network device name as linux sees it, like "eth0"
*               addr_number - number of addresses to add
*               *addresses - array of strings of ipv4 addresses
*               netmask - CIDR notation netmask
* Return Code/Output - On Success - 0, on Error -1
********************************************************************************/
int add_secondary_ip_addrs (const char*const interface, 
                            int addr_number, 
                            const char**const addresses,
                            int netmask);

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



extern int stop_loading;


#endif /* LOADER_H */
