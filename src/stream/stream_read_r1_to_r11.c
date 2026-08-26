/* Stream reader for DWG R1.1 through R11. */

#define _DEFAULT_SOURCE 1
#define _GNU_SOURCE 1
#include "config.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IS_DECODER
#include "common.h"
#include "bits.h"
#include "dwg.h"
#include "hash.h"
#include "decode.h"
#include "dynapi.h"
#include "free.h"
#include "stream_reader_internal.h"

static int cur_ver = 0;
static BITCODE_BL rcount1 = 0, rcount2 = 0;
static bool is_teigha = false;

#define DWG_LOGLEVEL loglevel
#include "logging.h"
#include "dec_macros.h"

static void
stream_clear_pre_r13_refs (Dwg_Data *restrict dwg,
                           const BITCODE_BL base_ref_count)
{
  BITCODE_BL i;

  if (base_ref_count < dwg->num_object_refs)
    {
      free (dwg->object_ordered_ref);
      dwg->object_ordered_ref = NULL;
      dwg->num_object_ordered_refs = (BITCODE_BL)-1;
    }
  for (i = base_ref_count; i < dwg->num_object_refs; i++)
    {
      free (dwg->object_ref[i]);
      dwg->object_ref[i] = NULL;
    }
  dwg->num_object_refs = base_ref_count;
}

static void
stream_update_pre_r13_handle_marker (Dwg_Data *restrict dwg,
                                     Dwg_Object_Ref *restrict marker,
                                     const Dwg_Object *restrict obj)
{
  Dwg_Object_Ref *handseed = dwg->header_vars.HANDSEED;

  if (!obj->handle.value || obj->handle.value <= marker->absolute_ref)
    return;
  marker->absolute_ref = obj->handle.value;
  marker->handleref.value = obj->handle.value;
  if (handseed && obj->handle.value > handseed->absolute_ref)
    {
      handseed->absolute_ref = obj->handle.value;
      handseed->handleref.value = obj->handle.value;
      dwg->auxheader.HANDSEED = obj->handle.value;
    }
}

static int
stream_set_pre_r13_header_ref (BITCODE_H *restrict field,
                               const BITCODE_RLL handle)
{
  if (!*field)
    *field = dwg_add_handleref_free (3, handle);
  if (!*field)
    return DWG_ERR_OUTOFMEM;

  (*field)->handleref.code = 3;
  (*field)->handleref.value = handle;
  (*field)->absolute_ref = handle;
  (*field)->obj = NULL;
  return 0;
}
static int
stream_append_pre_r13_header_entity (Dwg_Object_BLOCK_HEADER *restrict header,
                                     const BITCODE_RLL handle)
{
  BITCODE_H ref = dwg_add_handleref_free (3, handle);
  BITCODE_H *entities;

  if (!ref)
    return DWG_ERR_OUTOFMEM;
  ref->absolute_ref = handle;
  entities = (BITCODE_H *)realloc (header->entities, (header->num_owned + 1)
                                                         * sizeof (BITCODE_H));
  if (!entities)
    {
      free (ref);
      return DWG_ERR_OUTOFMEM;
    }
  header->entities = entities;
  header->entities[header->num_owned++] = ref;
  return 0;
}

static int
stream_finalize_pre_r13_entity (Bit_Chain *restrict dat,
                                Dwg_Object *restrict obj)
{
  const size_t address = obj->address & 0x3FFFFFFF;
  int error = 0;

  if (obj->type == DWG_TYPE_JUMP_r11)
    return 0;
  if (address > dat->size)
    return DWG_ERR_INVALIDDWG;

  if (dat->version < R_2_0b)
    {
      obj->size = (dat->byte - address) & 0xFFFFFFFF;
      return 0;
    }

  if (dat->version < R_11)
    {
      if (obj->size > dat->size - address
          || obj->size + address > dat->byte + 1)
        {
          LOG_ERROR ("Invalid obj->size " FORMAT_RL " changed to %" PRIuSIZE,
                     obj->size, dat->byte - address);
          error |= DWG_ERR_VALUEOUTOFBOUNDS;
          obj->size = (dat->byte - address) & 0xFFFFFFFF;
        }
      else if (address + obj->size != dat->byte)
        {
          if (address + obj->size > dat->byte)
            {
              BITCODE_RL offset
                  = (BITCODE_RL)(address + obj->size - dat->byte);
              obj->num_unknown_rest = 8 * offset;
              obj->unknown_rest = bit_read_TF (dat, offset);
              if (!obj->unknown_rest)
                {
                  LOG_ERROR ("Out of memory");
                  obj->num_unknown_rest = 0;
                }
            }
          if (obj->size > 2)
            dat->byte = address + obj->size;
        }
      return error;
    }

  if (obj->size > dat->size - address || obj->size + address > dat->byte + 2)
    {
      LOG_ERROR ("Invalid obj->size " FORMAT_RL " changed to %" PRIuSIZE,
                 obj->size, dat->byte + 2 - address);
      error |= DWG_ERR_VALUEOUTOFBOUNDS;
      obj->size = ((dat->byte + 2) - address) & 0xFFFFFFFF;
    }
  else if (address + obj->size != dat->byte + 2)
    {
      if (address + obj->size > dat->byte + 2)
        {
          BITCODE_RL offset
              = (BITCODE_RL)(address + obj->size - (dat->byte + 2));
          obj->num_unknown_rest = 8 * offset;
          obj->unknown_rest = bit_read_TF (dat, offset);
          if (!obj->unknown_rest)
            {
              LOG_ERROR ("Out of memory");
              obj->num_unknown_rest = 0;
            }
        }
      if (address + obj->size > dat->byte + 2)
        dat->byte = address + obj->size - 2;
    }
  if (!bit_check_CRC (dat, address, 0xC0C1))
    error |= DWG_ERR_WRONGCRC;
  return error;
}

