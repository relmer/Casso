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
`AssemblerOptions`. The "unrecognized first argument is a source filename"
fallback is **unchanged**, and sets the dialect to AS65 as an inference.

`--cpu` is rejected under `merlin` with a message naming `XC` (FR-026).

**Rationale**: The header at `CommandLineParser.h:22` states the table is data
precisely so a subcommand is a row plus a parser rather than a reshaped
dispatcher. Spec 020 is being developed concurrently against the same file, and
`UnitTest/CommandLineTests.cpp` pins current behavior, so the change must be
purely additive.

**Deferred, deliberately**: Removing the fallback heuristic. It breaks the
documented `CassoCli input.a65 -o out.bin` form and needs its own decision plus a
CHANGELOG entry. Recorded on GitHub issue #92.

## D6 — Corpus fixtures and capture

**Decision**: Corpus entries are **compiled-in**: source as string literals,
expected bytes as byte arrays, in a generated header under `UnitTest`. Multi-file
entries are served by a `MockFileReader` promoted out of `IncludeTests.cpp` into
its own shared header. A documented PowerShell capture script regenerates an
entry by running real Merlin 8 under Casso.

Each entry records the Merlin version it was captured from.

**Rationale**: Constitution II forbids unit tests reading disk. Compiled-in
fixtures satisfy that by construction rather than by discipline. The `FileReader`
seam already exists on `AssemblerOptions` for exactly the multi-file case.

**Capture dependency**: none on `020-disk-file-access`. The Merlin 8 disk is a
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

**Blocked on**: how to leave the editor's Add mode. `ESC` (as `WM_CHAR` and as
`WM_KEYDOWN`), a bare `RETURN` on an empty line, and `Ctrl-C` all fail to exit —
each is simply appended as another source line. Without exiting there is no way
to reach Save, and no file to extract and compare.

This wants the Merlin manual's editor command list rather than more guessing.
The rest of the capture path is proven: Merlin boots, the editor accepts typed
source, and extraction works.

**Verified incidentally**: the emulator's key path needs `WM_CHAR` for printable
characters (see `scripts/SendCassoKeys.ps1`), and control keys may need
`WM_KEYDOWN` — though for `ESC` neither form exits Add mode, so the blocker is
Merlin's command set rather than the input path.

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
