# LibreDWG C stream TDD target

This document defines the active C-side target for the DWG stream work. The
development target is libredwg C code, and the verification path is C blocking
read versus C stream read.

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
# fixtures, plus TS1.dwg for OLE/OLE2 entity classification coverage. It also
# verifies that generated unsupported pre-R13 fixtures reject with
# DWG_ERR_NOTYETSUPPORTED. Stream APIs do not fall back to full load.
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
- Callback abort errors are propagated.
- Unsupported versions return `DWG_ERR_NOTYETSUPPORTED` without invoking any
  stream callbacks.

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
lightweight stream path, invalid public API arguments, and unsupported-version
rejection without object, decoded-object, or decode-error callbacks. It also
verifies that both old `dwg_stream_file` callbacks and `dwg_stream_file_ex`
callbacks can return `DWG_ERR_NOTYETSUPPORTED` as a callback abort once a
stream path has been selected, without being mistaken for an unsupported
version. The old `DWG_STREAM_F_NO_FULL_FALLBACK` flag is retained for source
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

Latest local `test1.dwg` semantic coverage:

```text
block_headers=2434 block_headers_owned=2433 block_chains=2434
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

Default unsupported-version guard:

```text
Generated pre-R13 fixtures reject stream reading with DWG_ERR_NOTYETSUPPORTED
and no object, decoded-object, or decode-error callbacks.
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
`dwg_stream_file_ex` distinguishes unsupported DWG versions from callback
errors raised inside an already-selected stream decoder. Unsupported versions
return `DWG_ERR_NOTYETSUPPORTED` directly, so callback return values are not
swallowed or reinterpreted as fallback requests.
Decoded-object streaming uses an isolated temporary Dwg_Data wrapper with its
own single-object pool, object_ref vector, and object_map for each decoded
callback. The host Dwg_Data object pool, object_ref, HANDSEED, dirty_refs, and
object_map are not borrowed or mutated by decoded callback emission.
`stream_test` directly covers this host-state isolation and verifies that the
host object_ref, HANDSEED, dirty_refs, and object_map state is preserved after a
decode-error path.
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

Latest local large-file `dwgprobe` stream metadata memory sample:

```text
rss_mb=210
decode_mode=r2004-object-map
input_mode=file-map
precheck=high:max-object>=16MiB
```

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

Earlier local Autotools validation for this stream target:

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

