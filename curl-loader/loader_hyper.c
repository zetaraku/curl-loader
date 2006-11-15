/* 
*     loader_hyper.c
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
*
* Cooked from the CURL-project examples with thanks to the 
* great CURL-project authors and contributors.
*/

#include <unistd.h>

#include "loader.h"
#include "batch.h"
#include "client.h"
#include "conf.h"


/****************************************************************************************
* Function name - user_activity_hyper
*
* Description - Simulates user-activities, like login, uas, logoff, using HYPER-MODE
* Input -       *cctx_array - array of client contexts (related to a certain batch of clients)
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
int user_activity_hyper (client_context*const cctx_array)
{
  (void) cctx_array;
  return 0;
}
