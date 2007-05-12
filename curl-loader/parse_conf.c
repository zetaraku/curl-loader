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

// must be first include
#include "fdsetsize.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "conf.h"
#include "batch.h"
#include "client.h"
#include "cl_alloc.h"

#define EXPLORER_USERAGENT_STR "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0)" 
#define BATCH_MAX_CLIENTS_NUM 4096

#define NON_APPLICABLE_STR ""

#define REQ_GET "GET"
#define REQ_POST "POST"
#define REQ_PUT "PUT"

#define FT_UNIQUE_USERS_AND_PASSWORDS "UNIQUE_USERS_AND_PASSWORDS"
#define FT_UNIQUE_USERS_SAME_PASSWORD "UNIQUE_USERS_SAME_PASSWORD"
#define FT_SINGLE_USER "SINGLE_USER"
#define FT_RECORDS_FROM_FILE "RECORDS_FROM_FILE"
#define FT_AS_IS "AS_IS"


/*
  value - supposed to be a null-terminated string.
*/
typedef int (*fparser) (batch_context*const bctx, char*const value);

/*
  Used to map a tag to its value parser function.
*/
typedef struct tag_parser_pair
{
    char* tag; /* string name of the param */
    fparser parser;
} tag_parser_pair;

/* 
 * Declarations of tag parsing functions.
*/

/*
 * GENERAL section tag parsers. 
*/
static int batch_name_parser (batch_context*const bctx, char*const value);
static int clients_num_max_parser (batch_context*const bctx, char*const value);
static int clients_num_start_parser (batch_context*const bctx, char*const value);
static int interface_parser (batch_context*const bctx, char*const value);
static int netmask_parser (batch_context*const bctx, char*const value);
static int ip_addr_min_parser (batch_context*const bctx, char*const value);
static int ip_addr_max_parser (batch_context*const bctx, char*const value);
static int cycles_num_parser (batch_context*const bctx, char*const value);
static int clients_initial_inc_parser (batch_context*const bctx, char*const value);
static int user_agent_parser (batch_context*const bctx, char*const value);
static int urls_num_parser (batch_context*const bctx, char*const value);

/*
 * URL section tag parsers. 
*/
static int url_parser (batch_context*const bctx, char*const value);
static int url_short_name_parser (batch_context*const bctx, char*const value);
static int url_use_current_parser (batch_context*const bctx, char*const value);
static int url_dont_cycle_parser (batch_context*const bctx, char*const value);
static int header_parser (batch_context*const bctx, char*const value);
static int request_type_parser (batch_context*const bctx, char*const value);

static int username_parser (batch_context*const bctx, char*const value);
static int password_parser (batch_context*const bctx, char*const value);
static int form_usage_type_parser (batch_context*const bctx, char*const value);
static int form_string_parser (batch_context*const bctx, char*const value);
static int form_records_file_parser (batch_context*const bctx, char*const value);

static int upload_file_parser (batch_context*const bctx, char*const value);

static int web_auth_method_parser (batch_context*const bctx, char*const value);
static int web_auth_credentials_parser (batch_context*const bctx, char*const value);
static int proxy_auth_method_parser (batch_context*const bctx, char*const value);
static int proxy_auth_credentials_parser (batch_context*const bctx, char*const value);

static int connection_reestablish_parser (batch_context*const bctx, char*const value);

static int timer_tcp_conn_setup_parser (batch_context*const bctx, char*const value);
static int timer_url_completion_parser (batch_context*const bctx, char*const value);
static int timer_after_url_sleep_parser (batch_context*const bctx, char*const value);


static fparser find_tag_parser (const char* tag);

/*
 * The mapping between tag strings and parsing functions.
 */
static const tag_parser_pair tp_map [] =
{
    /*------------------------ GENERAL SECTION ------------------------------ */
    {"BATCH_NAME", batch_name_parser},
    {"CLIENTS_NUM_MAX", clients_num_max_parser},
    {"CLIENTS_NUM_START", clients_num_start_parser},
    {"CLIENTS_INITIAL_INC", clients_initial_inc_parser},
    {"INTERFACE", interface_parser},
    {"NETMASK", netmask_parser},
    {"IP_ADDR_MIN", ip_addr_min_parser},
    {"IP_ADDR_MAX", ip_addr_max_parser},
    {"CYCLES_NUM", cycles_num_parser},
    {"USER_AGENT", user_agent_parser},
    {"URLS_NUM", urls_num_parser},
    

    /*------------------------ URL SECTION -------------------------------- */

    {"URL", url_parser},
    {"URL_SHORT_NAME", url_short_name_parser},
    {"URL_USE_CURRENT", url_use_current_parser},
    {"URL_DONT_CYCLE", url_dont_cycle_parser},
    {"HEADER", header_parser},
    {"REQUEST_TYPE", request_type_parser},

    {"USERNAME", username_parser},
    {"PASSWORD", password_parser},
    {"FORM_USAGE_TYPE", form_usage_type_parser},
    {"FORM_STRING", form_string_parser},
    {"FORM_RECORDS_FILE", form_records_file_parser},

    {"UPLOAD_FILE", upload_file_parser},

    {"WEB_AUTH_METHOD", web_auth_method_parser},
    {"WEB_AUTH_CREDENTIALS", web_auth_credentials_parser},
    {"PROXY_AUTH_METHOD", proxy_auth_method_parser},
    {"PROXY_AUTH_CREDENTIALS", proxy_auth_credentials_parser},

    {"CONNECTION_REESTABLISH", connection_reestablish_parser},

    {"TIMER_TCP_CONN_SETUP", timer_tcp_conn_setup_parser},
    {"TIMER_URL_COMPLETION", timer_url_completion_parser},
    {"TIMER_AFTER_URL_SLEEP", timer_after_url_sleep_parser},

    {NULL, 0}
};

static int validate_batch (batch_context*const bctx);
static int validate_batch_general (batch_context*const bctx);
static int validate_batch_url (batch_context*const bctx);

static int post_validate_init (batch_context*const bctx);
static int load_form_records_file (batch_context*const bctx, url_context* url);
static int load_form_record_string (char*const input, 
                             size_t input_length,
                             form_records_cdata* form_record,
                             size_t record_num,
                             char* separator);

static int add_param_to_batch (char*const input, 
                               size_t input_length,
                               batch_context*const bctx, 
                               int*const batch_num);

static int pre_parser (char** ptr, size_t* len);
static url_appl_type url_schema_classification (const char* const url);
static char* skip_non_ws (char*ptr, size_t*const len);
static char* eat_ws (char*ptr, size_t*const len);
static int is_ws (char*const ptr);
static int is_non_ws (char*const ptr);

static int find_first_cycling_url (batch_context* bctx);
static int find_last_cycling_url (batch_context* bctx);
static int netmask_to_cidr (char *dotted_ipv4);
static int print_correct_form_usagetype (form_usagetype ftype, char* value);


