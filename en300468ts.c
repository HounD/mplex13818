/*
 * SI table generator (EN 300468)
 * Copyright (C) 2004,2005 Oskar Schirmer (schirmer@scara.com)
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
 * Module:  SI table generator
 * Purpose: From a list of descriptive files generate SI tables and feed
 *          them to stdout.
 * known bug: missing check for section length <= 1021
 */

//#define DEBUG

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include "crc32.h"

#ifdef PATH_MAX
#define MY_PATH_MAX PATH_MAX
#else
#define MY_PATH_MAX 4096
#endif

static struct pollfd *pfd = NULL;
static int npfd = 0;
static int ipfd;

#define TABLE_PID_FIRST 0x10
#define TABLE_PID_LAST  0x1F
#define TABBUF_SIZE     (1<<15)
#define MAX_PSI_SIZE    (4096+1)

static struct {
  unsigned char *tabbuf;
  unsigned int tabin;
  unsigned int tabout;
  unsigned char tabinold;
  unsigned char conticnt;
} perpid[TABLE_PID_LAST-TABLE_PID_FIRST+1];

#define TABLEID_FIRST   0x40
#define TABLEID_LAST    0x7F

#define TS_PACKET_SIZE  188
#define TS_HEADSLEN     3
#define TS_PACKET_HEADSIZE      4
#define TS_PFIELDLEN    1
#define TS_SYNC_BYTE    0x47
#define OUTBUF_PACKETS  256
#define OUTBUF_SIZE     (TS_PACKET_SIZE*OUTBUF_PACKETS)

static unsigned int outin = 0;
static unsigned int outout = 0;
static unsigned char outbuf[OUTBUF_SIZE];

static void pollfd_init()
{
  ipfd = 0;
}

static int pollfd_add(int f, short e)
{
  if (npfd <= ipfd) {
    npfd = 2*npfd+1;
    pfd = realloc(pfd, npfd * sizeof(struct pollfd));
  }
  pfd[ipfd].fd = f;
  pfd[ipfd].events = e;
  return ipfd++;
}

static int pollfd_poll(int timeout)
{
  return poll(pfd, ipfd, timeout);
}

static int pollfd_rev(int i)
{
  return pfd[i].revents;
}

static void signalhandler(int sig)
{
  exit(0);
}

static void system_init()
{
  signal(SIGINT, (void *) (*signalhandler));
  signal(SIGPIPE, SIG_IGN);
}

static void unblockf(int f)
{
  int r;
  r = fcntl(f, F_GETFL);
  if (r >= 0) {
    r = fcntl(f, F_SETFL, r | O_NONBLOCK);
  }
  if (r < 0) {
    fprintf(stderr, "fcntl failed(%d): %d\n", errno, f);
  }
}

enum enumsi {
#define ENDEF_BEGIN(name,pid,tableid) name ,
#define ENDEF_END
#define ENDEF_NUMBER0(name)
#define ENDEF_NUMBER1(name)
#define ENDEF_DESCR
#define ENDEF_DATETIME
#define ENDEF_LOOP
#define ENDEF_LOOPEND0
#include "en300468ts.table"
#undef ENDEF_BEGIN
#undef ENDEF_END
#undef ENDEF_NUMBER0
#undef ENDEF_NUMBER1
#undef ENDEF_DESCR
#undef ENDEF_DATETIME
#undef ENDEF_LOOP
#undef ENDEF_LOOPEND0
  num_si,
  si_none = -1
};

#define DESCR_FIRST     0x40
#define DESCR_LAST      0x6E
#define PMT_POSS        0

static unsigned char descrcnt[DESCR_LAST-DESCR_FIRST+1];

const static unsigned char possible_descr[DESCR_LAST-DESCR_FIRST+1] = {
  (1<<nit),
  (1<<nit) | (1<<bat),
  0 /* (1<<nit) | (1<<bat) | (1<<sdt) | (1<<eit) | (1<<sit) */,
  (1<<nit),
  (1<<nit),
  PMT_POSS,
  PMT_POSS,
  (1<<bat) | (1<<sdt) | (1<<sit),
  (1<<sdt) | (1<<sit),
  (1<<bat) | (1<<sdt) | (1<<sit),
  (1<<nit) | (1<<bat) | (1<<sdt) | (1<<eit) | (1<<sit),
  (1<<sdt) | (1<<sit),
  (1<<sdt) | (1<<sit),
  (1<<eit) | (1<<sit),
  (1<<eit) | (1<<sit),
  (1<<eit) | (1<<sit),
  (1<<eit) | (1<<sit),
  (1<<sdt) | PMT_POSS | (1<<sit),
  PMT_POSS,
  (1<<bat) | (1<<sdt) | (1<<eit) | (1<<sit),
  (1<<eit) | (1<<sit),
  (1<<eit) | (1<<sit),
  PMT_POSS,
  (1<<sdt) | (1<<eit) | (1<<sit),
  (1<<tot),
  PMT_POSS,
  (1<<nit),
  (1<<nit),
  (1<<bat),
  (1<<sdt) | (1<<sit),
  (1<<eit) | (1<<sit),
  (1<<nit) | (1<<bat) | (1<<sdt) | (1<<eit) | PMT_POSS | (1<<sit),
  PMT_POSS,
  (1<<eit) | (1<<sit),
  (1<<nit),
  (1<<sit),
  (1<<sdt) | (1<<eit) | (1<<sit),
  PMT_POSS,
  PMT_POSS,
  0,
  0,
  (1<<eit),
  PMT_POSS,
  PMT_POSS,
  (1<<nit),
  (1<<nit),
  (1<<nit)
};

#define LOOP_DEPTH      4
#define REALLOC_CHUNK   32

static struct {
  int tablen;
  int itab;
  int fd;
  int isdescr; /* 0: main table, >0: descr loop depth */
  int descrtag;
  int isyn;
  int isyntab;
  int numcount;
  int loopbegin[LOOP_DEPTH];
  int loopcount[LOOP_DEPTH];
  int ibuf;
  char buf[2048];
} tabnew;

struct sitab {
  struct sitab *next;
  unsigned char version;
  unsigned char tableid;
  unsigned short tableid_ext;
  long pid;
  long freqmsec;
  enum enumsi esi;
  unsigned long *tab;
  struct timeval soon;
  unsigned char descrnum[DESCR_LAST-DESCR_FIRST+1];
};

static struct sitab *newtab = NULL;
static struct sitab *runtab = NULL;
static struct sitab *oldtab = NULL;

#define SYNTAX_END      0
#define SYNTAX_LOOPEND  1
#define SYNTAX_LOOP     2
#define SYNTAX_DESCR    3
#define SYNTAX_DESCRTAG 4
#define SYNTAX_DATETIME 5
#define SYNTAX_STRING   6
#define SYNTAX_NUMBER  -1

#define ENDEF_BEGIN(name,tag) \
        const static signed char syntax_##name [] = {SYNTAX_DESCRTAG,
