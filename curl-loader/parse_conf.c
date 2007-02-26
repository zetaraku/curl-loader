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

#define BATCH_MAX_CLIENTS_NUM 4096

#define NON_APPLICABLE_STR ""
#define REQ_GET "GET"
#define REQ_POST "POST"
#define REQ_GET_POST "GET+POST"

/*
  value - supposed to be a null-terminated string.
*/
typedef int (*fparser) (batch_context*const bctx, char*const value);

/*
  Used to map tag to its value parser function.
*/
typedef struct tag_parser_pair
{
    char* tag; /* string name of the param */
    fparser parser;
} tag_parser_pair;

static int batch_name_parser (batch_context*const bctx, char*const value);
static int clients_num_parser (batch_context*const bctx, char*const value);
static int interface_parser (batch_context*const bctx, char*const value);
static int netmask_parser (batch_context*const bctx, char*const value);
static int ip_addr_min_parser (batch_context*const bctx, char*const value);
static int ip_addr_max_parser (batch_context*const bctx, char*const value);
static int cycles_num_parser (batch_context*const bctx, char*const value);
static int clients_initial_inc_parser (batch_context*const bctx, char*const value);

static int login_parser (batch_context*const bctx, char*const value);
static int login_username_parser (batch_context*const bctx, char*const value);
static int login_password_parser (batch_context*const bctx, char*const value);
static int login_req_type_parser (batch_context*const bctx, char*const value);
static int login_req_type_get_post_parser (batch_context*const bctx, char*const value);
static int login_req_type_post_parser (batch_context*const bctx, char*const value);
static int login_post_str_parser (batch_context*const bctx, char*const value);
static int login_url_parser (batch_context*const bctx, char*const value);
static int login_url_max_time_parser (batch_context*const bctx, char*const value);
static int login_url_interleave_time_parser (batch_context*const bctx, char*const value);
static int login_cycling_parser (batch_context*const bctx, char*const value);

static int uas_parser (batch_context*const bctx, char*const value);
static int uas_urls_num_parser (batch_context*const bctx, char*const value);
static int uas_url_parser (batch_context*const bctx, char*const value);
static int uas_url_max_time_parser (batch_context*const bctx, char*const value);
static int uas_url_interleave_time_parser (batch_context*const bctx, char*const value);

static int logoff_parser (batch_context*const bctx, char*const value);
static int logoff_req_type_parser (batch_context*const bctx, char*const value);
static int logoff_req_type_get_post_parser (batch_context*const bctx, char*const value);
static int logoff_req_type_post_parser (batch_context*const bctx, char*const value);
static int logoff_req_type_get_parser (batch_context*const bctx, char*const value);
static int logoff_post_str_parser (batch_context*const bctx, char*const value);
static int logoff_url_parser (batch_context*const bctx, char*const value);
static int logoff_url_max_time_parser (batch_context*const bctx, char*const value);
static int logoff_url_interleave_time_parser (batch_context*const bctx, char*const value);
static int logoff_cycling_parser (batch_context*const bctx, char*const value);
static fparser find_tag_parser (const char* tag);

