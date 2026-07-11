# DWG Stream reader source map

This directory separates Stream-reader responsibilities from the blocking DWG
decoder. File names use explicit DWG release ranges so a maintainer can locate
the relevant implementation without knowing internal family nicknames.

Current files:

- `stream_version_policy.c`: the single list of versions deliberately rejected
  by Stream APIs.
- `stream_callbacks.c`: decoded-object and decode-error callback handling.
- `stream_object_helpers.c`: object bounds, record-size, entity classification,
  and handle-offset checks shared by multiple format readers.
- `stream_reader.c`: header detection, exact-version refinement, and dispatch
  to a reader whose name states its DWG release range.
- `stream_read_r13_to_r2002.c`: R13, R14, R2000, R2000i, and R2002 object-map
  reading.
- `stream_read_r2004_to_r2006_and_r2010_to_r2022.c`: compressed object pages
  and handle maps shared by the AC1018 and R2010-and-later format ranges.

Planned reader files, migrated one format range at a time:

- `stream_read_r1_to_r11.c`: R1.1 through R11.
- `stream_read_r2007.c`: the R2007 handle stream and object routing.

Shared compression or bit-reading helpers must be named after the mechanism,
not called `legacy`, `modern`, or `common`. Public Stream API declarations stay
in `include/dwg.h`; declarations used only by these files stay private to this
directory.

Every migration is structural only. Object counts, callback order, errors, and
the no-blocking-fallback rule must remain unchanged and pass `stream_test` before
the next format range is moved.
