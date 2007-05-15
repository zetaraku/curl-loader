/* 
 *     loader.c
 *
 * 2006-2007 Copyright (c) 
 * Robert Iakobashvili, <coroberti@gmail.com>
 * Michael Moser, <moser.michael@gmail.com>
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
 *
 * Cooked from the CURL-project examples with thanks to the 
 * great CURL-project authors and contributors.
 */

// must be the first include
#include "fdsetsize.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <curl/curl.h>
#include <curl/multi.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

#include "batch.h"
#include "client.h"
#include "loader.h"
#include "conf.h"
#include "ssl_thr_lock.h"
#include "screen.h"
#include "cl_alloc.h"


static int create_ip_addrs (batch_context* bctx, int bctx_num);
int client_tracing_function (CURL *handle, 
                             curl_infotype type, 
                             unsigned char *data, 
                             size_t size, 
                             void *userp);

size_t do_nothing_write_func (void *ptr, 
                              size_t size, 
                              size_t nmemb, 
                              void *stream);

static void* batch_function (void *batch_data);
static int initial_handles_init (struct client_context*const cdata);
static int setup_curl_handle_appl (struct client_context*const cctx,  url_context* url_ctx);
static int init_client_post_buffer (client_context* cctx, url_context* url);
static int alloc_init_client_contexts (batch_context* bctx, FILE* output_file);
static void free_batch_data_allocations (struct batch_context* bctx);
static int ipv6_increment(const struct in6_addr *const src, 
                          struct in6_addr *const dest);

int stop_loading = 0;


static void sigint_handler (int signum)
{
  (void) signum;

  stop_loading = 1;

  screen_release ();

  close (STDIN_FILENO);

  fprintf (stderr, "\n\n======= SIGINT Received ============.\n");
}

typedef int (*pf_user_activity) (struct client_context*const);

/*
 * Batch functions for the 3 modes.
*/
static pf_user_activity ua_array[3] = 
{ 
  user_activity_hyper,
  user_activity_smooth
};


int 
main (int argc, char *argv [])
{
  batch_context bc_arr[BATCHES_MAX_NUM];
  pthread_t tid[BATCHES_MAX_NUM];
  int batches_num = 0; 
  int i = 0, error = 0;


  signal (SIGPIPE, SIG_IGN);

  if (parse_command_line (argc, argv) == -1)
    {
      fprintf (stderr, 
               "%s - error: failed parsing of the command line.\n", __func__);
      return -1;
    }

  if (geteuid())
    {
      fprintf (stderr, 
               "%s - error: lacking root preveledges to run this program.\n", __func__);
      return -1;
    }
  
   memset(bc_arr, 0, sizeof(bc_arr));

  /* 
     Parse the configuration file. 
  */
  if ((batches_num = parse_config_file (config_file, bc_arr, 
                                        sizeof(bc_arr)/sizeof(*bc_arr))) <= 0)
    {
      fprintf (stderr, "%s - error: parse_config_file () failed.\n", __func__);
      return -1;
    }

   /*
    * De-facto in full we are supporting only a single batch.
    * TODO: test env for all batches.
    */
   if (test_environment (&bc_arr[0]) == -1)
   {
       fprintf (stderr, "%s - error: test_environment () - error.\n", __func__);
      return -1;
   }
   
  /* 
     Add ip-addresses to the loading network interfaces
     and keep them in batch-contexts. 
  */
  if (create_ip_addrs (bc_arr, batches_num) == -1)
    {
      fprintf (stderr, "%s - error: create_ip_addrs () failed. \n", __func__);
      return -1;
    }

  fprintf (stderr, "%s - accomplished setting IP-addresses to the loading interfaces.\n", 
           __func__);

  signal (SIGINT, sigint_handler);

  screen_init ();
  
  if (! threads_run)
    {
      fprintf (stderr, "\nRUNNING WITHOUT THREADS\n\n");
      sleep (1) ;
      batch_function (&bc_arr[0]);
	 fprintf (stderr, "Exited batch_function\n");
    }
  else
    {
      fprintf (stderr, "\n%s - STARTING THREADS\n\n", __func__);
      sleep (1);
      
      /* Init openssl mutexes and pass two callbacks to openssl. */
      if (thread_openssl_setup () == -1)
        {
          fprintf (stderr, "%s - error: thread_setup () - failed.\n", __func__);
          return -1;
        }
      
      /* Opening threads for the batches of clients */
      for (i = 0 ; i < batches_num ; i++) 
        {
          error = pthread_create (&tid[i], NULL, batch_function, &bc_arr[i]);

          if (0 != error)
            fprintf(stderr, "%s - error: Couldn't run thread number %d, errno %d\n", 
                    __func__, i, error);
          else 
            fprintf(stderr, "%s - note: Thread %d, started normally\n", __func__, i);
        }

      /* Waiting for all the threads to terminate */
      for (i = 0 ; i < batches_num ; i++) 
        {
          error = pthread_join (tid[i], NULL) ;
          fprintf(stderr, "%s - note: Thread %d terminated normally\n", __func__, i) ;
        }

      thread_openssl_cleanup ();
    }
   
  return 0;
}

