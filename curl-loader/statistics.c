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
#include <string.h>

#include "batch.h"
#include "client.h"

#include "statistics.h"
#include "screen.h"

#define SECURE_APPL_STR "S "
#define UNSECURE_APPL_STR "  "

static void
dump_snapshot_interval_and_advance_total_statistics (batch_context* bctx,
                                                     unsigned long now_time);

static void dump_statistics (unsigned long period, 
                             stat_point *http, 
                             stat_point *https);

static void print_statistics_footer_to_file (FILE* file);

static void print_statistics_data_to_file (FILE* file, 
                                   unsigned long timestamp,
                                   char* prot,
                                   long clients_num,
                                   stat_point *sd,
                                   unsigned long period);

static void print_operational_statistics (op_stat_point*const osp);

static void dump_stat_to_screen (char* protocol, 
                                 stat_point* sd, 
                                 unsigned long period);

static void dump_clients (client_context* cctx_array);

/****************************************************************************************
* Function name - stat_point_add
*
* Description - Adds counters of one stat_point structure to anther
* Input -       *left -  pointer to the stat_point, where counter will be added
*               *right -  pointer to the stat_point, which counter will be added to the <left>
* Return Code/Output - None
****************************************************************************************/
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

/****************************************************************************************
* Function name - stat_point_reset
*
* Description - Nulls counters of a stat_point structure
* Input -       *point -  pointer to the stat_point
* 
* Return Code/Output - None
****************************************************************************************/
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
* Function name - op_stat_point_add
*
* Description - Adds counters of one op_stat_point structure to anther
* Input -       *left -  pointer to the op_stat_point, where counter will be added
*               *right -  pointer to the op_stat_point, which counter will be added to the <left>
* Return Code/Output - None
****************************************************************************************/
void op_stat_point_add (op_stat_point* left, op_stat_point* right)
{
  size_t i;
  
  if (!left || !right)
    return;

  if (left->uas_url_num != right->uas_url_num)
    return;
  
  if (left->login_ok)
    {
      *left->login_ok += *right->login_ok;
      *left->login_failed += *right->login_failed;
    }

  for ( i = 0; i < left->uas_url_num; i++)
    {
      left->uas_url_ok[i] += right->uas_url_ok[i];
      left->uas_url_failed[i] += right->uas_url_failed[i];
    }

  if (left->logoff_ok)
    {
      *left->logoff_ok += *right->logoff_ok;
      *left->logoff_failed += *right->logoff_failed;
    }

  left->call_init_count += right->call_init_count;
}

/****************************************************************************************
* Function name - op_stat_point_reset
*
* Description - Nulls counters of an op_stat_point structure
* Input -       *point -  pointer to the op_stat_point
* Return Code/Output - None
****************************************************************************************/
void op_stat_point_reset (op_stat_point* point)
{
  if (!point)
    return;

  if (point->login_ok)
    {
      *point->login_ok = *point->login_failed = 0;
    }

  if (point->uas_url_num)
    {
      size_t i;
      for ( i = 0; i < point->uas_url_num; i++)
        {
          point->uas_url_ok[i] = point->uas_url_failed[i] = 0;
        }
    }
    /* Don't null point->uas_url_num ! */

   if (point->logoff_ok)
    {
      *point->logoff_ok = *point->logoff_failed = 0;
    }

   point->call_init_count = 0;
}

/****************************************************************************************
* Function name -  op_stat_point_release
*
* Description - Releases memory allocated by op_stat_point_init ()
* Input -       *point -  pointer to the op_stat_point, where counter will be added
* Return Code/Output - None
****************************************************************************************/
void op_stat_point_release (op_stat_point* point)
{
  if (point->login_ok)
    free (point->login_ok);

  if (point->login_failed)
    free (point->login_failed);

  if (point->uas_url_ok)
    free (point->uas_url_ok);

  if (point->uas_url_failed)
    free (point->uas_url_failed);

  if (point->logoff_ok)
    free (point->logoff_ok);

  if (point->logoff_failed)
    free (point->logoff_failed);

  memset (point, 0, sizeof (op_stat_point));
}

