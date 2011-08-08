/*
 * ISO 13818 stream multiplexer
 * Copyright (C) 2001 Convergence Integrated Media GmbH Berlin
 * Copyright (C) 2005 Oskar Schirmer (schirmer@scara.com)
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
 * Module:  Splice PS
 * Purpose: Generate a program stream
 *
 * This module generates from the available input stream data (as
 * seperated by the split functions) the complete output stream.
 * It provides functions to handle programs for the resulting stream,
 * as these are output format dependent (though multiple programs are
 * obsolete for a program stream). Further, it accepts PSI data
 * just in time, validating it not earlier than with the arrival of
 * the corresponding payload at this stage.
 */

#include "global.h"
#include "crc32.h"
#include "error.h"
#include "input.h"
#include "output.h"
#include "descref.h"
#include "pes.h"
#include "ps.h"
#include "splice.h"
#include "spliceps.h"

const boolean splice_multipleprograms = FALSE;

static int psi_size;
static byte psi_data [MAX_PSI_SIZE + PS_SYSTHD_SIZE + PS_STRMAP_SIZE +
    MAX_STRPERPRG * (PS_SYSTHD_STRLEN + PS_STRMAP_STRLEN)];

static prog_descr prog;

boolean splice_specific_init (void)
{
  prog.program_number = 0;
  prog.pmt_conticnt = 0;
  prog.pmt_version = 0;
  prog.changed = TRUE;
  prog.streams = 0;
  prog.stump = NULL;
  clear_descrdescr (&prog.manudescr);
  psi_size = 0;
  return (TRUE);
}

void splice_settransportstreamid (int tsid)
{
}

void splice_setpsifrequency (t_msec freq)
{
  psi_frequency_msec = freq;
  psi_frequency_changed = TRUE;
}

void splice_setnetworkpid (short pid)
{
}

void splice_all_configuration (void)
{
  if (configuration_must_print) {
    fprintf (stderr, configuration_total, 1);
    splice_one_configuration (&prog);
    configuration_was_printed;
  }
}

void splice_addsirange (file_descr *f,
    int lower,
    int upper)
{
}

void splice_createstump (int programnb,
    short pid,
    byte styp)
{
}
 
stump_descr *splice_getstumps (int programnb,
    short pid)
{
  return (NULL);
}

void splice_modifytargetdescriptor (int programnb,
    short sid,
    short pid,
    int dtag,
    int dlength,
    byte *data)
{
  splice_modifytargetdescrprog (&prog,0,sid,pid,dtag,dlength,data,NULL);
}

prog_descr *splice_getprog (int programnb)
{
  return (&prog);
}

prog_descr *splice_openprog (int programnb)
{
/*  prog.program_number = programnb;  must stay zero */
/*  configuration_changed = TRUE; */
  warn (LIMP,"Open prog",EPSC,1,0,programnb);
  return (&prog);
}

void splice_closeprog (prog_descr *p)
{
  stream_descr *s;
  file_descr *f;
  warn (LIMP,"Close prog",EPSC,2,0,prog.streams);
  while (prog.streams > 0) {
    s = prog.stream[0];
    input_delprog (s,&prog);
    splice_delstream (&prog,s);
    if (s->u.d.progs == 0) {
      f = s->fdescr;
      input_closestream (s);
      input_closefileifunused (f);
    } else {
      warn (LERR,"Close prog",EPSC,2,s->u.d.progs,s->stream_id);
    }
  }
  configuration_changed = TRUE;
}

int splice_addstream (prog_descr *p,
    stream_descr *s,
    boolean force_sid)
{
  /* add stream to main program */
  int sid = 0;
  warn (LIMP,"Add stream",EPSC,3,force_sid,s->stream_id);
  if (prog.streams < MAX_STRPERPRG) {
    if (!force_sid) {
      s->stream_id = splice_findfreestreamid (&prog,s->stream_id);
    }
    prog.stream[prog.streams++] = s;
    prog.changed = TRUE;
    s->u.d.pid = sid = s->stream_id;
    configuration_changed = TRUE;
    splice_modifycheckmatch (0,&prog,s,NULL);
  }
  return (sid);
}

