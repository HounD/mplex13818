/*
 * cyclic redundancy check 16 bit
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

#define MSBFCOEFF(x) | (1UL << x)

#define POLYNOMIAL_16_MSBF 0 \
    MSBFCOEFF(0)  \
    MSBFCOEFF(1)  \
    MSBFCOEFF(5)  \
    MSBFCOEFF(12)

#define CRC_INIT_16   ((uint16_t)(-1))

#define update_crc_16(crc, data) \
    ((crc << 8) ^ crc_16_table[((int)(crc >> 8 ) ^ (data)) & 0xFF])

extern uint16_t crc_16_table[256];

extern void gen_crc16_table();

extern uint16_t update_crc_16_block(uint16_t crc, char *data_block_ptr, int data_block_size);

extern void crc16_calc (char *data,
    int size,
    char *crc);

