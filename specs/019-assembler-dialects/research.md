# Phase 0 Research: Merlin Assembler Dialect

**Feature**: `019-assembler-dialects` | **Date**: 2026-08-15 | **Spec**: [spec.md](./spec.md)

Every decision below is grounded in what the assembler looks like today, because
the mechanism has to be threaded through existing code rather than built beside
it. File and line references are to the tree at `868e9bc1`.

## Starting position

What already exists, and what each piece implies for the seam:

| Component | Today | Consequence |
|---|---|---|
| `Parser::ParseLine` | `static`, one hard-coded grammar; comment stripping, `label:` splitting, dotted-directive detection, and `NAME = expr` in a fixed order (`Parser.cpp:213`) | This *is* the dialect. It must become per-profile, and being `static` means it cannot hold profile state as written. |
| `Directive` enum | Shared token vocabulary; header already states "The name -> token table IS the assembler dialect… a second dialect is a second table" (`Directive.h:23`) | The intended seam is already documented. Follow it rather than inventing one. |
| `DirectiveTable::FromSpelling` | One global spelling table | Becomes per-profile; the token enum stays shared. |
| `Assembler` | Takes `const Microcode instructionSet[256]`, builds one `OpcodeTable` (`Assembler.h:25`) | A fixed instruction set. `XC` needs two, switchable mid-assembly. |
| `AssemblySession` | Holds `const OpcodeTable & m_opcodeTable` (`AssemblySession.h:321`) | A reference cannot be re-seated. Must become a pointer or an indirection. |
| `PendingLine` | Already carries `sourceFile` (`AssemblySession.h:44`) | FR-025 is mostly plumbing, not new tracking. |
| `AssemblyError` | `lineNumber` and `message` only (`AssemblerTypes.h:35`) | Needs `file` and `column`, defaulted. |
| `AssemblerOptions` | Already carries `fileReader` and `baseDir` | FR-006 lands here; the mock seam for multi-file corpus entries already exists. |
| `CommandLineParser` | `s_kSubcommands` table with one row (`run`), `LookUpSubcommand`, per-subcommand flag parsers | A new dialect is one row plus one arm, as the header promises. |
| `MockFileReader` | Nested inside `IncludeTests.cpp:40` | Needs promoting to a shared header for corpus use. |

## D1 — Shape of the dialect seam

**Decision**: An abstract `DialectProfile` class, one concrete subclass per
dialect in its own header/source pair, discovered through a
`DialectRegistry` table exposed by a `GetAllDialects()` accessor.

The profile is **mostly data with a few behavioral hooks**, not a wide virtual
interface:

- Data: comment introducers and their column rules, the label rule, the field
  model, the directive spelling table (span), the string-encoding table (span).
- Virtual hooks, only where behavior genuinely cannot be a table.

**As implemented, that is one hook** — parse a line into a `ParsedLine`. The four
sketched here before the extraction (field segmentation, local labels, variable
symbols, macro parameters) turned out to be internal to a profile that needs
them, not obligations on every profile; AS65 needs none. Virtuals get added when
a dialect proves it needs one, and Merlin is expected to add some.

**Rationale**: SC-009 requires a third profile to need no change to the shared
engine, evaluator, or opcode tables. A wide virtual interface makes each new
dialect a large implementation burden and invites shared-engine changes to
accommodate it; a mostly-data profile makes the common case declarative. Splitting
one class per file follows the project rule that anything with methods gets its own
pair. The `GetAll`-style accessor matches `DirectiveTable::GetAllSpellings` and
`CommandLineParser::GetAllSubcommands`, both of which exist so tests can sweep a
whole table rather than a sample.

**Alternatives considered**:

- *Pure data profile, no virtuals.* Rejected: Merlin's `]variables` and `<<<`
  macro invocation are not expressible as a table without inventing a
  mini-language, which is a worse abstraction than a virtual.
- *Function-pointer struct.* Rejected: equivalent power, but the project prefers
  classes over free functions, and a vtable is the idiomatic form here.
- *Templating the assembler on the dialect.* Rejected: forces the dialect to be a
  compile-time choice, which breaks FR-006 (runtime selection at every entry
  point) and would instantiate the whole engine per dialect.

