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

#include <string.h>
#include <errno.h>
#include <stdio.h>

#define EINI 0x01 /* init */
#define EDIS 0x02 /* dispatch */
#define EERR 0x03 /* error */
#define EINP 0x04 /* input */
#define EOUT 0x05 /* output */
#define ECOM 0x06 /* command */
#define EGLO 0x07 /* global */
#define EPES 0x08 /* splitpes */
#define EPST 0x09 /* splitps */
#define ETST 0x0A /* splitts */
#define EPSC 0x0B /* spliceps */
#define ETSC 0x0C /* splicets */
#define ESPC 0x0D /* splice */
#define EDES 0x0E /* descref */

#define LERR 0x01 /* program error */
#define LWAR 0x02 /* input data error */
#define LIMP 0x03 /* constellation change */
#define LINF 0x04 /* sporadic information */
#define LSEC 0x05 /* running information */
#define LDEB 0x06 /* debug level */

extern char *warn_level_name[];
extern char *warn_module_name[];

extern int verbose_level;

void do_warn (int level1,
    char *text,
    int module1,
    int func,
    int numb,
    long value);

#define warn(level,text,module,function,number,value) \
  (((level) > verbose_level) ? 0 : \
  (do_warn ((level-1),(text),(module-1),(function),(number),(long)(value)), 0))