/****************************************************************************************
* Function name - find_tag_parser
*
* Description - Makes a look-up of a tag value parser function for an input tag-string 
* 
* Input -       *tag - pointer to the tag string, coming from the configuration file
* Return Code/Output - On success - parser function, on failure - NULL
****************************************************************************************/
static fparser find_tag_parser (const char* tag)
{
    size_t index;

    for (index = 0; tp_map[index].tag; index++)
    {
        if (!strcmp (tp_map[index].tag, tag))
            return tp_map[index].parser;
    }    
    return NULL;
}

/****************************************************************************************
* Function name - add_param_to_batch
*
* Description - Takes configuration file string of the form TAG = value and extacts
*               loading batch configuration parameters from it.
* 
* Input -       *str_buff   - pointer to the configuration file string of the form TAG = value
*               str_len     - length of the <str_buff> string
*               *bctx_array - array of the batch contexts
* Input/Output  batch_num   - index of the batch to fill and advance, when required.
*                             Still supporting multiple batches in one batch file.
*
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int add_param_to_batch (
    char*const str_buff, 
    size_t  str_len,
    batch_context*const bctx_array, 
    int*const batch_num)
{
  if (!str_buff || !str_len || !bctx_array)
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

  int value_len = 0;
  if ((value_len = str_len - (equal - str_buff) - 1) < 0)
    {
      *equal = '=' ;
      fprintf(stderr, "%s - error: in \"%s\" a valid name should follow '='.\n", 
               __func__, str_buff);
      return -1;
    }

  /* remove TWS */
  str_len = strlen (str_buff) + 1;
  char* str_end = skip_non_ws (str_buff, &str_len);
  if (str_end)
      *str_end = '\0';
  
  /* Lookup for value parsing function for the input tag */
  fparser parser = 0;
  if (! (parser = find_tag_parser (str_buff)))
  {
      fprintf (stderr, "%s - warning: unknown tag %s.\n",__func__, str_buff);
      return 0;
  }

  /* Removing LWS, TWS and comments from the value */
  char* value = equal + 1;
  if (pre_parser (&value, (unsigned int *)&value_len) == -1)
  {
      fprintf (stderr,"%s - error: pre_parser () failed for tag %s and value \"%s\".\n",
               __func__, str_buff, equal + 1);
      return -1;
  }

  if (!strlen (value))
    {
      fprintf (stderr,"%s - warning: tag %s has an empty value string.\n",
               __func__, str_buff);
      return 0;
    }

  /* Remove quotes from the value */
  if (*value == '"')
    {
      value++, value_len--;
      if (value_len < 2)
        {
          return 0;
        }
      else
        {
          if (*(value +value_len-2) == '"')
            {
              *(value +value_len-2) = '\0';
              value_len--;
            }
        }
    }

  if (strstr (str_buff, tp_map[0].tag))
  {
      /* On string "BATCH_NAME" - next batch and move the number */
       ++(*batch_num);
  }

  if ((*parser) (&bctx_array[*batch_num], value) == -1)
    {
      fprintf (stderr,"%s - parser failed for tag %s and value %s.\n",
               __func__, str_buff, equal + 1);
      return -1;
    }

  return 0;
}

/****************************************************************************************
* Function name - load_form_record_string
*
* Description - Parses string with credentials <user>SP<password>, allocates at virtual 
*               client memory and places the credentials to the client post buffer.
* 
* Input -       *input        - pointer to the credentials file string
*               input_len     - length of the <input> string
*               *bctx         - batch context to which the initia
* Input/Output  *client_num   - index of the client in the array
*               *separator    - the separating symbol initialized by the first string and 
*                               further used.
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int load_form_record_string (char*const input, 
                             size_t input_len,
                             form_records_cdata* form_record, 
                             size_t record_num,
                             char* separator)
{
  const char separators_supported [] =
    {
      ':', ' ', '@', '/', '\0'
    };
  char* sp = NULL;
  int i;
  
  /* 
     Figure out the separator used by the first string analyses 
  */
  if (! record_num)
    {
      for (i = 0; separators_supported [i]; i++)
        {
          if ((sp = strchr (input, separators_supported [i])))
            {
              *separator = *sp; /* Remember the separator */
              break;
            }
        }

      if (!separators_supported [i])
        {
          fprintf (stderr,
                   "%s - failed to locate in the first string \"%s\" \n" 
                   "any supported separator.\nThe supported separators are:\n",
               __func__, input);
          for (i = 0; separators_supported [i]; i++)
            {
              fprintf (stderr,"\"%c\"\n", separators_supported [i]);
            }
          return -1;
        }
    }

  if ((sp = strchr (input, *separator)))
    {
      *sp = '\0'; /* The idea from Igor Potulnitsky */
    }
  else
    {
      fprintf (stderr, "%s - separator not found.\n", __func__);
      return -1;
    }

  char* username = input;
  char* password = NULL;

  if ((input_len - (sp - input) - 1) > 0)
    {
      sp = sp + 1;

      if (strlen (sp))
        {
          password = sp;
        }
    }

  /*
    Empty passwords are allowed.
  */
  size_t len_username = 0, len_password = 0;

  /* 
     TODO: The tokens below to be treated in a cycle and up to 8
     tokens to be supported
  */
  if (username)
  {
      if (! (len_username = strlen (username)))
      {
          fprintf (stderr, "%s - even the very first token of the record is empty. \n", __func__);
          return -1;
      }

      if (! (form_record->form_tokens[0] = calloc (len_username +1, sizeof (char))))
      {
          fprintf (stderr, "%s - calloc() for username failed with errno %d\n", __func__, errno);
          return -1;
      }
      else
      {
          strcpy (form_record->form_tokens[0], username);
      }
  }

  if (password)
  {
      if (!(len_password = strlen (password)))
      {
          return 0;
      }
      
      if (! (form_record->form_tokens[1] = calloc (len_password +1, sizeof (char))))
      {
          fprintf (stderr, "%s - calloc() for password failed with errno %d\n", __func__, errno);
          return -1;
      }
      else
      {
          strcpy (form_record->form_tokens[1], password);
      }
  }
  
  return 0;
}


