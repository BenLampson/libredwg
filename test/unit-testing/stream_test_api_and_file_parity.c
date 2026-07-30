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
stream_callback_trace_reserve_event (Stream_Callback_Trace *restrict trace,
                                     Stream_Callback_Event **restrict event)
{
  if (trace->num_events == trace->events_capacity)
    {
      size_t new_capacity
          = trace->events_capacity ? trace->events_capacity * 2U : 1024U;
      Stream_Callback_Event *new_events;

      if (new_capacity < trace->events_capacity
          || new_capacity > SIZE_MAX / sizeof (*trace->events))
        return DWG_ERR_OUTOFMEM;
      new_events = (Stream_Callback_Event *)realloc (
          trace->events, new_capacity * sizeof (*trace->events));
      if (!new_events)
        return DWG_ERR_OUTOFMEM;
      trace->events = new_events;
      trace->events_capacity = new_capacity;
    }
  *event = &trace->events[trace->num_events++];
  memset (*event, 0, sizeof (**event));
  return 0;
}

static int
stream_callback_trace_append_acds (Stream_Callback_Trace *restrict trace,
                                   Stream_Callback_Event *restrict event,
                                   const BITCODE_RC *restrict data,
                                   const size_t size)
{
  size_t required;

  if (!data || !size)
    return 0;
  if (size > SIZE_MAX - trace->acds_size)
    return DWG_ERR_OUTOFMEM;
  required = trace->acds_size + size;
  if (required > trace->acds_capacity)
    {
      size_t new_capacity
          = trace->acds_capacity ? trace->acds_capacity : 4096U;
      BITCODE_RC *new_acds;

      while (new_capacity < required)
        {
          if (new_capacity > SIZE_MAX / 2U)
            {
              new_capacity = required;
              break;
            }
          new_capacity *= 2U;
        }
      new_acds = (BITCODE_RC *)realloc (trace->acds, new_capacity);
      if (!new_acds)
        return DWG_ERR_OUTOFMEM;
      trace->acds = new_acds;
      trace->acds_capacity = new_capacity;
    }
  event->acds_offset = trace->acds_size;
  event->acds_size = size;
  memcpy (&trace->acds[trace->acds_size], data, size);
  trace->acds_size = required;
  return 0;
}

static void
stream_callback_trace_copy_info (Stream_Callback_Event *restrict event,
                                 const Dwg_Stream_Object_Info *restrict info)
{
  event->index = info->index;
  event->address = info->address;
  event->size = info->size;
  event->info_handle = info->handle.value;
  event->info_handle_code = info->handle.code;
  event->info_handle_size = info->handle.size;
  event->type = info->type;
  event->fixedtype = info->fixedtype;
  event->supertype = info->supertype;
  event->version = info->version;
  event->decode_mode = info->decode_mode;
  event->input_mode = info->input_mode;
}

static int
stream_callback_trace_object (const Dwg_Stream_Object_Info *restrict info,
                              void *restrict user)
{
  Stream_Callback_Trace *trace = (Stream_Callback_Trace *)user;
  Stream_Callback_Event *event;
  int error;

  error = stream_callback_trace_reserve_event (trace, &event);
  if (error)
    return error;
  event->kind = STREAM_CALLBACK_EVENT_OBJECT;
  stream_callback_trace_copy_info (event, info);
  return 0;
}

static int
stream_callback_trace_decoded (const Dwg_Stream_Object_Info *restrict info,
                               const Dwg_Object *restrict object,
                               void *restrict user)
{
  Stream_Callback_Trace *trace = (Stream_Callback_Trace *)user;
  Stream_Callback_Event *event;
  int error;

  error = stream_callback_trace_reserve_event (trace, &event);
  if (error)
    return error;
  event->kind = STREAM_CALLBACK_EVENT_DECODED;
  stream_callback_trace_copy_info (event, info);
  event->object_index = object->index;
  event->object_address = object->address;
  event->object_size = object->size;
  event->object_handle = object->handle.value;
  event->object_type = object->type;
  event->object_fixedtype = object->fixedtype;
  event->object_supertype = object->supertype;
  if (object->supertype == DWG_SUPERTYPE_ENTITY && object->tio.entity
      && object->tio.entity->has_ds_data && dwg_obj_is_3dsolid (object))
    {
      const Dwg_Entity_3DSOLID *solid = object->tio.entity->tio._3DSOLID;

      if (solid)
        return stream_callback_trace_append_acds (
            trace, event, solid->acis_data, solid->sab_size);
    }
  return 0;
}