/****************************************************************************************
* Function name - op_stat_point_init
*
* Description - Initializes an allocated op_stat_point by allocating relevant fields for counters
*
* Input -       *point -  pointer to the op_stat_point, where counter will be added
*               login -  boolean flag, whether login is relevant (1) or not (0)
*               uas_url_num -  number of UAS urls, which can be 0, if UAS is not relevant
*               logoff -  boolean flag, whether login is relevant (1) or not (0)
*
* Return Code/Output - None
****************************************************************************************/
int op_stat_point_init (op_stat_point* point, 
                        size_t login, 
                        size_t uas_url_num, 
                        size_t logoff)
{
  if (! point)
    return -1;

  if (login)
    { 
      if (!(point->login_ok = calloc (1, sizeof (unsigned long))) ||
          !(point->login_failed = calloc (1, sizeof (unsigned long))))
        {
          goto allocation_failed;
        }
    }

   if (uas_url_num)
    { 
      if (!(point->uas_url_ok = calloc (uas_url_num, sizeof (unsigned long))) ||
          !(point->uas_url_failed = calloc (uas_url_num, sizeof (unsigned long))))
        {
          goto allocation_failed;
        }
      else
        point->uas_url_num = uas_url_num;
    }

  if (logoff)
    { 
      if (!(point->logoff_ok = calloc (1, sizeof (unsigned long))) ||
          !(point->logoff_failed = calloc (1, sizeof (unsigned long))))
        {
          goto allocation_failed;
        }
    }

  point->call_init_count = 0;

  return 0;

 allocation_failed:
  fprintf(stderr, "%s - calloc () failed with errno %d.\n", 
              __func__, errno);
  return -1;
}

/****************************************************************************************
* Function name -  op_stat_update
*
* Description - Updates operation statistics using information from client context
*
* Input -       *point -  pointer to the op_stat_point, where counters to be updated
*               current_state - current state of a client
*               prev_state -  previous state of a client
*               current_uas_url_index -  current uas url index of a the client
*               prev_uas_url_index -  previous uas url index of a the client
*
* Return Code/Output - None
****************************************************************************************/
void op_stat_update (op_stat_point* op_stat, 
                     int current_state, 
                     int prev_state,
                     size_t current_uas_url_index,
                     size_t prev_uas_url_index)
{
  (void) current_uas_url_index;

  if (!op_stat)
    return;

  switch (prev_state)
    {
    case CSTATE_LOGIN:
      if (current_state != CSTATE_LOGIN)
        {
          (current_state == CSTATE_ERROR) ? ++*op_stat->login_failed : 
            ++*op_stat->login_ok;
        }
      break;
      
    case CSTATE_UAS_CYCLING:
      if (current_state != CSTATE_UAS_CYCLING)
        {
          /* 
             If loading has advanced to the next state, update operational 
             statistics counters of the last UAS URL (sometimes, the last is also
             the first and the only)
          */
          (current_state == CSTATE_ERROR) ? 
            op_stat->uas_url_failed[op_stat->uas_url_num-1]++ : 
            op_stat->uas_url_ok[op_stat->uas_url_num-1]++;
        }
      else
        {
          //  if (current_uas_url_index == (prev_uas_url_index + 1))
          // {
              /* 
                 If loading has advanced to the next UAS url, update
                 previous url operational statistics counters.
              */
              (current_state == CSTATE_ERROR) ? 
                op_stat-> uas_url_failed[prev_uas_url_index]++ : 
                op_stat->uas_url_ok[prev_uas_url_index]++;
          //}
        }
      break;

    case CSTATE_LOGOFF:
      if (current_state != CSTATE_LOGOFF)
        {
          (current_state == CSTATE_ERROR) ? ++*op_stat->logoff_failed : 
            ++*op_stat->logoff_ok;
        }
      break;
    }
  
  return;
}

void op_stat_call_init_count_inc (op_stat_point* op_stat)
{
  op_stat->call_init_count++;
}

