/*
 * ISO 13818 stream multiplexer
 * Copyright (C) 2001 Convergence Integrated Media GmbH Berlin
 * Copyright (C) 2004..2006 Oskar Schirmer (schirmer@scara.com)
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
 * Module:  Split PES
 * Purpose: Split a packetized elementary stream.
 *
 * This module examines a packetized elementary stream and copies the
 * packets to the input stream buffer. Some of the service functions
 * provided are also used by module Split PS due to similarity of the
 * format.
 */

#include "global.h"
#include "error.h"
#include "pes.h"
#include "splitpes.h"
#include "input.h"
#include "splice.h"

/* Guess a stream type.
 * Return: a stream type according to ISO 13818-1 table 2-29,  0 if none such
 */
int guess_streamtype (int streamid)
{
  if ((streamid >= PES_CODE_AUDIO)
   && (streamid < PES_CODE_AUDIO + PES_NUMB_AUDIO)) {
    return (PES_STRTYP_AUDIO13818);
  }
  if ((streamid >= PES_CODE_VIDEO)
   && (streamid < PES_CODE_VIDEO + PES_NUMB_VIDEO)) {
    return (PES_STRTYP_VIDEO13818);
  }
  switch (streamid) {
    case PES_CODE_PRIVATE1:
    case PES_CODE_PRIVATE2:
      return (PES_STRTYP_PRIVATDATA);
    case PES_CODE_DSMCC:
      return (PES_STRTYP_DSMCC);
    case PES_CODE_ITU222A:
    case PES_CODE_ITU222B:
    case PES_CODE_ITU222C:
    case PES_CODE_ITU222D:
    case PES_CODE_ITU222E:
      return (PES_STRTYP_ITUH222);
    case PES_CODE_ISO13522:
      return (PES_STRTYP_MHEG13522);
    default:
      return (0); 
  }
}

/* Skip to find a PES/PS stream prefix
 * Precondition: f!=NULL
 * Postcondition: if found: f->data.out indicates the prefix.
 * Return: TRUE, if found, FALSE otherwise.
 */
boolean pes_skip_to_prefix (file_descr *f)
{
  int i, l, k, n;
  byte d;
  l = k = list_size (f->data);
  i = f->data.out;
  n = 1;
  d = f->data.ptr[i];
  while ((--l > 0)
      && ((n >= 0)
       || (d != 0x01))) {
    list_incr (i,f->data,1);
    if (d) n = 1; else n -= 1;
    d = f->data.ptr[i];
  }
  k = k - l - PES_SYNC_SIZE;
  if (k > 0) {
    warn (LWAR,"Skipped",EPES,1,1,k);
    f->skipped += k; /* evaluate: skip > good and skip > CONST -> bad */
    f->total += k;
    list_incr (f->data.out,f->data,k);
  }
  return ((n < 0) && (d == 0x01));
}

/* Determine the stream id of a packet.
 * Precondition: d!=NULL points to a package.
 * Return: stream id.
 */
int pes_stream_id (refr_data *d)
{
  int i;
  i = d->out;
  list_incr (i,*d,PES_STREAM_ID);
  warn (LINF,"Stream Id",EPES,2,1,d->ptr[i]);
  return (d->ptr[i]);
}

/* Determine the length of a packet.
 * Precondition: d!=NULL points to a package.
 * Return: packet length.
 */
int pes_packet_length (refr_data *d)
{ /* special case len = 0: to do 2.4.3.7 */
#define MAX_PACKETSIZE_PROCESSABLE \
    (mmin((MAX_DATA_RAWB-HIGHWATER_RAW),(MAX_DATA_INB/2)) - PES_HEADER_SIZE)
  int i;
  uint16_t l;
  i = d->out;
  list_incr (i,*d,PES_PACKET_LENGTH);
  l = d->ptr[i] << 8;
  list_incr (i,*d,1);
  l = l | d->ptr[i];
  if (l > MAX_PACKETSIZE_PROCESSABLE) {
    warn (LWAR,"Packet too large",EPES,3,2,l);
    l = MAX_PACKETSIZE_PROCESSABLE;
  }
  warn (LINF,"Packet Length",EPES,3,1,l);
  return (l);
}

