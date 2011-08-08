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


void alloc_descriptor (stream_descr *s,
    int sourceid,
    int programnumber,
    byte version);
int put_descriptor (file_descr *f,
    stream_descr *s,
    int index,
    int *infolen);
byte *put_descriptor_s (byte *d,
    stream_descr *s,
    int *infolen);
void finish_descriptor (stream_descr *s);
void validate_mapref (stream_descr *m);
void clear_descrdescr (descr_descr *dd);

