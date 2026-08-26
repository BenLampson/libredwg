typedef struct _stream_header_capture
{
  BITCODE_BS insunits;
  BITCODE_BL calls;
} Stream_Header_Capture;

typedef struct _stream_filedeplist_capture
{
  const Dwg_Data *baseline;
  const char *feature;
  const char *filename;
  int compare_full;
  BITCODE_BL calls;
  BITCODE_BL matches;
} Stream_FileDepList_Capture;

static int
stream_filedeplist_text_equal (const Dwg_Data *restrict dwg,
                               const BITCODE_T32 value,
                               const char *restrict expected)
{
  char *utf8;
  int equal;

  if (!value)
    return 0;
  if (dwg->header.from_version < R_2007)
    return strcmp (value, expected) == 0;

  utf8 = bit_convert_TU ((BITCODE_TU)value);
  if (!utf8)
    return 0;
  equal = strcmp (utf8, expected) == 0;
  free (utf8);
  return equal;
}

static int
stream_filedeplist_text_fields_equal (const Dwg_Data *restrict left_dwg,
                                      const BITCODE_T32 left,
                                      const Dwg_Data *restrict right_dwg,
                                      const BITCODE_T32 right)
{
  char *left_utf8;
  char *right_utf8;
  int equal;

  if (!left || !right)
    return left == right;
  if ((left_dwg->header.from_version < R_2007)
      != (right_dwg->header.from_version < R_2007))
    return 0;
  if (left_dwg->header.from_version < R_2007)
    return strcmp (left, right) == 0;

  left_utf8 = bit_convert_TU ((BITCODE_TU)left);
  right_utf8 = bit_convert_TU ((BITCODE_TU)right);
  if (!left_utf8 || !right_utf8)
    {
      free (left_utf8);
      free (right_utf8);
      return 0;
    }
  equal = strcmp (left_utf8, right_utf8) == 0;
  free (left_utf8);
  free (right_utf8);
  return equal;
}

static int
stream_filedeplists_equal (const Dwg_Data *restrict streamed,
                           const Dwg_Data *restrict baseline)
{
  const Dwg_FileDepList *left = &streamed->filedeplist;
  const Dwg_FileDepList *right = &baseline->filedeplist;
  BITCODE_RL i;

  if (left->num_features != right->num_features
      || left->num_files != right->num_files
      || (left->num_features && (!left->features || !right->features))
      || (left->num_files && (!left->files || !right->files)))
    return 0;
  for (i = 0; i < left->num_features; i++)
    if (!stream_filedeplist_text_fields_equal (streamed, left->features[i],
                                               baseline, right->features[i]))
      return 0;
  for (i = 0; i < left->num_files; i++)
    {
      const Dwg_FileDepList_Files *left_file = &left->files[i];
      const Dwg_FileDepList_Files *right_file = &right->files[i];

      if (!stream_filedeplist_text_fields_equal (
              streamed, left_file->filename, baseline, right_file->filename)
          || !stream_filedeplist_text_fields_equal (
              streamed, left_file->filepath, baseline, right_file->filepath)
          || !stream_filedeplist_text_fields_equal (
              streamed, left_file->fingerprint, baseline,
              right_file->fingerprint)
          || !stream_filedeplist_text_fields_equal (
              streamed, left_file->version, baseline, right_file->version)
          || left_file->feature_index != right_file->feature_index
          || left_file->timestamp != right_file->timestamp
          || left_file->filesize != right_file->filesize
          || left_file->affects_graphics != right_file->affects_graphics
          || left_file->refcount != right_file->refcount)
        return 0;
    }
  return 1;
}

static int
capture_stream_filedeplist (const Dwg_Stream_Object_Info *restrict info,
                            const Dwg_Object *restrict object,
                            void *restrict user)
{
  Stream_FileDepList_Capture *capture = (Stream_FileDepList_Capture *)user;
  const Dwg_FileDepList *filedeplist;

  (void)info;
  capture->calls++;
  if (!object || !object->parent)
    return DWG_ERR_INTERNALERROR;
  if (capture->matches)
    return 0;
  filedeplist = &object->parent->filedeplist;
  capture->matches
      = (!capture->compare_full || capture->baseline)
        && filedeplist->num_features && filedeplist->features
        && filedeplist->num_files && filedeplist->files
        && stream_filedeplist_text_equal (
            object->parent, filedeplist->features[0], capture->feature)
        && stream_filedeplist_text_equal (
            object->parent, filedeplist->files[0].filename, capture->filename)
        && (!capture->compare_full
            || stream_filedeplists_equal (object->parent, capture->baseline));
  return 0;
}