static void
stream_pre_r13_object_info (const Dwg_Data *restrict dwg,
                            const Dwg_Object *restrict obj,
                            const BITCODE_BL index,
                            const Dwg_Stream_Input_Mode input_mode,
                            Dwg_Stream_Object_Info *restrict info)
{
  memset (info, 0, sizeof (*info));
  info->size = obj->size;
  info->address = obj->address;
  info->type = obj->type;
  info->index = index;
  info->fixedtype = obj->fixedtype;
  info->name = obj->name;
  info->dxfname = obj->dxfname;
  info->supertype = obj->supertype;
  info->handle = obj->handle;
  info->version = dwg->header.from_version;
  info->decode_mode = DWG_STREAM_DECODE_PRER13_ENTITY;
  info->input_mode = input_mode;
}

static int
is_pre_r13_existing_table_object (const Dwg_Object *restrict obj)
{
  if (!obj)
    return 0;

  if (obj->supertype == DWG_SUPERTYPE_ENTITY)
    return (obj->fixedtype == DWG_TYPE_BLOCK
            || obj->fixedtype == DWG_TYPE_ENDBLK)
           && !obj->address && !obj->size;

  switch (obj->fixedtype)
    {
    case DWG_TYPE_BLOCK_CONTROL:
    case DWG_TYPE_BLOCK_HEADER:
    case DWG_TYPE_LAYER_CONTROL:
    case DWG_TYPE_LAYER:
    case DWG_TYPE_STYLE_CONTROL:
    case DWG_TYPE_STYLE:
    case DWG_TYPE_LTYPE_CONTROL:
    case DWG_TYPE_LTYPE:
    case DWG_TYPE_VIEW_CONTROL:
    case DWG_TYPE_VIEW:
    case DWG_TYPE_UCS_CONTROL:
    case DWG_TYPE_UCS:
    case DWG_TYPE_VPORT_CONTROL:
    case DWG_TYPE_VPORT:
    case DWG_TYPE_APPID_CONTROL:
    case DWG_TYPE_APPID:
    case DWG_TYPE_DIMSTYLE_CONTROL:
    case DWG_TYPE_DIMSTYLE:
    case DWG_TYPE_VX_CONTROL:
    case DWG_TYPE_VX_TABLE_RECORD:
      return 1;
    default:
      return 0;
    }
}

static int
emit_pre_r13_existing_table_objects_stream (
    Dwg_Data *restrict dwg, const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user,
    const BITCODE_BL object_count, BITCODE_BL *restrict index)
{
  BITCODE_BL i;

  for (i = 0; i < object_count; i++)
    {
      Dwg_Object *obj = &dwg->object[i];
      Dwg_Stream_Object_Info info;
      int callback_error;

      if (!is_pre_r13_existing_table_object (obj))
        continue;

      stream_pre_r13_object_info (dwg, obj, *index, input_mode, &info);
      if (callbacks->object)
        {
          callback_error = callbacks->object (&info, user);
          if (callback_error)
            return callback_error;
        }
      if (callbacks->decoded_object)
        {
          dwg_stream_fixup_decoded_object (dwg, obj);
          callback_error = callbacks->decoded_object (&info, obj, user);
          if (callback_error)
            return callback_error;
        }
      (*index)++;
    }

  return 0;
}

