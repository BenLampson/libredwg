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
hash_append_3rd (unsigned long long hash, BITCODE_3RD point)
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
    case DWG_TYPE__3DLINE:
      if (entity->tio._3DLINE)
        {
          Dwg_Entity__3DLINE *line = entity->tio._3DLINE;
          hash = hash_append_3rd (hash, line->start);
          hash = hash_append_3rd (hash, line->end);
          hash = hash_append_double (hash, line->thickness);
          hash = hash_append_3rd (hash, line->extrusion);
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
canonical_objects_equal (const Dwg_Object *baseline,
                         const Dwg_Object *streamed)
{
  Dwg_Object normalized_streamed;
  Bit_Chain baseline_dat = { 0 };
  Bit_Chain streamed_dat = { 0 };
  unsigned char baseline_buf[4096];
  unsigned char streamed_buf[4096];
  size_t baseline_size;
  size_t streamed_size;
  size_t offset = 0;
  int equal = 1;
  int baseline_error;
  int streamed_error;

  normalized_streamed = *streamed;
  normalized_streamed.index = baseline->index;
  baseline_dat.fh = tmpfile ();
  streamed_dat.fh = tmpfile ();
  if (!baseline_dat.fh || !streamed_dat.fh)
    {
      if (baseline_dat.fh)
        fclose (baseline_dat.fh);
      if (streamed_dat.fh)
        fclose (streamed_dat.fh);
      return -1;
    }
  baseline_dat.opts = DWG_OPTS_MINIMAL;
  streamed_dat.opts = DWG_OPTS_MINIMAL;
  baseline_error
      = dwg_write_json_object (&baseline_dat, (Dwg_Object *)baseline);
  streamed_error
      = dwg_write_json_object (&streamed_dat, &normalized_streamed);
  if (baseline_error >= DWG_ERR_CRITICAL
      || streamed_error >= DWG_ERR_CRITICAL
      || fflush (baseline_dat.fh) != 0 || fflush (streamed_dat.fh) != 0
      || fseek (baseline_dat.fh, 0, SEEK_SET) != 0
      || fseek (streamed_dat.fh, 0, SEEK_SET) != 0)
    equal = -1;

  while (equal > 0)
    {
      baseline_size
          = fread (baseline_buf, 1, sizeof (baseline_buf), baseline_dat.fh);
      streamed_size
          = fread (streamed_buf, 1, sizeof (streamed_buf), streamed_dat.fh);
      if (baseline_size != streamed_size
          || (baseline_size
              && memcmp (baseline_buf, streamed_buf, baseline_size) != 0))
        {
          size_t shown_baseline
              = baseline_size < 512 ? baseline_size : (size_t)512;
          size_t shown_streamed
              = streamed_size < 512 ? streamed_size : (size_t)512;

          printf ("canonical bytes differ at chunk offset=%zu "
                  "baseline_size=%zu streamed_size=%zu\nbaseline: ",
                  offset, baseline_size, streamed_size);
          fwrite (baseline_buf, 1, shown_baseline, stdout);
          printf ("\nstreamed: ");
          fwrite (streamed_buf, 1, shown_streamed, stdout);
          printf ("\n");
          equal = 0;
          break;
        }
      if (!baseline_size)
        break;
      offset += baseline_size;
    }
  if (ferror (baseline_dat.fh) || ferror (streamed_dat.fh))
    equal = -1;
  fclose (baseline_dat.fh);
  fclose (streamed_dat.fh);
  return equal;
}

static const Dwg_Object *
find_unmatched_baseline_object (Stream_Stats *stats,
                                const Dwg_Stream_Object_Info *info,
                                const Dwg_Object *streamed)
{
  const Dwg_Object *baseline;
  BITCODE_BL i;

  if (!stats || !stats->baseline_dwg || !stats->baseline_objects_matched
      || !info || !streamed)
    return NULL;
  if (streamed->handle.value)
    {
      baseline = dwg_resolve_handle_silent (
          stats->baseline_dwg, streamed->handle.value);
      if (!baseline || baseline->index >= stats->baseline_dwg->num_objects
          || stats->baseline_objects_matched[baseline->index]
          || baseline->type != streamed->type
          || baseline->fixedtype != streamed->fixedtype
          || baseline->supertype != streamed->supertype
          || baseline->size != info->size
          || baseline->address != info->address)
        return NULL;
      stats->baseline_objects_matched[baseline->index] = 1;
      return baseline;
    }
  for (i = 0; i < stats->baseline_dwg->num_objects; i++)
    {
      baseline = &stats->baseline_dwg->object[i];

      if (stats->baseline_objects_matched[i]
          || baseline->type != streamed->type
          || baseline->fixedtype != streamed->fixedtype
          || baseline->supertype != streamed->supertype
          || baseline->size != info->size
          || baseline->address != info->address
          || baseline->handle.value)
        continue;
      stats->baseline_objects_matched[i] = 1;
      return baseline;
    }
  return NULL;
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
  const Dwg_Object *baseline_object;
  BITCODE_BL host_entities = 0;
  BITCODE_BL i;
  int canonical_equal;

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
  if (stats->baseline_dwg)
    {
      baseline_object = find_unmatched_baseline_object (stats, info, object);
      if (!baseline_object)
        {
          stats->canonical_object_mismatches++;
          printf ("decoded object has no unmatched blocking peer: "
                  "index=%lu handle=%llu type=%u fixedtype=%u address=%zu "
                  "size=%lu\n",
                  (unsigned long)object->index,
                  (unsigned long long)object->handle.value,
                  (unsigned)object->type, (unsigned)object->fixedtype,
                  object->address, (unsigned long)object->size);
          return DWG_ERR_INTERNALERROR;
        }
      canonical_equal = canonical_objects_equal (baseline_object, object);
      stats->canonical_objects_checked++;
      if (canonical_equal != 1)
        {
          stats->canonical_object_mismatches++;
          printf ("canonical object mismatch blocking_index=%lu "
                  "stream_index=%lu handle=%llu type=%u serialization=%s\n",
                  (unsigned long)baseline_object->index,
                  (unsigned long)object->index,
                  (unsigned long long)object->handle.value,
                  (unsigned)object->fixedtype,
                  canonical_equal < 0 ? "failed" : "different");
          return DWG_ERR_INTERNALERROR;
        }
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
