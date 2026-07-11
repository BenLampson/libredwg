/* Private contracts between the Stream dispatcher and format readers. */

#ifndef LIBREDWG_STREAM_READER_INTERNAL_H
#define LIBREDWG_STREAM_READER_INTERNAL_H

#include "decode.h"

#define DWG_R13_MAX_HEADER_SIZE 2048

#pragma pack(push)
#pragma pack(1)
typedef union _encrypted_section_header
{
  uint32_t long_data[8];
  struct
  {
    uint32_t page_type;
    uint32_t section_type;
    uint32_t data_size;
    uint32_t page_size;
    uint32_t address;
    uint32_t unknown;
    uint32_t page_header_crc;
    uint32_t data_crc;
  } fields;
} encrypted_section_header;
#pragma pack(pop)

Dwg_Section *find_section (Dwg_Data *dwg, BITCODE_RLd index);
int decode_R2004_header (Bit_Chain *file_dat, Dwg_Data *dwg);
int read_R2004_section_map (Bit_Chain *dat, Dwg_Data *dwg);
int read_R2004_section_info (Bit_Chain *dat, Dwg_Data *dwg,
                             uint32_t comp_data_size,
                             uint32_t decomp_data_size);
int read_2004_compressed_section (Bit_Chain *dat, Dwg_Data *dwg,
                                  Bit_Chain *section_dat,
                                  Dwg_Section_Type type);
int read_2004_section_header (Bit_Chain *dat, Dwg_Data *dwg);
int read_2004_section_classes (Bit_Chain *dat, Dwg_Data *dwg);
unsigned int read_literal_length (Bit_Chain *dat, unsigned char opcode);
int read_compressed_bytes (Bit_Chain *dat, unsigned char opcode,
                           unsigned int bits);
BITCODE_RC two_byte_offset (Bit_Chain *dat, int plus, int *offset);

void dwg_refine_from_version (Bit_Chain *dat, Dwg_Data *dwg);

int dwg_stream_read_r13_to_r2002 (
    Bit_Chain *dat, Dwg_Data *dwg,
    const Dwg_Stream_Callbacks_Ex *callbacks,
    Dwg_Stream_Input_Mode input_mode, void *user);
int dwg_stream_read_r2004_to_r2006_and_r2010_to_r2022 (
    Bit_Chain *dat, Dwg_Data *dwg,
    const Dwg_Stream_Callbacks_Ex *callbacks,
    Dwg_Stream_Input_Mode input_mode, void *user);
int dwg_stream_read_r1_to_r2 (
    Bit_Chain *dat, Dwg_Data *dwg,
    const Dwg_Stream_Callbacks_Ex *callbacks,
    Dwg_Stream_Input_Mode input_mode, void *user);
int dwg_stream_read_r2_to_r10 (
    Bit_Chain *dat, Dwg_Data *dwg,
    const Dwg_Stream_Callbacks_Ex *callbacks,
    Dwg_Stream_Input_Mode input_mode, void *user);
int dwg_stream_read_r11 (
    Bit_Chain *dat, Dwg_Data *dwg,
    const Dwg_Stream_Callbacks_Ex *callbacks,
    Dwg_Stream_Input_Mode input_mode, void *user);

#endif