## D2 — Directive vocabulary

**Decision**: Keep `Directive` as the shared token enum. Reuse an existing token
wherever the *operation* is identical, and add a token only for an operation the
assembler cannot already perform.

Reused: `DFB`→`Byte`, `DA`→`Word`, `PUT`/`USE`→`Include`, `ORG`→`Org`,
`EQU`/`=`→ existing constant handling, `DS`→`Ds`.

New tokens for new operations: reversed-order words (`DDB`), raw hex data
(`HEX`), the loop construct and its terminator, the dummy section and its
terminator, the CPU-selection directive, and the encoded-string family.

The encoded-string family is **one token plus an encoding mode**, not five
tokens. `ASC`, `DCI`, `INV`, `FLS`, and `STR` differ only in high-bit handling,
inversion, and terminator convention — all of which are parameters, not
operations.

**Rationale**: The enum is exhaustiveness-checked in a `switch`, which is stated
in `Directive.h` as the reason it is a token rather than a string. Adding five
near-identical tokens would multiply that switch for no behavioral gain and would
push five nearly identical rows into `AssemblySession`'s directive table.

**Alternatives considered**: One token per Merlin spelling. Rejected — it makes
the shared enum grow with every dialect, which is precisely the coupling SC-009
forbids.

## D3 — Two instruction sets, switchable mid-assembly

**Decision**: Introduce an instruction-set provider holding both the 6502 and
65C02 `OpcodeTable`s, and change `AssemblySession`'s member from
`const OpcodeTable &` to a pointer re-seated at a line boundary. `Assembler`'s
existing single-`Microcode` constructor stays, so present callers are unaffected.

**Rationale**: FR-003 and FR-015 require the in-source directive to take effect
for the remainder of the assembly, and the current fixed reference cannot express
that. Re-seating at a line boundary — never mid-line — keeps the two passes
agreeing about which table was active for a given line, which matters because pass
1 sizes instructions and pass 2 emits them.

**Open risk**: The active table must be recorded **per line** during pass 1 and
replayed in pass 2, not recomputed. Recomputing would work only if the directive's
position were identical in both passes, which conditional assembly can break: a
CPU-selection directive inside a conditional block whose taken-ness differs
between passes leaves pass 2 sizing instructions against a table pass 1 did not
bind them with. The symptom is a corpus byte mismatch arbitrarily far from its
cause, which makes this a design constraint rather than an optimization. Captured
as task T018 rather than left to discovery.

**Alternatives considered**: Building a merged table and validating legality
separately. Rejected — it duplicates the legality rule in two places and makes
"which opcodes exist" depend on a check rather than on the table, which is the
opposite of how the assembler works today.

## D4 — Diagnostic positions

**Decision**: Add `file` (empty by default) and `column` (0 by default) to
`AssemblyError`. Populate both from every diagnostic this feature emits. Route
`RecordError` and `RecordWarning` through the current `PendingLine`, which already
carries `sourceFile`.

**Rationale**: Defaults make the change additive, so the roughly 2,961 existing
tests keep passing and existing diagnostics keep compiling — the condition the
spec sets in FR-021. The include-file attribution defect (FR-025) is then a
plumbing fix rather than new bookkeeping, because the field it needs already
exists on `PendingLine`.

**Note**: `ReportAssemblyDiagnostics` in `CassoCli/CommandLine.cpp:300` prints
`ar.inputFile` for every diagnostic, which is the misattribution FR-025 names. It
must print the error's own `file` when set, falling back to the input path when
empty, so AS65's position-less diagnostics are unchanged.

## D5 — Command-line surface

**Decision**: One row in `s_kSubcommands` (`{ "merlin", Subcommand::Merlin }`),
one `Subcommand::Merlin` enumerator, one arm in `Parse`, and one
`ParseMerlinFlags`. A `dialect` field is added to `CommandLineOptions` and to
`AssemblerOptions`. An `as65` row is added alongside, giving AS65 the explicit
selector FR-001 requires, and the "unrecognized first argument is a source
filename" fallback is **removed** once that selector exists.

