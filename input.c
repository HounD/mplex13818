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
 * Module:  Input
 * Purpose: Data Acquisition from the various open files,
 *          promotion to the single split-functions.
 *
 * This module holds two main data structures, an unsorted list of
 * open input files, and an unsorted list of open input streams
 * (extracted from these files).
 * Further provided are functions to open and close files and streams,
 * to detect states of the input buffers, and to read data into the
 * (raw) file input buffers.
 */

#include "global.h"
#include "error.h"
#include "pes.h"
#include "splitpes.h"
#include "splitps.h"
#include "splitts.h"
#include "splice.h"
#include "input.h"
#include "descref.h"
#include "ts.h"

/* index of files in use, containing i.a. the raw input data buffers:
 */
static file_descr *inf [MAX_INFILE];
static int in_files;
static int in_openfiles[number_ct];

/* index of streams in use, containing i.a. the input pes buffers:
 */
static stream_descr *ins [MAX_INSTREAM];
static int in_streams;
static int in_openstreams[number_sd];

static t_msec trigger_msec_input;

boolean input_init (void)
{
  in_files = 0;
  memset (in_openfiles, 0, sizeof (in_openfiles));
  in_streams = 0;
  memset (in_openstreams, 0, sizeof (in_openstreams));
  trigger_msec_input = TRIGGER_MSEC_INPUT;
  return (TRUE);
}

#ifdef DEBUG_TIMEPOLL
int deb_inraw_free (int f)
{
  register int r;
  r = (f < in_files) ? list_free (inf[f]->data) : 0;
  return (r);
}

int deb_instr_free (int s)
{
  register int r;
  r = (s < in_streams) ? list_free (ins[s]->data) : 0;
  return (r);
}
#endif

/* Determine whether data is expected as input.
 * Return: TRUE, if any valuable file is open, FALSE otherwise
 */
boolean input_expected (void)
{
  return ((in_files > 0)
       && ((in_openfiles[ct_transport] != in_files)
        || (in_openstreams[sd_unparsedsi] != in_streams)));
}

/* Set trigger timing value.
 */
void input_settriggertiming (t_msec time)
{
  trigger_msec_input = time;
}

/* Determine whether input data is acceptable, i.e. there is space in buffers.
 * If so, set the poll struct accordingly for each file in question.
 * Check the streams for time stamps and set the timeout^ accordingly,
 * if the streams might be responsible alone for blocking pipes.
 * Return: TRUE, if at least one buffer has enough space to accept new data.
 *         FALSE otherwise.
 */
boolean input_acceptable (unsigned int *nfds,
    struct pollfd *ufds,
    t_msec *timeout,
    boolean outnotfull)
{
  boolean accept = FALSE;
  int i;
  t_msec t, now;
  file_descr *f;
  stream_descr *s;
  i = in_files;
  while (--i >= 0) {
    f = inf[i];
    warn (LDEB,"Acceptable",EINP,2,1,i);
    warn (LDEB,"Free Raw",EINP,2,2,list_free (f->data));
    if ((list_free (f->data) >= HIGHWATER_RAW)
     && (f->handle >= 0)) {
      ufds->fd = f->handle;
      ufds->events = POLLIN;
      *nfds += 1;
      accept = TRUE;
      f->ufds = ufds++;
    } else {
      f->ufds = NULL;
    }
  }
  if (outnotfull) {
    now = msec_now ();
    i = in_streams;
    while (--i >= 0) {
      s = ins[i];
      if (s->streamdata == sd_data) {
        if (!list_empty (s->ctrl)) {
          if (s->u.d.trigger) {
            t = s->ctrl.ptr[s->ctrl.out].msecpush - now + s->u.d.delta;
          } else {
            t = s->ctrl.ptr[s->ctrl.out].msecread - now + trigger_msec_input;
          }
          if ((t > 0)
           && ((*timeout < 0)
            || (*timeout > t))) {
            *timeout = t;
          }
        }
      }
    }
  }
  return (accept);
}

