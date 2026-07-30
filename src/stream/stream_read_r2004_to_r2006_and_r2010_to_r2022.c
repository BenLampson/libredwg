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
#define R2004_STREAM_PAGE_CACHE_SLOTS 16U
#define R2004_STREAM_BUFFERED_MAX_BYTES (64U * 1024U * 1024U)
#define R2004_STREAM_INVALID_PAGE ((BITCODE_BL)-1)

typedef struct _r2004_object_stream
{
  Bit_Chain *dat;
  Dwg_Section_Info *info;
  BITCODE_RC history[R2004_STREAM_HISTORY_SIZE];
  BITCODE_BL next_section;
  size_t output_pos;
  int initialized;
} R2004_Object_Stream;

typedef struct _r2004_stream_page_descriptor
{
  size_t logical_start;
  size_t logical_size;
  size_t payload_offset;
  uint32_t data_size;
  BITCODE_BL cache_slot;
} R2004_Stream_Page_Descriptor;

typedef struct _r2004_stream_page_cache_entry
{
  Bit_Chain page;
  BITCODE_BL descriptor_index;
  uint64_t last_used;
} R2004_Stream_Page_Cache_Entry;

typedef struct _r2004_stream_page_cache
{
  Bit_Chain *dat;
  Dwg_Section_Info *info;
  R2004_Stream_Page_Descriptor *descriptors;
  BITCODE_BL num_descriptors;
  BITCODE_BL num_cache_entries;
  R2004_Stream_Page_Cache_Entry entries[R2004_STREAM_PAGE_CACHE_SLOTS];
  R2004_Stream_Page_Cache_Entry *active;
  uint64_t clock;
} R2004_Stream_Page_Cache;

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
r2004_stream_scan_extended_length (const BITCODE_RC *restrict data,
                                   const size_t data_size,
                                   size_t *restrict position,
                                   const unsigned char opcode,
                                   const unsigned int mask, const size_t extra,
                                   size_t *restrict length)
{
  size_t value = opcode & mask;
  BITCODE_RC lastbyte;

  if (!value)
    {
      lastbyte = 0;
      while (*position < data_size && data[*position] == 0)
        {
          if (value > SIZE_MAX - 0xffU)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          value += 0xffU;
          (*position)++;
        }
      if (*position >= data_size)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      lastbyte = data[(*position)++];
      if (value > SIZE_MAX - (size_t)lastbyte - extra)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      value += (size_t)lastbyte + extra;
    }
  if (value > SIZE_MAX - 2U)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  *length = value + 2U;
  return 0;
}

static int
r2004_stream_scan_literal_length (const BITCODE_RC *restrict data,
                                  const size_t data_size,
                                  size_t *restrict position,
                                  const unsigned char opcode,
                                  size_t *restrict length)
{
  size_t encoded;
  int error;

  error = r2004_stream_scan_extended_length (data, data_size, position, opcode,
                                             0xfU, 0xfU, &encoded);
  if (error)
    return error;
  if (encoded > SIZE_MAX - 1U)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  *length = encoded + 1U;
  return 0;
}

/* Validate one compressed page without materializing it.  The R2004 LZ
   offsets are page-local for valid data pages; rejecting an offset before the
   beginning of this page lets the random-access backend fall back before the
   first user callback. */
