#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#include "dwg.h"
#include "dwg_api.h"
#include "decode.h"

#define STREAM_DECODE_ERROR_BUCKETS 16

typedef struct _stream_decode_error_bucket
{
  int used;
  int error;
  BITCODE_BS type;
  Dwg_Object_Supertype supertype;
  BITCODE_BL count;
  BITCODE_RLL first_handle;
  size_t first_address;
  BITCODE_RL first_size;
  BITCODE_RL first_index;
  char name[64];
  char dxfname[64];
} Stream_Decode_Error_Bucket;

typedef struct _stream_semantic_coverage
{
  BITCODE_BL block_headers;
  BITCODE_BL block_headers_with_owned;
  BITCODE_BL block_headers_with_inserts;
  BITCODE_BL block_entity_chains;
  BITCODE_BL anonymous_dimension_blocks;
  BITCODE_BL inserts;
  BITCODE_BL minserts;
  BITCODE_BL dimension_blocks;
  BITCODE_BL polylines_3d_with_vertices;
  BITCODE_BL hatches;
  BITCODE_BL wipeouts;
  BITCODE_BL texts;
  BITCODE_BL mtexts;
  BITCODE_BL ownerhandle_entities;
  BITCODE_BL ownerless_entities;
  BITCODE_BL model_owned_entities;
  BITCODE_BL paper_owned_entities;
  BITCODE_BL block_owned_entities;
  BITCODE_BL entmode0_entities;
  BITCODE_BL entmode1_entities;
  BITCODE_BL entmode2_entities;
  BITCODE_BL entmode3_entities;
  BITCODE_BL entmode_other_entities;
} Stream_Semantic_Coverage;

typedef struct _stream_stats
{
  BITCODE_BL num_objects;
  BITCODE_BL num_entities;
  BITCODE_BL num_non_entities;
  BITCODE_BL full_decode_objects;
  BITCODE_BL lightweight_objects;
  BITCODE_BL decoded_objects;
  BITCODE_BL decoded_entities;
  BITCODE_BL decoded_non_entities;
  BITCODE_BL max_host_entities;
  Dwg_Version_Type version;
  BITCODE_BL version_mismatches;
  unsigned long long decoded_handle_mix;
  BITCODE_BL r13_object_map_objects;
  BITCODE_BL r2007_object_map_objects;
  BITCODE_BL r2004_object_map_objects;
  BITCODE_BL prer13_entity_objects;
  BITCODE_BL file_map_objects;
  BITCODE_BL heap_objects;
  unsigned long long total_size;
  unsigned long long handle_mix;
  unsigned long long r11_type_mask;
  unsigned long long r11_block_section_type_mask;
  unsigned long long r11_dimension_fixedtype_mask;
  unsigned long long r11_block_dimension_fixedtype_mask;
  unsigned long long r11_polyline_fixedtype_mask;
  unsigned long long r11_block_polyline_fixedtype_mask;
  unsigned long long r11_vertex_fixedtype_mask;
  unsigned long long r11_table_fixedtype_mask;
  unsigned long long prer2_legacy_fixedtype_mask;
  BITCODE_BL r11_block_section_objects;
  BITCODE_BL r11_block_section_active;
  size_t min_address;
  size_t max_address;
  BITCODE_RL max_size;
  size_t last_address;
  BITCODE_RL last_size;
  BITCODE_BS last_type;
  BITCODE_RLL last_handle;
  Dwg_Object_Supertype last_supertype;
  struct _stream_ref_snapshot *baseline_refs;
  size_t baseline_ref_count;
  BITCODE_BL decoded_ref_mismatches;
  BITCODE_BL decoded_ref_missing;
  BITCODE_BL decoded_ref_checked;
  BITCODE_BL decode_error_objects;
  BITCODE_BL decode_error_entities;
  BITCODE_BL decode_error_non_entities;
  int first_decode_error;
  unsigned long long decode_error_handle_mix;
  Stream_Decode_Error_Bucket decode_error_buckets[STREAM_DECODE_ERROR_BUCKETS];
  BITCODE_BL decode_error_unbucketed;
  Stream_Semantic_Coverage semantic;
} Stream_Stats;

typedef struct _stream_ref_snapshot
{
  BITCODE_RLL handle;
  BITCODE_BS fixedtype;
  Dwg_Object_Supertype supertype;
  BITCODE_BB entmode;
  BITCODE_RLL ownerhandle;
  BITCODE_RLL layer;
  BITCODE_RLL block_header;
  BITCODE_RLL dimension_block;
  BITCODE_BL num_owned;
  BITCODE_RLL first_vertex;
  BITCODE_RLL last_vertex;
  unsigned long long owned_refs_mix;
  BITCODE_RLL seqend;
  BITCODE_RLL block_entity;
  BITCODE_RLL first_entity;
  BITCODE_RLL last_entity;
  BITCODE_RLL endblk_entity;
  unsigned long long block_entities_mix;
  BITCODE_RL block_num_inserts;
  unsigned long long block_inserts_mix;
  unsigned long long block_header_name_hash;
  unsigned long long semantic_hash;
  double block_base_x;
  double block_base_y;
  double block_base_z;
} Stream_Ref_Snapshot;

typedef struct _abort_stats
{
  BITCODE_BL calls;
  BITCODE_BL limit;
  int error;
} Abort_Stats;

typedef struct _emit_capacity_stats
{
  BITCODE_BL decoded_calls;
  BITCODE_BL decode_error_calls;
  int error;
  const Dwg_Stream_Object_Info *info;
} Emit_Capacity_Stats;

typedef struct _r11_minsert_opts_stats
{
  Stream_Stats stats;
  BITCODE_BL found;
} R11_Minsert_Opts_Stats;

static void print_stats (const char *label, const Stream_Stats *stats);
static void print_decode_error_buckets (const Stream_Stats *stats);
static void stream_trace_stage (const char *stage);
static long stream_test_process_id (void);
static int stream_test_source_path (char *path, size_t size,
                                    const char *relative);
static int test_stream_api_invalid_args (void);
static int test_repository_stream_fixtures (void);
static int test_repository_stream_sweep (int compare_refs);
static int test_invalid_version_stream_file_ex_rejects (void);
static int test_invalid_versions_reject (void);
static int test_generated_minsert_stream_fixture (Dwg_Version_Type version,
                                                  const char *label);
static int test_modern_header_version_stream (void);
static int test_pre_r13_minsert_opts_stream (void);
static int test_pre_r2_legacy_entity_stream (void);
static int test_generated_pre_r2_version_stream (void);
static int test_pre_r11_real_fixture_stream (void);
static int test_generated_pre_r11_version_stream (void);
static int test_pre_r13_legacy_entity_stream (void);
static int test_generated_pre_r13_stream_basic (void);
static int test_stream_file_parity (const char *path, int compare_refs,
                                    int test_abort_callbacks,
                                    const char *label, int skip_missing,
                                    Stream_Semantic_Coverage *coverage);
static unsigned long long hash_text (const char *text);
static unsigned long long hash_append_u64 (unsigned long long hash,
                                           unsigned long long value);
static unsigned long long hash_append_double (unsigned long long hash,
                                              double value);
static unsigned long long hash_append_color (unsigned long long hash,
                                             BITCODE_CMC color);
static unsigned long long hash_append_2rd (unsigned long long hash,
                                           BITCODE_2RD point);
static unsigned long long hash_append_2bd (unsigned long long hash,
                                           BITCODE_2BD point);
static unsigned long long hash_append_3bd (unsigned long long hash,
                                           BITCODE_3BD point);
static unsigned long long hash_ref_array (BITCODE_H *refs,
                                          BITCODE_BL num_refs);
static unsigned long long utf8_field_hash (void *entity, const char *name,
                                           const char *field);
static int utf8_field_has_prefix (void *entity, const char *name,
                                  const char *field, const char *prefix);
static unsigned long long entity_semantic_hash (const Dwg_Object *obj);
static void stats_add_owner_coverage (Stream_Semantic_Coverage *coverage,
                                      const Dwg_Object *obj,
                                      const Dwg_Object_Entity *entity,
                                      BITCODE_RLL ownerhandle);
static void stats_add_semantic_coverage (Stream_Stats *stats,
                                         const Dwg_Object *obj);
static unsigned long long pre_r13_table_entry_bit (BITCODE_BS fixedtype);
static int semantic_coverage_equal (const Stream_Semantic_Coverage *a,
                                    const Stream_Semantic_Coverage *b);
static void semantic_coverage_add (Stream_Semantic_Coverage *dst,
                                   const Stream_Semantic_Coverage *src);
static void print_semantic_coverage (const char *label,
                                     const Stream_Semantic_Coverage *coverage);
static int semantic_coverage_require_repository_sweep (
    const Stream_Semantic_Coverage *coverage);
static void snapshot_object_refs (const Dwg_Object *obj,
                                  Stream_Ref_Snapshot *snapshot);
static void snapshot_block_header_refs (const Dwg_Object *obj,
                                        Stream_Ref_Snapshot *snapshot);
static int compare_ref_snapshot_key (const void *a, const void *b);
static const Stream_Ref_Snapshot *
find_ref_snapshot (const Stream_Stats *stats, const Stream_Ref_Snapshot *key);
static int ref_snapshots_equal (const Stream_Ref_Snapshot *a,
                                const Stream_Ref_Snapshot *b);
static void stream_record_decode_error_bucket (
    Stream_Stats *stats, const Dwg_Stream_Object_Info *info, int error);

static void
stream_trace_stage (const char *stage)
{
  if (getenv ("LIBREDWG_STREAM_TEST_TRACE"))
    {
      fprintf (stderr, "stream_test: %s\n", stage);
      fflush (stderr);
    }
}

typedef struct _invalid_stream_fixture
{
  const char *magic;
  const char *label;
} Invalid_Stream_Fixture;

static const Invalid_Stream_Fixture invalid_fixtures[] = {
  { "AC9999", "unknown version" },
};

static int
write_invalid_version_fixture (const char *path, const char *magic)
{
  unsigned char buffer[64] = { 0 };
  FILE *fp;
  size_t len;

  fp = fopen (path, "wb");
  if (!fp)
    return 0;
  len = strlen (magic);
  if (len > 10)
    {
      fclose (fp);
      remove (path);
      return 0;
    }
  memcpy (buffer, magic, len);
  if (fwrite (buffer, 1, sizeof (buffer), fp) != sizeof (buffer))
    {
      fclose (fp);
      remove (path);
      return 0;
    }
  fclose (fp);
  return 1;
}

static long
stream_test_process_id (void)
{
#ifdef _WIN32
  return (long)_getpid ();
#else
  return (long)getpid ();
#endif
}

static void
stats_add_r11_polyline_fixedtype (Stream_Stats *stats,
                                  const Dwg_Object *object)
{
  if (!object || object->type != DWG_TYPE_POLYLINE_r11)
    return;

  switch (object->fixedtype)
    {
    case DWG_TYPE_POLYLINE_2D:
      stats->r11_polyline_fixedtype_mask |= 1ULL << 0;
      if (object->tio.entity && object->tio.entity->entmode == 3)
        stats->r11_block_polyline_fixedtype_mask |= 1ULL << 0;
      break;
    case DWG_TYPE_POLYLINE_3D:
      stats->r11_polyline_fixedtype_mask |= 1ULL << 1;
      if (object->tio.entity && object->tio.entity->entmode == 3)
        stats->r11_block_polyline_fixedtype_mask |= 1ULL << 1;
      break;
    case DWG_TYPE_POLYLINE_MESH:
      stats->r11_polyline_fixedtype_mask |= 1ULL << 2;
      if (object->tio.entity && object->tio.entity->entmode == 3)
        stats->r11_block_polyline_fixedtype_mask |= 1ULL << 2;
      break;
    case DWG_TYPE_POLYLINE_PFACE:
      stats->r11_polyline_fixedtype_mask |= 1ULL << 3;
      if (object->tio.entity && object->tio.entity->entmode == 3)
        stats->r11_block_polyline_fixedtype_mask |= 1ULL << 3;
      break;
    default:
      break;
    }
}

static void
stats_add_r11_vertex_fixedtype (Stream_Stats *stats, const Dwg_Object *object)
{
  if (!object || object->type != DWG_TYPE_VERTEX_r11)
    return;

  switch (object->fixedtype)
    {
    case DWG_TYPE_VERTEX_2D:
      stats->r11_vertex_fixedtype_mask |= 1ULL << 0;
      break;
    case DWG_TYPE_VERTEX_3D:
      stats->r11_vertex_fixedtype_mask |= 1ULL << 1;
      break;
    case DWG_TYPE_VERTEX_MESH:
      stats->r11_vertex_fixedtype_mask |= 1ULL << 2;
      break;
    case DWG_TYPE_VERTEX_PFACE:
      stats->r11_vertex_fixedtype_mask |= 1ULL << 3;
      break;
    case DWG_TYPE_VERTEX_PFACE_FACE:
      stats->r11_vertex_fixedtype_mask |= 1ULL << 4;
      break;
    default:
      break;
    }
}

static void
stats_add_r11_block_section_type (Stream_Stats *stats,
                                  const Dwg_Stream_Object_Info *info,
                                  const Dwg_Object *object)
{
  if (!info || !object || object->type >= 64
      || object->supertype != DWG_SUPERTYPE_ENTITY
      || info->decode_mode != DWG_STREAM_DECODE_PRER13_ENTITY)
    return;

  if (object->type == DWG_TYPE_BLOCK_r11)
    stats->r11_block_section_active = 1;
  if (!stats->r11_block_section_active)
    return;

  stats->r11_block_section_objects++;
  stats->r11_block_section_type_mask |= 1ULL << object->type;
  if (object->type == DWG_TYPE_ENDBLK_r11)
    stats->r11_block_section_active = 0;
}

static void
stats_add_r11_dimension_fixedtype (Stream_Stats *stats,
                                   const Dwg_Stream_Object_Info *info,
                                   const Dwg_Object *object)
{
  unsigned long long bit = 0;

  if (!object || object->type != DWG_TYPE_DIMENSION_r11)
    return;

  switch (object->fixedtype)
    {
    case DWG_TYPE_DIMENSION_LINEAR:
      bit = 1ULL << 0;
      break;
    case DWG_TYPE_DIMENSION_ALIGNED:
      bit = 1ULL << 1;
      break;
    case DWG_TYPE_DIMENSION_ANG2LN:
      bit = 1ULL << 2;
      break;
    case DWG_TYPE_DIMENSION_ANG3PT:
      bit = 1ULL << 3;
      break;
    case DWG_TYPE_DIMENSION_DIAMETER:
      bit = 1ULL << 4;
      break;
    case DWG_TYPE_DIMENSION_ORDINATE:
      bit = 1ULL << 5;
      break;
    case DWG_TYPE_DIMENSION_RADIUS:
      bit = 1ULL << 6;
      break;
    default:
      break;
    }
  stats->r11_dimension_fixedtype_mask |= bit;
  if (info && info->decode_mode == DWG_STREAM_DECODE_PRER13_ENTITY
      && stats->r11_block_section_active)
    stats->r11_block_dimension_fixedtype_mask |= bit;
}

static unsigned long long
pre_r13_table_entry_bit (BITCODE_BS fixedtype)
{
  switch (fixedtype)
    {
    case DWG_TYPE_BLOCK_HEADER:
      return 1ULL << 0;
    case DWG_TYPE_LAYER:
      return 1ULL << 1;
    case DWG_TYPE_STYLE:
      return 1ULL << 2;
    case DWG_TYPE_LTYPE:
      return 1ULL << 3;
    case DWG_TYPE_VIEW:
      return 1ULL << 4;
    case DWG_TYPE_UCS:
      return 1ULL << 5;
    case DWG_TYPE_VPORT:
      return 1ULL << 6;
    case DWG_TYPE_APPID:
      return 1ULL << 7;
    case DWG_TYPE_DIMSTYLE:
      return 1ULL << 8;
    case DWG_TYPE_VX_TABLE_RECORD:
      return 1ULL << 9;
    default:
      return 0;
    }
}

static unsigned long long
pre_r2_legacy_entity_bit (BITCODE_BS fixedtype)
{
  switch (fixedtype)
    {
    case DWG_TYPE_REPEAT:
      return 1ULL << 0;
    case DWG_TYPE_ENDREP:
      return 1ULL << 1;
    case DWG_TYPE_LOAD:
      return 1ULL << 2;
    default:
      return 0;
    }
}

