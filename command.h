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


/* Tokens to denote main commands:
 */
enum {
  C_HELP = 1,
  C_VERS,
  C_QUIT,
  C_VERB,
  C_OPES,
  C_OPS,
  C_OTS,
  C_CLOS,
  C_APPE,
  C_FILE,
  C_CROP,
  C_REPT,
  C_FROP,
  C_BUSY,
  C_TIMD,
  C_FPSI,
  C_TSID,
  C_TSSI,
  C_TSSP,
  C_DSCR,
  C_DSCS,
  C_DSCP,
  C_TRGI,
  C_TRGO,
  C_CONF,
  C_STAT,
  C_NETW,
  C_BSCR,
  C_CPID
};

typedef struct {
  byte token;      /* 0, if com* is not set */
  byte width;      /* size of field to print comlong in help, 0 to term list */
  short comshort;  /* -1, if no short version */
  char *comlong;   /* NULL for purely explanatory lines */
  char *explain;   /* exaplanatory string, may contain %s to insert explmulti*/
  char *explmulti; /* is inserted only when splice_multipleprograms is TRUE */
} command_list;    /* otherwise if explmulti is NULL, ignore the whole line */

/* Linked list to hold pairs of filerefnum and filenames, as
 * long as not used during an "open" or "append" operation:
 */
typedef struct filerefer_list {
  struct filerefer_list *next;
  int filerefnum;
  char *filename;
} filerefer_list;

boolean command_init (int cargc,
    char **cargv);
boolean command_expected (unsigned int *nfds,
    struct pollfd *ufds);
void command_process (boolean readable);