/****************************************************************************************
* Function name - batch_function
* Description -   Runs the batch test either within the main-thread or in a separate thread.
*
* Input -         *batch_data - contains loading configuration and active entities for a 
*                               particular batch of clients.
* Return Code/Output - NULL in all cases
****************************************************************************************/
static void* batch_function (void * batch_data)
{
  batch_context* bctx = (batch_context *) batch_data;
  FILE* log_file = 0;
  FILE* statistics_file = 0;
  
  int  rval = -1;

  if (!bctx)
    {
      fprintf (stderr, 
               "%s - error: batch_data input is zero.\n", __func__);
      return NULL;
    }
 

  if (! stderr_print_client_msg)
    {
      /*
        Init batch logfile for the batch client output 
      */
      sprintf (bctx-> batch_logfile, "./%s.log", bctx->batch_name);

      if (!(log_file = fopen(bctx-> batch_logfile, "w")))
        {
          fprintf (stderr, 
                   "%s - \"%s\" - failed to open file \"%s\" with errno %d.\n", 
                   __func__, bctx->batch_name, bctx-> batch_logfile, errno);
          return NULL;
        }
    }

  /*
    Init batch statistics file
  */
  sprintf (bctx->batch_statistics, "./%s.txt", bctx->batch_name);
      
  if (!(statistics_file = fopen(bctx->batch_statistics, "w")))
  {
      fprintf (stderr, 
               "%s - \"%s\" - failed to open file \"%s\" with errno %d.\n", 
               __func__, bctx->batch_name, bctx->batch_statistics, errno);
      return NULL;
  }
  else
  {
      bctx->statistics_file = statistics_file;
      print_statistics_header (statistics_file);
  }
  
  /* 
     Allocate and init objects, containing client-context information.
  */
  if (alloc_init_client_contexts (bctx, log_file) == -1)
    {
      fprintf (stderr, "%s - \"%s\" - failed to allocate or init client_contexts.\n", 
               __func__, bctx->batch_name);
      goto cleanup;
    }
 
  /* 
     Init libcurl MCURL and CURL handles. Setup of the handles is delayed to
     the later step, depending on the flavors of url required.
  */
  if (initial_handles_init (bctx->cctx_array) == -1)
    {
      fprintf (stderr, "%s - \"%s\" initial_handles_init () failed.\n", 
               __func__, bctx->batch_name);
      goto cleanup;
    }

  /* 
     Now run configuration-defined actions, like login, fetching various urls and and 
     sleeping in between, loggoff
     Calls user activity loading function corresponding to the loading mode
     used (user_activity_smooth () or user_activity_hyper ()).
  */ 
  rval = ua_array[loading_mode] (bctx->cctx_array);

  if (rval == -1)
    {
      fprintf (stderr, "%s - \"%s\" -user activity failed.\n", 
               __func__, bctx->batch_name);
      goto cleanup;
    }

 cleanup:
  if (bctx->multiple_handle)
    curl_multi_cleanup(bctx->multiple_handle);

  if (log_file)
      fclose (log_file);
  if (statistics_file)
      fclose (statistics_file);

  free_batch_data_allocations (bctx);

  return NULL;
}

/****************************************************************************************
* Function name - initial_handles_init
*
* Description - Libcurl initialization of curl multi-handle and the curl handles (clients), 
*               used in the batch
*
* Input -       *ctx_array - array of clients for a particular batch of clients
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int initial_handles_init (client_context*const ctx_array)
{
  batch_context* bctx = ctx_array->bctx;
  int k = 0;

  /* Init CURL multi-handle. */
  if (! (bctx->multiple_handle = curl_multi_init()) )
    {
      fprintf (stderr, 
               "%s - error: curl_multi_init() failed for batch \"%s\" .\n", 
               __func__, bctx->batch_name) ;
      return -1;
    }

  /* Initialize all CURL handles */
  for (k = 0 ; k < bctx->client_num_max ; k++)
    {
      if (!(bctx->cctx_array[k].handle = curl_easy_init ()))
        {
          fprintf (stderr,"%s - error: curl_easy_init () failed for k=%d.\n",
                   __func__, k);
          return -1;
        }
    }
        
  return 0;
}

/****************************************************************************************
* Function name - setup_curl_handle
*
* Description - Setup for a single curl handle (client): removes a handle from multi-handle, 
*               inits it, using setup_curl_handle_init () function, and adds the
*               handle back to the multi-handle.
*
* Input -       *cctx        - pointer to client context, containing CURL handle pointer;
*               *url     - pointer to url-context, containing all url-related information;
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int setup_curl_handle (client_context*const cctx, url_context* url)
{
  if (!cctx || !url)
    {
      return -1;
    }
  
  if (setup_curl_handle_init (cctx, url) == -1)
  {
      fprintf (stderr,"%s - error: failed.\n",__func__);
      return -1;
  }

  return 0;
}

/****************************************************************************
* Function name - setup_curl_handle_init
*
* Description - Resets client context kept CURL handle and inits it locally, using 
*               setup_curl_handle_appl () function for the application-specific 
*               (HTTP/FTP) initialization.
*
* Input -       *cctx        - pointer to client context, containing CURL handle pointer;
*               *url_ctx     - pointer to url-context, containing all url-related information;
* Return Code/Output - On Success - 0, on Error -1
******************************************************************************/
int setup_curl_handle_init (client_context*const cctx, url_context* url_ctx)
{
  batch_context* bctx = cctx->bctx;
  CURL* handle = cctx->handle;

  if (!cctx || !url_ctx)
    {
      return -1;
    }

  curl_easy_reset (handle);

  if (bctx->ipv6)
    curl_easy_setopt (handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
      
  /* Bind the handle to a certain IP-address */
  curl_easy_setopt (handle, CURLOPT_INTERFACE, 
                    bctx->ip_addr_array [cctx->client_index]);

  curl_easy_setopt (handle, CURLOPT_NOSIGNAL, 1);
    
  /* Set the url */
  if (url_ctx->url_str && url_ctx->url_str[0])
    {
      /* 
         Note, target URL for PUT should include a file
         name, not only a directory 
      */
      curl_easy_setopt (handle, CURLOPT_URL, url_ctx->url_str);
    }
  else
    {
      fprintf (stderr,"%s - error: empty url provided.\n",
                   __func__);
      exit (-1);
    }
  
  /* Set the index to client for the smooth and hyper modes */
  if (url_ctx->url_ind >= 0)
  {
    cctx->url_curr_index = url_ctx->url_ind;
  }
  
  bctx->url_index = url_ctx->url_ind;

  curl_easy_setopt (handle, CURLOPT_DNS_CACHE_TIMEOUT, -1);

  /* Set the connection timeout */
  curl_easy_setopt (handle, 
                    CURLOPT_CONNECTTIMEOUT, 
                    url_ctx->connect_timeout ? url_ctx->connect_timeout : connect_timeout);

  /* Define the connection re-use policy. When passed 1, re-establish */
  curl_easy_setopt (handle, CURLOPT_FRESH_CONNECT, url_ctx->fresh_connect);

  /* 
     If DNS resolving is necesary, global DNS cache is enough,
     otherwise compile libcurl with ares (cares) library support.
     Attention: DNS global cache is not thread-safe, therefore use
     cares for asynchronous DNS lookups.

     curl_easy_setopt (handle, CURLOPT_DNS_USE_GLOBAL_CACHE, 1); 
  */
  
  curl_easy_setopt (handle, CURLOPT_VERBOSE, 1);
  curl_easy_setopt (handle, CURLOPT_DEBUGFUNCTION, 
                    client_tracing_function);

  /* 
     This is to return cctx pointer as the void* userp to the 
     tracing function. 
  */
  curl_easy_setopt (handle, CURLOPT_DEBUGDATA, cctx);

#if 0
  curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, prog_cb);
  curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, cctx);
