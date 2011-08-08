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


#define TS_PACKET_PID      1
#define TS_PACKET_CONTICNT (TS_PACKET_PID+2)
#define TS_PACKET_HEADSIZE (TS_PACKET_CONTICNT+1)
#define TS_PACKET_ADAPTLEN TS_PACKET_HEADSIZE
#define TS_PACKET_FLAGS1   (TS_PACKET_ADAPTLEN+1)

#define TS_SYNC_BYTE  0x47
#define TS_UNIT_START (1<<6)
#define TS_AFC_PAYLD  (1<<4)
#define TS_AFC_ADAPT  (1<<5)
#define TS_AFC_BOTH   (TS_AFC_PAYLD | TS_AFC_ADAPT)

#define TS_ADAPT_DISCONTI (1<<7)
#define TS_ADAPT_RANDOMAC (1<<6)
#define TS_ADAPT_PRIORITY (1<<5)
#define TS_ADAPT_PCRFLAG  (1<<4)
#define TS_ADAPT_OPCRFLAG (1<<3)
#define TS_ADAPT_SPLICING (1<<2)
#define TS_ADAPT_TPRIVATE (1<<1)
#define TS_ADAPT_EXTENSIO (1<<0)

#define TS_ADAPT2_LTWFLAG  (1<<7)
#define TS_ADAPT2_PIECEWRF (1<<6)
#define TS_ADAPT2_SEAMLESS (1<<5)

#define TS_PID_PAT      0x0000
#define TS_PID_CAT      0x0001
#define TS_PID_LOWEST   0x0010
#define TS_PID_HIGHEST  0x1FFE
#define TS_PID_NULL     0x1FFF
#define TS_PID_SPLICELO 0x0100 /* not 0x0010 because of ETSI EN 300 468 */
#define TS_PID_SPLICEHI 0x1FEF /* not 0x1FFE because of ATSC / ETSI ETR 211 */
#define TS_UNPARSED_SI  TS_PID_NULL

#define TS_TABLEID_PAT  0x00
#define TS_TABLEID_CAT  0x01
#define TS_TABLEID_PMT  0x02
#define TS_TABLEID_STUFFING 0xFF

#define TS_TABLE_ID     0
#define TS_SECTIONLEN   (TS_TABLE_ID+1)
#define TS_HEADSLEN     (TS_SECTIONLEN+2)
#define TS_TRANSPORTID  TS_HEADSLEN
#define TS_VERSIONNB    (TS_TRANSPORTID+2)
#define TS_SECTIONNB    (TS_VERSIONNB+1)
#define TS_LASTSECNB    (TS_SECTIONNB+1)
#define TS_SECTIONHEAD  (TS_LASTSECNB+1)

#define TS_PMT_PCRPID   TS_SECTIONHEAD
#define TS_PMT_PILEN    (TS_SECTIONHEAD+2)
#define TS_PMTSECTHEAD  (TS_PMT_PILEN+2)

#define TS_PATSECT_SIZE (TS_SECTIONHEAD+4)
#define TS_CATSECT_SIZE (TS_SECTIONHEAD+4)
#define TS_PMTSECT_SIZE (TS_PMTSECTHEAD+4)

#define TS_PATPROG_SIZE 4
#define TS_PMTELEM_SIZE 5

#define TS_MAX_SECTLEN  1021

