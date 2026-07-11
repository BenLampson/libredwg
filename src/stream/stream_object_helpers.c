/* Object-record helpers shared by Stream format readers. */

#include "config.h"
#include <stdint.h>
#include "stream_object_helpers.h"

int
dwg_stream_fixed_type_is_entity (const Dwg_Object_Type type)
{
  switch (type)
    {
    case DWG_TYPE_TEXT:
    case DWG_TYPE_ATTRIB:
    case DWG_TYPE_ATTDEF:
    case DWG_TYPE_BLOCK:
    case DWG_TYPE_ENDBLK:
    case DWG_TYPE_SEQEND:
    case DWG_TYPE_INSERT:
    case DWG_TYPE_MINSERT:
    case DWG_TYPE_VERTEX_2D:
    case DWG_TYPE_VERTEX_3D:
    case DWG_TYPE_VERTEX_MESH:
    case DWG_TYPE_VERTEX_PFACE:
    case DWG_TYPE_VERTEX_PFACE_FACE:
    case DWG_TYPE_POLYLINE_2D:
    case DWG_TYPE_POLYLINE_3D:
    case DWG_TYPE_ARC:
    case DWG_TYPE_CIRCLE:
    case DWG_TYPE_LINE:
    case DWG_TYPE_DIMENSION_ORDINATE:
    case DWG_TYPE_DIMENSION_LINEAR:
    case DWG_TYPE_DIMENSION_ALIGNED:
    case DWG_TYPE_DIMENSION_ANG3PT:
    case DWG_TYPE_DIMENSION_ANG2LN:
    case DWG_TYPE_DIMENSION_RADIUS:
    case DWG_TYPE_DIMENSION_DIAMETER:
    case DWG_TYPE_POINT:
    case DWG_TYPE__3DFACE:
    case DWG_TYPE_POLYLINE_PFACE:
    case DWG_TYPE_POLYLINE_MESH:
    case DWG_TYPE_SOLID:
    case DWG_TYPE_TRACE:
    case DWG_TYPE_SHAPE:
    case DWG_TYPE_VIEWPORT:
    case DWG_TYPE_ELLIPSE:
    case DWG_TYPE_SPLINE:
    case DWG_TYPE_REGION:
    case DWG_TYPE__3DSOLID:
    case DWG_TYPE_BODY:
    case DWG_TYPE_RAY:
    case DWG_TYPE_XLINE:
    case DWG_TYPE_OLEFRAME:
    case DWG_TYPE_MTEXT:
    case DWG_TYPE_LEADER:
    case DWG_TYPE_TOLERANCE:
    case DWG_TYPE_MLINE:
    case DWG_TYPE_OLE2FRAME:
    case DWG_TYPE_LWPOLYLINE:
    case DWG_TYPE_HATCH:
    case DWG_TYPE_PROXY_ENTITY:
      return 1;
    default:
      return 0;
    }
}

int
dwg_stream_object_span_size (const Dwg_Stream_Object_Info *restrict info,
                             const size_t object_start,
                             const size_t total_size,
                             size_t *restrict object_size)
{
  size_t prefix_size;

  if (!info || !object_size || info->address < object_start)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  prefix_size = info->address - object_start;
  if ((size_t)info->size > (size_t)-1 - prefix_size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  *object_size = prefix_size + info->size;

  if (total_size)
    {
      if (object_start > total_size || (size_t)info->size > total_size
          || info->address > total_size - info->size
          || *object_size > total_size - object_start)
        return DWG_ERR_VALUEOUTOFBOUNDS;
    }
  return 0;
}

int
dwg_stream_add_handle_offset (size_t *restrict last_offset,
                              const BITCODE_MC offset)
{
  size_t delta;

  if (!last_offset)
    return DWG_ERR_INTERNALERROR;

  if (offset < 0)
    {
      delta = (size_t)(-(int64_t)offset);
      if (delta > *last_offset)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      *last_offset -= delta;
      return 0;
    }

  delta = (size_t)offset;
  if (*last_offset > (size_t)-1 - delta)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  *last_offset += delta;
  return 0;
}

int
dwg_stream_add_handle_value (BITCODE_RLL *restrict last_handle,
                             const BITCODE_UMC handleoff,
                             const BITCODE_RLL max_handles)
{
  (void)max_handles;

  if (!last_handle)
    return DWG_ERR_INTERNALERROR;
  if (*last_handle > (BITCODE_RLL)-1 - handleoff)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  *last_handle += handleoff;
  return 0;
}

int
dwg_stream_object_record_size (const Dwg_Stream_Object_Info *restrict info,
                               const size_t object_start,
                               const size_t total_size,
                               size_t *restrict object_size)
{
  int error
      = dwg_stream_object_span_size (info, object_start, total_size, object_size);
  if (error)
    return error;

  if (*object_size > (size_t)-1 - 2)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  if (total_size && *object_size + 2 > total_size - object_start)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  *object_size += 2;
  return 0;
}