static void
stats_add_object (Stream_Stats *stats, const Dwg_Stream_Object_Info *info)
{
  if (!stats->num_objects)
    stats->version = info->version;
  else if (stats->version != info->version)
    stats->version_mismatches++;
  stats->num_objects++;
  if (info->supertype == DWG_SUPERTYPE_ENTITY)
    stats->num_entities++;
  else
    stats->num_non_entities++;

  stats->total_size += (unsigned long long)info->size;
  stats->handle_mix ^= (unsigned long long)info->handle.value
                       + ((unsigned long long)info->type << 33)
                       + ((unsigned long long)info->supertype << 49);
  if (info->decode_mode == DWG_STREAM_DECODE_PRER13_ENTITY
      && info->supertype == DWG_SUPERTYPE_ENTITY && info->type < 64)
    stats->r11_type_mask |= 1ULL << info->type;
  if (info->decode_mode == DWG_STREAM_DECODE_PRER13_ENTITY
      && info->supertype != DWG_SUPERTYPE_ENTITY)
    stats->r11_table_fixedtype_mask
        |= pre_r13_table_entry_bit (info->fixedtype);
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
  else if (info->decode_mode == DWG_STREAM_DECODE_R13_OBJECT_MAP)
    {
      stats->lightweight_objects++;
      stats->r13_object_map_objects++;
    }
  else if (info->decode_mode == DWG_STREAM_DECODE_PRER13_ENTITY)
    {
      stats->lightweight_objects++;
      stats->prer13_entity_objects++;
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
  stats->last_address = info->address;
  stats->last_size = info->size;
  stats->last_type = info->type;
  stats->last_handle = info->handle.value;
  stats->last_supertype = info->supertype;
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
  info.version = obj->parent->header.from_version;
  info.decode_mode = DWG_STREAM_DECODE_FULL;

  stats_add_object (stats, &info);
  stats_add_semantic_coverage (stats, obj);
}

static BITCODE_RLL
ref_absolute (const BITCODE_H ref)
{
  return ref ? ref->absolute_ref : 0;
}

static unsigned long long
hash_text (const char *text)
{
  unsigned long long hash = 1469598103934665603ULL;

  if (!text)
    return 0;
  while (*text)
    {
      hash ^= (unsigned char)*text;
      hash *= 1099511628211ULL;
      text++;
    }
  return hash ? hash : 1;
}

static unsigned long long
hash_append_u64 (unsigned long long hash, unsigned long long value)
{
  hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
  hash *= 1099511628211ULL;
  return hash ? hash : 1;
}

static unsigned long long
hash_append_double (unsigned long long hash, double value)
{
  unsigned char bytes[sizeof (value)];
  size_t i;

  memcpy (bytes, &value, sizeof (bytes));
  for (i = 0; i < sizeof (bytes); i++)
    hash = hash_append_u64 (hash, bytes[i]);
  return hash;
}

static unsigned long long
hash_append_color (unsigned long long hash, BITCODE_CMC color)
{
  hash = hash_append_u64 (hash, (unsigned long long)(long long)color.index);
  hash = hash_append_u64 (hash, (unsigned long long)color.flag);
  hash = hash_append_u64 (hash, (unsigned long long)color.raw);
  hash = hash_append_u64 (hash, (unsigned long long)color.rgb);
  hash = hash_append_u64 (hash, (unsigned long long)color.method);
  hash = hash_append_u64 (hash, hash_text ((const char *)color.name));
  hash = hash_append_u64 (hash, hash_text ((const char *)color.book_name));
  hash = hash_append_u64 (hash, ref_absolute (color.handle));
  hash = hash_append_u64 (hash, (unsigned long long)color.alpha_raw);
  hash = hash_append_u64 (hash, (unsigned long long)color.alpha_type);
  hash = hash_append_u64 (hash, (unsigned long long)color.alpha);
  return hash;
}

static unsigned long long
hash_append_2rd (unsigned long long hash, BITCODE_2RD point)
{
  hash = hash_append_double (hash, point.x);
  hash = hash_append_double (hash, point.y);
  return hash;
}

static unsigned long long
hash_append_2bd (unsigned long long hash, BITCODE_2BD point)
{
  hash = hash_append_double (hash, point.x);
  hash = hash_append_double (hash, point.y);
  return hash;
}

static unsigned long long
hash_append_3bd (unsigned long long hash, BITCODE_3BD point)
{
  hash = hash_append_double (hash, point.x);
  hash = hash_append_double (hash, point.y);
  hash = hash_append_double (hash, point.z);
  return hash;
}

static unsigned long long
hash_ref_array (BITCODE_H *refs, BITCODE_BL num_refs)
{
  BITCODE_BL i;
  unsigned long long hash = 1469598103934665603ULL;

  if (!refs || !num_refs)
    return 0;
  for (i = 0; i < num_refs; i++)
    {
      BITCODE_RLL absolute_ref = ref_absolute (refs[i]);
      hash ^= (unsigned long long)absolute_ref;
      hash *= 1099511628211ULL;
    }
  return hash ? hash : 1;
}

static unsigned long long
utf8_field_hash (void *entity, const char *name, const char *field)
{
  char *text = NULL;
  int isnew = 0;
  int ok;
  unsigned long long hash = 0;

  if (!entity || !name || !field)
    return 0;
  ok = dwg_dynapi_entity_utf8text (entity, name, field, &text, &isnew, NULL);
  if (ok && text)
    hash = hash_text (text);
  if (isnew && text)
    free (text);
  return hash;
}

static int
utf8_field_has_prefix (void *entity, const char *name, const char *field,
                       const char *prefix)
{
  char *text = NULL;
  int isnew = 0;
  int ok;
  int matches = 0;
  size_t prefix_len;

  if (!entity || !name || !field || !prefix)
    return 0;
  ok = dwg_dynapi_entity_utf8text (entity, name, field, &text, &isnew, NULL);
  prefix_len = strlen (prefix);
  if (ok && text && strncmp (text, prefix, prefix_len) == 0)
    matches = 1;
  if (isnew && text)
    free (text);
  return matches;
}

static void
stats_add_owner_coverage (Stream_Semantic_Coverage *coverage,
                          const Dwg_Object *obj,
                          const Dwg_Object_Entity *entity,
                          BITCODE_RLL ownerhandle)
{
  BITCODE_RLL model_handle = 0;
  BITCODE_RLL paper_handle = 0;

  if (!coverage || !entity)
    return;
  if (obj && obj->parent)
    {
      model_handle
          = ref_absolute (obj->parent->header_vars.BLOCK_RECORD_MSPACE);
      paper_handle
          = ref_absolute (obj->parent->header_vars.BLOCK_RECORD_PSPACE);
    }

  if (ownerhandle)
    {
      if (ownerhandle == model_handle)
        coverage->model_owned_entities++;
      else if (ownerhandle == paper_handle)
        coverage->paper_owned_entities++;
      else
        coverage->block_owned_entities++;
      return;
    }

  switch (entity->entmode)
    {
    case 1:
      coverage->paper_owned_entities++;
      break;
    case 2:
      coverage->model_owned_entities++;
      break;
    case 3:
      coverage->block_owned_entities++;
      break;
    default:
      break;
    }
}

static void
stats_add_semantic_coverage (Stream_Stats *stats, const Dwg_Object *obj)
{
  Stream_Semantic_Coverage *coverage;
  Dwg_Object_Entity *entity;
  BITCODE_RLL ownerhandle;

  if (!stats || !obj)
    return;
  coverage = &stats->semantic;
  if (obj->fixedtype == DWG_TYPE_BLOCK_HEADER)
    {
      if (obj->tio.object && obj->tio.object->tio.BLOCK_HEADER)
        {
          Dwg_Object_BLOCK_HEADER *block = obj->tio.object->tio.BLOCK_HEADER;

          coverage->block_headers++;
          if (block->num_owned)
            coverage->block_headers_with_owned++;
          if (block->num_inserts)
            coverage->block_headers_with_inserts++;
          if (block->block_entity || block->first_entity || block->last_entity
              || block->endblk_entity || block->num_owned)
            coverage->block_entity_chains++;
          if (utf8_field_has_prefix (block, "BLOCK_HEADER", "name", "*D"))
            coverage->anonymous_dimension_blocks++;
        }
      return;
    }

  if (obj->supertype != DWG_SUPERTYPE_ENTITY || !obj->tio.entity)
    return;

  entity = obj->tio.entity;
  ownerhandle = ref_absolute (entity->ownerhandle);
  if (ownerhandle)
    coverage->ownerhandle_entities++;
  else
    coverage->ownerless_entities++;
  stats_add_owner_coverage (coverage, obj, entity, ownerhandle);

  switch (entity->entmode)
    {
    case 0:
      coverage->entmode0_entities++;
      break;
    case 1:
      coverage->entmode1_entities++;
      break;
    case 2:
      coverage->entmode2_entities++;
      break;
    case 3:
      coverage->entmode3_entities++;
      break;
    default:
      coverage->entmode_other_entities++;
      break;
    }

  switch (obj->fixedtype)
    {
    case DWG_TYPE_INSERT:
      if (entity->tio.INSERT)
        coverage->inserts++;
      break;
    case DWG_TYPE_MINSERT:
      if (entity->tio.MINSERT)
        coverage->minserts++;
      break;
    case DWG_TYPE_DIMENSION_ALIGNED:
    case DWG_TYPE_DIMENSION_ANG2LN:
    case DWG_TYPE_DIMENSION_ANG3PT:
    case DWG_TYPE_DIMENSION_DIAMETER:
    case DWG_TYPE_DIMENSION_LINEAR:
    case DWG_TYPE_DIMENSION_ORDINATE:
    case DWG_TYPE_DIMENSION_RADIUS:
    case DWG_TYPE_ARC_DIMENSION:
    case DWG_TYPE_LARGE_RADIAL_DIMENSION:
      if (entity->tio.DIMENSION_common && entity->tio.DIMENSION_common->block)
        coverage->dimension_blocks++;
      break;
    case DWG_TYPE_POLYLINE_3D:
      if (entity->tio.POLYLINE_3D
          && (entity->tio.POLYLINE_3D->first_vertex
              || entity->tio.POLYLINE_3D->last_vertex
              || entity->tio.POLYLINE_3D->num_owned))
        coverage->polylines_3d_with_vertices++;
      break;
    case DWG_TYPE_HATCH:
      if (entity->tio.HATCH)
        coverage->hatches++;
      break;
    case DWG_TYPE_WIPEOUT:
      if (entity->tio.WIPEOUT)
        coverage->wipeouts++;
      break;
    case DWG_TYPE_TEXT:
      if (entity->tio.TEXT)
        coverage->texts++;
      break;
    case DWG_TYPE_MTEXT:
      if (entity->tio.MTEXT)
        coverage->mtexts++;
      break;
    default:
      break;
    }
}

static int
semantic_coverage_equal (const Stream_Semantic_Coverage *a,
                         const Stream_Semantic_Coverage *b)
{
  return a->block_headers == b->block_headers
         && a->block_headers_with_owned == b->block_headers_with_owned
         && a->block_headers_with_inserts == b->block_headers_with_inserts
         && a->block_entity_chains == b->block_entity_chains
         && a->anonymous_dimension_blocks == b->anonymous_dimension_blocks
         && a->inserts == b->inserts && a->minserts == b->minserts
         && a->dimension_blocks == b->dimension_blocks
         && a->polylines_3d_with_vertices == b->polylines_3d_with_vertices
         && a->hatches == b->hatches && a->wipeouts == b->wipeouts
         && a->texts == b->texts && a->mtexts == b->mtexts
         && a->ownerhandle_entities == b->ownerhandle_entities
         && a->ownerless_entities == b->ownerless_entities
         && a->model_owned_entities == b->model_owned_entities
         && a->paper_owned_entities == b->paper_owned_entities
         && a->block_owned_entities == b->block_owned_entities
         && a->entmode0_entities == b->entmode0_entities
         && a->entmode1_entities == b->entmode1_entities
         && a->entmode2_entities == b->entmode2_entities
         && a->entmode3_entities == b->entmode3_entities
         && a->entmode_other_entities == b->entmode_other_entities;
}

static void
semantic_coverage_add (Stream_Semantic_Coverage *dst,
                       const Stream_Semantic_Coverage *src)
{
  if (!dst || !src)
    return;

  dst->block_headers += src->block_headers;
  dst->block_headers_with_owned += src->block_headers_with_owned;
  dst->block_headers_with_inserts += src->block_headers_with_inserts;
  dst->block_entity_chains += src->block_entity_chains;
  dst->anonymous_dimension_blocks += src->anonymous_dimension_blocks;
  dst->inserts += src->inserts;
  dst->minserts += src->minserts;
  dst->dimension_blocks += src->dimension_blocks;
  dst->polylines_3d_with_vertices += src->polylines_3d_with_vertices;
  dst->hatches += src->hatches;
  dst->wipeouts += src->wipeouts;
  dst->texts += src->texts;
  dst->mtexts += src->mtexts;
  dst->ownerhandle_entities += src->ownerhandle_entities;
  dst->ownerless_entities += src->ownerless_entities;
  dst->model_owned_entities += src->model_owned_entities;
  dst->paper_owned_entities += src->paper_owned_entities;
  dst->block_owned_entities += src->block_owned_entities;
  dst->entmode0_entities += src->entmode0_entities;
  dst->entmode1_entities += src->entmode1_entities;
  dst->entmode2_entities += src->entmode2_entities;
  dst->entmode3_entities += src->entmode3_entities;
  dst->entmode_other_entities += src->entmode_other_entities;
}

static void
print_semantic_coverage (const char *label,
                         const Stream_Semantic_Coverage *coverage)
{
  printf ("%s: block_headers=%lu block_headers_owned=%lu "
          "block_headers_inserted=%lu block_chains=%lu "
          "anonymous_dim_blocks=%lu inserts=%lu "
          "minserts=%lu dimension_blocks=%lu poly3d_vertices=%lu "
          "hatches=%lu wipeouts=%lu texts=%lu mtexts=%lu "
          "ownerhandle_entities=%lu ownerless_entities=%lu model=%lu "
          "paper=%lu block_owned=%lu entmode0=%lu entmode1=%lu "
          "entmode2=%lu entmode3=%lu entmode_other=%lu\n",
          label, (unsigned long)coverage->block_headers,
          (unsigned long)coverage->block_headers_with_owned,
          (unsigned long)coverage->block_headers_with_inserts,
          (unsigned long)coverage->block_entity_chains,
          (unsigned long)coverage->anonymous_dimension_blocks,
          (unsigned long)coverage->inserts, (unsigned long)coverage->minserts,
          (unsigned long)coverage->dimension_blocks,
          (unsigned long)coverage->polylines_3d_with_vertices,
          (unsigned long)coverage->hatches, (unsigned long)coverage->wipeouts,
          (unsigned long)coverage->texts, (unsigned long)coverage->mtexts,
          (unsigned long)coverage->ownerhandle_entities,
          (unsigned long)coverage->ownerless_entities,
          (unsigned long)coverage->model_owned_entities,
          (unsigned long)coverage->paper_owned_entities,
          (unsigned long)coverage->block_owned_entities,
          (unsigned long)coverage->entmode0_entities,
          (unsigned long)coverage->entmode1_entities,
          (unsigned long)coverage->entmode2_entities,
          (unsigned long)coverage->entmode3_entities,
          (unsigned long)coverage->entmode_other_entities);
}

static int
semantic_coverage_require_repository_sweep (
    const Stream_Semantic_Coverage *coverage)
{
  int missing = 0;

  if (!coverage)
    return 1;
#define REQUIRE_COVERAGE(field)                                               \
  do                                                                          \
    {                                                                         \
      if (!coverage->field)                                                   \
        {                                                                     \
          printf ("repository sweep semantic coverage missing: %s\n",         \
                  #field);                                                    \
          missing = 1;                                                        \
        }                                                                     \
    }                                                                         \
  while (0)

  REQUIRE_COVERAGE (block_headers);
  REQUIRE_COVERAGE (block_headers_with_owned);
  REQUIRE_COVERAGE (block_headers_with_inserts);
  REQUIRE_COVERAGE (block_entity_chains);
  REQUIRE_COVERAGE (anonymous_dimension_blocks);
  REQUIRE_COVERAGE (inserts);
  REQUIRE_COVERAGE (dimension_blocks);
  REQUIRE_COVERAGE (polylines_3d_with_vertices);
  REQUIRE_COVERAGE (hatches);
  REQUIRE_COVERAGE (wipeouts);
  REQUIRE_COVERAGE (texts);
  REQUIRE_COVERAGE (mtexts);
  REQUIRE_COVERAGE (ownerhandle_entities);
  REQUIRE_COVERAGE (ownerless_entities);
  REQUIRE_COVERAGE (model_owned_entities);
  REQUIRE_COVERAGE (paper_owned_entities);
  REQUIRE_COVERAGE (block_owned_entities);
  REQUIRE_COVERAGE (entmode0_entities);
  REQUIRE_COVERAGE (entmode1_entities);
  REQUIRE_COVERAGE (entmode2_entities);
#undef REQUIRE_COVERAGE

  if (missing)
    print_semantic_coverage ("repository sweep semantic coverage", coverage);
  return missing;
}

static unsigned long long
hash_append_common_entity (unsigned long long hash,
                           const Dwg_Object_Entity *entity)
{
  hash = hash_append_u64 (hash, (unsigned long long)entity->entmode);
  hash = hash_append_u64 (hash, ref_absolute (entity->ownerhandle));
  hash = hash_append_u64 (hash, ref_absolute (entity->prev_entity));
  hash = hash_append_u64 (hash, ref_absolute (entity->next_entity));
  hash = hash_append_u64 (hash, ref_absolute (entity->layer));
  hash = hash_append_u64 (hash, ref_absolute (entity->ltype));
  hash = hash_append_u64 (hash, ref_absolute (entity->material));
  hash = hash_append_u64 (hash, ref_absolute (entity->plotstyle));
  hash = hash_append_u64 (hash, ref_absolute (entity->xdicobjhandle));
  hash = hash_append_u64 (hash, entity->num_reactors);
  hash = hash_append_u64 (
      hash, hash_ref_array (entity->reactors, entity->num_reactors));
  hash = hash_append_color (hash, entity->color);
  hash = hash_append_double (hash, entity->ltype_scale);
  hash = hash_append_u64 (hash, entity->ltype_flags);
  hash = hash_append_u64 (hash, entity->plotstyle_flags);
  hash = hash_append_u64 (hash, entity->material_flags);
  hash = hash_append_u64 (hash, entity->shadow_flags);
  hash = hash_append_u64 (hash, entity->invisible);
  hash = hash_append_u64 (hash, entity->linewt);
  hash = hash_append_u64 (hash, entity->isbylayerlt);
  hash = hash_append_u64 (hash, entity->nolinks);
  return hash;
}

static unsigned long long
hash_append_insert (unsigned long long hash, const Dwg_Entity_INSERT *insert)
{
  hash = hash_append_3bd (hash, insert->ins_pt);
  hash = hash_append_u64 (hash, insert->scale_flag);
  hash = hash_append_3bd (hash, insert->scale);
  hash = hash_append_double (hash, insert->rotation);
  hash = hash_append_3bd (hash, insert->extrusion);
  hash = hash_append_u64 (hash, insert->has_attribs);
  hash = hash_append_u64 (hash, insert->num_owned);
  hash = hash_append_u64 (hash, ref_absolute (insert->block_header));
  hash = hash_append_u64 (hash, ref_absolute (insert->first_attrib));
  hash = hash_append_u64 (hash, ref_absolute (insert->last_attrib));
  hash = hash_append_u64 (hash,
                          hash_ref_array (insert->attribs, insert->num_owned));
  hash = hash_append_u64 (hash, ref_absolute (insert->seqend));
  hash = hash_append_u64 (hash, insert->num_cols);
  hash = hash_append_u64 (hash, insert->num_rows);
  hash = hash_append_double (hash, insert->col_spacing);
  hash = hash_append_double (hash, insert->row_spacing);
  return hash;
}

static unsigned long long
hash_append_minsert (unsigned long long hash, const Dwg_Entity_MINSERT *insert)
{
  hash = hash_append_3bd (hash, insert->ins_pt);
  hash = hash_append_u64 (hash, insert->scale_flag);
  hash = hash_append_3bd (hash, insert->scale);
  hash = hash_append_double (hash, insert->rotation);
  hash = hash_append_3bd (hash, insert->extrusion);
  hash = hash_append_u64 (hash, insert->has_attribs);
  hash = hash_append_u64 (hash, insert->num_owned);
  hash = hash_append_u64 (hash, insert->num_cols);
  hash = hash_append_u64 (hash, insert->num_rows);
  hash = hash_append_double (hash, insert->col_spacing);
  hash = hash_append_double (hash, insert->row_spacing);
  hash = hash_append_u64 (hash, ref_absolute (insert->block_header));
  hash = hash_append_u64 (hash, ref_absolute (insert->first_attrib));
  hash = hash_append_u64 (hash, ref_absolute (insert->last_attrib));
  hash = hash_append_u64 (hash,
                          hash_ref_array (insert->attribs, insert->num_owned));
  hash = hash_append_u64 (hash, ref_absolute (insert->seqend));
  return hash;
}

static unsigned long long
hash_append_dimension_common (unsigned long long hash,
                              const Dwg_DIMENSION_common *dimension)
{
  hash = hash_append_u64 (hash, dimension->class_version);
  hash = hash_append_3bd (hash, dimension->extrusion);
  hash = hash_append_3bd (hash, dimension->def_pt);
  hash = hash_append_2rd (hash, dimension->text_midpt);
  hash = hash_append_double (hash, dimension->elevation);
  hash = hash_append_u64 (hash, dimension->flag);
  hash = hash_append_u64 (hash, dimension->flag1);
  hash
      = hash_append_u64 (hash, hash_text ((const char *)dimension->user_text));
  hash = hash_append_double (hash, dimension->text_rotation);
  hash = hash_append_double (hash, dimension->horiz_dir);
  hash = hash_append_3bd (hash, dimension->ins_scale);
  hash = hash_append_double (hash, dimension->ins_rotation);
  hash = hash_append_u64 (hash, dimension->attachment);
  hash = hash_append_u64 (hash, dimension->lspace_style);
  hash = hash_append_double (hash, dimension->lspace_factor);
  hash = hash_append_double (hash, dimension->act_measurement);
  hash = hash_append_u64 (hash, dimension->flip_arrow1);
  hash = hash_append_u64 (hash, dimension->flip_arrow2);
  hash = hash_append_2rd (hash, dimension->clone_ins_pt);
  hash = hash_append_u64 (hash, ref_absolute (dimension->block));
  hash = hash_append_u64 (hash, ref_absolute (dimension->dimstyle));
  return hash;
}

static unsigned long long
hash_append_lwpolyline (unsigned long long hash,
                        const Dwg_Entity_LWPOLYLINE *lwpolyline)
{
  BITCODE_BL i;

  hash = hash_append_u64 (hash, lwpolyline->flag);
  hash = hash_append_double (hash, lwpolyline->const_width);
  hash = hash_append_double (hash, lwpolyline->elevation);
  hash = hash_append_double (hash, lwpolyline->thickness);
  hash = hash_append_3bd (hash, lwpolyline->extrusion);
  hash = hash_append_u64 (hash, lwpolyline->num_points);
  for (i = 0; i < lwpolyline->num_points; i++)
    hash = hash_append_2rd (hash, lwpolyline->points[i]);
  hash = hash_append_u64 (hash, lwpolyline->num_bulges);
  for (i = 0; i < lwpolyline->num_bulges; i++)
    hash = hash_append_double (hash, lwpolyline->bulges[i]);
  hash = hash_append_u64 (hash, lwpolyline->num_widths);
  for (i = 0; i < lwpolyline->num_widths; i++)
    {
      hash = hash_append_double (hash, lwpolyline->widths[i].start);
      hash = hash_append_double (hash, lwpolyline->widths[i].end);
    }
  return hash;
}

static unsigned long long
hash_append_hatch (unsigned long long hash, const Dwg_Entity_HATCH *hatch)
{
  BITCODE_BL i;

  hash = hash_append_u64 (hash, hatch->is_gradient_fill);
  hash = hash_append_double (hash, hatch->gradient_angle);
  hash = hash_append_double (hash, hatch->gradient_shift);
  hash = hash_append_u64 (hash, hatch->single_color_gradient);
  hash = hash_append_double (hash, hatch->gradient_tint);
  hash = hash_append_u64 (hash, hatch->num_colors);
  for (i = 0; i < hatch->num_colors; i++)
    {
      hash = hash_append_double (hash, hatch->colors[i].shift_value);
      hash = hash_append_color (hash, hatch->colors[i].color);
    }
  hash
      = hash_append_u64 (hash, hash_text ((const char *)hatch->gradient_name));
  hash = hash_append_double (hash, hatch->elevation);
  hash = hash_append_3bd (hash, hatch->extrusion);
  hash = hash_append_u64 (hash, hash_text ((const char *)hatch->name));
  hash = hash_append_u64 (hash, hatch->is_solid_fill);
  hash = hash_append_u64 (hash, hatch->is_associative);
  hash = hash_append_u64 (hash, hatch->num_paths);
  for (i = 0; i < hatch->num_paths; i++)
    {
      Dwg_HATCH_Path *path = &hatch->paths[i];
      BITCODE_BL j;

      hash = hash_append_u64 (hash, path->flag);
      hash = hash_append_u64 (hash, path->num_segs_or_paths);
      hash = hash_append_u64 (hash, path->bulges_present);
      hash = hash_append_u64 (hash, path->closed);
      hash = hash_append_u64 (hash, path->num_boundary_handles);
      hash = hash_append_u64 (
          hash,
          hash_ref_array (path->boundary_handles, path->num_boundary_handles));
      if (path->flag & 2)
        {
          for (j = 0; j < path->num_segs_or_paths; j++)
            {
              hash = hash_append_2rd (hash, path->polyline_paths[j].point);
              hash = hash_append_double (hash, path->polyline_paths[j].bulge);
            }
        }
      else
        {
          for (j = 0; j < path->num_segs_or_paths; j++)
            {
              Dwg_HATCH_PathSeg *seg = &path->segs[j];
              BITCODE_BL k;

              hash = hash_append_u64 (hash, seg->curve_type);
              hash = hash_append_2rd (hash, seg->first_endpoint);
              hash = hash_append_2rd (hash, seg->second_endpoint);
              hash = hash_append_2rd (hash, seg->center);
              hash = hash_append_double (hash, seg->radius);
              hash = hash_append_double (hash, seg->start_angle);
              hash = hash_append_double (hash, seg->end_angle);
              hash = hash_append_u64 (hash, seg->is_ccw);
              hash = hash_append_2rd (hash, seg->endpoint);
              hash = hash_append_double (hash, seg->minor_major_ratio);
              hash = hash_append_u64 (hash, seg->degree);
              hash = hash_append_u64 (hash, seg->is_rational);
              hash = hash_append_u64 (hash, seg->is_periodic);
              hash = hash_append_u64 (hash, seg->num_knots);
              for (k = 0; k < seg->num_knots; k++)
                hash = hash_append_double (hash, seg->knots[k]);
              hash = hash_append_u64 (hash, seg->num_control_points);
              for (k = 0; k < seg->num_control_points; k++)
                {
                  hash = hash_append_2rd (hash, seg->control_points[k].point);
                  hash = hash_append_double (hash,
                                             seg->control_points[k].weight);
                }
              hash = hash_append_u64 (hash, seg->num_fitpts);
              for (k = 0; k < seg->num_fitpts; k++)
                hash = hash_append_2rd (hash, seg->fitpts[k]);
              hash = hash_append_2rd (hash, seg->start_tangent);
              hash = hash_append_2rd (hash, seg->end_tangent);
            }
        }
    }
  hash = hash_append_u64 (hash, hatch->style);
  hash = hash_append_u64 (hash, hatch->pattern_type);
  hash = hash_append_double (hash, hatch->angle);
  hash = hash_append_double (hash, hatch->scale_spacing);
  hash = hash_append_u64 (hash, hatch->double_flag);
  hash = hash_append_u64 (hash, hatch->num_deflines);
  for (i = 0; i < hatch->num_deflines; i++)
    {
      BITCODE_BS j;

      hash = hash_append_double (hash, hatch->deflines[i].angle);
      hash = hash_append_2bd (hash, hatch->deflines[i].pt0);
      hash = hash_append_2bd (hash, hatch->deflines[i].offset);
      hash = hash_append_u64 (hash, hatch->deflines[i].num_dashes);
      for (j = 0; j < hatch->deflines[i].num_dashes; j++)
        hash = hash_append_double (hash, hatch->deflines[i].dashes[j]);
    }
  hash = hash_append_u64 (hash, hatch->has_derived);
  hash = hash_append_double (hash, hatch->pixel_size);
  hash = hash_append_u64 (hash, hatch->num_seeds);
  for (i = 0; i < hatch->num_seeds; i++)
    hash = hash_append_2rd (hash, hatch->seeds[i]);
  return hash;
}

static unsigned long long
hash_append_wipeout (unsigned long long hash,
                     const Dwg_Entity_WIPEOUT *wipeout)
{
  BITCODE_BL i;

  hash = hash_append_u64 (hash, wipeout->class_version);
  hash = hash_append_3bd (hash, wipeout->pt0);
  hash = hash_append_3bd (hash, wipeout->uvec);
  hash = hash_append_3bd (hash, wipeout->vvec);
  hash = hash_append_2rd (hash, wipeout->image_size);
  hash = hash_append_u64 (hash, wipeout->display_props);
  hash = hash_append_u64 (hash, wipeout->clipping);
  hash = hash_append_u64 (hash, wipeout->brightness);
  hash = hash_append_u64 (hash, wipeout->contrast);
  hash = hash_append_u64 (hash, wipeout->fade);
  hash = hash_append_u64 (hash, wipeout->clip_mode);
  hash = hash_append_u64 (hash, wipeout->clip_boundary_type);
  hash = hash_append_u64 (hash, wipeout->num_clip_verts);
  for (i = 0; i < wipeout->num_clip_verts; i++)
    hash = hash_append_2rd (hash, wipeout->clip_verts[i]);
  hash = hash_append_u64 (hash, ref_absolute (wipeout->imagedef));
  hash = hash_append_u64 (hash, ref_absolute (wipeout->imagedefreactor));
  return hash;
}

static unsigned long long
entity_semantic_hash (const Dwg_Object *obj)
{
  Dwg_Object_Entity *entity;
  unsigned long long hash = 1469598103934665603ULL;

  if (!obj || obj->supertype != DWG_SUPERTYPE_ENTITY || !obj->tio.entity)
    return 0;

  entity = obj->tio.entity;
  hash = hash_append_common_entity (hash, entity);
  switch (obj->fixedtype)
    {
    case DWG_TYPE_TEXT:
      if (entity->tio.TEXT)
        {
          Dwg_Entity_TEXT *text = entity->tio.TEXT;
          hash = hash_append_2bd (hash, text->ins_pt);
          hash = hash_append_2bd (hash, text->alignment_pt);
          hash = hash_append_3bd (hash, text->extrusion);
          hash = hash_append_double (hash, text->elevation);
          hash = hash_append_double (hash, text->thickness);
          hash = hash_append_double (hash, text->oblique_angle);
          hash = hash_append_double (hash, text->rotation);
          hash = hash_append_double (hash, text->height);
          hash = hash_append_double (hash, text->width_factor);
          hash = hash_append_u64 (
              hash, utf8_field_hash (text, "TEXT", "text_value"));
          hash = hash_append_u64 (hash, text->generation);
          hash = hash_append_u64 (hash, text->horiz_alignment);
          hash = hash_append_u64 (hash, text->vert_alignment);
          hash = hash_append_u64 (hash, ref_absolute (text->style));
        }
      break;
    case DWG_TYPE_ATTRIB:
      if (entity->tio.ATTRIB)
        {
          Dwg_Entity_ATTRIB *attrib = entity->tio.ATTRIB;
          hash = hash_append_2bd (hash, attrib->ins_pt);
          hash = hash_append_2bd (hash, attrib->alignment_pt);
          hash = hash_append_3bd (hash, attrib->extrusion);
          hash = hash_append_double (hash, attrib->elevation);
          hash = hash_append_double (hash, attrib->thickness);
          hash = hash_append_double (hash, attrib->oblique_angle);
          hash = hash_append_double (hash, attrib->rotation);
          hash = hash_append_double (hash, attrib->height);
          hash = hash_append_double (hash, attrib->width_factor);
          hash = hash_append_u64 (
              hash, utf8_field_hash (attrib, "ATTRIB", "text_value"));
          hash = hash_append_u64 (hash,
                                  utf8_field_hash (attrib, "ATTRIB", "tag"));
          hash = hash_append_u64 (hash, attrib->flags);
          hash = hash_append_u64 (hash, ref_absolute (attrib->style));
        }
      break;
    case DWG_TYPE_ATTDEF:
      if (entity->tio.ATTDEF)
        {
          Dwg_Entity_ATTDEF *attdef = entity->tio.ATTDEF;
          hash = hash_append_2bd (hash, attdef->ins_pt);
          hash = hash_append_2bd (hash, attdef->alignment_pt);
          hash = hash_append_3bd (hash, attdef->extrusion);
          hash = hash_append_double (hash, attdef->elevation);
          hash = hash_append_double (hash, attdef->thickness);
          hash = hash_append_double (hash, attdef->oblique_angle);
          hash = hash_append_double (hash, attdef->rotation);
          hash = hash_append_double (hash, attdef->height);
          hash = hash_append_double (hash, attdef->width_factor);
          hash = hash_append_u64 (
              hash, utf8_field_hash (attdef, "ATTDEF", "default_value"));
          hash = hash_append_u64 (hash,
                                  utf8_field_hash (attdef, "ATTDEF", "tag"));
          hash = hash_append_u64 (
              hash, utf8_field_hash (attdef, "ATTDEF", "prompt"));
          hash = hash_append_u64 (hash, attdef->flags);
          hash = hash_append_u64 (hash, ref_absolute (attdef->style));
        }
      break;
    case DWG_TYPE_INSERT:
      if (entity->tio.INSERT)
        hash = hash_append_insert (hash, entity->tio.INSERT);
      break;
    case DWG_TYPE_MINSERT:
      if (entity->tio.MINSERT)
        hash = hash_append_minsert (hash, entity->tio.MINSERT);
      break;
    case DWG_TYPE_LINE:
      if (entity->tio.LINE)
        {
          Dwg_Entity_LINE *line = entity->tio.LINE;
          hash = hash_append_u64 (hash, line->z_is_zero);
          hash = hash_append_3bd (hash, line->start);
          hash = hash_append_3bd (hash, line->end);
          hash = hash_append_double (hash, line->thickness);
          hash = hash_append_3bd (hash, line->extrusion);
        }
      break;
    case DWG_TYPE_MTEXT:
      if (entity->tio.MTEXT)
        {
          Dwg_Entity_MTEXT *mtext = entity->tio.MTEXT;
          BITCODE_BL i;

          hash = hash_append_3bd (hash, mtext->ins_pt);
          hash = hash_append_3bd (hash, mtext->extrusion);
          hash = hash_append_3bd (hash, mtext->x_axis_dir);
          hash = hash_append_double (hash, mtext->rect_height);
          hash = hash_append_double (hash, mtext->rect_width);
          hash = hash_append_double (hash, mtext->text_height);
          hash = hash_append_u64 (hash, mtext->attachment);
          hash = hash_append_u64 (hash, mtext->flow_dir);
          hash = hash_append_double (hash, mtext->extents_width);
          hash = hash_append_double (hash, mtext->extents_height);
          hash = hash_append_u64 (hash,
                                  utf8_field_hash (mtext, "MTEXT", "text"));
          hash = hash_append_u64 (hash, ref_absolute (mtext->style));
          hash = hash_append_u64 (hash, mtext->linespace_style);
          hash = hash_append_double (hash, mtext->linespace_factor);
          hash = hash_append_u64 (hash, mtext->bg_fill_flag);
          hash = hash_append_u64 (hash, mtext->bg_fill_scale);
          hash = hash_append_color (hash, mtext->bg_fill_color);
          hash = hash_append_u64 (hash, mtext->bg_fill_trans);
          hash = hash_append_u64 (hash, ref_absolute (mtext->appid));
          hash = hash_append_u64 (hash, mtext->column_type);
          hash = hash_append_double (hash, mtext->column_width);
          hash = hash_append_double (hash, mtext->gutter);
          hash = hash_append_u64 (hash, mtext->num_column_heights);
          for (i = 0; i < mtext->num_column_heights; i++)
            hash = hash_append_double (hash, mtext->column_heights[i]);
        }
      break;
    case DWG_TYPE_DIMENSION_ALIGNED:
    case DWG_TYPE_DIMENSION_ANG2LN:
    case DWG_TYPE_DIMENSION_ANG3PT:
    case DWG_TYPE_DIMENSION_DIAMETER:
    case DWG_TYPE_DIMENSION_LINEAR:
    case DWG_TYPE_DIMENSION_ORDINATE:
    case DWG_TYPE_DIMENSION_RADIUS:
    case DWG_TYPE_ARC_DIMENSION:
    case DWG_TYPE_LARGE_RADIAL_DIMENSION:
      if (entity->tio.DIMENSION_common)
        hash = hash_append_dimension_common (hash,
                                             entity->tio.DIMENSION_common);
      break;
    case DWG_TYPE_VERTEX_2D:
      if (entity->tio.VERTEX_2D)
        {
          Dwg_Entity_VERTEX_2D *vertex = entity->tio.VERTEX_2D;
          hash = hash_append_u64 (hash, vertex->flag);
          hash = hash_append_3bd (hash, vertex->point);
          hash = hash_append_double (hash, vertex->start_width);
          hash = hash_append_double (hash, vertex->end_width);
          hash = hash_append_u64 (hash, vertex->id);
          hash = hash_append_double (hash, vertex->bulge);
          hash = hash_append_double (hash, vertex->tangent_dir);
        }
      break;
    case DWG_TYPE_VERTEX_3D:
    case DWG_TYPE_VERTEX_MESH:
    case DWG_TYPE_VERTEX_PFACE:
      if (entity->tio.VERTEX_3D)
        {
          hash = hash_append_u64 (hash, entity->tio.VERTEX_3D->flag);
          hash = hash_append_3bd (hash, entity->tio.VERTEX_3D->point);
        }
      break;
    case DWG_TYPE_VERTEX_PFACE_FACE:
      if (entity->tio.VERTEX_PFACE_FACE)
        {
          Dwg_Entity_VERTEX_PFACE_FACE *face = entity->tio.VERTEX_PFACE_FACE;
          int i;

          hash = hash_append_u64 (hash, face->flag);
          for (i = 0; i < 4; i++)
            hash = hash_append_u64 (
                hash, (unsigned long long)(long long)face->vertind[i]);
        }
      break;
    case DWG_TYPE_LWPOLYLINE:
      if (entity->tio.LWPOLYLINE)
        hash = hash_append_lwpolyline (hash, entity->tio.LWPOLYLINE);
      break;
    case DWG_TYPE_HATCH:
      if (entity->tio.HATCH)
        hash = hash_append_hatch (hash, entity->tio.HATCH);
      break;
    case DWG_TYPE_WIPEOUT:
      if (entity->tio.WIPEOUT)
        hash = hash_append_wipeout (hash, entity->tio.WIPEOUT);
      break;
    default:
      break;
    }
  return hash;
}

static void
snapshot_block_header_refs (const Dwg_Object *obj,
                            Stream_Ref_Snapshot *snapshot)
{
  Dwg_Object_BLOCK_HEADER *block;

  if (!obj || !obj->tio.object || !obj->tio.object->tio.BLOCK_HEADER
      || !snapshot)
    return;

  block = obj->tio.object->tio.BLOCK_HEADER;
  snapshot->num_owned = block->num_owned;
  snapshot->block_base_x = block->base_pt.x;
  snapshot->block_base_y = block->base_pt.y;
  snapshot->block_base_z = block->base_pt.z;
  snapshot->block_entity = ref_absolute (block->block_entity);
  snapshot->first_entity = ref_absolute (block->first_entity);
  snapshot->last_entity = ref_absolute (block->last_entity);
  snapshot->endblk_entity = ref_absolute (block->endblk_entity);
  snapshot->block_entities_mix
      = hash_ref_array (block->entities, block->num_owned);
  snapshot->block_num_inserts = block->num_inserts;
  snapshot->block_inserts_mix
      = hash_ref_array (block->inserts, block->num_inserts);
  snapshot->block_header_name_hash
      = utf8_field_hash (block, "BLOCK_HEADER", "name");
}

static void
snapshot_polyline_refs (const Dwg_Object *obj, Stream_Ref_Snapshot *snapshot)
{
  Dwg_Object_Entity *entity;

  if (!obj || !obj->tio.entity || !snapshot)
    return;
  entity = obj->tio.entity;
  switch (obj->fixedtype)
    {
    case DWG_TYPE_POLYLINE_2D:
      if (entity->tio.POLYLINE_2D)
        {
          snapshot->num_owned = entity->tio.POLYLINE_2D->num_owned;
          snapshot->first_vertex
              = ref_absolute (entity->tio.POLYLINE_2D->first_vertex);
          snapshot->last_vertex
              = ref_absolute (entity->tio.POLYLINE_2D->last_vertex);
          snapshot->owned_refs_mix
              = hash_ref_array (entity->tio.POLYLINE_2D->vertex,
                                entity->tio.POLYLINE_2D->num_owned);
          snapshot->seqend = ref_absolute (entity->tio.POLYLINE_2D->seqend);
        }
      break;
    case DWG_TYPE_POLYLINE_3D:
      if (entity->tio.POLYLINE_3D)
        {
          snapshot->num_owned = entity->tio.POLYLINE_3D->num_owned;
          snapshot->first_vertex
              = ref_absolute (entity->tio.POLYLINE_3D->first_vertex);
          snapshot->last_vertex
              = ref_absolute (entity->tio.POLYLINE_3D->last_vertex);
          snapshot->owned_refs_mix
              = hash_ref_array (entity->tio.POLYLINE_3D->vertex,
                                entity->tio.POLYLINE_3D->num_owned);
          snapshot->seqend = ref_absolute (entity->tio.POLYLINE_3D->seqend);
        }
      break;
    case DWG_TYPE_POLYLINE_MESH:
      if (entity->tio.POLYLINE_MESH)
        {
          snapshot->num_owned = entity->tio.POLYLINE_MESH->num_owned;
          snapshot->first_vertex
              = ref_absolute (entity->tio.POLYLINE_MESH->first_vertex);
          snapshot->last_vertex
              = ref_absolute (entity->tio.POLYLINE_MESH->last_vertex);
          snapshot->owned_refs_mix
              = hash_ref_array (entity->tio.POLYLINE_MESH->vertex,
                                entity->tio.POLYLINE_MESH->num_owned);
          snapshot->seqend = ref_absolute (entity->tio.POLYLINE_MESH->seqend);
        }
      break;
    case DWG_TYPE_POLYLINE_PFACE:
      if (entity->tio.POLYLINE_PFACE)
        {
          snapshot->num_owned = entity->tio.POLYLINE_PFACE->num_owned;
          snapshot->first_vertex
              = ref_absolute (entity->tio.POLYLINE_PFACE->first_vertex);
          snapshot->last_vertex
              = ref_absolute (entity->tio.POLYLINE_PFACE->last_vertex);
          snapshot->owned_refs_mix
              = hash_ref_array (entity->tio.POLYLINE_PFACE->vertex,
                                entity->tio.POLYLINE_PFACE->num_owned);
          snapshot->seqend = ref_absolute (entity->tio.POLYLINE_PFACE->seqend);
        }
      break;
    default:
      break;
    }
}

static void
snapshot_object_refs (const Dwg_Object *obj, Stream_Ref_Snapshot *snapshot)
{
  Dwg_Object_Entity *entity;

  memset (snapshot, 0, sizeof (*snapshot));
  if (!obj)
    return;
  snapshot->handle = obj->handle.value;
  snapshot->fixedtype = obj->fixedtype;
  snapshot->supertype = obj->supertype;
  if (obj->fixedtype == DWG_TYPE_BLOCK_HEADER)
    {
      snapshot_block_header_refs (obj, snapshot);
      return;
    }
  if (obj->supertype != DWG_SUPERTYPE_ENTITY || !obj->tio.entity)
    return;

  entity = obj->tio.entity;
  snapshot->semantic_hash = entity_semantic_hash (obj);
  snapshot->entmode = entity->entmode;
  snapshot->ownerhandle = ref_absolute (entity->ownerhandle);
  snapshot->layer = ref_absolute (entity->layer);
  if (obj->fixedtype == DWG_TYPE_INSERT && entity->tio.INSERT)
    {
      snapshot->block_header = ref_absolute (entity->tio.INSERT->block_header);
      snapshot->num_owned = entity->tio.INSERT->num_owned;
    }
  else if (obj->fixedtype == DWG_TYPE_MINSERT && entity->tio.MINSERT)
    {
      snapshot->block_header
          = ref_absolute (entity->tio.MINSERT->block_header);
      snapshot->num_owned = entity->tio.MINSERT->num_owned;
    }
  else if (obj->fixedtype == DWG_TYPE_DIMENSION_ALIGNED
           || obj->fixedtype == DWG_TYPE_DIMENSION_ANG2LN
           || obj->fixedtype == DWG_TYPE_DIMENSION_ANG3PT
           || obj->fixedtype == DWG_TYPE_DIMENSION_DIAMETER
           || obj->fixedtype == DWG_TYPE_DIMENSION_LINEAR
           || obj->fixedtype == DWG_TYPE_DIMENSION_ORDINATE
           || obj->fixedtype == DWG_TYPE_DIMENSION_RADIUS
           || obj->fixedtype == DWG_TYPE_ARC_DIMENSION
           || obj->fixedtype == DWG_TYPE_LARGE_RADIAL_DIMENSION)
    {
      Dwg_DIMENSION_common *dimension = entity->tio.DIMENSION_common;
      snapshot->dimension_block
          = dimension ? ref_absolute (dimension->block) : 0;
    }
  else
    snapshot_polyline_refs (obj, snapshot);
}

static int
compare_ref_snapshot_key (const void *a, const void *b)
{
  const Stream_Ref_Snapshot *left = (const Stream_Ref_Snapshot *)a;
  const Stream_Ref_Snapshot *right = (const Stream_Ref_Snapshot *)b;

  if (left->handle < right->handle)
    return -1;
  if (left->handle > right->handle)
    return 1;
  if (left->fixedtype < right->fixedtype)
    return -1;
  if (left->fixedtype > right->fixedtype)
    return 1;
  if (left->supertype < right->supertype)
    return -1;
  if (left->supertype > right->supertype)
    return 1;
  return 0;
}

static const Stream_Ref_Snapshot *
find_ref_snapshot (const Stream_Stats *stats, const Stream_Ref_Snapshot *key)
{
  if (!stats || !stats->baseline_refs || !stats->baseline_ref_count)
    return NULL;
  return (const Stream_Ref_Snapshot *)bsearch (
      key, stats->baseline_refs, stats->baseline_ref_count,
      sizeof (stats->baseline_refs[0]), compare_ref_snapshot_key);
}

static int
ref_snapshots_equal (const Stream_Ref_Snapshot *a,
                     const Stream_Ref_Snapshot *b)
{
  return a->handle == b->handle && a->fixedtype == b->fixedtype
         && a->supertype == b->supertype && a->entmode == b->entmode
         && a->ownerhandle == b->ownerhandle && a->layer == b->layer
         && a->block_header == b->block_header
         && a->dimension_block == b->dimension_block
         && a->num_owned == b->num_owned && a->first_vertex == b->first_vertex
         && a->last_vertex == b->last_vertex
         && a->owned_refs_mix == b->owned_refs_mix && a->seqend == b->seqend
         && a->block_entity == b->block_entity
         && a->first_entity == b->first_entity
         && a->last_entity == b->last_entity
         && a->endblk_entity == b->endblk_entity
         && a->block_entities_mix == b->block_entities_mix
         && a->block_num_inserts == b->block_num_inserts
         && a->block_inserts_mix == b->block_inserts_mix
         && a->block_header_name_hash == b->block_header_name_hash
         && a->semantic_hash == b->semantic_hash
         && a->block_base_x == b->block_base_x
         && a->block_base_y == b->block_base_y
         && a->block_base_z == b->block_base_z;
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
abort_decoded_object_callback (const Dwg_Stream_Object_Info *info,
                               const Dwg_Object *object, void *user)
{
  Abort_Stats *stats = (Abort_Stats *)user;

  (void)info;
  (void)object;
  stats->calls++;
  if (stats->calls >= stats->limit)
    return stats->error;
  return 0;
}

static int
stream_decoded_object_callback (const Dwg_Stream_Object_Info *info,
                                const Dwg_Object *object, void *user)
{
  Stream_Stats *stats = (Stream_Stats *)user;
  const Stream_Ref_Snapshot *baseline_ref;
  Stream_Ref_Snapshot decoded_ref;
  BITCODE_BL host_entities = 0;
  BITCODE_BL i;

  if (!object)
    return DWG_ERR_INTERNALERROR;
  if (object->parent)
    {
      for (i = 0; i < object->parent->num_objects; i++)
        {
          if (object->parent->object[i].supertype == DWG_SUPERTYPE_ENTITY)
            host_entities++;
        }
      if (host_entities > stats->max_host_entities)
        stats->max_host_entities = host_entities;
    }
  stats->decoded_objects++;
  stats_add_r11_block_section_type (stats, info, object);
  stats_add_r11_polyline_fixedtype (stats, object);
  stats_add_r11_vertex_fixedtype (stats, object);
  stats_add_r11_dimension_fixedtype (stats, info, object);
  if (info && info->decode_mode == DWG_STREAM_DECODE_PRER13_ENTITY
      && object->supertype == DWG_SUPERTYPE_ENTITY)
    stats->prer2_legacy_fixedtype_mask
        |= pre_r2_legacy_entity_bit (object->fixedtype);
  stats_add_semantic_coverage (stats, object);
  if (object->supertype == DWG_SUPERTYPE_ENTITY)
    stats->decoded_entities++;
  else
    stats->decoded_non_entities++;
  stats->decoded_handle_mix ^= (unsigned long long)object->handle.value
                               + ((unsigned long long)object->type << 33)
                               + ((unsigned long long)object->supertype << 49);
  if ((info->decode_mode != DWG_STREAM_DECODE_R2004_OBJECT_MAP
       && object->handle.value != info->handle.value)
      || object->type != info->type || object->supertype != info->supertype)
    {
      printf ("decoded object mismatch: info handle=%llu type=%u super=%u; "
              "object handle=%llu type=%u super=%u\n",
              (unsigned long long)info->handle.value, (unsigned)info->type,
              (unsigned)info->supertype,
              (unsigned long long)object->handle.value, (unsigned)object->type,
              (unsigned)object->supertype);
      return DWG_ERR_INTERNALERROR;
    }
  if (stats->baseline_refs && object->handle.value)
    {
      snapshot_object_refs (object, &decoded_ref);
      baseline_ref = find_ref_snapshot (stats, &decoded_ref);
      if (!baseline_ref)
        stats->decoded_ref_missing++;
      else
        {
          stats->decoded_ref_checked++;
          if (!ref_snapshots_equal (baseline_ref, &decoded_ref))
            {
              stats->decoded_ref_mismatches++;
              if (stats->decoded_ref_mismatches <= 25)
                printf (
                    "decoded ref mismatch handle=%llu type=%u "
                    "owner=%llu/%llu layer=%llu/%llu block=%llu/%llu "
                    "dimblock=%llu/%llu owned=%lu/%lu first=%llu/%llu "
                    "last=%llu/%llu owned_refs=%llu/%llu "
                    "seqend=%llu/%llu entmode=%u/%u sem=%llu/%llu\n",
                    (unsigned long long)object->handle.value,
                    (unsigned)object->fixedtype,
                    (unsigned long long)baseline_ref->ownerhandle,
                    (unsigned long long)decoded_ref.ownerhandle,
                    (unsigned long long)baseline_ref->layer,
                    (unsigned long long)decoded_ref.layer,
                    (unsigned long long)baseline_ref->block_header,
                    (unsigned long long)decoded_ref.block_header,
                    (unsigned long long)baseline_ref->dimension_block,
                    (unsigned long long)decoded_ref.dimension_block,
                    (unsigned long)baseline_ref->num_owned,
                    (unsigned long)decoded_ref.num_owned,
                    (unsigned long long)baseline_ref->first_vertex,
                    (unsigned long long)decoded_ref.first_vertex,
                    (unsigned long long)baseline_ref->last_vertex,
                    (unsigned long long)decoded_ref.last_vertex,
                    baseline_ref->owned_refs_mix, decoded_ref.owned_refs_mix,
                    (unsigned long long)baseline_ref->seqend,
                    (unsigned long long)decoded_ref.seqend,
                    (unsigned)baseline_ref->entmode,
                    (unsigned)decoded_ref.entmode, baseline_ref->semantic_hash,
                    decoded_ref.semantic_hash);
              if (object->fixedtype == DWG_TYPE_BLOCK_HEADER)
                printf ("decoded block mismatch handle=%llu "
                        "blockent=%llu/%llu first=%llu/%llu "
                        "last=%llu/%llu endblk=%llu/%llu "
                        "entities=%llu/%llu inserts=%lu/%lu "
                        "insert_refs=%llu/%llu name=%llu/%llu "
                        "base=(%.17g,%.17g,%.17g)/"
                        "(%.17g,%.17g,%.17g)\n",
                        (unsigned long long)object->handle.value,
                        (unsigned long long)baseline_ref->block_entity,
                        (unsigned long long)decoded_ref.block_entity,
                        (unsigned long long)baseline_ref->first_entity,
                        (unsigned long long)decoded_ref.first_entity,
                        (unsigned long long)baseline_ref->last_entity,
                        (unsigned long long)decoded_ref.last_entity,
                        (unsigned long long)baseline_ref->endblk_entity,
                        (unsigned long long)decoded_ref.endblk_entity,
                        baseline_ref->block_entities_mix,
                        decoded_ref.block_entities_mix,
                        (unsigned long)baseline_ref->block_num_inserts,
                        (unsigned long)decoded_ref.block_num_inserts,
                        baseline_ref->block_inserts_mix,
                        decoded_ref.block_inserts_mix,
                        baseline_ref->block_header_name_hash,
                        decoded_ref.block_header_name_hash,
                        baseline_ref->block_base_x, baseline_ref->block_base_y,
                        baseline_ref->block_base_z, decoded_ref.block_base_x,
                        decoded_ref.block_base_y, decoded_ref.block_base_z);
            }
        }
    }
  return 0;
}

static int
r11_minsert_opts_decoded_object_callback (const Dwg_Stream_Object_Info *info,
                                          const Dwg_Object *object, void *user)
{
  R11_Minsert_Opts_Stats *capture = (R11_Minsert_Opts_Stats *)user;
  int error;

  error = stream_decoded_object_callback (info, object, &capture->stats);
  if (error)
    return error;
  if (object && object->fixedtype == DWG_TYPE_INSERT
      && object->type == DWG_TYPE_INSERT_r11 && object->tio.entity
      && object->tio.entity->tio.INSERT)
    {
      Dwg_Entity_INSERT *insert = object->tio.entity->tio.INSERT;
      if (insert->num_cols == 3 && insert->num_rows == 2
          && insert->col_spacing == 2.5 && insert->row_spacing == 1.25)
        {
          capture->found++;
        }
    }
  return 0;
}

static int
stream_decode_error_callback (const Dwg_Stream_Object_Info *info, int error,
                              void *user)
{
  Stream_Stats *stats = (Stream_Stats *)user;

  if (!info)
    return DWG_ERR_INTERNALERROR;
  stats->decode_error_objects++;
  if (info->supertype == DWG_SUPERTYPE_ENTITY)
    stats->decode_error_entities++;
  else
    stats->decode_error_non_entities++;
  if (!stats->first_decode_error)
    stats->first_decode_error = error;
  stats->decode_error_handle_mix
      ^= (unsigned long long)info->handle.value
         + ((unsigned long long)info->type << 33)
         + ((unsigned long long)info->supertype << 49);
  stream_record_decode_error_bucket (stats, info, error);
  return 0;
}

static int
emit_capacity_decoded_object_callback (const Dwg_Stream_Object_Info *info,
                                       const Dwg_Object *object, void *user)
{
  Emit_Capacity_Stats *stats = (Emit_Capacity_Stats *)user;

  (void)info;
  (void)object;
  stats->decoded_calls++;
  return DWG_ERR_INTERNALERROR;
}

static int
emit_capacity_decode_error_callback (const Dwg_Stream_Object_Info *info,
                                     int error, void *user)
{
  Emit_Capacity_Stats *stats = (Emit_Capacity_Stats *)user;

  stats->decode_error_calls++;
  stats->error = error;
  stats->info = info;
  return 0;
}

static void
stream_record_decode_error_bucket (Stream_Stats *stats,
                                   const Dwg_Stream_Object_Info *info,
                                   int error)
{
  unsigned int i;

  for (i = 0; i < STREAM_DECODE_ERROR_BUCKETS; i++)
    {
      Stream_Decode_Error_Bucket *bucket = &stats->decode_error_buckets[i];
      if (bucket->used && bucket->error == error && bucket->type == info->type
          && bucket->supertype == info->supertype)
        {
          bucket->count++;
          return;
        }
    }

  for (i = 0; i < STREAM_DECODE_ERROR_BUCKETS; i++)
    {
      Stream_Decode_Error_Bucket *bucket = &stats->decode_error_buckets[i];
      if (!bucket->used)
        {
          bucket->used = 1;
          bucket->error = error;
          bucket->type = info->type;
          bucket->supertype = info->supertype;
          bucket->count = 1;
          bucket->first_handle = info->handle.value;
          bucket->first_address = info->address;
          bucket->first_size = info->size;
          bucket->first_index = info->index;
          if (info->name)
            snprintf (bucket->name, sizeof (bucket->name), "%s", info->name);
          if (info->dxfname)
            snprintf (bucket->dxfname, sizeof (bucket->dxfname), "%s",
                      info->dxfname);
          return;
        }
    }

  stats->decode_error_unbucketed++;
}

static int
test_emit_decoded_object_isolated_host_state (void)
{
  Dwg_Data dwg = { 0 };
  Bit_Chain object_dat = { 0 };
  Dwg_Stream_Object_Info info = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Emit_Capacity_Stats stats = { 0 };
  Dwg_Object object_pool[1];
  Dwg_Object_Ref handseed = { 0 };
  Dwg_Object_Ref *host_ref = NULL;
  Dwg_Object_Ref **old_object_ref = &host_ref;
  int old_dirty_refs = 1;
  int error;

  memset (object_pool, 0, sizeof (object_pool));
  dwg.object = object_pool;
  dwg.num_objects = 1;
  dwg.num_alloced_objects = 1;
  dwg.object_ref = old_object_ref;
  dwg.header_vars.HANDSEED = &handseed;
  dwg.dirty_refs = old_dirty_refs;
  info.handle.value = 0x1234;
  info.type = DWG_TYPE_LINE;
  info.supertype = DWG_SUPERTYPE_ENTITY;
  callbacks.decoded_object = emit_capacity_decoded_object_callback;
  callbacks.decode_error = emit_capacity_decode_error_callback;

  error = dwg_stream_emit_decoded_object (&dwg, &object_dat, &info, &callbacks,
                                          &stats);
  if (error || stats.decoded_calls || stats.decode_error_calls != 1
      || stats.error != DWG_ERR_VALUEOUTOFBOUNDS || stats.info != &info
      || dwg.object != object_pool || dwg.num_objects != 1
      || dwg.num_alloced_objects != 1 || dwg.object_ref != old_object_ref
      || dwg.num_object_refs || dwg.header_vars.HANDSEED != &handseed
      || dwg.dirty_refs != old_dirty_refs || dwg.object_map)
    {
      printf ("emit decoded isolated host state failed: error=0x%x "
              "decoded=%lu decode_errors=%lu callback_error=0x%x "
              "objects=%lu alloced=%lu refs=%lu restored=%d\n",
              error, (unsigned long)stats.decoded_calls,
              (unsigned long)stats.decode_error_calls, stats.error,
              (unsigned long)dwg.num_objects,
              (unsigned long)dwg.num_alloced_objects,
              (unsigned long)dwg.num_object_refs,
              dwg.object == object_pool && dwg.object_ref == old_object_ref
                  && dwg.header_vars.HANDSEED == &handseed
                  && dwg.dirty_refs == old_dirty_refs && !dwg.object_map);
      return 1;
    }
  return 0;
}

static int
test_no_full_fallback (void)
{
  char path[128];
  Stream_Stats stats = { 0 };
  Stream_Stats default_stats = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  int error;

  snprintf (path, sizeof (path), "stream_invalid_version_fixture_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));

  if (!write_invalid_version_fixture (path, "AC9999"))
    {
      printf ("failed to create invalid-version streaming fixture\n");
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  callbacks.flags = 0;
  error = dwg_stream_file_ex (path, &callbacks, &default_stats);
  if (error != DWG_ERR_INVALIDDWG || default_stats.num_objects
      || default_stats.decoded_objects || default_stats.decode_error_objects)
    {
      remove (path);
      printf (
          "default invalid-version stream failed: error=0x%x expected=0x%x "
          "objects=%lu decoded=%lu decode_errors=%lu\n",
          error, DWG_ERR_INVALIDDWG, (unsigned long)default_stats.num_objects,
          (unsigned long)default_stats.decoded_objects,
          (unsigned long)default_stats.decode_error_objects);
      return 1;
    }

  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  remove (path);
  if (error != DWG_ERR_INVALIDDWG || stats.num_objects || stats.decoded_objects
      || stats.decode_error_objects)
    {
      printf ("no-full-fallback failed: error=0x%x expected=0x%x "
              "objects=%lu decoded=%lu decode_errors=%lu\n",
              error, DWG_ERR_INVALIDDWG, (unsigned long)stats.num_objects,
              (unsigned long)stats.decoded_objects,
              (unsigned long)stats.decode_error_objects);
      return 1;
    }
  return 0;
}

static int
test_stream_api_invalid_args (void)
{
  Dwg_Stream_Callbacks callbacks = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks_ex = { 0 };
  int error;

  error = dwg_stream_file (NULL, &callbacks, NULL);
  if (error != DWG_ERR_INTERNALERROR)
    {
      printf ("dwg_stream_file NULL filename failed: error=0x%x\n", error);
      return 1;
    }

  error = dwg_stream_file ("missing.dwg", NULL, NULL);
  if (error != DWG_ERR_INTERNALERROR)
    {
      printf ("dwg_stream_file NULL callbacks failed: error=0x%x\n", error);
      return 1;
    }

  error = dwg_stream_file_ex (NULL, &callbacks_ex, NULL);
  if (error != DWG_ERR_INTERNALERROR)
    {
      printf ("dwg_stream_file_ex NULL filename failed: error=0x%x\n", error);
      return 1;
    }

  error = dwg_stream_file_ex ("missing.dwg", NULL, NULL);
  if (error != DWG_ERR_INTERNALERROR)
    {
      printf ("dwg_stream_file_ex NULL callbacks failed: error=0x%x\n", error);
      return 1;
    }

  return 0;
}

static int
test_legacy_callback_initializer (void)
{
#if defined __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
  Dwg_Stream_Callbacks callbacks
      = { stream_object_callback, DWG_STREAM_F_NO_FULL_FALLBACK };
#if defined __GNUC__
#  pragma GCC diagnostic pop
#endif

  if (callbacks.object != stream_object_callback
      || callbacks.flags != DWG_STREAM_F_NO_FULL_FALLBACK)
    {
      printf ("legacy callback initializer mismatch\n");
      return 1;
    }
  return 0;
}

static int
test_legacy_stream_file_api (void)
{
  char path[1024];
  char invalid_path[128];
  char r2010_path[1024];
  char r2013_path[1024];
  Stream_Stats stats = { 0 };
  Stream_Stats r2010_stats = { 0 };
  Stream_Stats r2013_stats = { 0 };
  Stream_Stats invalid_stats = { 0 };
  Stream_Stats no_fallback_stats = { 0 };
  Abort_Stats aborted = { 0 };
  Dwg_Stream_Callbacks callbacks = { 0 };
  int error;

  if (!stream_test_source_path (path, sizeof (path),
                                "test/test-data/example_r13.dwg"))
    {
      printf ("legacy stream file fixture path too long\n");
      return 1;
    }
  if (!stream_test_source_path (r2010_path, sizeof (r2010_path),
                                "test/test-data/example_2010.dwg"))
    {
      printf ("legacy R2010 fixture path too long\n");
      return 1;
    }
  if (!stream_test_source_path (r2013_path, sizeof (r2013_path),
                                "test/test-data/example_2013.dwg"))
    {
      printf ("legacy R2013 fixture path too long\n");
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file (path, &callbacks, &stats);
  if (error >= DWG_ERR_CRITICAL || !stats.num_objects
      || stats.full_decode_objects || stats.decoded_objects
      || stats.decode_error_objects)
    {
      printf ("legacy stream file API failed: error=0x%x objects=%lu "
              "full=%lu decoded=%lu decode_errors=%lu\n",
              error, (unsigned long)stats.num_objects,
              (unsigned long)stats.full_decode_objects,
              (unsigned long)stats.decoded_objects,
              (unsigned long)stats.decode_error_objects);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = 0;
  error = dwg_stream_file (r2013_path, &callbacks, &r2013_stats);
  if (error >= DWG_ERR_CRITICAL || !r2013_stats.num_objects
      || r2013_stats.full_decode_objects
      || r2013_stats.lightweight_objects != r2013_stats.num_objects)
    {
      printf ("legacy R2013 stream failed: error=0x%x objects=%lu "
              "full=%lu lightweight=%lu\n",
              error, (unsigned long)r2013_stats.num_objects,
              (unsigned long)r2013_stats.full_decode_objects,
              (unsigned long)r2013_stats.lightweight_objects);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = 0;
  error = dwg_stream_file (r2010_path, &callbacks, &r2010_stats);
  if (error >= DWG_ERR_CRITICAL || !r2010_stats.num_objects
      || r2010_stats.full_decode_objects
      || r2010_stats.lightweight_objects != r2010_stats.num_objects)
    {
      printf ("legacy R2010 stream failed: error=0x%x objects=%lu "
              "full=%lu lightweight=%lu\n",
              error, (unsigned long)r2010_stats.num_objects,
              (unsigned long)r2010_stats.full_decode_objects,
              (unsigned long)r2010_stats.lightweight_objects);
      return 1;
    }

  snprintf (invalid_path, sizeof (invalid_path),
            "stream_legacy_invalid_version_fixture_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));
  if (!write_invalid_version_fixture (invalid_path, "AC9999"))
    {
      printf ("failed to create legacy invalid-version streaming fixture\n");
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = 0;
  error = dwg_stream_file (invalid_path, &callbacks, &invalid_stats);
  if (error != DWG_ERR_INVALIDDWG || invalid_stats.num_objects
      || invalid_stats.full_decode_objects
      || invalid_stats.lightweight_objects)
    {
      printf ("legacy invalid-version stream failed: error=0x%x expected=0x%x "
              "objects=%lu full=%lu lightweight=%lu\n",
              error, DWG_ERR_INVALIDDWG,
              (unsigned long)invalid_stats.num_objects,
              (unsigned long)invalid_stats.full_decode_objects,
              (unsigned long)invalid_stats.lightweight_objects);
      remove (invalid_path);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file (invalid_path, &callbacks, &no_fallback_stats);
  if (error != DWG_ERR_INVALIDDWG || no_fallback_stats.num_objects
      || no_fallback_stats.full_decode_objects
      || no_fallback_stats.lightweight_objects)
    {
      printf ("legacy no-full-fallback failed: error=0x%x expected=0x%x "
              "objects=%lu full=%lu lightweight=%lu\n",
              error, DWG_ERR_INVALIDDWG,
              (unsigned long)no_fallback_stats.num_objects,
              (unsigned long)no_fallback_stats.full_decode_objects,
              (unsigned long)no_fallback_stats.lightweight_objects);
      remove (invalid_path);
      return 1;
    }
  remove (invalid_path);

  aborted.limit = 3;
  aborted.error = DWG_ERR_NOTYETSUPPORTED;
  callbacks.object = abort_object_callback;
  callbacks.flags = 0;
  error = dwg_stream_file (path, &callbacks, &aborted);
  if (error != aborted.error || aborted.calls != aborted.limit)
    {
      printf ("legacy callback not-supported-bit abort failed: error=0x%x "
              "expected=0x%x calls=%lu expected_calls=%lu\n",
              error, aborted.error, (unsigned long)aborted.calls,
              (unsigned long)aborted.limit);
      return 1;
    }

  return 0;
}

static int
test_invalid_version_stream_file_ex_rejects (void)
{
  char path[128];
  Stream_Stats combined = { 0 };
  Stream_Stats decoded_only = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  int error;

  snprintf (path, sizeof (path),
            "stream_ex_invalid_version_fixture_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));
  if (!write_invalid_version_fixture (path, "AC9999"))
    {
      printf ("failed to create stream_ex invalid-version fixture\n");
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  error = dwg_stream_file_ex (path, &callbacks, &combined);
  if (error != DWG_ERR_INVALIDDWG || combined.num_objects
      || combined.full_decode_objects || combined.lightweight_objects
      || combined.decoded_objects || combined.decode_error_objects)
    {
      printf ("invalid-version decoded API failed: error=0x%x expected=0x%x\n",
              error, DWG_ERR_INVALIDDWG);
      print_stats ("invalid-version decoded API", &combined);
      remove (path);
      return 1;
    }

  callbacks.object = NULL;
  error = dwg_stream_file_ex (path, &callbacks, &decoded_only);
  if (error != DWG_ERR_INVALIDDWG || decoded_only.num_objects
      || decoded_only.full_decode_objects || decoded_only.lightweight_objects
      || decoded_only.decoded_objects || decoded_only.decode_error_objects)
    {
      printf ("invalid-version decoded-only API failed: error=0x%x "
              "expected=0x%x\n",
              error, DWG_ERR_INVALIDDWG);
      print_stats ("invalid-version decoded-only API", &decoded_only);
      remove (path);
      return 1;
    }

  remove (path);
  return 0;
}

static int
test_invalid_versions_reject (void)
{
  size_t i;

  for (i = 0; i < (sizeof (invalid_fixtures) / sizeof (invalid_fixtures[0]));
       i++)
    {
      const Invalid_Stream_Fixture *fixture = &invalid_fixtures[i];
      Dwg_Stream_Callbacks_Ex callbacks = { 0 };
      Stream_Stats stats = { 0 };
      char path[128];
      int error;

      snprintf (path, sizeof (path), "stream_invalid_version_%lu_%ld_%ld.dwg",
                (unsigned long)i, stream_test_process_id (),
                (long)time (NULL));
      if (!write_invalid_version_fixture (path, fixture->magic))
        {
          printf ("failed to create invalid %s fixture\n", fixture->label);
          return 1;
        }

      callbacks.object = stream_object_callback;
      callbacks.decoded_object = stream_decoded_object_callback;
      callbacks.decode_error = stream_decode_error_callback;
      error = dwg_stream_file_ex (path, &callbacks, &stats);
      remove (path);
      if (error != DWG_ERR_INVALIDDWG || stats.num_objects
          || stats.full_decode_objects || stats.lightweight_objects
          || stats.decoded_objects || stats.decode_error_objects)
        {
          printf ("invalid %s stream failed: magic=%s error=0x%x "
                  "expected=0x%x objects=%lu full=%lu lightweight=%lu "
                  "decoded=%lu decode_errors=%lu\n",
                  fixture->label, fixture->magic, error, DWG_ERR_INVALIDDWG,
                  (unsigned long)stats.num_objects,
                  (unsigned long)stats.full_decode_objects,
                  (unsigned long)stats.lightweight_objects,
                  (unsigned long)stats.decoded_objects,
                  (unsigned long)stats.decode_error_objects);
          return 1;
        }
    }
  return 0;
}

static int
test_large_stream_fixture (void)
{
  const char *path = getenv ("LIBREDWG_STREAM_TEST_LARGE_DWG");
  Stream_Stats stats = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  FILE *fp;
  int error;
  int decode_objects = 0;

  if (!path || !*path)
    return 0;

  fp = fopen (path, "rb");
  if (!fp)
    {
      printf ("skip: cannot open large stream fixture %s\n", path);
      return 0;
    }
  fclose (fp);

  decode_objects = getenv ("LIBREDWG_STREAM_TEST_LARGE_DECODED") != NULL;
  callbacks.object = stream_object_callback;
  if (decode_objects)
    {
      callbacks.decoded_object = stream_decoded_object_callback;
      callbacks.decode_error = stream_decode_error_callback;
    }
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("large stream fixture failed: %s error=0x%x\n", path, error);
      print_stats ("large stream fixture partial", &stats);
      return 1;
    }
  if (!stats.num_objects || stats.full_decode_objects
      || stats.lightweight_objects != stats.num_objects)
    {
      print_stats ("large stream fixture did not use lightweight path",
                   &stats);
      return 1;
    }
  if (decode_objects && !stats.decoded_objects)
    {
      print_stats ("large stream fixture decoded no objects", &stats);
      return 1;
    }
  if (decode_objects
      && stats.decoded_objects + stats.decode_error_objects
             != stats.num_objects)
    {
      print_stats ("large stream fixture decoded accounting mismatch", &stats);
      return 1;
    }

  print_stats ("large stream fixture ok", &stats);
  if (decode_objects && getenv ("LIBREDWG_STREAM_TEST_ERROR_BUCKETS"))
    print_decode_error_buckets (&stats);
  return 0;
}

static int
stats_equal (const Stream_Stats *a, const Stream_Stats *b)
{
  return a->num_objects == b->num_objects && a->num_entities == b->num_entities
         && a->num_non_entities == b->num_non_entities
         && a->version == b->version && !a->version_mismatches
         && !b->version_mismatches && a->total_size == b->total_size
         && (a->handle_mix == b->handle_mix
             || b->r2004_object_map_objects == b->num_objects)
         && a->min_address == b->min_address
         && a->max_address == b->max_address && a->max_size == b->max_size;
}

static void
print_stats (const char *label, const Stream_Stats *stats)
{
  printf (
      "%s: objects=%lu entities=%lu non_entities=%lu total_size=%llu "
      "max_size=%lu min_address=%zu max_address=%zu handle_mix=%llu "
      "lightweight=%lu decoded=%lu decoded_entities=%lu "
      "decoded_non_entities=%lu max_host_entities=%lu "
      "decoded_handle_mix=%llu decode_errors=%lu "
      "decode_error_entities=%lu decode_error_non_entities=%lu "
      "first_decode_error=0x%x decode_error_handle_mix=%llu r2007=%lu "
      "r2004=%lu r13=%lu prer13=%lu full=%lu file_map=%lu heap=%lu "
      "last_address=%zu last_size=%lu last_type=%u last_handle=%llu "
      "last_super=%u\n",
      label, (unsigned long)stats->num_objects,
      (unsigned long)stats->num_entities,
      (unsigned long)stats->num_non_entities, stats->total_size,
      (unsigned long)stats->max_size, stats->min_address, stats->max_address,
      stats->handle_mix, (unsigned long)stats->lightweight_objects,
      (unsigned long)stats->decoded_objects,
      (unsigned long)stats->decoded_entities,
      (unsigned long)stats->decoded_non_entities,
      (unsigned long)stats->max_host_entities, stats->decoded_handle_mix,
      (unsigned long)stats->decode_error_objects,
      (unsigned long)stats->decode_error_entities,
      (unsigned long)stats->decode_error_non_entities,
      stats->first_decode_error, stats->decode_error_handle_mix,
      (unsigned long)stats->r2007_object_map_objects,
      (unsigned long)stats->r2004_object_map_objects,
      (unsigned long)stats->r13_object_map_objects,
      (unsigned long)stats->prer13_entity_objects,
      (unsigned long)stats->full_decode_objects,
      (unsigned long)stats->file_map_objects,
      (unsigned long)stats->heap_objects, stats->last_address,
      (unsigned long)stats->last_size, (unsigned)stats->last_type,
      (unsigned long long)stats->last_handle, (unsigned)stats->last_supertype);
}

static void
print_decode_error_buckets (const Stream_Stats *stats)
{
  unsigned int i;

  for (i = 0; i < STREAM_DECODE_ERROR_BUCKETS; i++)
    {
      const Stream_Decode_Error_Bucket *bucket
          = &stats->decode_error_buckets[i];
      if (!bucket->used)
        continue;
      printf ("decode_error_bucket[%u]: count=%lu error=0x%x type=%u "
              "super=%u first_handle=%llu first_index=%lu "
              "first_address=%zu first_size=%lu name=%s dxfname=%s\n",
              i, (unsigned long)bucket->count, bucket->error,
              (unsigned)bucket->type, (unsigned)bucket->supertype,
              (unsigned long long)bucket->first_handle,
              (unsigned long)bucket->first_index, bucket->first_address,
              (unsigned long)bucket->first_size, bucket->name,
              bucket->dxfname);
    }
  if (stats->decode_error_unbucketed)
    printf ("decode_error_unbucketed=%lu\n",
            (unsigned long)stats->decode_error_unbucketed);
}

static int
stream_test_source_path (char *path, size_t size, const char *relative)
{
  const char *file = __FILE__;
  const char *marker = "test/unit-testing/stream_test.c";
  const char *pos = strstr (file, marker);
  size_t root_len;
  int written;

  if (!pos)
    {
      marker = "test\\unit-testing\\stream_test.c";
      pos = strstr (file, marker);
    }
  if (pos)
    {
      root_len = (size_t)(pos - file);
      if (root_len + strlen (relative) + 1 > size)
        return 0;
      memcpy (path, file, root_len);
      strcpy (&path[root_len], relative);
      return 1;
    }

  written = snprintf (path, size, "%s", relative);
  return written >= 0 && (size_t)written < size;
}

static int
test_stream_file_parity (const char *path, int compare_refs,
                         int test_abort_callbacks, const char *label,
                         int skip_missing, Stream_Semantic_Coverage *coverage)
{
  FILE *fp;
  int error;
  Dwg_Data dwg = { 0 };
  Stream_Stats baseline = { 0 };
  Stream_Stats streamed = { 0 };
  Stream_Stats decoded_only = { 0 };
  Abort_Stats aborted = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  BITCODE_BL i;

  fp = fopen (path, "rb");
  if (!fp)
    {
      printf ("skip: cannot open %s\n", path);
      return skip_missing ? 77 : 1;
    }
  fclose (fp);

  stream_trace_stage ("baseline dwg_read_file");
  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("dwg_read_file failed: %s error=0x%x\n", path, error);
      return 1;
    }

  if (compare_refs && dwg.num_objects)
    {
      baseline.baseline_refs = (Stream_Ref_Snapshot *)calloc (
          dwg.num_objects, sizeof (baseline.baseline_refs[0]));
      if (!baseline.baseline_refs)
        {
          dwg_free (&dwg);
          return 1;
        }
      baseline.baseline_ref_count = dwg.num_objects;
    }
  for (i = 0; i < dwg.num_objects; i++)
    {
      stats_add_dwg_object (&baseline, &dwg.object[i]);
      if (baseline.baseline_refs)
        snapshot_object_refs (&dwg.object[i], &baseline.baseline_refs[i]);
    }
  if (baseline.baseline_refs)
    qsort (baseline.baseline_refs, baseline.baseline_ref_count,
           sizeof (baseline.baseline_refs[0]), compare_ref_snapshot_key);
  streamed.baseline_refs = baseline.baseline_refs;
  streamed.baseline_ref_count = baseline.baseline_ref_count;
  dwg_free (&dwg);

  stream_trace_stage ("combined dwg_stream_file_ex");
  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (path, &callbacks, &streamed);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("dwg_stream_file_ex failed: %s error=0x%x\n", path, error);
      print_stats ("streamed partial", &streamed);
      free (baseline.baseline_refs);
      return 1;
    }

  if (!stats_equal (&baseline, &streamed))
    {
      print_stats ("baseline", &baseline);
      print_stats ("streamed", &streamed);
      free (baseline.baseline_refs);
      return 1;
    }
  if (streamed.lightweight_objects != streamed.num_objects
      || streamed.full_decode_objects)
    {
      print_stats ("streamed did not use the lightweight path", &streamed);
      free (baseline.baseline_refs);
      return 1;
    }
  if (streamed.r13_object_map_objects == streamed.num_objects)
    {
      if (!streamed.decoded_objects || !streamed.decoded_entities
          || streamed.decode_error_objects)
        {
          print_stats ("streamed decoded object mismatch", &streamed);
          free (baseline.baseline_refs);
          return 1;
        }
    }
  else if (streamed.decoded_objects != streamed.num_objects
           || streamed.decoded_entities != streamed.num_entities
           || streamed.decoded_non_entities != streamed.num_non_entities
           || streamed.decoded_handle_mix != baseline.handle_mix
           || streamed.decode_error_objects)
    {
      print_stats ("baseline", &baseline);
      print_stats ("streamed decoded object mismatch", &streamed);
      free (baseline.baseline_refs);
      return 1;
    }
  if (compare_refs
      && (streamed.decoded_ref_missing || streamed.decoded_ref_mismatches))
    {
      printf ("decoded ref compare failed: checked=%lu missing=%lu "
              "mismatches=%lu\n",
              (unsigned long)streamed.decoded_ref_checked,
              (unsigned long)streamed.decoded_ref_missing,
              (unsigned long)streamed.decoded_ref_mismatches);
      free (baseline.baseline_refs);
      return 1;
    }
  if (!semantic_coverage_equal (&baseline.semantic, &streamed.semantic))
    {
      print_semantic_coverage ("baseline semantic", &baseline.semantic);
      print_semantic_coverage ("streamed semantic", &streamed.semantic);
      free (baseline.baseline_refs);
      return 1;
    }

  if (test_abort_callbacks)
    {
      stream_trace_stage ("decoded-only dwg_stream_file_ex");
      decoded_only.baseline_refs = baseline.baseline_refs;
      decoded_only.baseline_ref_count = baseline.baseline_ref_count;
      callbacks.object = NULL;
      callbacks.decoded_object = stream_decoded_object_callback;
      callbacks.decode_error = stream_decode_error_callback;
      error = dwg_stream_file_ex (path, &callbacks, &decoded_only);
      if (error >= DWG_ERR_CRITICAL || decoded_only.num_objects
          || decoded_only.decoded_objects != baseline.num_objects
          || decoded_only.decoded_entities != baseline.num_entities
          || decoded_only.decoded_non_entities != baseline.num_non_entities
          || decoded_only.decode_error_objects
          || (compare_refs
              && (decoded_only.decoded_ref_missing
                  || decoded_only.decoded_ref_mismatches)))
        {
          printf ("decoded-only stream mismatch: error=0x%x "
                  "objects=%lu decoded=%lu/%lu entities=%lu/%lu "
                  "non_entities=%lu/%lu decode_errors=%lu missing=%lu "
                  "mismatches=%lu\n",
                  error, (unsigned long)decoded_only.num_objects,
                  (unsigned long)decoded_only.decoded_objects,
                  (unsigned long)baseline.num_objects,
                  (unsigned long)decoded_only.decoded_entities,
                  (unsigned long)baseline.num_entities,
                  (unsigned long)decoded_only.decoded_non_entities,
                  (unsigned long)baseline.num_non_entities,
                  (unsigned long)decoded_only.decode_error_objects,
                  (unsigned long)decoded_only.decoded_ref_missing,
                  (unsigned long)decoded_only.decoded_ref_mismatches);
          free (baseline.baseline_refs);
          return 1;
        }
      if (!semantic_coverage_equal (&baseline.semantic,
                                    &decoded_only.semantic))
        {
          print_semantic_coverage ("baseline semantic", &baseline.semantic);
          print_semantic_coverage ("decoded-only semantic",
                                   &decoded_only.semantic);
          free (baseline.baseline_refs);
          return 1;
        }

      stream_trace_stage ("abort object dwg_stream_file_ex");
      aborted.limit = 7;
      aborted.error = 12345;
      callbacks.object = abort_object_callback;
      callbacks.decoded_object = NULL;
      callbacks.decode_error = NULL;
      error = dwg_stream_file_ex (path, &callbacks, &aborted);
      if (error != aborted.error || aborted.calls != aborted.limit)
        {
          printf ("callback abort failed: error=0x%x expected=0x%x calls=%lu "
                  "expected_calls=%lu\n",
                  error, aborted.error, (unsigned long)aborted.calls,
                  (unsigned long)aborted.limit);
          free (baseline.baseline_refs);
          return 1;
        }

      aborted.calls = 0;
      aborted.limit = 3;
      aborted.error = DWG_ERR_NOTYETSUPPORTED;
      callbacks.object = abort_object_callback;
      callbacks.decoded_object = NULL;
      callbacks.decode_error = NULL;
      callbacks.flags = 0;
      error = dwg_stream_file_ex (path, &callbacks, &aborted);
      if (error != aborted.error || aborted.calls != aborted.limit)
        {
          printf ("callback not-supported-bit abort failed: error=0x%x "
                  "expected=0x%x calls=%lu expected_calls=%lu\n",
                  error, aborted.error, (unsigned long)aborted.calls,
                  (unsigned long)aborted.limit);
          free (baseline.baseline_refs);
          return 1;
        }

      stream_trace_stage ("abort decoded dwg_stream_file_ex");
      aborted.calls = 0;
      aborted.limit = 5;
      aborted.error = 23456;
      callbacks.object = NULL;
      callbacks.decoded_object = abort_decoded_object_callback;
      callbacks.decode_error = NULL;
      callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
      error = dwg_stream_file_ex (path, &callbacks, &aborted);
      if (error != aborted.error || aborted.calls != aborted.limit)
        {
          printf ("decoded abort mismatch: error=0x%x calls=%lu "
                  "expected=0x%x/%lu\n",
                  error, (unsigned long)aborted.calls, aborted.error,
                  (unsigned long)aborted.limit);
          free (baseline.baseline_refs);
          return 1;
        }
    }

  semantic_coverage_add (coverage, &baseline.semantic);
  print_stats (label ? label : "stream parity ok", &streamed);
  free (baseline.baseline_refs);
  return 0;
}

static int
test_repository_stream_fixtures (void)
{
  const char *fixtures[]
      = { "test/test-data/example_r13.dwg",  "test/test-data/example_r14.dwg",
          "test/test-data/example_2000.dwg", "test/test-data/example_2004.dwg",
          "test/test-data/example_2007.dwg", "test/test-data/example_2010.dwg",
          "test/test-data/example_2013.dwg", "test/test-data/example_2018.dwg",
          "test/test-data/sample_2018.dwg",  "test/test-data/2000/TS1.dwg" };
  char path[1024];
  size_t i;
  int error;

  for (i = 0; i < sizeof (fixtures) / sizeof (fixtures[0]); i++)
    {
      stream_trace_stage (fixtures[i]);
      if (!stream_test_source_path (path, sizeof (path), fixtures[i]))
        {
          printf ("repository fixture path too long: %s\n", fixtures[i]);
          return 1;
        }
      error = test_stream_file_parity (path, 0, i == 0,
                                       "repository stream parity ok", 0, NULL);
      if (error)
        return error;
    }
  return 0;
}

static int
test_repository_stream_sweep (int compare_refs)
{
  const char *fixtures[] = { "test/test-data/2000/Arc.dwg",
                             "test/test-data/2000/circle.dwg",
                             "test/test-data/2000/Cone.dwg",
                             "test/test-data/2000/Constraints.dwg",
                             "test/test-data/2000/ConstructionLine.dwg",
                             "test/test-data/2000/Donut.dwg",
                             "test/test-data/2000/Ellipse.dwg",
                             "test/test-data/2000/entities-2d.dwg",
                             "test/test-data/2000/entities-3d.dwg",
                             "test/test-data/2000/Helix.dwg",
                             "test/test-data/2000/Leader.dwg",
                             "test/test-data/2000/Line.dwg",
                             "test/test-data/2000/Multiline.dwg",
                             "test/test-data/2000/Point.dwg",
                             "test/test-data/2000/Polygon.dwg",
                             "test/test-data/2000/Polyline.dwg",
                             "test/test-data/2000/PolyLine2D.dwg",
                             "test/test-data/2000/PolyLine3D.dwg",
                             "test/test-data/2000/RAY.dwg",
                             "test/test-data/2000/Spline.dwg",
                             "test/test-data/2000/Text.dwg",
                             "test/test-data/2000/TS1.dwg",
                             "test/test-data/2004/Arc.dwg",
                             "test/test-data/2004/circle.dwg",
                             "test/test-data/2004/Constraints.dwg",
                             "test/test-data/2004/ConstructionLine.dwg",
                             "test/test-data/2004/Donut.dwg",
                             "test/test-data/2004/Ellipse.dwg",
                             "test/test-data/2004/HatchG.dwg",
                             "test/test-data/2004/Helix.dwg",
                             "test/test-data/2004/Leader.dwg",
                             "test/test-data/2004/Line.dwg",
                             "test/test-data/2004/material.dwg",
                             "test/test-data/2004/Multiline.dwg",
                             "test/test-data/2004/Point.dwg",
                             "test/test-data/2004/Polygon.dwg",
                             "test/test-data/2004/Polyline.dwg",
                             "test/test-data/2004/PolyLine3D.dwg",
                             "test/test-data/2004/RAY.dwg",
                             "test/test-data/2004/Spline.dwg",
                             "test/test-data/2004/Surface.dwg",
                             "test/test-data/2004/Text.dwg",
                             "test/test-data/2004/Underlay.dwg",
                             "test/test-data/2007/Arc.dwg",
                             "test/test-data/2007/ATMOS-DC22S.dwg",
                             "test/test-data/2007/circle.dwg",
                             "test/test-data/2007/Constraints.dwg",
                             "test/test-data/2007/ConstructionLine.dwg",
                             "test/test-data/2007/Donut.dwg",
                             "test/test-data/2007/Ellipse.dwg",
                             "test/test-data/2007/Helix.dwg",
                             "test/test-data/2007/Leader.dwg",
                             "test/test-data/2007/Line.dwg",
                             "test/test-data/2007/Multiline.dwg",
                             "test/test-data/2007/Point.dwg",
                             "test/test-data/2007/Polygon.dwg",
                             "test/test-data/2007/Polyline.dwg",
                             "test/test-data/2007/PolyLine3D.dwg",
                             "test/test-data/2007/RAY.dwg",
                             "test/test-data/2007/Spline.dwg",
                             "test/test-data/2007/Text.dwg",
                             "test/test-data/example_2000.dwg",
                             "test/test-data/example_2004.dwg",
                             "test/test-data/example_2007.dwg",
                             "test/test-data/example_r13.dwg",
                             "test/test-data/example_r14.dwg",
                             "test/test-data/sample_2000.dwg" };
  char path[1024];
  Stream_Semantic_Coverage coverage = { 0 };
  size_t i;
  int error;

  for (i = 0; i < sizeof (fixtures) / sizeof (fixtures[0]); i++)
    {
      stream_trace_stage (fixtures[i]);
      if (!stream_test_source_path (path, sizeof (path), fixtures[i]))
        {
          printf ("repository sweep fixture path too long: %s\n", fixtures[i]);
          return 1;
        }
      error = test_stream_file_parity (path, compare_refs, i == 0,
                                       "repository stream sweep ok", 0,
                                       &coverage);
      if (error)
        return error;
    }
  print_semantic_coverage ("repository stream sweep semantic coverage",
                           &coverage);
  if (semantic_coverage_require_repository_sweep (&coverage))
    return 1;
  printf ("repository stream sweep summary: files=%lu refs=%d\n",
          (unsigned long)(sizeof (fixtures) / sizeof (fixtures[0])),
          compare_refs ? 1 : 0);
  return 0;
}

static int
write_generated_minsert_fixture (Dwg_Version_Type version, const char *label,
                                 char *path, size_t path_size)
{
  Dwg_Data *dwg;
  Dwg_Object *mspace;
  Dwg_Object_BLOCK_HEADER *hdr;
  Dwg_Object_BLOCK_HEADER *blk;
  Dwg_Entity_LINE *block_line;
  dwg_point_3d pt1 = { 1.5, 2.5, 0.2 };
  dwg_point_3d pt2 = { 2.5, 1.5, 0.0 };
  const char *tag;
  unsigned i;
  int error;

  tag = dwg_version_type (version);
  snprintf (path, path_size, "stream_minsert_%s_fixture_%ld_%ld.dwg", tag,
            stream_test_process_id (), (long)time (NULL));
  dwg = dwg_new_Document (version, 0, 0);
  if (!dwg)
    {
      printf ("failed to create generated %s MINSERT document\n", label);
      return 1;
    }
  mspace = dwg_model_space_object (dwg);
  if (!mspace || !mspace->tio.object || !mspace->tio.object->tio.BLOCK_HEADER)
    {
      printf ("generated %s MINSERT document has no model space\n", label);
      dwg_free (dwg);
      return 1;
    }
  hdr = mspace->tio.object->tio.BLOCK_HEADER;
  blk = dwg_add_BLOCK_HEADER (dwg, "bloko");
  if (!blk)
    {
      printf ("failed to create generated %s MINSERT block header\n", label);
      dwg_free (dwg);
      return 1;
    }
  dwg_add_BLOCK (blk, "bloko");
  block_line = dwg_add_LINE (blk, &pt1, &pt2);
  if (!block_line || !block_line->parent)
    {
      printf ("failed to create generated %s MINSERT block LINE\n", label);
      dwg_free (dwg);
      return 1;
    }
  block_line->parent->entmode = 3;
  dwg_add_ENDBLK (blk);
  dwg_add_MINSERT (hdr, &pt1, "bloko", 1.0, 1.0, 1.0, 0.0, 2, 1, 1.0, 0.0);
  if (version >= R_2007a && !dwg_add_OLE2FRAME (hdr, &pt1, &pt2))
    {
      printf ("failed to create generated %s OLE2FRAME\n", label);
      dwg_free (dwg);
      return 1;
    }
  if (version == R_2007)
    {
      for (i = 0; i < 2000; i++)
        {
          if (!dwg_add_LINE (hdr, &pt1, &pt2))
            {
              printf ("failed to create generated %s multipage LINE %u\n",
                      label, i);
              dwg_free (dwg);
              return 1;
            }
        }
    }

  error = dwg_write_file (path, dwg);
  dwg_free (dwg);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("failed to write generated %s MINSERT fixture: error=0x%x\n",
              label, error);
      remove (path);
      return 1;
    }
  return 0;
}

static int
test_generated_minsert_stream_fixture (Dwg_Version_Type version,
                                       const char *label)
{
  Stream_Semantic_Coverage coverage = { 0 };
  Dwg_Data blocking = { 0 };
  char path[128];
  int error;

  error
      = write_generated_minsert_fixture (version, label, path, sizeof (path));
  if (error)
    return error;

  error = dwg_read_file (path, &blocking);
  if (error >= DWG_ERR_CRITICAL || blocking.header.from_version != version)
    {
      printf ("generated %s blocking version failed: error=0x%x "
              "version=%u expected=%u\n",
              label, error, (unsigned)blocking.header.from_version,
              (unsigned)version);
      dwg_free (&blocking);
      remove (path);
      return 1;
    }
  if (version == R_2007
      && blocking.fhdr.r2007_file_header.pages_amount
             <= blocking.fhdr.r2007_file_header.num_sections + 3)
    {
      printf ("generated %s did not cover a multipage data section: "
              "pages=%ld sections=%ld\n",
              label, (long)blocking.fhdr.r2007_file_header.pages_amount,
              (long)blocking.fhdr.r2007_file_header.num_sections);
      dwg_free (&blocking);
      remove (path);
      return 1;
    }
  dwg_free (&blocking);

  error = test_stream_file_parity (path, 1, 0, label, 0, &coverage);
  remove (path);
  if (error)
    return error;
  print_semantic_coverage (label, &coverage);
  if (!coverage.minserts)
    {
      printf ("generated %s MINSERT fixture did not cover MINSERT\n", label);
      return 1;
    }
  if (!coverage.block_owned_entities || !coverage.entmode3_entities)
    {
      printf (
          "generated %s MINSERT fixture did not cover block-owned entmode=3: "
          "block_owned=%lu entmode3=%lu\n",
          label, (unsigned long)coverage.block_owned_entities,
          (unsigned long)coverage.entmode3_entities);
      return 1;
    }
  return 0;
}

static int
test_generated_minsert_stream_rejects (Dwg_Version_Type version,
                                       const char *label)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Dwg_Stream_Callbacks legacy_callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data blocking = { 0 };
  char path[128];
  unsigned int flags;
  int error;
  int i;

  error
      = write_generated_minsert_fixture (version, label, path, sizeof (path));
  if (error)
    return error;

  error = dwg_read_file (path, &blocking);
  if (error >= DWG_ERR_CRITICAL || blocking.header.from_version != version
      || !blocking.num_objects)
    {
      printf ("generated %s blocking version failed: error=0x%x "
              "version=%u expected=%u objects=%lu\n",
              label, error, (unsigned)blocking.header.from_version,
              (unsigned)version, (unsigned long)blocking.num_objects);
      dwg_free (&blocking);
      remove (path);
      return 1;
    }
  dwg_free (&blocking);

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  for (i = 0; i < 2; i++)
    {
      flags = i ? DWG_STREAM_F_NO_FULL_FALLBACK : 0;
      memset (&stats, 0, sizeof (stats));
      callbacks.flags = flags;
      error = dwg_stream_file_ex (path, &callbacks, &stats);
      if (error != DWG_ERR_NOTYETSUPPORTED || stats.num_objects
          || stats.lightweight_objects || stats.decoded_objects
          || stats.decode_error_objects || stats.full_decode_objects)
        {
          printf ("generated %s Stream rejection failed: flags=0x%x "
                  "error=0x%x expected=0x%x objects=%lu lightweight=%lu "
                  "decoded=%lu decode_errors=%lu full=%lu\n",
                  label, flags, error, DWG_ERR_NOTYETSUPPORTED,
                  (unsigned long)stats.num_objects,
                  (unsigned long)stats.lightweight_objects,
                  (unsigned long)stats.decoded_objects,
                  (unsigned long)stats.decode_error_objects,
                  (unsigned long)stats.full_decode_objects);
          remove (path);
          return 1;
        }
    }

  memset (&stats, 0, sizeof (stats));
  legacy_callbacks.object = stream_object_callback;
  legacy_callbacks.flags = 0;
  error = dwg_stream_file (path, &legacy_callbacks, &stats);
  remove (path);
  if (error != DWG_ERR_NOTYETSUPPORTED || stats.num_objects
      || stats.lightweight_objects || stats.decoded_objects
      || stats.decode_error_objects || stats.full_decode_objects)
    {
      printf ("generated %s legacy Stream rejection failed: error=0x%x "
              "expected=0x%x objects=%lu lightweight=%lu decoded=%lu "
              "decode_errors=%lu full=%lu\n",
              label, error, DWG_ERR_NOTYETSUPPORTED,
              (unsigned long)stats.num_objects,
              (unsigned long)stats.lightweight_objects,
              (unsigned long)stats.decoded_objects,
              (unsigned long)stats.decode_error_objects,
              (unsigned long)stats.full_decode_objects);
      return 1;
    }

  printf ("generated %s rejected as unsupported by Stream APIs\n", label);
  return 0;
}

static int
mutate_r2004_section_map_checksum (const char *path,
                                   const BITCODE_RL section_map_address)
{
  const size_t checksum_address = (size_t)section_map_address + 0x110;
  FILE *fp;
  int byte;

  if (checksum_address > LONG_MAX)
    return 1;
  fp = fopen (path, "r+b");
  if (!fp)
    return 1;
  if (fseek (fp, (long)checksum_address, SEEK_SET) != 0
      || (byte = fgetc (fp)) == EOF
      || fseek (fp, (long)checksum_address, SEEK_SET) != 0
      || fputc (byte ^ 1, fp) == EOF)
    {
      fclose (fp);
      return 1;
    }
  return fclose (fp) != 0;
}

static int
test_callback_abort_preserves_error (void)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data blocking = { 0 };
  char path[128];
  BITCODE_RL section_map_address;
  int error;

  error = write_generated_minsert_fixture (R_2004b, "callback warning", path,
                                           sizeof (path));
  if (error)
    return error;
  error = dwg_read_file (path, &blocking);
  if (error >= DWG_ERR_CRITICAL || blocking.num_objects < 7)
    {
      printf ("callback warning fixture read failed: error=0x%x objects=%lu\n",
              error, (unsigned long)blocking.num_objects);
      dwg_free (&blocking);
      remove (path);
      return 1;
    }
  section_map_address = blocking.fhdr.r2004_header.section_map_address;
  dwg_free (&blocking);
  if (mutate_r2004_section_map_checksum (path, section_map_address))
    {
      printf ("failed to mutate callback warning fixture checksum\n");
      remove (path);
      return 1;
    }

  error = dwg_read_file (path, &blocking);
  if (!(error & DWG_ERR_WRONGCRC) || error >= DWG_ERR_CRITICAL
      || blocking.num_objects < 7)
    {
      printf ("mutated blocking warning missing: error=0x%x objects=%lu\n",
              error, (unsigned long)blocking.num_objects);
      dwg_free (&blocking);
      remove (path);
      return 1;
    }
  dwg_free (&blocking);

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  if (!(error & DWG_ERR_WRONGCRC) || error >= DWG_ERR_CRITICAL
      || stats.num_objects < 7 || stats.full_decode_objects)
    {
      printf (
          "mutated stream warning missing: error=0x%x objects=%lu full=%lu\n",
          error, (unsigned long)stats.num_objects,
          (unsigned long)stats.full_decode_objects);
      remove (path);
      return 1;
    }

  error = test_stream_file_parity (
      path, 1, 1, "callback abort with prior warning parity ok", 0, NULL);
  remove (path);
  return error;
}

static int
write_magic_override_fixture (const char *source_path, const char *path,
                              const char *magic, BITCODE_RC dwg_version)
{
  unsigned char buffer[8192];
  FILE *source;
  FILE *output;
  size_t size;
  int ok = 1;

  source = fopen (source_path, "rb");
  if (!source)
    return 0;
  output = fopen (path, "wb");
  if (!output)
    {
      fclose (source);
      return 0;
    }

  if (fread (buffer, 1, 6, source) != 6 || fwrite (magic, 1, 6, output) != 6)
    ok = 0;
  while (ok && (size = fread (buffer, 1, sizeof (buffer), source)) > 0)
    {
      if (fwrite (buffer, 1, size, output) != size)
        ok = 0;
    }
  if (ok
      && (fseek (output, 17, SEEK_SET) != 0
          || fputc ((int)dwg_version, output) == EOF))
    ok = 0;
  if (ferror (source) || fclose (output) != 0)
    ok = 0;
  fclose (source);
  if (!ok)
    remove (path);
  return ok;
}

static int
test_modern_header_version_stream (void)
{
  static const struct
  {
    const char *relative_path;
    Dwg_Version_Type version;
    const char *magic;
    BITCODE_RC dwg_version;
    const char *label;
  } versions[] = {
    { "test/test-data/example_2000.dwg", R_2000i, "AC1016", 0x17,
      "synthetic R2000i header stream parity ok" },
    { "test/test-data/example_2000.dwg", R_2002, "AC1017", 0x17,
      "synthetic R2002 header stream parity ok" },
  };
  char source_path[1024];
  size_t i;

  for (i = 0; i < sizeof (versions) / sizeof (versions[0]); i++)
    {
      Dwg_Stream_Callbacks_Ex callbacks = { 0 };
      Stream_Stats default_stats = { 0 };
      Dwg_Data blocking = { 0 };
      BITCODE_BL blocking_objects;
      char path[128];
      int error;

      if (!stream_test_source_path (source_path, sizeof (source_path),
                                    versions[i].relative_path))
        {
          printf ("failed to resolve %s source fixture path\n",
                  versions[i].label);
          return 1;
        }
      snprintf (path, sizeof (path), "stream_modern_header_v%u_%ld_%ld.dwg",
                (unsigned)versions[i].version, stream_test_process_id (),
                (long)time (NULL));
      if (!write_magic_override_fixture (source_path, path, versions[i].magic,
                                         versions[i].dwg_version))
        {
          printf ("failed to create %s fixture\n", versions[i].label);
          return 1;
        }

      error = dwg_read_file (path, &blocking);
      blocking_objects = blocking.num_objects;
      if (error >= DWG_ERR_CRITICAL
          || blocking.header.from_version != versions[i].version
          || !blocking_objects)
        {
          printf (
              "%s blocking read failed: error=0x%x version=%u "
              "expected=%u objects=%lu\n",
              versions[i].label, error, (unsigned)blocking.header.from_version,
              (unsigned)versions[i].version, (unsigned long)blocking_objects);
          dwg_free (&blocking);
          remove (path);
          return 1;
        }
      dwg_free (&blocking);

      callbacks.object = stream_object_callback;
      callbacks.decoded_object = stream_decoded_object_callback;
      callbacks.decode_error = stream_decode_error_callback;
      error = dwg_stream_file_ex (path, &callbacks, &default_stats);
      if (error >= DWG_ERR_CRITICAL
          || default_stats.num_objects != blocking_objects
          || default_stats.decoded_objects != blocking_objects
          || default_stats.version != versions[i].version
          || default_stats.version_mismatches
          || default_stats.full_decode_objects
          || default_stats.decode_error_objects)
        {
          printf ("%s default stream failed: error=0x%x objects=%lu/%lu "
                  "decoded=%lu version=%u/%u version_mismatches=%lu full=%lu "
                  "decode_errors=%lu\n",
                  versions[i].label, error,
                  (unsigned long)default_stats.num_objects,
                  (unsigned long)blocking_objects,
                  (unsigned long)default_stats.decoded_objects,
                  (unsigned)default_stats.version,
                  (unsigned)versions[i].version,
                  (unsigned long)default_stats.version_mismatches,
                  (unsigned long)default_stats.full_decode_objects,
                  (unsigned long)default_stats.decode_error_objects);
          remove (path);
          return 1;
        }

      error = test_stream_file_parity (path, 1, 0, versions[i].label, 0, NULL);
      remove (path);
      if (error)
        return error;
    }
  return 0;
}

static int
write_generated_r11_minsert_opts_fixture (char *path, size_t path_size)
{
  Dwg_Data dwg = { 0 };
  Dwg_Data *gen;
  Dwg_Object *mspace;
  Dwg_Object_BLOCK_HEADER *hdr;
  Dwg_Object_BLOCK_HEADER *blk;
  Dwg_Entity_INSERT *insert;
  Dwg_Entity_LINE *block_line;
  BITCODE_BL i;
  int found_opts = 0;
  dwg_point_3d pt1 = { 1.0, 2.0, 0.0 };
  dwg_point_3d pt2 = { 2.0, 3.0, 0.0 };
  int error;

  snprintf (path, path_size, "stream_r11_minsert_opts_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));
  gen = dwg_new_Document (R_11, 0, 0);
  if (!gen)
    {
      printf ("failed to create generated R11 INSERT document\n");
      return 1;
    }
  mspace = dwg_model_space_object (gen);
  if (!mspace || !mspace->tio.object || !mspace->tio.object->tio.BLOCK_HEADER)
    {
      printf ("generated R11 INSERT document has no model space\n");
      dwg_free (gen);
      return 1;
    }
  hdr = mspace->tio.object->tio.BLOCK_HEADER;
  blk = dwg_add_BLOCK_HEADER (gen, "r11blk");
  if (!blk || !dwg_add_BLOCK (blk, "r11blk"))
    {
      printf ("failed to create generated R11 INSERT block\n");
      dwg_free (gen);
      return 1;
    }
  block_line = dwg_add_LINE (blk, &pt1, &pt2);
  if (!block_line || !block_line->parent || !dwg_add_ENDBLK (blk)
      || !(insert = dwg_add_INSERT (hdr, &pt1, "r11blk", 1.0, 1.0, 1.0, 0.0)))
    {
      printf ("failed to create generated R11 INSERT fixture\n");
      dwg_free (gen);
      return 1;
    }
  block_line->parent->entmode = 3;
  insert->num_cols = 3;
  insert->num_rows = 2;
  insert->col_spacing = 2.5;
  insert->row_spacing = 1.25;
  insert->parent->opts_r11
      |= OPTS_R11_INSERT_HAS_NUM_COLS | OPTS_R11_INSERT_HAS_NUM_ROWS
         | OPTS_R11_INSERT_HAS_COL_SPACING | OPTS_R11_INSERT_HAS_ROW_SPACING;

  error = dwg_write_file (path, gen);
  dwg_free (gen);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("failed to write generated R11 INSERT fixture: error=0x%x\n",
              error);
      remove (path);
      return 1;
    }

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL || !dwg.num_objects)
    {
      printf ("generated R11 INSERT blocking read failed: error=0x%x "
              "objects=%lu\n",
              error, (unsigned long)dwg.num_objects);
      dwg_free (&dwg);
      remove (path);
      return 1;
    }

  for (i = 0; i < dwg.num_objects; i++)
    {
      Dwg_Object *obj = &dwg.object[i];
      if (obj->fixedtype == DWG_TYPE_INSERT && obj->type == DWG_TYPE_INSERT_r11
          && obj->tio.entity && obj->tio.entity->tio.INSERT)
        {
          Dwg_Entity_INSERT *read_insert = obj->tio.entity->tio.INSERT;
          if (read_insert->num_cols == 3 && read_insert->num_rows == 2
              && read_insert->col_spacing == 2.5
              && read_insert->row_spacing == 1.25)
            {
              found_opts = 1;
            }
          break;
        }
    }
  dwg_free (&dwg);
  if (!found_opts)
    {
      printf ("generated R11 INSERT fixture did not cover MINSERT opts\n");
      remove (path);
      return 1;
    }
  return 0;
}

static int
write_generated_r11_line_fixture (char *path, size_t path_size,
                                  BITCODE_RL *line_address)
{
  Dwg_Data dwg = { 0 };
  Dwg_Data *gen;
  Dwg_Object *mspace;
  Dwg_Object_BLOCK_HEADER *hdr;
  BITCODE_BL i;
  dwg_point_3d pt1 = { 1.0, 2.0, 0.0 };
  dwg_point_3d pt2 = { 2.0, 3.0, 0.0 };
  int error;

  snprintf (path, path_size, "stream_r11_line_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));
  gen = dwg_new_Document (R_11, 0, 0);
  if (!gen)
    {
      printf ("failed to create generated R11 LINE document\n");
      return 1;
    }
  mspace = dwg_model_space_object (gen);
  if (!mspace || !mspace->tio.object || !mspace->tio.object->tio.BLOCK_HEADER)
    {
      printf ("generated R11 LINE document has no model space\n");
      dwg_free (gen);
      return 1;
    }
  hdr = mspace->tio.object->tio.BLOCK_HEADER;
  if (!dwg_add_LINE (hdr, &pt1, &pt2))
    {
      printf ("failed to create generated R11 LINE fixture\n");
      dwg_free (gen);
      return 1;
    }

  error = dwg_write_file (path, gen);
  dwg_free (gen);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("failed to write generated R11 LINE fixture: error=0x%x\n",
              error);
      remove (path);
      return 1;
    }

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL || !dwg.num_objects)
    {
      printf ("generated R11 LINE blocking read failed: error=0x%x "
              "objects=%lu\n",
              error, (unsigned long)dwg.num_objects);
      dwg_free (&dwg);
      remove (path);
      return 1;
    }

  *line_address = 0;
  for (i = 0; i < dwg.num_objects; i++)
    {
      Dwg_Object *obj = &dwg.object[i];
      if (obj->fixedtype == DWG_TYPE_LINE && obj->type == DWG_TYPE_LINE_r11
          && obj->address)
        {
          *line_address = (BITCODE_RL)obj->address;
          break;
        }
    }
  dwg_free (&dwg);
  if (!*line_address)
    {
      printf ("generated R11 LINE fixture did not find LINE address\n");
      remove (path);
      return 1;
    }
  return 0;
}

static int
mark_r11_entity_type (const char *path, const BITCODE_RL address,
                      const int type)
{
  FILE *fp;

  fp = fopen (path, "r+b");
  if (!fp)
    return 1;
  if (fseek (fp, (long)address, SEEK_SET))
    {
      fclose (fp);
      return 1;
    }
  if (fputc (type, fp) == EOF)
    {
      fclose (fp);
      return 1;
    }
  fclose (fp);
  return 0;
}

typedef struct _r11_mutated_entity_case
{
  int r11_type;
  Dwg_Object_Type fixedtype;
  const char *name;
} R11_Mutated_Entity_Case;

static int
test_pre_r13_mutated_entity_stream (const R11_Mutated_Entity_Case *test_case)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data dwg = { 0 };
  BITCODE_RL line_address;
  BITCODE_BL i;
  int found_entity = 0;
  char path[128];
  int error;

  error
      = write_generated_r11_line_fixture (path, sizeof (path), &line_address);
  if (error)
    return error;
  if (mark_r11_entity_type (path, line_address, test_case->r11_type))
    {
      printf ("failed to mark generated R11 LINE as %s\n", test_case->name);
      remove (path);
      return 1;
    }

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL || !dwg.num_objects)
    {
      printf ("generated R11 %s blocking read failed: error=0x%x "
              "objects=%lu\n",
              test_case->name, error, (unsigned long)dwg.num_objects);
      dwg_free (&dwg);
      remove (path);
      return 1;
    }
  for (i = 0; i < dwg.num_objects; i++)
    {
      Dwg_Object *obj = &dwg.object[i];
      if (obj->fixedtype == test_case->fixedtype
          && obj->type == test_case->r11_type)
        {
          found_entity = 1;
          break;
        }
    }
  dwg_free (&dwg);
  if (!found_entity)
    {
      printf ("generated R11 %s fixture did not cover %s in blocking "
              "read\n",
              test_case->name, test_case->name);
      remove (path);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  remove (path);
  if (error >= DWG_ERR_CRITICAL || stats.full_decode_objects
      || !stats.num_objects || !stats.decoded_objects
      || stats.decode_error_objects
      || !(stats.r11_type_mask & (1ULL << test_case->r11_type)))
    {
      printf ("R11 %s stream failed: error=0x%x objects=%lu decoded=%lu "
              "full=%lu decode_errors=%lu mask=0x%llx expected=0x%llx\n",
              test_case->name, error, (unsigned long)stats.num_objects,
              (unsigned long)stats.decoded_objects,
              (unsigned long)stats.full_decode_objects,
              (unsigned long)stats.decode_error_objects, stats.r11_type_mask,
              1ULL << test_case->r11_type);
      return 1;
    }
  return 0;
}

static int
test_pre_r13_legacy_entity_stream (void)
{
  static const R11_Mutated_Entity_Case jump_case
      = { DWG_TYPE_JUMP_r11, DWG_TYPE_JUMP, "JUMP" };

  return test_pre_r13_mutated_entity_stream (&jump_case);
}

static int
test_pre_r2_legacy_entity_stream (void)
{
  static const unsigned long long expected_mask
      = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data dwg = { 0 };
  char path[1024];
  BITCODE_BL baseline_objects = 0;
  BITCODE_BL baseline_entities = 0;
  BITCODE_BL baseline_non_entities = 0;
  BITCODE_BL i;
  unsigned long long baseline_mask = 0;
  int error;

  if (!stream_test_source_path (path, sizeof (path),
                                "test/test-data/r1.4/entities.dwg"))
    {
      printf ("failed to resolve R1.4 legacy entity fixture path\n");
      return 1;
    }

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL || !dwg.num_objects)
    {
      printf ("R1.4 legacy blocking read failed: error=0x%x objects=%lu\n",
              error, (unsigned long)dwg.num_objects);
      dwg_free (&dwg);
      return 1;
    }

  baseline_objects = dwg.num_objects;
  for (i = 0; i < dwg.num_objects; i++)
    {
      Dwg_Object *obj = &dwg.object[i];
      if (obj->supertype == DWG_SUPERTYPE_ENTITY)
        baseline_entities++;
      else
        baseline_non_entities++;
      baseline_mask |= pre_r2_legacy_entity_bit (obj->fixedtype);
    }
  dwg_free (&dwg);
  if ((baseline_mask & expected_mask) != expected_mask)
    {
      printf ("R1.4 legacy blocking read missed fixedtypes: mask=0x%llx "
              "expected=0x%llx\n",
              baseline_mask, expected_mask);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  if (error >= DWG_ERR_CRITICAL || stats.num_objects != baseline_objects
      || stats.num_entities != baseline_entities
      || stats.num_non_entities != baseline_non_entities
      || stats.full_decode_objects
      || stats.lightweight_objects != stats.num_objects
      || stats.prer13_entity_objects != stats.num_objects
      || stats.decoded_objects != stats.num_objects
      || stats.decoded_entities != baseline_entities
      || stats.decoded_non_entities != baseline_non_entities
      || stats.max_host_entities > 3 || stats.decode_error_objects
      || (stats.prer2_legacy_fixedtype_mask & expected_mask) != expected_mask)
    {
      printf ("R1.4 legacy stream failed: error=0x%x objects=%lu "
              "expected_objects=%lu entities=%lu expected_entities=%lu "
              "non_entities=%lu expected_non_entities=%lu "
              "lightweight=%lu prer13=%lu full=%lu decoded=%lu "
              "decoded_entities=%lu max_host_entities=%lu decode_errors=%lu "
              "mask=0x%llx "
              "expected=0x%llx\n",
              error, (unsigned long)stats.num_objects,
              (unsigned long)baseline_objects,
              (unsigned long)stats.num_entities,
              (unsigned long)baseline_entities,
              (unsigned long)stats.num_non_entities,
              (unsigned long)baseline_non_entities,
              (unsigned long)stats.lightweight_objects,
              (unsigned long)stats.prer13_entity_objects,
              (unsigned long)stats.full_decode_objects,
              (unsigned long)stats.decoded_objects,
              (unsigned long)stats.decoded_entities,
              (unsigned long)stats.max_host_entities,
              (unsigned long)stats.decode_error_objects,
              stats.prer2_legacy_fixedtype_mask, expected_mask);
      return 1;
    }
  return 0;
}

static int
test_generated_pre_r2_version_stream (void)
{
  static const struct
  {
    Dwg_Version_Type version;
    const char *label;
  } versions[] = {
    { R_1_1, "generated R1.1" },
    { R_1_2, "generated R1.2" },
    { R_1_3, "generated R1.3" },
  };
  size_t i;

  for (i = 0; i < sizeof (versions) / sizeof (versions[0]); i++)
    {
      Dwg_Stream_Callbacks_Ex callbacks = { 0 };
      Stream_Stats stats = { 0 };
      Dwg_Data blocking = { 0 };
      Dwg_Data *gen;
      Dwg_Object *mspace;
      Dwg_Object_BLOCK_HEADER *hdr;
      BITCODE_BL expected_objects = 0;
      BITCODE_BL expected_entities = 0;
      BITCODE_BL expected_non_entities = 0;
      BITCODE_BL j;
      int found_line = 0;
      dwg_point_3d pt1 = { 1.0, 2.0, 0.0 };
      dwg_point_3d pt2 = { 3.0, 4.0, 0.0 };
      char path[128];
      int error;

      snprintf (path, sizeof (path), "stream_prer2_v%u_%ld_%ld.dwg",
                (unsigned)versions[i].version, stream_test_process_id (),
                (long)time (NULL));
      gen = dwg_new_Document (versions[i].version, 0, 0);
      if (!gen)
        {
          printf ("%s document creation failed\n", versions[i].label);
          return 1;
        }
      mspace = dwg_model_space_object (gen);
      if (!mspace || !mspace->tio.object
          || !mspace->tio.object->tio.BLOCK_HEADER)
        {
          printf ("%s document has no model space\n", versions[i].label);
          dwg_free (gen);
          return 1;
        }
      hdr = mspace->tio.object->tio.BLOCK_HEADER;
      if (!dwg_add_LINE (hdr, &pt1, &pt2))
        {
          printf ("%s failed to add LINE\n", versions[i].label);
          dwg_free (gen);
          return 1;
        }
      error = dwg_write_file (path, gen);
      dwg_free (gen);
      if (error >= DWG_ERR_CRITICAL)
        {
          printf ("%s write failed: error=0x%x\n", versions[i].label, error);
          remove (path);
          return 1;
        }

      error = dwg_read_file (path, &blocking);
      if (error >= DWG_ERR_CRITICAL || !blocking.num_objects
          || blocking.header.from_version != versions[i].version)
        {
          printf ("%s blocking read failed: error=0x%x objects=%lu "
                  "version=%u expected_version=%u\n",
                  versions[i].label, error,
                  (unsigned long)blocking.num_objects,
                  (unsigned)blocking.header.from_version,
                  (unsigned)versions[i].version);
          dwg_free (&blocking);
          remove (path);
          return 1;
        }
      expected_objects = blocking.num_objects;
      for (j = 0; j < blocking.num_objects; j++)
        {
          Dwg_Object *obj = &blocking.object[j];
          if (obj->supertype == DWG_SUPERTYPE_ENTITY)
            expected_entities++;
          else
            expected_non_entities++;
          if (obj->fixedtype == DWG_TYPE_LINE)
            found_line = 1;
        }
      dwg_free (&blocking);
      if (!expected_entities || !found_line)
        {
          printf ("%s blocking coverage failed: entities=%lu line=%d\n",
                  versions[i].label, (unsigned long)expected_entities,
                  found_line);
          remove (path);
          return 1;
        }

      callbacks.object = stream_object_callback;
      callbacks.decoded_object = stream_decoded_object_callback;
      callbacks.decode_error = stream_decode_error_callback;
      callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
      error = dwg_stream_file_ex (path, &callbacks, &stats);
      remove (path);
      if (error >= DWG_ERR_CRITICAL || stats.num_objects != expected_objects
          || stats.num_entities != expected_entities
          || stats.num_non_entities != expected_non_entities
          || stats.full_decode_objects
          || stats.lightweight_objects != expected_objects
          || stats.prer13_entity_objects != expected_objects
          || stats.decoded_objects != expected_objects
          || stats.decoded_entities != expected_entities
          || stats.decoded_non_entities != expected_non_entities
          || stats.max_host_entities > 3 || stats.decode_error_objects)
        {
          printf ("%s stream failed: error=0x%x objects=%lu expected=%lu "
                  "entities=%lu expected_entities=%lu non_entities=%lu "
                  "expected_non_entities=%lu lightweight=%lu prer13=%lu "
                  "full=%lu decoded=%lu decoded_entities=%lu "
                  "decoded_non_entities=%lu max_host_entities=%lu "
                  "decode_errors=%lu\n",
                  versions[i].label, error, (unsigned long)stats.num_objects,
                  (unsigned long)expected_objects,
                  (unsigned long)stats.num_entities,
                  (unsigned long)expected_entities,
                  (unsigned long)stats.num_non_entities,
                  (unsigned long)expected_non_entities,
                  (unsigned long)stats.lightweight_objects,
                  (unsigned long)stats.prer13_entity_objects,
                  (unsigned long)stats.full_decode_objects,
                  (unsigned long)stats.decoded_objects,
                  (unsigned long)stats.decoded_entities,
                  (unsigned long)stats.decoded_non_entities,
                  (unsigned long)stats.max_host_entities,
                  (unsigned long)stats.decode_error_objects);
          return 1;
        }
    }
  return 0;
}

typedef struct _pre_r11_stream_fixture
{
  const char *relative_path;
  const char *label;
  Dwg_Version_Type expected_version;
  unsigned long long required_legacy_mask;
  BITCODE_BS required_fixedtype;
} Pre_R11_Stream_Fixture;

static int
test_pre_r11_fixture_path (const char *restrict path,
                           const Pre_R11_Stream_Fixture *restrict fixture)
{
  static const unsigned stream_flags[] = {
    0,
    DWG_STREAM_F_NO_FULL_FALLBACK,
  };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data dwg = { 0 };
  BITCODE_BL expected_entities = 0;
  BITCODE_BL expected_non_entities = 0;
  BITCODE_BL i;
  Stream_Ref_Snapshot *baseline_refs;
  size_t baseline_ref_count;
  unsigned long long expected_table_mask = 0;
  unsigned long long expected_type_mask = 0;
  unsigned long long baseline_legacy_mask = 0;
  size_t flags_index;
  int found_required_fixedtype = 0;
  int error;

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL || !dwg.num_objects
      || dwg.header.from_version != fixture->expected_version)
    {
      printf ("%s blocking read failed: error=0x%x objects=%lu "
              "version=%u expected_version=%u\n",
              fixture->label, error, (unsigned long)dwg.num_objects,
              (unsigned)dwg.header.from_version,
              (unsigned)fixture->expected_version);
      dwg_free (&dwg);
      return 1;
    }

  baseline_ref_count = dwg.num_objects;
  baseline_refs = (Stream_Ref_Snapshot *)calloc (baseline_ref_count,
                                                 sizeof (baseline_refs[0]));
  if (!baseline_refs)
    {
      dwg_free (&dwg);
      return 1;
    }
  for (i = 0; i < dwg.num_objects; i++)
    {
      Dwg_Object *obj = &dwg.object[i];
      snapshot_object_refs (obj, &baseline_refs[i]);
      if (obj->supertype == DWG_SUPERTYPE_ENTITY)
        {
          expected_entities++;
          if (obj->type < 64)
            expected_type_mask |= 1ULL << obj->type;
        }
      else
        {
          expected_non_entities++;
          if (pre_r13_table_entry_bit (obj->fixedtype))
            expected_table_mask |= pre_r13_table_entry_bit (obj->fixedtype);
        }
      baseline_legacy_mask |= pre_r2_legacy_entity_bit (obj->fixedtype);
      if (fixture->required_fixedtype
          && obj->fixedtype == fixture->required_fixedtype)
        found_required_fixedtype = 1;
    }
  qsort (baseline_refs, baseline_ref_count, sizeof (baseline_refs[0]),
         compare_ref_snapshot_key);
  dwg_free (&dwg);
  if (!expected_entities || !expected_non_entities
      || (baseline_legacy_mask & fixture->required_legacy_mask)
             != fixture->required_legacy_mask
      || (fixture->required_fixedtype && !found_required_fixedtype))
    {
      printf ("%s blocking coverage failed: entities=%lu non_entities=%lu "
              "legacy_mask=0x%llx expected_legacy=0x%llx "
              "required_fixedtype=%u found=%d\n",
              fixture->label, (unsigned long)expected_entities,
              (unsigned long)expected_non_entities, baseline_legacy_mask,
              fixture->required_legacy_mask,
              (unsigned)fixture->required_fixedtype, found_required_fixedtype);
      free (baseline_refs);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  for (flags_index = 0;
       flags_index < sizeof (stream_flags) / sizeof (stream_flags[0]);
       flags_index++)
    {
      memset (&stats, 0, sizeof (stats));
      stats.baseline_refs = baseline_refs;
      stats.baseline_ref_count = baseline_ref_count;
      callbacks.flags = stream_flags[flags_index];
      error = dwg_stream_file_ex (path, &callbacks, &stats);
      if (error >= DWG_ERR_CRITICAL
          || stats.num_objects != expected_entities + expected_non_entities
          || stats.num_entities != expected_entities
          || stats.num_non_entities != expected_non_entities
          || stats.full_decode_objects
          || stats.lightweight_objects != stats.num_objects
          || stats.prer13_entity_objects != stats.num_objects
          || stats.decoded_objects != stats.num_objects
          || stats.decoded_entities != expected_entities
          || stats.decoded_non_entities != expected_non_entities
          || stats.max_host_entities > 3 || stats.decode_error_objects
          || stats.decoded_ref_missing || stats.decoded_ref_mismatches
          || stats.r11_type_mask != expected_type_mask
          || stats.r11_table_fixedtype_mask != expected_table_mask
          || (stats.prer2_legacy_fixedtype_mask
              & fixture->required_legacy_mask)
                 != fixture->required_legacy_mask)
        {
          printf ("%s stream failed: flags=0x%x error=0x%x objects=%lu "
                  "expected=%lu entities=%lu expected_entities=%lu "
                  "non_entities=%lu expected_non_entities=%lu "
                  "lightweight=%lu prer13=%lu full=%lu decoded=%lu "
                  "decoded_entities=%lu decoded_non_entities=%lu "
                  "max_host_entities=%lu decode_errors=%lu ref_missing=%lu "
                  "ref_mismatches=%lu type_mask=0x%llx "
                  "expected_type_mask=0x%llx table_mask=0x%llx "
                  "expected_table_mask=0x%llx legacy_mask=0x%llx "
                  "expected_legacy_mask=0x%llx\n",
                  fixture->label, callbacks.flags, error,
                  (unsigned long)stats.num_objects,
                  (unsigned long)(expected_entities + expected_non_entities),
                  (unsigned long)stats.num_entities,
                  (unsigned long)expected_entities,
                  (unsigned long)stats.num_non_entities,
                  (unsigned long)expected_non_entities,
                  (unsigned long)stats.lightweight_objects,
                  (unsigned long)stats.prer13_entity_objects,
                  (unsigned long)stats.full_decode_objects,
                  (unsigned long)stats.decoded_objects,
                  (unsigned long)stats.decoded_entities,
                  (unsigned long)stats.decoded_non_entities,
                  (unsigned long)stats.max_host_entities,
                  (unsigned long)stats.decode_error_objects,
                  (unsigned long)stats.decoded_ref_missing,
                  (unsigned long)stats.decoded_ref_mismatches,
                  stats.r11_type_mask, expected_type_mask,
                  stats.r11_table_fixedtype_mask, expected_table_mask,
                  stats.prer2_legacy_fixedtype_mask,
                  fixture->required_legacy_mask);
          free (baseline_refs);
          return 1;
        }
    }
  free (baseline_refs);
  return 0;
}

static int
test_pre_r11_real_fixture_stream (void)
{
  static const Pre_R11_Stream_Fixture fixtures[] = {
    { "test/test-data/r1.4-ac1_40/ARC1.DWG", "BSD AC1.40 ARC1", R_1_4, 0, 0 },
    { "test/test-data/r1.4-ac1_40/BLOCK1.DWG", "BSD AC1.40 BLOCK1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/BLOCK2.DWG", "BSD AC1.40 BLOCK2", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/CIRCLE1.DWG", "BSD AC1.40 CIRCLE1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/CIRCLE2.DWG", "BSD AC1.40 CIRCLE2", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/LINE1.DWG", "BSD AC1.40 LINE1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/LINE2.DWG", "BSD AC1.40 LINE2", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/LOAD1.DWG", "BSD AC1.40 LOAD1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/POINT1.DWG", "BSD AC1.40 POINT1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/POINT2.DWG", "BSD AC1.40 POINT2", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/REPEAT1.DWG", "BSD AC1.40 REPEAT1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/SHAPE1.DWG", "BSD AC1.40 SHAPE1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/SOLID1.DWG", "BSD AC1.40 SOLID1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TEXT1.DWG", "BSD AC1.40 TEXT1", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_ARC.DWG", "BSD AC1.40 TMP_ARC", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_CIR.DWG", "BSD AC1.40 TMP_CIR", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_INS.DWG", "BSD AC1.40 TMP_INS", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_LINE.DWG", "BSD AC1.40 TMP_LINE", R_1_4,
      0, 0 },
    { "test/test-data/r1.4-ac1_40/TMP_PO.DWG", "BSD AC1.40 TMP_PO", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_SHA.DWG", "BSD AC1.40 TMP_SHA", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_SOL.DWG", "BSD AC1.40 TMP_SOL", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TMP_TEXT.DWG", "BSD AC1.40 TMP_TEXT", R_1_4,
      0, 0 },
    { "test/test-data/r1.4-ac1_40/TMP_TR.DWG", "BSD AC1.40 TMP_TR", R_1_4, 0,
      0 },
    { "test/test-data/r1.4-ac1_40/TRACE1.DWG", "BSD AC1.40 TRACE1", R_1_4, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/ARC1.DWG", "BSD AC2.10 ARC1", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/ATTDEF1.DWG", "BSD AC2.10 ATTDEF1", R_2_10,
      0, 0 },
    { "test/test-data/r2.10-ac2_10/ATTDEF2.DWG", "BSD AC2.10 ATTDEF2", R_2_10,
      0, 0 },
    { "test/test-data/r2.10-ac2_10/ATTRIB1.DWG", "BSD AC2.10 ATTRIB1", R_2_10,
      0, 0 },
    { "test/test-data/r2.10-ac2_10/CIRCLE1.DWG", "BSD AC2.10 CIRCLE1", R_2_10,
      0, 0 },
    { "test/test-data/r2.10-ac2_10/LINE1.DWG", "BSD AC2.10 LINE1", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/LINE2.DWG", "BSD AC2.10 LINE2", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/PLINE1.DWG", "BSD AC2.10 PLINE1", R_2_10, 0,
      DWG_TYPE_POLYLINE_2D },
    { "test/test-data/r2.10-ac2_10/POINT1.DWG", "BSD AC2.10 POINT1", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/POINT2.DWG", "BSD AC2.10 POINT2", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/REPEAT1.DWG", "BSD AC2.10 REPEAT1", R_2_10,
      (1ULL << 0) | (1ULL << 1), 0 },
    { "test/test-data/r2.10-ac2_10/SHAPE1.DWG", "BSD AC2.10 SHAPE1", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/SOLID1.DWG", "BSD AC2.10 SOLID1", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/TEXT1.DWG", "BSD AC2.10 TEXT1", R_2_10, 0,
      0 },
    { "test/test-data/r2.10-ac2_10/TRACE1.DWG", "BSD AC2.10 TRACE1", R_2_10, 0,
      0 },
    { "test/test-data/r2.6-ac1003/3DFACE1.DWG", "BSD AC1003 3DFACE1", R_2_6, 0,
      DWG_TYPE__3DFACE },
    { "test/test-data/r2.6-ac1003/3DFACE2.DWG", "BSD AC1003 3DFACE2", R_2_6, 0,
      DWG_TYPE__3DFACE },
    { "test/test-data/r2.6-ac1003/3DFACE3.DWG", "BSD AC1003 3DFACE3", R_2_6, 0,
      DWG_TYPE__3DFACE },
    { "test/test-data/r2.6-ac1003/ARC1.DWG", "BSD AC1003 ARC1", R_2_6, 0, 0 },
    { "test/test-data/r2.6-ac1003/ARC2.DWG", "BSD AC1003 ARC2", R_2_6, 0, 0 },
    { "test/test-data/r2.6-ac1003/ATTDEF1.DWG", "BSD AC1003 ATTDEF1", R_2_6, 0,
      0 },
    { "test/test-data/r2.6-ac1003/ATTDEF2.DWG", "BSD AC1003 ATTDEF2", R_2_6, 0,
      0 },
    { "test/test-data/r2.6-ac1003/CIRCLE1.DWG", "BSD AC1003 CIRCLE1", R_2_6, 0,
      0 },
    { "test/test-data/r10-ac1006/3DFACE1.DWG", "BSD AC1006 3DFACE1", R_10, 0,
      DWG_TYPE__3DFACE },
    { "test/test-data/r10-ac1006/3DFACE2.DWG", "BSD AC1006 3DFACE2", R_10, 0,
      DWG_TYPE__3DFACE },
    { "test/test-data/r10-ac1006/3DLINE1.DWG", "BSD AC1006 3DLINE1", R_10, 0,
      DWG_TYPE__3DLINE },
    { "test/test-data/r10-ac1006/ARC1.DWG", "BSD AC1006 ARC1", R_10, 0, 0 },
    { "test/test-data/r10-ac1006/CIRCLE1.DWG", "BSD AC1006 CIRCLE1", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/CIRCLE2.DWG", "BSD AC1006 CIRCLE2", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/DIM1.DWG", "BSD AC1006 DIM1", R_10, 0, 0 },
    { "test/test-data/r10-ac1006/INSERT1.DWG", "BSD AC1006 INSERT1", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/INSERT2.DWG", "BSD AC1006 INSERT2", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/JUMP1.DWG", "BSD AC1006 JUMP1", R_10, 0, 0 },
    { "test/test-data/r10-ac1006/LINE1.DWG", "BSD AC1006 LINE1", R_10, 0, 0 },
    { "test/test-data/r10-ac1006/LINE2.DWG", "BSD AC1006 LINE2", R_10, 0, 0 },
    { "test/test-data/r10-ac1006/PL1.DWG", "BSD AC1006 PL1", R_10, 0,
      DWG_TYPE_POLYLINE_2D },
    { "test/test-data/r10-ac1006/POINT1.DWG", "BSD AC1006 POINT1", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/SHAPE1.DWG", "BSD AC1006 SHAPE1", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/SOLID1.DWG", "BSD AC1006 SOLID1", R_10, 0,
      0 },
    { "test/test-data/r10-ac1006/TEXT1.DWG", "BSD AC1006 TEXT1", R_10, 0, 0 },
    { "test/test-data/r2.10/entities.dwg", "R2.10 entities", R_2_10,
      (1ULL << 0) | (1ULL << 1), 0 },
    { "test/test-data/r2.10/block.dwg", "R2.10 block", R_2_10, 0, 0 },
    { "test/test-data/r2.6/entities.dwg", "R2.6 entities", R_2_6, 0,
      DWG_TYPE__3DLINE },
    { "test/test-data/r2.6/dim.dwg", "R2.6 dimensions", R_2_6, 0, 0 },
    { "test/test-data/r9/entities.dwg", "R9 entities", R_9, 0,
      DWG_TYPE__3DLINE },
    { "test/test-data/r10/entities.dwg", "R10 entities", R_10, 0,
      DWG_TYPE__3DLINE },
    { "test/test-data/r10/tmp_line.dwg", "R10 line", R_10, 0, 0 },
    { "test/test-data/r12-oda/Constraints.dwg", "ODA ACAD12 Constraints", R_11,
      0, 0 },
    { "test/test-data/r11/ACEB10.dwg", "R11 ACEB10", R_11, 0, 0 },
    { "test/test-data/r11/entities-2d.dwg", "R11 2D entities", R_11, 0, 0 },
    { "test/test-data/r11/entities-3d.dwg", "R11 3D entities", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/3DFACE1.DWG", "BSD AC1009 3DFACE1", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/3DFACE2.DWG", "BSD AC1009 3DFACE2", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/ARC1.DWG", "BSD AC1009 ARC1", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/ARC2.DWG", "BSD AC1009 ARC2", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/CIRCLE1.DWG", "BSD AC1009 CIRCLE1", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/CIRCLE2.DWG", "BSD AC1009 CIRCLE2", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/CIRCLE3.DWG", "BSD AC1009 CIRCLE3", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/DIM1.DWG", "BSD AC1009 DIM1", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/DIM2.DWG", "BSD AC1009 DIM2", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/INSERT1.DWG", "BSD AC1009 INSERT1", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/INSERT2.DWG", "BSD AC1009 INSERT2", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/JUMP1.DWG", "BSD AC1009 JUMP1", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/LINE1.DWG", "BSD AC1009 LINE1", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/LINE2.DWG", "BSD AC1009 LINE2", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/LINE3.DWG", "BSD AC1009 LINE3", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/PL1.DWG", "BSD AC1009 PL1", R_11, 0, 0 },
    { "test/test-data/r11-ac1009/POINT1.DWG", "BSD AC1009 POINT1", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/POINT2.DWG", "BSD AC1009 POINT2", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/SHAPE1.DWG", "BSD AC1009 SHAPE1", R_11, 0,
      0 },
    { "test/test-data/r11-ac1009/TEXT1.DWG", "BSD AC1009 TEXT1", R_11, 0, 0 },
  };
  char path[1024];
  size_t i;

  for (i = 0; i < sizeof (fixtures) / sizeof (fixtures[0]); i++)
    {
      if (!stream_test_source_path (path, sizeof (path),
                                    fixtures[i].relative_path))
        {
          printf ("failed to resolve %s fixture path\n", fixtures[i].label);
          return 1;
        }
      if (test_pre_r11_fixture_path (path, &fixtures[i]))
        return 1;
    }
  return 0;
}

static int
test_generated_pre_r11_version_stream (void)
{
  static const struct
  {
    Dwg_Version_Type version;
    const char *label;
  } versions[] = {
    { R_2_0b, "generated R2.0 beta" }, { R_2_0, "generated R2.0" },
    { R_2_21, "generated R2.21" },     { R_2_22, "generated R2.22" },
    { R_2_4, "generated R2.4" },       { R_2_5, "generated R2.5" },
    { R_9c1, "generated R9c1" },       { R_11b1, "generated R11b1" },
    { R_11b2, "generated R11b2" },
  };
  size_t i;

  for (i = 0; i < sizeof (versions) / sizeof (versions[0]); i++)
    {
      Pre_R11_Stream_Fixture fixture
          = { NULL, versions[i].label, versions[i].version, 0, 0 };
      Dwg_Data *gen;
      Dwg_Object *mspace;
      Dwg_Object_BLOCK_HEADER *hdr;
      dwg_point_3d pt1 = { 1.0, 2.0, 0.0 };
      dwg_point_3d pt2 = { 3.0, 4.0, 0.0 };
      char path[128];
      int error;

      snprintf (path, sizeof (path), "stream_prer11_v%u_%ld_%ld.dwg",
                (unsigned)versions[i].version, stream_test_process_id (),
                (long)time (NULL));
      gen = dwg_new_Document (versions[i].version, 0, 0);
      if (!gen)
        {
          printf ("%s document creation failed\n", versions[i].label);
          return 1;
        }
      mspace = dwg_model_space_object (gen);
      if (!mspace || !mspace->tio.object
          || !mspace->tio.object->tio.BLOCK_HEADER)
        {
          printf ("%s document has no model space\n", versions[i].label);
          dwg_free (gen);
          return 1;
        }
      hdr = mspace->tio.object->tio.BLOCK_HEADER;
      if (!dwg_add_LINE (hdr, &pt1, &pt2))
        {
          printf ("%s failed to add LINE\n", versions[i].label);
          dwg_free (gen);
          return 1;
        }
      error = dwg_write_file (path, gen);
      dwg_free (gen);
      if (error >= DWG_ERR_CRITICAL)
        {
          printf ("%s write failed: error=0x%x\n", versions[i].label, error);
          remove (path);
          return 1;
        }
      error = test_pre_r11_fixture_path (path, &fixture);
      remove (path);
      if (error)
        return 1;
    }
  return 0;
}

static int
test_pre_r13_minsert_opts_stream (void)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  R11_Minsert_Opts_Stats capture = { 0 };
  char path[128];
  int error;

  error = write_generated_r11_minsert_opts_fixture (path, sizeof (path));
  if (error)
    return error;

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = r11_minsert_opts_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (path, &callbacks, &capture);
  remove (path);
  if (error >= DWG_ERR_CRITICAL || capture.stats.full_decode_objects
      || capture.stats.decode_error_objects || !capture.found
      || !(capture.stats.r11_type_mask & (1ULL << DWG_TYPE_INSERT_r11)))
    {
      printf ("generated R11 MINSERT opts stream failed: error=0x%x "
              "full=%lu decode_errors=%lu found=%lu mask=0x%llx "
              "expected=0x%llx\n",
              error, (unsigned long)capture.stats.full_decode_objects,
              (unsigned long)capture.stats.decode_error_objects,
              (unsigned long)capture.found, capture.stats.r11_type_mask,
              1ULL << DWG_TYPE_INSERT_r11);
      print_stats ("generated R11 MINSERT opts stream", &capture.stats);
      return 1;
    }
  printf ("generated R11 MINSERT opts stream parity ok\n");
  return 0;
}

static int
is_pre_r13_table_entry (const Dwg_Object *obj)
{
  if (!obj || obj->supertype == DWG_SUPERTYPE_ENTITY)
    return 0;

  return pre_r13_table_entry_bit (obj->fixedtype) != 0;
}

static int
test_generated_pre_r13_stream_basic (void)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data dwg = { 0 };
  Dwg_Data *gen;
  Dwg_Object *mspace;
  Dwg_Object_BLOCK_HEADER *hdr;
  Dwg_Object_BLOCK_HEADER *blk;
  Dwg_Object_BLOCK_HEADER *nested_blk;
  Dwg_Entity_INSERT *insert;
  Dwg_Entity_INSERT *block_insert;
  Dwg_Entity_LINE *extra_gate;
  Dwg_Entity_LINE *extra_line;
  unsigned long long expected_mask = 0;
  unsigned long long expected_block_mask = 0;
  unsigned long long expected_dim_mask = (1ULL << 7) - 1;
  unsigned long long expected_polyline_mask = (1ULL << 4) - 1;
  unsigned long long expected_vertex_mask = (1ULL << 5) - 1;
  unsigned long long expected_table_mask = 0;
  unsigned long long expected_table_fixedtype_mask = (1ULL << 10) - 1;
  const BITCODE_BL required_entity_count = 71;
  BITCODE_BL expected_entity_count = 0;
  BITCODE_BL expected_non_entity_count = 0;
  BITCODE_BL expected_table_count = 0;
  BITCODE_BL expected_count = 0;
  BITCODE_BL expected_block_count = 46;
  BITCODE_BL i;
  dwg_point_3d pt1 = { 0.0, 0.0, 0.0 };
  dwg_point_3d pt2 = { 2.0, 1.0, 0.0 };
  dwg_point_3d pt3 = { 3.0, 1.0, 0.0 };
  dwg_point_3d pt4 = { 3.0, 3.0, 0.0 };
  dwg_point_2d pt2d1 = { 0.0, 0.0 };
  dwg_point_2d pt2d2 = { 1.0, 0.0 };
  dwg_point_2d pt2d3 = { 1.0, 1.0 };
  dwg_point_2d pl2d_pts[] = { { 0.0, 0.0 }, { 2.0, 0.0 } };
  dwg_point_3d pl3d_pts[] = { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 } };
  dwg_point_3d mesh_pts[] = {
    { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 }
  };
  dwg_point_3d pface_pts[]
      = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 1.0 } };
  dwg_face pface_faces[] = { { 1, 2, 3, 0 } };
  dwg_point_3d ucs_xdir = { 1.0, 0.0, 0.0 };
  dwg_point_3d ucs_ydir = { 0.0, 1.0, 0.0 };
  char path[128];
  int error;

  expected_mask |= 1ULL << DWG_TYPE_LINE_r11;
  expected_mask |= 1ULL << DWG_TYPE_POINT_r11;
  expected_mask |= 1ULL << DWG_TYPE_CIRCLE_r11;
  expected_mask |= 1ULL << DWG_TYPE_TEXT_r11;
  expected_mask |= 1ULL << DWG_TYPE_ARC_r11;
  expected_mask |= 1ULL << DWG_TYPE_TRACE_r11;
  expected_mask |= 1ULL << DWG_TYPE_SOLID_r11;
  expected_mask |= 1ULL << DWG_TYPE_BLOCK_r11;
  expected_mask |= 1ULL << DWG_TYPE_ENDBLK_r11;
  expected_mask |= 1ULL << DWG_TYPE_3DFACE_r11;
  expected_mask |= 1ULL << DWG_TYPE_SHAPE_r11;
  expected_mask |= 1ULL << DWG_TYPE_INSERT_r11;
  expected_mask |= 1ULL << DWG_TYPE_ATTDEF_r11;
  expected_mask |= 1ULL << DWG_TYPE_ATTRIB_r11;
  expected_mask |= 1ULL << DWG_TYPE_SEQEND_r11;
  expected_mask |= 1ULL << DWG_TYPE_POLYLINE_r11;
  expected_mask |= 1ULL << DWG_TYPE_VERTEX_r11;
  expected_mask |= 1ULL << DWG_TYPE_DIMENSION_r11;
  expected_mask |= 1ULL << DWG_TYPE_VIEWPORT_r11;

  expected_block_mask |= 1ULL << DWG_TYPE_BLOCK_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_LINE_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_POINT_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_CIRCLE_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_TEXT_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_ARC_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_TRACE_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_SOLID_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_3DFACE_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_SHAPE_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_ATTDEF_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_INSERT_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_ATTRIB_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_DIMENSION_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_SEQEND_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_POLYLINE_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_VERTEX_r11;
  expected_block_mask |= 1ULL << DWG_TYPE_ENDBLK_r11;

  snprintf (path, sizeof (path), "stream_basic_r11_fixture_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));
  gen = dwg_new_Document (R_11, 0, 0);
  if (!gen)
    {
      printf ("failed to create generated R11 LINE document\n");
      return 1;
    }
  mspace = dwg_model_space_object (gen);
  if (!mspace || !mspace->tio.object || !mspace->tio.object->tio.BLOCK_HEADER)
    {
      printf ("generated R11 LINE document has no model space\n");
      dwg_free (gen);
      return 1;
    }
  hdr = mspace->tio.object->tio.BLOCK_HEADER;
  blk = dwg_add_BLOCK_HEADER (gen, "r11blk");
  nested_blk = dwg_add_BLOCK_HEADER (gen, "nestedblk");
  if (!blk || !nested_blk)
    {
      printf ("failed to create generated R11 basic block header\n");
      dwg_free (gen);
      return 1;
    }
  if (!dwg_add_VIEW (gen, "r11view")
      || !dwg_add_UCS (gen, &pt1, &ucs_xdir, &ucs_ydir, "r11ucs"))
    {
      printf ("failed to create generated R11 VIEW/UCS tables\n");
      dwg_free (gen);
      return 1;
    }
  if (!dwg_add_BLOCK (nested_blk, "nestedblk")
      || !dwg_add_LINE (nested_blk, &pt2, &pt3)
      || !dwg_add_ENDBLK (nested_blk))
    {
      printf ("failed to create generated R11 nested block definition\n");
      dwg_free (gen);
      return 1;
    }
  if (!dwg_add_BLOCK (blk, "r11blk") || !dwg_add_LINE (blk, &pt1, &pt2)
      || !dwg_add_POINT (blk, &pt3) || !dwg_add_CIRCLE (blk, &pt2, 1.25)
      || !dwg_add_TEXT (blk, "BLK", &pt1, 0.25)
      || !dwg_add_ARC (blk, &pt3, 1.0, 0.0, 1.0)
      || !dwg_add_TRACE (blk, &pt1, &pt2d1, &pt2d2, &pt2d3)
      || !dwg_add_SOLID (blk, &pt2, &pt2d1, &pt2d2, &pt2d3)
      || !dwg_add_3DFACE (blk, &pt1, &pt2, &pt3, &pt4)
      || !dwg_add_SHAPE (blk, "bshape", &pt1, 1.0, 0.0)
      || !dwg_add_POLYLINE_2D (blk, 2, pl2d_pts)
      || !dwg_add_POLYLINE_3D (blk, 2, pl3d_pts)
      || !dwg_add_POLYLINE_MESH (blk, 2, 2, mesh_pts)
      || !dwg_add_POLYLINE_PFACE (blk, 3, 1, pface_pts, pface_faces)
      || !dwg_add_INSERT (blk, &pt4, "nestedblk", 1.0, 1.0, 1.0, 0.0)
      || !dwg_add_ATTDEF (blk, 0.25, 0, "Block prompt", &pt2, "BTAG", "BValue")
      || !(block_insert
           = dwg_add_INSERT (blk, &pt1, "nestedblk", 1.0, 1.0, 1.0, 0.0))
      || !dwg_add_ATTRIB (block_insert, 0.25, 0, &pt3, "BATTR", "BAttr")
      || !dwg_add_DIMENSION_LINEAR (blk, &pt1, &pt2, &pt3, 0.0)
      || !dwg_add_DIMENSION_ALIGNED (blk, &pt1, &pt2, &pt3)
      || !dwg_add_DIMENSION_ANG2LN (blk, &pt1, &pt2, &pt3, &pt4)
      || !dwg_add_DIMENSION_ANG3PT (blk, &pt1, &pt2, &pt3, &pt4)
      || !dwg_add_DIMENSION_DIAMETER (blk, &pt2, &pt3, 1.0)
      || !dwg_add_DIMENSION_ORDINATE (blk, &pt1, &pt2, true)
      || !dwg_add_DIMENSION_RADIUS (blk, &pt2, &pt3, 1.0)
      || !dwg_add_ENDBLK (blk))
    {
      printf ("failed to create generated R11 basic block definition\n");
      dwg_free (gen);
      return 1;
    }
  if (!dwg_add_LINE (hdr, &pt1, &pt2) || !dwg_add_POINT (hdr, &pt3)
      || !dwg_add_CIRCLE (hdr, &pt2, 1.25)
      || !dwg_add_TEXT (hdr, "R11", &pt1, 0.25)
      || !dwg_add_ARC (hdr, &pt3, 1.0, 0.0, 1.0)
      || !dwg_add_TRACE (hdr, &pt1, &pt2d1, &pt2d2, &pt2d3)
      || !dwg_add_SOLID (hdr, &pt2, &pt2d1, &pt2d2, &pt2d3)
      || !dwg_add_3DFACE (hdr, &pt1, &pt2, &pt3, &pt4)
      || !dwg_add_SHAPE (hdr, "shape1", &pt1, 1.0, 0.0)
      || !dwg_add_ATTDEF (hdr, 0.25, 0, "Prompt", &pt2, "TAG1", "Value")
      || !(insert = dwg_add_INSERT (hdr, &pt3, "r11blk", 1.0, 1.0, 1.0, 0.0))
      || !dwg_add_ATTRIB (insert, 0.25, 0, &pt4, "TAG2", "Attr")
      || !dwg_add_DIMENSION_LINEAR (hdr, &pt1, &pt2, &pt3, 0.0)
      || !dwg_add_DIMENSION_ALIGNED (hdr, &pt1, &pt2, &pt3)
      || !dwg_add_DIMENSION_ANG2LN (hdr, &pt1, &pt2, &pt3, &pt4)
      || !dwg_add_DIMENSION_ANG3PT (hdr, &pt1, &pt2, &pt3, &pt4)
      || !dwg_add_DIMENSION_DIAMETER (hdr, &pt2, &pt3, 1.0)
      || !dwg_add_DIMENSION_ORDINATE (hdr, &pt1, &pt2, true)
      || !dwg_add_DIMENSION_RADIUS (hdr, &pt2, &pt3, 1.0)
      || !dwg_add_POLYLINE_2D (hdr, 2, pl2d_pts)
      || !dwg_add_POLYLINE_3D (hdr, 2, pl3d_pts)
      || !dwg_add_POLYLINE_MESH (hdr, 2, 2, mesh_pts)
      || !dwg_add_POLYLINE_PFACE (hdr, 3, 1, pface_pts, pface_faces)
      || !dwg_add_VIEWPORT (hdr, "vp1"))
    {
      printf ("failed to create generated R11 basic entities\n");
      dwg_free (gen);
      return 1;
    }
  extra_gate = dwg_add_LINE (hdr, &pt3, &pt4);
  extra_line = dwg_add_LINE (hdr, &pt4, &pt1);
  if (!extra_gate || !extra_gate->parent)
    {
      printf ("failed to create generated R11 extra section gate LINE\n");
      dwg_free (gen);
      return 1;
    }
  if (!extra_line || !extra_line->parent)
    {
      printf ("failed to create generated R11 extra section LINE\n");
      dwg_free (gen);
      return 1;
    }
  extra_gate->parent->entmode = 3;
  extra_line->parent->entmode = 0;
  error = dwg_write_file (path, gen);
  dwg_free (gen);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("failed to write generated R11 basic fixture: error=0x%x\n",
              error);
      remove (path);
      return 1;
    }

  error = dwg_read_file (path, &dwg);
  if (error >= DWG_ERR_CRITICAL || !dwg.num_objects)
    {
      printf ("generated R11 blocking read failed: error=0x%x objects=%lu\n",
              error, (unsigned long)dwg.num_objects);
      dwg_free (&dwg);
      remove (path);
      return 1;
    }
  for (i = 0; i < dwg.num_objects; i++)
    {
      if (dwg.object[i].supertype == DWG_SUPERTYPE_ENTITY)
        expected_entity_count++;
      else
        expected_non_entity_count++;
      if (is_pre_r13_table_entry (&dwg.object[i]))
        {
          expected_table_count++;
          expected_table_mask
              |= pre_r13_table_entry_bit (dwg.object[i].fixedtype);
        }
    }
  if (expected_entity_count < required_entity_count || !expected_table_count)
    {
      printf ("generated R11 blocking coverage too small: entities=%lu "
              "required=%lu table_entries=%lu\n",
              (unsigned long)expected_entity_count,
              (unsigned long)required_entity_count,
              (unsigned long)expected_table_count);
      dwg_free (&dwg);
      remove (path);
      return 1;
    }
  if ((expected_table_mask & expected_table_fixedtype_mask)
      != expected_table_fixedtype_mask)
    {
      printf ("generated R11 blocking read missed table fixedtypes: "
              "mask=0x%llx expected=0x%llx\n",
              expected_table_mask, expected_table_fixedtype_mask);
      dwg_free (&dwg);
      remove (path);
      return 1;
    }
  expected_count = dwg.num_objects;
  dwg_free (&dwg);

  callbacks.object = stream_object_callback;
  callbacks.decoded_object = stream_decoded_object_callback;
  callbacks.decode_error = stream_decode_error_callback;
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  remove (path);
  if (error >= DWG_ERR_CRITICAL || stats.num_objects != expected_count
      || stats.num_entities != expected_entity_count
      || stats.num_non_entities != expected_non_entity_count
      || stats.full_decode_objects
      || stats.lightweight_objects != expected_count
      || stats.prer13_entity_objects != expected_count
      || stats.decoded_objects != expected_count
      || stats.decoded_entities != expected_entity_count
      || stats.decoded_non_entities != expected_non_entity_count
      || stats.decode_error_objects
      || stats.r11_table_fixedtype_mask != expected_table_mask
      || stats.r11_block_section_objects != expected_block_count
      || (stats.r11_type_mask & expected_mask) != expected_mask
      || (stats.r11_block_section_type_mask & expected_block_mask)
             != expected_block_mask
      || (stats.r11_dimension_fixedtype_mask & expected_dim_mask)
             != expected_dim_mask
      || (stats.r11_block_dimension_fixedtype_mask & expected_dim_mask)
             != expected_dim_mask
      || (stats.r11_polyline_fixedtype_mask & expected_polyline_mask)
             != expected_polyline_mask
      || (stats.r11_block_polyline_fixedtype_mask & expected_polyline_mask)
             != expected_polyline_mask
      || (stats.r11_vertex_fixedtype_mask & expected_vertex_mask)
             != expected_vertex_mask)
    {
      printf ("generated R11 basic stream failed: error=0x%x objects=%lu "
              "entities=%lu entity_expected=%lu non_entities=%lu "
              "non_entity_expected=%lu table_entries=%lu lightweight=%lu "
              "prer13=%lu full=%lu "
              "decoded=%lu decoded_entities=%lu decoded_non_entities=%lu "
              "decode_errors=%lu mask=0x%llx expected=0x%llx "
              "table_mask=0x%llx table_mask_expected=0x%llx "
              "block_objects=%lu block_expected=%lu "
              "block_mask=0x%llx block_mask_expected=0x%llx "
              "dim_mask=0x%llx block_dim_mask=0x%llx "
              "dim_expected=0x%llx "
              "pline_mask=0x%llx block_pline_mask=0x%llx "
              "pline_expected=0x%llx vertex_mask=0x%llx "
              "vertex_expected=0x%llx "
              "last_type=%u\n",
              error, (unsigned long)stats.num_objects,
              (unsigned long)stats.num_entities,
              (unsigned long)expected_entity_count,
              (unsigned long)stats.num_non_entities,
              (unsigned long)expected_non_entity_count,
              (unsigned long)expected_table_count,
              (unsigned long)stats.lightweight_objects,
              (unsigned long)stats.prer13_entity_objects,
              (unsigned long)stats.full_decode_objects,
              (unsigned long)stats.decoded_objects,
              (unsigned long)stats.decoded_entities,
              (unsigned long)stats.decoded_non_entities,
              (unsigned long)stats.decode_error_objects, stats.r11_type_mask,
              expected_mask, stats.r11_table_fixedtype_mask,
              expected_table_mask,
              (unsigned long)stats.r11_block_section_objects,
              (unsigned long)expected_block_count,
              stats.r11_block_section_type_mask, expected_block_mask,
              stats.r11_dimension_fixedtype_mask,
              stats.r11_block_dimension_fixedtype_mask, expected_dim_mask,
              stats.r11_polyline_fixedtype_mask,
              stats.r11_block_polyline_fixedtype_mask, expected_polyline_mask,
              stats.r11_vertex_fixedtype_mask, expected_vertex_mask,
              stats.last_type);
      return 1;
    }
  return 0;
}

