/* Stream reader entry point and version dispatch. */

#define _DEFAULT_SOURCE 1
#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IS_DECODER
#include "common.h"
#include "bits.h"
#include "dwg.h"
#include "decode.h"
#include "free.h"
#include "stream_reader_internal.h"
#include "stream_version_policy.h"

static int cur_ver = 0;
static BITCODE_BL rcount1 = 0, rcount2 = 0;
static bool is_teigha = false;

#ifdef USE_TRACING
static bool env_var_checked_p;
#endif
#define DWG_LOGLEVEL loglevel
#include "logging.h"
#include "dec_macros.h"

int
dwg_decode_stream (Bit_Chain *restrict dat,
                   const Dwg_Stream_Callbacks_Ex *restrict callbacks,
                   Dwg_Stream_Input_Mode input_mode,
                   int *restrict stream_supported, void *restrict user)
{
  char magic[11];
  Dwg_Data dwg = { 0 };
  Bit_Chain r2007_hdl_dat;
  int error = 0;

  if (!callbacks || !dat || !dat->chain || dat->size < 58)
    return DWG_ERR_INVALIDDWG;
  if (stream_supported)
    *stream_supported = 0;

  dat->byte = 0;
  dat->bit = 0;
  strncpy (magic, (const char *)dat->chain, 11);
  magic[10] = '\0';

  dwg.header.from_version = dwg_version_hdr_type (magic);
  if (!dwg.header.from_version)
    return DWG_ERR_INVALIDDWG;

  dat->from_version = dwg.header.from_version;
  dat->version = dwg.header.version = dat->from_version;
  dat->byte = 0xb;

#ifdef USE_TRACING
  if (!env_var_checked_p)
    {
      char *probe = getenv ("LIBREDWG_TRACE");
      if (probe)
        loglevel = atoi (probe);
      env_var_checked_p = true;
    }
#endif /* USE_TRACING */

  if (dwg.header.from_version >= R_13b1 && dwg.header.from_version <= R_2002)
    {
      if (stream_supported)
        *stream_supported = 1;
      {
        Dwg_Header *_obj = &dwg.header;
        Bit_Chain *hdl_dat = dat;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }
      dwg_refine_from_version (dat, &dwg);
      error = dwg_stream_reject_unsupported_version (
          dwg.header.from_version, stream_supported);
      if (error)
        {
          dwg_free (&dwg);
          return error;
        }

      error = dwg_stream_read_r13_to_r2002 (dat, &dwg, callbacks, input_mode,
                                            user);
      dwg_free (&dwg);
      return error;
    }

  if (dwg.header.from_version >= R_2004a && dwg.header.from_version <= R_2004)
    {
      if (stream_supported)
        *stream_supported = 1;
      {
        Dwg_Header *_obj = &dwg.header;
        Dwg_Object *obj = NULL;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }
      dwg_refine_from_version (dat, &dwg);
      error = dwg_stream_reject_unsupported_version (
          dwg.header.from_version, stream_supported);
      if (error)
        {
          dwg_free (&dwg);
          return error;
        }

      error = dwg_stream_read_r2004_to_r2006_and_r2010_to_r2022 (
          dat, &dwg, callbacks, input_mode, user);
      dwg_free (&dwg);
      return error;
    }

  if (dwg.header.from_version >= R_2010b && dwg.header.from_version <= R_2022b)
    {
      if (stream_supported)
        *stream_supported = 1;
      read_r2007_init (&dwg);
      {
        Dwg_Header *_obj = &dwg.header;
        Dwg_Object *obj = NULL;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }
      dwg_refine_from_version (dat, &dwg);
      error = dwg_stream_reject_unsupported_version (
          dwg.header.from_version, stream_supported);
      if (error)
        {
          dwg_free (&dwg);
          return error;
        }

      error = dwg_stream_read_r2004_to_r2006_and_r2010_to_r2022 (
          dat, &dwg, callbacks, input_mode, user);
      dwg_free (&dwg);
      return error;
    }

  if (dwg.header.from_version >= R_2007a && dwg.header.from_version <= R_2007)
    {
      if (stream_supported)
        *stream_supported = 1;
      r2007_hdl_dat = *dat;
      {
        Dwg_Header *_obj = &dwg.header;
        Dwg_Object *obj = NULL;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }
      dwg_refine_from_version (dat, &dwg);
      error = dwg_stream_reject_unsupported_version (
          dwg.header.from_version, stream_supported);
      if (error)
        {
          dwg_free (&dwg);
          return error;
        }
      r2007_hdl_dat.from_version = dat->from_version;

      error = read_r2007_meta_data_stream (dat, &r2007_hdl_dat, &dwg,
                                           callbacks, input_mode, user);
      dwg_free (&dwg);
      return error;
    }

  if (dwg.header.from_version >= R_1_1 && dwg.header.from_version <= R_1_4)
    {
      if (stream_supported)
        *stream_supported = 1;
      {
        Dwg_Header *_obj = &dwg.header;
        Bit_Chain *hdl_dat = dat;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }

      error = dwg_stream_read_r1_to_r2 (dat, &dwg, callbacks, input_mode,
                                        user);
      dwg_free (&dwg);
      return error;
    }

  if (dwg.header.from_version >= R_2_0 && dwg.header.from_version <= R_11b2)
    {
      if (stream_supported)
        *stream_supported = 1;
      {
        Dwg_Header *_obj = &dwg.header;
        Bit_Chain *hdl_dat = dat;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }

      if (dwg.header.from_version == R_2_0 && dwg.header.numheader_vars <= 74)
        {
          dwg.header.from_version = dat->from_version = R_2_0b;
          if (dwg.header.version == R_2_0)
            dwg.header.version = R_2_0b;
          if (dat->version == R_2_0)
            dat->version = R_2_0b;
        }
      error = dwg_stream_read_r2_to_r10 (dat, &dwg, callbacks, input_mode,
                                         user);
      dwg_free (&dwg);
      return error;
    }

  if (dwg.header.from_version == R_11)
    {
      if (stream_supported)
        *stream_supported = 1;
      {
        Dwg_Header *_obj = &dwg.header;
        Bit_Chain *hdl_dat = dat;
        int i;
        BITCODE_BL vcount;

        // clang-format off
        #include "header.spec"
        // clang-format on
      }

      error = dwg_stream_read_r11 (dat, &dwg, callbacks, input_mode, user);
      dwg_free (&dwg);
      return error;
    }

  return DWG_ERR_NOTYETSUPPORTED;
}
