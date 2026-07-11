/* Private object-record helpers shared by Stream format readers. */

#ifndef LIBREDWG_STREAM_OBJECT_HELPERS_H
#define LIBREDWG_STREAM_OBJECT_HELPERS_H

#include "decode.h"

int dwg_stream_fixed_type_is_entity (Dwg_Object_Type type);
int dwg_stream_object_span_size (const Dwg_Stream_Object_Info *info,
                                 size_t object_start, size_t total_size,
                                 size_t *object_size);
int dwg_stream_add_handle_offset (size_t *last_offset, BITCODE_MC offset);
int dwg_stream_add_handle_value (BITCODE_RLL *last_handle,
                                 BITCODE_UMC handleoff,
                                 BITCODE_RLL max_handles);
int dwg_stream_object_record_size (const Dwg_Stream_Object_Info *info,
                                   size_t object_start, size_t total_size,
                                   size_t *object_size);

#endif
