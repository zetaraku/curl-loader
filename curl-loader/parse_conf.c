/* 
*     parse_conf.c
*
* 2006-2007 Copyright (c) 
* Robert Iakobashvili, <coroberti@gmail.com>
* All rights reserved.*
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
#define NON_APPLICABLE_STR_2 "N/A"

#define REQ_GET "GET"
#define REQ_POST "POST"
#define REQ_PUT "PUT"

#define FT_UNIQUE_USERS_AND_PASSWORDS "UNIQUE_USERS_AND_PASSWORDS"
#define FT_UNIQUE_USERS_SAME_PASSWORD "UNIQUE_USERS_SAME_PASSWORD"
#define FT_SINGLE_USER "SINGLE_USER"
#define FT_RECORDS_FROM_FILE "RECORDS_FROM_FILE"
#define FT_AS_IS "AS_IS"

#define AUTH_BASIC "BASIC"
#define AUTH_DIGEST "DIGEST"
#define AUTH_GSS_NEGOTIATE "GSS_NEGOTIATE"
#define AUTH_NTLM "NTLM"
#define AUTH_ANY "ANY"

static unsigned char 
resp_status_errors_tbl_default[URL_RESPONSE_STATUS_ERRORS_TABLE_SIZE];


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
static int clients_rampup_inc_parser (batch_context*const bctx, char*const value);
static int interface_parser (batch_context*const bctx, char*const value);
static int netmask_parser (batch_context*const bctx, char*const value);
static int ip_addr_min_parser (batch_context*const bctx, char*const value);
static int ip_addr_max_parser (batch_context*const bctx, char*const value);
static int ip_shared_num_parser (batch_context*const bctx, char*const value);
static int cycles_num_parser (batch_context*const bctx, char*const value);
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

static int multipart_form_data_parser (batch_context*const bctx, char*const value);

static int web_auth_method_parser (batch_context*const bctx, char*const value);
static int web_auth_credentials_parser (batch_context*const bctx, char*const value);
static int proxy_auth_method_parser (batch_context*const bctx, char*const value);
static int proxy_auth_credentials_parser (batch_context*const bctx, char*const value);

static int fresh_connect_parser (batch_context*const bctx, char*const value);

static int timer_tcp_conn_setup_parser (batch_context*const bctx, char*const value);
static int timer_url_completion_parser (batch_context*const bctx, char*const value);
static int timer_after_url_sleep_parser (batch_context*const bctx, char*const value);

static int ftp_active_parser (batch_context*const bctx, char*const value);
static int log_resp_headers_parser (batch_context*const bctx, char*const value);
static int log_resp_bodies_parser (batch_context*const bctx, char*const value);
static int response_status_errors_parser (batch_context*const bctx, char*const value);
static int transfer_limit_rate_parser (batch_context*const bctx, char*const value);

static int fetch_probability_parser (batch_context*const bctx, char*const value);
static int fetch_probability_once_parser (batch_context*const bctx, char*const value);

static int form_records_random_parser (batch_context*const bctx, char*const value);
static int form_records_file_max_num_parser(batch_context*const bctx, char*const value);

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
    {"CLIENTS_RAMPUP_INC", clients_rampup_inc_parser},
    {"INTERFACE", interface_parser},
    {"NETMASK", netmask_parser},
    {"IP_ADDR_MIN", ip_addr_min_parser},
    {"IP_ADDR_MAX", ip_addr_max_parser},
    {"IP_SHARED_NUM", ip_shared_num_parser},
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

    {"MULTIPART_FORM_DATA", multipart_form_data_parser},

    {"WEB_AUTH_METHOD", web_auth_method_parser},
    {"WEB_AUTH_CREDENTIALS", web_auth_credentials_parser},
    {"PROXY_AUTH_METHOD", proxy_auth_method_parser},
    {"PROXY_AUTH_CREDENTIALS", proxy_auth_credentials_parser},

    {"FRESH_CONNECT", fresh_connect_parser},

    {"TIMER_TCP_CONN_SETUP", timer_tcp_conn_setup_parser},
    {"TIMER_URL_COMPLETION", timer_url_completion_parser},
    {"TIMER_AFTER_URL_SLEEP", timer_after_url_sleep_parser},

    {"FTP_ACTIVE", ftp_active_parser},
    {"LOG_RESP_HEADERS", log_resp_headers_parser},
    {"LOG_RESP_BODIES", log_resp_bodies_parser},
    {"RESPONSE_STATUS_ERRORS", response_status_errors_parser},

    {"TRANSFER_LIMIT_RATE", transfer_limit_rate_parser},

    {"FETCH_PROBABILITY", fetch_probability_parser},
    {"FETCH_PROBABILITY_ONCE", fetch_probability_once_parser},

    {"FORM_RECORDS_RANDOM", form_records_random_parser},
    {"FORM_RECORDS_FILE_MAX_NUM", form_records_file_max_num_parser},

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
                                    char** separator);

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
static int parse_timer_range (char* input, 
                              size_t input_len, 
                              long* first_val, 
                              long* second_val);

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
static int add_param_to_batch (char*const str_buff, 
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
      fprintf (stderr, "%s - error: unknown tag %s.\n"
               "\nATTENTION: If the tag not misspelled, read README.Migration file.\n\n",
               __func__, str_buff);
      return -1;
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
*
* Input/Output  *form_record  - pointer to the form_records_cdata array
*               record_num    - index of the record ?
*               *separator    - the separating symbol initialized by the first string and 
*                               further used.
* Return Code/Output - On success - 0, on failure - (-1)
****************************************************************************************/
static int load_form_record_string (char*const input, 
                                    size_t input_len,
                                    form_records_cdata* form_record, 
                                    size_t record_num,
                                    char** separator)
{
  static const char* separators_supported [] =
    {
      ",",
      ":",
      ";",
      " ", 
      "@", 
      "/", 
      0
    };
  char* sp = NULL;
  int i;

  if (!input || !input_len)
    {
      fprintf (stderr, "%s - error: wrong input\n", __func__);
      return -1;
    }
  
  /* 
     Figure out the separator used by the first string analyses 
  */
  if (! record_num)
    {
      for (i = 0; separators_supported [i]; i++)
        {
          if ((sp = strchr (input, *separators_supported [i])))
            {
              *separator = (char *) separators_supported [i]; /* Remember the separator */
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
              fprintf (stderr,"\"%s\"\n", separators_supported [i]);
            }
          return -1;
        }
    }

  char * token = 0, *strtokp = 0;
  size_t token_count  = 0;

  for (token = strtok_r (input, *separator, &strtokp); 
       token != 0;
       token = strtok_r (0, *separator, &strtokp))
    {
      size_t token_len = strlen (token);

      if (! token_len)
        {
          fprintf (stderr, "%s - warning: token is empty. \n", __func__);
        }
      else if (token_len >= FORM_RECORDS_TOKEN_MAX_LEN)
        {
          fprintf (stderr, "%s - error: token is above the allowed "
                   "FORM_RECORDS_TOKEN_MAX_LEN (%d). \n", 
                   __func__, FORM_RECORDS_TOKEN_MAX_LEN);
        }
      else
        {
          if (! (form_record->form_tokens[token_count] = 
                 calloc (token_len +1, sizeof (char))))
            {
              fprintf (stderr, "%s - error: calloc() failed with errno %d\n", 
                       __func__, errno);
              return -1;
            }
          else
            {
              strcpy (form_record->form_tokens[token_count], token);
            }
        }

      if (++token_count >= FORM_RECORDS_MAX_TOKENS_NUM)
        {
          fprintf (stderr, "%s - warning: tokens number is above" 
                   " FORM_RECORDS_MAX_TOKENS_NUM (%d). \n", 
                   __func__, FORM_RECORDS_MAX_TOKENS_NUM);
          break;
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

/*******************************************************************************
* Function name - parse_timer_range
*
* Description - Parses potential timer ranges with values looking either as 
*                  "1000" or "1000-2000"
* 
* Input-    *input - pointer to value string
*                  input_len - length of the value string, pointed by <input>
* Input/Output - *first_val - used to return the first long value
*                               *second_val - used to return the second long value, which is optional
*
* Return Code/Output - On success - 0, on failure - (-1)
*********************************************************************************/
static int parse_timer_range (char* input,
                              size_t input_len,
                              long* first_val,
                              long* second_val)
{
  if (!input || !input_len || !first_val || !second_val)
    {
      fprintf (stderr, "%s - error: wrong input\n", __func__);
      return -1;
    }

  const char separator = '-';
  char* second = 0;
  char* sep = 0;
  sep = strchr (input, separator);

  if (sep)
    {
      *sep = '\0';

      if ((sep - input < (int)input_len) && (*(sep + 1)))
        {
          second = sep + 1;
        }
      else
        {
          *sep = separator;
          fprintf (stderr, "%s - error: wrong input %s. "
                   "Separator %c exists, but no value after the separator.\n", 
                   __func__, input, separator);
          return -1 ;
        }
    }

  *first_val = atol (input);

  if (*first_val < 0)
    {
      fprintf (stderr, "%s - error: wrong input %s. "
               "Only non-negative values are allowed.\n", 
               __func__, input);
      return -1;
    }

  if (sep)
    {
      *second_val = atol (second);

      if (sep && *second_val < 0)
        {
          fprintf (stderr, "%s - error: wrong input %s. "
                   "Only non-negative values are allowed.\n", 
                   __func__, second);
          return -1;
        }
      
      if (sep && *first_val >= *second_val)
        {
          fprintf (stderr, "%s - error: wrong input. "
                   "First value (%ld) should be less then the second (%ld).\n"
                   "Switch the order.\n", __func__, *first_val, *second_val);
          return -1 ;
        }
    }

  return 0;
}

/******************************************************************************
* Function name - eat_ws
*
* Description - Eats leading white space. Returns pointer to the start of 
*                    the non-white-space or NULL. Returns via len a new length.
* 
* Input -               *ptr - pointer to the url context
* Input/Output- *len - pointer to a lenght 
* Return Code/Output - Returns pointer to the start of the non-white-space or NULL
*******************************************************************************/
char* eat_ws (char* ptr, size_t*const len)
{
  if (!ptr || !*len)
    return NULL;

  while (*len && is_ws (ptr))
    ++ptr, --(*len);

  return *len ? ptr : NULL;
}

/******************************************************************************
* Function name - skip_non_ws
*
* Description - Skips non-white space. Returns pointer to the start of 
*                    the white-space or NULL. Returns via len a new length.
* 
* Input -               *ptr - pointer to the url context
* Input/Output- *len - pointer to a lenght 
* Return Code/Output - Returns pointer to the start of the white-space or NULL
*******************************************************************************/
static char* skip_non_ws (char*ptr, size_t*const len)
{
  if (!ptr || !*len)
    return NULL;

  while (*len && is_non_ws (ptr))
    ++ptr, --(*len);

  return *len ? ptr : NULL;
}

/******************************************************************************
* Function name - is_ws
*
* Description - Determines, whether a char pointer points to a white space
* Input -               *ptr - pointer to the url context
* Return Code/Output - If white space - 1, else 0
*******************************************************************************/
static int is_ws (char*const ptr)
{
  return (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') ? 1 : 0;
}

/******************************************************************************
* Function name - is_non_ws
*
* Description - Determines, whether a char pointer points to a non-white space
* Input -               *ptr - pointer to the url context
* Return Code/Output - If non-white space - 1, else 0
*******************************************************************************/
static int is_non_ws (char*const ptr)
{
  return ! is_ws (ptr);
}

/*
**
** TAG PARSERS IMPLEMENTATION
**
*/
static int batch_name_parser (batch_context*const bctx, char*const value)
{
    strncpy (bctx->batch_name, value, BATCH_NAME_SIZE);
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
static int ip_shared_num_parser (batch_context*const bctx, char*const value)
{
  bctx->ip_shared_num = atol (value);
  if (bctx->ip_shared_num <= 0)
    {
      fprintf (stderr, 
               "%s - error: a positive number is expected as the value"
               "for tag IP_SHARED_NUM\n", __func__);
      return -1;
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
static int clients_rampup_inc_parser (batch_context*const bctx, char*const value)
{
    bctx->clients_rampup_inc = atol (value);
    if (bctx->clients_rampup_inc < 0)
    {
        fprintf (stderr, 
                 "%s - error: clients_rampup_inc (%s) should be a zero or positive number\n", 
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
                 "%s - error: number of urls above the value of URLS_NUM value.\n",
                 __func__);
        return -1;
    }
    
    if (! (url_length = strlen (value)))
    {
        if (! bctx->url_index)
        {
            fprintf(stderr, "%s - error: empty url is OK not as the first url\n", __func__);
            return -1;
        }

        /* Inherits application type of the primary url */
        bctx->url_ctx_array[bctx->url_index].url_appl_type = 
            bctx->url_ctx_array[bctx->url_index -1].url_appl_type;
    }
    else 
    {
        if (! (bctx->url_ctx_array[bctx->url_index].url_str = 
               (char *) calloc (url_length +1, sizeof (char))))
        {
            fprintf (stderr,
                     "%s - error: allocation failed for url string \"%s\"\n", 
                     __func__, value);
            return -1;
        }

        bctx->url_ctx_array[bctx->url_index].url_str_len = url_length +1;

        strcpy(bctx->url_ctx_array[bctx->url_index].url_str, value);

        bctx->url_ctx_array[bctx->url_index].url_appl_type = 
            url_schema_classification (value);
    }
    
    bctx->url_ctx_array[bctx->url_index].url_ind = bctx->url_index;
    
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
  size_t hdr_len;
  if (!value || !(hdr_len = strlen (value)))
    {
      fprintf (stderr, "%s - error: wrong input.\n", __func__);
      return -1;
    }
  
  const char colomn = ':';
  url_context* url = &bctx->url_ctx_array[bctx->url_index];

  if (url->url_appl_type == URL_APPL_HTTP || 
      url->url_appl_type == URL_APPL_HTTPS)
    {
      if (!strchr (value, colomn))
        {
          fprintf (stderr, 
                   "%s - error: HTTP protocol requires \"%c\" colomn symbol" 
                   " in HTTP headers.\n", __func__, colomn);
          return -1;
        }
    }

  if (url->custom_http_hdrs_num >= CUSTOM_HDRS_MAX_NUM)
    {
      fprintf (stderr, 
               "%s - error: number of custom HTTP headers is limited to %d.\n", 
               __func__, CUSTOM_HDRS_MAX_NUM);
      return -1;
    }

  if (!(url->custom_http_hdrs = curl_slist_append (url->custom_http_hdrs,
                                                   value)))
    {
      fprintf (stderr, "%s - error: failed to append the header \"%s\"\n", 
               __func__, value);
      return -1;
    }
  
  url->custom_http_hdrs_num++;

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
      fprintf(stderr, "%s - warning: empty USERNAME \n", 
              __func__);
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
      fprintf(stderr, "%s - warning: empty PASSWORD\n", 
              __func__);
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

  const size_t value_len = strlen (value);

  if (!value_len)
    {
      fprintf(stderr, "%s - error: empty FORM_STRING tag is not supported.\n", 
              __func__);
      return -1;
    }

  if (ftype <= FORM_USAGETYPE_START || ftype >= FORM_USAGETYPE_END)
    {
      fprintf(stderr, "%s - error: please, beyond FORM_STRING place the "
              "defined FORM_USAGE_TYPE tag with its values to be choosen from:"
              "%s , %s ,\n" "%s , %s , %s \n" , __func__, 
              FT_UNIQUE_USERS_AND_PASSWORDS, FT_UNIQUE_USERS_SAME_PASSWORD,
              FT_SINGLE_USER, FT_RECORDS_FROM_FILE, FT_AS_IS);
      return -1;
    }

  if (strcmp (value, NON_APPLICABLE_STR) || 
      strcmp (value, NON_APPLICABLE_STR_2))
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

      if (! (bctx->url_ctx_array[bctx->url_index].form_str = 
             calloc (value_len +1, sizeof (char))))
        {
          fprintf(stderr, 
                  "%s - error: failed to allocate memory for FORM_STRING value.\n", 
                  __func__);
          return -1;
        }

      strncpy (bctx->url_ctx_array[bctx->url_index].form_str, value, value_len);
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
          fprintf(stderr, 
                  "%s error: failed to allocate memory for form_records_file" 
                  "with errno %d.\n",  __func__, errno);
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

static int form_records_random_parser (batch_context*const bctx, char*const value)
{
  long url_form_records_random_flag = 0;
  url_form_records_random_flag = atol (value);

  if (url_form_records_random_flag < 0 || url_form_records_random_flag > 1)
    {
      fprintf (stderr, 
               "%s - error: FORM_RECORDS_RANDOM should be either 0 or 1 and not %ld.\n",
               __func__, url_form_records_random_flag);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].form_records_random =  url_form_records_random_flag;
  return 0;

}

static int form_records_file_max_num_parser(batch_context*const bctx, char*const value)
{
  long max_records = 0;
  max_records = atol (value);

  if (max_records < 0)
    {
      fprintf (stderr, 
               "%s - error: FORM_RECORDS_FILE_MAX_NUM should be a "
               "positive value and not %ld.\n", __func__, max_records);
      return -1;
    }

  if (bctx->url_ctx_array[bctx->url_index].form_records_file)
    {
      fprintf (stderr, 
               "%s - error: FORM_RECORDS_FILE_MAX_NUM should be specified "
               "prior to tag FORM_RECORDS_FILE\n"
               "Please, change the order of the tags in your configuration.\n", __func__);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].form_records_file_max_num =  max_records;
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

      bctx->url_ctx_array[bctx->url_index].upload_file_size = statbuf.st_size;
    }
    return 0;
}

static int multipart_form_data_parser (batch_context*const bctx, char*const value)
{
  char* fieldname = 0, *eq = 0, *content;
  size_t value_len = strlen (value);
  url_context* url = &bctx->url_ctx_array[bctx->url_index];
  
  if (!value_len)
    {
      fprintf(stderr, "%s - error: zero length value passed.\n", __func__);
      return -1;
    }
  
  /* 
     Examples:
         
     "yourname=Michael" 
     "filedescription=Cool text file with cool text inside"
     "htmlcode=<HTML></HTML>;type=text/html"

     "file=@cooltext.txt"
     "coolfiles=@fil1.gif,fil2.txt,fil3.html" 
  */
  
  if (! (eq = strchr (value, '=')))
    {
      fprintf(stderr, "%s - error: no '=' sign in multipart_form_data.\n", __func__);
      return -1;
    }
  
  *eq = '\0';
  fieldname = value;
  
  /* TODO: Test also fieldname not to be empty space */
  if (!strlen (fieldname))
    {
      fprintf(stderr, "%s - error: name prior to = is empty.\n", __func__);
      return -1;
    }
      
  if (eq - value >= (int) value_len)
    {
      fprintf(stderr, "%s - error: no data after = sign.\n", __func__);
      return -1;
    }
  
  content = eq + 1;
  

#define FORM_CONTENT_TYPE_STR ";type=" 
  
  char* content_type = 0;
  size_t content_type_len = 0;
  int files = 0;
  char* pos_current = content;
  size_t files_number = 1;
  const char comma = ',';

  if (*content == '@')
    {
      files = 1;
      if (content - value >= (int) value_len)
        {
          fprintf(stderr, "%s - error: no filename after  sign '@'.\n", __func__);
          return -1;
        }
      content += 1;

        /* Count the number of files/commas */
      while (*pos_current && (pos_current = strchr (pos_current, comma)))
        {
          ++files_number;
          ++pos_current;
        }
    }

  content_type = strstr (content, FORM_CONTENT_TYPE_STR);
  
  if (content_type)
    content_type_len = strlen (content_type);
  
  if (content_type && content_type_len)
    {
      if (content_type_len <= strlen (FORM_CONTENT_TYPE_STR))
        {
          fprintf(stderr, "%s - error: content type, if appears should not be empty.\n", 
                  __func__);
          return -1;
        }
      
      *content_type = '\0'; /* place instead of ';' of ';type=' zero - '\0' */
      content_type = content_type + strlen (FORM_CONTENT_TYPE_STR);
      
    }

  if (! files)
    {
  
      if (content_type)
        {
          curl_formadd (&url->mpart_form_post, &url->mpart_form_last, 
                        CURLFORM_COPYNAME, fieldname,
                        CURLFORM_COPYCONTENTS, content,
                        CURLFORM_CONTENTTYPE, content_type, 
                        CURLFORM_END);
        }
      else
        {
          /* Default content-type */
          curl_formadd (&url->mpart_form_post, &url->mpart_form_last, 
                        CURLFORM_COPYNAME, fieldname,
                        CURLFORM_COPYCONTENTS, content,
                        CURLFORM_END);
        }

      return 0;
    }

  /* Coming here, if content is a file or files 'if (*content == '@')' is TRUE */
  
  // We allow content-type only for a single file. 
  if (content_type)
    {
      if (files_number != 1)
        {
          fprintf(stderr, "%s - error: content type is allowed only " 
                  "when a single file passed.\n", __func__);
          return -1;
        }
      else
        {
          curl_formadd (&url->mpart_form_post, &url->mpart_form_last, 
                        CURLFORM_COPYNAME, fieldname,
                        CURLFORM_FILE, content,
                        CURLFORM_CONTENTTYPE, content_type, 
                        CURLFORM_END);
        }
    }

  if (!content_type)
    {
      // Multiple files, or a single file without content type
      struct curl_forms* forms =  NULL;

      if (! (forms = calloc (files_number + 1, sizeof (struct curl_forms))))
        {
          fprintf(stderr, "%s - error: calloc of curl_forms failed with errno %d\n",
                  __func__, errno);
          return -1;
        }

      char * token = 0, *strtokp = 0;
      size_t token_index = 0;
      
      for (token = strtok_r (content, &comma, &strtokp); 
           token != 0;
           token = strtok_r (0, &comma, &strtokp))
        {
          size_t token_len = strlen (token);
          
          if (! token_len)
            {
              fprintf (stderr, "%s - warning: token is empty. \n", __func__);
            }
          else
            {
              forms [token_index].option = CURLFORM_FILE;
              forms [token_index].value  = token;
            }
          
          token_index++;
        }

      curl_formadd (&url->mpart_form_post, &url->mpart_form_last, 
                   CURLFORM_COPYNAME, fieldname,
                   CURLFORM_ARRAY, forms, 
                   CURLFORM_END);
    }

  return 0;
}

static int web_auth_method_parser (batch_context*const bctx, char*const value)
{
  url_context* url = &bctx->url_ctx_array[bctx->url_index];

  if (!strcmp (value, NON_APPLICABLE_STR))
    {
      url->web_auth_method = AUTHENTICATION_NO;
      return 0;
    }

  if (!strcmp (value, AUTH_BASIC))
      url->web_auth_method = AUTHENTICATION_BASIC;
  else if (!strcmp (value, AUTH_DIGEST))
      url->web_auth_method = AUTHENTICATION_DIGEST;
  else if (!strcmp (value, AUTH_GSS_NEGOTIATE))
    url->web_auth_method = 
      AUTHENTICATION_GSS_NEGOTIATE;
  else if (!strcmp (value, AUTH_NTLM))
    url->web_auth_method = AUTHENTICATION_NTLM;
  else if (!strcmp (value, AUTH_ANY))
    url->web_auth_method = AUTHENTICATION_ANY;
  else
    {
      fprintf (stderr, 
               "\n%s - error: WEB_AUTH_METHOD (%s) is not valid. \n"
               "Please, use: %s, %s \n" "%s, %s, %s\n",
               __func__, value, AUTH_BASIC, AUTH_DIGEST, 
               AUTH_GSS_NEGOTIATE, AUTH_NTLM, AUTH_ANY);
      return -1;
    }
    
  return 0;
}
static int web_auth_credentials_parser (batch_context*const bctx, char*const value)
{
  size_t string_len = 0;

  if (! (string_len = strlen (value)))
    {
      fprintf(stderr, "%s - warning: empty WEB_AUTH_CREDENTIALS\n", 
              __func__);
      return 0;
    }

  string_len++;
  
  if (!(bctx->url_ctx_array[bctx->url_index].web_auth_credentials = 
       (char *) calloc (string_len, sizeof (char))))
    {
      fprintf(stderr, 
                  "%s error: failed to allocate memory for WEB_AUTH_CREDENTIALS" 
                  "with errno %d.\n",  __func__, errno);
      return -1;
    }

  const char separator = ':';
  if (!strchr (value, separator))
    {
      fprintf(stderr, 
                  "%s error: separator (%c) of username and password to be "
              "present in the credentials string \"%s\"\n", 
              __func__, separator, value);
      return -1;
    }

  strncpy (bctx->url_ctx_array[bctx->url_index].web_auth_credentials, 
           value, 
           string_len -1);
  
  return 0;
}
static int proxy_auth_method_parser (batch_context*const bctx, char*const value)
{
  url_context* url = &bctx->url_ctx_array[bctx->url_index];

  if (!strcmp (value, NON_APPLICABLE_STR))
    {
      url->proxy_auth_method = AUTHENTICATION_NO;
      return 0;
    }

  if (!strcmp (value, AUTH_BASIC))
      url->proxy_auth_method = AUTHENTICATION_BASIC;
  else if (!strcmp (value, AUTH_DIGEST))
      url->proxy_auth_method = AUTHENTICATION_DIGEST;
  else if (!strcmp (value, AUTH_GSS_NEGOTIATE))
    url->proxy_auth_method = AUTHENTICATION_GSS_NEGOTIATE;
  else if (!strcmp (value, AUTH_NTLM))
    url->proxy_auth_method = AUTHENTICATION_NTLM;
  else if (!strcmp (value, AUTH_ANY))
    url->proxy_auth_method = AUTHENTICATION_ANY;
  else
    {
      fprintf (stderr, 
               "\n%s - error: PROXY_AUTH_METHOD (%s) is not valid. \n"
               "Please, use: %s, %s \n" "%s, %s, %s\n",
               __func__, value, AUTH_BASIC, AUTH_DIGEST, AUTH_GSS_NEGOTIATE,
               AUTH_NTLM, AUTH_ANY);
      return -1;
    }
  return 0;
}
static int proxy_auth_credentials_parser (batch_context*const bctx, char*const value)
{
  size_t string_len = 0;

  if (! (string_len = strlen (value)))
    {
      fprintf(stderr, "%s - warning: empty PROXY_AUTH_CREDENTIALS\n", 
              __func__);
      return 0;
    }

  string_len++;
  
  if (! (bctx->url_ctx_array[bctx->url_index].proxy_auth_credentials = 
       (char *) calloc (string_len, sizeof (char))))
    {
      fprintf(stderr, 
                  "%s error: failed to allocate memory for PROXY_AUTH_CREDENTIALS" 
                  "with errno %d.\n",  __func__, errno);
      return -1;
    }

  const char separator = ':';
  if (!strchr (value, separator))
    {
      fprintf(stderr, 
                  "%s error: separator (%c) of username and password to be "
              "present in the credentials string \"%s\"\n", 
              __func__, separator, value);
      return -1;
    }

  strncpy (bctx->url_ctx_array[bctx->url_index].proxy_auth_credentials, 
           value, 
           string_len -1);

  return 0;
}

static int fresh_connect_parser (batch_context*const bctx, char*const value)
{
    long boo = atol (value);

    if (boo < 0 || boo > 1)
    {
        fprintf(stderr, 
                "%s error: boolean input 0 or 1 is expected\n", __func__);
        return -1;
    }
    bctx->url_ctx_array[bctx->url_index].fresh_connect = boo;
    return 0;
}

static int timer_tcp_conn_setup_parser (batch_context*const bctx, char*const value)
{
    long timer = atol (value);

    if (timer <= 0 || timer > 50)
    {
        fprintf(stderr, 
                "%s error: input of the timer is expected  to be from " 
		"1 up to 50 seconds.\n", __func__);
        return -1;
    }
    bctx->url_ctx_array[bctx->url_index].connect_timeout= timer;
    return 0;
}
static int timer_url_completion_parser (batch_context*const bctx, 
					char*const value)
{
  long timer_lrange = 0;
  long timer_hrange = 0;
  size_t value_len = strlen (value) + 1;

  if (parse_timer_range (value,
                         value_len,
                         &timer_lrange,
                         &timer_hrange) == -1)
    {
      fprintf(stderr, "%s error: parse_timer_range () failed.\n", __func__);
      return -1;
    }
  
  if (!timer_hrange && timer_lrange > 0 && timer_lrange < 20)
    {
      fprintf(stderr, 
              "%s error: the timer should be either 0 or 20 msec and more, not %ld.\n"
              "Note, that since version 0.31 the timer is in msec and enforced by\n"
              "monitoring time of each url fetching and cancelling, when it \n"
              "takes msec above the timer. Operation statistics provides a view\n"
              "on what happens as URL-Timed out and statistics T-Err counter.\n"
              "To preserve the previous behavior without the timer enforcement,\n"
              "please, place 0 value here.\n",
              __func__, timer_lrange);
      return -1;
    }

    bctx->url_ctx_array[bctx->url_index].timer_url_completion_lrange = 
      timer_lrange;

    bctx->url_ctx_array[bctx->url_index].timer_url_completion_hrange = 
      timer_hrange;

    return 0;
}
static int timer_after_url_sleep_parser (batch_context*const bctx, 
					 char*const value)
{
  long timer_lrange = 0;
  long timer_hrange = 0;
  size_t value_len = strlen (value) + 1;
  
  if (parse_timer_range (value,
                         value_len,
                         &timer_lrange,
                         &timer_hrange) == -1)
    {
      fprintf(stderr, "%s error: parse_timer_range () failed.\n", __func__);
      return -1;
    }
  
    if (!timer_hrange && timer_lrange > 0 && timer_lrange < 20)
      {
        fprintf(stderr, 
                "%s error: the timer should be either 0 or 20 msec and more.\n",
                __func__);
        return -1;
      }

    bctx->url_ctx_array[bctx->url_index].timer_after_url_sleep_lrange = 
      timer_lrange;

    bctx->url_ctx_array[bctx->url_index].timer_after_url_sleep_hrange = 
      timer_hrange;

    return 0;
}

static int ftp_active_parser (batch_context*const bctx, char*const value)
{
  long status = atol (value);
  if (status < 0 || status > 1)
    {
      fprintf(stderr, "%s error: ether 0 or 1 are allowed.\n", __func__);
      return -1;
    }
  bctx->url_ctx_array[bctx->url_index].ftp_active = status;
  return 0;
}

static int log_resp_headers_parser (batch_context*const bctx, char*const value)
{
  long status = atol (value);
  if (status < 0 || status > 1)
    {
      fprintf(stderr, "%s error: ether 0 or 1 are allowed.\n", __func__);
      return -1;
    }
  bctx->url_ctx_array[bctx->url_index].log_resp_headers = status;
  return 0;
}
static int log_resp_bodies_parser (batch_context*const bctx, char*const value)
{
  long status = atol (value);
  if (status < 0 || status > 1)
    {
      fprintf(stderr, "%s error: ether 0 or 1 are allowed.\n", __func__);
      return -1;
    }
  bctx->url_ctx_array[bctx->url_index].log_resp_bodies = status;
  return 0;
}

static int response_status_errors_parser (batch_context*const bctx, 
                                          char*const value)
{
  url_context* url = &bctx->url_ctx_array[bctx->url_index];
 
  /* Allocate the table */
  if (! (url->resp_status_errors_tbl = 
         calloc (URL_RESPONSE_STATUS_ERRORS_TABLE_SIZE, 
                 sizeof (unsigned char))))
    {
      fprintf (stderr, "%s - error: calloc () failed with errno %d.\n", 
              __func__, errno);
      return -1;
    }

  /* Copy the default table */
  memcpy (url->resp_status_errors_tbl, 
          resp_status_errors_tbl_default,
          URL_RESPONSE_STATUS_ERRORS_TABLE_SIZE);

  long status = 0;
  const char separator =  ',';
  char * token = 0, *strtokp = 0;
  size_t token_len = 0;

  for (token = strtok_r (value, &separator, &strtokp); 
       token != 0;
       token = strtok_r (0, &separator, &strtokp))
    {
      if ((token_len = strlen (token)) < 2)
        {
          fprintf (stderr, "%s - error: token %s is too short.\n"
                   "Each valid token should start from + or - with a following "
                   "response status number.\n", __func__, token);
          return -1;
        }

      if (*token != '+' && *token != '-')
        {
          fprintf (stderr, "%s - error: token %s does not have leading + or - symbol.\n"
                   "Each valid token should start from + or - with a following "
                   "response status number.\n", __func__, token);
          return -1;
        }

      status = atol (token + 1);

      if (status < 0 || status > URL_RESPONSE_STATUS_ERRORS_TABLE_SIZE)
        {
          fprintf (stderr, "%s - error: token %s non valid.\n"
                   "Each valid token should start from + or - with a following "
                   "response status number in the range from 0 up to %d.\n", 
                   __func__, token, URL_RESPONSE_STATUS_ERRORS_TABLE_SIZE);
          return -1;
        }

      if (*token == '+')
        {
          url->resp_status_errors_tbl[status] = 1;
        }
      else if (*token == '-')
        {
          url->resp_status_errors_tbl[status] = 0;
        }
    }

  return 0;
}

static int transfer_limit_rate_parser (batch_context*const bctx, char*const value)
{
  long rate = atol (value);

  if (rate < 0)
    {
      fprintf(stderr, "%s error: negative rate is not allowed.\n", __func__);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].transfer_limit_rate = (curl_off_t) rate;
  return 0;
}

static int fetch_probability_parser (batch_context*const bctx, char*const value)
{
  long probability = atol (value);

  if (probability < 1 || probability > 100)
    {
      fprintf(stderr, "%s error: tag FETCH_PROBABILITY should be with "
              "from 1 up to 100.\n", __func__);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].fetch_probability = (int) probability;
  return 0;

}

static int fetch_probability_once_parser (batch_context*const bctx, 
                                          char*const value)
{
  long probability_once = atol (value);

  if (probability_once != 0 && probability_once != 1)
    {
      fprintf (stderr, 
              "%s error: tag FETCH_PROBABILITY_ONCE can be either 0 or 1.\n", 
              __func__);
      return -1;
    }

  bctx->url_ctx_array[bctx->url_index].fetch_probability_once = 
    (int) probability_once;
  return 0;
}

/******************************************************************************
* Function name - url_schema_classification
*
* Description - Makes url analyses to return the type (e.g. http, ftps, telnet, etc)
* 
* Input -      *url - pointer to the url context
* Return Code/Output - On success - a url schema type, on failure - 
*                    (URL_APPL_UNDEF)
*******************************************************************************/
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


/******************************************************************************
* Function name - validate_batch
*
* Description - Validates all parameters in the batch. Calls validation 
*               functions for all sections.
* 
* Input -      *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
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
        fprintf (stderr, "%s - error: failed to validate batch section URL.\n", 
                 __func__);
        return -1;
    }

    return 0;
}

/******************************************************************************
* Function name - validate_batch_general
*
* Description - Validates section general parameters
* 
* Input -       *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
********************************************************************************/
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
        fprintf (stderr, "%s - error: CLIENT_NUM_START is less than 0.\n", 
		__func__);
        return -1;
    }
    if (bctx->client_num_start > bctx->client_num_max)
      {
        fprintf (stderr, 
                 "%s - error: CLIENT_NUM_START (%d) is less than CLIENT_NUM_MAX (%d).\n", 
                 __func__, bctx->client_num_start, bctx->client_num_max);
        return -1;
      }
    if (bctx->clients_rampup_inc < 0)
    {
        fprintf (stderr, "%s - error: CLIENTS_RAMPUP_INC is negative.\n",__func__);
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

    if (! bctx->ipv6)
      {
        if (bctx->ip_addr_min && (bctx->ip_addr_min == bctx->ip_addr_max))
          {
            bctx->ip_shared_num =1;
          }
        else
          {
            if (!bctx->ip_shared_num &&
                ((bctx->ip_addr_max - bctx->ip_addr_min + 1) < bctx->client_num_max))
              {
                fprintf (stderr, "%s - error: range of IPv4 addresses is less than number of clients.\n"
                         "Please, increase IP_ADDR_MAX.\n", __func__);
                return -1;
              }
          }
      }
    else
      {
        // IPv6
        if (! memcmp (&bctx->ipv6_addr_min, 
                      &bctx->ipv6_addr_max, 
                      sizeof (bctx->ipv6_addr_min)))
          {
            bctx->ip_shared_num =1;
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

/******************************************************************************
* Function name - validate_batch_url
*
* Description - Validates section URL parameters
* 
* Input -       *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
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

          /* 
             HTTP POST-ing requires either FORM_STRING or 
             MULTIPART_FORM_DATA tags.
          */ 
          if (req_type == HTTP_REQ_TYPE_POST)
            {
              if (!url->form_str && !url->mpart_form_post)
                {
                  fprintf (stderr, "%s - error: either FORM_STRING or "
                           "MULTIPART_FORM_DATA tags should be defined to "
                           "make HTTP POST\n", __func__);
                  return -1;   
                }

              if (url->form_str && url->mpart_form_post)
                {
                  fprintf (stderr, "%s - error: either FORM_STRING or "
                           "MULTIPART_FORM_DATA tags, but not the both, should be "
                           "defined to make HTTP POST\n", __func__);
                  return -1;   
                }
             }
        }
      
      if (url->form_records_file && !url->form_str)
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
          if (!url->url_str || ! url->url_str_len)
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

/******************************************************************************
* Function name - create_response_logfiles_dirs
*
* Description - When at least a single URL configuration requires logging of
*                    responses, creates a directory with the same name as for the batch. 
* 
* Input -      *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
int create_response_logfiles_dirs (batch_context* bctx)
{
  int dir_created_flag = 0;
  char dir_log_resp[256];
  const mode_t mode= S_IRWXU|S_IRWXG|S_IRWXO;
  
  memset (dir_log_resp, 0, sizeof (dir_log_resp));

  snprintf (dir_log_resp, sizeof (dir_log_resp) -1, 
                  "./%s", bctx->batch_name);

  int k = 0;
  for (k = 0; k < bctx->urls_num; k++)
    {
      url_context* url = &bctx->url_ctx_array[k];

      if (url->log_resp_bodies || url->log_resp_headers)
        {

          /* Create the directory, if not created before. */
          if (!dir_created_flag)
            {  
              if(mkdir (dir_log_resp, mode) == -1 && errno !=EEXIST )
                {
                  fprintf (stderr, 
                           "%s - error: mkdir () failed with errno %d to create dir \"%s\".\n",
                           __func__, errno, dir_log_resp);
                  return -1;
                }
              dir_created_flag =1;
            }

          /* 
             Create subdirs for responses logfiles, if configured.
          */
          memset (dir_log_resp, 0, sizeof (dir_log_resp));

          snprintf (dir_log_resp, sizeof (dir_log_resp) -1, 
                    "./%s/url%ld/", 
                    bctx->batch_name, 
                    url->url_ind);

          if (mkdir (dir_log_resp, mode) == -1 && errno !=EEXIST )
            {
              fprintf (stderr, "%s - error: mkdir () failed with errno %d.\n",
                       __func__, errno);
              return -1;
            }

          const size_t dir_log_len = strlen (dir_log_resp) + 1;

          if (! (url->dir_log = calloc (dir_log_len, sizeof (char))))
            {
              fprintf (stderr, "%s - error:  calloc () failed with errno %d.\n",
                       __func__, errno);
              return -1;
            }
          else
            {
              strncpy (url->dir_log, dir_log_resp, dir_log_len -1);
            }
        }
    }
  return 0;
}

/******************************************************************************
* Function name - alloc_client_formed_buffers
*
* Description - Allocates client formed buffers to be used for POST-ing. Size of the
*                    buffers is taken as a maximum possible lenght. The buffers will be
*                    initialized for POST-ing for each URL, that is using POST, and 
*                    according to the detailed URL-based configuration, tokens, 
*                    form-type, etc.
* 
* Input -      *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
int alloc_client_formed_buffers (batch_context* bctx)
{
  int k = 0;

  for (k = 0; k < bctx->urls_num; k++)
    {
      url_context* url = &bctx->url_ctx_array[k];
      
      /*
        Allocate posting buffers for clients (login, logoff other posting), 
        if at least a single url contains method HTTP POST and 
        FORM_STRING.
      */
      if (url->req_type == HTTP_REQ_TYPE_POST && url->form_str)
        {
          int i;
          for (i = 0;  i < bctx->client_num_max; i++)
            {
              client_context* cctx = &bctx->cctx_array[i];
              
              if (!cctx->post_data && !cctx->post_data_len)
                {
                  size_t form_string_len = strlen (url->form_str);
                  
                  if (form_string_len)
                    {
                      cctx->post_data_len = form_string_len + 1 +
                        FORM_RECORDS_MAX_TOKENS_NUM*
                        (FORM_RECORDS_TOKEN_MAX_LEN +
                         FORM_RECORDS_SEQ_NUM_LEN);
                      
                      if (! (cctx->post_data = 
                             (char *) calloc (cctx->post_data_len, sizeof (char))))
                        {
                          fprintf (stderr,
                                   "\"%s\" error: failed to allocate client "
				   "post_data buffer.\n", 
                                   __func__) ;
                          return -1;
                        }
                    }
                }
            }
        } /* end of post-ing buffers allocation */
      
      else if (url->req_type == HTTP_REQ_TYPE_GET && url->form_str)
        {
          int j;
          for (j = 0;  j < bctx->client_num_max; j++)
            {
              client_context* cctx = &bctx->cctx_array[j];
              
              if (!cctx->get_url_form_data && !cctx->get_url_form_data_len)
                {
                  size_t form_string_len = strlen (url->form_str);
                  
                  if (form_string_len)
                    {
                      cctx->get_url_form_data_len = url->url_str_len + 
		        form_string_len + 1 +
                        FORM_RECORDS_MAX_TOKENS_NUM*
                        (FORM_RECORDS_TOKEN_MAX_LEN + FORM_RECORDS_SEQ_NUM_LEN);
                      
                      if (! (cctx->get_url_form_data = 
                             (char *) calloc (cctx->get_url_form_data_len, sizeof (char))))
                        {
                          fprintf (stderr,
                                   "\"%s\" error: failed to allocate client "
				   "get_url_form_data buffer.\n", __func__) ;
                          return -1;
                        }
                    }
                }
            }
        } /* end of get-url-form buffers allocation */
      
    }

  return 0;
}

/******************************************************************************
* Function name - alloc_client_fetch_decision_array
*
* Description - Allocates client URL fetch decision arrays to be used, when 
*                    fetching decision to be done only during the first cycle 
*                    and remembered (in fetch_decision_array).
* 
* Input -      *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
int alloc_client_fetch_decision_array (batch_context* bctx)
{
  int k = 0;
  
  for (k = 0; k < bctx->urls_num; k++)
    {
      url_context* url = &bctx->url_ctx_array[k];

      if (url->fetch_probability && url->fetch_probability_once)
        {
          int i;
          for (i = 0;  i < bctx->client_num_max; i++)
            {
              client_context* cctx = &bctx->cctx_array[i];

              if (!cctx->url_fetch_decision)
                {
                  if (!(cctx->url_fetch_decision = calloc (bctx->urls_num, sizeof (char))))
                    {
                      fprintf (stderr, "\"%s\" error: failed to allocate client "
                               "url_fetch_decision buffer.\n", __func__) ;
                      return -1;
                    }
                  memset (cctx->url_fetch_decision, -1, bctx->urls_num);
                }
            }
        }
    }
  return 0;
}

/******************************************************************************
* Function name - init_operational_statistics
*
* Description - Allocates and inits operational statistics structures.
* 
* Input -      *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
int init_operational_statistics(batch_context* bctx)
{
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

  return 0;
}


/******************************************************************************
* Function name - post_validate_init
*
* Description - Performs post validate initializations of a batch context.
* 
* Input -       *bctx - pointer to the initialized batch context to validate
* Return Code/Output - On success - 0, on failure - (-1)
*******************************************************************************/
static int post_validate_init (batch_context*const bctx)
{
  /*
    Allocate client contexts, if not allocated before.
  */
  if (!bctx->cctx_array)
    {
      if (!(bctx->cctx_array =
            (client_context *) cl_calloc (bctx->client_num_max, 
                                          sizeof (client_context))))
        {
          fprintf (stderr, "\"%s\" - %s - failed to allocate cctx.\n", 
                   bctx->batch_name, __func__);
          return -1;
        }
    }

  if (create_response_logfiles_dirs (bctx) == -1)
    {
      fprintf (stderr, 
               "\"%s\" - create_response_logfiles_dirs () failed .\n", 
               __func__);
      return -1;
    }

  if (alloc_client_formed_buffers (bctx) == -1)
    {
      fprintf (stderr, 
               "\"%s\" - alloc_client_formed_buffers () failed .\n", 
               __func__);
      return -1;
    }

  if (alloc_client_fetch_decision_array (bctx) == -1)
    {
      fprintf (stderr, 
               "\"%s\" - alloc_client_fetch_decision_array () failed .\n", 
               __func__);
      return -1;
    }
 
  if (init_operational_statistics (bctx) == -1)
    {
      fprintf (stderr, 
               "\"%s\" - init_operational_statistics () failed .\n", 
               __func__);
      return -1;
    }

  /* 
     It should be the last check.
  */
  fprintf (stderr, "\nThe configuration has been post-validated successfully.\n\n");

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
* Function name - set_default_response_errors_table
*
* Description - Inits by defaults the response codes, considered as errors.
*                          
* Return Code/Output - None
********************************************************************************/
static  void set_default_response_errors_table ()
{
  memset (&resp_status_errors_tbl_default, 
          0, 
          sizeof (resp_status_errors_tbl_default));

  memset (resp_status_errors_tbl_default + 400, 
          1,
          sizeof (resp_status_errors_tbl_default) - 400);

  resp_status_errors_tbl_default[401] = 0;
  resp_status_errors_tbl_default[407] = 0;
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
               "If you are using example configurations, note, that "
	       	"directory \"configs\" have been renamed to \"conf-examples\".", 
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

  set_default_response_errors_table ();

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
                   "%s - error: post_validate_init () for batch %d failed.\n",
		   __func__, k);
          return -1;
        }
    }

  return (batch_index + 1);
}


/*******************************************************************************
* Function name - load_form_records_file
*
* Description - Itializes client post form buffers, using credentials loaded 
* 		from file. To be called after batch context validation.
*
* Input -       *bctx - pointer to the batch context
*                          
* Return Code/Output - On Success - number of batches >=1, on Error -1
********************************************************************************/
static int load_form_records_file (batch_context*const bctx, url_context* url)
{
  char fgets_buff[512];
  FILE* fp;
  char* sep = 0; // strtok_r requires a string with '\0' 

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

  const size_t max_records = url->form_records_file_max_num ? 
    url->form_records_file_max_num : (size_t)bctx->client_num_max;

  /* 
     Allocate the place to keep form records tokens for clients
  */
  if (! (url->form_records_array =  calloc (max_records, 
                                            sizeof (form_records_cdata))))
  {
      fprintf (stderr, "%s - failed to allocate memory for "
               "url->form_records_array with errno %d.\n", __func__, errno);
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

          if (fgets_buff[string_len - 2] == '\r')
            {
              fgets_buff[string_len - 2] = '\0';
            }

          if (fgets_buff[string_len -1] == '\n')
            {
              fgets_buff[string_len -1] = '\0';
            }

          if (url->form_records_num >= max_records)
            {
              fprintf (stderr, "%s - error: CLIENTS_NUM (%d) and "
                       "FORM_RECORDS_FILE_MAX_NUM (%d) are both less than the number of" 
                       "records is the file form_records_file.\n", 
                       __func__, bctx->client_num_max, url->form_records_file_max_num);
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
                       "%s - error: load_client_credentials_buffers () failed "
		       "on records line \"%s\"\n", __func__, fgets_buff);
              fclose (fp);
              return -1 ;
          }
          
          url->form_records_num++;
        }
    }

  if (!url->form_records_random && (int)url->form_records_num < bctx->client_num_max)
    {
      fprintf (stderr, 
               "%s - error: CLIENTS_NUM (%d) is above the number " 
               "of records in the form_records_file\nPlease, either decrease "
	       "the CLIENTS_NUM or add more records strings to the file.\n", 
               __func__, bctx->client_num_max);
      fclose (fp);
      return -1 ;
    }

  fclose (fp);
  return 0;
}

