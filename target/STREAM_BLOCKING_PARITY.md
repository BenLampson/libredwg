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

## Development priority

DWG 2000 and later is the current high-priority target. Work on real historical
evidence must proceed in this order:

1. `R_2000b`, `R_2004a`, `R_2004c`,
   `R_2007a`, `R_2007b`, `R_2010b`, `R_2013b`, `R_2018b`, and `R_2022b`;
2. remaining pre-2000 exact versions and the historical R12 evidence gap.

Do not let searches for early AutoCAD media displace the first group. A version
leaves either group only after an independently sourced real file passes strict
C blocking-versus-Stream parity with no fallback.

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
R_2010b, R_2013b, R_2018b, R_2022b
```

Original AutoCAD 2000i and AutoCAD 2002 media contain 151 DWG paths reducing
to 76 unique files. All use the shared `AC1015`/R2000 on-disk format, and all 76
pass strict parity over 150,147 objects. `AC1016`/`R_2000i` and
`AC1017`/`R_2002` remain synthetic dispatch guards, not missing historical
product formats. An independently sourced historical R12 file remains
desirable because `R_12` aliases `R_11` in the enum and cannot be distinguished
by header bytes alone.

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
- `target/internet-archive-dwg-search-audit.txt`: direct Archive.org DWG search
  boundary and classifications.
- `target/gnu-release-dwg-audit.txt`: signed GNU release archive evidence.
- `target/sourceforge-libdxfrw-sha256.txt`: libdxfrw release audit boundary.

## Maintenance rule

When evidence changes, update all three locations in the same commit:

1. this status file;
2. the exact status and gap sections in `target/README.md`; and
3. the short C stream status paragraph in the top-level `README`.
