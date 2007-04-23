/*
*     screen.c
*
* 2007 Copyright (c) 
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
#include <unistd.h>
#include <sys/select.h>

#include <curses.h>

static int test_fd_readable (int fd);
static int on_keybord_input (int key);

void screen_init ()
{
    /* Initializes curses library */
  initscr();

  /* Improves performances */
  noecho();
}

int screen_test_keyboard_input ()
{
  int rval_input = -1;

  if ((rval_input = test_fd_readable (STDIN_FILENO)) == -1)
    {
      fprintf(stderr, "%s - select () failed with errno %d.\n", 
              __func__, errno);
      return -1;
    }

  if (!rval_input)
    return 0;
  

  int the_key;

  if (rval_input > 0)
    {
      if ((the_key = getch ())== ERR)
        {
          //fprintf(stderr, "%s - getch () failed with errno %d.\n", 
          //        __func__, errno);
          return 0;
        }
      
      return on_keybord_input (the_key);
    }

  return 0;
}

int on_keybord_input (int key)
{
     switch (key) 
       {
       case '+':

         break;
         
       case '-':

         break;
         
       case '*':

         break;
         
       case '/':

         break;
       }

     fprintf(stderr, "%s - got %c\n", __func__, key);
     return 0;
}

/****************************************************************************************
* Function name - test_fd_readable
*
* Description - Tests, whether the descriptor is readable
* Input -       fd -  file descriptor
*
* Return Code - When readable (1), when not readable (0), on error (-1)
****************************************************************************************/
static int test_fd_readable (int fd)
{
  fd_set fdset;
  struct timeval timeout = {0,0};

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);

  return select (fd + 1, &fdset, NULL, NULL, &timeout);
}


