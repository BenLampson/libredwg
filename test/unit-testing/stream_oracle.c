/*  Copyright (C) 2026 Free Software Foundation, Inc.                        */
/*                                                                          */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 3 of the License, or (at your option) any later version.  */

/* Out-of-process canonical Stream oracle for BenStreamDWG parity tests. */

#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <psapi.h>
#else
#  include <sys/resource.h>
#  include <time.h>
#endif

#include "bits.h"
#include "dwg.h"
#include "out_json.h"

typedef struct _stream_oracle_state
{
  FILE *output;
  char magic[7];
  unsigned long long object_count;
  unsigned long long entity_count;
  unsigned long long non_entity_count;
  unsigned long long decoded_count;
  unsigned long long decode_error_count;
  unsigned long long handle_mix;
  int document_written;
  int filedeplist_written;
  int filedeplist_core;
  int io_error;
} Stream_Oracle_State;

static uint64_t
benchmark_now_nanoseconds (void)
{
#ifdef _WIN32
  LARGE_INTEGER counter;
  LARGE_INTEGER frequency;
  if (!QueryPerformanceCounter (&counter)
      || !QueryPerformanceFrequency (&frequency) || frequency.QuadPart <= 0)
    return 0;
  return (uint64_t)((long double)counter.QuadPart * 1000000000.0L
                    / (long double)frequency.QuadPart);
#else
  struct timespec value;
  if (clock_gettime (CLOCK_MONOTONIC, &value) != 0)
    return 0;
  return (uint64_t)value.tv_sec * UINT64_C (1000000000)
         + (uint64_t)value.tv_nsec;
#endif
}

static uint64_t
benchmark_peak_working_set_bytes (void)
{
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS counters;
  memset (&counters, 0, sizeof (counters));
  counters.cb = sizeof (counters);
  if (!GetProcessMemoryInfo (GetCurrentProcess (), &counters,
                             sizeof (counters)))
    return 0;
  return (uint64_t)counters.PeakWorkingSetSize;
#else
  struct rusage usage;
  if (getrusage (RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0)
    return 0;
#  ifdef __APPLE__
  return (uint64_t)usage.ru_maxrss;
#  else
  return (uint64_t)usage.ru_maxrss * UINT64_C (1024);
#  endif
#endif
}

static int
trace_hex_bytes (FILE *restrict output, const unsigned char *restrict bytes,
                 size_t size)
{
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0; i < size; i++)
    {
      if (fputc (digits[bytes[i] >> 4], output) == EOF
          || fputc (digits[bytes[i] & 0x0f], output) == EOF)
        return 0;
    }
  return 1;
}

static int
trace_hex_text (FILE *restrict output, const char *text)
{
  if (!text)
    return 1;
  return trace_hex_bytes (output, (const unsigned char *)text, strlen (text));
}

static int
trace_hex_t32 (FILE *restrict output, const Dwg_Data *restrict dwg,
               const BITCODE_T32 value)
{
  char *utf8;
  int ok;

  if (!value)
    return 1;
  if (dwg->header.from_version < R_2007)
    return trace_hex_text (output, value);
  utf8 = bit_convert_TU ((BITCODE_TU)value);
  if (!utf8)
    return 0;
  ok = trace_hex_text (output, utf8);
  free (utf8);
  return ok;
}

static int
trace_hex_file (FILE *restrict output, FILE *restrict input)
{
  unsigned char buffer[4096];
  size_t size;

  if (fflush (input) != 0 || fseek (input, 0, SEEK_SET) != 0)
    return 0;
  while ((size = fread (buffer, 1, sizeof (buffer), input)) > 0)
    {
      if (!trace_hex_bytes (output, buffer, size))
        return 0;
    }
  return ferror (input) == 0 && ferror (output) == 0;
}

static int
trace_document (Stream_Oracle_State *restrict state,
                const Dwg_Stream_Object_Info *restrict info)
{
  if (state->document_written)
    return 1;
  if (fputs ("DOCUMENT\t", state->output) == EOF
      || !trace_hex_bytes (state->output, (const unsigned char *)state->magic,
                           6)
      || fprintf (state->output, "\t%u\t\n", (unsigned)info->version) < 0)
    return 0;
  state->document_written = 1;
  return 1;
}

