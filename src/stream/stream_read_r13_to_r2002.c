/* Stream reader for R13, R14, R2000, R2000i, and R2002. */

#define _DEFAULT_SOURCE 1
#include "config.h"
#include <limits.h>
#include <stdbool.h>
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

static int
read_r13_to_r2002_object_info (Dwg_Data *restrict dwg,
                                   Bit_Chain *restrict dat,
                                   const Dwg_Stream_Input_Mode input_mode,
                                   const size_t address,
                                   const BITCODE_RLL handle_value,
                                   Dwg_Stream_Object_Info *restrict info)
{
  Bit_Chain body;
  Dwg_Object_Type fixedtype;

  memset (info, 0, sizeof (*info));
  if (address >= dat->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  body = *dat;
  body.byte = address;
  body.bit = 0;
  info->size = bit_read_MS (&body);
  info->address = body.byte;
  if (info->size > dat->size || info->address > dat->size - info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  bit_reset_chain (&body);
  body.size = info->size;
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
  info->decode_mode = DWG_STREAM_DECODE_R13_OBJECT_MAP;
  info->input_mode = input_mode;
  return 0;
}

static int
read_r13_to_r2002_handle_map (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user)
{
  BITCODE_RS section_size = 0;
  BITCODE_RS crc, crc2;
  BITCODE_BL index = 0;
  size_t lastmap;
  int error = 0;
  int callback_error = 0;

  if (dwg->header.sections <= SECTION_HANDLES_R13)
    return DWG_ERR_SECTIONNOTFOUND;
  dat->byte = dwg->header.section[SECTION_HANDLES_R13].address;
  dat->bit = 0;
  lastmap = dat->byte + dwg->header.section[SECTION_HANDLES_R13].size;
  if (lastmap > dat->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  do
    {
      size_t last_offset = 0;
      size_t oldpos = 0;
      size_t startpos = dat->byte;
      BITCODE_RLL last_handle = 0;
      BITCODE_RLL maxh
          = (BITCODE_RLL)dwg->header.section[SECTION_HANDLES_R13].size << 1;
      BITCODE_RLL max_handles
          = maxh < INT32_MAX ? maxh
                             : dwg->header.section[SECTION_HANDLES_R13].size;

      if (lastmap - startpos < 2)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      section_size = bit_read_RS_BE (dat);
      if (section_size > 2040 || (size_t)section_size + 2 > lastmap - startpos)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }

      while (dat->byte - startpos < section_size)
        {
          BITCODE_UMC handleoff;
          BITCODE_MC offset;
          Dwg_Stream_Object_Info info;
          int object_error;

          oldpos = dat->byte;
          handleoff = bit_read_UMC (dat);
          offset = bit_read_MC (dat);
          if (!handleoff || handleoff > (max_handles - last_handle))
            error |= DWG_ERR_VALUEOUTOFBOUNDS;
          object_error
              = dwg_stream_add_handle_value (&last_handle, handleoff,
                                             max_handles);
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
          if (dat->byte == oldpos)
            break;

          object_error = read_r13_to_r2002_object_info (
              dwg, dat, input_mode, last_offset, last_handle, &info);
          if (object_error)
            {
              error |= object_error;
              if (object_error >= DWG_ERR_CRITICAL)
                break;
              continue;
            }

          info.index = index++;
          if (callbacks->object)
            {
              callback_error = callbacks->object (&info, user);
              if (callback_error)
                {
                  error = callback_error;
                  goto done;
                }
            }

          if (callbacks->decoded_object)
            {
              Bit_Chain object_dat = *dat;
              size_t object_size;
              object_error = dwg_stream_object_record_size (
                  &info, last_offset, dat->size, &object_size);
              if (object_error)
                {
                  error |= object_error;
                  if (object_error >= DWG_ERR_CRITICAL)
                    break;
                  continue;
                }
              object_dat.chain = &dat->chain[last_offset];
              object_dat.byte = 0;
              object_dat.bit = 0;
              object_dat.size = object_size;
              object_error = dwg_stream_emit_decoded_object (
                  dwg, &object_dat, &info, callbacks, user);
              if (object_error)
                {
                  callback_error = object_error;
                  error = object_error;
                  goto done;
                }
            }
        }

      if (dat->byte == oldpos)
        break;
      if (dat->bit > 0)
        {
          dat->byte += 1;
          dat->bit = 0;
        }
      if (dat->byte >= dat->size)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      crc = bit_read_RS_BE (dat);
      crc2 = bit_calc_CRC (0xC0C1, dat->chain + startpos, section_size);
      if (crc != crc2 && dat->from_version != R_14)
        error |= DWG_ERR_WRONGCRC;
      if (dat->byte >= lastmap)
        break;
    }
  while (section_size > 2);

done:
  if (callback_error)
    return callback_error;
  return error >= DWG_ERR_CRITICAL ? error : 0;
}

int
dwg_stream_read_r13_to_r2002 (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    Dwg_Stream_Input_Mode input_mode, void *restrict user)
{
  Dwg_Object *obj = NULL;
  BITCODE_RS crc, crc2;
  size_t size, endpos, pvz = 0;
  BITCODE_BL j;
  int error = 0;
  int sentinel_size = 16;
  const char *section_names[]
      = { "AcDb:Header",       "AcDb:Classes",  "AcDb:Handles",
          "AcDb:ObjFreeSpace", "AcDb:Template", "AcDb:AuxHeader" };

  if ((error = dwg_sections_init (dwg)))
    return error;
  if (dat->byte != 0x19)
    return DWG_ERR_INVALIDDWG;

  for (j = 0; j < dwg->header.sections; j++)
    {
      dwg->header.section[j].number = (BITCODE_RLd)bit_read_RC (dat);
      dwg->header.section[j].address = (BITCODE_RLL)bit_read_RL (dat);
      dwg->header.section[j].size = bit_read_RL (dat);
      if (j < 6)
        strcpy (dwg->header.section[j].name, section_names[j]);
      if (dwg->header.section[j].address + dwg->header.section[j].size
          > dat->size)
        return DWG_ERR_INVALIDDWG;
    }

  crc2 = bit_calc_CRC (0xC0C1, &dat->chain[0], dat->byte);
  crc = bit_read_RS (dat);
  if (crc != crc2)
    error |= DWG_ERR_WRONGCRC;
  (void)bit_search_sentinel (dat, dwg_sentinel (DWG_SENTINEL_HEADER_END));

  if (dwg->header.sections == 6 && dwg->header.version >= R_13c3)
    {
      Dwg_AuxHeader *_obj = &dwg->auxheader;
      Bit_Chain *hdl_dat = dat;
      size_t end_address = dwg->header.section[SECTION_AUXHEADER_R2000].address
                           + dwg->header.section[SECTION_AUXHEADER_R2000].size;
      if (dat->size >= end_address)
        {
          size_t old_size = dat->size;
          BITCODE_BL vcount;
          dat->byte = dwg->header.section[SECTION_AUXHEADER_R2000].address;
          dat->size = end_address;
          // clang-format off
          #include "auxheader.spec"
          // clang-format on
          dat->size = old_size;
        }
    }

  if (dwg->header.section[SECTION_HEADER_R13].address < 58
      || dwg->header.section[SECTION_HEADER_R13].address
                 + dwg->header.section[SECTION_HEADER_R13].size
             > dat->size)
    {
      error |= DWG_ERR_SECTIONNOTFOUND;
      goto classes_section;
    }

  dat->byte = pvz = dwg->header.section[SECTION_HEADER_R13].address + 16;
  dwg->header_vars.size = bit_read_RL (dat);
  if (dwg->header_vars.size > DWG_R13_MAX_HEADER_SIZE)
    {
      dwg->header_vars.size = dwg->header.section[SECTION_HEADER_R13].size;
      if (dwg->header_vars.size > 20)
        dwg->header_vars.size -= (16 + 4);
    }
  dat->bit = 0;
  error |= dwg_decode_header_variables (dat, dat, dat, dwg);
  if (dwg->header_vars.size < DWG_R13_MAX_HEADER_SIZE)
    {
      size_t crcpos = pvz + dwg->header_vars.size + 4;
      bit_set_position (dat, crcpos * 8);
      crc = bit_read_RS (dat);
      if (dwg->header.section[SECTION_HEADER_R13].size > 34
          && dwg->header.section[SECTION_HEADER_R13].size < 0xfff
          && pvz < dat->byte
          && pvz + dwg->header.section[SECTION_HEADER_R13].size < dat->size)
        {
          BITCODE_RL crc_size
              = dwg->header.section[SECTION_HEADER_R13].size - 34;
          crc2 = bit_calc_CRC (0xC0C1, &dat->chain[pvz], crc_size);
          if (crc != crc2)
            error |= DWG_ERR_WRONGCRC;
        }
    }
  else
    error |= DWG_ERR_SECTIONNOTFOUND;

classes_section:
  dat->byte = dwg->header.section[SECTION_CLASSES_R13].address;
  if (dwg->header.section[SECTION_CLASSES_R13].size < 30)
    {
      error |= DWG_ERR_SECTIONNOTFOUND;
      goto handles_section;
    }
  if (memcmp (dwg_sentinel (DWG_SENTINEL_CLASS_BEGIN), &dat->chain[dat->byte],
              16)
      == 0)
    dat->byte += 16;
  else
    sentinel_size = 0;
  dat->bit = 0;
  size = bit_read_RL (dat);
  if (size
      != dwg->header.section[SECTION_CLASSES_R13].size
             - ((sentinel_size * 2) + 6))
    {
      error |= DWG_ERR_SECTIONNOTFOUND;
      goto handles_section;
    }
  endpos = dat->byte + size;
  dwg->layout_type = 0;
  dwg->num_classes = 0;

  while (dat->byte < endpos - 1)
    {
      BITCODE_BS i = dwg->num_classes;
      Dwg_Class *klass;
      if ((size_t)i >= 100 + (size / sizeof (Dwg_Class)) || i >= 65535)
        {
          error |= DWG_ERR_VALUEOUTOFBOUNDS;
          goto handles_section;
        }
      if (i == 0)
        dwg->dwg_class = (Dwg_Class *)malloc (sizeof (Dwg_Class));
      else
        dwg->dwg_class = (Dwg_Class *)realloc (dwg->dwg_class,
                                               (i + 1) * sizeof (Dwg_Class));
      if (!dwg->dwg_class)
        return DWG_ERR_OUTOFMEM;
      klass = &dwg->dwg_class[i];
      memset (klass, 0, sizeof (Dwg_Class));
      klass->number = bit_read_BS (dat);
      klass->proxyflag = bit_read_BS (dat);
      if (dat->byte >= endpos)
        break;
      klass->appname = bit_read_TV (dat);
      if (dat->byte >= endpos)
        {
          free (klass->appname);
          break;
        }
      klass->cppname = bit_read_TV (dat);
      klass->dxfname = bit_read_TV (dat);
      klass->is_zombie = bit_read_B (dat);
      klass->item_class_id = bit_read_BS (dat);
      if (klass->dxfname && strEQc ((const char *)klass->dxfname, "LAYOUT"))
        dwg->layout_type = klass->number;
      dwg->num_classes++;
    }

handles_section:
  return read_r13_to_r2002_handle_map (dat, dwg, callbacks,
                                                input_mode, user);
}
