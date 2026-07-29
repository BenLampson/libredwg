/* Stream reader for R2004-R2006 and R2010-R2022. */

#define _DEFAULT_SOURCE 1
#include "config.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IS_DECODER
#include "common.h"
#include "bits.h"
#include "dwg.h"
#include "decode.h"
#include "stream_object_helpers.h"
#include "stream_reader_internal.h"

static int cur_ver = 0;
static BITCODE_BL rcount1 = 0, rcount2 = 0;
static bool is_teigha = false;

#define DWG_LOGLEVEL loglevel
#include "logging.h"
#include "dec_macros.h"
#define FIELD_VALUE(name) _obj->name

static Dwg_Section_Info *
find_2004_section_info (Dwg_Data *restrict dwg, Dwg_Section_Type type)
{
  BITCODE_BL i;

  for (i = 0; i < dwg->header.section_infohdr.num_desc; ++i)
    if (dwg->header.section_info[i].fixedtype == type)
      return &dwg->header.section_info[i];
  return NULL;
}

#define R2004_STREAM_HISTORY_SIZE 65536U
#define R2004_STREAM_OBJECT_PREFIX_SIZE 32U

typedef struct _r2004_object_stream
{
  Bit_Chain *dat;
  Dwg_Section_Info *info;
  BITCODE_RC history[R2004_STREAM_HISTORY_SIZE];
  BITCODE_BL next_section;
  size_t output_pos;
  int initialized;
} R2004_Object_Stream;

typedef struct _r2004_stream_handle_entry
{
  size_t address;
  BITCODE_RLL handle;
} R2004_Stream_Handle_Entry;

typedef struct _r2004_stream_acds
{
  Bit_Chain dat;
  size_t next_offset;
  const BITCODE_RC *current_data;
  size_t current_size;
} R2004_Stream_AcDs;

typedef struct _r2004_stream_decoded_context
{
  const Dwg_Stream_Callbacks_Ex *callbacks;
  void *user;
  R2004_Stream_AcDs *acds;
} R2004_Stream_Decoded_Context;

static int
r2004_stream_acds_find_next (R2004_Stream_AcDs *restrict acds)
{
  const char start[] = "ACIS BinaryFile";
  const char end[] = "\016\003End\016\002of\016\003ASM\r\004data";
  const char *s;
  const char *e;

  acds->current_data = NULL;
  acds->current_size = 0;
  while (acds->dat.chain && acds->next_offset < acds->dat.size)
    {
      size_t offset;
      size_t size;

      s = (const char *)memmem (&acds->dat.chain[acds->next_offset],
                                acds->dat.size - acds->next_offset, start,
                                strlen (start));
      if (!s)
        return 0;
      offset = (size_t)(s - (const char *)acds->dat.chain);
      e = (const char *)memmem (s, acds->dat.size - offset, end,
                                strlen (end));
      if (!e)
        {
          acds->next_offset = offset + 20;
          continue;
        }

      size = (size_t)(e - s) + strlen (end);
      acds->current_data = &acds->dat.chain[offset];
      acds->current_size = size;
      acds->next_offset = offset + size;
      return 1;
    }
  return 0;
}

static int
r2004_stream_acds_init (Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
                        R2004_Stream_AcDs *restrict acds)
{
  int error;

  memset (acds, 0, sizeof (*acds));
  if (dwg->header.from_version < R_2013)
    return 0;

  acds->dat.opts = dwg->opts & DWG_OPTS_LOGLEVEL;
  error = read_2004_compressed_section (dat, dwg, &acds->dat, SECTION_ACDS);
  if (error >= DWG_ERR_CRITICAL || !acds->dat.chain)
    {
      if (acds->dat.chain)
        free (acds->dat.chain);
      memset (acds, 0, sizeof (*acds));
      return 0;
    }

  r2004_stream_acds_find_next (acds);
  return 0;
}

static void
r2004_stream_acds_free (R2004_Stream_AcDs *restrict acds)
{
  if (acds->dat.chain)
    free (acds->dat.chain);
  memset (acds, 0, sizeof (*acds));
}

