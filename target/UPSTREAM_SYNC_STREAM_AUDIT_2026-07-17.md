# Upstream 0.14.8447 Stream impact audit

## Scope

This audit covers the 28 upstream `master` commits merged at `f2804663` into
the C Stream branch on 2026-07-17.  A successful Git merge is not sufficient
evidence that a fix reaches Stream: each fix must be classified by its runtime
call path.

## Shared fixes inherited by Stream

- R2004 buffered sections call `read_2004_compressed_section`, so upstream's
  64-bit section-size multiplication fix applies directly.  The large-object
  Stream path does not perform that multiplication; it selects incremental
  page decoding with a division guard and validates section-relative ranges.
- 3DSOLID block-size overflow checks, ACIS wire color changes, HATCH handle
  stream handling, TABLESTYLE fields, and other `dwg.spec`/`dwg2.spec` object
  decoder changes are shared.  Stream decoded-object callbacks invoke the same
  object decoders and therefore inherit these fixes.
- R11 HANDSEED and second-header handle fixes are shared through
  `auxheader.spec`, the pre-R13 decoders, and the existing Stream adapters.

## Stream-specific fix required

Upstream commit `4594de3c` added a `page->offset < dat->size` check to the
blocking R2007 whole-section reader.  Stream loads individual R2007 pages via
`read_data_section_page`, so the new check did not cover the Stream call path.

The branch now validates the source page offset in the shared single-page
reader before assigning `dat->byte` or subtracting it from `dat->size`.  It also
validates the R2007 section-map page offset before the corresponding subtraction
in both blocking and Stream entry points.  This prevents the same `size_t`
underflow and out-of-bounds file read in every path.

## Changes outside DWG Stream reading

The remaining upstream commits primarily affect DXF import/export, dynapi
string ownership, object stability metadata, generated `.pi` evidence, and
TABLESTYLE upconversion.  They are retained by the full upstream merge but do
not add a separate DWG Stream reader code path.

## Verification

- CMake `stream_test`: exit 0.
- Autotools strict build and `stream_test`: exit 0.
- `3F.00.dwg` R2004 Stream decoded 1,769,224 of 1,769,224 objects with zero
  decode errors and no blocking fallback; peak RSS remained 366 MiB.
- The default repository Stream regression includes the generated and real
  R2007 fixtures, exercising the updated shared single-page loader.