static int
trace_info (Stream_Oracle_State *restrict state,
            const Dwg_Stream_Object_Info *restrict info)
{
  if (fprintf (state->output,
               "INFO\t%lu\t%zu\t%lu\t%llu\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t",
               (unsigned long)info->index, info->address,
               (unsigned long)info->size,
               (unsigned long long)info->handle.value,
               (unsigned)info->handle.code, (unsigned)info->handle.size,
               (unsigned)info->type, (unsigned)info->fixedtype,
               (unsigned)info->supertype, (unsigned)info->version,
               (unsigned)info->decode_mode, (unsigned)info->input_mode)
          < 0
      || !trace_hex_text (state->output, info->name)
      || fputc ('\t', state->output) == EOF
      || !trace_hex_text (state->output, info->dxfname)
      || fputc ('\n', state->output) == EOF)
    return 0;
  return 1;
}

static int
trace_object (Stream_Oracle_State *restrict state,
              const Dwg_Stream_Object_Info *restrict info,
              const Dwg_Object *restrict object)
{
  Dwg_Object normalized;
  Bit_Chain json = { 0 };
  int error;
  int ok;

  normalized = *object;
  normalized.index = info->index;
  json.fh = tmpfile ();
  if (!json.fh)
    return 0;
  json.opts = DWG_OPTS_MINIMAL;
  error = dwg_write_json_object (&json, &normalized);
  if (error >= DWG_ERR_CRITICAL)
    {
      fclose (json.fh);
      return 0;
    }

  ok = fprintf (
           state->output,
           "OBJECT\t%lu\t%zu\t%lu\t%llu\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t",
           (unsigned long)info->index, info->address,
           (unsigned long)info->size,
           (unsigned long long)object->handle.value,
           (unsigned)object->handle.code, (unsigned)object->handle.size,
           (unsigned)object->type, (unsigned)object->fixedtype,
           (unsigned)object->supertype, (unsigned)info->version,
           (unsigned)info->decode_mode, (unsigned)info->input_mode)
           >= 0
       && trace_hex_file (state->output, json.fh)
       && fputc ('\n', state->output) != EOF;
  fclose (json.fh);
  return ok;
}

static int
oracle_object_callback (const Dwg_Stream_Object_Info *restrict info,
                        void *restrict user)
{
  Stream_Oracle_State *state = (Stream_Oracle_State *)user;

  if (!info || !trace_document (state, info) || !trace_info (state, info))
    {
      state->io_error = 1;
      return DWG_ERR_IOERROR;
    }
  state->object_count++;
  if (info->supertype == DWG_SUPERTYPE_ENTITY)
    state->entity_count++;
  else
    state->non_entity_count++;
  state->handle_mix
      ^= (unsigned long long)info->handle.value
         + ((unsigned long long)info->type << 33)
         + ((unsigned long long)info->supertype << 49);
  return 0;
}

static int
benchmark_info_callback (const Dwg_Stream_Object_Info *restrict info,
                         void *restrict user)
{
  Stream_Oracle_State *state = (Stream_Oracle_State *)user;
  if (!info)
    return DWG_ERR_INTERNALERROR;
  state->object_count++;
  if (info->supertype == DWG_SUPERTYPE_ENTITY)
    state->entity_count++;
  else
    state->non_entity_count++;
  state->handle_mix
      ^= (unsigned long long)info->handle.value
         + ((unsigned long long)info->type << 33)
         + ((unsigned long long)info->supertype << 49);
  return 0;
}

static int
oracle_decoded_callback (const Dwg_Stream_Object_Info *restrict info,
                         const Dwg_Object *restrict object,
                         void *restrict user)
{
  Stream_Oracle_State *state = (Stream_Oracle_State *)user;

  if (!info || !object || !trace_object (state, info, object))
    {
      state->io_error = 1;
      return DWG_ERR_IOERROR;
    }
  state->decoded_count++;
  return 0;
}