#endif

  if (!output_to_stdout)
    {
      curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION,
                        do_nothing_write_func);
    }

  curl_easy_setopt (handle, CURLOPT_SSL_VERIFYPEER, 0);
  curl_easy_setopt (handle, CURLOPT_SSL_VERIFYHOST, 0);
    
  /* Set the private pointer to be used by the smooth-mode. */
  curl_easy_setopt (handle, CURLOPT_PRIVATE, cctx);

  if (loading_mode == LOAD_MODE_HYPER)
    {
      curl_easy_setopt (handle, CURLOPT_WRITEDATA, cctx);
    }

  /* Without the buffer set, we do not get any errors in tracing function. */
  curl_easy_setopt (handle, CURLOPT_ERRORBUFFER, bctx->error_buffer); 

  if (url_ctx->upload_file)
    {
      if (! url_ctx->upload_file_ptr)
        {
          if (! (url_ctx->upload_file_ptr = fopen (url_ctx->upload_file, "rb")))
            {
              fprintf (stderr, 
                       "%s - error: failed to open() %s with errno %d.\n", 
                       __func__, url_ctx->upload_file, errno);
              return -1;
            }
        }
      
      /* Enable uploading */
      
      curl_easy_setopt(handle, CURLOPT_UPLOAD, 1);
      
      /* 
         Do we want to use our own read function ? On windows - MUST.
         curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
      */
      
      /* Now specify which file to upload */
      curl_easy_setopt(handle, CURLOPT_READDATA, 
                       url_ctx->upload_file_ptr);
      
      /* Provide the size of the upload */
      curl_easy_setopt(handle, CURLOPT_INFILESIZE, 
                       (long) url_ctx->upload_file_size);
    }

  /* 
     Application (url) specific setups, like HTTP-specific, FTP-specific, etc. 
  */
  if (setup_curl_handle_appl (cctx, url_ctx) == -1)
    {
      fprintf (stderr,
               "%s - error: setup_curl_handle_appl () failed .\n",
               __func__);
      return -1;
    }

  return 0;
}
  