/****************************************************************************************
* Function name - pre_parser
*
* Description - Prepares value token from the configuration file to parsing. Removes LWS,
*               cuts off comments, removes TWS or after quotes closing, removes quotes.
* 
* Input/Output - **ptr - second pointer to value string
*                *len  - pointer to the length of the value string
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int pre_parser (char** ptr, size_t* len)
{
    char* value_start = NULL;
    char* quotes_closing = NULL;
    char* value_end = NULL;

    /* remove LWS */
    if ( ! (value_start = eat_ws (*ptr, len)))
    {
        fprintf (stderr, "%s - error: only LWS found in the value \"%s\".\n", 
                 __func__, value_start);
        return -1;
    }

    /* Cut-off the comments in value string, starting from '#' */
    char* comments = NULL;
    if ((comments = strchr (value_start, '#')))
    {
        *comments = '\0'; /* The idea from Igor Potulnitsky */
        if (! (*len = strlen (value_start)))
        {  
            fprintf (stderr, "%s - error: value \"%s\" has only comments.\n", 
                     __func__, value_start);
            return -1;
        }
    }

    /* Everything after quotes closing or TWS */
    
    if (*value_start == '"')
    {
        /* Enable usage of quotted strings with wight spaces inside, line User-Agent strings. */

        if (*(value_start + 1))
        {
            if ((quotes_closing = strchr (value_start + 1, '"')))
                value_end = quotes_closing + 1;
        }
        else
        {
            value_end = value_start;
        }
    }
     
    /* If not quotted strings, thus, cut the value on the first white space */ 
    if (!value_end)
        value_end = skip_non_ws (value_start, len);

    if (value_end)
    {
        *value_end = '\0';
    }

    *ptr = value_start;
    *len = strlen (value_start) + 1;
  
    return 0;
}

/*
**
** TAG PARSERS IMPLEMENTATION
**
*/
static int batch_name_parser (batch_context*const bctx, char*const value)
{
    strncpy (bctx->batch_name, value, sizeof (bctx->batch_name) -1);
    return 0;
}
static int clients_num_max_parser (batch_context*const bctx, char*const value)
{
    bctx->client_num_max = 0;
    bctx->client_num_max = atoi (value);
    
    /* fprintf (stderr, "\nclients number is %d\n", bctx->client_num_max); */
    if (bctx->client_num_max < 1)
    {
        fprintf (stderr, "%s - error: clients number (%d) is out of the range\n", 
                 __func__, bctx->client_num_max);
        return -1;
    }
    return 0;
}
static int clients_num_start_parser (batch_context*const bctx, char*const value)
{
    bctx->client_num_start = 0;
    bctx->client_num_start = atoi (value);
    
    /* fprintf (stderr, "\nclients number is %d\n", bctx->client_num_start); */
    if (bctx->client_num_start < 0)
    {
        fprintf (stderr, "%s - error: clients starting number (%d) is out of the range\n", 
                 __func__, bctx->client_num_start);
        return -1;
    }
    return 0;
}
static int interface_parser (batch_context*const bctx, char*const value)
{
    strncpy (bctx->net_interface, value, sizeof (bctx->net_interface) -1);
    return 0;
}
static int netmask_parser (batch_context*const bctx, char*const value)
{
    /* CIDR number of non-masked first bits -16, 24, etc */

  if (! strchr (value, '.') && !strchr (value, ':'))
    {
      /* CIDR number of non-masked first bits -16, 24, etc */
      bctx->cidr_netmask = atoi (value);
    }
  else
    {
      bctx->cidr_netmask = netmask_to_cidr (value);
    }
  
  if (bctx->cidr_netmask < 1 || bctx->cidr_netmask > 128)
    {
      fprintf (stderr, 
               "%s - error: network mask (%d) is out of range. Expecting from 1 to 128.\n", 
               __func__, bctx->cidr_netmask);
      return -1;
    }

  return 0;
}
static int ip_addr_min_parser (batch_context*const bctx, char*const value)
{
    struct in_addr inv4;
    memset (&inv4, 0, sizeof (struct in_addr));

    bctx->ipv6 = strchr (value, ':') ? 1 : 0;

    if (inet_pton (bctx->ipv6 ? AF_INET6 : AF_INET, 
                   value, 
                   bctx->ipv6 ? (void *)&bctx->ipv6_addr_min : (void *)&inv4) == -1)
      {
        fprintf (stderr, 
                 "%s - error: inet_pton ()  failed for ip_addr_min %s\n", 
                 __func__, value);
        return -1;
      }
    
    if (!bctx->ipv6)
      {
        bctx->ip_addr_min = ntohl (inv4.s_addr);
      }

    return 0;
}
static int ip_addr_max_parser (batch_context*const bctx, char*const value)
{
  struct in_addr inv4;
  memset (&inv4, 0, sizeof (struct in_addr));
  
  bctx->ipv6 = strchr (value, ':') ? 1 : 0;

  if (inet_pton (bctx->ipv6 ? AF_INET6 : AF_INET, 
                 value, 
                 bctx->ipv6 ? (void *)&bctx->ipv6_addr_max : (void *)&inv4) == -1)
    {
      fprintf (stderr, 
               "%s - error: inet_pton ()  failed for ip_addr_max %s\n", 
               __func__, value);
      return -1;
    }
  
  if (!bctx->ipv6)
    {
      bctx->ip_addr_max = ntohl (inv4.s_addr);
    }

  return 0;
}
static int cycles_num_parser (batch_context*const bctx, char*const value)
{
    bctx->cycles_num = atol (value);
    if (bctx->cycles_num < 0)
    {
        bctx->cycles_num = LONG_MAX - 1;
    }
    return 0;
}
static int clients_initial_inc_parser (batch_context*const bctx, char*const value)
{
    bctx->clients_initial_inc = atol (value);
    if (bctx->clients_initial_inc < 0)
    {
        fprintf (stderr, 
                 "%s - error: clients_initial_inc (%s) should be a zero or positive number\n", 
                 __func__, value);
        return -1;
    }
    return 0;
}
static int user_agent_parser (batch_context*const bctx, char*const value)
{
    if (strlen (value) <= 0)
    {
        fprintf(stderr, "%s - warning: empty USER_AGENT "
                "\"%s\", taking the defaults\n", __func__, value);
        return 0;
    }
    strncpy (bctx->user_agent, value, sizeof(bctx->user_agent) - 1);
    return 0;
}
static int urls_num_parser (batch_context*const bctx, char*const value)
{
    bctx->urls_num = atoi (value);
    
    if (bctx->urls_num < 1)
    {
        fprintf (stderr, 
                 "%s - error: urls_num (%s) should be one or more.\n",
                 __func__, value);
        return -1;
    }    
    /* Preparing the staff to load URLs and handles */
    if (! (bctx->url_ctx_array = 
           (url_context *) cl_calloc (bctx->urls_num, sizeof (url_context))))
    {
        fprintf (stderr, 
                 "%s - error: failed to allocate URL-context array for %d urls\n", 
                 __func__, bctx->urls_num);
        return -1;
    }

    bctx->url_index = -1;  /* Starting from the 0 position in the arrays */

    return 0;
}

