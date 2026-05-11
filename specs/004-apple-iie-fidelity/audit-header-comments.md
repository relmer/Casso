# T126 — Header function-comment audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T126
**Constitution**: §I 1.4.0 — function comments live ONLY in `.cpp`
**Audit date**: Phase 16

## Method

For every `.h` file changed in this feature (51 files), grep for the 80-`/`
banner pattern (`^/{60,}\s*$`). For each banner pair, classify the construct
that follows the banner:

- **Type-level** (allowed in headers): `class`, `struct`, `enum`, `namespace`,
  `template`, `union`, `using NAME = ...`, `static constexpr`, `extern const`,
  named constants block.
- **Function-level** (forbidden in headers per Constitution §I 1.4.0):
  any banner immediately preceding a function declaration.

## Result

| Category                                         | Count | Verdict |
|--------------------------------------------------|------:|---------|
| Banner introduces `class` / `struct` / `enum`    |    50 | OK      |
| Banner introduces `namespace`                    |     2 | OK      |
| Banner introduces `using` type alias             |     1 | OK      |
| Banner introduces named-constants block          |     1 | OK      |
| Banner introduces a function declaration         |     0 | OK      |

Note: the raw scanner reported 1 hit at
`CassoEmuCore/Core/IInterruptController.h:9` whose post-banner line is
`using IrqSourceId = uint8_t;`. This is a **type-alias declaration** and is
allowed in a header per the constitution (and is consistent with project
practice in `CassoCore/Cpu.h` etc.). The match was a false-positive caused
by a `\b` boundary subtlety in the scanner's classification regex; manual
inspection confirms 0 real violations.

## Verdict

**PASS** — 0 function-comment banners detected in changed `.h` files.
Function-level documentation in this feature lives exclusively in the
corresponding `.cpp` files.
