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

#include <stdint.h>
#include "ts2pes.h"
#include "crc32.h"

unsigned long CRC;

// current stream parameters

#define TS_packet_length 188

// list of TS-Programs (as linked list)
ElementaryStream *first_ES=NULL;

// the curent valid PAT from PID 0x0000
ProgramAssociationTable *PAT=NULL;
// the intermediate PAT while building it from PID 0x0000
ProgramAssociationTable *current_PAT,*next_PAT;

// the curent valid CAT from PID 0x0001
ConditionalAccessTable *CAT=NULL;
// the intermediate CAT while building it from PID 0x0001
ConditionalAccessTable *current_CAT,*next_CAT;

// list of Programs (as linked list)
ProgramMapTable *first_PMT=NULL, *first_nextPMT=NULL, *phantom_PMT=NULL;


// -------------------------------------------------------------- //
//   List management function for transport stream packet headers
// -------------------------------------------------------------- //

ElementaryStream *find_ES(unsigned int PID) {  // find TSP with this PID
  ElementaryStream *ptr=first_ES;
  while (ptr!=NULL) {
    if ((ptr->TSP!=NULL) && (ptr->TSP->PID==PID)) return ptr;
    ptr=(ElementaryStream *)ptr->next;
  }
  return ptr;
}

ElementaryStream *new_ES(unsigned int PID) {  // find or create TSP with this PID
  ElementaryStream *ptr=first_ES;
  while (ptr!=NULL) {
    if ((ptr->TSP!=NULL) && (ptr->TSP->PID==PID)) return ptr;
    ptr=(ElementaryStream *)ptr->next;
  }
  if (first_ES==NULL) {
    first_ES=(ElementaryStream *)malloc(sizeof(ElementaryStream));
    ptr=first_ES;
  } else {
    ptr=first_ES;
    while (ptr->next!=NULL) ptr=(ElementaryStream *)ptr->next;
    ptr->next=malloc(sizeof(ElementaryStream));
    ptr=(ElementaryStream *)ptr->next;
  }
  if (ptr==NULL) return NULL;
  ptr->next=NULL;
  ptr->TSP=(TransportStreamProgram *)malloc(sizeof(TransportStreamProgram));
  if (ptr->TSP==NULL) return NULL;
  ptr->TSP->AF=NULL;
  ptr->TSP->continuity_counter=0x0F; // For continuity. First real CC is 0x00
  ptr->TSP->PID=PID;  // set PID
  ptr->Payload=NULL;
  ptr->payload_count=0;
  ptr->payload_length=0;
  return ptr;
}

void kill_ES(unsigned int PID) {  // search&destroy(tm Henry Rollins) TSP with this PID
  ElementaryStream *ptr=first_ES, *ptr0=NULL;
  while (ptr!=NULL) {
    if ((ptr->TSP!=NULL) && (ptr->TSP->PID==PID)) {
      if (ptr->TSP!=NULL) free(ptr->TSP);
      if (ptr->Payload!=NULL) free(ptr->Payload);
      if (ptr0==NULL) {
        first_ES=ptr->next;
        free(ptr);
        ptr=first_ES;
      } else {
        ptr0->next=ptr->next;
        free(ptr);
        ptr=ptr0;
      }
    }
    ptr0=ptr;
    if (ptr!=NULL) ptr=(ElementaryStream *)ptr->next;
  }
}


// -------------------------------------------------------------- //
//   List management functions for program map tables
// -------------------------------------------------------------- //

ProgramMapTable *find_PMT(ProgramMapTable **anker, unsigned int program_number) {  // find TSP with this program_number
  ProgramMapTable *ptr=*anker;
  while (ptr!=NULL) {
    if ((ptr->program_number==program_number)) return ptr;
    ptr=(ProgramMapTable *)ptr->next;
  }
  return ptr;
}

ProgramMapTable *new_PMT(ProgramMapTable **anker, unsigned int program_number) {  // find or create TSP with this program_number
  int i;
  ProgramMapTable *ptr=*anker;
  while (ptr!=NULL) {
    if ((ptr->program_number==program_number)) return ptr;
    ptr=(ProgramMapTable *)ptr->next;
  }
  if (*anker==NULL) {
    *anker=(ProgramMapTable *)malloc(sizeof(ProgramMapTable));
    ptr=*anker;
  } else {
    ptr=*anker;
    while (ptr->next!=NULL) ptr=(ProgramMapTable *)ptr->next;
    ptr->next=malloc(sizeof(ProgramMapTable));
    ptr=(ProgramMapTable *)ptr->next;
  }
  if (ptr==NULL) return NULL;
  ptr->next=NULL;
  ptr->descriptor_count=0;
  for (i=0; i<16; i++) ptr->descriptor[i]=NULL;
  ptr->program_count=0;
  for (i=0; i<256; i++) ptr->program[i]=NULL;
  ptr->program_number=program_number;  // set program_number
  return ptr;
}

void kill_PMT(ProgramMapTable **anker, unsigned int program_number) {  // search&destroy(tm Henry Rollins) TSP with this program_number
  ProgramMapTable *ptr=*anker, *ptr0=NULL;
  int i,j;
  while (ptr!=NULL) {
    if ((ptr->program_number==program_number)) {
      if (ptr0==NULL) {
        *anker=(ProgramMapTable *)ptr->next;
        ptr0=*anker;
      } else {
        ptr0->next=ptr->next;
      }
      for (i=0; i<16; i++) if (ptr->descriptor[i]!=NULL) {
        if (ptr->descriptor[i]->data!=NULL) free(ptr->descriptor[i]->data);
        free(ptr->descriptor[i]);
      }
      for (i=0; i<256; i++) if (ptr->program[i]!=NULL) {
        for (j=0; j<16; j++) if (ptr->program[i]->descriptor[j]!=NULL) {
          if (ptr->program[i]->descriptor[j]->data!=NULL) free(ptr->program[i]->descriptor[j]->data);
          free(ptr->program[i]->descriptor[j]);
        }
        free(ptr->program[i]);
      }
      free(ptr);
      ptr=ptr0;
    }
    ptr0=ptr;
    if (ptr!=NULL) ptr=(ProgramMapTable *)ptr->next;
  }
}


// -------------------------------------------------------------- //
//   debug output of header information
// -------------------------------------------------------------- //

void print_TP(TransportStreamProgram *TP) {  // debug output of TP-Header
  printf("- Transport Packet Header -\n\
transport_error_indicator:     %i\n\
payload_unit_start_indicator:  %i\n\
transport_priority:            %i\n\
PID:                           0x%04X\n\
transport_scrambling_control:  0x%02X\n\
adaption_field_control_1:      %i\n\
adaption_field_control_0:      %i\n\
continuity_counter:            0x%02X\n\
\n",
    ((TP->transport_error_indicator)?1:0),
    ((TP->payload_unit_start_indicator)?1:0),
    ((TP->transport_priority)?1:0),
    TP->PID,
    TP->transport_scrambling_control,
    ((TP->adaption_field_control_1)?1:0),
    ((TP->adaption_field_control_0)?1:0),
    TP->continuity_counter
  );
}

