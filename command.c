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
 * Module:  Command
 * Purpose: User interface.
 */

#include "global.h"
#include "error.h"
#include "input.h"
#include "output.h"
#include "command.h"
#include "splice.h"
#include "splitpes.h"
#include "splitts.h"
#include "dispatch.h"
#include "ts.h"

static int argc, argi;
static char **argv;
static boolean first;

static int cmdf;

static byte combuf[MAX_DATA_COMB];
static int combln, comlln;
static char *comarg;

static char *lastfile;  /* Hold last filename used */

static filerefer_list *filerefer;

static command_list coms[] = {
 {C_HELP,14,-1, "help",   "display this help", ""},
 {C_VERS,14,'V',"version","output version information", ""},
 {C_QUIT,14,'Q',"quit",   "quit this program", ""},
 {C_VERB,8, 'v',"verbose","[<level>]", ""},
 {0,     18,-1, NULL,
    "verbose mode 0..6, default=2, initial=1", ""},
 {C_OPES,4, 'p',"pes",    "<file>%s", " <target program>"},
 {0,     18,-1, NULL,
    "open a PES input <file>%s", ", output as <target program>"},
 {C_OPES,4, 'p',"pes",   "<file'>%s <target stream id>", " <target program>"},
 {0,     18,-1, NULL,
    "open a PES input <file'>, output as <target stream id>", ""},
 {C_OPS, 3, 'P',"ps",    "<file>%s", " <target program>"},
 {0,     18,-1, NULL,
    "open a PS input <file>%s", ", output as <target program>"},
 {C_OPS, 3, 'P',"ps",
    "<file'>%s <source stream id> [<target stream id>]", " <target program>"},
 {0,     18,-1, NULL,
    "take a stream from PS, output as <target stream id>", ""},
 {C_OTS, 3, 'T',"ts",    "<file>     open a TS input <file>", ""},
 {C_OTS, 3, 'T',"ts",    "<file'> <source program>%s", " [<target program>]"},
 {0,     18,-1, NULL,
    "open <source program>%s", ", output as <target program>"},
 {C_OTS, 3, 'T',"ts",
    "<file'> <source prog> %s<stream id> [<target stream id>]",
    "<target prog> "},
 {0,     18,-1, NULL,
    "open <source prog>, take <stream id>, output%s", " in"},
 {0,     18,-1, NULL,    "%sas <target stream id>", "<target prog> "},
 {C_OTS, 3, 'T',"ts",    "<file'> 0 %s<source stream id> [<target stream id>]",
    "<target prog> "},
 {0,     18,-1, NULL,
    "from a single-program TS with broken PSI, take a stream", ""},
 {C_CLOS,6, 'c',"close", "<file>  close <file> as if eof is reached", ""},
 {C_APPE,7, 'a',"append","<file'> <file2> [<num>]", ""},
 {0,     18,-1, NULL,
    "append <file2> to open as soon as <file'> is done,", ""},
 {0,     18,-1, NULL,
    "set repeatition for <file2> to <num>, infinite=0", ""},
 {C_FILE,5, 'f',"file",  "<refnum> <filename>", ""},
 {0,     18,-1, NULL,
    "declare a number to be used instead of a file name", ""},
 {C_CROP,5, 'x',"crop",  "%s[<target stream id>]", "<target prog> "},
 {0,     18,-1, NULL,
    "crop %sprogram or a stream", "a "},
 {C_REPT,7, 'r',"repeat","<file'> <num>", ""},
 {0,     18,-1, NULL,
    "set repeatition for <file'> to <num>, infinite=0", ""},
 {C_FROP,7,'R',"reopen",
    "...    open the next file with new handle even if yet open", ""},
 {C_TSSI,3, -1, "si",
    "<file'> [<low> <high>]", NULL},
 {0,     18,-1, NULL,
    "in TS <file'> handle pids in <low>..<high> as SI-streams", NULL},
 {C_TSSP,6, -1, "sipid",
    "<target program> [<pid> [<stream type>]]", NULL},
 {0,    18, -1, NULL,
    "manually add/delete entry in PMT for <target program>", NULL},
 {C_DSCR,6, -1, "descr",
    "%s[<tag> [<length> <data>...]]", "<target_prog> "},
 {0,    18, -1, NULL,
    "add/inhibit/delete a descriptor (not stream specific)", ""},
 {C_DSCS,7, -1, "sdescr",
    "[%s[<stream_id> [<tag> [<length> <data>...]]]]", "<target_prog> "},
 {0,    18, -1, NULL,
    "add/inhibit/delete a descriptor for <stream_id>", ""},
 {C_DSCP,7, -1, "pdescr",
    "[<target_prog> [<pid> [<tag> [<length> <data>...]]]]", NULL},
 {0,    18, -1, NULL,
    "add/inhibit/delete a descriptor for <pid>", NULL},
 {C_TSID,6, 'I',"ident",
    "<num>   set the output transport stream id", NULL},
 {C_BUSY,5, 'B',"busy",  "[0|1]    if '1' do not stop if idle", ""},
 {C_TIMD,14,-1, "timed",
    "force delay timing even if solely diskfiles in use", ""},
 {C_FPSI,5 ,'F',"fpsi",
    "<msec>   set psi table frequency, single=0, initial=0", ""},
 {C_TRGI,7, -1, "trigin",
    "<msec> set input buffer trigger timing, initial=250", ""},
 {C_TRGO,8, -1, "trigout",
    "<msec>", ""},
 {0,     18,-1, NULL,
    "set output buffer trigger timing, initial=250", ""},
 {C_CONF,7 ,'C',"config",
    "0..2   show current stream configuration (off=0, on=1, more=2)", ""},
 {C_STAT,11,'S',"statistics",
    "<msec>", ""},
 {0,     18,-1, NULL,
    "show load statistics periodically (off=0)", ""},
 {C_NETW,4 ,-1, "nit",
    "[<pid>]   add/omit network pid to program association table", NULL},
 {C_BSCR,14,-1, "badtiming",
    "accept and maybe correct bad timing from DVB card", ""},
 {C_CPID,17,-1, "conservativepids", "[0|1]", NULL},
 {0,     18,-1, NULL,
    "be conservative in pid assignment", NULL},
 {0,     0,  0, NULL,    NULL, NULL}
};

