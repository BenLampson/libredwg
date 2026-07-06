/*****************************************************************************/
/*  LibreDWG - free implementation of the DWG file format                    */
/*                                                                           */
/*  Copyright (C) 2026 Free Software Foundation, Inc.                        */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 3 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

/*
 * dwgprobe.c: probe DWG object-map statistics without expanding full objects
 */

#include "../src/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#endif
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <psapi.h>
#  ifdef CP_UTF8
#    undef CP_UTF8
#  endif
#elif defined(HAVE_SYS_RESOURCE_H)
#  include <sys/resource.h>
#endif
#include "my_getopt.h"
#include "my_stat.h"

#include <dwg.h>

typedef struct _probe_stats
{
  BITCODE_BL num_objects;
  BITCODE_BL num_entities;
  BITCODE_BL num_non_entities;
  BITCODE_BL lightweight_objects;
  BITCODE_BL r2007_object_map_objects;
  BITCODE_BL r2004_object_map_objects;
  BITCODE_BL full_decode_objects;
  BITCODE_BL file_map_objects;
  BITCODE_BL heap_objects;
  Dwg_Version_Type version;
  unsigned long long total_size;
  BITCODE_RL max_size;
  size_t min_address;
  size_t max_address;
  unsigned long long rss_mb;
} Probe_Stats;

static void sample_rss (Probe_Stats *stats);

static int
usage (void)
{
  printf ("\nUsage: dwgprobe [DWGFILE|DIR]...\n");
  return 1;
}

static int
opt_version (void)
{
  printf ("dwgprobe %s\n", PACKAGE_VERSION);
  return 0;
}

static int
help (void)
{
  printf ("\nUsage: dwgprobe [OPTION]... DWGFILE|DIR...\n");
  printf ("Probe DWG object counts and object record sizes.\n\n");
  printf ("Unsupported DWG versions are reported without full decoding.\n\n");
#ifdef HAVE_GETOPT_LONG
  printf ("      --help           display this help and exit\n");
  printf ("      --version        output version information and exit\n\n");
#else
  printf ("  -h     display this help and exit\n");
  printf ("  -i     output version information and exit\n\n");
#endif
  printf ("GNU LibreDWG online manual: "
          "<https://www.gnu.org/software/libredwg/>\n");
  return 0;
}

static int
probe_object (const Dwg_Stream_Object_Info *info, void *user)
{
  Probe_Stats *stats = (Probe_Stats *)user;

  stats->num_objects++;
  if (!stats->version)
    stats->version = info->version;
  if (info->supertype == DWG_SUPERTYPE_ENTITY)
    stats->num_entities++;
  else
    stats->num_non_entities++;

  stats->total_size += (unsigned long long)info->size;
  if (stats->num_objects == 1 || info->address < stats->min_address)
    stats->min_address = info->address;
  if (info->address > stats->max_address)
    stats->max_address = info->address;
  if (info->size > stats->max_size)
    stats->max_size = info->size;

  if (info->decode_mode == DWG_STREAM_DECODE_R2007_OBJECT_MAP)
    {
      stats->lightweight_objects++;
      stats->r2007_object_map_objects++;
    }
  else if (info->decode_mode == DWG_STREAM_DECODE_R2004_OBJECT_MAP)
    {
      stats->lightweight_objects++;
      stats->r2004_object_map_objects++;
    }
  else if (info->decode_mode == DWG_STREAM_DECODE_FULL)
    stats->full_decode_objects++;

  if (info->input_mode == DWG_STREAM_INPUT_FILE_MAP)
    stats->file_map_objects++;
  else if (info->input_mode == DWG_STREAM_INPUT_HEAP)
    stats->heap_objects++;

  if (stats->num_objects == 1 || (stats->num_objects % 4096UL) == 0)
    sample_rss (stats);

  return 0;
}

static const char *
decode_mode_name (const Probe_Stats *stats)
{
  if (stats->num_objects
      && stats->r2007_object_map_objects == stats->num_objects)
    return "r2007-object-map";
  if (stats->num_objects
      && stats->r2004_object_map_objects == stats->num_objects)
    return "r2004-object-map";
  if (stats->num_objects && stats->full_decode_objects == stats->num_objects)
    return "full";
  if (stats->lightweight_objects || stats->full_decode_objects)
    return "mixed";
  return "none";
}

static const char *
input_mode_name (const Probe_Stats *stats)
{
  if (stats->num_objects && stats->file_map_objects == stats->num_objects)
    return "file-map";
  if (stats->num_objects && stats->heap_objects == stats->num_objects)
    return "heap";
  if (stats->file_map_objects || stats->heap_objects)
    return "mixed";
  return "none";
}