static int
r2004_stream_preflight_compressed_page (const BITCODE_RC *restrict data,
                                        const size_t data_size,
                                        const size_t output_limit)
{
  size_t position = 0;
  size_t produced = 0;
  size_t literal_length;
  unsigned char opcode;
  int error;

  if (!data || !data_size || !output_limit)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  opcode = data[position++];
  if ((opcode & 0xf0U) == 0)
    {
      error = r2004_stream_scan_literal_length (data, data_size, &position,
                                                opcode, &literal_length);
      if (error || literal_length > output_limit
          || literal_length >= data_size - position)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      produced = literal_length;
      position += literal_length;
      opcode = data[position++];
    }

  while (position < data_size && produced < output_limit && opcode != 0x11U)
    {
      size_t compressed_bytes;
      size_t compressed_offset;
      size_t first_byte;
      size_t second_byte;

      compressed_bytes = 0;
      compressed_offset = 0;
      if (opcode >= 0x40U)
        {
          if (position >= data_size)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          compressed_bytes = (opcode >> 4) - 1U;
          compressed_offset
              = (((opcode >> 2) & 3U) | ((size_t)data[position++] << 2)) + 1U;
        }
      else if (opcode >= 0x10U && opcode < 0x20U)
        {
          error = r2004_stream_scan_extended_length (
              data, data_size, &position, opcode, 7U, 7U, &compressed_bytes);
          if (error || data_size - position < 2U)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          first_byte = data[position++];
          second_byte = data[position++];
          compressed_offset = ((size_t)(opcode & 8U) << 11) + (first_byte >> 2)
                              + (second_byte << 6) + 0x4000U;
          opcode = (unsigned char)first_byte;
        }
      else if (opcode >= 0x20U)
        {
          error = r2004_stream_scan_extended_length (data, data_size,
                                                     &position, opcode, 0x1fU,
                                                     0x1fU, &compressed_bytes);
          if (error || data_size - position < 2U)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          first_byte = data[position++];
          second_byte = data[position++];
          compressed_offset = (first_byte >> 2) + (second_byte << 6) + 1U;
          opcode = (unsigned char)first_byte;
        }
      else
        return DWG_ERR_VALUEOUTOFBOUNDS;

      if (!compressed_bytes || !compressed_offset
          || compressed_offset > produced
          || compressed_bytes > output_limit - produced)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      produced += compressed_bytes;

      literal_length = opcode & 3U;
      if (!literal_length)
        {
          if (position >= data_size)
            return 0;
          opcode = data[position++];
          if ((opcode & 0xf0U) == 0)
            {
              error = r2004_stream_scan_literal_length (
                  data, data_size, &position, opcode, &literal_length);
              if (error)
                return error;
            }
        }
      if (literal_length)
        {
          if (literal_length > output_limit - produced
              || literal_length >= data_size - position)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          produced += literal_length;
          position += literal_length;
          opcode = data[position++];
        }
    }
  return 0;
}

static void
r2004_stream_page_cache_free (R2004_Stream_Page_Cache *restrict cache)
{
  size_t i;

  for (i = 0; i < cache->num_cache_entries; i++)
    free (cache->entries[i].page.chain);
  free (cache->descriptors);
  memset (cache, 0, sizeof (*cache));
}

