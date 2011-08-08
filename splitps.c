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
 * Module:  Split PS
 * Purpose: Split a program stream.
 *
 * This module examines a program stream and copies the packets to
 * the input stream buffers. PSI data is extracted, descriptors are
 * copied to the mapstream (stream 0).
 */

#include "global.h"
#include "error.h"
#include "pes.h"
#include "ps.h"
#include "input.h"
#include "splice.h"
#include "descref.h"
#include "splitpes.h"
#include "splitps.h"

static int ps_program_end_code (file_descr *f)
{
  warn (LIMP,"Program End Code",EPST,3,0,0);
  /* close file, do ... */
  return (PES_HDCODE_SIZE);
}

static int ps_pack_header (file_descr *f,
    int l)
{
  int i, s;
  long x;
  byte a, b;
  clockref oldscr; /* used for correcting bad DVB scr values */
  if (l < PS_PACKHD_SIZE) {
    warn (LDEB,"Pack header (incomplete)",EPST,2,1,l);
    return (0);
  }
  i = f->data.out;
  list_incr (i,f->data,PS_PACKHD_STUFLN);
  s = (f->data.ptr[i] & 0x07) + PS_PACKHD_SIZE;
  if (l < s) {
    warn (LDEB,"Pack header (incomplete)",EPST,2,2,l);
    return (0);
  }
  oldscr = f->u.ps.ph.scr;
  warn (LINF,"Pack header",EPST,2,0,s);
  i = f->data.out;
  list_incr (i,f->data,PS_PACKHD_SCR);
  a = f->data.ptr[i];
  marker_check (a,0x44,0xC4);
  f->u.ps.ph.scr.ba33 = (a >> 5) & 1;
  x = ((a & 0x18) | ((a & 0x03) << 1)) << 7;
  list_incr (i,f->data,1);
  x = (x | f->data.ptr[i]) << 8;
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  marker_bit (a,2);
  x = (x | ((a & 0xF8) | ((a & 0x03) << 1))) << 7;
  list_incr (i,f->data,1);
  x = (x | f->data.ptr[i]);
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  marker_bit (a,2);
  f->u.ps.ph.scr.base = (x << 5) | (a >> 3);
  warn (LSEC,"SCR base",EPST,2,3,f->u.ps.ph.scr.base);
  if (accept_weird_scr
   && ((oldscr.base - (90*40+1)) == f->u.ps.ph.scr.base)) {
    /* the DVB card produces weird scr-s, every second scr is less than
       previous one, indicating an odd value decrease of 40ms. weird! */
    f->u.ps.ph.scr.base = oldscr.base + (90*40);
  }
  list_incr (i,f->data,1);
  b = f->data.ptr[i];
  marker_bit (b,0);
  f->u.ps.ph.scr.ext = ((a & 0x03) << 7) | (b >> 1);
  warn (LSEC,"SCR ext",EPST,2,4,f->u.ps.ph.scr.ext);
  f->u.ps.ph.scr.valid = TRUE;
  cref2msec (&f->u.ps.stream[0]->u.m.conv,
      f->u.ps.ph.scr,
      &f->u.ps.stream[0]->u.m.msectime);
  warn (LDEB,"(map time)",EPST,2,5,f->u.ps.stream[0]->u.m.msectime);
  list_incr (i,f->data,1);
  x = f->data.ptr[i] << 8;
  list_incr (i,f->data,1);
  x = (x | f->data.ptr[i]) << 6;
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  marker_check (a,0x03,0x03);
  f->u.ps.ph.muxrate = x | (a >> 2);
  warn (LSEC,"muxrate",EPST,2,6,f->u.ps.ph.muxrate);
  return (s);
}

