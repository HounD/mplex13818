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
 * Module:  Split TS
 * Purpose: Split a transport stream.
 *
 * This module examines a transport stream and collects the packets to
 * the input stream buffers. PSI data is extracted (and stored in the
 * files pat structures), descriptors are copied to the corresponding
 * mapstreams.
 */

#include "global.h"
#include "error.h"
#include "input.h"
#include "splice.h"
#include "ts.h"
#include "pes.h"
#include "descref.h"
#include "splitpes.h"
#include "splitts.h"
#include "crc32.h"

static byte pusi, afcc, afflg1;
static int paylen;

/* Skip in input raw data buffer for TS-syncbyte.
 * Precondition: f!=NULL
 * Postcondition: if found: f->data.out indicates the syncbyte.
 * Return: TRUE if found, FALSE otherwise
 */
static boolean ts_skip_to_syncbyte (file_descr *f)
{
  int i, l, k;
  boolean r = FALSE;
  l = k = list_size (f->data);
  i = f->data.out;
  while ((l > 0)
      && ((f->data.ptr[i] != TS_SYNC_BYTE) || (r = TRUE, FALSE))) {
    list_incr (i,f->data,1);
    l -= 1;
  }
  k -= l;
  if (k > 0) {
    warn (LWAR,"Skipped",ETST,1,1,k);
    f->skipped += k;
    f->total += k;
    list_incr (f->data.out,f->data,k);
  }
  return (r);
}

/* Determine the PID of a TS packet and other details.
 * Procondition: d!=NULL points to a packet.
 * Postcondition: Set global "Payload Unit Start Indicator", "Adaption Field
 * Control & Continuity Counter", "Adataption Field Flags 1".
 * Return: 13 bit PID.
 */
static int ts_packet_headinfo (refr_data *d)
{
  int i, l;
  i = d->out;
  list_incr (i,*d,TS_PACKET_PID);
  l = (pusi = d->ptr[i]) & 0x1F;
  list_incr (i,*d,1);
  l = (l << 8) | d->ptr[i];
  list_incr (i,*d,1);
  if (!((afcc = d->ptr[i]) & TS_AFC_BOTH)) {
    l = TS_PID_NULL;
  } else {
    if (afcc & TS_AFC_ADAPT) {
      byte aflen;
      list_incr (i,*d,1);
      if ((aflen = d->ptr[i]) == 0) {
        afflg1 = 0x00;
      } else {
        list_incr (i,*d,1);
        afflg1 = d->ptr[i];
      }
      paylen = TS_PACKET_SIZE - TS_PACKET_FLAGS1 - aflen;
    } else {
      paylen = TS_PACKET_SIZE - TS_PACKET_HEADSIZE;
    }
  }
  warn (LSEC,"Packet PID",ETST,2,1,l);
  warn (LDEB,"Packet PID",ETST,afcc,afflg1,paylen);
  return (l);
}

/* Extract adaption field from a TS packet.
 * Distribute the information as needed (to s, s->ctrl, etc).
 * Precondition: f!=NULL, adf indicates a packet in f->data, s!=NULL is the
 * destined data stream, m!=NULL the map stream, afcc and afflg1 set.
 */
static void ts_adaption_field (file_descr *f,
    int adf,
    stream_descr *s,
    stream_descr *m)
{
  warn (LDEB,"AdaptF",ETST,11,adf,afcc);
  if (afcc & TS_AFC_ADAPT) {
    list_incr (adf,f->data,TS_PACKET_FLAGS1+1);
    if (afflg1 & TS_ADAPT_DISCONTI) {
    }
    if (afflg1 & TS_ADAPT_RANDOMAC) {
    }
    if (afflg1 & TS_ADAPT_PRIORITY) {
    }
    if (afflg1 & TS_ADAPT_PCRFLAG) {
      clockref *pcr;
      long b;
      byte a;
      pcr = &s->ctrl.ptr[s->ctrl.in].pcr;
      a = f->data.ptr[adf];
      list_incr (adf,f->data,1);
      pcr->ba33 = (a >> 7) & 1;
      b = (a << 8) | f->data.ptr[adf];
      list_incr (adf,f->data,1);
      b = (b << 8) | f->data.ptr[adf];
      list_incr (adf,f->data,1);
      b = (b << 8) | f->data.ptr[adf];
      list_incr (adf,f->data,1);
      a = f->data.ptr[adf];
      pcr->base = (b << 1) | ((a >> 7) & 1);
      marker_check (a,0x7E,0x7E);
      list_incr (adf,f->data,1);
      pcr->ext = ((a & 1) << 8) | f->data.ptr[adf];
      warn (LINF,"PCR",ETST,10,pcr->ba33,pcr->base);
      pcr->valid = TRUE;
      list_incr (adf,f->data,1);
/* attention ! what if it is not PCR_PID ? xxx */
      if (S_ISREG (f->st_mode)) {
        cref2msec (&m->u.m.conv, *pcr, &m->u.m.msectime); 
      } else {
        cref2msec (&m->u.m.conv, *pcr, &m->u.m.msectime); 
      }
    }
    if (afflg1 & TS_ADAPT_OPCRFLAG) {
      clockref *opcr;
      long b;
      byte a;
      opcr = &s->ctrl.ptr[s->ctrl.in].opcr;
      a = f->data.ptr[adf];
      list_incr (adf,f->data,1);
      opcr->ba33 = (a >> 7) & 1;
      b = (a << 8) | f->data.ptr[adf];
      list_incr (adf,f->data,1);
      b = (b << 8) | f->data.ptr[adf];
      list_incr (adf,f->data,1);
      b = (b << 8) | f->data.ptr[adf];
      list_incr (adf,f->data,1);
      a = f->data.ptr[adf];
      opcr->base = (b << 1) | ((a >> 7) & 1);
      marker_check (a,0x7E,0x7E);
      list_incr (adf,f->data,1);
      opcr->ext = ((a & 1) << 8) | f->data.ptr[adf];
      warn (LINF,"OPCR",ETST,10,opcr->ba33,opcr->base);
      opcr->valid = TRUE;
      s->u.d.has_opcr = TRUE;
      list_incr (adf,f->data,1);
    }
    if (afflg1 & TS_ADAPT_SPLICING) {
      list_incr (adf,f->data,1);
    }
    if (afflg1 & TS_ADAPT_TPRIVATE) {
      byte tpdl;
      tpdl = f->data.ptr[adf];
      list_incr (adf,f->data,tpdl+1);
    }
    if (afflg1 & TS_ADAPT_EXTENSIO) {
      byte afflg2, afel;
      afel = f->data.ptr[adf];
      list_incr (adf,f->data,1);
      afflg2 = f->data.ptr[adf];
      list_incr (adf,f->data,1);
      if (afflg2 & TS_ADAPT2_LTWFLAG) {
        list_incr (adf,f->data,2);
      }
      if (afflg2 & TS_ADAPT2_PIECEWRF) {
        list_incr (adf,f->data,3);
      }
      if (afflg2 & TS_ADAPT2_SEAMLESS) {
        list_incr (adf,f->data,5);
      }
    }
  }
}

