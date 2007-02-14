/*
*     statistics.c
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

// must be first include
#include "fdsetsize.h"

#include <errno.h>
#include <stdlib.h>

#include "batch.h"
#include "client.h"

#include "statistics.h"

#define SECURE_APPL_STR "HTTPS/FTPS"
#define UNSECURE_APPL_STR "HTTP/FTP"

static void 
dump_snapshot_interval_and_advance_total_statistics (
                                                    struct batch_context* bctx,
                                                    unsigned long now_time);

static void dump_statistics (
                             unsigned long period, 
                             stat_point *http, 
                             stat_point *https);

static void print_statistics_footer (FILE* file);

static void print_statistics_data (
                                   FILE* file, 
                                   unsigned long timestamp,
                                   char* prot,
                                   long clients_num, 
                                   stat_point *sd,
                                   unsigned long period);

static void dump_stat_to_screen (
                                 char* protocol, 
                                 stat_point* sd, 
                                 unsigned long period);

static void dump_clients (client_context* cctx_array);

void stat_point_add (stat_point* left, stat_point* right)
{
  if (!left || !right)
    return;

  left->data_in += right->data_in;
  left->data_out += right->data_out;
  
  left->requests += right->requests;
  left->resp_oks += right->resp_oks;
  left->resp_redirs += right->resp_redirs;
  left->resp_cl_errs += right->resp_cl_errs;
  left->resp_serv_errs += right->resp_serv_errs;
  left->other_errs += right->other_errs;
  
  const int total_points = left->appl_delay_points + right->appl_delay_points;

  if (total_points > 0)
    {
      left->appl_delay = (left->appl_delay * left->appl_delay_points  + 
                          right->appl_delay * right->appl_delay_points) / total_points;

      left->appl_delay_points = total_points;
    }
  else
      left->appl_delay = 0;

   const int total_points_2xx = left->appl_delay_2xx_points + right->appl_delay_2xx_points;

  if (total_points_2xx > 0)
    {
      left->appl_delay_2xx = (left->appl_delay_2xx * left->appl_delay_2xx_points  + 
                          right->appl_delay_2xx * right->appl_delay_2xx_points) / total_points_2xx;
      left->appl_delay_2xx_points = total_points_2xx;
    }
  else
      left->appl_delay_2xx = 0;
}

void stat_point_reset (stat_point* p)
{
  if (!p)
    return;

  p->data_in = p->data_out = 0;
  p->requests = p->resp_redirs = p->resp_oks = p->resp_cl_errs = 
      p->resp_serv_errs = p->other_errs = 0;

  p->appl_delay_points = 0;
  p->appl_delay = 0;

  p->appl_delay_2xx_points = 0;
  p->appl_delay_2xx = 0;
}


/****************************************************************************************
* Function name - get_tick_count
*
* Description - Delivers timestamp in milliseconds.
*
* Return Code/Output - timestamp in milliseconds or -1 on errors
****************************************************************************************/
unsigned long get_tick_count ()
{
  struct timeval  tval;

  if (!gettimeofday (&tval, NULL) == -1)
    {
      fprintf(stderr, "%s - gettimeofday () failed with errno %d.\n", 
              __func__, errno);
      exit (1);
    }
  return tval.tv_sec * 1000 + (tval.tv_usec / 1000);
}


/****************************************************************************************
* Function name - dump_final_statistics
*
* Description - Dumps final statistics counters to stderr and statistics file using 
*                     print_snapshot_interval_statistics and print_statistics_* functions as well as calls
*                     dump_clients () to dump the clients table.
* Input -       *cctx - pointer to client context, where the decision to complete loading 
*                     (and dump) has been made. 
* Return Code/Output - None
****************************************************************************************/
void dump_final_statistics (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;
  unsigned long now = get_tick_count();

  print_snapshot_interval_statistics (
		pending_active_and_waiting_clients_num (bctx),
		now - bctx->last_measure,
		&bctx->http_delta,  
		&bctx->https_delta);

  stat_point_add (&bctx->http_total, &bctx->http_delta);
  stat_point_add (&bctx->https_total, &bctx->https_delta);  
    
  fprintf(stderr,"===========================================\n");
  fprintf(stderr,"End of test:\n"); 
  fprintf(stderr,"===========================================\n");
  
  now = get_tick_count();
  
  dump_statistics ((now - bctx->start_time)/ 1000, 
                   &bctx->http_total,  
                   &bctx->https_total);

  if (bctx->statistics_file)
    {

      print_statistics_footer (bctx->statistics_file);
      print_statistics_header (bctx->statistics_file);

      const unsigned long loading_time = (now - bctx->start_time > 0) ? 
        (now - bctx->start_time) : 1;
 
      print_statistics_data (
				bctx->statistics_file,
				loading_time/1000,
				UNSECURE_APPL_STR,
				pending_active_and_waiting_clients_num (bctx),
				&bctx->http_total,
				loading_time);
			
      print_statistics_data (
				bctx->statistics_file, 
				loading_time/1000,
				SECURE_APPL_STR,
				pending_active_and_waiting_clients_num (bctx),
				&bctx->https_delta,
				loading_time);
    }

  dump_clients (cctx);
}

