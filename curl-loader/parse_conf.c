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

/*
  Enumerations mapped to the string in the configuration file.
  Each enum means required string, whereas url blocks:
  URL,
  URL_MAX_TIME,
  URL_INTERLEAVE_TIME,

  may be repeated several times, supporting emulation of the several 
  url fetches by a single client.

  Note, that URL_MAX_TIME and URL_INTERLEAVE_TIME are
  relevant only for loading using storming mode.
*/
enum
{
    NOT_INIT = 0,
    BATCH_NAME,
    CLIENTS_NUM,
    INTERFACE,
    NETMASK,
    IP_ADDR_MIN,
    IP_ADDR_MAX,
    USERNAME,
    PASSWORD,
    CYCLES_NUM,
    URLS_NUM,

    URL,
    URL_MAX_TIME,
    URL_INTERLEAVE_TIME,

    VALID_BATCH,
};

/*
  Used to map parameter by id to its name used in
  configuration file. Assists in parsing.
*/
typedef struct batch_params_map
{
  int        param_id;
  char*   param_str; /* string name of the param */
} batch_params_map;

#define BATCH_MAX_CLIENTS_NUM 4096



static int add_param_to_batch (char*const input, size_t input_length,
                               batch_context*const bctx, int*const batch_num);
static int set_value_to_param (batch_context*const bctx, char*const value, 
                               size_t value_length);
static void advance_batch_parser_state (int*const parser_state);
static char* skip_non_ws (char*ptr, size_t*const len);
static char* eat_ws (char*ptr, size_t*const len);
static int is_ws (char*const ptr);
static int is_non_ws (char*const ptr);