/****************************************************************************************
* Function name - get_tick_count
*
* Description - Delivers timestamp in milliseconds.
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
*               print_snapshot_interval_statistics and print_statistics_* functions as well as calls
*               dump_clients () to dump the clients table.
*
* Input -       *cctx - pointer to client context, where the decision to complete loading 
*                       (and dump) has been made. 
* Return Code/Output - None
****************************************************************************************/
void dump_final_statistics (client_context* cctx)
{
  batch_context* bctx = cctx->bctx;
  unsigned long now = get_tick_count();

  print_snapshot_interval_statistics (now - bctx->last_measure,
		&bctx->http_delta,  
		&bctx->https_delta);

  stat_point_add (&bctx->http_total, &bctx->http_delta);
  stat_point_add (&bctx->https_total, &bctx->https_delta); 
  op_stat_point_add (&bctx->op_delta, &bctx->op_total);
    
  fprintf(stdout,"==================================================\n");
  fprintf(stdout,"End of the test for batch: %-10.10s\n", bctx->batch_name); 
  fprintf(stdout,"==================================================\n");
  
  now = get_tick_count();

  const int seconds_run = (int)(now - bctx->start_time)/ 1000;
  if (!seconds_run)
    return;
  
  fprintf(stdout,"Test total duration was %d seconds and CAPS average %ld:\n", 
          seconds_run, bctx->op_total.call_init_count / seconds_run);

  dump_statistics (seconds_run, 
                   &bctx->http_total,  
                   &bctx->https_total);

  print_operational_statistics (&bctx->op_total);


  if (bctx->statistics_file)
    {

      print_statistics_footer_to_file (bctx->statistics_file);
      print_statistics_header (bctx->statistics_file);

      const unsigned long loading_t = now - bctx->start_time;
      const unsigned long loading_time = loading_t ? loading_t : 1;
 
      print_statistics_data_to_file (
				bctx->statistics_file,
				loading_time/1000,
				UNSECURE_APPL_STR,
				pending_active_and_waiting_clients_num (bctx),
				&bctx->http_total,
				loading_time);
			
      print_statistics_data_to_file (
				bctx->statistics_file, 
				loading_time/1000,
				SECURE_APPL_STR,
				pending_active_and_waiting_clients_num (bctx),
				&bctx->https_total,
				loading_time);
    }

  dump_clients (cctx);
  fprintf (stderr, "\nExited. For details look in the files:\n"
           "- %s.log for errors and traces;\n"
           "- %s.txt for loading statistics;\n"
           "- %s.ctx for virtual client based statistics.\n"
           "You may add -v and -u options to the command line for more verbouse output to %s.log file.\n",
           bctx->batch_name, bctx->batch_name, bctx->batch_name, bctx->batch_name);

  screen_release ();
}

/****************************************************************************************
* Function name - dump_snapshot_interval
*
* Description - Dumps summary statistics since the start of load
* Input -       *bctx - pointer to batch structure
*               now -  current time in msec since epoch
* Return Code/Output - None
****************************************************************************************/
void dump_snapshot_interval (batch_context* bctx, unsigned long now)
{
  if (!stop_loading)
    {
      fprintf(stdout, "\033[2J");
    }

  dump_snapshot_interval_and_advance_total_statistics(bctx, now);

  int seconds_run = (int)(now - bctx->start_time)/ 1000;
  if (!seconds_run)
    {
      seconds_run = 1;
    }

  fprintf(stdout,"-----------------------------------------------------\n");
  fprintf(stdout,"Summary stats since load start (load runs:%d secs, CAPS-average:%ld):\n", 
          seconds_run, bctx->op_total.call_init_count / seconds_run); 

  print_operational_statistics (&bctx->op_total);
  
  dump_statistics (seconds_run, 
                   &bctx->http_total,  
                   &bctx->https_total);

  fprintf(stdout,"=============================================================\n");

  if (bctx->do_client_num_gradual_increase && 
      (bctx->stop_client_num_gradual_increase == 0))
    {
      fprintf(stdout," Automatic: adding %ld clients/sec. Stop inc and manual [M].\n",
              bctx->clients_initial_inc);
    }
  else
    {
      const int current_clients = pending_active_and_waiting_clients_num (bctx);

      fprintf(stdout," Manual: clients:max[%d],curr[%d]. Inc num: [+|*].",
              bctx->client_num, current_clients);

      if (bctx->stop_client_num_gradual_increase && 
          bctx->clients_initial_inc &&
          current_clients < bctx->client_num)
        {
          fprintf(stdout," Automatic: [A].\n");
        }
      else
        {
          fprintf(stdout,"\n");
        }
    }

  fprintf(stdout,"=============================================================\n");
  fflush (stdout);
  
  //fprintf (stderr, "%s do_client %d, stop_client %d.\n", 
  //         __func__, bctx->do_client_num_gradual_increase,
  //         bctx->stop_client_num_gradual_increase);
}