`--cpu` is rejected under `merlin` with a message naming `XC` (FR-026).

**Rationale**: The header at `CommandLineParser.h:22` states the table is data
precisely so a subcommand is a row plus a parser rather than a reshaped
dispatcher. Spec 020 is being developed concurrently against the same file, and
`UnitTest/CommandLineTests.cpp` pins current behavior, so everything **added**
here is additive. The fallback removal below is the deliberate exception, and it
is an exception to the additivity constraint rather than to the
don't-reshape-the-dispatcher one — the dispatcher keeps its shape.

**Decided, not deferred** (reversing an earlier deferral in this document):
the fallback heuristic is **removed in this feature**. It was deferred while AS65
had no explicit selector, because removing the only route to a dialect is not a
cleanup; adding `as65` removes that objection and the decision was taken
knowingly rather than by drift.

Two costs come with it and are accepted rather than discovered later. It breaks
the documented `CassoCli input.a65 -o out.bin` form, so it needs a prominent
`CHANGELOG.md` entry under breaking changes and an error that names the
replacement instead of printing usage. And it forces edits to
`UnitTest/CommandLineTests.cpp`, which spec 020 is developing against
concurrently — the change stops being purely additive at exactly that file, and
that session will need to rebase. Recorded on GitHub issue #92.

## D6 — Corpus fixtures and capture

**Decision (revised)**: Corpus entries are **committed fixtures** under
`UnitTest/Fixtures/Merlin/`, read through `IFixtureProvider::OpenFixture`.
Multi-file entries are served by a `MockFileReader` promoted out of
`IncludeTests.cpp` into its own shared header.

This replaces an earlier plan to compile source and bytes in as generated
literals, and it is better on every axis: the fixtures are the vendor's actual
files rather than a transcription of them, they need no capture step to run, and
`OpenFixture` is the project's **only** sanctioned path to fixture bytes —
`UnitTest/Fixtures/README.md` states the Constitution II contract plainly, that
there is no `std::ifstream` of host paths from test code and that anything
outside `UnitTest/Fixtures/` is a violation. A design reading `DevDisks/` would
have been reverted in review, whatever its merits.

**Capture is now only for ADDING a fixture**, not for running the suite. Tests
run on every build on every machine with no fetch step;
`scripts/ExtractMerlinFixtures.ps1` re-derives every fixture from the hash-pinned
disk, so the provenance chain from archive.org to the directory is re-runnable
end to end.

**Fixture format**: raw DOS 3.3 bytes. Skip the 4-byte BIN header, take the
length from bytes 2–3, mask bit 7, translate `$8D` to newline. Do **not** assert
bit 7 is set — `DCI` marks its terminator by clearing it, which this feature
established from the oracle. Compare objects from offset 4.

**A fixture is never edited to make a test pass.** The ND term forbids altered
copies, and a mismatch is a finding about Casso rather than about the file.

**Capture dependency**: none on `020-disk-file-access`. The Merlin Pro disk is a
flat DOS-order image, so sector *S* of track *T* sits at `((T * 16) + S) * 256`
with no nibble decoding — walking the VTOC, catalog chain, and track/sector list
is about forty lines of PowerShell. `scripts/ExtractDos33File.ps1` does exactly
that, validated against the DOS 3.3 System Master where `FID` extracts at its
canonical load address of `$0803`.

That script is **throwaway capture tooling**. It does not duplicate 020's
`disk get`, which is tested C++ spanning every mountable format including WOZ; it
is viable only because this one disk happens to be flat DOS-order, and it should
be deleted once 020's extraction makes it redundant.

Source goes in by typing or pasting into Merlin's editor. Paste is **not trusted**
— issue #110 reports the guest paste path garbling input — so each entry's source
is round-tripped: saved to the disk from within Merlin, extracted back, and
compared against what was intended. The answer to an unreliable channel is to
verify it, not to avoid it. Typing cost drops by batching many constructs into a
few composite source files, assembled once and split by known offsets.

Nothing at test time touches Merlin, a disk, or the emulator.

**Alternatives considered**: An on-disk corpus directory read by integration
tests. Rejected — it moves the corpus out of the unit-test tier for no benefit,
and the corpus is the primary correctness evidence for the whole feature.