static int
decode_pre_r13_polyline_variant (Bit_Chain *restrict dat,
                                 Dwg_Object *restrict obj)
{
  BITCODE_RC flag_r11;
  BITCODE_RS opts_r11;
  BITCODE_RC extra_r11 = 0;
  BITCODE_RS eed_size;
  BITCODE_RC handling_len;
  BITCODE_RC pline_flag;
  size_t start_byte;

  LOG_TRACE ("Detect polyline:");
  start_byte = dat->byte;
  LOG_TRACE (" start_byte: %" PRIuSIZE ",", start_byte);
  flag_r11 = bit_read_RC (dat);
  LOG_TRACE (" flag_r11: 0x%x,", flag_r11);
  dat->byte += 4;
  opts_r11 = bit_read_RS (dat);
  LOG_TRACE (" opts_r11: 0x%x", opts_r11);
  if (opts_r11 & OPTS_R11_POLYLINE_HAS_FLAG)
    {
      if (flag_r11 & FLAG_R11_HAS_PSPACE)
        {
          extra_r11 = bit_read_RC (dat);
          LOG_TRACE (", extra_r11: 0x%x", extra_r11);
        }
      if (flag_r11 & FLAG_R11_HAS_COLOR)
        dat->byte += 1;
      if (flag_r11 & FLAG_R11_HAS_LTYPE)
        {
          PRE (R_11)
          {
            dat->byte += 1;
          }
          else dat->byte += 2;
        }
      if (flag_r11 & FLAG_R11_HAS_THICKNESS)
        dat->byte += 8;
      if (flag_r11 & FLAG_R11_HAS_ELEVATION)
        dat->byte += 8;
      if (extra_r11 & EXTRA_R11_HAS_EED)
        {
          eed_size = bit_read_RS (dat);
          LOG_TRACE (", eed_size: %d", eed_size);
          dat->byte += eed_size;
        }
      if (flag_r11 & FLAG_R11_HAS_HANDLING)
        {
          handling_len = bit_read_RC (dat);
          LOG_TRACE (", handling_len: %d", handling_len);
          dat->byte += handling_len;
        }
      if (extra_r11 & EXTRA_R11_HAS_VIEWPORT)
        dat->byte += 2;
      pline_flag = bit_read_RC (dat);
      LOG_TRACE (", pline_flag: 0x%x", pline_flag);
      LOG_POS;
      dat->byte = start_byte;
      if (pline_flag & FLAG_POLYLINE_3D)
        return dwg_stream_decode_pre_r13_POLYLINE_3D (dat, obj);
      if (pline_flag & FLAG_POLYLINE_MESH)
        return dwg_stream_decode_pre_r13_POLYLINE_MESH (dat, obj);
      if (pline_flag & FLAG_POLYLINE_PFACE_MESH)
        return dwg_stream_decode_pre_r13_POLYLINE_PFACE (dat, obj);
      return dwg_stream_decode_pre_r13_POLYLINE_2D (dat, obj);
    }

  dat->byte = start_byte;
  LOG_TRACE ("\n");
  return dwg_stream_decode_pre_r13_POLYLINE_2D (dat, obj);
}

static int
decode_pre_r13_vertex_variant (Bit_Chain *restrict dat,
                               Dwg_Object *restrict obj)
{
  BITCODE_RC flag_r11;
  BITCODE_RS opts_r11;
  BITCODE_RC extra_r11 = 0;
  BITCODE_RS eed_size;
  BITCODE_RC handling_len;
  BITCODE_RC vertex_flag;
  size_t start_byte;

  LOG_TRACE ("Detect vertex:");
  start_byte = dat->byte;
  LOG_TRACE (" start_byte: %" PRIuSIZE ",", start_byte);
  flag_r11 = bit_read_RC (dat);
  LOG_TRACE (" flag_r11: 0x%x,", flag_r11);
  dat->byte += 4;
  opts_r11 = bit_read_RS (dat);
  LOG_TRACE (" opts_r11: 0x%x", opts_r11);
  if (flag_r11 & FLAG_R11_HAS_COLOR)
    dat->byte += 1;
  if (flag_r11 & FLAG_R11_HAS_LTYPE)
    {
      PRE (R_11)
      {
        dat->byte += 1;
      }
      else dat->byte += 2;
    }
  if (flag_r11 & FLAG_R11_HAS_THICKNESS)
    dat->byte += 8;
  if (flag_r11 & FLAG_R11_HAS_ELEVATION)
    dat->byte += 8;
  if (flag_r11 & FLAG_R11_HAS_PSPACE)
    {
      extra_r11 = bit_read_RC (dat);
      LOG_TRACE (", extra_r11: 0x%x", extra_r11);
    }
  if (extra_r11 && extra_r11 & EXTRA_R11_HAS_EED)
    {
      eed_size = bit_read_RS (dat);
      LOG_TRACE (", eed_size: %d", eed_size);
      dat->byte += eed_size;
    }
  if (flag_r11 & FLAG_R11_HAS_HANDLING)
    {
      handling_len = bit_read_RC (dat);
      LOG_TRACE (", handling_len: %d", handling_len);
      dat->byte += handling_len;
    }
  if (extra_r11 && extra_r11 & EXTRA_R11_HAS_VIEWPORT)
    dat->byte += 2;
  if (!(opts_r11 & OPTS_R11_VERTEX_HAS_NOT_X_Y))
    dat->byte += 16;
  if (opts_r11 & OPTS_R11_VERTEX_HAS_START_WIDTH)
    dat->byte += 8;
  if (opts_r11 & OPTS_R11_VERTEX_HAS_END_WIDTH)
    dat->byte += 8;
  if (opts_r11 & OPTS_R11_VERTEX_HAS_BULGE)
    dat->byte += 8;
  if (opts_r11 & OPTS_R11_VERTEX_HAS_FLAG)
    {
      vertex_flag = bit_read_RC (dat);
      LOG_TRACE (", vertex_flag: 0x%x", vertex_flag);
      LOG_POS;
      dat->byte = start_byte;
      if (vertex_flag & FLAG_VERTEX_MESH
          && vertex_flag & FLAG_VERTEX_PFACE_MESH)
        return dwg_stream_decode_pre_r13_VERTEX_PFACE (dat, obj);
      if (vertex_flag & FLAG_VERTEX_MESH)
        return dwg_stream_decode_pre_r13_VERTEX_MESH (dat, obj);
      if (vertex_flag & FLAG_VERTEX_PFACE_MESH)
        return dwg_stream_decode_pre_r13_VERTEX_PFACE_FACE (dat, obj);
      if (vertex_flag & FLAG_VERTEX_3D)
        return dwg_stream_decode_pre_r13_VERTEX_3D (dat, obj);
      return dwg_stream_decode_pre_r13_VERTEX_2D (dat, obj);
    }

  dat->byte = start_byte;
  LOG_TRACE ("\n");
  return dwg_stream_decode_pre_r13_VERTEX_2D (dat, obj);
}