/* Pop m characters from command input buffer by moving
 * the rest to the left.
 * Precondition: 0<=m<=cmbln
 */
static void moveleft (int m)
{
  combln -= m;
  memmove (&combuf[0], &combuf[m], combln);
}

/* Determine whether there is a complete command line
 * available. If so, split into single words.
 * Precondition: combln>=0
 * Postcondition: argc==WordCount and argi==1 and comarg^==Word[0]
 *                and comlln==LineLength
 * Return: TRUE, if line complete, FALSE otherwise.
 */ 
static boolean line_complete (void)
{
  int i;
  i = 0;
  while (i < combln) {
    if (combuf[i] == '\n') {
      argc = 1;
      comlln = i;
      while (i >= 0) {
        if (combuf[i] <= ' ') {
          combuf[i] = 0;
          argi = argc;
        } else {
          comarg = &combuf[i];
          argc = argi+1;
        }
        i -= 1;
      }
      argi = 1;
      return (TRUE);
    }
    i += 1;
  }
  return (FALSE);
}

/* Determine, if there is a tokenword available.
 * Precondition: argc==Number_of_Words
 * Return: Pointer to the word, if available, NULL otherwise.
 */
static char *available_token (void)
{
  char *r = NULL;
  if (argi < argc) {
    if (first) {
      r = argv[argi];
    } else {
      r = comarg;
    }
  }
  return (r);
}

/* Scan to the next tokenword.
 * Precondition: argc==WordCount and if !first: comarg^==Word[argi-1]
 * Postcondition: dito.
 */ 
static void next_token (void)
{
  if (argi < argc) {
    argi += 1;
    if (!first) {
      if (argi < argc) {
        do {
          comarg += 1;
        } while (*comarg != 0);
        do {
          comarg += 1;
        } while (*comarg == 0);
      }
    }
  }
}

/* Determine the token code for a word.
 * If first: Decide for long command word, if double hyphen.
 * If !first: Decide for long command word, if word length > 1
 * Precondition: argc==WordCount and if !first: comarg^==Word[argi-1]
 * Return: token>0, if found, -1 otherwise.
 */