/* Parse one TS packet with given PID.
 * Depending on the actual state (c->length) and the contents of the packet,
 * provide the data into the stream and possibly complete a PES package.
 * c->length keeps the progress of data acquisition, as follows:
 *  =0: initial, waiting for payload unit start indicator
 *  <-2, increasing: not yet enough data to evaluate the PES header
 *  =-2: evaluate the PES packet length field
 *  =-1: acquiring packet with unspecified length
 *  >0, decreasing: amount of data still missing
 *  =0: final, close PES packet, start new one
 * Precondition: f!=NULL.
 * Return: TRUE if something was processed, FALSE if no data/space available
 */
static boolean ts_data_stream (file_descr *f,
    int pid)
{
  stream_descr *s;
  ctrl_buffer *c;
  int i, fdo, sdi, adf;
  warn (LDEB,"Data Packet",ETST,3,0,pid);
  s = ts_file_stream (f,pid);
  if (s != NULL) {
    if (!list_full (s->ctrl)) {
      c = &s->ctrl.ptr[s->ctrl.in];
      if (c->length == -1) {
        if (pusi & TS_UNIT_START) {
          c->length = s->data.in - c->index;
          warn (LINF,"Closed unbound packet",ETST,3,5,c->length);
          f->payload += c->length;
          c->sequence = f->sequence++;
          c->scramble = 0;
          c->msecread = msec_now ();
          c->msecpush = s->u.d.mapstream->u.m.msectime;
          list_incr (s->ctrl.in,s->ctrl,1);
          c = &s->ctrl.ptr[s->ctrl.in];
          c->length = 0;
          c->pcr.valid = FALSE;
          c->opcr.valid = FALSE;
        }
      }
      if (c->length == 0) {
        if (pusi & TS_UNIT_START) {
          if (list_free (s->data) >=
              (2*(PES_HEADER_SIZE-1+TS_PACKET_SIZE-TS_PACKET_HEADSIZE)-1)) {
            if ((PES_HEADER_SIZE-1+TS_PACKET_SIZE-TS_PACKET_HEADSIZE)
                > list_freeinend (s->data)) {
              s->data.in = 0;
            }
            c->index = s->data.in;
            c->length = -2 - PES_HEADER_SIZE;
          } else {
            return (FALSE);
          }
        } else {
          f->skipped += TS_PACKET_SIZE;
          list_incr (f->data.out,f->data,TS_PACKET_SIZE);
          f->total += TS_PACKET_SIZE;
          return (TRUE);
        }
      }
      sdi = s->data.in;
      fdo = adf = f->data.out;
      list_incr (fdo,f->data,TS_PACKET_SIZE - paylen);
      if (c->length == -1) {
        if (list_freecachedin (s->data,sdi) >= paylen) {
          i = list_freeinendcachedin (s->data,sdi);
          if (i <= paylen) {
            if (list_freecachedin (s->data,sdi) >=
                (i + (sdi - c->index) + paylen)) {
              sdi -= c->index;
              memmove (&s->data.ptr[0],&s->data.ptr[c->index],sdi);
              c->index = 0;
            } else {
              return (FALSE);
            }
          }
          while (paylen > 0) {
            i = MAX_DATA_RAWB - fdo;
            if (i > paylen) {
              i = paylen;
            }
            memcpy (&s->data.ptr[sdi],&f->data.ptr[fdo],i);
            list_incr (sdi,s->data,i);
            list_incr (fdo,f->data,i);
            paylen -= i;
          }
        } else {
          return (FALSE);
        }
      }
      if (c->length < -2) {
        c->length += paylen;
        while (paylen > 0) {
          i = MAX_DATA_RAWB - fdo;
          if (i > paylen) {
            i = paylen;
          }
          memcpy (&s->data.ptr[sdi],&f->data.ptr[fdo],i);
          list_incr (sdi,s->data,i);
          list_incr (fdo,f->data,i);
          paylen -= i;
        }
        if (c->length > -2) {
          c->length = -2;
        }
      }
      if (c->length == -2) {
        if ((s->data.ptr[c->index] != 0x00)
         || (s->data.ptr[c->index+1] != 0x00)
         || (s->data.ptr[c->index+2] != 0x01)) {
          warn (LWAR,"Payload not good PES",ETST,3,3,0);
          c->length = 0;
          return (TRUE);
        }
        i = (s->data.ptr[c->index+PES_PACKET_LENGTH] << 8)
           | s->data.ptr[c->index+PES_PACKET_LENGTH+1];
        if (i == 0) {
          warn (LSEC,"Payload length 0",ETST,3,4,0);
          c->length = -1;
        } else {
          if (i > list_freeinendcachedin (s->data,c->index) - PES_HEADER_SIZE) {
            if (list_freecachedin (s->data,sdi) <
                  (2 * (i + PES_HEADER_SIZE) - (sdi - c->index) - 1)) {
              return (FALSE);
            } else {
              sdi -= c->index;
              memmove (&s->data.ptr[0],&s->data.ptr[c->index],sdi);
              c->index = 0;
            }
          } else {
            if (list_freecachedin (s->data,sdi) <
                  (i + PES_HEADER_SIZE - (sdi - c->index))) {
              return (FALSE);
            }
          }
          c->length = i + PES_HEADER_SIZE - (sdi - c->index);
          if (c->length < 0) {
            warn (LWAR,"Too much payload",ETST,3,1,c->length);
            c->length = 0;
          }
        }
      }
      if ((c->length > 0)
       && (paylen > 0)) {
        if (paylen > c->length) {
          warn (LWAR,"Too much payload",ETST,3,2,c->length);
          paylen = c->length;
        }
        c->length -= paylen;
        while (paylen > 0) {
          i = MAX_DATA_RAWB - fdo;
          if (i > paylen) {
            i = paylen;
          }
          memcpy (&s->data.ptr[sdi],&f->data.ptr[fdo],i);
          list_incr (sdi,s->data,i);
          list_incr (fdo,f->data,i);
          paylen -= i;
        }
      }
      s->data.in = sdi;
      f->data.out = fdo;
      ts_adaption_field (f,adf,s,s->u.d.mapstream);
      if (c->length == 0) {
        c->length = s->data.in - c->index;
        f->payload += c->length;
        c->sequence = f->sequence++;
        c->scramble = 0;
        c->msecread = msec_now ();
        c->msecpush = s->u.d.mapstream->u.m.msectime;
        list_incr (s->ctrl.in,s->ctrl,1);
        c = &s->ctrl.ptr[s->ctrl.in];
        c->length = 0;
        c->pcr.valid = FALSE;
        c->opcr.valid = FALSE;
      }
    } else {
      return (FALSE);
    }
  } else {
    list_incr (f->data.out,f->data,TS_PACKET_SIZE);
  }
  f->total += TS_PACKET_SIZE;
  return (TRUE);
}