static boolean ps_system_header (file_descr *f,
    int size)
{
  int i;
  byte a, sid;
  long x;
  boolean bbs;
  int bsb;
  warn (LINF,"System header",EPST,1,0,size);
  if (size < PS_SYSTHD_STREAM) {
    warn (LWAR,"System header too short",EPST,1,1,size);
    return (FALSE);
  }
  i = f->data.out;
  list_incr (i,f->data,PES_HEADER_SIZE);
  a = f->data.ptr[i];
  marker_bit (a,7);
  x = (a & 0x7F) << 8;
  list_incr (i,f->data,1);
  x = (x | f->data.ptr[i]) << 7;
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  marker_bit (a,0);
  f->u.ps.sh.ratebound = x | (a >> 1);
  warn (LSEC,"rate_bound",EPST,1,5,f->u.ps.sh.ratebound);
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  f->u.ps.sh.csps_flag = a & 0x01;
  warn (LSEC,"csps_flag",EPST,1,6,f->u.ps.sh.csps_flag);
  f->u.ps.sh.fixed_flag = (a >>= 1) & 0x01;
  warn (LSEC,"fixed_flag",EPST,1,7,f->u.ps.sh.fixed_flag);
  f->u.ps.sh.audiobound = a >> 1;
  warn (LSEC,"audiobound",EPST,1,8,f->u.ps.sh.audiobound);
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  marker_bit (a,5);
  f->u.ps.sh.videobound = a & 0x1F;
  warn (LSEC,"videobound",EPST,1,9,f->u.ps.sh.videobound);
  f->u.ps.sh.system_video_lock_flag = (a >>= 6) & 0x01;
  warn (LSEC,
    "system_video_lock_flag",EPST,1,10,f->u.ps.sh.system_video_lock_flag);
  f->u.ps.sh.system_audio_lock_flag = a >> 1;
  warn (LSEC,
    "system_audio_lock_flag",EPST,1,11,f->u.ps.sh.system_audio_lock_flag);
  list_incr (i,f->data,1);
  f->u.ps.sh.packet_rate_restriction_flag = f->data.ptr[i] >> 7;
  warn (LSEC,"packet_rate_restriction_flag",
    EPST,1,12,f->u.ps.sh.packet_rate_restriction_flag);
  memset (f->u.ps.sh.buffer_bound,0,sizeof(f->u.ps.sh.buffer_bound));
  size -= PS_SYSTHD_STREAM;
  while ((size -= PS_SYSTHD_STRLEN) >= 0) {
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    if (a & 0x80) {
      sid = a;
      warn (LSEC,"stream_id",EPST,1,13,sid);
      list_incr (i,f->data,1);
      a = f->data.ptr[i];
      if (marker_check (a,0xC0,0xC0)) {
        warn (LWAR,"Missing 11",EPST,1,3,a);
        return (FALSE);
      }
      bbs = (a >> 5) & 0x01;
      warn (LSEC,"buffer bound scale",EPST,1,14,bbs);
      list_incr (i,f->data,1);
      bsb = ((a & 0x1F) << 8) | f->data.ptr[i];
      warn (LSEC,"buffer size bound",EPST,1,15,bsb);
      /* register stream here, if auto */
      if (a == PES_JOKER_AUDIO) {
        a = PES_CODE_AUDIO;
        x = PES_NUMB_AUDIO;
      } else if (a == PES_JOKER_VIDEO) {
        a = PES_CODE_VIDEO;
        x = PES_NUMB_VIDEO;
      } else if (a >= PES_LOWEST_SID) {
        x = 1;
      } else {
        x = 0;
      }
      if (bbs) {
        bsb = -bsb;
      }
      while (--x >= 0) {
        f->u.ps.sh.buffer_bound[a-PES_LOWEST_SID] = bsb;
        a += 1;
      }
    } else {
      warn (LWAR,"Next bit 0",EPST,1,2,a);
      return (FALSE);
    }
  }
  if (size != -PS_SYSTHD_STRLEN) {
    warn (LWAR,"System header length",EPST,1,4,size);
    return (FALSE);
  }
  return (TRUE);
}