static int token_code (char *t)
{
  command_list *l;
  boolean islong;
  if (t == NULL) {
    return (0);
  }
  if (first) {
    if (*t++ != '-') {
      return (-1);
    }
    if ((islong = (*t == '-'))) {
      t += 1;
    }
  } else {
    islong = (t[1] != 0);
  }
  l = &coms[0];
  while (l->width != 0) {
    if (splice_multipleprograms || (l->explmulti != NULL)) {
      if (l->comlong != NULL) {
        if (islong ? (!strcmp(t,l->comlong))
                   : (*t == l->comshort)) {
          return (l->token);
        }
      }
    }
    l += 1;
  }
  return (-1);
}

/* Print usage information to stderr.
 */
static void command_help (void)
{
  command_list *l;
  fprintf (stderr, "Usage: %s [COMMAND...]\n\n  ", argv[0]);
  l = &coms[0];
  while (l->width != 0) {
    if (splice_multipleprograms || (l->explmulti != NULL)) {
      if (l->comshort > 0) {
        fprintf (stderr, "-%c,", l->comshort);
      } else {
        fprintf (stderr, "   ");
      }
      if (l->comlong != NULL) {
        fprintf (stderr, "  --%-*s", l->width, l->comlong);
      } else {
        fprintf (stderr, "%-*s", l->width, "");
      }
      fprintf (stderr, l->explain, splice_multipleprograms ? l->explmulti : "");
      fprintf (stderr, "\n  ");
    }
    l += 1;
  }
  fprintf (stderr, "\n"
    "Any command may be fed to stdin omitting the leading (double) hyphen\n"
    "during operation. '...' means following command wanted on same line.\n"
    "Files may be specified by filename or reference number (set by --file).\n"
    "<file'> may be replaced with '=' to use previously mentioned file.\n");
}

/* Check the filerefer list for the given filerefnum
 * Precondition: filerefnum>=0
 * Return: Pointer to corresponding filename, if found, NULL otherwise.
 */
static char *filerefer_name (int filerefnum)
{
  filerefer_list *fref;
  fref = filerefer;
  while ((fref != NULL)
      && (fref->filerefnum != filerefnum)) {
    fref = fref->next;
  }
  if (fref != NULL) {
    return (fref->filename);
  } else {
    return (input_filerefername (filerefnum));
  }
}

/* Add a pair of filerefnum and filename to the filerefer list.
 * Replace a pair with same filerefnum, if found.
 * Precondition: filerefnum>=0
 * Postcondition: Only one entry with filerefnum is in the list.
 */
static void filerefer_remind (int filerefnum,
    char *filename)
{
  filerefer_list *fref;
  fref = filerefer;
  while ((fref != NULL)
      && (fref->filerefnum != filerefnum)) {
    fref = fref->next;
  }
  if (fref == NULL) {
    if ((fref = malloc (sizeof (filerefer_list))) != NULL) {
      fref->next = filerefer;
      fref->filerefnum = filerefnum;
      filerefer = fref;
    }
  } else {
    if (fref->filename != NULL) {
      free (fref->filename);
    }
  }
  if (fref != NULL) {
    if ((fref->filename = malloc (strlen (filename) + 1)) != NULL) {
      strcpy (fref->filename,filename);
    }
  }
}

/* Remove a pair of filerefnum and filename from the filerefer list
 * Precondition: Only one entry with filerefnum in the list.
 * Postcondition: No entry with filerefnum in the list.
 */
static void filerefer_forget (int filerefnum)
{
  filerefer_list **rref;
  rref = &filerefer;
  while ((*rref != NULL)
      && ((*rref)->filerefnum != filerefnum)) {
    rref = &((*rref)->next);
  }
  if (*rref != NULL) {
    filerefer_list *fref;
    if ((*rref)->filename != NULL) {
      free ((*rref)->filename);
    }
    fref = *rref;
    *rref = (*rref)->next;
    free (fref);
  }
}

/* Check for special token "=" that denotes "same file again"
 * return: TRUE is name=="=", FALSE otherwise
 */
static boolean fileagain (char *name)
{
  if (name != NULL) {
    return ((name[0] == '=') && (name[1] == 0));
  } else {
    return (FALSE);
  }
}

/* Remind the filename that was just used.
 * Precondition: name!=NULL
 */