/* Process changes that arise from a new PMT.
 * Match the new PMT with the existing PAT/PMT and adjust this to
 * reflect the changes in the TS. Try to keep PIDs etc stable.
 * Precondition: f!=NULL, new!=NULL the partial new PMT
 */
static void remap_new_program (file_descr *f,
    pmt_descr *new)
{
  pmt_descr* p;
  int i, j, k;
  byte *d;
  struct {
    boolean tnew : 1;
    boolean tpat : 1;
  } treated[MAX_PMTSTREAMS];
  warn (LINF,"Remap",ETST,7,new->descrlen,new->pmt_pid);
  p = f->u.ts.pat;
  while ((p != NULL) && (p->programnumber != new->programnumber)) {
    p = p->next;
  }
  warn (LDEB,"Remap 2",ETST,p?p->streams:0,(long)f->u.ts.pat,p);
  if (p != NULL) {
  warn (LDEB,"Remap 2a",ETST,p->pmt_version,new->pmt_version,new);
    if (p->pmt_version == new->pmt_version) {
      /* check more here, or fall through ? */
      p->pat_section = new->pat_section; /* announce presence */
      return;
    }
  } else {
    if ((p = malloc (sizeof(pmt_descr))) == NULL) {
      warn (LERR,"PAT malloc failed",ETST,7,1,sizeof(pmt_descr));
      return;
    }
    p->next = f->u.ts.pat;
    p->programnumber = new->programnumber;
    p->streams = 0;
    f->u.ts.pat = p;
    warn (LIMP,"Remap PAT",ETST,7,new->pat_section,new->pmt_pid);
  }
  p->pat_section = new->pat_section;
  p->pmt_version = new->pmt_version;
  p->pcr_pid = new->pcr_pid;
  if (ts_file_stream(f,new->pmt_pid) == NULL) {
    if ((ts_file_stream(f,new->pmt_pid) =
        input_openstream (f,new->pmt_pid,0,0,sd_map,NULL)) == NULL) {
      p->pat_section = -1;
      return;
    }
  }
  p->pmt_pid = new->pmt_pid;
  memset (&treated[0],0,sizeof(treated));
  i = new->streams;
  while (--i >= 0) {
    j = p->streams;
    warn (LDEB,"Remap 3",ETST,0,i,j);
    while ((--j >= 0)
        && (p->streamtype[j] != new->streamtype[i])
        && (p->stream[j] != new->stream[i])) {
    }
    if (j >= 0) {
      treated[j].tpat = TRUE;
      treated[i].tnew = TRUE;
      warn (LIMP,"Remap Match",ETST,7,new->streamtype[i],new->stream[i]);
    }
  }
  i = new->streams;
  while (--i >= 0) {
    if (!treated[i].tnew) {
      j = p->streams;
      while ((--j >= 0)
          && (treated[j].tpat)
          && (p->streamtype[j] != new->streamtype[i])) {
      }
      if (j >= 0) {
        treated[j].tpat = TRUE;
        warn (LIMP,"Remap Move",ETST,7,p->stream[j],new->stream[i]);
        warn (LSEC,"Remap Move",ETST,7,p->streamtype[j],new->streamtype[i]);
        if (ts_file_stream(f,p->stream[j]) != NULL) {
          k = ts_file_stream(f,p->stream[j])->u.d.progs;
          while (--k >= 0) {
            ts_file_stream(f,new->stream[i]) =
              connect_streamprog (
                f,ts_file_stream(f,p->stream[j])->u.d.pdescr[k]->program_number,
                new->stream[i],
                ts_file_stream(f,p->stream[j])->stream_id,new->streamtype[i],
                ts_file_stream(f,new->stream[i]),
                ts_file_stream(f,new->pmt_pid),FALSE);
          }
          if (ts_file_stream(f,new->stream[i]) != NULL) {
            ts_file_stream(f,new->stream[i])->u.d.trigger = TRUE;
          }
          ts_file_stream(f,p->stream[j])->endaction = ENDSTR_KILL;
        } else {
          /* was not used -> will not use */
        }
      } else {
        warn (LIMP,"Remap New",ETST,7,new->streamtype[i],new->stream[i]);
      }
    }
  }
  j = p->streams;
  while (--j >= 0) {
    if (!(treated[j].tpat)) {
      warn (LIMP,"Remap Close",ETST,7,p->streamtype[j],p->stream[j]);
      ts_file_stream (f,p->stream[j])->endaction = ENDSTR_KILL;
    }
  }
  p->streams = new->streams;
  memcpy (p->stream,new->stream,p->streams * sizeof(p->stream[0]));
  memcpy (p->streamtype,new->streamtype,p->streams * sizeof(p->streamtype[0]));
  p->descrlen = i = new->descrlen;
  if (i != 0) {
    memcpy (p->elemdescr,new->elemdescr,i);
    d = &p->elemdescr[0];
    j = ((d[0] & 0x0F) << 8) + d[1];
    i -= (j+2);
    if (i >= 0) {
      d += 2;
      alloc_descriptor (ts_file_stream(f,p->pmt_pid),p->pmt_pid,
          p->programnumber,p->pmt_version);
      while (j > 1) {
        d = put_descriptor_s (d,ts_file_stream(f,p->pmt_pid),&j);
      }
      if (j != 0) {
        warn (LWAR,"PAT descr error",ETST,7,3,j);
      } else {
        finish_descriptor (ts_file_stream(f,p->pmt_pid));
        while (i >= TS_PMTELEM_SIZE) {
          k = ((d[1] & 0x1F) << 8) + d[2];
          j = ((d[3] & 0x0F) << 8) + d[4];
          i -= (j+TS_PMTELEM_SIZE);
          if (i >= 0) {
            d += TS_PMTELEM_SIZE;
            alloc_descriptor (ts_file_stream(f,p->pmt_pid),k,
                p->programnumber,p->pmt_version);
            while (j > 1) {
              d = put_descriptor_s (d,ts_file_stream(f,p->pmt_pid),&j);
            }
            if (j != 0) {
              warn (LWAR,"PAT descr error",ETST,7,5,j);
              i = -1;
            } else {
              finish_descriptor (ts_file_stream(f,p->pmt_pid));
            }
          }
        }
        if (i != 0) {
          warn (LWAR,"PAT descr error",ETST,7,4,i);
        }
      }
    } else {
      warn (LWAR,"PAT descr error",ETST,7,2,i);
    }
  }
}