/*
** URL section tag parsers. 
*/
static int url_parser (batch_context*const bctx, char*const value)
{
    size_t url_length = 0;

    bctx->url_index++;

    if ((int)bctx->url_index >= bctx->urls_num)
    {
        fprintf (stderr, 
                 "%s - error: URL_NUM (%d) is below uas-urls number in conf-file.\n",
                 __func__, bctx->url_index);
        return -1;
    }
    
    if ((url_length = strlen (value)) <= 0)
    {
        fprintf(stderr, "%s - warning: empty url is OK only "
                "when URL_USE_CURRENT tag defined\n", __func__);
        return 0;
    }
    
    if (! (bctx->url_ctx_array[bctx->url_index].url_str = 
           (char *) calloc (url_length +1, sizeof (char))))
    {
        fprintf (stderr,
                 "%s - error: allocation failed for url string \"%s\"\n", 
                 __func__, value);
        return -1;
    }
    strcpy(bctx->url_ctx_array[bctx->url_index].url_str, value);
    bctx->url_ctx_array[bctx->url_index].url_appl_type = 
      url_schema_classification (value);
    bctx->url_ctx_array[bctx->url_index].url_uas_num = bctx->url_index;
    
    return 0;
}

static int url_short_name_parser (batch_context*const bctx, char*const value)
{
    size_t url_name_length = 0;
        
    if ((url_name_length = strlen (value)) <= 0)
    {
        fprintf(stderr, "%s - warning: empty url short name is OK\n ", __func__);
        return 0;
    }

    strncpy(bctx->url_ctx_array[bctx->url_index].url_short_name, value, 
            sizeof (bctx->url_ctx_array[bctx->url_index].url_short_name) -1);
    
    return 0;
}
static int url_use_current_parser (batch_context*const bctx, char*const value)
{
  long url_use_current_flag = 0;
  url_use_current_flag = atol (value);

  if (url_use_current_flag < 0 || url_use_current_flag > 1)
    {
      fprintf (stderr, 
               "%s - error: URL_USE_CURRENT should be "
               "either 0 or 1 and not %ld.\n", __func__, url_use_current_flag);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].url_use_current = url_use_current_flag;

  return 0;
}
static int url_dont_cycle_parser (batch_context*const bctx, char*const value)
{
  long url_dont_cycle_flag = 0;
  url_dont_cycle_flag = atol (value);

  if (url_dont_cycle_flag < 0 || url_dont_cycle_flag > 1)
    {
      fprintf (stderr, 
               "%s - error: URL_DONT_CYCLE should be either 0 or 1 and not %ld.\n",
               __func__, url_dont_cycle_flag);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].url_dont_cycle = url_dont_cycle_flag;
  return 0;
}
static int header_parser (batch_context*const bctx, char*const value)
{
  const char colomn = ':';
  size_t hdr_len;
  
  if (!value || !(hdr_len = strlen (value)))
    {
      fprintf (stderr, "%s - error: wrong input.\n", __func__);
      return -1;
    }

  if (!strchr (value, colomn))
    {
      fprintf (stderr, 
               "%s - error: HTTP protocol requires \"%c\" colomn symbol" 
               " in HTTP headers.\n", __func__, colomn);
      return -1;
    }

  if (bctx->url_ctx_array[bctx->url_index].custom_http_hdrs_num >= 
      CUSTOM_HTTP_HDRS_MAX_NUM)
    {
      fprintf (stderr, 
               "%s - error: number of custom HTTP headers is limited to %d.\n", 
               __func__, CUSTOM_HTTP_HDRS_MAX_NUM);
      return -1;
    }

  if (!(bctx->url_ctx_array[bctx->url_index].custom_http_hdrs = 
        curl_slist_append (
                           bctx->url_ctx_array[bctx->url_index].custom_http_hdrs,
                           value)))
    {
      fprintf (stderr, "%s - error: failed to append the header \"%s\"\n", 
               __func__, value);
      return -1;
    }
  
  bctx->url_ctx_array[bctx->url_index].custom_http_hdrs_num++;

  return 0;
}
static int request_type_parser (batch_context*const bctx, char*const value)
{
    if (!strcmp (value, REQ_GET))
    {
        bctx->url_ctx_array[bctx->url_index].req_type = 
          HTTP_REQ_TYPE_GET;
    }
    else if (!strcmp (value, REQ_POST))
    {
        bctx->url_ctx_array[bctx->url_index].req_type = 
          HTTP_REQ_TYPE_POST;
    }
    else if (!strcmp (value, REQ_PUT))
    {
        bctx->url_ctx_array[bctx->url_index].req_type = 
          HTTP_REQ_TYPE_PUT;
    }
    else
    {
        fprintf (stderr, 
                 "%s - error: REQ_TYPE (%s) is not valid. Use %s, %s or %s.\n", 
                 __func__, value, REQ_GET, REQ_POST, REQ_PUT);
        return -1;
    }
    return 0;
}
static int username_parser (batch_context*const bctx, char*const value)
{
  if (strlen (value) <= 0)
    {
      fprintf(stderr, "%s - warning: empty USERNAME\"%s\"\n", 
              __func__, value);
      return 0;
    }

  strncpy (bctx->url_ctx_array[bctx->url_index].username, 
           value, 
           sizeof(bctx->url_ctx_array[bctx->url_index].username) - 1);

  return 0;
}
static int password_parser (batch_context*const bctx, char*const value)
{
  if (strlen (value) <= 0)
    {
      fprintf(stderr, "%s - warning: empty PASSWORD\"%s\"\n", 
              __func__, value);
      return 0;
    } 
  strncpy (bctx->url_ctx_array[bctx->url_index].password, 
           value, 
           sizeof(bctx->url_ctx_array[bctx->url_index].password) - 1);
  return 0;
}
static int form_usage_type_parser (batch_context*const bctx, char*const value)
{

  if (!strcmp (value, FT_UNIQUE_USERS_AND_PASSWORDS))
    {
      bctx->url_ctx_array[bctx->url_index].form_usage_type = 
        FORM_USAGETYPE_UNIQUE_USERS_AND_PASSWORDS;
    }
  else if (!strcmp (value, FT_UNIQUE_USERS_SAME_PASSWORD))
    {
      bctx->url_ctx_array[bctx->url_index].form_usage_type = 
        FORM_USAGETYPE_UNIQUE_USERS_SAME_PASSWORD;
    }
  else if (!strcmp (value, FT_SINGLE_USER))
    {
      bctx->url_ctx_array[bctx->url_index].form_usage_type = 
        FORM_USAGETYPE_SINGLE_USER;
    }
  else if (!strcmp (value, FT_RECORDS_FROM_FILE))
    {
      bctx->url_ctx_array[bctx->url_index].form_usage_type = 
        FORM_USAGETYPE_RECORDS_FROM_FILE;
    }
  else if (!strcmp (value, FT_AS_IS))
    {
      bctx->url_ctx_array[bctx->url_index].form_usage_type = 
        FORM_USAGETYPE_AS_IS;
    }
  else
    {
      fprintf(stderr, "%s - error: FORM_USAGE_TYPE to be choosen from:"
              "%s , %s ,\n" "%s , %s , %s \n" ,  __func__, 
              FT_UNIQUE_USERS_AND_PASSWORDS, FT_UNIQUE_USERS_SAME_PASSWORD,
              FT_SINGLE_USER, FT_RECORDS_FROM_FILE, FT_AS_IS);
      return -1;
    }
  
  return 0;
}
static int form_string_parser (batch_context*const bctx, char*const value)
{
  int count_percent_s_percent_d = 0, count_percent_s = 0;
  char* pos_current = NULL;
  const form_usagetype ftype = 
        bctx->url_ctx_array[bctx->url_index].form_usage_type;

  if (ftype <= FORM_USAGETYPE_START || ftype >= FORM_USAGETYPE_END)
    {
      fprintf(stderr, "%s - error: please, beyond FORM_STRING place the "
              "defined FORM_USAGE_TYPE tag with its values to be choosen from:"
              "%s , %s ,\n" "%s , %s , %s \n" , __func__, 
              FT_UNIQUE_USERS_AND_PASSWORDS, FT_UNIQUE_USERS_SAME_PASSWORD,
              FT_SINGLE_USER, FT_RECORDS_FROM_FILE, FT_AS_IS);
      return -1;
    }

  if (strcmp (value, NON_APPLICABLE_STR) || strcmp (value, "N/A"))
    {
      /*count "%s%d" and "%s" sub-stritngs*/

      pos_current = value;
      while (*pos_current && (pos_current = strstr (pos_current, "%s%d")))
        {
          ++count_percent_s_percent_d;
          ++pos_current;
        }

      pos_current = value;
      while (*pos_current && (pos_current = strstr (pos_current, "%s")))
        {
          ++count_percent_s;
          ++pos_current;
        }

      if (count_percent_s_percent_d == 2 && count_percent_s == 2)
        {
          if (ftype != FORM_USAGETYPE_UNIQUE_USERS_AND_PASSWORDS)
            {
              return print_correct_form_usagetype (ftype, value);
            }
        }
      else if (count_percent_s_percent_d == 1 && count_percent_s == 2)
        {
          if (ftype  != FORM_USAGETYPE_UNIQUE_USERS_SAME_PASSWORD)
            {
              return print_correct_form_usagetype (ftype, value);
            }
        }
      else if (count_percent_s_percent_d == 0 && count_percent_s == 2)
        {
          if (ftype != FORM_USAGETYPE_SINGLE_USER &&
              ftype != FORM_USAGETYPE_RECORDS_FROM_FILE)
            {
              return print_correct_form_usagetype (ftype, value);
            }
        }
      else
        {

          if (ftype != FORM_USAGETYPE_AS_IS)
            {
              fprintf (stderr, 
                       "\n%s - error: FORM_STRING (%s) is not valid. \n"
                       "Please, use:\n"
                       "- to generate unique users with unique passwords two \"%%s%%d\" , something like " 
                       "\"user=%%s%%d&password=%%s%%d\" \n"
                       "- to generate unique users with the same passwords one \"%%s%%d\" \n"
                       "for users and one \"%%s\" for the password," 
                       "something like \"user=%%s%%d&password=%%s\" \n"
                       "- for a single configurable user with a password two \"%%s\" , something like "
                       "\"user=%%s&password=%%s\" \n",
                       "- to load user credentials (records) from a file two \"%%s\" , something like "
                       "\"user=%%s&password=%%s\" \n and _FILE defined.\n",
                       __func__);
              return -1;
            }
        }

      strncpy (bctx->url_ctx_array[bctx->url_index].form_str,
               value, 
               sizeof (bctx->url_ctx_array[bctx->url_index].form_str) - 1);

      /*
        Allocate client contexts, if not allocated before.
      */
      if (!bctx->cctx_array)
      {
          if (!(bctx->cctx_array  = 
                (client_context *) cl_calloc (bctx->client_num_max,
                                                                 sizeof (client_context))))
          {
              fprintf (stderr, "\"%s\" - %s - failed to allocate cctx.\n", 
                       bctx->batch_name, __func__);
              return -1;
          }
      }

      /*
        Allocate client buffers for POST-ing login and logoff credentials.
      */
      int i;
      for (i = 0;  i < bctx->client_num_max; i++)
      {
          if (! (bctx->cctx_array[i].post_data = 
                 (char *) calloc (POST_DATA_BUF_SIZE, sizeof (char))))
          {
              fprintf (stderr,
                       "\"%s\" failed to allocate post data buffer.\n", __func__) ;
              return -1;
          }
      }
    }

  return 0;
}
static int form_records_file_parser (batch_context*const bctx, char*const value)
{
  struct stat statbuf;
  size_t string_len = 0;

  if (strcmp (value, NON_APPLICABLE_STR))
    {
      /* Stat the file, it it exists. */
      if (stat (value, &statbuf) == -1)
        {
          fprintf(stderr, "%s error: file \"%s\" does not exist.\n",  __func__,value);
          return -1;
        }

      string_len = strlen (value) + 1;
      if (! (bctx->url_ctx_array[bctx->url_index].form_records_file = 
             (char *) calloc (string_len, sizeof (char))))
        {
          fprintf(stderr, "%s error: failed to allocate memory for form_records_file with errno %d.\n",  
                  __func__, errno);
          return -1;
        }

      strncpy (bctx->url_ctx_array[bctx->url_index].form_records_file, 
               value, 
               string_len -1);

      if (load_form_records_file (bctx, &bctx->url_ctx_array[bctx->url_index]) == -1)
      {
          fprintf(stderr, "%s error: load_form_records_file () failed.\n", __func__);
          return -1;
      }
    }

    return 0;
}
static int upload_file_parser  (batch_context*const bctx, char*const value)
{
  struct stat statbuf;
  size_t string_len = 0;

  if (strcmp (value, NON_APPLICABLE_STR))
    {
      /* Stat the file, it it exists. */
      if (stat (value, &statbuf) == -1)
        {
          fprintf(stderr, "%s error: file \"%s\" does not exist.\n",  __func__,value);
          return -1;
        }

      string_len = strlen (value) + 1;
      if (! (bctx->url_ctx_array[bctx->url_index].upload_file = 
             (char *) calloc (string_len, sizeof (char))))
        {
          fprintf(stderr, 
                  "%s error: failed to allocate memory with errno %d.\n",
                  __func__, errno);
          return -1;
        }

      strncpy (bctx->url_ctx_array[bctx->url_index].upload_file,
               value, 
               string_len -1);
    }
    return 0;
}

