# Stream strict read-parity change list

This file records the active TDD work needed to make
`dwg_stream_file_ex` produce the same decoded object semantics as the
blocking `dwg_read_file` baseline.  Keep it current so another maintainer can
continue from the first failing strict fixture without repeating the audit.

## Comparison contract

- `dwg_read_file` is the authoritative baseline.
- Stream must use `DWG_STREAM_F_NO_FULL_FALLBACK`.
- Every baseline object must match exactly one decoded Stream callback.
- Objects are paired by stable DWG identity rather than callback order because
  the documented pre-R13 Stream order differs from `dwg->object[]` order.
- Container-only `Dwg_Object.index` is normalized after pairing.
- The complete canonical object representation emitted from the object specs
  is compared byte for byte.  This covers decoded scalar, string, binary,
  handle, vector, array, and nested-structure fields without comparing pointer
  addresses.
- Existing count, classification, reference, semantic-coverage, callback-abort,
  decode-error, and `full=0` assertions remain mandatory.

## 2026-07-30: strict object comparator

Status: green on the generated and repository fixture suites.

Changes:

- Added a single-object canonical JSON writer using the same object-spec path
  as the normal JSON `OBJECTS` section.
- Added exact baseline-object accounting and byte comparison to `stream_test`.
- Removed the R13 exception which previously allowed fewer decoded callbacks
  than blocking objects.
- Kept callback-order testing separate and normalized the temporary host index.

TDD evidence:

- Generated R13, R2000, R2004b, R2007, and R2022 fixtures pass the new exact
  comparison.
- Repository fixtures through `example_2010.dwg` pass.
- The first current failure is `test/test-data/example_2013.dwg`, object handle
  374 (`REGION`): blocking has `acis_empty=0` and populated SAB/ACIS data;
  Stream has `acis_empty=1`.

Repair:

- The R2004/R2010+ Stream reader now decompresses the optional R2013+
  `AcDb:AcDsPrototype_1b` section without materializing the full object graph.
- It follows the blocking decoder's pairing rule: entities marked
  `has_ds_data` are encountered in object-decode order and consume SAB records
  in section order.
- The matching SAB bytes, size, ACIS version, and `acis_empty` state are
  attached before the decoded-object callback.  Callback order is unchanged.

Green evidence:

- `test/test-data/example_2013.dwg` passes with reference checks enabled:
  all 481 blocking objects have one byte-identical canonical Stream object.
- The complete default `stream_test` run passes all generated variants and all
  repository fixtures, including the R2013 regression.
- The changed reader compiles with `-Werror
  -Wdeclaration-after-statement`.

Next work:

- No strict object mismatch is currently known in the exercised fixture set.
- Any newly discovered mismatch should be added here with its file, handle,
  object type, first differing canonical field, and the red/green command.

## 2026-07-30: isolated Stream allocation policy

Status: green on strict C object parity and the downstream AC1021 rendering
fixture.  This iteration changes only isolated Stream decoding; the blocking
reader keeps its original allocation policy.

Root cause:

- `dwg_stream_emit_decoded_object_ex` inherited the full-graph
  `REFS_PER_REALLOC` quantum and allocated 16,384 reference slots for each
  transient object.
- Every isolated object also allocated a heap handle map.
- The R2007 reader copied object records already contained in its active
  decompressed page.

Changes:

- Added `DWG_OPTS_STREAM_DECODE` and a 16-reference growth quantum used only by
  the isolated Stream host.
- Kept the blocking reader on the existing 16,384-reference quantum.
- Replaced the per-object heap handle map with a bounded stack map and copied
  only the `Dwg_Data` state required by object decoding and callbacks.
- Borrowed R2007 cached-page storage for records contained in one page while
  retaining the owned copying path for cross-page records.

Iteration artifacts:

- Branch: `codex/libredwg-c-stream`.
- Code commit: `24f809ab`.
- Release build:
  `cmake --build build-cad-yolo-release --target redwg`.
- Test build:
  `cmake --build build-codex-stream-tdd --target stream_test`.
- Default `stream_test.exe`: passed all generated and repository fixtures.
- `LIBREDWG_STREAM_TEST_REFS=1 stream_test.exe`: exit 0, ten repository
  fixtures passed, no full decode, and no decode errors.
- The Stream path continued to report `full=0`; it did not call
  `dwg_read_file`.
- The AC1021 product fixture retained 659,049 rendered lines, 3,698 texts,
  910 fills, 77 wipeouts, and the accepted exact color/geometry fingerprints.

Five-run interleaved product benchmark:

- Before: Stream native median 2,682.60 ms; private memory 66.82 MiB.
- After: Stream native median 2,455.61 ms; private memory 65.75 MiB.
- Blocking-reader median after the change: 405.24 ms; its allocation path was
  unchanged.

The Stream improvement is 8.46%, but Stream remains slower than the blocking
reader.  This result is not evidence that allocation tuning alone can close
the architectural gap; downstream spool and publication costs remain separate
work items.

## 2026-07-31: preserve raw PROXY_OBJECT handle encoding

Status: green without normalization on all customer DWGs smaller than 30 MiB.

Root cause:

- One R2007 customer drawing exposed four `PROXY_OBJECT` references whose raw
  encoding was `code=5, size=1, value=0`.
- Blocking decode retained `5.1.0` through its global reference pool, while an
  isolated Stream host rebuilt the reference as canonical `5.0.0`.
- `dwg.spec` read the complete `Dwg_Handle`, but then discarded its encoded
  `size` by calling `dwg_add_handleref` with only `code` and `value`.

Changes:

- Added an internal PROXY decoder helper which creates an independent global
  reference and copies the complete raw `Dwg_Handle`.
- Changed `PROXY_OBJECT.objids` decode to use the raw helper and report
  allocation failure.
- Added a regression test for the non-canonical encodings `5.1.0` and
  `5.3.1`; code, size, value, and absolute reference must all survive exactly.

Green evidence:

- The CMake `stream_test` target and its complete generated/repository run
  pass.
- Canonical Autotools `make check` passes all 255 unit tests; the program and
  example suites also pass.
- Recursive strict comparison of 14 customer DWGs (162.28 MiB and 2,897,546
  objects) passes 14/14 with `decode_errors=0` and `full=0`.
- The formerly failing R2007 drawing now has all 89,034 objects byte-identical
  to blocking decode without test-side normalization.
- A post-fix three-run warm-cache `dwgprobe -d` sweep passes all 14 files.  The
  sum of per-file medians is 6.852 seconds versus 7.150 seconds in the prior
  run, so no Stream-open performance regression was observed.