/****************************************************************************************
* Function name - setup_curl_handle_appl
*
* Description - Application/url-type specific setup for a single curl handle (client)
*
* Input -       *cctx       - pointer to client context, containing CURL handle pointer;
*               *url_ctx    - pointer to url-context, containing all url-related information;
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int setup_curl_handle_appl (client_context*const cctx, url_context* url_ctx)
{
    batch_context* bctx = cctx->bctx;
    CURL* handle = cctx->handle;
    
    cctx->is_https = (url_ctx->url_appl_type == URL_APPL_HTTPS);
    
    if (url_ctx->url_appl_type == URL_APPL_HTTPS ||
        url_ctx->url_appl_type == URL_APPL_HTTP)
    {
        
        /* HTTP-specific initialization */
        
      /* 
         Follow possible HTTP-redirection from header Location of the 
         3xx HTTP responses, like 301, 302, 307, etc. It also updates the url, 
         thus no need to parse header Location. Great job done by the libcurl 
         people.
      */
      curl_easy_setopt (handle, CURLOPT_FOLLOWLOCATION, 1);
      curl_easy_setopt (handle, CURLOPT_UNRESTRICTED_AUTH, 1);
      
      /* Enable infinitive (-1) redirection number. */
      curl_easy_setopt (handle, CURLOPT_MAXREDIRS, -1);

      /* 
         Setup the User-Agent header, configured by user. The default is MSIE-6 header.
       */
      curl_easy_setopt (handle, CURLOPT_USERAGENT, bctx->user_agent);

      /*
        Setup the custom (HTTP) headers, if appropriate.
      */
      if (url_ctx->custom_http_hdrs && url_ctx->custom_http_hdrs_num)
        {
          curl_easy_setopt (handle, CURLOPT_HTTPHEADER, 
                            url_ctx->custom_http_hdrs);
        }
      
      /* Enable cookies. This is important for various authentication schemes. */
      curl_easy_setopt (handle, CURLOPT_COOKIEFILE, "");
      
      /* Make POST, using post buffer, if requested. */
      if (url_ctx->req_type == HTTP_REQ_TYPE_POST)
        {
          if (!cctx->post_data)
            {            
              fprintf (stderr, "%s - error: post_data is NULL.\n", __func__);
              return -1;
            }
          else
            {
              if (set_client_url_post_data (cctx, url_ctx) == -1)
                {
                  fprintf (stderr,
                           "%s - error: set_client_url_post_data() failed.\n",
                           __func__);
                  return -1;
                }
            }
        }
      else if (url_ctx->req_type == HTTP_REQ_TYPE_PUT)
        {
          if (!url_ctx->upload_file || ! url_ctx->upload_file_ptr)
            {            
              fprintf (stderr, 
                       "%s - error: upload file is NULL or cannot be opened.\n", 
                       __func__);
              return -1;
            }
          else
            {
              // Upload is enabled earlier.

              /* 
                 HTTP PUT method.
                 Note, target URL for PUT should include a file
                 name, not only a directory 
              */
              curl_easy_setopt(handle, CURLOPT_PUT, 1);
            }
        }

      if (url_ctx->web_auth_method)
        {
          if (!url_ctx->web_auth_credentials)
            {
              if (!url_ctx->username || !url_ctx->password)
                {
                  return -1;
                }

              char web_userpwd[256];
              sprintf (web_userpwd, "%s:%s", url_ctx->username, url_ctx->password);
              curl_easy_setopt(handle, CURLOPT_USERPWD, web_userpwd);
            }
          else
            {
              curl_easy_setopt(handle, CURLOPT_USERPWD, url_ctx->web_auth_credentials);
            }
          curl_easy_setopt(handle, CURLOPT_HTTPAUTH, url_ctx->web_auth_method);
        }

      if (url_ctx->proxy_auth_method)
        {
          if (!url_ctx->proxy_auth_credentials)
            {
              if (!url_ctx->username || !url_ctx->password)
                {
                  return -1;
                }

              char proxy_userpwd[256];
              sprintf (proxy_userpwd, "%s:%s", url_ctx->username, url_ctx->password);
              curl_easy_setopt(handle, CURLOPT_USERPWD, proxy_userpwd);
            }
          else
            {
              curl_easy_setopt(handle, CURLOPT_USERPWD, url_ctx->proxy_auth_credentials);
            }
          curl_easy_setopt(handle, CURLOPT_HTTPAUTH, url_ctx->proxy_auth_method);
        }

    }
    else if (url_ctx->url_appl_type == URL_APPL_FTP ||
             url_ctx->url_appl_type == URL_APPL_FTPS)
    {
      /***********  FTP-specific setup. *****************/

      if (url_ctx->ftp_active)
        {
          curl_easy_setopt(handle, 
                           CURLOPT_FTPPORT, 
                           bctx->ip_addr_array [cctx->client_index]);
        }

      /*
        Send custom FTP headers after the transfer.
      */
      if (url_ctx->custom_http_hdrs && url_ctx->custom_http_hdrs_num)
        {
          curl_easy_setopt (handle, CURLOPT_POSTQUOTE, 
                            url_ctx->custom_http_hdrs);
        }

      
    }

  return 0;
}

/**********************************************************************
* Function name - set_client_url_post_data
*
* Description - Initialize client post form buffer to be used for a url POST-ing
* 
* Input -       *cctx - pointer to client context
*                *url - pointer to url context
* Return Code/Output - On Success - 0, on Error -1
***********************************************************************/
int set_client_url_post_data (client_context* cctx, url_context* url)
{
  if (init_client_post_buffer (cctx, url) == -1)
    {
      fprintf (stderr,
               "%s - error: init_client_post_buffers() failed.\n",
               __func__);
      return -1;
    }

  curl_easy_setopt (cctx->handle, CURLOPT_POSTFIELDS, cctx->post_data);

  return 0;
}

/***********************************************************************
* Function name - init_client_post_buffers
*
* Description - Initialize post form buffers to be used for POST-ing
* 
* Input -       *cctx - pointer to client context
*                *url - pointer to url context
* Return Code/Output - On Success - 0, on Error -1
*************************************************************************/
static int init_client_post_buffer (client_context* cctx, url_context* url)
{
  int i;

  if (!url->form_str || !url->form_str[0])
    {
 
      fprintf (stderr, "%s - error: FORM_STR not defined.\n",
               __func__);
      return -1;
    }

  switch (url->form_usage_type)
    {
    case FORM_USAGETYPE_UNIQUE_USERS_AND_PASSWORDS:
      /*
        For each client init post buffer, containing username and password 
        with uniqueness added via added to the base username and password
        client index.
      */
      snprintf (cctx->post_data,
                POST_DATA_BUF_SIZE,
                url->form_str,
                url->username,
                i + 1,
                url->password[0] ? url->password : "",
                i + 1);
      break;

    case FORM_USAGETYPE_UNIQUE_USERS_SAME_PASSWORD:
      /* 
         For each client init post buffer, containing username with uniqueness 
         added via added to the base username client index. Password is kept
         the same for all users.
      */
      snprintf (cctx->post_data,
                POST_DATA_BUF_SIZE,
                url->form_str,
                url->username,
                i + 1,
                url->password[0] ? url->password : "");
      break;

    case FORM_USAGETYPE_SINGLE_USER:
      /* All clients have the same login_username and password.*/
      snprintf (cctx->post_data,
                POST_DATA_BUF_SIZE,
                url->form_str,
                url->username,
                url->password[0] ? url->password : "");
      break;

    case FORM_USAGETYPE_RECORDS_FROM_FILE:
      {
        if (! url->form_records_array)
          {
            fprintf (stderr,
                     "\"%s\" error: url->form_records_array is NULL.\n", 
                     __func__);
            return -1;
          }

        const form_records_cdata*const fcd = 
          &url->form_records_array[cctx->client_index];
        
        snprintf (cctx->post_data, 
                  POST_DATA_BUF_SIZE, 
                  url->form_str,
                  fcd->form_tokens[0], 
                  fcd->form_tokens[1] ? fcd->form_tokens[1] : "");
      }
      break;

    default:
      {
        fprintf (stderr,
                 "\"%s\" error: none valid bctx->post_str_usertype.\n", 
                 __func__);
        return -1;
      }
    }
  
  return 0;
}


