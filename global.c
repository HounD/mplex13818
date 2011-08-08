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
 * Module:  Global
 * Purpose: Service functions.
 */

#include "global.h"
#include "error.h"

boolean timed_io;
boolean accept_weird_scr;
boolean conservative_pid_assignment;
t_msec global_delta;

#ifdef DEBUG_TIMEPOLL
timepoll logtp [max_timepoll];
long logtpc;
timepoll *ltp;
#endif

/* Provide the present system time in relative milliseconds.
 * The zero point may be moved as unconditional waiting is proposed
 * in the dispatcher, but timed_io=FALSE.
 * Return: milliseconds
 */
t_msec msec_now (void)
{
#define MSEC_EXPONENT 21
  static long last;
  static int local_delta;
  struct timeval tv;
  register int now;
  gettimeofday (&tv,NULL);
#ifdef DEBUG_TIMEPOLL
  ltp->tv.tv_sec = tv.tv_sec;
  ltp->tv.tv_usec = tv.tv_usec;
#endif
  if ((tv.tv_sec & (~((1L << MSEC_EXPONENT) - 1))) != last) {
    last = tv.tv_sec & (~((1L << MSEC_EXPONENT) - 1));
    local_delta += 1000 * (1L << MSEC_EXPONENT);
  }
  now = (tv.tv_sec & ((1L << MSEC_EXPONENT) - 1)) * 1000
      + tv.tv_usec / 1000 + local_delta;
  warn (LDEB,"msec_now",EGLO,3,0,now);
#ifdef DEBUG_TIMEPOLL
  ltp->cnt_msecnow += 1;
  ltp->msec_now = now + global_delta;
#endif
  return (now + global_delta);
}

/* Convert a clock reference value (90kHz) to milliseconds,
 * using a conversion base to avoid wrap around errors.
 */
void cref2msec (conversion_base *b,
    clockref c,
    t_msec *m)
{
#define CREF2MSEC_LIMIT (90 * 1024 * 16) /* 16 sec */
  unsigned long d;
  d = c.base - b->base;
  if (d >= (2 * CREF2MSEC_LIMIT)) {
    if (d >= (3 * CREF2MSEC_LIMIT)) {
      warn (LDEB,"cref2msec",EGLO,4,1,d);
      b->base = c.base - CREF2MSEC_LIMIT;
      b->msec = b->base / 90;
    } else {
      warn (LDEB,"cref2msec",EGLO,4,2,d);
      b->base += CREF2MSEC_LIMIT;
      b->msec += CREF2MSEC_LIMIT / 90;
    }
    d = c.base - b->base;
  }
  *m = (d / 90) + b->msec;
}
 
/* Convert milliseconds to a clock reference value (90kHz),
 * using a conversion base to avoid wrap around errors.
 */
void msec2cref (conversion_base *b,
    t_msec m,
    clockref *c)
{
#define MSEC2CREF_LIMIT (1024 * 10) /* 10 sec */
  unsigned int d;
  d = m - b->msec;
  if (d >= (2 * MSEC2CREF_LIMIT)) {
    if (d >= (3 * MSEC2CREF_LIMIT)) {
      warn (LDEB,"msec2cref",EGLO,5,1,d);
      b->msec = m - MSEC2CREF_LIMIT;
      b->base = b->msec * 45;
    } else {
      warn (LDEB,"msec2cref",EGLO,5,2,d);
      b->msec += MSEC2CREF_LIMIT;
      b->base += MSEC2CREF_LIMIT * 45;
    }
    d = m - b->msec;
  }
  d = (d * 45) + b->base;
  c->base = d * 2;
  c->ba33 = (d >> 31) & 0x01;
  c->ext = 0;
  c->valid = TRUE;
}

void global_init (void)
{
#ifdef DEBUG_TIMEPOLL
  {
    struct timeval tv;
    memset (&logtp[0],0,sizeof(logtp));
    gettimeofday (&tv,NULL);
    logtp[0].usec = tv.tv_usec;
    logtpc = 0;
    ltp = &logtp[0];
  }
#endif
  verbose_level = LWAR;
  global_delta = 0;
  global_delta = - msec_now ();
  timed_io = FALSE;
  accept_weird_scr = FALSE;
  conservative_pid_assignment = FALSE;
}

