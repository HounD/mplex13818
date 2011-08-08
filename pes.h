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


#if PES_LOWEST_SID != 0xBC
#error "PES_LOWEST_SID must be 0xBC"
#endif
#define PES_CODE_STR_MAP  (0xBC)
#define PES_CODE_PRIVATE1 (0xBD)
#define PES_CODE_PADDING  (0xBE)
#define PES_CODE_PRIVATE2 (0xBF)
#define PES_CODE_AUDIO    (0xC0)
#define PES_NUMB_AUDIO    (0x20)
#define PES_CODE_VIDEO    (0xE0)
#define PES_NUMB_VIDEO    (0x10)
#define PES_CODE_ECM      (0xF0)
#define PES_CODE_EMM      (0xF1)
#define PES_CODE_DSMCC    (0xF2)
#define PES_CODE_ISO13522 (0xF3)
#define PES_CODE_ITU222A  (0xF4)
#define PES_CODE_ITU222B  (0xF5)
#define PES_CODE_ITU222C  (0xF6)
#define PES_CODE_ITU222D  (0xF7)
#define PES_CODE_ITU222E  (0xF8)
#define PES_CODE_ANCILARY (0xF9)
#define PES_CODE_STR_DIR  (0xFF)

#define PES_JOKER_AUDIO   (0xB8)
#define PES_JOKER_VIDEO   (0xB9)

#define PES_SYNC_SIZE     3
#define PES_STREAM_ID     (PES_SYNC_SIZE)
#define PES_HDCODE_SIZE   (PES_SYNC_SIZE+1)
#define PES_PACKET_LENGTH (PES_HDCODE_SIZE)
#define PES_HEADER_SIZE   (PES_HDCODE_SIZE+2)

#define ELEMD_MAIN        0
#define ELEMD_VIDEOSTR    2
#define ELEMD_AUDIOSTR    3
#define ELEMD_HIERARCHY   4
#define ELEMD_REGISTRAT   5
#define ELEMD_ALIGNMENT   6
#define ELEMD_TARGETBGG   7
#define ELEMD_VIDWINDOW   8
#define ELEMD_CA          9
#define ELEMD_ISO639LNG  10
#define ELEMD_SYSTEMCLK  11
#define ELEMD_MPLEXBUTL  12
#define ELEMD_COPYRIGHT  13
#define ELEMD_MAXBITRAT  14
#define ELEMD_PRIVATDAT  15
#define ELEMD_SMOOTHING  16
#define ELEMD_STD        17
#define ELEMD_IBP        18

#define PES_STRTYP_VIDEO11172   0x01
#define PES_STRTYP_VIDEO13818   0x02
#define PES_STRTYP_AUDIO11172   0x03
#define PES_STRTYP_AUDIO13818   0x04
#define PES_STRTYP_PRIVATESEC   0x05
#define PES_STRTYP_PRIVATDATA   0x06
#define PES_STRTYP_MHEG13522    0x07
#define PES_STRTYP_DSMCC        0x08
#define PES_STRTYP_ITUH222      0x09
#define PES_STRTYP_13818TYPA    0x0A
#define PES_STRTYP_13818TYPB    0x0B
#define PES_STRTYP_13818TYPC    0x0C
#define PES_STRTYP_13818TYPD    0x0D
#define PES_STRTYP_AUXILIARY    0X0E

#define streamtype_isvideo(typ) \
  ((typ == PES_STRTYP_VIDEO11172) || \
   (typ == PES_STRTYP_VIDEO13818))

#define streamtype_isaudio(typ) \
  ((typ == PES_STRTYP_AUDIO11172) || \
   (typ == PES_STRTYP_AUDIO13818))