static int
filedeplist_callback (const Dwg_Stream_Object_Info *restrict info,
                      const Dwg_Object *restrict object,
                      void *restrict user)
{
  Stream_Oracle_State *state = (Stream_Oracle_State *)user;
  const Dwg_Data *dwg;
  const Dwg_FileDepList *dependencies;
  BITCODE_RL i;

  (void)info;
  if (state->filedeplist_written)
    return 0;
  if (!object || !object->parent)
    return DWG_ERR_INTERNALERROR;
  dwg = object->parent;
  dependencies = &dwg->filedeplist;
  if (fputs ("BENSTREAMDWG_FILEDEPLIST\t1\n", state->output) == EOF)
    goto io_error;
  if (state->filedeplist_core)
    {
      if (!dependencies->num_features || !dependencies->features
          || !dependencies->num_files || !dependencies->files
          || fputs ("FEATURE\t0\t", state->output) == EOF
          || !trace_hex_t32 (state->output, dwg, dependencies->features[0])
          || fputs ("\nFILE\t0\t", state->output) == EOF
          || !trace_hex_t32 (state->output, dwg,
                             dependencies->files[0].filename)
          || fputs ("\nEND\n", state->output) == EOF)
        goto io_error;
      state->filedeplist_written = 1;
      return 0;
    }
  for (i = 0; i < dependencies->num_features; i++)
    if (fprintf (state->output, "FEATURE\t%lu\t", (unsigned long)i) < 0
        || !trace_hex_t32 (state->output, dwg, dependencies->features[i])
        || fputc ('\n', state->output) == EOF)
      goto io_error;
  for (i = 0; i < dependencies->num_files; i++)
    {
      const Dwg_FileDepList_Files *file = &dependencies->files[i];
      if (fprintf (state->output, "FILE\t%lu\t", (unsigned long)i) < 0
          || !trace_hex_t32 (state->output, dwg, file->filename)
          || fputc ('\t', state->output) == EOF
          || !trace_hex_t32 (state->output, dwg, file->filepath)
          || fputc ('\t', state->output) == EOF
          || !trace_hex_t32 (state->output, dwg, file->fingerprint)
          || fputc ('\t', state->output) == EOF
          || !trace_hex_t32 (state->output, dwg, file->version)
          || fprintf (state->output, "\t%lu\t%lu\t%lu\t%u\t%lu\n",
                      (unsigned long)file->feature_index,
                      (unsigned long)file->timestamp,
                      (unsigned long)file->filesize,
                      (unsigned)file->affects_graphics,
                      (unsigned long)file->refcount)
                 < 0)
        goto io_error;
    }
  if (fputs ("END\n", state->output) == EOF)
    goto io_error;
  state->filedeplist_written = 1;
  return 0;

io_error:
  state->io_error = 1;
  return DWG_ERR_IOERROR;
}

static int
oracle_error_callback (const Dwg_Stream_Object_Info *restrict info, int error,
                       void *restrict user)
{
  Stream_Oracle_State *state = (Stream_Oracle_State *)user;

  if (!info
      || fprintf (state->output,
                  "ERROR\t%lu\t%zu\t%lu\t%llu\t%u\t%u\t%d\t",
                  (unsigned long)info->index, info->address,
                  (unsigned long)info->size,
                  (unsigned long long)info->handle.value,
                  (unsigned)info->type, (unsigned)info->supertype, error)
             < 0
      || !trace_hex_text (state->output, info->name)
      || fputc ('\t', state->output) == EOF
      || !trace_hex_text (state->output, info->dxfname)
      || fputc ('\n', state->output) == EOF)
    {
      state->io_error = 1;
      return DWG_ERR_IOERROR;
    }
  state->decode_error_count++;
  return 0;
}

static int
read_magic (const char *restrict path, char magic[7])
{
  FILE *input;
  size_t size;

  input = fopen (path, "rb");
  if (!input)
    return 0;
  size = fread (magic, 1, 6, input);
  fclose (input);
  magic[6] = '\0';
  return size == 6;
}