void print_AF(AdaptionField *AF) {  // debug output of AF
  int i;
  printf("- Adaption Field -\n\
discontinuity_indicator:               %i\n\
random_access_indicator:               %i\n\
elementary_stream_priority_indicator:  %i\n\
PCR_flag:                              %i\n\
OPCR_flag:                             %i\n\
splicing_point_flag:                   %i\n\
transport_private_data_flag:           %i\n\
adaption_field_extension_flag:         %i\n",
    ((AF->discontinuity_indicator)?1:0),
    ((AF->random_access_indicator)?1:0),
    ((AF->elementary_stream_priority_indicator)?1:0),
    ((AF->PCR_flag)?1:0),
    ((AF->OPCR_flag)?1:0),
    ((AF->splicing_point_flag)?1:0),
    ((AF->transport_private_data_flag)?1:0),
    ((AF->adaption_field_extension_flag)?1:0));
  if (AF->PCR_flag) printf("\
program_clock_reference_base:          %lld\n\
program_clock_reference_extension:     %d\n",
    AF->program_clock_reference_base,
    AF->program_clock_reference_extension);
  if (AF->OPCR_flag) printf("\
original_program_clock_reference_base: %lld\n\
original_program_clock_reference_extension: %d\n",
    AF->original_program_clock_reference_base,
    AF->original_program_clock_reference_extension);
  if (AF->splicing_point_flag) printf("SC:    %i\n",
    AF->splice_countdown);
  if (AF->transport_private_data_flag) {
    printf("\
transport_private_data_length:         %i\n",
      AF->transport_private_data_length);
    for (i=0; i<AF->transport_private_data_length; i++) {
      printf("%02X ",(AF->private_data_byte[i] & 0xFF));
      if (((i % 24)==23) || (i == AF->transport_private_data_length-1)) printf("\n");
    }
  }
  if (AF->adaption_field_extension_flag) {
    printf("\
adaption_field_extension_length:       %i\n\
ltw_flag:                              %i\n\
piecewise_rate_flag:                   %i\n\
seamless_splice_flag:                  %i\n",
      AF->adaption_field_extension_length,
      ((AF->ltw_flag)?1:0),
      ((AF->piecewise_rate_flag)?1:0),
      ((AF->seamless_splice_flag)?1:0));
    if (AF->ltw_flag) {
      printf("\
ltw_valid_flag:                        %i\n\
ltw_offset:                            %i\n",
        ((AF->ltw_valid_flag)?1:0),
        AF->ltw_offset);
    }
    if (AF->piecewise_rate_flag) {
      printf("\
piecewise_rate:                        %i\n",
        AF->piecewise_rate);
    }
    if (AF->seamless_splice_flag) {
      printf("\
splice_type:                           0x%01X\n\
DTS_next_AU:                           %lld\n",
        AF->splice_type,
        AF->DTS_next_AU);
    }
  }
  printf("\n");
}

void print_PAT(ProgramAssociationTable *PAT) {  // debug output of PAT
  int i;
  printf("- Program Association Table -\n\
table_id:                  %i\n\
section_syntax_indicator:  %i\n\
private_indicator:         %i\n\
section_length:            %i\n\
transport_stream_id:       0x%04X\n\
version_number:            %i\n\
current_next_indicator:    %i\n\
section_number:            %i\n\
last_section_number:       %i\n\
Table Size:                %i\n\
  PN     PID\n",
    PAT->table_id,
    ((PAT->section_syntax_indicator)?1:0),
    ((PAT->private_indicator)?1:0),
    PAT->section_length,
    PAT->transport_stream_id,
    PAT->version_number,
    ((PAT->current_next_indicator)?1:0),
    PAT->section_number,
    PAT->last_section_number,
    PAT->program_count);
  for (i=0; i<PAT->program_count; i++) {
    printf("  0x%04X 0x%04X\n",PAT->program_number[i],PAT->PID[i]);
  }
  printf("\n");
}

void print_CAT(ConditionalAccessTable *CAT) {  // debug output of CAT
  int i;
  printf("- Conditional Access Table -\n\
table_id:                  %i\n\
section_syntax_indicator:  %i\n\
private_indicator:         %i\n\
section_length:            %i\n\
version_number:            %i\n\
current_next_indicator:    %i\n\
section_number:            %i\n\
last_section_number:       %i\n\
Table Size:                %i\n\
  SysID  CA_PID\n",
    CAT->table_id,
    ((CAT->section_syntax_indicator)?1:0),
    ((CAT->private_indicator)?1:0),
    CAT->section_length,
    CAT->version_number,
    ((CAT->current_next_indicator)?1:0),
    CAT->section_number,
    CAT->last_section_number,
    CAT->descriptor_count);
  for (i=0; i<CAT->descriptor_count; i++) {
    printf("\
  0x%04X 0x%04X",
      CAT->CA_descriptor[i]->CA_system_ID,
      CAT->CA_descriptor[i]->CA_PID);
    show_descriptor(CAT->CA_descriptor[i]->descriptor,1);
    printf("\n");
  }
  printf("\n");
}

void print_PMT(ProgramMapTable *PMT) {  // debug output of PMT
  int i,j;
  Program *prog;           // temp. pointer
  printf("- Program Map Table -\n\
table_id:                  %i\n\
section_syntax_indicator:  %i\n\
private_indicator:         %i\n\
section_length:            %i\n\
program_number:            0x%04X\n\
version_number:            %i\n\
current_next_indicator:    %i\n\
section_number:            %i\n\
last_section_number:       %i\n\
PCR_PID:                   0x%04X\n\
Table Size:                %i\n\
  type PID\n",
    PMT->table_id,
    ((PMT->section_syntax_indicator)?1:0),
    ((PMT->private_indicator)?1:0),
    PMT->section_length,
    PMT->program_number,
    PMT->version_number,
    ((PMT->current_next_indicator)?1:0),
    PMT->section_number,
    PMT->last_section_number,
    PMT->PCR_PID,
    PMT->program_count);
  for (i=0; i<PMT->descriptor_count; i++) {
    show_descriptor(PMT->descriptor[i],1);
  }
  for (i=0; i<PMT->program_count; i++) {
    prog=PMT->program[i];
    printf("  0x%02X 0x%04X\n",prog->stream_type,prog->elementary_PID);
    for (j=0; j<prog->descriptor_count; j++) {
      show_descriptor(prog->descriptor[j],1);
    }
  }
  printf("\n");
}

void print_TSP(TransportStreamProgram *TSP) {  // debug output of whole TP
  print_TP(TSP);
  if (TSP->AF!=NULL) print_AF(TSP->AF);
}


