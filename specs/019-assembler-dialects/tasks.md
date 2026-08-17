---
description: "Task list for 019-assembler-dialects"
---

# Tasks: Merlin Assembler Dialect

## State of play

*Updated 2026-08-16. Keep this current or delete it — a stale status block is
read by whoever has no other way to check.*

**ALL SIX ORACLE OBJECTS NOW REPRODUCE BYTE FOR BYTE.** Five vendor sources,
handed to `Assembler::Assemble` exactly as they sit in the fixtures, zero
diagnostics each:

| Source | Object | Bytes | Load |
|---|---|---:|---|
| `LABELS.S` | `LABELS` | 984 | `$8000` |
| `MAKE DUMP.S` | `MAKE DUMP` | 589 | `$9000` |
| `KEYMAC.S` | `KEYMAC` | 674 | `$9000` |
| `PRINTFILER.S` | `PRINTFILER` | 286 | `$02A0` |
| `CLOCK.S` | `CLOCK.24` | 365 | `$0240` |
| `CLOCK.S` | `CLOCK.12` | 365 | `$0240` |

**Done.** Phases 1 and 2 complete: the dialect seam, diagnostic file/column
positions, and the switchable instruction set. In Phase 3, the Merlin profile
exists with its field-based line model (T027–T029), its directive table
(T032/T033), string encoding (T034/T035), local labels (T030), raw hexadecimal
data, equates, listing directives, instruction aliases, the directive behaviors
`LABELS.S` needs, **macros in full (T038)**, **variable symbols (T031)**, the
**emit-cursor split (T035e–T035g)**, and **the keyboard-input directive and the
four expression facts the last three oracles needed (T035h)**. **T069 and T070
are also done** — see the note on T069 for why its hold expired. Suite is
**3194** Release / **3197** Debug, both green; Dormann and Harte both pass. In
Phase 4, the conflict-free core subset is done: the **exit-code mapping (T078)**
and the **dialect-and-CPU reporting decision (T053/T053a/T053c)**.

## Reporting and exit codes — done in core, with nothing wired to the CLI yet

**T078 and T053/T053a/T053c were taken as a deliberately conflict-free slice.**
All four are new files: `CassoCore/AssemblerExitCode.{h,cpp}`,
`CassoCore/DialectReporting.{h,cpp}`, `UnitTest/AssemblerExitCodeTests.cpp` and
`UnitTest/DialectReportingTests.cpp`. Nothing under `CommandLineParser`,
`CommandLineOptions` or `CassoCli/CommandLine` was touched, because spec 020
holds unmerged work in exactly those files. The consequence is that **nothing
calls either of these yet** — T052 and T053b are the wiring, and until they land
the reporting and the exit-code vocabulary are reachable only from the tests.
That is the intended state, not an oversight.

**Provenance is a field on `AssemblerOptions`, not something derived.**
`DialectSelection` (`Stated` / `Defaulted`) lives in `CassoCore/AssemblerTypes.h`
beside `dialect` and defaults to `Defaulted`. Deriving it was not an option: AS65
is both a dialect a caller can state and the value a caller that stated nothing
ends up with, so the dialect alone cannot say which happened. The default is the
safe direction — a stated dialect that forgot to say so is merely over-reported,
where the reverse suppresses exactly the report the "defaulted" rows exist for.
A test constructs `AssemblerOptions` and touches nothing, so the default itself
is covered rather than assumed.

**`ReportSink::StandardOutput` exists and is never produced.** That is the point
of it. "A report never reaches stdout" is otherwise a property of code that can
be inspected but not asserted; with the enumerator present, a sweep over every
combination of dialect, provenance, CPU provenance, verbosity and listing asserts
that no report ever claims it. Routing either sink to stdout fails that sweep.
The listing header is deliberately NOT stdout even when the listing itself lands
there, because the header is part of the listing rather than a line beside it.

**The CPU target's NAME is supplied by the caller.** `CpuReport` carries a string
and a provenance; `DialectReporting` decides whether and where to say it and
composes the line. Core's assembler has no CPU-target vocabulary of its own —
instruction sets arrive as unnamed `Microcode` tables — so the alternatives were
inventing a second CPU enumeration in core to serve one report, or depending on
`CommandLineOptions::CpuTarget`, which would couple the assembler layer to the
command-line parse struct in a file spec 020 is editing. Asking for the CPU to be
reported without naming it is a caller bug and is rejected as one.

**A contradiction in [contracts/cli.md](./contracts/cli.md), resolved in favor of
the reading that can fire.** The reporting table's last row says a CPU left at
the dialect's default is "reported wherever the dialect is". Read literally --
only where the dialect itself is reported -- the row is unreachable: via the
command line the dialect is now always stated, so it would be reported nowhere,
and the row's own stated purpose ("so 'no directive was seen' is not read as 'the
flag was ignored'") could never be served. The row two above it settles the
question: a CPU selected in source is reported under `-v` even though the dialect
was stated, which only makes sense if the two axes are decided independently. So
"wherever the dialect is" means the same SINKS -- stderr under `-v`, the listing
header when a listing is produced, never stdout -- and not "only when the dialect
is also reported". Both halves are tested separately, so the choice is visible
rather than buried in an implementation.

**Macros and variables landed as ONE commit, deliberately.** Merlin writes a
positional parameter and a reassignable symbol with the same character — `]1` is
an argument and `]COUNT` a symbol, and the digit is the entire distinction — so
the two share one lexing decision and there is no ordering in which either is
correct alone.

## `MAKE DUMP.S` — REACHED, all 589 bytes

The second whole-file oracle now assembles **byte for byte**: 589 bytes at
`$9000`, zero diagnostics, whole file through `Assembler::Assemble`, with an
AS65 counterpart proving the comparison discriminates. Nothing was loosened;
the previous slice deliberately wrote no test at all rather than a weakened one,
and this is the test it was waiting for.

The census closed as follows. Every row below was a distinct diagnostic class
measured off the file, not estimated:

| Needs | Status |
|---|---|
| `BLT` / `BGE` instruction aliases | **done** |
| `HEX`, whose handler rows were null | **done** |
| Trailing hexadecimal bytes after a string operand | **done** — settled against the object |
| `TR` / `EXP` / `AST` listing directives, and `NAME = expr` equates | **done** |
| Macro parameters `]1`..`]n` with `;`-separated arguments | **done** (T038) |
| A parameter substituted INTO a symbol name — `LDX #A]1-ADRTBL` | **done** (T038) |
| Macro-body labels unique per expansion — `NI`, `ND`, `LP` each recur | **done** (T038) |
| **`ORG` moves the PC and not the output cursor** | **done** (T035e) — the spec amendment |
| A LABEL on an `ORG` line never binds — `HEREINT ORG INTRFACE` | **done** (T035f) |
| `ERR \expr`, the "does this fit below" form | **done** (T035g) |
| Bare `ORG` with no operand | **done** (T035e) |
| An operandless shift meaning accumulator mode — `LSR` | **done** (T035g) |

**A twelfth class the earlier census could not name, because it is not a
diagnostic.** `LDA #>HEREMAIN-1` assembled cleanly and produced the wrong byte:
in Merlin the selector after the immediate sigil picks a byte out of the WHOLE
expression, where the shared evaluator's `<` and `>` are prefix operators
binding to the term beside them. Both readings agree on the LOW byte of every
such pair, so half the evidence matches either way — it surfaced only as two
wrong bytes out of 589, at offsets 28 and 57. Fixed as a parse-time operand
rewrite in the profile.

**The census also under-counted the character-constant class.** `CMP #"N"`
reported as an expression error, which reads as one problem and is two: `"` is a
second character-constant spelling AND it means high ASCII, matching the
convention Merlin's string directives take from their delimiter.

**Two guards are deliberately uncovered and are recorded at the code**, in the
pattern this feature has used twice before. The implied-mode test in the
operandless-accumulator rule cannot be reached — no mnemonic in either table
carries an implied and an accumulator encoding — and `m_segmentOutputPos` cannot
be told from `m_segmentPC`, because segment directives are as65-only and as65's
two cursors never part. Both were mutated and neither was caught; both stay,
because each is what keeps the property true by construction.

**`LABELS.S` now assembles WHOLE-FILE to all 984 bytes of `LABELS`, at `$8000`,
through the real assembler.** Not through the encoder in isolation — the file is
handed to `Assembler::Assemble` exactly as it sits on the disk, and every line
has to be understood for the byte count alone to come out right. The comparison
lives in `UnitTest/MerlinCorpusTests.cpp` (`MerlinVendorOracleTests`), with a
companion asserting the same source under AS65 does **not** produce those bytes.

**Four things stood between 983 and 984, and only two were the expected ones.**

1. **`AssemblySession` never consulted the dialect at all.** It called the AS65
   `Parser::ParseLine` overload unconditionally, so `AssemblerOptions::dialect`
   reached nothing that mattered. The earlier 983-byte result had been measured
   through a hand-rolled loop over string lines in the test, not through the
   assembler. The session now resolves the profile once and reads every line
   through it, takes its origin from it, and evaluates with its operator binding.
2. **`Directive::StringData` had no handler in either pass** — its row was
   `{ nullptr, nullptr }` exactly like `ErrorIf`'s, so the 105 `DCI` lines
   emitted nothing. Pass 1 now sizes a string by *running the encoder and
   measuring* rather than by counting characters, so a mode carrying a length
   prefix cannot make the two passes disagree about where the next label binds.
3. **`ERR` got its behavior**, in **pass 2** so its expression may name a forward
   label — which is the point of such assertions. Pulled forward from the
   T036–T042 band deliberately, as planned.
4. **Merlin has NO operator precedence.** This was on nobody's list and is the
   sharpest find of the slice. `LABELS.S` ends with `ERR END-LABTBL-1/$700`,
   bounding its own table at seven pages. Under ordinary precedence the division
   binds first, the expression collapses to `END-LABTBL` = 983, and the assertion
   fires on a file the vendor shipped a working object for. Folded left to right
   it is `(END-LABTBL-1)/$700` = 0. That settles the last of the contract's
   unsettled questions — "Do Merlin's expression operators and precedence match
   the shared evaluator?" — from bytes rather than from the manual. The answer is
   **no**, on binding. *(The clause that followed — "the operator set itself is
   unchallenged so far" — was true of `LABELS.S` and false of the disk. `CLOCK.S`
   challenges it: `!` is exclusive-or and `.` is inclusive-or. See the `KBD`
   section above.)*

**`/` in expressions was already supported.** Measured before writing anything:
`TokType::Slash` and `TryApplyDiv` have been in the evaluator all along. The
state-of-play line naming it as a gap was reading a requirement as a hole.

**`ExpressionEvaluator.cpp` WAS modified, and legitimately.** SC-009 (T070) names
it as one of three files that adding a dialect must not touch, but that criterion
is evaluated against **T069's own commit**, not against `origin/master` — T070
says so itself. The change here is one dialect-neutral branch: when
`ExprContext::binding` is `LeftToRight`, every operator flattens to the loosest
level, so the recursion for the right operand can absorb nothing. The operator
set, the folds and the diagnostics are untouched, and AS65 is unaffected because
the field defaults to `ByPrecedence`. **This is not an SC-009 violation and must
not be recorded as one later.**

