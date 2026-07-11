/* Private contracts between the Stream dispatcher and format readers. */

#ifndef LIBREDWG_STREAM_READER_INTERNAL_H
#define LIBREDWG_STREAM_READER_INTERNAL_H

#include "decode.h"

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