/* Unlink stream in an old program that will be released.
 * Mark the corresponding streams to be closed as they cease.
 * Precondition: f!=NULL, old!=NULL
 */
static void unmap_old_program (file_descr *f,
    pmt_descr *old)
{
  int i;
  warn (LIMP,"Unmap",ETST,8,old->descrlen,old->pmt_pid);
  i = old->streams;
  while (--i >= 0) {
    if (ts_file_stream (f,old->stream[i]) != NULL) {
      warn (LIMP,"Unmap Close",ETST,8,old->streamtype[i],old->stream[i]);
      ts_file_stream (f,old->stream[i])->endaction = ENDSTR_KILL;
    }
  }
  if (ts_file_stream (f,old->pmt_pid) != NULL) {
    ts_file_stream (f,old->pmt_pid)->endaction = ENDSTR_CLOSE;
  }
}

/* Release old programs that are no longer valid.
 * Old programs are marked with pat_section < 0.
 * If f!=NULL, then also unmap all the old programs (used with current pat).
 * Precondition: pp!=NULL.
 */
static void release_old_progs (file_descr *f,
    pmt_descr **pp)
{
  pmt_descr *p;
  p = *pp;
  while (p != NULL) {
    if (p->pat_section < 0) {
      if (f != NULL) {
        unmap_old_program (f,p);
      }
      *pp = p->next;
      free (p);
    } else {
      pp = &p->next;
    }
    p = *pp;
  }
}

