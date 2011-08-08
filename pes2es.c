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

#include <stdint.h>
#include "pes2es.h"
#include "crc16.h"

PES_header *PES;

unsigned short CRC;

void print_PES(PES_header *PES) {  // debug output of PES-Header
  int i;
  printf("- PES Header -\n\
stream_id:               0x%02X\n\
PES_packet_length:       %i\n",
    PES->stream_id,
    PES->PES_packet_length);
  if (
    (PES->stream_id!=padding_stream_id) &&
    (PES->stream_id!=private_stream_2_id) &&
    (PES->stream_id!=ECM_stream_id) &&
    (PES->stream_id!=EMM_stream_id) &&
    (PES->stream_id!=DSMCC_stream_id) &&
    (PES->stream_id!=ITU_E_id) &&
    (PES->stream_id!=program_stream_directory_id)) {
    printf("\
PES_scrambling_control:      %i\n\
PES_priority:                %i\n\
data_alignment_indicator:    %i\n\
copyright:                   %i\n\
original_or_copy:            %i\n\
PTS_flag:                    %i\n\
DTS_flag:                    %i\n\
ESCR_flag:                   %i\n\
ES_rate_flag:                %i\n\
DSM_trick_mode_flag:         %i\n\
additional_copy_info_flag:   %i\n\
PES_CRC_flag:                %i\n\
PES_extension_flag:          %i\n\
PES_header_data_length:      %i\n",
      PES->PES_scrambling_control,
      ((PES->PES_priority)?1:0),
      ((PES->data_alignment_indicator)?1:0),
      ((PES->copyright)?1:0),
      ((PES->original_or_copy)?1:0),
      ((PES->PTS_flag)?1:0),
      ((PES->DTS_flag)?1:0),
      ((PES->ESCR_flag)?1:0),
      ((PES->ES_rate_flag)?1:0),
      ((PES->DSM_trick_mode_flag)?1:0),
      ((PES->additional_copy_info_flag)?1:0),
      ((PES->PES_CRC_flag)?1:0),
      ((PES->PES_extension_flag)?1:0),
      PES->PES_header_data_length);
    if (PES->PTS_flag) {
      printf("\
PTS:                         %lld\n",
        PES->PTS);
      if (PES->DTS_flag) {
        printf("\
DTS:                         %lld\n",
          PES->DTS);
      }
    }
    if (PES->ESCR_flag) {
      printf("\
ESCR_base:                   %lld\n\
ESCR_extension:              %d\n",
        PES->ESCR_base,
        PES->ESCR_extension);
    }
    if (PES->ES_rate_flag) {
      printf("\
ES_rate:                     %i (%i bytes/sec)\n",
        PES->ES_rate,PES->ES_rate*50);
    }
    if (PES->DSM_trick_mode_flag) {
      printf("\
trick_mode_control:          0x%02X\n",
        PES->trick_mode_control);
    }
    if (PES->additional_copy_info_flag) {
      printf("\
additional_copy_info:        0x%02X\n",
        PES->additional_copy_info);
    }
    if (PES->PES_CRC_flag) {
      printf("\
previous_PES_packet_CRC:     0x%04X\n",
        PES->previous_PES_packet_CRC);
    }
    if (PES->PES_extension_flag) {
      printf("\
PES_private_data_flag        %i\n\
pack_header_field_flag       %i\n\
program_packet_sequence_counter_flag  %i\n\
P_STD_buffer_flag            %i\n\
PES_extension_flag_2         %i\n",
        ((PES->PES_private_data_flag)?1:0),
        ((PES->pack_header_field_flag)?1:0),
        ((PES->program_packet_sequence_counter_flag)?1:0),
        ((PES->P_STD_buffer_flag)?1:0),
        ((PES->PES_extension_flag_2)?1:0));
      if (PES->PES_private_data_flag) {
        for (i=0; i<128; i++) {
          printf("%02X ",PES->PES_private_data[i] & 0xFF);
          if ((i % 24)==23) printf("\n");
        }
        printf("\n");
      }
      if (PES->pack_header_field_flag) {
        printf("\
pack_field_length            %i\n",
          PES->pack_field_length);
      }
      if (PES->program_packet_sequence_counter_flag) {
        printf("\
program_packet_sequence_counter  %i\n\
MPEG1_MPEG2_identifier       %i\n\
original_stuff_length        %i\n",
          PES->program_packet_sequence_counter,
          ((PES->MPEG1_MPEG2_identifier)?1:0),
          PES->original_stuff_length);
      }
      if (PES->P_STD_buffer_flag) {
        printf("\
P_STD_buffer_scale           %i\n\
P_STD_buffer_size            %i\n",
          ((PES->P_STD_buffer_scale)?1:0),
          PES->P_STD_buffer_size);
      }
      if (PES->PES_extension_flag_2) {
        printf("\
PES_extension_field_length   %i\n",
          PES->PES_extension_field_length);
      }
    }
  }
  printf("\n");
}