/* Set the trigger on a stream, enabling the data to be spliced now.
 * Set the trigger for all streams that correspond thru the target program, too
 * Precondition: s!=NULL
 */
static void set_trigger (stream_descr *s,
    t_msec now)
{
  int q, i;
  prog_descr *p;
  if (!list_empty (s->data)) {
    s->u.d.lasttime = now;
    s->u.d.delta =
      now - s->ctrl.ptr[s->ctrl.out].msecpush;
    warn (LDEB,"Set Trigger",EINP,8,s->u.d.pid,s->u.d.delta);
    s->u.d.trigger = TRUE;
    s->u.d.mention = TRUE;
    q = s->u.d.progs;
    while (--q >= 0) {
      p = s->u.d.pdescr[q];
      p->unchanged = TRUE;
      i = p->streams;
      while (--i >= 0) {
        if (!p->stream[i]->u.d.trigger) {
          set_trigger (p->stream[i],now);
        }
      }
    }
  }
}

/* Clear the trigger on a stream, clear it for all corresponding streams, too
 * Precondition: s!=NULL
 */
static void clear_trigger (stream_descr *s)
{
  int q, i;
  prog_descr *p;
  warn (LDEB,"Clear Trigger",EINP,13,s->u.d.pid,s->u.d.delta);
  s->u.d.discontinuity = TRUE;
  s->u.d.trigger = FALSE;
  q = s->u.d.progs;
  while (--q >= 0) {
    p = s->u.d.pdescr[q];
    i = p->streams;
    while (--i >= 0) {
      if (p->stream[i]->u.d.trigger) {
        clear_trigger (p->stream[i]);
      }
    }
  }
}

/* Check if mapstream provides prominent data.
 * Precondition: d!=NULL, !list_empty(d->ctrl)
 * Return: TRUE, if mapstream has prominent data, FALSE otherwise
 */
static boolean preceding_sequence (stream_descr *d,
    stream_descr *m)
{
  if (m != NULL) {
    if (!list_empty (m->ctrl)) {
      if (m->ctrl.ptr[m->ctrl.out].sequence
        - d->ctrl.ptr[d->ctrl.out].sequence <= 0) {
        return (TRUE);
      }
    }
  }
  return (FALSE);
}

/* Check for every stream whether data is available to be spliced.
 * Unparsed SI from an otherwise unused TS has priority.
 * If the stream with the lowest time stamp has a corresponding map stream,
 * that provides data to be spliced first, the map stream is returned.
 * Prior, check if a stream is empty and end it, if necessary; check if a
 * stream is ready but not yet triggered, so trigger it.
 * Return: stream to be spliced next.
 */