static void eval_pat_section (file_descr *f,
    stream_descr *s,
    int seclen,
    int tsid,
    int versionnb,
    boolean curni,
    int sectionnb,
    int lastsecnb)
{
  pmt_descr *p;
  byte *d;
  int i, l;
  warn (LINF,"eval PAT",ETST,5,seclen,tsid);
  if (curni) {
    if (versionnb != f->u.ts.newpat_version) {
      f->u.ts.newpat_version = versionnb;
      p = f->u.ts.newpat;
      while (p != NULL) {
        p->pat_section = -1;
        p = p->next;
      }
    }
    d = &s->u.m.psi_data[TS_SECTIONHEAD];
    l = seclen - TS_PATSECT_SIZE;
    while (l >= TS_PATPROG_SIZE) {
      i = (d[0] << 8) + d[1];
      if (i != 0) {
        p = f->u.ts.newpat;
        while ((p != NULL) && (p->programnumber != i)) {
          p = p->next;
        }
        warn (LDEB,"eval PAT 2",ETST,i,(long)f->u.ts.newpat,p);
        if (p == NULL) {
          if ((p = malloc (sizeof(pmt_descr))) == NULL) {
            warn (LERR,"PAT malloc failed",ETST,5,3,sizeof(pmt_descr));
            return;
          }
          p->next = f->u.ts.newpat;
          p->pmt_version = 0xFF;
          p->programnumber = i;
          p->pcr_pid = 0;
          p->streams = 0;
          p->descrlen = 0;
          f->u.ts.newpat = p;
        }
        p->pat_section = sectionnb;
        p->pmt_pid = ((d[2] & 0x1F) << 8) + d[3];
      }
      d += TS_PATPROG_SIZE;
      l -= TS_PATPROG_SIZE;
    }
    if (l != 0) {
      warn (LWAR,"PAT data error",ETST,5,2,seclen);
    } else {
      if (sectionnb == lastsecnb) {
        p = f->u.ts.pat;
        while (p != NULL) {
          warn (LDEB,"eval PAT 3",ETST,p->pcr_pid,p->pat_section,p);
          p->pat_section = -1;
          p = p->next;
        }
        release_old_progs (NULL,&f->u.ts.newpat);
        p = f->u.ts.newpat;
        while (p != NULL) {
          remap_new_program (f,p);
          p = p->next;
        }
        release_old_progs (f,&f->u.ts.pat);
      }
    }
  } else {
    if (versionnb == f->u.ts.pat_version) {
      warn (LWAR,"Same PAT version w/o cni",ETST,5,1,versionnb);
    }
  }
}

static void eval_cat_section (file_descr *f,
    stream_descr *s,
    int seclen,
    int versionnb,
    boolean curni,
    int sectionnb,
    int lastsecnb)
{
}

static void eval_pmt_section (file_descr *f,
    stream_descr *s,
    int seclen,
    int prognb,
    int versionnb,
    boolean curni)
{
  pmt_descr *p;
  int i, epid, esil;
  byte *d;
  byte styp;
  warn (LINF,"eval PMT",ETST,6,seclen,prognb);
  p = f->u.ts.newpat;
  while ((p != NULL) && (p->programnumber != prognb)) {
    p = p->next;
  }
  if (curni) {
    if (p == NULL) {
      warn (LWAR,"Program not in PAT",ETST,6,1,prognb);
    } else {
      p->pmt_version = versionnb;
      p->pcr_pid = ((s->u.m.psi_data[TS_PMT_PCRPID] & 0x1F) << 8)
                 + s->u.m.psi_data[TS_PMT_PCRPID+1];
      p->streams = 0;
      p->descrlen = seclen-TS_PMT_PILEN-CRC_SIZE;
      memcpy (&p->elemdescr[0],&s->u.m.psi_data[TS_PMT_PILEN],
              seclen-TS_PMT_PILEN-CRC_SIZE);
      i = ((s->u.m.psi_data[TS_PMT_PILEN] & 0x0F) << 8)
        + s->u.m.psi_data[TS_PMT_PILEN+1];
      d = &s->u.m.psi_data[TS_PMTSECTHEAD+i];
      i = seclen - i - TS_PMTSECT_SIZE;
      while (i >= TS_PMTELEM_SIZE) {
        i -= TS_PMTELEM_SIZE;
        styp = *d++;
        epid = (*d++ & 0x1F) << 8;
        epid = epid | *d++;
        if ((epid >= TS_PID_LOWEST) && (epid <= TS_PID_HIGHEST)) {
          p->stream[p->streams] = epid;
          p->streamtype[p->streams] = styp;
          p->streams += 1;
          if ((ts_file_stream(f,epid) != NULL)
           && (ts_file_stream(f,epid)->streamdata == sd_data)
           && (ts_file_stream(f,epid)->u.d.mapstream == ts_file_stream(f,0))) {
            ts_file_stream(f,epid)->u.d.mapstream = s;
            warn (LIMP,"PMT stream caught",ETST,6,5,epid);
          }
          esil = (*d++ & 0x0F) << 8;
          esil = esil | *d++;
          d += esil;
          i -= esil;
        } else {
          warn (LWAR,"PMT data error",ETST,6,3,epid);
          i = -1;
        }
      }
      if (i != 0) {
        warn (LWAR,"PMT data error",ETST,6,2,i);
        p->streams = 0;
      } else {
        remap_new_program (f,p);
        release_old_progs (f,&f->u.ts.pat);
      }
    }
  } else {
    if ((p != NULL) && (versionnb == p->pmt_version)) {
      warn (LWAR,"PMT next same version",ETST,6,4,versionnb);
    }
  }
}

/* Extract a partial PSI section from a TS packet.
 * If complete, process its contents (via eval_*_section)
 * Precondition: f!=NULL
 * Return: TRUE if something was processed, FALSE if no data/space available
 */
