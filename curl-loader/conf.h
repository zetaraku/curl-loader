/*
*     conf.h
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

#ifndef CONF_H
#define CONF_H

#include <linux/limits.h> /* NAME_MAX, PATH_MAX */

extern int authentication_url_load; /* Whether to use the first "authentication url"
                                    also for load. */
extern int connect_timeout; /* Configurable time to cut connect () in prog */  
extern int verbose_logging; /* Flag, whether to perform verbose logging */
extern int threads_run; /* Flag, whether to run batches as batch per thread. */

/*
  Output to the logfile will be re-directed to the file's start, thus 
  overwriting previous logged strings. Effectively, keeps the log history
  limited to below number of cycles.
*/
extern int logfile_rewind_cycles_num;

/* Zero - means reuse connections, any non-zero -
   don't re-use, refresh connections
*/
extern int reuse_connection_forbidden;

/* Whether to print to stdout the downloaded file body. */
extern int output_to_stdout;

/* If to output client messages to stderr, otherwise to logfile */
extern int stderr_print_client_msg;

extern unsigned long snapshot_timeout;


#define DEFAULT_POST_LOGIN_STR_1 "username=%s%d&password=%s%d"
#define DEFAULT_POST_LOGIN_STR_2 "username=%s&password=%s"
#define DEFAULT_POST_LOGOFF_STR "op=logoff"

#define POST_BASE_BUF_SIZE 64
extern char post_login_format [POST_BASE_BUF_SIZE];
extern char post_logoff_format [POST_BASE_BUF_SIZE];

enum
  {
    MODE_LOAD_STORMING = 1,
    MODE_LOAD_SMOOTH = 2,
  };

#define DEFAULT_MODE_LOADING MODE_LOAD_STORMING
extern int mode_loading;

extern int url_logging; /* Whether to include url to all log outputs. */

extern int w_logoff_mode; /* When positive we do logoff. */

/*
Modes of logoff: via GET, via GET and following POST
and using only POST method with post-string supplied as the
second part of the -p option string.
*/
enum
  {
    LOGOFF_NOT_DOING = 0,
    LOGOFF_GET_ONLY, // 1
    LOGOFF_GET_AND_POST, //2
    LOGOFF_POST_ONLY // 3
  };

extern int z_login_mode;
/*
Modes of login: via GET, optional redirection-GET, and following POST or
via POST only.
*/
enum
  {
    LOGIN_GET_AND_POST = 2,
    LOGIN_POST_ONLY //3
  };

extern char config_file[PATH_MAX + 1]; /* Name of the configuration file */

/* 
   Parses command line and fills configuration
   params.
*/
int parse_command_line (int argc, char *argv []);

/*
  Prints out usage of the program.
*/
void print_help ();

#endif /* CONF_H */