int
main (int argc, char **argv)
{
  Stream_Oracle_State state = { 0 };
  Dwg_Stream_Callbacks_Ex callbacks = { 0 };
  const char *input_path;
  const char *output_path = NULL;
  unsigned int backend_flags = 0;
  int benchmark_info = 0;
  int filedeplist = 0;
  int argument = 1;
  int stream_error;
  int close_error = 0;
  uint64_t benchmark_start = 0;
  uint64_t benchmark_end = 0;

  if (argument < argc && strcmp (argv[argument], "--low-memory") == 0)
    {
      backend_flags = DWG_STREAM_F_LOW_MEMORY;
      argument++;
    }
  if (argument < argc && strcmp (argv[argument], "--benchmark-info") == 0)
    {
      benchmark_info = 1;
      argument++;
    }
  if (argument < argc && strcmp (argv[argument], "--filedeplist") == 0)
    {
      filedeplist = 1;
      argument++;
    }
  else if (argument < argc
           && strcmp (argv[argument], "--filedeplist-core") == 0)
    {
      filedeplist = 1;
      state.filedeplist_core = 1;
      argument++;
    }
  if (argc - argument != 1 && argc - argument != 2)
    {
      fprintf (stderr,
               "usage: stream_oracle [--low-memory] [--benchmark-info] "
               "[--filedeplist|--filedeplist-core] "
               "INPUT.dwg "
               "[OUTPUT.trace]\n");
      return 2;
    }
  input_path = argv[argument++];
  if (argument < argc)
    output_path = argv[argument];
  if (benchmark_info && output_path)
    {
      fprintf (stderr, "--benchmark-info does not accept an output path\n");
      return 2;
    }

  if (!read_magic (input_path, state.magic))
    {
      fprintf (stderr, "failed to read DWG header: %s\n", input_path);
      return 2;
    }

  state.output = output_path ? fopen (output_path, "wb") : stdout;
  if (!state.output)
    {
      fprintf (stderr, "failed to open trace output: %s\n", output_path);
      return 2;
    }
  if (!benchmark_info && !filedeplist
      && fputs ("BENSTREAMDWG_TRACE\t1\n", state.output) == EOF)
    state.io_error = 1;

  callbacks.object = filedeplist ? NULL
                                 : (benchmark_info ? benchmark_info_callback
                                                   : oracle_object_callback);
  callbacks.decoded_object
      = filedeplist ? filedeplist_callback
                    : (benchmark_info ? NULL : oracle_decoded_callback);
  callbacks.decode_error
      = benchmark_info || filedeplist ? NULL : oracle_error_callback;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK | backend_flags;
  benchmark_start = benchmark_now_nanoseconds ();
  stream_error = dwg_stream_file_ex (input_path, &callbacks, &state);
  benchmark_end = benchmark_now_nanoseconds ();

  if (filedeplist)
    {
      if (!state.filedeplist_written || stream_error >= DWG_ERR_CRITICAL)
        state.io_error = 1;
    }
  else if (benchmark_info)
    {
      if (!benchmark_start || benchmark_end <= benchmark_start
          || fprintf (state.output,
                      "BENSTREAMDWG_BENCHMARK\t1\trecord-count-v1\n"
                      "MEASUREMENT\t%s\t%llu\t%llu\t%llu\t0\n",
                      input_path,
                      (unsigned long long)(benchmark_end - benchmark_start),
                      (unsigned long long)benchmark_peak_working_set_bytes (),
                      state.object_count)
                 < 0)
        state.io_error = 1;
    }
  else if (fprintf (
               state.output,
               "SUMMARY\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t0\t%d\nEND\n",
               state.object_count, state.entity_count, state.non_entity_count,
               state.decoded_count, state.decode_error_count, state.handle_mix,
               stream_error)
           < 0)
    state.io_error = 1;
  if (output_path)
    close_error = fclose (state.output);
  else if (fflush (state.output) != 0)
    close_error = 1;

  return state.io_error || close_error ? 2 : 0;
}
