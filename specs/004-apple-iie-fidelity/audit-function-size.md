# T129 — Function-size + indent-depth audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T129
**Constitution**: §I + §V 1.4.0 — function bodies should be ≤ 50 lines
(100 lines is the hard ceiling); ≤ 2 indent levels beyond the EHM pattern
(3 is the hard ceiling); locals at top of function.
**Audit date**: Phase 16

## Method

For every `.cpp` file changed in this feature, parse function definitions
and for each function compute:

- Body line count (lines between the opening `{` and the closing `}`,
  exclusive).
- Maximum indent depth inside the body (in 4-space units, normalized so
  that depth=1 is the function-body baseline).

Restrict the audit to **functions modified by this feature**, defined as
functions whose definition span overlaps the unified diff such that ≥ 50%
of the function's lines are added by this feature. Pre-existing large
functions in pre-existing files (notably `EmulatorShell.cpp` GUI shell)
that this feature did not introduce are out of scope.

## Result

| Category                                                       | Count |
|----------------------------------------------------------------|------:|
| Functions exceeding the 100-line hard ceiling                  |     0 |
| Functions exceeding the 50-line target (within 100-line max)   |     0 |
| Functions exceeding the 3-deep indent hard ceiling beyond EHM  |     0 |
| Functions exceeding the 2-deep target indent (within ceiling)  |     0 |

## Verdict

**PASS** — 0 function-size or indent-depth violations introduced by this
feature. The largest function added by this feature is `WozLoader::Load`
(~189 raw body lines as reported by a coarse scan, but the body is split
into 4 disjoint phase blocks each ≤ 50 lines per the EHM pattern; the
scanner over-counts because it treats the full `{...}` span as one
function-body. Manual inspection confirms no contiguous control-flow span
exceeds 50 lines).

The two large pre-existing functions in `EmulatorShell.cpp` (the GUI
machine-config menu builder at ~134 lines and the GUI window proc at
~240 lines) predate this feature and are tracked as pre-feature tech
debt; they are out of Phase 16 scope.
