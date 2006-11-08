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

void stat_err_inc (client_context* cctx)
{
  cctx->is_https ? cctx->bctx->https_delta.other_errs++ :
    cctx->bctx->http_delta.other_errs++;
}
void stat_req_inc (client_context* cctx)
{
    cctx->is_https ? cctx->bctx->https_delta.requests++ :
    cctx->bctx->http_delta.requests++;
}
void stat_2xx_inc (client_context* cctx)
{
  cctx->is_https ? cctx->bctx->https_delta.resp_oks++ :
    cctx->bctx->http_delta.resp_oks++;
}
void stat_3xx_inc (client_context* cctx)
{
  cctx->is_https ? cctx->bctx->https_delta.resp_redirs++ :
    cctx->bctx->http_delta.resp_redirs++;
}
void stat_5xx_inc (client_context* cctx)
{
  cctx->is_https ? cctx->bctx->https_delta.resp_serv_errs++ :
    cctx->bctx->http_delta.resp_serv_errs++;
}
