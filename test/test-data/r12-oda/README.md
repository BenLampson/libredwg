# ODA ACAD12 fixture

`Constraints.dwg` was generated from the repository's GPLv3+ fixture
`test/test-data/r12/Constraints.dxf` with ODA File Converter 27.1.0.0:

```text
ODAFileConverter <input-dir> <output-dir> ACAD12 DWG 0 1 Constraints.dxf
```

Input SHA256:
`0ADE46C5CC1FB50735FD88631C4536D9E976346B054FE77641EEF65136372E78`

Committed output SHA256:
`17A277EBB932AB4BA96C57EC0FCED2E99207664489E23898C56C15BA43FF5771`

The output begins with the shared R11/R12 `AC1009` magic. The explicit
`ACAD12` converter target supplies the R12 provenance; the file header alone
cannot distinguish R11 from R12. ODA output is not byte-for-byte deterministic,
so the committed file is the fixed regression artifact and tests compare its
decoded semantics rather than regenerating it.