static int
r2004_stream_attach_acds (R2004_Stream_AcDs *restrict acds,
                          Dwg_Object *restrict obj)
{
  Dwg_Entity_3DSOLID *solid;
  BITCODE_RC *acis_data;

  if (!acds->current_data || !acds->current_size
      || obj->supertype != DWG_SUPERTYPE_ENTITY || !obj->tio.entity
      || !obj->tio.entity->has_ds_data || !dwg_obj_is_3dsolid (obj))
    return 0;
  if (acds->current_size > UINT32_MAX)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  /* The blocking decoder queues has_ds_data objects in decode order and
     consumes the SAB records in section order.  Preserve that pairing, but
     attach the record before the Stream callback observes the object. */
  acis_data = (BITCODE_RC *)malloc (acds->current_size);
  if (!acis_data)
    return DWG_ERR_OUTOFMEM;
  memcpy (acis_data, acds->current_data, acds->current_size);

  solid = obj->tio.entity->tio._3DSOLID;
  solid->acis_data = acis_data;
  solid->sab_size = (BITCODE_BL)acds->current_size;
  solid->version = 2;
  solid->acis_empty = 0;
  r2004_stream_acds_find_next (acds);
  return 0;
}

static int
r2004_stream_decoded_object (const Dwg_Stream_Object_Info *restrict info,
                             const Dwg_Object *restrict obj,
                             void *restrict user)
{
  R2004_Stream_Decoded_Context *context
      = (R2004_Stream_Decoded_Context *)user;
  int error;

  error = r2004_stream_attach_acds (context->acds, (Dwg_Object *)obj);
  if (error)
    return error;
  return context->callbacks->decoded_object (info, obj, context->user);
}

static int
r2004_stream_decode_error (const Dwg_Stream_Object_Info *restrict info,
                           int error, void *restrict user)
{
  R2004_Stream_Decoded_Context *context
      = (R2004_Stream_Decoded_Context *)user;

  return context->callbacks->decode_error (info, error, context->user);
}

static int
r2004_stream_emit_decoded_object (
    Dwg_Data *restrict dwg, Bit_Chain *restrict object_dat,
    const Dwg_Stream_Object_Info *restrict info,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks, void *restrict user,
    R2004_Stream_AcDs *restrict acds, int *restrict callback_error)
{
  Dwg_Stream_Callbacks_Ex wrapped_callbacks = *callbacks;
  R2004_Stream_Decoded_Context context;

  context.callbacks = callbacks;
  context.user = user;
  context.acds = acds;
  wrapped_callbacks.object = NULL;
  wrapped_callbacks.decoded_object = r2004_stream_decoded_object;
  if (callbacks->decode_error)
    wrapped_callbacks.decode_error = r2004_stream_decode_error;
  return dwg_stream_emit_decoded_object_ex (
      dwg, object_dat, info, &wrapped_callbacks, &context, callback_error);
}

static int
compare_r2004_stream_handle_entries (const void *a, const void *b)
{
  const R2004_Stream_Handle_Entry *left = (const R2004_Stream_Handle_Entry *)a;
  const R2004_Stream_Handle_Entry *right
      = (const R2004_Stream_Handle_Entry *)b;

  if (left->address < right->address)
    return -1;
  if (left->address > right->address)
    return 1;
  if (left->handle < right->handle)
    return -1;
  if (left->handle > right->handle)
    return 1;
  return 0;
}

static int
r2004_object_stream_init (R2004_Object_Stream *restrict stream,
                          Bit_Chain *restrict dat,
                          Dwg_Section_Info *restrict info)
{
  memset (stream, 0, sizeof (*stream));
  if (!dat || !info || !info->sections || !info->num_sections
      || !info->max_decomp_size || info->size < 0)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  stream->dat = dat;
  stream->info = info;
  stream->initialized = 1;
  return 0;
}

static int
r2004_stream_get_history (R2004_Object_Stream *restrict stream,
                          const size_t position, BITCODE_RC *restrict byte)
{
  if (position >= stream->output_pos
      || stream->output_pos - position > R2004_STREAM_HISTORY_SIZE)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  *byte = stream->history[position % R2004_STREAM_HISTORY_SIZE];
  return 0;
}