static int web_auth_method_parser (batch_context*const bctx, char*const value)
{
    (void) bctx; (void) value;
  return 0;
}
static int web_auth_credentials_parser (batch_context*const bctx, char*const value)
{
  (void) bctx; (void) value;
  return 0;
}
static int proxy_auth_method_parser (batch_context*const bctx, char*const value)
{
  (void) bctx; (void) value;
  return 0;
}
static int proxy_auth_credentials_parser (batch_context*const bctx, char*const value)
{
  (void) bctx; (void) value;
  return 0;
}

static int connection_reestablish_parser (batch_context*const bctx, char*const value)
{
    long boo = atol (value);

    if (boo < 0 || boo > 1)
    {
        fprintf(stderr, 
                "%s error: boolean input 0 or 1 is expected\n", __func__);
        return -1;
    }
    bctx->url_ctx_array[bctx->url_index].connection_reestablish = boo;
    return 0;
}

static int timer_tcp_conn_setup_parser (batch_context*const bctx, char*const value)
{
    long timer = atol (value);

    if (timer <= 0 || timer > 50)
    {
        fprintf(stderr, 
                "%s error: input of the timer is expected  to be from 1 up to 50 seconds.\n", __func__);
        return -1;
    }
    bctx->url_ctx_array[bctx->url_index].connect_timeout= timer;
    return 0;
}
static int timer_url_completion_parser (batch_context*const bctx, char*const value)
{
    bctx->url_ctx_array[bctx->url_index].timer_url_completion = atol (value);
    return 0;
}
static int timer_after_url_sleep_parser (batch_context*const bctx, char*const value)
{
    bctx->url_ctx_array[bctx->url_index].timer_after_url_sleep = atol (value);
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
#define SFTP_SCHEMA_STR "sftp://"
#define TELNET_SCHEMA_STR "telnet://"

  if (strstr (url, HTTPS_SCHEMA_STR))
    return URL_APPL_HTTPS;
  else if (strstr (url, HTTP_SCHEMA_STR))
    return URL_APPL_HTTP;
  else if (strstr (url, FTPS_SCHEMA_STR))
    return URL_APPL_FTPS;
  else if (strstr (url, FTP_SCHEMA_STR))
    return URL_APPL_FTP;
  else if (strstr (url, SFTP_SCHEMA_STR))
    return URL_APPL_SFTP;
  else if (strstr (url, TELNET_SCHEMA_STR))
    return URL_APPL_TELNET;

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

/****************************************************************************************
* Function name - validate_batch
*
* Description - Validates all parameters in the batch. Calls validation functions for all sections.
* 
* Input -      *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int validate_batch (batch_context*const bctx)
{
    if (validate_batch_general (bctx) == -1)
    {
        fprintf (stderr, "%s - error: failed to validate batch section GENERAL.\n", 
                 __func__);
        return -1;
    }

    if (validate_batch_url (bctx) == -1)
    {
        fprintf (stderr, "%s - error: failed to validate batch section UAS.\n", 
                 __func__);
        return -1;
    }

    return 0;
}

/****************************************************************************************
* Function name - validate_batch_general
*
* Description - Validates section general parameters
* 
* Input -       *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int validate_batch_general (batch_context*const bctx)
{
    if (!strlen (bctx->batch_name))
    {
        fprintf (stderr, "%s - error: BATCH_NAME is empty.\n", __func__);
        return -1;
    }
    if (bctx->client_num_max < 1)
    {
        fprintf (stderr, "%s - error: CLIENT_NUM_MAX is less than 1.\n", __func__);
        return -1;
    }
    if (bctx->client_num_start < 0)
    {
        fprintf (stderr, "%s - error: CLIENT_NUM_START is less than 0.\n", __func__);
        return -1;
    }
    if (bctx->clients_initial_inc < 0)
    {
        fprintf (stderr, "%s - error: CLIENTS_INITIAL_INC is negative.\n",__func__);
        return -1;
    }

    // TODO: validate the existence of the network interface
    if (!strlen (bctx->net_interface))
    {
        fprintf (stderr, "%s - error: INTERFACE name is empty.\n", __func__);
        return -1;
    }

    if (bctx->ipv6)
      {
        if (bctx->cidr_netmask < 1 || bctx->cidr_netmask > 128)
          {
            fprintf (stderr, 
                     "%s - error: IPv6 network mask (%d) is out of the range\n", 
                     __func__, bctx->cidr_netmask);
            return -1;
          }
      }
    else
      {
        if (bctx->cidr_netmask < 1 || bctx->cidr_netmask > 32)
          {
            fprintf (stderr, 
                     "%s - error: IPv4 network mask (%d) is out of the range\n", 
                     __func__, bctx->cidr_netmask);
            return -1;
          }
      }
    
    if (!bctx->ipv6)
      {
        if ((bctx->ip_addr_max - bctx->ip_addr_min + 1) < bctx->client_num_max)
          {
            fprintf (stderr, "%s - error: range of IPv4 addresses is less than number of clients.\n"
                     "Please, increase IP_ADDR_MAX.\n", __func__);
            return -1;
          }
      }

    if (bctx->cycles_num < 1)
    {
        fprintf (stderr, "%s - error: CYCLES_NUM is less than 1.\n"
                 "To cycle or not to cycle - this is the question.\n",__func__);
        return -1;
    }

    if (!strlen (bctx->user_agent))
    {
        /* user-agent not provided, taking the defaults */
        strncpy (bctx->user_agent, 
                 EXPLORER_USERAGENT_STR, 
                 sizeof (bctx->user_agent) -1);
    }
  
    return 0;
}


