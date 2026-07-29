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

Status: red test, implementation pending.

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

Next repair:

1. Read the R2013+ `AcDb:AcDsPrototype_1b` section in the Stream path without
   calling `dwg_read_file`.
2. Associate each SAB record with its entity handle before that entity's
   decoded callback is emitted.
3. Re-run the strict fixture set and record the next mismatch, if any.
