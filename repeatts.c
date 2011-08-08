/*
 * ISO 13818 stream multiplexer / additional repeater tool
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
 * Module:  Repeater
 * Purpose: Additional tool to repeat and pipe an input stream.
 *
 * This tool accepts a (time1,time2,filename) tuple via stdin,
 * opens the filename, assuming it contains a ISO 13818 Transport Stream,
 * and sends it packet-wise to stdout timed in such a way, that
 * all pakets are sent equally distributed within time2 msec.
 * As long as no further tuple is provided, after time1 msec the same
 * file is sent over and over again.
 */


#include <stdio.h>
#include "global.h"

#define D(x) /* x */

#define MAX_ANOTATE (16 * 256)

static boolean quit;
static int cmdf, outf, nextf;

static int combln, comlln;
static byte combuf[MAX_DATA_COMB];
static int pollc;

static int dati, dato;
static byte data[MAX_ANOTATE];

static t_msec nextrdelay, nextfdelay;
static off_t nextpackets;

t_msec msec_now (void)
{
#define MSEC_EXPONENT 21
  static long last;
  static int local_delta;
  struct timeval tv;
  gettimeofday (&tv,NULL);
  if ((tv.tv_sec & (~((1L << MSEC_EXPONENT) - 1))) != last) {
    last = tv.tv_sec & (~((1L << MSEC_EXPONENT) - 1));
    local_delta += 1000 * (1L << MSEC_EXPONENT);
  }
  return ((tv.tv_sec & ((1L << MSEC_EXPONENT) - 1)) * 1000
         + tv.tv_usec / 1000 + local_delta);
}

static void command_help (char *command, char *errmsg)
{
  fprintf (stderr, "%s\nUsage:\t%s [OPTIONS...] [<file>]\n"
  "  -d <delay>\ttime in msec after which the sending shall be repeated\n"
  "  -t <time>\ttime in msec until the file shall be sent completely\n"
  "  -i\t\taccept from stdin tripels: <delay> <time> <file>\n\n"
  "Send <file> repeated every <delay> msec evenly distributed within <time> msec.\n"
  "When omitted, <time> defaults to <delay>. When <delay> is omitted, the file is\n"
  "sent only once, and <time> must be given. When <file> is ommitted, only -i must\n"
  "be given, to allow commands be enter through stdin.\n",
     errmsg, command);
}

static boolean line_complete (char **s1,
    char **s2,
    char **s3)
{
  int i;
  boolean b;
  *s3 = NULL;
  *s2 = NULL;
  *s1 = NULL;
  i = 0;
  while (i < combln) {
    if (combuf[i] == '\n') {
      comlln = i;
      while (i >= 0) {
        if (combuf[i] <= ' ') {
          combuf[i] = 0;
          b = TRUE;
        } else {
          if (b) {
            *s3 = *s2;
            *s2 = *s1;
            b = FALSE;
          }
          *s1 = &combuf[i];
        }
        i -= 1;
      }
      return (TRUE);
    }
    i += 1;
  }
  return (FALSE);
}

static boolean is_long (char *s,
    long *r)
{
  long i;
  char *e;
  if (s == NULL) {
    return (FALSE);
  }
  errno = 0;
  i = strtol (s,&e,0);
  if ((errno != 0)
   || (*e != 0)) {
    return (FALSE);
  }
  *r = i;
  return (TRUE);
}

