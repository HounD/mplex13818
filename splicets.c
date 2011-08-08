/*
 * ISO 13818 stream multiplexer
 * Copyright (C) 2001 Convergence Integrated Media GmbH Berlin
 * Copyright (C) 2004..2005 Oskar Schirmer (schirmer@scara.com)
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
 * Module:  Splice TS
 * Purpose: Generate transport stream.
 *
 * This module generates from the available input stream data (as
 * seperated by the split functions) the complete output stream.
 * It provides functions to handle programs for the resulting stream,
 * as these are output format dependent. Further, it accepts PSI data
 * just in time, validating it not earlier than with the arrival of
 * the corresponding payload at this stage.
 */

#include "global.h"
#include "crc32.h"
#include "error.h"
#include "input.h"
#include "output.h"
#include "descref.h"
#include "splitts.h"
#include "pes.h"
#include "ts.h"
#include "splice.h"
#include "splicets.h"

const boolean splice_multipleprograms = TRUE;

static boolean changed_pat;
static boolean unchanged_pat;
static int pat_section;
static const int last_patsection = 0;
static byte nextpat_version;
static byte pat_conticnt;

static int transportstreamid;

static int psi_size;
static int psi_done;
static byte psi_data [MAX_PSI_SIZE];

static byte unit_start;
static byte *conticnt;
static int psi_pid;
static short network_pid = 0;

static int progs;
static prog_descr *prog [MAX_OUTPROG];

static int nextpid;
static stream_descr *outs [MAX_STRPERTS];

static stump_descr *globalstumps;

boolean splice_specific_init (void)
{
  progs = 0;
  nextpid = 0;
  memset (outs,0,sizeof(outs));
  changed_pat = TRUE;
  pat_section = 0;
  nextpat_version = 0;
  pat_conticnt = 0;
  psi_size = psi_done = 0;
  unit_start = TS_UNIT_START;
  transportstreamid = 0x4227;
  globalstumps = NULL;
  return (TRUE);
}

void splice_settransportstreamid (int tsid)
{
  transportstreamid = tsid;
}

void splice_setpsifrequency (t_msec freq)
{
  psi_frequency_msec = freq;
  psi_frequency_changed = TRUE;
}

void splice_setnetworkpid (short pid)
{
  if ((pid < 0) || (pid > TS_PID_HIGHEST)) {
    network_pid = 0;
  } else {
    network_pid = pid;
  }
}

static int findapid (stream_descr *s, int desire)
{
  byte okness = 2;
  int h;
  if (conservative_pid_assignment
   && (desire >= 0)
   && (outs[desire] == NULL)
   && (input_tssiinafilerange (desire) < 0)) {
    return (desire);
  }
  do {
    if ((nextpid < TS_PID_SPLICELO) || (nextpid >= TS_PID_SPLICEHI)) {
      warn (LDEB,"Next PID",ETSC,1,okness,nextpid);
      if (okness == 0) {
        warn (LERR,"No PID found",ETSC,2,1,0);
        return (0);
      }
      okness -= 1;
      nextpid = TS_PID_SPLICELO;
    } else {
      warn (LDEB,"Next PID",ETSC,2,okness,nextpid);
      nextpid += 1;
    }
    if (okness != 0) {
      h = input_tssiinafilerange (nextpid);
      warn (LDEB,"Next PID",ETSC,3,h,nextpid);
      if (h >= 0) {
        nextpid = h;
      }
    } else {
      h = -1;
    }
  } while ((h >= 0)
        || (outs[nextpid] != NULL));
  outs[nextpid] = s;
  warn (LDEB,"Next PID",ETSC,2,2,nextpid);
  return (nextpid);
}

void splice_all_configuration (void)
{
  int i;
  if (configuration_must_print) {
    i = progs;
    fprintf (stderr, configuration_total, i);
    while (--i >= 0) {
      splice_one_configuration (prog[i]);
    }
    configuration_was_printed;
  }
}

