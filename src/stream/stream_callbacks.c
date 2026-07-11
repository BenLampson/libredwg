/* Stream reader decoded-object callback handling. */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "bits.h"
#include "decode.h"
#include "free.h"
#include "hash.h"

int
dwg_stream_emit_decoded_object_ex (
    Dwg_Data *restrict dwg, Bit_Chain *restrict object_dat,
    const Dwg_Stream_Object_Info *restrict info,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks, void *restrict user,
    int *restrict callback_error)
{
  Dwg_Data stream_dwg;
  Dwg_Object stream_object;
  int error;
  int emitted_error = 0;
  BITCODE_BL i;

  if (callback_error)
    *callback_error = 0;
  if (!dwg || !object_dat || !info || !callbacks || !callbacks->decoded_object)
    return 0;

  memset (&stream_object, 0, sizeof (stream_object));
  stream_dwg = *dwg;
  stream_dwg.num_objects = 0;
  stream_dwg.num_object_refs = 0;
  stream_dwg.num_alloced_objects = 1;
  stream_dwg.object_ref = NULL;
  stream_dwg.object = &stream_object;
  stream_dwg.object_map = hash_new (1024);
  stream_dwg.header_vars.HANDSEED = NULL;
  stream_dwg.dirty_refs = 0;
  if (!stream_dwg.object_map)
    return DWG_ERR_OUTOFMEM;

  object_dat->byte = 0;
  object_dat->bit = 0;
  object_dat->version = stream_dwg.header.version;
  object_dat->from_version = stream_dwg.header.from_version;

  error = dwg_decode_add_object (&stream_dwg, object_dat, object_dat, 0);
  if (error >= DWG_ERR_CRITICAL)
    goto done;
  if (error < DWG_ERR_CRITICAL && stream_dwg.num_objects)
    {
      Dwg_Object *obj = &stream_dwg.object[0];
      emitted_error = callbacks->decoded_object (info, obj, user);
    }
  else if (error < DWG_ERR_CRITICAL && callbacks->decode_error)
    emitted_error = callbacks->decode_error (
        info, error ? error : DWG_ERR_INTERNALERROR, user);

done:
  for (i = 0; i < stream_dwg.num_objects; i++)
    {
      dwg_free_object (&stream_dwg.object[i]);
      memset (&stream_dwg.object[i], 0, sizeof (stream_dwg.object[i]));
    }
  for (i = 0; i < stream_dwg.num_object_refs; i++)
    {
      free (stream_dwg.object_ref[i]);
      stream_dwg.object_ref[i] = NULL;
    }
  free (stream_dwg.object_ref);
  hash_free (stream_dwg.object_map);
  if (emitted_error)
    {
      if (callback_error)
        *callback_error = emitted_error;
      return emitted_error;
    }
  return error >= DWG_ERR_CRITICAL ? error : 0;
}

EXPORT int
dwg_stream_emit_decoded_object (
    Dwg_Data *restrict dwg, Bit_Chain *restrict object_dat,
    const Dwg_Stream_Object_Info *restrict info,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks, void *restrict user)
{
  return dwg_stream_emit_decoded_object_ex (dwg, object_dat, info, callbacks,
                                            user, NULL);
}
