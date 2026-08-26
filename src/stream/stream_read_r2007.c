/* Stream reader for the R2007 DWG format. */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "bits.h"
#include "dwg.h"
#include "decode.h"
#include "stream_object_helpers.h"
#include "stream_reader_internal.h"
#include "stream_r2007_internal.h"

#define DWG_LOGLEVEL loglevel
#include "logging.h"

/* Normal R2007 object pages are small.  Keep nearby pages without allowing
   irregular files to turn the Stream cache into an unbounded section copy. */
#define R2007_STREAM_PAGE_CACHE_SLOTS 16
#define R2007_STREAM_PAGE_CACHE_MAX_BYTES (1024U * 1024U)

typedef struct _r2007_stream_page_cache_entry
{
  Bit_Chain page;
  int64_t page_index;
  size_t section_offset;
  uint64_t last_used;
} R2007_Stream_Page_Cache_Entry;

typedef struct _r2007_stream_page_cache
{
  R2007_Stream_Page_Cache_Entry entries[R2007_STREAM_PAGE_CACHE_SLOTS];
  R2007_Stream_Page_Cache_Entry *active;
  size_t cached_bytes;
  uint64_t clock;
  bool ordered_pages;
} R2007_Stream_Page_Cache;

static void
r2007_stream_page_cache_init (R2007_Stream_Page_Cache *restrict cache)
{
  size_t i;

  memset (cache, 0, sizeof (*cache));
  for (i = 0; i < R2007_STREAM_PAGE_CACHE_SLOTS; i++)
    cache->entries[i].page_index = -1;
}

static void
r2007_stream_page_cache_evict (R2007_Stream_Page_Cache *cache,
                               R2007_Stream_Page_Cache_Entry *entry)
{
  if (!entry || !entry->page.chain)
    return;

  if (cache->active == entry)
    cache->active = NULL;
  if (entry->page.size <= cache->cached_bytes)
    cache->cached_bytes -= entry->page.size;
  else
    cache->cached_bytes = 0;
  free (entry->page.chain);
  memset (entry, 0, sizeof (*entry));
  entry->page_index = -1;
}

static void
r2007_stream_page_cache_free (R2007_Stream_Page_Cache *restrict cache)
{
  size_t i;

  for (i = 0; i < R2007_STREAM_PAGE_CACHE_SLOTS; i++)
    r2007_stream_page_cache_evict (cache, &cache->entries[i]);
  cache->active = NULL;
}

static bool
r2007_stream_section_pages_are_ordered (const r2007_section *restrict section)
{
  size_t previous_end = 0;
  bool have_previous = false;
  int64_t i;

  if (!section || !section->pages || section->num_pages <= 0)
    return false;

  for (i = 0; i < section->num_pages; i++)
    {
      const r2007_section_page *section_page = section->pages[i];
      size_t start, size;

      if (!section_page
          || section_page->offset != (uint64_t)(size_t)section_page->offset
          || section_page->uncomp_size
                 != (uint64_t)(size_t)section_page->uncomp_size)
        return false;
      start = (size_t)section_page->offset;
      size = (size_t)section_page->uncomp_size;
      if (size > (size_t)-1 - start || (have_previous && start < previous_end))
        return false;
      previous_end = start + size;
      have_previous = true;
    }
  return true;
}

static bool
r2007_stream_page_contains (
    const R2007_Stream_Page_Cache_Entry *restrict entry, const size_t address)
{
  return entry->page.chain && address >= entry->section_offset
         && address - entry->section_offset < entry->page.size;
}

static R2007_Stream_Page_Cache_Entry *
r2007_stream_page_cache_find_index (R2007_Stream_Page_Cache *restrict cache,
                                    const int64_t page_index)
{
  size_t i;

  for (i = 0; i < R2007_STREAM_PAGE_CACHE_SLOTS; i++)
    if (cache->entries[i].page.chain
        && cache->entries[i].page_index == page_index)
      return &cache->entries[i];
  return NULL;
}