void splice_addsirange (file_descr *f,
    int lower,
    int upper)
{
  int i, r;
  tssi_descr *tssi;
  prog_descr *p;
  tssi = malloc (sizeof (tssi_descr));
  if (tssi != NULL) {
    if (ts_file_stream (f,TS_UNPARSED_SI) == NULL) {
      ts_file_stream (f,TS_UNPARSED_SI) = input_openstream (f,
              TS_UNPARSED_SI,0,0,sd_unparsedsi,NULL);
    }
    if (ts_file_stream (f,TS_UNPARSED_SI) != NULL) {
      tssi->next = f->u.ts.tssi;
      tssi->pid_low = lower;
      tssi->pid_high = upper;
      f->u.ts.tssi = tssi;
      r = upper; /* check for collision against existing PIDs, first sd_data */
      while (r >= lower) {
        stream_descr *s;
        s = outs[r];
        if ((s != NULL)
         && (s != PMT_STREAM)) {
          if (s->streamdata == sd_data) {
            i = findapid (s, -1);
            if (input_tssiinafilerange (i) >= 0) { /* none free! */
              outs[i] = NULL;
            } else {
              int j;
              s->u.d.pid = i;
              j = s->u.d.progs;
              while (--j >= 0) {
                p = s->u.d.pdescr[j];
                p->changed = TRUE;
                if (p->pcr_pid == r) {
                  p->pcr_pid = i;
                }
              }
              configuration_changed = TRUE;
              outs[r] = NULL;
            }
          } else {
            warn (LERR,"Bad PID",ETSC,11,s->streamdata,r);
          }
        }
        r -= 1;
      }
      i = progs; /* ...then sd_map */
      while (--i >= 0) {
        p = prog[i];
        r = p->pmt_pid;
        if ((r >= lower)
         && (r <= upper)) {
          int q;
          q = findapid (PMT_STREAM, -1);
          if (input_tssiinafilerange (q) >= 0) { /* none free! */
            outs[q] = NULL;
          } else {
            int j;
            outs[r] = NULL;
            j = i;
            while (--j >= 0) {
              if (prog[j]->pmt_pid == r) {
                prog[j]->pmt_pid = q;
              }
            }
            p->pmt_pid = q;
            changed_pat = TRUE;
            configuration_changed = TRUE;
          }
        }
      }
    } else {
      free (tssi);
    }
  }
}

void splice_createstump (int programnb,
    short pid,
    byte styp)
{
  prog_descr *p;
  stump_descr **pst;
  stump_descr *st;
  p = splice_getprog (programnb);
  if (p != NULL) {
    configuration_changed = TRUE;
    p->changed = TRUE;
    pst = &(p->stump);
  } else {
    pst = &globalstumps;
  }
  st = *pst;
  while ((st != NULL)
      && ((st->pid != pid)
       || (st->program_number != programnb))) {
    st = st->next;
  }
  if (st == NULL) {
    st = malloc (sizeof(stump_descr));
    st->next = *pst;
    st->program_number = programnb;
    st->pid = pid;
    *pst = st;
  }
  st->stream_type = styp;
  clear_descrdescr (&(st->manudescr));
  splice_modifycheckmatch (programnb,p,NULL,st);
}

stump_descr *splice_getstumps (int programnb,
    short pid)
{
  prog_descr *p;
  stump_descr **pst;
  stump_descr *rl;
  rl = NULL;
  p = splice_getprog (programnb);
  if (p != NULL) {
    pst = &(p->stump);
  } else {
    pst = &globalstumps;
  }
  while (*pst != NULL) {
    stump_descr *st;
    st = *pst;
    if ((st->program_number == programnb)
     && ((pid < 0)
      || (pid == st->pid))) {
      st = *pst;
      *pst = st->next;
      st->next = rl;
      rl = st;
      if (p != NULL) {
        configuration_changed = TRUE;
        p->changed = TRUE;
      }
    } else {
      pst = &((*pst)->next);
    }
  }
  return (rl);
}

void splice_modifytargetdescriptor (int programnb,
    short sid,
    short pid,
    int dtag,
    int dlength,
    byte *data)
{
  int i;
  if (programnb < 0) {
    i = progs;
    while (--i >= 0) {
      splice_modifytargetdescrprog (prog[i],
          prog[i]->program_number,-1,0,-1,-1,NULL,globalstumps);
    }
    splice_modifytargetdescrprog (NULL,-1,-1,0,-1,-1,NULL,globalstumps);
  } else {
    splice_modifytargetdescrprog (splice_getprog (programnb),
        programnb,sid,pid,dtag,dlength,data,globalstumps);
  }
}

