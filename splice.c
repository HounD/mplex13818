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
 * Module:  Splice
 * Purpose: Service functions for specific Splice* modules
 *
 * This module provides functions needed for splicing
 * which are independent from the splice type.
 */

#include "global.h"
#include "error.h"
#include "splice.h"
#include "input.h"
#include "pes.h"
#include "descref.h"

t_msec next_psi_periodic;
t_msec psi_frequency_msec;
boolean psi_frequency_changed;

int configuration_on;
boolean configuration_changed;
boolean configuration_descr_changed;
const char *configuration_total = "Conf: progs: %d\n";

static modifydescr_descr *globalmodifydescr;

boolean splice_init (void)
{
  psi_frequency_msec = 0;
  psi_frequency_changed = FALSE;
  configuration_on = 0;
  globalmodifydescr = NULL;
  return (splice_specific_init ());
}

/* Connect a stream with a target program.
 * programnb denotes the program to connect,
 * stream is the stream to connect,
 * all further parameters are as with input_openstream.
 * If stream is NULL, open a stream first.
 * Add the stream to the programs list of streams and vice versa.
 * Precondition: f!=NULL
 * Return: the changed stream on success, the unchanged "stream" otherwise
 */
stream_descr *connect_streamprog (file_descr *f,
    int programnb,
    int sourceid,
    int streamid,
    int streamtype,
    stream_descr *stream,
    stream_descr *mapstream,
    boolean mention)
{
  stream_descr *s;
  prog_descr *p;
  if (stream == NULL) {
    s = input_openstream (f,sourceid,streamid<0?-streamid:streamid,
            streamtype,sd_data,mapstream);
  } else {
    if (streamid < 0) {
      streamid = -streamid;
      warn (LWAR,"Cannot refind sid",ESPC,1,1,streamid);
    }
    s = stream;
  }
  if (s != NULL) {
    p = splice_openprog (programnb);
    if (p != NULL) {
      if (input_addprog (s,p)) {
        if (splice_addstream (p,s,streamid>=0) > 0) {
/*
          if (p->pcr_pid < 0) {
            if (xxx) {
              p->pcr_pid = s->u.d.pid;
              s->u.d.has_clockref = TRUE;
              s->u.d.next_clockref = msec_now () - MAX_MSEC_PCRDIST;
            }
          }
*/
          s->endaction = ENDSTR_WAIT;
          s->u.d.mention = mention;
          return (s);
        }
        input_delprog (s,p);
      }
      if (p->streams <= 0) {
        splice_closeprog (p);
      }
    }
    if (stream == NULL) {
      input_closestream (s);
    }
  }
  return (stream);
}

/* Unlink a stream from a target program.
 * If the stream comes out to be in no program then, close it.
 *   This function may be used only, if the program in question will either
 *   be non-empty after the call, or will be closed by the calling function.
 * Precondition: s!=NULL, p!=NULL
 */
void unlink_streamprog (stream_descr *s,
    prog_descr *p)
{
  splice_delstream (p,s);
  input_delprog (s,p);
  if (s->u.d.progs <= 0) {
    file_descr *f;
    f = s->fdescr;
    input_closestream (s);
    input_closefileifunused (f);
  }
}

/* Remove a stream from a target program.
 * Close stream and/or program, if not contained in another program or stream.
 * The input file is no longer automatic, because we do manual changes here.
 * Precondition: s!=NULL, p!=NULL, s is stream in target program p
 */
void remove_streamprog (stream_descr *s,
    prog_descr *p)
{
  s->fdescr->automatic = FALSE;
  if (p->streams > 1) {
    unlink_streamprog (s,p);
  } else {
    splice_closeprog (p);
  }
}

/* Find the right stream in a program
 * Precondition: p!=NULL
 * Return: stream, if found, NULL otherwise
 */
stream_descr *get_streamprog (prog_descr *p,
    int streamid)
{
  int i;
  i = p->streams;
  while (--i >= 0) {
    stream_descr *s;
    s = p->stream[i];
    if (s->stream_id == streamid) {
      return (s);
    }
  }
  return (NULL);
}

/* Find a free stream ID in a program that is equivalent to the given stream id
 * Precondition: p!=NULL
 * Return: Free ID, if found; given sid otherwise.
 */
int splice_findfreestreamid (prog_descr *p,
    int sid)
{
  int s0, s, n;
  s0 = sid;
  if ((sid >= PES_CODE_AUDIO)
   && (sid < (PES_CODE_AUDIO+PES_NUMB_AUDIO))) {
    s = PES_CODE_AUDIO;
    n = PES_NUMB_AUDIO;
  } else if ((sid >= PES_CODE_VIDEO)
   && (sid < (PES_CODE_VIDEO+PES_NUMB_VIDEO))) {
    s = PES_CODE_VIDEO;
    n = PES_NUMB_VIDEO;
  } else {
    s = sid;
    n = 1;
  }
  while (--n >= 0) {
    int i;
    i = p->streams;
    while ((--i >= 0)
        && (p->stream[i]->stream_id != s0)) {
    }
    if (i < 0) {
      warn (LIMP,"Found SID free",ESPC,2,sid,s0);
      return (s0);
    }
    s0 = s;
    s += 1;
  }
  warn (LIMP,"Found SID",ESPC,2,sid,sid);
  return (sid);
}

