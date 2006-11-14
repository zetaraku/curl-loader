/*
*     client.h
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

#include "client.h"
#include "batch.h" /* we need only statistics structure */

int hdrs_req (client_context* cctx)
{
  return cctx->hdrs_req;
}
void hdrs_req_inc (client_context* cctx)
{
  cctx->hdrs_req++;
}

int hdrs_2xx (client_context* cctx)
{
  return cctx->hdrs_2xx;
}
void hdrs_2xx_inc (client_context* cctx)
{
  cctx->hdrs_2xx++;
}

int hdrs_3xx (client_context* cctx)
{
  return cctx->hdrs_3xx++;
}
void hdrs_3xx_inc (client_context* cctx)
{
  cctx->hdrs_3xx++;
}

int hdrs_5xx (client_context* cctx)
{
  return cctx->hdrs_5xx;
}
void hdrs_5xx_inc (client_context* cctx)
{
  cctx->hdrs_5xx++;
}


void hdrs_clear_all (client_context* cctx)
{
  cctx->hdrs_req = cctx->hdrs_2xx = cctx->hdrs_3xx = cctx->hdrs_5xx = 0;
}
void hdrs_clear_non_req (client_context* cctx)
{
  cctx->hdrs_2xx = cctx->hdrs_3xx = cctx->hdrs_5xx = 0;
}
void hdrs_clear_non_2xx (client_context* cctx)
{
  cctx->hdrs_req = cctx->hdrs_3xx = cctx->hdrs_5xx = 0;
}
void hdrs_clear_non_3xx (client_context* cctx)
{
  cctx->hdrs_req = cctx->hdrs_2xx = cctx->hdrs_5xx = 0;
}
void hdrs_clear_non_5xx (client_context* cctx)
{
  cctx->hdrs_req = cctx->hdrs_2xx = cctx->hdrs_3xx = 0;
}



void stat_data_out_add (client_context* cctx, u_long bytes)
{
  cctx->st.data_out += bytes;

  cctx->is_https ? (cctx->bctx->https_delta.data_out += bytes) :
    (cctx->bctx->http_delta.data_out += bytes);
}

void stat_data_in_add (client_context* cctx, u_long bytes)
{
  cctx->st.data_in += bytes;
  
  cctx->is_https ? (cctx->bctx->https_delta.data_in += bytes) :
    (cctx->bctx->http_delta.data_in += bytes);
}

void stat_err_inc (client_context* cctx)
{
  cctx->st.other_errs++;
  cctx->is_https ? cctx->bctx->https_delta.other_errs++ :
    cctx->bctx->http_delta.other_errs++;
}
void stat_req_inc (client_context* cctx)
{
  cctx->st.requests++;
    cctx->is_https ? cctx->bctx->https_delta.requests++ :
    cctx->bctx->http_delta.requests++;
}
void stat_2xx_inc (client_context* cctx)
{
  cctx->st.resp_oks++;
  cctx->is_https ? cctx->bctx->https_delta.resp_oks++ :
    cctx->bctx->http_delta.resp_oks++;
}
void stat_3xx_inc (client_context* cctx)
{
  cctx->st.resp_redirs++;
  cctx->is_https ? cctx->bctx->https_delta.resp_redirs++ :
    cctx->bctx->http_delta.resp_redirs++;
}
void stat_5xx_inc (client_context* cctx)
{
  cctx->st.resp_serv_errs++;
  cctx->is_https ? cctx->bctx->https_delta.resp_serv_errs++ :
    cctx->bctx->http_delta.resp_serv_errs++;
}

void stat_appl_delay_add (client_context* cctx, u_long resp_timestamp)
{
  if (resp_timestamp > cctx->req_timestamp)
    {
      if (cctx->is_https)
        {
          cctx->bctx->https_delta.appl_delay = 
            (cctx->bctx->https_delta.appl_delay * cctx->bctx->https_delta.appl_delay_points +
             resp_timestamp - cctx->req_timestamp) / ++cctx->bctx->https_delta.appl_delay_points;
        }
      else
        {
          cctx->bctx->http_delta.appl_delay = 
            (cctx->bctx->http_delta.appl_delay * cctx->bctx->http_delta.appl_delay_points +
             resp_timestamp - cctx->req_timestamp) / ++cctx->bctx->http_delta.appl_delay_points;
        }
    }
}
void stat_appl_delay_2xx_add (client_context* cctx, u_long resp_timestamp)
{
    if (resp_timestamp > cctx->req_timestamp)
    {
      if (cctx->is_https)
        {
          cctx->bctx->https_delta.appl_delay_2xx = 
            (cctx->bctx->https_delta.appl_delay_2xx * cctx->bctx->https_delta.appl_delay_2xx_points +
             resp_timestamp - cctx->req_timestamp) / ++cctx->bctx->https_delta.appl_delay_2xx_points;
        }
      else
        {
          cctx->bctx->http_delta.appl_delay_2xx = 
            (cctx->bctx->http_delta.appl_delay_2xx * cctx->bctx->http_delta.appl_delay_2xx_points +
             resp_timestamp - cctx->req_timestamp) / ++cctx->bctx->http_delta.appl_delay_2xx_points;
        }
    }
}

void dump_client (FILE* file, client_context* cctx)
{
  if (!file || !cctx)
    return;

  fprintf (file, "%s,cycles:%ld,state:%d,d_in:%lld,d_out:%lld,req:%ld,rsp_3xx:%ld,rsp_oks:%ld,rsp_5xx:%ld,err:%ld\n", 
           cctx->client_name, cctx->cycle_num, cctx->client_state, 
           cctx->st.data_in,  cctx->st.data_out, cctx->st.requests, cctx->st.resp_redirs, 
           cctx->st.resp_oks, cctx->st.resp_serv_errs, cctx->st.other_errs);
  fflush (file);
}