Shared out-of-tree Autotools builds are not the local acceptance route on this
Windows/MSYS setup because libtool executable wrappers can return success
without launching the real .libs executable when the build-local DLL is not in
the DLL search path. Static Autotools builds avoid that wrapper/DLL problem and
are the canonical local `make check` route for this stream target.
```

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
- Unsupported or not-yet-streamed versions are explicitly reported as gaps and
  must not be hidden behind full fallback.

Exact current implementation status by libredwg `Dwg_Version_Type` enum:

| Status | Exact versions | Meaning |
| --- | --- | --- |
| Pure stream supported | `R_13b1`, `R_13b2`, `R_13`, `R_13c3`, `R_14`, `R_2000b`, `R_2000`, `R_2000i`, `R_2002` | Uses the R13/R2000 handles/object-map stream reader. Must pass parity with `full=0`. |
| Pure stream supported | `R_2004a`, `R_2004b`, `R_2004c`, `R_2004` | Uses the R2004 object-map stream reader. Must pass parity with `full=0`. AutoCAD 2005/2006 are not separate enum values here; they are covered by the `R_2004` file family when the file identifies that way. |
| Pure stream supported | `R_2007a`, `R_2007b`, `R_2007` | Uses the R2007 object-map stream reader. Must pass parity with `full=0`. AutoCAD 2008/2009 are not separate enum values here; they are covered by the `R_2007` file family when the file identifies that way. |
| Pure stream supported | `R_2010b`, `R_2010` | Uses the R2004/2010+ data-section object-map stream reader with R2010 object headers. Must pass parity with `full=0`. AutoCAD 2011/2012 are not separate enum values here; they are covered by the `R_2010` file family when the file identifies that way. |
| Pure stream supported | `R_2013b`, `R_2013` | Uses the R2004/2010+ data-section object-map stream reader with R2010+ object headers. Must pass parity with `full=0`. AutoCAD 2014/2015/2016/2017 are not separate enum values here; they are covered by the `R_2013` file family when the file identifies that way. |
| Pure stream supported | `R_2018b`, `R_2018` | Uses the R2004/2010+ data-section object-map stream reader with R2010+ object headers. Must pass parity with `full=0`. AutoCAD 2019/2020/2021 are not separate enum values here; they are covered by the `R_2018` file family when the file identifies that way. |
| Pure stream supported | `R_2022b` | Uses the R2004/2010+ data-section object-map stream reader with R2010+ object headers. Must pass parity with `full=0`. Current validation uses a generated R2022b MINSERT fixture because no repository R2022 fixture exists. |
| Not pure stream supported | `R_2_0b`, `R_2_0`, `R_2_10`, `R_2_21`, `R_2_22`, `R_2_4`, `R_2_5`, `R_2_6`, `R_9`, `R_9c1`, `R_10`, `R_11b1`, `R_11b2`, `R_11` / `R_12` | Pre-R13 formats are not implemented in the current pure stream path. Stream APIs must return `DWG_ERR_NOTYETSUPPORTED` for these versions until a real stream reader exists. |

Version routing is determined from the DWG file header before selecting a stream
reader. The C decoder reads the header magic at the start of the file and maps
it through `dwg_version_hdr_type()` to `Dwg_Version_Type`. The stream target must
use that version gate first, then dispatch to a pure stream reader or return
`DWG_ERR_NOTYETSUPPORTED`. Do not infer stream support from filename, AutoCAD
marketing year, downstream behavior, or successful blocking read elsewhere.

Header magic codes relevant to the current stream target:

| Header magic | libredwg version | Pure stream status |
| --- | --- | --- |
| `AC1010` | `R_13b1` | Supported |
| `AC1011` | `R_13b2` | Supported |
| `AC1012` | `R_13` | Supported |
| `AC1013` | `R_13c3` | Supported |
| `AC1014` | `R_14` | Supported |
| `AC1500` | `R_2000b` | Supported |
| `AC1015` | `R_2000` | Supported |
| `AC1016` | `R_2000i` | Supported |
| `AC1017` | `R_2002` | Supported |
| `AC402a` | `R_2004a` | Supported |
| `AC402b` | `R_2004b` | Supported |
| `AC1018` | `R_2004c` / `R_2004` | Supported |
| `AC701a` | `R_2007a` | Supported |
| `AC1021` | `R_2007b` / `R_2007` | Supported |
| `AC1024` | `R_2010b` / `R_2010` | Supported |
| `AC1027` | `R_2013b` / `R_2013` | Supported |
| `AC1032` | `R_2018b` / `R_2018` | Supported |
| `AC103-4` | `R_2022b` | Supported |

Pre-R13 header magic values are also version-detectable but are not current pure
stream support: `AC1.50`, `AC2.10`, `AC2.21`, `AC2.22`, `AC1001`, `AC1002`,
`AC1003`, `AC1004`, `AC1005`, `AC1006`, `AC1007`, `AC1008`, and `AC1009`.

The important distinction is that a successful blocking read elsewhere does not
count as stream support. A stream API call on an unsupported version must fail
clearly with `DWG_ERR_NOTYETSUPPORTED` instead of loading the whole file through
`dwg_read_file`.

The explicit missing pure-stream list is therefore:

```text
Pre-R13:
R_2_0b, R_2_0, R_2_10, R_2_21, R_2_22, R_2_4, R_2_5, R_2_6,
R_9, R_9c1, R_10, R_11b1, R_11b2, R_11/R_12
```

The explicit completed pure-stream list is:

```text
R_13b1, R_13b2, R_13, R_13c3,
R_14,
R_2000b, R_2000, R_2000i, R_2002,
R_2004a, R_2004b, R_2004c, R_2004,
R_2007a, R_2007b, R_2007,
R_2010b, R_2010,
R_2013b, R_2013,
R_2018b, R_2018,
R_2022b
```

Development targets from the current state to complete stream parity:

1. Add pre-R13 pure stream support if those blocking-reader versions are in
   scope for complete coverage.
   - Until a real pre-R13 stream reader exists, keep generated pre-R13
     unsupported tests returning `DWG_ERR_NOTYETSUPPORTED`.
2. Keep unsupported-version behavior explicit.
   - Supported pure stream versions must pass with or without
     `DWG_STREAM_F_NO_FULL_FALLBACK`.
   - Not-yet-supported versions must return `DWG_ERR_NOTYETSUPPORTED` with no
     object, decoded-object, or decode-error callbacks.
3. Measure memory on the large-file set after each new version family.
   - The pure stream path should keep the native side incremental.
   - Downstream aggregation is not part of this C-side acceptance check.

The project status is: C stream parity is real for
R13/R14/R2000/R2004/R2007/R2010/R2013/R2018/R2022b, but complete stream
coverage across every version enum is not done while pre-R13 remains open.

## 9. C-side work items

The stream implementation needs to expose DWG semantics that are currently only reliable in the full-load path.

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