static boolean ts_psi_table_section (file_descr *f,
    int pid,
    int tableid)
{
  stream_descr *s;
  int i, b, adf;
  int seclen;
  boolean complete;
  warn (LDEB,"PSI",ETST,4,pid,tableid);
  adf = f->data.out;
  list_incr (f->data.out,f->data,TS_PACKET_SIZE - paylen);
  s = ts_file_stream (f,pid);
  if (pusi & TS_UNIT_START) {
    i = f->data.ptr[f->data.out];
    if (i >= paylen) {
      warn (LWAR,"PSI pointer exceed",ETST,4,1,b);
      s->u.m.psi_length = 0;
      list_incr (f->data.out,f->data,paylen);
      return (TRUE);
    }
    if (s->u.m.psi_length == 0) {
      b = i + 1;
      complete = FALSE;
    } else {
      seclen = s->u.m.psi_length + i;
      b = 1;
      complete = TRUE;
    }
  } else {
    if (s->u.m.psi_length == 0) {
      f->total += TS_PACKET_SIZE;
      list_incr (f->data.out,f->data,paylen);
      return (TRUE);
    } else {
      b = 0;
      complete = FALSE;
    }
  }
  list_incr (f->data.out,f->data,b);
  b = paylen - b;
  if (s->u.m.psi_length + b > sizeof(s->u.m.psi_data)) {
    warn (LWAR,"PSI overflow",ETST,4,2,s->u.m.psi_length + b);
    s->u.m.psi_length = 0;
    list_incr (f->data.out,f->data,b);
    return (TRUE);
  }
  ts_adaption_field (f,adf,s,s);
  while (b > 0) {
    i = MAX_DATA_RAWB - f->data.out;
    if (i > b) {
      i = b;
    }
    memcpy (&s->u.m.psi_data[s->u.m.psi_length],
            &f->data.ptr[f->data.out],i);
    list_incr (f->data.out,f->data,i);
    s->u.m.psi_length += i;
    b -= i;
  }
  if (complete) {
    if (s->u.m.psi_length >= TS_HEADSLEN) {
      i = ((s->u.m.psi_data[TS_SECTIONLEN] & 0x0F) << 8)
        + s->u.m.psi_data[TS_SECTIONLEN+1] + TS_HEADSLEN;
    } else {
      i = 0;
    }
    if (seclen != i) {
      warn (LWAR,"PSI length wrong",ETST,4,i,seclen);
      s->u.m.psi_length -= seclen;
      memmove (&s->u.m.psi_data[0],&s->u.m.psi_data[seclen],s->u.m.psi_length);
      complete = FALSE;
    }
  }
  if (!complete) {
    if (s->u.m.psi_length >= TS_HEADSLEN) {
      seclen = ((s->u.m.psi_data[TS_SECTIONLEN] & 0x0F) << 8)
             + s->u.m.psi_data[TS_SECTIONLEN+1] + TS_HEADSLEN;
      if (s->u.m.psi_length >= seclen) {
        complete = TRUE;
      }
    }
  }
  while (complete) {
    if (tableid < 0) {
      if (marker_check (s->u.m.psi_data[TS_SECTIONLEN],0x00,0x40)) {
        warn (LWAR,"PSI section syntax ind",ETST,4,5,
            s->u.m.psi_data[TS_SECTIONLEN]);
      }
    } else if (s->u.m.psi_data[TS_TABLE_ID] == tableid) {
      if (marker_check (s->u.m.psi_data[TS_SECTIONLEN],0x80,0xC0)) {
        warn (LWAR,"PSI section syntax ind",ETST,4,6,
            s->u.m.psi_data[TS_SECTIONLEN]);
      } else {
        unsigned char a;
        a = s->u.m.psi_data[TS_VERSIONNB];
        if ((i = update_crc_32_block (CRC_INIT_32,
                &s->u.m.psi_data[0],seclen)) != 0) {
          warn (LWAR,"PSI CRC error",ETST,4,11,i);
        } else {
          i = (s->u.m.psi_data[TS_TRANSPORTID] << 8)
              + s->u.m.psi_data[TS_TRANSPORTID+1];
          switch (tableid) {
            case TS_TABLEID_PAT:
              if (seclen <= TS_MAX_SECTLEN) {
                eval_pat_section (f,s,seclen,i,(a >> 1) & 0x1F,a & 1,
                  s->u.m.psi_data[TS_SECTIONNB],s->u.m.psi_data[TS_LASTSECNB]);
              } else {
                warn (LWAR,"PSI data error",ETST,4,8,seclen);
              }
              break;
            case TS_TABLEID_CAT:
              if ((i == 0xFFFF)
               && (seclen <= TS_MAX_SECTLEN)) {
                eval_cat_section (f,s,seclen,(a >> 1) & 0x1F,a & 1,
                  s->u.m.psi_data[TS_SECTIONNB],s->u.m.psi_data[TS_LASTSECNB]);
              } else {
                warn (LWAR,"PSI data error",ETST,4,9,seclen);
              }
              break;
            case TS_TABLEID_PMT:
              if ((s->u.m.psi_data[TS_SECTIONNB] == 0)
               && (s->u.m.psi_data[TS_LASTSECNB] == 0)
               && (seclen <= TS_MAX_SECTLEN)
               && (seclen >= TS_PMTSECT_SIZE)) {
                eval_pmt_section (f,s,seclen,i,(a >> 1) & 0x1F,a & 1);
              } else {
                warn (LWAR,"PSI data error",ETST,4,10,seclen);
              }
              break;
          }
        }
      }
    } else {
      warn (LWAR,"PSI wrong table id",ETST,4,4,s->u.m.psi_data[TS_TABLE_ID]);
    }
    complete = FALSE;
    s->u.m.psi_length -= seclen;
    if (s->u.m.psi_length > 0) {
      if (s->u.m.psi_data[seclen] == TS_TABLEID_STUFFING) {
        s->u.m.psi_length = 0;
      } else {
        memmove (&s->u.m.psi_data[0],&s->u.m.psi_data[seclen],
            s->u.m.psi_length);
        if (s->u.m.psi_length >= TS_HEADSLEN) {
          seclen = ((s->u.m.psi_data[TS_SECTIONLEN] & 0x0F) << 8)
                 + s->u.m.psi_data[TS_SECTIONLEN+1] + TS_HEADSLEN;
          if (s->u.m.psi_length >= seclen) {
            complete = TRUE;
          }
        }
      }
    }
  }
  return (TRUE);
}

