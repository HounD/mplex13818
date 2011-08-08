/*
 * read transport stream
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
#include "ts2pesdescr.h"

typedef struct {  // all possible data in a transport stream adaption field, long word aligned
  int discontinuity_indicator,  // bool. true means PCR jump
      random_access_indicator,  // bool.
      elementary_stream_priority_indicator,  // bool.
      PCR_flag,  // bool. Indicates valid PCR
      OPCR_flag,  // bool. Indicates valid OPCR
      splicing_point_flag,  // bool. Indicates valid splice_countdown
      transport_private_data_flag,  // bool. Indicates valid tp-data
      adaption_field_extension_flag;  // bool. Indicates valid AFE
  unsigned long long int program_clock_reference_base;
  unsigned int program_clock_reference_extension;
  unsigned long long int original_program_clock_reference_base;
  unsigned int original_program_clock_reference_extension;
  int splice_countdown;  // signed!
  unsigned int transport_private_data_length;
  unsigned char private_data_byte[180];
  unsigned int adaption_field_extension_length;
  int ltw_flag,
      piecewise_rate_flag,
      seamless_splice_flag,
      ltw_valid_flag;
  unsigned int ltw_offset,
      piecewise_rate,
      splice_type;
  unsigned long long int DTS_next_AU;
} AdaptionField;

typedef struct {
  unsigned int table_id,                // 0:PAT 1:CAT 2:PMT
               section_syntax_indicator,
               private_indicator,
               section_length,
               transport_stream_id,     // Identifier of the Stream
               version_number,          // Table version ID
               current_next_indicator,
               section_number,
               last_section_number;
  int          program_count;           // Number of Entries in Table
  unsigned int program_number[256],     // 0:NetworkPID >0:ProgramMapPID
               PID[256];                // List of PIDs
} ProgramAssociationTable;

typedef struct {
  Descriptor   *descriptor;
  unsigned int CA_system_ID,
               CA_PID;
} ConditionalAccessDescriptor; 

typedef struct {
  unsigned int table_id,                // 0:PAT 1:CAT 2:PMT
               section_syntax_indicator,
               private_indicator,
               section_length,
               version_number,          // Table version ID
               current_next_indicator,
               section_number,
               last_section_number;
  int          descriptor_count;           // Number of Entries in Table
  ConditionalAccessDescriptor *CA_descriptor[256];
} ConditionalAccessTable;

typedef struct {
  unsigned char stream_type;        // Stream Type
  unsigned int  elementary_PID;     // Elementary PID
  int           descriptor_count;   // Number of Entries in Table
  Descriptor    *descriptor[16];    // *** TO DO *** Is 16 enough?
} Program;
  
typedef struct {
  unsigned int table_id,                // 0:PAT 1:CAT 2:PMT
               section_syntax_indicator,
               private_indicator,
               section_length,
               program_number,   
               version_number,          // Table version ID
               current_next_indicator,
               section_number,
               last_section_number,
               PCR_PID;
  int          descriptor_count;         // Number of Entries in Table
  Descriptor   *descriptor[16];          // *** TO DO *** Is 16 enough?  
  int          program_count;            // Number of Entries in Table
  Program      *program[256];            // *** TO DO *** Is 256 enough?  
  void *next;
} ProgramMapTable;

typedef struct {
  unsigned int transport_error_indicator,     // bool. true: This packet is broken
               payload_unit_start_indicator,  // bool. true: this is the start of a PES/PSI block.
               transport_priority,            // bool. true: i am very important!
               PID,                           // 13 bit Program IDentification
               transport_scrambling_control,  // 2 bits. Indicates scrambling method, if any.
               adaption_field_control_1,      // bool. true: adaption field present
               adaption_field_control_0,      // bool. true: payload (PES/PSI block) present
               continuity_counter,            // 4 bit counter for continued payload blocks
               previous_cc;                   // continuity_counter from last received TP-Packet with this PID
  AdaptionField *AF;                          // AF, if any
} TransportStreamProgram;

typedef struct {
  TransportStreamProgram *TSP;
  unsigned char *Payload;
  int payload_count,  // current length of Payload
      payload_length; // allocated length of Payload
  void *next;
} ElementaryStream;


//TransportStreamProgram *find_TSP(unsigned int PID);
//TransportStreamProgram *new_TSP(unsigned int PID);
//void kill_TSP(unsigned int PID);
ElementaryStream *find_ES(unsigned int PID);  // find TSP with this PID
ElementaryStream *new_ES(unsigned int PID);  // find or create TSP with this PID
void kill_ES(unsigned int PID);  // search&destroy(tm Henry Rollins) TSP with this PID

void print_TP(TransportStreamProgram *TP);  // debug output of TP-Header
void print_AF(AdaptionField *AF);  // debug output of AF
void print_PAT(ProgramAssociationTable *PAT);  // debug output of PAT
void print_PMT(ProgramMapTable *PMT);  // debug output of PMT
void print_TSP(TransportStreamProgram *TSP);  // debug output of whole TP

int parse_PSI(ElementaryStream *ES, unsigned int PSI, int verbose, int useCRC);

int parse_TP(
  FILE* inputstream,            // mpeg-2 file
  int parse,                    // true: parse  false: skip packet
  int verbose,                  // true: debug output
  int pipe,                     // true: selected PES as binary  false: as Hexdump
  unsigned int select_program,  // number of the program to pipe out
  unsigned int select_stream,   // number of the stream in the PMT
  int useCRC);                  // true: ignore CRC of PSI
