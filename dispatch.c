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
 * Module:  Dispatch
 * Purpose: Main dispatching loop.
 *
 * The first two buffer stages (raw file data and input stream data)
 * are filled gready, the following two stages (from input stream to
 * output data buffer, and further to stdout) are timing controlled.
 */

#include "global.h"
#include "error.h"
#include "splice.h"
#include "input.h"
#include "output.h"
#include "command.h"
#include "dispatch.h"

boolean fatal_error;
boolean force_quit;
boolean busy_work;

boolean dispatch_init (void)
{
  fatal_error = FALSE;
  force_quit = FALSE;
  busy_work = FALSE;
  return (TRUE);
}

/* Dispatch work to the modules as needed.
 * Mainly, check a few internal conditions (buffer space,
 * data availability), check the corresponding files with
 * poll for readiness, then do something at the appropriate
 * points to push data forward.
 * Data is pushed thru the multiplexer unidirectionally:
 * input --> f.rawdata --> s.pesdata --> o.spliceddata --> output
 * The main loop continues as long as there is something to do.
 * Thereafter, the last two stages may generate a finish and
 * another loop tries to pump the buffers out.
 */
void dispatch (void)
{
  boolean bi, bo, bs;
  stream_descr *st;
  t_msec tmo;
  unsigned int nfds, onfds, infds;
  int pollresult;
  struct pollfd ufds [MAX_POLLFD];
  warn (LDEB,"Dispatch",EDIS,0,0,0);
  bs = FALSE;
  st = input_available ();
  nfds = 0;
  command_expected (&nfds, &ufds[0]);
  onfds = nfds;
  bo = output_available (&nfds, &ufds[onfds], &tmo);
  while ((bo
       || bs
       || (st != NULL)
       || input_expected ()
       || ((tmo >= 0) && (tmo <= MAX_MSEC_OUTDELAY))
       || busy_work)
      && (!fatal_error)
      && (!force_quit)) {
    warn (LDEB,"Loop",EDIS,0,
      bo | (bs << 1) | (input_expected () << 2) | (st ? 1 << 3 : 0),tmo);
    infds = nfds;
    bi = input_acceptable (&nfds, &ufds[infds], &tmo, output_acceptable ());
    if ((bs)
     || ((st != NULL) && output_acceptable ())) {
      tmo = 0;
    }
    warn (LDEB,"Poll",EDIS,1,nfds,tmo);
#ifdef DEBUG_TIMEPOLL
    ltp->tmo = tmo;
    if (ltp->usec != 0) {
      struct timeval tv;
      gettimeofday (&tv,NULL);
      ltp->usec -= tv.tv_usec;
    }
    ltp->nfdso = infds - onfds;
    ltp->nfdsi = nfds - infds;
    ltp->flags |=
      (bo ? LTP_FLAG_OUTPUT : 0) |
      (bi ? LTP_FLAG_INPUT : 0) |
      (bs ? LTP_FLAG_SPLIT : 0) |
      (st != NULL ? LTP_FLAG_PROCESS : 0);
    ltp->sr = deb_inraw_free (0);
    ltp->si = deb_instr_free (1);
    ltp->so = output_free ();
    ltp->nfdsrevent =
#endif
    pollresult =
      poll (&ufds[0], nfds, ((!timed_io) && (tmo > 0)) ? 0 : tmo);
    if ((!timed_io) && (tmo > 0)) {
      if (pollresult == 0) {
        global_delta += tmo;
        warn (LDEB,"Global Delta",EDIS,0,3,global_delta);
#ifdef DEBUG_TIMEPOLL
        ltp->flags |= LTP_FLAG_DELTASHIFT;
#endif
      }
    }
#ifdef DEBUG_TIMEPOLL
    if (logtpc < (max_timepoll-1)) {
      struct timeval tv;
      logtpc += 1;
      ltp++;
      gettimeofday (&tv,NULL);
      ltp->usec = tv.tv_usec;
    }
#endif
    warn (LDEB,"Poll done",EDIS,0,2,pollresult);
    if ((0 < onfds)
     && (ufds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
      command_process (ufds[0].revents & POLLIN);
    }
    if (bo
     && (ufds[onfds].revents & (POLLOUT | POLLHUP | POLLERR))) {
      output_something (ufds[onfds].revents & POLLOUT);
    }
    output_gen_statistics ();
    if (bi) {
      while (infds < nfds) {
        if (ufds[infds].revents & (POLLIN | POLLHUP | POLLERR)) {
          input_something (input_filehandle (ufds[infds].fd),
              ufds[infds].revents & POLLIN);
          bi = FALSE;
          bs = TRUE;
        }
        infds += 1;
      }
      if (!bi) {
        if (st == NULL) {
          st = input_available ();
        }
      }
    }
    if (bs) {
      bs = split_something ();
    }
    if ((st != NULL) && output_acceptable ()) {
      st = process_something (st);
      bs = TRUE;
    }
    if (st == NULL) {
      st = input_available ();
    }
    nfds = 0;
    command_expected (&nfds, &ufds[0]);
    onfds = nfds;
    bo = output_available (&nfds, &ufds[onfds], &tmo);
    splice_all_configuration ();
  }
  process_finish ();
  output_finish ();
  while ((output_available (&nfds, &ufds[0], &tmo)
       || (tmo >= 0))
      && (!fatal_error)) {
    output_something (TRUE);
  }
#ifdef DEBUG_TIMEPOLL
  {
    int i, u, s;
    i = 0;
    s = 0;
    fprintf (stderr, "lines: %8d\n", (int)logtpc);
    while (i < logtpc) {
      u = (logtp[i].usec > 0 ? 1000000 : 0) - logtp[i].usec;
      s += u;
      fprintf (stderr,
        "%08d %10d.%06d:%8d (%6d) %c%5d %d %d/%d %c%c%c%c %d (F:%6d,S:%6d,O:%6d)\n",
        i,
        (int)logtp[i].tv.tv_sec,
        (int)logtp[i].tv.tv_usec,
        logtp[i].msec_now,
        u,
        logtp[i].flags & LTP_FLAG_DELTASHIFT ? 'D' : ' ',
        logtp[i].tmo,
        logtp[i].cnt_msecnow,
        logtp[i].nfdsi,
        logtp[i].nfdso,
        logtp[i].flags & LTP_FLAG_INPUT ? 'I' : ' ',
        logtp[i].flags & LTP_FLAG_SPLIT ? 'S' : ' ',
        logtp[i].flags & LTP_FLAG_PROCESS ? 'P' : ' ',
        logtp[i].flags & LTP_FLAG_OUTPUT ? 'O' : ' ',
        logtp[i].nfdsrevent,
        logtp[i].sr,
        logtp[i].si,
        logtp[i].so
        );
      i += 1;
    }
    fprintf (stderr, "%43d\n", s);
  }
#endif
}

