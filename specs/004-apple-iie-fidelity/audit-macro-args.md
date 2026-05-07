# T127 — Macro-argument audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T127
**Constitution**: §I 1.4.0 — EHM macros and other macros may only contain
trivial expressions (no function calls with side effects, allocations, or
out params). Store the result first, then pass to the macro.
**Audit date**: Phase 16

## Method

For every `.cpp` file changed in this feature (67 files), scan every line
**added or modified** by this feature (per `git diff master...HEAD --unified=0`)
for macro invocations of `CHR`, `CWR`, `CHRA`, `CWRA`, `CHRF`, `CWRF`, `CBR`,
`CBRA`, `CBRF`, `CBRN`, `CHRN`, `IGNORE_RETURN_VALUE`. For each invocation,
extract its complete argument list (across line continuations), strip string/
char literals, and search for `\b\w+\s*\(` patterns indicating a non-trivial
function call. Trivial method calls (`.size()`, `.empty()`, `.load()`,
`.data()`, `.c_str()`, `.length()`, `.count()`, `.begin()`, `.end()`,
`.front()`, `.back()`, `.at()`, `.find()`, `.contains()`, `.first/.second`,
`.flags()`, zero-arg calls, `wstring(...)` / `string(...)` constructors) are
excluded per project convention.

## Result

| Category                                                 | Count |
|----------------------------------------------------------|------:|
| Macro args containing non-trivial function calls         |     0 |

The raw scanner reported a single hit at `Casso/EmulatorShell.cpp:1418`
(`CHRN (hr, format (L"…", …).c_str())`). `git blame` confirms this line was
introduced before this feature branch (`commit 4523b667`, May 2026), and
`git diff master...HEAD` shows the line is **unchanged** (context-only) by
this feature. Per the audit's "every changed `.cpp` line" scope, this line
is **out of scope** for Phase 16. (It is a pre-existing pattern that also
appears at `Casso/Main.cpp:127`, `CassoEmuCore/Core/JsonParser.cpp:458`,
and `CassoEmuCore/Core/MachineConfig.cpp:42-409`. These are tracked
separately as pre-feature tech debt.)

## Verdict

**PASS** — 0 macro-arg violations on lines added or modified by this
feature. Where new EHM-bearing code was added (e.g. `DiskIIController`,
`AppleIIeMmu`, `WozLoader`, `DiskImageStore`, `NibblizationLayer`,
`InterruptController`, `Cpu6502`), every fallible call stores its result
into a local first and the local is then passed to the macro.
