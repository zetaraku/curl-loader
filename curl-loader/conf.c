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

// must be first include
#include "fdsetsize.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"

/*
  Command line configuration options. Setting defaults here.
*/
 /* Configurable time to cut connect () in prog */  
int connect_timeout = 5;

/* Flag, whether to perform verbose logging */
int verbose_logging = 0;

/* Flag, whether to run batches as batch per thread. */
int threads_run = 0;

/* Rewind logfile after num cycles */
int logfile_rewind_cycles_num = 10;

 /* Non-zero - dont reuse connections */
int reuse_connection_forbidden = 0;

/* Whether to stdout the downloaded file body */
int output_to_stdout = 0;

/* If to output client messages to stderr, otherwise to logfile */
int stderr_print_client_msg = 0;

/* Storming or smooth loading */
int loading_mode = LOAD_MODE_DEFAULT;

 /* Whether to include url to all log outputs. */
int url_logging = 0;

/* Name of the configuration file */
char config_file[PATH_MAX + 1];

/* TODO: make it configurable vis command-line.
   Milli-seconds between intermidiate statistics printouts in smooth-mode. 
*/
unsigned long snapshot_timeout = 2000;

/* 
   On errors, whether to continue loading for this client 
   from the next cycle, or to give it up.
 */
unsigned long error_recovery_client = 1;



