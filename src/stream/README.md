# DWG Stream reader source map

This directory separates Stream-reader responsibilities from the blocking DWG
decoder. File names use explicit DWG release ranges so a maintainer can locate
the relevant implementation without knowing internal family nicknames.

Current files:

- `stream_version_policy.c`: the single list of versions deliberately rejected
  by Stream APIs.
- `stream_callbacks.c`: decoded-object and decode-error callback handling.
- `stream_reader.c`: header detection, exact-version refinement, and dispatch
  to a reader whose name states its DWG release range.

Planned reader files, migrated one format range at a time:

- `stream_read_r1_to_r11.c`: R1.1 through R11.
- `stream_read_r13_to_r2002.c`: R13, R14, R2000, R2000i, and R2002.
- `stream_read_r2004_to_r2006.c`: the AC1018 format family.
- `stream_read_r2007.c`: the R2007 handle stream and object routing.
- `stream_read_r2010_to_r2018.c`: R2010, R2013, and R2018.

Shared compression or bit-reading helpers must be named after the mechanism,
not called `legacy`, `modern`, or `common`. Public Stream API declarations stay
in `include/dwg.h`; declarations used only by these files stay private to this
directory.

Every migration is structural only. Object counts, callback order, errors, and
the no-blocking-fallback rule must remain unchanged and pass `stream_test` before
the next format range is moved.