static R2007_Stream_Page_Cache_Entry *
r2007_stream_page_cache_find_address (R2007_Stream_Page_Cache *restrict cache,
                                      const size_t address)
{
  size_t i;

  if (cache->active && r2007_stream_page_contains (cache->active, address))
    {
      cache->active->last_used = ++cache->clock;
      return cache->active;
    }
  for (i = 0; i < R2007_STREAM_PAGE_CACHE_SLOTS; i++)
    if (&cache->entries[i] != cache->active
        && r2007_stream_page_contains (&cache->entries[i], address))
      {
        cache->entries[i].last_used = ++cache->clock;
        return &cache->entries[i];
      }
  return NULL;
}

static R2007_Stream_Page_Cache_Entry *
r2007_stream_page_cache_empty (R2007_Stream_Page_Cache *restrict cache)
{
  size_t i;

  for (i = 0; i < R2007_STREAM_PAGE_CACHE_SLOTS; i++)
    if (!cache->entries[i].page.chain)
      return &cache->entries[i];
  return NULL;
}

static R2007_Stream_Page_Cache_Entry *
r2007_stream_page_cache_lru (R2007_Stream_Page_Cache *restrict cache)
{
  R2007_Stream_Page_Cache_Entry *entry = NULL;
  size_t i;

  for (i = 0; i < R2007_STREAM_PAGE_CACHE_SLOTS; i++)
    {
      if (!cache->entries[i].page.chain)
        continue;
      if (!entry || cache->entries[i].last_used < entry->last_used)
        entry = &cache->entries[i];
    }
  return entry;
}