stream_descr *input_available (void)
{
  int i, s, q;
  t_msec t, u, now;
  stream_descr *d, *e;
  ctrl_buffer *c;
  file_descr *f;
  now = msec_now ();
  i = in_files;
  while (--i >= 0) {
    f = inf[i];
    if (f->content == ct_transport) {
      d = ts_file_stream (f,TS_UNPARSED_SI);
      if (d != NULL) {
        if (((f->openstreams[sd_data] == 0)
          && (!list_empty (d->ctrl)))
         || (list_full (d->ctrl))) {
          return (d);
        }
      }
    }
  }
  i = in_streams;
  while (--i >= 0) {
    d = ins[i];
    if (d->streamdata == sd_data) {
      if (list_empty (d->ctrl)) {
        switch (d->endaction) {
          case ENDSTR_CLOSE:
            input_endstream (d);
            if (i > in_streams) {
              i = in_streams;
            }
            break;
          case ENDSTR_KILL:
            input_endstreamkill (d);
            if (i > in_streams) {
              i = in_streams;
            }
            break;
          case ENDSTR_WAIT:
            break;
          default:
            warn (LERR,"End Action",EINP,3,1,d->endaction);
            break;
        }
        /* trigger:=false if empty? no ! */
      } else {
        if (!d->u.d.trigger) {
          if (list_full (d->ctrl)
           || list_partialfull (d->data)
        /* || (list_free (d->fdescr->data) < HIGHWATER_IN) */
           || (d->endaction == ENDSTR_CLOSE)
           || (d->endaction == ENDSTR_KILL)
           || ((now - d->ctrl.ptr[d->ctrl.out].msecread)
                 >= trigger_msec_input)) {
            set_trigger (d,now);
          }
        }
      }
    }
  }
  d = NULL;
  i = in_streams;
  while (--i >= 0) {
    e = ins[i];
    if ((e->streamdata == sd_data)
     && (e->u.d.trigger)) {
      if (!list_empty (e->ctrl)) {
        warn (LDEB,"Available",EINP,3,2,i);
        c = &(e->ctrl.ptr[e->ctrl.out]);
        t = c->msecpush + e->u.d.delta;
        if (t - e->u.d.lasttime < 0) {
          warn (LWAR,"Time Decrease",EINP,3,3,t - e->u.d.lasttime);
          clear_trigger (e);
        } else {
          e->u.d.lasttime = t; 
          t -= now;
          if ((t > MAX_MSEC_PUSHJTTR)
           || (t < -MAX_MSEC_PUSHJTTR)) {
            warn (LWAR,"Time Jumpness",EINP,3,4,t);
            clear_trigger (e);
          } else {
            q = c->sequence;
            if ((t <= 0)
             && ((d == NULL)
              || (t < u)
              || ((t == u) && (q - s < 0)))) {
              u = t;
              s = q;
              d = e;
            }
          }
        }
      }
    }
  }
  if (d != NULL) {
    switch (d->fdescr->content) {
      case ct_transport:
        e = d;
        if (preceding_sequence (d, ts_file_stream (d->fdescr,0))) {
          d = ts_file_stream (d->fdescr,0);
        } else {
          if (preceding_sequence (d, d->u.d.mapstream)) {
            d = d->u.d.mapstream;
          } else {
            if (preceding_sequence (d,
                  ts_file_stream (d->fdescr,TS_UNPARSED_SI))) {
              d = ts_file_stream (d->fdescr,TS_UNPARSED_SI);
            }
          }
        }
        break;
      case ct_program:
        if (preceding_sequence (d,d->u.d.mapstream)) {
          d = d->u.d.mapstream;
        }
        break;
      default:
        break;
    }
  }
  return (d);
}

/* Check all files for a given filerefnum.
 * Precondition: filerefnum>=0
 * Return: filename, if filerefnum matches, NULL otherwise
 */
char *input_filerefername (int filerefnum)
{
  int i;
  file_descr *f;
  i = in_files;
  while (--i >= 0) {
    f = inf[i];
    if (f->filerefnum == filerefnum) {
      return (f->name);
    }
    if ((f->append_name != NULL)
     && (f->append_filerefnum == filerefnum)) {
      return (f->append_name);
    }
  }
  return (NULL);
}

/* Open a file. Allocate and initialize it.
 * Precondition: name!=NULL
 * Return: file, if successful, NULL otherwise
 */