/****************************************************************************************
* Function name - validate_batch_url
*
* Description - Validates section URL parameters
* 
* Input -       *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int validate_batch_url (batch_context*const bctx)
{
  if (bctx->urls_num < 1)
    {
      fprintf (stderr, "%s - error: at least a single url is expected "
               "for a valid URL section .\n", __func__);
      return -1;
    }

  int noncycling_cycling = 0, cycling_noncycling = 0;
  int prev_url_cycling = 0;

  int k = 0;
  for (k = 0; k < bctx->urls_num; k++)
    {
      url_context* url = &bctx->url_ctx_array[k];

      /*
        Test, that HTTP methods (req-types) are GET, POST or PUT
      */
      const url_appl_type url_type = url->url_appl_type;
      const int req_type = url->req_type;
      
      if (url_type == URL_APPL_HTTP || url_type == URL_APPL_HTTPS)
        {
          if (req_type < HTTP_REQ_TYPE_FIRST || 
              req_type > HTTP_REQ_TYPE_LAST)
            {
              fprintf (stderr, "%s - error: REQUEST_TYPE is out of valid range .\n", 
                       __func__);
              return -1;
            }
        }
      
      if (url->form_records_file && ! url->form_str[0])
        {
          fprintf (stderr, "%s - error: empty FORM_STRING, "
                   "when FORM_RECORDS_FILE defined.\n Either disable" 
                   "FORM_RECORDS_FILE or define FORM_STRING\n", __func__);
          return -1;
        }
      
      /*
        Test, that there is only a single continues area of cycling URLs 
        in meanwhile, like this:
        
        don't-cycle - URL;
        cycle -URL;
        cycle -URL;
        don't-cycle - URL;
        don't-cycle - URL;
        
        We are not supporting several regions of cycling right now, like that
        don't-cycle - URL;
        cycle -URL;
        don't-cycle - URL; separates cycling area.
        cycle -URL;
        don't-cycle - URL;
      */
      if (k)
      {
          if (prev_url_cycling && url->url_dont_cycle)
          {
              cycling_noncycling++;
          }
          else if (!prev_url_cycling && !url->url_dont_cycle)
          {
              noncycling_cycling++;
          }
      }

      // Remember this url cycling status to prev_url_cycling tobe used the next time
      prev_url_cycling = url->url_dont_cycle ? 0 : 1;
      
      
      if (! url->url_use_current)
        {
          /*
            Check non-empty URL, when URL_USE_CURRENT is not defined.
          */
          if (!url->url_str || !strlen (url->url_str))
            {
              fprintf (stderr, 
                       "%s - error: empty URL in position %d.\n", __func__, k);
              return -1;
            }
        }
      else
        {
          /*
             URL_USE_CURRENT cannot appear in the first URL, 
             nothing is current.
          */
          if (0 == k)
            {
              fprintf (stderr, 
                       "%s - error: empty URL with URL_USE_CURRENT " 
                       "defined cannot appear as the very first url.\n"
                       "There is no any url to take as \"current\"\n", __func__);
              return -1;
            }

          /*
            Test, that CURRENT_URL, if HTTP/S  is of req_type POST or PUT;
          */
          if (url->url_appl_type == URL_APPL_HTTP || 
              url->url_appl_type == URL_APPL_HTTPS)
            {
              if (url->req_type != HTTP_REQ_TYPE_POST ||
                  url->req_type != HTTP_REQ_TYPE_PUT)
                {
                  fprintf (stderr, 
                           "%s - error: URL_USE_CURRENT tag works, when" 
                           "REQUEST_TYPE is POST or PUT.\n"
                           "Y do not need to care about 3xx redirections after GET.\n", __func__);
                  return -1;
                }
            }
          
          /*
              Test, that cycling or not-cycling status of the 
              CURRENT_URLs is the same as for the primary-URL
            */
          int m;
          for (m = k - 1; m >= 0; m--)
            {
              url_context* url_m = &bctx->url_ctx_array[m];

              if (url_m->url_dont_cycle != url_m->url_dont_cycle)
                {
                  fprintf (stderr, 
                           "%s - error: cycling of the primary url and all urls "
                           "afterwards with URL_USE_CURRENT defined should be the same.\n" 
                           "Check tags URL_DONT_CYCLE values. Either cycle or don't cycle\n"
                           "for both the primary and the \"use current\" urls.\n", __func__);
                  return -1;
                }

              if (! url_m->url_use_current)
                break;
            }
        } /* else */

    }

  if (cycling_noncycling > 1 || noncycling_cycling > 1)
  {
      fprintf (stderr, 
               "%s - error: this version supports only a single cycling area.\n"
               "Several non-cycling urls can be before and/or after this cycling area, \n"
               "e.g. for login and logoff purposes\n", 
               __func__);
      return -1;
  }
      


  return 0;
}

