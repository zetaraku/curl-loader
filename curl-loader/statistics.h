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

/*
  stat_point - is the object used to keep all statistics
  counters together and collect statistics.
*/
typedef struct stat_point
{
  unsigned long long data_in; /* Inbound bytes number */
  unsigned long long data_out; /* Outbound bytes number */

  unsigned long requests; /* Number of requests issued */
  unsigned long resp_redirs;  /* Number of 3xx redirections */
  unsigned long resp_oks;       /* Number of 2xx responses */
  unsigned long resp_serv_errs; /* Number of 5xx responses */
  unsigned long other_errs; /* Errors of resolving, connecting, internal errors, etc. */

  int appl_delay_points; /* Num of data points used to calculate average 
                            application delay */
  unsigned long  appl_delay; /* Average delay in msec between request and response */

  int appl_delay_2xx_points; /* Num of data points used to calculate average 
                                application delay for 2xx-OK responses*/
  unsigned long  appl_delay_2xx; /* Average delay in msec between request and 2xx-OK response */

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