file_descr* input_openfile (char *name,
    int filerefnum,
    content_type content,
    boolean automatic,
    int programnb)
{
  file_descr *f;
  struct stat stat;
  warn (LIMP,"Create file",EINP,4,automatic,content);
  warn (LIMP,name,EINP,4,4,programnb);
  if (in_files < MAX_INFILE) {
    switch (content) {
      case ct_packetized:
        f = unionalloc (file_descr,pes);
        break;
      case ct_program:
        f = unionalloc (file_descr,ps);
        break;
      case ct_transport:
        f = unionalloc (file_descr,ts);
        break;
      default:
        warn (LERR,"Unknown contents",EINP,4,7,0);
        f = NULL;
        break;
    }
    if (f != NULL) {
      if ((f->name = malloc (strlen(name) + 1)) != NULL) {
        if (list_create (f->data,MAX_DATA_RAWB)) {
          if ((f->handle = open (name,O_RDONLY|O_NONBLOCK)) >= 0) {
            if (fstat (f->handle,&stat) == 0) {
              f->st_mode = stat.st_mode;
              if (!S_ISREG (f->st_mode)) {
                timed_io = TRUE;
              }
              strcpy (f->name,name);
              f->filerefnum = filerefnum;
              f->skipped = 0;
              f->payload = 0;
              f->total = 0;
              f->sequence = 0;
              memset (f->openstreams,0,sizeof(f->openstreams));
              f->append_name = NULL;
              f->repeatitions = 0;
              f->auto_programnb = programnb;
              f->automatic = automatic;
              f->stopfile = FALSE;
              f->content = content;
              switch (content) {
                case ct_packetized:
                  f->u.pes.stream = NULL;
                  in_openfiles[content] += 1;
                  inf[in_files++] = f;
                  return (f);
                  break;
                case ct_program:
                  memset (f->u.ps.stream,0,sizeof(f->u.ps.stream));
                  f->u.ps.stream[0] = input_openstream (f,0,0,0,sd_map,NULL);
                  if (f->u.ps.stream[0] != NULL) {
                    in_openfiles[content] += 1;
                    inf[in_files++] = f;
                    return (f);
                  }
                  break;
                case ct_transport:
                  f->u.ts.pat_version = 0xFF;
                  f->u.ts.newpat_version = 0xFF;
                  f->u.ts.pat = NULL;
                  f->u.ts.newpat = NULL;
                  f->u.ts.tsauto = NULL;
                  f->u.ts.tssi = NULL;
                  memset (f->u.ts.stream,0,sizeof(f->u.ts.stream));
                  ts_file_stream (f,0) = input_openstream (f,0,0,0,sd_map,NULL);
                  if (ts_file_stream (f,0) != NULL) {
                    in_openfiles[content] += 1;
                    inf[in_files++] = f;
                    return (f);
                  }
                  break;
                default:
                  break;
              }
            } else {
              warn (LERR,"FStat fail",EINP,4,6,0);
            }
            close (f->handle);
          } else {
            warn (LERR,"Open fail",EINP,4,5,f->handle);
          }
          list_release (f->data);
        }
        free (f->name);
      } else {
        warn (LERR,"Alloc fail",EINP,4,8,in_files);
      }
      free (f);
    } else {
      warn (LERR,"Alloc fail",EINP,4,2,in_files);
    }
  } else {
    warn (LERR,"Max file open",EINP,4,3,in_files);
  }
  return (NULL);
}

/* Check if a file with a given name is yet open.
 * The file name comparision is purely textual.
 * Precondition: name!=NULL
 * Return: file if found, NULL otherwise.
 */
file_descr* input_existfile (char *name)
{
  int i;
  i = in_files;
  while (--i >= 0) {
    if (!strcmp (name,inf[i]->name)) {
      return (inf[i]);
    }
  }
  return (NULL);
}

/* Mark all streams in a file to end soon, close the file itself
 * Precondition: f!=NULL
 */
static void input_endfile (file_descr *f)
{
  int i;
  stream_descr *s;
  i = in_streams;
  while (--i >= 0) {
    s = ins[i];
    if (s->fdescr == f) {
      s->endaction = ENDSTR_CLOSE;
    }
  }
  if (f->handle >= 0) {
    close (f->handle);
    f->handle = -1;
  }
  input_closefileifunused (f);
}

/* Close a file and release all corresponding data structures
 * Precondition: f!=NULL, f->u.*.stream[*]==NULL
 */