prog_descr *splice_getprog (int programnb)
{
  int i;
  i = progs;
  while (--i >= 0) {
    if (prog[i]->program_number == programnb) {
      return (prog[i]);
    }
  }
  return (NULL);
}

prog_descr *splice_openprog (int programnb)
{
  prog_descr *p;
  int pid;
  warn (LIMP,"Open prog",ETSC,1,0,programnb);
  p = splice_getprog (programnb);
  if (p == NULL) {
    if (progs < MAX_OUTPROG) {
      if ((pid = findapid (PMT_STREAM, -1)) > 0) {
        if ((p = malloc(sizeof(prog_descr))) != NULL) {
          p->program_number = programnb;
          p->pcr_pid = -1;
          p->pmt_pid = pid;
          p->pmt_conticnt = 0;
          p->pmt_version = 0;
          p->changed = TRUE;
          p->pat_section = 0; /* more ? */
          p->streams = 0;
          p->stump = splice_getstumps (programnb,-1);
          clear_descrdescr (&p->manudescr);
          prog[progs++] = p;
          changed_pat = TRUE;
          configuration_changed = TRUE;
          splice_modifycheckmatch (programnb,p,NULL,NULL);
        } else {
          outs[pid] = NULL;
          warn (LERR,"Open prog",ETSC,1,1,0);
        }
      }
    } else {
      warn (LERR,"Max prog open",ETSC,1,2,0);
    }
  }
  return (p);
}

void splice_closeprog (prog_descr *p)
{
  int i, n;
  warn (LIMP,"Close prog",ETSC,3,0,p->program_number);
  configuration_changed = TRUE;
  while (p->streams > 0) {
    unlink_streamprog (p->stream[0],p);
  }
  releasechain (stump_descr,p->stump);
  n = -1;
  if (p->pmt_pid >= 0) {
    i = progs;
    while (--i >= 0) {
      if (prog[i]->pmt_pid == p->pmt_pid) {
        n += 1;
      }
    }
  }
  i = progs;
  while (--i >= 0) {
    if (prog[i] == p) {
      prog[i] = prog[--progs];
      if (n == 0) {
        outs[p->pmt_pid] = NULL;
      }
      free (p);
      changed_pat = TRUE;
      return;
    }
  }
  warn (LERR,"Close lost prog",ETSC,3,1,progs);
}

int splice_addstream (prog_descr *p,
    stream_descr *s,
    boolean force_sid)
{
  int pid = 0;
  warn (LIMP,"Add stream",ETSC,4,force_sid,s->stream_id);
  if (p->streams < MAX_STRPERPRG) {
    pid = findapid (s,(s->fdescr->content == ct_transport) ? s->sourceid : -1);
    if (pid > 0) {
      if (!force_sid) {
        s->stream_id = splice_findfreestreamid (p,s->stream_id);
      }
      p->stream[p->streams++] = s;
      p->changed = TRUE;
      s->u.d.pid = pid;
      configuration_changed = TRUE;
      splice_modifycheckmatch (p->program_number,p,s,NULL);
    }
  }
  return (pid);
}

boolean splice_delstream (prog_descr *p,
    stream_descr *s)
{
  int i;
  warn (LIMP,"Del stream",ETSC,5,0,s->u.d.pid);
  configuration_changed = TRUE;
  i = p->streams;
  while (--i >= 0) {
    if (p->stream[i] == s) {
      outs[s->u.d.pid] = NULL;
      p->stream[i] = p->stream[--(p->streams)];
      p->changed = TRUE;
      if (p->pcr_pid == s->u.d.pid) {
        p->pcr_pid = -1;
      }
      s->u.d.pid = 0;
      return (TRUE);
    }
  }
  warn (LERR,"Del lost stream",ETSC,5,1,p->streams);
  return (FALSE);
}

void process_finish (void)
{
  warn (LIMP,"Finish",ETSC,6,0,0);
}