**One engine bug fixed on the way.** `ClassifyPrelude` recognized the origin
directive by comparing the canonical *spelling* against `".ORG"`. A dialect
spelling it without a dot parsed correctly, resolved to `Directive::Org`, and
then silently did nothing — output at the wrong address with no diagnostic. It
now compares the token. AS65 is unaffected: both its spellings already reported
the same canonical name.

**The three sibling comparisons are now closed**, and they were not three copies
of one thing. Each needed a different answer, and one of the differences is
worth carrying forward: *a spelling comparison is only convertible to a token
where a token exists.*

- `".END"` (struct closing) → `Directive::End`, a clean conversion. **No test
  can discriminate it**, and that is a property of the site rather than an
  omission: it is reachable only inside a `.STRUCT` body, `Directive::Struct` is
  an as65-only token, and as65 reaches `Directive::End` by both its spellings.
  Pinned by both-spellings tests plus a mutation check — pointing the comparison
  at the wrong token makes `StructTests` crash, which is how we know the site is
  covered at all.
- `".ENDM"` → `Directive::MacroEnd` **added alongside** the as65 route, not
  replacing it. as65 has no token here: `.ENDM` is absent from its spelling
  table entirely and parses as an unrecognized dotted directive that merely
  keeps its text. Merlin's `<<<` does have the token, and was being swallowed
  into the body it closed. Its as65 keyword is now profile data rather than a
  literal.
- `".LOCAL"` → **cannot** become a token comparison; there is no
  `Directive::Local` in any dialect, and adding one would tokenize the word on
  every line of every file to serve lines that appear only inside a macro body.
  Converted to profile-supplied data instead, at both sites (`CollectMacroBody`
  and `SubstituteMacroParams`). The hazard there is the mirror image of the
  others: a fixed comparison *deletes* a line another dialect's source merely
  begins with that word, and in a field-based dialect the first word is a label,
  so `LOCAL LDA #$42` lost its instruction and its label together.

`EXITM` in `CheckForExitm` is the same shape and is **not** converted — it was
outside this slice's brief. It is the last one.

**Debug was red before this slice and nobody had said so.** Three
`MerlinFixtureTests` drive the fixture decoder's asserting EHM rejections, which
`SetupForUnitTests` routes to `Assert::Fail`; they passed in Release, where the
assertions compile away. Fixed with `ExpectedEhmAssert`, production code
untouched. **Report both configurations, not just Release.**

## `KBD` — DONE, and it was never interactive input

**`KBD` is a DIRECTIVE, not a terminal read.** It binds the symbol in its label
field to an answer given to the assembly, and every artifact that described it as
"interactive keyboard input" or as a permanent barrier was wrong. The answer path
already existed: `AssemblerOptions::predefinedSymbols`, the map behind `-d`. It
had no implementation at all — zero matches for the spelling anywhere in
`CassoCore` — so what looked like a design problem was an unwritten table row and
a handler.

**No answer supplied is an ERROR**, naming the symbol and quoting the prompt. The
two easier outcomes are both silent failures of exactly the kind this feature
exists to avoid: blocking on a prompt hangs an unattended build, and defaulting
the answer assembles a *different program* cleanly, since the vendor sources gate
whole sections on these symbols.

**It DID get a `Directive` token**, and guarantee 2 admits it. The assembler
could not already require a value from outside the source and say which value was
missing — `-d` binds whatever it is handed and can say nothing about what a source
*needs*. The alternative considered and rejected was reusing the equate path with
a self-referential expression, which produces "undefined symbol" and loses the
prompt, i.e. exactly the information the source went to the trouble of writing.

**The label must NOT also bind to the program counter**, which is why the line is
claimed in the pass-1 prelude beside the origin directive rather than in content
dispatch. A `SAVOBJ` bound to `$0240` makes `DO SAVOBJ` true for any ordinary
origin, so every gated block assembles regardless of the answer.

**Four more things the three sources needed**, none of them `KBD` and every one
settled from shipped bytes rather than from the manual:

1. **Merlin's operator SET differs, not just its binding.** `!` is exclusive-or
   and `.` is inclusive-or. `LDX #HOURS/24!1` in `CLOCK.S` places the time
   editor's cursor and must be exclusive-or; `CMP #HOURS/24+3."0"` must be
   inclusive-or. The contract's line saying "the operator set itself is
   unchallenged so far" was true of `LABELS.S` and false of the disk.
2. **Merlin computes in unsigned 16-bit quantities.** `HOURS = VERSION-25/-1*12+12`
   with `ERR HOURS-VERSION` beneath it is an equality test written as arithmetic,
   and it only holds because `$FFF3 / $FFFF` is 0 while `$FFFF / $FFFF` is 1.
   Signed 32-bit reads the same line as -13 / -1 = 13 and fails the assembly.
3. **A variable symbol may stand as a program-counter label, repeatedly.** T031
   refused this deliberately; the refusal is lifted. `CLOCK.S` names eight
   separate `]LOOP` targets. Pass 2 rebinds as it walks, which the reassignable
   constant already needed for the same reason — and only a DATA test caught the
   pass-2 half, for the fourth time in this feature.
4. **`?` inside a symbol**, in both the label rule and the identifier lexer.

**A fifth is an engine correction rather than a Merlin fact.** T035f bound a
label sharing a line with an origin to the OUTPUT CURSOR. That agrees with the
right answer everywhere the two cursors are in step, which is everywhere `MAKE
DUMP` looks — and disagrees on a **bare** origin closing a relocated section,
which `CLOCK.S` has. `IRQEND ORG` closes a section relocated to `$BFC8`, and
`LDY #IRQEND-IRQHAND-1` is `$12` in the shipped object where the cursor reading
gives `$30`. The rule is now the plainest one available and has no dialect input
at all: **a label binds to the program counter as its line was reached**, exactly
like a label on any other line. as65's answer changed with it, and its test was
rewritten rather than special-cased.

**`&` against a character literal turned out not to be a gap.** The emit-cursor
slice listed it beside `?` as unbuilt. Measured: `#"Q"&$9F` and `$9F&"N"` already
worked — `&` was always bitwise-and and the high-ASCII delimiter already landed.
What actually broke on those lines was the OPERAND SCANNER: a character constant
may hold a space (`LDA #" "`), and a whitespace-delimited scan kept `#"` and
handed the rest to the comment field.

**`PRINTFILER.S` identified the vendor's own build configuration.** Its two
answers are semantic and which pair produced the shipped 286 bytes is recorded
nowhere; all four were tried and exactly one matches — **formatting on,
monitoring off**. The test asserts the COUNT as well as the pair, because more
than one match would mean an answer reaches no byte.

**The corpus caught a macro bug every synthetic test had missed.** `KEYMAC.S`
closes a macro with `NI <<<` — the label the body branches to sits on the line
that *closes the definition*, which is precisely the line closing a body throws
away. Twelve hand-written macro tests were green before the vendor source was
tried. That is the third demonstration in this feature that synthetic tests and
the corpus cover different sets, and this time it was the corpus's turn.

**`BLT`/`BGE` are done, and are dialect-scoped DATA rather than a branch.** The
profile supplies a spelling table and `Parser::ParseLine` rewrites the mnemonic
on the way out, so nothing downstream ever sees the alternate name. The
alternative — teaching the opcode lookup, the size estimator, the branch-range
check and the encoder each about a second spelling — is a per-dialect special
case in four places in shared mechanism for two words, which is what
`contracts/dialect-profile.md` guarantee 3 forbids and what SC-009/T069 exists
to catch. as65 declares no aliases and is swept to prove it.

**The other three oracle objects did not come along with them** at the time, but
they have since: `PRINTFILER` and `CLOCK` also needed the keyboard-input
directive, and both use macros. The aliases unblocked those objects without
delivering any of them.

**SC-004 re-verified after the engine change.** `AssemblerTests`,
`RegressionTests`, `IntegrationTests` and `OutputFormatTests` — 180 tests
between them — are green in both configurations, and the split was mutated
eleven ways to check they would have noticed: freezing the output cursor alone
fails twelve of them.

**Blocked on someone else.** T049 (explicit `as65` selector + fallback removal)
is **held until spec 020's command-line work reaches `master`** — see
[docs/coordination.md](../../docs/coordination.md). Nothing else is blocked.

**Carrying a known incompleteness.** Every new `Directive` token has a row in
`AssemblySession`'s handler table, but several handlers are still null. Null
means *not implemented yet*, not *does nothing*; they are unreachable while as65
is the only selectable dialect, and T036–T042 fill them. `StringData`, `ErrorIf`
and `HexData` are now filled, and `MacroDef`/`MacroEnd` act through the
collection state rather than through their rows; `WordHighFirst`,
`Loop`/`LoopEnd`, `DummySection`/`DummySectionEnd`, `CpuSelect` and `ObjectFile`
are not. **`KeyboardInput`'s rows are null for the third reason**, the one `Org`
already used: it acts entirely in the pass-1 prelude, before a label can bind,
so a row would never be reached. Three meanings for one null is worth watching,
and each is stated at its own rows.

**Two guards in the macro work are deliberately uncovered, and are recorded at
the code rather than left to be rediscovered.** The digit test that separates
`]1` from `]COUNT` cannot be reached by any test: a macro body is stored as raw
text and re-parsed only after substitution, so a parameter reference never
reaches the profile's rewriter on a line whose parse is used. The bracket-depth
clamp in `Parser::SplitOnSeparator` is the same shape — variable references are
rewritten before an argument list reaches the splitter, so the clamp and its
absence are indistinguishable today. Both were mutated and neither was caught;
both stay, because each is what keeps the property true by construction rather
than by ordering luck.

**`PMC` is NOT implemented.** It is documented as the word form of the explicit
invocation prefix, and nothing on the disk uses either. Adding a second
unverified spelling on the strength of the same absent evidence was not worth
it; the prefix form covers the construct and the gap is one table row when
someone has a source that needs it.

**The refused string form is SETTLED, and by the object rather than by
reasoning.** `ASC "TEXT"8D` — digits after the closing delimiter — is a run of
hexadecimal bytes appended verbatim. `MAKE DUMP` carries
`ASC "This destroys current source."8D8D` and
`ASC "Do you really want it (Y/N)? "00`, and its object holds the high-ASCII
text followed by `8D 8D` and then `00`. The second half is the part no reasoning
would have produced: the trailing run does **not** take the delimiter's
high-bit convention the way the text does, which `00` staying `00` proves. So
this no longer waits on `KEYMAC.S`, and there is no refusal left for T046b's
sweep to account for. Still unverified: a comma-separated form (no vendor line
uses one) and a trailing run after `DCI` (every one on the disk follows `ASC`).

**Evidence gaps that capture must close, not reasoning.** Three of the six string
encodings have no oracle: `INV` appears once in a linker demo that ships no
object, and `FLS` and `STR` appear nowhere in the corpus. The apostrophe half of
the delimiter rule is likewise unverified. Each is marked `UNVERIFIED` at its own
line in `CassoCore/StringEncoding.cpp`.