/* Check for a single tsauto entry, whether a given pes id matches
 * For each program in source PAT/PMT check and connect if applicable.
 * If none matches, but tsauto entry is sprg=unknown, check that sid.
 * Precondition: f!=NULL.
 * Return: TRUE, if at least one match, FALSE otherwise.
 */
static boolean split_autostreammatch (file_descr *f,
    int pid,
    byte sid,
    tsauto_descr *a)
{
  int i;
  boolean match, inpmt;
  pmt_descr *pmt;
  match = FALSE;
  inpmt = FALSE;
  pmt = f->u.ts.pat;
  while (pmt != NULL) {
    i = pmt->streams;
    while (--i >= 0) {
      if (pmt->stream[i] == pid) {
        inpmt = TRUE;
        if (a == NULL) {
          warn (LIMP,"Auto T",ETST,13,sid,pmt->programnumber);
          ts_file_stream (f,pid) =
            connect_streamprog (f,pmt->programnumber,pid,
              -sid,guess_streamtype(sid),
              ts_file_stream (f,pid),
              ts_file_stream (f,pmt->pmt_pid),TRUE);
          match = TRUE;
          i = 0;
        } else {
          if (a->sprg > 0) {
            if (a->sprg == pmt->programnumber) {
              if (a->ssid < 0) {
                warn (LIMP,"Auto P",ETST,13,sid,a->tprg);
                ts_file_stream (f,pid) =
                  connect_streamprog (f,a->tprg,pid,
                    -sid,guess_streamtype(sid),
                    ts_file_stream (f,pid),
                    ts_file_stream (f,pmt->pmt_pid),TRUE);
                match = TRUE;
                i = 0;
              } else if (a->ssid == sid) {
                warn (LIMP,"Auto S",ETST,13,a->tsid,a->tprg);
                ts_file_stream (f,pid) =
                  connect_streamprog (f,a->tprg,pid,
                    a->tsid,guess_streamtype(a->tsid<0?-a->tsid:a->tsid),
                    ts_file_stream (f,pid),
                    ts_file_stream (f,pmt->pmt_pid),TRUE);
                match = TRUE;
                i = 0;
              }
            }
          }
        }
      }
    }
    pmt = pmt->next;
  }
  if ((!inpmt) /* pid not registered in PAT/PMT -> check for unknown sprg */
   && (a != NULL)
   && (a->sprg == 0)
   && (a->ssid == sid)) {
    warn (LIMP,"Auto 0",ETST,9,a->tsid,a->tprg);
    ts_file_stream (f,pid) =
      connect_streamprog (f,a->tprg,pid,
        a->tsid,guess_streamtype(a->tsid<0?-a->tsid:a->tsid),
        ts_file_stream (f,pid),
        ts_file_stream (f,0),TRUE);
    match = TRUE;
  }
  return (match);
}

/* For a packet that does not belong to an active stream, check whether
 * to open a stream for it automatically.
 * Precondition: f!=NULL.
 * Return: TRUE, if a stream was opened now, FALSE otherwise.
 */
static boolean split_autostream (file_descr *f,
    int pid)
{
  pmt_descr *pmt;
  boolean r;
  tsauto_descr *a;
  tsauto_descr **aa;
  warn (LDEB,"Auto Stream",ETST,9,0,pid);
  r = FALSE;
  if (pusi & TS_UNIT_START) {
    if ((f->automatic)
     || (f->u.ts.tsauto != NULL)) {
      if (paylen >= PES_HDCODE_SIZE) {
        int x;
        x = f->data.out;
        list_incr (x,f->data,TS_PACKET_SIZE - paylen);
        if (f->data.ptr[x] == 0x00) {
          list_incr (x,f->data,1);
          if (f->data.ptr[x] == 0x00) {
            list_incr (x,f->data,1);
            if (f->data.ptr[x] == 0x01) {
              list_incr (x,f->data,1);
              pmt = f->u.ts.pat;
              while (pmt != NULL) {
                if (pmt->pmt_pid == pid) {
                  warn (LWAR,"Auto PMT PID",ETST,9,r,pid);
                  return (r);
                }
                pmt = pmt->next;
              }
              if (f->automatic) {
                split_autostreammatch (f,pid,f->data.ptr[x],NULL);
                r = TRUE;
              }
              aa = &f->u.ts.tsauto;
              a = *aa;
              while (a != NULL) {
                if (split_autostreammatch (f,pid,f->data.ptr[x],a)) {
                  r = TRUE;
                  if (a->ssid >= 0) {
                    *aa = a->next; /* delete single entries when matched */
                    free (a);
                  } else {
                    aa = &((a)->next);
                  }
                } else {
                  aa = &((a)->next);
                }
                a = *aa;
              }
            }
          }
        }
      }
    }
  }
  return (r);
}

/* For a packet that does not belong to an active stream, check whether
 * it is categorized to be unparsed SI.
 * Precondition: f!=NULL, f->content==ct_transport.
 * Return: higher bound of range, if it is unparsed SI, -1 otherwise.
 */
