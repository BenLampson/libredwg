# LibreDWG C stream TDD target

This document defines the active C-side target for the DWG stream work. The
development target is libredwg C code, and the verification path is C blocking
read versus C stream read.

For the concise current answer, real-version coverage, remaining exact-version
gaps, and maintenance rule, see `target/STREAM_BLOCKING_PARITY.md`. Keep that
status file, this detailed target, and the top-level `README` synchronized.

Scope correction: the active target is not ProtocolVNext and not any downstream
consumer. Stream TDD is developed and judged on the C side only. Every blocking
versus stream comparison in this phase means `dwg_read_file` in libredwg C
against `dwg_stream_file_ex` in libredwg C.

All stream TDD, parity checks, regression checks, smoke tests, and acceptance
reports for the current iteration use the C implementation on both sides of the
comparison. The blocking side is `dwg_read_file`; the stream side is
`dwg_stream_file_ex` and its C callbacks. Do not substitute a downstream
consumer, C# bridge, frontend path, or ProtocolVNext result for this C-to-C
comparison.

ProtocolVNext is not part of this target. Do not inspect, edit, build, or use
ProtocolVNext for stream TDD or validation unless the user explicitly changes
the scope.

Current version priority is DWG 2000 and later. Complete real-file strict parity
for the remaining R2000, R2004, R2007, R2010, R2013, R2018, and R2022 exact
variants before actively searching for additional pre-2000 historical evidence.
Older gaps remain recorded and must not be forgotten, but they are the second
queue rather than the current development focus.

Current iteration rule: all TDD and verification work stays in this repository.
Compare the C blocking reader against the C stream reader only:
`dwg_read_file` as the baseline and the C stream callbacks as the stream
candidate. `dwg_stream_file` remains the old object-only API, while
`dwg_stream_file_ex` carries decoded-object callbacks for strict stream parity.
This applies to stream TDD, regression checks, parity checks, large file smoke
tests, and acceptance reporting.

## 1. Hard objective

Build a real stream-oriented DWG read/decode path in libredwg.

The stream path must:

- Decode and emit enough DWG object, reference, and semantic content to match
  the C full-read baseline.
- Avoid calling `dwg_read_file` in the stream path.
- Preserve the old full-read path and old APIs.
- Match the `dwg_read_file` result for the test DWG before it is considered
  correct.
- Make memory reduction possible by not materializing the entire DWG object graph in native code.

Correctness comes first. Memory optimization is not accepted if the stream result diverges from the existing full-load result.

## 2. Non-goals

Do not solve this by:

- Modifying downstream repositories while this C-side target is active.
- Adding downstream layer-name heuristics before the C contract is fixed.
- Falling back to `dwg_read_file` inside the stream implementation.
- Overwriting the old DLL or old full-read shim.
- Using ProtocolVNext or frontend behavior as the primary validation path.
- Treating one specific layer as a special case.

If a stream result is wrong, the preferred fix is to expose the missing DWG semantics from libredwg.

## 3. Primary test file

Use this file as the first TDD fixture:

```text
E:\cadTestFolder\test1.dwg
```

Optional large-file validation set:

```text
C:\Users\benla\Desktop\CAD测试文件\最新5个大的
E:\项目额外大文件\CAD项目\CAD算量材料(1)\3F.00.dwg
```

The optional large files are useful for performance and memory checks, but they must not replace the strict parity test on `test1.dwg`.

## 4. Validation boundary

The current development and verification boundary is the C/libredwg side only.

The authoritative comparison is:

```text
dwg_read_file blocking C path  <->  dwg_stream_file_ex C stream path
```

Use `test/unit-testing/stream_test.c` as the TDD harness. The stream test should
compare C-observable data from the blocking baseline against C-observable data
from stream callbacks. Do not validate current stream work by running
ProtocolVNext, a C# shim, or a frontend endpoint unless explicitly requested
after the C side is stable.

## 5. TDD verification commands

After each C/libredwg stream change, build and run the C stream test.

```powershell
cd D:\Codes\libredwg

$env:PATH = "D:\Codes\libredwg\build-codex-stream-tdd;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"

cmake --build .\build-codex-stream-tdd --target stream_test

# With no fixture environment variables, `stream_test.exe` runs the default
# repository R13/R14/R2000/R2004/R2007/R2010/R2013/R2018 stream parity
# fixtures, plus TS1.dwg for OLE/OLE2 entity classification coverage. It
# distinguishes R2.0 beta from R2.0 through the shared AC1.50 header and checks
# generated blocking-versus-stream parity for both. It cross-checks the real
# R1.4, R2.10, R2.6, R9, R10, and R11 fixtures against the blocking reader and
# checks generated fixtures for old versions without repository DWG files.
# Modern exact-version checks include generated R13b1/R13b2/R13c3/R2000b,
# R2004a/R2004b/R2004c, R2007a/R2007b/R2007,
# R2010b/R2013b/R2018b/R2022b fixtures and synthetic AC1016/AC1017/R2004c
# header variants. The generated R2007 release fixture spans multiple object
# data pages.
# An unknown AC9999 header must return DWG_ERR_INVALIDDWG with no callbacks.
# Old-version entity sections are decoded one object at a time; the tests compare
# reference snapshots and require at most three resident host entities.
# Stream APIs do not fall back to full load.
Remove-Item Env:LIBREDWG_STREAM_TEST_DWG -ErrorAction SilentlyContinue
Remove-Item Env:LIBREDWG_STREAM_TEST_REFS -ErrorAction SilentlyContinue
Remove-Item Env:LIBREDWG_STREAM_TEST_LARGE_DWG -ErrorAction SilentlyContinue
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe

# Optional broader repository sweep across root examples and
# test/test-data/{2000,2004,2007}. `LIBREDWG_STREAM_TEST_REFS=1` can be layered
# on to validate reference snapshots for the same sweep.
$env:LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP = "1"
Remove-Item Env:LIBREDWG_STREAM_TEST_REFS -ErrorAction SilentlyContinue
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe

$env:LIBREDWG_STREAM_TEST_REFS = "1"
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe
Remove-Item Env:LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP -ErrorAction SilentlyContinue

$env:LIBREDWG_STREAM_TEST_DWG = "E:\cadTestFolder\test1.dwg"
Remove-Item Env:LIBREDWG_STREAM_TEST_REFS -ErrorAction SilentlyContinue
Remove-Item Env:LIBREDWG_STREAM_TEST_LARGE_DWG -ErrorAction SilentlyContinue
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe

$env:LIBREDWG_STREAM_TEST_REFS = "1"
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe
```

Use the large fixture as a smoke/performance guard, not as a replacement for
strict C parity on `test1.dwg`:

```powershell
Remove-Item Env:LIBREDWG_STREAM_TEST_DWG -ErrorAction SilentlyContinue
Remove-Item Env:LIBREDWG_STREAM_TEST_REFS -ErrorAction SilentlyContinue
$env:LIBREDWG_STREAM_TEST_LARGE_DWG = "E:\项目额外大文件\CAD项目\CAD算量材料(1)\3F.00.dwg"
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe
```

Optional decoded large-file smoke:

```powershell
$env:LIBREDWG_STREAM_TEST_LARGE_DECODED = "1"
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe
```

## 6. Required pass criteria

The iteration is accepted only when C-side strict parity passes.

Smoke requirements:

- Stream metadata is present.
- Stream path reports `dwg_read_file` was not used (`full=0`).
- Stream object count is positive.
- Decoded stream object count matches the blocking baseline.
- Entity and non-entity counts match the blocking baseline.
- Handle mix and decoded handle mix match the blocking baseline.
- No native crash.
- Callback abort errors are propagated exactly, even when decoding accumulated
  an earlier noncritical warning.
- Any known legal version without a stream route returns
  `DWG_ERR_NOTYETSUPPORTED` without invoking any stream callbacks. There are no
  such versions in the current `Dwg_Version_Type` enum.
- Unknown or invalid version headers return `DWG_ERR_INVALIDDWG` without
  invoking any stream callbacks.

Strict C parity requirements against `dwg_read_file`:

- Object count must match.
- Entity and non-entity counts must match.
- Object handles and handle mix must match.
- Owner/layer reference snapshots must match when
  `LIBREDWG_STREAM_TEST_REFS=1`.
- `INSERT` / `MINSERT` block references and attribute counters must match.
- `DIMENSION_COMMON.block`, dimension style, and common dimension data must
  match.
- `BLOCK_HEADER` name/base/owned-entity snapshots must match.
- POLYLINE vertex ownership snapshots must match.
- Pre-R13 direct-walker tests must keep resident host entities bounded at three
  or fewer while matching the blocking result.
- Semantic hashes for common entities, color/ltype/material/plotstyle, text,
  insert, line, dimension, vertex, LWPOLYLINE, HATCH, and WIPEOUT must match.
- Semantic coverage counters for block headers, anonymous dimension blocks,
  INSERT/MINSERT, dimension block references, POLYLINE_3D vertices, HATCH,
  WIPEOUT, TEXT/MTEXT, ownerhandle/ownerless entities, model/paper/block-owned
  entities, and entmode buckets must match between the blocking and decoded
  stream paths.

The `stream_test.exe` output is the authoritative TDD result for the current
C-side target.

