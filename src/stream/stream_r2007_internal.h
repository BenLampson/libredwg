/* Private R2007 page-map contracts shared by blocking and Stream readers. */

#ifndef LIBREDWG_STREAM_R2007_INTERNAL_H
#define LIBREDWG_STREAM_R2007_INTERNAL_H

#include "decode.h"

typedef struct _r2007_page
{
  int64_t id;
  uint64_t size;
  uint64_t offset;
  struct _r2007_page *next;
} r2007_page;

typedef struct _r2007_section_page
{
  uint64_t offset;
  uint64_t size;
  int64_t id;
  uint64_t uncomp_size;
  uint64_t comp_size;
  uint64_t checksum;
  uint64_t crc;
} r2007_section_page;

typedef struct _r2007_section
{
  uint64_t data_size;
  uint64_t max_size;
  int64_t encrypted;
  uint64_t hashcode;
  int64_t name_length;
  int64_t unknown;
  int64_t encoded;
  int64_t num_pages;
  DWGCHAR *name;
  Dwg_Section_Type type;
  r2007_section_page **pages;
  struct _r2007_section *next;
} r2007_section;

r2007_section *get_section (r2007_section *sections_map,
                            Dwg_Section_Type section_type);
r2007_page *get_page (r2007_page *pages_map, int64_t id);
void pages_destroy (r2007_page *page);
void sections_destroy (r2007_section *section);
r2007_section *read_sections_map (Bit_Chain *dat, int64_t size_comp,
                                  int64_t size_uncomp, int64_t correction);
r2007_page *read_pages_map (Bit_Chain *dat, int64_t size_comp,
                            int64_t size_uncomp, int64_t correction);
int read_data_section_page (Bit_Chain *page_dat, Bit_Chain *dat,
                            r2007_page *pages_map,
                            r2007_section_page *section_page);
int read_data_section (Bit_Chain *section_dat, Bit_Chain *dat,
                       r2007_section *sections_map, r2007_page *pages_map,
                       Dwg_Section_Type section_type);
int read_file_header (Bit_Chain *dat, Dwg_R2007_Header *file_header);
int read_2007_section_classes (Bit_Chain *dat, Dwg_Data *dwg,
                               r2007_section *sections_map,
                               r2007_page *pages_map);

#endif