## D7 — The subset boundary as data

**Decision**: One table in code, one row per refused construct, carrying the
spelling, the reason class (needs a linker / needs an unemulated CPU / owned by
another feature), and the explanation. Exposed by a `GetAll`-style accessor. The
help text describing where the subset ends is **generated from the table**.

**Rationale**: FR-019 requires implementation and documentation not to disagree.
Generating help from the table removes that possibility rather than detecting it,
and needs no test at all for that pair. A unit test then sweeps the accessor and
asserts each row produces a refusal, entirely in memory. Keeping prose
documentation in step is a repository-level check, because a unit test may not
read a file.

## The capture pipeline is proven, and needs no editor

There is a complete path with **zero editor interaction**, which sidesteps the
Add-mode problem entirely:

```
L  -> filename     load a source file already on the disk
E                  enter ED/ASM
ASM                assemble
Q                  back to the menu
O  -> filename     save object code
                   then extract with ExtractDos33File.ps1
```

Run end to end against `LABELS.S`. Merlin reported `--End assembly, 984 bytes,
Errors: 0` with a full symbol table (`LABTBL =$8000`, `END =$83D7`), the menu
showed `Object: A$8000,L$03D8`, and the extractor independently read back **load
`$8000`, 984 bytes**. Every number agrees.

**And the re-assembled object is byte-identical to the one the vendor shipped.**
Extracting `LABELS` from the pristine disk and comparing against the object
produced by re-assembling `LABELS.S` gives an exact match over all 984 bytes.
Merlin Pro running under Casso reproduces Glen Bredon's own 1984 output exactly.
That is the oracle validated as strongly as it can be.

*(The earlier "unchanged image hash" caveat is now **settled**, not carried.
Saving the object under its existing name leaves the image byte-identical,
because DOS 3.3 rewrites the same sectors with the same content. Saving the same
object under a **new** name changed the image hash and produced a second catalog
entry, also byte-identical to the shipped object. So the save writes; an
unchanged hash means an identical in-place write, not a no-op.)*

Two incidental facts about the object-save prompt, learned the hard way: typing
**replaces** the pre-filled filename rather than appending to it, and `$08` is not
a backspace here — sending it corrupts the prompt into a `SYNTAX ERROR`.

### DCI encoding, captured rather than read

`LABELS.S` is a table of `DCI` strings — one of the five string-encoding
directives, and the highest-risk area in the dialect. From the captured bytes:

```
DCI "0200IN"  ->  B0 B2 B0 B0 C9 4E
                  '0' '2' '0' '0' 'I' with the high bit SET
                                   'N' with the high bit CLEAR
```

So `DCI` sets the high bit on every character **except the last**, which
identifies the terminator convention empirically. Confirmed across the following
entries (`0100ST` → `... D3 D4`).

### Read the prompt, not the line counter

The mode signal is the **prompt**: `%` at the main menu, `:` in the editor's
command mode, a line number in Add mode. The line counter is *not* a mode
signal — a bare `RETURN` both adds a line and exits, so an incrementing counter
proves nothing. Several wrong state inferences earlier came from reading the
counter; the prompt was visible in the same screenshots all along.

## T021d, partially settled: does Merlin's editor normalize?

**Established.** Merlin's editor **tabs fields to fixed display columns**. Typing
`LABEL   LDA #$41` with three spaces renders `LDA` far beyond three spaces out,
at a tab stop. Typing a leading space with no label puts the opcode at a
different column again. So what appears on screen is not what was typed, and the
column positions in Merlin listings are the editor's doing — which is what
`FR-008`'s field-based line model already assumed, now with evidence.

**Not yet settled.** Whether the *stored bytes* are normalized, which is the part
that actually matters. Strong prior evidence says they are compact rather than
column-padded: the vendor's own `PI.ADD.S` on disk holds ` ADD SUMSTR;DEFLEN;PL`
with a single leading space, not padding out to the display column. But that is
inference from someone else's file, not a closed loop on one we wrote.

**Blocked on**: how to leave the editor's Add mode.

