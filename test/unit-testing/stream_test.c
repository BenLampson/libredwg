#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwg.h"

typedef struct _stream_stats
{
  BITCODE_BL num_objects;
  BITCODE_BL num_entities;
  BITCODE_BL num_non_entities;
  BITCODE_BL full_decode_objects;
  BITCODE_BL lightweight_objects;
  BITCODE_BL r2007_object_map_objects;
  BITCODE_BL r2004_object_map_objects;
  BITCODE_BL file_map_objects;
  BITCODE_BL heap_objects;
  unsigned long long total_size;
  unsigned long long handle_mix;
  size_t min_address;
  size_t max_address;
  BITCODE_RL max_size;
} Stream_Stats;

typedef struct _abort_stats
{
  BITCODE_BL calls;
  BITCODE_BL limit;
  int error;
} Abort_Stats;

static void print_stats (const char *label, const Stream_Stats *stats);

static int
write_unsupported_fixture (const char *path)
{
  static const char magic[] = "AC1015";
  unsigned char buffer[64] = { 0 };
  FILE *fp;

  fp = fopen (path, "wb");
  if (!fp)
    return 0;
  memcpy (buffer, magic, sizeof (magic) - 1);
  if (fwrite (buffer, 1, sizeof (buffer), fp) != sizeof (buffer))
    {
      fclose (fp);
      remove (path);
      return 0;
    }
  fclose (fp);
  return 1;
}

static void
stats_add_object (Stream_Stats *stats, const Dwg_Stream_Object_Info *info)
{
  stats->num_objects++;
  if (info->supertype == DWG_SUPERTYPE_ENTITY)
    stats->num_entities++;
  else
    stats->num_non_entities++;

  stats->total_size += (unsigned long long)info->size;
  stats->handle_mix ^= (unsigned long long)info->handle.value
                       + ((unsigned long long)info->type << 33)
                       + ((unsigned long long)info->supertype << 49);
  if (info->decode_mode == DWG_STREAM_DECODE_FULL)
    stats->full_decode_objects++;
  else if (info->decode_mode == DWG_STREAM_DECODE_R2007_OBJECT_MAP)
    {
      stats->lightweight_objects++;
      stats->r2007_object_map_objects++;
    }
  else if (info->decode_mode == DWG_STREAM_DECODE_R2004_OBJECT_MAP)
    {
      stats->lightweight_objects++;
      stats->r2004_object_map_objects++;
    }
  if (info->input_mode == DWG_STREAM_INPUT_FILE_MAP)
    stats->file_map_objects++;
  else if (info->input_mode == DWG_STREAM_INPUT_HEAP)
    stats->heap_objects++;

  if (stats->num_objects == 1 || info->address < stats->min_address)
    stats->min_address = info->address;
  if (info->address > stats->max_address)
    stats->max_address = info->address;
  if (info->size > stats->max_size)
    stats->max_size = info->size;
}

static void
stats_add_dwg_object (Stream_Stats *stats, const Dwg_Object *obj)
{
  Dwg_Stream_Object_Info info = { 0 };

  info.size = obj->size;
  info.address = obj->address;
  info.type = obj->type;
  info.index = obj->index;
  info.fixedtype = obj->fixedtype;
  info.name = obj->name;
  info.dxfname = obj->dxfname;
  info.supertype = obj->supertype;
  info.handle = obj->handle;
  info.version = obj->parent->header.version;
  info.decode_mode = DWG_STREAM_DECODE_FULL;

  stats_add_object (stats, &info);
}

static int
stream_object_callback (const Dwg_Stream_Object_Info *info, void *user)
{
  stats_add_object ((Stream_Stats *)user, info);
  return 0;
}

static int
abort_object_callback (const Dwg_Stream_Object_Info *info, void *user)
{
  Abort_Stats *stats = (Abort_Stats *)user;

  (void)info;
  stats->calls++;
  if (stats->calls >= stats->limit)
    return stats->error;
  return 0;
}