static boolean command_do (char *arg1,
    char *arg2,
    char *arg3)
{
  long l1, l2;
  struct stat stat;
D(fprintf(stderr,"command_do(%s,%s,%s)\n",arg1,arg2,arg3));
  if (arg1 != NULL) {
    if (is_long (arg1, &l1)) {
      if (l1 < 0) {
        quit = TRUE;
        return (TRUE);
      }
      if (arg2 != NULL) {
        if (is_long (arg2, &l2)) {
          if (l2 >= 0) {
            if ((l1 >= l2) || (l1 == 0)) {
              if (arg3 != NULL) {
                if (nextf >= 0) {
                  close (nextf);
                }
                if ((nextf = open (arg3, O_RDONLY)) >= 0) {
                  if (fstat (nextf, &stat) == 0) {
  D(fprintf(stderr,"file %d, mode %07o, name %s, ino %ld, size %ld\n",nextf,stat.st_mode,arg3,stat.st_ino,stat.st_size));
                    if (S_ISREG (stat.st_mode)) {
                      if ((stat.st_size % TS_PACKET_SIZE) == 0) {
                        nextrdelay = l2;
                        nextpackets = stat.st_size / TS_PACKET_SIZE;
                        nextfdelay = l1;
  D(fprintf(stderr,"next opened(%d,%d,%d)\n",nextfdelay,nextrdelay,nextpackets));
                        return (TRUE);
                      } else {
                        fprintf (stderr, "File size not multiple of 188\n");
                      }
                    } else {
                      fprintf (stderr, "File not regular\n");
                    }
                  } else {
                    fprintf (stderr, "Cannot stat file\n");
                  }
                  close (nextf);
                  nextf = -1;
                } else {
                  fprintf (stderr, "Cannot open file\n");
                }
              } else {
                fprintf (stderr, "File name missing\n");
              }
            } else {
              fprintf (stderr, "0<delay<time not allowed\n");
            }
          } else {
            fprintf (stderr, "Time must not be negative\n");
          }
        } else {
          fprintf (stderr, "Time must be numeric\n");
        }
      } else {
        fprintf (stderr, "Time missing\n");
      }
    } else {
      fprintf (stderr, "Delay must be numeric\n");
    }
  } else {
    return (TRUE);
  }
  return (FALSE);
}

static boolean command_init (int cargc,
    char **cargv)
{
  char *cdelay = NULL;
  char *ctime = NULL;
  char *cfile = NULL;
  int cc = 0;
  nextf = -1;
  quit = FALSE;
  combln = 0;
  dati = dato = 0;
  pollc = -1;
  while (++cc < cargc) {
    if ((!strcmp(cargv[cc],"--help")) || (!strcmp(cargv[cc],"-h"))) {
      command_help (cargv[0],"");
      return (FALSE);
    } else if ((!strcmp(cargv[cc],"--version")) || (!strcmp(cargv[cc],"-V"))) {
      fprintf(stderr, MPLEX_VERSION "\n");
      return FALSE;
    } else if (!strcmp (cargv[cc],"-i")) {
      pollc = 0;
    } else if (!strcmp (cargv[cc],"-d")) {
      if ((cdelay != NULL) || (++cc >= cargc)) {
        command_help (cargv[0],"must not use -d twice.\n");
        return (FALSE);
      }
      cdelay = cargv[cc];
    } else if (!strcmp (cargv[cc],"-t")) {
      if ((ctime != NULL) || (++cc >= cargc)) {
        command_help (cargv[0],"must not use -t twice.\n");
        return (FALSE);
      }
      ctime = cargv[cc];
    } else {
      if (cfile != NULL) {
        command_help (cargv[0],"too many parameters.\n");
        return (FALSE);
      }
      cfile = cargv[cc];
    }
  }
  if (cfile != NULL) {
    if (!command_do (cdelay ? cdelay : "0", ctime ? ctime : cdelay, cfile)) {
      command_help (cargv[0],"");
      return (FALSE);
    }
  } else if ((ctime != NULL) || (cdelay != NULL) || (pollc < 0)) {
D(fprintf(stderr,"ctime=%p, cdelay=%p, pollc=%d\n", ctime, cdelay, pollc));
    command_help (cargv[0],"only -i must be given when started with no file.\n");
    return (FALSE);
  }
  cmdf = STDIN_FILENO;
  outf = STDOUT_FILENO;
  return ((cmdf >= 0) && (outf >= 0));
}