The harness also verifies callback combinations that downstream C consumers
depend on: legacy two-field `dwg_stream_file` callbacks, object-only streaming,
decoded-only `dwg_stream_file_ex` streaming, callback abort propagation in the
lightweight stream path, invalid public API arguments, and invalid-version
rejection without object, decoded-object, or decode-error callbacks. It also
verifies that both old `dwg_stream_file` callbacks and `dwg_stream_file_ex`
callbacks can return `DWG_ERR_NOTYETSUPPORTED` as a callback abort once a
stream path has been selected, without being mistaken for a missing version
route. The old `DWG_STREAM_F_NO_FULL_FALLBACK` flag is retained for source
compatibility, but stream APIs no longer perform full fallback with or without
that flag.

## 7. Current C baseline

Latest local `test1.dwg` C parity:

```text
objects=89034
entities=77824
non_entities=11210
lightweight=89034
decoded=89034
decoded_entities=77824
decoded_non_entities=11210
r2007=89034
full=0
file_map=89034
```

Both the normal run and the same run with
`LIBREDWG_STREAM_TEST_REFS=1` pass with exit code zero. The reference-enabled
run compares the per-handle reference snapshots emitted by the decoded Stream
callbacks against the blocking `dwg_read_file` baseline. For every decoded
`BLOCK_HEADER`, this includes `num_inserts` and an ordered hash of the complete
`inserts[]` handle vector, not only its owned entity chain.

Latest local `test1.dwg` memory comparison:

```text
blocking dwgread externally sampled peak working set: 138.06 MB
dwgprobe metadata Stream process peak RSS:             10 MB
dwgprobe --decoded Stream process peak RSS:            15 MB
```

The Stream measurements use the file map and do not retain the complete object
graph. `dwgprobe --decoded` requires every object callback to have a matching
decoded callback, requires zero decode errors, and exits nonzero if either
condition fails. On Windows its `rss_mb` value uses `PeakWorkingSetSize`; on
POSIX it uses `ru_maxrss`.

Latest local `test1.dwg` semantic coverage:

```text
block_headers=2434 block_headers_owned=2433 block_headers_inserted=248
block_chains=2434
anonymous_dim_blocks=2067 inserts=8753 minserts=0 dimension_blocks=2067
poly3d_vertices=5 hatches=804 wipeouts=16 texts=663 mtexts=2817
ownerhandle_entities=57479 ownerless_entities=20345 model=20343 paper=2
block_owned=57479 entmode0=57479 entmode1=2 entmode2=20343 entmode3=0
```

Repository fixture C parity is also covered by default:

```text
example_r13.dwg:  objects=5785 decoded=5785 r13=5785 full=0
example_r14.dwg:  objects=832 decoded=832 r13=832 full=0
example_2000.dwg: objects=750 decoded=750 r13=750 full=0
example_2004.dwg: objects=735 decoded=735 r2004=735 full=0
example_2007.dwg: objects=540 decoded=540 r2007=540 full=0
test/test-data/2000/TS1.dwg: objects=336 decoded=336 r13=336 full=0
```

Extended local repository fixture sweep:

```text
R13/R14/R2000/R2004/R2007 root examples plus test/test-data/{2000,2004,2007}
LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP=1: files=67 refs=0 passed
LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP=1 LIBREDWG_STREAM_TEST_REFS=1: files=67 refs=1 passed
The refs=1 sweep also compares the semantic coverage counters listed above.
The repository sweep now hard-fails if its aggregate C-side coverage loses
block headers, block headers with owned entities, block entity chains,
anonymous dimension blocks, INSERT, dimension block references, POLYLINE_3D
vertices, HATCH, WIPEOUT, TEXT, MTEXT, ownerhandle/ownerless entities,
model/paper/block-owned entities, or entmode 0/1/2 buckets.
The default `stream_test` also creates a temporary R2000 MINSERT DWG through
the libredwg C add/write API and immediately verifies it through
`dwg_read_file` versus `dwg_stream_file_ex`, with `full=0`, `minserts=1`, and
`entmode3=1`.
Block-owned entities are classified by owner semantics, not by treating
`entmode=3` as the only block-owned case: an entity whose ownerhandle points to
a non-model/non-paper owner is counted as block-owned. Current local stream
fixtures cover block-owned entities through that C contract, and the generated
R2000 fixture covers the standalone `entmode=3` bucket.
Latest repository sweep semantic coverage:

block_headers=293 block_headers_owned=75 block_chains=293
anonymous_dim_blocks=58 inserts=57 minserts=0 dimension_blocks=58
poly3d_vertices=9 hatches=8 wipeouts=10 texts=14 mtexts=107
ownerhandle_entities=6303 ownerless_entities=850 model=708 paper=142
block_owned=6303 entmode0=6303 entmode1=142 entmode2=708 entmode3=0
```

Generated MINSERT C fixture parity:

```text
objects=38 decoded=38 r13=38 full=0 file_map=38
minserts=1 block_owned=3 entmode3=1
```

Modern exact-version fixture parity:

```text
Generated C-writer MINSERT fixtures pass exact-version blocking-versus-stream
parity for R_13b1, R_13b2, R_13c3, R_2000b, R_2000, R_2004a, R_2004b,
R_2004c, R_2007a, R_2007b, R_2007, R_2010b, R_2013b, R_2018b, and R_2022b.

The R2007 C writer now emits an R2007 container instead of silently upgrading
the target to R2010. Its generated fixtures use the exact AC701a/AC1021 header,
the 0x400 file header, interleaved RS(255,239) system pages, duplicate page and
section maps, direct uncompressed data pages, and a duplicate trailing file
header. R2007a and R2007b each pass 37-object strict parity. The generated
R2007 release fixture adds 2,000 LINE entities, spans multiple object data
pages, and passes 2,037-object strict parity with `full=0`.

This is C-writer/C-reader evidence for the stream target. CRC64/check-data and
external AutoCAD/ODA writer interoperability are not claimed by these tests;
real independently sourced R2007 alpha/beta files are still required for
historical evidence.

Synthetic header fixtures derived from real family payloads pass for R_2000i,
R_2002, and R_2004c. The R_2000i and R_2002 fixtures replace the six-byte
R2000 magic with AC1016 or AC1017; the R_2004c fixture also sets the internal
dwg_version byte to 0x18. Each blocking read reports the exact expected enum,
both stream flag settings match the blocking object and decoded-object counts,
all callbacks report the same source version, and full=0.

These synthetic files prove routing, header classification, and family-payload
parity. For R2004c they do not replace an independently sourced historical
file. For R2000i/R2002 they guard accepted synthetic identifiers while original
product media independently proves that real files use the shared AC1015
format.
```

Default version guards:

