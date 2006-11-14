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


#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdio.h>
#include <linux/types.h>
/*
  HTTP/S Statistics
*/
typedef struct stat_point
{
  unsigned long long data_in;
  unsigned long long data_out;

  u_long requests; /* number of requests issued */
  u_long resp_redirs;  /* number of 3xx redirections */
  u_long resp_oks;       /* number of 2xx responses */
  u_long resp_serv_errs; /* number of 5xx responses */
  u_long other_errs; /* Errors of resolving, connecting, internal errors, etc. */

  int appl_delay_points;
  u_long  appl_delay; /* Average delay in msec between request and response */

  int appl_delay_2xx_points;
  u_long  appl_delay_2xx; /* Average delay in msec between request and 2xx-OK response */

} stat_point;

void stat_point_add (stat_point* left, stat_point* right);
void stat_point_reset (stat_point* point);

unsigned long get_tick_count ();

struct client_context;
struct batch_context;

void dump_final_statistics (struct client_context* cctx);
void dump_intermediate_statistics (
                                   int clients, 
                                   unsigned long period,  
                                   stat_point *http, 
                                   stat_point *https);
void dump_intermediate_and_advance_total_statistics(struct batch_context* bctx);

void print_statistics_header (FILE* file);

#endif /* STATISTICS_H */