/*******************************************************************************
* Function name -  find_first_cycling_url
*
* Description - Finds the first (by index) url, where cycling is required.
*
* Input -       *bctx - pointer to the batch context
*                          
* Return Code/Output - If found - the url non-negative index. Return ( -1),
*                      when no cycling urls configured.
********************************************************************************/
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

/*******************************************************************************
* Function name -  find_last_cycling_url
*
* Description - Finds the last (by index) url, where cycling is required.
*
* Input -       *bctx - pointer to the batch context
*                          
* Return Code/Output - If found - the url non-negative index. Return ( -1),
*                      when no cycling urls configured.
********************************************************************************/
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

/*******************************************************************************
* Function name -  print_correct_form_usagetype
*
* Description - Outputs explanation about mismatching of FORM_STRING and
*                     FORM_USAGE_TYPE. 
*
* Input -       ftype - type of form usage
*                     *value - the value of FORM_USAGE_TYPE tag
*                          
* Return Code/Output - Returns always -1 as being error output.
********************************************************************************/
static int print_correct_form_usagetype (form_usagetype ftype, char* value)
{
  switch (ftype)
    {
    case FORM_USAGETYPE_UNIQUE_USERS_AND_PASSWORDS:

      fprintf (stderr, 
               "\n%s - error: FORM_STRING value (%s) is not valid. \nPlease, use:\n"
               "- to generate unique users with unique passwords two \"%%s%%d\" "
	       ", something like \"user=%%s%%d&password=%%s%%d\" \n", 
	       __func__, value);
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
               "- for a single configurable user with a password two \"%%s\" "
	       ", something like \"user=%%s&password=%%s\" \n",
	       __func__, value);
      break;

    case FORM_USAGETYPE_RECORDS_FROM_FILE:
      fprintf (stderr, 
               "\n%s - error: FORM_STRING value (%s) is not valid. \nPlease, use:\n"
               "- to load user credentials (records) from a file two \"%%s\" "
	       ", something like \"user=%%s&password=%%s\" \n and _FILE defined.\n", 
	       __func__, value);
      break;

    default:
      break;
    }

  return -1;
}

