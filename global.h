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

#include <stdint.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* for a timing and poll profile: */
#if 0
#define DEBUG_TIMEPOLL
#endif

#define PES_LOWEST_SID    (0xBC)
#define NUMBER_DESCR   256
#define TS_PACKET_SIZE 188

#define CRC_SIZE 4

#define MAX_MSEC_OUTDELAY  500
#define MAX_MSEC_PUSHJTTR  (2 * 250)
#define MAX_MSEC_PCRDIST   (100 * 80/100)

#define TRIGGER_MSEC_INPUT  250
#define TRIGGER_MSEC_OUTPUT 250

#define MAX_DATA_COMB 512
#define HIGHWATER_COM 8

#define MAX_CTRL_OUTB (1 << 16)
#define MAX_DATA_OUTB (MAX_CTRL_OUTB << 7)
#define HIGHWATER_OUT 512
#define MAX_WRITE_OUT (44 * TS_PACKET_SIZE)

#define MAX_CTRL_INB  (1 << 10)
#define MAX_DATA_INBV (MAX_CTRL_INB << 10)
#define MAX_DATA_INBA (MAX_CTRL_INB << 9)
#define MAX_DATA_INB  (MAX_CTRL_INB << 9)
#define MAX_DATA_INBPSI (MAX_CTRL_INB << 6)
#define HIGHWATER_IN  (16 * 1024)

#define MAX_DATA_RAWB (1 << 18)
#define HIGHWATER_RAW (16 * 1024)
#define MAX_READ_IN   (8 * 1024)

#define MAX_INSTREAM  64 /* ? */
#define MAX_INFILE    8  /* ? */

#define MAX_STRPERPS  (1<<8)
#define MAX_STRPERTS  (1<<13)

#define MAX_DESCR_LEN 0xFF
#define MAX_PSI_SIZE  (4096+1)
#define CAN_PSI_SIZE  (1024+1)
#define MAX_PMTSTREAMS (CAN_PSI_SIZE / 4)

#define MAX_STRPERPRG 42 /* ? */
#define MAX_OUTPROG   32  /* ? */
#define MAX_PRGFORSTR MAX_OUTPROG

#define MAX_POLLFD    (MAX_INFILE+3)

#define ENDSTR_KILL      0
#define ENDSTR_CLOSE     1
#define ENDSTR_WAIT      2

#define boolean uint8_t
#define FALSE   0
#define TRUE    1

#define byte uint8_t
#define t_msec int32_t

/* ISO 13818 clock reference 90kHz (33 bit) with 27MHz extension (9 bit).
 * ba33 holds the high bit of base.
 */
typedef struct {
  uint32_t base;
  uint16_t ext;
  byte ba33;
  boolean valid;
} clockref;

/* For conversion purposes, this pair of values holds a partial clock
 * reference and an internal value in milliseconds. This is to eliminate
 * wrapping faults without producing conversion inaccuracies.
 */
typedef struct {
  uint32_t base;
  t_msec msec;
} conversion_base;

/* On reference to a controlled data buffer, this one holds the control
 * information, mainly: index into the data buffer, length of the referenced
 * data block. In a controlled data buffer, a data block is never split to
 * wrap at the end of the buffer. The other fields are usage dependend.
 */
typedef struct {
  int index;
  int length;
  int sequence;
  t_msec msecread;
  t_msec msecpush;
  clockref pcr;
  clockref opcr;
  byte scramble;
} ctrl_buffer;

/* Control buffer */
typedef struct {
  ctrl_buffer *ptr;
  int in;
  int out;
  int mask;
} refr_ctrl;

/* Data buffer */
typedef struct {
  byte *ptr;
  int in;
  int out;
  int mask;
} refr_data;


/* Create a new buffer, return TRUE on success, FALSE otherwise */
#define list_create(refr,size) \
  ((((size) & ((size)-1)) || (size < 2)) ? \
     warn (LERR,"List Create",EGLO,1,1,size), FALSE : \
   ((refr).ptr = malloc((size) * sizeof(*(refr).ptr))) == NULL ? \
     warn (LERR,"List Create",EGLO,1,2,size), FALSE : \
   ((refr).mask = (size)-1, (refr).in = (refr).out = 0, TRUE))

/* Release a buffer no longer used */
#define list_release(refr) \
  ((refr).mask = 0, free((refr).ptr), (refr).ptr = NULL)