/****************************************************************************************
* Function name - dump_snapshot_interval and up to the interval time summary statistics
*
* Description - Dumps summary statistics since the start of load
* Input -       *bctx - pointer to batch structure
*                   now -  current time in msec since epoch
*
* Return Code/Output - None
****************************************************************************************/
void dump_snapshot_interval (batch_context* bctx, unsigned long now)
{
  fprintf(stderr, "\033[2J");
  dump_snapshot_interval_and_advance_total_statistics(bctx, now);

  fprintf(stderr,"-------------------------------------------\n");
  fprintf(stderr,"Summary statistics of the test since loading started:\n"); 
  
  dump_statistics ((now - bctx->start_time)/ 1000, 
                   &bctx->http_total,  
                   &bctx->https_total);

  fprintf(stderr,"===========================================\n\n");
}

/****************************************************************************************
* Function name - print_snapshot_interval_statistics
*
* Description - Dumps final statistics counters to stderr and statistics file using 
*                     print_snapshot_interval_statistics and print_statistics_* functions as well as calls
*                     dump_clients () to dump the clients table.
* Input -       clients - number of active clients
*                     period - latest time period in milliseconds
*                     *http - pointer to the HTTP collected statistics to output
*                     *https - pointer to the HTTPS collected statistics to output
* Return Code/Output - None
****************************************************************************************/
void print_snapshot_interval_statistics (int clients, 
                                   unsigned long period,  
                                   stat_point *http, 
                                   stat_point *https)
{
  period /= 1000;
  if (period == 0)
    {
      period = 1;
    }

  fprintf(stderr, "Loading clients: %d, Snapshot Interval %d (sec)\n", (int) clients, (int) period);
  dump_stat_to_screen (UNSECURE_APPL_STR, http, period);
  dump_stat_to_screen (SECURE_APPL_STR, https, period);
}


/****************************************************************************************
* Function name - dump_snapshot_interval_and_advance_total_statistics
*
* Description - Dumps snapshot_interval statistics for the latest loading time period and adds
*                     this statistics to the total loading counters 
* Input -       *bctx - pointer to batch context
*                   now_time - current time in msec since the epoch
*
* Return Code/Output - None
****************************************************************************************/
void dump_snapshot_interval_and_advance_total_statistics(
                                                    batch_context* bctx,
                                                    unsigned long now_time)
{
  //const unsigned long now_time = get_tick_count ();
  const unsigned long delta_time = now_time - bctx->last_measure;

  if (stop_loading)
    {
      dump_final_statistics (bctx->cctx_array);
      exit (1); 
    }

  fprintf(stderr,"===========================================\n");
  fprintf(stderr,"The latest time interval statistics:\n"); 
  //  fprintf(stderr,"===========================================\n");

  print_snapshot_interval_statistics(
                                     pending_active_and_waiting_clients_num (bctx),
                                     delta_time, 
                                     &bctx->http_delta,  
                                     &bctx->https_delta);

  if (bctx->statistics_file)
    {
      const unsigned long timestamp_sec =  (now_time - bctx->start_time) / 1000;

      print_statistics_data (
                             bctx->statistics_file,
                             timestamp_sec,
                             UNSECURE_APPL_STR,
                             pending_active_and_waiting_clients_num (bctx),
                             &bctx->http_delta,
                             delta_time ? delta_time : 1);
    
      print_statistics_data (
                             bctx->statistics_file, 
                             timestamp_sec,
                             SECURE_APPL_STR, 
                             pending_active_and_waiting_clients_num (bctx),
                             &bctx->https_delta,
                             delta_time ? delta_time : 1);
    }

  stat_point_add (&bctx->http_total, &bctx->http_delta);
  stat_point_add (&bctx->https_total, &bctx->https_delta);

  stat_point_reset (&bctx->http_delta); 
  stat_point_reset (&bctx->https_delta);
        
  bctx->last_measure = now_time;
}

