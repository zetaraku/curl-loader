/* 
*     parse_conf.c
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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "parse_conf.h"
#include "batch.h"
#include "conf.h"

#define NON_APPLICABLE_STR "N/A"

/**** enum parsing_state **************

  Each enumeration is mapped to the string in the configuration file.
  Each enum means, that there is a required string in the batch configuration, 
  whereas url blocks:
  URL,
  URL_MAX_TIME,
  URL_INTERLEAVE_TIME,

  may be repeated several times, supporting emulation of user-activity.

  Note, that URL_MAX_TIME and URL_INTERLEAVE_TIME are
  relevant only for loading using storming mode.
*/
enum parsing_state
{
    NOT_INIT = 0,

    /*------------------------ GENERAL SECTION ------------------------------ */

    BATCH_NAME,
    CLIENTS_NUM,
    INTERFACE,
    NETMASK,
    IP_ADDR_MIN,
    IP_ADDR_MAX,
    CYCLES_NUM,

    /*------------------------ LOGIN SECTION -------------------------------- */

    LOGIN,
    /* if Login Authentication is yes - optional fields*/
    LOGIN_USERNAME,
    LOGIN_PASSWORD,
    LOGIN_REQ_TYPE, /* GET-POST or POST */
    LOGIN_POST_STR, /* if includes POST - should provide strings in two possible 
                       formats variants currently supported to deliver either: 
                       1. the same login_username and login_password for all users - like
                       username=steve&password=pass;
                       2. a unique login_username and login_password for each user - like
                       username=steve<N>&password=pass<N>, where N - client number 1, 2, 3*/
    LOGIN_URL,
    LOGIN_URL_MAX_TIME,
    LOGIN_URL_INTERLEAVE_TIME,
    LOGIN_CYCLING,

    
    /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */

    UAS,
    /* USER_ACTIVITY_SIMULATION - if yes, then optional N-URLs: */
    UAS_URLS_NUM,
    UAS_URL,
    UAS_URL_MAX_TIME,
    UAS_URL_INTERLEAVE_TIME,


    /*------------------------LOGOFF SECTION ---------------------------------*/

    LOGOFF,
    /* if Logoff is yes - optional fields */
    LOGOFF_REQ_TYPE, /* GET, GET-POST or POST */
    LOGOFF_POST_STR, /* if LOGOFF_TYPE is GET-POST or POST */
    LOGOFF_URL,
    LOGOFF_URL_MAX_TIME,
    LOGOFF_URL_INTERLEAVE_TIME, /* time to be up after logoff */
    LOGOFF_CYCLING,

    VALID_BATCH,
};

/*
  Used to map parameter by id to its name used in
  configuration file. Assists in parsing.
*/
typedef struct batch_params_map
{
  int        id;
  char*   str; /* string name of the param */
} batch_params_map;

#define BATCH_MAX_CLIENTS_NUM 4096

#define REQ_GET "GET"
#define REQ_POST "POST"
#define REQ_GET_POST "GET+POST"



static int add_param_to_batch (char*const input, 
                               size_t input_length,
                               batch_context*const bctx, 
                               int*const batch_num);
static int set_value_to_param (batch_context*const bctx, 
                               char*const value, 
                               size_t value_length);
static void advance_batch_parser_state (batch_context*const bctx);
static url_appl_type url_schema_classification (const char* const url);
static char* skip_non_ws (char*ptr, size_t*const len);
static char* eat_ws (char*ptr, size_t*const len);
static int is_ws (char*const ptr);
static int is_non_ws (char*const ptr);


