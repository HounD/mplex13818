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


boolean output_init (void);
boolean output_acceptable (void);
byte *output_pushdata (int size,
    boolean timed,
    int push);
void output_settriggertiming (t_msec time);
boolean output_available (unsigned int *nfds,
    struct pollfd *ufds,
    t_msec *timeout);
void output_set_statistics (t_msec time);
void output_gen_statistics (void);
void output_finish (void);
void output_something (boolean writeable);