```text
`AC1.50` is shared by `R_2_0b` and `R_2_0`. The magic-only mapper initially
selects `R_2_0`; after the pre-R13 header is parsed, `numheader_vars <= 74`
refines it to `R_2_0b`, while 83 remains `R_2_0`. A generated R2.0 beta LINE
fixture is accepted by `dwg_read_file` with exact version `R_2_0b`, then passes
pure stream parity with both stream flag settings and `full=0`. The blocking
pre-R13 decoder uses the exact R2.0 beta minimum size `0x1bc`, because its
74-variable header ends at `0x1b9`; other pre-R13 versions retain the existing
`0x1f0` minimum.

An unknown `AC9999` header must return `DWG_ERR_INVALIDDWG` through both stream
APIs and both stream flag settings without invoking object, decoded-object, or
decode-error callbacks.

A generated R11 basic-entity fixture is readable by `dwg_read_file`, then reads
through the pure stream pre-R13 entity walker as lightweight entities with
decoded-object callback coverage and `full=0`. The covered R11 main-entity
section types are `DWG_TYPE_LINE_r11`, `DWG_TYPE_POINT_r11`, `DWG_TYPE_CIRCLE_r11`,
`DWG_TYPE_TEXT_r11`, `DWG_TYPE_ARC_r11`, `DWG_TYPE_TRACE_r11`,
`DWG_TYPE_SOLID_r11`, `DWG_TYPE_3DFACE_r11`, `DWG_TYPE_SHAPE_r11`,
`DWG_TYPE_INSERT_r11`, `DWG_TYPE_ATTDEF_r11`, `DWG_TYPE_ATTRIB_r11`,
and `DWG_TYPE_VIEWPORT_r11`. It also covers `DWG_TYPE_DIMENSION_r11` with decoded
fixedtype coverage for linear, aligned, two-line angular, three-point angular,
diameter, ordinate, and radius dimensions. `DWG_TYPE_POLYLINE_r11` is
covered through generated 2D polyline, 3D polyline, polygon mesh, and
polyface fixtures. The generated main-entity-section polyline fixtures also
validate `DWG_TYPE_VERTEX_r11` across 2D, 3D, mesh, polyface vertex, and
polyface face decoded fixedtypes, and `DWG_TYPE_SEQEND_r11`. The default
stream test also validates a generated R11 fixture whose main-entity-section
type byte is changed from `DWG_TYPE_LINE_r11` to `DWG_TYPE_JUMP_r11`; the
blocking reader must recognize it as `DWG_TYPE_JUMP`, and stream mode must emit
it with `DWG_STREAM_DECODE_PRER13_ENTITY` and `full=0`. Real R1.4 parity now
covers the original `test/test-data/r1.4/entities.dwg` plus 24 independently
sourced BSD-2-Clause AC1.40 entity files. The 25-file set covers ARC, BLOCK,
CIRCLE, LINE, LOAD, POINT, REPEAT/ENDREP, SHAPE, SOLID, TEXT, TRACE, and
INSERT data with no full fallback. Real R2.10 parity now covers the original
`entities.dwg` and `block.dwg` plus 15 unique independently sourced
BSD-2-Clause AC2.10 entity files. The 17-file set covers ARC, ATTDEF, ATTRIB,
CIRCLE, LINE, POINT, POLYLINE/VERTEX/SEQEND, REPEAT/ENDREP, SHAPE, SOLID,
TEXT, TRACE, block sections, and pre-R13 table entries.
The complete real pre-R11/R11 fixture set also compares per-handle reference
snapshots against the blocking reader with both stream flag settings. This
includes entity owner/layer references and complete `BLOCK_HEADER` block,
end-block, and owned-entity chains. Pre-R2 and pre-R11 entity sections are now
walked and released one object at a time; `stream_test` fails if more than three
host entities are resident during these callbacks. Table objects remain only as
the retained metadata needed to resolve those old-format relationships.
The default test also cross-checks ten real R2.6 fixtures, the real R9
fixture, 19 real R10 fixtures, all three original R11 DWG files, and 20
independently sourced BSD-2-Clause AC1009 entity fixtures. The 64 newly
imported AC1.40, AC2.10, AC1003, and AC1006 files all run through both Stream
flag settings with strict object/type/table counts and per-handle references.
For versions without repository DWG files it writes and then
blocking-reads minimal C-generated fixtures for `R_2_0b`, `R_2_0`, `R_2_21`,
`R_2_22`, `R_2_4`, `R_2_5`, `R_9c1`, `R_11b1`, and `R_11b2` before running
stream parity with both zero flags and `DWG_STREAM_F_NO_FULL_FALLBACK`. The R11
beta writer uses the pre-R11 control-table record sizes; both generated files
are accepted by the blocking reader with their exact beta version. The real
R10 coverage includes UCS, VPORT, and APPID table sections. Independently,
external 1986 AutoCAD 2.53J training media supplies real `R_2_4` and `R_2_5`
strict parity; the generated fixtures remain committed regression coverage.
A generated R11 `DWG_TYPE_INSERT_r11` fixture also validates
MINSERT-specific option fields: `OPTS_R11_INSERT_HAS_NUM_COLS`,
`OPTS_R11_INSERT_HAS_NUM_ROWS`, `OPTS_R11_INSERT_HAS_COL_SPACING`, and
`OPTS_R11_INSERT_HAS_ROW_SPACING`. The blocking reader first confirms the
legacy INSERT rows/columns/spacing values, then stream mode must decode the
same field values with `DWG_STREAM_DECODE_PRER13_ENTITY` and `full=0`.
The block entity section is currently validated
for a generated `DWG_TYPE_BLOCK_r11` / `DWG_TYPE_ENDBLK_r11` pair with
block-owned `DWG_TYPE_LINE_r11`, `DWG_TYPE_POINT_r11`,
`DWG_TYPE_CIRCLE_r11`, `DWG_TYPE_TEXT_r11`, `DWG_TYPE_ARC_r11`,
`DWG_TYPE_TRACE_r11`, `DWG_TYPE_SOLID_r11`, `DWG_TYPE_3DFACE_r11`, and
`DWG_TYPE_SHAPE_r11`, plus block-owned ordinary and attributed
`DWG_TYPE_INSERT_r11` coverage with block-owned `DWG_TYPE_ATTDEF_r11`,
`DWG_TYPE_ATTRIB_r11`, and `DWG_TYPE_SEQEND_r11`. It also validates
block-owned `DWG_TYPE_POLYLINE_r11` across the same four decoded polyline
fixedtypes. The block-owned polyline fixture also validates
`DWG_TYPE_VERTEX_r11` across 2D, 3D, mesh, polyface vertex, and polyface face
decoded fixedtypes, and `DWG_TYPE_SEQEND_r11`. The same generated R11 fixture
now compares blocking versus stream table-entry coverage by count and fixedtype
mask. Current generated table-entry coverage includes `DWG_TYPE_BLOCK_HEADER`,
`DWG_TYPE_LAYER`, `DWG_TYPE_STYLE`, `DWG_TYPE_LTYPE`, `DWG_TYPE_VIEW`,
`DWG_TYPE_UCS`, `DWG_TYPE_VPORT`, `DWG_TYPE_APPID`, `DWG_TYPE_DIMSTYLE`,
and `DWG_TYPE_VX_TABLE_RECORD`, including the default entries created by
`dwg_add_Document` and the entries decoded from R11 table sections.
Real R11 coverage now includes `ACEB10.dwg`, `entities-2d.dwg`, and
`entities-3d.dwg`. `ACEB10.dwg` validates 1815 entities and 67 table entries.
It also includes 20 entity fixtures from the independently maintained,
BSD-2-Clause licensed `michal-josef-spacek/CAD-Format-DWG-AC1009` repository at
commit `186656e511fa1d1145ebdcfbe89fc002e018759e`. Those files cover 3DFACE, ARC,
CIRCLE, aligned DIMENSION, INSERT, JUMP, LINE, POINT, POLYLINE, SHAPE, and TEXT,
including several EED variants. Every file passes both Stream flag settings,
strict object/type/table counts, and per-handle blocking reference snapshots.
The direct R11 walker decodes each entity in a reusable `dwg->object[]` slot
with an isolated handle map, then releases it after the callback; this is
required by legacy EED decoding without retaining the complete entity graph.
This is still not proof for every possible R11/R12 file. An ODA 27.1
`ACAD12` conversion now supplies a separately identified R12 fixture generated
by an implementation independent of LibreDWG. It is not represented as a
historical R12 file. Any future known version without a stream route must fail
clearly with `DWG_ERR_NOTYETSUPPORTED` instead of falling back.
```

Current stream hardening:

```text
R13/R2000 and R2007 handle-page stream readers validate that the handle page
has room for the page-size field and trailing CRC before reading page contents.
R13/R2000, R2004, and R2007 stream handle readers also validate signed object
offset deltas before applying them to the cumulative object address, so
malformed handle maps cannot wrap the stream object address through unsigned
arithmetic.
Handle value accumulation is also protected against unsigned overflow. The
reader deliberately preserves the existing tolerant behavior for suspicious but
historically accepted `handleoff` values; treating those as hard skips broke
valid C parity on `example_2004.dwg`.
`dwg_stream_file_ex` distinguishes version routing errors from callback errors
raised inside an already-selected stream decoder. Every current legal enum
version has a stream route. Unknown headers return `DWG_ERR_INVALIDDWG`; a
future known version without a route would return `DWG_ERR_NOTYETSUPPORTED`.
Callback return values are propagated exactly and are not swallowed,
reinterpreted as fallback requests, or combined with earlier noncritical decode
warnings. The generated R2004b regression fixture corrupts only the Section Page
Map checksum, verifies blocking and Stream `DWG_ERR_WRONGCRC`, and then verifies
exact object, not-supported-bit, and decoded-object callback abort values.
Reference-enabled parity snapshots include each `BLOCK_HEADER`'s owned entity
chain, `num_inserts`, and ordered `inserts[]` handle mix. The repository semantic
coverage gate requires at least one block header with reverse INSERT references;
`test1.dwg` covers 248 such block headers and 8,753 INSERT entities. This makes
block reference counting and later block replay verifiable without layer-name
or traversal-order guesses.
POLYLINE reference snapshots likewise compare `num_owned`, `first_vertex`,
`last_vertex`, the ordered hash of every `vertex[]` handle, and `seqend` for 2D,
3D, mesh, and polyface polylines. The semantic gate requires real
POLYLINE_3D-with-vertices coverage; `test1.dwg` currently supplies five. A
Stream implementation with reordered, dropped, or guessed pending vertices
therefore fails C parity.
The blocking and stream R13/R2000 dispatch ranges both extend through `R_2002`,
so unique `AC1016` and `AC1017` headers no longer fall out after `R_2000`.
After the common modern header is parsed, both paths refine shared magic through
`dwg_version_hdr_type2()`. This distinguishes beta/release source versions such
as `R_2004c`/`R_2004`, `R_2007b`/`R_2007`, `R_2010b`/`R_2010`,
`R_2013b`/`R_2013`, and `R_2018b`/`R_2018` when the internal `dwg_version`
supports that distinction. `Dwg_Stream_Object_Info.version` reports this
refined source version, and the test harness rejects mixed versions across
callbacks.
Decoded-object streaming uses an isolated temporary Dwg_Data wrapper with its
own single-object pool, object_ref vector, and object_map for each decoded
callback. The host Dwg_Data object pool, object_ref, HANDSEED, dirty_refs, and
object_map are not borrowed or mutated by decoded callback emission.
`stream_test` directly covers this host-state isolation and verifies that the
host object_ref, HANDSEED, dirty_refs, and object_map state is preserved after a
decode-error path.
`dwgprobe` reports this old-format incremental route as `prer13-entity` rather
than the previous misleading `none` decode mode.
```

Latest local large-file smoke:

```text
objects=1769224
entities=1081856
non_entities=687368
r2004=1769224
full=0
file_map=1769224
```

Latest local large-file decoded smoke:

```text
objects=1769224
decoded=1769224
decoded_entities=1081856
decoded_non_entities=687368
decode_errors=0
r2004=1769224
full=0
file_map=1769224
```

Latest local large-file `dwgprobe` Stream memory samples:

```text
metadata: rss_mb=210 decode_mode=r2004-object-map input_mode=file-map
decoded:  rss_mb=365 decoded=1769224 decode_errors=0 input_mode=file-map
precheck: high:max-object>=16MiB
```

