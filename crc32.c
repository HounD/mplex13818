/*
 * cyclic redundancy check 32 bit
 * Copyright (C) 1999 Christian Wolff, 2004 Oskar Schirmer
 * for Convergence Integrated Media GmbH (http://www.convergence.de)
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

#include <stdint.h>
#include "crc32.h"

uint32_t crc_32_table[256];

// generate the tables of CRC-32 remainders for all possible bytes
void gen_crc32_table() { 
  register int i,j;  
  register uint32_t crc32;
  for (i=0; i<256; i++) {
    crc32=(uint32_t)i << 24;
    for (j=0; j<8; j++) {
      crc32 = (crc32 << 1) ^ ((crc32 & (1<<31)) ? POLYNOMIAL_32_MSBF : 0);
    }
    crc_32_table[i]=crc32; 
  }
}

// update the CRC on the data block one byte at a time
uint32_t update_crc_32_block(uint32_t crc, char *data_block_ptr, int data_block_size)
{
  register int i;
  for (i=data_block_size; i>0; i--)
    crc=update_crc_32(crc, *data_block_ptr++);
  return crc;
}

void crc32_calc (char *data,
    int size,
    char *crc)
{
  uint32_t c;
  c = CRC_INIT_32;
  while (--size >= 0) {
    c = update_crc_32 (c,*data++);
  }
  crc[3] = c; c >>= 8;
  crc[2] = c; c >>= 8;
  crc[1] = c; c >>= 8;
  crc[0] = c;
}

