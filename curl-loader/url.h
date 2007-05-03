/*
*     url.h
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


#ifndef URL_H
#define URL_H

#include <stddef.h>

#define URL_AUTH_STR_LEN 64
#define FORM_BUFFER_SIZE 256

/* 
   Current loading step: login, uas or logoff. If we are in the loading step 
   login, pick up the login url; whin in logoff, take the logoff url. 
   Being in UAS step, pick up the next url according to <url_uas_num>. 
*/
typedef enum url_load_step
  {
    URL_LOAD_UNDEF = 0, /*calloc sets it */
    URL_LOAD_LOGIN,
    URL_LOAD_UAS,
    URL_LOAD_LOGOFF,
  } url_load_step;

/*
  Application types of URLs.
*/
typedef enum url_appl_type
  {
    URL_APPL_UNDEF = 0, /* set by calloc */
    URL_APPL_HTTP,
    URL_APPL_HTTPS,
    URL_APPL_FTP,
    URL_APPL_FTPS,
    URL_APPL_SFTP,
  } url_appl_type;


/*
  url_context - structure, that concentrates our knowledge about the url
  to fetch.
*/
typedef struct url_context
{
   /* URL buffer */
  char* url_str;

  long url_cycles_num;

  /* 
     Number of custom  HTTP headers in array.
  */
  size_t custom_http_hdrs_num;
  
  /* 
     The list of custom  HTTP headers.
  */
  struct curl_slist *custom_http_hdrs;

  /* 
     REQ_TYPE_GET_AND_POST, REQ_TYPE_POST, REQ_TYPE_GET
  */
  size_t req_type;

     /* 
      The username to be used to access the URL by filling the POST 
      form or in GET url
   */
  char username[URL_AUTH_STR_LEN];

  /* 
     The password to be used to access the URL by filling the POST form or 
     in GET url.
  */
  char password[URL_AUTH_STR_LEN];

  /* 
     The type of <login_post_str>. Valid types are: 
     UNIQUE_USERS_AND_PASSWORDS, - like "user=%s%d&password=%s%d"
     UNIQUE_USERS_SAME_PASSWORD, - like "user=%s%d&password=%s"
     SINGLE_USER,                - like "user=%s&password=%s"
     LOAD_USERS_FROM_FILE,       - like "user=%s&password=%s" and 
                                                           login_credentials_file defined.
   */
  int form_usage_type;

    /* The string to be used as the base for login post message */
  char form_str [FORM_BUFFER_SIZE + 1];

    /*
     The file with strings like "user:password", where separator may be 
     ':', '@', '/' and ' ' (space) in line with RFC1738. The file may be created
     as a dump of DB tables of users and passwords.
  */
  char* credentials_file;

  char* upload_file;

  int web_auth_method;
  
  char* web_auth_credentials;


  int proxy_auth_method;
  
  char* proxy_auth_credentials;

  
   /*
     Maximum time to accomplish fetching  of the url
   */
  unsigned long timer_url_completion;

  /* Time to relax/sleep after fetching this url (msec).*/
  unsigned long timer_after_url_sleep;


  /* Application type of url, e.g. HTTP, HTTPS, FTP, etc */
  url_appl_type url_appl_type;

  /*
    Our index in the url array
  */
  long url_uas_num;

} url_context;


#endif /* URL_H */