static int make_patsection (int section,
    byte *dest)
{
  int i;
  byte *d;
  prog_descr *p;
  d = dest;
  *d++ = TS_TABLEID_PAT;
  d += 2;
  *d++ = transportstreamid >> 8;
  *d++ = (byte)transportstreamid;
  *d++ = 0xC0 | 0x01 | (nextpat_version << 1);
  *d++ = section;
  *d++ = last_patsection;
  i = progs;
  while (--i >= 0) {
    p = prog[i];
    if (p->pat_section == section) {
      int x;
      x = p->program_number;
      *d++ = (x >> 8);
      *d++ = x;
      x = p->pmt_pid;
      *d++ = 0xE0 | (x >> 8);
      *d++ = x;
    }
  }
  if (network_pid > 0) {
    *d++ = 0;
    *d++ = 0;
    *d++ = 0xE0 | (network_pid >> 8);
    *d++ = network_pid;
  }
  i = d + CRC_SIZE - dest - TS_TRANSPORTID;
  dest[TS_SECTIONLEN] = 0xB0 | (i >> 8);
  dest[TS_SECTIONLEN+1] = i;
  crc32_calc (dest,i + TS_TRANSPORTID - CRC_SIZE,d);
  return (i + TS_TRANSPORTID);
}

static int make_pmtsection (stream_descr *s,
    prog_descr *p,
    byte *dest)
{
  int i;
  byte *d;
  stump_descr *st;
  stream_descr *t;
  d = dest;
  *d++ = TS_TABLEID_PMT;
  d += 2;
  i = p->program_number;
  *d++ = (i >> 8);
  *d++ = i;
  *d++ = 0xC0 | 0x01 | (p->pmt_version << 1);
  *d++ = 0;
  *d++ = 0;
  if (p->pcr_pid < 0) {
    stream_descr *pcrs;
    pcrs = splice_findpcrstream (p);
    if (pcrs == NULL) {
      pcrs = s;
    }
    pcrs->u.d.has_clockref = TRUE;
    pcrs->u.d.next_clockref = msec_now () - MAX_MSEC_PCRDIST;
    p->pcr_pid = pcrs->u.d.pid;
    configuration_changed = TRUE;
  }
  i = p->pcr_pid;
  *d++ = 0xE0 | (i >> 8);
  *d++ = i;
  d += 2;
  i = NUMBER_DESCR;
  while (--i >= 0) {
    byte *y;
    y = p->manudescr.refx[i];
    if ((y == NULL)
     && (s->u.d.mapstream != NULL)) {
      y = s->u.d.mapstream->autodescr->refx[i]; /* why this one? */
    }
    if (y != NULL) {
      int yl = y[1];
      if (yl != 0) {
        yl += 2;
        memcpy (d,y,yl);
        d += yl;
      }
    }
  }
  i = d - dest - (TS_PMT_PILEN+2);
  dest[TS_PMT_PILEN] = 0xF0 | (i >> 8);
  dest[TS_PMT_PILEN+1] = i;
  i = p->streams;
  while (--i >= 0) {
    t = p->stream[i];
    if (t->u.d.mention) {
      int x;
      byte *e;
      *d++ = t->stream_type;
      x = t->u.d.pid;
      *d++ = 0xE0 | (x >> 8);
      *d++ = x;
      d += 2;
      e = d;
      x = NUMBER_DESCR;
      while (--x >= 0) {
        byte *y;
        y = t->manudescr->refx[x];
        if (y == NULL) {
          y = t->autodescr->refx[x];
        }
        if (y != NULL) {
          int yl = y[1];
          if (yl != 0) {
            yl += 2;
            memcpy (d,y,yl);
            d += yl;
          }
        }
      }
      x = d - e;
      *--e = x;
      *--e = 0xF0 | (x >> 8);
    }
  }
  st = p->stump;
  while (st != NULL) {
    int x;
    byte *e;
    *d++ = st->stream_type;
    x = st->pid;
    *d++ = 0xE0 | (x >> 8);
    *d++ = x;
    d += 2;
    e = d;
    x = NUMBER_DESCR;
    while (--x >= 0) {
      byte *y;
      y = st->manudescr.refx[x];
      if (y != NULL) {
        int yl = y[1];
        if (yl != 0) {
          yl += 2;
          memcpy (d,y,yl);
          d += yl;
        }
      }
    }
    x = d - e;
    *--e = x;
    *--e = 0xF0 | (x >> 8);
    st = st->next;
  }
  i = d + CRC_SIZE - dest - TS_TRANSPORTID;
  dest[TS_SECTIONLEN] = 0xB0 | (i >> 8);
  dest[TS_SECTIONLEN+1] = i;
  crc32_calc (dest,i + TS_TRANSPORTID - CRC_SIZE,d);
  return (i + TS_TRANSPORTID);
}

