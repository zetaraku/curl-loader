/*
*     conf.c
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"

/*
  Command line configuration options. Setting defaults here.
*/
int authentication_url_load = 0; /* Whether to use the first "authentication url"
                                    also for load. */

int connect_timeout = 1; /* Configurable time to cut connect () in prog */  
int verbose_logging = 0; /* Flag, whether to perform verbose logging */
int threads_run = 0; /* Flag, whether to run batches as batch per thread. */
int logfile_rewind_cycles_num = 10; /* Rewind logfile after num cycles */
int reuse_connection_forbidden = 1; /* Non-zero - dont reuse connections */
int output_to_stdout = 0; /* Whether to stdout the downloaded file body */
 int stderr_print_client_msg = 0; /* If to output client messages to stderr, 
                                     otherwise to logfile */
unsigned long snapshot_timeout = 1000;
int loading_mode = LOAD_MODE_DEFAULT; /* Storming or smooth loading */
int url_logging = 0; /* Whether to include url to all log outputs. */

char config_file[PATH_MAX + 1]; /* Name of the configuration file */

int parse_command_line (int argc, char *argv [])
{
  int rget_opt = 0;

    while ((rget_opt = getopt (argc, argv, "c:hf:l:m:op:rstvu")) != EOF) 
    {
      switch (rget_opt) 
        {
        case 'c': /* Connection establishment timeout */
          if (!optarg || (connect_timeout = atoi (optarg)) <= 0)
            {
              fprintf (stderr, 
                       "main: -c option should be a positive number in seconds.\n");
            }
          break;

        case 'h':
          print_help ();
          exit (0);

        case 'f': /* Configuration file */
          if (optarg)
            strcpy(config_file, optarg);
          else
            {
              fprintf (stderr, 
                       "main: -f option should be followed by a filename.\n");
              return -1;
            }
          break;
            
        case 'l': /* Number of cycles before log file rewind. */
          if (!optarg || 
              (logfile_rewind_cycles_num = atoi (optarg)) < 2)
            {
              fprintf (stderr, 
                       "main: -l option should be followed by a number above 2.\n");
              return -1;
            }
          break;

        case 'm': /* Modes of loading: SMOOTH and STORMING */

            if (!optarg || 
                ((loading_mode = atoi (optarg)) != LOAD_MODE_STORMING &&
                 loading_mode != LOAD_MODE_SMOOTH))
            {
              fprintf (stderr, 
                       "main: -m to be followed by a number either %d or %d.\n",
                       LOAD_MODE_STORMING, LOAD_MODE_SMOOTH);
              return -1;
            }
          
          break;

        case 'o': /* Print body of fetched file to stdout. Default - just skip it. */
          output_to_stdout = 1;
          break;

        case 'r': /* Means - reuse connections, dont close and re-open. */
          reuse_connection_forbidden = 0;
          break;

        case 's': /* Stderr printout of client messages instead of to batch logfile. */
          stderr_print_client_msg = 1;
          break;

        case 't': /* Run each batch of clients in a dedicated thread. */
            threads_run = 1;
            break;

        case 'v':
          verbose_logging = 1; 
          break;

        case 'u':
          url_logging = 1; 
          break;

        default: 
          fprintf (stderr, "main: error - not supported option\n");
          print_help ();
          return -1;
        }
    }

  if (optind < argc) 
    {
      fprintf (stderr, "main: error - non-option argv-elements: ");
      while (optind < argc)
        fprintf (stderr, "%s ", argv[optind++]);
      fprintf (stderr, "\n");
      print_help ();
      return -1;
    }

  return 0;
}