The large file reports the noncritical `DWG_ERR_VALUEOUTOFBOUNDS` bit (`0x40`)
on both probes, while still emitting and decoding all 1,769,224 object records;
both probe commands return zero. This warning is preserved rather than hidden.

These values are C-side regression anchors, not downstream/export acceptance
criteria.

Latest local CMake unit-test sweep:

```text
ctest --test-dir .\build-codex-stream-tdd --output-on-failure
8/8 passed
```

Earlier local build and syntax checks for this stream target:

```text
cmake --build .\build-codex-stream-tdd
passed

gcc from build-codex-stream-tdd/compile_commands.json with:
-Werror=declaration-after-statement -fsyntax-only
checked src/decode.c, src/decode_r2007.c, src/dwg.c,
programs/dwgprobe.c, and test/unit-testing/stream_test.c
passed
```

Latest local Autotools validation for this stream target:

```text
MSYS2 packages installed for canonical Autotools validation:
autoconf automake libtool make pkgconf perl git texinfo

./autogen.sh
passed

../configure CC=gcc --disable-shared --enable-static
passed in .build-autotools-static-codex

make -j2 check
passed

Autotools test summary:
programs: 3/3 passed
examples: 2/2 passed
test/unit-testing: 255/255 passed
doc: makeinfo passed after installing texinfo
```

This run includes the incremental pre-R2/pre-R11 entity walker and its strict
blocking-reference snapshot checks.

The shared Windows/MSYS2 route also passes when Bash is started as a UCRT64
shell rather than as the default MSYS shell:

```powershell
$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"
```

With `/ucrt64/bin` before `/usr/bin`, a fresh
`.build-shared-ucrt64-codex` configuration reports both build and host as
`x86_64-w64-mingw32`. The default shared/static configuration then builds
`src/.libs/libredwg-0.dll`, and this command executes the shared test suite
against that build-local DLL:

```text
PATH=$PWD/src/.libs:/ucrt64/bin:/usr/bin:$PATH make -j4 check

programs: 3/3 passed
examples: 2/2 passed
test/unit-testing: 253/253 passed
doc: makeinfo passed
```

The MinGW shared test set intentionally omits `decode_test.exe` and
`encode_test.exe`; `stream_test.exe` is included in the 253 tests and passed.
It also passed as an explicit `TESTS=stream_test` run. A direct execution of
the shared `examples/load_dwg.exe` against a repository DWG returned zero.

The earlier duplicate `bit_utf8_to_TU` link failure was an environment
misclassification, not a C source failure. Starting `/usr/bin/bash` without
`MSYSTEM=UCRT64` made `configure` identify the host as Cygwin even while using
the UCRT64 MinGW compiler. That left `HAVE_MINGW` false and linked `bits.lo`
beside the DLL import library. The correct UCRT64 environment makes
`HAVE_MINGW` true and removes that duplicate input. Both audit build
directories match the existing `/.build-*/` ignore rule.

Latest R2010/R2013/R2018 stream validation:

```text
PATH=/ucrt64/bin:/usr/bin:$PATH \
make -C .build-autotools-static-codex/test/unit-testing check TESTS=stream_test

PASS: stream_test
1 test passed

Default strict parity includes example_2010.dwg, example_2013.dwg,
example_2018.dwg, sample_2018.dwg, and a generated R2022b MINSERT fixture.
Each passed with `full=0`.
```

## 8. Current gap to complete stream coverage

Complete stream coverage means this:

- Every DWG version family that the C blocking reader accepts has a pure stream
  path.
- The pure stream path does not call `dwg_read_file`.
- Supported stream versions succeed with or without
  `DWG_STREAM_F_NO_FULL_FALLBACK`; the flag is compatibility-only.
- Object metadata, decoded-object callbacks, references, ownership, and the
  semantic counters listed above match the blocking C baseline.
- Any future known version without a stream route is explicitly reported as a
  gap and must not be hidden behind full fallback. There is no current legal
  enum version in that state.
- Unknown or invalid version headers fail as `DWG_ERR_INVALIDDWG` without
  callbacks or fallback.

Exact current implementation status by libredwg `Dwg_Version_Type` enum:

| Status | Exact versions | Meaning |
| --- | --- | --- |
| Pure stream route; external real fixture evidence | `R_1_1`, `R_1_2` | A 1983 AutoCAD-86 archive supplies one `MC0.0`/R1.1 and 17 `AC1.2`/R1.2 files. All pass strict blocking-versus-Stream reference parity with `full=0` and no decode errors: 185 R1.1 objects and 1,522 R1.2 objects. The item has no license URL, so these are external historical evidence and generated fixtures remain the committed regression coverage. |
| Partial pure stream support | `R_1_3` | The pre-R2 stream reader passes exact-version blocking-versus-stream parity on a minimal LINE fixture produced by the LibreDWG C writer. No real historical `AC1.3` DWG is known, so this remains generated coverage. |
| Pure stream route; real fixture evidence | `R_1_4` | Strict C parity covers 25 real files: the original fixture plus 24 independent BSD-2-Clause AC1.40 entity files. They cover ARC, BLOCK, CIRCLE, LINE, LOAD, POINT, REPEAT/ENDREP, SHAPE, SOLID, TEXT, TRACE, and INSERT data with both Stream flag settings, bounded host entities, and `full=0`. No known committed or audited R1.4 fixture currently fails Stream. |
| Partial pure stream support | `R_2_0b`, `R_2_0`, `R_2_21`, `R_2_22`, `R_9c1` | The shared pre-R11 reader passes blocking-vs-stream parity on minimal fixtures produced by the LibreDWG C writer and accepted by `dwg_read_file`. For shared magic `AC1.50`, 74 header variables refine the exact version to `R_2_0b`, while 83 remain `R_2_0`; both pass with `full=0`. These versions have no real historical DWG evidence, so this remains generated single-entity/table coverage. |
| Pure stream route; external real fixture evidence | `R_2_4`, `R_2_5` | An AutoCAD 2.53J training disk from 1986 supplies 12 `AC1001`/R2.4 and 16 `AC1002`/R2.5 files. Every file passes strict blocking-versus-Stream reference parity with `full=0` and no decode errors. R2.4 covers 274 objects and R2.5 covers 5,014. The source item has no license URL, so these are external historical evidence and the binaries are not committed. |
| Pure stream route; real fixture evidence | `R_2_10` | Strict C parity covers 17 real files: the original entity/block fixtures plus 15 independent BSD-2-Clause AC2.10 entity files. Coverage includes POLYLINE with four VERTEX records and SEQEND, REPEAT/ENDREP, block sections, table entries, and per-handle references. The Stream reader now treats a zero optional-section start as absent even when its size word contains legacy high-bit flags, matching the blocking reader. No known committed or audited R2.10 fixture currently fails Stream. |
| Pure stream route; real fixture evidence | `R_2_6`, `R_9`, `R_10` | Strict C parity covers ten R2.6 files, one R9 file, and 19 R10 files. The independent sets add 3DFACE, ATTDEF, ARC, CIRCLE, DIMENSION, INSERT, JUMP, POLYLINE/VERTEX/SEQEND, SHAPE, SOLID, and TEXT evidence. Object/type counts, decoded callbacks, per-handle references, and table fixedtype masks match the blocking reader with `full=0`; R10 also streams UCS, VPORT, and APPID tables. |
| Partial pure stream support | `R_11b1`, `R_11b2` | The shared pre-R11 reader passes exact-version blocking-vs-stream parity on minimal C-writer LINE fixtures with table-entry coverage, decoded callbacks, `full=0`, and both stream flag settings. No real historical beta DWG fixture is present, so this remains generated coverage. |
| Pure stream route; mixed evidence | `R_13b1`, `R_13b2`, `R_13`, `R_13c3`, `R_14`, `R_2000b`, `R_2000`, `R_2000i`, `R_2002` | Uses the R13/R2000 handles/object-map reader through `R_2002`. Real fixtures cover R13, R14, and R2000. Original AutoCAD 2000i and 2002 media add 76 unique real AC1015/R2000-family files and 150,147 strictly aligned objects; neither product emits AC1016/AC1017. Generated MINSERT fixtures cover R13b1, R13b2, R13c3, and R2000b. Synthetic exact-header variants retain dispatch regression coverage for R2000i and R2002 with 750 objects and both flag settings. |
| Pure stream route; mixed evidence | `R_2004a`, `R_2004b`, `R_2004c`, `R_2004` | Uses the R2004 object-map reader. Generated exact-version MINSERT fixtures cover R2004a, R2004b, and R2004c with 37 objects, strict references, both stream flag settings, and `full=0`. An external real Autodesk ObjectARX `AC402b` sample covers R2004b with 50-object strict parity; its license prevents committing the binary. Real fixtures cover R2004, and a synthetic exact R2004c header over the real R2004 payload additionally passes 735-object parity. AutoCAD 2005/2006 are not separate enum values here. |
| Pure stream route; mixed evidence | `R_2007a`, `R_2007b`, `R_2007` | Uses the R2007 object-map reader. Generated exact-version MINSERT fixtures cover R2007a and R2007b with 37 objects, strict references, both stream flag settings, and `full=0`. The generated R2007 release fixture covers a multipage object section with 2,037 objects. Real fixtures cover R2007 release; no independently sourced historical R2007a/b files are present. AutoCAD 2008/2009 are not separate enum values here. |
| Pure stream route; mixed evidence | `R_2010b`, `R_2010` | Uses the R2004/2010+ data-section object-map reader with R2010 object headers. Real fixtures cover R2010 and a generated MINSERT fixture covers exact R2010b. AutoCAD 2011/2012 are not separate enum values here. |
| Pure stream route; mixed evidence | `R_2013b`, `R_2013` | Uses the R2004/2010+ data-section object-map reader. Real fixtures cover R2013, and a generated exact R2013b MINSERT fixture passes 37-object strict parity with `full=0`. The beta object layout is aligned by reading `has_ds_data` from the same R2013b boundary used by the encoder. AutoCAD 2014/2015/2016/2017 are not separate enum values here. |
| Pure stream route; mixed evidence | `R_2018b`, `R_2018` | Uses the R2004/2010+ data-section object-map reader. Real fixtures cover R2018 and a generated MINSERT fixture covers exact R2018b. AutoCAD 2019/2020/2021 are not separate enum values here. |
| Pure stream route; generated evidence | `R_2022b` | Uses the R2004/2010+ data-section object-map reader. Current validation uses a generated exact-version R2022b MINSERT fixture because no repository R2022 fixture exists. |
| Pure stream route; real R11 plus generated R12 evidence | `R_11` / `R_12` | The pre-R13 reader passes generated main, block, extra-entity, control, table, dimension, polyline, vertex, INSERT/MINSERT, and EED coverage. Real C parity covers 23 R11 DWGs from two independent sources. An ODA 27.1 `ACAD12` conversion adds explicit R12 provenance and covers two entities plus 24 table records. Seventeen external 2010 R12-labelled files add 732-object strict parity after exposing omitted table controls and synthetic block boundaries. Both Stream flag settings are covered by committed pre-R13 tests; per-handle references and `full=0` pass. `R_12` aliases `R_11` in the enum, and no independently sourced historical R12 file is present. |

