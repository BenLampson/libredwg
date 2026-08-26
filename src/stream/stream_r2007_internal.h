/* Private R2007 page-map contracts shared by blocking and Stream readers. */

#ifndef LIBREDWG_STREAM_R2007_INTERNAL_H
#define LIBREDWG_STREAM_R2007_INTERNAL_H

#include "decode.h"

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
int read_file_header (Bit_Chain *dat, Dwg_R2007_Header *file_header);
int read_2007_section_header (Bit_Chain *dat, Bit_Chain *hdl_dat,
                              Dwg_Data *dwg, r2007_section *sections_map,
                              r2007_page *pages_map);
int read_2007_section_classes (Bit_Chain *dat, Dwg_Data *dwg,
                               r2007_section *sections_map,
                               r2007_page *pages_map);
int read_2007_section_filedeplist (Bit_Chain *dat, Dwg_Data *dwg,
                                   r2007_section *sections_map,
                                   r2007_page *pages_map);

#endif
