# Contract: Command-Line Surface

**Feature**: `019-assembler-dialects`

`CassoCli`'s grammar is a user-facing contract, and Principle III forbids changing
established behavior without notification. This document states exactly what is
added and what is guaranteed untouched.

## Guaranteed unchanged

These are pinned by `UnitTest/CommandLineTests.cpp`, which spec 020 is also
developing against. Breaking any of them breaks that session too.

- `CassoCli input.a65 -o out.bin` — an unrecognized first argument is a source
  filename, not an error. **The fallback stays.** Removing it is deferred to its
  own decision with a CHANGELOG entry; recorded on GitHub issue #92.
- `CassoCli run …` — untouched.
- Every existing AS65 flag, its spelling, its concatenation rules, and its
  defaults.
- Both flag prefixes: the parser records which one the user typed and echoes it
  back in usage text.
- Output written to stdout. A dialect banner is never printed there
  unconditionally, because `-l` with no filename sends the listing to stdout and
  build scripts pipe it.

## Added

### The `merlin` subcommand

```
CassoCli merlin <source> [options]
```

A bare word matching the `run` precedent, not a `--merlin` flag. Implemented as
one row in the subcommand table, one enumerator, one arm in `Parse`, and one
flag parser — the additive shape the table was made data for.

### Flags

| Flag | Behavior |
|---|---|
| `-o <file>` | Output path. **Takes precedence over an in-source object-file directive**, so a build script can override the source. |
| `-l[<file>]` | Listing. With no filename, goes to stdout, as today. The listing header states the active dialect. |
| `-v` | Verbose. The active dialect is reported on **stderr**. |
| `--cpu <target>` | **Refused.** Merlin selects its CPU in source; the message names that directive. |

`--cpu` is refused rather than ignored because a flag that is accepted and does
nothing is worse than one that errors — and because accepting extended opcodes
without the in-source directive would assemble source real Merlin rejects,
violating FR-005.

## Dialect reporting

| Situation | Where reported |
|---|---|
| Dialect stated by subcommand | Nowhere — the invocation is the record |
| Dialect inferred by fallback, `-v` given | stderr |
| Dialect inferred by fallback, listing produced | Listing header |
| Dialect inferred, neither `-v` nor a listing | Not reported; discoverable, not unconditionally emitted |

"Discoverable" means available without guessing. It does not mean always printed,
and it must never mean printed on stdout.

## Diagnostics

Format is unchanged for AS65:

```
<file>:<line>: error: <message>
```

Merlin diagnostics add a column, which editors parse the same way:

```
<file>:<line>:<column>: error: <message>
```

`<file>` is the file the diagnostic **originated in**, not the top-level input.
This corrects an existing defect: every diagnostic is currently attributed to the
input path, which misattributes errors inside included files. AS65 diagnostics
carry no file of their own and so continue to print the input path exactly as they
do today.

## Subset-boundary refusals

Distinguishable from syntax errors, and all of them reported in one pass rather
than stopping at the first — so the scale of the gap is visible at once. Each
names the construct and the reason, and never surfaces as an unknown-directive
error.

## Exit codes

`merlin` speaks the vocabulary the tool already uses, so a script driving it needs
no per-subcommand knowledge of what a number means.

| Code | Meaning |
|---|---|
| 0 | Clean — assembled, no complaints |
| 1 | Succeeded with complaints — assembled, but warnings were emitted |
| 2 | No output — assembly failed, including every subset-boundary refusal |

This matches the existing assembler path (`0` clean, `1` warned, `2` no output).
The `run` path extends the same vocabulary with `3` for an illegal opcode, which
has no analogue here.

A subset-boundary refusal exits **2**, not a distinct code. The refusal is
distinguished by its *message* (FR-017), which is where a developer reads the
difference; the exit code answers only "did I get an output file," which is what
a script needs. Spec `020-disk-file-access` has been given the same convention.

## Help

`CassoCli merlin --help` describes the dialect's own flags and states where the
supported subset ends. **That help text is generated from the subset-boundary
table**, so it cannot drift from what the assembler actually refuses.