// -------------------------------------------------------------- //
//   parsing of one PSI, collected from the TS-packet payload
// -------------------------------------------------------------- //

int parse_PSI(ElementaryStream *ES, unsigned int PSI, int verbose, int useCRC) {
  // header data for all PSI's
  unsigned char table_id;                 // Table ID
  unsigned int  section_syntax_indicator, // Section Syntax Indicator
                private_indicator,        // Private Indicator
                section_length,           // Section Length
                PSI_word;                 // Transport Stream ID / Program Number / Table ID Extension
  unsigned char version_number;           // Version Number
  unsigned int  current_next_indicator,   // Current/Next Indicator
                section_number,           // Section Number
                last_section_number;      // Last Section Number

  ProgramMapTable *PMT, *ptr;  // current PMT

  int i,j,n,               // counters
      program_info_length, // ProgramInformationLength
      ES_info_length,      // ESinfolength
      desc_length;         // descriptor length
  unsigned char ch,        // single byte
      desc_tag;            // descriptor tag
  Program *prog;           // temp. pointer

  table_id=ES->Payload[0];
  section_syntax_indicator=ES->Payload[1] & 0x80;
  private_indicator=ES->Payload[1] & 0x40;
  section_length=((ES->Payload[1] << 8) | (ES->Payload[2] & 0xFF)) & 0x0FFF;
  if (section_syntax_indicator) {
    PSI_word=((ES->Payload[3] << 8) | (ES->Payload[4] & 0xFF)) & 0xFFFF;
    version_number=(ES->Payload[5] & 0x3E) >> 1;
    current_next_indicator=ES->Payload[5] & 0x01;  
    section_number=ES->Payload[6] & 0xFF;   
    last_section_number=ES->Payload[7] & 0xFF;  
  } else {
    PSI_word=0;
    version_number=0;
    current_next_indicator=0;  
    section_number=0;   
    last_section_number=0;  
  }

  if (section_length+3 > ES->payload_count) {
    if (verbose) printf("ERROR in PSI-header, section length shows %i byte, but there are only %i\n",
      section_length+3,
      ES->payload_count);
    return 0x0104;
  }
  if (section_syntax_indicator) 
    CRC=update_crc_32_block(CRC_INIT_32,ES->Payload,section_length+3);
  
  if (verbose) {
    printf("PSI Section: %i bytes\n",section_length+3);
    for (i=0; i<section_length+3; i++) {
      printf("%02X ",(ES->Payload[i] & 0xFF));
      if ((i % 24)==23) printf("\n");
    }
    printf("\n");
    if (section_syntax_indicator) 
      printf("CRC of section %i: 0x%08lX - %s\n",
        section_number,
        CRC,
        ((CRC==0x00000000L)?"OK":"NG"));  // if you do it wrong, you get CRC==0xC704DD7BL
  }

  if (section_syntax_indicator && CRC) {  // CRC error
    if (verbose) printf("ERROR in PSI - CRC error (0x%08lX)\n",CRC);
    if (useCRC) return 0x0204;
  }
  
  if (PSI==1) {  // This is a PAT
    if (table_id!=0x00) {
      if (verbose) printf("- WRONG Table ID 0x%02X in section, PSI-type=%i!\n",table_id,PSI);
      if (useCRC) return 0x0304;
    }
    if (!section_syntax_indicator || private_indicator) {
      if (verbose) printf("- SYNTAX error in section, PSI-type=%i!\n",PSI);
      if (useCRC) return 0x0404;
    }
    next_PAT->table_id=table_id;
    next_PAT->section_syntax_indicator=section_syntax_indicator;
    next_PAT->private_indicator=private_indicator;
    next_PAT->section_length=section_length;
    next_PAT->transport_stream_id=PSI_word;
    next_PAT->version_number=version_number;
    next_PAT->current_next_indicator=current_next_indicator;
    next_PAT->section_number=section_number;
    next_PAT->last_section_number=last_section_number;
    i=8;
    while (i+4<section_length) {
      ch=ES->Payload[i++];  // you'll never know the evaluation order...
      next_PAT->program_number[next_PAT->program_count]=((ch << 8) | (ES->Payload[i++] & 0xFF)) & 0xFFFF;
      ch=ES->Payload[i++];
      next_PAT->PID[next_PAT->program_count++]=((ch << 8) | (ES->Payload[i++] & 0xFF)) & 0x1FFF;
    }
    if (verbose) print_PAT(next_PAT);
    if (current_next_indicator && (section_number==last_section_number)) {  // our new PAT has completely arrived
      PAT=next_PAT;
      next_PAT=current_PAT;
      current_PAT=PAT;
      next_PAT->program_count=0;
    }
    
  } else if (PSI==2) {  // CAT
    //if (verbose) printf("- Conditional Access Table -\n\n");
    if (table_id!=0x01) {
      if (verbose) printf("- WRONG Table ID 0x%02X in section, PSI-type=%i!\n",table_id,PSI);
      if (useCRC) return 0x0304;
    }
    if (!section_syntax_indicator || private_indicator) {
      if (verbose) printf("- SYNTAX error in section, PSI-type=%i!\n",PSI);
      if (useCRC) return 0x0404;
    }
    next_CAT->table_id=table_id;
    next_CAT->section_syntax_indicator=section_syntax_indicator;
    next_CAT->private_indicator=private_indicator;
    next_CAT->section_length=section_length;
    next_CAT->version_number=version_number;
    next_CAT->current_next_indicator=current_next_indicator;
    next_CAT->section_number=section_number;
    next_CAT->last_section_number=last_section_number;
    i=8; 
    while ((i+2<=section_length) && (next_CAT->descriptor_count<256)) {
      n=next_CAT->descriptor_count;
      if (next_CAT->CA_descriptor[n]==NULL) {
        next_CAT->CA_descriptor[n]=(ConditionalAccessDescriptor *)malloc(sizeof(ConditionalAccessDescriptor));
        next_CAT->CA_descriptor[n]->descriptor=NULL;
      }
      desc_tag=ES->Payload[i++];
      desc_length=ES->Payload[i++] & 0xFF;
      if (next_CAT->CA_descriptor[n]==NULL) {
        next_CAT->CA_descriptor[n]->descriptor=(Descriptor *)malloc(sizeof(Descriptor));
        next_CAT->CA_descriptor[n]->descriptor->data=NULL;
      }
      next_CAT->CA_descriptor[n]->descriptor->tag=desc_tag;
      next_CAT->CA_descriptor[n]->descriptor->length=desc_length;
      if (desc_length>0) {
        if (i+desc_length>n) {
          if (verbose) printf("ERROR: insufficent data in descriptor\n");
          if (useCRC) return 0x0704;
        }
        next_CAT->CA_descriptor[n]->descriptor->data=(unsigned char *)realloc(next_CAT->CA_descriptor[n]->descriptor->data,desc_length*sizeof(unsigned char));
        for (j=0; j<desc_length; j++) {
          next_CAT->CA_descriptor[n]->descriptor->data[j]=ES->Payload[i++];
        }
        if (desc_length>=2) next_CAT->CA_descriptor[n]->CA_system_ID=((next_CAT->CA_descriptor[n]->descriptor->data[0] << 8) | (next_CAT->CA_descriptor[n]->descriptor->data[1] & 0xFF)) & 0xFFFF;
        else next_CAT->CA_descriptor[n]->CA_system_ID=0;
        if (desc_length>=4) next_CAT->CA_descriptor[n]->CA_PID=((next_CAT->CA_descriptor[n]->descriptor->data[2] << 8) | (next_CAT->CA_descriptor[n]->descriptor->data[3] & 0xFF)) & 0x1FFF;
        else next_CAT->CA_descriptor[n]->CA_PID=0x1FFF;
      }
      next_CAT->descriptor_count++;
    }

/*    
    while (i+4<section_length) {
      next_CAT->descriptor[next_CAT->descriptor_count].descriptor_tag=ES->Payload[i++];
      next_CAT->descriptor[next_CAT->descriptor_count].descriptor_length=ES->Payload[i++];
      ch=ES->Payload[i++]; 
      next_CAT->descriptor[next_CAT->descriptor_count].CA_system_ID=((ch << 8) | (ES->Payload[i++] & 0xFF)) & 0xFFFF;
      ch=ES->Payload[i++]; 
      next_CAT->descriptor[next_CAT->descriptor_count].CA_PID=((ch << 8) | (ES->Payload[i++] & 0xFF)) & 0x1FFF;
      next_CAT->descriptor[next_CAT->descriptor_count].descriptor_tag=ES->Payload[i++];
      n=next_CAT->descriptor[next_CAT->descriptor_count].descriptor_length;
      if (n>4) {
        next_CAT->descriptor[next_CAT->descriptor_count].private_data_length=n-4;
        next_CAT->descriptor[next_CAT->descriptor_count].private_data_byte=(char *)malloc(n-4);
        for (j=0; j<n-4; j++)
          next_CAT->descriptor[next_CAT->descriptor_count].private_data_byte[j]=ES->Payload[i++];
      } else next_CAT->descriptor[next_CAT->descriptor_count].private_data_length=0;
      next_CAT->descriptor_count++;
    }
*/
    
    if (verbose) print_CAT(next_CAT);
    if (current_next_indicator && (section_number==last_section_number)) {  // our new CAT has completely arrived
      CAT=next_CAT;
      next_CAT=current_CAT;
      current_CAT=CAT;
      next_CAT->descriptor_count=0;
    }
    
  } else if (PSI==3) {  // PMT
    if (table_id!=0x02) {
      if (verbose) printf("- WRONG Table ID 0x%02X in section, PSI-type=%i!\n",table_id,PSI);
      if (useCRC) return 0x0304;
    }
    if (!section_syntax_indicator || private_indicator) {
      if (verbose) printf("- SYNTAX error in section, PSI-type=%i!\n",PSI);
      if (useCRC) return 0x0404;
    }
    PMT=new_PMT(&first_nextPMT,PSI_word);
    if (PMT==NULL) return 0x0503;
    PMT->table_id=table_id;
    PMT->section_syntax_indicator=section_syntax_indicator;
    PMT->private_indicator=private_indicator;
    PMT->section_length=section_length;
    PMT->program_number=PSI_word;
    PMT->version_number=version_number;
    PMT->current_next_indicator=current_next_indicator;
    PMT->section_number=section_number;
    PMT->last_section_number=last_section_number;
    PMT->PCR_PID=((ES->Payload[8] << 8) | (ES->Payload[9] & 0xFF)) & 0x1FFF;
    program_info_length=((ES->Payload[10] << 8) | (ES->Payload[11] & 0xFF)) & 0x0FFF;
    i=12; n=i+program_info_length;
    while ((i+2<=n) && (PMT->descriptor_count<16)) {
      desc_tag=ES->Payload[i++];
      desc_length=ES->Payload[i++] & 0xFF;
      if (PMT->descriptor[PMT->descriptor_count]==NULL) {
        PMT->descriptor[PMT->descriptor_count]=(Descriptor *)malloc(sizeof(Descriptor));
        PMT->descriptor[PMT->descriptor_count]->data=NULL;
      }
      PMT->descriptor[PMT->descriptor_count]->tag=desc_tag;
      PMT->descriptor[PMT->descriptor_count]->length=desc_length;
      if (desc_length>0) {
        if (i+desc_length>n) {
          if (verbose) printf("ERROR: insufficent data in descriptor %i+%i>=%i\n",i,desc_length,n);
          if (useCRC) return 0x0604;
        }
        PMT->descriptor[PMT->descriptor_count]->data=(unsigned char *)realloc(PMT->descriptor[PMT->descriptor_count]->data,desc_length*sizeof(unsigned char));
        for (j=0; j<desc_length; j++) {
          PMT->descriptor[PMT->descriptor_count]->data[j]=ES->Payload[i++];
        }
      }
      PMT->descriptor_count++;
    }
    i=n;
    while ((i+4<section_length) && (PMT->program_count<256)) {
      prog=PMT->program[PMT->program_count];
      if (prog==NULL) {
        prog=(Program *)malloc(sizeof(Program));
        PMT->program[PMT->program_count]=prog;
        prog->descriptor_count=0;
        for (j=0; j<16; j++) prog->descriptor[j]=NULL;
      }
      prog->stream_type=ES->Payload[i++];
      ch=ES->Payload[i++];  // you'll never know the evaluation order...
      prog->elementary_PID=((ch << 8) | (ES->Payload[i++] & 0xFF)) & 0x1FFF;
      ch=ES->Payload[i++];
      ES_info_length=((ch << 8) | (ES->Payload[i++] & 0xFF)) & 0x0FFF;
      n=i+ES_info_length;
      while ((i+2<=n) && (prog->descriptor_count<16)) {
        desc_tag=ES->Payload[i++];
        desc_length=ES->Payload[i++] & 0xFF;
        if (prog->descriptor[prog->descriptor_count]==NULL) {
          prog->descriptor[prog->descriptor_count]=(Descriptor *)malloc(sizeof(Descriptor));
          prog->descriptor[prog->descriptor_count]->data=NULL;
        }
        prog->descriptor[prog->descriptor_count]->tag=desc_tag;
        prog->descriptor[prog->descriptor_count]->length=desc_length;
        if (desc_length>0) {
          if (i+desc_length>n) {
            if (verbose) printf("ERROR: insufficent data in descriptor %i+%i>=%i\n",i,desc_length,n);
            if (useCRC) return 0x0704;
          }
          prog->descriptor[prog->descriptor_count]->data=(unsigned char *)realloc(prog->descriptor[prog->descriptor_count]->data,desc_length*sizeof(unsigned char));
          for (j=0; j<desc_length; j++) {
            prog->descriptor[prog->descriptor_count]->data[j]=ES->Payload[i++];
          }
        }
        prog->descriptor_count++;
      }
      PMT->program_count++;
      i=n;
    }
    if (verbose) print_PMT(PMT);
    if (current_next_indicator && (section_number==last_section_number)) {  // our new PMT has completely arrived
      kill_PMT(&first_PMT,PMT->program_number);  // kill outdated PMT
      ptr=first_PMT;  // move PMT from "next" to "current" list
      if (first_PMT==NULL) {
        first_PMT=PMT;
      } else {
        ptr=first_PMT;
        while (ptr->next!=NULL) ptr=(ProgramMapTable *)ptr->next;
        ptr->next=PMT;
      }
      if (first_nextPMT==PMT) {
        first_nextPMT=PMT->next;
      } else {
        ptr=first_nextPMT;
        while (ptr!=NULL) {
          if ((ptr->next==PMT)) {
            ptr->next=PMT->next;
          }
          ptr=(ProgramMapTable *)ptr->next;
        }
      }
      PMT->next=NULL;
    }
    
  } else if (PSI==4) {  // NIT
    if ((table_id<0x40) || (table_id>0xFE)) {
      if (verbose) printf("- WRONG Table ID 0x%02X in section, PSI-type=%i!\n",table_id,PSI);
      if (useCRC) return 0x0304;
    }
    if (!private_indicator) {
      if (verbose) printf("- SYNTAX error in section, PSI-type=%i!\n",PSI);
      if (useCRC) return 0x0404;
    }
    if (verbose) printf("- Network Information Table -\n\n");
  } else if (verbose) printf("- Internal error, Unknown PSI-id 0x%02X, table_id 0x%02X.\n",PSI,table_id);
  return 0;
}


