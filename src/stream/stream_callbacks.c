/* Stream reader decoded-object callback handling. */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "bits.h"
#include "decode.h"
#include "free.h"
#include "hash.h"
#include "stream_reader_internal.h"

/* Match the graph-independent object fixups which dwg_read_file applies after
   decoding.  The Stream path can preserve the same VIEWPORT sequence with one
   counter and does not need to materialize the object graph. */
void
dwg_stream_fixup_decoded_object (Dwg_Data *restrict dwg,
                                 Dwg_Object *restrict obj)
{
  Dwg_Entity_VIEWPORT *viewport;

  if (!dwg || !obj || obj->supertype != DWG_SUPERTYPE_ENTITY
      || obj->fixedtype != DWG_TYPE_VIEWPORT || !obj->tio.entity
      || !obj->tio.entity->tio.VIEWPORT)
    return;
  viewport = obj->tio.entity->tio.VIEWPORT;
  if (obj->tio.entity->entmode == 0)
    {
      viewport->on_off = 0;
      viewport->id = 0;
      dwg->last_viewport_id = 0;
    }
  else
    {
      viewport->on_off = 1;
      dwg->last_viewport_id++;
      viewport->id = dwg->last_viewport_id;
    }
}

int
dwg_stream_emit_decoded_object_ex (
    Dwg_Data *restrict dwg, Bit_Chain *restrict object_dat,
    const Dwg_Stream_Object_Info *restrict info,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks, void *restrict user,
    int *restrict callback_error)
{
  Dwg_Data stream_dwg;
  Dwg_Object stream_object;
  dwg_inthash stream_object_map;
  struct _hashbucket stream_object_buckets[32];
  int error;
  int emitted_error = 0;
  BITCODE_BL i;

  if (callback_error)
    *callback_error = 0;
  if (!dwg || !object_dat || !info || !callbacks || !callbacks->decoded_object)
    return 0;

  memset (&stream_object, 0, sizeof (stream_object));
  memset (&stream_dwg, 0, sizeof (stream_dwg));
  stream_dwg.header = dwg->header;
  stream_dwg.num_classes = dwg->num_classes;
  stream_dwg.dwg_class = dwg->dwg_class;
  stream_dwg.opts = dwg->opts | DWG_OPTS_STREAM_DECODE;
  stream_dwg.header_vars = dwg->header_vars;
  /* File dependency metadata is owned by the parent stream DWG and remains
     valid for the callback.  Expose it read-only with the temporary object. */
  stream_dwg.filedeplist = dwg->filedeplist;
  stream_dwg.layout_type = dwg->layout_type;
  stream_dwg.num_acis_sab_hdl = 0;
  stream_dwg.acis_sab_hdl = NULL;
  stream_dwg.num_objects = 0;
  stream_dwg.num_object_refs = 0;
  stream_dwg.num_alloced_objects = 1;
  stream_dwg.object_ref = NULL;
  stream_dwg.object = &stream_object;
  memset (stream_object_buckets, 0, sizeof (stream_object_buckets));
  stream_object_map.array = stream_object_buckets;
  stream_object_map.size
      = sizeof (stream_object_buckets) / sizeof (stream_object_buckets[0]);
  stream_object_map.elems = 0;
  stream_dwg.object_map = &stream_object_map;
  stream_dwg.dirty_refs = 0;
  stream_dwg.num_object_ordered_refs = (BITCODE_BL)-1;

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
      dwg_stream_fixup_decoded_object (dwg, obj);
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
  /* These are non-global refs from dwg_add_handleref_free.  Neither the
     decoded object nor object_ref owns them. */
  for (i = 0; i < stream_dwg.num_acis_sab_hdl; i++)
    {
      free (stream_dwg.acis_sab_hdl[i]);
      stream_dwg.acis_sab_hdl[i] = NULL;
    }
  free (stream_dwg.acis_sab_hdl);
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