static int
r2004_stream_page_cache_init (R2004_Stream_Page_Cache *restrict cache,
                              Bit_Chain *restrict dat,
                              Dwg_Section_Info *restrict info)
{
  uint64_t max_decomp_size;
  size_t total_size;
  size_t previous_end = 0;
  size_t allocation_size;
  BITCODE_BL i;

  memset (cache, 0, sizeof (*cache));
  if (!dat || !info || !info->sections || !info->num_sections
      || !info->max_decomp_size || info->size <= 0
      || (uint64_t)info->size != (uint64_t)(size_t)info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  max_decomp_size
      = (uint64_t)info->num_sections * (uint64_t)info->max_decomp_size;
  if (!max_decomp_size || max_decomp_size > dwg_get_max_r2004_decomp_size ()
      || (uint64_t)info->size > max_decomp_size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  total_size = (size_t)info->size;
  allocation_size = (size_t)info->num_sections * sizeof (*cache->descriptors);
  if (allocation_size / (size_t)info->num_sections
      != sizeof (*cache->descriptors))
    return DWG_ERR_OUTOFMEM;
  cache->descriptors
      = (R2004_Stream_Page_Descriptor *)calloc (1, allocation_size);
  if (!cache->descriptors)
    return DWG_ERR_OUTOFMEM;
  cache->dat = dat;
  cache->info = info;

  for (i = 0; i < info->num_sections; i++)
    {
      Dwg_Section *section = info->sections[i];
      encrypted_section_header es;
      R2004_Stream_Page_Descriptor *descriptor;
      size_t physical_address;
      size_t payload_offset;
      size_t remaining;
      size_t logical_size;
      uint32_t sec_mask;
      int error;

      if (!section)
        {
          if (i == info->num_sections - 1)
            {
              r2004_stream_page_cache_free (cache);
              return DWG_ERR_SECTIONNOTFOUND;
            }
          continue;
        }
      if (section->address != (BITCODE_RLL)(size_t)section->address
          || section->address > UINT32_MAX)
        {
          r2004_stream_page_cache_free (cache);
          return DWG_ERR_VALUEOUTOFBOUNDS;
        }
      physical_address = (size_t)section->address;
      if (physical_address > dat->size
          || dat->size - physical_address < sizeof (es))
        {
          r2004_stream_page_cache_free (cache);
          return DWG_ERR_VALUEOUTOFBOUNDS;
        }
      payload_offset = physical_address + sizeof (es);
      memcpy (es.long_data, &dat->chain[physical_address], sizeof (es));
      sec_mask = htole32 (0x4164536bU ^ (uint32_t)physical_address);
      for (int k = 0; k < 8; k++)
        es.long_data[k] = le32toh (es.long_data[k] ^ sec_mask);

      if (es.fields.page_type != 0x4163043bU || es.fields.address >= total_size
          || (info->compressed != 2
              && (!es.fields.page_size
                  || es.fields.page_size > info->max_decomp_size))
          || (size_t)es.fields.data_size > dat->size - payload_offset)
        {
          r2004_stream_page_cache_free (cache);
          return DWG_ERR_VALUEOUTOFBOUNDS;
        }
      remaining = total_size - es.fields.address;
      logical_size = info->compressed == 2
                         ? MIN ((size_t)info->max_decomp_size, remaining)
                         : MIN ((size_t)es.fields.page_size, remaining);
      if (!logical_size || es.fields.address < previous_end)
        {
          r2004_stream_page_cache_free (cache);
          return DWG_ERR_VALUEOUTOFBOUNDS;
        }
      if (info->compressed == 2)
        {
          error = r2004_stream_preflight_compressed_page (
              &dat->chain[payload_offset], es.fields.data_size,
              info->max_decomp_size);
          if (error)
            {
              r2004_stream_page_cache_free (cache);
              return error;
            }
        }
      else if (logical_size > dat->size - payload_offset)
        {
          r2004_stream_page_cache_free (cache);
          return DWG_ERR_VALUEOUTOFBOUNDS;
        }

      descriptor = &cache->descriptors[cache->num_descriptors++];
      descriptor->logical_start = es.fields.address;
      descriptor->logical_size = logical_size;
      descriptor->payload_offset = payload_offset;
      descriptor->data_size = es.fields.data_size;
      descriptor->cache_slot = R2004_STREAM_INVALID_PAGE;
      previous_end = descriptor->logical_start + descriptor->logical_size;
    }
  if (!cache->num_descriptors)
    {
      r2004_stream_page_cache_free (cache);
      return DWG_ERR_VALUEOUTOFBOUNDS;
    }

  cache->num_cache_entries
      = MIN (cache->num_descriptors, R2004_STREAM_PAGE_CACHE_SLOTS);
  for (i = 0; i < cache->num_cache_entries; i++)
    {
      R2004_Stream_Page_Cache_Entry *entry = &cache->entries[i];

      entry->page.chain = (BITCODE_RC *)calloc (info->max_decomp_size, 1);
      if (!entry->page.chain)
        {
          r2004_stream_page_cache_free (cache);
          return DWG_ERR_OUTOFMEM;
        }
      entry->page.version = dat->version;
      entry->page.from_version = dat->from_version;
      entry->page.opts = dat->opts;
      entry->page.codepage = dat->codepage;
      entry->descriptor_index = R2004_STREAM_INVALID_PAGE;
    }
  return 0;
}

static BITCODE_BL
r2004_stream_page_cache_find_descriptor (
    const R2004_Stream_Page_Cache *restrict cache, const size_t address,
    size_t *restrict next_start)
{
  BITCODE_BL low = 0;
  BITCODE_BL high = cache->num_descriptors;

  if (cache->active
      && cache->active->descriptor_index < cache->num_descriptors)
    {
      const R2004_Stream_Page_Descriptor *active_descriptor
          = &cache->descriptors[cache->active->descriptor_index];

      if (address >= active_descriptor->logical_start
          && address - active_descriptor->logical_start
                 < active_descriptor->logical_size)
        return cache->active->descriptor_index;
    }
  while (low < high)
    {
      BITCODE_BL middle = low + (high - low) / 2;

      if (cache->descriptors[middle].logical_start <= address)
        low = middle + 1;
      else
        high = middle;
    }
  if (low)
    {
      const R2004_Stream_Page_Descriptor *descriptor
          = &cache->descriptors[low - 1];

      if (address - descriptor->logical_start < descriptor->logical_size)
        return low - 1;
    }
  if (next_start)
    *next_start = low < cache->num_descriptors
                      ? cache->descriptors[low].logical_start
                      : (size_t)cache->info->size;
  return R2004_STREAM_INVALID_PAGE;
}

static int
r2004_stream_page_cache_load (R2004_Stream_Page_Cache *restrict cache,
                              const BITCODE_BL descriptor_index)
{
  R2004_Stream_Page_Cache_Entry *entry = NULL;
  R2004_Stream_Page_Descriptor *descriptor;
  size_t i;

  if (descriptor_index >= cache->num_descriptors)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  descriptor = &cache->descriptors[descriptor_index];
  if (descriptor->cache_slot < cache->num_cache_entries)
    {
      entry = &cache->entries[descriptor->cache_slot];
      if (entry->descriptor_index != descriptor_index)
        return DWG_ERR_INTERNALERROR;
      entry->last_used = ++cache->clock;
      cache->active = entry;
      return 0;
    }
  for (i = 0; i < cache->num_cache_entries; i++)
    {
      R2004_Stream_Page_Cache_Entry *candidate = &cache->entries[i];

      if (candidate->descriptor_index == R2004_STREAM_INVALID_PAGE)
        entry = candidate;
      else if (!entry || candidate->last_used < entry->last_used)
        entry = candidate;
    }
  if (!entry)
    return DWG_ERR_INTERNALERROR;

  if (entry->descriptor_index < cache->num_descriptors)
    cache->descriptors[entry->descriptor_index].cache_slot
        = R2004_STREAM_INVALID_PAGE;
  if (cache->active == entry)
    cache->active = NULL;
  entry->descriptor_index = R2004_STREAM_INVALID_PAGE;
  memset (entry->page.chain, 0, cache->info->max_decomp_size);
  entry->page.byte = 0;
  entry->page.bit = 0;
  entry->page.size = cache->info->max_decomp_size;
  if (cache->info->compressed == 2)
    {
      Bit_Chain src = *cache->dat;
      BITCODE_RC *page_chain = entry->page.chain;
      int error;

      src.byte = descriptor->payload_offset;
      src.bit = 0;
      src.size = src.byte + descriptor->data_size;
      error = decompress_R2004_section (&src, &entry->page);
      if (error)
        return error;
      if (entry->page.chain != page_chain
          || entry->page.size != cache->info->max_decomp_size)
        {
          LOG_WARN ("R2004 page cache decompressor grew page %u: %" PRIuSIZE
                    " -> %" PRIuSIZE,
                    descriptor_index, (size_t)cache->info->max_decomp_size,
                    entry->page.size);
          return DWG_ERR_VALUEOUTOFBOUNDS;
        }
    }
  else
    memcpy (entry->page.chain, &cache->dat->chain[descriptor->payload_offset],
            descriptor->logical_size);

  entry->page.byte = 0;
  entry->page.bit = 0;
  entry->page.size = descriptor->logical_size;
  entry->descriptor_index = descriptor_index;
  descriptor->cache_slot = (BITCODE_BL)(entry - cache->entries);
  entry->last_used = ++cache->clock;
  cache->active = entry;
  return 0;
}

static int
r2004_stream_page_cache_read (R2004_Stream_Page_Cache *restrict cache,
                              const size_t address,
                              BITCODE_RC *restrict buffer, const size_t size)
{
  size_t copied = 0;

  if (!buffer || !size || size > (size_t)cache->info->size
      || address > (size_t)cache->info->size - size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  memset (buffer, 0, size);

  while (copied < size)
    {
      size_t position = address + copied;
      size_t next_start = (size_t)cache->info->size;
      BITCODE_BL descriptor_index = r2004_stream_page_cache_find_descriptor (
          cache, position, &next_start);

      if (descriptor_index == R2004_STREAM_INVALID_PAGE)
        {
          size_t gap = MIN (next_start - position, size - copied);

          if (!gap)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          copied += gap;
        }
      else
        {
          const R2004_Stream_Page_Descriptor *descriptor
              = &cache->descriptors[descriptor_index];
          size_t page_offset = position - descriptor->logical_start;
          size_t available = descriptor->logical_size - page_offset;
          size_t take = MIN (available, size - copied);
          int error = r2004_stream_page_cache_load (cache, descriptor_index);

          if (error)
            return error;
          memcpy (&buffer[copied], &cache->active->page.chain[page_offset],
                  take);
          copied += take;
        }
    }
  return 0;
}

static int
r2004_stream_page_cache_borrow (R2004_Stream_Page_Cache *restrict cache,
                                const size_t address, const size_t size,
                                Bit_Chain *restrict window,
                                bool *restrict borrowed)
{
  BITCODE_BL descriptor_index;
  const R2004_Stream_Page_Descriptor *descriptor;
  size_t page_offset;
  int error;

  *borrowed = false;
  if (!size || size > (size_t)cache->info->size
      || address > (size_t)cache->info->size - size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  descriptor_index
      = r2004_stream_page_cache_find_descriptor (cache, address, NULL);
  if (descriptor_index == R2004_STREAM_INVALID_PAGE)
    return 0;
  descriptor = &cache->descriptors[descriptor_index];
  page_offset = address - descriptor->logical_start;
  if (size > descriptor->logical_size - page_offset)
    return 0;

  error = r2004_stream_page_cache_load (cache, descriptor_index);
  if (error)
    return error;
  *window = cache->active->page;
  window->chain = &cache->active->page.chain[page_offset];
  window->byte = 0;
  window->bit = 0;
  window->size = size;
  *borrowed = true;
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
      size_t address, payload_offset;
      uint32_t sec_mask;
      int error = 0;

      if (stream->next_section >= stream->info->num_sections)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      section = stream->info->sections[stream->next_section++];
      if (!section)
        continue;

      if (section->address != (BITCODE_RLL)(size_t)section->address)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      address = (size_t)section->address;
      if (address > stream->dat->size || stream->dat->size - address < 32)
        return DWG_ERR_VALUEOUTOFBOUNDS;
      payload_offset = address + 32;

      memcpy (es.long_data, &stream->dat->chain[address], 32);
      sec_mask = htole32 (0x4164536bU ^ (uint32_t)address);
      for (int k = 0; k < 8; ++k)
        es.long_data[k] = le32toh (es.long_data[k] ^ sec_mask);

      if (es.fields.page_type != 0x4163043b
          || (uint64_t)es.fields.address > (uint64_t)stream->info->size
          || (size_t)es.fields.data_size > stream->dat->size - payload_offset)
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
      src.byte = payload_offset;
      src.bit = 0;
      src.size = src.byte + es.fields.data_size;

      if (stream->info->compressed == 2)
        error = r2004_stream_decompress_page (stream, &src, target, buffer,
                                              buffer_size);
      else
        {
          uint64_t remaining
              = (uint64_t)stream->info->size - es.fields.address;
          uint32_t size = remaining < es.fields.page_size
                              ? (uint32_t)remaining
                              : es.fields.page_size;
          if ((size_t)size > stream->dat->size - payload_offset)
            return DWG_ERR_VALUEOUTOFBOUNDS;
          for (uint32_t i = 0; i < size; i++)
            {
              error = r2004_stream_put_byte (
                  stream, stream->dat->chain[payload_offset + i], target,
                  buffer, buffer_size);
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
read_2004_page_cached_object_info (Dwg_Data *restrict dwg,
                                   R2004_Stream_Page_Cache *restrict cache,
                                   const Dwg_Stream_Input_Mode input_mode,
                                   const size_t address,
                                   const BITCODE_RLL handle_value,
                                   Dwg_Stream_Object_Info *restrict info)
{
  BITCODE_RC prefix[R2004_STREAM_OBJECT_PREFIX_SIZE];
  Bit_Chain body = { 0 };
  Dwg_Object_Type fixedtype;
  size_t prefix_size;
  bool borrowed = false;
  int error;

  memset (info, 0, sizeof (*info));
  if (address >= (uint64_t)cache->info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  memset (prefix, 0, sizeof (prefix));
  prefix_size = MIN (sizeof (prefix), (size_t)cache->info->size - address);
  if (prefix_size == sizeof (prefix))
    {
      if (cache->active
          && cache->active->descriptor_index < cache->num_descriptors)
        {
          const R2004_Stream_Page_Descriptor *descriptor
              = &cache->descriptors[cache->active->descriptor_index];

          if (descriptor->logical_size >= sizeof (prefix)
              && address >= descriptor->logical_start
              && address - descriptor->logical_start
                     <= descriptor->logical_size - sizeof (prefix))
            {
              size_t page_offset = address - descriptor->logical_start;

              body = cache->active->page;
              body.chain = &cache->active->page.chain[page_offset];
              body.byte = 0;
              body.bit = 0;
              body.size = sizeof (prefix);
              cache->active->last_used = ++cache->clock;
              borrowed = true;
            }
        }
      if (!borrowed)
        {
          error = r2004_stream_page_cache_borrow (
              cache, address, sizeof (prefix), &body, &borrowed);
          if (error)
            return error;
        }
    }
  if (!borrowed)
    {
      error
          = r2004_stream_page_cache_read (cache, address, prefix, prefix_size);
      if (error)
        return error;
      body.chain = prefix;
      body.size = sizeof (prefix);
      body.version = cache->dat->version;
      body.from_version = cache->dat->from_version;
    }
  info->size = bit_read_MS (&body);
  if (body.from_version >= R_2010b)
    (void)bit_read_UMC (&body);
  info->address = address + body.byte;
  if (info->size > (uint64_t)cache->info->size
      || info->address > (uint64_t)cache->info->size - info->size)
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
    info->supertype = dwg_stream_fixed_type_is_entity (fixedtype)
                          ? DWG_SUPERTYPE_ENTITY
                          : DWG_SUPERTYPE_OBJECT;
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

      if (startpos > endpos || endpos - startpos < 2)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      section_size = bit_read_RS_BE (&hdl_dat);
      if (section_size > 2040)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      if ((size_t)section_size + 2 > endpos - startpos)
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
          if (!handleoff
              || (last_handle <= max_handles
                  && handleoff > max_handles - last_handle)
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
read_2004_section_handles_page_cached_stream (
    Bit_Chain *restrict dat, Dwg_Data *restrict dwg,
    R2004_Stream_Page_Cache *restrict cache,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user,
    R2004_Stream_AcDs *restrict acds, int *restrict callback_error)
{
  Bit_Chain hdl_dat = { 0 };
  BITCODE_RC *object_bytes = NULL;
  BITCODE_RS section_size = 0;
  BITCODE_RLL last_handle = 0;
  BITCODE_MC prevsize = 0;
  BITCODE_BL index = 0;
  size_t object_capacity = 0;
  size_t endpos;
  int error;

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

      if (startpos > endpos || endpos - startpos < 2)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      section_size = bit_read_RS_BE (&hdl_dat);
      if (section_size > 2040)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      if ((size_t)section_size + 2 > endpos - startpos)
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
          if (!handleoff
              || (last_handle <= max_handles
                  && handleoff > max_handles - last_handle)
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

          object_error = read_2004_page_cached_object_info (
              dwg, cache, input_mode, last_offset, last_handle, &info);
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
              Bit_Chain object_dat = { 0 };
              size_t object_size;
              bool borrowed;
              int emitted_error;

              object_error = dwg_stream_object_record_size (
                  &info, last_offset, (size_t)cache->info->size, &object_size);
              if (object_error)
                {
                  error = object_error;
                  goto done;
                }
              object_error = r2004_stream_page_cache_borrow (
                  cache, last_offset, object_size, &object_dat, &borrowed);
              if (object_error)
                {
                  error = object_error;
                  goto done;
                }
              if (!borrowed)
                {
                  if (object_size > object_capacity)
                    {
                      BITCODE_RC *new_object_bytes
                          = (BITCODE_RC *)realloc (object_bytes, object_size);

                      if (!new_object_bytes)
                        {
                          error = DWG_ERR_OUTOFMEM;
                          goto done;
                        }
                      object_bytes = new_object_bytes;
                      object_capacity = object_size;
                    }
                  object_error = r2004_stream_page_cache_read (
                      cache, last_offset, object_bytes, object_size);
                  if (object_error)
                    {
                      error = object_error;
                      goto done;
                    }
                  object_dat = *dat;
                  object_dat.chain = object_bytes;
                  object_dat.byte = 0;
                  object_dat.bit = 0;
                  object_dat.size = object_size;
                }
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
  free (object_bytes);
  if (hdl_dat.chain)
    free (hdl_dat.chain);
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
  R2004_Stream_Page_Cache page_cache;
  R2004_Stream_Handle_Entry *entries = NULL;
  BITCODE_RS section_size = 0;
  BITCODE_RLL last_handle = 0;
  BITCODE_BL num_entries = 0;
  BITCODE_BL entries_capacity = 0;
  BITCODE_RC *object_bytes = NULL;
  size_t object_capacity = 0;
  size_t endpos;
  uint64_t buffered_allocation;
  bool buffered_safe;
  bool entries_ordered = true;
  bool prefer_low_memory;
  int error;

  memset (&page_cache, 0, sizeof (page_cache));
  obj_info = find_2004_section_info (dwg, SECTION_OBJECTS);
  if (!obj_info || obj_info->num_sections == 0 || !obj_info->sections)
    {
      LOG_ERROR ("Failed to find streamable %s section", "AcDbObjects");
      return DWG_ERR_SECTIONNOTFOUND;
    }
  buffered_allocation
      = (uint64_t)obj_info->num_sections * (uint64_t)obj_info->max_decomp_size;
  buffered_safe = buffered_allocation && buffered_allocation <= 0x2f000000U;
  prefer_low_memory = (callbacks->flags & DWG_STREAM_F_LOW_MEMORY) != 0;
  if (!prefer_low_memory && buffered_safe
      && (buffered_allocation <= R2004_STREAM_BUFFERED_MAX_BYTES
          || (uint64_t)dat->size > buffered_allocation / 2U))
    return read_2004_section_handles_buffered_stream (
        dat, dwg, callbacks, input_mode, user, acds, callback_error);
  if (!buffered_safe)
    goto forward_backend;
  error = r2004_stream_page_cache_init (&page_cache, dat, obj_info);
  if (!error)
    {
      LOG_TRACE ("R2004 Stream Objects backend: page-cache\n");
      error = read_2004_section_handles_page_cached_stream (
          dat, dwg, &page_cache, callbacks, input_mode, user, acds,
          callback_error);
      r2004_stream_page_cache_free (&page_cache);
      return error;
    }
  if (error == DWG_ERR_OUTOFMEM)
    return error;
  if (buffered_safe)
    {
      LOG_TRACE ("R2004 Stream Objects backend: buffered (cache rejected)\n");
      return read_2004_section_handles_buffered_stream (
          dat, dwg, callbacks, input_mode, user, acds, callback_error);
    }

forward_backend:
  LOG_TRACE ("R2004 Stream Objects backend: forward\n");
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

      if (startpos > endpos || endpos - startpos < 2)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      section_size = bit_read_RS_BE (&hdl_dat);
      if (section_size > 2040)
        {
          error = DWG_ERR_VALUEOUTOFBOUNDS;
          goto done;
        }
      if ((size_t)section_size + 2 > endpos - startpos)
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
          if (!handleoff
              || (last_handle <= max_handles
                  && handleoff > max_handles - last_handle))
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
              size_t allocation_size;

              if (new_capacity < entries_capacity)
                {
                  error = DWG_ERR_OUTOFMEM;
                  goto done;
                }
              allocation_size = (size_t)new_capacity * sizeof (*entries);
              if (new_capacity
                  && allocation_size / (size_t)new_capacity
                         != sizeof (*entries))
                {
                  error = DWG_ERR_OUTOFMEM;
                  goto done;
                }
              new_entries = (R2004_Stream_Handle_Entry *)realloc (
                  entries, allocation_size);
              if (!new_entries)
                {
                  error = DWG_ERR_OUTOFMEM;
                  goto done;
                }
              entries = new_entries;
              entries_capacity = new_capacity;
            }

          if (num_entries
              && (last_offset < entries[num_entries - 1].address
                  || (last_offset == entries[num_entries - 1].address
                      && last_handle < entries[num_entries - 1].handle)))
            entries_ordered = false;
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

  if (!entries_ordered)
    qsort (entries, num_entries, sizeof (*entries),
           compare_r2004_stream_handle_entries);

  for (BITCODE_BL i = 0; i < num_entries; i++)
    {
      Dwg_Stream_Object_Info info;
      int object_error;

      object_error = read_2004_stream_object_info (
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
          if (object_size > object_capacity)
            {
              BITCODE_RC *new_object_bytes
                  = (BITCODE_RC *)realloc (object_bytes, object_size);
              if (!new_object_bytes)
                {
                  error = DWG_ERR_OUTOFMEM;
                  goto done;
                }
              object_bytes = new_object_bytes;
              object_capacity = object_size;
            }
          object_error = r2004_object_stream_read (
              &obj_stream, entries[i].address, object_bytes, object_size);
          if (object_error)
            {
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
  if (object_bytes)
    free (object_bytes);
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
