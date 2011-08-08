/*
 * ISO 13818 stream multiplexer
 * Copyright (C) 2001 Convergence Integrated Media GmbH Berlin
 * Author: Oskar Schirmer (schirmer@scara.com)
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

/*
 * Module:  Init
 * Purpose: Initialization and start the dispatcher loop.
 */

#include <signal.h>
#include "global.h"
#include "crc32.h"
#include "error.h"
#include "input.h"
#include "output.h"
#include "splice.h"
#include "command.h"
#include "dispatch.h"

static void signalhandler(int sig)
{
  exit (0);
}

static void system_init ()
{
  signal (SIGINT, (void *) (*signalhandler));
  signal (SIGPIPE, SIG_IGN);
}

int main (int argc,
    char *argv[])
{
  int a;
  system_init ();
  global_init ();
  a = msec_now ();
  gen_crc32_table ();
  if (input_init ()) {
    if (output_init ()) {
      if (splice_init ()) {
        if (dispatch_init ()) {
          if (command_init (argc,&argv[0])) {
            dispatch ();
#ifdef DEBUG_TIMEPOLL
            warn (LDEB,"Global delta",EINI,0,0,global_delta);
            warn (LERR,"(msec) Exit Time ",EINI,0,0,
              msec_now () - a);
            warn (LERR,"(msec) Exit Clock",EINI,0,0,
              clock ()/(CLOCKS_PER_SEC/1000));
#endif
          }
          exit (EXIT_SUCCESS);
        } else {
          warn (LERR,"Dispatch Fail",EINI,0,6,0);
        }
      } else {
        warn (LERR,"Splice Fail",EINI,0,5,0);
      }
    } else {
      warn (LERR,"Output Fail",EINI,0,4,0);
    }
  } else {
    warn (LERR,"Input Fail",EINI,0,3,0);
  }
  exit (EXIT_FAILURE);
}