/****************************************************************************************
* Function name - client_tracing_function
* 
* Description - Used to log activities of each client to the <batch_name>.log file
*
* Input -       *handle - pointer to CURL handle;
*               type    - type of libcurl information passed, like headers, data, info, etc
*               *data   - pointer to data, like headers, etc
*               size    - number of bytes passed with <data> pointer
*               *userp  - pointer to user-specific data, which in our case is the 
*                         client_context structure
*
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int client_tracing_function (CURL *handle, 
                             curl_infotype type, 
                             unsigned char *data, 
                             size_t size, 
                             void *userp)
{
  client_context* cctx = (client_context*) userp;
  char*url_target = NULL, *url_effective = NULL;

#if 0
  char buf[300];
  int n;

  n = snprintf(buf,sizeof(buf)-1,"->client_tracing_function cctx=%p size=%d | %s",cctx,size,data);
  buf[n] = '\0';
  printf("%s\n",buf);
#endif  

  if (url_logging)
    {

      url_target = cctx->bctx->url_ctx_array[cctx->url_curr_index].url_str;
      
      /* Clients are being redirected back and forth by 3xx redirects. */
      curl_easy_getinfo (handle, CURLINFO_EFFECTIVE_URL, &url_effective);
    }

  const char*const url = url_effective ? url_effective : url_target;
  const int url_print = (url_logging && url) ? 1 : 0;
  const int url_diff = (url_print && url_effective && url_target) ? 
    strcmp(url_effective, url_target) : 0;

  switch (type)
    {
    case CURLINFO_TEXT:
      if (verbose_logging)
          fprintf(cctx->file_output, "%ld %s :== Info: %s: eff-url: %s, url: %s\n",
                  cctx->cycle_num, cctx->client_name, data,
                  url_print ? url : "", url_diff ? url_target : "");
      break;

    case CURLINFO_ERROR:
      fprintf(cctx->file_output, "%ld %s !! ERROR: %s: eff-url: %s, url: %s\n", 
              cctx->cycle_num, cctx->client_name, data, 
              url_print ? url : "", url_diff ? url_target : "");

      cctx->client_state = CSTATE_ERROR;

      stat_err_inc (cctx);
      first_hdrs_clear_all (cctx);
      break;

    case CURLINFO_HEADER_OUT:
      if (verbose_logging)
          fprintf(cctx->file_output, "%ld %s => Send header: eff-url: %s, url: %s\n", 
                  cctx->cycle_num, cctx->client_name, url_print ? url : "", url_diff ? url_target : "");

      stat_data_out_add (cctx, (unsigned long) size);

      if (! first_hdr_req (cctx))
        {
          /* First header of the HTTP-request. */
          first_hdr_req_inc (cctx);
          stat_req_inc (cctx); /* Increment number of requests */
          cctx->req_sent_timestamp = get_tick_count ();
        }
      first_hdrs_clear_non_req (cctx);
      break;

    case CURLINFO_DATA_OUT:
      if (verbose_logging)
          fprintf(cctx->file_output, "%ld %s => Send data: eff-url: %s, url: %s\n", 
                  cctx->cycle_num, cctx->client_name, url_print ? url : "",
                  url_diff ? url_target : "");

      stat_data_out_add (cctx, (unsigned long) size);
      first_hdrs_clear_all (cctx);
      break;

    case CURLINFO_SSL_DATA_OUT:
      if (verbose_logging) 
          fprintf(cctx->file_output, "%ld %s => Send ssl data: eff-url: %s, url: %s\n", 
                  cctx->cycle_num, cctx->client_name, url_print ? url : "",
                  url_diff ? url_target : "");

      stat_data_out_add (cctx, (unsigned long) size);
      first_hdrs_clear_all (cctx);
      break;
      
    case CURLINFO_HEADER_IN:
      /* 
         CURL library assists us by passing here the full HTTP-header, 
         not just parts. 
      */
      stat_data_in_add (cctx, (unsigned long) size);

      {
        long response_status = 0, response_module = 0;
        
        if (verbose_logging)
          fprintf(cctx->file_output, "%ld %s <= Recv header: eff-url: %s, url: %s\n", 
                  cctx->cycle_num, cctx->client_name, url_print ? url : "",
                  url_diff ? url_target : "");
        
        curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &response_status);

        response_module = response_status / (long)100;
        
        switch (response_module)
          {

          case 1: /* 100-Continue and 101 responses */
            if (! first_hdr_1xx (cctx))
              {
                if (verbose_logging)
                  fprintf(cctx->file_output, "%ld %s:!! %ld CONTINUE: eff-url: %s, url: %s\n", 
                          cctx->cycle_num, cctx->client_name, response_status,
                          url_print ? url : "", url_diff ? url_target : "");

                /* First header of 1xx response */
                first_hdr_1xx_inc (cctx);
                stat_1xx_inc (cctx); /* Increment number of 1xx responses */
                const unsigned long time_1xx_resp = get_tick_count ();
                stat_appl_delay_add (cctx, time_1xx_resp);
              }
            
            first_hdrs_clear_non_1xx (cctx);
            break;


          case 2: /* 200 OK */
            if (! first_hdr_2xx (cctx))
              {
                if (verbose_logging)
                    fprintf(cctx->file_output, "%ld %s:!! %ld OK: eff-url: %s, url: %s\n",
                            cctx->cycle_num, cctx->client_name, response_status,
                            url_print ? url : "", url_diff ? url_target : "");

                /* First header of 2xx response */
                first_hdr_2xx_inc (cctx);
                stat_2xx_inc (cctx); /* Increment number of 2xx responses */

                /* Count into the averages HTTP/S server response delay */
                const unsigned long time_2xx_resp = get_tick_count ();
                stat_appl_delay_2xx_add (cctx, time_2xx_resp);
                stat_appl_delay_add (cctx, time_2xx_resp);
              }
            first_hdrs_clear_non_2xx (cctx);
            break;
       
          case 3: /* 3xx REDIRECTIONS */
            if (! first_hdr_3xx (cctx))
              {
                fprintf(cctx->file_output, "%ld %s:!! %ld REDIRECTION: %s: eff-url: %s, url: %s\n", 
                    cctx->cycle_num, cctx->client_name, response_status, data,
                    url_print ? url : "", url_diff ? url_target : "");

                /* First header of 3xx response */
                first_hdr_3xx_inc (cctx);
                stat_3xx_inc (cctx); /* Increment number of 3xx responses */
                const unsigned long time_3xx_resp = get_tick_count ();
                stat_appl_delay_add (cctx, time_3xx_resp);
              }
            first_hdrs_clear_non_3xx (cctx);
            break;

          case 4: /* 4xx Client Error */
              if (! first_hdr_4xx (cctx))
              {
                fprintf(cctx->file_output, "%ld %s :!! %ld CLIENT_ERROR : %s: eff-url: %s, url: %s\n", 
                      cctx->cycle_num, cctx->client_name, response_status, data,
                      url_print ? url : "", url_diff ? url_target : "");

                /*
                  We are not marking client on 401 and 407 as coming to the
                  error state, because the responses are just authentication 
                  challenges, that virtual client may overcome.
                */
                if (response_status != 401 || response_status != 407)
                  {
                    cctx->client_state = CSTATE_ERROR;
                  }

                /* First header of 4xx response */
                first_hdr_4xx_inc (cctx);
                stat_4xx_inc (cctx);  /* Increment number of 4xx responses */

                const unsigned long time_4xx_resp = get_tick_count ();
                stat_appl_delay_add (cctx, time_4xx_resp);
              }
             first_hdrs_clear_non_4xx (cctx);
             break;


          case 5: /* 5xx Server Error */
            if (! first_hdr_5xx (cctx))
              {
                fprintf(cctx->file_output, "%ld %s :!! %ld SERVER_ERROR : %s: eff-url: %s, url: %s\n", 
                    cctx->cycle_num, cctx->client_name, response_status, data,
                    url_print ? url : "", url_diff ? url_target : "");

                cctx->client_state = CSTATE_ERROR;

                /* First header of 5xx response */
                first_hdr_5xx_inc (cctx);
                stat_5xx_inc (cctx);  /* Increment number of 5xx responses */

                const unsigned long time_5xx_resp = get_tick_count ();
                stat_appl_delay_add (cctx, time_5xx_resp);
              }
            first_hdrs_clear_non_5xx (cctx);
            break;

          default :
            fprintf(cctx->file_output, 
                    "%ld %s:<= WARNING: parsing error: wrong response code (FTP?) %ld .\n", 
                    cctx->cycle_num, cctx->client_name, response_status /*(char*) data*/);
            /* FTP breaks it: - cctx->client_state = CSTATE_ERROR; */
            break;
          }
      }
      break;

    case CURLINFO_DATA_IN:
      if (verbose_logging) 
          fprintf(cctx->file_output, "%ld %s <= Recv data: eff-url: %s, url: %s\n", 
                  cctx->cycle_num, cctx->client_name, 
                  url_print ? url : "", url_diff ? url_target : "");

      stat_data_in_add (cctx,  (unsigned long) size);
      first_hdrs_clear_all (cctx);
      break;

    case CURLINFO_SSL_DATA_IN:
      if (verbose_logging) 
          fprintf(cctx->file_output, "%ld %s <= Recv ssl data: eff-url: %s, url: %s\n", 
                  cctx->cycle_num, cctx->client_name, 
                  url_print && url ? url : "", url_diff ? url_target : "");

      stat_data_in_add (cctx,  (unsigned long) size);
      first_hdrs_clear_all (cctx);
      break;

    default:
      fprintf (stderr, "default OUT - \n");
    }

  // fflush (cctx->file_output); // Don't do it
  return 0;
}