static void input_closefile (file_descr *f)
{
  int i;
  if (f->handle >= 0) {
    close (f->handle);
  }
  list_release (f->data);
  free (f->name);
  switch (f->content) {
    case ct_transport:
      releasechain (pmt_descr,f->u.ts.pat);
      releasechain (pmt_descr,f->u.ts.newpat);
      releasechain (tsauto_descr,f->u.ts.tsauto);
      releasechain (tssi_descr,f->u.ts.tssi);
      break;
    default:
      break;
  }
  i = in_files;
  while (--i >= 0) {
    if (inf[i] == f) {
      inf[i] = inf[--in_files];
      in_openfiles[f->content] -= 1;
      warn (LDEB,"Close file",EINP,11,1,in_files);
      free (f);
      return;
    }
  }
  warn (LERR,"Close lost file",EINP,11,2,in_files);
  free (f);
}

/* Close a file if there are no more data streams open.
 * If so, close any map streams.
 * Precondition: f!=NULL
 */
void input_closefileifunused (file_descr *f)
{
  int i;
  stream_descr *s;
  if (f->openstreams[sd_data] <= 0) {
    if (f->openstreams[sd_unparsedsi] > 0) {
      switch (f->content) {
        case ct_transport:
          i = MAX_STRPERTS;
          while ((f->openstreams[sd_unparsedsi] > 0)
              && (--i >= 0)) {
            s = ts_file_stream (f,i);
            if ((s != NULL)
             && (s->streamdata == sd_unparsedsi)
             && (s->endaction == ENDSTR_CLOSE)
             && (list_empty (s->ctrl))) {
              input_closestream (ts_file_stream (f,i));
            }
          }
          break;
        default:
          break;
      }
    }
    if (f->openstreams[sd_unparsedsi] <= 0) {
      if (f->openstreams[sd_map] > 0) {
        switch (f->content) {
          case ct_program:
            i = MAX_STRPERPS;
            while (--i >= 0) {
              if (f->u.ps.stream[i] != NULL) {
                input_closestream (f->u.ps.stream[i]);
              }
            }
            break;
          case ct_transport:
            i = MAX_STRPERTS;
            while (--i >= 0) {
              s = ts_file_stream (f,i);
              if ((s != NULL)
               && (s->streamdata == sd_map)) {
                input_closestream (ts_file_stream (f,i));
              }
            }
            break;
          default:
            warn (LERR,"unexpected map",EINP,12,1,f->content);
            break;
        }
      }
      input_closefile (f);
    }
  }
}

/* Add a target program to a stream's list of programs that contain it.
 * Precondition: s!=NULL, p!=NULL, p not in s->u.d.pdescr[*]
 * Postcondition: p in s->u.d.pdescr[*] not more than once.
 * Return: TRUE if successful, FALSE otherwise.
 */
boolean input_addprog (stream_descr *s,
    prog_descr *p)
{
  if (s->u.d.progs < MAX_PRGFORSTR) {
    s->u.d.pdescr[s->u.d.progs++] = p;
    warn (LDEB,"Add prog",EINP,10,2,s->u.d.progs);
    return (TRUE);
  }
  warn (LERR,"Max add prog",EINP,10,1,s->u.d.progs);
  return (FALSE);
}

/* Delete a target program from a stream's list of programs that contain it.
 * Precondition: s!=NULL, p!=NULL, p in s->u.d.pdescr[*] not more than once.
 * Postcondition: p not in s->u.d.pdescr[*]
 * Return: TRUE if successful, FALSE otherwise.
 */
boolean input_delprog (stream_descr *s,
    prog_descr *p)
{
  int i;
  i = s->u.d.progs;
  while (--i >= 0) {
    if (s->u.d.pdescr[i] == p) {
      s->u.d.pdescr[i] = s->u.d.pdescr[--(s->u.d.progs)];
      warn (LDEB,"Del prog",EINP,9,2,i);
      return (TRUE);
    }
  }
  warn (LERR,"Del lost prog",EINP,9,1,s->u.d.progs);
  return (FALSE);
}

/* Open a stream in a file. Allocate and initialize it.
 * sourceid is the stream's ID in the source file.
 * streamid is the PES packet stream id.
 * streamtype is the stream type according to ISO 13818-1 table 2-29.
 * isamap is TRUE for a map stream, FALSE for data stream.
 * mapstream is the superior correlated map stream.
 * Precondition: f!=NULL
 * Return: stream if successful, NULL otherwise
 */