// -------------------------------------------------------------- //
//   Parsing the TS-packet and calling parse_PSI 
//   or copy PES to stdout, if neccessary.
// parse_TP is analyzing one Transport Stream Packet at a time, 
// then the payload gets either sent to the parse_PSI function
// or, if it's a PES, is piped to stdout.
// -------------------------------------------------------------- //

int parse_TP(
  FILE* inputstream,            // mpeg-2 file
  int parse,                    // true: parse  false: skip packet
  int verbose,                  // true: debug output
  int pipe,                     // true: selected PES as binary  false: as Hexdump
  unsigned int select_program,  // number of the program to pipe out
  unsigned int select_stream,   // number of the stream in the PMT
  int useCRC) {                 // true: ignore CRC of PSI

  // Return codes:
  // 0x0000: OK
  // 0xXXEE: XX: error location  EE: error type
  // 0x0001: unexpected end of file
  // 0x0002: unexpected end of data
  // 0x0003: unexpected end of memory
  
  // bool evals to true if not 0
  
  unsigned char TP[TS_packet_length];  // one Transport Packet, raw
  
  ElementaryStream *ES;           // current ES

  TransportStreamProgram *TSP;    // current TSP
  
  unsigned int PID,   // Packet IDentification (13 bits)
               pointer_field;    // Pointer Field (1 byte, used in PSI-packets)
               
  AdaptionField *AF;        // current Adaption Field
  unsigned int AFL;      // Adaption Field Length
               
  int PSI,
      partlen;
  unsigned char table_id;                 // Table ID
  unsigned int  section_length;           // Section Length

  ProgramMapTable *PMT;  // current PMT

  Program *prog;           // temp. pointer

  unsigned int  PESprog;          // Elementary Stream Program Number
  unsigned char PEStype;          // Elementary Stream Media Type
  unsigned int  PESstream;        // Elementary Stream PMT-number
               
  int n,i,j,k;          // counters
  unsigned char ch;   // single byte
  int filechar;       

  if (!parse) verbose=0;  // skipping

  // scanning up to 1000 chars for the sync-character 0x47
  k=1000; i=0;
  while (((filechar=fgetc(inputstream))!=0x47) && (filechar>=0) && (k--)) {
    if (verbose) {
      printf("%02X ",filechar & 0xFF); // looking for sync byte
      if ((i++ % 24) == 23) printf("\n");
    }
  }
  if (k) { 
    if ((verbose) && (k<1000)) printf("\nSkipped %i byte looking for 0x47\n",1000-k);
    if (filechar<0) return 0x0001;
    TP[0]=filechar;
    
    // reading the remaining 187 bytes of the 188 byte TP-Packet
    if (fread(&TP[1],1,TS_packet_length-1,inputstream)!=TS_packet_length-1) return 0x0101;
      
    //if (!parse) return 0;  // skipping
    
    // finding/creating our TSP-structure for this PID
    PID=((TP[1] & 0x1F) << 8) | (TP[2] & 0xFF);
    ES=new_ES(PID);
    if (ES==NULL) return 0x0003;  // Insufficient Memory;
    TSP=ES->TSP;
    if (TSP==NULL) return 0x0103;  // Insufficient Memory;

    // parsing header parameters into TSP-structure
    TSP->transport_error_indicator=    TP[1] & 0x80;
    TSP->payload_unit_start_indicator= TP[1] & 0x40;
    TSP->transport_priority=           TP[1] & 0x20;
    //TSP->PID=((TP[1] & 0x1F) << 8) | (TP[2] & 0xFF);  // is already done
    TSP->transport_scrambling_control=(TP[3] >> 6) & 0x03;
    TSP->adaption_field_control_1=     TP[3] & 0x20;
    TSP->adaption_field_control_0=     TP[3] & 0x10;
    TSP->previous_cc=TSP->continuity_counter;
    TSP->continuity_counter=           TP[3] & 0x0F;
    
    // debugging output
    if (verbose) print_TP(TSP);

    if (TSP->transport_error_indicator) {
      if (verbose) printf("- Broken packet (transport_error_indicator)%s",(useCRC?", skipping...\n":""));
      if (useCRC) return 0;
    }
    
    if (PID==0x1FFF) {  // Null packet
      if (verbose) printf("- Null packet - %i byte\n\n",TS_packet_length-4);
      ES->payload_count=0;
      if (TSP->transport_scrambling_control)
        if (verbose) printf("- ERROR in transport_scrambling_control of null packet\n");
      if (!TSP->adaption_field_control_0 || TSP->adaption_field_control_1)
        if (verbose) printf("- ERROR in adaption_field_control of null packet\n");
      return 0;
    }
    
    if (!(TSP->adaption_field_control_1 && (TP[5] & 0x80))) {  // AF discontinuity_indicator?
      if (TSP->adaption_field_control_0 && (TSP->continuity_counter==TSP->previous_cc)) {
        if (verbose) printf("- Duplicate Packet - skipping...\n");
        return 0;
      }
      if (TSP->adaption_field_control_0 && (TSP->continuity_counter!=((TSP->previous_cc+1)&0x0F))) {
        if (verbose) printf("ERROR: Missing Packet(s): %i\n",(TSP->continuity_counter-TSP->previous_cc)&0x0F);
      }
    }

    // parsing Adaption Field, if any, and storing it into our TSP
    if (TSP->adaption_field_control_1) {  // Adaption Field present
      AFL=TP[4] & 0xFF;
      TSP->AF=(AdaptionField *)malloc(sizeof(AdaptionField));
      if ((AF=TSP->AF)==NULL) return 0x0203;
      if (AFL) {
        AF->discontinuity_indicator=TP[5] & 0x80;
        AF->random_access_indicator=TP[5] & 0x40;
        AF->elementary_stream_priority_indicator=TP[5] & 0x20;
        AF->PCR_flag=TP[5] & 0x10;
        AF->OPCR_flag=TP[5] & 0x08;
        AF->splicing_point_flag=TP[5] & 0x04;
        AF->transport_private_data_flag=TP[5] & 0x02;
        AF->adaption_field_extension_flag=TP[5] & 0x01;

        n=6;  // from here on we don't have static positions anymore
        
        if (AF->PCR_flag && ((n+6) <= (AFL+5))) {
          AF->program_clock_reference_base=TP[n++] & 0x000000FFULL;  // Bit 32-25
          AF->program_clock_reference_base=(AF->program_clock_reference_base << 8) | (TP[n++] & 0x000000FFULL);  // Bit 24-17
          AF->program_clock_reference_base=(AF->program_clock_reference_base << 8) | (TP[n++] & 0x000000FFULL);  // Bit 16-9
          AF->program_clock_reference_base=(AF->program_clock_reference_base << 8) | (TP[n++] & 0x000000FFULL);  // Bit 8-1
          ch=TP[n++];
          AF->program_clock_reference_base=(AF->program_clock_reference_base << 1) | ((ch >> 7) & 0x00000001ULL);  // Bit 0
          AF->program_clock_reference_extension=ch & 0x01;
          AF->program_clock_reference_extension=(AF->program_clock_reference_extension << 8) | (TP[n++] & 0xFF);
        } else if (AF->PCR_flag) return 0x0102;
        
        if (AF->OPCR_flag && ((n+6) <= (AFL+5))) {
          AF->original_program_clock_reference_base=TP[n++] & 0x000000FFULL;  // Bit 32-25
          AF->original_program_clock_reference_base=(AF->original_program_clock_reference_base << 8) | (TP[n++] & 0x000000FFULL);  // Bit 24-17
          AF->original_program_clock_reference_base=(AF->original_program_clock_reference_base << 8) | (TP[n++] & 0x000000FFULL);  // Bit 16-9
          AF->original_program_clock_reference_base=(AF->original_program_clock_reference_base << 8) | (TP[n++] & 0x000000FFULL);  // Bit 8-1
          ch=TP[n++];
          AF->original_program_clock_reference_base=(AF->original_program_clock_reference_base << 1) | ((ch >> 7) & 0x00000001ULL);  // Bit 0
          AF->original_program_clock_reference_extension=ch & 0x01;
          AF->original_program_clock_reference_extension=(AF->original_program_clock_reference_extension << 8) | (TP[n++] & 0xFF);
        } else if (AF->OPCR_flag) return 0x0202;
        
        if (AF->splicing_point_flag && (n < (AFL+5))) {
          AF->splice_countdown=TP[n++] & 0xFF;
        } else if (AF->splicing_point_flag) return 0x0302;
        
        if (AF->transport_private_data_flag && (n < (AFL+5))) {
          AF->transport_private_data_length=TP[n++] & 0xFF;
          if (n+AF->transport_private_data_length > AFL) return 0x0402;
          for (i=0; (i<AF->transport_private_data_length) && (n<AFL); i++) {
            AF->private_data_byte[i]=TP[n++];
          }
        } else if (AF->transport_private_data_flag) return 0x0502;
        
        if (AF->adaption_field_extension_flag && (n < (AFL+5))) {
          AF->adaption_field_extension_length=TP[n++] & 0xFF;
          if (n+AF->adaption_field_extension_length > (AFL+5)) return 0x0602;
          ch=TP[n++];
          AF->ltw_flag=ch & 0x80;
          AF->piecewise_rate_flag=ch & 0x40;
          AF->seamless_splice_flag=ch & 0x20;
          if (AF->ltw_flag) {
            ch=TP[n++];
            AF->ltw_valid_flag=ch & 0x80;
            AF->ltw_offset=((ch & 0x7F) << 8) | (TP[n++] & 0xFF);
          }
          if (AF->piecewise_rate_flag) {
            AF->piecewise_rate=TP[n++] & 0x3F; 
            AF->piecewise_rate=(AF->piecewise_rate << 8) | (TP[n++] & 0xFF); 
            AF->piecewise_rate=(AF->piecewise_rate << 8) | (TP[n++] & 0xFF); 
          }
          if (AF->seamless_splice_flag) {
            ch=TP[n++];
            AF->splice_type=(ch >> 4) & 0x0F;
            AF->DTS_next_AU=(ch >> 1) & 0x07ULL;  // Bit 32-30
            AF->DTS_next_AU=(AF->DTS_next_AU << 8) | (TP[n++] & 0xFFULL);  // Bit 29-22
            AF->DTS_next_AU=(AF->DTS_next_AU << 7) | ((TP[n++] >> 1) & 0x7FULL);  // Bit 21-15
            AF->DTS_next_AU=(AF->DTS_next_AU << 8) | (TP[n++] & 0xFFULL);  // Bit 14-7
            AF->DTS_next_AU=(AF->DTS_next_AU << 7) | ((TP[n++] >> 1) & 0x7FULL);  // Bit 6-0
          }
        } else if (AF->adaption_field_extension_flag) return 0x0702;

        // debugging output        
        if (verbose) {
          //printf("Adaption Field Length: %i bytes\n",AFL);
          //for (i=0; i<AFL; i++) {
          //  printf("%02X ",(TP[i+5] & 0xFF));
          //  if (((i % 24)==23) || (i == AFL-1)) printf("\n");
          //}
          print_AF(AF);
        }
        
      }
      AFL++; // AFL includes now the AFL-byte itself
    } else AFL=0;

    if (TSP->adaption_field_control_0) {  // Payload present
    
      n=AFL+4;  // offset of Payload in TP[]
      
      // determine payload type: PES or PSI?
      // PSI-type: 0:PES 1:PAT 2:CAT 3:PMT 4:NIT 5:Null -1:Unknown
      if (PID==0x0000) PSI=1;          // is the PSI a PAT?
      else if (PID==0x0001) PSI=2;     // is the PSI a CAT?
      else if (PID==0x1FFF) PSI=-2;    // Null Packet? Already sorted out, but anyhow...
      else {
        PSI=-1;    // default to Unknown
        if (PAT!=NULL) {
          for (i=0; i<PAT->program_count; i++) {  // unless the PID is in the current PAT
            if (PID==PAT->PID[i]) 
              PSI=((PAT->program_number[i])?3:4); // Program Number 0 means NIT, otherwise PMT
          }
        }
        if (CAT!=NULL) {
          for (i=0; i<CAT->descriptor_count; i++) {  // PES-PID can be in CAT, too. (ECM and EMM)
            if (PID==CAT->CA_descriptor[i]->CA_PID) {
              PSI=0;  // ECM or EMM, has to go to the PES-decoder
              PESprog=0; // Program Number
              PEStype=0; // Stream Media Type
              PESstream=0;
            }
          }
        }
        PMT=first_PMT;
        while (PMT!=NULL) {  // check all current PMT's
          for (i=0; i<PMT->program_count; i++) {  // or the PID is in the current PMT, then it's a PES
            if (PID==PMT->program[i]->elementary_PID) {
              PSI=0;                       // it's a PES
              PESprog=PMT->program_number; // Program Number
              PEStype=PMT->program[i]->stream_type; // Stream Media Type
              PESstream=i+1;
            }
          }
          PMT=(ProgramMapTable *)PMT->next;
        }
      }
      
      if (PSI<0) {  // unknown PSI, let's see if we already guessed the PID
        if (phantom_PMT!=NULL) {
          for (i=0; i<phantom_PMT->program_count; i++) {  // or the PID is in the current PMT, then it's a PES
            if (PID==phantom_PMT->program[i]->elementary_PID) {
              PSI=0;                       // it's a PES
              PESprog=phantom_PMT->program_number; // Program Number
              PEStype=phantom_PMT->program[i]->stream_type; // Stream Media Type
              PESstream=i+1;
              if (verbose) printf("  previous GUESS: PES # %i, type 0x%02X\n",PESstream,PEStype);
            }
          }
        }
      }

      if (PSI<0) {  // unknown PSI, let's try to guess what it might be
        if (TSP->payload_unit_start_indicator) {  // a new payload block, only here we have a chance
          if ((!TP[n]) && (!TP[n+1]) && (TP[n+2]=0x01)) {
            PSI=0;  // Packet start code prefix, it's a PES
            PESprog=0xFFFF; // Program Number
            PEStype=0xFF;   // Stream Media Type
            PESstream=0;
            if ((TP[n+3] & 0xF0) == 0xE0) {
              PEStype=0x02;  // MPEG-2 Video
            } else if ((TP[n+3] & 0xE0) == 0xC0) {
              PEStype=0x04;  // MPEG-2 Audio
            }
            if (phantom_PMT!=NULL) {
              prog=phantom_PMT->program[phantom_PMT->program_count];
              if (prog==NULL) {
                prog=(Program *)malloc(sizeof(Program));
                phantom_PMT->program[phantom_PMT->program_count]=prog;
                prog->descriptor_count=0;
                for (j=0; j<16; j++) prog->descriptor[j]=NULL;
              }
              prog->elementary_PID=PID;
              prog->stream_type=PEStype;
              phantom_PMT->program_count++;
              PESstream=phantom_PMT->program_count;
            }
            if (verbose) printf("  GUESS: PES # %i, type 0x%02X\n",PESstream,PEStype);
          } else {
            pointer_field=TP[n];
            if (n+pointer_field+1<TS_packet_length) {
              table_id=TP[n+pointer_field+1];
              if (table_id==0x02) PSI=3;  // might be a PMT
              else if (table_id==0xFF) PSI=5;  // might be a Null PSI
              else if (table_id>=0x40) PSI=4;  // might be a NIT
              if (verbose && (PSI>0)) printf("  GUESS: PSI, %s, table_id 0x%02X\n",((PSI==3)?"PMT":((PSI==4)?"NIT":"Null PSI")),table_id);
            }
          }
        }
      }

      if (PSI<0) {
         if (verbose) printf("- ERROR packet with Unknown PID.\n");
      } else if (PSI) {
        pointer_field=((TSP->payload_unit_start_indicator)?TP[n++] & 0xFF:0);
        while (n<TS_packet_length) {
          if (ES->payload_count>=1) {  // this PSI has already started
            table_id=ES->Payload[0];
            if (ES->payload_count>=2) {
              section_length=((ES->Payload[1] << 8) | (TP[n] & 0xFF)) & 0x0FFF;
            } else if (ES->payload_count>=3) {
              section_length=((ES->Payload[1] << 8) | (ES->Payload[2] & 0xFF)) & 0x0FFF;
            } else {
              section_length=((TP[n] << 8) | (TP[n+1] & 0xFF)) & 0x0FFF;
            }
          } else {
            table_id=TP[n];
            section_length=((TP[n+1] << 8) | (TP[n+2] & 0xFF)) & 0x0FFF;
          }
          if (table_id==0xFF) {  // stuffing bytes
            i=n;
            while (TP[n]==0xFF) n++;
            ES->payload_count=0;
            if (verbose) printf("- PSI section - %i stuffing bytes\n",n-i);
          } else {
            partlen=TS_packet_length-n;
            if (partlen+ES->payload_count>section_length+3) partlen=section_length+3-ES->payload_count;
            if ((ES->payload_length - ES->payload_count) < partlen) {  // we need more memory
              ES->Payload=(char *)realloc(ES->Payload,ES->payload_count+partlen);
              if (ES->Payload==NULL) return 0x0203;
              ES->payload_length=ES->payload_count+partlen;
            }
            for (i=0; i<partlen; i++) {
              ES->Payload[ES->payload_count++]=TP[n++];
              //if (verbose) {
              //  printf("- Payload - for PID 0x%04X (PSI) now %i byte\n",PID,ES->payload_count);
              //  for (i=0; i<ES->payload_count; i++) {
              //    printf("%02X ",(ES->Payload[i] & 0xFF));
              //    if ((i % 24)==23) printf("\n");
              //  }
              //  printf("\n");
              //}
            }
            if (ES->payload_count>=section_length+3) {  // section complete
              parse_PSI(ES,PSI,verbose,useCRC);
              ES->payload_count=0;  // PSI done, dispose.
            } else {
              if (verbose) printf("Incomplete PSI section, appending %i byte to buffer, now %i byte.\n",partlen,ES->payload_count);
            }
          }
        }

      } else if (parse) {
        if (verbose || !pipe) {
          if (verbose || ((select_program==PESprog) && (select_stream==PESstream))) {
            printf("PES packet:\nProgram: 0x%04X\nStream:  %i\nType:    0x%02X\n",PESprog,PESstream,PEStype);
            printf("- PES fragment - %i byte\n",TS_packet_length-n);
            for (i=0; i<TS_packet_length-n; i++) {
              printf("%02X ",(TP[n+i] & 0xFF));
              if ((i % 24)==23) printf("\n");
            }
            printf("\n");
          }
        } else if (pipe && ((select_program==PESprog) && (select_stream==PESstream))) {
          while (n<TS_packet_length) putchar(TP[n++]);  // ... und dafuer der ganze Aufwand.
        }
      }
    }
    if (verbose) printf("\n");
  }
  return 0;
}