static const tag_parser_pair tp_map [] =
{
    /*------------------------ GENERAL SECTION ------------------------------ */
    {"BATCH_NAME", batch_name_parser},
    {"CLIENTS_NUM", clients_num_parser},
    {"INTERFACE", interface_parser},
    {"NETMASK", netmask_parser},
    {"IP_ADDR_MIN", ip_addr_min_parser},
    {"IP_ADDR_MAX", ip_addr_max_parser},
    {"CYCLES_NUM", cycles_num_parser},
    {"CLIENTS_INITIAL_INC", clients_initial_inc_parser},
    

    /*------------------------ LOGIN SECTION -------------------------------- */
    {"LOGIN", login_parser},
    /* if Login Authentication is yes - then the optional fields follow: */
    {"LOGIN_USERNAME", login_username_parser},
    {"LOGIN_PASSWORD", login_password_parser},
    {"LOGIN_REQ_TYPE", login_req_type_parser},
    {"LOGIN_REQ_TYPE_GET_POST", login_req_type_get_post_parser},
    {"LOGIN_REQ_TYPE_POST", login_req_type_post_parser},
    {"LOGIN_POST_STR", login_post_str_parser},
    {"LOGIN_URL", login_url_parser},
    {"LOGIN_URL_MAX_TIME", login_url_max_time_parser},
    {"LOGIN_URL_INTERLEAVE_TIME", login_url_interleave_time_parser},
    {"LOGIN_CYCLING", login_cycling_parser},

    /*------- UAS (User Activity Simulation) SECTION - fetching urls ----- */ 
    {"UAS", uas_parser},
    /* USER_ACTIVITY_SIMULATION - if yes, then optional N-URLs: */
    {"UAS_URLS_NUM", uas_urls_num_parser},
    {"UAS_URL", uas_url_parser},
    {"UAS_URL_MAX_TIME", uas_url_max_time_parser},
    {"UAS_URL_INTERLEAVE_TIME", uas_url_interleave_time_parser},


    /*------------------------LOGOFF SECTION ---------------------------------*/
    {"LOGOFF", logoff_parser},
    /* if logoff is yes - then the optional fields follow: */
    {"LOGOFF_REQ_TYPE", logoff_req_type_parser},
    {"LOGOFF_REQ_TYPE_GET_POST", logoff_req_type_get_post_parser},
    {"LOGOFF_REQ_TYPE_POST", logoff_req_type_post_parser},
    {"LOGOFF_REQ_TYPE_GET", logoff_req_type_get_parser},
    {"LOGOFF_POST_STR", logoff_post_str_parser},
    {"LOGOFF_URL", logoff_url_parser},
    {"LOGOFF_URL_MAX_TIME", logoff_url_max_time_parser},
    {"LOGOFF_URL_INTERLEAVE_TIME", logoff_url_interleave_time_parser},
    {"LOGOFF_CYCLING", logoff_cycling_parser},
  
    {NULL, 0}
};