/****************************************************************************************
* Function name - print_snapshot_interval_statistics
*
* Description - Dumps final statistics counters to stderr and statistics file using 
*               print_snapshot_interval_statistics and print_statistics_* functions as well 
*               as calls dump_clients () to dump the clients table.
*
* Input -       period - latest time period in milliseconds
*               *http - pointer to the HTTP collected statistics to output
*               *https - pointer to the HTTPS collected statistics to output
* Return Code/Output - None
****************************************************************************************/
void print_snapshot_interval_statistics (unsigned long period,  
                                         stat_point *http, 
                                         stat_point *https)
{
  period /= 1000;
  if (period == 0)
    {
      period = 1;
    }

  dump_stat_to_screen (UNSECURE_APPL_STR, http, period);
  dump_stat_to_screen (SECURE_APPL_STR, https, period);
}


/****************************************************************************************
* Function name - dump_snapshot_interval_and_advance_total_statistics
*
* Description - Dumps snapshot_interval statistics for the latest loading time period and adds
*               this statistics to the total loading counters 
*
* Input -       *bctx - pointer to batch context
*               now_time - current time in msec since the epoch
*
* Return Code/Output - None
****************************************************************************************/
void dump_snapshot_interval_and_advance_total_statistics(batch_context* bctx,
                                                    unsigned long now_time)
{
  const unsigned long delta_t = now_time - bctx->last_measure; 
  const unsigned long delta_time = delta_t ? delta_t : 1;

  if (stop_loading)
    {
      dump_final_statistics (bctx->cctx_array);
      exit (1); 
    }

  fprintf(stdout,"============  loading batch is: %-10.10s ==================\n",
          bctx->batch_name);
  fprintf(stdout,"Last interval stats (interval:%ld sec, clients:%d, CAPS:%ld):\n",
          (unsigned long ) delta_time/1000, pending_active_and_waiting_clients_num (bctx),
          bctx->op_delta.call_init_count* 1000/delta_time);

  print_operational_statistics (&bctx->op_delta);

  print_snapshot_interval_statistics(delta_time, 
                                     &bctx->http_delta,  
                                     &bctx->https_delta);

  if (bctx->statistics_file)
    {
      const unsigned long timestamp_sec =  (now_time - bctx->start_time) / 1000;

      print_statistics_data_to_file (
                             bctx->statistics_file,
                             timestamp_sec,
                             UNSECURE_APPL_STR,
                             pending_active_and_waiting_clients_num (bctx),
                             &bctx->http_delta,
                             delta_time);
    
      print_statistics_data_to_file (
                             bctx->statistics_file, 
                             timestamp_sec,
                             SECURE_APPL_STR, 
                             pending_active_and_waiting_clients_num (bctx),
                             &bctx->https_delta,
                             delta_time);
    }

  stat_point_add (&bctx->http_total, &bctx->http_delta);
  stat_point_add (&bctx->https_total, &bctx->https_delta);

  stat_point_reset (&bctx->http_delta); 
  stat_point_reset (&bctx->https_delta);

  op_stat_point_add (&bctx->op_total, &bctx->op_delta );
  op_stat_point_reset (&bctx->op_delta);
        
  bctx->last_measure = now_time;
}

/****************************************************************************************
* Function name - dump_statistics
*
* Description - Dumps statistics to screen
*
* Input -       period - time interval of the statistics collection in msecs. 
*               *http - pointer to stat_point structure with HTTP/FTP counters collection
*               *https - pointer to stat_point structure with HTTPS/FTPS counters collection
*
* Return Code/Output - None
****************************************************************************************/
static void dump_statistics (unsigned long period,  
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
  
  dump_stat_to_screen (UNSECURE_APPL_STR, http, period);
  dump_stat_to_screen (SECURE_APPL_STR, https, period);
}