boolean splice_delstream (prog_descr *p,
    stream_descr *s)
{
  int i;
  warn (LIMP,"Del stream",EPSC,4,0,s->u.d.pid);
  i = p->streams;
  while (--i >= 0) {
    if (p->stream[i] == s) {
      prog.stream[i] = prog.stream[--(prog.streams)];
      prog.changed = TRUE;
      s->u.d.pid = 0;
      configuration_changed = TRUE;
      return (TRUE);
    }
  }
  warn (LERR,"Del lost stream",EPSC,4,1,p->streams);
  return (FALSE);
}

void process_finish (void)
{
  byte *d;
  warn (LIMP,"Finish",EPSC,5,0,0);
  d = output_pushdata (PES_HDCODE_SIZE, FALSE, 0);
  if (d != NULL) {
    *d++ = 0x00;
    *d++ = 0x00;
    *d++ = 0x01;
    *d++ = PS_CODE_END;
    warn (LIMP,"Finish Done",EPSC,5,1,0);
  }
}

static int make_systemheader (stream_descr *s,
    byte *dest)
{
  int i, pid;
  byte v, a;
  byte *d;
  d = dest;
  *d++ = 0x00;
  *d++ = 0x00;
  *d++ = 0x01;
  *d++ = PS_CODE_SYST_HDR;
  d += 2;
  if (s->fdescr->content == ct_program) {
    i = s->fdescr->u.ps.sh.ratebound;
  } else {
    i = 4500000 / 400; /* wrong for sure */
  }
  *d++ = 0x80
       | (i >> 15);
  *d++ = (i >> 7);
  *d++ = (i << 1)
       | 0x01;
  d += 2;
  *d++ = 0 /* packet_rate_restriction_flag */
       | 0x7F;
  a = v = 0;
  i = prog.streams;
  while (--i >= 0) {
    pid = prog.stream[i]->u.d.pid;
    if ((pid >= PES_CODE_VIDEO)
     && (pid < (PES_CODE_VIDEO + PES_NUMB_VIDEO))) {
      if (v == 0) {
        *d++ = PES_JOKER_AUDIO;
        *d++ = 0xC0
             | (0 << 5) /* buffer bound scale */
             | ((4096/128) >> 8);
        *d++ = (4096/128) & 0xFF; /* fixme! propagate from input, or derive */
      }
      v += 1;
    } else {
      if ((pid >= PES_CODE_AUDIO)
       && (pid < (PES_CODE_AUDIO + PES_NUMB_AUDIO))) {
        if (a == 0) {
          *d++ = PES_JOKER_VIDEO;
          *d++ = 0xC0
               | (0 << 5) /* buffer bound scale */
               | ((237568/128) >> 8);
          *d++ = (237568/128) & 0xFF;
        }
        a += 1;
      } else {
        *d++ = pid;
        *d++ = 0xC0
             | (0 << 5) /* buffer bound scale */
             | ((2048/128) >> 8);
        *d++ = (2048/128) & 0xFF;
      }
    }
  }
  dest[PS_SYSTHD_AUDBND] = (a << 2)
       | 0 /* fixed_flag */
       | 0; /* CSPS_flag */
  dest[PS_SYSTHD_VIDBND] = 0 /* system_audio_lock_flag */
       | 0 /* system_video_lock_flag */
       | 0x20
       | v;
  i = d - dest - PES_HEADER_SIZE;
  dest[PES_PACKET_LENGTH] = (i >> 8);
  dest[PES_PACKET_LENGTH+1] = i;
  return (i + PES_HEADER_SIZE);
}