stream_descr *input_openstream (file_descr *f,
    int sourceid,
    int streamid,
    int streamtype,
    streamdata_type streamdata,
    stream_descr *mapstream)
{
  stream_descr *s;
  warn (LIMP,"Open stream",EINP,5,sourceid,streamid);
  if (in_streams < MAX_INSTREAM) {
    switch (streamdata) {
      case sd_data:
        s = unionalloc (stream_descr,d);
        break;
      case sd_map:
        s = unionalloc (stream_descr,m);
        break;
      case sd_unparsedsi:
        s = unionalloc (stream_descr,usi);
        break;
      default:
        s = NULL;
        break;
    }
    if ((s != NULL)
     && ((s->autodescr = malloc (sizeof (descr_descr))) != NULL)
     && ((s->manudescr = malloc (sizeof (descr_descr))) != NULL)) {
      if (list_create (s->ctrl,MAX_CTRL_INB)) {
        if ((streamdata == sd_map) ?
            list_create (s->data,MAX_DATA_INBPSI) :
            streamtype_isvideo (streamtype) ?
            list_create (s->data,MAX_DATA_INBV) :
            streamtype_isaudio (streamtype) ?
            list_create (s->data,MAX_DATA_INBA) :
            list_create (s->data,MAX_DATA_INB)) {
          s->streamdata = streamdata;
          switch (streamdata) {
            case sd_data:
              s->u.d.mapstream = mapstream;
              s->u.d.discontinuity = FALSE;
              s->u.d.trigger = FALSE;
              s->u.d.mention = FALSE;
              s->u.d.has_clockref = FALSE;
              s->u.d.has_opcr = FALSE;
              s->u.d.conv.base = 0;
              s->u.d.conv.msec = 0;
              s->u.d.progs = 0;
              f->openstreams[streamdata] += 1;
              in_openstreams[streamdata] += 1;
              break;
            case sd_map:
              s->u.m.msectime = 0;
              s->u.m.conv.base = 0;
              s->u.m.conv.msec = 0;
              s->u.m.psi_length = 0;
              f->openstreams[streamdata] += 1;
              in_openstreams[streamdata] += 1;
              break;
            case sd_unparsedsi:
              f->openstreams[streamdata] += 1;
              in_openstreams[streamdata] += 1;
              break;
            default:
              break;
          }
          s->ctrl.ptr[0].length = 0;
          s->ctrl.ptr[0].pcr.valid = FALSE;
          s->ctrl.ptr[0].opcr.valid = FALSE;
          s->fdescr = f;
          s->sourceid = sourceid;
          s->stream_id = streamid;
          s->stream_type = streamtype;
          s->version = 0xFF;
          s->conticnt = 0;
          s->endaction = ENDSTR_WAIT;
          clear_descrdescr (s->autodescr);
          clear_descrdescr (s->manudescr);
          ins[in_streams++] = s;
          return (s);
        }
        list_release (s->ctrl);
      }
      free (s);
    } else {
      warn (LERR,"Alloc fail",EINP,5,1,in_streams);
    }
  } else {
    warn (LERR,"Max stream open",EINP,5,2,in_streams);
  }
  return (NULL);
}

/* End a stream, if opportune.
 * Check all target programs this stream is in, close those that
 * do not contain another stream from the same file that is still running.
 * Note, that the stream stays open if there is at least one other stream
 * from the same file, that is still running together with it in a program.
 * Precondition: s!=NULL
 */
void input_endstream (stream_descr *s)
{
  int q, i;
  prog_descr *p;
  stream_descr *t;
  q = s->u.d.progs;
  while (--q >= 0) {
    p = s->u.d.pdescr[q];
    i = p->streams;
    while (--i >= 0) {
      t = p->stream[i];
      if ((!list_empty (t->ctrl))
       && (t->u.d.trigger)
       && (t->fdescr == s->fdescr)) {
        break;
      }
    }
    if (i < 0) {
      splice_closeprog (p);
    }
  }
}