int split_unparsedsi (file_descr *f,
    int pid)
{
  tssi_descr *tssi;
  tssi = f->u.ts.tssi;
  while (tssi != NULL) {
    warn (LDEB,"Split Chk SI",ETST,12,tssi->pid_low,tssi->pid_high);
    if ((pid >= tssi->pid_low)
     && (pid <= tssi->pid_high)) {
      warn (LDEB,"Split Chk SI",ETST,12,-1,pid);
      return (tssi->pid_high);
    }
    tssi = tssi->next;
  }
  warn (LDEB,"Split Chk SI",ETST,12,pid,-1);
  return (-1);
}

/* Extract one TS packet of not-to-be-parsed SI.
 * Precondition: f!=NULL, pid is unparsed_si,
 *               list_size(f->data) >= TS_PACKET_SIZE.
 * Return: TRUE if something was processed, FALSE if no data/space available
 */
static boolean ts_unparsed_si (file_descr *f)
{
  stream_descr *s;
  ctrl_buffer *c;
  s = ts_file_stream (f,TS_UNPARSED_SI);
  if (s != NULL) {
    if (!list_full (s->ctrl)) {
      c = &s->ctrl.ptr[s->ctrl.in];
      if (list_free (s->data) >= (2*TS_PACKET_SIZE-1)) {
        if (TS_PACKET_SIZE > list_freeinend (s->data)) {
          s->data.in = 0;
        }
        c->index = s->data.in;
        c->length = 0;
        while (c->length < TS_PACKET_SIZE) {
          int i;
          i = MAX_DATA_RAWB - f->data.out;
          if (i > (TS_PACKET_SIZE - c->length)) {
            i = TS_PACKET_SIZE - c->length;
          }
          memcpy (&s->data.ptr[s->data.in],&f->data.ptr[f->data.out],i);
          list_incr (s->data.in,s->data,i);
          list_incr (f->data.out,f->data,i);
          c->length += i;
        }
        f->payload += TS_PACKET_SIZE;
        c->sequence = f->sequence++;
        c->scramble = 0;
        c->msecread = msec_now ();
/* c->msecpush not set, because there is no scr/pcr or similar available */
/*
        c->pcr.valid = FALSE;
        c->opcr.valid = FALSE;
*/
        list_incr (s->ctrl.in,s->ctrl,1);
      } else {
        return (FALSE);
      }
    } else {
      return (FALSE);
    }
  } else {
    list_incr (f->data.out,f->data,TS_PACKET_SIZE);
  }
  f->total += TS_PACKET_SIZE;
  return (TRUE);
}

/* Check an otherwise unused stream for PCR.
 * Precondition: f!=NULL
 */
void split_checkpcrpid (file_descr *f,
    int pid)
{
  pmt_descr *pmt;
  pmt = f->u.ts.pat;
  while (pmt != NULL) {
    if (pmt->pcr_pid == pid) {
      ts_adaption_field (f,f->data.out,
          ts_file_stream (f,pmt->pmt_pid),ts_file_stream (f,pmt->pmt_pid));
      return;
/* only needed, if any of the streams is in use, but then the pcr
 * evaluation might come too late. therefor eval always.
 */
    }
    pmt = pmt->next;
  }
}

/* Split data from a TS stream.
 * Precondition: f!=NULL
 * Return: TRUE, if something was processed, FALSE if not enough data available
 */
boolean split_ts (file_descr *f)
{
  int l, pid;
  warn (LDEB,"Split TS",ETST,0,0,f);
  if (ts_skip_to_syncbyte (f)) {
    l = list_size (f->data);
    if (l >= TS_PACKET_SIZE) {
      pid = ts_packet_headinfo (&f->data);
      if ((pid >= TS_PID_LOWEST) && (pid <= TS_PID_HIGHEST)) {
        if (ts_file_stream (f,pid) != NULL) {
          if (ts_file_stream (f,pid)->streamdata == sd_data) {
            return (ts_data_stream (f,pid));
          } else {
            return (ts_psi_table_section (f,pid,TS_TABLEID_PMT));
          }
        } else {
          if (split_autostream (f,pid)) {
            return (ts_data_stream (f,pid));
          }
          if (split_unparsedsi (f,pid) >= 0) {
            warn (LDEB,"Unparsed SI",ETST,0,2,pid);
            return (ts_unparsed_si (f));
          }
          split_checkpcrpid (f,pid);
          warn (LDEB,"Data Packet (ignored)",ETST,0,1,pid);
          f->total += TS_PACKET_SIZE;
          list_incr (f->data.out,f->data,TS_PACKET_SIZE);
          return (TRUE);
        }
      } else if (pid == TS_PID_PAT) {
        return (ts_psi_table_section (f,TS_PID_PAT,TS_TABLEID_PAT));
/*
      } else if (pid == TS_PID_CAT) {
        return (ts_psi_table_section (f,TS_PID_CAT,TS_TABLEID_CAT));
*/
      } else if (pid == TS_PID_NULL) {
        f->total += TS_PACKET_SIZE;
        list_incr (f->data.out,f->data,TS_PACKET_SIZE);
        return (TRUE);
      } else if (split_unparsedsi (f,pid) >= 0) {
        warn (LDEB,"Unparsed SI",ETST,0,3,pid);
        return (ts_unparsed_si (f));
      } else {
        /* don't skip 188 here, because it might be an asynchronity */
        f->skipped += 1;
        f->total += 1;
        list_incr (f->data.out,f->data,1);
        return (TRUE);
      }
    }
  }
  return (FALSE);
}

