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
 * Module:  Descref
 * Purpose: Put descriptor data into map stream buffers.
 */

#include "global.h"
#include "error.h"
#include "splitts.h"

static mapreference mapref;
static int din;

/* Start descriptor processing into a map stream s.
 * The descriptor functions are to be used always in the
 * sequence "alloc, put...put, finish"
 * Postcondition: din==InputIndex, enough free space for PSI.
 */
void alloc_descriptor (stream_descr *s,
    int sourceid,
    int programnumber,
    byte version)
{
  if (s != NULL) {
    warn (LINF,"Alloc Descriptor",EDES,1,version,sourceid);
    mapref.sourceid = sourceid;
    mapref.programnumber = programnumber;
    mapref.version = version;
    memset (&mapref.elemdnew,0,sizeof(mapref.elemdnew));
    din = s->data.in;
    while (list_free (s->data) < 2*(sizeof(mapreference) + MAX_PSI_SIZE) + 1) {
      list_incr (s->ctrl.out,s->ctrl,1);
      s->data.out = s->ctrl.ptr[s->ctrl.out].index;
    }
    if (list_freeinend (s->data) < sizeof(mapreference) + MAX_PSI_SIZE) {
      din = 0;
    }
    s->ctrl.ptr[s->ctrl.in].index = din;
    s->ctrl.ptr[s->ctrl.in].length = sizeof(mapreference);
    din += sizeof(mapreference);
  }
}

/* Scan a descriptor and put it into the map stream s.
 * Source is raw data from file f.
 * Decrease infolen^ by number of bytes processed, or set to -1 on error.
 * Precondition: as thru alloc_descriptor
 * Return: index increased by number of bytes processed.
 */
int put_descriptor (file_descr *f,
  stream_descr *s,
  int index,
  int *infolen)
{
  byte t, l;
  t = f->data.ptr[index];
#if (NUMBER_DESCR < 0x100)
  if (t >= NUMBER_DESCR) {
    warn (LWAR,"Bad Descriptor Tag",EDES,2,1,t);
    *infolen = -1;
    return (index);
  }
#endif
  warn (LINF,"Put Descriptor",EDES,2,0,t);
  list_incr (index,f->data,1);
  l = f->data.ptr[index];
  if ((*infolen -= (l + 2)) >= 0) {
    if (s != NULL) {
      mapref.elemdnew[t] = &s->data.ptr[din];
      s->ctrl.ptr[s->ctrl.in].length += (l + 2);
      s->data.ptr[din++] = t;
      s->data.ptr[din++] = l;
      while (l > 0) {
        list_incr (index,f->data,1);
        s->data.ptr[din++] = f->data.ptr[index];
        l -= 1;
      }
      list_incr (index,f->data,1);
    } else {
      list_incr (index,f->data,l+1);
    }
  }
  return (index);
}

/* Scan a descriptor and put it into the map stream s.
 * Source is direct data from byte pointer d.
 * Decrease infolen^ by number of bytes processed, or set to -1 on error.
 * Precondition: as thru alloc_descriptor
 * Return: byte pointer d increased by number of bytes processed.
 */
byte *put_descriptor_s (byte *d,
  stream_descr *s,
  int *infolen)
{
  byte t, l;
  t = *d++;
#if (NUMBER_DESCR < 0x100)
  if (t >= NUMBER_DESCR) {
    warn (LWAR,"Bad Descriptor Tag",EDES,3,1,t);
    *infolen = -1;
    return (d);
  }
#endif
  warn (LINF,"Put Descriptor",EDES,3,0,t);
  l = *d++;
  if ((*infolen -= (l + 2)) >= 0) {
    if (s != NULL) {
      mapref.elemdnew[t] = &s->data.ptr[din];
      s->ctrl.ptr[s->ctrl.in].length += (l + 2);
      s->data.ptr[din++] = t;
      s->data.ptr[din++] = l;
      while (l > 0) {
        s->data.ptr[din++] = *d++;
        l -= 1;
      }
    } else {
      d += l;
    }
  }
  return (d);
}

/* Finish the collection of descriptors into the map stream s.
 */
void finish_descriptor (stream_descr *s)
{
  if (s != NULL) {
    warn (LINF,"Finish Descriptor",EDES,4,
        s->fdescr->sequence,s->ctrl.ptr[s->ctrl.in].length);
    memcpy (&s->data.ptr[s->ctrl.ptr[s->ctrl.in].index],&mapref,sizeof(mapref));
    s->data.in = din;
    warn (LDEB,"Sequence",EDES,4,1,s->fdescr->sequence);
    s->ctrl.ptr[s->ctrl.in].sequence = s->fdescr->sequence++;
    list_incr (s->ctrl.in,s->ctrl,1);
    if (s->ctrl.out == s->ctrl.in) {
      list_incr (s->ctrl.out,s->ctrl,1);
      s->data.out = s->ctrl.ptr[s->ctrl.out].index;
    }
  }
}

/* Save a set of descriptors map with a stream s,
 * including both the references and the raw data.
 */
static void save_mapreference (mapreference *map,
    byte *dscr,
    int size,
    stream_descr *s)
{
  int i;
  warn (LINF,"Save Mapref",EDES,5,0,size);
  s->version = map->version;
  /* ... = map->programnumber */
  memcpy (&s->autodescr->data[0],dscr,size);
  i = NUMBER_DESCR;
  while (--i >= 0) {
    if (map->elemdnew[i] == NULL) {
      s->autodescr->refx[i] = NULL;
    } else {
      s->autodescr->refx[i] =
        map->elemdnew[i] + ((&s->autodescr->data[0]) - dscr);
    }
  }
}

/* Take a set of descriptors from map stream m,
 * determine the right stream to put the descriptors into
 * (either the map stream itself, or a related data stream),
 * save the descriptors into that stream.
 */
void validate_mapref (stream_descr *m)
{
  stream_descr *s;
  int l;
  mapreference *pmapref;
  pmapref = (mapreference *)&m->data.ptr[m->data.out];
  if (m->sourceid == pmapref->sourceid) {
    s = m;
  } else {
    switch (m->fdescr->content) {
      case ct_program:
        s = m->fdescr->u.ps.stream[pmapref->sourceid];
        break;
      case ct_transport:
        s = ts_file_stream (m->fdescr,pmapref->sourceid);
        break;
      default:
        warn (LERR,"Mapref NULL",EDES,6,1,m->fdescr->content);
        s = NULL;
        break;
    }
  }
  if (s != NULL) {
    if (s->version != pmapref->version) {
      save_mapreference (pmapref,
          &m->data.ptr[m->data.out+sizeof(mapreference)],
          m->ctrl.ptr[m->ctrl.out].length-sizeof(mapreference),s);
      if (s->streamdata != sd_data) {
        warn (LDEB,"Mapref isamap",EDES,6,3,pmapref->sourceid);
        /* must do something, if input program is related to output prog */
      } else {
        s->u.d.mention = TRUE;
        l = s->u.d.progs;
        while (--l >= 0) {
          s->u.d.pdescr[l]->unchanged = TRUE;
/* check this: changed in which cases ? */
        }
      }
    }
  } else {
    warn (LDEB,"Mapref NULL",EDES,6,2,pmapref->sourceid);
  }
  list_incr (m->ctrl.out,m->ctrl,1);
  m->data.out = m->ctrl.ptr[m->ctrl.out].index;
}

/* Clear a descriptor struct
 */
void clear_descrdescr (descr_descr *dd)
{
  memset (dd,0,sizeof(descr_descr));
}

