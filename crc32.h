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

#define MSBFCOEFF(x) | (1UL << x)

#define POLYNOMIAL_32_MSBF 0 \
    MSBFCOEFF(0)  \
    MSBFCOEFF(1)  \
    MSBFCOEFF(2)  \
    MSBFCOEFF(4)  \
    MSBFCOEFF(5)  \
    MSBFCOEFF(7)  \
    MSBFCOEFF(8)  \
    MSBFCOEFF(10) \
    MSBFCOEFF(11) \
    MSBFCOEFF(12) \
    MSBFCOEFF(16) \
    MSBFCOEFF(22) \
    MSBFCOEFF(23) \
    MSBFCOEFF(26)

#define CRC_INIT_32   ((uint32_t)(-1))

#define update_crc_32(crc, data) \
    ((crc << 8) ^ crc_32_table[((int)(crc >> 24) ^ (data)) & 0xFF])

extern uint32_t crc_32_table[256];

extern void gen_crc32_table();

extern uint32_t update_crc_32_block(uint32_t crc, char *data_block_ptr, int data_block_size);

extern void crc32_calc (char *data,
    int size,
    char *crc);