/*******************************************************************************
* Function name - parse_config_file
*
* Description - Parses configuration file and fills batch contexts in array
* Output -       *bctx_array - array of batch contexts to be filled on parsing
* Input-              bctx_array_size - number of bctx contexts in <bctx_array>.
* Return Code/Output - On Success - number of batches >=1, on Error -1
********************************************************************************/
int parse_config_file (char* const filename, 
                       batch_context* bctx_array, 
                       size_t bctx_array_size)
{
  char fgets_buff[2048];
  int batches_number = 0;
  FILE* fp;
  struct stat statbuf;

  /* Check, if the configuration file exists. */
  if (stat (filename, &statbuf) == -1)
    {
      fprintf (stderr, 
               "%s - failed to find configuration file \"%s\" with errno %d.\n", 
               __func__, filename, errno);
      return -1;
    }

  if (!(fp = fopen (filename, "r")))
    {
      fprintf (stderr, 
               "%s - fopen() failed to open for reading filename \"%s\", errno %d.\n", 
               __func__, filename, errno);
      return -1;
    }

  while (fgets (fgets_buff, sizeof (fgets_buff) - 1, fp))
    {
      fprintf (stderr, "%s - processing file string \"%s\n", 
               __func__, fgets_buff);

      char* string_buff = NULL;
      size_t string_len = 0;

      if ((string_len = strlen (fgets_buff)) && 
          (string_buff = eat_ws (fgets_buff, &string_len)))
        {

          if (batches_number >= (int) bctx_array_size)
            {
              fprintf(stderr, "%s - error: maximum batches limit (%d) reached \n", 
                       __func__, bctx_array_size);
              fclose (fp);
              return -1 ;
            }

          /* Line may be commented out by '#'.*/
          if (fgets_buff[0] == '#')
            {
              fprintf (stderr, "%s - skipping commented file string \"%s\n", 
                       __func__, fgets_buff);
              continue;
            }

          if (add_param_to_batch (fgets_buff,
                                  string_len,
                                  &bctx_array[batches_number], 
                                  &batches_number) == -1)
            {
              fprintf (stderr, 
                       "%s - error: add_param_to_batch () failed processing line \"%s\"\n", 
                       __func__, fgets_buff);
              fclose (fp);
              return -1 ;
            }
        }
    }

  fclose (fp);

  if (!batches_number)
    {
      fprintf (stderr, 
                   "%s - error: failed to load even a single valid batch\n", __func__);
    }
  else
    {
      fprintf (stderr, 
               "%s - loaded %d batches\n", __func__, batches_number);
    }
  return batches_number;
}

static const batch_params_map bp_map [] =
  {
    {NOT_INIT, "NOT_INIT"}, /* Starts from 0. Zero entry point to the map.*/

    /*------------------------ GENERAL SECTION ------------------------------ */

    {BATCH_NAME, "BATCH_NAME"},
    {CLIENTS_NUM, "CLIENTS_NUM"},
    {INTERFACE,"INTERFACE"},
    {NETMASK, "NETMASK"},
    {IP_ADDR_MIN, "IP_ADDR_MIN"},
    {IP_ADDR_MAX, "IP_ADDR_MAX"},
    {CYCLES_NUM,"CYCLES_NUM"},


   /*------------------------ LOGIN SECTION -------------------------------- */

    {LOGIN, "LOGIN"},
    /* if Login Authentication is yes - then the optional fields follow: */
    {LOGIN_USERNAME, "LOGIN_USERNAME"},
    {LOGIN_PASSWORD,"LOGIN_PASSWORD"},
    {LOGIN_REQ_TYPE, "LOGIN_REQ_TYPE"},
    {LOGIN_POST_STR, "LOGIN_POST_STR"},
    {LOGIN_URL, "LOGIN_URL"},
    {LOGIN_URL_MAX_TIME, "LOGIN_URL_MAX_TIME"},
    {LOGIN_URL_INTERLEAVE_TIME, "LOGIN_URL_INTERLEAVE_TIME"},
    {LOGIN_CYCLING, "LOGIN_CYCLING"},


    /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */ 

    {UAS, "UAS"},
    /* USER_ACTIVITY_SIMULATION - if yes, then optional N-URLs: */

    {UAS_URLS_NUM, "UAS_URLS_NUM"},
    {UAS_URL,"UAS_URL"},
    {UAS_URL_MAX_TIME,"UAS_URL_MAX_TIME"},
    {UAS_URL_INTERLEAVE_TIME,"UAS_URL_INTERLEAVE_TIME"},


    /*------------------------LOGOFF SECTION ---------------------------------*/

    {LOGOFF, "LOGOFF"},
    /* if logoff is yes - then the optional fields follow: */

    {LOGOFF_REQ_TYPE, "LOGOFF_REQ_TYPE"},
    {LOGOFF_POST_STR, "LOGOFF_POST_STR"},
    {LOGOFF_URL, "LOGOFF_URL"},
    {LOGOFF_URL_MAX_TIME, "LOGOFF_URL_MAX_TIME"},
    {LOGOFF_URL_INTERLEAVE_TIME,"LOGOFF_URL_INTERLEAVE_TIME"},
    {LOGOFF_CYCLING, "LOGOFF_CYCLING"},
  
    {0, NULL}
  };