static void dump_statistics (
                             unsigned long period,  
                             stat_point *http, 
                             stat_point *https)
{
  if (period == 0)
    {
      fprintf(stderr,
              "%s - less than 1 second duration test without statistics.\n",
              __func__);
      return;
    } 
  
  fprintf(stderr, "Test took %d seconds\n", (int) period);
  dump_stat_to_screen (UNSECURE_APPL_STR, http, period);
  dump_stat_to_screen (SECURE_APPL_STR, https, period);
}

static void dump_stat_to_screen (
                                 char* protocol, 
                                 stat_point* sd, 
                                 unsigned long period)
{
  fprintf(stderr, "%s-Req:%ld,2xx:%ld,3xx:%ld,4xx:%ld,5xx:%ld,Err:%ld,"
          "Delay:%ld,Delay-2xx:%ld,Thr-in:%lld(b/s),Thr-out:%lld(b/s)\n",
          protocol, sd->requests, sd->resp_oks, sd->resp_redirs, sd->resp_cl_errs,
          sd->resp_serv_errs, sd->other_errs, sd->appl_delay, sd->appl_delay_2xx,
          sd->data_in/period, sd->data_out/period);

    //fprintf (stderr, "Appl-Delay-Points %d, Appl-Delay-2xx-Points %d \n", 
  //         sd->appl_delay_points, sd->appl_delay_2xx_points);
}

/****************************************************************************************
* Function name - print_statistics_header
*
* Description - Prints to a file header for statistics numbers, describing counters
* Input -       *file - open file pointer
* Return Code/Output - None
****************************************************************************************/
void print_statistics_header (FILE* file)
{
    fprintf (file, 
             "Time,Appl,Clients,Req,Rsp-OK,Redirs,Rsp-Cl-Err,Rsp-Serv-Err,"
             "Err,Delay,Delay-2xx,Thr-In,Thr-Out\n");
    fflush (file);
}

/****************************************************************************************
* Function name - print_statistics_footer
*
* Description - Prints to a file separation string between the snapshot_interval statistics and 
*                     the final statistics number for the total loading process
* Input -       *file - open file pointer
* Return Code/Output - None
****************************************************************************************/
static void print_statistics_footer (FILE* file)
{
    fprintf (file, "*, *, *, *, *, *, *, *, *, *, *, *, *\n");
    fflush (file);
}

static void print_statistics_data (FILE* file, 
                                   unsigned long timestamp,
                                   char* prot,
                                   long clients_num, 
                                   stat_point *sd,
                                   unsigned long period)
{
    period /= 1000;
    if (period == 0)
      {
        period = 1;
      }

    fprintf (file, "%ld, %s, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %lld, %lld\n",
             timestamp, prot, clients_num, sd->requests, sd->resp_oks,
             sd->resp_redirs, sd->resp_cl_errs, sd->resp_serv_errs, 
             sd->other_errs, sd->appl_delay, sd->appl_delay_2xx, 
             sd->data_in/period, sd->data_out/period);
    fflush (file);
}

static void dump_clients (client_context* cctx_array)
{
  batch_context* bctx = cctx_array->bctx;
  char client_table_filename[BATCH_NAME_SIZE+4];
  FILE* ct_file = NULL;
  int i;

  /*
    Init batch logfile for the batch clients output 
  */
  sprintf (client_table_filename, "%s.ctx", bctx->batch_name);
  
  if (!(ct_file = fopen(client_table_filename, "w")))
    {
      fprintf (stderr, 
               "%s - \"%s\" - failed to open file \"%s\" with errno %d.\n", 
               __func__, bctx->batch_name, client_table_filename, errno);
      return;
    }

  for (i = 0 ; i < bctx->client_num; i++)
    {
      dump_client (ct_file, &cctx_array[i]);
    }

  fclose (ct_file);
}