char *StreamType(unsigned char stream_type) {
  if (stream_type==0x00) return "ITU-T | ISO/IEC reserved";
  else if (stream_type==0x01) return "ISO/IEC 11172-2 Video";
  else if (stream_type==0x02) return "ITU-T Rec. H.262 | ISO/IEC 13818-2 Video";
  else if (stream_type==0x03) return "ISO/IEC 11172-3 Audio";
  else if (stream_type==0x04) return "ISO/IEC 13818-3 Audio";
  else if (stream_type==0x05) return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections";
  else if (stream_type==0x06) return "ITU-T Rec. H.222.0 | ISO/IEC 13818-3 PES private data";
  else if (stream_type==0x07) return "ISO/IEC 13522 MHEG";
  else if (stream_type==0x08) return "DSM CC";
  else if (stream_type==0x09) return "ITU-T Rec. H.222.1";
  else if (stream_type==0x0A) return "ISO/IEC 13818-6 Type A";
  else if (stream_type==0x0B) return "ISO/IEC 13818-6 Type B";
  else if (stream_type==0x0C) return "ISO/IEC 13818-6 Type C";
  else if (stream_type==0x0D) return "ISO/IEC 13818-6 Type D";
  else if (stream_type==0x0E) return "ISO/IEC 13818-1 auxiliary";
  else if (stream_type>=0x80) return "User private";
  else return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 reserved";
}