static 
int add_param_to_batch (char*const str_buff, 
                        size_t  str_len,
                        batch_context*const bctx, 
                        int*const batch_num)
{
  if (!str_buff || !str_len || !bctx)
    return -1;

  /*We are not eating LWS, as it supposed to be done before... */
    
  char* equal = NULL;

  if ( ! (equal = strchr (str_buff, '=')))
    {
      fprintf (stderr, 
               "%s - error: input string \"%s\" is short of '=' sign.\n", 
               __func__, str_buff) ;
      return -1;
    }
  else
    {
      *equal = '\0'; /* The idea from Igor Potulnitsky */
    }

  if (bctx->batch_init_state == NOT_INIT)
    {
      bctx->batch_init_state++; /* Welcome to the new batch. */
    }
    
  if (! strstr (str_buff, bp_map[bctx->batch_init_state].str))
    {
      if (bctx->batch_init_state ==  UAS_URL &&
          strstr (str_buff, bp_map[LOGOFF].str))
        {
          bctx->batch_init_state = LOGOFF; /* UAS triplets end on LOGOFF */
        }
      else if (!strstr (str_buff, bp_map[BATCH_NAME].str))
        {
          fprintf (stderr, "\n%s - error: in batch \"%s\"\n", 
                   __func__, bctx->batch_name);
          return -1;
        }
      else
        {
          fprintf (stderr, "%s - error: parameter %s is expected.\n", 
                   __func__, bp_map[bctx->batch_init_state].str);
          return -1;
        }
    }

  int value_len = 0;
  if ((value_len = str_len - (equal - str_buff) - 1) < 0)
    {
      *equal = '=' ;
      fprintf(stderr, "%s - error: in \"%s\" a valid name should follow '='.\n", 
               __func__, str_buff);
      return -1;
    }

  if (set_value_to_param (bctx, equal + 1, value_len) == -1)
    {
      fprintf (stderr, 
               "%s - error: set_value_to_param () failed for state %d, param %s and value %s.\n", 
               __func__, bctx->batch_init_state, str_buff, equal + 1);
      return -1;
    }

  advance_batch_parser_state (bctx);

  if (bctx->batch_init_state == VALID_BATCH)
    ++(*batch_num);
    
  return 0;
}

static void advance_batch_parser_state (batch_context*const bctx)
{
  if (bctx->batch_init_state == VALID_BATCH)
    return;

  switch (bctx->batch_init_state)
    {
    case LOGIN:
      if (! bctx->do_login) /* If Login is of no interest - jump to UAS */
          bctx->batch_init_state = UAS;
      else
           ++bctx->batch_init_state;
      break;
      
    case UAS:
      if (! bctx->do_uas) /* if UAS is of no interest - jump to LOGOFF */
          bctx->batch_init_state = LOGOFF;
      else
           ++bctx->batch_init_state;
      break;

    case UAS_URL_INTERLEAVE_TIME:
      bctx->batch_init_state = UAS_URL; /* There is a possiblity, that the next 
                                           state will be LOGOFF instead. Treated upper. */
      break;

    case LOGOFF:
      if (! bctx->do_logoff) /* If Logoff is of no interest - go forward to  VALID_BATCH */
          bctx->batch_init_state = VALID_BATCH;
      else
           ++bctx->batch_init_state;
      break;
 
    default: /*advance to the next state */
      ++bctx->batch_init_state;
      break;
    }

  return;
}