static boolean ps_stream_map (file_descr *f,
    int size)
{
  int i, psmv, psil, esil, esml;
  byte a, styp, esid;
  boolean cni;
  warn (LINF,"Stream Map",EPST,4,0,size);
  if ((size > (1018 + PES_HEADER_SIZE))
   || (size < PS_STRMAP_SIZE)) {
    warn (LWAR,"Map size bad",EPST,4,12,size);
    return (FALSE);
  }
  i = f->data.out;
  list_incr (i,f->data,PES_HEADER_SIZE);
  a = f->data.ptr[i];
  psmv = a & 0x1F;
  warn (LSEC,"Map Version",EPST,4,1,psmv);
  alloc_descriptor (f->u.ps.stream[0],0,0,psmv);
  cni = a >> 7;
  warn (LSEC,"Current Next",EPST,4,2,cni);
  list_incr (i,f->data,2);
  psil = f->data.ptr[i] << 8;
  list_incr (i,f->data,1);
  psil = psil | f->data.ptr[i];
  list_incr (i,f->data,1);
  warn (LSEC,"PS Info Length",EPST,4,3,psil);
  size -= PS_STRMAP_SIZE;
  if (size < psil) {
    warn (LWAR,"Invalid Size",EPST,4,7,size);
    return (FALSE);
  }
  size -= psil;
  while (psil > 0) {
    i = put_descriptor (f,f->u.ps.stream[0],i,&psil);
  }
  if (psil < 0) {
    warn (LWAR,"PS Info Broken",EPST,4,4,psil);
    return (FALSE);
  }
  if (cni) {
    finish_descriptor (f->u.ps.stream[0]);
  }
  esml = f->data.ptr[i] << 8;
  list_incr (i,f->data,1);
  esml = esml | f->data.ptr[i];
  list_incr (i,f->data,1);
  warn (LSEC,"ES Map Length",EPST,4,5,esml);
  if (size != esml) {
    warn (LWAR,"Invalid Size",EPST,4,8,size);
    return (FALSE);
  }
  while (esml > 0) {
    styp = f->data.ptr[i];
    list_incr (i,f->data,1);
    esid = f->data.ptr[i];
    list_incr (i,f->data,1);
    esil = f->data.ptr[i] << 8;
    list_incr (i,f->data,1);
    esil = esil | f->data.ptr[i];
    list_incr (i,f->data,1);
    warn (LSEC,"Stream Type",EPST,4,9,styp);
    warn (LSEC,"E Stream Id",EPST,4,10,esid);
    warn (LSEC,"ES Info Length",EPST,4,11,esil);
    if (esid >= PES_LOWEST_SID) {
      if (f->automatic) {
        if (f->u.ps.stream[esid] == NULL) {
          f->u.ps.stream[esid] =
            connect_streamprog (
                f,f->auto_programnb,esid,-esid,styp,
                NULL,f->u.ps.stream[0],FALSE);
        }
      }
    }
    esml -= (esil + 4);
    if (esml >= 0) {
      alloc_descriptor (f->u.ps.stream[0],esid,0,psmv);
      while (esil > 0) {
        i = put_descriptor (f,f->u.ps.stream[0],i,&esil);
      }
      if (esil < 0) {
        warn (LWAR,"ES Map Broken",EPST,4,13,esil);
        return (FALSE);
      }
      if (cni) {
        finish_descriptor (f->u.ps.stream[0]);
      }
    }
  }
  if (esml < 0) {
    warn (LWAR,"ES Map Broken",EPST,4,6,esml);
    return (FALSE);
  }
  return (TRUE);
}

/* Parse a 45 bit stream directory offset value in 3 parts a 15 bit.
 * Precondition: f!=NULL, result!=NULL.
 * Postcontition: *result = offset.
 * Return: Index increased by 6.
 */
static int ps_stream_dir_get45 (file_descr *f,
    int i,
    long long *result)
{
  byte a;
  int b, n;
  long long r;
  n = 2;
  r = 0;
  do {
    list_incr (i,f->data,1);
    b = f->data.ptr[i];
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    marker_bit (a,0);
    b = (b << 7) | (a >> 1);
    r = (r << 15) | b;
  } while (--n >= 0);
  *result = r;
  return (i);
}

static boolean ps_stream_directory (file_descr *f,
    int size)
{
  int i, n;
  long x;
  byte a;
  int numoau;
  long long prevdo, nextdo;
  warn (LINF,"Stream Dir",EPST,5,0,0);
  i = f->data.out;
  list_incr (i,f->data,PES_HEADER_SIZE);
  x = f->data.ptr[i];
  list_incr (i,f->data,1);
  a = f->data.ptr[i];
  marker_bit (a,0);
  numoau = (x << 7) | (a >> 1);
  warn (LSEC,"Num Acces Units",EPST,5,1,numoau);
  if (size != (PS_STRDIR_SIZE + numoau * PS_STRDIR_SIZEAU)) {
    warn (LWAR,"Invalid Size",EPST,5,2,size);
    return (FALSE);
  }
  i = ps_stream_dir_get45 (f,i,&prevdo);
  warn (LSEC,"Prev Dir Offset",EPST,5,3,((long)prevdo));
  i = ps_stream_dir_get45 (f,i,&nextdo);
  warn (LSEC,"Next Dir Offset",EPST,5,4,((long)nextdo));
  n = 0;
  while (n < numoau) {
    byte psid;
    boolean idi;
    int refo, btr, cpi;
    long long headpo;
    clockref pts; /* and process all this ... */
    list_incr (i,f->data,1);
    psid = f->data.ptr[i];
    warn (LSEC,"Packet Str Id",EPST,5,5,psid);
    i = ps_stream_dir_get45 (f,i,&headpo);
    if (headpo & (1LL << 44)) {
      headpo = (1LL << 44) - headpo;
    }
    warn (LSEC,"Head Pos Offset",EPST,5,6,((long)headpo));
    list_incr (i,f->data,1);
    refo = f->data.ptr[i];
    list_incr (i,f->data,1);
    refo = (refo << 8) | f->data.ptr[i];
    warn (LSEC,"Reference Offset",EPST,5,7,refo);
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    marker_check (a,0x81,0x81);
    pts.ba33 = (a >> 3) & 1;
    x = a & 0x06;
    list_incr (i,f->data,1);
    x = (x << 7) | f->data.ptr[i];
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    marker_bit (a,0);
    x = (x << 8) | (a & 0xFE);
    list_incr (i,f->data,1);
    x = (x << 7) | f->data.ptr[i];
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    marker_bit (a,0);
    pts.base = (x << 7) | (a >> 1);
    pts.ext = 0;
    warn (LSEC,"PTS base",EPST,5,8,pts.base);
    list_incr (i,f->data,1);
    btr = f->data.ptr[i];
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    marker_bit (a,0);
    btr = (btr << 8) | (a & 0xFE);
    list_incr (i,f->data,1);
    btr = (btr << 7) | f->data.ptr[i];
    list_incr (i,f->data,1);
    a = f->data.ptr[i];
    marker_bit (a,7);
    cpi = (a >> 4) & 0x03;
    idi = (a >> 6) & 0x01;
    n += 1;
  }
  return (TRUE);
}

