# Project-owned DWG blocking versus Stream results

Measurement date: 2026-07-11

## Conclusion

The accepted project-owned corpus contains 20 DWG files. All 20 files can be
read independently by the pure C Stream path and pass the strict comparison
against the C blocking reader:

- files: 20/20 passed;
- bytes: 1,012,448,531;
- objects: 6,347,881;
- entities: 5,410,837;
- non-entities: 937,044;
- Stream decoded objects: 6,347,881;
- Stream per-object decode errors: 0; and
- Stream full-reader fallback: 0 for every file.

The structurally abnormal private R2004 input that reports warning `0x40` is
not part of this result. The project owner excludes it from the supported
business corpus.

## Per-file result

Private filenames and paths are represented by stable IDs in sorted-path
order. They are intentionally not committed. Times are seconds and peak memory
is process peak working set in MiB.

| ID | Version | Size MiB | Blocking time | Blocking peak | Blocking objects | Stream time | Stream peak | Stream decoded | Entities | Non-entities | Errors |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| F01 | R2000 | 92.96 | 3.876 | 8,192 | 152,346 | 2.639 | 109 | 152,346 | 102,779 | 49,567 | 0 |
| F02 | R2004 | 3.02 | 0.138 | 104 | 80,530 | 0.527 | 18 | 80,530 | 72,868 | 7,662 | 0 |
| F03 | R2013 | 9.98 | 0.478 | 286 | 247,813 | 1.035 | 45 | 247,813 | 223,912 | 23,901 | 0 |
| F04 | R2004 | 23.97 | 4.047 | 5,085 | 478,534 | 4.479 | 299 | 478,534 | 312,149 | 166,385 | 0 |
| F05 | R2004 | 3.33 | 0.156 | 91 | 69,488 | 0.447 | 19 | 69,488 | 60,904 | 8,584 | 0 |
| F06 | R2004 | 5.84 | 0.218 | 140 | 91,559 | 0.840 | 31 | 91,559 | 54,661 | 36,898 | 0 |
| F07 | R2007 | 3.63 | 0.154 | 112 | 65,989 | 1.260 | 12 | 65,989 | 58,698 | 7,291 | 0 |
| F08 | R2007 | 8.50 | 0.405 | 269 | 174,505 | 3.006 | 19 | 174,505 | 134,457 | 40,048 | 0 |
| F09 | R2010 | 27.03 | 1.176 | 595 | 515,173 | 2.047 | 96 | 515,173 | 499,092 | 16,081 | 0 |
| F10 | R2010 | 27.79 | 1.198 | 626 | 492,855 | 2.043 | 97 | 492,855 | 473,695 | 19,160 | 0 |
| F11 | R2000 | 12.74 | 0.287 | 216 | 229,538 | 0.769 | 20 | 229,538 | 226,512 | 3,026 | 0 |
| F12 | R2007 | 17.75 | 0.358 | 183 | 73,926 | 1.437 | 25 | 73,926 | 72,174 | 1,752 | 0 |
| F13 | R2000 | 91.27 | 2.311 | 3,477 | 200,732 | 2.337 | 100 | 200,732 | 150,245 | 50,487 | 0 |
| F14 | R2000 | 178.10 | 6.582 | 11,272 | 462,872 | 5.392 | 189 | 462,872 | 252,155 | 210,717 | 0 |
| F15 | R2004 | 118.64 | 9.489 | 3,292 | 2,274,473 | 9.807 | 327 | 2,274,473 | 2,251,390 | 23,083 | 0 |
| F16 | R2000 | 112.63 | 5.213 | 11,329 | 100,149 | 2.579 | 841 | 100,149 | 35,025 | 65,124 | 0 |
| F17 | R2000 | 209.67 | 9.274 | 17,674 | 259,763 | 5.849 | 2,680 | 259,763 | 103,717 | 156,046 | 0 |
| F18 | R2007 | 4.63 | 0.190 | 146 | 89,034 | 1.796 | 15 | 89,034 | 77,824 | 11,210 | 0 |
| F19 | R2018 | 5.84 | 0.248 | 155 | 114,441 | 0.779 | 29 | 114,441 | 94,358 | 20,083 | 0 |
| F20 | R2018 | 8.22 | 0.354 | 238 | 174,161 | 0.761 | 40 | 174,161 | 154,222 | 19,939 | 0 |
| **Total** | **6 versions** | **965.55** | **46.152** | **17,674 max** | **6,347,881** | **49.831** | **2,680 max** | **6,347,881** | **5,410,837** | **937,044** | **0** |

## Version totals

| Version | Files | Objects | Entities | Non-entities |
| --- | ---: | ---: | ---: | ---: |
| R2000 | 6 | 1,405,400 | 870,433 | 534,967 |
| R2004 | 5 | 2,994,584 | 2,751,972 | 242,612 |
| R2007 | 4 | 403,454 | 343,153 | 60,301 |
| R2010 | 2 | 1,008,028 | 972,787 | 35,241 |
| R2013 | 1 | 247,813 | 223,912 | 23,901 |
| R2018 | 2 | 288,602 | 248,580 | 40,022 |
| **Total** | **20** | **6,347,881** | **5,410,837** | **937,044** |

## Measurement method

Each file was first read once with decoded Stream mode to warm the operating
system file cache. The measured blocking and Stream runs then used separate
fresh processes. Odd and even file IDs alternated which measured path ran
first. The blocking command was `dwgread -v0`; the Stream command was
`dwgprobe -d`, which calls `dwg_stream_file_ex`, decodes every object, requires
zero per-object decode errors, and reports its own object classification.

Elapsed values are wall-clock time from process start through exit. Peak memory
was sampled from Windows `PeakWorkingSet64` while each process was alive. These
are single-run measurements on this machine, suitable for comparing these 20
files under this build; they are not portable performance guarantees. Peak
memory is a per-file maximum and must not be summed across rows.

The blocking object counts shown above are the authoritative counts previously
captured by the strict blocking-versus-Stream harness. The measured Stream
counts reproduce those totals exactly. Correctness remains the acceptance gate;
the timing table shows that Stream may be faster or slower depending on the
file, while consistently using substantially less peak memory in this corpus.