/****************************************************************************************
* Function name - post_validate_init
*
* Description - Performs post validate initializations of a batch context.
* 
* Input -       *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int post_validate_init (batch_context*const bctx)
{
  /*
    Allocate client contexts, if not allocated before.
  */
  if (!bctx->cctx_array)
    {
      if (!(bctx->cctx_array  = 
            (client_context *) cl_calloc (bctx->client_num_max, 
                                          sizeof (client_context))))
        {
          fprintf (stderr, "\"%s\" - %s - failed to allocate cctx.\n", 
                   bctx->batch_name, __func__);
          return -1;
        }
    }
  
  /* 
     Init operational statistics structures 
  */
  if (op_stat_point_init(&bctx->op_delta, 
                         bctx->urls_num) == -1)
    {
      fprintf (stderr, "%s - error: init of op_delta failed.\n",__func__);
      return -1;
    }
  
  if (op_stat_point_init(&bctx->op_total,
                         bctx->urls_num) == -1)
    {
      fprintf (stderr, "%s - error: init of op_total failed.",__func__);
      return -1;
    }

  /* 
     It should be the last check.
  */
  fprintf (stderr, "\nThe configuration has been validated successfully.\n\n");

  /* 
     Check, that this configuration has cycling. 
     Namely, at least a single url is without tag URL_DONT_CYCLE
  */
  const int first_cycling_url = find_first_cycling_url (bctx);

  find_last_cycling_url (bctx);

  if (first_cycling_url < 0)
    {
      fprintf (stderr, "The configuration has not cycling urls defined.\n"
               "Are you sure, that this is what you are planning to do?\n"
               "To make cycling you may wish to remove tag URL_DONT_CYCLE \n"
               "or set it to zero for the urls that you wish to run in cycle.\n"
               " Please, press ENTER to continue or Cntl-C to stop.\n");
             
      getchar ();
    }
  
  return 0;
}