static int
stream_callback_trace_error (const Dwg_Stream_Object_Info *restrict info,
                             const int decode_error, void *restrict user)
{
  Stream_Callback_Trace *trace = (Stream_Callback_Trace *)user;
  Stream_Callback_Event *event;
  int error;

  error = stream_callback_trace_reserve_event (trace, &event);
  if (error)
    return error;
  event->kind = STREAM_CALLBACK_EVENT_ERROR;
  stream_callback_trace_copy_info (event, info);
  event->error = decode_error;
  return 0;
}

static void
stream_callback_trace_free (Stream_Callback_Trace *restrict trace)
{
  free (trace->events);
  free (trace->acds);
  memset (trace, 0, sizeof (*trace));
}

static int
stream_callback_trace_run (const char *restrict path,
                           const unsigned int backend_flag,
                           Stream_Callback_Trace *restrict trace)
{
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };

  callbacks.object = stream_callback_trace_object;
  callbacks.decoded_object = stream_callback_trace_decoded;
  callbacks.decode_error = stream_callback_trace_error;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK | backend_flag;
  return dwg_stream_file_ex (path, &callbacks, trace);
}

static int
stream_callback_traces_equal (const Stream_Callback_Trace *restrict left,
                              const Stream_Callback_Trace *restrict right,
                              size_t *restrict mismatch)
{
  size_t i;

  if (left->num_events != right->num_events
      || left->acds_size != right->acds_size)
    return 0;
  for (i = 0; i < left->num_events; i++)
    if (memcmp (&left->events[i], &right->events[i], sizeof (left->events[i]))
        != 0)
      {
        *mismatch = i;
        return 0;
      }
  if (left->acds_size
      && memcmp (left->acds, right->acds, left->acds_size) != 0)
    {
      *mismatch = left->num_events;
      return 0;
    }
  return 1;
}

