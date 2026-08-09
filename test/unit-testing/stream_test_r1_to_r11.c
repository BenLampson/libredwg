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
    { R_9c1, "generated R9c1" },       { R_10, "generated R10" },
    { R_11b1, "generated R11b1" },     { R_11b2, "generated R11b2" },
    { R_11, "generated R11" },
  };
  size_t i;

  for (i = 0; i < sizeof (versions) / sizeof (versions[0]); i++)
    {
      Pre_R11_Stream_Fixture fixture
          = { NULL, versions[i].label, versions[i].version, 0, 0 };
      Dwg_Data *gen;
      Dwg_Object *mspace;
      Dwg_Object_BLOCK_HEADER *hdr;
      Dwg_Entity_ARC *arc;
      Dwg_Entity_LINE *line;
      Dwg_Entity__3DLINE *line3d;
      Dwg_Entity_POINT *point;
      Dwg_Data decoded = { 0 };
      Dwg_Object *line_obj;
      dwg_point_3d pt1 = { 1.0, 2.0, 0.0 };
      dwg_point_3d pt2 = { 3.0, 4.0, 0.0 };
      dwg_point_3d arc_center = { 7.0, 8.0, 9.0 };
      dwg_point_3d elevated_point = { 1.0, 2.0, 5.0 };
      dwg_point_3d invalid_point = { 1.0, 2.0, 3.0 };
      char path[128];
      int expect_3dline;
      int found_arc_elevation = 0;
      int found_point_elevation = 0;
      int line_error = 0;
      int error;
      BITCODE_BL j;
      BITCODE_BL num_objects;
      BITCODE_BL num_owned;
      BITCODE_RS numentities;

      expect_3dline
          = versions[i].version >= R_2_4 && versions[i].version < R_10;
      if (expect_3dline)
        {
          pt1.z = 5.0;
          pt2.z = 6.0;
          fixture.required_fixedtype = DWG_TYPE__3DLINE;
        }
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
      if (versions[i].version < R_2_4)
        {
          num_objects = gen->num_objects;
          num_owned = hdr->num_owned;
          numentities = gen->header_vars.numentities;
          point = dwg_add_POINT (hdr, &invalid_point);
          if (point || gen->num_objects != num_objects
              || hdr->num_owned != num_owned
              || gen->header_vars.numentities != numentities)
            {
              printf ("%s retained an invalid 3D POINT\n", versions[i].label);
              dwg_free (gen);
              return 1;
            }
          point = dwg_add_POINT (hdr, &pt1);
          if (!point)
            {
              printf ("%s failed to add a valid 2D POINT\n",
                      versions[i].label);
              dwg_free (gen);
              return 1;
            }
          fixture.required_fixedtype = DWG_TYPE_POINT;
        }
      num_objects = gen->num_objects;
      line = dwg_add_LINE (hdr, &pt1, &pt2);
      if (expect_3dline)
        {
          if (line || gen->num_objects != num_objects)
            {
              printf ("%s accepted an unsafe LINE return for nonzero Z\n",
                      versions[i].label);
              dwg_free (gen);
              return 1;
            }
          line3d = dwg_add_3DLINE (hdr, &pt1, &pt2);
          if (!line3d)
            {
              printf ("%s failed to add 3DLINE\n", versions[i].label);
              dwg_free (gen);
              return 1;
            }
          line_obj = dwg_ent_generic_to_object (line3d, &line_error);
        }
      else if (!line)
        {
          printf ("%s failed to add LINE\n", versions[i].label);
          dwg_free (gen);
          return 1;
        }
      else
        line_obj = dwg_ent_generic_to_object (line, &line_error);
      if (expect_3dline
          && (line_error || !line_obj
              || line_obj->type != DWG_TYPE_3DLINE_r11
              || line_obj->fixedtype != DWG_TYPE__3DLINE
              || !line_obj->tio.entity
              || !line_obj->tio.entity->tio._3DLINE
              || line_obj->tio.entity->tio._3DLINE->start.x != pt1.x
              || line_obj->tio.entity->tio._3DLINE->start.y != pt1.y
              || line_obj->tio.entity->tio._3DLINE->start.z != pt1.z
              || line_obj->tio.entity->tio._3DLINE->end.x != pt2.x
              || line_obj->tio.entity->tio._3DLINE->end.y != pt2.y
              || line_obj->tio.entity->tio._3DLINE->end.z != pt2.z
              || (line_obj->tio.entity->opts_r11 & 3) != 3))
        {
          printf ("%s did not allocate a valid 3DLINE for nonzero Z\n",
                  versions[i].label);
          dwg_free (gen);
          return 1;
        }
      if ((versions[i].version == R_10 || versions[i].version == R_11)
          && (line_error || !line_obj || !line_obj->tio.entity
              || !(line_obj->tio.entity->flag_r11
                   & FLAG_R11_HAS_ELEVATION)))
        {
          printf ("%s LINE did not preserve the 2D R11 layout flag\n",
                  versions[i].label);
          dwg_free (gen);
          return 1;
        }
      if (expect_3dline)
        {
          point = dwg_add_POINT (hdr, &elevated_point);
          if (!point)
            {
              printf ("%s failed to add an elevated POINT\n",
                      versions[i].label);
              dwg_free (gen);
              return 1;
            }
        }
      if (versions[i].version >= R_10)
        {
          arc = dwg_add_ARC (hdr, &arc_center, 1.0, 0.0, 1.0);
          if (!arc)
            {
              printf ("%s failed to add an elevated ARC\n",
                      versions[i].label);
              dwg_free (gen);
              return 1;
            }
        }
      error = dwg_write_file (path, gen);
      dwg_free (gen);
      if (error >= DWG_ERR_CRITICAL)
        {
          printf ("%s write failed: error=0x%x\n", versions[i].label, error);
          remove (path);
          return 1;
        }
      if (expect_3dline || versions[i].version >= R_10)
        {
          error = dwg_read_file (path, &decoded);
          if (error >= DWG_ERR_CRITICAL)
            {
              printf ("%s elevation blocking read failed: error=0x%x\n",
                      versions[i].label, error);
              dwg_free (&decoded);
              remove (path);
              return 1;
            }
          for (j = 0; j < decoded.num_objects; j++)
            {
              Dwg_Object *obj = &decoded.object[j];

              if (expect_3dline && obj->fixedtype == DWG_TYPE_POINT
                  && obj->tio.entity && obj->tio.entity->tio.POINT
                  && obj->tio.entity->tio.POINT->z == elevated_point.z)
                found_point_elevation = 1;
              if (versions[i].version >= R_10
                  && obj->fixedtype == DWG_TYPE_ARC && obj->tio.entity
                  && obj->tio.entity->tio.ARC
                  && obj->tio.entity->tio.ARC->center.z == arc_center.z)
                found_arc_elevation = 1;
            }
          dwg_free (&decoded);
          if ((expect_3dline && !found_point_elevation)
              || (versions[i].version >= R_10 && !found_arc_elevation))
            {
              printf ("%s lost POINT/ARC elevation in blocking roundtrip\n",
                      versions[i].label);
              remove (path);
              return 1;
            }
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