/* Copy data from a raw data input buffer to a stream buffer.
 * wrapping is done for the raw data input buffer.
 * If the data does not fit at the end of the buffer, break to the start
 * of the buffer and put it there.
 * Precondition: src!=NULL providing at least size bytes
 *               dst!=NULL with free unbroken space of a least size bytes.
 * Return: Pointer to copied data.
 */
int pes_transfer (refr_data *src,
    refr_data *dst,
    int size)
{
  int l, r;
  if (size > list_freeinend (*dst)) {
    dst->in = 0;
  }
  r = dst->in;
  while (size > 0) {
    l = MAX_DATA_RAWB - src->out;
    if (l > size) {
      l = size;
    }
    memcpy (&dst->ptr[dst->in],&src->ptr[src->out],l);
    list_incr (dst->in,*dst,l);
    list_incr (src->out,*src,l);
    warn (LDEB,"Transfer",EPES,4,1,l);
    size -= l;
  }
  return (r);
}

/* Split data from a PES stream.
 * Precondition: f!=NULL
 * Return: TRUE, if something was processed, FALSE if no data/space available
 */
boolean split_pes (file_descr *f)
{
  int l, p, q;
  stream_descr *s;
  ctrl_buffer *c;
  warn (LDEB,"Split PES",EPES,0,0,f);
  if (pes_skip_to_prefix (f)) {
    l = list_size (f->data);
    if (l >= PES_HDCODE_SIZE) {
      p = pes_stream_id (&f->data);
      if (p >= PES_LOWEST_SID) {
        if (f->u.pes.stream == NULL) {
          if (f->automatic) {
            f->u.pes.stream = connect_streamprog (f,f->auto_programnb,
                p,-p,guess_streamtype(p),NULL,NULL,FALSE);
            if (f->u.pes.stream == NULL) {
              f->automatic = FALSE;
              return (FALSE);
            }
          } else {
            if (list_free (f->data) < HIGHWATER_RAW) {
              if (!S_ISREG(f->st_mode)) {
                f->skipped += PES_SYNC_SIZE;
                f->total += PES_SYNC_SIZE;
                list_incr (f->data.out,f->data,PES_SYNC_SIZE);
                return (TRUE);
              }
            }
            return (FALSE);
          }
        }
        s = f->u.pes.stream;
        if (l >= PES_HEADER_SIZE) {
          q = pes_packet_length (&f->data);
          q += PES_HEADER_SIZE;
          if (l >= q) {
            if (p == s->stream_id) {
              if (list_free (s->data) >= 2*q-1) {
                c = &s->ctrl.ptr[s->ctrl.in];
                c->length = q;
                f->payload += q;
                f->total += q;
                c->index = pes_transfer (&f->data,&s->data,q);
                warn (LDEB,"Sequence",EPES,0,1,f->sequence);
                c->sequence = f->sequence++;
                c->scramble = 0;
                c->msecread = msec_now ();
                if (S_ISREG (f->st_mode)) {
                  c->msecpush = c->msecread; /* wrong, but how ? */
                } else {
                  c->msecpush = c->msecread; /* enough ? */
                }
                c->pcr.valid = FALSE;
                c->opcr.valid = FALSE;
                list_incr (s->ctrl.in,s->ctrl,1);
                return (TRUE);
              }
            } else {
              warn (LDEB,"Dropped PES Packet",EPES,0,p,q);
              f->skipped += q;
              f->total += q;
              list_incr (f->data.out,f->data,q);
              return (TRUE);
            }
          }
        }
      } else {
        warn (LWAR,"Unknown PES Packet",EPES,0,2,p);
        f->skipped += PES_SYNC_SIZE;
        f->total += PES_SYNC_SIZE;
        list_incr (f->data.out,f->data,PES_SYNC_SIZE);
        return (TRUE);
      }
    }
  }
  return (FALSE);
}

