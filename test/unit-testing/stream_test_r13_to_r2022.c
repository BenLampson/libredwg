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