static int
r2007_stream_add_handle_offset (size_t *restrict last_offset,
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

static int64_t
find_data_section_page_index (const r2007_section *restrict section,
                              const size_t address,
                              R2007_Stream_Page_Cache *restrict cache)
{
  R2007_Stream_Page_Cache_Entry *entry;
  int64_t i;

  if (!section)
    return -1;

  if (cache && cache->ordered_pages)
    {
      int64_t low = 0;
      int64_t high = section->num_pages;

      entry = r2007_stream_page_cache_find_address (cache, address);
      if (entry)
        return entry->page_index;
      while (low < high)
        {
          const int64_t mid = low + (high - low) / 2;
          const r2007_section_page *section_page = section->pages[mid];
          const size_t start = (size_t)section_page->offset;
          const size_t size = (size_t)section_page->uncomp_size;

          if (address < start)
            high = mid;
          else if (address - start < size)
            return mid;
          else
            low = mid + 1;
        }
      return -1;
    }
  for (i = 0; i < section->num_pages; i++)
    {
      const r2007_section_page *section_page = section->pages[i];
      size_t start, size;

      if (!section_page)
        continue;
      start = (size_t)section_page->offset;
      size = (size_t)section_page->uncomp_size;
      if (address >= start && address - start < size)
        return i;
    }
  return -1;
}

static int
load_stream_data_page (R2007_Stream_Page_Cache *restrict cache,
                       Bit_Chain *restrict dat,
                       r2007_section *restrict section,
                       r2007_page *restrict pages_map,
                       const int64_t page_index)
{
  r2007_section_page *section_page;
  R2007_Stream_Page_Cache_Entry *entry;
  Bit_Chain page = { 0 };
  size_t expected_size;
  int error;

  entry = r2007_stream_page_cache_find_index (cache, page_index);
  if (entry)
    {
      entry->last_used = ++cache->clock;
      cache->active = entry;
      return 0;
    }
  if (page_index < 0 || page_index >= section->num_pages)
    return DWG_ERR_PAGENOTFOUND;

  section_page = section->pages[page_index];
  if (!section_page
      || section_page->uncomp_size
             != (uint64_t)(size_t)section_page->uncomp_size)
    return DWG_ERR_VALUEOUTOFBOUNDS;
  expected_size = (size_t)section_page->uncomp_size;
  while (cache->cached_bytes
         > R2007_STREAM_PAGE_CACHE_MAX_BYTES
               - MIN (expected_size, R2007_STREAM_PAGE_CACHE_MAX_BYTES))
    {
      entry = r2007_stream_page_cache_lru (cache);
      if (!entry || !entry->page.chain)
        break;
      r2007_stream_page_cache_evict (cache, entry);
    }
  entry = r2007_stream_page_cache_empty (cache);
  if (!entry)
    {
      entry = r2007_stream_page_cache_lru (cache);
      r2007_stream_page_cache_evict (cache, entry);
    }
  if (!entry)
    {
      return DWG_ERR_INTERNALERROR;
    }

  error = read_data_section_page (&page, dat, pages_map, section_page);
  if (error >= DWG_ERR_CRITICAL || !page.chain)
    {
      if (page.chain)
        free (page.chain);
      return error ? error : DWG_ERR_INTERNALERROR;
    }
  entry->page = page;
  entry->page_index = page_index;
  entry->section_offset = (size_t)section_page->offset;
  entry->last_used = ++cache->clock;
  cache->cached_bytes += page.size;
  cache->active = entry;
  return 0;
}

static int
copy_data_section_window (Bit_Chain *restrict window, Bit_Chain *restrict dat,
                          r2007_section *restrict section,
                          r2007_page *restrict pages_map,
                          R2007_Stream_Page_Cache *restrict cache,
                          const size_t address, const size_t size)
{
  size_t copied = 0;

  memset (window, 0, sizeof (*window));
  if (size > (size_t)section->data_size
      || address > (size_t)section->data_size - size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  window->chain = (BITCODE_RC *)calloc (size, 1);
  if (!window->chain)
    return DWG_ERR_OUTOFMEM;
  window->size = size;
  window->version = dat->version;
  window->from_version = dat->from_version;
  window->opts = dat->opts;
  window->codepage = dat->codepage;

  while (copied < size)
    {
      const size_t pos = address + copied;
      const int64_t page_index
          = find_data_section_page_index (section, pos, cache);
      r2007_section_page *section_page;
      size_t page_start, page_offset, available, take;
      Bit_Chain page = { 0 };
      Bit_Chain *source;
      int error;

      if (page_index < 0)
        {
          free (window->chain);
          memset (window, 0, sizeof (*window));
          return DWG_ERR_PAGENOTFOUND;
        }

      section_page = section->pages[page_index];
      page_start = (size_t)section_page->offset;
      page_offset = pos - page_start;
      available = (size_t)section_page->uncomp_size - page_offset;
      take = MIN (available, size - copied);

      if (cache)
        {
          error = load_stream_data_page (cache, dat, section, pages_map,
                                         page_index);
          if (error)
            {
              free (window->chain);
              memset (window, 0, sizeof (*window));
              return error;
            }
          source = &cache->active->page;
        }
      else
        {
          error = read_data_section_page (&page, dat, pages_map, section_page);
          if (error >= DWG_ERR_CRITICAL)
            {
              free (window->chain);
              memset (window, 0, sizeof (*window));
              return error;
            }
          source = &page;
        }

      memcpy (&window->chain[copied], &source->chain[page_offset], take);
      if (source == &page && page.chain)
        free (page.chain);
      copied += take;
    }

  return 0;
}

static int
read_2007_stream_object_body (Dwg_Data *restrict dwg,
                              Bit_Chain *restrict body,
                              const Dwg_Stream_Input_Mode input_mode,
                              Dwg_Stream_Object_Info *restrict info)
{
  Dwg_Object_Type fixedtype;
  BITCODE_RL bitsize;
  int error = 0;

  info->type = bit_read_BS (body);
  fixedtype = (Dwg_Object_Type)info->type;
  bitsize = bit_read_RL (body);
  if (bitsize > info->size * 8)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  error = bit_read_H (body, &info->handle);
  if (error & DWG_ERR_INVALIDHANDLE)
    return error;

  if (info->type >= 500
      && (BITCODE_BL)(info->type - 500) < dwg->num_classes)
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
      info->supertype
          = dwg_stream_fixed_type_is_entity (fixedtype) ? DWG_SUPERTYPE_ENTITY
                                                    : DWG_SUPERTYPE_OBJECT;
    }
  info->fixedtype = fixedtype;
  info->version = dwg->header.from_version;
  info->decode_mode = DWG_STREAM_DECODE_R2007_OBJECT_MAP;
  info->input_mode = input_mode;
  return 0;
}

static int
read_2007_stream_object_info (Dwg_Data *restrict dwg,
                              Bit_Chain *restrict dat,
                              r2007_section *restrict objects_section,
                              r2007_page *restrict pages_map,
                              R2007_Stream_Page_Cache *restrict cache,
                              const Dwg_Stream_Input_Mode input_mode,
                              const size_t address,
                              Dwg_Stream_Object_Info *restrict info)
{
  Bit_Chain size_dat, body = { 0 };
  R2007_Stream_Page_Cache_Entry *entry;
  int64_t page_index;
  size_t page_offset;
  size_t size_base;
  int error;

  memset (info, 0, sizeof (*info));
  page_index = find_data_section_page_index (objects_section, address, cache);
  if (page_index < 0)
    return DWG_ERR_PAGENOTFOUND;

  error = load_stream_data_page (cache, dat, objects_section, pages_map,
                                 page_index);
  if (error)
    return error;

  entry = cache->active;
  page_offset = address - entry->section_offset;
  if (page_offset >= entry->page.size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  if (entry->page.size - page_offset >= 4)
    {
      size_dat = entry->page;
      size_dat.byte = page_offset;
      size_base = entry->section_offset;
    }
  else
    {
      error = copy_data_section_window (&size_dat, dat, objects_section,
                                        pages_map, cache, address, 4);
      if (error)
        return error;
      size_base = address;
    }
  size_dat.bit = 0;
  info->size = bit_read_MS (&size_dat);
  info->address = size_base + size_dat.byte;
  if (size_dat.chain != entry->page.chain && size_dat.chain)
    free (size_dat.chain);

  if (info->size > (size_t)objects_section->data_size
      || info->address > (size_t)objects_section->data_size - info->size)
    return DWG_ERR_VALUEOUTOFBOUNDS;

  entry = cache->ordered_pages
              ? r2007_stream_page_cache_find_address (cache, info->address)
              : NULL;
  if (entry && info->address >= entry->section_offset)
    {
      page_offset = info->address - entry->section_offset;
      if (page_offset <= entry->page.size
          && info->size <= entry->page.size - page_offset)
        {
          body = entry->page;
          body.byte = page_offset;
          body.bit = 0;
          bit_reset_chain (&body);
          body.size = info->size;
          return read_2007_stream_object_body (dwg, &body, input_mode, info);
        }
    }

  error = copy_data_section_window (&body, dat, objects_section, pages_map,
                                    cache, info->address, info->size);
  if (!error)
    error = read_2007_stream_object_body (dwg, &body, input_mode, info);
  if (body.chain)
    free (body.chain);
  return error;
}

static int
read_2007_section_handles_stream (
    Bit_Chain *dat, Dwg_Data *restrict dwg,
    r2007_section *restrict sections_map, r2007_page *restrict pages_map,
    const Dwg_Stream_Callbacks_Ex *restrict callbacks,
    const Dwg_Stream_Input_Mode input_mode, void *restrict user,
    int *restrict callback_error)
{
  R2007_Stream_Page_Cache object_page = { 0 };
  Bit_Chain hdl_dat = { 0 };
  r2007_section *objects_section;
  r2007_section *handles_section;
  int error;
  size_t endpos;
  BITCODE_RS section_size = 0;
  BITCODE_BL index = 0;

  r2007_stream_page_cache_init (&object_page);
  objects_section = get_section (sections_map, SECTION_OBJECTS);
  if (!objects_section)
    {
      LOG_ERROR ("Failed to find objects section");
      return DWG_ERR_SECTIONNOTFOUND;
    }
  object_page.ordered_pages
      = r2007_stream_section_pages_are_ordered (objects_section);
  handles_section = get_section (sections_map, SECTION_HANDLES);
  if (!handles_section)
    {
      LOG_ERROR ("Failed to find handles section");
      return DWG_ERR_SECTIONNOTFOUND;
    }

  error = read_data_section (&hdl_dat, dat, sections_map, pages_map,
                             SECTION_HANDLES);
  if (error >= DWG_ERR_CRITICAL || !hdl_dat.chain)
    {
      LOG_ERROR ("Failed to read handles section");
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
      uint16_t crc1, crc2;

      if (endpos - startpos < 2)
        {
          error |= DWG_ERR_VALUEOUTOFBOUNDS;
          break;
        }
      section_size = bit_read_RS_BE (&hdl_dat);
      if (section_size > 2050)
        {
          error |= DWG_ERR_VALUEOUTOFBOUNDS;
          break;
        }
      if ((size_t)section_size + 2 > endpos - startpos)
        {
          error |= DWG_ERR_VALUEOUTOFBOUNDS;
          break;
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
          object_error = r2007_stream_add_handle_offset (&last_offset,
                                                         offset);
          if (object_error)
            {
              error = object_error;
              goto done;
            }
          (void)handleoff;

          if (hdl_dat.byte == oldpos)
            break;

          object_error = read_2007_stream_object_info (
              dwg, dat, objects_section, pages_map, &object_page, input_mode,
              last_offset, &info);
          if (object_error)
            {
              error |= object_error;
              goto done;
            }

          info.index = index++;
          if (callbacks->object)
            {
              *callback_error = callbacks->object (&info, user);
              if (*callback_error)
                {
                  error = *callback_error;
                  goto done;
                }
            }
          if (callbacks->decoded_object)
            {
              Bit_Chain object_dat = { 0 };
              R2007_Stream_Page_Cache_Entry *entry;
              bool owns_object_dat = false;
              size_t prefix_size;
              size_t object_size;
              int decoded_error;

              if (info.address < last_offset)
                {
                  error = DWG_ERR_VALUEOUTOFBOUNDS;
                  goto done;
                }
              prefix_size = info.address - last_offset;
              if ((size_t)info.size > (size_t)-1 - prefix_size)
                {
                  error = DWG_ERR_VALUEOUTOFBOUNDS;
                  goto done;
                }
              object_size = prefix_size + info.size;
              entry = object_page.ordered_pages
                          ? r2007_stream_page_cache_find_address (&object_page,
                                                                  last_offset)
                          : NULL;
              if (entry && last_offset >= entry->section_offset)
                {
                  const size_t page_offset
                      = last_offset - entry->section_offset;
                  if (page_offset <= entry->page.size
                      && object_size <= entry->page.size - page_offset)
                    {
                      object_dat = entry->page;
                      object_dat.byte = page_offset;
                      object_dat.bit = 0;
                      bit_reset_chain (&object_dat);
                      object_dat.size = object_size;
                    }
                }
              if (!object_dat.chain)
                {
                  decoded_error = copy_data_section_window (
                      &object_dat, dat, objects_section, pages_map,
                      &object_page, last_offset, object_size);
                  if (decoded_error)
                    {
                      error = decoded_error;
                      goto done;
                    }
                  owns_object_dat = true;
                }
              decoded_error = dwg_stream_emit_decoded_object_ex (
                  dwg, &object_dat, &info, callbacks, user, callback_error);
              if (owns_object_dat && object_dat.chain)
                free (object_dat.chain);
              if (decoded_error)
                {
                  error = decoded_error;
                  goto done;
                }
            }
        }

      if (error >= DWG_ERR_CRITICAL || hdl_dat.byte == oldpos)
        break;

      crc1 = bit_calc_CRC (0xC0C1, &(hdl_dat.chain[startpos]),
                           hdl_dat.byte - startpos);
      crc2 = bit_read_RS_BE (&hdl_dat);
      if (crc1 != crc2)
        {
          LOG_WARN ("Handles section page CRC mismatch: %04X vs calc. %04X "
                    "from %" PRIuSIZE "-%" PRIuSIZE "\n",
                    crc2, crc1, startpos, hdl_dat.byte - 2);
          error |= DWG_ERR_WRONGCRC;
        }

      if (hdl_dat.byte >= endpos)
        break;
    }
  while (section_size > 2);

done:
  if (hdl_dat.chain)
    free (hdl_dat.chain);
  r2007_stream_page_cache_free (&object_page);
  return error;
}

int
dwg_stream_read_r2007 (Bit_Chain *dat, Bit_Chain *hdl_dat,
                       Dwg_Data *restrict dwg,
                       const Dwg_Stream_Callbacks_Ex *restrict callbacks,
                       Dwg_Stream_Input_Mode input_mode,
                       void *restrict user)
{
  Dwg_R2007_Header *file_header;
  r2007_page *restrict pages_map = NULL, *restrict page;
  r2007_section *restrict sections_map = NULL;
  int error;
  int callback_error = 0;
  int stream_error;
#ifdef USE_TRACING
  char *probe;
#endif

  read_r2007_init (dwg);
#ifdef USE_TRACING
  probe = getenv ("LIBREDWG_TRACE");
  if (probe)
    loglevel = atoi (probe);
#endif

  error = read_file_header (dat, &dwg->fhdr.r2007_file_header);
  if (error >= DWG_ERR_VALUEOUTOFBOUNDS)
    return error;
  file_header = &dwg->fhdr.r2007_file_header;

  dat->byte += 0x28;
  dat->byte += file_header->pages_map_offset;
  if ((size_t)file_header->pages_map_size_comp > dat->size - dat->byte)
    {
      error |= DWG_ERR_VALUEOUTOFBOUNDS;
      goto error;
    }
  pages_map = read_pages_map (dat, file_header->pages_map_size_comp,
                              file_header->pages_map_size_uncomp,
                              file_header->pages_map_correction);
  if (!pages_map)
    return DWG_ERR_PAGENOTFOUND;

  page = get_page (pages_map, file_header->sections_map_id);
  if (!page)
    {
      error |= DWG_ERR_SECTIONNOTFOUND;
      goto error;
    }
  if (page->offset >= dat->size
      || (size_t)file_header->sections_map_size_comp
             > dat->size - (size_t)page->offset)
    {
      error |= DWG_ERR_VALUEOUTOFBOUNDS;
      goto error;
    }
  dat->byte = page->offset;
  sections_map = read_sections_map (dat, file_header->sections_map_size_comp,
                                    file_header->sections_map_size_uncomp,
                                    file_header->sections_map_correction);
  if (!sections_map)
    {
      error |= DWG_ERR_SECTIONNOTFOUND;
      goto error;
    }

  error
      = read_2007_section_header (dat, hdl_dat, dwg, sections_map, pages_map);
  if (error < DWG_ERR_CRITICAL)
    error |= read_2007_section_classes (dat, dwg, sections_map, pages_map);
  if (error < DWG_ERR_CRITICAL && callbacks->decoded_object)
    error |= read_2007_section_filedeplist (dat, dwg, sections_map, pages_map);
  if (error < DWG_ERR_CRITICAL)
    {
      stream_error = read_2007_section_handles_stream (
          dat, dwg, sections_map, pages_map, callbacks, input_mode, user,
          &callback_error);
      if (callback_error)
        error = callback_error;
      else
        error |= stream_error;
    }

error:
  pages_destroy (pages_map);
  if (sections_map)
    sections_destroy (sections_map);
  return error;
}