**Input**: Design documents from `/specs/019-assembler-dialects/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/)

**Tests**: Test tasks ARE included. Constitution Principle II requires unit tests for all production code, and the spec's own success criteria (SC-001, SC-009) are stated as tests.

**Organization**: Grouped by user story so each is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story the task belongs to (US1, US2, US3)
- Exact file paths are included in every task

Tasks added after generation sit in their **execution** position rather than at
the end, so the file still reads in the order the work happens. They carry either
the next free number (T078, T079) or a letter suffix where they belong beside an
existing task (T033a, T053a, T062a). Existing IDs are never renumbered, because
the phase notes and the dependency graph reference them.

## Path Conventions

Paths are repository-relative and follow the structure in [plan.md](./plan.md):
`CassoCore/` (all new logic), `CassoCli/` (formatting edge only), `UnitTest/`,
`scripts/`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Test scaffolding that later phases fill in.

- [x] T001 [P] Promote `MockFileReader` out of `UnitTest/IncludeTests.cpp` into `UnitTest/MockFileReader.h`, register in `UnitTest.vcxproj`, and update `UnitTest/IncludeTests.cpp` to include it instead of defining it
- [x] T002a Create `scripts/ExtractDos33File.ps1` — catalog walk and file extraction from a flat DOS-order image, stripping the DOS BIN header. **Throwaway capture tooling, not a product feature**: it exists only because the Merlin disk happens to be flat DOS-order, and it does not duplicate `020-disk-file-access`'s `disk get`, which is tested C++ spanning every mountable format including WOZ. Delete it if 020's extraction lands first. *(Validated against the DOS 3.3 System Master: FID extracts at load `$0803`, CHAIN at `$0208` / 453 bytes with the stripped payload matching the raw sectors after the 4-byte header.)*
- [x] T002 [P] Create `UnitTest/MerlinCorpus/README.md` documenting the capture procedure end to end: source goes in by typing or pasting into Merlin's editor, and bytes come back out via `scripts/ExtractDos33File.ps1`. Record the Merlin-version-per-entry rule. *(Two premises here are obsolete and the README must be corrected, not just extended: the developer no longer supplies their own image — `UnitTest/Fixtures/Disks/Merlin-proDos2.23.dsk` is committed — and "the disk image is never committed" is now false. Capture is for **adding** a fixture; the five vendor oracles need none of it.)*
- [x] T002b [P] Create `scripts/FetchMerlin.ps1` — retrieve the Merlin Pro volumes from the archive item and verify each against a pinned SHA-256. Recorded after the fact: the script was written during capture and had no task, which is how the provenance chain came to be re-runnable without being planned. It stays useful now that the volumes are committed, because it is what makes "these bytes are the archive's bytes" checkable rather than asserted
- [x] T002c [P] Create `scripts/ExtractMerlinFixtures.ps1` — lift the vendor source and object files off the hash-pinned image into `UnitTest/Fixtures/Merlin/`. Also recorded after the fact. This is the **only** sanctioned way to add a Merlin fixture; nothing in the test suite runs it, and the fixtures it produces must never be edited afterward
- [x] T003 [P] Create `scripts/CaptureMerlinCorpus.ps1` skeleton with `-Entry` and `-MerlinImage` parameters and usage text, calling `ExtractDos33File.ps1` for the read-back half

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The dialect seam, diagnostic positions, and the switchable instruction set. Nothing story-specific can begin until these land.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Seam extraction — must change no behavior

- [x] T004 Create `CassoCore/Dialect.h` with the `DialectId` enum and register it in `CassoCore.vcxproj`. *(Ships with `As65` only. `Merlin` was initially listed and the registry sweep failed on its first run, correctly: an enumerator with no profile resolves to the wrong profile while looking like support that exists. The enumerator lands with the profile in T027.)*
- [x] T005 Create `CassoCore/DialectProfile.h` declaring the abstract seam — identity, CPU-selection source, and `ParseLine` — and register it in `CassoCore.vcxproj`. *(Built with **one** virtual, not the four originally sketched: extraction showed line parsing is the honest boundary and the other three are internal to a profile that needs them. Virtuals get added when a dialect proves it needs one — see [data-model.md](./data-model.md).)*
- [x] T006 Create `CassoCore/As65Dialect.h` / `CassoCore/As65Dialect.cpp` holding today's grammar moved verbatim from `Parser::ParseLine`, and register both in `CassoCore.vcxproj`. **The AS65 directive spelling table does not move**: `DirectiveTable` keeps its global table and `GetAllSpellings()` accessor, and the profile delegates to them. Moving it would change `UnitTest/DirectiveTokenTests.cpp:70`, which sweeps that accessor — and T010 forbids exactly that
- [x] T007 Create `CassoCore/DialectRegistry.h` / `CassoCore/DialectRegistry.cpp` with the name-to-profile table and a `GetAllDialects()` accessor matching the `DirectiveTable::GetAllSpellings` pattern, and register both in `CassoCore.vcxproj`
- [x] T008 Route `Parser::ParseLine` through the active profile in `CassoCore/Parser.cpp` and `CassoCore/Parser.h`, moving the file-scope `StripComments` helper into the profile
- [x] T009 Add `dialect` to `AssemblerOptions` in `CassoCore/AssemblerTypes.h`, defaulting to `DialectId::As65` so every existing caller is unaffected
- [x] T010 Verify the seam changed nothing, **before any new test file is added**: full suite green in `x64\Release` AND `git diff --stat origin/master -- UnitTest/` shows no *existing* test file modified. Adding new files is expected later and does not violate this gate; editing one that already existed does, and means behavior moved with the code — stop and find out what
- [x] T011 [P] Add `DialectRegistry` sweep tests to `UnitTest/DialectMechanismTests.cpp` asserting every `DialectId` enumerator resolves to a profile, and register the file in `UnitTest.vcxproj`. Runs **after** T010, since it adds a file under `UnitTest/`

### Diagnostic positions

- [x] T012 Add `file` (default empty) and `column` (default 0) to `AssemblyError` in `CassoCore/AssemblerTypes.h`
- [x] T013 Route `RecordError` and `RecordWarning` through the current `PendingLine`'s `sourceFile` in `CassoCore/AssemblySession.cpp`. **Populate the position where the error is CREATED, not where it is reported** — that is the whole difficulty. Extending the error record is trivial; include attribution is only correct if the originating file is captured at the point of failure, since by reporting time the only file in hand is the top-level input. The value must also survive the trip out of core to reach the reporting site in the executable
- [x] T014 Make `ReportAssemblyDiagnostics` in `CassoCli/CommandLine.cpp` print the error's own `file` when set, falling back to the input path when empty so AS65 diagnostics are byte-for-byte unchanged
- [x] T015 [P] Add tests to `UnitTest/MerlinDiagnosticTests.cpp` proving a diagnostic raised inside an included file names that file rather than the top-level input, and register the file in `UnitTest.vcxproj`

### Switchable instruction set

- [x] T016 Create `CassoCore/InstructionSetProvider.h` / `.cpp` holding both the 6502 and 65C02 `OpcodeTable`s with an active selection, and register both in `CassoCore.vcxproj`
- [x] T017 Change `AssemblySession`'s `const OpcodeTable & m_opcodeTable` to a re-seatable pointer in `CassoCore/AssemblySession.h`, keeping `Assembler`'s existing single-`Microcode` constructor working
- [x] T018 Record the active instruction table **per line** during pass 1 and replay it in pass 2 in `CassoCore/AssemblySession.cpp` — never recompute, because conditional assembly can move where the directive is reached
- [x] T019 Verify SC-004: full suite green, with `UnitTest/AssemblerTests.cpp`, `RegressionTests.cpp`, `IntegrationTests.cpp`, and `OutputFormatTests.cpp` confirming AS65 output bytes are unchanged

**Checkpoint**: Seam in place, diagnostics carry position, both instruction tables held. User story work can begin.

---

## Phase 3: User Story 1 - Assemble existing Merlin source unmodified (Priority: P1) 🎯 MVP

**Goal**: A developer points Casso at unmodified Merlin source and gets the bytes Merlin produces.

**Independent test**: `UnitTest/MerlinCorpusTests.cpp` assembles every corpus entry and compares byte-for-byte against bytes captured from real Merlin Pro. Nothing reads a file or invokes another assembler.

### Corpus first — these settle open questions the parser depends on

**⚠️ The five vendor oracles no longer need capturing.** They are committed under
`UnitTest/Fixtures/Merlin/` and read through `IFixtureProvider::OpenFixture`, the
project's only sanctioned path to fixture bytes — `UnitTest/Fixtures/README.md`
states the Constitution II contract: no `std::ifstream` of host paths from test
code, and anything outside `UnitTest/Fixtures/` is a violation. **No test may read
`DevDisks/`.** Capture tooling exists only to *add* a fixture, via
`scripts/ExtractMerlinFixtures.ps1` against the hash-pinned disk.

Fixture format: raw DOS 3.3 bytes — skip the 4-byte BIN header, take the length
from bytes 2–3, mask bit 7, translate `$8D` to newline, compare objects from
offset 4. Do **not** assert bit 7 is set; `DCI` clears it on its terminator.
Never edit a fixture to make a test pass.

That reorders this phase: the byte-comparison work (T045-series) is unblocked
**now** and no longer waits on capture, while only the settle-by-capture entries
(T022–T025f), which need source authored here, still want the editor.

- [x] T020 [US1] Define the `CorpusEntry` shape from [data-model.md](./data-model.md) and the comparison harness in `UnitTest/MerlinCorpusTests.cpp`, serving multi-source entries through `UnitTest/MockFileReader.h`, and register the file in `UnitTest.vcxproj`. *(Comparison logic and its ten self-tests are done. The entry source changes: bytes now come from `IFixtureProvider::OpenFixture`, not from generated literals — see T020e.)*
- [x] T020f [US1] Add a **type-T** read path to `UnitTest/MerlinCorpus/MerlinFixture.h` / `.cpp`. DOS 3.3 gives a text file **no header at all** — `T.SENDMSG` begins with the literal characters `SE`, and its first four bytes read as a header claiming 50382 bytes of a 149-byte file. Kept as a separate entry point rather than sniffed from the bytes, because guessing the file type is the kind of inference that succeeds on the sample and fails on the next file; the type-B length check turns a wrong choice into a loud failure instead of text quietly missing its first four characters. Arrived with the two vendor macro libraries (`T.PI.MACS`, `T.SENDMSG`) that T045f wants- [x] T020e [US1] Read corpus bytes through `IFixtureProvider::OpenFixture` (e.g. `OpenFixture("Merlin/LABELS.S")`) and add a fixture-decoding helper covering the DOS 3.3 BIN convention once rather than per entry: skip the 4-byte header, read the length from bytes 2–3, mask bit 7 for source text, translate `$8D` to newline, compare objects from offset 4. It must **not** assert bit 7 is set — `DCI` clears it on the terminating character, which is exactly the encoding this corpus exists to pin. *(Landed as `UnitTest/MerlinCorpus/MerlinFixture.h` / `.cpp` with seven tests. Two findings from the fixtures themselves. The high-bit prohibition turned out to have a **second and much earlier** reason than `DCI`: Merlin stores source as high-bit ASCII **except spaces, which are plain `$20`** — 81 of them in `LABELS.S` alone — so a decoder asserting bit 7 would fail on the first space of the first line, long before reaching any `DCI` terminator. And the declared length is **verified** against the payload rather than skipped past, since all 13 committed fixtures carry an exact match, making the strict form free; a truncated or sector-padded extraction otherwise decodes into plausible bytes, which is the failure this corpus exists to catch.)*
- [ ] T020d [US1] Assert a **non-zero entry count** in the corpus sweep, and assert the count against the corpus floor once the floor is met. This is the half of the absent-corpus guard T020a could not land, since counting needs the entry table T020e introduces; it is not a duplicate of it. Lands with the first real entry rather than now, because asserting it against an empty corpus would leave a permanently red test in the suite — which masks other failures and is its own version of a signal nobody reads
- [x] T020a [US1] Make the corpus harness **fail when the corpus is absent**, not pass. A loop over an empty entry table reports success while covering nothing, which is the same failure shape as a stale test assembly and as an integration test whose data cannot be reached — success reported, coverage absent. Make an entry with empty expected bytes an error rather than a trivially satisfied comparison, and make two empty vectors comparing equal an error too — that is the worst case, since a naive comparison calls it a match. *(Done, and **only** that half. The entry-**count** assertions this task originally also claimed cannot exist yet: there is no entry table to count until T020e supplies one, so they are T020d's and the checkbox here covers the empty-expectation guards alone.)*
- [ ] T020b [US1] *(Flag and rationale are in place on `CorpusEntry`; the assertion lands with the first real entry, since it needs an assembly to run. **The shape is now proven** — `LabelsSourceUnderAs65DoesNotProduceMerlinsBytes` assembles the vendor fixture under both dialects and requires the results to differ. What remains is wiring that check into the entry-table sweep, which still needs the entry table.)* Add a `discriminates` flag to `CorpusEntry` and have the harness in `UnitTest/MerlinCorpusTests.cpp` assert every entry carrying it **fails under the AS65 profile** as well as matching under Merlin. This closes the second vacuity shape: labels, origin, literals, and the evaluator are shared, so an entry built from those alone is green whether the Merlin profile works or is never consulted. An entry that passes under both dialects while claiming a Merlin construct is a defect either way — it is not exercising what it claims, or the profile is not being consulted. Shared-construct entries leave the flag clear and stay legitimate engine regression cover
- [ ] T020c [US1] Set `discriminates` on every settle-by-capture and Merlin-construct entry as it is captured (T022–T025f, T043–T045), so the classification is recorded with the entry rather than reconstructed later
- [ ] T021 [US1] Implement `scripts/CaptureMerlinCorpus.ps1` to assemble one entry under real Merlin Pro in Casso and emit source, bytes, and Merlin version as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), reading bytes back with `scripts/ExtractDos33File.ps1`
- [ ] T021c [US1] Make **delete-before-assemble** a required step of capture: delete the target object from within DOS before every assembly and confirm its absence with `scripts/CaptureMerlinCorpus.ps1 -ConfirmAbsent`. DOS 3.3 catalogs carry no timestamps, so there is no equivalent of the test suite's staleness guard — this is the only freshness check available. Without it, an assembly that errors before saving leaves the *previous* entry's object on the disk, and capturing it records one entry's bytes as another's expectation: self-consistent, plausible, wrong, and it will never fail, because the assembler faithfully reproduces the first entry's bytes from the first entry's constructs. Absence after assembly proves nothing wrote it; presence proves *this* assembly did
- [ ] T021a [US1] Read the source back off the disk and **commit that copy**, not the text that was pasted. The disk copy is what Merlin assembled, so it is the only text guaranteed to correspond to the captured bytes, and the entry becomes self-consistent by construction. This matters because Merlin's editor may normalize whitespace or column positions on save — it is column-oriented over a high-bit, CR-terminated format — which would otherwise fail every entry's verification and invite loosening the comparison until it guarded nothing. Keep the comparison and keep it loud, but treat a mismatch as information about the editor rather than a failed capture (issue #110)
- [ ] T021d [US1] **Settle on entry one**: does Merlin's editor store pasted source byte-for-byte, or normalize it? Record the answer in `specs/019-assembler-dialects/research.md`. *(Partially settled: the editor demonstrably tabs fields to fixed display columns, so the screen is not what was typed. Whether STORED bytes are normalized is still open.)* **The "how do you exit Add mode" blocker is CLOSED and this note was stale.** [quickstart.md](./quickstart.md), section "Driving Merlin under emulation", records the answer: `RETURN` as the very first character of a line exits; `ESC`, a bare `RETURN` mid-line, and `Ctrl-C` are all appended as text, which is what made it look unexitable. Read the quickstart before estimating this task — the stale note here has already produced one wrong estimate. **The fixtures pivot demoted it further.** It was written when every entry came through the editor, so an editor that silently normalized would have invalidated the whole corpus. The five vendor oracles are committed and never passed through the editor, so this now gates only the first **entry authored here** — T022 onward — and nothing in the byte-comparison path.
- [ ] T021f [US1] Add the disk **work-copy** discipline to `scripts/CaptureMerlinCorpus.ps1` so it refuses to operate on the pristine image: capture writes source, writes objects, and deletes targets, all on irreplaceable commercial software the developer supplied and this repository cannot regenerate. The procedure now mandates a copy; the tooling should enforce it rather than rely on remembering
- [ ] T021e [US1] Record in `UnitTest/MerlinCorpus/README.md` the residual gap automation cannot close: if the paste garbled *and* Merlin assembled the garbled source, the entry is self-consistent and simply tests a construct nobody intended. The `discriminates` flag catches the worst version — a garble that destroys the Merlin construct stops the entry failing under AS65 — but not one that merely changes an operand. Read the first few entries by eye; nothing downstream will report it. **This guard has a demonstrated near-miss, not a theoretical one**: the first version of `SendCassoKeys.ps1` corrupted every shifted character, and it surfaced only because the garbled text happened to be a BASIC syntax error. Had the first thing typed been valid either way, subtly wrong source would have been assembled faithfully and its bytes captured — self-consistent, wrong, and caught by none of the five automated axes
- [ ] T021b [US1] Batch constructs into a few **composite** source files rather than one file per construct: assemble once with the listing on, save the object, extract, and split by known offsets. A handful of composites covers the FR-007..FR-015 floor at a fraction of the typing
- [ ] T022 [P] [US1] Capture irregular-spacing entries — extra spaces, tabs, and mixtures — as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle the field-based line model empirically
- [ ] T023 [P] [US1] Capture mixed-case and long-symbol entries as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle symbol case sensitivity, length limit, and legal character set (research.md CHK008, CHK009)
- [ ] T024 [P] [US1] Capture expression entries covering Merlin's operator set, precedence, and the current-program-counter form as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) (research.md CHK052)
- [ ] T025 [P] [US1] Capture a `XC OFF` entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle whether Merlin accepts a reset form (spec Edge Cases)
- [ ] T025a [P] [US1] Capture the comment-field experiment as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file): one line whose fourth field begins with an ordinary word and would be a syntax error if parsed as anything but a comment. Acceptance confirms comment-by-position; an error proves `;` is required. Keep the entry either way, to pin the answer against regression
- [ ] T025b [P] [US1] Capture quoted-string entries with **leading, embedded, and trailing spaces inside the quotes** as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file). A whitespace-delimited operand scanner breaks on these, and the disk's own `PI.START.S` is full of them — the spaces are payload bytes, so getting this wrong both truncates the operand and silently changes emitted data
- [ ] T025c [P] [US1] Capture a macro fall-through entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), modeled on the vendor library's `ADDX MAC` / `TXA` / `ADDA MAC` with one shared `<<<`, to settle whether an unterminated `MAC` falls into the next. Also settles that the unterminated-macro diagnostic must not fire on legitimate vendor source
- [x] T025d [P] [US1] ~~Capture a macro-local label entry~~ — **ANSWERED BY THE DISK; no capture needed or planned.** The premise was that the vendor library "carefully never creates" the case. The library does not, but the vendor **program** does: `MAKE DUMP.S` expands `INCD` twice and `STORE` three times, and each expansion redefines `NI` / `LP`, while `DECD` redefines `ND`. A shipped, working 589-byte object is therefore proof that Merlin makes macro-body labels unique per expansion — the second of the two outcomes this task was written to distinguish, settled from bytes rather than from an experiment. `KEYMAC.S` adds a detail no capture would have thought to try: the label may sit on the terminator line itself (`NI <<<`). Implemented in T038; the corresponding tests are synthetic because the *rule* is now known, so a capture would only re-confirm it
- [ ] T025e [P] [US1] Capture a `>>>` macro-invocation entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file). The vendor library invokes macros by bare name only, so the disk can never report whether an explicit invocation prefix is also accepted — and a user's source may well contain one. First instance of the general rule that absence from the disk is not absence from the language
  *(**The construct is IMPLEMENTED and the capture is still open** — those are different things, and conflating them is how an unverified guess becomes a settled fact. Both spellings work, spaced and flush against the name, and both are covered by tests; the macro's name is taken as the first `;`-separated item of the operand. Evidence status: **UNVERIFIED**, marked as such at `s_kpszExplicitCallKeyword` in `CassoCore/MerlinDialect.cpp` and in the test's own comment. The tests prove self-consistency and nothing about real Merlin. What capture must still settle: that the prefix is accepted at all, and whether the name is separated from the arguments by the same character that separates the arguments from each other. `PMC`, its documented word synonym, is deliberately NOT implemented.)*
- [ ] T025f [P] [US1] Capture the vendor library's five-deep nested first-character conditional (`MOVD`) as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) as a stress entry. This is how Merlin macros dispatch on addressing mode, so any macro library of consequence exercises it
- [ ] T026 [US1] Cross-check a sample of captured entries against hand-derived expectations from the Merlin manual, and record the answers to all settle-by-capture items in `specs/019-assembler-dialects/research.md`. **If T025 shows a CPU-target reset form exists**, amend FR-015 and the `InstructionSetProvider` state transition in `data-model.md` — both currently describe a one-way `base → extended` change — and add the implementing task before T040 rather than discovering the conflict during it

### The Merlin profile

- [x] T027 [US1] Create `CassoCore/MerlinDialect.h` / `.cpp` as a `DialectProfile` subclass, register it in `CassoCore/DialectRegistry.cpp` and `CassoCore.vcxproj`, and **add the `DialectId::Merlin` enumerator in the same change**. The enumerator is added *with* its profile, never ahead of it — a placeholder resolves to the wrong profile while looking like support that exists, and `DialectMechanismTests` fails until the registry answers for it
- [x] T028 [US1] Implement Merlin comment conventions in `CassoCore/MerlinDialect.cpp` — asterisk in column 1 for a whole-line comment; a semicolon **beginning the field after the operand** introduces a trailing comment. **Not "a semicolon anywhere"**: inside the operand field a semicolon is data, and the disk's own macro library depends on it (`ADD SUMSTR;DEFLEN;PL`). Whether the introducer is even required, or whether a fourth field is a comment regardless of what starts it, is one of the settle-by-capture questions — T025a answers it and this task implements the answer (FR-007)
- [x] T029 [US1] Implement field-based line segmentation in `CassoCore/MerlinDialect.cpp` — whitespace runs separate label, opcode, operand, and comment; tabs are ordinary whitespace with no tab-stop expansion; no field is required at a specific column. **The scanner must respect quoting**: whitespace ends the operand only outside a quoted string, or `ASC "HELLO WORLD"` splits into an operand and a bogus comment. **And a `;` inside the operand field is data**, not a comment — it is Merlin's macro-argument separator (FR-007, FR-008)
  *(T027-T029 landed as one commit, deliberately. The operand scanner cannot be written without the comment rule, and neither is correct without the delimiter rule, so splitting them would have shipped a knowingly-wrong quoting rule for one commit. `DialectId::Merlin` and its registry row land here too, per the enum's own rule that an enumerator arrives WITH its profile — safe because `merlin` is unreachable from the command line until US2. **Directive spellings are NOT included**: `directiveToken` stays `Directive::None` until T032/T033, so this is an incomplete profile, not a broken advertised feature.*

  *Two findings from the vendor sources changed the implementation. `;` in column 1 is a whole-line comment — 8 such lines across 3 files — and it is not a special case: with no label, column 1 IS the first field boundary, so the general rule already covers it. And the string delimiter is **any character**, taken from the source: `ASC !" ASC ""!` in `KEYMAC.S` chooses `!` precisely because its text contains quotes, so a `"`-only scanner ends the operand inside the data. 164 of 166 string lines use `"`; the 2 that do not are why this is a rule about delimiters.)*