/* Check for generated psi data to-be-sent, select data source.
 * If PAT or PMT needs to be rebuild, do so. If PAT or PMT is (partially)
 * pending to be transmitted, select that to be packaged next. Otherwise
 * select data payload. Set pid, scramble mode and PES paket size.
 * Precondition: s!=NULL, !list_empty(s->ctrl), s->streamdata==sd_data.
 * Input: stream s, current ctrl fifo out c. 
 * Output: *pid, *scramble, *size (PES paket ~) for the stream to generate.
 */
static void procdata_check_psi (int *pid,
    byte *scramble,
    int *size,
    stream_descr *s,
    ctrl_buffer *c)
{
  t_msec now;
  int i, l;
  prog_descr *p;
  if (psi_size > 0) {
    *pid = psi_pid;
    *scramble = 0;
    *size = psi_size;
  } else {
    if (unit_start != 0) {
      now = msec_now ();
      if ((psi_frequency_changed)
       || ((psi_frequency_msec > 0)
        && ((next_psi_periodic - now) <= 0))) {
        unchanged_pat = TRUE;
        l = progs;
        while (--l >= 0) {
          prog[l]->unchanged = TRUE;
        }
        psi_frequency_changed = FALSE;
        next_psi_periodic = now + psi_frequency_msec;
      }
      if (unchanged_pat || changed_pat) {
        psi_pid = TS_PID_PAT;
        conticnt = &pat_conticnt;
        psi_data[0] = 0;
        if ((pat_section == 0)
         && (changed_pat)) {
          nextpat_version = (nextpat_version+1) & 0x1F;
        }
        psi_size = make_patsection (pat_section,&psi_data[1]) + 1;
        if (pat_section >= last_patsection) {
          changed_pat = FALSE;
          unchanged_pat = FALSE;
          pat_section = 0;
        } else {
          pat_section += 1;
        }
        psi_done = 0;
        *pid = psi_pid;
        *scramble = 0;
        *size = psi_size;
      } else {
        l = s->u.d.progs;
        while (--l >= 0) {
          p = s->u.d.pdescr[l];
          if (p->unchanged || p->changed) {
            i = p->streams;
            while ((--i >= 0)
                && (!p->stream[i]->u.d.mention)) {
            }
            if (i >= 0) {
              psi_pid = p->pmt_pid;
              conticnt = &p->pmt_conticnt;
              psi_data[0] = 0;
              if (p->changed) {
                p->pmt_version = (p->pmt_version+1) & 0x1F;
              }
              psi_size = make_pmtsection (s,p,&psi_data[1]) + 1;
              p->changed = FALSE;
              p->unchanged = FALSE;
              psi_done = 0;
              *pid = psi_pid;
              *scramble = 0;
              *size = psi_size;
              return;
            }
          }
        }
        s->data.ptr[c->index+PES_STREAM_ID] = s->stream_id;
        conticnt = &s->conticnt;
        *pid = s->u.d.pid;
        *scramble = c->scramble;
        *size = c->length;
      }
    } else {
      *pid = s->u.d.pid;
      *scramble = c->scramble;
      *size = c->length;
    }
  }
}

/* Check for adaption field items to be filled in.
 * First assume no adaption field is set and the complete packet except the
 * header is available for payload. Then check which adaption fields have
 * to be set and decrease the free space accordingly.
 * Precondition: s!=NULL, !list_empty(s->ctrl), s->streamdata==sd_data.
 * Input: stream s, current ctrl fifo out c.
 * Output: *adapt_flags1, *adapt_flags2, *adapt_ext_len.
 * Return: number of bytes of free space available for payload.
 */