int parse_PES(
  FILE* inputstream,            // mpeg-2 file
  int parse,                    // true: parse  false: skip packet
  int verbose,                  // true: debug output
  int pipe,                     // true: selected PES as binary  false: as Hexdump
  int useCRC) {                 // true: ignore CRC of PSI

  #define fetchchar if((filechar=fgetc(inputstream))<0)return

  char HD[256];
  
  int i,k,n,filechar;
  unsigned char ch,ch0,ch1,ch2;
  
  unsigned int packet_data_length;
  
  unsigned int compare;
  #define sequence_end_code 0x000001B7

  // scanning for 23 zero bits and a one-bit in up to 1003 bytes
  fetchchar 0x0101; ch0=filechar;
  fetchchar 0x0201; ch1=filechar;
  fetchchar 0x0301; ch2=filechar;
  k=1000; i=0;
  while (((ch0) || (ch1) || (ch2!=0x01)) && (k--)) {
    if (verbose) {
      printf("%02X ",ch0 & 0xFF);
      if ((i++ % 24) == 23) printf("\n");
    }
    ch0=ch1;
    ch1=ch2;
    fetchchar 0x0401; ch2=filechar;
  }
  
  if (k) {  // 0x000001 found
    if ((verbose) && (k<1000)) printf("\nSkipped %i byte looking for 0x000001\n",1000-k);
    
    fetchchar 0x0501; PES->stream_id=filechar & 0xFF;

    //if (verbose) printf("- PES Packet with %i bytes and stream_id 0x%02X\n",PES->PES_packet_length,PES->stream_id);

    // Padding bytes, ignore
    if (PES->stream_id==padding_stream_id) {
      fetchchar 0x0601; ch=filechar;
      fetchchar 0x0701; PES->PES_packet_length=((ch << 8) | (filechar & 0xFF)) & 0xFFFF;
      if (verbose) printf("- %i Padding bytes.\n",PES->PES_packet_length);
      for (i=0; i<PES->PES_packet_length; i++)
        fetchchar 0x0801;
    // Direct data, no further header
    } else if (
      (PES->stream_id==private_stream_2_id) || 
      (PES->stream_id==ECM_stream_id) || 
      (PES->stream_id==EMM_stream_id) || 
      (PES->stream_id==DSMCC_stream_id) || 
      (PES->stream_id==ITU_E_id) || 
      (PES->stream_id==program_stream_directory_id)) {
      fetchchar 0x0601; ch=filechar;
      fetchchar 0x0701; PES->PES_packet_length=((ch << 8) | (filechar & 0xFF)) & 0xFFFF;
      if (verbose) print_PES(PES);
      if (verbose || !pipe) {
        printf("- %i Packet data bytes.\n",PES->PES_packet_length);
        for (i=0; i<PES->PES_packet_length; i++) {
          fetchchar 0x0901;
          printf("%02X ",filechar & 0xFF);
          if ((i % 24)==23) printf("\n");
        }
        printf("\n");
      } else if (pipe) {
        for (i=0; i<PES->PES_packet_length; i++) {
          fetchchar 0x0A01;
          putchar(filechar & 0xFF);
        }
      }
      
    // PES-Header, parse for subheaders
    } else if (PES->stream_id>=0xBC) {
      fetchchar 0x0601; ch=filechar;
      fetchchar 0x0701; PES->PES_packet_length=((ch << 8) | (filechar & 0xFF)) & 0xFFFF;
      fetchchar 0x0B01;
      if ((filechar & 0xC0) != 0x80) {
        if (verbose) printf("- SYNTAX error (wrong marker bits)\n");
        if (useCRC) return 0x0104;
      }
      PES->PES_scrambling_control=((filechar >> 4) & 0x03);
      PES->PES_priority=filechar & 0x08;
      PES->data_alignment_indicator=filechar & 0x04;
      PES->copyright=filechar & 0x02;
      PES->original_or_copy=filechar & 0x01;
      fetchchar 0x0C01;
      PES->PTS_flag=filechar & 0x80;
      PES->DTS_flag=filechar & 0x40;
      PES->ESCR_flag=filechar & 0x20;
      PES->ES_rate_flag=filechar & 0x10;
      PES->DSM_trick_mode_flag=filechar & 0x08;
      PES->additional_copy_info_flag=filechar & 0x04;
      PES->PES_CRC_flag=filechar & 0x02;
      PES->PES_extension_flag=filechar & 0x01;
      fetchchar 0x0D01;
      PES->PES_header_data_length=filechar & 0xFF;
      
      // Read remaining header bytes
      if (fread(&HD[0],1,PES->PES_header_data_length,inputstream)!=PES->PES_header_data_length) return 0x0E01;
      
      // parse subheaders
      n=0;
      if (PES->PTS_flag) {
        if ((HD[n] & 0xF0) != ((PES->DTS_flag)?0x30:0x20)) {
          if (verbose) printf("- SYNTAX error (wrong marker bits for PTS)\n");
          if (useCRC) return 0x0204;
        }
        PES->PTS=(HD[n++] >> 1) & 0x07ULL;  // Bit 32-30
        PES->PTS=(PES->PTS << 8) | (HD[n++] & 0xFFULL);  // Bit 29-22
        PES->PTS=(PES->PTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 21-15
        PES->PTS=(PES->PTS << 8) | (HD[n++] & 0xFFULL);  // Bit 14-7
        PES->PTS=(PES->PTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 6-0
        if (PES->DTS_flag) {
          if ((HD[n] & 0xF0) != 0x10) {
            if (verbose) printf("- SYNTAX error (wrong marker bits for DTS)\n");
            if (useCRC) return 0x0304;
          }
          PES->DTS=(HD[n++] >> 1) & 0x07ULL;  // Bit 32-30
          PES->DTS=(PES->DTS << 8) | (HD[n++] & 0xFFULL);  // Bit 29-22
          PES->DTS=(PES->DTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 21-15
          PES->DTS=(PES->DTS << 8) | (HD[n++] & 0xFFULL);  // Bit 14-7
          PES->DTS=(PES->DTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 6-0
        }
      }
      if (PES->ESCR_flag) {
        PES->ESCR_base=(HD[n] >> 3) & 0x07ULL;  // Bit 32-30
        PES->ESCR_base=(PES->ESCR_base << 2) | (HD[n++] & 0x03ULL);  // Bit 29-28
        PES->ESCR_base=(PES->ESCR_base << 8) | (HD[n++] & 0xFFULL);  // Bit 27-20
        PES->ESCR_base=(PES->ESCR_base << 5) | ((HD[n] >> 3) & 0x1FULL);  // Bit 19-15
        PES->ESCR_base=(PES->ESCR_base << 2) | (HD[n++] & 0x03ULL);  // Bit 14-13
        PES->ESCR_base=(PES->ESCR_base << 8) | (HD[n++] & 0xFFULL);  // Bit 12-5
        PES->ESCR_base=(PES->ESCR_base << 5) | ((HD[n] >> 3) & 0x7FULL);  // Bit 4-0
        PES->ESCR_extension=HD[n++] & 0x03; // Bit 8-7
        PES->ESCR_extension=(PES->ESCR_extension << 7) | ((HD[n++] >> 1) & 0x7F);  // Bit 6-0
      }
      if (PES->ES_rate_flag) {
        PES->ES_rate=HD[n++] & 0x7F;  // Bit 21-15
        PES->ES_rate=(PES->ES_rate << 7) | (HD[n++] & 0xFF);  // Bit 14-7
        PES->ES_rate=(PES->ES_rate << 8) | ((HD[n++] >> 1) & 0x7F);  // Bit 6-0
      }
      if (PES->DSM_trick_mode_flag) {
        PES->trick_mode_control=HD[n++] & 0xFF;
      }
      if (PES->additional_copy_info_flag) {
        PES->additional_copy_info=HD[n++] & 0x7F;
      }
      if (PES->PES_CRC_flag) {
        PES->previous_PES_packet_CRC=HD[n++] & 0xFF;
        PES->previous_PES_packet_CRC=(PES->previous_PES_packet_CRC << 8) | (HD[n++] & 0xFF);
        if (PES->previous_PES_packet_CRC!=CRC) {
          if (verbose) printf("- CRC error in previous block 0x%04X / 0x%04X\n",PES->previous_PES_packet_CRC,CRC);
          //if (useCRC) return 0x0304;
        }
      }
      if (PES->PES_extension_flag) {
        PES->PES_private_data_flag=HD[n] & 0x80;
        PES->pack_header_field_flag=HD[n] & 0x40;
        PES->program_packet_sequence_counter_flag=HD[n] & 0x20;
        PES->P_STD_buffer_flag=HD[n] & 0x10;
        PES->PES_extension_flag_2=HD[n] & 0x01;
        if (PES->PES_private_data_flag) {
          for (i=0; i<128; i++) {
            PES->PES_private_data[i]=HD[n++] & 0xFF;
          }
        }
        if (PES->pack_header_field_flag) {
          PES->pack_field_length=HD[n++] & 0xFF;
          n+=PES->pack_field_length;
          // *** TO DO *** parse pack header data
        }
        if (PES->program_packet_sequence_counter_flag) {
          PES->program_packet_sequence_counter=HD[n++] & 0x7F;
          PES->MPEG1_MPEG2_identifier=HD[n] & 0x40;
          PES->original_stuff_length=HD[n++] & 0x3F;
        }
        if (PES->P_STD_buffer_flag) {
          if ((HD[n] & 0xC0) != 0x40) {
            if (verbose) printf("- SYNTAX error (wrong marker bits for P-STD)\n");
            if (useCRC) return 0x0404;
          }
          PES->P_STD_buffer_scale=HD[n] & 0x20;
          PES->P_STD_buffer_size=HD[n++] & 0x1F;
          PES->P_STD_buffer_size=(PES->P_STD_buffer_size << 8) | (HD[n++] & 0xFF);
        }
        if (PES->PES_extension_flag_2) {
          PES->PES_extension_field_length=HD[n++] & 0x7F;
          n+=PES->PES_extension_field_length;
        }
      }

      // debug output
      if (verbose) print_PES(PES);
      
      //  initialise CRC for next block
      CRC=CRC_INIT_16;
      
      if (PES->PES_packet_length) {
        // how long is the size of the remaining PES-data? (now ES-data, that is)
        packet_data_length=PES->PES_packet_length - (3 + PES->PES_header_data_length);
        compare=0xFFFFFFFF;
  
        // dump or pipe raw ES-data
        if (verbose || !pipe) {
          printf("- %i Packet data bytes.\n%5d - ",packet_data_length,0);
          for (i=0; i<packet_data_length; i++) {
            fetchchar 0x0F01;
            CRC=update_crc_16(CRC,filechar & 0xFF);
            compare=(compare << 8) | (filechar & 0xFF);
            // ANSI colors, 0=black, 1=red, 2=green, 3=yellow, 4=blue, 5=magenta; 6=cyan, 7=white ESC[3<fgcol>;4<bgcol>;1<for bold>m
            //if ((compare & 0xFFFFFF00)==0x100) printf("\033[32m%02X\033[37m ",filechar & 0xFF);
            //else printf("%02X ",filechar & 0xFF);
            printf("%02X%c",filechar & 0xFF,(((compare & 0x00FFFFFF)==0x00000001)?'[':(((compare & 0xFFFFFF00)==0x00000100)?']':' ')));
            //printf("%02X ",filechar & 0xFF);
            if ((i % 24)==23) printf("\n%5d - ",i+1);
          }
          printf("\n");
        } else if (pipe) {
          for (i=0; i<packet_data_length; i++) {
            fetchchar 0x1001;
            CRC=update_crc_16(CRC,filechar & 0xFF);
            putchar(filechar & 0xFF);
          }
        }
      } else if ((PES->stream_id & 0xF0) == 0xE0) {  // endless video stream
        compare=0xFFFFFFFF;
        // dump or pipe raw ES-data
        if (verbose || !pipe) {
          printf("- Unbound video stream.\n");
          for (i=0; compare!=sequence_end_code; i++) {
            fetchchar 0x1101;
            CRC=update_crc_16(CRC,filechar & 0xFF);
            compare=(compare << 8) | (filechar & 0xFF);
            printf("%02X%c",filechar & 0xFF,(((compare & 0x00FFFFFF)==0x00000001)?'<':(((compare & 0xFFFFFF00)==0x00000100)?'>':' ')));
            if ((i % 24)==23) printf("\n");
          }
          printf("\n");
        } else if (pipe) {
          while (compare!=sequence_end_code) {
            fetchchar 0x1201;
            CRC=update_crc_16(CRC,filechar & 0xFF);
            compare=(compare << 8) | (filechar & 0xFF);
            putchar(filechar & 0xFF);
          }
        }
      } else {
        if (verbose) printf("- ERROR: Unbound stream, but no video stream.\n");
      }
      if (verbose) printf("CRC: 0x%04X\n\n",CRC);
    } else {
      if (verbose) {
        printf("ERROR - This is not a PES-Packet!\n  Start code value: 0x%02X (",PES->stream_id);
        if (PES->stream_id==0x00)
          printf("picture_start_code");
        else if (PES->stream_id<=0xAF)
          printf("slice_start_code");
        else if ((PES->stream_id==0xB0) || (PES->stream_id==0xB1) || (PES->stream_id==0xB6))
          printf("reserved");
        else if (PES->stream_id==0xB2)
          printf("user_data_start_code");
        else if (PES->stream_id==0xB3)
          printf("sequence_header_code");
        else if (PES->stream_id==0xB4)
          printf("sequence_error_code");
        else if (PES->stream_id==0xB5)
          printf("extension_start_code");
        else if (PES->stream_id==0xB7)
          printf("sequence_end_code");
        else if (PES->stream_id==0xB8)
          printf("group_start_code");
        else if (PES->stream_id==0xB9)
          printf("iso_11172_end_code");
        else if (PES->stream_id==0xBA)
          printf("pack_start_code");
        else if (PES->stream_id==0xBB)
          printf("system_header_start_code");
        else 
          printf("unidentified start code");
        printf(")\n");
      }
    }
  }
  return 0;
}



int main (int argc, char* argv[]) {

  FILE* inputstream;
  int i,filearg,startpacket,stoppacket,err,verbose,pipe,useCRC;
  
  if (argc < 2) {
    printf("\
Usage: %s [-v] [-p] [-c] [<file> [<first packet> <last packet> | <packet>]]\n\
-v: verbose on\n\
-p: pipe on (only raw output)\n\
-c: use CRC on PSI\n\
-s <prog>/<type>: show/pipe only PES of this program and this type\n\
",argv[0]);
    exit (0);
  }
  
  i=1;
  filearg=0;
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
  
  gen_crc16_table();

  if (startpacket) {
    if (!stoppacket) stoppacket=startpacket;
  } else stoppacket=32767;  // argh, i have to change this...
  
  PES=(PES_header *)malloc(sizeof(PES_header));
  
  err=0;
  for (i=0; (i<stoppacket) && (err==0); i++) {
    if ((startpacket<=(i+1)) && verbose) printf("--- Packet %i --- %ld\n",i+1,ftell(inputstream));
    err=parse_PES(inputstream,(startpacket<=(i+1)),verbose,pipe,useCRC);
  }
  if (err && verbose) printf("\nError in packet %i at %ld: 0x%04X (%s)\n",i,ftell(inputstream),err,(((err & 0xFF)==0x01)?"End of File":(((err & 0xFF)==0x02)?"Data Format Error":"Unknown Error")));
  fclose(inputstream);

  return 0;
}
