# C blocking versus Stream parity status

This file is the short status record for LibreDWG C reading parity. Keep it in
sync with `target/README.md` whenever Stream routing, validation evidence, or
the remaining exact-version list changes.

## Scope

The comparison is entirely on the LibreDWG C side:

```text
dwg_read_file              blocking baseline
dwg_stream_file_ex         pure Stream candidate
```

ProtocolVNext, C# adapters, frontends, and other downstream consumers are not
part of this acceptance path. The Stream reader must not call or fall back to
`dwg_read_file`.

## What aligned means

A file has strict parity only when the blocking and Stream paths agree on the
applicable data and behavior checked by `test/unit-testing/stream_test.c`:

- detected source version;
- object, entity, and non-entity counts;
- decoded callback count and object classification;
- object types and per-object reference snapshots;
- relevant old-version table and block semantics;
- callback abort and invalid-version behavior;
- no critical Stream error or per-object decode error; and
- `full=0`, proving that the Stream path did not use full-reader fallback.

Passing these checks proves alignment for the tested files and semantics. It
does not prove that every possible DWG, corrupt input, or object type for a
version has been exhaustively covered.

## Current answer

Yes, blocking and pure Stream reading are aligned for real files in multiple
versions. R2.5 is a concrete standalone example: 16 independent DWGs from 1986
all pass strict reference parity, covering 5,014 objects, 4,601 entities, and
413 non-entities, with zero Stream decode errors and `full=0`.

Real-file strict parity evidence currently covers:

```text
R_1_1, R_1_2, R_1_4,
R_2_4, R_2_5, R_2_6, R_2_10,
R_9, R_10, R_11,
R_13, R_14,
R_2000, R_2004b, R_2004, R_2007, R_2010, R_2013, R_2018
```

Some evidence is external-only because the source archive does not provide a
license that permits importing its DWG binaries. The hashes, source media, and
results remain recorded so the evidence is reproducible without committing the
files.

Two Apache-2.0 Kaggle R2018 files add 9,670 strictly aligned objects. A broader
132-file Kaggle audit found no remaining modern development-version identifier.

Original AutoCAD 2007 media adds 372 clean unique AC1021/R2007 files and
713,309 strictly aligned objects. It exposed an OLEFRAME/OLE2FRAME Stream
classification defect that is fixed and covered by a generated regression.
The media contains no R2007a or R2007b file, so both beta gaps remain open.

A project-owned private business corpus adds 20 clean real DWGs and 6,347,881
strictly aligned objects across R2000, R2004, R2007, R2010, R2013, and R2018.
Every file reports zero decode errors and `full=0`. Identifying source details
and binaries remain private and are not committed; `result.md` records only the
filenames at the project owner's request.

One additional private R2004 file reports structural warning 0x40 and does not
produce a comparable blocking baseline in the strict harness. The project
owner classifies it as an abnormal input outside the supported business corpus,
so it is an explicit exclusion rather than an open Stream-parity task. Excluding
that file, the accepted project-owned corpus passes 20/20.

`result.md` records a fresh per-file benchmark of those 20 accepted inputs.
Across 1,012,448,531 bytes and 6,347,881 objects, the single-run warm-cache
totals were 46.152 seconds for blocking and 49.831 seconds for Stream. The
largest per-file peak working sets were 17,674 MiB for blocking and 2,680 MiB
for Stream. These measurements describe this machine and corpus, not a general
performance guarantee.

## Development priority

DWG 2000 and later is the current high-priority target. This is a development
gate, not merely a search preference. Work on real historical evidence must
proceed in this order:

1. `R_2000b`, `R_2004a`, `R_2004c`,
   `R_2007a`, `R_2007b`, `R_2010b`, `R_2013b`, and `R_2018b`;
2. remaining pre-2000 exact versions and the historical R12 evidence gap.