static int
test_r2004_page_cache_backend_parity (void)
{
  const char *fixtures[]
      = { "test/test-data/example_2004.dwg",
          "test/test-data/2004/material.dwg",
          "test/test-data/example_2010.dwg", "test/test-data/example_2013.dwg",
          "test/test-data/2018/Dynblocks.dwg" };
  size_t i;

  for (i = 0; i < sizeof (fixtures) / sizeof (fixtures[0]); i++)
    {
      char path[1024];
      Stream_Callback_Trace buffered = { 0 };
      Stream_Callback_Trace page_cached = { 0 };
      size_t mismatch = 0;
      int buffered_error;
      int page_cached_error;
      int equal;

      if (!stream_test_source_path (path, sizeof (path), fixtures[i]))
        {
          printf ("R2004 backend parity fixture path too long: %s\n",
                  fixtures[i]);
          return 1;
        }
      buffered_error = stream_callback_trace_run (path, 0, &buffered);
      page_cached_error = stream_callback_trace_run (
          path, DWG_STREAM_F_LOW_MEMORY, &page_cached);
      equal
          = stream_callback_traces_equal (&buffered, &page_cached, &mismatch);
      if (buffered_error != page_cached_error || !buffered.num_events || !equal
          || (strstr (fixtures[i], "example_2013.dwg") && !buffered.acds_size))
        {
          printf ("R2004 backend parity failed: %s errors=0x%x/0x%x "
                  "events=%zu/%zu acds=%zu/%zu mismatch=%zu\n",
                  fixtures[i], buffered_error, page_cached_error,
                  buffered.num_events, page_cached.num_events,
                  buffered.acds_size, page_cached.acds_size, mismatch);
          if (mismatch < buffered.num_events
              && mismatch < page_cached.num_events)
            printf (
                "event kinds=%d/%d indexes=%lu/%lu "
                "addresses=%zu/%zu handles=%llu/%llu\n",
                (int)buffered.events[mismatch].kind,
                (int)page_cached.events[mismatch].kind,
                (unsigned long)buffered.events[mismatch].index,
                (unsigned long)page_cached.events[mismatch].index,
                buffered.events[mismatch].address,
                page_cached.events[mismatch].address,
                (unsigned long long)buffered.events[mismatch].info_handle,
                (unsigned long long)page_cached.events[mismatch].info_handle);
          stream_callback_trace_free (&buffered);
          stream_callback_trace_free (&page_cached);
          return 1;
        }
      stream_callback_trace_free (&buffered);
      stream_callback_trace_free (&page_cached);
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
  const char *backend = getenv ("LIBREDWG_STREAM_TEST_R2004_BACKEND");
  const char *expected_objects_text
      = getenv ("LIBREDWG_STREAM_TEST_EXPECT_OBJECTS");
  const char *expected_error_text
      = getenv ("LIBREDWG_STREAM_TEST_EXPECT_ERROR");
  const int large_only
      = getenv ("LIBREDWG_STREAM_TEST_LARGE_ONLY") != NULL;
  char *expected_end;
  unsigned long long expected_objects;
  unsigned long expected_error;
  FILE *fp;
  clock_t started;
  int error;
  int decode_objects = 0;

  if (!path || !*path)
    {
      if (large_only)
        printf ("large-only stream test requires "
                "LIBREDWG_STREAM_TEST_LARGE_DWG\n");
      return large_only ? 1 : 0;
    }

  fp = fopen (path, "rb");
  if (!fp)
    {
      printf ("%s: cannot open large stream fixture %s\n",
              large_only ? "failed" : "skip", path);
      return large_only ? 1 : 0;
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
  if (backend && strcmp (backend, "low-memory") == 0)
    callbacks.flags |= DWG_STREAM_F_LOW_MEMORY;
  started = clock ();
  error = dwg_stream_file_ex (path, &callbacks, &stats);
  if (backend)
    printf ("large stream backend timing: backend=%s cpu_ms=%.2f error=0x%x\n",
            backend, 1000.0 * (double)(clock () - started) / CLOCKS_PER_SEC,
            error);
  if (expected_error_text)
    {
      expected_error = strtoul (expected_error_text, &expected_end, 0);
      if (!*expected_error_text || *expected_end
          || error != (int)expected_error)
        {
          printf ("large stream fixture error mismatch: got=0x%x "
                  "expected=%s\n",
                  error, expected_error_text);
          return 1;
        }
    }
  if (expected_objects_text)
    {
      expected_objects = strtoull (expected_objects_text, &expected_end, 0);
      if (!*expected_objects_text || *expected_end
          || (unsigned long long)stats.num_objects != expected_objects)
        {
          printf ("large stream fixture object mismatch: got=%lu "
                  "expected=%s\n",
                  (unsigned long)stats.num_objects, expected_objects_text);
          return 1;
        }
    }
  if (error >= DWG_ERR_CRITICAL)
    {
      if (expected_error_text)
        return 0;
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
  const char *marker = "test/unit-testing/stream_test_api_and_file_parity.c";
  const char *pos = strstr (file, marker);
  FILE *probe;
  size_t root_len;
  int written;

  probe = fopen (relative, "rb");
  if (probe)
    {
      fclose (probe);
      written = snprintf (path, size, "%s", relative);
      return written >= 0 && (size_t)written < size;
    }
  if (!pos)
    {
      marker = "test\\unit-testing\\stream_test_api_and_file_parity.c";
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
stream_parity_baseline_alloc (Dwg_Data *dwg, Stream_Stats *streamed)
{
  if (!dwg->num_objects)
    return 1;
  streamed->baseline_objects_matched
      = (unsigned char *)calloc (dwg->num_objects, 1);
  return streamed->baseline_objects_matched != NULL;
}

static void
stream_parity_baseline_free (Dwg_Data *dwg, Stream_Stats *baseline,
                             Stream_Stats *streamed)
{
  dwg_free (dwg);
  free (baseline->baseline_refs);
  baseline->baseline_refs = NULL;
  free (streamed->baseline_objects_matched);
  streamed->baseline_objects_matched = NULL;
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
  streamed.baseline_dwg = &dwg;
  streamed.baseline_refs = baseline.baseline_refs;
  streamed.baseline_ref_count = baseline.baseline_ref_count;
  if (!stream_parity_baseline_alloc (&dwg, &streamed))
    {
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
      return 1;
    }

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
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
      return 1;
    }

  if (!stats_equal (&baseline, &streamed))
    {
      print_stats ("baseline", &baseline);
      print_stats ("streamed", &streamed);
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
      return 1;
    }
  if (streamed.lightweight_objects != streamed.num_objects
      || streamed.full_decode_objects)
    {
      print_stats ("streamed did not use the lightweight path", &streamed);
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
      return 1;
    }
  if (streamed.decoded_objects != streamed.num_objects
      || streamed.decoded_entities != streamed.num_entities
      || streamed.decoded_non_entities != streamed.num_non_entities
      || streamed.decoded_handle_mix != baseline.handle_mix
      || streamed.decode_error_objects
      || streamed.canonical_objects_checked != baseline.num_objects
      || streamed.canonical_object_mismatches)
    {
      print_stats ("baseline", &baseline);
      print_stats ("streamed decoded object mismatch", &streamed);
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
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
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
      return 1;
    }
  if (!semantic_coverage_equal (&baseline.semantic, &streamed.semantic))
    {
      print_semantic_coverage ("baseline semantic", &baseline.semantic);
      print_semantic_coverage ("streamed semantic", &streamed.semantic);
      stream_parity_baseline_free (&dwg, &baseline, &streamed);
      return 1;
    }

  if (test_abort_callbacks)
    {
      stream_trace_stage ("decoded-only dwg_stream_file_ex");
      decoded_only.baseline_refs = baseline.baseline_refs;
      decoded_only.baseline_ref_count = baseline.baseline_ref_count;
      decoded_only.baseline_dwg = &dwg;
      memset (streamed.baseline_objects_matched, 0, dwg.num_objects);
      decoded_only.baseline_objects_matched
          = streamed.baseline_objects_matched;
      callbacks.object = NULL;
      callbacks.decoded_object = stream_decoded_object_callback;
      callbacks.decode_error = stream_decode_error_callback;
      error = dwg_stream_file_ex (path, &callbacks, &decoded_only);
      if (error >= DWG_ERR_CRITICAL || decoded_only.num_objects
          || decoded_only.decoded_objects != baseline.num_objects
          || decoded_only.decoded_entities != baseline.num_entities
          || decoded_only.decoded_non_entities != baseline.num_non_entities
          || decoded_only.decode_error_objects
          || decoded_only.canonical_objects_checked != baseline.num_objects
          || decoded_only.canonical_object_mismatches
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
          stream_parity_baseline_free (&dwg, &baseline, &streamed);
          return 1;
        }
      if (!semantic_coverage_equal (&baseline.semantic,
                                    &decoded_only.semantic))
        {
          print_semantic_coverage ("baseline semantic", &baseline.semantic);
          print_semantic_coverage ("decoded-only semantic",
                                   &decoded_only.semantic);
          stream_parity_baseline_free (&dwg, &baseline, &streamed);
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
          stream_parity_baseline_free (&dwg, &baseline, &streamed);
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
          stream_parity_baseline_free (&dwg, &baseline, &streamed);
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
          stream_parity_baseline_free (&dwg, &baseline, &streamed);
          return 1;
        }
    }

  semantic_coverage_add (coverage, &baseline.semantic);
  print_stats (label ? label : "stream parity ok", &streamed);
  stream_parity_baseline_free (&dwg, &baseline, &streamed);
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
                             "test/test-data/2010/Arc.dwg",
                             "test/test-data/2010/circle.dwg",
                             "test/test-data/2010/Constraints.dwg",
                             "test/test-data/2010/ConstructionLine.dwg",
                             "test/test-data/2010/Donut.dwg",
                             "test/test-data/2010/Ellipse.dwg",
                             "test/test-data/2010/gh209_1.dwg",
                             "test/test-data/2010/Helix.dwg",
                             "test/test-data/2010/Leader.dwg",
                             "test/test-data/2010/Line.dwg",
                             "test/test-data/2010/Multiline.dwg",
                             "test/test-data/2010/Point.dwg",
                             "test/test-data/2010/Polygon.dwg",
                             "test/test-data/2010/Polyline.dwg",
                             "test/test-data/2010/PolyLine3D.dwg",
                             "test/test-data/2010/RAY.dwg",
                             "test/test-data/2010/Spline.dwg",
                             "test/test-data/2010/Text.dwg",
                             "test/test-data/2013/Arc.dwg",
                             "test/test-data/2013/circle.dwg",
                             "test/test-data/2013/Constraints.dwg",
                             "test/test-data/2013/ConstructionLine.dwg",
                             "test/test-data/2013/Donut.dwg",
                             "test/test-data/2013/Ellipse.dwg",
                             "test/test-data/2013/gh109_1.dwg",
                             "test/test-data/2013/gh44-error.dwg",
                             "test/test-data/2013/Helix.dwg",
                             "test/test-data/2013/Leader.dwg",
                             "test/test-data/2013/Line.dwg",
                             "test/test-data/2013/Multiline.dwg",
                             "test/test-data/2013/Point.dwg",
                             "test/test-data/2013/Polygon.dwg",
                             "test/test-data/2013/Polyline.dwg",
                             "test/test-data/2013/PolyLine3D.dwg",
                             "test/test-data/2013/RAY.dwg",
                             "test/test-data/2013/Spline.dwg",
                             "test/test-data/2013/Text.dwg",
                             "test/test-data/2018/Arc.dwg",
                             "test/test-data/2018/circle.dwg",
                             "test/test-data/2018/Constraints.dwg",
                             "test/test-data/2018/ConstructionLine.dwg",
                             "test/test-data/2018/Donut.dwg",
                             "test/test-data/2018/Dynblocks.dwg",
                             "test/test-data/2018/Ellipse.dwg",
                             "test/test-data/2018/Helix.dwg",
                             "test/test-data/2018/Leader.dwg",
                             "test/test-data/2018/Line.dwg",
                             "test/test-data/2018/LiveSection1.dwg",
                             "test/test-data/2018/Multiline.dwg",
                             "test/test-data/2018/Point.dwg",
                             "test/test-data/2018/Polygon.dwg",
                             "test/test-data/2018/Polyline.dwg",
                             "test/test-data/2018/PolyLine3D.dwg",
                             "test/test-data/2018/RAY.dwg",
                             "test/test-data/2018/Spline.dwg",
                             "test/test-data/2018/Text.dwg",
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