int parse_config_file (char* const filename, batch_context* bctx_array, 
                       size_t bctx_array_size)
{
  char fgets_buff[2048];
  size_t batches_number = 0;
  FILE* fp;
  struct stat statbuf;

  /* Check, if the configuration file exists. */
  if (stat (filename, &statbuf) == -1)
    {
      fprintf (stderr, 
               "%s - failed to stat file \"%s\" with errno %d.\n", 
               __func__, filename, errno);
      return -1;
    }

  if (!(fp = fopen (filename, "r")))
    {
      fprintf (stderr, 
               "%s - fopen() failed to open for read file \"%s\", errno %d.\n", 
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
          if (batches_number >= bctx_array_size)
            {
              fprintf (stderr, 
                       "%s - error: maximum batches number limit (%d) reached \n", 
                       __func__, bctx_array_size);
              fclose (fp);
              return -1 ;
            }
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
                       "%s - create_batch_data_array () processing string \"%s\"\n", 
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
                   "%s - failed to load even a single valid batch\n", __func__);
      fprintf (stderr, 
                   "Most probably URLS_NUM is not equal to the specified url count.\n");
      fprintf (stderr, 
                   "or missed URL_MAX_TIME/URL_INTERLEAVE_TIME params.\n");
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
    {BATCH_NAME, "BATCH_NAME"},
    {CLIENTS_NUM, "CLIENTS_NUM"},
    {INTERFACE,"INTERFACE"},
    {NETMASK, "NETMASK"},
    {IP_ADDR_MIN, "IP_ADDR_MIN"},
    {IP_ADDR_MAX, "IP_ADDR_MAX"},
    {USERNAME, "USERNAME"},
    {PASSWORD,"PASSWORD"},
    {CYCLES_NUM,"CYCLES_NUM"},
    {URLS_NUM,"URLS_NUM"},
  
    {URL,"URL"},
    {URL_MAX_TIME,"URL_MAX_TIME"},
    {URL_INTERLEAVE_TIME,"URL_INTERLEAVE_TIME"},
  
    {0, NULL}   
  };


static int add_param_to_batch (char*const input, size_t  input_length,
                    batch_context*const bctx, int*const batch_num)
{
  if (!input || !input_length || !bctx)
    return -1;

  /*We are not eating LWS, as it supposed to be done before... */
    
  char* equal = NULL;

  if ( ! (equal = strchr (input, '=')))
    {
      fprintf (stderr, 
               "%s - error: input string \"%s\" is short of '=' sign.\n", 
               __func__, input) ;
      return -1;
    }
  else
    {
      *equal = '\0'; /* The idea from Igor Potulnitsky */
    }

  if (bctx->batch_init_state == NOT_INIT) /* New coming batch is welcome. */
    bctx->batch_init_state++;
    
  if (! strstr (input, bp_map[bctx->batch_init_state].param_str))
    {
      if (!strstr (input, bp_map[BATCH_NAME].param_str))
        {
          fprintf (stderr, 
                   "\n%s - error: in batch \"%s\"; correct URLS_NUM (%d)\n", 
                   __func__, bctx->batch_name, bctx->urls_num);
          fprintf (stderr, "prior to defining the next batch \"%s\".\n\n", 
                   input);
          return -1;
        }
      else
        {
          fprintf (stderr, "%s - error: parameter %s is expected.\n", 
                   __func__, bp_map[bctx->batch_init_state].param_str);
          return -1;
        }
    }

  int value_len = 0;
  if ((value_len = input_length - (equal - input) - 1) < 0)
    {
      *equal = '=' ;
      fprintf (stderr, 
               "%s - error: in \"%s\" a non-empty value after '=' is expected.\n", 
               __func__, input);
      return -1;
    }

  if (set_value_to_param (bctx, equal + 1, value_len) == -1)
    {
      fprintf (stderr, 
               "%s - error: set_value_to_param () failed for state %d, param %s and value %s.\n", 
               __func__, bctx->batch_init_state, input, equal + 1);
      return -1;
    }

  advance_batch_parser_state (&bctx->batch_init_state);

  if (bctx->batch_init_state == VALID_BATCH)
    ++(*batch_num);
    
  return 0;
}

static void advance_batch_parser_state (int*const parser_state)
{
  if (*parser_state == VALID_BATCH)
    return;

  if (*parser_state == URL_INTERLEAVE_TIME)
    {
      *parser_state = URL;
    }
  else
    {
      ++(*parser_state);
    }
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

static int set_value_to_param (batch_context*const bctx, char*const value, 
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
      return -1;
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
    The most Object-Oriented switch
  */
  switch (bctx->batch_init_state)
    {
    case BATCH_NAME:
      strncpy (bctx->batch_name, value_start, sizeof (bctx->batch_name) -1);
      break;

    case INTERFACE:
      strncpy (bctx->net_interface, value_start, sizeof (bctx->net_interface) -1);
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

      if (! (bctx->client_handles_array = 
             (CURL **)calloc (bctx->client_num, sizeof (CURL *))))
        {
          fprintf (stderr, 
                   "%s - error: failed to allocate bctx->client_handles_array\n", 
                   __func__);
          return -1;
        }
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
                         the address is more or less for programmer self-control. */
     
      if (!inet_aton (value_start, &in_address))
        {
          fprintf (stderr, 
                   "%s - error: inet_aton failed for ip_addr_max %s\n", 
                   __func__, value_start);
          return -1;
        }
      bctx->ip_addr_max = ntohl (in_address.s_addr);
      break;

    case USERNAME:
      strncpy (bctx->username, value_start, sizeof(bctx->username) -1);
      bctx->do_auth = strcmp (bctx->username, "NO") ? 1 : 0;
      break;

    case PASSWORD: /* When NO set in username, set NO here as well */
      strncpy (bctx->password, value_start, sizeof(bctx->password) -1);
      break;

    case CYCLES_NUM: /*zero means forever */
      bctx->repeat_cycles_num = atol (value_start);
      if (bctx->repeat_cycles_num <= 0)
        {
          fprintf (stderr, 
                   "%s - note: repeat_cycles_num (%s) should be 0  or positive\n", 
                   __func__, value_start);
          bctx->repeat_cycles_num = LONG_MAX - 1;
        }
      break;

    case URLS_NUM:
      bctx->urls_num = atoi (value_start);
      if (bctx->urls_num < 1)
        {
          fprintf (stderr, 
                   "%s - error: urls_num (%s) should be one or more.\n", 
                   __func__, value_start);
          return -1;
        }    
      /* Preparing the staff to load URLs and handles */
      if (! (bctx->url_ctx_arr = 
             (url_context *)calloc (bctx->urls_num, sizeof (url_context))))
        {
          fprintf (stderr, 
                   "%s - error: failed to allocate URL-context array for %d urls\n", 
                   __func__, bctx->urls_num);
          return -1;
        }

      bctx->url_index = 0;  /* Starting from the 0 position in the arrays */
      break;

    case  URL:
      if ((int)bctx->url_index >= bctx->urls_num)
        {
          fprintf (stderr, 
                   "%s - skiping loading a new url after %d url\n",
                   __func__, bctx->url_index);
          return 0;
        }

      if ((url_length = strlen (value_start)) <= 0)
        {
          fprintf (stderr, 
                   "%s - empty of error value, strlen returns %d for \"%s\"\n", 
                   __func__, url_length, value_start);
          return -1;
        }

      if (! (bctx->url_ctx_arr[bctx->url_index].url_str = 
             (char *) calloc (url_length +1, sizeof (char))))
        {
          fprintf (stderr, 
                   "%s - error: allocation failed for url string \"%s\"\n", 
                   __func__, value_start);
          return -1;
        }
      strcpy(bctx->url_ctx_arr[bctx->url_index].url_str, value_start);
      break;

    case  URL_MAX_TIME:
      if ((int)bctx->url_index >= bctx->urls_num)
        {
          fprintf (stderr, 
                   "%s - skiping loading a url_completion_time after %d url\n",
                   __func__, bctx->url_index);
          return 0;
        }
      bctx->url_ctx_arr[bctx->url_index].url_completion_time = atof (value_start);
      break;

    case  URL_INTERLEAVE_TIME:
      if ((int)bctx->url_index >= bctx->urls_num)
        {
          fprintf (stderr, 
                   "%s - skiping loading a url_inter_sleeping_time after %d url\n",
                   __func__, bctx->url_index);
          return 0;
        }

      bctx->url_ctx_arr[bctx->url_index].url_interleave_time = atoi(value_start);
      bctx->url_index++; /* advance the position */

      if ((int)bctx->url_index >= bctx->urls_num)
        {
          bctx->batch_init_state = VALID_BATCH ;
          fprintf (stderr, "%s - completing loading url after loading %d url\n",
                   __func__, bctx->url_index);
          return 0;
        }
      break;

    default:
      fprintf (stderr, 
               "%s - some parsing error, Sir. Falling into the switch default.\n",
               __func__);
      return -1;

    } /* 'from switch' */

  return 0;
}