void print_help ()
{
  fprintf (stderr, "usage: run as a root:\n");
  fprintf (stderr, "#./curl-loader -f <configuration file name> with [other options below]:\n");
  fprintf (stderr, " -a[uthentication url to be used also for load]\n");
  fprintf (stderr, " -c[onnection establishment timeout, seconds]\n");
  fprintf (stderr, " -l[ogfile rewind after this number of cycles]\n");
  fprintf (stderr, " -o[utput to stdout bodies of downloaded files - attn!- bulky]\n");
  fprintf (stderr, " -r[euse established connections, don't run close-open cycles]\n");
  fprintf (stderr, " -s[tderr printout of client messages instread of to logfile - attn!- bulky]\n");
  fprintf (stderr, " -t[hreads enable - enables threads, each runs a batch of clients]\n");
  fprintf (stderr, " -v[erbose output to the logfiles; includes info about headers sent/received]\n");
  fprintf (stderr, " -u[rl logging - enables logging of url names to all client outputs]\n");
  fprintf (stderr, "\n");

  fprintf (stderr, "Note, that running with threads (-t) sometimes gots stacked,\n");
  fprintf (stderr, "whereas without this option runs only the first batch of clients specified.\n");
  fprintf (stderr, "Thus, running several client batches without threads requires some script with \n");
  fprintf (stderr, "several curl-loader processes, each with its own batch config file.\n\n");
  fprintf (stderr, "\nConfiguration file should possess at least one client batch defined\n");
  fprintf (stderr, "with the following params in each batch:\n");
  fprintf (stderr, "BATCH_NAME;CLIENTS_NUM;INTERFACE;NETMASK;IP_ADDR_MIN;\n");
  fprintf (stderr, "IP_ADDR_MAX;USERNAME;LOGIN_PASSWORD;CYCLES_NUM;URLS_NUM;\n");
  fprintf (stderr, "and further one or more sequences of url-data in the following format:\n");
  fprintf (stderr, "\t URL;URL_MAX_TIME;URL_INTERLEAVE_TIME\n\n");
  fprintf (stderr, "Here is an example of the file with a single batch and a single url:\n\n");

  fprintf (stderr, "BATCH_NAME = first_batch\n");
  fprintf (stderr, "CLIENTS_NUM = 640\n");
  fprintf (stderr, "INTERFACE = eth0\n");    
  fprintf (stderr, "NETMASK = 20\n");
  fprintf (stderr, "IP_ADDR_MIN = 192.168.1.1\n");
  fprintf (stderr, "IP_ADDR_MAX =192.168.5.255\n");
  fprintf (stderr, "LOGIN_USERNAME = NO\n");
  fprintf (stderr, "LOGIN_PASSWORD = NO\n"); 
  fprintf (stderr, "CYCLES_NUM = 10000\n");
  fprintf (stderr, "URLS_NUM = 1\n");
  fprintf (stderr, "URL = http://192.168.112.200/ACE-INSTALL.html\n");
  fprintf (stderr, "URL_MAX_TIME = 9\n");
  fprintf (stderr, "URL_INTERLEAVE_TIME = 1\n");
  fprintf (stderr, "\n");
  fprintf (stderr, " Please, use string \"NO\" both as a username and a password to skip\n");
  fprintf (stderr, " authentication, e.g. just simulating http, ftp, telnet, etc load for netfilter testing.\n");
  fprintf (stderr, " For more details about POST login string form option -p, please look in README.\n\n");
  fprintf (stderr, " Set CYCLES_NUM to 0 (zero), when willing to run a batch forever.\n\n");
  fprintf (stderr, " Note, that currenly there is a limit of 1000 sockets per batch of clients.\n\n");
  fprintf (stderr, " Running thousands and more clients, please do not forget the options:\n");
  fprintf (stderr, "   - to increase limit of open desriptors in shell by running e.g. \"#ulimit -n 10000\":\n");
  fprintf (stderr, "   - to increase total limit of  open descriptors in systeme somewhere in /proc \n");
  fprintf (stderr, "   - to consider reusing sockets in time-wait state: by \"#echo 1 > /proc/sys/net/ipv4/tcp_tw_recycle\" \n");
  fprintf (stderr, "   - and/or  \"#echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\" \n\n");
  fprintf (stderr, "URLS_NUM - is a total number of urls to fetch\n");
  fprintf (stderr, "URLS_MAX_TIME is a maximum time in secondsto be wait for completion of the url retrival and\n");
  fprintf (stderr, "URL_INTERLEAVE_TIME is time in seconds to sleep till the next url will be put in work.\n");
  fprintf (stderr, "\n");
}