static int validate_batch (batch_context*const bctx);
static int validate_batch_general (batch_context*const bctx);
static int validate_batch_login (batch_context*const bctx);
static int validate_batch_uas (batch_context*const bctx);
static int validate_batch_logoff (batch_context*const bctx);

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
static int netmask_to_cidr (char *dotted_ipv4);


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
  
  fparser parser = 0;
  if (! (parser = find_tag_parser (str_buff)))
  {
      fprintf (stderr, "%s - warning: unknown tag %s.\n",__func__, str_buff);
      return 0;
  }

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

  /* Remove quotes */
  if (*value == '"')
    {
      value++, value_len--;
      if (!value_len)
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
      /* On string "BATCH_NAME" move the number */
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

static int pre_parser (char** ptr, size_t* len)
{
    char* value_start = NULL;

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

    /* remove TWS */
    char* value_end = skip_non_ws (value_start, len);

    if (value_end)
    {
        *value_end = '\0';
    }

    *ptr = value_start;
    *len = strlen (value_start) + 1;
  
    return 0;
}

/**
**
** TAG PARSERS
**
*/
static int batch_name_parser (batch_context*const bctx, char*const value)
{
    strncpy (bctx->batch_name, value, sizeof (bctx->batch_name) -1);
    return 0;
}
static int clients_num_parser (batch_context*const bctx, char*const value)
{
    bctx->client_num = 0;
    bctx->client_num = atoi (value);
    
    /* fprintf (stderr, "\nclients number is %d\n", bctx->client_num); */
    if (bctx->client_num < 1)
    {
        fprintf (stderr, "%s - error: clients number (%d) is out of the range\n", 
                 __func__, bctx->client_num);
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

  if (! strchr (value, '.'))
    {
      /* CIDR number of non-masked first bits -16, 24, etc */
      bctx->cidr_netmask = atoi (value);
    }
  else
    {
      bctx->cidr_netmask = netmask_to_cidr (value);
    }
  
  if (bctx->cidr_netmask < 1 || bctx->cidr_netmask > 32)
    {
      fprintf (stderr, 
               "%s - error: network mask (%d) is out of the range\n", 
               __func__, bctx->cidr_netmask);
      return -1;
    }
  return 0;
}

static int ip_addr_min_parser (batch_context*const bctx, char*const value)
{
    struct in_addr in_address;
    memset (&in_address, 0, sizeof (struct in_addr));
    
    if (! inet_aton (value, &in_address))
        {
          fprintf (stderr, 
                   "%s - error: inet_aton failed for ip_addr_min %s\n", 
                   __func__, value);
          return -1;
        }
      bctx->ip_addr_min = ntohl (in_address.s_addr);
      return 0;
}
static int ip_addr_max_parser (batch_context*const bctx, char*const value)
{
    struct in_addr in_address;
    memset (&in_address, 0, sizeof (struct in_addr));
    
    if (!inet_aton (value, &in_address))
    {
        fprintf (stderr, 
                 "%s - error: inet_aton failed for ip_addr_max %s\n", 
                 __func__, value);
        return -1;
    }
    bctx->ip_addr_max = ntohl (in_address.s_addr);
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


static int login_parser (batch_context*const bctx, char*const value)
{
    bctx->do_login = (*value == 'Y' || *value == 'y') ? 1 : 0;
    return 0;
}
static int login_username_parser (batch_context*const bctx, char*const value)
{
    strncpy (bctx->login_username, value, sizeof(bctx->login_username) - 1);
    return 0;
}
static int login_password_parser (batch_context*const bctx, char*const value)
{
    strncpy (bctx->login_password, value, sizeof(bctx->login_password) - 1);
    return 0;
}
static int login_req_type_parser (batch_context*const bctx, char*const value)
{
    if (!strcmp (value, REQ_GET_POST))
        bctx->login_req_type = LOGIN_REQ_TYPE_GET_AND_POST;
    else if (!strcmp (value, REQ_POST))
        bctx->login_req_type = LOGIN_REQ_TYPE_POST;
    else
    {
        fprintf (stderr, 
                 "%s - error: LOGIN_REQ_TYPE (%s) is not valid. Use %s or %s .\n", 
                 __func__, value, REQ_GET_POST, REQ_POST);
        return -1;
    }
    return 0;
}
static int login_req_type_get_post_parser (batch_context*const bctx, char*const value)
{
  (void) value;
  bctx->login_req_type = LOGIN_REQ_TYPE_GET_AND_POST;
  return 0;
}
static int login_req_type_post_parser (batch_context*const bctx, char*const value)
{
  (void) value;
  bctx->login_req_type = LOGIN_REQ_TYPE_POST;
  return 0;
}
static int login_post_str_parser (batch_context*const bctx, char*const value)
{
    // TODO: validate the input. Important !!!.
  if (strcmp (value, NON_APPLICABLE_STR) || strcmp (value, "N/A"))
    {
      strncpy (bctx->login_post_str, value, sizeof (bctx->login_post_str) - 1);
    }
    return 0;
}
static int login_url_parser (batch_context*const bctx, char*const value)
{
    size_t url_length = 0;

    if ((url_length = strlen (value)) <= 0)
    {
        fprintf(stderr, "%s - warning: empty url for \"%s\"\n",  __func__,value);
        return 0;
    }

    if (! (bctx->login_url.url_str =(char *)calloc (url_length+1,sizeof (char))))
    {
        fprintf (stderr, 
                 "%s - error: login_url allocation failed for url \"%s\"\n",
                 __func__, value);
        return -1;
    }
      
    strcpy (bctx->login_url.url_str, value);
    bctx->login_url.url_lstep = URL_LOAD_LOGIN;
    bctx->login_url.url_appl_type = url_schema_classification (value);
    bctx->login_url.url_uas_num = -1; // means N/A
    return 0;
}
static int login_url_max_time_parser (batch_context*const bctx, char*const value)
{
    bctx->login_url.url_completion_time = atof (value);
    return 0;
}
static int login_url_interleave_time_parser (batch_context*const bctx, char*const value)
{
    bctx->login_url.url_interleave_time = atol (value);
    return 0;
}
static int login_cycling_parser (batch_context*const bctx, char*const value)
{
    bctx->login_cycling = (*value == 'Y' || *value == 'y') ? 1 : 0;
    return 0;
}

static int uas_parser (batch_context*const bctx, char*const value)
{
    bctx->do_uas = (*value == 'Y' || *value == 'y') ? 1 : 0;
    return 0;
}
static int uas_urls_num_parser (batch_context*const bctx, char*const value)
{
    bctx->uas_urls_num = atoi (value);
    
    if (bctx->uas_urls_num < 1)
    {
        fprintf (stderr, 
                 "%s - error: urls_num (%s) should be one or more.\n",
                 __func__, value);
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

    return 0;
}
static int uas_url_parser (batch_context*const bctx, char*const value)
{
    size_t url_length = 0;

    if ((int)bctx->url_index >= bctx->uas_urls_num)
    {
        fprintf (stderr, 
                 "%s - error: UAS_URL_NUM (%d) is below uas-url triplets number in conf-file.\n",
                 __func__, bctx->url_index);
        return -1;
    }
    
    if ((url_length = strlen (value)) <= 0)
    {
        fprintf(stderr, "%s - warning: empty UAS URL\"%s\"\n", 
                __func__, value);
        return 0;
    }
    
    if (! (bctx->uas_url_ctx_array[bctx->url_index].url_str = 
           (char *) calloc (url_length +1, sizeof (char))))
    {
        fprintf (stderr, 
                 "%s - error: allocation failed for url string \"%s\"\n", 
                 __func__, value);
        return -1;
    }
    strcpy(bctx->uas_url_ctx_array[bctx->url_index].url_str, value);
    bctx->uas_url_ctx_array[bctx->url_index].url_lstep = URL_LOAD_UAS;
    bctx->uas_url_ctx_array[bctx->url_index].url_appl_type = url_schema_classification (value);
    bctx->uas_url_ctx_array[bctx->url_index].url_uas_num = bctx->url_index;
    
    return 0;
}
static int uas_url_max_time_parser (batch_context*const bctx, char*const value)
{
    bctx->uas_url_ctx_array[bctx->url_index].url_completion_time = 
        atof (value);
    return 0;
}
static int uas_url_interleave_time_parser (batch_context*const bctx, char*const value)
{
    bctx->uas_url_ctx_array[bctx->url_index].url_interleave_time = 
        atol(value);
    bctx->url_index++; /* advance the position */
    return 0;
}

static int logoff_parser (batch_context*const bctx, char*const value)
{
    bctx->do_logoff = (*value == 'Y' || *value == 'y') ? 1 : 0;
    return 0;
}
static int logoff_req_type_parser (batch_context*const bctx, char*const value)
{
    if (!strcmp (value, REQ_GET_POST)) 
        bctx->logoff_req_type = LOGOFF_REQ_TYPE_GET_AND_POST;
    else if (!strcmp (value, REQ_GET))
        bctx->logoff_req_type = LOGOFF_REQ_TYPE_GET;
    else if (!strcmp (value, REQ_POST))
        bctx->logoff_req_type = LOGOFF_REQ_TYPE_POST;
    else
    {
        fprintf (stderr, 
                 "%s - error: LOGOFF_REQ_TYPE (%s) is not valid.\n" 
                 "Consider %s, %s or %s.\n", 
                 __func__, value, REQ_GET, REQ_POST, REQ_GET_POST);
        return -1;
    }
    return 0;     
}
static int logoff_req_type_get_post_parser (batch_context*const bctx, char*const value)
{
  (void) value;
  bctx->logoff_req_type = LOGOFF_REQ_TYPE_GET_AND_POST;
  return 0;
}
static int logoff_req_type_post_parser (batch_context*const bctx, char*const value)
{
  (void) value;
  bctx->logoff_req_type = LOGOFF_REQ_TYPE_POST;
  return 0;
}
static int logoff_req_type_get_parser (batch_context*const bctx, char*const value)
{
  (void) value;
  bctx->logoff_req_type = LOGOFF_REQ_TYPE_GET;
  return 0;
}
static int logoff_post_str_parser (batch_context*const bctx, char*const value)
{
  if (strcmp (value, NON_APPLICABLE_STR) || strcmp (value, "N/A")) /* not an empty string "" */
    {
        strncpy (bctx->logoff_post_str, value, sizeof (bctx->logoff_post_str) - 1);
    }
    return 0;
}
static int logoff_url_parser (batch_context*const bctx, char*const value)
{
    size_t url_length;

    if ((url_length = strlen (value)) <= 0)
    {
        fprintf(stderr, "%s - warning: empty url for \"%s\"\n", __func__,value);
        return 0;
    }

    if (! (bctx->logoff_url.url_str =(char *) calloc (url_length+1,sizeof (char))))
    {
        fprintf (stderr, 
                 "%s - error: logoff_url allocation failed for url \"%s\"\n", 
                 __func__, value);
        return -1;
    }
    strcpy(bctx->logoff_url.url_str, value);
    bctx->logoff_url.url_lstep = URL_LOAD_LOGOFF;
    bctx->logoff_url.url_appl_type = url_schema_classification (value);
    bctx->logoff_url.url_uas_num = -1; // means N/A
    
    return 0;
}
static int logoff_url_max_time_parser (batch_context*const bctx, char*const value)
{
    bctx->logoff_url.url_completion_time = atof (value);
    return 0;
}
static int logoff_url_interleave_time_parser (batch_context*const bctx, char*const value)
{
   bctx->logoff_url.url_interleave_time = atol (value);
   return 0;
}
static int logoff_cycling_parser (batch_context*const bctx, char*const value)
{
    bctx->logoff_cycling = (*value == 'Y' || *value == 'y') ? 1 : 0;
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

static int validate_batch (batch_context*const bctx)
{
    if (validate_batch_general (bctx) == -1)
    {
        fprintf (stderr, "%s - error: failed to validate batch section GENERAL.\n", 
                 __func__);
        return -1;
    }

    if (validate_batch_login (bctx) == -1)
    {
        fprintf (stderr, "%s - error: failed to validate batch section LOGIN.\n", 
                 __func__);
        return -1;
    }

    if (validate_batch_uas (bctx) == -1)
    {
        fprintf (stderr, "%s - error: failed to validate batch section UAS.\n", 
                 __func__);
        return -1;
    }

    if (validate_batch_logoff (bctx) == -1)
    {
        fprintf (stderr, "%s - error: failed to validate batch section LOGOFF.\n", 
                 __func__);
        return -1;
    }
    return 0;
}

static int validate_batch_general (batch_context*const bctx)
{
    if (!strlen (bctx->batch_name))
    {
        fprintf (stderr, "%s - error: BATCH_NAME is empty.\n", __func__);
        return -1;
    }
    if (bctx->client_num < 1)
    {
        fprintf (stderr, "%s - error: CLIENT_NUM is less than 1.\n", __func__);
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
    if (bctx->cidr_netmask < 0 || bctx->cidr_netmask > 32)
    {
        fprintf (stderr, "%s - error: NETMASK out of the valid range.\n", __func__);
        return -1;
    }
    if ((bctx->ip_addr_max - bctx->ip_addr_min + 1) < bctx->client_num)
    {
        fprintf (stderr, "%s - error: range of ip-addresses "
                 "is less than number of clients.\n"
                 "Increase IP_ADDR_MAX.\n",
                 __func__);
        return -1;
    }
    if (bctx->cycles_num < 0)
    {
        fprintf (stderr, "%s - error: CYCLES_NUM is negative.\n",__func__);
        return -1;
    }

  
    return 0;
}

static int validate_batch_login (batch_context*const bctx)
{
  if (! bctx->do_login)
    {
      if (strlen (bctx->login_username) || strlen (bctx->login_password) ||
          bctx->login_req_type || strlen (bctx->login_post_str) || 
          bctx->login_url.url_str)
        {
          fprintf (stderr, "%s - error: when login section is disabled by \"LOGIN=N\", \n"
                   "comment out all tags of the section after the tag LOGIN string.\n", 
                   __func__);
          return -1;
        }
      return 0;
    }

  if (!strlen (bctx->login_username))
    {
      fprintf (stderr, "%s - error: empty LOGIN_USERNAME .\n", 
               __func__);
      return -1;
    }

  // Let empty passwords in

  if (bctx->login_req_type != LOGIN_REQ_TYPE_POST &&
      bctx->login_req_type != LOGIN_REQ_TYPE_GET_AND_POST)
    {
      fprintf (stderr, "%s - error: LOGIN_REQ_TYPE is out of valid range .\n", 
               __func__);
      return -1;
    }
    
  if (!bctx->login_url.url_str || !strlen (bctx->login_url.url_str))
    {
      fprintf (stderr, "%s - error: empty LOGIN_URL is not useful for your login .\n", 
               __func__);
      return -1;
    }

  if (bctx->login_url.url_completion_time < 0.0)
    {
      fprintf (stderr, 
               "%s - error: LOGIN_URL_MAX_TIME should not be negative.\n", 
               __func__);
      return -1;
    }

  return 0;
}

static int validate_batch_uas (batch_context*const bctx)
{
  if (! bctx->do_uas)
    {
      if (bctx->uas_urls_num)
        {
          fprintf (stderr, "%s - error: when UAS section is disabled by \"UAS=N\", \n"
                   "comment out all tags of the section after the tag UAS string.\n", 
                   __func__);
          return -1;
        }
      return 0;
    }

  if (bctx->uas_urls_num < 1)
    {
      fprintf (stderr, "%s - error: at least a single url is expected "
               "for a valid UAS section .\n", __func__);
      return -1;
    }

  int k = 0;
  for (k = 0; k < bctx->uas_urls_num; k++)
    {
      if (!bctx->uas_url_ctx_array[k].url_str || !strlen (bctx->uas_url_ctx_array[k].url_str))
        {
          fprintf (stderr, 
                   "%s - error: empty UAS_URL in position %d.\n"
                   "Check, that number of UAS_URL triplets is equal to UAS_URLS_NUM.\n", 
                   __func__, k);
          return -1;
        }

      if (bctx->uas_url_ctx_array[k].url_completion_time < 0.0)
        {
          fprintf (stderr, 
                   "%s - error: UAS_URL_MAX_TIME should not be negative.\n", 
                   __func__);
          return -1;
        }			

    }
  return 0;
}

static int validate_batch_logoff (batch_context*const bctx)
{
    if (! bctx->do_logoff)
    {
        if (bctx->logoff_req_type || strlen (bctx->logoff_post_str) || 
            bctx->logoff_url.url_str)
        {
            fprintf (stderr, "%s - error: when login section is disabled by \"LOGOFF=N\", \n"
                     "comment out all tags of the section after the tag LOGOFF string.\n", 
                 __func__);
            return -1;
        }
        return 0;
    }

    if (bctx->login_req_type != LOGOFF_REQ_TYPE_GET_AND_POST &&
        bctx->login_req_type != LOGOFF_REQ_TYPE_GET &&
        bctx->login_req_type != LOGOFF_REQ_TYPE_POST)
    {
        fprintf (stderr, "%s - error: LOGOFF_REQ_TYPE is out of valid range .\n", 
                 __func__);
        return -1;
    }
    
    if (!bctx->logoff_url.url_str || !strlen (bctx->logoff_url.url_str))
    {
        fprintf (stderr, "%s - error: empty LOGOFF_URL is not useful for your logoff .\n", 
                 __func__);
        return -1;
    }

    if (bctx->logoff_url.url_completion_time < 0.0)
    {
        fprintf (stderr, 
                 "%s - error: LOGOFF_URL_MAX_TIME should not be negative.\n", 
                 __func__);
        return -1;
    }

   if (bctx->logoff_url.url_interleave_time > 0 && 
			bctx->logoff_url.url_interleave_time < 10)
    {
        fprintf (stderr, 
                 "%s - error: LOGOFF_URL_INTERLEAVE_TIME should be either 0 or above 10 msec.\n"
			  "ATTENTION !!! The value starting from version 0.24 is not in seconds, but in msec.\n"
			 "Please, correct the value from seconds to milliseconds.\n\n", 
                 __func__);
        return -1;
    }

    return 0;
}

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
        fprintf (stderr, "%s - processing file string \"%s\n", 
                 __func__, fgets_buff);

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
                fprintf (stderr, "%s - skipping commented file string \"%s\n", 
                         __func__, fgets_buff);
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
        if (validate_batch (&bctx_array[k]) == -1)
        {
            fprintf (stderr, 
                     "%s - error: validation of batch %d failed.\n",__func__, k);
            return -1;
        }

        /* Init operational statistics structures */
        if (op_stat_point_init(&bctx_array[k].op_delta, 
                               (size_t)bctx_array[k].do_login, 
                               bctx_array[k].uas_urls_num, 
                               (size_t)bctx_array[k].do_logoff) == -1)
          {
            fprintf (stderr, "%s - error: init of op_delta failed for batch %d.\n",__func__, k);
            return -1;
          }

        if (op_stat_point_init(&bctx_array[k].op_total, 
                               (size_t)bctx_array[k].do_login, 
                               bctx_array[k].uas_urls_num, 
                               (size_t)bctx_array[k].do_logoff) == -1)
          {
            fprintf (stderr, "%s - error: init of op_total failed for batch %d.\n",__func__, k);
            return -1;
          }
    }

    return (batch_index + 1);
}

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
    tmp++;

  return (32 - tmp); 
 }