static int
decode_pre_r13_table_section_stream (Bit_Chain *restrict dat,
                                     Dwg_Data *restrict dwg,
                                     const Dwg_Section_Type_r11 section)
{
  Dwg_Section *tbl;
  int error;

  tbl = &dwg->header.section[section];
  if (!tbl->address || !tbl->number)
    return 0;
  if (dwg->header.from_version >= R_11 && tbl->address < 16)
    return DWG_ERR_INVALIDDWG;

  dat->byte
      = dwg->header.from_version >= R_11 ? tbl->address - 16 : tbl->address;
  dat->bit = 0;
  error = decode_preR13_section (section, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    LOG_ERROR ("Failed to decode pre-R13 stream table %s at " FORMAT_RLL,
               tbl->name, tbl->address);
  return error;
}

static int
read_pre_r13_entity_section_stream (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user,
    const BITCODE_RL start, const BITCODE_RL end,
    const Dwg_Sentinel begin_sentinel, const char *restrict begin_name,
    const Dwg_Sentinel end_sentinel, const char *restrict end_name,
    const EntitySectionIndexR11 entity_section, BITCODE_BL *restrict index)
{
  int error = 0;
  BITCODE_BL owner_index = 0;
  BITCODE_BL block_index = 0;
  Dwg_Object_Ref *handle_marker;
  BITCODE_RLL next_handle;
  int have_owner = 0;

  if (entity_section != BLOCKS_SECTION_INDEX)
    {
      Dwg_Object *owner = dwg_model_space_object (dwg);

      if (owner && owner->fixedtype == DWG_TYPE_BLOCK_HEADER)
        {
          Dwg_Object_BLOCK_HEADER *header
              = owner->tio.object->tio.BLOCK_HEADER;

          owner_index = owner->index;
          have_owner = 1;
          header->block_offset_r11 = (BITCODE_RL)-1;
        }
    }

  if (end < start || end > dat->size
      || (dwg->header.from_version >= R_11 && start < 16))
    return DWG_ERR_INVALIDDWG;
  if (end == start)
    return 0;
  if (!start)
    return DWG_ERR_INVALIDDWG;

  dat->byte = dwg->header.from_version >= R_11 ? start - 16 : start;
  dat->bit = 0;
  if (dwg->header.from_version >= R_11)
    {
      error |= decode_preR13_sentinel (begin_sentinel, begin_name, dat, dwg);
      if (error >= DWG_ERR_CRITICAL)
        return error;
    }
  if ((BITCODE_RL)dat->byte != start)
    return DWG_ERR_INVALIDDWG;

  next_handle = dwg_next_handle (dwg);
  handle_marker
      = dwg_add_handleref (dwg, 0, next_handle ? next_handle - 1 : 0, NULL);
  if (!handle_marker)
    return DWG_ERR_OUTOFMEM;

  while ((BITCODE_RL)dat->byte < end)
    {
      Dwg_Object *obj;
      Dwg_Object_Type_r11 abstype;
      BITCODE_BL base_object_count = dwg->num_objects;
      BITCODE_BL base_ref_count = dwg->num_object_refs;
      Dwg_Stream_Object_Info info;
      dwg_inthash *host_object_map = dwg->object_map;
      int add_error;
      int callback_error = 0;
      size_t iter_start = dat->byte;

      dwg->object_map = hash_new (100);
      if (!dwg->object_map)
        {
          dwg->object_map = host_object_map;
          return DWG_ERR_OUTOFMEM;
        }
      add_error = dwg_add_object (dwg);
      if (add_error >= DWG_ERR_CRITICAL)
        {
          hash_free (dwg->object_map);
          dwg->object_map = host_object_map;
          return add_error;
        }
      obj = &dwg->object[base_object_count];
      obj->address = dat->byte;
      if (entity_section == BLOCKS_SECTION_INDEX)
        obj->address |= 0x40000000;
      obj->supertype = DWG_SUPERTYPE_ENTITY;
      if (dwg->header.from_version < R_2_0b)
        {
          obj->type = bit_read_RS (dat);
          abstype = obj->type > 127
                        ? (Dwg_Object_Type_r11)abs ((int8_t)obj->type)
                        : (Dwg_Object_Type_r11)obj->type;
        }
      else
        {
          obj->type = bit_read_RC (dat);
          abstype = obj->type > 127
                        ? (Dwg_Object_Type_r11)((unsigned)obj->type & 0x7F)
                        : (Dwg_Object_Type_r11)obj->type;
        }

      switch (abstype)
        {
        case DWG_TYPE_LINE_r11:
          error |= dwg_stream_decode_pre_r13_LINE (dat, obj);
          break;
        case DWG_TYPE_POINT_r11:
          error |= dwg_stream_decode_pre_r13_POINT (dat, obj);
          break;
        case DWG_TYPE_CIRCLE_r11:
          error |= dwg_stream_decode_pre_r13_CIRCLE (dat, obj);
          break;
        case DWG_TYPE_SHAPE_r11:
          error |= dwg_stream_decode_pre_r13_SHAPE (dat, obj);
          break;
        case DWG_TYPE_REPEAT_r11:
          error |= dwg_stream_decode_pre_r13_REPEAT (dat, obj);
          break;
        case DWG_TYPE_ENDREP_r11:
          error |= dwg_stream_decode_pre_r13_ENDREP (dat, obj);
          break;
        case DWG_TYPE_TEXT_r11:
          error |= dwg_stream_decode_pre_r13_TEXT (dat, obj);
          break;
        case DWG_TYPE_ARC_r11:
          error |= dwg_stream_decode_pre_r13_ARC (dat, obj);
          break;
        case DWG_TYPE_TRACE_r11:
          error |= dwg_stream_decode_pre_r13_TRACE (dat, obj);
          break;
        case DWG_TYPE_LOAD_r11:
          error |= dwg_stream_decode_pre_r13_LOAD (dat, obj);
          break;
        case DWG_TYPE_SOLID_r11:
          error |= dwg_stream_decode_pre_r13_SOLID (dat, obj);
          break;
        case DWG_TYPE_BLOCK_r11:
          error |= dwg_stream_decode_pre_r13_BLOCK (dat, obj);
          if (entity_section == BLOCKS_SECTION_INDEX)
            {
              BITCODE_RL offset = (BITCODE_RL)(iter_start - start);
              BITCODE_BL i;

              if (dwg->header.from_version > R_2_22)
                offset |= 0x40000000;
              have_owner = 0;
              for (i = 0; i < base_object_count; i++)
                {
                  Dwg_Object *candidate = &dwg->object[i];
                  Dwg_Object_BLOCK_HEADER *header;

                  if (candidate->fixedtype != DWG_TYPE_BLOCK_HEADER
                      || !candidate->tio.object
                      || !candidate->tio.object->tio.BLOCK_HEADER)
                    continue;
                  header = candidate->tio.object->tio.BLOCK_HEADER;
                  if (header->block_offset_r11 != offset)
                    continue;
                  owner_index = i;
                  have_owner = 1;
                  block_index++;
                  if (!obj->handle.value)
                    obj->handle.value = dwg_next_handle (dwg);
                  error |= stream_set_pre_r13_header_ref (
                      &header->block_entity, obj->handle.value);
                  if (error >= DWG_ERR_CRITICAL)
                    goto object_done;
                  if (obj->tio.entity && obj->tio.entity->tio.BLOCK
                      && !obj->tio.entity->tio.BLOCK->name && header->name)
                    obj->tio.entity->tio.BLOCK->name = strdup (header->name);
                  break;
                }
            }
          break;
        case DWG_TYPE_ENDBLK_r11:
          error |= dwg_stream_decode_pre_r13_ENDBLK (dat, obj);
          if (have_owner && owner_index < base_object_count)
            {
              Dwg_Object *owner = &dwg->object[owner_index];
              Dwg_Object_BLOCK_HEADER *header
                  = owner->tio.object->tio.BLOCK_HEADER;

              if (!obj->handle.value)
                obj->handle.value = dwg_next_handle (dwg);
              error |= stream_set_pre_r13_header_ref (&header->endblk_entity,
                                                      obj->handle.value);
              if (error >= DWG_ERR_CRITICAL)
                goto object_done;
            }
          have_owner = 0;
          break;
        case DWG_TYPE_3DFACE_r11:
          error |= dwg_stream_decode_pre_r13__3DFACE (dat, obj);
          break;
        case DWG_TYPE_DIMENSION_r11:
          error |= decode_preR13_DIMENSION (dat, obj);
          break;
        case DWG_TYPE_INSERT_r11:
          error |= dwg_stream_decode_pre_r13_INSERT (dat, obj);
          break;
        case DWG_TYPE_ATTDEF_r11:
          error |= dwg_stream_decode_pre_r13_ATTDEF (dat, obj);
          break;
        case DWG_TYPE_ATTRIB_r11:
          error |= dwg_stream_decode_pre_r13_ATTRIB (dat, obj);
          break;
        case DWG_TYPE_SEQEND_r11:
          error |= dwg_stream_decode_pre_r13_SEQEND (dat, obj);
          break;
        case DWG_TYPE_JUMP_r11:
          error |= dwg_stream_decode_pre_r13_JUMP (dat, obj);
          break;
        case DWG_TYPE_POLYLINE_r11:
          error |= decode_pre_r13_polyline_variant (dat, obj);
          break;
        case DWG_TYPE_VERTEX_r11:
          error |= decode_pre_r13_vertex_variant (dat, obj);
          break;
        case DWG_TYPE_3DLINE_r11:
          error |= dwg_stream_decode_pre_r13__3DLINE (dat, obj);
          break;
        case DWG_TYPE_VIEWPORT_r11:
          error |= dwg_stream_decode_pre_r13_VIEWPORT (dat, obj);
          break;
        default:
          LOG_ERROR ("DWG stream reader does not support pre-R13 entity type "
                     "%u",
                     (unsigned)abstype);
          error = DWG_ERR_NOTYETSUPPORTED;
          goto object_done;
        }

      error |= stream_finalize_pre_r13_entity (dat, obj);
      if (error >= DWG_ERR_CRITICAL)
        goto object_done;
      stream_update_pre_r13_handle_marker (dwg, handle_marker, obj);
      if (have_owner && obj->supertype == DWG_SUPERTYPE_ENTITY
          && obj->tio.entity && !obj->tio.entity->ownerhandle
          && obj->fixedtype != DWG_TYPE_UNUSED
          && obj->fixedtype != DWG_TYPE_JUMP
          && obj->type != DWG_TYPE_VERTEX_r11
          && obj->fixedtype != DWG_TYPE_SEQEND
          && owner_index < base_object_count)
        {
          Dwg_Object *owner = &dwg->object[owner_index];

          if (owner->handle.value)
            {
              Dwg_Object_BLOCK_HEADER *header
                  = owner->tio.object->tio.BLOCK_HEADER;

              if (obj->fixedtype != DWG_TYPE_BLOCK)
                {
                  error |= stream_append_pre_r13_header_entity (
                      header, obj->handle.value);
                  if (error >= DWG_ERR_CRITICAL)
                    goto object_done;
                }
              obj->tio.entity->ownerhandle
                  = dwg_add_handleref (dwg, 4, owner->handle.value, obj);
              obj->tio.entity->ownerhandle->r11_idx = block_index;
            }
        }
      stream_pre_r13_object_info (dwg, obj, *index, input_mode, &info);
      if (callbacks->object)
        {
          callback_error = callbacks->object (&info, user);
          if (callback_error)
            {
              error = callback_error;
              goto object_done;
            }
        }
      if (callbacks->decoded_object)
        {
          dwg_stream_fixup_decoded_object (dwg, obj);
          callback_error = callbacks->decoded_object (&info, obj, user);
          if (callback_error)
            {
              error = callback_error;
              goto object_done;
            }
        }

    object_done:
      if (obj->tio.object)
        dwg_free_object (obj);
      dwg->num_objects = base_object_count;
      stream_clear_pre_r13_refs (dwg, base_ref_count);
      hash_free (dwg->object_map);
      dwg->object_map = host_object_map;
      if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
        return error;
      if (dat->byte == iter_start)
        return DWG_ERR_INVALIDDWG;
      (*index)++;
    }
  if ((BITCODE_RL)dat->byte != end)
    {
      LOG_ERROR ("Pre-R13 stream entity section ended at %" PRIuSIZE
                 ", expected " FORMAT_RL,
                 dat->byte, end);
      return DWG_ERR_INVALIDDWG;
    }
  if (dwg->header.from_version >= R_11)
    error |= decode_preR13_sentinel (end_sentinel, end_name, dat, dwg);
  return error >= DWG_ERR_CRITICAL ? error : 0;
}

int
dwg_stream_read_r1_to_r2 (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user)
{
  BITCODE_RL start;
  BITCODE_RL end;
  BITCODE_BL index = 0;
  int error = 0;

  if (dwg->header.from_version < R_1_1 || dwg->header.from_version > R_1_4)
    return DWG_ERR_NOTYETSUPPORTED;

  error = dwg_add_Document (dwg, 0);
  if (error >= DWG_ERR_CRITICAL)
    return error;

  error = decode_preR13_header_variables (dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  if (dat->byte + 2 >= dat->size)
    return error | DWG_ERR_CRITICAL;

  dwg->header.entities_start = dat->byte & 0xFFFFFFFF;
  dwg->header.entities_end = dwg->header_vars.dwg_size;
  start = dwg->header.entities_start;
  end = dwg->header.entities_end;

  error = read_pre_r13_entity_section_stream (
      dat, dwg, callbacks, input_mode, user, start, end,
      DWG_SENTINEL_R11_ENTITIES_BEGIN, "DWG_SENTINEL_R11_ENTITIES_BEGIN",
      DWG_SENTINEL_R11_ENTITIES_END, "DWG_SENTINEL_R11_ENTITIES_END",
      ENTITIES_SECTION_INDEX, &index);
  if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
    return error;

  return emit_pre_r13_existing_table_objects_stream (
      dwg, callbacks, input_mode, user, dwg->num_objects, &index);
}

int
dwg_stream_read_r2_to_r10 (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user)
{
  BITCODE_RL blocks_start;
  BITCODE_RL blocks_end;
  BITCODE_RL blocks_size;
  BITCODE_RL extras_start;
  BITCODE_RL extras_end;
  BITCODE_RL extras_size;
  BITCODE_BL index = 0;
  int error = 0;

  if (dwg->header.from_version < R_2_0b || dwg->header.from_version > R_11b2)
    return DWG_ERR_NOTYETSUPPORTED;

  blocks_start = dwg->header.blocks_start;
  blocks_size = dwg->header.blocks_size;
  if (blocks_size > 0xffffff)
    blocks_size &= 0xffffff;
  if (!blocks_start)
    blocks_size = 0;
  blocks_end = blocks_start + blocks_size;
  extras_start = dwg->header.extras_start;
  extras_size = dwg->header.extras_size;
  if (extras_size > 0xffffff)
    extras_size &= 0xffffff;
  if (!extras_start)
    extras_size = 0;
  extras_end = extras_start + extras_size;

  error = dwg_add_Document (dwg, 0);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  dwg->header.section[0].number = 0;
  dwg->header.section[0].type = (Dwg_Section_Type)SECTION_HEADER_R11;
  strcpy (dwg->header.section[0].name, "HEADER");

  error = decode_preR13_section_hdr ("BLOCK", SECTION_BLOCK, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("LAYER", SECTION_LAYER, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("STYLE", SECTION_STYLE, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("LTYPE", SECTION_LTYPE, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("VIEW", SECTION_VIEW, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;

  error = decode_preR13_header_variables (dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  if (dat->byte + 2 >= dat->size)
    return error | DWG_ERR_CRITICAL;

  error = read_pre_r13_entity_section_stream (
      dat, dwg, callbacks, input_mode, user, dwg->header.entities_start,
      dwg->header.entities_end, DWG_SENTINEL_R11_ENTITIES_BEGIN,
      "DWG_SENTINEL_R11_ENTITIES_BEGIN", DWG_SENTINEL_R11_ENTITIES_END,
      "DWG_SENTINEL_R11_ENTITIES_END", ENTITIES_SECTION_INDEX, &index);
  if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
    return error;

#define STREAM_PRER10_TABLE_SECTION(section)                                  \
  do                                                                          \
    {                                                                         \
      error = decode_pre_r13_table_section_stream (dat, dwg, section);        \
      if (error >= DWG_ERR_CRITICAL)                                          \
        return error;                                                         \
    }                                                                         \
  while (0)

  STREAM_PRER10_TABLE_SECTION (SECTION_BLOCK);
  STREAM_PRER10_TABLE_SECTION (SECTION_LAYER);
  STREAM_PRER10_TABLE_SECTION (SECTION_STYLE);
  STREAM_PRER10_TABLE_SECTION (SECTION_LTYPE);
  STREAM_PRER10_TABLE_SECTION (SECTION_VIEW);
  if (dwg->header.num_sections >= SECTION_VPORT)
    {
      STREAM_PRER10_TABLE_SECTION (SECTION_UCS);
      STREAM_PRER10_TABLE_SECTION (SECTION_VPORT);
    }
  if (dwg->header.num_sections >= SECTION_APPID)
    STREAM_PRER10_TABLE_SECTION (SECTION_APPID);
  if (dwg->header.num_sections >= SECTION_VX)
    {
      STREAM_PRER10_TABLE_SECTION (SECTION_DIMSTYLE);
      STREAM_PRER10_TABLE_SECTION (SECTION_VX);
    }

#undef STREAM_PRER10_TABLE_SECTION

  if (blocks_size)
    {
      error = read_pre_r13_entity_section_stream (
          dat, dwg, callbacks, input_mode, user, blocks_start, blocks_end,
          DWG_SENTINEL_R11_BLOCK_ENTITIES_BEGIN,
          "DWG_SENTINEL_R11_BLOCK_ENTITIES_BEGIN",
          DWG_SENTINEL_R11_BLOCK_ENTITIES_END,
          "DWG_SENTINEL_R11_BLOCK_ENTITIES_END", BLOCKS_SECTION_INDEX, &index);
      if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
        return error;
    }

  if (extras_size)
    {
      error = read_pre_r13_entity_section_stream (
          dat, dwg, callbacks, input_mode, user, extras_start, extras_end,
          DWG_SENTINEL_R11_EXTRA_ENTITIES_BEGIN,
          "DWG_SENTINEL_R11_EXTRA_ENTITIES_BEGIN",
          DWG_SENTINEL_R11_EXTRA_ENTITIES_END,
          "DWG_SENTINEL_R11_EXTRA_ENTITIES_END", EXTRAS_SECTION_INDEX, &index);
      if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
        return error;
    }

  return emit_pre_r13_existing_table_objects_stream (
      dwg, callbacks, input_mode, user, dwg->num_objects, &index);
}

int
dwg_stream_read_r11 (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user)
{
  BITCODE_RL blocks_size;
  BITCODE_RL extras_size;
  BITCODE_BL index = 0;
  int error = 0;

  if (dwg->header.from_version != R_11)
    return DWG_ERR_NOTYETSUPPORTED;

  error = dwg_add_Document (dwg, 0);
  if (error >= DWG_ERR_CRITICAL)
    return error;

  dwg->header.section[0].number = 0;
  dwg->header.section[0].type = (Dwg_Section_Type)SECTION_HEADER_R11;
  strcpy (dwg->header.section[0].name, "HEADER");

  error = decode_preR13_section_hdr ("BLOCK", SECTION_BLOCK, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("LAYER", SECTION_LAYER, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("STYLE", SECTION_STYLE, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("LTYPE", SECTION_LTYPE, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = decode_preR13_section_hdr ("VIEW", SECTION_VIEW, dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;

  error = decode_preR13_header_variables (dat, dwg);
  if (error >= DWG_ERR_CRITICAL)
    return error;
  error = read_pre_r13_entity_section_stream (
      dat, dwg, callbacks, input_mode, user, dwg->header.entities_start,
      dwg->header.entities_end, DWG_SENTINEL_R11_ENTITIES_BEGIN,
      "DWG_SENTINEL_R11_ENTITIES_BEGIN", DWG_SENTINEL_R11_ENTITIES_END,
      "DWG_SENTINEL_R11_ENTITIES_END", ENTITIES_SECTION_INDEX, &index);
  if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
    return error;

#define STREAM_PRER13_TABLE_SECTION(section)                                  \
  do                                                                          \
    {                                                                         \
      error = decode_pre_r13_table_section_stream (dat, dwg, section);        \
      if (error >= DWG_ERR_CRITICAL)                                          \
        return error;                                                         \
    }                                                                         \
  while (0)

  STREAM_PRER13_TABLE_SECTION (SECTION_BLOCK);
  STREAM_PRER13_TABLE_SECTION (SECTION_LAYER);
  STREAM_PRER13_TABLE_SECTION (SECTION_STYLE);
  STREAM_PRER13_TABLE_SECTION (SECTION_LTYPE);
  STREAM_PRER13_TABLE_SECTION (SECTION_VIEW);
  if (dwg->header.num_sections >= SECTION_VPORT)
    {
      STREAM_PRER13_TABLE_SECTION (SECTION_UCS);
      STREAM_PRER13_TABLE_SECTION (SECTION_VPORT);
    }
  if (dwg->header.num_sections >= SECTION_APPID)
    STREAM_PRER13_TABLE_SECTION (SECTION_APPID);
  if (dwg->header.num_sections >= SECTION_VX)
    {
      STREAM_PRER13_TABLE_SECTION (SECTION_DIMSTYLE);
      STREAM_PRER13_TABLE_SECTION (SECTION_VX);
    }

#undef STREAM_PRER13_TABLE_SECTION

  blocks_size = dwg->header.blocks_size;
  if (blocks_size > 0xffffff)
    blocks_size &= 0xffffff;
  if (blocks_size)
    {
      error = read_pre_r13_entity_section_stream (
          dat, dwg, callbacks, input_mode, user, dwg->header.blocks_start,
          dwg->header.blocks_start + blocks_size,
          DWG_SENTINEL_R11_BLOCK_ENTITIES_BEGIN,
          "DWG_SENTINEL_R11_BLOCK_ENTITIES_BEGIN",
          DWG_SENTINEL_R11_BLOCK_ENTITIES_END,
          "DWG_SENTINEL_R11_BLOCK_ENTITIES_END", BLOCKS_SECTION_INDEX, &index);
      if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
        return error;
    }

  extras_size = dwg->header.extras_size;
  if (extras_size > 0xffffff)
    extras_size &= 0xffffff;
  if (extras_size)
    {
      error = read_pre_r13_entity_section_stream (
          dat, dwg, callbacks, input_mode, user, dwg->header.extras_start,
          dwg->header.extras_start + extras_size,
          DWG_SENTINEL_R11_EXTRA_ENTITIES_BEGIN,
          "DWG_SENTINEL_R11_EXTRA_ENTITIES_BEGIN",
          DWG_SENTINEL_R11_EXTRA_ENTITIES_END,
          "DWG_SENTINEL_R11_EXTRA_ENTITIES_END", EXTRAS_SECTION_INDEX, &index);
      if (error >= DWG_ERR_CRITICAL || error == DWG_ERR_NOTYETSUPPORTED)
        return error;
    }

  error = emit_pre_r13_existing_table_objects_stream (
      dwg, callbacks, input_mode, user, dwg->num_objects, &index);
  if (error)
    return error;

  return 0;
}
