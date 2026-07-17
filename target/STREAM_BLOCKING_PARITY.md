# C blocking versus Stream parity status

This is the concise status record for LibreDWG C Stream reading. Keep it in
sync with `target/README.md`, the top-level `README`, and
`target/MODERN_DEVELOPMENT_VERSION_POLICY.md`.

## Scope

The comparison stays entirely in LibreDWG C:

```text
dwg_read_file              blocking baseline
dwg_stream_file_ex         pure Stream candidate
```

ProtocolVNext and downstream consumers are outside this acceptance path. The
Stream APIs never call or fall back to `dwg_read_file`.

The product target is formal DWG release formats. These eight modern
development formats are recognized but deliberately unsupported:

```text
R_2000b, R_2004a, R_2004c, R_2007a, R_2007b,
R_2010b, R_2013b, R_2018b
```

Both Stream APIs return `DWG_ERR_NOTYETSUPPORTED` for them before invoking any
object, decoded-object, or decode-error callback. This result is independent of
`DWG_STREAM_F_NO_FULL_FALLBACK`. Unknown headers return
`DWG_ERR_INVALIDDWG`. See `target/MODERN_DEVELOPMENT_VERSION_POLICY.md`.

`R_2004b` is not excluded: an external real AC402b sample passes strict parity,
so its existing Stream support remains in scope.

## Supported result

Strict parity means matching version, object/entity classification, decoded
callbacks, object types, references, semantic snapshots, callback behavior,
zero unaccounted object decode errors, and `full=0`.

Real-file strict parity evidence covers:

```text
R_1_1, R_1_2, R_1_4,
R_2_4, R_2_5, R_2_6, R_2_10,
R_9, R_10, R_11,
R_13, R_14,
R_2000, R_2004b, R_2004, R_2007, R_2010, R_2013, R_2018
```

Generated fixtures additionally cover supported old/internal enum versions
where no redistributable historical file is present. That historical evidence
backlog does not mean the version lacks a Stream route and is not a blocker for
the formal modern-format product target.

## Project files

The accepted project-owned corpus contains 20 formal-version DWGs across
R2000, R2004, R2007, R2010, R2013, and R2018. All 6,347,881 objects pass strict
C blocking-versus-Stream parity, with zero Stream object decode errors and
`full=0`. `result.md` records every filename, per-file object counts, elapsed
time, and peak working set.

One additional private R2004 file reports structural warning 0x40. The project
owner classifies it as an abnormal input outside the supported corpus, so the
accepted business result is 20/20.

## Current acceptance result

The revised target is achieved:

- formal modern release formats in scope have pure Stream routes;
- the 20 accepted business files pass 20/20;
- the Stream path never falls back to blocking;
- all eight excluded modern development versions are detected and return
  `DWG_ERR_NOTYETSUPPORTED` with zero callbacks;
- unknown headers return `DWG_ERR_INVALIDDWG` with zero callbacks; and
- the default `stream_test` regression passes.

This does not claim exhaustive support for every corrupt DWG, custom class, or
object combination never represented by a test file.

## Evidence map

- `target/MODERN_DEVELOPMENT_VERSION_POLICY.md`: authoritative unsupported
  modern development-version policy and exact behavior.
- `test/unit-testing/stream_test.c`: executable parity and rejection-contract
  entry point. Its private implementation remains one compilation unit and is
  separated by maintenance responsibility into:
  `stream_test_statistics_and_callbacks.c`,
  `stream_test_api_and_file_parity.c`,
  `stream_test_r13_to_r2022.c`, and `stream_test_r1_to_r11.c`.
- `result.md`: Chinese 20-file business result and benchmark.
- `target/project-owned-dwg-corpus-audit.txt`: aggregate private corpus audit.
- `target/LARGE_R2004_3F_00.md`: separate large-R2004 result, decompression-cap
  history, Stream evidence, and blocking-reader safety boundary.
- `target/UPSTREAM_SYNC_STREAM_AUDIT_2026-07-17.md`: classification of the
  upstream 0.14.8447 fixes by shared or Stream-specific execution path.
- `target/modern-beta-source-audit.txt`: retained historical search and decision
  background; it is no longer a completion backlog.
- `target/internet-archive-autocad-2000i-2002-audit.txt`: AC1015 product-family
  evidence and 76-file parity.
- `target/internet-archive-autocad-2007-media-audit.txt`: 372-file R2007 parity.
- `target/internet-archive-autocad-2022-media-audit.txt`: AutoCAD 2022 product
  media uses the R2018-family format.

## Maintenance rule

When support policy or evidence changes, update the top-level `README`, this
file, `target/README.md`, and the policy file in the same commit.
