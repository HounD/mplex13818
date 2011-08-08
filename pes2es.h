/*
 * read packetised elementary stream
 * Copyright (C) 1999-2004 Christian Wolff
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {  // data in the PES-header
  unsigned int stream_id,
               PES_packet_length,
               PES_scrambling_control;
  int          PES_priority,
               data_alignment_indicator,
               copyright,
               original_or_copy,
               PTS_flag,
               DTS_flag,
               ESCR_flag,
               ES_rate_flag,
               DSM_trick_mode_flag,
               additional_copy_info_flag,
               PES_CRC_flag,
               PES_extension_flag;
  unsigned int PES_header_data_length;
  unsigned long long int PTS,
                         DTS,
                         ESCR_base;
  unsigned int ESCR_extension,
               ES_rate,
               trick_mode_control,
               additional_copy_info,
               previous_PES_packet_CRC;
  int          PES_private_data_flag,
               pack_header_field_flag,
               program_packet_sequence_counter_flag,
               P_STD_buffer_flag,
               PES_extension_flag_2;
  char         PES_private_data[128];
  unsigned int pack_field_length,
               program_packet_sequence_counter;
  int          MPEG1_MPEG2_identifier;
  unsigned int original_stuff_length;
  int          P_STD_buffer_scale;
  unsigned int P_STD_buffer_size,
               PES_extension_field_length;
} PES_header;

#define program_stream_map_id 0xBC
#define private_stream_1_id 0xBD
#define padding_stream_id 0xBE
#define private_stream_2_id 0xBF
#define ECM_stream_id 0xF0
#define EMM_stream_id 0xF1
#define DSMCC_stream_id 0xF2
#define ISO_13522_stream_id 0xF3
#define ITU_A_id 0xF4
#define ITU_B_id 0xF5
#define ITU_C_id 0xF6
#define ITU_D_id 0xF7
#define ITU_E_id 0xF8
#define ancillary_stream_id 0xF9
#define program_stream_directory_id 0xFF
                              
void print_PES(PES_header *PES);  // debug output of PES-Header

int parse_PES(
  FILE* inputstream,            // mpeg-2 file
  int parse,                    // true: parse  false: skip packet
  int verbose,                  // true: debug output
  int pipe,                     // true: selected PES as binary  false: as Hexdump
  int useCRC);                  // true: ignore CRC of PSI