/****************************************************************************************
* Function name - alloc_init_client_contexts
*
* Description - Allocate and initialize client contexts
* 
* Input -       *bctx     - pointer to batch context to be set to all clients  of the batch
*               *log_file - output file to be used by all clients of the batch
*
* Return Code -  On Success - 0, on Error -1
****************************************************************************************/
static int alloc_init_client_contexts (batch_context* bctx,
                                       FILE* log_file)
{
  int i;

  /* 
     Iterate through client contexts and initialize them. 
  */
  for (i = 0 ; i < bctx->client_num_max ; i++)
    {
      /* 
         Set the timer handling function, which is used by the smooth 
         loading mode. 
      */
      set_timer_handling_func (&bctx->cctx_array[i], handle_cctx_timer);

      /* 
         Build client name for logging, based on sequence number and 
         ip-address for each simulated client. 
      */
      bctx->cctx_array[i].cycle_num = 0;

      snprintf(bctx->cctx_array[i].client_name, 
               sizeof(bctx->cctx_array[i].client_name) - 1, 
               "%d (%s) ", 
               i + 1, 
               bctx->ip_addr_array[i]);

       /* 
         Set index of the client within the batch.
         Useful to get the client's CURL handle from bctx. 
      */
      bctx->cctx_array[i].client_index = i;
      bctx->cctx_array[i].url_curr_index = 0; /* Actually zeroed by calloc. */
      
      /* Set output stream for each client to be either batch logfile or stderr. */
      bctx->cctx_array[i].file_output = stderr_print_client_msg ? stderr : log_file;

      /* 
         Set pointer in client to its batch object. The pointer will be used to get 
         configuration and set back statistics to batch.
      */
      bctx->cctx_array[i].bctx = bctx;
    }

  return 0;
}