static void set_lastfile (char *name)
{
  char *lf;
  if ((lf = malloc (strlen (name) + 1)) != NULL) {
    strcpy (lf,name);
  }
  if (lastfile != NULL) {
    free (lastfile);
  }
  lastfile = lf;
}

/* Open a file given by name. If name is "=", use last filename, else
 * set last filename. If opening succeeds, delete any corresponding
 * entry from the filerefer list.
 * Precondition: name!=NULL
 * Return: File if opened, NULL otherwise.
 */
static file_descr* openfile (char *name,
    int filerefnum,
    content_type content,
    boolean automatic,
    int programnb,
    boolean forcereopen)
{
  file_descr *f;
  if (fileagain (name)) {
    name = lastfile;
  } else {
    set_lastfile (name);
  }
  if (name != NULL) {
    if ((!forcereopen)
     && ((f = input_existfile (name)) != NULL)) {
      warn (LIMP,"Open Existing",ECOM,3,2,f);
    } else {
      f = input_openfile (name,filerefnum,content,automatic,programnb);
      if (f != NULL) {
        filerefer_forget (filerefnum);
      }
    }
  } else {
    fprintf (stderr, "Missing file name\n");
    f = NULL;
  }
  return (f);
}

/* Parse a token word to see if it is a number in range low..high.
 * high=-1 denotes upper limit = infinite.
 * Precondition: low>=0
 * Return: Number if parsed, -1 otherwise.
 */
static long com_number (char *t,
    long low,
    long high)
{
  long i;
  char *e;
  if (t == NULL) {
    return (-1);
  } else if (token_code (t) >= 0) {
    return (-1);
  }
  errno = 0;
  i = strtol (t,&e,0);
  if (errno != 0) {
    return (-1);
  } else if (*e != 0) {
    return (-1);
  } else if ((i < low) || ((high >= low) && (i > high))) {
    fprintf (stderr, "Number out of bounds: %ld\n", i);
    return (-1);
  } else {
    return (i);
  }
}

static void command_toofew (void)
{
  fprintf (stderr, "Too few or bad arguments.\n");
}

/* Process one line of command words.
 * Precondition: argc==WordCount and argi==1 and comarg^==Word[0]
 * Return: TRUE, if processed ok, FALSE otherwise.
 */