static int
current_rss_mb (unsigned long long *rss_mb)
{
  *rss_mb = 0;
#ifdef _WIN32
  {
    PROCESS_MEMORY_COUNTERS counters;
    memset (&counters, 0, sizeof (counters));
    if (GetProcessMemoryInfo (GetCurrentProcess (), &counters,
                              sizeof (counters)))
      {
        *rss_mb = ((unsigned long long)counters.WorkingSetSize + 1048575ULL)
                  / 1048576ULL;
        return 1;
      }
  }
#elif defined(HAVE_SYS_RESOURCE_H)
  {
    struct rusage usage;
    if (getrusage (RUSAGE_SELF, &usage) == 0)
      {
        unsigned long long bytes;
#  if defined(__APPLE__) && defined(__MACH__)
        bytes = (unsigned long long)usage.ru_maxrss;
#  else
        bytes = (unsigned long long)usage.ru_maxrss * 1024ULL;
#  endif
        *rss_mb = (bytes + 1048575ULL) / 1048576ULL;
        return 1;
      }
  }
#endif
  return 0;
}

static void
sample_rss (Probe_Stats *stats)
{
  unsigned long long rss_mb;

  if (current_rss_mb (&rss_mb) && rss_mb > stats->rss_mb)
    stats->rss_mb = rss_mb;
}

static const char *
rss_text (const Probe_Stats *stats, char *buffer, size_t size)
{
  if (stats->rss_mb)
    {
      snprintf (buffer, size, "%llu", stats->rss_mb);
      return buffer;
    }
  return "unknown";
}

static const char *
object_size_ratio_text (const Probe_Stats *stats,
                        const unsigned long long file_size, char *buffer,
                        size_t size)
{
  unsigned long long ratio100;

  if (!file_size)
    return "unknown";
  ratio100 = (stats->total_size * 100ULL) / file_size;
  snprintf (buffer, size, "%llu.%02llux", ratio100 / 100ULL,
            ratio100 % 100ULL);
  return buffer;
}

static const char *
precheck_name (const Probe_Stats *stats, const unsigned long long file_size)
{
  if (!stats->num_objects)
    return "none";
  if (stats->max_size >= 16UL * 1024UL * 1024UL)
    return "high:max-object>=16MiB";
  if (file_size && stats->total_size >= file_size * 8ULL)
    return "high:object-bytes>=8x-file";
  if (stats->num_objects >= 1000000UL)
    return "high:object-count>=1M";
  if (stats->max_size >= 1024UL * 1024UL)
    return "warn:max-object>=1MiB";
  if (file_size && stats->total_size >= file_size * 2ULL)
    return "warn:object-bytes>=2x-file";
  if (stats->num_objects >= 100000UL)
    return "warn:object-count>=100k";
  return "low";
}

static Dwg_Version_Type
probe_header_version (const char *path)
{
  char magic[12] = { 0 };
  FILE *fp;
  size_t read_size;

  if (!path || !strcmp (path, "-"))
    return R_INVALID;

  fp = fopen (path, "rb");
  if (!fp)
    return R_INVALID;
  read_size = fread (magic, 1, 11, fp);
  fclose (fp);
  if (read_size < 6)
    return R_INVALID;
  return dwg_version_hdr_type (magic);
}

static int
probe_file (const char *path, unsigned long long file_size)
{
  Probe_Stats stats = { 0 };
  Dwg_Stream_Callbacks callbacks = { 0 };
  char peak_buffer[32];
  char ratio_buffer[32];
  char status_buffer[32];
  int error;
  unsigned long long avg_size;
  const char *version;
  const char *status = "ok";
  Dwg_Version_Type header_version;

  header_version = probe_header_version (path);
  sample_rss (&stats);
  callbacks.object = probe_object;
  callbacks.flags = DWG_STREAM_F_NO_FULL_FALLBACK;
  error = dwg_stream_file (path, &callbacks, &stats);
  sample_rss (&stats);
  if (error >= DWG_ERR_CRITICAL)
    {
      printf ("| `%s` | error 0x%x | %llu |  |  |  |  |  |  |  |  |  |  | "
              "%s |  |  |\n",
              path, error, file_size, rss_text (&stats, peak_buffer,
                                                sizeof (peak_buffer)));
      return 1;
    }

  avg_size = stats.num_objects ? stats.total_size / stats.num_objects : 0;
  version = stats.version ? dwg_version_type (stats.version)
                          : header_version ? dwg_version_type (header_version)
                                           : "unknown";
  if (error)
    {
      if (error == DWG_ERR_NOTYETSUPPORTED)
        snprintf (status_buffer, sizeof (status_buffer), "unsupported 0x%x",
                  error);
      else
        snprintf (status_buffer, sizeof (status_buffer), "warn 0x%x", error);
      status = status_buffer;
    }
  printf ("| `%s` | %s | %llu | %s | %lu | %lu | %lu | %llu | %llu | %lu | "
          "%zu-%zu | %s | %s | %s | %s | %s |\n",
          path, status, file_size, version,
          (unsigned long)stats.num_objects,
          (unsigned long)stats.num_entities,
          (unsigned long)stats.num_non_entities, stats.total_size,
          avg_size, (unsigned long)stats.max_size, stats.min_address,
          stats.max_address, decode_mode_name (&stats),
          input_mode_name (&stats),
          rss_text (&stats, peak_buffer, sizeof (peak_buffer)),
          object_size_ratio_text (&stats, file_size, ratio_buffer,
                                  sizeof (ratio_buffer)),
          precheck_name (&stats, file_size));
  return 0;
}