/****************************************************************************************
* Function name - free_batch_data_allocations
*
* Description - Deallocates all  the kings batch allocations
* Input -       *bctx - pointer to batch context to release its allocations
* Return Code/Output - None
****************************************************************************************/
static void free_batch_data_allocations (batch_context* bctx)
{
  int i;
  
  /* 
     Free client contexts 
  */
  if (bctx->cctx_array)
    {
       for (i = 0 ; i < bctx->client_num_max ; i++)
         {
           if (bctx->cctx_array[i].handle)
             curl_easy_cleanup(bctx->cctx_array[i].handle);
           
           /* Free client POST-buffers */ 
           if (bctx->cctx_array[i].post_data)
             {
              free (bctx->cctx_array[i].post_data);
              bctx->cctx_array[i].post_data = NULL;
             }
           
           free(bctx->cctx_array);
           bctx->cctx_array = NULL;
         }
     }
       
   /* 
      Free url contexts
   */
   if (bctx->url_ctx_array)
     {
       /* Free all URL objects */

       for (i = 0 ; i < bctx->urls_num; i++)
         {
           url_context* url = &bctx->url_ctx_array[i];
           
           /* Free url string */
           free (url->url_str);
           url->url_str = NULL ;
           
           /* Free custom HTTP headers. */
           if (url->custom_http_hdrs)
             {
               curl_slist_free_all(url->custom_http_hdrs);
               url->custom_http_hdrs = NULL;
             }
           
           /* Free Form records file (credentials). */
           if (url->form_records_file)
             {
               free (url->form_records_file);
               url->form_records_file = NULL;
             }
           
           /* Free form_records_array */
           if (url->form_records_array)
             {
               int j;
               for (j = 0; j < bctx->client_num_max; j++)
                 {
                   int m;
                   for (m = 0; m < FORM_RECORDS_MAX_TOKENS_NUM; m++)
                     { 
                       if (url->form_records_array[j].form_tokens[m])
                         {
                           free (url->form_records_array[j].form_tokens[m]);
                           url->form_records_array[j].form_tokens[m] = NULL;
                         }
                     }
                 }
               
               free (url->form_records_array);
               url->form_records_array = NULL;
             }
           
           /* Free upload file */
           if (url->upload_file)
             {
               free (url->upload_file);
               url->upload_file = NULL;
             }
           
           /* Close file pointer of upload file */
           if (url->upload_file_ptr)
             {
               fclose (url->upload_file_ptr);
               url->upload_file_ptr = NULL;
             }
           
           /* Free web-authentication credentials */
           if (url->web_auth_credentials)
             {
               free (url->web_auth_credentials);
               url->web_auth_credentials = NULL;
             }
           
           /* Free proxy-authentication credentials */
           if (url->proxy_auth_credentials)
             {
               free (url->proxy_auth_credentials);
               url->proxy_auth_credentials = NULL;
             }
         }
       
       /* Free URL context array */
       free (bctx->url_ctx_array);
       bctx->url_ctx_array = NULL;
    }

}

