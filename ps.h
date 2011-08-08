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


#define PS_CODE_END      (0xB9)
#define PS_CODE_PACK_HDR (0xBA)
#define PS_CODE_SYST_HDR (0xBB)

#define PS_PACKHD_SCR    (PES_HDCODE_SIZE)
#define PS_PACKHD_MUXRAT (PS_PACKHD_SCR+6)
#define PS_PACKHD_STUFLN (PS_PACKHD_MUXRAT+3)
#define PS_PACKHD_SIZE   (PS_PACKHD_STUFLN+1)

#define PS_SYSTHD_RATBND (PES_HEADER_SIZE)
#define PS_SYSTHD_AUDBND (PS_SYSTHD_RATBND+3)
#define PS_SYSTHD_VIDBND (PS_SYSTHD_AUDBND+1)
#define PS_SYSTHD_PKTRRF (PS_SYSTHD_VIDBND+1)
#define PS_SYSTHD_STREAM (PS_SYSTHD_PKTRRF+1)
#define PS_SYSTHD_SIZE   (PS_SYSTHD_STREAM)
#define PS_SYSTHD_STRLEN 3

#define PS_STRMAP_CNI    (PES_HEADER_SIZE)
#define PS_STRMAP_PSIL   (PS_STRMAP_CNI+2)
#define PS_STRMAP_PSID   (PS_STRMAP_PSIL+2)
#define PS_STRMAP_SIZE   (PS_STRMAP_PSID+6)
#define PS_STRMAP_STRLEN 4

#define PS_STRDIR_NUMOAU (PES_HEADER_SIZE)
#define PS_STRDIR_PREVDO (PS_STRDIR_NUMOAU+2)
#define PS_STRDIR_NEXTDO (PS_STRDIR_PREVDO+6)
#define PS_STRDIR_SIZE   (PS_STRDIR_NEXTDO+6)
#define PS_STRDIR_SIZEAU 18