/* Check if there is a source pcr stream in a target program
 * Precondition: p!=NULL
 * Return: pcr-stream, if found; NULL otherwise.
 */
stream_descr *splice_findpcrstream (prog_descr *p)
{
  int i;
  pmt_descr *pmt;
  warn (LIMP,"Find PCR Stream",ESPC,3,0,p->program_number);
  i = p->streams;
  while (--i >= 0) {
    if (p->stream[i]->fdescr->content == ct_transport) {
      pmt = p->stream[i]->fdescr->u.ts.pat;
      while (pmt != NULL) {
        if (pmt->pcr_pid == p->stream[i]->sourceid) {
          warn (LIMP,"Found PCR Stream",ESPC,3,1,p->stream[i]->sourceid);
          return (p->stream[i]);
        }
        pmt = pmt->next;
      }
    }
  }
  return (NULL);
}

/* Print configuration of descriptors
 * Precondition: manud!=NULL
 */
static void splice_descr_configuration (descr_descr *manud,
    descr_descr *autod)
{
  int i, l;
  byte *y;
  i = NUMBER_DESCR;
  while (--i >= 0) {
    y = manud->refx[i];
    if (y == NULL) {
      if (autod != NULL) {
        y = autod->refx[i];
      }
    } else if (y[1] == 0) {
      y = NULL;
    }
    if (y != NULL) {
      l = y[1];
      fprintf (stderr, "Conf: descr:%02X len:%d data:", *y++, l);
      while (--l >= 0) {
        fprintf (stderr, "%02X", *++y);
      }
      fprintf (stderr, "\n");
    }
  }
}

/* Print configuration for one program
 * Precondition: p!=NULL
 */
void splice_one_configuration (prog_descr *p)
{
  int i, s;
  stump_descr *st;
  i = p->streams;
  s = 0;
  st = p->stump;
  while (st != NULL) {
    s += 1;
    st = st->next;
  }
  fprintf (stderr, "Conf: prog:%04X pmt:%04hX pcr:%04hX streams:%2d",
      p->program_number, p->pmt_pid, p->pcr_pid, i+s);
  if (s > 0) {
    fprintf (stderr, " (%d)", s);
  }
  fprintf (stderr, "\n");
  if (configuration_on > 1) {
    splice_descr_configuration (&p->manudescr, NULL); /* Missing auto descr! */
  }
  while (--i >= 0) {
    stream_descr *s;
    s = p->stream[i];
    fprintf (stderr, "Conf: stream:%04hX type:%02X sid:%02X "
      "file:%d source:%04hX num:%2d name:%s\n",
      s->u.d.pid, s->stream_type, s->stream_id,
      s->fdescr->content, s->sourceid, s->fdescr->filerefnum, s->fdescr->name);
    if (configuration_on > 1) {
      splice_descr_configuration (s->manudescr, s->autodescr);
    }
  }
  st = p->stump;
  while (st != NULL) {
    fprintf (stderr, "Conf: stream:%04hX type:%02X\n",
      st->pid, st->stream_type);
    if (configuration_on > 1) {
      splice_descr_configuration (&st->manudescr, NULL);
    }
    st = st->next;
  }
}

void splice_set_configuration (int on)
{
  configuration_on = on;
  configuration_changed = TRUE;
}

static void splice_modifydescriptor (descr_descr *md,
    int dtag,
    int dlength,
    byte *data,
    stream_descr *s)
{
  int i, j;
  byte *t, *u;
  if (dtag < 0) {
    clear_descrdescr (md);
  } else {
    t = md->refx[dtag];
    if ((dlength < 0)
     || ((t != NULL)
      && (t != &md->null[0]))) {
      j = t[1]+2;
      i = NUMBER_DESCR;
      while (--i >= 0) {
        if ((md->refx[i]-t) > 0) {
          md->refx[i] -= j;
        }
      }
      memmove (t,&t[j],sizeof(md->data)-(j+(t-&md->data[0])));
    }
    if (dlength == 0) {
      t = &md->null[0];
    } else if (dlength < 0) {
      t = NULL;
    } else {
      i = NUMBER_DESCR;
      t = &md->data[0];
      while (--i >= 0) {
        u = md->refx[i];
        if (u != NULL) {
          u = &u[u[1]+2];
          if ((u-t) > 0) {
            t = u;
          }
        }
      }
      if (t-&md->data[0] < 0) {
        warn (LERR,"No space left",ESPC,5,2,t-&md->data[0]);
        return;
      }
      if ((t-&md->data[0]+dlength+2-MAX_PSI_SIZE) > 0) {
        warn (LWAR,"No space left",ESPC,5,1,t-&md->data[0]);
        return;
      }
      t[0] = dtag;
      t[1] = dlength;
      memcpy (&t[2],data,dlength);
    }
    md->refx[dtag] = t;
  }
  if (s != NULL) {
    i = s->u.d.progs;
    while (--i >= 0) {
      s->u.d.pdescr[i]->changed = TRUE;
    }
  }
}