/* Test on buffer emptiness */
#define list_empty(refr) ((refr).out == (refr).in)

/* Compute number of free elements in buffer */
#define list_free(refr) \
  (((refr).out - (refr).in - 1) & (refr).mask)

/* Version of list_free with cached (refr).in */
#define list_freecachedin(refr,refrin) \
  (((refr).out - refrin - 1) & (refr).mask)

/* Compute number of free elements up to the wrapping point, if the
   latter is included in the free part of the buffer */
#define list_freeinend(refr) \
  ((refr).mask + 1 - (refr).in)

/* Version of list_freeinend with cached (refr).in */
#define list_freeinendcachedin(refr,refrin) \
  ((refr).mask + 1 - refrin)

/* Compute number of used elements in buffer (i.e. its current size) */
#define list_size(refr) \
  (((refr).in - (refr).out) & (refr).mask)

/* Test on buffer fullness */
#define list_full(refr) \
  (list_free(refr) == 0)

/* Test on buffer partial fullness (as trigger criterium) */
#define list_partialfull(refr) \
  (list_size(refr) > ((refr).mask * 3/4))

/* Increment an index variable that points in to a buffer by a given value */
#define list_incr(var,refr,incr) \
  ((var) = (((var) + (incr)) & (refr).mask))


/* Check a data byte against a mask */
#define marker_check(data,val,mask) \
  (((data & mask) != val) ? \
    warn(LWAR,"Marker bit",EGLO,2,data,mask), TRUE : FALSE)

/* Check whether a given bit is set in a data byte */
#define marker_bit(data,bit) \
  marker_check(data,1<<bit,1<<bit)

#define mmin(a,b) ((a)<(b)?(a):(b))
#define mmax(a,b) ((a)<(b)?(b):(a))

/* Allocate memory for a struct with known union usage */
#define unionalloc(typ,fld) \
  (malloc (sizeof(typ)-sizeof(((typ*)0)->u)+sizeof(((typ*)0)->u.fld)))

/* Release a chained list completely */
#define releasechain(typ,root) \
  { register typ *relchn_n, *relchn_p = root; \
    while (relchn_p != NULL) { \
      relchn_n = relchn_p->next; free (relchn_p); relchn_p = relchn_n; \
  } }

/* Supported input file types */
typedef enum {
  ct_packetized, /* packetized elementary stream */
  ct_program,    /* program stream */
  ct_transport,  /* transport stream */
  number_ct
} content_type;

/* stream data types, dependend on buffer contents */
typedef enum {
  sd_data,       /* PES packet */
  sd_map,        /* mapreference containing descriptors */
  sd_unparsedsi, /* TS packet containing raw partial SI sections */
  number_sd
} streamdata_type;

/* Reference descriptors as these are parsed from PSI */
typedef struct {
  int programnumber;
  short sourceid;
  byte version;
  byte *elemdnew[NUMBER_DESCR];
} mapreference;

/* Source TS PMT list */
typedef struct pmtdescr {
  struct pmtdescr *next;
  short pat_section;
  byte pmt_version;
  int programnumber;
  short pcr_pid;
  short pmt_pid;
  short streams;
  short stream[MAX_PMTSTREAMS];
  byte streamtype[MAX_PMTSTREAMS];
  short descrlen;
  byte elemdescr[MAX_PSI_SIZE];
} pmt_descr;

/* Automatic stream usage requests as stated via command */
typedef struct tsautodescr {
  struct tsautodescr *next;
  int sprg;
  int tprg;
  int ssid; /* ssid<0 when referencing a complete program */
  int tsid;
} tsauto_descr;

/* Declaration of pid ranges as being not-to-be-parsed SI */
typedef struct tssidescr {
  struct tssidescr *next;
  short pid_low;
  short pid_high;
} tssi_descr;