int main (int argc,
    char *argv[])
{
  int polli, pollo, polls;
  int toberead;
  int currentf;
  boolean dotime;
  t_msec rtime, ftime, rdelay, fdelay, now;
  struct pollfd ufds [3];
  off_t rpackets, rpartial, rpdone;
  if (command_init (argc,&argv[0])) {
    currentf = -1;
    rtime = ftime = msec_now ();
    while (!quit) {
      now = msec_now ();
D(fprintf(stderr,"now(%d)\n",now));
      if (currentf < 0) {
        toberead = 0;
        if (nextpackets > 0) {
          rpackets = nextpackets;
          rdelay = nextrdelay / nextpackets;
          rpartial = nextrdelay % nextpackets;
        } else {
          rpackets = 1;
          rdelay = 0;
          rpartial = 0;
        }
        rpdone = 0;
        fdelay = nextfdelay;
        if ((ftime-now) < 0) {
          ftime = now;
        }
        rtime = ftime;
        currentf = nextf;
        nextf = -1;
D(fprintf(stderr,"next current(%d,%d,%d)\n",currentf,fdelay,rdelay));
      }
      if (currentf >= 0) {
        if ((rtime - now) <= 0) {
          if ((((dato-dati-1) & (MAX_ANOTATE-1)) - toberead) > TS_PACKET_SIZE) {
            toberead += TS_PACKET_SIZE;
            rtime += rdelay;
            rpdone += rpartial;
            if (rpdone >= rpackets) {
              rpdone -= rpackets; /* equaly distribute the rounded msecs by */
              rtime += 1;      /* counting them in an rpackets modulo-space */
            }
            dotime = TRUE;
D(fprintf(stderr,"timer a(%d,%d,%d)\n",toberead,rtime,rpdone));
          } else {
            rtime = now;
            dotime = FALSE;
D(fprintf(stderr,"timer b(%d,%d)\n",toberead,rtime));
          }
        } else {
          dotime = TRUE;
D(fprintf(stderr,"timer c(%d,%d)\n",toberead,rtime));
        }
      } else {
        dotime = FALSE;
D(fprintf(stderr,"timer c(%d,%d)\n",toberead,rtime));
      }
      polls = pollc+1;
      if (pollc >= 0) {
        ufds[pollc].fd = cmdf;
        ufds[pollc].events = POLLIN;
      }
      if (dati != dato) {
        pollo = polls++;
        ufds[pollo].fd = outf;
        ufds[pollo].events = POLLOUT;
      } else {
        pollo = -1;
      }
      if (toberead > 0) {
        polli = polls++;
        ufds[polli].fd = currentf;
        ufds[polli].events = POLLIN;
      } else {
        polli = -1;
      }
      poll (&ufds[0], polls, dotime ? ((rtime-now) > 0) ? (rtime-now) : 0 : -1);
      if ((pollc >= 0)
       && (ufds[pollc].revents & POLLIN)) {
        char *s1, *s2, *s3;
        if (combln >= MAX_DATA_COMB-HIGHWATER_COM) {
          combln -= HIGHWATER_COM;
          memmove (&combuf[0], &combuf[HIGHWATER_COM], combln);
        }
        combln += read (cmdf, &combuf[combln], MAX_DATA_COMB-combln);
        while (line_complete (&s1, &s2, &s3)) {
          command_do (s1, s2, s3);
          combln -= comlln;
          memmove (&combuf[0], &combuf[comlln], combln);
        }
      }
      if ((polli >= 0)
       && (ufds[polli].revents & (POLLIN | POLLHUP | POLLERR))) {
        int l;
        if (ufds[polli].revents & POLLIN) {
          l = toberead;
          if (l > (MAX_ANOTATE - dati)) {
            l = MAX_ANOTATE - dati;
          }
          l = read (currentf, &data[dati], l);
          dati = (dati+l) & (MAX_ANOTATE-1);
          toberead -= l;
        } else {
          l = 0;
        }
        if (l == 0) {
          if ((nextf >= 0)
           || (fdelay == 0)) {
            close (currentf);
            currentf = -1;
          } else {
            lseek (currentf,0,SEEK_SET);
            toberead = ((toberead-1) / TS_PACKET_SIZE) * TS_PACKET_SIZE;
          }
          ftime += fdelay;
          now = msec_now ();
          if ((ftime-now) < 0) {
            ftime = now;
          }
          rtime = ftime;
        }
      }
      if ((pollo >= 0)
       && (ufds[pollo].revents & (POLLOUT | POLLHUP | POLLERR))) {
        if (ufds[pollo].revents & POLLOUT) {
          int l;
          if (dati < dato) {
            l = MAX_ANOTATE - dato;
          } else {
            l = dati - dato;
          }
          l = write (outf, &data[dato], l);
          dato = (dato+l) & (MAX_ANOTATE-1);
          if (l == 0) {
            quit = TRUE;
          }
        } else {
          quit = TRUE;
        }
      }
    }
    return (EXIT_SUCCESS);
  }
  return (EXIT_FAILURE);
}