/* End a stream.
 * Unlink it from all target programs, no matter what is left therein.
 * Precondition: s!=NULL
 */
void input_endstreamkill (stream_descr *s)
{
  int i;
  i = s->u.d.progs;
  while (--i >= 0) {
    prog_descr *p;
    p = s->u.d.pdescr[i];
    if (p->streams > 1) {
      unlink_streamprog (s,p);
    } else {
      splice_closeprog (p);
    }
  }
}

/* Close a stream.
 * Release all structures. If this is the only data stream related to the
 * corresponding map stream, close the map stream, too.
 * Precondition: s!=NULL
 */
void input_closestream (stream_descr *s)
{
  int i;
  warn (LIMP,"Close stream",EINP,6,0,s->sourceid);
  switch (s->fdescr->content) {
    case ct_packetized:
      s->fdescr->u.pes.stream = NULL;
      break;
    case ct_program:
      s->fdescr->u.ps.stream[s->sourceid] = NULL;
      break;
    case ct_transport:
      ts_file_stream (s->fdescr,s->sourceid) = NULL;
      break;
    default:
      warn (LERR,"Unknown contents",EINP,6,2,s->fdescr->content);
      break;
  }
  s->fdescr->openstreams[s->streamdata] -= 1;
  if (s->streamdata == sd_data) {
    switch (s->fdescr->content) {
      case ct_transport:
        if (s->u.d.mapstream != ts_file_stream (s->fdescr,0)) {
          i = MAX_STRPERTS;
          while ((--i >= 0)
            && ((ts_file_stream (s->fdescr,i) == NULL)
             || (ts_file_stream (s->fdescr,i)->streamdata != sd_data)
             || (ts_file_stream (s->fdescr,i)->u.d.mapstream
                != s->u.d.mapstream))) {
          }
          if (i < 0) {
            if (s->u.d.mapstream->endaction == ENDSTR_CLOSE) {
              input_closestream (s->u.d.mapstream);
            }
          }
        }
        break;
      default:
        break;
    }
  }
  i = in_streams;
  while (--i >= 0) {
    if (ins[i] == s) {
      ins[i] = ins[--in_streams];
      in_openstreams[s->streamdata] -= 1;
      break;
    }
  }
  if (i < 0) {
    warn (LERR,"Close lost stream",EINP,6,1,in_streams);
  }
  list_release (s->data);
  list_release (s->ctrl);
  free (s->manudescr);
  free (s->autodescr);
  free (s);
}

/* Split data from raw input buffers to PES data stream buffers.
 * Return: TRUE, if something was processed, FALSE if no data/space available
 */
boolean split_something (void)
{
  int i;
  boolean r = FALSE;
  warn (LDEB,"Split some",EINP,7,0,in_files);
  i = in_files;
  while (--i >= 0) {
    switch (inf[i]->content) {
      case ct_packetized:
        if (split_pes (inf[i])) {
          r = TRUE;
        }
        break;
      case ct_program:
        if (split_ps (inf[i])) {
          r = TRUE;
        }
        break;
      case ct_transport:
        if (split_ts (inf[i])) {
          r = TRUE;
        }
        break;
      default:
        warn (LERR,"Unknown contents",EINP,7,3,inf[i]->content);
        /* error ? */
        break;
    }
  }
  return (r);
}

/* Check all files of type ct_transport for --si ranges.
 * Return: higher bound of range, if match is found, -1 otherwise
 */
int input_tssiinafilerange (int pid)
{
  int i;
  i = in_files;
  while (--i >= 0) {
    warn (LDEB,"TSSI in file",EINP,14,-1,inf[i]->content);
    if (inf[i]->content == ct_transport) {
      int h;
      h = split_unparsedsi (inf[i],pid);
      if (h >= 0) {
        warn (LDEB,"TSSI in file",EINP,14,pid,h);
        return (h);
      }
    }
  }
  warn (LDEB,"TSSI in file",EINP,14,pid,-1);
  return (-1);
}