static int
capture_stream_header (const Dwg_Stream_Object_Info *info,
                       const Dwg_Object *object, void *user)
{
  Stream_Header_Capture *capture = (Stream_Header_Capture *)user;

  (void)info;
  if (!object || !object->parent)
    return DWG_ERR_INTERNALERROR;
  capture->insunits = object->parent->header_vars.INSUNITS;
  capture->calls++;
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
      dwg->header_vars.INSUNITS = 6;
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
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Header_Capture header = { 0 };
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
  if (version == R_2007 && blocking.header_vars.INSUNITS != 6)
    {
      printf ("generated R2007 blocking INSUNITS=%u expected=6\n",
              (unsigned)blocking.header_vars.INSUNITS);
      dwg_free (&blocking);
      remove (path);
      return 1;
    }
  dwg_free (&blocking);

  if (version == R_2007)
    {
      callbacks.decoded_object = capture_stream_header;
      error = dwg_stream_file_ex (path, &callbacks, &header);
      if (error >= DWG_ERR_CRITICAL || !header.calls || header.insunits != 6)
        {
          printf ("generated R2007 Stream INSUNITS failed: error=0x%x "
                  "calls=%lu value=%u expected=6\n",
                  error, (unsigned long)header.calls,
                  (unsigned)header.insunits);
          remove (path);
          return 1;
        }
    }

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
test_stream_filedeplist_metadata (void)
{
  const struct
  {
    const char *path;
    const char *feature;
    const char *filename;
    BITCODE_RL feature_index;
    BITCODE_RL timestamp;
    BITCODE_RL filesize;
    BITCODE_RS affects_graphics;
    BITCODE_RL refcount;
    int compare_full;
  } fixtures[] = {
    { "test/test-data/2007/Line.dwg", "Acad:Text", "arial.ttf", 0, 995532864,
      772192, 1, 2, 1 },
    { "test/test-data/2010/Line.dwg", "Acad:Text", "arial.ttf", 0, 995532864,
      772192, 1, 2, 1 },
    { "test/test-data/2013/Line.dwg", "Acad:Text", "arial.ttf", 0, 995532864,
      772192, 1, 2, 1 },
    { "test/test-data/2013/gh44-error.dwg", "Acad:XRef",
      "N:\\2017\\Royal Wharf Phase 3\\3. Design\\BS9251 (RS)\\"
      "Incoming-Client\\Architectural\\RCP's\\Plot 20.01\\"
      "Latest 22-08-18\\C1203-TCL-ME-20.01-011_iss2_revC2.dwg",
      0, 0, 0, 0, 0, 0 },
  };
  size_t i;

  for (i = 0; i < sizeof (fixtures) / sizeof (fixtures[0]); i++)
    {
      Dwg_Stream_Callbacks_Ex callbacks = { 0 };
      Stream_FileDepList_Capture capture = { 0 };
      Dwg_Data blocking = { 0 };
      char path[1024];
      int error;

      if (!stream_test_source_path (path, sizeof (path), fixtures[i].path))
        {
          printf ("FileDepList fixture path too long: %s\n", fixtures[i].path);
          return 1;
        }
      error = dwg_read_file (path, &blocking);
      if (error >= DWG_ERR_CRITICAL)
        {
          printf ("Blocking FileDepList metadata failed: %s error=0x%x\n",
                  fixtures[i].path, error);
          dwg_free (&blocking);
          return 1;
        }
      if (fixtures[i].compare_full
          && (!blocking.filedeplist.num_files || !blocking.filedeplist.files
              || blocking.filedeplist.files[0].feature_index
                     != fixtures[i].feature_index
              || blocking.filedeplist.files[0].timestamp
                     != fixtures[i].timestamp
              || blocking.filedeplist.files[0].filesize != fixtures[i].filesize
              || blocking.filedeplist.files[0].affects_graphics
                     != fixtures[i].affects_graphics
              || blocking.filedeplist.files[0].refcount
                     != fixtures[i].refcount))
        {
          printf ("Blocking FileDepList numeric metadata failed: %s\n",
                  fixtures[i].path);
          dwg_free (&blocking);
          return 1;
        }
      capture.baseline = &blocking;
      capture.feature = fixtures[i].feature;
      capture.filename = fixtures[i].filename;
      capture.compare_full = fixtures[i].compare_full;
      callbacks.decoded_object = capture_stream_filedeplist;
      callbacks.flags
          = DWG_STREAM_F_NO_FULL_FALLBACK | DWG_STREAM_F_LOW_MEMORY;
      error = dwg_stream_file_ex (path, &callbacks, &capture);
      if (error >= DWG_ERR_CRITICAL || !capture.calls || !capture.matches)
        {
          printf ("Stream FileDepList metadata failed: %s error=0x%x "
                  "calls=%lu matches=%lu\n",
                  fixtures[i].path, error, (unsigned long)capture.calls,
                  (unsigned long)capture.matches);
          dwg_free (&blocking);
          return 1;
        }
      dwg_free (&blocking);
    }
  return 0;
}

static int
test_proxy_handle_encoding_preserved (void)
{
  static const Dwg_Handle handles[]
      = { { 5, 1, 0, 0 }, { 5, 3, 1, 0 } };
  Dwg_Data dwg = { 0 };
  size_t i;
  int failed = 0;

  dwg.opts = DWG_OPTS_STREAM_DECODE;
  for (i = 0; i < sizeof (handles) / sizeof (handles[0]); i++)
    {
      Dwg_Object_Ref *ref
          = dwg_decode_proxy_handleref (&dwg, &handles[i]);
      if (!ref)
        {
          printf ("failed to allocate raw PROXY_OBJECT handle %lu\n",
                  (unsigned long)i);
          failed = 1;
          break;
        }
      if (ref->handleref.code != handles[i].code
          || ref->handleref.size != handles[i].size
          || ref->handleref.value != handles[i].value
          || ref->absolute_ref != handles[i].value)
        {
          printf ("raw PROXY_OBJECT handle %lu changed: got " FORMAT_REF
                  " expected (" FORMAT_H ") abs:" FORMAT_HV "\n",
                  (unsigned long)i, ARGS_REF (ref), ARGS_H (handles[i]),
                  handles[i].value);
          failed = 1;
          break;
        }
    }

  for (i = 0; i < dwg.num_object_refs; i++)
    free (dwg.object_ref[i]);
  free (dwg.object_ref);
  return failed;
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
copy_file_prefix (const char *source_path, const char *output_path,
                  size_t prefix_size)
{
  unsigned char buffer[8192];
  FILE *source;
  FILE *output;
  size_t remaining = prefix_size;
  int failed = 0;

  source = fopen (source_path, "rb");
  if (!source)
    return 1;
  output = fopen (output_path, "wb");
  if (!output)
    {
      fclose (source);
      return 1;
    }
  while (remaining)
    {
      const size_t chunk_size = MIN (remaining, sizeof (buffer));
      if (fread (buffer, 1, chunk_size, source) != chunk_size
          || fwrite (buffer, 1, chunk_size, output) != chunk_size)
        {
          failed = 1;
          break;
        }
      remaining -= chunk_size;
    }
  if (fclose (output) != 0)
    failed = 1;
  fclose (source);
  if (failed)
    remove (output_path);
  return failed;
}

static int
test_r2004_truncated_section_map_rejected (void)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  Stream_Stats stats = { 0 };
  Dwg_Data blocking = { 0 };
  char source_path[128];
  char truncated_path[128];
  size_t truncated_size;
  int error;

  error = write_generated_minsert_fixture (
      R_2004b, "truncated section map", source_path, sizeof (source_path));
  if (error)
    return error;
  error = dwg_read_file (source_path, &blocking);
  if (error >= DWG_ERR_CRITICAL)
    {
      dwg_free (&blocking);
      remove (source_path);
      return 1;
    }
  truncated_size
      = (size_t)blocking.fhdr.r2004_header.section_map_address + 0x80;
  dwg_free (&blocking);
  snprintf (truncated_path, sizeof (truncated_path),
            "stream_r2004_truncated_map_%ld_%ld.dwg",
            stream_test_process_id (), (long)time (NULL));
  if (copy_file_prefix (source_path, truncated_path, truncated_size))
    {
      remove (source_path);
      return 1;
    }
  remove (source_path);

  memset (&blocking, 0, sizeof (blocking));
  error = dwg_read_file (truncated_path, &blocking);
  dwg_free (&blocking);
  if (!(error & DWG_ERR_VALUEOUTOFBOUNDS))
    {
      printf ("truncated R2004 blocking read was not rejected: error=0x%x\n",
              error);
      remove (truncated_path);
      return 1;
    }

  callbacks.object = stream_object_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file_ex (truncated_path, &callbacks, &stats);
  remove (truncated_path);
  if (!(error & DWG_ERR_VALUEOUTOFBOUNDS) || stats.num_objects
      || stats.full_decode_objects)
    {
      printf ("truncated R2004 Stream read was not rejected: error=0x%x "
              "objects=%lu full=%lu\n",
              error, (unsigned long)stats.num_objects,
              (unsigned long)stats.full_decode_objects);
      return 1;
    }
  return 0;
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