static int procdata_adaptfield_flags (byte *adapt_flags1,
    byte *adapt_flags2,
    int *adapt_ext_len,
    stream_descr *s,
    ctrl_buffer *c)
{
  int space;
  *adapt_ext_len = 1;
  *adapt_flags2 = 0;
  *adapt_flags1 = 0;
  space = TS_PACKET_SIZE - TS_PACKET_HEADSIZE;
  if ((psi_size <= 0)
   && (s->u.d.discontinuity)) { /* o, not for contents, but PCR-disco ? */
    s->u.d.discontinuity = FALSE;
    *adapt_flags1 |= TS_ADAPT_DISCONTI;
  }
  if (0) {
    *adapt_flags1 |= TS_ADAPT_RANDOMAC;
  }
  if (0) {
    *adapt_flags1 |= TS_ADAPT_PRIORITY;
  }
  if ((psi_size <= 0)
   && (s->u.d.has_clockref)
   && ((c->pcr.valid)
    || (s->u.d.next_clockref - (c->msecpush + s->u.d.delta) <= 0))) {
    *adapt_flags1 |= TS_ADAPT_PCRFLAG;
    space -= 6;
  }
  if ((psi_size <= 0)
   && ((c->opcr.valid)
    || ((!s->u.d.has_opcr)
     && (c->pcr.valid)))) {
    *adapt_flags1 |= TS_ADAPT_OPCRFLAG;
    space -= 6;
  }
  if (0) {
    *adapt_flags1 |= TS_ADAPT_SPLICING;
    space -= 1;
  }
  if (0) {
    int privdata;
    *adapt_flags1 |= TS_ADAPT_TPRIVATE;
    privdata = 0;
    space -= (privdata + 1);
  }
  if (0) {
    *adapt_flags2 |= TS_ADAPT2_LTWFLAG;
    *adapt_ext_len += 2;
  }
  if (0) {
    *adapt_flags2 |= TS_ADAPT2_PIECEWRF;
    *adapt_ext_len += 3;
  }
  if (0) {
    *adapt_flags2 |= TS_ADAPT2_SEAMLESS;
    *adapt_ext_len += 5;
  }
  if (*adapt_flags2 != 0) {
    *adapt_flags1 |= TS_ADAPT_EXTENSIO;
    space -= *adapt_ext_len;
  }
  if (*adapt_flags1 != 0) {
    space -= 2;
  }
  return (space);
}

/* Adjust size of adaption field against size of payload. Set flags.
 * Input: *space (number of bytes of free space available for payload),
 *   *adapt_flags1, size (number of bytes left to be sent).
 * Output: *space (corrected in case of padding is done via adaption field),
 *   *adapt_field_ctrl.
 * Return: Number of bytes of payload to be inserted into THIS packet.
 */
static int procdata_adaptfield_frame (int *space,
    byte *adapt_field_ctrl,
    byte adapt_flags1,
    int size)
{
  int payload;
  if (size < *space) {
    payload = size;
    if (adapt_flags1 == 0) {
      *space -= 1;
      if (*space > payload) {
        *space -= 1;
      }
    }
    *adapt_field_ctrl = TS_AFC_BOTH;
  } else {
    payload = *space;
    if (payload == 0) {
      *adapt_field_ctrl = TS_AFC_ADAPT;
    } else if (adapt_flags1 == 0) {
      *adapt_field_ctrl = TS_AFC_PAYLD;
    } else {
      *adapt_field_ctrl = TS_AFC_BOTH;
    }
  }
  return (payload);
}

/* Generate packet header.
 * Keep track of continuity counter (which is selected in procdata_check_psi).
 * Precondition: d!=NULL (data destination).
 * Input: pid, scramble, adaption_field_ctrl.
 * Return: d (increased by header size).
 */