static int set_value_to_param (
                               batch_context*const bctx, 
                               char*const value, 
                               size_t value_length)
{
  if (!bctx || !value || !value_length)
    {
      fprintf (stderr, "%s - error: wrong input.\n", __func__);
      return -1;
    }
  size_t length = value_length; 

  /* remove LWS */
  char* value_start = NULL;
  if ( ! (value_start = eat_ws (value, &length)))
    {
      fprintf (stderr, "%s - error: only LWS found in the value \"%s\".\n", 
               __func__, value);
      fprintf (stderr, "%s - if the field is not applicable, place %s .\n", 
               __func__, NON_APPLICABLE_STR);
      return -1;
    }

  /* Cut-off the comments in value string, starting from '#' */
  char* comments = NULL;
  if ((comments = strchr (value_start, '#')))
    {
      *comments = '\0'; /* The idea from Igor Potulnitsky */
      if (! (length = strlen (value_start)))
        {  
          fprintf (stderr, "%s - error: value \"%s\" has only comments.\n", 
                   __func__, value);
          fprintf (stderr, "%s - if the field is not applicable, place %s .\n", 
                   __func__, NON_APPLICABLE_STR);
          return -1;
        }
    }

  /* remove TWS */
  char* value_end = skip_non_ws (value_start, &length);
  if (value_end)
    {
      *value_end = '\0';
    }

  struct in_addr in_address;
  memset (&in_address, 0, sizeof (struct in_addr));
  size_t url_length = 0;

  /*
    The most Object-Oriented switch.

    TODO: consider splitting it into some functions, 
    e.g. according to the sections below.
  */
  switch (bctx->batch_init_state)
    {

      /*------------------------ GENERAL SECTION ------------------------------ */

    case BATCH_NAME:
      strncpy (bctx->batch_name, value_start, sizeof (bctx->batch_name) -1);
      break;

    case CLIENTS_NUM:
      bctx->client_num = 0;
      bctx->client_num = atoi (value_start);

      /* fprintf (stderr, "\nclients number is %d\n", bctx->client_num); */

      if (bctx->client_num < 1 || bctx->client_num > BATCH_MAX_CLIENTS_NUM)
        {
          fprintf (stderr, "%s - error: clients number (%d) is out of the range\n", 
                   __func__, bctx->client_num);
          return -1;
        }

      /*
      if (! (bctx->client_handles_array = 
             (CURL **)calloc (bctx->client_num, sizeof (CURL *))))
        {
          fprintf (stderr, 
                   "%s - error: failed to allocate bctx->client_handles_array\n", 
                   __func__);
          return -1;
        }
      */
      break;

    case INTERFACE:
      strncpy (bctx->net_interface, value_start, sizeof (bctx->net_interface) -1);
      break;

    case NETMASK: /* CIDR number of non-masked first bits -16, 24, etc */
      bctx->cidr_netmask = atoi (value_start);
      if (bctx->cidr_netmask < 1 || bctx->cidr_netmask > 32)
        {
          fprintf (stderr, 
                   "%s - error: network mask (%d) is out of the range\n", 
                   __func__, bctx->cidr_netmask);
          return -1;
        }
      break;

    case IP_ADDR_MIN:
      if (! inet_aton (value_start, &in_address))
        {
          fprintf (stderr, 
                   "%s - error: inet_aton failed for ip_addr_min %s\n", 
                   __func__, value_start);
          return -1;
        }
      bctx->ip_addr_min = ntohl (in_address.s_addr);
      break;

    case IP_ADDR_MAX: /* We have number of clients, therefore 
                         this IP-address is more or less for self-control. */
     
      if (!inet_aton (value_start, &in_address))
        {
          fprintf (stderr, 
                   "%s - error: inet_aton failed for ip_addr_max %s\n", 
                   __func__, value_start);
          return -1;
        }
      bctx->ip_addr_max = ntohl (in_address.s_addr);
      break;

    case CYCLES_NUM: /*zero means forever */
      bctx->cycles_num = atol (value_start);
      if (bctx->cycles_num <= 0)
        {
          fprintf (stderr, 
                   "%s - note: cycles_num (%s) should be 0  or positive\n", 
                   __func__, value_start);
          bctx->cycles_num = LONG_MAX - 1;
        }
      break;


   /*------------------------ LOGIN SECTION -------------------------------- */

    case LOGIN:
      bctx->do_login = (*value_start == 'Y' || *value_start == 'y') ? 1 : 0;
      break;

    case LOGIN_USERNAME:
      strncpy (bctx->login_username, value_start, sizeof(bctx->login_username) - 1);
      break;

    case LOGIN_PASSWORD:
      strncpy (bctx->login_password, value_start, sizeof(bctx->login_password) - 1);
      break;

    case LOGIN_REQ_TYPE:
      if (!strcmp (value_start, REQ_GET_POST))
          bctx->login_req_type = LOGIN_REQ_TYPE_GET_AND_POST;
      else if (!strcmp (value_start, REQ_POST))
          bctx->login_req_type = LOGIN_REQ_TYPE_POST;
      else
        {
          fprintf (stderr, 
                   "%s - error: LOGIN_REQ_TYPE (%s) is not valid. Use %s or %s .\n", 
                   __func__, value_start, REQ_GET_POST, REQ_POST);
          return -1;
        }        
      break;

    case LOGIN_POST_STR:
      // TODO: validate the input. Important !!!.
      if (strcmp (value_start, NON_APPLICABLE_STR))
        {
          strncpy (bctx->login_post_str, 
                   value_start, 
                   sizeof (bctx->login_post_str) - 1);
        }
      break;

    case  LOGIN_URL:
      if ((url_length = strlen (value_start)) <= 0)
        {
          fprintf(stderr, "%s - error: empty url for \"%s\"\n",  __func__,value_start);
          return -1;
        }
      if (!(bctx->login_url.url_str =(char *)calloc(url_length+1,sizeof (char))))
        {
          fprintf (stderr, 
                   "%s - error: login_url allocation failed for url \"%s\"\n", 
                   __func__, value_start);
          return -1;
        }
      strcpy(bctx->login_url.url_str, value_start);
      bctx->login_url.url_lstep = URL_LOAD_LOGIN;
      bctx->login_url.url_appl_type = url_schema_classification (value_start);
      bctx->login_url.url_uas_num = -1; // means N/A
      break;

    case  LOGIN_URL_MAX_TIME:
      bctx->login_url.url_completion_time = atof (value_start);
      break;

    case  LOGIN_URL_INTERLEAVE_TIME:
      bctx->login_url.url_interleave_time = atoi(value_start);
      break;

    case LOGIN_CYCLING:
      bctx->login_cycling = (*value_start == 'Y' || *value_start == 'y') ? 1 : 0;
      break;


      /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */

    case UAS:
      bctx->do_uas = (*value_start == 'Y' || *value_start == 'y') ? 1 : 0;
      break;

    case UAS_URLS_NUM:
      bctx->uas_urls_num = atoi (value_start);
      if (bctx->uas_urls_num < 1)
        {
          fprintf (stderr, 
                   "%s - error: urls_num (%s) should be one or more.\n",
                   __func__, value_start);
          return -1;
        }    
      /* Preparing the staff to load URLs and handles */
      if (! (bctx->uas_url_ctx_array = 
             (url_context *)calloc (bctx->uas_urls_num, sizeof (url_context))))
        {
          fprintf (stderr, 
                   "%s - error: failed to allocate URL-context array for %d urls\n", 
                   __func__, bctx->uas_urls_num);
          return -1;
        }
      bctx->url_index = 0;  /* Starting from the 0 position in the arrays */
      break;

    case  UAS_URL:
      if ((int)bctx->url_index >= bctx->uas_urls_num)
        {
          fprintf (stderr, 
                   "%s - error: UAS_URL_NUM (%d) is below uas-url triplets number in conf-file.\n",
                   __func__, bctx->url_index);
          return -1;
        }

      if ((url_length = strlen (value_start)) <= 0)
        {
          fprintf(stderr, "%s - error: url is not correct with\"%s\"\n", 
                  __func__, value_start);
          return -1;
        }

      if (! (bctx->uas_url_ctx_array[bctx->url_index].url_str = 
             (char *) calloc (url_length +1, sizeof (char))))
        {
          fprintf (stderr, 
                   "%s - error: allocation failed for url string \"%s\"\n", 
                   __func__, value_start);
          return -1;
        }
      strcpy(bctx->uas_url_ctx_array[bctx->url_index].url_str, value_start);
      bctx->uas_url_ctx_array[bctx->url_index].url_lstep = URL_LOAD_UAS;
      bctx->uas_url_ctx_array[bctx->url_index].url_appl_type = url_schema_classification (value_start);
      bctx->uas_url_ctx_array[bctx->url_index].url_uas_num = bctx->url_index;
      break;

    case  UAS_URL_MAX_TIME:
      bctx->uas_url_ctx_array[bctx->url_index].url_completion_time = 
        atof (value_start);
      break;

    case  UAS_URL_INTERLEAVE_TIME:
      bctx->uas_url_ctx_array[bctx->url_index].url_interleave_time = 
        atoi(value_start);
      bctx->url_index++; /* advance the position */
      break;

      /*------------------------LOGOFF SECTION ---------------------------------*/

    case LOGOFF:
      bctx->do_logoff = (*value_start == 'Y' || *value_start == 'y') ? 1 : 0;
      break;

      /* Username and password logoff authentication - looks like
         future options to support. */

    case LOGOFF_REQ_TYPE:
      if (!strcmp (value_start, REQ_GET_POST)) 
          bctx->logoff_req_type = LOGOFF_REQ_TYPE_GET_AND_POST;
      else if (!strcmp (value_start, REQ_GET))
        bctx->logoff_req_type = LOGOFF_REQ_TYPE_GET;
      else if (!strcmp (value_start, REQ_POST))
        bctx->logoff_req_type = LOGOFF_REQ_TYPE_POST;
      else
        {
          fprintf (stderr, 
                   "%s - error: LOGOFF_REQ_TYPE (%s) is not valid. Consider %s, %s or %s.\n", 
                   __func__, value_start, REQ_GET, REQ_POST, REQ_GET_POST);
          return -1;
        }
      break;

    case LOGOFF_POST_STR:
      if (strcmp (value_start, NON_APPLICABLE_STR))
        {
          strncpy (bctx->logoff_post_str, 
                   value_start, 
                   sizeof (bctx->logoff_post_str) - 1);
        }
      break;

    case  LOGOFF_URL:
      if ((url_length = strlen (value_start)) <= 0)
        {
          fprintf(stderr, "%s - error: empty url for \"%s\"\n", __func__,value_start);
          return -1;
        }
 
      if (!(bctx->logoff_url.url_str =(char *)calloc(url_length+1,sizeof (char))))
        {
          fprintf (stderr, 
                   "%s - error: logoff_url allocation failed for url \"%s\"\n", 
                   __func__, value_start);
          return -1;
        }
      strcpy(bctx->logoff_url.url_str, value_start);
      bctx->logoff_url.url_lstep = URL_LOAD_LOGOFF;
      bctx->logoff_url.url_appl_type = url_schema_classification (value_start);
      bctx->logoff_url.url_uas_num = -1; // means N/A 
      break;

    case  LOGOFF_URL_MAX_TIME:
      bctx->logoff_url.url_completion_time = atof (value_start);
      break;

    case  LOGOFF_URL_INTERLEAVE_TIME: /* means time to be up after logoff */
      bctx->logoff_url.url_interleave_time = atoi(value_start);
      break;

    case LOGOFF_CYCLING:
      bctx->logoff_cycling = (*value_start == 'Y' || *value_start == 'y') ? 1 : 0;
      break;
      

    default:
      fprintf (stderr, 
               "%s - some parsing error, Sir. Falling into the switch default.\n",
               __func__);
      return -1;

    } /* 'from switch' */

  return 0;
}