static int make_streammap (stream_descr *s,
    byte *dest)
{
  int i, l;
  byte *d;
  stream_descr *t;
  d = dest;
  *d++ = 0x00;
  *d++ = 0x00;
  *d++ = 0x01;
  *d++ = PES_CODE_STR_MAP;
  d += 2;
  *d++ = (1 << 7) /* current applicable */
       | 0x60
       | prog.pmt_version;
  *d++ = 0xFE
       | 0x01;
  d += 2;
  i = NUMBER_DESCR;
  while (--i >= 0) {
    byte *y;
    y = prog.manudescr.refx[i];
    if ((y == NULL)
     && (s->u.d.mapstream != NULL)) {
      y = s->u.d.mapstream->autodescr->refx[i];
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
  l = d - dest - PS_STRMAP_PSID;
  dest[PS_STRMAP_PSIL] = (l >> 8);
  dest[PS_STRMAP_PSIL+1] = l;
  d += 2;
  i = prog.streams;
  while (--i >= 0) {
    t = prog.stream[i];
    if (t->u.d.mention) {
      int x;
      byte *e;
      *d++ = t->stream_type;
      *d++ = t->u.d.pid;
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
      *--e = (x >> 8);
    }
  }
  i = d - dest - l - (PS_STRMAP_SIZE - CRC_SIZE);
  dest[PS_STRMAP_PSID + l] = (i >> 8);
  dest[PS_STRMAP_PSID+l+1] = i;
  i = d + CRC_SIZE - dest - PES_HEADER_SIZE;
  dest[PES_PACKET_LENGTH] = (i >> 8);
  dest[PES_PACKET_LENGTH+1] = i;
  crc32_calc (dest,i + PES_HEADER_SIZE - CRC_SIZE,d);
  return (i + PES_HEADER_SIZE);
}

stream_descr *process_something (stream_descr *s)
{
  byte *d;
  ctrl_buffer *c;
  clockref pcr;
  int i;
  t_msec now;
  warn (LDEB,"Splice PS",EPSC,0,0,s->ctrl.out);
  if (s->streamdata == sd_map) {
    validate_mapref (s);
    return (NULL);
  }
  c = &s->ctrl.ptr[s->ctrl.out];
  now = msec_now ();
  if ((psi_frequency_changed)
   || ((psi_frequency_msec > 0)
    && ((next_psi_periodic - now) <= 0))) {
    prog.unchanged = TRUE;
    psi_frequency_changed = FALSE;
    next_psi_periodic = now + psi_frequency_msec;
  }
  if (prog.unchanged || prog.changed) {
    if (prog.changed) {
      prog.pmt_version = (prog.pmt_version+1) & 0x1F;
    }
    psi_size = make_systemheader (s,&psi_data[0]);
    psi_size += make_streammap (s,&psi_data[psi_size]);
    prog.changed = FALSE;
    prog.unchanged = FALSE;
  }
  i = c->msecpush + s->u.d.delta;
  d = output_pushdata (PS_PACKHD_SIZE + psi_size + c->length, TRUE, i);
  if (d == NULL) {
    return (s);
  }
  *d++ = 0x00;
  *d++ = 0x00;
  *d++ = 0x01;
  *d++ = PS_CODE_PACK_HDR;
  msec2cref (&s->u.d.conv, i, &pcr);
  *d++ = 0x40
       | (((pcr.ba33 << 5) | (pcr.base >> 27)) & 0x38)
       | 0x04
       | ((pcr.base >> 28) & 0x03);
  *d++ = (pcr.base >> 20);
  *d++ = ((pcr.base >> 12) & 0xF8)
       | 0x04
       | ((pcr.base >> 13) & 0x03);
  *d++ = (pcr.base >> 5);
  *d++ = ((pcr.base << 3) & 0xF8)
       | 0x04
       | ((pcr.ext >> 7) & 0x03);
  *d++ = (pcr.ext << 1)
       | 0x01;
  if (s->fdescr->content == ct_program) {
    i = s->fdescr->u.ps.ph.muxrate; /* wrong, because maybe old */
  } else {
    i = 4500000 / 400; /* xxx wrong for sure */
  }
  *d++ = (i >> 14);
  *d++ = (i >> 6);
  *d++ = (i << 2)
       | 0x03;
  *d++ = 0xF8
       | 0; /* stuffing length */
  if (psi_size > 0) {
    memcpy (d,&psi_data[0],psi_size);
    d += psi_size;
    psi_size = 0;
  }
  memcpy (d,&s->data.ptr[c->index],c->length);
  d[PES_STREAM_ID] = s->stream_id;
  warn (LINF,"Splice Done",EPSC,0,s->stream_id,c->length);
  list_incr (s->ctrl.out,s->ctrl,1);
  if (list_empty (s->ctrl)) {
    s->data.out = s->data.in;
  } else {
    s->data.out = s->ctrl.ptr[s->ctrl.out].index;
  }
  return (NULL);
}