/****************************************************************************************
* Function name - create_ip_addrs
*
* Description - Adds ip-addresses of batches of loading clients to network adapter/s
* Input -       *bctx_array - pointer to the array of batch contexts
*               bctx_num - number of batch contexts in <bctx_array>
* Return Code/Output - None
****************************************************************************************/
static int create_ip_addrs (batch_context* bctx_array, int bctx_num)
{
  int batch_index, client_index; /* Batch and client indexes */
  struct in_addr in_address;
  struct in6_addr in6_prev, in6_new;
  char*** ip_addresses =0;
  char* ipv4_string = 0;
  char ipv6_string[INET6_ADDRSTRLEN+1];

  /* 
     Add secondary IP-addresses to the "loading" network interface. 
  */
  if (!(ip_addresses = (char***)calloc (bctx_num, sizeof (char**))))
    {
      fprintf (stderr, "%s - error: failed to allocate ip_addresses.\n", __func__);
      return -1;
    }
  
  for (batch_index = 0 ; batch_index < bctx_num ; batch_index++) 
    {
      /* 
         Allocate the array of IP-addresses 
      */
      if (!(ip_addresses[batch_index] = (char**)calloc (bctx_array[batch_index].client_num_max, 
                                               sizeof (char *))))
        {
          fprintf (stderr, 
                   "%s - error: failed to allocate array of ip-addresses for batch %d.\n", 
                   __func__, batch_index);
          return -1;
        }

      batch_context* bctx = &bctx_array[batch_index];

      /* 
         Set them to the batch contexts to remember them. 
      */
      bctx->ip_addr_array = ip_addresses[batch_index]; 

      /* 
         Allocate for each client a buffer and snprintf to it the IP-address string.
      */
      for (client_index = 0; client_index < bctx->client_num_max; client_index++)
        {
          if (!(ip_addresses[batch_index][client_index] = 
                (char*)calloc (bctx->ipv6 ? INET6_ADDRSTRLEN + 1 : 
                               INET_ADDRSTRLEN + 1, sizeof (char))))
            {
              fprintf (stderr, "%s - allocation of ip_addresses[%d][%d] failed\n", 
                       __func__, batch_index, client_index) ;
              return -1;
            }

          if (bctx->ipv6 == 0)
            {
                /* 
                   When clients are not using common IP, advance the 
                   IPv4-address, using client index as the offset. 
                */
                in_address.s_addr = htonl (bctx->ip_addr_min + 
                                           (bctx->ip_common ? 0 : client_index));
         
                    if (! (ipv4_string = inet_ntoa (in_address)))
                    {
                        fprintf (stderr, "%s - inet_ntoa() failed for ip_addresses[%d][%d]\n", 
                                 __func__, batch_index, client_index) ;
                        return -1;
                    }
            }
          else
            {
              /* 
                 When clients are not using common IP,  advance the 
                 IPv6-address by incrementing previous address. 
              */
              if (client_index == 0 || bctx->ip_common)
                {
                  memcpy (&in6_prev, &bctx->ipv6_addr_min, sizeof (in6_prev));
                  memcpy (&in6_new, &bctx->ipv6_addr_min, sizeof (in6_new));
                }
              else
                {
                  if (ipv6_increment (&in6_prev, &in6_new) == -1)
                    {
                      fprintf (stderr, "%s - ipv6_increment() failed for ip_addresses[%d][%d]\n", 
                               __func__, batch_index, client_index) ;
                      return -1;
                    }
                }

              if (! inet_ntop (AF_INET6, &in6_new, ipv6_string, sizeof (ipv6_string)))
                {
                  fprintf (stderr, "%s - inet_ntoa() failed for ip_addresses[%d][%d]\n", 
                           __func__, batch_index, client_index) ;
                  return -1;
                }
              else
                {
                  /* Remember in6_new address in in6_prev */
                  memcpy (&in6_prev, &in6_new, sizeof (in6_prev));
                }
            }

          snprintf (ip_addresses[batch_index][client_index], 
                    bctx->ipv6 ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN, 
                    "%s", 
                    bctx->ipv6 ? ipv6_string : ipv4_string);
        }

      /* 
         Add all the addresses to the network interface as the secondary 
         ip-addresses, using netlink userland-kernel interface.
      */
      if (add_secondary_ip_addrs (bctx->net_interface,
                                  bctx->client_num_max, 
                                  (const char** const) ip_addresses[batch_index], 
                                  bctx->cidr_netmask,
                                  bctx->scope) == -1)
        {
          fprintf (stderr, 
                   "%s - error: add_secondary_ip_addrs() - failed for batch = %d\n", 
                   __func__, batch_index);
          return -1;
        }
    }

  return 0;
}

/*
  The callback to libcurl to skip all body bytes of the fetched urls.
*/
size_t 
do_nothing_write_func (void *ptr, size_t size, size_t nmemb, void *stream)
{
  (void)ptr;
  (void)stream;

  /* 
     Overwriting the default behavior to write body bytes to stdout and 
     just skipping the body bytes without any output. 
  */
  return (size*nmemb);
}

/****************************************************************************************
* Function name - rewind_logfile_above_maxsize
*
* Description - Rewinds the file pointer, when reaching configurable max-size
* Input -       *filepointer - file pointer to control and rewind, when necessary
*
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int rewind_logfile_above_maxsize (FILE* filepointer)
{
  long position = -1;

  if (!filepointer)
    return -1;

  if (filepointer == stderr)
    return 0;

  if ((position = ftell (filepointer)) == -1)
    {
      fprintf (stderr, 
               "%s - error: ftell () failed with errno = %d\n", __func__, errno);
      return 0;
    }

  if (position > (logfile_rewind_size* 1024*1024))
    {
      rewind (filepointer);
      fprintf (stderr, "%s - logfile with size %ld rewinded.\n", __func__, position);
    }

  return 0;
}

/****************************************************************************************
* Function name - ipv6_increment
*
* Description - Increments the source IPv6 address to provide the next
* 
* Input -       *src - pointer to the IPv6 address to be used as the source
* Input/Output  *dest - pointer to the resulted incremented address
*
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int ipv6_increment(const struct in6_addr *const src, 
                          struct in6_addr *const dest)
{
  uint32_t temp = 1;
  int i;

  for (i = 15; i > 1; i--) 
    {
      temp += src->s6_addr[i];
      dest->s6_addr[i] = temp & 0xff;
      temp >>= 8;
    }

  if (temp != 0)
    {
      fprintf (stderr, "%s - error: passing the scope.\n "
               "Check you IPv6 range to be within the same scope.\n", __func__);
      return -1;
    }

  dest->s6_addr[1] = src->s6_addr[1];
  dest->s6_addr[0] = src->s6_addr[0];

  return 0;
}