// -------------------------------------------------------------- //
//   main: opening file and repeat calling parse_TP
// -------------------------------------------------------------- //

int main (int argc, char* argv[]) {

  FILE* inputstream;
  int i,j,filearg,startpacket,stoppacket,err,verbose,pipe,useCRC;
  unsigned int prog,stream;
  char buf[256];
  
  ProgramMapTable *PMT;  // current PMT

  i=1;
  filearg=0;
  prog=0;
  stream=0;
  verbose=0;
  pipe=0;
  startpacket=0;
  stoppacket=0;
  useCRC=0;
  while (i<argc) {
    if (argv[i][0]=='-') {  // option
      if (argv[i][1]=='v') verbose=1;    // verbose on
      else if (argv[i][1]=='p') pipe=1;  // pipe on
      else if (argv[i][1]=='c') useCRC=1;  // CRC on
      else if (argv[i][1]=='s') {        // select a program
        i++;
        for (j=0; (j<strlen(argv[i])) && (argv[i][j]!='/'); j++);
        if (j<strlen(argv[i])) {
          prog=atoi(strncpy(buf,argv[i],j));
          stream=atoi(strcpy(buf,&argv[i][j+1]));
        }
      } else {  // unknown option
        printf("\
Usage: %s [-v] [-p] [-c] [-s <prog>/<stream>] [<file> [<first packet> <last packet> | <packet>]]\n\
-v: verbose on\n\
-p: pipe on (only raw output)\n\
-c: use CRC on PSI\n\
-s <prog>/<stream>: show/pipe only the <stream>th PES-stream of program <prog>\n\
",argv[0]);
      }
    } else if (!filearg) filearg=i;
    else if (!startpacket) startpacket=atoi(argv[i]);
    else if (!stoppacket) stoppacket=atoi(argv[i]);
    i++;
  }

  if (filearg) {
    if ((inputstream=fopen(argv[filearg],"r"))==NULL) {
      printf("Couldn't open file %s\n",argv[1]);
      exit (0);
    }
  } else inputstream=stdin;

  gen_crc32_table();
  
  if (startpacket) {
    if (!stoppacket) stoppacket=startpacket;
  } else stoppacket=-1;
  
  current_PAT=(ProgramAssociationTable *)malloc(sizeof(ProgramAssociationTable));
  next_PAT=(ProgramAssociationTable *)malloc(sizeof(ProgramAssociationTable));
  PAT=NULL;
  current_PAT->program_count=0;
  next_PAT->program_count=0;
  
  current_CAT=(ConditionalAccessTable *)malloc(sizeof(ConditionalAccessTable));
  next_CAT=(ConditionalAccessTable *)malloc(sizeof(ConditionalAccessTable));
  CAT=NULL;
  current_CAT->descriptor_count=0;
  for (i=0; i<256; i++) current_CAT->CA_descriptor[i]=NULL;
  next_CAT->descriptor_count=0;
  for (i=0; i<256; i++) next_CAT->CA_descriptor[i]=NULL;
  
  phantom_PMT=(ProgramMapTable *)malloc(sizeof(ProgramMapTable));  // PMT for guessed channels
  phantom_PMT->program_number=0xFFFF;
  phantom_PMT->program_count=0;
  
  err=0;
  for (i=0; ((stoppacket<0) || (i<stoppacket)) && (err==0); i++) {
    if ((startpacket<=(i+1)) && verbose) printf("--- Packet %i --- %ld\n",i+1,ftell(inputstream));
    err=parse_TP(inputstream,(startpacket<=(i+1)),verbose,pipe,prog,stream,useCRC);
  }
  if (err && verbose) printf("\nError in packet %i at %ld: 0x%04X (%s)\n",i,ftell(inputstream),err,(((err & 0xFF)==0x01)?"End of File":(((err & 0xFF)==0x02)?"Data Format Error":"Unknown Error")));
  
  if (!pipe) {
    if (PAT!=NULL) {
      printf("Transport stream ID: 0x%04X\n",PAT->transport_stream_id);
      if (PAT->program_count>0) {
        printf("  Contains %i Program%s:\n",PAT->program_count,((PAT->program_count==1)?"":"s"));
        for (i=0; i<PAT->program_count; i++) {
          printf("    %i\n",PAT->program_number[i]);
        }
      }
    }
    PMT=first_PMT;
    while (PMT!=NULL) {
      printf("  Program %i with %i stream%s\n",PMT->program_number,PMT->program_count,((PMT->program_count==1)?"":"s"));
      for (i=0; i<PMT->descriptor_count; i++) {
        printf("    ");
        show_descriptor(PMT->descriptor[i],0);
      }
      for (i=0; i<PMT->program_count; i++) {
        printf("    Stream %i, Type: %s (0x%02X)\n",
          i+1,
          StreamType(PMT->program[i]->stream_type),
          PMT->program[i]->stream_type);
        for (j=0; j<PMT->program[i]->descriptor_count; j++) {
          printf("      ");
          show_descriptor(PMT->program[i]->descriptor[j],0);
        }
      }
      PMT=(ProgramMapTable *)PMT->next;
    }
  }
  fclose(inputstream);

  return 0;
}