static byte *procdata_syn_head (byte *d,
    int pid,
    byte scramble,
    byte adapt_field_ctrl)
{
  *d++ = TS_SYNC_BYTE;
  warn (LSEC,"Splice unitstart",ETSC,7,1,unit_start);
  warn (LSEC,"Splice PID",ETSC,7,2,pid);
  *d++ = (0 << 7) /* transport_error_indicator */
       | unit_start
       | (0 << 5) /* transport_priority */
       | (pid >> 8);
  *d++ = pid;
  *d++ = (scramble << 6)
       | adapt_field_ctrl
       | *conticnt;
  warn (LSEC,"Splice continuity cnt",ETSC,7,3,*conticnt);
  if (adapt_field_ctrl & TS_AFC_PAYLD) {
    *conticnt = (*conticnt+1) & 0x0F;
  }
  return (d);
}

/* Generate adpation field.
 * This MUST match the calculations in procdata_adaptfield_flags.
 * Precondition: s!=NULL.
 * Input: s (stream), c (current ctrl fifo out), d (data destination),
 *   padding (number of padding bytes needed), payload (number of payload bytes
 *   to insert), adapt_field_ctrl, adapt_flags1, adapt_flags2, adapt_ext_len.
 * Return: d (increased by adaption field size).
 */
static byte *procdata_syn_adaptfield (stream_descr *s,
    ctrl_buffer *c,
    byte *d,
    int padding,
    int payload,
    byte adapt_field_ctrl,
    byte adapt_flags1,
    byte adapt_flags2,
    int adapt_ext_len)
{
  if (adapt_field_ctrl & TS_AFC_ADAPT) {
    if ((*d++ = (TS_PACKET_SIZE - TS_PACKET_FLAGS1) - payload) != 0) {
      *d++ = adapt_flags1;
      if (adapt_flags1 & TS_ADAPT_PCRFLAG) {
        clockref pcr;
        msec2cref (&s->u.d.conv, c->msecpush + s->u.d.delta, &pcr);
        *d++ = (pcr.base >> 25) | (pcr.ba33 << 7);
        *d++ = pcr.base >> 17;
        *d++ = pcr.base >> 9;
        *d++ = pcr.base >> 1;
        *d++ = (pcr.base << 7) | (pcr.ext >> 8) | 0x7E;
        *d++ = pcr.ext;
        s->u.d.next_clockref =
          (c->msecpush + s->u.d.delta) + MAX_MSEC_PCRDIST;
        c->pcr.valid = FALSE;
      }
      if (adapt_flags1 & TS_ADAPT_OPCRFLAG) {
        clockref *opcr;
        if (c->opcr.valid) {
          opcr = &c->opcr;
        } else {
          opcr = &c->pcr;
        }
        *d++ = (opcr->base >> 25) | (opcr->ba33 << 7);
        *d++ = opcr->base >> 17;
        *d++ = opcr->base >> 9;
        *d++ = opcr->base >> 1;
        *d++ = (opcr->base << 7) | (opcr->ext >> 8) | 0x7E;
        *d++ = opcr->ext;
        opcr->valid = FALSE;
      }
      if (adapt_flags1 & TS_ADAPT_SPLICING) {
      }
      if (adapt_flags1 & TS_ADAPT_TPRIVATE) {
      }
      if (adapt_flags1 & TS_ADAPT_EXTENSIO) {
        *d++ = adapt_ext_len;
        *d++ = adapt_flags2 | 0x1F;
        if (adapt_flags2 & TS_ADAPT2_LTWFLAG) {
        }
        if (adapt_flags2 & TS_ADAPT2_PIECEWRF) {
        }
        if (adapt_flags2 & TS_ADAPT2_SEAMLESS) {
        }
      }
    }
    if (padding > 0) {
      warn (LSEC,"Splice padding",ETSC,8,1,padding);
      memset (d,-1,padding);
      d += padding;
    }
  }
  return (d);
}

/* Generate payload portion.
 * Insert the appropriate payload (either PSI or data), check whether payload
 * from this PES packet or section is left.
 * Precondition: s!=NULL.
 * Input: s (stream), c (current ctrl fifo out), d (data destination),
 *   payload (number of payload bytes to insert).
 * Return: processed stream s, if there is more data from the current PES
 *   packet to be processed, NULL otherwise.
 */