- [x] T030 [US1] Implement label rules and the local-label prefix in `CassoCore/MerlinDialect.cpp`, scoping locals to the enclosing global label (FR-008)
  *(**Divergence, deliberate**: the prefix is declared in `MerlinDialect.h` — one character, `':'` — but the SCOPING is in `AssemblySession.cpp`, because it is stateful and profiles are stateless and shared. A definition binds under the global label joined to the local name, and every local REFERENCE inside an operand is rewritten to match. The reference half is not optional: the vendor sources write `LDA :TABLE+5,X`, so a dialect answering only "is this a local definition" leaves every use unresolvable.*

  *The separator is a **period** because `Parser::ValidateLabel` rejects one in a label while the expression tokenizer accepts one inside an identifier. That pair of facts is the whole design: no symbol a source can spell may contain a period, so a scoped name cannot collide with a global however either is written, and the scoped name still resolves through the ordinary expression path rather than a second lookup. A local is validated as the name it SPELLS and stored under the name it BINDS to, since validating the joined form would reject every one of them.*

  *A local before any global label is an error rather than a symbol in an unnamed scope. A colon inside string payload is left alone — `ASC ":::6::6:6:"` is on the vendor disk, and rewriting there would change emitted bytes rather than resolve a symbol.*

  *Still open, and NOT part of this: **`?` is legal in a Merlin label**. `CMD?`, `CORR?`, `ISY?` and `RNGOK?` are in the vendor sources, and `Parser::ValidateLabel` rejects all four today. The corpus therefore already answers half of research's "legal label character set" question, ahead of T023 capturing it — but accepting `?` also needs the expression tokenizer to lex it, and that is a change to `ExpressionEvaluator.cpp` with no oracle forcing it yet. Variable symbols (`]name`) are T031 and were not touched.)*
- [x] T031 [US1] Implement variable symbols in `CassoCore/MerlinDialect.cpp` with reassignment semantics (FR-011)
  *(**No oracle exists for any of this.** Not one variable symbol appears in the nine committed vendor sources — the sigil occurs there only as `]1`..`]3` inside macro bodies — so every test is self-consistency plus the documented rule, and must not be quoted as corpus evidence.*

  *A variable binds in a NAMESPACE OF ITS OWN, because the sigil is part of the name: `]COUNT` and `COUNT` are two symbols and either may exist without the other. The stored name cannot keep the sigil, since the shared expression tokenizer will not lex it, so the profile rewrites both definitions and references to a prefixed form carrying TWO periods. That count is the whole design: an ordinary label may hold none — `ValidateLabel` rejects the character — and a scoped local binds as one label joined to another, so it holds exactly one. Nothing a source can spell can therefore collide.*

  *Reassignment reuses the existing mutable symbol kind rather than inventing one, which gives the rule its shape for free: a variable may be redefined, an ordinary equate may not.*

  ***Divergence, deliberate at the time and now LIFTED: the form where a variable stands as a program-counter label was REFUSED.** The reasoning was sound — pass 2 holds one symbol table, so a repeated program-counter symbol would point every branch at the last copy — but the fix already existed a few lines above it. `RebindMutableConstant` gives a reassignable symbol its value again where pass 2 reaches its definition, and a reassignable LABEL is the same problem with the program counter for an expression. `CLOCK.S` forced it: eight `]LOOP` targets, each branch meaning the one above it. It also opens no local-label scope, which no oracle can discriminate and a synthetic test does. See the `KBD` section in the state of play.*

  *One shared-engine fix came with it, and it is a two-pass disagreement rather than a Merlin matter. A mutable constant is now re-evaluated in pass 2 where its definition is reached. Pass 1 walks the file in order and sees each assignment in turn, which is what sizes the lines between them; pass 2 read one table built after pass 1 finished, so **data emitted in pass 2 took the value the file assigned last** while an instruction on the line above took the right one. An immutable symbol cannot show the difference, which is why it went unnoticed. The instruction-shaped test does NOT discriminate it — that was confirmed by mutation, and a data-directive test was added because of it.)*
- [x] T032 [US1] Add the Merlin directive spelling table to `CassoCore/MerlinDialect.cpp`, reusing existing `Directive` tokens wherever the operation is identical
- [x] T033 [P] [US1] Add **all** new `Directive` tokens this feature introduces in `CassoCore/Directive.h` and `CassoCore/Directive.cpp`, with their pass-1/pass-2 rows in `CassoCore/AssemblySession.cpp`: reversed-order words, raw hexadecimal data, the loop construct and its terminator, the dummy section and its terminator, CPU selection, and the single encoded-string token. Adding them in one task keeps the exhaustiveness-checked `switch` compiling once rather than breaking at each of T035–T040 (research.md D2, FR-009)
  *(The exhaustiveness check is real and fired exactly once, as this task predicted — but it is a `static_assert` on a handler ROW TABLE in `AssemblySession.cpp`, not a switch: "s_kRows must have one row per Directive". All 17 new tokens got rows in the same edit. Their handlers are null, meaning **not implemented yet** rather than "does nothing"; they are unreachable while as65 is the only selectable dialect, and T034-T042 fill them. Emitting nothing for a `HEX` line would be precisely the silent wrong-bytes failure this feature exists to avoid.)*

  *(`Directive` stopped being total over as65's spelling table here, which broke `EveryToken_HasACanonicalSpelling` correctly. Merlin tokens must NOT gain an as65 spelling — FR-005 forbids admitting one dialect's constructs into another — so the test's claim was narrowed to as65's own tokens, keeping the RMB regression it was written for. The totality that survives is checked in `DialectMechanismTests`: every token is claimed by **at least one** dialect. A token claimed by none is unreachable, which is the bug the original sweep actually defended against.)*
- [ ] T033a [US1] Resolve directive spellings that collide with an instruction mnemonic by the active dialect's rule in `CassoCore/MerlinDialect.cpp`, using the `DirectiveTable::FromAmbiguousSpelling` precedent so resolution never depends on which table is consulted first (spec Edge Cases)
- [x] T034 [P] [US1] Create `CassoCore/StringEncoding.h` / `.cpp` implementing high-bit, inverse, flashing, and terminator handling per [contracts/merlin-directives.md](./contracts/merlin-directives.md), and register both in `CassoCore.vcxproj`
- [x] T035 [US1] Wire the five Merlin string spellings to one `Directive` token carrying a `StringEncodingMode` in `CassoCore/MerlinDialect.cpp`, including delimiter-driven high-bit inference (FR-010)
  *(**First byte-identical result against vendor object code.** `LABELS.S`'s 105 DCI lines reproduce 983 of `LABELS`'s 984 bytes exactly, through the real parser and the real encoder. The 984th is the `$00` of `END BRK`, an instruction rather than string data — and `END` there is a LABEL in column 0, not the END directive, which the parser gets right only because directive lookup reads the mnemonic field.*

  *DCI **inverts** the last character's high bit rather than clearing it. The distinction is invisible to the corpus, and the sabotage proves it: an implementation that CLEARS instead of inverting **passes all 983 vendor bytes** and is caught only by the synthetic low-ASCII test. That is the sharpest demonstration so far that a corpus of real vendor source is not sufficient on its own — it can only test what the vendor happened to write, and every DCI on the disk is high-ASCII.*

  *Three of the six encodings have **no oracle**: `INV` appears once, in a linker demo shipping no object, and `FLS` and `STR` appear nowhere. They follow documentation rather than bytes and are marked UNVERIFIED at each line. Same for the apostrophe half of the delimiter rule — `"` and `!` both give high ASCII, which disproves any ASCII-ordering rule, but no `'`-delimited string exists in the corpus.*

  *Also lands `GetDefaultOrigin` on the seam: Merlin defaults to `$8000`, as `LABELS.S` proves by containing no origin directive while its object loads there. as65 keeps 0. A wrong default yields byte-perfect output at the wrong address, which reads as a far deeper problem than it is.)*
- [x] T035a [US1] Give `Directive::StringData` and `Directive::ErrorIf` their handler rows in `CassoCore/AssemblySession.cpp`. **Recorded after the fact, and pulled forward out of the T036–T042 band on purpose**: `LABELS.S` is 105 string lines and one `ERR`, so the first whole-file oracle cannot exist without both, and leaving them null would have meant claiming an end-to-end result from a hand-rolled loop in a test. `ERR` acts in **pass 2**, where every symbol is known, because the assertions people write bound a table by the distance between its own two ends and one of those ends is always a forward reference. Its pass-1 row is `IgnorePass1Directive` rather than null — a directive with no pass-1 handler is not marked as one, and an unmarked line never reaches pass-2 dispatch, so a pass-2-only directive silently does nothing. The remaining null rows are still T036–T042's
- [x] T035b [US1] Give `Directive::HexData` its handler rows in `CassoCore/AssemblySession.cpp`, and accept a hexadecimal run after a string operand's closing delimiter. **Recorded after the fact**, and pulled forward for the same reason T035a was: `MAKE DUMP.S` cannot be read at all without them. One encoder serves both passes, so the size pass 1 reserves and the bytes pass 2 writes cannot disagree. *(The trailing-run form was previously REFUSED because nothing pinned what the digits meant; `MAKE DUMP`'s object settles it. The bytes are hexadecimal, and — the half no reasoning would have produced — they are NOT put through the delimiter's high-bit convention, which `00` staying `00` proves. An odd digit count is refused rather than padded, since both plausible repairs change every byte after it. Comma separators are accepted and marked UNVERIFIED: no vendor line uses one, and refusing a documented form can only cost a user source that assembles elsewhere.)*
- [x] T035c [US1] Add Merlin equates and the listing directives to `CassoCore/MerlinDialect.cpp`. **Recorded after the fact.** An equate puts its sign in the OPCODE field with the name beside it in the label field, so it is a field-model fact rather than an expression one — a parser hunting for the sign inside the operand finds it in the wrong place and leaves the line looking like an instruction named for it. The name must NOT also bind to the program counter, or the equate is reported as a duplicate of the label its own line just defined. 128 uses across the vendor sources and no other equate spelling; `EQU` is accepted as language rather than as oracle. `TR`/`EXP`/`AST` reuse `Directive::OptNoop` rather than each bringing a token whose handler would do nothing
- [x] T035d [US1] Add dialect-scoped instruction aliases to `CassoCore/DialectProfile.h` and `CassoCore/MerlinDialect.cpp` — `BLT`/`BGE` for `BCC`/`BCS` — resolved once in `Parser::ParseLine`. **Recorded after the fact**, and deliberately DATA on the seam rather than a Merlin arm in the instruction machinery: the alternative touches the opcode lookup, the size estimator, the branch-range check and the encoder, which is a per-dialect special case in four places in shared mechanism for two words (`contracts/dialect-profile.md` guarantee 3, SC-009/T069). as65 declares none and is swept to prove it (FR-005)
- [x] T035e [US1] **Separate the emit cursor from the program counter** in `CassoCore/AssemblySession.h` / `.cpp`. `AssemblySession` gains an output cursor beside `m_pc`; `ReserveBytes` is the one place both advance, and only an origin directive can part them. Which it does is profile data — `DialectProfile::GetOriginSemantic` — following T051's `cpuSource` precedent rather than a Merlin arm in the driver. A bare origin resyncs the program counter to the cursor, and is refused as a missing operand where the two cannot differ. **This is the spec amendment guarantee 1 asks for**, recorded in [contracts/dialect-profile.md](./contracts/dialect-profile.md) — the gap is in the ENGINE, which had no way to express "advance the PC without advancing output" in any dialect. Editing `AssemblySession.cpp` is **not** an SC-009 violation: T070 is judged against T069's own commit, exactly as the `ExpressionEvaluator` binding change was
- [x] T035f [US1] Bind a label sharing a line with an origin directive, in `CassoCore/AssemblySession.cpp`. It never bound, because the origin claims the line as a prelude before `RecordLabel` runs — dialect-neutral, as65 dropped it too. ~~**The value is the OUTPUT CURSOR**~~ — **CORRECTED: the value is the PROGRAM COUNTER as the line was reached.** The cursor reading agrees everywhere the two cursors are in step, which is everywhere `MAKE DUMP` looks, and disagrees on a **bare** origin closing a relocated section. `CLOCK.S` has one: `IRQEND ORG`, with `LDY #IRQEND-IRQHAND-1` reading `$12` in the shipped object where the cursor reading gives `$30`. The corrected rule takes no dialect input at all — a label binds where its line was reached, exactly like a label on any other line — and as65's expectation was rewritten with it rather than special-cased
- [x] T035g [US1] The four remaining `MAKE DUMP` diagnostic classes, in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp`. `ERR \expr` and the immediate byte selectors are parse-time operand rewrites in the profile — the assembler can already compute both, so guarantee 2 admits no new token; the operandless accumulator form and the double-quoted high-ASCII character constant are profile data, the latter carried into the shared evaluator through `ExprContext` exactly as `binding` already was. **Divergence, deliberate at the time and now CLOSED**: `?` in a label and `&` against a character literal were left undone, since nothing forced either while `KEYMAC.S` could not be an oracle. Both are settled in T035h. `?` needed the label rule and the identifier lexer together; `&` needed **nothing** — it was already bitwise-and and the high-ASCII delimiter already landed, and what actually failed on those lines was the operand scanner breaking on a space inside a character constant. Naming it as an unbuilt construct was a wrong diagnosis, not a wrong decision
- [x] T035h [US1] The keyboard-input directive and the four expression facts the remaining three oracles need, in `CassoCore/Directive.h`, `CassoCore/MerlinDialect.h`/`.cpp`, `CassoCore/AssemblySession.h`/`.cpp`, `CassoCore/AssemblerTypes.h`, `CassoCore/ExpressionEvaluator.h`/`.cpp` and `CassoCore/Parser.h`/`.cpp`. **Recorded after the fact**, in the T035 band because that is where it executes. `KBD` binds a symbol from `AssemblerOptions::predefinedSymbols` and refuses by name when no answer was supplied; it gets a `Directive` token, because requiring a value from outside the source and saying which one is missing is an operation the assembler could not already perform. Beside it: Merlin's own spellings for exclusive-or and inclusive-or, unsigned 16-bit arithmetic, `?` inside a symbol, and a variable symbol standing as a repeated program-counter label. The three expression facts are carried through `ExprContext` as profile DATA, exactly as the operator binding and the high-ASCII delimiter already were, so the evaluator gains no dialect branch. **Divergence:** T031's refusal of the program-counter variable is lifted, and T035f's label-on-origin rule is corrected — see both
- [ ] T036 [P] [US1] Implement the loop construct and its terminator in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-011)
- [ ] T037 [P] [US1] Implement dummy sections and their terminator in `CassoCore/AssemblySession.cpp` — assign addresses, emit no bytes (FR-012)
- [x] T038 [US1] Implement Merlin macro definition, positional parameters, and invocation syntax in `CassoCore/MerlinDialect.cpp`, reusing the existing `kMaxMacroDepth` limit (FR-013)
  *(The definition shape — `MAC` in the opcode field with the name in the label, closed by the triple-angle token — arrived earlier with the terminator spelling fix. This completes the substance.*

  ***Positional substitution ignores identifier boundaries, and that is the requirement rather than an oversight.** The vendor library splices a parameter into the MIDDLE of a name in both directions: `LDX #A]1-ADRTBL` pastes the argument after a prefix, `LDX #]1END-]1-1` before a suffix. A whole-word rule — which is what the named-parameter path correctly uses — leaves both unresolvable. Both directions have their own test, and they earned it: a mutation adding a right-hand boundary check breaks only the suffix one.*

  ***Body labels are unique per expansion with no declaration, and a label an expansion produced no longer opens a local-label scope.** The second half is not a refinement of the first. `MAKE DUMP.S` calls macros defining `LP` and `ND` in the middle of routines whose locals belong to a global further up, so a macro label becoming the enclosing global strands every local after the call — which is exactly what the file's diagnostics said before the fix.*

  ***The terminator line may carry a label.*** `KEYMAC.S` writes `NI <<<`, so the body's own branch target sits on the line that closes the definition — the one line closing a body discards. Twelve synthetic macro tests were green before the vendor source found it.*

  ***Reuses `kMaxMacroDepth` unchanged**, as the task asks; nesting still costs queue entries rather than C++ stack, so a Merlin macro calling another simply re-enters the expander one level deeper. Argument separator and parameter sigil are profile DATA, not a Merlin branch in the expander.*

  ***Divergence, minor: the explicit invocation is spelled only with the prefix form.** `PMC` is documented as its word synonym; neither appears on the disk, so implementing both would double the unverified surface for no evidence. See the state-of-play note.*

  *This answers T025d from the disk rather than from capture — see that task.)*
