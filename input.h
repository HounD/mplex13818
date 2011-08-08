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


boolean input_init (void);
boolean input_expected (void);
void input_settriggertiming (t_msec time);
boolean input_acceptable (unsigned int *nfds,
    struct pollfd *ufds,
    t_msec *timeout,
    boolean outnotfull);
stream_descr *input_available (void);
char *input_filerefername (int filerefnum);
file_descr* input_openfile (char *name,
    int filerefnum,
    content_type content,
    boolean automatic,
    int programnb);
file_descr* input_existfile (char *name);
void input_closefileifunused (file_descr *f);
boolean input_addprog (stream_descr *s,
    prog_descr *p);
boolean input_delprog (stream_descr *s,
    prog_descr *p);
stream_descr *input_openstream (file_descr *f,
    int sourceid,
    int streamid,
    int streamtype,
    streamdata_type streamdata,
    stream_descr *mapstream);
void input_endstream (stream_descr *s);
void input_endstreamkill (stream_descr *s);
void input_closestream (stream_descr *s);
boolean split_something (void);
int input_tssiinafilerange (int pid);
file_descr *input_filehandle (int handle);
file_descr *input_filereferenced (int filerefnum,
    char *filename);
void input_stopfile (file_descr *f);
void input_something (file_descr *f,
    boolean readable);

