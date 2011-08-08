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

/* Local type to splice: To keep information on modification
 * of a descriptor, for which the stream has not yet begun.
 * Used in splice.c as a list.
 */
typedef struct modifydescrdescr {
  struct modifydescrdescr *next;
  int programnb;
  short sid;
  short pid;
  int dtag;
  int dlength;
  byte data[MAX_DESCR_LEN];
} modifydescr_descr;

extern t_msec next_psi_periodic;
extern t_msec psi_frequency_msec;
extern boolean psi_frequency_changed;

extern int configuration_on;
extern boolean configuration_changed;
extern boolean configuration_descr_changed;
extern const char *configuration_total;

#define configuration_must_print \
    (configuration_changed ? configuration_on != 0 : \
     configuration_descr_changed && (configuration_on > 1))

#define configuration_was_printed \
    (configuration_changed = FALSE, configuration_descr_changed = FALSE)

boolean splice_init (void);
stream_descr *connect_streamprog (file_descr *f,
    int programnb,
    int sourceid,
    int streamid,
    int streamtype,
    stream_descr *stream,
    stream_descr *mapstream,
    boolean mention);
void unlink_streamprog (stream_descr *s,
    prog_descr *p);
void remove_streamprog (stream_descr *s,
    prog_descr *p);
stream_descr *get_streamprog (prog_descr *p,
    int streamid);
int splice_findfreestreamid (prog_descr *p,
    int sid);
stream_descr *splice_findpcrstream (prog_descr *p);
void splice_set_configuration (int on);
void splice_one_configuration (prog_descr *p);
void splice_modifycheckmatch (int programnb,
    prog_descr *p,
    stream_descr *s,
    stump_descr *st);
void splice_modifytargetdescrprog (prog_descr *p,
    int programnb,
    short sid,
    short pid,
    int dtag,
    int dlength,
    byte *data,
    stump_descr *globstump);

/*
 * the following are specific to the splice type
 * and thus are provided in splice*.c :
 */

/* Determines, whether the spliced stream format supports more than 1 program:
 */
extern const boolean splice_multipleprograms;

boolean splice_specific_init (void);

/* Set the transport stream ID, if applicable:
 */
void splice_settransportstreamid (int tsid);

/* Set the PSI frequency in milliseconds. 0 denotes infinite.
 */
void splice_setpsifrequency (t_msec freq);

/* Set the network PID in a PAT
 */
void splice_setnetworkpid (short pid);

/* Print configuration for all target programs.
 */
void splice_all_configuration (void);

/* Get the program with the specified number.
 * Return: program, if found; NULL otherwise.
 */
prog_descr *splice_getprog (int programnb);

/* Open a program with the specified number.
 * If the program yet exists, don't open another one.
 * Return: program, if found or opened; NULL on error.
 */
prog_descr *splice_openprog (int programnb);

/* Close a program.
 * Unlink any streams that are still linked to the program.
 * Precondition: p!=NULL
 */
void splice_closeprog (prog_descr *p);

/* Add a stream to a program.
 * If !force_sid and the stream ID is occupied, try to find an equivalent one.
 * Precondition: p!=NULL, s!=NULL
 * Return: target ID > 0 on success, 0 or negative otherwise.
 */
int splice_addstream (prog_descr *p,
    stream_descr *s,
    boolean force_sid);

/* Delete a stream from a program.
 * Precondition: p!=NULL, s!=NULL
 * Return: TRUE on success, FALSE otherwise
 */
boolean splice_delstream (prog_descr *p,
    stream_descr *s);

/* Prepare to finish.
 * Generate any data needed to close the output stream cleanly.
 */
void process_finish (void);

/* Process data from a stream.
 * If it is a map stream, then it is a set of descriptors, so just
 * validate them. Otherwise, if PSI information is due, generate it.
 * Check whether there is enough space via output_pushdata, exit if not.
 * Compile one output package (whatever that might be) of the foreseen
 * size, and crop the consumed amount of payload data from the stream.
 * The stream payload data block now may be empty, but not necessarily is.
 * Precondition: s!=NULL, !list_empty(s->ctrl)
 * Return: NULL, if the stream data block is consumed completely and thus empty
 *         the stream s otherwise.
 */
stream_descr *process_something (stream_descr *s);

/* Add a range of unparsed si PIDs to a source file.
 * Check all PIDs covered for collision in the target stream.
 */
void splice_addsirange (file_descr *f,
    int lower,
    int upper);

/* Create a stump.
 * If it exists yet, change it, if the program does not yet exist,
 * store it into a global stump list.
 */
void splice_createstump (int programnb,
    short pid,
    byte styp);

/* Get all stumps for a given program (for pid<0) or get
 * a single stump for a given program and pid. Unchain these from
 * the list (global or prog) and return them as separate list.
 */
stump_descr *splice_getstumps (int programnb,
    short pid);

/* Modify an entry in a manudescr struct.
 * If programnb<0, modify all descriptors for all programs,
 * if pid==0, modify all descriptors (global and for all streams),
 * if sid<0 and pid<0, modify a global descriptor, non-stream-specific,
 * if pid>0, modify a descriptor for that stream (TS only),
 * if sid>=0, modify a descriptor for that stream,
 * if dtag<0, delete manual entries for all descriptors for the stream(s),
 * if dtag>=0 and dlength<0, delete one manual entry,
 * if dtag>=0 and dlength==0, inhibit auto propagation of this descriptor,
 * else add a manual entry.
 */
void splice_modifytargetdescriptor (int programnb,
    short sid,
    short pid,
    int dtag,
    int dlength,
    byte *data);


