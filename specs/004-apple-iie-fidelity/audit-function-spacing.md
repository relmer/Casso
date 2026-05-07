# T128 — Function-spacing audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T128
**Constitution**: §I 1.4.0 — `func()` (zero args, no space), `func (a, b)`
(args, space), `if (...)` / `for (...)` / `while (...)` / `switch (...)`
(keyword always followed by space).
**Audit date**: Phase 16

## Method

For every line added or modified by this feature in any `.cpp` or `.h`
(118 files, ~16,500 added lines), scan for the regex `\b[a-zA-Z_]\w*\(`
(word immediately followed by `(` with no space). Classify each match:

- `name()` (immediately followed by `)`) — zero-arg call: **OK**.
- `name(` followed by an argument — **violation** (call with args needs space).
- `if(` / `for(` / `while(` / `switch(` / `catch(` / `sizeof(` — **violation**
  (control-flow keyword needs space).

String/character literals and line-end comments are stripped before scanning.

## Result

| Category                                              | Count |
|-------------------------------------------------------|------:|
| Function calls with args missing space before `(`     |     0 |
| Control-flow keywords missing space before `(`        |     0 |

## Verdict

**PASS** — 0 spacing violations on lines added or modified by this feature.
Every new function definition, function call, and control-flow keyword in
this feature uses the project's spacing convention.