static int
r2004_stream_put_byte (R2004_Object_Stream *restrict stream,
                       const BITCODE_RC byte, const size_t target,
                       BITCODE_RC *restrict buffer, const size_t buffer_size)
{
  if ((int64_t)stream->output_pos >= stream->info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  stream->history[stream->output_pos % R2004_STREAM_HISTORY_SIZE] = byte;
  if (stream->output_pos >= target
      && stream->output_pos < target + buffer_size)
    buffer[stream->output_pos - target] = byte;
  stream->output_pos++;
  return 0;
}

static int
r2004_stream_copy_literal (R2004_Object_Stream *restrict stream,
                           Bit_Chain *restrict src,
                           const unsigned int lit_length, const size_t target,
                           BITCODE_RC *restrict buffer,
                           const size_t buffer_size,
                           unsigned char *restrict opcode)
{
  for (unsigned int i = 0; i < lit_length; ++i)
    {
      int error;
      if (src->byte >= src->size)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      error = r2004_stream_put_byte (stream, bit_read_RC (src), target, buffer,
                                     buffer_size);
      if (error)
        return error;
    }
  if (src->byte >= src->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  *opcode = bit_read_RC (src);
  return 0;
}

static int
r2004_stream_decompress_page (R2004_Object_Stream *restrict stream,
                              Bit_Chain *restrict src, const size_t target,
                              BITCODE_RC *restrict buffer,
                              const size_t buffer_size)
{
  unsigned int lit_length;
  int comp_offset, comp_bytes;
  unsigned char opcode1 = 0, opcode2;

  if (src->byte > src->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  opcode1 = bit_read_RC (src);
  if ((opcode1 & 0xF0) == 0)
    {
      int error = r2004_stream_copy_literal (
          stream, src, read_literal_length (src, opcode1), target, buffer,
          buffer_size, &opcode1);
      if (error)
        return error;
    }

  while (src->byte < src->size && opcode1 != 0x11)
    {
      size_t pos, end;
      int error;

      comp_bytes = 0;
      comp_offset = 0;
      if (opcode1 < 0x10 || opcode1 >= 0x40)
        {
          comp_bytes = (opcode1 >> 4) - 1;
          opcode2 = bit_read_RC (src);
          comp_offset = (((opcode1 >> 2) & 3) | (opcode2 << 2)) + 1;
        }
      else if (opcode1 < 0x20)
        {
          comp_bytes = read_compressed_bytes (src, opcode1, 7);
          comp_offset = (opcode1 & 8) << 11;
          opcode1 = two_byte_offset (src, 0x4000, &comp_offset);
        }
      else if (opcode1 >= 0x20)
        {
          comp_bytes = read_compressed_bytes (src, opcode1, 0x1f);
          opcode1 = two_byte_offset (src, 1, &comp_offset);
        }
      else if (opcode1 == 0x11)
        break;
      else
        return DWG_ERR_INTERNALERROR;

      pos = stream->output_pos;
      end = pos + comp_bytes;
      if (end < pos || (int64_t)end > stream->info->size
          || (long)pos < comp_offset
          || (size_t)comp_offset > R2004_STREAM_HISTORY_SIZE)
        return DWG_ERR_VALUEOUTOFBOUNDS;

      for (; pos < end; pos++)
        {
          BITCODE_RC byte;
          error = r2004_stream_get_history (stream, pos - comp_offset, &byte);
          if (error)
            return error;
          error = r2004_stream_put_byte (stream, byte, target, buffer,
                                         buffer_size);
          if (error)
            return error;
        }

      lit_length = opcode1 & 3;
      if (lit_length == 0)
        {
          if (src->byte >= src->size)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          opcode1 = bit_read_RC (src);
          if ((opcode1 & 0xf0) == 0)
            lit_length = read_literal_length (src, opcode1);
        }
      if (lit_length)
        {
          error = r2004_stream_copy_literal (stream, src, lit_length, target,
                                             buffer, buffer_size, &opcode1);
          if (error)
            return error;
        }
    }
  return 0;
}

static int
r2004_object_stream_read (R2004_Object_Stream *restrict stream,
                          const size_t target, BITCODE_RC *restrict buffer,
                          const size_t buffer_size)
{
  size_t end;

  if (!stream->initialized || !buffer || !buffer_size
      || target + buffer_size < target
      || (int64_t)(target + buffer_size) > stream->info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  end = target + buffer_size;

  if (end <= stream->output_pos)
    {
      for (size_t i = 0; i < buffer_size; i++)
        {
          int error
              = r2004_stream_get_history (stream, target + i, &buffer[i]);
          if (error)
            return error;
        }
      return 0;
    }

  memset (buffer, 0, buffer_size);
  if (target < stream->output_pos)
    {
      size_t available_end
          = stream->output_pos < end ? stream->output_pos : end;
      for (size_t pos = target; pos < available_end; pos++)
        {
          int error;
          error
              = r2004_stream_get_history (stream, pos, &buffer[pos - target]);
          if (error)
            return error;
        }
    }

  while (stream->output_pos < end)
    {
      Dwg_Section *section;
      encrypted_section_header es;
      Bit_Chain src;
      uint32_t address, sec_mask;
      int error = 0;

      if (stream->next_section >= stream->info->num_sections)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      section = stream->info->sections[stream->next_section++];
      if (!section)
        continue;

      address = section->address;
      if (address + 32 > stream->dat->size)
        return DWG_ERR_VALUEOUTOFBOUNDS;

      memcpy (es.long_data, &stream->dat->chain[address], 32);
      sec_mask = htole32 (0x4164536b ^ address);
      for (int k = 0; k < 8; ++k)
        es.long_data[k] = le32toh (es.long_data[k] ^ sec_mask);

      if (es.fields.page_type != 0x4163043b
          || es.fields.address > (uint32_t)stream->info->size
          || address + 32 + es.fields.data_size > stream->dat->size)
        return DWG_ERR_VALUEOUTOFBOUNDS;

      while (stream->output_pos < es.fields.address)
        {
          error
              = r2004_stream_put_byte (stream, 0, target, buffer, buffer_size);
          if (error)
            return error;
        }
      if (stream->output_pos != es.fields.address)
        return DWG_ERR_VALUEOUTOFBOUNDS;

      src = *stream->dat;
      src.byte = address + 32;
      src.bit = 0;
      src.size = src.byte + es.fields.data_size;

      if (stream->info->compressed == 2)
        error = r2004_stream_decompress_page (stream, &src, target, buffer,
                                              buffer_size);
      else
        {
          uint32_t size
              = MIN ((BITCODE_RL)(stream->info->size - es.fields.address),
                     es.fields.page_size);
          if (address + 32 + size > stream->dat->size)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          for (uint32_t i = 0; i < size; i++)
            {
              error = r2004_stream_put_byte (
                  stream, stream->dat->chain[address + 32 + i], target, buffer,
                  buffer_size);
              if (error)
                return error;
            }
        }
      if (error)
        return error;
    }
  return 0;
}

static int
read_2004_stream_object_info (Dwg_Data *restrict dwg,
                              R2004_Object_Stream *restrict obj_stream,
                              const Dwg_Stream_Input_Mode input_mode,
                              const size_t address,
                              const BITCODE_RLL handle_value,
                              Dwg_Stream_Object_Info *restrict info)
{
  BITCODE_RC prefix[R2004_STREAM_OBJECT_PREFIX_SIZE];
  Bit_Chain body;
  Dwg_Object_Type fixedtype;
  int error;

  memset (info, 0, sizeof (*info));
  if (address >= (uint64_t)obj_stream->info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  error = r2004_object_stream_read (obj_stream, address, prefix,
                                    sizeof (prefix));
  if (error)
    return error;

  memset (&body, 0, sizeof (body));
  body.chain = prefix;
  body.size = sizeof (prefix);
  body.version = obj_stream->dat->version;
  body.from_version = obj_stream->dat->from_version;
  body.bit = 0;
  info->size = bit_read_MS (&body);
  if (body.from_version >= R_2010b)
    (void)bit_read_UMC (&body);
  info->address = address + body.byte;
  if (info->size > (uint64_t)obj_stream->info->size
      || info->address > (uint64_t)obj_stream->info->size - info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  bit_reset_chain (&body);
  if (body.from_version >= R_2010b)
    info->type = bit_read_BOT (&body);
  else
    info->type = bit_read_BS (&body);
  fixedtype = (Dwg_Object_Type)info->type;
  if (info->type >= 500 && (BITCODE_BL)(info->type - 500) < dwg->num_classes)
    {
      const Dwg_Class *klass = &dwg->dwg_class[info->type - 500];
      info->supertype = dwg_class_is_entity (klass) ? DWG_SUPERTYPE_ENTITY
                                                    : DWG_SUPERTYPE_OBJECT;
      info->name = klass->dxfname;
      info->dxfname = klass->dxfname;
      fixedtype = (Dwg_Object_Type)info->type;
    }
  else
    {
      info->supertype = dwg_stream_fixed_type_is_entity (fixedtype)
                            ? DWG_SUPERTYPE_ENTITY
                            : DWG_SUPERTYPE_OBJECT;
    }
  info->fixedtype = fixedtype;
  info->handle.value = handle_value;
  info->version = dwg->header.from_version;
  info->decode_mode = DWG_STREAM_DECODE_R2004_OBJECT_MAP;
  info->input_mode = input_mode;
  return 0;
}

static int
read_2004_buffered_object_info (Dwg_Data *restrict dwg,
                                Bit_Chain *restrict obj_dat,
                                const Dwg_Stream_Input_Mode input_mode,
                                const size_t address,
                                const BITCODE_RLL handle_value,
                                Dwg_Stream_Object_Info *restrict info)
{
  Bit_Chain body;
  Dwg_Object_Type fixedtype;

  memset (info, 0, sizeof (*info));
  if (address >= obj_dat->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  body = *obj_dat;
  body.byte = address;
  body.bit = 0;
  info->size = bit_read_MS (&body);
  if (body.from_version >= R_2010b)
    (void)bit_read_UMC (&body);
  info->address = body.byte;
  if (info->size > obj_dat->size || info->address > obj_dat->size - info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  bit_reset_chain (&body);
  body.size = info->size;
  if (body.from_version >= R_2010b)
    info->type = bit_read_BOT (&body);
  else
    info->type = bit_read_BS (&body);
  fixedtype = (Dwg_Object_Type)info->type;
  if (info->type >= 500 && (BITCODE_BL)(info->type - 500) < dwg->num_classes)
    {
      const Dwg_Class *klass = &dwg->dwg_class[info->type - 500];
      info->supertype = dwg_class_is_entity (klass) ? DWG_SUPERTYPE_ENTITY
                                                    : DWG_SUPERTYPE_OBJECT;
      info->name = klass->dxfname;
      info->dxfname = klass->dxfname;
      fixedtype = (Dwg_Object_Type)info->type;
    }
  else
    {
      info->supertype = dwg_stream_fixed_type_is_entity (fixedtype)
                            ? DWG_SUPERTYPE_ENTITY
                            : DWG_SUPERTYPE_OBJECT;
    }
  info->fixedtype = fixedtype;
  info->handle.value = handle_value;
  info->version = dwg->header.from_version;
  info->decode_mode = DWG_STREAM_DECODE_R2004_OBJECT_MAP;
  info->input_mode = input_mode;
  return 0;
}

static int
read_2004_section_handles_buffered_stream (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user,
    R2004_Stream_AcDs *restrict acds,
    int *restrict callback_error)
{
  Bit_Chain obj_dat = { 0 }, hdl_dat = { 0 };
  BITCODE_RS section_size = 0;
  BITCODE_RLL last_handle = 0;
  BITCODE_MC prevsize = 0;
  BITCODE_BL index = 0;
  size_t endpos;
  int error;

  obj_dat.opts = hdl_dat.opts = dwg->opts & DWG_OPTS_LOGLEVEL;
  error = read_2004_compressed_section (dat, dwg, &obj_dat, SECTION_OBJECTS);
  if (error >= DWG_ERR_CRITICAL || !obj_dat.chain)
    {
      LOG_ERROR ("Failed to read compressed %s section", "AcDbObjects");
      if (obj_dat.chain)
        free (obj_dat.chain);
      return error;
    }

  error = read_2004_compressed_section (dat, dwg, &hdl_dat, SECTION_HANDLES);
  if (error >= DWG_ERR_CRITICAL || !hdl_dat.chain)
    {
      LOG_ERROR ("Failed to read compressed %s section", "Handles");
      free (obj_dat.chain);
      if (hdl_dat.chain)
        free (hdl_dat.chain);
      return error;
    }

  endpos = hdl_dat.byte + hdl_dat.size;
  do
    {
      size_t last_offset = 0;
      size_t oldpos = 0;
      size_t startpos = hdl_dat.byte;
      size_t max_handles = hdl_dat.size * 2;
      uint16_t crc1, crc2;

      section_size = bit_read_RS_BE (&hdl_dat);
      if (section_size > 2040)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }

      while ((long)(hdl_dat.byte - startpos) < (long)section_size)
        {
          BITCODE_UMC handleoff;
          BITCODE_MC offset;
          Dwg_Stream_Object_Info info;
          int object_error;

          oldpos = hdl_dat.byte;
          handleoff = bit_read_UMC (&hdl_dat);
          offset = bit_read_MC (&hdl_dat);
          if (!handleoff || handleoff > max_handles - last_handle
              || (offset > -4 && offset < prevsize))
            LOG_WARN ("Ignore invalid handleoff (@%" PRIuSIZE ")", oldpos);
          object_error = dwg_stream_add_handle_value (
              &last_handle, handleoff, (BITCODE_RLL)max_handles);
          if (object_error)
            {
              error = object_error;
              goto done;
            }

          object_error = dwg_stream_add_handle_offset (&last_offset, offset);
          if (object_error)
            {
              error = object_error;
              goto done;
            }
          if (hdl_dat.byte == oldpos)
            break;

          object_error = read_2004_buffered_object_info (
              dwg, &obj_dat, input_mode, last_offset, last_handle, &info);
          if (object_error)
            {
              error |= object_error;
              goto done;
            }

          info.index = index++;
          prevsize = info.size + 4;
          if (callbacks->object)
            {
              int emitted_error = callbacks->object (&info, user);
              if (emitted_error)
                {
                  *callback_error = emitted_error;
                  error = emitted_error;
                  goto done;
                }
            }
          if (callbacks->decoded_object)
            {
              Bit_Chain object_dat = obj_dat;
              size_t object_size;
              int emitted_error;

              object_error = dwg_stream_object_record_size (
                  &info, last_offset, obj_dat.size, &object_size);
              if (object_error)
                {
                  error = object_error;
                  goto done;
                }
              object_dat.chain = &obj_dat.chain[last_offset];
              object_dat.byte = 0;
              object_dat.bit = 0;
              object_dat.size = object_size;
              emitted_error = r2004_stream_emit_decoded_object (
                  dwg, &object_dat, &info, callbacks, user, acds,
                  callback_error);
              if (emitted_error)
                {
                  error = emitted_error;
                  goto done;
                }
            }
        }

      if (hdl_dat.byte == oldpos)
        break;

      crc1 = bit_calc_CRC (0xC0C1, &(hdl_dat.chain[startpos]),
                           hdl_dat.byte - startpos);
      crc2 = bit_read_RS_BE (&hdl_dat);
      if (crc1 != crc2)
        {
          LOG_WARN ("Handles page CRC mismatch: %04X vs calc. %04X from "
                    "%" PRIuSIZE "-%" PRIuSIZE "=%ld\n",
                    crc2, crc1, startpos, hdl_dat.byte - 2,
                    (long)(hdl_dat.byte - startpos - 2));
          error |= DWG_ERR_WRONGCRC;
        }

      if (hdl_dat.byte >= endpos)
        break;
    }
  while (section_size > 2);

done:
  if (hdl_dat.chain)
    free (hdl_dat.chain);
  if (obj_dat.chain)
    free (obj_dat.chain);
  return error;
}

static int
read_2004_section_handles_stream (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user,
    R2004_Stream_AcDs *restrict acds,
    int *restrict callback_error)
{
  Bit_Chain hdl_dat = { 0 };
  Dwg_Section_Info *obj_info;
  R2004_Object_Stream obj_stream;
  R2004_Stream_Handle_Entry *entries = NULL;
  BITCODE_RS section_size = 0;
  BITCODE_RLL last_handle = 0;
  BITCODE_BL num_entries = 0;
  BITCODE_BL entries_capacity = 0;
  size_t endpos;
  int error;

  obj_info = find_2004_section_info (dwg, SECTION_OBJECTS);
  if (!obj_info || obj_info->num_sections == 0 || !obj_info->sections)
    {
      LOG_ERROR ("Failed to find streamable %s section", "AcDbObjects");
      return DWG_ERR_SECTIONNOTFOUND;
    }
  if (obj_info->max_decomp_size
      && obj_info->num_sections
             <= (BITCODE_RL)(0x2f000000U / obj_info->max_decomp_size))
    return read_2004_section_handles_buffered_stream (
        dat, dwg, callbacks, input_mode, user, acds, callback_error);

  error = r2004_object_stream_init (&obj_stream, dat, obj_info);
  if (error)
    return error;

  hdl_dat.opts = dwg->opts & DWG_OPTS_LOGLEVEL;
  error = read_2004_compressed_section (dat, dwg, &hdl_dat, SECTION_HANDLES);
  if (error >= DWG_ERR_CRITICAL || !hdl_dat.chain)
    {
      LOG_ERROR ("Failed to read compressed %s section", "Handles");
      if (hdl_dat.chain)
        free (hdl_dat.chain);
      return error;
    }

  endpos = hdl_dat.byte + hdl_dat.size;
  do
    {
      size_t last_offset = 0;
      size_t oldpos = 0;
      size_t startpos = hdl_dat.byte;
      size_t max_handles = hdl_dat.size * 2;
      uint16_t crc1, crc2;

      section_size = bit_read_RS_BE (&hdl_dat);
      if (section_size > 2040)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }

      while ((long)(hdl_dat.byte - startpos) < (long)section_size)
        {
          BITCODE_UMC handleoff;
          BITCODE_MC offset;
          int invalid_entry = 0;

          oldpos = hdl_dat.byte;
          handleoff = bit_read_UMC (&hdl_dat);
          offset = bit_read_MC (&hdl_dat);
          if (!handleoff || handleoff > max_handles - last_handle)
            {
              LOG_WARN ("Ignore invalid handleoff (@%" PRIuSIZE ")", oldpos);
              error |= DWG_ERR_VALUEOUTOFBOUNDS;
              invalid_entry = 1;
            }
          if (dwg_stream_add_handle_value (&last_handle, handleoff,
                                           (BITCODE_RLL)max_handles))
            {
              error = DWG_ERR_VALUEOUTOFBOUNDS;
              goto done;
            }
          if (dwg_stream_add_handle_offset (&last_offset, offset))
            {
              error = DWG_ERR_VALUEOUTOFBOUNDS;
              goto done;
            }
          if (hdl_dat.byte == oldpos)
            break;
          if (invalid_entry)
            continue;

          if (num_entries == entries_capacity)
            {
              R2004_Stream_Handle_Entry *new_entries;
              BITCODE_BL new_capacity
                  = entries_capacity ? entries_capacity * 2 : 4096;
              if (new_capacity < entries_capacity)
                {
                  error = DWG_ERR_OUTOFMEM;
                  goto done;
                }
              new_entries = (R2004_Stream_Handle_Entry *)realloc (
                  entries, (size_t)new_capacity * sizeof (*entries));
              if (!new_entries)
                {
                  error = DWG_ERR_OUTOFMEM;
                  goto done;
                }
              entries = new_entries;
              entries_capacity = new_capacity;
            }

          entries[num_entries].address = last_offset;
          entries[num_entries].handle = last_handle;
          num_entries++;
        }

      if (hdl_dat.byte == oldpos)
        break;

      crc1 = bit_calc_CRC (0xC0C1, &(hdl_dat.chain[startpos]),
                           hdl_dat.byte - startpos);
      crc2 = bit_read_RS_BE (&hdl_dat);
      if (crc1 != crc2)
        {
          LOG_WARN ("Handles page CRC mismatch: %04X vs calc. %04X from "
                    "%" PRIuSIZE "-%" PRIuSIZE "=%ld\n",
                    crc2, crc1, startpos, hdl_dat.byte - 2,
                    (long)(hdl_dat.byte - startpos - 2));
          error |= DWG_ERR_WRONGCRC;
        }

      if (hdl_dat.byte >= endpos)
        break;
    }
  while (section_size > 2);

  qsort (entries, num_entries, sizeof (*entries),
         compare_r2004_stream_handle_entries);

  for (BITCODE_BL i = 0; i < num_entries; i++)
    {
      Dwg_Stream_Object_Info info;
      int object_error = read_2004_stream_object_info (
          dwg, &obj_stream, input_mode, entries[i].address, entries[i].handle,
          &info);
      if (object_error)
        {
          LOG_WARN ("Failed to stream R2004 object %" PRIuSIZE
                    " handle " FORMAT_RLL,
                    entries[i].address, entries[i].handle);
          error |= object_error;
          if (object_error >= DWG_ERR_CRITICAL)
            goto done;
          continue;
        }

      info.index = i;
      if (callbacks->object)
        {
          int emitted_error = callbacks->object (&info, user);
          if (emitted_error)
            {
              *callback_error = emitted_error;
              error = emitted_error;
              goto done;
            }
        }
      if (callbacks->decoded_object)
        {
          BITCODE_RC *object_bytes;
          Bit_Chain object_dat = { 0 };
          size_t object_size;
          int emitted_error;

          object_error = dwg_stream_object_record_size (
              &info, entries[i].address, 0, &object_size);
          if (object_error)
            {
              error = object_error;
              goto done;
            }
          object_bytes = (BITCODE_RC *)calloc (object_size, 1);
          if (!object_bytes)
            {
              error = DWG_ERR_OUTOFMEM;
              goto done;
            }
          object_error = r2004_object_stream_read (
              &obj_stream, entries[i].address, object_bytes, object_size);
          if (object_error)
            {
              free (object_bytes);
              error = object_error;
              goto done;
            }
          object_dat = *dat;
          object_dat.chain = object_bytes;
          object_dat.byte = 0;
          object_dat.bit = 0;
          object_dat.size = object_size;
          emitted_error = r2004_stream_emit_decoded_object (
              dwg, &object_dat, &info, callbacks, user, acds, callback_error);
          free (object_bytes);
          if (emitted_error)
            {
              error = emitted_error;
              goto done;
            }
        }
    }

done:
  if (hdl_dat.chain)
    free (hdl_dat.chain);
  if (entries)
    free (entries);
  return error;
}

// may return OUTOFBOUNDS, needs to free the chain then

int
dwg_stream_read_r2004_to_r2006_and_r2010_to_r2022 (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    Dwg_Stream_Input_Mode input_mode, void *restrict user)
{
  int error = 0;
  int callback_error = 0;
  int stream_error;
  Dwg_Section *section;
  R2004_Stream_AcDs acds;

  memset (&acds, 0, sizeof (acds));

  error |= decode_R2004_header (dat, dwg);
  if (error > DWG_ERR_CRITICAL)
    return error;

  error |= read_R2004_section_map (dat, dwg);
  if (!dwg->header.section || error >= DWG_ERR_CRITICAL)
    {
      LOG_ERROR ("Failed to read R2004 Section Page Map.");
      return error | DWG_ERR_INTERNALERROR;
    }

  section = find_section (dwg, dwg->fhdr.r2004_header.section_info_id);
  if (section)
    {
      Dwg_Object *obj = NULL;
      Dwg_Section *_obj = section;

      dat->byte = section->address;
      FIELD_RLx (section_type, 0);
      if (FIELD_VALUE (section_type) != 0x4163003b)
        return error | DWG_ERR_SECTIONNOTFOUND;
      FIELD_RL (decomp_data_size, 0);
      FIELD_RL (comp_data_size, 0);
      FIELD_RL (compression_type, 0);
      FIELD_RLx (checksum, 0);

      error |= read_R2004_section_info (dat, dwg, _obj->comp_data_size,
                                        _obj->decomp_data_size);
    }
  else
    error |= DWG_ERR_SECTIONNOTFOUND;

  if (error >= DWG_ERR_CRITICAL)
    return error;

  error |= read_2004_section_header (dat, dwg);
  if (error < DWG_ERR_CRITICAL)
    error |= read_2004_section_classes (dat, dwg);
  if (error < DWG_ERR_CRITICAL && callbacks->decoded_object)
    error |= r2004_stream_acds_init (dat, dwg, &acds);
  if (error < DWG_ERR_CRITICAL)
    {
      stream_error = read_2004_section_handles_stream (
          dat, dwg, callbacks, input_mode, user, &acds, &callback_error);
      if (callback_error)
        {
          r2004_stream_acds_free (&acds);
          return callback_error;
        }
      error |= stream_error;
    }
  r2004_stream_acds_free (&acds);
  return error;
}