Version routing is determined from the DWG file header before selecting a stream
reader. The C decoder reads the header magic at the start of the file and maps
it through `dwg_version_hdr_type()` to `Dwg_Version_Type`. The stream target must
use that version gate first, then dispatch to a pure stream reader or return the
appropriate explicit error. All current legal enum versions dispatch to a pure
stream reader. Do not infer stream support from filename, AutoCAD marketing
year, downstream behavior, or successful blocking read elsewhere.

For shared `AC1.50`, the initial magic result is refined after reading the
pre-R13 header: 74 or fewer header variables means `R_2_0b`; 83 means `R_2_0`.
For modern shared magic, the common header's internal `dwg_version` is combined
with the magic through `dwg_version_hdr_type2()` to refine the source enum.
The stream callback version is this refined source version, not merely the
initial family selected from the six-byte magic.

Header magic codes relevant to the current stream target:

| Header magic | libredwg version | Pure stream status |
| --- | --- | --- |
| `MC0.0` | `R_1_1` | Real external historical parity plus generated regression coverage |
| `AC1.2` | `R_1_2` | Real external historical parity plus generated regression coverage |
| `AC1.3` | `R_1_3` | Partial support: generated fixture coverage |
| `AC1.40` | `R_1_4` | Partial support: real entity-section fixture coverage |
| `AC1.50` | `R_2_0b` / `R_2_0` | Header variable count distinguishes them; both have generated fixture coverage |
| `AC2.10` | `R_2_10` | Partial support: real entity/table-section fixture coverage |
| `AC2.21` | `R_2_21` | Partial support: generated fixture coverage |
| `AC2.22` | `R_2_22` | Partial support: generated fixture coverage |
| `AC1001` | `R_2_4` | Real external historical parity plus generated regression coverage |
| `AC1002` | `R_2_5` | Real external historical parity plus generated regression coverage |
| `AC1003` | `R_2_6` | Partial support: real fixture coverage |
| `AC1004` | `R_9` | Partial support: real fixture coverage |
| `AC1005` | `R_9c1` | Partial support: generated fixture coverage |
| `AC1006` | `R_10` | Partial support: real fixture coverage |
| `AC1007` | `R_11b1` | Partial support: generated fixture coverage |
| `AC1008` | `R_11b2` | Partial support: generated fixture coverage |
| `AC1009` | `R_11` / `R_12` | Generated coverage, 23 real R11 fixtures from two independent sources, and one ODA 27.1 `ACAD12` fixture; no independently sourced historical R12 file |
| `AC1010` | `R_13b1` | Generated exact-version parity |
| `AC1011` | `R_13b2` | Generated exact-version parity |
| `AC1012` | `R_13` | Real fixture parity |
| `AC1013` | `R_13c3` | Generated exact-version parity |
| `AC1014` | `R_14` | Real fixture parity |
| `AC1500` | `R_2000b` | Generated exact-version parity |
| `AC1015` | `R_2000` | Real fixture parity, including original AutoCAD 2000i and 2002 media |
| `AC1016` | `R_2000i` | Synthetic dispatch guard; original 2000i media uses AC1015 |
| `AC1017` | `R_2002` | Synthetic dispatch guard; original 2002 media uses AC1015 |
| `AC402a` | `R_2004a` | Generated exact-version parity |
| `AC402b` | `R_2004b` | External real-file strict parity plus generated exact-version parity; binary not redistributable |
| `AC1018` | `R_2004c` / `R_2004` | Internal version refines the enum; generated and synthetic candidate parity plus real release parity |
| `AC701a` | `R_2007a` | Generated exact-version parity; no real historical fixture |
| `AC1021` | `R_2007b` / `R_2007` | Internal version refines the enum; generated beta parity plus generated multipage and real release parity |
| `AC1024` | `R_2010b` / `R_2010` | Generated beta and real release parity |
| `AC1027` | `R_2013b` / `R_2013` | Generated beta and real release parity |
| `AC1032` | `R_2018b` / `R_2018` | Generated beta and real release parity |
| `AC103-4` | `R_2022b` | Generated exact-version parity |

The evidence labels above are intentional. Generated coverage proves that the
C writer, blocking reader, and stream reader agree on the generated fixture;
it does not replace real historical DWG files. Synthetic exact-header coverage
proves header classification and reader parity on a real payload from the same
family, but it is not an independently sourced historical file of that exact
version. A route-only entry proves dispatch exists, not that the unrepresented
format variant has passed parity. Real fixture coverage is broader but still
cannot prove every file variant.

The 2026-07-10 exact-fixture audit checked every DWG blob in this repository's
Git history, 43 DWG files under the local user profile, all tracked DWG history
in the MIT-licensed ACadSharp repository, and 73 DWG fixtures from LibreCAD's
`dwgsupp2` branch. The libdxfrw repository has no tracked DWG fixture. These
sources contained no `AC701a` file and no AC1021 file whose internal version
identified `R_2007b`. Public search found definitions for those variants but no
downloadable sample with a verifiable reusable license. This records the
current evidence boundary; it does not assert that historical R2007 alpha or
beta files cannot exist elsewhere.

The 2026-07-11 follow-up audit found the BSD-2-Clause licensed
`michal-josef-spacek/CAD-Format-DWG-AC1009` repository. Its README explicitly
identifies its fixtures as AutoCAD 11 AC1009 files. Twenty entity DWGs were
imported with the upstream license and exact commit recorded in
`test/test-data/r11-ac1009/README.md`. This closes the independent-source R11
entity gap, but it is not evidence for a separately identified R12 file.

ODA File Converter 27.1.0.0 was then run with the repository's
`test/test-data/r12/Constraints.dxf` input and the explicit `ACAD12 DWG`
target. The resulting `AC1009` file initially failed both `dwg_read_file` and
the pure Stream path because valid table addresses were paired with table
sentinels that did not match the R11 positional assumption. Table record and
file bounds remain mandatory, but a missing table sentinel is now a
noncritical integrity warning. The fixed artifact passes blocking read and
both Stream flag settings with 26 decoded objects, including two entities and
24 table records, zero decode-error callbacks, strict per-handle references,
and `full=0`. Its command, input/output hashes, and independent-implementation
provenance are recorded in `test/test-data/r12-oda/README.md`. This is explicit
R12 format evidence, but not a historical R12 sample.

The same audit inspected the BSD-2-Clause AC1.40, AC2.10, AC1003, AC1004, and
AC1006 repositories. Decoded Stream screening passed all 80 AC1.40 files, all
272 AC1003 files, the single AC1004 file, and all 32 AC1006 files. AC2.10 first
passed 176 of 179 paths; the three failures were byte-identical copies of
`PLINE1.DWG`. The blocking reader accepted that file, but Stream rejected its
zero `extras_start` paired with a high-bit-marked `extras_size`. After matching
the blocking reader's absent optional-section rule, all 179 AC2.10 paths pass.
The committed strict regression subset contains 64 unique entity files from
AC1.40, AC2.10, AC1003, and AC1006 with exact commits and licenses recorded in
their fixture directories.

A broader 2026-07-11 GitHub audit enumerated 1,970 unique repositories returned
by `dwg` and `autocad` repository searches. Of the 331 repositories with an
explicit SPDX license result, 79 contained DWG paths: 870 paths were inspected
and 808 complete valid DWGs were classified by their file headers. The exact
header counts were `AC1.40` 2, `AC2.10` 4, `AC1003` 4, `AC1004` 2, `AC1006`
37, `AC1009` 6, `AC1012` 4, `AC1014` 37, `AC1015` 96, `AC1018` 77, `AC1021`
74, `AC1024` 77, `AC1027` 114, and `AC1032` 274. None supplied one of the
generated-only or synthetic-only exact-version magics listed below. The six
`AC1009` paths were copies of R11 fixtures already represented by the committed
R11 sources.