static stream_descr *procdata_syn_payload (stream_descr *s,
    ctrl_buffer *c,
    byte *d,
    int payload)
{
  if (payload > 0) {
    if (psi_size > 0) {
      memcpy (d,&psi_data[psi_done],payload);
      if (payload < psi_size) {
        warn (LSEC,"Splice PSI Data",ETSC,9,s->stream_id,payload);
        psi_done += payload;
        psi_size -= payload;
        unit_start = 0;
      } else {
        warn (LINF,"Splice PSI Done",ETSC,9,s->stream_id,payload);
        psi_done = psi_size = 0;
        unit_start = TS_UNIT_START;
      }
    } else {
      memcpy (d,&s->data.ptr[c->index],payload);
      if (payload < c->length) {
        warn (LSEC,"Splice Data",ETSC,9,s->stream_id,payload);
        c->length -= payload;
        s->data.out = (c->index += payload);
        unit_start = 0;
      } else {
        warn (LINF,"Splice Done",ETSC,9,s->stream_id,payload);
        list_incr (s->ctrl.out,s->ctrl,1);
        if (list_empty (s->ctrl)) {
          s->data.out = s->data.in;
        } else {
          s->data.out = s->ctrl.ptr[s->ctrl.out].index;
        }
        unit_start = TS_UNIT_START;
        return (NULL);
      }
    }
  }
  return (s);
}

/* Process unparsed si data and generate output.
 * Take one TS paket, copy it to output stream data buffer.
 * Precondition: s!=NULL, !list_empty(s->ctrl), s->streamdata==sd_unparsedsi,
 *   s->ctrl.ptr[s->ctrl.out].length==TS_PACKET_SIZE, d!=NULL.
 */
static void proc_unparsedsi (stream_descr *s,
    byte *d)
{
  warn (LINF,"Splice Unparsed SI",ETSC,10,s->sourceid,s->streamdata);
  memcpy (d,&s->data.ptr[s->ctrl.ptr[s->ctrl.out].index],TS_PACKET_SIZE);
                /* check if ==  s->ctrl.ptr[s->ctrl.out].length); ? */
  list_incr (s->ctrl.out,s->ctrl,1);
  if (list_empty (s->ctrl)) {
    s->data.out = s->data.in;
    input_closefileifunused (s->fdescr);
  } else {
    s->data.out = s->ctrl.ptr[s->ctrl.out].index;
  }
}

stream_descr *process_something (stream_descr *s)
{
  byte *d;
  int pid;
  byte scramble;
  int size;
  ctrl_buffer *c;
  int payload;
  int space;
  int adapt_ext_len;
  byte adapt_field_ctrl;
  byte adapt_flags1, adapt_flags2;
  warn (LDEB,"Splice TS",ETSC,0,0,s->ctrl.out);
  switch (s->streamdata) {
    case sd_data:
      c = &s->ctrl.ptr[s->ctrl.out];
      procdata_check_psi (&pid, &scramble, &size, s, c);
      d = output_pushdata (TS_PACKET_SIZE, TRUE, c->msecpush + s->u.d.delta);
      if (d == NULL) {
        return (s);
      }
      space = procdata_adaptfield_flags (&adapt_flags1, &adapt_flags2,
          &adapt_ext_len, s, c);
      payload = procdata_adaptfield_frame (&space, &adapt_field_ctrl,
          adapt_flags1, size);
      d = procdata_syn_head (d, pid, scramble, adapt_field_ctrl);
      d = procdata_syn_adaptfield (s, c, d, space-payload, payload,
          adapt_field_ctrl, adapt_flags1, adapt_flags2, adapt_ext_len);
      return (procdata_syn_payload (s, c, d, payload));
      break;
    case sd_map:
      validate_mapref (s);
      return (NULL);
      break;
    case sd_unparsedsi:
      c = &s->ctrl.ptr[s->ctrl.out];
      d = output_pushdata (TS_PACKET_SIZE, FALSE, 0);
      if (d == NULL) {
        return (s);
      }
      proc_unparsedsi (s,d);
      return (NULL);
      break;
    default:
      return (NULL);
      break;
  }
}

