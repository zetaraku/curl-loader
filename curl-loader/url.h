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

  /* 
     Current loading step: login, uas, logoff. If we are in the loading step 
     login - take login url, logoff - take logoff url. For UAS step - take the 
     next url according to <url_uas_num>. 
  */
typedef enum url_load_step
  {
    URL_LOAD_UNDEF = 0, /*calloc sets it */
    URL_LOAD_LOGIN,
    URL_LOAD_UAS,
    URL_LOAD_LOGOFF,
  } url_load_step;

/*
  Types of URLs.
*/
typedef enum url_appl_type
  {
    URL_APPL_UNDEF = 0, /* set by calloc */
    URL_APPL_HTTP,
    URL_APPL_HTTPS,
    URL_APPL_FTP,
    URL_APPL_FTPS,
  } url_appl_type;


/*
  url_context - structure, that concentrates our knowledge about the url
  to fetch.
*/
typedef struct url_context
{
   /* URL buffer */
  char* url_str;

   /*
     Maximum time for all clients in a batch to accomplish fetching. 
   */
  float url_completion_time;

  /* Sleeping interval after fetching this url in msec.*/
  unsigned long url_interleave_time;

  char username[URL_AUTH_STR_LEN];

  char password[URL_AUTH_STR_LEN];

  /* Application type of url */
  url_appl_type url_appl_type;

  /* 
     At which loading step the url is used: login, uas, logoff. 
     
     If we are in the loading step login - take login url, logoff - take logoff url. 
     For UAS step - take the next url according to <url_uas_num>. 
  */
  url_load_step url_lstep;

  long url_uas_num;

} url_context;


#endif /* URL_H */