#define ENDEF_END SYNTAX_END};
#define ENDEF_NUMBER0(name) SYNTAX_NUMBER
#define ENDEF_NUMBER1(name) SYNTAX_NUMBER,
#define ENDEF_DATETIME SYNTAX_DATETIME,
#define ENDEF_LOOP SYNTAX_LOOP,
#define ENDEF_LOOPEND0
#define ENDEF_LOOPEND1 SYNTAX_LOOPEND,
#define ENDEF_STRING(name) SYNTAX_STRING,
#define ENDEF_STRING3(name) SYNTAX_STRING,
#include "en300468ts.descr"
#undef ENDEF_BEGIN
#undef ENDEF_END
#undef ENDEF_NUMBER0
#undef ENDEF_NUMBER1
#undef ENDEF_DATETIME
#undef ENDEF_LOOP
#undef ENDEF_LOOPEND0
#undef ENDEF_LOOPEND1
#undef ENDEF_STRING
#undef ENDEF_STRING3

static unsigned char *gen_network_name(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_service_list(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  while (i > 0) {
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_satellite_delivery_system(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  *d++ = *t >> 24;
  *d++ = *t >> 16;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = *t >> 8;
  *d++ = *t++;
  c = *t++ << 7;
  c |= (*t++ & 0x03) << 5;
  *d++ = c | (*t++ & 0x1F);
  *d++ = *t >> 20;
  *d++ = *t >> 12;
  *d++ = *t >> 4;
  c = *t++ << 4;
  *d++ = c | (*t++ & 0x0F);
  *tt = t;
  return d;
}

static unsigned char *gen_cable_delivery_system(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  *d++ = *t >> 24;
  *d++ = *t >> 16;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = 0xFF;
  *d++ = 0xF0 | *t++;
  *d++ = *t++;
  *d++ = *t >> 20;
  *d++ = *t >> 12;
  *d++ = *t >> 4;
  c = *t++ << 4;
  *d++ = c | (*t++ & 0x0F);
  *tt = t;
  return d;
}

static unsigned char *gen_bouquet_name(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_service(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  *d++ = *t++;
  *d++ = i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *d++ = i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_country_availability(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  *d++ = (*t++ << 7) | 0x7F;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_linkage(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  unsigned char l, h, o;
  unsigned long n, s;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = l = *t++;
  h = *t++ & 0x0F;
  o = *t++ & 0x01;
  n = *t++;
  s = *t++;
  if (l == 8) {
    *d++ = (h << 4) | 0x0E | o;
    if ((h >= 1) && (h <= 3)) {
      *d++ = n >> 8;
      *d++ = n;
    }
    if (o == 0) {
      *d++ = s >> 8;
      *d++ = s;
    }
  }
  i = *t++;
  while (i > 0) {
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_nvod_reference(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  while (i > 0) {
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 8;
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_time_shifted_service(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  *d++ = *t >> 8;
  *d++ = *t++;
  *tt = t;
  return d;
}

static unsigned char *gen_short_event(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  memcpy(d, t, 3);
  d += 3;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *d++ = i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *d++ = i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_extended_event(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  *d++ = (descrcnt[0x4E - DESCR_FIRST] << 4)
       | (st->descrnum[0x4E - DESCR_FIRST] & 0x0F);
  i = *t++;
  memcpy(d, t, 3);
  d += 4;
  t += (i + sizeof(long) - 1) / sizeof(long);
  i = *t++;
  while (i > 0) {
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    i -= 1;
  }
  p[4] = d-p-5;
  *d++ = i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_time_shifted_event(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = *t >> 8;
  *d++ = *t++;
  *tt = t;
  return d;
}

static unsigned char *gen_component(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  *d++ = 0xF0 | *t++;
  *d++ = *t++;
  *d++ = *t++;
  i = *t++;
  memcpy(d, t, 3);
  d += 3;
  t += (i + sizeof(long) - 1) / sizeof(long);
  i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_mosaic(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char *e;
  unsigned char c;
  int i, j;
  c = *t++ << 7;
  c = c | ((*t++ & 0x07) << 4);
  *d++ = c | 0x08 | (*t++ & 0x07);
  i = *t++;
  while (i > 0) {
    *d++ = (*t++ << 2) | 0x03;
    *d++ = 0xF8 | *t++;
    e = d++;
    j = *t++;
    while (j > 0) {
      *d++ = 0xC0 | *t++;
      j -= 1;
    }
    *e = d-e-1;
    switch (*d++ = *t++) {
      case 0x01:
        *d++ = t[0] >> 8;
        *d++ = t[0];
        break;
      case 0x02:
      case 0x03:
        *d++ = t[1] >> 8;
        *d++ = t[1];
        *d++ = t[2] >> 8;
        *d++ = t[2];
        *d++ = t[3] >> 8;
        *d++ = t[3];
        break;
      case 0x04:
        *d++ = t[1] >> 8;
        *d++ = t[1];
        *d++ = t[2] >> 8;
        *d++ = t[2];
        *d++ = t[3] >> 8;
        *d++ = t[3];
        *d++ = t[4] >> 8;
        *d++ = t[4];
        break;
    }
    t += 5;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_ca_identifier(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  while (i > 0) {
    *d++ = *t >> 8;
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_content(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  i = *t++;
  while (i > 0) {
    *d++ = *t++;
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_parental_rating(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_telephone(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  int i;
  c = *t++ << 5;
  *d = 0xC0 | c | (*t++ & 0x1F);
  d += 3;
  i = *t++;
  c = i << 5;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  i = *t++;
  c |= (i & 0x07) << 2;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  i = *t++;
  p[1] = 0x80 | c | (i & 0x03);
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  i = *t++;
  c = i << 4;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  i = *t++;
  p[2] = 0x80 | c | (i & 0x0F);
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_local_time_offset(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  int i, j;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    c = *t++ << 2;
    *d++ = c | 0x02 | (*t++ & 0x01);
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 16;
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 8;
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_terrestrial_delivery_system(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  *d++ = *t >> 24;
  *d++ = *t >> 16;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = (*t++ << 5) | 0x1F;
  c = *t++ << 6;
  c |= (*t++ & 0x07) << 3;
  *d++ = c | (*t++ & 0x07);
  c = *t++ << 5;
  c |= (*t++ & 0x03) << 3;
  c |= (*t++ & 0x03) << 1;
  *d++ = c | (*t++ & 0x01);
  *d++ = 0xFF;
  *d++ = 0xFF;
  *d++ = 0xFF;
  *d++ = 0xFF;
  *tt = t;
  return d;
}

static unsigned char *gen_multilingual_network_name(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_multilingual_bouquet_name(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_multilingual_service_name(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_multilingual_component(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  *d++ = *t++;
  i = *t++;
  while (i > 0) {
    j = *t++;
    memcpy(d, t, 3);
    d += 3;
    t += (j + sizeof(long) - 1) / sizeof(long);
    *d++ = j = *t++;
    memcpy(d, t, j);
    d += j;
    t += (j + sizeof(long) - 1) / sizeof(long);
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_private_data_specifier(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  *d++ = *t >> 24;
  *d++ = *t >> 16;
  *d++ = *t >> 8;
  *d++ = *t++;
  *tt = t;
  return d;
}

static unsigned char *gen_short_smoothing_buffer(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  c = *t++ << 6;
  *d++ = c | (*t++ & 0x3F);
  *tt = t;
  return d;
}

static unsigned char *gen_frequency_list(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  *d++ = 0xFC | *t++;
  i = *t++;
  while (i > 0) {
    *d++ = *t >> 24;
    *d++ = *t >> 16;
    *d++ = *t >> 8;
    *d++ = *t++;
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_partial_transport_stream(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  *d++ = 0xC0 | (*t >> 16);
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = 0xC0 | (*t >> 16);
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = 0xC0 | (*t >> 8);
  *d++ = *t++;
  *tt = t;
  return d;
}

static unsigned char *gen_data_broadcast(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i;
  *d++ = *t >> 8;
  *d++ = *t++;
  *d++ = *t++;
  *d++ = i = *t++;
  while (i > 0) {
    *d++ = *t++;
    i -= 1;
  }
  i = *t++;
  memcpy(d, t, 3);
  d += 3;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *d++ = i = *t++;
  memcpy(d, t, i);
  d += i;
  t += (i + sizeof(long) - 1) / sizeof(long);
  *tt = t;
  return d;
}

static unsigned char *gen_pdc(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  *d++ = 0xF0 | (*t >> 16);
  *d++ = *t >> 8;
  *d++ = *t++;
  *tt = t;
  return d;
}

static unsigned char *gen_cell_list(struct sitab *st, unsigned char *p,
        unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c;
  int i, j;
  i = *t++;
  while (i > 0) {
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 4;
    c = *t++ << 4;
    *d++ = c | ((*t >> 8) & 0x0F);
    *d++ = *t++;
    j = *t++;
    *d++ = 8 * j;
    while (j > 0) {
      *d++ = *t++;
      *d++ = *t >> 8;
      *d++ = *t++;
      *d++ = *t >> 8;
      *d++ = *t++;
      *d++ = *t >> 4;
      c = *t++ << 4;
      *d++ = c | ((*t >> 8) & 0x0F);
      *d++ = *t++;
      j -= 1;
    }
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_cell_frequency_link(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  int i, j;
  i = *t++;
  while (i > 0) {
    *d++ = *t >> 8;
    *d++ = *t++;
    *d++ = *t >> 24;
    *d++ = *t >> 16;
    *d++ = *t >> 8;
    *d++ = *t++;
    j = *t++;
    *d++ = 5 * j;
    while (j > 0) {
      *d++ = *t++;
      *d++ = *t >> 24;
      *d++ = *t >> 16;
      *d++ = *t >> 8;
      *d++ = *t++;
      j -= 1;
    }
    i -= 1;
  }
  *tt = t;
  return d;
}

static unsigned char *gen_announcement_support(struct sitab *st,
        unsigned char *p, unsigned long **tt)
{
  unsigned char *d = p;
  unsigned long *t = *tt;
  unsigned char c, v;
  int i;
  *d++ = *t >> 8;
  *d++ = *t++;
  i = *t++;
  while (i > 0) {
    c = *t++ << 4;
    v = *t++ & 0x07;
    *d++ = c | 0x08 | v;
    switch (v) {
      case 0x01:
      case 0x02:
      case 0x03:
        *d++ = *t >> 8;
        *d++ = *t++;
        *d++ = *t >> 8;
        *d++ = *t++;
        *d++ = *t >> 8;
        *d++ = *t++;
        *d++ = *t++;
        break;
    }
    i -= 1;
  }
  *tt = t;
  return d;
}

const static signed char *const descr_syntax[DESCR_LAST-DESCR_FIRST+1] = {
  &syntax_network_name[0],
  &syntax_service_list[0],
  NULL,
  &syntax_satellite_delivery_system[0],
  &syntax_cable_delivery_system[0],
  NULL,
  NULL,
  &syntax_bouquet_name[0],
  &syntax_service[0],
  &syntax_country_availability[0],
  &syntax_linkage[0],
  &syntax_nvod_reference[0],
  &syntax_time_shifted_service[0],
  &syntax_short_event[0],
  &syntax_extended_event[0],
  &syntax_time_shifted_event[0],
  &syntax_component[0],
  &syntax_mosaic[0],
  NULL,
  &syntax_ca_identifier[0],
  &syntax_content[0],
  &syntax_parental_rating[0],
  NULL,
  &syntax_telephone[0],
  &syntax_local_time_offset[0],
  NULL,
  &syntax_terrestrial_delivery_system[0],
  &syntax_multilingual_network_name[0],
  &syntax_multilingual_bouquet_name[0],
  &syntax_multilingual_service_name[0],
  &syntax_multilingual_component[0],
  &syntax_private_data_specifier[0],
  NULL,
  &syntax_short_smoothing_buffer[0],
  &syntax_frequency_list[0],
  &syntax_partial_transport_stream[0],
  &syntax_data_broadcast[0],
  NULL,
  NULL,
  NULL,
  NULL,
  &syntax_pdc[0],
  NULL,
  NULL,
  &syntax_cell_list[0],
  &syntax_cell_frequency_link[0],
  &syntax_announcement_support[0]
};

static unsigned char *gendescr(struct sitab *st, unsigned char *p,
        unsigned long **tt, unsigned char msb)
{
  unsigned char *d = p+2;
  unsigned long *t = *tt;
  unsigned char *b;
  unsigned char v;
  int i;
  i = *t++;
  while (i > 0) {
    b = d+2;
    switch (*d = v = *t++) {
#define ENDEF_BEGIN(name,tag) case tag : d = gen_##name (st, b, &t); break;
#define ENDEF_END
#define ENDEF_NUMBER0(name)
#define ENDEF_NUMBER1(name)
#define ENDEF_DATETIME
#define ENDEF_LOOP
#define ENDEF_LOOPEND0
#define ENDEF_LOOPEND1
#define ENDEF_STRING(name)
#define ENDEF_STRING3(name)
#include "en300468ts.descr"
#undef ENDEF_BEGIN
#undef ENDEF_END
#undef ENDEF_NUMBER0
#undef ENDEF_NUMBER1
#undef ENDEF_DATETIME
#undef ENDEF_LOOP
#undef ENDEF_LOOPEND0
#undef ENDEF_LOOPEND1
#undef ENDEF_STRING
#undef ENDEF_STRING3
      default:
        fprintf(stderr, "error: descr not implemented (%02x)\n", *d);
        exit(1);
    }
    descrcnt[v-DESCR_FIRST] += 1;
    b[-1] = d-b;
    i -= 1;
  }
  i = d-p-2;
  p[0] = msb | (i >> 8);
  p[1] = i;
  *tt = t;
  return d;
}

#define ENDEF_BEGIN(name,pid,tableid) \
        const static signed char syntax_##name [] = {
#define ENDEF_END SYNTAX_END};
#define ENDEF_NUMBER0(name) SYNTAX_NUMBER
#define ENDEF_NUMBER1(name) SYNTAX_NUMBER,
#define ENDEF_DESCR SYNTAX_DESCR,
#define ENDEF_DATETIME SYNTAX_DATETIME,
#define ENDEF_LOOP SYNTAX_LOOP,
#define ENDEF_LOOPEND0
#include "en300468ts.table"
#undef ENDEF_BEGIN
#undef ENDEF_END
#undef ENDEF_NUMBER0
#undef ENDEF_NUMBER1
#undef ENDEF_DESCR
#undef ENDEF_DATETIME
#undef ENDEF_LOOP
#undef ENDEF_LOOPEND0

static int gentab_nit(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  unsigned char *q;
  int i;
  *p = st->tableid;
  p += 3;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = 0xC0 | (st->version << 1) | 0x01;
  *p++ = 0;
  *p++ = 0;
  p = gendescr(st, p, &t, 0xF0);
  q = p;
  p += 2;
  i = *t++;
  while (i > 0) {
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 8;
    *p++ = *t++;
    p = gendescr(st, p, &t, 0xF0);
    i -= 1;
  }
  i = p-q-2;
  q[0] = 0xF0 | (i >> 8);
  q[1] = i;
  i = p-b+1;
  b[1] = 0xF0 | (i >> 8);
  b[2] = i;
  crc32_calc(b, i-1, p);
  return p-b+4;
}

static int gentab_sdt(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  int i;
  unsigned char c;
  *p = st->tableid;
  p += 3;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = 0xC0 | (st->version << 1) | 0x01;
  *p++ = 0;
  *p++ = 0;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = 0xFF;
  i = *t++;
  while (i > 0) {
    *p++ = *t >> 8;
    *p++ = *t++;
    c = *t++ << 1;
    *p++ = 0xFC | c | (*t++ & 1);
    c = *t++ << 5;
    p = gendescr(st, p, &t, c);
    i -= 1;
  }
  i = p-b+1;
  b[1] = 0xF0 | (i >> 8);
  b[2] = i;
  crc32_calc(b, i-1, p);
  return p-b+4;
}

static int gentab_bat(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  unsigned char *q;
  int i;
  *p = st->tableid;
  p += 3;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = 0xC0 | (st->version << 1) | 0x01;
  *p++ = 0;
  *p++ = 0;
  p = gendescr(st, p, &t, 0xF0);
  q = p;
  p += 2;
  i = *t++;
  while (i > 0) {
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 8;
    *p++ = *t++;
    p = gendescr(st, p, &t, 0xF0);
    i -= 1;
  }
  i = p-q-2;
  q[0] = 0xF0 | (i >> 8);
  q[1] = i;
  i = p-b+1;
  b[1] = 0xF0 | (i >> 8);
  b[2] = i;
  crc32_calc(b, i-1, p);
  return p-b+4;
}

static int gentab_eit(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  int i;
  unsigned char c;
  struct sitab *gt;
  *p = i = st->tableid;
  p += 3;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = 0xC0 | (st->version << 1) | 0x01;
  *p++ = 0;
  *p++ = 0;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = *t >> 8;
  *p++ = *t++;
  *p++ = 0;
  gt = runtab;
  while (gt != NULL) {
    if ((gt->esi == eit) && (gt->tableid > i)) {
      i = gt->tableid;
    }
    gt = gt->next;
  }
  *p++ = i;
  i = *t++;
  while (i > 0) {
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 16;
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 16;
    *p++ = *t >> 8;
    *p++ = *t++;
    c = *t++ << 5;
    p = gendescr(st, p, &t, c);
    i -= 1;
  }
  i = p-b+1;
  b[1] = 0xF0 | (i >> 8);
  b[2] = i;
  crc32_calc(b, i-1, p);
  return p-b+4;
}

static int gentab_rst(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  int i;
  *p = st->tableid;
  p += 3;
  i = *t++;
  while (i > 0) {
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = *t >> 8;
    *p++ = *t++;
    *p++ = 0xF8 | *t++;
    i -= 1;
  }
  i = p-b-3;
  b[1] = 0x70 | (i >> 8);
  b[2] = i;
  return p-b;
}

static unsigned char *gentvdatetime(unsigned char *p, struct timeval *tv)
{
  unsigned long d, s;
  s = tv->tv_sec;
  d = s / 86400;
  s = s % 86400;
  d += 40587;
  *p++ = d >> 8;
  *p++ = d;
  d = s / 3600;
  s = s % 3600;
  d += (d / 10) * 6;
  *p++ = d;
  d = s / 60;
  s = s % 60;
  d += (d / 10) * 6;
  *p++ = d;
  s += (s / 10) * 6;
  *p++ = s;
  return p;
}

static int gentab_tdt(struct sitab *st, unsigned char *b, struct timeval *tv)
{
  unsigned char *p = b;
  *p++ = st->tableid;
  *p++ = 0x70;
  *p++ = 0x05;
  p = gentvdatetime(p, tv);
  return p-b;
}

static int gentab_tot(struct sitab *st, unsigned char *b, struct timeval *tv)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  int i;
  *p = st->tableid;
  p += 3;
  p = gentvdatetime(p, tv);
  p = gendescr(st, p, &t, 0xF0);
  i = p-b+1;
  b[1] = 0x70 | (i >> 8);
  b[2] = i;
  crc32_calc(b, i-1, p);
  return p-b+4;
}

static int gentab_sit(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  unsigned char c;
  int i;
  *p = st->tableid;
  p += 3;
  *p++ = 0xFF;
  *p++ = 0xFF;
  *p++ = 0xC0 | (st->version << 1) | 0x01;
  *p++ = 0;
  *p++ = 0;
  p = gendescr(st, p, &t, 0xF0);
  i = *t++;
  while (i > 0) {
    *p++ = *t >> 8;
    *p++ = *t++;
    c = *t++ << 4;
    p = gendescr(st, p, &t, 0x80 | c);
    i -= 1;
  }
  i = p-b+1;
  b[1] = 0xF0 | (i >> 8);
  b[2] = i;
  crc32_calc(b, i-1, p);
  return p-b+4;
}

static int gentab_dit(struct sitab *st, unsigned char *b)
{
  unsigned char *p = b;
  unsigned long *t = st->tab;
  *p++ = st->tableid;
  *p++ = 0x70;
  *p++ = 0x01;
  *p++ = (*t++ << 7) | 0x7F;
  return p-b;
}

const static signed char *const tab_syntax[num_si] = {
#define ENDEF_BEGIN(name,pid,tableid) &syntax_##name [0],
#define ENDEF_END
#define ENDEF_NUMBER0(name)
#define ENDEF_NUMBER1(name)
#define ENDEF_DESCR
#define ENDEF_DATETIME
#define ENDEF_LOOP
#define ENDEF_LOOPEND0
#include "en300468ts.table"
#undef ENDEF_BEGIN
#undef ENDEF_END
#undef ENDEF_NUMBER0
#undef ENDEF_NUMBER1
#undef ENDEF_DESCR
#undef ENDEF_DATETIME
#undef ENDEF_LOOP
#undef ENDEF_LOOPEND0
};

const static signed char *const *const syntax[2] = {
  &tab_syntax[0],
  &descr_syntax[0]
};

static void gentab(struct sitab *st, struct timeval *tv)
{
  int i, l;
  unsigned char *b;
  memset(&descrcnt[0], 0, sizeof(descrcnt));
  i = st->pid - TABLE_PID_FIRST;
  b = &perpid[i].tabbuf[perpid[i].tabin];
  *b++ = st->pid >> 8;
  *b++ = st->pid;
  switch (st->esi) {
    case nit: l = gentab_nit(st, b); break;
    case sdt: l = gentab_sdt(st, b); break;
    case bat: l = gentab_bat(st, b); break;
    case eit: l = gentab_eit(st, b); break;
    case rst: l = gentab_rst(st, b); break;
    case tdt: l = gentab_tdt(st, b, tv); break;
    case tot: l = gentab_tot(st, b, tv); break;
    case sit: l = gentab_sit(st, b); break;
    case dit: l = gentab_dit(st, b); break;
    default:
      fprintf(stderr, "internal error (gentab, %d)\n", st->esi);
      exit(1);
  }
  perpid[i].tabin += l+2;
}

static enum enumsi alloctab(long pid, long tid, int *hastableidext)
{
  enum enumsi e = si_none;
  switch (pid) {
    case 0x0010:
      if (tid == 0x40) {
        e = nit;
      } else if (tid == 0x41) {
        e = nit;
        *hastableidext = 1;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    case 0x0011:
      if (tid == 0x42) {
        e = sdt;
      } else if (tid == 0x46) {
        e = sdt;
        *hastableidext = 1;
      } else if (tid == 0x4A) {
        e = bat;
        *hastableidext = 1;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    case 0x0012:
      if ((tid >= 0x4E) && (tid <= 0x6F)) {
        e = eit;
        *hastableidext = 1;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    case 0x0013:
      if (tid == 0x71) {
        e = rst;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    case 0x0014:
      if (tid == 0x70) {
        e = tdt;
      } else if (tid == 0x73) {
        e = tot;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    case 0x001E:
      if (tid == 0x7E) {
        e = dit;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    case 0x001F:
      if (tid == 0x7F) {
        e = sit;
      } else {
        fprintf(stderr, "bad tableid: 0x%02lx (pid: 0x%04lx)\n", tid, pid);
      }
      break;
    default:
      fprintf(stderr, "bad pid: 0x%04lx\n", pid);
      break;
  }
  return e;
}

static unsigned char nextversion(struct sitab *nt)
{
  struct sitab **pst;
  struct sitab *st;
  pst = &oldtab;
  while ((st = *pst)) {
    if ((st->pid == nt->pid) && (st->tableid == nt->tableid)
     && (st->tableid_ext == nt->tableid_ext)) {
      unsigned char v = st->version + 1;
      *pst = st->next;
      free(st->tab);
      free(st);
      return v;
    } else {
      pst = &st->next;
    }
  }
  return 0;
}

static void droptab(long pid, long tableid, long tableid_ext)
{
  struct sitab **pst;
  struct sitab *st;
  pst = &runtab;
  while ((st = *pst)) {
    if ((st->pid == pid) && (st->tableid == tableid)
     && ((tableid_ext < 0) || (st->tableid_ext == tableid_ext))) {
      *pst = st->next;
      st->next = oldtab;
      oldtab = st;
    } else {
      pst = &st->next;
    }
  }
}

static void maketab(long pid, long tableid, long freqmsec, int fd)
{
  struct sitab *t;
  enum enumsi e;
  int hastableidext = 0;
  e = alloctab(pid, tableid, &hastableidext);
  if (e >= 0) {
    t = malloc(sizeof(struct sitab));
    if (t != NULL) {
      t->pid = pid;
      t->tableid = tableid;
      t->tableid_ext = hastableidext;
      t->freqmsec = freqmsec;
      t->esi = e;
      t->tab = NULL;
      memset(&t->descrnum[0], 0, sizeof(t->descrnum));
      memset(&tabnew, 0, sizeof(tabnew));
      tabnew.fd = fd;
      newtab = t;
    } else {
      close(fd);
    }
  } else {
    close(fd);
  }
}

static int tabline(char *b, int n)
{
  char *e, *a, *z;
  int i;
  long v[3];
  char c;
  if ((n <= 0) || ((e = memchr(b, '\n', n)) == NULL)) {
    return 0;
  }
  *e = 0;
  if (b < e) {
    z = b+1;
    i = 0;
    c = toupper(*b);
    switch (c) {
      case '-':
        do {
          a = z;
          v[i] = strtol(a, &z, 0);
        } while ((a != z) && (++i < 2));
        if (a == z) {
          fprintf(stderr, "invalid line(%d): %s\n", a-b, b);
        } else {
          droptab(v[0], v[1], -1);
        }
        break;
      case 'S':
        do {
          a = z;
          v[i] = strtol(a, &z, 0);
        } while ((a != z) && (++i < 3));
        if (a == z) {
          fprintf(stderr, "invalid line(%d): %s\n", a-b, b);
        } else {
          while (isspace(*z)) {
            z += 1;
          }
          i = open(z, O_RDONLY | O_NONBLOCK);
          if (i < 0) {
            fprintf(stderr, "open failed(%d): %s\n", errno, z);
          } else {
#ifdef DEBUG
            fprintf(stderr, "table: %c, %ld, %ld, %ld, <%s>\n",
                c, v[0], v[1], v[2], z);
#endif
            maketab(v[0], v[1], v[2], i);
          }
        }
        break;
    }
  }
  return e-b+1;
}

static int taballoc(struct sitab *st, int cnt)
{
  while (tabnew.tablen < (tabnew.itab + cnt)) {
    tabnew.tablen += REALLOC_CHUNK;
    st->tab = realloc(st->tab, tabnew.tablen * sizeof(unsigned long));
    if (st->tab == NULL) {
      return -ENOMEM;
    }
  }
  return 0;
}

static long longval(char **n, int d)
{
  long v;
  int i;
  char c;
  v = 0;
  for (i = 0; i < d; i++) {
    c = *(*n)++;
    if (!isdigit(c)) {
      return -ENOMEM;
    }
    v = 10*v+c-'0';
  }
  return v;
}

static int siline(struct sitab *st, char *b, int n)
{
  char *e, *a, *z;
  long v;
  int s, i;
  s = syntax[(tabnew.isdescr > 0) ? 1 : 0]
    [(tabnew.isdescr > 0) ? tabnew.descrtag : st->esi][tabnew.isyn];
  if ((s == SYNTAX_END) || (s == SYNTAX_LOOPEND)) {
    int lc;
#ifdef DEBUG
    fprintf(stderr, "end:\n");
#endif
    if ((lc = --tabnew.loopcount[0]) <= 0) {
      if (lc == 0) {
        for (i = 0; i < LOOP_DEPTH-1; i++) {
          tabnew.loopbegin[i] = tabnew.loopbegin[i+1];
          tabnew.loopcount[i] = tabnew.loopcount[i+1];
        }
        tabnew.loopcount[LOOP_DEPTH-1] = 0;
        if (s == SYNTAX_LOOPEND) {
          tabnew.isyn += 1;
        } else if (tabnew.isdescr > 0) {
          if (--tabnew.isdescr == 0) {
            tabnew.isyn = tabnew.isyntab;
          }
        }
        return 0;
      } else {
        tabnew.tablen = tabnew.itab;
        st->tab = realloc(st->tab, tabnew.itab * sizeof(unsigned long));
        return -ENOBUFS;
      }
    } else {
      tabnew.isyn = tabnew.loopbegin[0];
      return 0;
    }
  }
  if ((n <= 0) || ((e = memchr(b, '\n', n)) == NULL)) {
    return -EAGAIN;
  }
  *e = 0;
  a = b;
  while (isspace(*a)) {
    a += 1;
  }
  if (taballoc(st, 1)) {
    return -ENOMEM;
  }
  if (a != e) {
    switch (s) {
      default:
        if (s > 0) {
          fprintf(stderr, "internal syntax error\n");
          exit(1);
        }
        if (tabnew.numcount == 0) {
          tabnew.numcount = s;
        }
        v = strtoul(a, &z, 0);
        if (a == z) {
          return -EINVAL;
        }
#ifdef DEBUG
        fprintf(stderr, "number: %ld, %d..%d\n", v, a-b, z-b);
#endif
        st->tab[tabnew.itab++] = v;
        if (++tabnew.numcount == 0) {
          tabnew.isyn += 1;
        }
        *e = '\n';
        return z-b;
      case SYNTAX_LOOP:
        v = strtol(a, &z, 0);
        if (a == z) {
          return -EINVAL;
        }
#ifdef DEBUG
        fprintf(stderr, "loop: %ld, %d..%d\n", v, a-b, z-b);
#endif
        st->tab[tabnew.itab++] = v;
        if (v != 0) {
          if (tabnew.isdescr > 0) {
            tabnew.isdescr += 1;
          }
          for (i = LOOP_DEPTH-2; i >= 0; i--) {
            tabnew.loopbegin[i+1] = tabnew.loopbegin[i];
            tabnew.loopcount[i+1] = tabnew.loopcount[i];
          }
          tabnew.loopbegin[0] = ++tabnew.isyn;
          tabnew.loopcount[0] = v;
        } else {
          do {
            tabnew.isyn += 1;
            s = syntax[(tabnew.isdescr > 0) ? 1 : 0]
                      [(tabnew.isdescr > 0) ? tabnew.descrtag : st->esi]
                      [tabnew.isyn];
            if (s == SYNTAX_LOOP) {
              v += 1;
            }
          } while ((s != SYNTAX_END) && ((s != SYNTAX_LOOPEND) || (--v >= 0)));
          if (s == SYNTAX_LOOPEND) {
            tabnew.isyn += 1;
          }
        }
        *e = '\n';
        return z-b;
      case SYNTAX_DESCR:
        v = strtol(a, &z, 0);
        if (a == z) {
          return -EINVAL;
        }
#ifdef DEBUG
        fprintf(stderr, "descr: %ld, %d..%d\n", v, a-b, z-b);
#endif
        st->tab[tabnew.itab++] = v;
        tabnew.isyn += 1;
        if (v != 0) {
          tabnew.isdescr = 1;
          tabnew.descrtag = 0;
          for (i = LOOP_DEPTH-2; i >= 0; i--) {
            tabnew.loopbegin[i+1] = tabnew.loopbegin[i];
            tabnew.loopcount[i+1] = tabnew.loopcount[i];
          }
          tabnew.loopbegin[0] = 0;
          tabnew.loopcount[0] = v;
          tabnew.isyntab = tabnew.isyn;
          tabnew.isyn = 0;
        }
        *e = '\n';
        return z-b;
      case SYNTAX_DESCRTAG:
        v = strtol(a, &z, 0);
        if (a == z) {
          return -EINVAL;
        }
#ifdef DEBUG
        fprintf(stderr, "descrtag: %ld, %d..%d\n", v, a-b, z-b);
#endif
        st->tab[tabnew.itab++] = v;
        v -= DESCR_FIRST;
        if ((v < 0) || (v > (DESCR_LAST - DESCR_FIRST))) {
          return -EINVAL;
        }
        st->descrnum[v] += 1;
        if (!((1 << st->esi) & possible_descr[v])) {
          return -EINVAL;
        }
        tabnew.descrtag = v;
        tabnew.isyn += 1;
        *e = '\n';
        return z-b;
      case SYNTAX_DATETIME:
        /* yyyy/mm/dd hh:mm:ss */
        z = a;
        i = e-z;
        if (i < 19) {
          return -EINVAL;
        }
        if (taballoc(st, 2)) {
          return -ENOMEM;
        }
        if ((v = longval(&z, 4)) < 0) {
          return v;
        }
        if ((v < 1582) || (*z++ != '/')) {
          return -EINVAL;
        }
        if ((s = longval(&z, 2)) < 0) {
          return s;
        }
        s -= 1;
        if ((s < 0) || (s > 11) || (*z++ != '/')) {
          return -EINVAL;
        }
        s -= 2;
        if (s < 0) {
          s += 12;
          v -= 1;
        }
        v = (1461*v)/4 - ((v/100+1)*3)/4 + (153*s+2)/5;
        if ((s = longval(&z, 2)) < 0) {
          return s;
        }
        if ((s <= 0) || (s > 31)) {
          return -EINVAL;
        }
        v += s - 678882;
        st->tab[tabnew.itab++] = v;
        v = 0;
        s = ' ';
        for (i = 2; i >= 0; i--) {
          if (*z++ != s) {
            return -EINVAL;
          }
          s = *z++;
          if (!isdigit(s)) {
            return -EINVAL;
          }
          v = (v<<4) + (s-'0');
          s = *z++;
          if (!isdigit(s)) {
            return -EINVAL;
          }
          v = (v<<4) + (s-'0');
          s = ':';
        }
        st->tab[tabnew.itab++] = v;
#ifdef DEBUG
        fprintf(stderr, "datetime: %04lx %06lx, %d..%d\n",
                st->tab[tabnew.itab-2], v, a-b, z-b);
#endif
        tabnew.isyn += 1;
        *e = '\n';
        return z-b;
      case SYNTAX_STRING:
        if (*a++ != '"') {
          return -EINVAL;
        }
        v = 0;
        z = strchr(a, '"');
        if (z == NULL) {
          return -EINVAL;
        }
        i = v;
        v += z-a;
        taballoc(st, 1 + (v + sizeof(long) - 1) / sizeof(long));
        memcpy(((char *)&st->tab[tabnew.itab+1]) + i, a, z-a);
        z += 1;
        while (*z == '"') {
          a = z;
          z = strchr(a+1, '"');
          if (z == NULL) {
            return -EINVAL;
          }
          i = v;
          v += z-a;
          taballoc(st, 1 + (v + sizeof(long) - 1) / sizeof(long));
          memcpy(((char *)&st->tab[tabnew.itab+1]) + i, a, z-a);
          z += 1;
        }
        st->tab[tabnew.itab] = v;
#ifdef DEBUG
        fprintf(stderr, "string: %ld, %d..%d\n", v, a-b, z-b);
#endif
        tabnew.itab += 1 + (v + sizeof(long) - 1) / sizeof(long);
        tabnew.isyn += 1;
        *e = '\n';
        return z-b;
    }
  } else {
    return e-b+1;
  }
}

static unsigned int tab2ts(unsigned char *t, unsigned char *conticnt)
{
  unsigned int l, d;
  unsigned char *i = &t[2];
  unsigned char *o;
  unsigned char c = *conticnt;
  l = ((t[3] & 0x0F) << 8) + t[4] + TS_HEADSLEN + TS_PFIELDLEN;
  d = (l-1) % (TS_PACKET_SIZE - TS_PACKET_HEADSIZE) + 1;
  if (outin >= OUTBUF_SIZE) {
    outin = 0;
  }
#ifdef DEBUG
  fprintf(stderr, "tab2ts(%02x,%02x,%02x,%02x,%02x; %2d), l=%d, d=%d, o:%d\n",
    t[0], t[1], t[2], t[3], t[4], c, l, d, outin);
#endif
  o = &outbuf[outin];
  if (d <= (TS_PACKET_SIZE - TS_PACKET_HEADSIZE - 1)) {
    if (d < (TS_PACKET_SIZE - TS_PACKET_HEADSIZE - 1)) {
      o[5] = 0; /* no indicators, no flags, padding: */
      memset(&o[6], -1, TS_PACKET_SIZE - TS_PACKET_HEADSIZE - 2 - d);
    }
    o[4] = TS_PACKET_SIZE - TS_PACKET_HEADSIZE - 1 - d;
    o[3] = (0x00 << 6) | (0x03 << 4) | c;
  } else {
    o[3] = (0x00 << 6) | (0x01 << 4) | c;
  }
  o[TS_PACKET_SIZE - d] = 0; /* pointer_field */
  d -= TS_PFIELDLEN;
  memcpy(&o[TS_PACKET_SIZE - d], i, d);
  i += d;
  d = l - d - TS_PFIELDLEN;
  o[1] = (0 << 7) | (1 << 6) | (0 << 5) | t[0];
  o[2] = t[1];
  o[0] = TS_SYNC_BYTE;
  c = (c + 1) & 0x0F;
  outin += TS_PACKET_SIZE;
  while (d > 0) {
    if (outin >= OUTBUF_SIZE) {
      outin = 0;
    }
    o = &outbuf[outin];
    o[3] = (0x00 << 6) | (0x01 << 4) | c;
    memcpy(&o[4], i, TS_PACKET_SIZE-TS_PACKET_HEADSIZE);
    i += (TS_PACKET_SIZE - TS_PACKET_HEADSIZE);
    d -= (TS_PACKET_SIZE - TS_PACKET_HEADSIZE);
    o[1] = (0 << 7) | (0 << 6) | (0 << 5) | t[0];
    o[2] = t[1];
    o[0] = TS_SYNC_BYTE;
    c = (c + 1) & 0x0F;
    outin += TS_PACKET_SIZE;
  }
  *conticnt = c;
  return l+1;
}

static void argloop(int f0)
{
  int i0 = 0;
  int o0 = 0;
  char buf0[PATH_MAX];
  do {
    int i, n, r, n0, n1, nst, tmo;
    struct timeval tv;
    struct sitab *st;
    struct sitab **pst;
    pollfd_init();
    tmo = -1;
    n0 = -1;
    nst = -1;
    if (newtab != NULL) {
      nst = pollfd_add(tabnew.fd, POLLIN);
    } else if ((r = tabline(&buf0[o0], i0 - o0))) {
      o0 += r;
      tmo = 0;
    } else {
      if ((o0 > 0) && (i0 > o0)) {
        memmove(&buf0[0], &buf0[o0], i0 - o0);
      }
      i0 -= o0;
      o0 = 0;
      if (i0 == sizeof(buf0)-1) {
        buf0[sizeof(buf0)-1] = '\n';
        i0 += 1;
        tmo = 0;
      } else if (f0 >= 0) {
        n0 = pollfd_add(f0, POLLIN);
      }
    }
    if (outin == 0) {
      r = OUTBUF_SIZE;
      n1 = -1;
    } else {
      r = outout - outin;
      if (r < 0) {
        r += OUTBUF_SIZE;
      }
      n1 = pollfd_add(STDOUT_FILENO, POLLOUT);
    }
    i = 0;
    while (tmo != 0 && i <= TABLE_PID_LAST-TABLE_PID_FIRST) {
      if ((perpid[i].tabin > perpid[i].tabout)
       && (r >= (((((perpid[i].tabbuf[perpid[i].tabout+3] & 0x0F) << 8)
                   + perpid[i].tabbuf[perpid[i].tabout+4] + TS_HEADSLEN
                   + TS_PFIELDLEN + TS_PACKET_SIZE - TS_PACKET_HEADSIZE)
                  * 131) / 128))) {
        tmo = 0;
#ifdef DEBUG
        {
          int x;
          fprintf(stderr, "tabbuf[%d..%d-1]:\n",
                  perpid[i].tabout, perpid[i].tabin);
          for (x = perpid[i].tabout; x < perpid[i].tabin; x++) {
            fprintf(stderr, "%02x ", perpid[i].tabbuf[x]);
          }
          fprintf(stderr, "\n");
        }
#endif
      }
      i += 1;
    }
    gettimeofday(&tv, NULL);
    st = runtab;
    while (st != NULL) {
      i = st->pid - TABLE_PID_FIRST;
      if ((i < 0) || (i >= (TABLE_PID_LAST-TABLE_PID_FIRST))) {
        fprintf(stderr, "internal error (pid)\n");
        exit(1);
      }
      perpid[i].tabinold = perpid[i].tabin ? 1 : 0;
      if (tmo != 0) {
        if (perpid[i].tabin == 0) {
          i = (st->soon.tv_sec - tv.tv_sec) * 1000
            + (st->soon.tv_usec - tv.tv_usec) / 1000;
          if (i <= 0) {
            tmo = 0;
          } else if ((tmo < 0) || (i < tmo)) {
            tmo = i;
          }
        }
      }
      st = st->next;
    }
    n = pollfd_poll(tmo);
    gettimeofday(&tv, NULL);
    for (i = 0; i <= TABLE_PID_LAST-TABLE_PID_FIRST; i++) {
      while ((perpid[i].tabin > perpid[i].tabout)
          && (r >= (((((perpid[i].tabbuf[perpid[i].tabout+3] & 0x0F) << 8)
                      + perpid[i].tabbuf[perpid[i].tabout+4] + TS_HEADSLEN
                      + TS_PFIELDLEN + TS_PACKET_SIZE - TS_PACKET_HEADSIZE)
                     * 131) / 128))) {
        perpid[i].tabout +=
          tab2ts(&perpid[i].tabbuf[perpid[i].tabout], &perpid[i].conticnt);
        r = outout - outin;
        if (r < 0) {
          r += OUTBUF_SIZE;
        }
      }
      if (perpid[i].tabin <= perpid[i].tabout) {
        perpid[i].tabin = perpid[i].tabout = 0;
      }
    }
    if ((n > 0) && (n1 >= 0) && (r = pollfd_rev(n1))) {
      if (r & (POLLNVAL | POLLERR)) {
        fprintf(stderr, "poll error: %x\n", r);
        return;
      }
      if (outout >= OUTBUF_SIZE) {
        outout = 0;
      }
      r = ((outin > outout) ? outin : OUTBUF_SIZE) - outout;
      r = write(STDOUT_FILENO, &outbuf[outout], r);
      if (r < 0) {
        fprintf(stderr, "write error(%d)\n", errno);
        return;
      }
      if (r == 0) {
        exit(0);
      }
      outout += r;
      if (outout == outin) {
        outin = outout = 0;
      }
    }
    pst = &runtab;
    while ((st = *pst) != NULL) {
      i = st->pid - TABLE_PID_FIRST;
      if ((perpid[i].tabinold == 0)
       && (perpid[i].tabin < (TABBUF_SIZE-MAX_PSI_SIZE+1-2))
       && ((st->soon.tv_sec < tv.tv_sec)
        || ((st->soon.tv_sec == tv.tv_sec)
         && (st->soon.tv_usec <= tv.tv_usec)))) {
        if (st->freqmsec > 0) {
          i = (st->soon.tv_sec - tv.tv_sec) * 1000
            + (st->soon.tv_usec - tv.tv_usec) / 1000;
          if (i < -st->freqmsec) {
            st->soon = tv;
          } else {
            st->soon.tv_sec += st->freqmsec / 1000;
            st->soon.tv_usec += (st->freqmsec % 1000) * 1000;
            if (st->soon.tv_usec > 1000000) {
              st->soon.tv_usec -= 1000000;
              st->soon.tv_sec += 1;
            }
          }
        }
#ifdef DEBUG
        fprintf(stderr, "do tab: %ld.%06ld: %ld, %u\n", tv.tv_sec, tv.tv_usec,
                st->pid, st->tableid);
#endif
        gentab(st, &tv);
        if (st->freqmsec <= 0) {
          *pst = st->next;
          st->next = oldtab;
          oldtab = st;
        } else {
          pst = &st->next;
        }
      } else {
        pst = &st->next;
      }
    }
    if ((n > 0) && (nst >= 0) && (r = pollfd_rev(nst))) {
      if (r & (POLLNVAL | POLLERR)) {
        fprintf(stderr, "poll error: %x\n", r);
        close(tabnew.fd);
        free(newtab->tab);
        free(newtab);
        newtab = NULL;
      } else {
        i = tabnew.ibuf;
        r = read(tabnew.fd, &tabnew.buf[i], sizeof(tabnew.buf) - i - 1);
        if (r < 0) {
          fprintf(stderr, "read error(%d): %d\n", errno, tabnew.fd);
          close(tabnew.fd);
          free(newtab->tab);
          free(newtab);
          newtab = NULL;
        } else {
          int e, j = 0;
          i += r;
          while ((e = siline(newtab, &tabnew.buf[j], i)) >= 0) {
            j += e;
            i -= e;
          }
          switch (e) {
          case -ENOBUFS:
            close(tabnew.fd);
#ifdef DEBUG
            fprintf(stderr, "done, itab=%d\n", tabnew.itab);
            for (i = 0; i < tabnew.itab; i++) {
              fprintf(stderr, "%lu,", newtab->tab[i]);
            }
            fprintf(stderr, "\n");
#endif
            i = newtab->pid - TABLE_PID_FIRST;
            if ((perpid[i].tabbuf == NULL)
             && ((perpid[i].tabbuf = malloc(TABBUF_SIZE)) == NULL)) {
              fprintf(stderr, "malloc failed for table buffer pid=%02lx\n",
                    newtab->pid);
              free(newtab->tab);
              free(newtab);
            } else {
              if (newtab->tableid_ext) {
                newtab->tableid_ext = newtab->tab[0];
              }
              droptab(newtab->pid, newtab->tableid, newtab->tableid_ext);
              newtab->version = nextversion(newtab);
              newtab->soon = tv;
              newtab->next = runtab;
              runtab = newtab;
            }
            newtab = NULL;
            break;
          case -EAGAIN:
            if (r == 0) {
              fprintf(stderr, "unexpected end of file: %d\n", tabnew.fd);
              close(tabnew.fd);
              free(newtab->tab);
              free(newtab);
              newtab = NULL;
            } else {
              if (i > 0) {
                memmove(&tabnew.buf[0], &tabnew.buf[j], i);
              }
              tabnew.ibuf = i;
            }
            break;
          default:
            fprintf(stderr, "eval error: %d\n", e);
            close(tabnew.fd);
            free(newtab->tab);
            free(newtab);
            newtab = NULL;
            break;
          }
        }
      }
      n -= 1;
    }
    if ((n > 0) && (n0 >= 0) && (r = pollfd_rev(n0))) {
      if (r & (POLLNVAL | POLLERR)) {
        fprintf(stderr, "poll error: %x\n", r);
        return;
      }
      r = read(f0, &buf0[i0], sizeof(buf0) - i0 - 1);
      if (r < 0) {
        fprintf(stderr, "read error(%d): %d\n", errno, f0);
        return;
      }
      if (r == 0) {
        return;
      }
      i0 += r;
      n -= 1;
    }
  } while (1);
}

int main(int argc, char *argv[])
{
  int f = -1;
  int a;
  if (sizeof(unsigned) != sizeof(struct loop_descr *)) {
    fprintf(stderr, "data type prerequisites not met\n");
    return 1;
  }
  gen_crc32_table();
  system_init();
  unblockf(STDIN_FILENO);
  unblockf(STDOUT_FILENO);
  memset(&perpid[0], 0, sizeof(perpid));
  a = 1;
  do {
    if ((a < argc) && (strcmp(argv[a], "-"))) {
      f = open(argv[a], O_RDONLY | O_NONBLOCK);
      if (f < 0) {
        fprintf(stderr, "open failed(%d): %s\n", errno, argv[a]);
      }
    } else {
      f = STDIN_FILENO;
    }
    argloop(f);
  } while (++a < argc);
  argloop(-1);
  return 0;
}
