/*
 * ISO 13818 stream multiplexer
 * Copyright (C) 2001 Convergence Integrated Media GmbH Berlin
 * Copyright (C) 2004 Oskar Schirmer (schirmer@scara.com)
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
 * Module:  Output
 * Purpose: Handle output buffers, write to stdout occasionally.
 *
 * Besides functions to detect states of the output buffer, and to
 * write data from it, support in filling the output buffer is
 * provided to the splicers.
 */

#include "global.h"
#include "error.h"
#include "output.h"

static refr_ctrl refc;
static refr_data refd;

static int outf;

static boolean outtrigger;
static int outdelta;
static t_msec trigger_msec_output;

static int next_size;

static t_msec statistics_msec;
static t_msec statistics_next;
static int statistics_load;
static int statistics_bursts;
static int statistics_refd_min;
static int statistics_refd_max;
static int statistics_time_min;
static int statistics_time_max;
static int statistics_burst_min;
static int statistics_burst_max;

boolean output_init (void)
{
  int r;
  struct stat outstat;
  outtrigger = FALSE;
  trigger_msec_output = TRIGGER_MSEC_OUTPUT;
  next_size = HIGHWATER_OUT;
  statistics_msec = 0;
  if (!list_create (refc,MAX_CTRL_OUTB)) {
    return (FALSE);
  }
  if (!list_create (refd,MAX_DATA_OUTB)) {
    return (FALSE);
  }
  r = -1;
  outf = STDOUT_FILENO;

  if (outf >= 0) {
    r = fstat (outf,&outstat);
  }
  if (r != 0) {
    warn (LERR,"Cannot open",EOUT,5,outf,r);
    warn (LERR,strerror(errno),EOUT,5,1,errno);
    return (FALSE);
  }
  r = fcntl(outf, F_GETFL);
  if (r >= 0) {
    r = fcntl(outf, F_SETFL, r | O_NONBLOCK);
  }
  if (r < 0) {
    warn (LERR,"Cannot fcntl",EOUT,5,2,errno);
    return (FALSE);
  }
  if (!S_ISREG (outstat.st_mode)) {
    timed_io = TRUE;
  }
  return (outf >= 0);
}

/* Calculate the free space in the output buffer
 * Return: Number of bytes
 */
static int output_free (void)
{
  register int r;
  r = list_full (refc) ? 0 : list_free (refd);
  warn (LDEB,"Free",EOUT,1,r,list_free (refd));
  return (r);
}

/* Determine whether there is probably enough space in the output buffer
 * Base for this decision is a guessed expected size "next_size", which
 * will be adapted by the functions that finally know the correct value.
 * Thus it is possible that processing data is started and then postponed
 * due to lack of space (see function output_pushdata).
 * Return: TRUE if probably enough space, FALSE otherwise
 */
boolean output_acceptable (void)
{
  warn (LDEB,"Acceptable",EOUT,2,0,next_size);
  return (output_free () >= next_size);
}

/* Reserve space in the output buffer.
 * size is the number of bytes wanted.
 * if timed==TRUE, then push is a time stamp (millisec),
 * otherwise a time stamp for greedy processing is calculated locally.
 * Precondition: size>0
 * Return: Pointer to data block, if available, NULL otherwise
 */
byte *output_pushdata (int size,
    boolean timed,
    int push)
{
  ctrl_buffer *c;
  byte *d;
  next_size = mmax (size,HIGHWATER_OUT);
  if (size > list_free (refd)) {
    warn (LDEB,"Pushdata (Postpone)",EOUT,3,1,size);
    return (NULL);
  }
  if (size > list_freeinend (refd)) {
    refd.in = 0;
  }
  if (size > list_free (refd)) {
    warn (LDEB,"Pushdata (Postpone)",EOUT,3,2,size);
    return (NULL);
  }
  warn (LDEB,"Pushdata",EOUT,3,3,size);
  warn (LDEB,"Pushdata Index",EOUT,3,4,refd.in);
  c = &refc.ptr[refc.in];
  c->index = refd.in;
  c->length = size;
  c->msecpush = timed
              ? push
              : (msec_now () - (outtrigger ? outdelta : trigger_msec_output));
  list_incr (refc.in,refc,1);
  d = &refd.ptr[refd.in];
  list_incr (refd.in,refd,size);
  if (statistics_msec > 0) {
    int tmp;
    tmp = list_size (refd);
    if (tmp > statistics_refd_max) {
      statistics_refd_max = tmp;
    }
    if (timed) {
      tmp = push + outdelta - msec_now ();
      if (tmp > statistics_time_max) {
        statistics_time_max = tmp;
      }
    }
  }
  next_size = HIGHWATER_OUT;
  return (d);
}

/* Set trigger timing value.
 */
void output_settriggertiming (t_msec time)
{
  trigger_msec_output = time;
}

/* Check whether data is available to be written to stdout.
 * If so, set the poll struct accordingly.
 * Check the time stamp and set the timeout^ accordingly.
 * Return: TRUE, if data is available to be written immediately,
 *         FALSE otherwise.
 */ 
