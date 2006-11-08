/*
*     batch.c
*
* 2006 Copyright (c) 
* Robert Iakobashvili, <coroberti@gmail.com>
* Michael Moser,  <moser.michael@gmail.com>                 
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

#include "batch.h"

void stat_point_add (stat_point* left, stat_point* right)
{
  if (!left || !right)
    return;

  left->data_in += right->data_in;
  left->data_out += right->data_out;
  
  left->requests += right->requests;
  left->resp_redirs += right->resp_redirs;
  left->resp_oks += right->resp_oks;
  left->resp_serv_errs += right->resp_serv_errs;
  left->other_errs += right->other_errs;
  
  const int total_points = left->appl_delay_points + right->appl_delay_points;

  if (total_points > 0)
    {
      left->appl_delay = (left->appl_delay * left->appl_delay_points  + 
                          right->appl_delay * right->appl_delay_points) / total_points;
         
    }
  else
    {
      left->appl_delay = 0;
    }
  
}

void stat_point_reset (stat_point* p)
{
  if (!p)
    return;

  p->data_in = p->data_out = 0;
  p->requests = p->resp_redirs = p->resp_oks = p->resp_serv_errs = 
    p->other_errs = 0;
  p->appl_delay_points = 0;
  p->appl_delay = 0;
}