/* Source file */
typedef struct {
  refr_data data;
  int handle;
  char *name;
  int filerefnum;
  int st_mode;
  struct pollfd *ufds;
  int skipped; /* undesired bytes skipped, total */
  int payload; /* split payload used total */
  int total; /* split total (skipped, used, wasted) */
  int sequence; /* source counter for PES sequence */
  short openstreams[number_sd];
  char *append_name;
  int append_filerefnum;
  int append_repeatitions;
  int repeatitions;
  int auto_programnb;
  boolean automatic; /* extract'o'use */
  boolean stopfile;
  content_type content;
  union {
    struct {
      struct streamdescr *stream;
    } pes;
    struct {
      struct {
        clockref scr;
        uint32_t muxrate;
      } ph;
      struct {
        uint32_t ratebound;
        byte audiobound;
        byte videobound;
        boolean csps_flag;
        boolean fixed_flag;
        boolean system_video_lock_flag;
        boolean system_audio_lock_flag;
        boolean packet_rate_restriction_flag;
        short buffer_bound[MAX_STRPERPS-PES_LOWEST_SID];
      } sh;
/*
      struct {
      } dir;
*/
      struct streamdescr *stream[MAX_STRPERPS];
    } ps;
    struct {
      uint16_t transportstreamid;
      byte pat_version;
      byte newpat_version;
      pmt_descr *pat;
      pmt_descr *newpat;
      tsauto_descr *tsauto;
      tssi_descr *tssi;
      struct streamdescr *stream[MAX_STRPERTS];
    } ts;
  } u;
} file_descr;

/* Descriptors, data and reference index list */
typedef struct {
  byte *refx[NUMBER_DESCR];
  byte null[2];
  byte data[MAX_PSI_SIZE];
} descr_descr;

/* Stream entry in target PMT, without corresponding input stream.
 * This is used to manually denote si streams, that are brought in
 * via --si and have to be mentioned in PMT in some way.
 */
typedef struct stumpdescr {
  struct stumpdescr *next;
  int program_number;
  short pid;
  byte stream_type;
  descr_descr manudescr;
} stump_descr;

/* Target program */
typedef struct {
  int program_number;
  short pcr_pid;
  short pmt_pid;
  byte pmt_conticnt;
  byte pmt_version;
  boolean changed; /* must generate new psi due to change */
  boolean unchanged; /* must generate new psi due to timing */
  short pat_section;
  short streams;
  struct streamdescr *stream[MAX_STRPERPRG];
  stump_descr *stump; /* just entries in PMT, not really data streams */
  descr_descr manudescr;
} prog_descr;

/* Single data or map stream */
typedef struct streamdescr {
  refr_ctrl ctrl;
  refr_data data;
  file_descr *fdescr;
  short sourceid; /* index into fdescr->u.xx.stream[] */
  byte stream_id; /* elementary stream id, table 2-35, etc */
  byte stream_type; /* table 2-29 */
  byte version;
  byte conticnt;
  byte endaction;
  descr_descr *autodescr; /* Descriptors copied from input stream */
  descr_descr *manudescr; /* Descriptors manually added */
/*what if a stream is leftupper corner in one prog, but elsewhere in another?*/
  streamdata_type streamdata;
  union {
    struct {
      short pid; /* splicets: 0010..1FFE, spliceps: ...FF */
      boolean discontinuity;
      boolean trigger;
      boolean mention;
      boolean has_clockref; /* in output */
      boolean has_opcr; /* in input */
      struct streamdescr *mapstream;
      t_msec next_clockref;
      t_msec delta;
      conversion_base conv;
      t_msec lasttime;
      short progs;
      prog_descr *pdescr[MAX_PRGFORSTR];
    } d;
    struct {
      t_msec msectime;
      conversion_base conv;
      int psi_length;
      byte psi_data[MAX_PSI_SIZE+TS_PACKET_SIZE];
    } m;
    struct {
    } usi;
  } u;
} stream_descr;


extern boolean timed_io;
extern boolean accept_weird_scr;
extern boolean conservative_pid_assignment;
extern t_msec global_delta;

t_msec msec_now (void);

void cref2msec (conversion_base *b,
    clockref c,
    t_msec *m);

void msec2cref (conversion_base *b,
    t_msec m,
    clockref *c);

void global_init (void);


#ifdef DEBUG_TIMEPOLL

#define max_timepoll (1024*1024)

typedef struct {
  struct timeval tv;
  t_msec msec_now;
  int usec;
  int tmo;
  int sr, si, so;
  unsigned char cnt_msecnow;
  unsigned char nfdso, nfdsi;
  unsigned char nfdsrevent;
  unsigned char flags;
} timepoll;
#define LTP_FLAG_DELTASHIFT 0x80
#define LTP_FLAG_OUTPUT     0x40
#define LTP_FLAG_INPUT      0x20
#define LTP_FLAG_SPLIT      0x10
#define LTP_FLAG_PROCESS    0x08

extern timepoll logtp [max_timepoll];
extern long logtpc;
extern timepoll *ltp;

#endif