boolean output_available (unsigned int *nfds,
    struct pollfd *ufds,
    t_msec *timeout)
{
  t_msec t, now;
  boolean avail;
  t = -1;
  avail = FALSE;
  now = msec_now ();
  if (list_empty (refc)) {
    warn (LDEB,"Available Empty",EOUT,4,1,0);
    /* outtrigger = FALSE; */
    /* and discontinuity things */
  } else {
    t = refc.ptr[refc.out].msecpush - now;
    if (!outtrigger) {
      if (list_partialfull (refd)
       || (-t >= trigger_msec_output)) {
        outdelta = -t;
        warn (LDEB,"Available Trigger",EOUT,4,2,outdelta);
        outtrigger = TRUE;
      }
    }
    if (outtrigger) {
      t += outdelta;
      if (t <= 0) {
        warn (LDEB,"Available",EOUT,4,3,t);
        ufds->fd = outf;
        ufds->events = POLLOUT;
        *nfds += 1;
        avail = TRUE;
        t = -1;
      }
    } else {
      t += trigger_msec_output;
    }
  }
  if ((statistics_msec > 0) && (statistics_load > 0)) {
    t_msec s;
    s = statistics_next - now;
    if (s < 0) {
      s = 0;
    }
    if ((t < 0) || (s < t)) {
      t = s;
    }
  }
  *timeout = t;
  return (avail);
}

/* Set statistics generation frequency, time=0 to switch off.
 */
void output_set_statistics (t_msec time)
{
  int tmp;
  statistics_msec = time;
  if (time > 0) {
    statistics_next = msec_now () + time;
  }
  statistics_load = 0;
  statistics_bursts = 0;
  statistics_refd_min = statistics_refd_max = list_size (refd);
  statistics_burst_min = statistics_burst_max = 0;
  statistics_time_min = statistics_time_max =
      list_empty (refc) ? 0 :
        (tmp = refc.in,
         refc.ptr[list_incr (tmp,refc,-1)].msecpush + outdelta - msec_now ()); 
}

/* Generate statistics, if the time is right for this.
 */
void output_gen_statistics (void)
{
  if (statistics_msec > 0) {
    t_msec now;
    int tmp;
    now = msec_now ();
    if (now >= statistics_next) {
      fprintf (stderr, "Stat: now:%8d out:%8d/%4d buf:%8d..%8d time:%6d..%6d burst:%6d..%6d\n",
          now, statistics_load, statistics_bursts,
          statistics_refd_min, statistics_refd_max,
          statistics_time_min, statistics_time_max,
          statistics_burst_min, statistics_burst_max);
      statistics_load = 0;
      statistics_bursts = 0;
      statistics_refd_min = statistics_refd_max = list_size (refd);
      statistics_burst_min = statistics_burst_max;
      statistics_burst_max = 0;
      statistics_time_min = statistics_time_max =
          list_empty (refc) ? 0 :
            (tmp = refc.in,
             refc.ptr[list_incr (tmp,refc,-1)].msecpush + outdelta - now),
      statistics_next = now + statistics_msec;
    }
  }
}

/* Prepare to finish.
 * provoke output buffer flush
 */
void output_finish (void)
{
  outtrigger = TRUE;
  output_set_statistics (0);
}

/* Write some data to stdout from the output buffer.
 * Write as much data as available unwrapped and with identical time stamp.
 * Precondition: poll has stated data or error for stdout
 */
void output_something (boolean writeable)
{
  t_msec msec;
  int i, l, o;
  o = refc.out;
  msec = refc.ptr[o].msecpush;
  i = refc.ptr[o].index;
  l = 0;
  if (writeable) {
    do {
      l += refc.ptr[o].length;
    } while ((l < MAX_WRITE_OUT)
          && (list_incr (o,refc,1) != refc.in)
          && (refc.ptr[o].msecpush == msec)
          && (refc.ptr[o].index == (i+l))); /* and < MAX_WRITE_OUT */
    warn (LDEB,"Something",EOUT,0,1,l);
    if (statistics_msec > 0) {
      if (l < statistics_burst_min) {
        statistics_burst_min = l;
      }
      if (l > statistics_burst_max) {
        statistics_burst_max = l;
      }
    }
    l = write (outf,&refd.ptr[i],l);
  }
  warn (LDEB,"Some Written",EOUT,0,2,l);
  if (l > 0) {
    statistics_load += l;
    statistics_bursts += 1;
    o = refc.out;
    do {
      if (l < refc.ptr[o].length) {
        refc.ptr[o].length -= l;
        refc.ptr[o].index += l;
        warn (LDEB,"Some Left",EOUT,0,3,refc.ptr[o].length);
        l = 0;
      } else {
        warn (LDEB,"Some Done",EOUT,0,4,o);
        l -= refc.ptr[o].length;
        refc.out = list_incr (o,refc,1);
      }
    } while (l > 0);
    if (list_empty (refc)) {
      refd.out = refd.in = 0;
    } else {
      refd.out = refc.ptr[o].index;
    }
    if (statistics_msec > 0) {
      int tmp, tin;
      tmp = list_size (refd);
      if (tmp < statistics_refd_min) {
        statistics_refd_min = tmp;
      }
      tmp = list_empty (refc) ? 0 :
        (tin = refc.in,
         refc.ptr[list_incr (tin,refc,-1)].msecpush + outdelta - msec);
      if (tmp < statistics_time_min) {
        statistics_time_min = tmp;
      }
    }
  }
}