The input layer is **not** the problem, and that was checked before blaming the
editor. Posting `Ctrl-C` while an Applesoft `10 GOTO 10` loop runs produces
`BREAK IN 10`, so control characters reach the guest through the same
`WM_CHAR`-only path the script uses. `Ctrl-X` also demonstrably reaches *Merlin*:
it is the one input that does **not** advance the line counter, which matches the
manual's "cancels the current line being edited."

What still fails is the exit itself. The manual says Add mode ends "if a null
line is input (a line with no text and `<R>` is typed)". Tried and rejected, each
appended as another source line: `ESC` as `WM_CHAR`, `ESC` as `WM_KEYDOWN`, a
bare `RETURN`, `Ctrl-C`, `Ctrl-X`, and `Ctrl-X` followed by `RETURN`.

The manual is explicit on all three relevant points: Add mode exits when `RETURN`
is "the FIRST character of a line"; typing "a space and then RETURN" enters an
empty line and **deliberately bypasses the exit**; and "the cursor is
automatically tabbed one space to the right of the line number", which is cursor
positioning rather than a character in the buffer.

A bare `WM_CHAR 0x0D` sent as the only input after entering Add mode **did**
leave an empty source at what appeared to be the command prompt. The same input
after four typed lines did not reliably do so, and subsequent `D5,20` and `L`
commands were appended as source lines rather than executed.

**Stopping here rather than continuing.** Several successive inferences about
which mode the editor was in — drawn from the line-number indicator and a small
prompt glyph — turned out to be wrong, and firing further commands into a state
that cannot be read reliably produces garbage lines rather than information. The
line counter in particular is not the mode signal it appears to be: a bare
`RETURN` both adds a line and (sometimes) exits, so an incrementing counter
proves nothing either way.

