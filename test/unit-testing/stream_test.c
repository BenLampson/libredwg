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
#include "out_json.h"

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
  const Dwg_Data *baseline_dwg;
  unsigned char *baseline_objects_matched;
  BITCODE_BL canonical_objects_checked;
  BITCODE_BL canonical_object_mismatches;
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

/* Keep the test internals private while separating maintenance ownership. */
#include "stream_test_statistics_and_callbacks.c"
#include "stream_test_api_and_file_parity.c"
#include "stream_test_r13_to_r2022.c"
#include "stream_test_r1_to_r11.c"

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
  stream_trace_stage ("test_r2004_truncated_section_map_rejected");
  if (test_r2004_truncated_section_map_rejected ())
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