int
main (void)
{
  const char *path = getenv ("LIBREDWG_STREAM_TEST_DWG");
  int compare_refs = getenv ("LIBREDWG_STREAM_TEST_REFS") != NULL;
  Stream_Semantic_Coverage coverage = { 0 };
  int error;

  stream_trace_stage ("test_no_full_fallback");
  if (test_no_full_fallback ())
    return 1;
  stream_trace_stage ("test_stream_api_invalid_args");
  if (test_stream_api_invalid_args ())
    return 1;
  stream_trace_stage ("test_legacy_callback_initializer");
  if (test_legacy_callback_initializer ())
    return 1;
  stream_trace_stage ("test_legacy_stream_file_api");
  if (test_legacy_stream_file_api ())
    return 1;
  stream_trace_stage ("test_invalid_version_stream_file_ex_rejects");
  if (test_invalid_version_stream_file_ex_rejects ())
    return 1;
  stream_trace_stage ("test_invalid_versions_reject");
  if (test_invalid_versions_reject ())
    return 1;
  stream_trace_stage ("test_emit_decoded_object_isolated_host_state");
  if (test_emit_decoded_object_isolated_host_state ())
    return 1;
  stream_trace_stage ("test_generated_minsert_stream_fixture");
  if (test_generated_minsert_stream_fixture (
          R_13b1, "generated R13 beta 1 MINSERT stream parity ok"))
    return 1;
  if (test_generated_minsert_stream_fixture (
          R_13b2, "generated R13 beta 2 MINSERT stream parity ok"))
    return 1;
  if (test_generated_minsert_stream_fixture (
          R_13c3, "generated R13c3 MINSERT stream parity ok"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2000b, "R2000 beta MINSERT"))
    return 1;
  if (test_generated_minsert_stream_fixture (
          R_2000, "generated R2000 MINSERT stream parity ok"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2004a, "R2004 alpha MINSERT"))
    return 1;
  if (test_generated_minsert_stream_fixture (
          R_2004b, "generated R2004 beta MINSERT stream parity ok"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2004c,
                                             "R2004 candidate MINSERT"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2007a, "R2007 alpha MINSERT"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2007b, "R2007 beta MINSERT"))
    return 1;
  if (test_generated_minsert_stream_fixture (
          R_2007, "generated R2007 MINSERT stream parity ok"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2010b, "R2010 beta MINSERT"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2013b, "R2013 beta MINSERT"))
    return 1;
  if (test_generated_minsert_stream_rejects (R_2018b, "R2018 beta MINSERT"))
    return 1;
  stream_trace_stage ("test_modern_header_version_stream");
  if (test_modern_header_version_stream ())
    return 1;
  stream_trace_stage ("test_generated_r2022_minsert_stream_fixture");
  if (test_generated_minsert_stream_fixture (
          R_2022b, "generated R2022 MINSERT stream parity ok"))
    return 1;
  stream_trace_stage ("test_callback_abort_preserves_error");
  if (test_callback_abort_preserves_error ())
    return 1;
  stream_trace_stage ("test_pre_r13_minsert_opts_stream");
  if (test_pre_r13_minsert_opts_stream ())
    return 1;
  stream_trace_stage ("test_pre_r13_legacy_entity_stream");
  if (test_pre_r13_legacy_entity_stream ())
    return 1;
  stream_trace_stage ("test_pre_r2_legacy_entity_stream");
  if (test_pre_r2_legacy_entity_stream ())
    return 1;
  stream_trace_stage ("test_generated_pre_r2_version_stream");
  if (test_generated_pre_r2_version_stream ())
    return 1;
  stream_trace_stage ("test_pre_r11_real_fixture_stream");
  if (test_pre_r11_real_fixture_stream ())
    return 1;
  stream_trace_stage ("test_generated_pre_r11_version_stream");
  if (test_generated_pre_r11_version_stream ())
    return 1;
  stream_trace_stage ("test_generated_pre_r13_stream_basic");
  if (test_generated_pre_r13_stream_basic ())
    return 1;
  stream_trace_stage ("test_large_stream_fixture");
  if (test_large_stream_fixture ())
    return 1;

  if (!path || !*path)
    {
      if (getenv ("LIBREDWG_STREAM_TEST_LARGE_DWG"))
        return 0;
      if (getenv ("LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP"))
        return test_repository_stream_sweep (compare_refs);
      return test_repository_stream_fixtures ();
    }

  error = test_stream_file_parity (path, compare_refs, 1, "stream parity ok",
                                   1, &coverage);
  if (!error)
    print_semantic_coverage ("stream parity semantic coverage", &coverage);
  return error;
}