static boolean command (void)
{
  char *fn;
  int rn;
  file_descr *f;
  char *t;
  boolean r;
  boolean frop;
  r = TRUE;
  frop = FALSE;
  t = available_token ();
  while (r && (t != NULL)) {
    next_token ();
    warn (LIMP,"Command",ECOM,1,0,token_code(t));
    switch (token_code (t)) {
      case C_OPES:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fn != NULL) {
          int tprg, tsid;
          if (splice_multipleprograms) {
            next_token ();
            tprg = com_number (available_token (),0x0001L,0xFFFFL);
          } else {
            tprg = 0;
          }
          if (tprg >= 0) {
            next_token ();
            tsid = com_number (available_token (),0x00,0xFF);
            if ((!fileagain (fn))
             && (tsid < 0)) {
              f = openfile (fn,rn,ct_packetized,TRUE,tprg,frop);
              if (f == NULL) {
                r = FALSE;
              }
            } else {
              if (tsid >= 0) {
                next_token ();
                f = openfile (fn,rn,ct_packetized,FALSE,0,frop);
                if (f != NULL) {
                  f->u.pes.stream =
                    connect_streamprog (
                      f,tprg,0,tsid,guess_streamtype (tsid),f->u.pes.stream,
                      NULL,TRUE);
                } else {
                  r = FALSE;
                }
              } else {
                command_toofew ();
                r = FALSE;
              }
            }
          } else {
            command_toofew ();
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        frop = FALSE;
        break;
      case C_OPS:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fn != NULL) {
          int tprg, tsid, ssid;
          if (splice_multipleprograms) {
            next_token ();
            tprg = com_number (available_token (),0x0001L,0xFFFFL);
          } else {
            tprg = 0;
          }
          if (tprg >= 0) {
            next_token ();
            ssid = com_number (available_token (),0x00,0xFF);
            if ((!fileagain (fn))
             && (ssid < 0)) {
              f = openfile (fn,rn,ct_program,TRUE,tprg,frop);
              if (f == NULL) {
                r = FALSE;
              }
            } else {
              if (ssid >= 0) {
                next_token ();
                f = openfile (fn,rn,ct_program,FALSE,0,frop);
                if (f != NULL) {
                  tsid = com_number (available_token (),0x00,0xFF);
                  if (tsid >= 0) {
                    next_token ();
                  }
                  f->u.ps.stream[ssid] =
                    connect_streamprog (
                      f,tprg,ssid,tsid<0?-ssid:tsid,
                      guess_streamtype (tsid<0?ssid:tsid),
                      f->u.ps.stream[ssid],f->u.ps.stream[0],TRUE);
                } else {
                  r = FALSE;
                }
              } else {
                command_toofew ();
                r = FALSE;
              }
            }
          } else {
            command_toofew ();
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        frop = FALSE;
        break;
      case C_OTS:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fn != NULL) {
          int sprg, tprg, tsid, ssid;
          next_token ();
          sprg = com_number (available_token (),0x0000L,0xFFFFL);
          if ((!fileagain (fn))
           && (sprg < 0)) {
            f = openfile (fn,rn,ct_transport,TRUE,0,frop);
          } else {
            if (sprg >= 0) {
              f = openfile (fn,rn,ct_transport,FALSE,0,frop);
              if (splice_multipleprograms) {
                next_token ();
                tprg = com_number (available_token (),0x0001L,0xFFFFL);
              } else {
                tprg = 0;
              }
              if (tprg >= 0) {
                next_token ();
                ssid = com_number (available_token (),0x00,0xFF);
                if (ssid >= 0) {
                  next_token ();
                  tsid = com_number (available_token (),0x00,0xFF);
                  if (tsid >= 0) {
                    next_token ();
                  } else {
                    tsid = -ssid;
                  }
/*
                  if (stream yet open somewhere) {
                    connect_streamprog (...);
                    f = NULL;
                  }
*/
                } else {
                  ssid = -1;
                  tsid = -1;
                }
              } else {
                tprg = sprg;
                ssid = -1;
                tsid = -1;
              }
              if ((sprg == 0)
               && (ssid < 0)) { /* if sprg=0, then tprg & ssid obligatory */
                command_toofew ();
                r = FALSE;
              } else {
                if (f != NULL) {
                  tsauto_descr *a;
                  a = malloc (sizeof (tsauto_descr));
                  if (a != NULL) {
                    a->next = f->u.ts.tsauto;
                    a->sprg = sprg;
                    a->tprg = tprg;
                    a->ssid = ssid;
                    a->tsid = tsid;
                    f->u.ts.tsauto = a;
                  }
                }
              }
            } else {
              command_toofew ();
              r = FALSE;
            }
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        frop = FALSE;
        break;
      case C_CLOS:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fn != NULL) {
          next_token ();
          f = input_filereferenced (rn,fn);
          if (f != NULL) {
            input_stopfile (f);
            set_lastfile (fn);
          } else {
            warn (LWAR,"File not open",ECOM,1,4,0);
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        break;
      case C_APPE:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fileagain (fn)) {
          fn = lastfile;
        }
        if (fn != NULL) {
          next_token ();
          f = input_filereferenced (rn,fn);
          if (f != NULL) {
            fn = available_token ();
            rn = com_number (fn,0,-1);
            if (rn >= 0) {
              fn = filerefer_name (rn);
            }
            if (fn != NULL) {
              next_token ();
              if (f->append_name != NULL) {
                free (f->append_name);
              }
              if ((f->append_name = malloc (strlen (fn) + 1)) != NULL) {
                int rept;
                strcpy (f->append_name,fn);
                f->append_filerefnum = rn;
                filerefer_forget (rn);
                rept = com_number (available_token (),0,-1);
                if (rept >= 0) {
                  next_token ();
                  f->append_repeatitions = rept-1;
                } else {
                  f->append_repeatitions = 0;
                }
              }
            } else {
              command_toofew ();
              r = FALSE;
            }
          } else {
            warn (LWAR,"File not open",ECOM,1,5,0);
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        break;
      case C_FILE:
        rn = com_number (available_token (),0,-1);
        if (rn >= 0) {
          next_token ();
          fn = available_token ();
          if (fn != NULL) {
            filerefer_remind (rn,fn);
            set_lastfile (fn);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        break;
      case C_CROP:
        {
          int tprg, tsid;
          prog_descr *p;
          stream_descr *s;
          if (splice_multipleprograms) {
            tprg = com_number (available_token (),0x0000L,0xFFFFL);
            next_token ();
          } else {
            tprg = 0;
          }
          if (tprg >= 0) {
            p = splice_getprog (tprg);
            if (p != NULL) {
              tsid = com_number (available_token (),0x00,0xFF);
              if (tsid >= 0) {
                next_token ();
                s = get_streamprog (p,tsid);
                if (s != NULL) {
                  remove_streamprog (s,p);
                } else {
                  warn (LWAR,"Stream not found",ECOM,1,3,0);
                  r = FALSE;
                }
              } else {
                splice_closeprog (p);
              }
            } else {
              warn (LWAR,"Program not found",ECOM,1,2,0);
              r = FALSE;
            }
          } else {
            command_toofew ();
            r = FALSE;
          }
          frop = FALSE;
        }
        break;
      case C_VERB:
        if ((verbose_level
             = com_number (available_token (),0,LDEB)) >= 0) {
          next_token ();
        } else {
          verbose_level = LWAR;
        }
        break;
      case C_VERS:
        fprintf (stderr, MPLEX_VERSION "\n");
        if (first) {
          force_quit = TRUE;
        }
        break;
      case C_HELP:
        command_help ();
        if (first) {
          r = FALSE;
        }
        break;
      case C_QUIT:
        force_quit = TRUE;
        break;
      case C_BUSY:
        {
          int busy;
          busy = com_number (available_token (),0,1);
          busy_work = (busy != 0);
          if (busy >= 0) {
            next_token ();
          }
        }
        break;
      case C_FROP:
        frop = TRUE;
        break;
      case C_REPT:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fileagain (fn)) {
          fn = lastfile;
        }
        if (fn != NULL) {
          next_token ();
          f = input_filereferenced (rn,fn);
          if (f != NULL) {
            int rept;
            rept = com_number (available_token (),0,-1);
            if (rept >= 0) {
              if (rept != 1) {
                if (S_ISREG (f->st_mode)) {
                  f->repeatitions = rept-1;
                } else {
                  warn (LWAR,"Cannot repeat nonregular file",ECOM,1,6,0);
                  f->repeatitions = 0;
                }
              } else {
                f->repeatitions = 0;
              }
              set_lastfile (fn);
              next_token ();
            } else {
              command_toofew ();
              r = FALSE;
            }
          } else {
            warn (LWAR,"File not open",ECOM,1,7,0);
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        break;
      case C_TIMD:
        timed_io = TRUE;
        break;
      case C_FPSI:
        {
          int msec;
          msec = com_number (available_token (),0,-1);
          if (msec >= 0) {
            splice_setpsifrequency (msec);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_TRGI:
        {
          int msec;
          msec = com_number (available_token (),0,-1);
          if (msec >= 0) {
            input_settriggertiming (msec);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_TRGO:
        {
          int msec;
          msec = com_number (available_token (),0,-1);
          if (msec >= 0) {
            output_settriggertiming (msec);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_TSID:
        {
          int tsid;
          tsid = com_number (available_token (),0x0000L,0xFFFFL);
          if (tsid >= 0) {
            splice_settransportstreamid (tsid);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_TSSI:
        fn = available_token ();
        rn = com_number (fn,0,-1);
        if (rn >= 0) {
          fn = filerefer_name (rn);
        }
        if (fileagain (fn)) {
          fn = lastfile;
        }
        if (fn != NULL) {
          next_token ();
          f = input_filereferenced (rn,fn);
          if (f != NULL) {
            if (f->content == ct_transport) {
              int lower, upper;
              lower = com_number (available_token (),0x0001,TS_PID_HIGHEST);
              if (lower >= 0) {
                next_token ();
                upper = com_number (available_token (),lower,TS_PID_HIGHEST);
                if (upper >= 0) {
                  splice_addsirange (f,lower,upper);
                  next_token ();
                } else {
                  command_toofew ();
                  r = FALSE;
                }
              } else {
                releasechain (tssi_descr,f->u.ts.tssi);
                f->u.ts.tssi = NULL;
              }
            } else {
              warn (LWAR,"File must be TS",ECOM,1,8,0);
              r = FALSE;
            }
          } else {
            warn (LWAR,"File not open",ECOM,1,9,0);
            r = FALSE;
          }
        } else {
          command_toofew ();
          r = FALSE;
        }
        break;
      case C_TSSP:
        {
          int tprg, tpid, ttyp;
          tprg = com_number (available_token (),0x0000L,0xFFFFL);
          if (tprg >= 0) {
            next_token ();
            tpid = com_number (available_token (),0x0001,TS_PID_HIGHEST);
            if (tpid >= 0) {
              next_token ();
              ttyp = com_number (available_token (),0x00,0xFF);
              if (ttyp >= 0) {
                next_token ();
                splice_createstump (tprg,tpid,ttyp);
              } else {
                releasechain (stump_descr,splice_getstumps (tprg,tpid));
              }
            } else {
              releasechain (stump_descr,splice_getstumps (tprg,tpid));
            }
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_DSCR:
        {
          int tprg, dtag, dlen, dbyt, i;
          byte data[MAX_DESCR_LEN];
          if (splice_multipleprograms) {
            tprg = com_number (available_token (),0x0000L,0xFFFFL);
            next_token ();
          } else {
            tprg = 0;
          }
          if (tprg >= 0) {
            dtag = com_number (available_token (),0,NUMBER_DESCR-1);
            if (dtag >= 0) {
              next_token ();
              dlen = com_number (available_token (),0,MAX_DESCR_LEN);
              if (dlen >= 0) {
                i = 0;
                next_token ();
                while ((i < dlen)
                    && ((dbyt = com_number (available_token (),0x00,0xFF))
                            >= 0)) {
                  data[i++] = dbyt;
                  next_token ();
                }
                if (i == dlen) { /* <tprg> <dtag> <dlen> ... add/inhibit one */
                  splice_modifytargetdescriptor (tprg,-1,-1,dtag,dlen,&data[0]);
                } else {
                  command_toofew ();
                  r = FALSE;
                }
              } else { /* <tprg> <dtag>  del one */
                splice_modifytargetdescriptor (tprg,-1,-1,dtag,-1,NULL);
              }
            } else { /* <tprg>  del all non-stream-specific */
              splice_modifytargetdescriptor (tprg,-1,-1,-1,-1,NULL);
            }
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_DSCS:
        {
          int tprg, tsid, dtag, dlen, dbyt, i;
          byte data[MAX_DESCR_LEN];
          if (splice_multipleprograms) {
            tprg = com_number (available_token (),0x0000L,0xFFFFL);
            if (tprg >= 0) {
              next_token ();
            }
          } else {
            tprg = 0;
          }
          if (tprg >= 0) {
            tsid = com_number (available_token (),0x00,0xFF);
            if (tsid >= 0) {
              next_token ();
              dtag = com_number (available_token (),0,NUMBER_DESCR-1);
              if (dtag >= 0) {
                next_token ();
                dlen = com_number (available_token (),0,MAX_DESCR_LEN);
                if (dlen >= 0) {
                  i = 0;
                  next_token ();
                  while ((i < dlen)
                      && ((dbyt = com_number (available_token (),0x00,0xFF))
                              >= 0)) {
                    data[i++] = dbyt;
                    next_token ();
                  }
                  if (i == dlen) { /* <tprg> <tsid> <dtag> <dlen> ... add/inh */
                    splice_modifytargetdescriptor (tprg,tsid,-1,dtag,dlen,
                        &data[0]);
                  } else {
                    command_toofew ();
                    r = FALSE;
                  }
                } else { /* <tprg> <tsid> <dtag>  del one */
                  splice_modifytargetdescriptor (tprg,tsid,-1,dtag,-1,NULL);
                }
              } else { /* <tprg> <tsid>  del all for tsid */
                splice_modifytargetdescriptor (tprg,tsid,-1,-1,-1,NULL);
              }
            } else { /* <tprg>  del all */
              splice_modifytargetdescriptor (tprg,-1,0,-1,-1,NULL);
            }
          } else {
            splice_modifytargetdescriptor (tprg,-1,0,-1,-1,NULL);
          }
        }
        break;
      case C_DSCP:
        {
          int tprg, tpid, dtag, dlen, dbyt, i;
          byte data[MAX_DESCR_LEN];
          tprg = com_number (available_token (),0x0000L,0xFFFFL);
          if (tprg >= 0) {
            next_token ();
            tpid = com_number (available_token (),0x0001,TS_PID_HIGHEST);
            if (tpid >= 0) {
              next_token ();
              dtag = com_number (available_token (),0,NUMBER_DESCR-1);
              if (dtag >= 0) {
                next_token ();
                dlen = com_number (available_token (),0,MAX_DESCR_LEN);
                if (dlen >= 0) {
                  i = 0;
                  next_token ();
                  while ((i < dlen)
                      && ((dbyt = com_number (available_token (),0x00,0xFF))
                              >= 0)) {
                    data[i++] = dbyt;
                    next_token ();
                  }
                  if (i == dlen) { /* <tprg> <tpid> <dtag> <dlen> ... add/inh */
                    splice_modifytargetdescriptor (tprg,-1,tpid,dtag,dlen,
                        &data[0]);
                  } else {
                    command_toofew ();
                    r = FALSE;
                  }
                } else { /* <tprg> <tpid> <dtag>  del one */
                  splice_modifytargetdescriptor (tprg,-1,tpid,dtag,-1,NULL);
                }
              } else { /* <tprg> <tpid>  del all for tpid */
                splice_modifytargetdescriptor (tprg,-1,tpid,-1,-1,NULL);
              }
            } else { /* <tprg>  del all */
              splice_modifytargetdescriptor (tprg,-1,0,-1,-1,NULL);
            }
          } else {
            splice_modifytargetdescriptor (tprg,-1,0,-1,-1,NULL);
          }
        }
        break;
      case C_CONF:
        {
          int conf;
          conf = com_number (available_token (),0,2);
          if (conf >= 0) {
            splice_set_configuration (conf);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_STAT:
        {
          int msec;
          msec = com_number (available_token (),0,-1);
          if (msec >= 0) {
            output_set_statistics (msec);
            next_token ();
          } else {
            command_toofew ();
            r = FALSE;
          }
        }
        break;
      case C_NETW:
        {
          int npid;
          npid = com_number (available_token (),1,TS_PID_HIGHEST);
          if (npid >= 0) {
            next_token ();
          }
          splice_setnetworkpid (npid);
        }
        break;
      case C_BSCR:
        accept_weird_scr = TRUE;
        break;
      case C_CPID:
        {
          int cpid;
          cpid = com_number (available_token (),0,1);
          conservative_pid_assignment = (cpid != 0);
          if (cpid >= 0) {
            next_token ();
          }
        }
        break;
      default:
        fprintf (stderr, "Unknown command: %s\n", t);
        if (first) {
          command_help ();
        } else {
          warn (LWAR,"Bad token",ECOM,1,1,*t);
        }
        r = FALSE;
        break;
    }
    t = available_token ();
  }
  return (r);
}

boolean command_init (int cargc,
    char **cargv)
{
  verbose_level = LERR;
  argi = 1;
  argc = cargc;
  argv = cargv;
  combln = 0;
  lastfile = NULL;
  first = TRUE;
  if (!command ()) {
    return (FALSE);
  }
  first = FALSE;
  cmdf = STDIN_FILENO;
  return (cmdf >= 0);
}

/* Determine whether command input can be processed.
 * Set the poll-struct accordingly.
 * Return: TRUE, if command input is expected, FALSE otherwise.
 */
boolean command_expected (unsigned int *nfds,
    struct pollfd *ufds)
{
  if (cmdf >= 0) {
    ufds->fd = cmdf;
    ufds->events = POLLIN;
    *nfds += 1;
  }
  return (cmdf >= 0);
}

/* Read command input and process it.
 */
void command_process (boolean readable)
{
  int i;
  if (combln >= MAX_DATA_COMB-HIGHWATER_COM) {
    warn (LWAR,"Too long",ECOM,2,1,combln);
    moveleft (HIGHWATER_COM);
  }
  if (readable) {
    i = read (cmdf,&combuf[combln],MAX_DATA_COMB-combln);
  } else {
    i = 0;
  }
  if (i > 0) {
    combln += i;
    while (line_complete ()) {
      command ();
      moveleft (comlln+1);
    }
  }
}