The remaining 1,639 repositories had no reusable repository-wide license
result. Of those, 358 contained 3,173 DWG paths and 3,167 complete valid DWGs.
The only files matching a generated-only exact-version magic were five
byte-identical `AC402b` copies of Autodesk ObjectARX `StandardTest.dwg`, SHA256
`623E79D3A5422DEE1EA9206E698466EC96C994CC0FA925F7F9A348DEB8E09D1D`.
Their repository license explicitly excludes Autodesk SDK files from its MIT
grant, so they cannot be imported as regression fixtures.

A separate GitHub code-index audit searched `.dwg` paths directly, so its
coverage does not depend on repository names or descriptions containing DWG or
AutoCAD. The API exposed 900 of 4,480 indexed results, spanning 75 repositories;
74 repositories were new relative to the 1,970-repository audit above. Seventeen
of those new repositories reported an explicit reusable SPDX license. Their
indexed results contained 512 paths and 447 unique blobs across 13 repositories.
Of those blobs, 430 were Git LFS pointer text rather than DWG content. The
remaining old-version hits were 4-6 byte file-signature skeletons from
DROID/Siegfried/PRONOM/Wikidata test suites, including `MC0.0`, `AC1.2`,
`AC1.3`, `AC1.40`, `AC1.50`, `AC2.10`, `AC2.21`, `AC2.22`, `AC1001` through
`AC1006`, and `AC1009` through `AC1015`. In particular, the only indexed
`AC1013` hit was the six-byte
`theseus-rs/file-type/test_data/wikidata/wikidata-27863128-signature-1.dwg`
blob, not an R13c3 drawing. Signature-only skeletons are invalid DWGs and do not
close generated-only or historical fixture gaps.
Additional code-index partitions for `r12`, `r13`, `r2004`, `r2007`, and
`acad12` returned no DWG results. The `alpha` partition returned three
byte-identical GPL-2.0 paths whose content starts with `<!DOCTYPE` and is HTML,
not DWG. The only `beta` result was in a repository without a reusable license.

The licensed-repository audit also enumerated every non-default branch of the
79 repositories that contain DWG files. Twenty-four repositories had multiple
branches: 125 alternate branch trees were read without error, yielding 1,322 DWG
paths and 377 unique blob IDs. Their complete header distribution was `AC1.40`
1, `AC2.10` 2, `AC1003` 2, `AC1004` 1, `AC1006` 2, `AC1009` 3, `AC1012` 3,
`AC1014` 19, `AC1015` 51, `AC1018` 46, `AC1021` 35, `AC1024` 31, `AC1027`
46, and `AC1032` 133, plus one non-DWG text file and one `AC402b` candidate.
The `AC402b` blob is
`kevinzhwl/AutoCAD.DK@arx-2004:ObjectARX/samples/editor/CircleStandard/StandardTest.dwg`.
It is byte-identical to the already excluded Autodesk SDK sample: 25,577 bytes,
SHA256 `623E79D3A5422DEE1EA9206E698466EC96C994CC0FA925F7F9A348DEB8E09D1D`.
Adjacent source identifies Autodesk copyright and grants software distribution
only in object-code form; it does not clearly relicense the DWG test data. The
repository's root GPL file cannot replace the third-party file's missing data
license, so this duplicate remains external-only evidence and is not imported.

Release tags were audited separately because they can retain fixtures deleted
from all live branches. Twenty-three of the 79 licensed DWG repositories had
tags. All 364 unique tagged commits were resolved and read with no tag or tree
errors, covering 4,409 DWG paths and 413 unique blobs. Their exact header counts
were `AC1.40` 1, `AC2.10` 2, `AC1003` 2, `AC1004` 1, `AC1006` 2, `AC1009` 3,
`AC1012` 1, `AC1014` 24, `AC1015` 55, `AC1018` 49, `AC1021` 40, `AC1024` 42,
`AC1027` 61, and `AC1032` 130. No generated-only or synthetic-only exact
version magic occurred in any licensed release tag.

GitLab was audited as a separate hosting source. Its anonymous global blob
search requires authentication, so the reproducible public boundary is project
search for `dwg` and `autocad`: 182 unique projects were enumerated. Twenty-eight
readable default branches contained 387 DWG paths and 244 unique blobs. Sixteen
project trees could not be read (15 returned HTTP 404 and one timed out), and
remain explicitly outside the evidence set. The readable unique blobs contained
two non-DWG files and these exact DWG header counts: `AC1.40` 1, `AC2.10` 2,
`AC1003` 2, `AC1004` 1, `AC1006` 2, `AC1009` 3, `AC1012` 1, `AC1014` 4,
`AC1015` 26, `AC1018` 24, `AC1021` 23, `AC1024` 30, `AC1027` 40, and
`AC1032` 83. No generated-only or synthetic-only exact-version magic occurred
in the readable GitLab set.

Debian source archives were audited independently of repository names. The
complete `main/Contents-source.gz` indexes for sid, bookworm, bullseye, buster,
and stretch contained respectively 71, 77, 76, 70, and 68 paths ending in
`.dwg`. Their SHA256 values were
`69814BE8047E366CAEE4D29E8D786A48868D1E9A36D17441198FDF6BA1A2BBAC`,
`541C7D9ED1867DFBBB52A10F3BD39664B278A912CAE28E59BFCDFB200563D394`,
`1848F8563CB212657798A77938C2009D9E33BC7B4E331B2AE74C4D86FB96ECD2`,
`EA213EF2CE036A735816C2EA7A9457F479EE3F7A83FA0B4A4486C92F6E45F307`,
and
`19711ECC947018290F2E4B4E5B17591029AE306C683DE2DC0BCF45B7C0C219EE`.
The corresponding `Sources.gz` records selected exact source versions and
archive checksums rather than assuming that a repeated path had unchanged
content. Every downloaded original archive matched its `Checksums-Sha256`
record.

Sixty-one of the paths are password dictionaries from `crack`, not CAD files.
The three `libpj-java` files begin with a Java serialization header and are not
AutoCAD DWGs. The remaining files came from `ezdxf`, `qgis`, `ruby-marcel`,
`z88`, and `spe`. All three audited ezdxf source versions (`0.14.2`, `0.18.1`,
and `1.4.3`) reduce to the same four `AC1027` documentation blobs and one
`AC1015` test blob. All three ruby-marcel versions (`0.3.2`, `1.0.1`, and
`1.2.1`) contain the same `AC1021` blob. QGIS `3.44.12`, `3.22.16`, `3.10.14`,
and `2.18.28` contain the same `AC1027` blob; QGIS `2.14.11` predates that
member and contains none. The historical-only additions are five `AC1015` and
one `AC1018` file from z88 `13.0.0`, plus one `AC1018` file from spe `0.8.4.h`.
This leaves 14 unique valid DWGs: six `AC1015`, two `AC1018`, one `AC1021`,
and five `AC1027`. None has a generated-only or synthetic-only exact-version
magic. Jessie and earlier Debian archive directories do not publish the same
source-file contents index, so they remain outside this reproducible global
file-path audit rather than being silently counted as searched.

All 14 unique Debian-source DWGs pass strict blocking-versus-Stream reference
parity with `full=0` and zero per-object decode errors. Together they cover
4,835 objects: 3,147 entities and 1,688 non-entities. The source packages carry
Debian `main` copyright records, but no binary was imported because these
release-version files do not close any exact-version gap. They remain external
audit evidence and avoid increasing the committed fixture set without changing
the acceptance boundary.

The complete SourceForge libdxfrw release tree was audited separately because
release archives can retain test data that is absent from live source-control
branches. Eleven releases in version directories cover `0.0.1` through `0.5.0`;
fourteen root releases cover `0.5.3` through `0.6.3`, including `0.6.0-b`.
Every one of the 25 unique source tarballs has a valid bzip2/tar structure, and
none contains a `.dwg` member. The `0.5.12` source RPM contains one source
tarball whose SHA256 is byte-for-byte identical to the root `0.5.12` archive,
plus packaging metadata, so it adds no independent payload. The exact archive
names, sizes, and SHA256 values are recorded in
`target/sourceforge-libdxfrw-sha256.txt`. The apparent non-beta `0.6.0` filename
found in page metadata has no downloadable file (the official mirror returns
HTTP 404) and is not counted as an audited archive. Therefore the published
libdxfrw releases supply no DWG candidate and close none of the exact-version
or historical fixture gaps.

Official GNU LibreDWG release archives were audited independently of the live
tree. All 24 formal `.tar.xz` releases from 0.5 through 0.13.3 pass XZ and tar
integrity checks and have valid detached signatures from fingerprint
`2895A881D34270FABFE8F747B4F63339E65D6414`. They contain 3,008 DWG paths but
only 145 unique blobs. Their exact archive hashes, sizes, member counts, and
verification results are in `target/gnu-release-dwg-audit.txt`.

The 145 unique file headers are one `AC1.40`, two `AC2.10`, two `AC1003`, one
`AC1004`, two `AC1006`, three `AC1009`, one `AC1012`, four `AC1014`, 24
`AC1015`, 25 `AC1018`, 19 `AC1021`, 20 `AC1024`, 20 `AC1027`, and 21
`AC1032`. None has a generated-only or synthetic-only exact-version magic. Six
release blobs are absent from the current repository: two `AC1015`, three
`AC1018`, and one `AC1024`. All six pass strict blocking-versus-Stream reference
parity with `full=0` and no per-object decode errors, covering 51,122 objects,
48,685 entities, and 2,437 non-entities. They are retained as external signed
release evidence because importing release-version duplicates would not close
an exact-version gap.