What this actually needs is the ability to **read the editor's state** rather
than infer it from a screenshot — the same capability gap as issue #117, and the
same answer (#51 / #59). Failing that, it needs someone who can drive Merlin
interactively and report the exact working sequence.

Everything either side of this transition is proven: Merlin boots, the editor
accepts typed source with correct shifted characters, control characters reach
it, and extraction works.

## Vendor-source validation — the result, recorded here rather than committed

The oracle is **Merlin Pro 2.23**, archive.org item `MerlinProMacroAssembler`,
CC BY-NC-ND 3.0, Glen Bredon / Roger Wagner Publishing 1984, obtained via
`scripts/FetchMerlin.ps1` and extracted by `scripts/ExtractMerlinFixtures.ps1`
from a disk pinned at SHA-256
`CB7FD9522A3B90792ACBB00D6C811323DC046DC2920FC05A640858BFE611F0E6`. Every count
below is measured against that release; a different Merlin ships different
source files, so a figure from one is not comparable to another.

**The result is recorded here and the sources are not re-committed as corpus
entries.** The bytes already live under `UnitTest/Fixtures/Merlin/` under their
own license; copying a vendor source a second time into a test file would add
redistribution without adding evidence. What carries information is the
*outcome* of re-assembling them, which is this:

**All five absolute-mode vendor sources reproduce all six shipped objects, byte
for byte, with zero diagnostics each.** The sources are handed to
`Assembler::Assemble` exactly as the fixtures hold them — no line edited, no
whitespace tidied, no construct removed.

| Source | Object | Bytes | Load | Answers supplied |
|---|---|---:|---|---|
| `LABELS.S` | `LABELS` | 984 | `$8000` | *(none)* |
| `MAKE DUMP.S` | `MAKE DUMP` | 589 | `$9000` | *(none)* |
| `KEYMAC.S` | `KEYMAC` | 674 | `$9000` | `SAVOBJ`=0 |
| `PRINTFILER.S` | `PRINTFILER` | 286 | `$02A0` | `FORMAT`=1, `MONITOR`=0 |
| `CLOCK.S` | `CLOCK.24` | 365 | `$0240` | `SAVOBJ`=0, `VERSION`=24 |
| `CLOCK.S` | `CLOCK.12` | 365 | `$0240` | `SAVOBJ`=0, `VERSION`=12 |

Four properties are asserted beside the bytes, because bytes alone do not
establish them:

1. **The load address**, per object. `LABELS.S` names no origin anywhere, so a
   wrong dialect default yields 984 byte-perfect bytes in the wrong place —
   which reads as a far deeper problem than it is.
2. **The input was not touched.** Each entry holds a fixture *path* and never
   text, and the comparison re-derives the stored bytes straight from
   `IFixtureProvider::OpenFixture` and requires one character of assembled text
   per stored byte, in order. A re-indented line, a stripped trailing space or a
   deleted comment fails even where the emitted bytes are identical.
3. **Every entry discriminates.** The same source under AS65 must NOT reproduce
   the object. Labels, origin, literals and the evaluator are shared, so an
   entry passing under both dialects is no evidence the Merlin profile was
   consulted at all.
4. **`PRINTFILER`'s answer pair was recovered rather than assumed.** Which of
   its four `FORMAT`/`MONITOR` combinations the vendor built with is recorded
   nowhere; all four are assembled and *exactly one* is required to match. More
   than one would mean an answer reaches no byte and the search proves nothing.

The two `PI.*` sources are **negative specimens only** and are never compared
positively. They ship no object of their own, and the `PI.*` objects sitting
beside them on the disk are post-link. `PI.ADD.S` draws 7 boundary refusals
(one `REL`, six `ENT`), `PI.START.S` draws 5 (one `REL`, three `EXT`, one
`ENT`), and the two must not carry the same message — an export-only module can
be made to assemble absolutely, an importing one cannot.

**One gap is open and is measured, not estimated.** `PI.ADD.S` also draws nine
diagnostics with no boundary row behind them, which SC-003 defines as a defect.
They are one cause: line 123 is `VAR MSGPNT;OUTPUT`, Merlin's way of binding the
positional parameters `]1`..`]n` for a fragment pulled in by the `PUT` on the
next line. `VAR` is absent from Casso's Merlin directive table, so that line is
rejected and the eight `]1`/`]2` references inside `T.SENDMSG` have nothing to
resolve to. The count is asserted exactly, per file and corpus-wide, so it can
only change deliberately.

**The two macro libraries are not standalone sources**, which was measured
rather than assumed. `T.SENDMSG` is a macro *body*: its first line is an
instruction and its second writes to a positional parameter, so assembling it on
its own produces seven expression errors and means nothing. Both libraries are
exercised as the `PUT`/`USE` inclusion corpus instead — served under the names
the disk stores while a real vendor source asks for them under the short names
it writes, which is how Merlin's `T.` prefix rule is pinned from vendor lines
rather than from the manual.

## Open items carried into tasks

These are the requirements-checklist gaps that need a technical answer. Each
becomes a task rather than an implementation-time guess:

| Item | Question | Proposed resolution |
|---|---|---|
| CHK006 | Default dialect for in-process callers | AS65, matching today's behavior, so existing internal callers are unaffected |
| CHK007 | Output after a boundary refusal | No output; refusal is an error, and FR-018 requires all of them be reported first |
| CHK008 | Merlin symbol case sensitivity | Settle by capture — assemble mixed-case symbol references and compare |
| CHK009 | Symbol length limit and legal characters | Settle by capture |
| CHK010 | Unwritable output named by a directive | Ordinary I/O error from the CLI, not an assembler diagnostic |
| CHK036 | Recovery guidance in a boundary refusal | The refusal names the construct and the reason; no fix-it suggestion |
| CHK037 | Accepted and refused constructs in one assembly | Refusals are collected across the whole pass (FR-018), then the assembly fails |
| CHK041 | Unterminated dummy section, loop, or macro at EOF | Diagnostic naming the construct and its opening line, matching the existing unclosed-conditional pattern |
| CHK042 | Nesting and inclusion depth limits, self-inclusion | Reuse the existing `kMaxMacroDepth` / `kMaxIncludeDepth` limits |
| CHK046 | Performance expectations | None specific; the constitution's general responsiveness bar carries |
| CHK052 | Merlin operator set and precedence | Settle by capture — this is why the corpus floor requires evaluator entries |

Six of these ("settle by capture") are the reason the corpus is built before the
parser is finished, not after.