/****************************************************************************************
* Function name - dump_stat_to_screen
*
* Description - Dumps statistics to screen
*
* Input -       *protocol - name of the applications/protocols
*               *sd - pointer to statistics data with statistics counters collection
*               period - time interval of the statistics collection in msecs. 
*
* Return Code/Output - None
****************************************************************************************/
static void dump_stat_to_screen (char* protocol, 
                                 stat_point* sd, 
                                 unsigned long period)
{
  fprintf(stdout, "%sReq:%ld,2xx:%ld,3xx:%ld,4xx:%ld,5xx:%ld,Err:%ld,"
          "D:%ldms,D-2xx:%ldms,T-i:%lldB/s,T-o:%lldB/s\n",
          protocol, sd->requests, sd->resp_oks, sd->resp_redirs, sd->resp_cl_errs,
          sd->resp_serv_errs, sd->other_errs, sd->appl_delay, sd->appl_delay_2xx,
          sd->data_in/period, sd->data_out/period);

    //fprintf (stdout, "Appl-Delay-Points %d, Appl-Delay-2xx-Points %d \n", 
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
             "Run-Time,Appl,Clients,Req,2xx,3xx,4xx,5xx,Err,Delay,Delay-2xx,Thr-In,Thr-Out\n");
    fflush (file);
}

/****************************************************************************************
* Function name - print_statistics_footer_to_file
*
* Description - Prints to a file separation string between the snapshot_interval statistics and 
*               the final statistics number for the total loading process
*
* Input -       *file - open file pointer
* Return Code/Output - None
****************************************************************************************/
static void print_statistics_footer_to_file (FILE* file)
{
    fprintf (file, "*, *, *, *, *, *, *, *, *, *, *, *, *\n");
    fflush (file);
}

/****************************************************************************************
* Function name - print_statistics_data_to_file
*
* Description - Prints to a file batch statistics. At run time the interval statistics 
*               is printed and at the end of a load - summary statistics.
*
* Input -       *file - open file pointer
*               timestamp - time in seconds since the load started
*               *protocol - name of the applications/protocols
*               clients_num - number of active (running + waiting) clients
*               *sd - pointer to statistics data with statistics counters collection
*               period - time interval of the statistics collection in msecs. 
*
* Return Code/Output - None
****************************************************************************************/
static void print_statistics_data_to_file (FILE* file, 
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

/****************************************************************************************
* Function name - dump_clients
*
* Description - Prints to the <batch-name>.ctx file debug and statistics information
*               collected by every loading client.
*
* Input -       *cctx_array - array of client contexts
*
* Return Code/Output - None
****************************************************************************************/
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

/****************************************************************************************
* Function name - print_operational_statistics
*
* Description - Prints to stdout number of login, UAS - for each URL and logoff operations
*               success and failure numbers
*
* Input -       *osp - pointer to the operational statistics point
*
* Return Code/Output - None
****************************************************************************************/
static void print_operational_statistics (op_stat_point*const osp)
{
  if (!osp)
    return;

  fprintf (stdout, " Operations:\tSuccess\t\tFailed\n");

  if (osp->login_ok && osp->login_failed)
    {
      fprintf (stdout, " LOGIN:\t\t%ld\t\t%ld\n", *osp->login_ok, *osp->login_failed);
    }

  if (osp->uas_url_num && osp->uas_url_ok && osp->uas_url_failed)
    {
      unsigned long i;
      for (i = 0; i < osp->uas_url_num; i++)
        {
          fprintf (stdout, " UAS-%ld:\t\t%ld\t\t%ld\n",
                   i, osp->uas_url_ok[i], osp->uas_url_failed[i]);
        }
    }

  if (osp->logoff_ok && osp->logoff_failed)
    {
      fprintf (stdout, " LOGOFF:\t%ld\t\t%ld\n", *osp->logoff_ok, *osp->logoff_failed);
    }
}