The three alpha.gnu.org archives (`0.4.900`, `0.4.924`, and `0.4.938`) could
not be downloaded through the current network path, so their archive signatures
are not claimed as verified. Their corresponding Git tags are present, however,
and each tag has 118 DWG paths whose path/blob pairs are identical to tag 0.5.
The signed 0.5 tar also matches tag 0.5 for all 118 DWG path/blob pairs. This
proves the alpha-tag DWG tree adds no different sample while keeping the
unavailable alpha release archives outside the verified-archive boundary.

The Digital Corpora Govdocs1 corpus was audited through its public S3 object
listing and complete 298,928,989-byte `dump.sql` metadata export. The SQL was
read only as text and was not executed. The complete `by_type` prefix has 17
office, image, text, and Flash archives but no DWG/CAD archive. Searching every
metadata tuple for `.dwg` URLs and DWG/AutoCAD type markers produced two
`.dwg` URLs. Docid 496507 is only a 260-byte FrontPage `_vti_cnf` text record.
Docid 332336 is an 86,817-byte `AC1014` R14 DWG whose extracted SHA1 matches
the metadata record despite its stored `.html` name. Strict C reference parity
passes with 808 objects, 739 entities, 69 non-entities, 808 decoded callbacks,
zero decode errors, and `full=0`. Exact S3, ZIP, member, and hash evidence is in
`target/govdocs1-dwg-audit.txt`. The corpus adds one independent real R14 check
but no generated-only or synthetic-only exact-version header, so it closes no
remaining historical fixture gap and no corpus binary is committed.

Historical AutoCAD installation and training media on Internet Archive were
then inspected internally rather than relying on item titles. The 956,430-byte
AutoCAD 2.53J German training ZIP has a dedicated sample-drawing floppy with 12
`AC1001` and 16 `AC1002` DWGs. LibreDWG maps these exact headers to `R_2_4` and
`R_2_5`; none duplicates a repository fixture. All 28 pass strict C reference
parity with `full=0` and no decode errors. R2.4 covers 274 objects, 145 entities,
and 129 non-entities; R2.5 covers 5,014 objects, 4,601 entities, and 413
non-entities. This replaces generated-only status for both exact versions with
real 1986 historical evidence. The item has no license URL, so no binary is
imported and committed generated fixtures remain the reproducible regression
coverage.

An archive titled "Autocad-86 ver 1.3" adds still earlier evidence. Its 18
DWGs date from 1983, but their actual headers are one `MC0.0` and 17 `AC1.2`,
not `AC1.3`. LibreDWG maps them to `R_1_1` and `R_1_2`. All pass strict C
reference parity with `full=0` and no decode errors: 185 objects for R1.1 and
1,522 for R1.2. None duplicates a repository fixture. This replaces
generated-only status for both versions, while `R_1_3` remains an explicit
real-historical-file gap. This item also has no license URL, so its binaries
remain external-only evidence. A separate Victor Pulse D005 AutoCAD-86
training disk was also checked: all 18 real members are `AC1.2`, five duplicate
the first archive and 13 are new, and all pass 1,664-object strict parity. Its
18 AppleDouble `._*.DWG` sidecars are not drawings. It therefore expands R1.2
evidence but confirms no `AC1.3` sample in the two available training sets.

A separate 1986 student work disk contains 20 `AC2.10` files. Nineteen pass
strict parity, covering 3,867 objects; `POLYLINE.DWG` fails the blocking reader
with critical error `0x800` and is therefore not a blocking-success/Stream-
failure case. The Public Domain Mark item titled "Autodesk AutoCAD R13 c4 beta
#4" contains 64 DWGs, but their actual headers are 16 `AC1009` and 48 `AC1012`,
not an R13 beta magic. All 64 pass strict reference parity with `full=0`,
covering 90,202 objects. It broadens R11/R13 evidence but does not close an R13
beta gap. Complete media hashes, member hashes, classifications, and parity
totals are in `target/internet-archive-autocad-media-audit.txt`.

Internet Archive metadata was searched directly on 2026-07-11 rather than by
assuming that an item title implied file content. `title:DWG` returned 161
identifiers and `subject:DWG` returned 224; 48 overlap, leaving 337 unique
items. Their static `_files.xml` manifests contain 22 direct DWG files. Exact
six-byte HTTP Range reads classify them as one `AC1018`, one `AC1021`, one
`AC1024`, five `AC1027`, and fourteen `AC1032`. Twenty have no item-level
license and two are CC BY-NC-ND 4.0, so none is a redistributable fixture and
none has a missing exact-version magic. Names, sizes, source SHA1 values,
licenses, and headers are recorded in
`target/internet-archive-dwg-search-audit.txt`.

The broader `description:DWG` query returned 1,331 items. Restricting it to the
explicitly reusable CC BY 3.0/4.0, CC BY-SA 3.0, CC0, and Public Domain Mark
license URLs produced 431 identifiers. Eleven overlap the title/subject set,
so these searches cover 757 unique item identifiers in total. Every one of the
431 reusable-description item manifests was inspected, and none contains a
direct `.dwg` file. This is an explicit metadata-search boundary, not a claim
that Archive.org provides a global filename index or that unindexed archives
were searched internally.

The separately identified 2002 `voloview2` item has no license URL and contains
Autodesk Volo View installers, not direct DWGs. Its 24,609,256-byte
`vve201setup.exe` matches the item SHA1
`e0b1aed2bd5212c2114b63d9adeb4d290c653c28`. Expanding the installer yields
28 files and 12 CAB archives; none of the CAB member paths ends in `.dwg`.
It therefore supplies neither a redistributable fixture nor external
`R_2000i`/`R_2002` interoperability evidence.