static boolean ps_data_stream (file_descr *f,
    int size,
    byte sourceid)
{
  stream_descr *s;
  ctrl_buffer *c;
  warn (LINF,"Data Stream",EPST,6,0,size);
  if ((f->u.ps.stream[sourceid] == NULL)
   && (f->automatic)) {
    f->u.ps.stream[sourceid] =
      connect_streamprog (f,f->auto_programnb,sourceid,-sourceid,
          guess_streamtype(sourceid),NULL,f->u.ps.stream[0],FALSE);
  }
  s = f->u.ps.stream[sourceid];
  if (s != NULL) {
    if ((!list_full (s->ctrl))
     && (list_free (s->data) >= 2*size-1)) {
      c = &s->ctrl.ptr[s->ctrl.in];
      c->length = size;
      f->payload += size;
      f->total += size;
      c->index = pes_transfer (&f->data,&s->data,size);
      warn (LDEB,"Sequence",EPST,6,1,f->sequence);
      c->sequence = f->sequence++;
      c->scramble = 0;
      c->msecread = msec_now ();
      c->msecpush = f->u.ps.stream[0]->u.m.msectime;
      c->pcr.valid = FALSE;
      c->opcr.valid = FALSE;
      list_incr (s->ctrl.in,s->ctrl,1);
      return (TRUE);
    }
    return (FALSE);
  }
  f->total += size;
  list_incr (f->data.out,f->data,size);
  return (TRUE);
}

/* Split data from a PS stream.
 * Precondition: f!=NULL
 * Return: TRUE, if something was processed, FALSE if no data/space available
 */
boolean split_ps (file_descr *f)
{
  int l, p;
  byte a;
  warn (LDEB,"Split PS",EPST,0,0,f);
  if (pes_skip_to_prefix (f)) {
    l = list_size (f->data);
    if (l >= PES_HDCODE_SIZE) {
      a = pes_stream_id (&f->data);
      if (a >= PS_CODE_SYST_HDR) {
        if (l >= PES_HEADER_SIZE) {
          p = pes_packet_length (&f->data);
          p += PES_HEADER_SIZE;
          if (l >= p) {
            switch (a) {
              case PS_CODE_SYST_HDR:
                if (!ps_system_header (f,p)) {
                  p = PES_SYNC_SIZE;
                }
                break;
              case PES_CODE_STR_MAP:
                if (!ps_stream_map (f,p)) {
                  p = PES_SYNC_SIZE;
                }
                break;
              case PES_CODE_PADDING:
                break;
              case PES_CODE_PRIVATE2:
/*                p = PES_SYNC_SIZE; */
                break;
              case PES_CODE_ECM:
/*                p = PES_SYNC_SIZE; */
                break;
              case PES_CODE_EMM:
/*                p = PES_SYNC_SIZE; */
                break;
              case PES_CODE_DSMCC:
/*                p = PES_SYNC_SIZE; */
                break;
              case PES_CODE_ITU222E:
/*                p = PES_SYNC_SIZE; */
                break;
              case PES_CODE_STR_DIR:
                if (!ps_stream_directory (f,p)) {
                  p = PES_SYNC_SIZE;
                }
                break;
              default:
                return (ps_data_stream (f,p,a));
                break;
            }
          } else {
            p = 0;
          }
        } else {
          p = 0;
        }
      } else if (a == PS_CODE_END) {
        p = ps_program_end_code (f);
      } else if (a == PS_CODE_PACK_HDR) {
        p = ps_pack_header (f,l);
      } else {
        warn (LWAR,"Unknown Stream Id",EPST,0,1,a);
        p = PES_SYNC_SIZE;
      }
      if (p > 0) {
        f->total += p;
        list_incr (f->data.out,f->data,p);
        return (TRUE);
      }
    }
  }
  return (FALSE);
}