static int
has_dwg_suffix (const char *name)
{
  size_t len = strlen (name);
  const char *suffix;

  if (len < 4)
    return 0;
  suffix = name + len - 4;
  return tolower ((unsigned char)suffix[0]) == '.'
         && tolower ((unsigned char)suffix[1]) == 'd'
         && tolower ((unsigned char)suffix[2]) == 'w'
         && tolower ((unsigned char)suffix[3]) == 'g';
}

#ifdef HAVE_DIRENT_H
static int
probe_dir (const char *path)
{
  DIR *dir = opendir (path);
  struct dirent *entry;
  int failures = 0;
  size_t path_len = strlen (path);

  if (!dir)
    return probe_file (path, 0);

  while ((entry = readdir (dir)) != NULL)
    {
      char *fullpath;
      size_t name_len;
      int needs_sep;
      struct_stat_t attrib;
      unsigned long long file_size = 0;

      if (!has_dwg_suffix (entry->d_name))
        continue;

      name_len = strlen (entry->d_name);
      needs_sep = path_len && path[path_len - 1] != '/'
                  && path[path_len - 1] != '\\';
      fullpath = (char *)malloc (path_len + (needs_sep ? 1 : 0) + name_len + 1);
      if (!fullpath)
        {
          failures = 1;
          continue;
        }

      memcpy (fullpath, path, path_len);
      if (needs_sep)
        fullpath[path_len] = '/';
      memcpy (fullpath + path_len + (needs_sep ? 1 : 0), entry->d_name,
              name_len + 1);
      if (!stat (fullpath, &attrib))
        file_size = (unsigned long long)attrib.st_size;
      failures |= probe_file (fullpath, file_size);
      free (fullpath);
    }

  closedir (dir);
  return failures;
}
#endif

static int
probe_path (const char *path)
{
  struct_stat_t attrib;

  if (stat (path, &attrib))
    {
      printf ("| `%s` | missing |  |  |  |  |  |  |  |  |  |  |  |  |  |  |\n",
              path);
      return 1;
    }

#ifdef HAVE_DIRENT_H
  if (S_ISDIR (attrib.st_mode))
    return probe_dir (path);
#endif

  return probe_file (path, (unsigned long long)attrib.st_size);
}

int
main (int argc, char *argv[])
{
  int failures = 0;
  int c;
#ifdef HAVE_GETOPT_LONG
  int option_index = 0;
  static struct option long_options[]
      = { { "help", 0, NULL, 0 }, { "version", 0, NULL, 0 },
          { NULL, 0, NULL, 0 } };
#endif

  while
#ifdef HAVE_GETOPT_LONG
      ((c = getopt_long (argc, argv, "hi", long_options, &option_index)) != -1)
#else
      ((c = getopt (argc, argv, "hi")) != -1)
#endif
    {
      switch (c)
        {
#ifdef HAVE_GETOPT_LONG
        case 0:
          if (!strcmp (long_options[option_index].name, "version"))
            return opt_version ();
          if (!strcmp (long_options[option_index].name, "help"))
            return help ();
          break;
#else
        case 'i':
          return opt_version ();
#endif
        case 'h':
          return help ();
        default:
          return usage ();
        }
    }

  if (optind >= argc)
    return usage ();

  printf ("| path | status | file_size | version | objects | entities | "
          "non_entities | total_object_size | avg_object_size | "
          "max_object_size | address_range | decode_mode | input_mode | "
          "rss_mb | object_size_ratio | precheck |\n");
  printf ("| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | "
          "---: | --- | --- | --- | ---: | ---: | --- |\n");

  for (int i = optind; i < argc; i++)
    failures |= probe_path (argv[i]);

  return failures;
}