The 2010 Internet Archive item
[`LibredwgTestSuite0.1`](https://archive.org/details/LibredwgTestSuite0.1)
was also audited outside the repository. Its 0.1 archive contains 1,335 DWGs:
267 each of `AC1012`,
`AC1014`, `AC1015`, `AC1018`, and `AC1021`, so it adds no missing exact-version
header. The later `testsuite0.2.tar.gz` contains 187 DWGs. Eighty-five paths
start with `AC1009`, but they are only 17 unique blobs: the files under the
`2.5`, `2.6`, `9`, `10`, and `12` directories are byte-identical by basename.
They therefore cannot be counted as five independent old formats. The archive
description labels the 17 drawings as empty, LINE, RAY, XLINE, 2D/3D POLYLINE,
polygon, rectangle, ARC, CIRCLE, donut, SPLINE, ELLIPSE, HATCH, REGION, TEXT,
and MTEXT cases.

Before the current fix, all 17 R12-labelled unique files were blocking-success
but Stream-failure cases. Stream omitted ten table control objects and the
zero-sized synthetic `BLOCK`/`ENDBLK` pair that the blocking pre-R13 decoder
creates. After emitting those existing decoded objects, all 17 pass strict
reference parity with `full=0`: 732 objects in total, 29-98 per file, with
4-71 entities and 25-30 non-entities per file. The regression suite now counts
the complete blocking object array for pre-R13 fixtures and keys reference
snapshots by handle, fixedtype, and supertype because old files can reuse a
handle across different object kinds.

The audit archives have SHA256
`363A4EEFF081268C78B79FF016FB289C40D4B8B36BC820693A2E9615475D8C71`
for 0.1 and
`4D8E5DA30278C07D294115B6D0528955B2B5FF4C57A49A27D3055CF6B277D79D`
for 0.2. The item metadata currently points to a public-domain license and the
[contemporaneous test-suite discussion](https://lists.gnu.org/archive/html/libredwg/2010-01/msg00076.html)
says newly created files should be released to the public domain. However, an
[earlier message](https://www.mail-archive.com/libredwg%40gnu.org/msg00049.html)
explicitly rejected the initial 0.1 upload as CC BY-NC, and the 0.2 tarball has
no embedded data license. The binary files therefore remain external-only audit
material. They were generated in 2010 and do not close the independently
sourced historical R12 gap.

Original AutoCAD 2000i and AutoCAD 2002 installation media were then inspected
without installing either product. Their ACAD1.CAB files contain 75 and 76 DWG
paths respectively; the 2002 set reuses all 75 files and adds `MKMPlan.dwg`, so
the union has 76 unique blobs. Every file is `AC1015` with internal DWG version
`0x17`; none is `AC1016` or `AC1017`. All 76 pass strict reference parity with
150,147 objects, 139,047 entities, 11,100 non-entities, zero Stream decode
errors, and `full=0`. This supplies real product-provenance evidence for 2000i
and 2002 while confirming that their files use the shared R2000 format. Exact
media hashes, maintenance-version distributions, and results are recorded in
`target/internet-archive-autocad-2000i-2002-audit.txt`.

The proprietary `AC402b` file supplies external real-file parity evidence.
Blocking and pure Stream both accept it as `R_2004b`; strict Stream
parity covers 50 objects, including 14 entities and 36 non-entities, references,
semantic counters, zero decode errors, and `full=0`. It exposed a callback
contract bug: an existing `DWG_ERR_VALUEOUTOFBOUNDS` warning was being ORed into
the caller's abort value. R2004 and R2007 metadata streaming now carry callback
abort state separately and return the callback value exactly. The generated
checksum-warning regression reproduces the condition without distributing the
proprietary file, and the external `AC402b` strict run now passes completely.
This closes the R_2004b real-file evidence gap under the same external-only
rule used for unlicensed historical media, but it does not provide a freely
redistributable regression fixture. Reproducible source, hash, result, and
license-boundary details are in `target/github-objectarx-ac402b-audit.txt`.

The important distinction is that a successful blocking read elsewhere does not
count as stream support. Every current legal enum version now has an explicit
stream route. A future known version without a route must fail clearly with
`DWG_ERR_NOTYETSUPPORTED`, while an unknown header fails with
`DWG_ERR_INVALIDDWG`; neither case may load the whole file through
`dwg_read_file`.

The explicit missing pure-stream route list is therefore:

```text
Current legal Dwg_Version_Type versions without a pure stream route:
none

Generated-only exact-version support still needing real historical DWG files:
R_1_3, R_2_0b, R_2_0, R_2_21, R_2_22, R_9c1,
R_11b1, R_11b2, R_13b1, R_13b2, R_13c3, R_2000b, R_2004a,
R_2004c, R_2007a, R_2007b, R_2010b, R_2013b, R_2018b,
R_2022b

Historical product aliases represented by the shared AC1015 format:
R_2000i, R_2002 (76 unique files from original product media pass as R_2000)

Pure stream routes with no valid exact-version fixture at all:
none

Known blocking-success/Stream-failure fixtures in the committed regression set,
the audited licensed GitHub and readable GitLab sets, the Debian-source set, or
the Govdocs1 set, historical AutoCAD media sets, externally checked proprietary
AC402b sample, and Internet Archive R12-labelled set:
none

Strict real-file counts for the expanded historical families:
R_1_4: 25
R_1_1: 1 external historical
R_1_2: 17 external historical
R_2_4: 12 external historical
R_2_5: 16 external historical
R_2_10: 17
R_2_6: 10
R_9: 1
R_10: 19
R_11: 23

R_12 remaining historical-evidence gap:
an independently sourced historical R12 DWG file. The committed ODA 27.1
`ACAD12` fixture supplies explicit R12 provenance from an independent writer,
but it was generated in 2026 and must not be relabeled as a historical sample.
The 17 external Internet Archive files were generated in 2010, all five
purported older-version directory copies collapse to the same 17 `AC1009`
blobs, and their tarball lacks an embedded data license. They expand external
R12-labelled interoperability coverage but do not close the historical or
redistributable-fixture gap.
`R_12` aliases `R_11` in the enum, so header bytes alone cannot distinguish
the two releases.

`DWG_TYPE_REPEAT` and `DWG_TYPE_ENDREP` are version-valid before `R_2_10`;
`DWG_TYPE_LOAD` is version-valid before `R_2_0b`; `_3DLINE` is valid for
`R_2_4` through `R_10`. Those are older pre-R13 version-family targets, not
R11/R12 entity gaps.
```

The explicit completed pure-stream route list is:

```text
R_1_4 strict parity on 25 real fixtures,
R_1_1 strict parity on one and R_1_2 on 17 external historical fixtures,
R_1_3 generated fixture coverage,
R_2_10 strict parity on 17 real fixtures including POLYLINE/VERTEX/SEQEND,
R_2_0b, R_2_0, R_2_21, R_2_22, and R_9c1 generated fixture coverage,
R_2_4 strict parity on 12 and R_2_5 on 16 external historical fixtures,
R_2_6 strict parity on 10 real fixtures, R_9 on one, and R_10 on 19,
R_11b1 and R_11b2 generated fixture coverage,
R_11 generated coverage plus 23 real R11 fixtures from two independent sources,
R_12 ODA 27.1 `ACAD12` fixture coverage with 26 decoded objects,
R_13b1 and R_13b2 generated, R_13 real, R_13c3 generated,
R_14,
R_2000b generated; R_2000 real, including 76 unique original AutoCAD 2000i
and 2002 media files; R_2000i and R_2002 synthetic dispatch guards,
R_2004a and R_2004c generated, R_2004b external real plus generated,
R_2004c also synthetic, R_2004 real,
R_2007a and R_2007b generated, R_2007 generated multipage and real,
R_2010b generated, R_2010 real,
R_2013b generated, R_2013 real,
R_2018b generated, R_2018 real,
R_2022b generated
```

Development targets from the current state to complete stream parity:

1. Complete the remaining exact-version and historical fixture gaps.
   - First, replace generated-only modern evidence for `R_2000b`, `R_2004a`,
     `R_2004c`, `R_2007a`, `R_2007b`, `R_2010b`, `R_2013b`,
     `R_2018b`, and `R_2022b` with independently sourced real historical DWG
     files.
   - Keep the synthetic `R_2000i` and `R_2002` dispatch checks, but do not count
     AC1016/AC1017 as missing historical product formats. Original AutoCAD
     2000i and 2002 media establish that both products use AC1015/R2000. R2004c
     retains a synthetic real-family payload check in addition to its generated
     exact fixture.
   - After the DWG 2000-and-later queue is complete, replace generated-only
     evidence for `R_1_3`, `R_2_0b`, `R_2_0`, `R_2_21`, `R_2_22`, `R_9c1`,
     `R_11b1`, `R_11b2`, `R_13b1`, `R_13b2`, and `R_13c3` with real historical
     DWG fixtures.
   - Add an independently sourced historical R12 DWG before calling the shared
     R11/R12 family historically complete. The ODA `ACAD12` fixture closes the
     independent-writer format gap but not the historical-evidence gap. R11
     has 23 real fixtures from two independent sources.
2. Keep unsupported-version behavior explicit.
   - Supported pure stream versions must pass with or without
     `DWG_STREAM_F_NO_FULL_FALLBACK`.
   - Unknown or invalid headers must return `DWG_ERR_INVALIDDWG` with no object,
     decoded-object, or decode-error callbacks.
   - Any future known version without a stream route must return
     `DWG_ERR_NOTYETSUPPORTED` with no callbacks.
   - Version detection is based on the DWG file header and any required header
     refinement, not filenames or blocking-reader behavior.
3. Measure memory on the large-file set after each new version family.
   - The pure stream path should keep the native side incremental.
   - Downstream aggregation is not part of this C-side acceptance check.

The project status is: every current legal `Dwg_Version_Type` has a C pure
stream route. C stream parity is real for the modern families and has real
pre-R13 coverage for R1.4, R2.10, R2.6, R9, R10, and R11. Generated coverage
additionally reaches R1.1-R1.3, R2.0 beta, R2.0, R2.21, R2.22, R2.4, R2.5,
R9c1, R11b1, R11b2, R13b1, R13b2, R13c3, R2000b, R2004a/b/c,
R2007a/b, R2010b, R2013b, R2018b, and R2022b. Generated multipage coverage
also reaches R2007 release. Synthetic exact-header parity reaches R2000i,
R2002, and R2004c. Every current enum now has at least one valid exact-version
fixture accepted by the blocking reader and the pure stream reader. Complete
historical evidence coverage is not done: the generated, synthetic, and
historical exact-version gaps above remain open. There is currently no known
blocking-success/Stream-failure fixture in the committed set, the broader
licensed GitHub audit, the readable GitLab audit, the Debian-source audit, or
the externally checked AC402b and Internet Archive samples.

## 9. C-side work items

These are continuing C-side acceptance invariants. The current harness checks
them against blocking-reader snapshots; a newly found mismatch reopens the
corresponding work item.

Required areas:

1. `dwg_stream_file_ex` or the equivalent stream API must emit enough decoded objects and references for C-side parity.
2. The stream path must expose valid references for:
   - `INSERT` / `MINSERT` block headers.
   - `DIMENSION_COMMON.block`.
   - Anonymous dimension blocks.
   - Block-owned entities.
   - POLYLINE vertices.
   - HATCH / fill / wipeout / text entities.
3. Owner semantics must be correct:
   - Model-owned entity.
   - Block-owned entity.
   - Ownerless entity.
   - Recovered owner.
   - `entmode` behavior.
4. Block reference counting must be complete enough to decide which block contents should be replayed.
5. Referenced `BLOCK_HEADER` data must be available without a full `dwg_read_file`.
6. Text/style/layer/color resolution from stream must match full load.
7. POLYLINE_3D vertex ownership must be deterministic. A downstream pending-vertex fallback is not a proper final fix.
8. The stream API should let future consumers cache and replay block contents deterministically without guessing by layer name.

If the C API cannot provide one of these pieces yet, the missing contract should be documented explicitly before any downstream workaround is added.

## 10. Memory target

The native stream implementation should not allocate the same full DWG object graph as `dwg_read_file`.

Acceptance is staged:

1. First pass C strict parity on `E:\cadTestFolder\test1.dwg`.
2. Then validate that native stream avoids `dwg_read_file`.
3. Then measure peak memory on the large-file set.

The old observed problem is that some DWGs required tens of GB in the full-load path. The stream design is only useful if the native side provides an API that can be consumed incrementally later.

Downstream code may still aggregate data later for parity/debug reports. That is
outside the current validation loop, and the C/libredwg stream API must not
force full materialization.

## 11. Required iteration artifacts

Each development iteration should report:

- libredwg branch and commit hash.
- Build target and command.
- `stream_test.exe` output for `test1.dwg`.
- `stream_test.exe` output with `LIBREDWG_STREAM_TEST_REFS=1`.
- Large-file smoke output, if run.
- Whether the stream path used `dwg_read_file`.
- Summary of changed stream APIs or structs.
- Pass/fail result for every required criterion.
- Peak memory, if measured.

Do not treat an iteration as successful just because a downstream viewer opens a
file. The authoritative result for this phase is C strict parity plus no
`dwg_read_file` in the stream path.