/*******************************************************************************
* Function name - parse_config_file
*
* Description - Parses configuration file and fills loading batch contexts in array
*
* Input -      *filename       - name of the configuration file to parse.
* Output -     *bctx_array     - array of batch contexts to be filled on parsing
* Input-       bctx_array_size - number of bctx contexts in <bctx_array>
*                          
* Return Code/Output - On Success - number of batches >=1, on Error -1
********************************************************************************/
int parse_config_file (char* const filename, 
                       batch_context* bctx_array, 
                       size_t bctx_array_size)
{
  char fgets_buff[1024];
  FILE* fp;
  struct stat statbuf;
  int batch_index = -1;

  /* Check, if the configuration file exists. */
  if (stat (filename, &statbuf) == -1)
    {
      fprintf (stderr,
               "%s - failed to find configuration file \"%s\" with errno %d.\n"
               "If you are using example configurations, note, that directory \"configs\" have "
               "been renamed to \"conf-examples\".", 
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
      // fprintf (stderr, "%s - processing file string \"%s\n", 
      //         __func__, fgets_buff);

      char* string_buff = NULL;
      size_t string_len = 0;

      if ((string_len = strlen (fgets_buff)) && 
          (string_buff = eat_ws (fgets_buff, &string_len)))
        {

          if ((batch_index + 1) >= (int) bctx_array_size)
            {
              fprintf(stderr, "%s - error: maximum batches limit (%d) reached \n", 
                      __func__, bctx_array_size);
              fclose (fp);
              return -1 ;
            }

          /* Line may be commented out by '#'.*/
          if (fgets_buff[0] == '#')
            {
              // fprintf (stderr, "%s - skipping commented file string \"%s\n", 
              //         __func__, fgets_buff);
              continue;
            }

          if (add_param_to_batch (fgets_buff,
                                  string_len,
                                  bctx_array, 
                                  &batch_index) == -1)
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

  if (! (batch_index + 1))
    {
      fprintf (stderr, 
               "%s - error: failed to load even a single batch.\n",__func__);
      return -1;
    }
  else
    {
      fprintf (stderr, "%s - loaded %d batches\n", 
               __func__, batch_index + 1);
    }

  int k = 0;
  for (k = 0; k < batch_index + 1; k++)
    {
      /* Validate batch configuration */
      if (validate_batch (&bctx_array[k]) == -1)
        {
          fprintf (stderr, 
                   "%s - error: validation of batch %d failed.\n",__func__, k);
          return -1;
        }

      if (post_validate_init (&bctx_array[k]) == -1)
        {
          fprintf (stderr, 
                   "%s - error: post_validate_init () for batch %d failed.\n",__func__, k);
          return -1;
        }
    }

  return (batch_index + 1);
}


/*******************************************************************************
* Function name - load_form_records_file
*
* Description - Itializes client post form buffers, using credentials loaded from file.
*               To be called after batch context validation.
*
* Input -       *bctx - pointer to the batch context
*                          
* Return Code/Output - On Success - number of batches >=1, on Error -1
********************************************************************************/
static int load_form_records_file (batch_context*const bctx, url_context* url)
{
  char fgets_buff[512];
  FILE* fp;
  char sep;

  /* 
     Open the file with form records 
  */
  if (!(fp = fopen (url->form_records_file, "r")))
    {
      fprintf (stderr, 
               "%s - fopen() failed to open for reading filename \"%s\", errno %d.\n", 
               __func__, url->form_records_file, errno);
      return -1;
    }

  /* 
     Allocate the place to keep form records tokens for clients
  */
  if (! (url->form_records_array =  calloc (bctx->client_num_max, sizeof (form_records_cdata))))
  {
      fprintf (stderr, 
               "%s - failed to allocate memory for url->form_records_array with errno %d.\n", 
               __func__, errno);
      return -1;
  }

  while (fgets (fgets_buff, sizeof (fgets_buff) - 1, fp))
    {
      fprintf (stderr, "%s - processing form records file string \"%s\n", 
               __func__, fgets_buff);

      char* string_buff = NULL;
      size_t string_len = 0;

      if ((string_len = strlen (fgets_buff)) && 
          (string_buff = eat_ws (fgets_buff, &string_len)))
        {
          // Line may be commented out by '#'.
          if (fgets_buff[0] == '#')
            {
              fprintf (stderr, "%s - skipping commented file string \"%s\n", 
                       __func__, fgets_buff);
              continue;
            }

          if (!string_len)
            {
              fprintf (stderr, "%s - skipping empty line.\n", __func__);
              continue;
            }

          if ((int)url->form_records_num >= bctx->client_num_max)
            {
              fprintf (stderr, 
                       "%s - warning: CLIENTS_NUM (%d) is less than the number of" 
                       "records is the file form_records_file.\n", __func__, bctx->client_num_max);
              sleep (3);
              break;
            }

          if (load_form_record_string (fgets_buff,
                                       string_len,
                                       &url->form_records_array[url->form_records_num],
                                       url->form_records_num,
                                       &sep) == -1)
          {
              fprintf (stderr, 
                       "%s - error: load_client_credentials_buffers () failed on records line \"%s\"\n", 
                       __func__, fgets_buff);
              fclose (fp);
              return -1 ;
          }
          
          url->form_records_num++;
        }
    }

  if ((int)url->form_records_num < bctx->client_num_max)
    {
      fprintf (stderr, 
               "%s - error: CLIENTS_NUM (%d) is above the number " 
               "of records in the form_records_file\nPlease, either decrease the CLIENTS_NUM "
               "or add more records strings to the file.\n", 
               __func__, bctx->client_num_max);
      fclose (fp);
      return -1 ;
    }

  fclose (fp);
  return 0;
}

static int find_first_cycling_url (batch_context* bctx)
{
  size_t i;
  int first_url = -1;
  
  for (i = 0;  i < (size_t)bctx->urls_num; i++)
    {
      if (! bctx->url_ctx_array[i].url_dont_cycle)
        {
          first_url = i;
          break;
        }
    }

  bctx->first_cycling_url = first_url;

  if (bctx->first_cycling_url < 0)
    bctx->cycling_completed = 1;

  return bctx->first_cycling_url;
}

static int find_last_cycling_url (batch_context* bctx)
{
  size_t i = 0;
  int last_url = -1;

  for (i = 0;  i < (size_t)bctx->urls_num; i++)
    {
      if (! bctx->url_ctx_array[i].url_dont_cycle)
        {
          last_url = i;
        }
    }

  bctx->last_cycling_url = last_url;

  if (bctx->last_cycling_url < 0)
    bctx->cycling_completed = 1;

  return bctx->last_cycling_url;
}

/*******************************************************************************
* Function name - netmask_to_cidr
*
* Description - Converts quad-dotted IPv4 address string to the CIDR number
*               from 0 to 32.
*
* Input -       *dotted_ipv4 - quad-dotted IPv4 address string
* Return Code/Output - On Success - CIDR number from 0 to 32, on Error - (-1)
********************************************************************************/
static int netmask_to_cidr (char *dotted_ipv4)
{
  int network = 0;
  int host = 0;
 
  if (inet_pton (AF_INET, dotted_ipv4, &network) < 1) 
    {
      return -1;
    }

  host = ntohl (network);

  int tmp = 0;
  
  while (!(host & (1 << tmp)) && tmp < 32)
    {
      tmp++;
    }

  return (32 - tmp); 
 }

static int print_correct_form_usagetype (form_usagetype ftype, char* value)
{
  switch (ftype)
    {
    case FORM_USAGETYPE_UNIQUE_USERS_AND_PASSWORDS:

      fprintf (stderr, 
               "\n%s - error: FORM_STRING value (%s) is not valid. \nPlease, use:\n"
               "- to generate unique users with unique passwords two \"%%s%%d\" , something like " 
               "\"user=%%s%%d&password=%%s%%d\" \n", __func__, value);
      break;

    case FORM_USAGETYPE_UNIQUE_USERS_SAME_PASSWORD:
      fprintf (stderr, 
               "\n%s - error: FORM_STRING value (%s) is not valid. \nPlease, use:\n"
               "- to generate unique users with the same passwords one \"%%s%%d\" \n"
               "for users and one \"%%s\" for the password," 
               "something like \"user=%%s%%d&password=%%s\" \n", __func__, value);
      break;

    case FORM_USAGETYPE_SINGLE_USER:
      fprintf (stderr, 
               "\n%s - error: FORM_STRING  value (%s) is not valid. \nPlease, use:\n"
               "- for a single configurable user with a password two \"%%s\" , something like "
                       "\"user=%%s&password=%%s\" \n",__func__, value);
      break;

    case FORM_USAGETYPE_RECORDS_FROM_FILE:
      fprintf (stderr, 
               "\n%s - error: FORM_STRING value (%s) is not valid. \nPlease, use:\n"
               "- to load user credentials (records) from a file two \"%%s\" , something like "
               "\"user=%%s&password=%%s\" \n and _FILE defined.\n", __func__, value);

      break;

    default:
      break;
    }

  return -1;
}