static void splice_modifydescrlater (int programnb,
    short sid,
    short pid,
    int dtag,
    int dlength,
    byte *data)
{
  modifydescr_descr *l;
  modifydescr_descr **pl;
  pl = &globalmodifydescr;
  l = *pl;
  while (l != NULL) {
    if ((programnb < 0) /* delete older matching entries */
     || ((programnb == l->programnb)
      && ((pid == 0)
       || ((pid == l->pid)
        && (sid == l->sid)
        && ((dtag < 0)
         || (dtag == l->dtag)))))) {
      *pl = l->next;
      free (l);
    } else {
      pl = &l->next;
    }
    l = *pl;
  }
  if ((dtag >= 0)
   && (dlength >= 0)) { /* don't save modifiers, that delete */
    if ((l = malloc (sizeof(modifydescr_descr))) != NULL) {
      l->next = NULL;
      l->programnb = programnb;
      l->sid = sid;
      l->pid = pid;
      l->dtag = dtag;
      l->dlength = dlength;
      if (dlength > 0) {
        memcpy (&l->data[0],data,dlength);
      }
      *pl = l; /* append at end of list */
    } else {
      warn (LERR,"Malloc fail",ETSC,12,1,programnb);
    }
  }
}

/* For a new program and maybe stream or stump,
 * check presence of applicable descriptors, that have been stored.
 * All non-NULL parameters must be completely linked into p upon call!
 * Precondition: p != NULL
 */
void splice_modifycheckmatch (int programnb,
    prog_descr *p,
    stream_descr *s,
    stump_descr *st)
{
  modifydescr_descr *l;
  modifydescr_descr **pl;
  pl = &globalmodifydescr;
  l = *pl;
  while (l != NULL) {
    if ((programnb == l->programnb)
     && (((l->sid < 0)
       && (l->pid < 0))
      || ((l->pid > 0)
       && (((s != NULL)
         && (l->pid == s->u.d.pid))
        || ((st != NULL)
         && (l->pid = st->pid))))
      || ((l->sid >= 0)
       && (s != NULL)
       && (l->sid == s->stream_id)))) {
      splice_modifytargetdescrprog (p,programnb,
          l->sid,l->pid,l->dtag,l->dlength,&l->data[0],st);
      *pl = l->next;
      free (l);
    } else {
      pl = &l->next;
    }
    l = *pl;
  }
}

/* Modify an entry in a manudescr struct.
 */
void splice_modifytargetdescrprog (prog_descr *p,
    int programnb,
    short sid,
    short pid,
    int dtag,
    int dlength,
    byte *data,
    stump_descr *globstump)
{
  int i;
  stream_descr *s;
  stump_descr *st;
  if (sid >= 0) {
    if (p != NULL) {
      i = p->streams;
      while (--i >= 0) {
        s = p->stream[i];
        if (s->stream_id == sid) {
          splice_modifydescriptor (s->manudescr,dtag,dlength,data,s);
          configuration_descr_changed = TRUE;
          return;
        }
      }
    }
    splice_modifydescrlater (programnb,sid,pid,dtag,dlength,data);
  } else {
    if (pid > 0) {
      if (p != NULL) {
        i = p->streams;
        while (--i >= 0) {
          s = p->stream[i];
          if (s->u.d.pid == pid) {
            splice_modifydescriptor (s->manudescr,dtag,dlength,data,s);
            configuration_descr_changed = TRUE;
            return;
          }
        }
        st = p->stump;
      } else {
        st = globstump;
      }
      while (st != NULL) {
        if ((st->pid == pid)
         && (st->program_number == programnb)) {
          splice_modifydescriptor (&st->manudescr,dtag,dlength,data,NULL);
          if (p != NULL) {
            p->changed = TRUE;
          }
          configuration_descr_changed = TRUE;
          return;
        }
        st = st->next;
      }
      splice_modifydescrlater (programnb,sid,pid,dtag,dlength,data);
    } else if (pid < 0) {
      if (p != NULL) {
        splice_modifydescriptor (&p->manudescr,dtag,dlength,data,NULL);
        p->changed = TRUE;
        configuration_descr_changed = TRUE;
      } else {
        splice_modifydescrlater (programnb,sid,pid,dtag,dlength,data);
      }
    } else {
      if (p != NULL) {
        i = p->streams;
        while (--i >= 0) {
          s = p->stream[i];
          splice_modifydescriptor (s->manudescr,dtag,dlength,data,s);
        }
        splice_modifydescriptor (&p->manudescr,dtag,dlength,data,NULL);
        configuration_descr_changed = TRUE;
        st = p->stump;
      } else {
        st = globstump;
      }
      while (st != NULL) {
        if (st->program_number == programnb) {
          splice_modifydescriptor (&st->manudescr,dtag,dlength,data,NULL);
        }
        st = st->next;
      }
      splice_modifydescrlater (programnb,sid,pid,dtag,dlength,data);
    }
  }
}