static url_appl_type 
url_schema_classification (const char* const url)
{
  if (!url)
    {
      return  URL_APPL_UNDEF;
    }

#define HTTPS_SCHEMA_STR "https://"
#define HTTP_SCHEMA_STR "http://"
#define FTPS_SCHEMA_STR "ftps://"
#define FTP_SCHEMA_STR "ftp://"

  if (strstr (url, HTTPS_SCHEMA_STR))
      return URL_APPL_HTTPS;
  else if (strstr (url, HTTP_SCHEMA_STR))
    return URL_APPL_HTTP;
  else if (strstr (url, FTPS_SCHEMA_STR))
    return URL_APPL_FTPS;
  else if (strstr (url, FTP_SCHEMA_STR))
    return URL_APPL_FTP;

  return  URL_APPL_UNDEF;
}

/*
  Eats leading white space. Returns pointer to the start of 
  the non-white-space or NULL. Returns via len a new length.
*/
char* eat_ws (char* ptr, size_t*const len)
{
  if (!ptr || !*len)
    return NULL;

  while (*len && is_ws (ptr))
    ++ptr, --(*len);

  return *len ? ptr : NULL;
}

static char* skip_non_ws (char*ptr, size_t*const len)
{
  if (!ptr || !*len)
    return NULL;

  while (*len && is_non_ws (ptr))
    ++ptr, --(*len);

  return *len ? ptr : NULL;
}

static int is_ws (char*const ptr)
{
  return (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') ? 1 : 0;
}

static int is_non_ws (char*const ptr)
{
  return ! is_ws (ptr);
}