static int
test_no_full_fallback (void)
{
  const char *path = "stream_unsupported_r2000_fixture.dwg";
  Stream_Stats stats = { 0 };
  Dwg_Stream_Callbacks callbacks = { 0 };
  int error;

  if (!write_unsupported_fixture (path))
    {
      printf ("failed to create unsupported streaming fixture\n");
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file (path, &callbacks, &stats);
  remove (path);
  if (error != DWG_ERR_NOTYETSUPPORTED || stats.num_objects)
    {
      printf ("no-full-fallback failed: error=0x%x expected=0x%x "
              "objects=%lu\n",
              error, DWG_ERR_NOTYETSUPPORTED,
              (unsigned long)stats.num_objects);
      return 1;
    }
  return 0;
}

static int
test_large_stream_fixture (void)
{
  const char *path = getenv ("LIBREDWG_STREAM_TEST_LARGE_DWG");
  Stream_Stats stats = { 0 };
  Dwg_Stream_Callbacks callbacks = { 0 };
  FILE *fp;
  int error;

  if (!path || !*path)
    return 0;

  fp = fopen (path, "rb");
  if (!fp)
    {
      printf ("skip: cannot open large stream fixture %s\n", path);
      return 0;
    }
  fclose (fp);

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file (path, &callbacks, &stats);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("large stream fixture failed: %s error=0x%x\n", path, error);
      return 1;
    }
  if (!stats.num_objects || stats.full_decode_objects
      || stats.lightweight_objects != stats.num_objects)
    {
      print_stats ("large stream fixture did not use lightweight path",
                   &stats);
      return 1;
    }

  print_stats ("large stream fixture ok", &stats);
  return 0;
}

static int
stats_equal (const Stream_Stats *a, const Stream_Stats *b)
{
  return a->num_objects == b->num_objects
         && a->num_entities == b->num_entities
         && a->num_non_entities == b->num_non_entities
         && a->total_size == b->total_size
         && (a->handle_mix == b->handle_mix
             || b->r2004_object_map_objects == b->num_objects)
         && a->min_address == b->min_address
         && a->max_address == b->max_address
         && a->max_size == b->max_size;
}

static void
print_stats (const char *label, const Stream_Stats *stats)
{
  printf ("%s: objects=%lu entities=%lu non_entities=%lu total_size=%llu "
          "max_size=%lu min_address=%zu max_address=%zu handle_mix=%llu "
          "lightweight=%lu r2007=%lu r2004=%lu full=%lu file_map=%lu "
          "heap=%lu\n",
          label, (unsigned long)stats->num_objects,
          (unsigned long)stats->num_entities,
          (unsigned long)stats->num_non_entities, stats->total_size,
          (unsigned long)stats->max_size, stats->min_address,
          stats->max_address, stats->handle_mix,
          (unsigned long)stats->lightweight_objects,
          (unsigned long)stats->r2007_object_map_objects,
          (unsigned long)stats->r2004_object_map_objects,
          (unsigned long)stats->full_decode_objects,
          (unsigned long)stats->file_map_objects,
          (unsigned long)stats->heap_objects);
}

int
main (void)
{
  const char *path = getenv ("LIBREDWG_STREAM_TEST_DWG");
  FILE *fp;
  int error;
  Dwg_Data dwg = { 0 };
  Stream_Stats baseline = { 0 };
  Stream_Stats streamed = { 0 };
  Abort_Stats aborted = { 0 };
  Dwg_Stream_Callbacks callbacks = { 0 };

  if (test_no_full_fallback ())
    return 1;
  if (test_large_stream_fixture ())
    return 1;

  if (!path || !*path)
    {
      printf ("skip: set LIBREDWG_STREAM_TEST_DWG to a local DWG fixture\n");
      return 77;
    }

  fp = fopen (path, "rb");
  if (!fp)
    {
      printf ("skip: cannot open %s\n", path);
      return 77;
    }
  fclose (fp);

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("dwg_read_file failed: %s error=0x%x\n", path, error);
      return 1;
    }

  for (BITCODE_BL i = 0; i < dwg.num_objects; i++)
    stats_add_dwg_object (&baseline, &dwg.object[i]);
  dwg_free (&dwg);

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file (path, &callbacks, &streamed);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("dwg_stream_file failed: %s error=0x%x\n", path, error);
      return 1;
    }

  if (!stats_equal (&baseline, &streamed))
    {
      print_stats ("baseline", &baseline);
      print_stats ("streamed", &streamed);
      return 1;
    }
  if (streamed.lightweight_objects != streamed.num_objects
      || streamed.full_decode_objects)
    {
      print_stats ("streamed did not use the lightweight path", &streamed);
      return 1;
    }

  aborted.limit = 7;
  aborted.error = 12345;
  callbacks.object = abort_object_callback;
  error = dwg_stream_file (path, &callbacks, &aborted);
  if (error != aborted.error || aborted.calls != aborted.limit)
    {
      printf ("callback abort failed: error=0x%x expected=0x%x calls=%lu "
              "expected_calls=%lu\n",
              error, aborted.error, (unsigned long)aborted.calls,
              (unsigned long)aborted.limit);
      return 1;
    }

  print_stats ("stream parity ok", &streamed);
  return 0;
}