/* Determine the appropriate file for a given handle
 * Return: file, if found, NULL otherwise
 */
file_descr *input_filehandle (int handle)
{
  int i;
  i = in_files;
  while (--i >= 0) {
    if (inf[i]->handle == handle) {
      return (inf[i]);
    }
  }
  return (NULL);
}

/* Check whether there is a pair of filerefnum and filename among the open
 * files, that fits the criteria.
 * Precondition: filename!=NULL or filerefnum>=0
 * Return: file, if found, NULL otherwise
 */
file_descr *input_filereferenced (int filerefnum,
    char *filename)
{
  int i;
  file_descr *f;
  i = in_files;
  while (--i >= 0) {
    f = inf[i];
    if ((filename == NULL)
        ? (filerefnum == f->filerefnum)
        : ((!strcmp (filename, f->name))
        && ((filerefnum < 0)
         || (filerefnum == f->filerefnum)))) {
      return (f);
    }
  }
  return (NULL);
}

/* Mark a file to be stopped.
 * If stopped, the file will be handled as if eof was encountered, the
 * next time there is data to be read from it.
 * Precondition: f!=NULL
 */
void input_stopfile (file_descr *f)
{
  f->stopfile = TRUE;
}

/* Read some data from the file into the raw buffer.
 * On eof or f->stopfile, end the file, or handle repeatitions or append,
 * if applicable.
 * Precondition: poll has stated data or error for the file
 */
void input_something (file_descr *f,
  boolean readable)
{
  int l, m;
  if (f != NULL) {
    if (f->handle >= 0) {
      warn (LDEB,"Something",EINP,0,1,f);
      if (readable && !f->stopfile) {
        l = list_freeinend (f->data);
        if (l > MAX_READ_IN) {
          l = MAX_READ_IN;
        }
        m = list_free (f->data);
        if (l > m) {
          l = m;
        }
        l = read (f->handle,&f->data.ptr[f->data.in],l);
      } else {
        l = 0;
      }
      warn (LDEB,"Some Read",EINP,0,2,l);
      if (l > 0) {
        list_incr (f->data.in,f->data,l);
      } else if (l == 0) {
        f->stopfile = FALSE;
        if (f->repeatitions != 0) {
          if (f->repeatitions > 0) {
            f->repeatitions -= 1;
          }
          if (lseek (f->handle,0,SEEK_CUR) > 255) {
            lseek (f->handle,0,SEEK_SET);
            warn (LIMP,"End Repeat",EINP,0,4,f);
          } else {
            warn (LWAR,"Repeat fail",EINP,0,5,f);
          }
        } else if (f->append_name != NULL) {
          free (f->name);
          f->name = f->append_name;
          f->append_name = NULL;
          if (f->append_filerefnum >= 0) {
            f->filerefnum = f->append_filerefnum;
          }
          close (f->handle);
          if ((f->handle = open (f->name,O_RDONLY|O_NONBLOCK)) >= 0) {
            struct stat stat;
            if (fstat (f->handle,&stat) == 0) {
              f->st_mode = stat.st_mode;
              if (!S_ISREG (f->st_mode)) {
                timed_io = TRUE;
                if (f->append_repeatitions != 0) {
                  warn (LWAR,"Cannot repeat nonregular file",
                        EINP,0,9,f->append_repeatitions);
                }
                f->repeatitions = 0;
              } else {
                f->repeatitions = f->append_repeatitions;
              }
              configuration_changed = TRUE;
              warn (LIMP,"End Append",EINP,0,f->repeatitions,f);
            } else {
              warn (LWAR,"Append fail",EINP,0,7,f);
              input_endfile (f);
            }
          } else {
            warn (LWAR,"Append fail",EINP,0,8,f);
            input_endfile (f);
          }
        } else {
          input_endfile (f);
        }
      } else {
        /* read error */
      }
    }
  }
}

