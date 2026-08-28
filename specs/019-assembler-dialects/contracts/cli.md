# Contract: Command-Line Surface

**Feature**: `019-assembler-dialects`

`CassoCli`'s grammar is a user-facing contract, and Principle III forbids changing
established behavior without notification. This document states exactly what is
added and what is guaranteed untouched.

## Guaranteed unchanged

These are pinned by `UnitTest/CommandLineTests.cpp`, which spec 020 is also
developing against. Breaking any of them breaks that session too.

- `CassoCli run …`: untouched.
- Every existing AS65 flag, its spelling, its concatenation rules, and its
  defaults.
- Both flag prefixes: the parser records which one the user typed and echoes it
  back in usage text.
- Output written to stdout. A dialect banner is never printed there
  unconditionally, because `-l` with no filename sends the listing to stdout and
  build scripts pipe it.

## Added

### The `as65` subcommand

```
CassoCli as65 <source> [options]
```

FR-001 requires an **explicit** selection of AS65, and there was none, AS65 was
reachable only by inference, through the fallback heuristic. That is a gap in the
contract, not a stylistic one: without it, "the assembler accepts an explicit
dialect selection covering AS65 and Merlin" is half true.

It costs one table row and no new arm, because `Subcommand::As65` already exists
and is already where the fallback landed.

## Removed

### The unrecognized-first-argument fallback

`CassoCli input.a65 -o out.bin`, treating an unrecognized first argument as a
source filename, **is removed** (GitHub issue #92). The replacement is
`CassoCli as65 input.a65 -o out.bin`.

This is a **breaking change to the most common existing invocation**, taken as a
deliberate decision rather than as a side effect of adding dialects. Two things
follow from it, and neither is optional:

- **`UnitTest/CommandLineTests.cpp` changes.** It pins the fallback's behavior,
  and spec 020 is developing against that same file with 384 lines in flight
  across the shared command-line surface. **020 merges first**, then this lands,
  the session holding unmerged work does not resolve around the other's edit. The
  deeper reason is not conflict: 020 adds `disk` to `s_kSubcommands`, and that
  table is what decides which bare words reach the fallback, so removing it
  earlier would mean writing tests against an intermediate table about to move.
- **Deleting `BareWordThatIsNotASubcommand_StaysAs65` is intended.** 020 added it
  as a tripwire so a new table row could not erode the fallback incidentally, and
  so removal would have to be a deliberate act with its own CHANGELOG entry. This
  is that act; the commit message must say so, or it reads later as someone
  deleting an inconvenient test.
- **Adding `as65` and removing the fallback happen in ONE commit.** The table on
  master holds only `{ "run", Subcommand::Run }`, so `Subcommand::As65` is
  reachable solely through the fallback. Split across two commits, the tree has a
  midpoint where AS65 cannot be selected at all.
- **A prominent `CHANGELOG.md` entry**, under breaking changes rather than inside
  the feature announcement. A script that has invoked Casso this way for its whole
  life will stop working, and the error it gets must name the replacement rather
  than print usage, a bare "unknown argument" turns a one-line fix into a
  bisect.

The removal is only *possible* because `as65` now exists: the heuristic cannot be
retired while it is the sole route to AS65, which is why the selector and the
removal land in that order and not the reverse.

### The `merlin` subcommand

```
CassoCli merlin <source> [options]
```

A bare word matching the `run` precedent, not a `--merlin` flag. Implemented as
one row in the subcommand table, one enumerator, one arm in `Parse`, and one
flag parser, the additive shape the table was made data for.

### Flags

| Flag | Behavior |
|---|---|
| `-o <file>` | Output path. **Takes precedence over an in-source object-file directive**, so a build script can override the source. |
| `-l[<file>]` | Listing. With no filename, goes to stdout, as today. The listing header states the active dialect. |
| `-v` | Verbose. The active dialect is reported on **stderr**. |
| `-d <symbol>[=<value>]` | Answers a symbol the source asks for at the keyboard. Repeatable; a bare symbol answers 1. Same spelling as `as65`, because the value means the same thing in both. Without it a source using `KBD` cannot be assembled at all, while the diagnostic names this flag. |
| `--cpu <target>` | **Refused.** Merlin selects its CPU in source; the message names that directive. |

`--cpu` is refused rather than ignored because a flag that is accepted and does
nothing is worse than one that errors, and because accepting extended opcodes
without the in-source directive would assemble source real Merlin rejects,
violating FR-005.

## Dialect reporting

Both halves of SC-005, the dialect **and** the CPU target, are covered.

| Situation | Where reported |
|---|---|
| Dialect stated by subcommand | Nowhere, the invocation is the record |
| Dialect defaulted (caller set none), `-v` given | stderr |
| Dialect defaulted, listing produced | Listing header |
| Dialect defaulted, neither `-v` nor a listing | Not reported; discoverable, not unconditionally emitted |
| CPU stated by `--cpu` | Nowhere, the invocation is the record |
| CPU selected in source, `-v` given | stderr |
| CPU selected in source, listing produced | Listing header |
| CPU left at the dialect's default | Reported wherever the dialect is, so "no directive was seen" is not read as "the flag was ignored" |

"Discoverable" means available without guessing. It does not mean always printed,
and it must never mean printed on stdout.

The three "defaulted" rows survived the fallback's removal but changed what
triggers them. They no longer describe the CLI guessing from an unrecognized
argument; that path is gone, and via the command line the dialect is now always
stated. They describe a **caller** that set no dialect and took
`AssemblerOptions`' AS65 default, which FR-006 makes reachable from entry points
that are not the command line. Deleting the rows along with the heuristic would
have removed reporting from the one case where it is still the only way to know.

**The decision of what to report and when lives in core**, not in the executable.
`CassoCli` receives a string and prints it. Same for the exit code below and for
generated help text: the executable is an I/O edge, and every choice behind it must
be reachable from `UnitTest`.

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
than stopping at the first, so the scale of the gap is visible at once. Each
names the construct and the reason, and never surfaces as an unknown-directive
error.

## Exit codes

`merlin` speaks the vocabulary the tool already uses, so a script driving it needs
no per-subcommand knowledge of what a number means.

| Code | Meaning |
|---|---|
| 0 | Clean: assembled, no complaints |
| 1 | Succeeded with complaints: assembled, but warnings were emitted |
| 2 | No output: assembly failed, including every subset-boundary refusal |

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