Do not move active development or evidence-search work to the second group
while any first-group item remains unresolved. A first-group version is
resolved only after an independently sourced real file passes strict C
blocking-versus-Stream parity with `full=0`, or after evidence for that exact
identifier is individually recorded and establishes that it was not a
historical public DWG format. The latter result must not be reported as
real-file parity. Pre-2000 gaps stay recorded so they become the active queue
after this modern gate is complete.

## Remaining exact-version evidence gaps

Every current legal `Dwg_Version_Type` has a pure Stream route. No exact
version lacks all validation: generated fixtures cover the versions below, but
real historical DWGs are still needed:

```text
R_1_3,
R_2_0b, R_2_0, R_2_21, R_2_22, R_9c1,
R_11b1, R_11b2,
R_13b1, R_13b2, R_13c3,
R_2000b,
R_2004a, R_2004c,
R_2007a, R_2007b,
R_2010b, R_2013b, R_2018b
```

Original AutoCAD 2000i and AutoCAD 2002 media contain 151 DWG paths reducing
to 76 unique files. All use the shared `AC1015`/R2000 on-disk format, and all 76
pass strict parity over 150,147 objects. `AC1016`/`R_2000i` and
`AC1017`/`R_2002` remain synthetic dispatch guards, not missing historical
product formats. An independently sourced historical R12 file remains
desirable because `R_12` aliases `R_11` in the enum and cannot be distinguished
by header bytes alone.

Original AutoCAD 2022 media contains 232 unique DWG/DWT files. All 27 current
files use `AC1032`/R2018-family format and pass strict parity over 7,893 objects;
none uses `AC103-4`. `R_2022b` therefore remains a synthetic dispatch guard,
not a missing historical product format.

Unknown headers return `DWG_ERR_INVALIDDWG` without callbacks or fallback. A
future known version without a Stream route must return
`DWG_ERR_NOTYETSUPPORTED` without callbacks or fallback.

## Evidence map

- `target/README.md`: authoritative target, TDD commands, detailed status, and
  development backlog.
- `test/unit-testing/stream_test.c`: executable strict parity contract.
- `target/internet-archive-autocad-media-audit.txt`: real R1.1, R1.2, R2.4,
  R2.5, R2.10, R11, and R13 historical-media evidence.
- `target/govdocs1-dwg-audit.txt`: independent real R14 evidence.
- `target/github-objectarx-ac402b-audit.txt`: external real R2004b strict
  parity evidence and its non-redistribution boundary.
- `target/internet-archive-autocad-2000i-2002-audit.txt`: original-product
  evidence that AutoCAD 2000i/2002 use AC1015, plus 76-file strict parity.
- `target/internet-archive-autocad-2007-media-audit.txt`: original AutoCAD 2007
  media classification, 372-file strict parity, and warning-file exclusions.
- `target/project-owned-dwg-corpus-audit.txt`: privacy-safe aggregate evidence
  for the 20-file, 6,347,881-object project-owned business corpus.
- `result.md`: Chinese per-file results with filenames, blocking/Stream elapsed
  time, peak working set, and object counts for the accepted 20-file corpus.
- `target/kaggle-dwg-audit.txt`: 132-file modern-version classification and two
  licensed R2018 strict parity results.
- `target/modern-beta-source-audit.txt`: exact screening rules and the
  2026-07-11 public catalog, web, and GitHub repository-index searches for the
  eight active modern gaps; no gap was closed by those searches.
- `target/internet-archive-autocad-2022-media-audit.txt`: original AutoCAD 2022
  media classification and 27-file strict parity.
- `target/internet-archive-dwg-search-audit.txt`: direct Archive.org DWG search
  boundary and classifications.
- `target/gnu-release-dwg-audit.txt`: signed GNU release archive evidence.
- `target/sourceforge-libdxfrw-sha256.txt`: libdxfrw release audit boundary.

## Maintenance rule

When evidence changes, update all three locations in the same commit:

1. this status file;
2. the exact status and gap sections in `target/README.md`; and
3. the short C stream status paragraph in the top-level `README`.