- [ ] T039 [US1] Map Merlin's file-inclusion directives to `Directive::Include` in `CassoCore/MerlinDialect.cpp`, resolving relative to the including source and reusing the existing `kMaxIncludeDepth` limit (FR-014)
- [ ] T040 [US1] Implement the first occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp`, switching `InstructionSetProvider` to the extended table for the remainder of the assembly (FR-003, FR-015)
- [ ] T041 [US1] Implement the object-file directive as naming the output in `CassoCore/MerlinDialect.cpp`, with the command line taking precedence over it (FR-027)
- [ ] T042 [US1] Add diagnostics for unterminated dummy sections, loops, and macros at end of file in `CassoCore/AssemblySession.cpp`, naming the construct and its opening line as the existing unclosed-conditional diagnostic does (research.md CHK041)

### Corpus completion

- [ ] T043 [P] [US1] Capture one entry per string-encoding spelling as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) — five entries, not one, because a high-bit or terminator error still looks plausible
- [ ] T044 [P] [US1] Capture at least one multi-file inclusion entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), served through `UnitTest/MockFileReader.h`
- [ ] T045 [P] [US1] Capture one entry per remaining construct in FR-007 through FR-015 and FR-027 as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), completing the corpus floor
- [x] T045a-partial — **superseded: all five sources and all six objects are done.** See T045a.
- [x] T045a [P] [US1] Validate against the **five** real positive oracles on the Merlin Pro 2.23 disk — source and shipped object both present, absolute mode — not "~40 files". Measured: `LABELS.S`→`LABELS` (984 @ `$8000`, 105× `DCI` plus one `ERR`), `KEYMAC.S`→`KEYMAC` (674 @ `$9000`), `PRINTFILER.S`→`PRINTFILER` (286 @ `$02A0`), `MAKE DUMP.S`→`MAKE DUMP` (589 @ `$9000`), and `CLOCK.S`→**both** `CLOCK.24` and `CLOCK.12` (365 @ `$0240` each). Five sources, six objects. Vendor source and objects **are committed**, under `UnitTest/Fixtures/Merlin/`, and read through `IFixtureProvider::OpenFixture` — the earlier "used, not committed" instruction is superseded
- [x] T045d [P] [US1] Prioritize `CLOCK.S`: one source producing two different objects through `DO HOURS-12` / `ELSE` / `FIN`, so a single capture yields conditional-assembly coverage **and** two independent byte-identical checks. The highest-value single entry on the disk *(Done, and it was every bit of that: it forced the operator set, the arithmetic width, the program-counter variable label, and the correction to T035f's label-on-origin rule, none of which any other source could discriminate. **Divergence from the task's own framing:** the two objects are selected by `VERSION`, an answer supplied to the assembly, NOT by `DO HOURS-12` alone — that inner conditional gates only which `SAV` line is reached, and `SAV` is out of subset. `HOURS` is derived from `VERSION` by arithmetic and is what the emitted bytes actually depend on. A third test requires the two results to differ, or the pair would be two copies of one check.)*
- [ ] T045e [P] [US1] Use `Merlin/PI.ADD.S` and `Merlin/PI.START.S` as **negative** subset-boundary specimens only, never positive comparisons — they ship no objects, and the APPLE PI group is the linker demo whose own header says "This is just a test source for the linker". `PI.ADD.S` is the export-only shape (`REL` + **6** `ENT`, no `EXT`); `PI.START.S` is the no-workaround shape (`REL` + 3× `EXT` + 1 `ENT`). Between them they exercise **both** refusal messages, which is why exactly these two are committed: `PI.MAIN.S` and `PI.DIV.S` also import and are redundant with `PI.START.S`, and `PI.LOOK.S` is redundant with `PI.ADD.S`
- [ ] T045f [P] [US1] Use the type-T macro libraries (`T.MACRO LIBRARY`, `T.SENDMSG`, `T.PRDEC`, `T.OUTPUT`, `T.FPMACROS`, `T.ROCKWELL MACROS`, `T.PI.MACS`, `PI.NAMES`) as the `PUT`/`USE` inclusion corpus. They are the only type-T files on the disk — every `.S` source is type B loading at `$0901` — so they also settle the DOS 3.3 text convention from real vendor files. `RWTS DEMO.S` ships **no** object, so it is a parse/assemble case only, never a comparison
- [ ] T045c [P] [US1] Record the vendor-source validation *result* — that re-assembling a vendor file reproduces its shipped object byte-for-byte — as evidence in `research.md` rather than as a committed corpus entry, which is licensing-safe and is the part that actually carries information
- [ ] T045b [P] [US1] Walk the **manual's directive list** and add a corpus entry for every construct the disk does not exercise. The disk demonstrates idiom but cannot report what the vocabulary holds that this vendor never used; absence from the disk is not evidence of absence from the language (spec Corpus Floor)
- [ ] T046 [US1] Verify SC-001: every corpus entry assembles byte-identically via `UnitTest/MerlinCorpusTests.cpp`
- [ ] T046a [US1] Verify SC-002 in `UnitTest/MerlinCorpusTests.cpp`: the five vendor sources assemble **exactly as committed**, with no edit to any line. The corpus already proves the bytes match; what this adds is the claim that the *input* was not touched to get there — assert each entry's source is the fixture bytes as `IFixtureProvider::OpenFixture` returns them, not a transcribed or tidied copy. Without it, SC-002 is only inspected, and "unmodified" is precisely the property a passing corpus can be made to fake
- [ ] T046b [US1] Verify SC-003 in `UnitTest/MerlinSubsetTests.cpp`: assemble every committed vendor source and assert that **each rejection maps to a row in `CassoCore/MerlinSubsetBoundary.cpp`**. SC-003 defines a rejection with no boundary row as a defect, so this is a sweep over rejections rather than a fixed list of expected errors — a new unexplained rejection fails the test by construction. Runs against `PI.ADD.S` and `PI.START.S` too, where rejections are the expected outcome and must still be table-backed
- [ ] T047 [P] [US1] Add focused parser tests to `UnitTest/MerlinParserTests.cpp` and directive tests to `UnitTest/MerlinDirectiveTests.cpp`, registering both in `UnitTest.vcxproj`
  *(Half done. `MerlinDirectiveTests.cpp` exists and is registered, covering the string family through both passes, `ERR` in **both** directions, local-label scoping, operator binding, the default origin, the keyboard-input directive in both the answered and unanswered directions, the two renamed operators, unsigned 16-bit division, `?` inside a symbol, and character constants holding a space — each with an AS65 counterpart where the construct is shared, since a test passing under both dialects is no evidence the profile was consulted. The remaining directives get theirs as T036–T042 land.*

  *Two of these could only be written as pairs. The vendor corpus contains only the SILENT case of `ERR`, because a source shipping an object necessarily assembled clean — so an `ERR` that never fires passes every oracle on the disk. Same for binding: an evaluator that happened to agree looks identical without the AS65 half.)*

**Checkpoint**: Unmodified Merlin source assembles to Merlin's bytes. This is the MVP.

---

## Phase 4: User Story 2 - Choose a dialect explicitly (Priority: P1)

**Goal**: A developer states the dialect and gets exactly that dialect's rules, strictly.

**Independent test**: One source file assembled under each dialect selection; constructs valid in one and invalid in the other are accepted and rejected accordingly.

**⚠️ Shared file warning**: `CassoCore/CommandLineParser.cpp` and `UnitTest/CommandLineTests.cpp` are shared with the concurrently developed spec 020. Everything **added** here is additive — two rows, one enumerator, one arm, one flag parser. Do not restructure the dispatcher; if that seems necessary, stop and raise it.

The **one** sanctioned exception is T049a, removing the fallback heuristic: a decision taken explicitly, not a restructuring reached for. It is on hold until 020's command-line work merges, so 019 never edits those files while 020 holds unmerged changes in them. It is the only place this feature edits shared behavior rather than extending it, and it is why the 020 session will need to rebase. Keep it confined to the fallback cases — a wider edit here is not covered by that decision.

- [ ] T048 [US2] Add `Subcommand::Merlin` and a `dialect` field to `CommandLineOptions` in `CassoCore/CommandLineOptions.h`
- [ ] T049 [US2] Add one row `{ "merlin", CommandLineOptions::Subcommand::Merlin }` to `s_kSubcommands` and one arm in `Parse` in `CassoCore/CommandLineParser.cpp`
- [x] T049a [US2] **⛔ HOLD until the user confirms spec 020's command-line work has merged to master.** Sequencing measured rather than guessed: 020 has 384 lines in flight across `CommandLineOptions.h`, `CommandLineParser.cpp`/`.h`, and `CommandLineTests.cpp`; 019 has touched none of them. Whoever holds unmerged work in a file should not have to resolve around someone else's edit. The stronger reason is not conflict at all: 020 is adding `disk` to `s_kSubcommands`, and that table is exactly what decides which bare words reach the fallback — so the behavior being removed *changes shape* when `disk` lands, and doing this first means writing tests against an intermediate table that is about to move. — **Then, in ONE commit**: add the row `{ "as65", CommandLineOptions::Subcommand::As65 }` to `s_kSubcommands`, remove the unrecognized-first-argument fallback from `Parse` (both in `CassoCore/CommandLineParser.cpp`), update `UnitTest/CommandLineTests.cpp`, and add the `CHANGELOG.md` breaking-changes entry.

  **One commit is a correctness requirement, not tidiness.** The table on master holds exactly one row, `{ "run", Subcommand::Run }` — there is no `as65` word, so `Subcommand::As65` is reachable *only* through the fallback being removed. Any commit that removes it without adding the row in the same change leaves AS65 unreachable, and a bisect lands on that broken midpoint.

  The error for an unrecognized first argument MUST name the replacement (`did you mean: CassoCli as65 <source>`) rather than print usage: the affected population is build scripts, which nobody re-reads until they fail, so a bare "unknown argument" turns a one-line fix into a bisect.

  **Deleting 020's `BareWordThatIsNotASubcommand_StaysAs65` is the intended outcome, not a regression.** That test was added as a tripwire, so that adding a table row could not erode the fallback incidentally and removal would have to be a deliberate act carrying its own CHANGELOG entry. This is that act. Say so in the commit message, or it reads later as someone deleting an inconvenient test.

  Reverses the earlier deferral recorded in [research.md](./research.md) D5 and in the CLI contract's guaranteed-unchanged list; both now say so.
- [ ] T050 [US2] Add `ParseMerlinFlags` to `CassoCore/CommandLineParser.h` and `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](./contracts/cli.md)
- [ ] T051 [US2] Refuse `--cpu` **when the active profile's `cpuSource` is in-source**, driven by profile data rather than by a merlin-specific branch, in `CassoCore/CommandLineParser.cpp`; the message names the directive supplied by the profile. A hard-coded merlin arm here would put a per-dialect branch in the shared mechanism, which is what `contracts/dialect-profile.md` guarantee 3 forbids and what SC-009 exists to catch (FR-026)
- [ ] T052 [US2] Set `AssemblerOptions::dialect` from `CommandLineOptions::dialect` in `CassoCli/CommandLine.cpp`, carrying **provenance** and not just the dialect. With the fallback gone the command line always states a dialect, but `AssemblerOptions` still defaults to AS65 for callers that set none — FR-006 makes the assembler reachable from entry points that are not the CLI — and the reporting table in [contracts/cli.md](./contracts/cli.md) keys off exactly that distinction: stated is reported nowhere, defaulted is reported under `-v` or in the listing header
- [x] T053 [US2] Create `CassoCore/DialectReporting.h` / `.cpp` deciding **what** dialect-and-CPU line to emit and **when**, per the reporting table in [contracts/cli.md](./contracts/cli.md), and register both in `CassoCore.vcxproj`. `CassoCli/CommandLine.cpp` only prints what it returns — never unconditionally on stdout, which carries the listing when no listing file is named. The decision lives in core so `UnitTest` can exercise it (FR-004, SC-005). *(Decision: the sink is an enum with a `StandardOutput` value that is never produced, so "never on stdout" becomes an assertion a sweep can make rather than a property only a reader can check. Provenance arrives as `AssemblerOptions::dialectSelection` — see the state-of-play note. Nothing calls this yet; T052/T053b are the wiring and they touch files spec 020 holds.)*
- [x] T053a [P] [US2] Report the **CPU target** alongside the dialect through the same path — including when it was left at the dialect's default, so "no directive was seen" is not misread as "the flag was ignored" — in `CassoCore/DialectReporting.cpp` (SC-005). *(Decision: the CPU's NAME is supplied by the caller in `CpuReport` rather than derived. Core's assembler has no CPU-target vocabulary — instruction tables arrive unnamed — and the two alternatives were a second CPU enumeration in core or a dependency on `CommandLineOptions::CpuTarget`, which sits in a file 020 is editing. Second decision: the CPU's reporting is decided INDEPENDENTLY of the dialect's provenance; the contract's wording admits a reading under which this row can never fire, and the state-of-play note records why that reading was rejected.)*
- [x] T053c [US2] Verify SC-005 in `UnitTest/DialectReportingTests.cpp`: walk **every row** of the reporting table in [contracts/cli.md](./contracts/cli.md) and assert `DialectReporting` produces what that row says — including the two negative rows, that nothing is emitted when a selection was stated, and that nothing reaches stdout in any case. The negatives are the half worth having: an implementation that reports unconditionally satisfies "the developer can determine it" while breaking the piped-listing guarantee FR-004 spends most of its words on. *(Every row has a test, both negatives included, plus one for the `AssemblerOptions` default itself. Nine mutations of the reporting code were caught, among them "report the dialect unconditionally", "report the CPU unconditionally", "drop the dialect-default CPU row", both sinks pointed at stdout, and the dialect name hard-coded.)*
- [ ] T053b [P] [US2] Register the `as65` and `merlin` subcommands and their flag tables in the tool's usage and help output via `CassoCli/CommandLine.h` and `CassoCli/CommandLine.cpp`, deriving the flag list from core so help cannot drift from the parser, with a test (FR-024, US2 acceptance 4)
- [ ] T054 [P] [US2] Add merlin grammar tests to a **new** `UnitTest/MerlinCommandLineTests.cpp` rather than editing `UnitTest/CommandLineTests.cpp`, and register it in `UnitTest.vcxproj`
- [ ] T055 [US2] Verify `UnitTest/CommandLineTests.cpp` passes with zero modifications, confirming spec 020's pinned behavior is intact
- [x] T078 [US2] Create `CassoCore/AssemblerExitCode.h` / `.cpp` mapping an `AssemblyResult` to the shared vocabulary — 0 clean, 1 succeeded with complaints, 2 no output — and register both in `CassoCore.vcxproj`; `CassoCli/CommandLine.cpp` returns what it computes. A subset-boundary refusal maps to 2 and is distinguished by its message, not by a distinct code. In core so the mapping is unit-testable rather than reachable only by running the exe (FR-030). *(Decision: tests live in a new `UnitTest/AssemblerExitCodeTests.cpp`, which the task did not name. `CassoCli/CommandLine.cpp` does NOT yet return what it computes — that edit belongs with T052's wiring and the file is shared with spec 020. The failure test reads `success` rather than the error list, since every recorded error clears the flag and warnings-as-errors clears it while filing the diagnostic as an error; the refusal case is pinned by asserting it earns the SAME code as a syntax error rather than by asserting each is 2.)*
- [ ] T079 [P] [US2] Add a cross-dialect strictness test to `UnitTest/MerlinCommandLineTests.cpp`: a Merlin-only construct assembled under AS65 is rejected naming the construct and the active dialect, and an AS65-only construct under Merlin likewise. This is US2's independent test and FR-005's direct evidence — no other task exercises the accept/reject matrix

**Checkpoint**: Dialect selection is explicit, strict, and additive to the shared command-line surface.

---

## Phase 5: User Story 3 - Diagnostics that speak the developer's dialect (Priority: P2)

**Goal**: Failures name constructs in the selected dialect's vocabulary, at the right line and column, and subset-boundary refusals are unmistakable.

**Independent test**: A known error introduced per dialect produces a diagnostic naming the right construct at the right position; every boundary construct produces a named refusal rather than a parse error.

- [ ] T056 [US3] Create `CassoCore/MerlinSubsetBoundary.h` / `.cpp` with one row per refused construct carrying spelling, reason class, explanation, and what widens it, exposed by a `GetAll`-style accessor; register both in `CassoCore.vcxproj` (FR-019)
- [ ] T057 [US3] Refuse relocatable-mode assembly and entry and external symbol declarations in `CassoCore/MerlinDialect.cpp` via the boundary table, naming each construct (FR-016)
- [ ] T057a [US3] Make the relocatable refusal **actionable** in `CassoCore/MerlinSubsetBoundary.cpp`: a module with relocatable mode and entry symbols but no external symbols exports without importing, so the message states it assembles on its own once relocatable mode is removed and an origin supplied. A module declaring any external symbol gets the other message — no workaround, resolving cross-module references needs the linker in issue #112 — because offering the first fix there sends the developer down a path that cannot work. The vendor's own sample is the export-only case, so this is the likely first encounter (FR-031)
- [ ] T058 [US3] Refuse the second occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp` as selecting an unemulated CPU (FR-016)
- [ ] T059 [P] [US3] Refuse the file-type directive in `CassoCore/MerlinDialect.cpp` as owned by `020-disk-file-access` (FR-028)
- [ ] T060 [P] [US3] Refuse the save-object directive in `CassoCore/MerlinDialect.cpp` as multi-output segmentation needing its own decision — the message must NOT describe it as waiting on 020 (FR-029)
- [ ] T061 [US3] Make a boundary refusal distinguishable from a syntax error in `CassoCore/AssemblySession.cpp`, and collect every offender across the whole pass before failing rather than stopping at the first (FR-017, FR-018)
- [ ] T062 [US3] Generate the subset-boundary help text **from the boundary table inside `CassoCore/MerlinSubsetBoundary.cpp`**, returning a string that `CassoCli/CommandLine.cpp` merely prints. Generation in the executable would be unreachable from `UnitTest`, so FR-019's "cannot disagree by construction" would gain no test — and Principle VI is non-negotiable (FR-019, FR-024)
- [ ] T062a [P] [US3] Add a test to `UnitTest/MerlinSubsetBoundaryTests.cpp` asserting the generated help text names every row the accessor returns, so a row added to the table without help coverage fails the build rather than shipping
- [ ] T063 [US3] Populate `column` on every Merlin diagnostic in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-021)
- [ ] T064 [US3] Describe constructs in the active dialect's vocabulary, and name which dialect defines a construct rejected as belonging to another, in `CassoCore/AssemblySession.cpp` (FR-020, FR-022)
- [ ] T065 [US3] Explain the column rule when a Merlin label is indented, rather than reporting an unknown symbol, in `CassoCore/MerlinDialect.cpp` (User Story 3 acceptance 1)
- [ ] T066 [P] [US3] Add the hand-authored negative corpus class — boundary refusals and diagnostic expectations, kept distinct from captured entries — to `UnitTest/MerlinCorpusTests.cpp` as hand-authored entries, including an entry where a macro is invoked with another dialect's argument syntax and must be **rejected rather than partially expanded** (spec Edge Cases)
- [ ] T067 [P] [US3] Add `UnitTest/MerlinSubsetBoundaryTests.cpp` sweeping the boundary accessor and asserting every row produces the expected refusal, and register it in `UnitTest.vcxproj`
- [ ] T068 [US3] Verify SC-006 and SC-007: every dialect-specific diagnostic identifies the correct line and column, and every out-of-subset construct is named rather than failing as a parse error

**Checkpoint**: Diagnostics are dialect-native, positioned, and boundary refusals are unmistakable.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [x] T069 Add a synthetic, test-only third dialect profile to `UnitTest/DialectMechanismTests.cpp` and prove it works end to end — this is what catches a mechanism secretly built for exactly two dialects (SC-009). ~~**Do not pull this forward.**~~ **That hold has EXPIRED and the task was pulled forward deliberately**, rather than being contradicted silently. Its reason was that against a seam shaped by AS65 alone the synthetic profile gets written to fit whatever seam exists; Merlin has since pressed on the seam with the field model, operand-internal semicolons, quoted operands, mnemonic aliases, macro syntax, variable symbols and now the origin semantic — so the seam it is written against is a real one. **The profile must declare the OPPOSITE origin semantic from AS65**, or it never exercises the axis the emit-cursor split added and passes while testing nothing, which is the exact trap this task's own warning describes
- [x] T070 Verify SC-009 against **T069's commit alone**, not against `origin/master`: `git show --stat HEAD -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp` must be empty. *(Run against T069's commit and empty: that commit changes exactly one file, `UnitTest/DialectMechanismTests.cpp`. The synthetic profile reaches the engine through `AssemblerOptions::dialectProfile`, an injection point added in the PRECEDING commit for the reason the registry cannot supply: a closed table cannot demonstrate that a dialect outside it would work. Same shape as `fileReader`.)* Diffing against master cannot work — T013, T018, T033, T036, T037, T042, T061, T063, and T064 all modify `AssemblySession.cpp` earlier in this same feature, so that diff is never empty and the criterion 023 gates on would go unverified. The claim is that *adding a dialect* touches none of the three, which is a property of the adding commit
- [x] T071a [P] Add a **breaking changes** entry to `CHANGELOG.md`, as its own heading rather than inside the feature announcement: `CassoCli input.a65 -o out.bin` no longer works and becomes `CassoCli as65 input.a65 -o out.bin`, and a bare `CassoCli as65` stops resolving `as65` as a source filename (both T049a). **Written as part of T049a's single commit, not afterward** — this entry is the deliberate-act record that 020's tripwire test was protecting. A reader scanning for what will break must not have to find it inside a paragraph about dialect support. State the replacement invocation literally, so the entry is copy-pasteable into a build script
- [ ] T071 [P] Update `CHANGELOG.md` with the merlin subcommand, the dialect mechanism, and the corrected include-file diagnostic attribution
- [ ] T072 [P] Update `README.md` with the new dialect, the updated test count, and the roadmap position relative to `023-ca65-dialect`
- [ ] T073 [P] Document the supported subset and where it ends in the repository docs, deriving the list from `CassoCore/MerlinSubsetBoundary.cpp` (SC-008)
- [ ] T074 Run `scripts/RunDormannTest.ps1` — required for assembler changes
- [ ] T075 Run `scripts/RunHarteTests.ps1 -SkipGenerate` — required for assembler changes
- [ ] T076 Run `scripts/Build.ps1 -RunCodeAnalysis` and `scripts/CheckStyle.ps1`, and confirm x64 Debug and Release are both green
- [ ] T077a Update `CLAUDE.md`'s spec inventory **at merge time**, not before: move 019 from "drafted but NOT started" to shipped, and strike the sequencing note that says 023 must wait on it — 023's gate is satisfied once this lands. Deliberately deferred rather than done during the feature, because `CLAUDE.md` names spec 020 as active and the concurrent session owns that block; editing it early would put the two specs in conflict on a shared file. Leave the active-spec pointer alone entirely — that is 020's to change
- [ ] T077 Revert `.specify/feature.json` to `specs/020-disk-file-access` before merging, so master does not thrash between two concurrent specs. This is the **only** mechanism for that file — plan.md states the same, and staging by explicit path throughout the feature is what keeps it from being committed accidentally in the meantime

---

## Dependencies

**Phase order**: Setup → Foundational → US1 → US2 → US3 → Polish.

**Hard blocks**:

- T010 gates everything. If the seam changed behavior, no later phase's evidence is trustworthy.
- T016–T018 block T040 — the CPU-selection directive has nothing to switch without the provider.
- T020 and T020e block T027 onward. The harness and the fixture read path are what make any parser claim checkable; without them the parser has no oracle.
- The **settle-by-capture** entries block only the tasks whose semantics they settle, not the phase: T025a → T028, T022 and T025b → T029, T023 → T030, T025c/T025d/T025e → T038, T025 → T040, T025f → the conditional handling in T038's neighborhood, T024 → the expression work T026 records. Each is a specific question with a specific dependent, and pretending the whole block gates the whole phase would idle work the committed fixtures already unblock.
- The five vendor oracles block nothing — they are committed. T045a and the rest of the T045-series can run as soon as T020e lands, which is why they no longer sit behind capture.
- T056 blocks T057–T062a. The boundary table is the single source those all derive from, including its generated help text.
- T033 blocks T035–T040. All the new `Directive` tokens land in one commit so the exhaustiveness-checked `switch` breaks once rather than at every directive task.
- T005 blocks T051. The `--cpu` refusal reads the profile's `cpuSource`, so the field must exist on the seam before the parser can consult it.
- T049a is a **single commit** and cannot be split. `s_kSubcommands` on master holds only `{ "run", Subcommand::Run }`, so `Subcommand::As65` is reachable solely through the fallback; adding the `as65` row, removing the fallback, and updating the tests in separate commits leaves a midpoint where AS65 is unreachable and a bisect lands on it. It is also gated on spec 020's command-line work merging first — see the task for why the `disk` row changes the shape of what is being removed.
- T069 depends on the whole mechanism, so it lands last despite being the criterion 023 gates on.
- T070 must be evaluated against T069's own commit. Diffing against `origin/master` cannot work, because earlier tasks in this feature legitimately modify `AssemblySession.cpp`.

**Story independence**: US2 and US3 both depend on US1's profile existing, so they are not parallel with it. They are independent of each other and can proceed in either order once US1 lands.

## Parallel Execution Examples

**Setup** — all three are independent files:

```
T001, T002, T003
```

**Corpus capture within US1** — each writes a distinct entry:

```
T022, T023, T024, T025      # the settle-by-capture entries
T043, T044, T045            # the corpus floor completion
```

**Independent implementation within US1**:

```
T033 (new Directive tokens)   +  T034 (StringEncoding)
T036 (loop construct)         +  T037 (dummy sections)
```

**Within US3** — distinct refusals and distinct test files:

```
T059, T060                  # file-type and save-object refusals
T066, T067                  # negative corpus and boundary sweep tests
```

**Polish** — documentation is independent of validation:

```
T071, T072, T073
```

## Implementation Strategy

**MVP is Phase 1 through Phase 3.** At that point unmodified Merlin source
assembles to Merlin's bytes, which the spec calls "the entire feature." US2 and
US3 are refinements on top — valuable, but a developer can already stop porting
their codebase.

**Commit per phase**, per the constitution's commit discipline. Do not accumulate
phases into one commit.

**Validate cheaply on the branch**: compile the touched project and run the
narrowest relevant tests per commit; Release runs the suite in roughly 2 minutes
against Debug's 15. Defer the full gate — both extended suites, code analysis,
and style — to the pre-merge check in Phase 6.

**Merge with `--no-ff`.** CONTRIBUTING forbids squashing.

**One deferral is deliberate and already recorded**: the "unrecognized first
argument falls back to AS65" heuristic stays. Issue #92 wants it gone, but it
breaks the documented `CassoCli input.a65 -o out.bin` form and needs its own
decision plus a CHANGELOG entry. The reasoning is on issue #92; do not remove it
as part of this feature.