int parse_command_line (int argc, char *argv [])
{
  int rget_opt = 0;

    while ((rget_opt = getopt (argc, argv, "c:ehf:l:m:op:rstvu")) != EOF) 
    {
      switch (rget_opt) 
        {
        case 'c': /* Connection establishment timeout */
          if (!optarg || (connect_timeout = atoi (optarg)) <= 0)
            {
              fprintf (stderr, 
                       "%s error: -c option should be a positive number in seconds.\n",
                  __func__);
              return -1;
            }
          break;
          
        case 'e':
          error_recovery_client = 0;
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
                       "%s error: -f option should be followed by a filename.\n", 
                       __func__);
              return -1;
            }
          break;
            
        case 'l': /* Number of cycles before a logfile rewinds. */
          if (!optarg || 
              (logfile_rewind_cycles_num = atoi (optarg)) < 2)
            {
              fprintf (stderr, 
                       "%s: error: -l option should be followed by a number above 2.\n",
                  __func__);
              return -1;
            }
          break;

        case 'm': /* Modes of loading: SMOOTH and STORMING */

            if (!optarg || 
                ((loading_mode = atoi (optarg)) != LOAD_MODE_STORMING &&
                 loading_mode != LOAD_MODE_SMOOTH))
            {
              fprintf (stderr, 
                       "%s error: -m to be followed by a number either %d or %d.\n",
                       __func__, LOAD_MODE_STORMING, LOAD_MODE_SMOOTH);
              return -1;
            }
          
          break;

        case 'o': /* Print body of the file to stdout. Default - just skip it. */
          output_to_stdout = 1;
          break;

        case 'r': /* Means - reuse connections, dont close and re-open. */
          reuse_connection_forbidden = 1;
          break;

        case 's': /* Stderr printout of client messages (instead of to a batch logfile). */
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
            fprintf (stderr, "%s error: not supported option\n", __func__);
          print_help ();
          return -1;
        }
    }

  if (optind < argc) 
    {
        fprintf (stderr, "%s error: non-option argv-elements: ", __func__);
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
  fprintf (stderr, " -c[onnection establishment timeout, seconds]\n");
  fprintf (stderr, " -e[rror drop client (smooth mode). Client on error doesn't attempt next cycle.]\n");
  fprintf (stderr, " -l[ogfile rewind after this number of cycles]\n");
  fprintf (stderr, " -m[ode of loading, 1 - storming, 2 - smooth (default)]\n");
  fprintf (stderr, " -o[utput to stdout bodies of downloaded files - attn!- bulky]\n");
  fprintf (stderr, " -r[euse onnections disabled. Close connections and re-open them. Try with and without]\n");
  fprintf (stderr, " -s[tderr printout of client messages instead of to logfile - attn!- bulky]\n");
  fprintf (stderr, " -t[hreads enable - enables threads, each runs a batch of clients]\n");
  fprintf (stderr, " -v[erbose output to the logfiles; includes info about headers sent/received]\n");
  fprintf (stderr, " -u[rl logging - logs url names to logfile, when -v verbose option is used]\n");
  fprintf (stderr, "\n");

  fprintf (stderr, "Note, that threads (-t) option is sometimes more buggy,\n");
  fprintf (stderr, "whereas without this option runs only the first batch of clients specified.\n");
  fprintf (stderr, "Thus, running several client batches without threads requires some script with \n");
  fprintf (stderr, "several curl-loader processes, each with its own batch config file.\n\n");
  fprintf (stderr, "Here is an example of the file with a single batch and a single url:\n\n");

  fprintf (stderr, " ----------------------------------------------------------------------------\n");
  fprintf (stderr, "########### GENERAL SECTION ################################\n");
  fprintf (stderr, "BATCH_NAME= bulk_batch     # The name of the batch.\n");
  fprintf (stderr, "CLIENTS_NUM=300            # Number of clients in the batch\n");
  fprintf (stderr, "CLIENTS_INITIAL_INC=30 # Startup increase num per second till CLIENTS_NUM reached\n");
  fprintf (stderr, "INTERFACE = eth0           # Name of interface from which to load \n");
  fprintf (stderr, "NETMASK=20                 # Netmask in CIDR-notation (number of bits with 1)\n");
  fprintf (stderr, "IP_ADDR_MIN= 192.168.1.1   # The client addresses starting address\n");
  fprintf (stderr, "IP_ADDR_MAX= 192.168.5.255 # Redundant, for self-control\n");
  fprintf (stderr, "CYCLES_NUM= 100            # Number of loading cycles to run, 0 -forever\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "########### LOGIN SECTION ##################################\n");
  fprintf (stderr, "LOGIN=n      # If 'y' or 'Y', login enabled, all other lines of the\n"); 
  fprintf (stderr, "                  # section to be filled\n");
  fprintf (stderr, "#LOGIN_USERNAME=  # Username string\n");
  fprintf (stderr, "#LOGIN_PASSWORD=  # Password string\n");
  fprintf (stderr, "#LOGIN_REQ_TYPE=  # Either GET+POST , or POST strings .\n");
  fprintf (stderr, "#LOGIN_POST_STR=  # POST string matrix. See below:\n");
  fprintf (stderr, "#\n");
  fprintf (stderr, "# To generate multiple unique users with unique passwords, use the string like\n");
  fprintf (stderr, "# \"username=%%s%%d&password=%%s%%d\". First %%s will be substituted by the \n");
  fprintf (stderr, "# string from LOGIN_USERNAME and %%d by the client number. Second %%s will\n");
  fprintf (stderr, "# be substituted by LOGIN_PASSWORD string and second %%d by the same client\n");
  fprintf (stderr, "# number. For example, if LOGIN_USERNAME=robert, LOGIN_PASSWORD=stam\n");
  fprintf (stderr, "# and LOGIN_POST_STR \"username=%%s%%d&password=%%s%%d\",  the string\n");
  fprintf (stderr, "# for POST-ing client number 1 will be \"username=robert1&password=stam1\".\n");
  fprintf (stderr, "# In such case LOGIN_USERNAME and LOGIN_PASSWORD strings are used \n");
  fprintf (stderr, "# as base-words to generate unique user credentials.\n");
  fprintf (stderr, "#\n");
  fprintf (stderr, "# To use the username and password 'as as', just provide LOGIN_POST_STR without \n");
  fprintf (stderr, "# %%d symbols, e.g. \"user=%%s&secret=%%s\". Thus, all clients will have the same\n");
  fprintf (stderr, "# POST credentials with the string looking like \"user=robert&secret=stam\".\n");
  fprintf (stderr, "#\n");
  fprintf (stderr, "# Note, that the words like 'username', 'user', 'password', 'secret', etc are the\n");
  fprintf (stderr, "# fields, that users are required to fill in the POST html page.\n");
  fprintf (stderr, "# Place N/A to LOGIN_POST_STR and LOGOFF_POST_STR, when no POST used.\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "#LOGIN_URL=                 # A valid http or https url to be used for login\n");
  fprintf (stderr, "#LOGIN_URL_MAX_TIME=        # Maximum batch time in seconds to login\n");
  fprintf (stderr, "#LOGIN_URL_INTERLEAVE_TIME= # Time in msec to sleep after login\n");
  fprintf (stderr, "#LOGIN_CYCLING=             # If 'y' login should be run in cycles, and not \n");
  fprintf (stderr, "                            # just done only once\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "########### UAS SECTION ####################################\n");
  fprintf (stderr, "UAS=y            # If 'y' or 'Y', login enabled, and other lines of the section \n");
   fprintf (stderr, "                # to be filled\n");
  fprintf (stderr, "UAS_URLS_NUM = 2 # Number of urls will be taken as in the format below\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "UAS_URL=http://localhost/apache2-default/ACE-INSTALL.html\n");
  fprintf (stderr, "UAS_URL_MAX_TIME = 6        # Maximum batch time in seconds to fetch the url\n");
  fprintf (stderr, "UAS_URL_INTERLEAVE_TIME = 0 # Time in msec to sleep after fetching the url\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "UAS_URL= http://localhost/apache2-default/index.html\n");
  fprintf (stderr, "UAS_URL_MAX_TIME = 4        # Maximum batch time in seconds to fetch the url\n");
  fprintf (stderr, "UAS_URL_INTERLEAVE_TIME = 0 # Time in seconds to sleep after fetching the url\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "########### LOGOFF SECTION #################################\n");
  fprintf (stderr, "LOGOFF=n                 # If 'y' or 'Y', login enabled, and other lines \n");
   fprintf (stderr, "                        # of the section to be filled\n");
  fprintf (stderr, "#LOGOFF_REQ_TYPE=        # Use GET , GET+POST , or POST strings\n");
  fprintf (stderr, "#LOGOFF_POST_STR=        # String to be used for logoff, like \"op=logoff\" \n");
  fprintf (stderr, "#LOGOFF_URL=             # A valid http or https url to be used for logoff\n");
  fprintf (stderr, "#LOGOFF_URL_MAX_TIME=    # Maximum batch time in seconds to logoff\n");
  fprintf (stderr, "#LOGOFF_URL_INTERLEAVE_TIME= # Time in msec to sleep after logoff\n");
  fprintf (stderr, "#LOGOFF_CYCLING=         # If 'y' login should be run in cycles, and not just \n");
   fprintf (stderr, "                        # done only once\n");
  fprintf (stderr, "--------------------------------------------------------------------------------\n");
  fprintf (stderr, "For more examples, please, look at configs directory.\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "Note, that there is no any more limit of 1000 sockets per batch of clients.\n");
  fprintf (stderr, "Running thousands and more clients, please do not forget the options:\n");
  fprintf (stderr, "- to increase limit of open descriptors in shell by running e.g. #ulimit -n 20000:\n");
  fprintf (stderr, "- to increase total limit of  open descriptors in systeme somewhere in /proc\n");
  fprintf (stderr, "- to consider reusing sockets in time-wait state: by #echo 1 > \n");
  fprintf (stderr, " /proc/sys/net/ipv4/tcp_tw_recycle\n");
  fprintf (stderr, "- and/or  #echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "\n");
}
