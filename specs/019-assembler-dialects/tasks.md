---
description: "Task list for 019-assembler-dialects"
---

# Tasks: Merlin Assembler Dialect

## State of play

*Updated 2026-08-21. Keep this current or delete it, a stale status block is
read by whoever has no other way to check.*

**MASTER IS MERGED IN A SECOND TIME AND EVERY GATE WAS RE-RUN AFTER IT.**
`origin/master` at `17e175d7` (1.17.0, salvage a damaged disk, 27 commits)
merged as `cd3c132b`. Four conflicts, all resolved toward this branch:
`Version.h` stays 1.18.0 above master's 1.17.0; `CHANGELOG.md`'s `[Unreleased]`
now holds the 019 entries alone, three it carried having shipped in 1.17.0
(`--raw`/`--dos-bin`, format-flag-over-extension, the guest-paste fix) and the
two `### Added` headings the first merge left folded into one, checked by
listing every title rather than eyeballed; `README.md` keeps the dialect-named
examples with the last `--cpu` corrected to `-x`; `docs/Assembler.md` is this
branch's document plus the one sentence master's later revisions had that it
lacked. The first merge, `a25a3c67` as `b2cce9b1`, stands beneath it.

**Suite is 3512 Debug / 3509 Release, both green** (3462 / 3459 before the
second merge; the fifty are master's salvage tests). Code analysis 0 warnings,
CheckStyle OK over 143 files, Harte passes against the reduced set,
`scripts/RunMerlinOracles.ps1` reproduces all six shipped objects through the
executable, and `scripts/BuildDemoDisk.ps1` reproduces its committed image byte
for byte.

**T074's caveat is discharged by the merge rather than by new work**: the branch
now carries `bcaf69a3`, so `scripts/RunDormannTest.ps1` fails a bad download
instead of assembling the error page. The run still reports the documented
pre-Jan-2020 `zps` shift as informational.

**THE VENDOR SOURCES ARE COMMITTED AS WINDOWS TEXT, AND THE MERLIN SUBCOMMAND
CAN NAME ITS OUTPUT SHAPE (T092–T094). Both were user decisions.** Suite was
**3411** Debug / **3408** Release at that point, both green. Dormann and Harte
pass, style and code analysis are clean (0 warnings), and
`scripts/BuildDemoDisk.ps1` reproduces its committed image byte for byte.

**The fixtures could not be read by the tool they test.** Stored as the disk
holds them, the only decoder was in `UnitTest`, so the corpus was assemblable by
the test project and by nothing else, `CassoCli merlin` pointed at a fixture
saw question marks. Transcoding them made one file feed both, and made
`scripts/RunMerlinOracles.ps1` possible: all six shipped objects reproduced
through the executable, compared against the WHOLE file including the 4-byte
header, which is stricter than the corpus tests are. **Objects were not
touched.** The provenance chain now holds by re-running the extraction rather
than by hashing in place, and the licensing reading it rests on is stated in the
fixtures README rather than left implied.

**`--dos-bin` and `--flat` needed no engine work.** All three writers existed in
`CassoCore/OutputFormats` and the executable already dispatched on the shape for
any dialect; only the merlin grammar refused to name them. The default is
unchanged. **as65 still has no `--flat`**, so the vocabulary is not symmetric
across the two dialects: deliberately left, because the as65 grammar is a
hand-rolled walk and widening it is not this feature's business.

**One gap this closed was recorded as open below and is now stale in that
entry**: T073's note that a source containing `KBD` cannot be assembled from the
command line was answered by the `-d` flag landing earlier on this branch.

**THE FOUR DIVERGENCES THE CAPTURE FOUND ARE ALL FIXED (T082–T085), AND A FIFTH
WAS FOUND AND LEFT OPEN.** Suite is **3391** Debug / **3388** Release, both green
(from 3375 / 3372); Dormann passes and `scripts/BuildDemoDisk.ps1` reproduces its
committed disk image byte for byte. **Fourteen mutations over this slice,
fourteen caught**, three only after a test was added.

**No new `Directive` token, and nothing shared was parameterized by a dialect.**
The first-character conditional shares `Directive::If` with `DO` and folds while
the line is parsed, because the assembler can already assemble a block when a
value is non-zero. The three engine edits all read data that already existed,
the byte emitter and the pass-1 argument evaluator ask the evaluation context for
its character-constant delimiter, and the CPU-selection handler stops refusing an
operand, and `MacroSyntax` got NARROWER: `callKeyword` had one user, could not
represent the case Merlin refuses, and is gone.

**THE FIFTH DIVERGENCE IS FIXED AND THE VENDOR MACRO LIBRARY IS COMMITTED
(T086–T090). Both were user decisions.** Suite is **3402** Debug / **3399**
Release, both green (from 3391 / 3388). **Ten mutations over this slice, ten
caught**, one only after the assertion that catches it was added.

**Macro fall-through is an ENGINE change and was taken as a SPEC AMENDMENT**, in
the open, FR-032 in `spec.md` and "Amendment: overlapping macro definitions" in
`contracts/dialect-profile.md`, which says plainly that it is user-approved and
why guarantee 1 is relaxed there rather than bypassed. The collector keeps a
STACK of definitions instead of one: a further opening line pushes rather than
being collected, every open definition receives every following line, and one
terminator closes them all. **Every dialect gets it and none opts in**, no
profile field, no dialect named in the collector, and the as65 half of the tests
is written in `macro`/`endm` beside the Merlin half so a dialect-shaped special
case could not hide as the mechanism. One spelling moved the other way, OUT of
the engine and onto the seam: the operand-form opening keyword was the literal
`"MACRO"`, which read ` PUT MACRO LIBRARY` as defining a macro called `PUT`.

**THE LIBRARY NEEDED NO EXCEPTION TO THE INCLUSION RULE.** The rule admits half
of a source/object pair; what was too narrow was reading "ships no object" as
"has no oracle". **An oracle can be GENERATED**, an authored source that uses
the library, assembled under real Merlin Pro 2.23, and the 279 bytes it produced.
Casso reproduces every one. That makes the library an ordinary pair whose other
half simply did not exist in 1984, and it is now the only fixture covering
fall-through and the first-character conditional against real vendor code. The
README records it as a third KIND of file with the old reasoning kept intact.

**Two `PUT`/`USE` facts fell out of the capture, neither settled by any vendor
line**: a macro library must be reached with `USE` and not `PUT` (a `PUT` file may
not hold macro definitions, and Merlin defines nothing while saying nothing), and
a **space inside the operand is part of the filename**, with the comment
introducer ending it. Both vendor inclusions name a space-free file and carry no
trailing comment, so the old scanner reproduced every byte on the disk.

**One pending row remains, and it is new.** `PRINT` writes its conditional with
the parameter first, and real Merlin's test is purely positional, which answers
the question T084 recorded as structurally unsettleable, in Casso's favor, and
also means the vendor's own `PRINT` is broken for any message longer than one
character. What diverges is unrelated: Casso refuses an invocation whose untaken
branch names a parameter the call did not supply. See T090.

**PHASE 3 IS CLOSED APART FROM THE INTERACTIVE CAPTURES, AND PHASE 6'S
DOCUMENTATION IS WRITTEN.** T081, T033a, T071, T072 and T073 all landed. The
interactive-capture entries (T021–T025f, T043, T044, T045, T045b) and the
pre-merge gates (T074–T077a) are the only open work left in the feature.

Suite is **3352** Debug / **3349** Release, both green (from 3336 / 3333);
Dormann and Harte both pass. **Thirteen mutations over this slice, thirteen
caught**, one of them only after a test was added, because the property it
broke was not observable until then.

**`VAR` IS IMPLEMENTED AND THE UNEXPLAINED-REJECTION COUNT IS NOW ZERO (T081).**
The pinned assertion did its job: the fix made it fail at 9-expected-0-actual,
and it was driven to zero by fixing the cause rather than by relaxing the
number. The corpus-wide figure is now stated as an exact **0**, so a defect
appearing in a file whose own row nobody revisited still fails there. The
parameters bind under names the PROFILE supplies, because a reference is
rewritten while the line is parsed and the directive must produce the identical
name; the engine never learns what that name looks like. See T081's own line.

**T033a NEEDED NO RESOLUTION TABLE, AND THAT IS THE FINDING.** No Merlin
spelling collides with an instruction mnemonic, measured, not assumed, by
asking every dialect in the registry about every mnemonic in both instruction
tables. Disjointness IS the property the spec's edge case asks for: with the two
tables sharing no spelling, either lookup order gives the same answer.
`MerlinDirectiveTable::FromAmbiguousSpelling` was deliberately NOT added, since
an always-None lookup is dead machinery standing in for a guarantee. See T033a's
own line, including the one case the sweep structurally cannot see.

**A DEFECT WAS FOUND WHILE WRITING T073 AND IS NOT FIXED.** The `merlin`
subcommand has no `-d`, so a source containing `KBD` cannot be assembled from
the command line, while the assembler's own diagnostic tells the user to pass
exactly that flag, which the subcommand then rejects as unknown. Verified by
running the executable, recorded in [docs/merlin-subset.md](../../docs/merlin-subset.md),
and left alone because the flag table lives in `CassoCore/CommandLineParser.cpp`,
which spec 020 holds unmerged work in. **It wants an owner before merge**: three
of the five oracle programs are unreachable from the CLI without it.

**PHASE 3'S EVIDENCE WORK IS CLOSED. T020b, T020c, T020d, T045c, T045e, T045f,
T046, T046a, T046b and T047 all landed, plus two tasks that did not exist,
T080, which was implemented, and T081, which was found and left open.** The
interactive-capture entries (T021–T025f, T043, T044, T045, T045b) are untouched
and remain the only unfinished Phase 3 work.

Suite is **3336** Debug / **3333** Release, both green (from 3319 / 3316);
Dormann and Harte both pass. The net is +17 rather than +26 because nine
hand-written oracle methods were replaced by four sweeps over a table.
**Seventeen mutations over this slice, seventeen caught.**

**`DDB` WAS SILENTLY IGNORED AND IS NOW IMPLEMENTED (T080).** The previous
state-of-play block called this out as a null handler row no task owned, and it
was right: `Directive::WordHighFirst` had `{ nullptr, nullptr }` after every one
of T036–T042 had landed, and a null pass-1 row drops the line without a word. A
Merlin source writing it assembled to a program two bytes short at every
following address, in silence. It now emits two bytes per value with the high one
first, sizes itself in pass 1 through the ordinary word directive's own handler,
and is refused by as65; the token has no as65 spelling and must never acquire
one, so **this is not an as65-visible change and does not belong in T071's
CHANGELOG.**

**A SECOND UNOWNED GAP WAS FOUND (T081).** *Fixed in the slice above; this
paragraph records what it was.* `VAR`, Merlin's way of binding `]1`..`]n` so an
included fragment can be parameterized without a macro call, was absent from the
directive table. `PI.ADD.S` writes `VAR MSGPNT;OUTPUT` immediately before
`PUT SENDMSG`, so that line was rejected and the eight parameter references
inside the included fragment had nothing to resolve to: **nine rejections of
valid Merlin source with no boundary row behind them, which SC-003 defines as a
defect.** Nothing had been looking, because the existing boundary tests filter to
refusals and never counted the rest. It was pinned at exactly nine, per file and
corpus-wide, so T081 landing failed that assertion and forced the count down
deliberately.

**THE MACRO LIBRARIES ARE NOT STANDALONE SOURCES, and that was measured rather
than assumed.** `T.SENDMSG` is a macro BODY: its first line is an instruction and
its second writes to a positional parameter, so assembling it alone produces
seven expression errors and means nothing. Both committed libraries now enter
the corpus the way the vendor used them, served under the names the disk stores
while a real vendor source asks for them under the short names it writes, and a
test asserts each is actually REQUESTED, because serving a file nobody asks for
looks exactly like inclusion coverage and is none. T045f's list of eight
libraries is also wrong: only two were ever extracted.

**THE UNMODIFIED-SOURCE CLAIM IS NOW ASSERTED RATHER THAN INSPECTED (T046a),
AND IT IS THE ONE ASSERTION THE BYTES CANNOT MAKE.** An entry holds a fixture
PATH and never text, so it cannot carry a tidied copy; it carries no copy. The
sweep then re-derives the stored bytes straight from `IFixtureProvider::
OpenFixture`, strips the header and reads the declared length **in the test**
rather than through the decoder the comparison uses, and requires one character
of assembled text per stored byte, in order. The mutation that proves it earns
its place: a source given one extra line it did not have assembles to
byte-identical output, so SC-001, the load address and the AS65 discrimination
sweep all stay green and this test alone goes red.

**SIX HAND-WRITTEN ORACLE METHODS BECAME ONE TABLE AND FOUR SWEEPS.** Nothing
counted the six, nothing recorded which of them discriminate, and two lists of
oracles, the per-file tests and anything that wanted to enumerate them, would
have drifted. The floor is asserted as an EQUALITY before any loop runs: six
shipped objects from five sources, a measured figure rather than a target.
**Note what that floor is NOT**: it is the vendor-oracle floor, and spec.md's
Corpus Floor also requires the hand-authored entries T045 and T045b add, neither
of which is captured. SC-001 and SC-002 are therefore verified against the
disk's contribution to the floor and not against the whole of it.

**MERLIN IS REACHABLE FROM THE COMMAND LINE. PHASE 4 IS COMPLETE.** T048, T049,
T050, T051, T052, T053b, T054, T055 and T079 all landed; see each task's own line
for what diverged. `CassoCli merlin <source>` assembles end to end, and it was
run rather than assumed: the vendor `LABELS.S`, decoded off its DOS 3.3 fixture
into plain text, assembles through the executable to 984 bytes at `$8000` that
match the committed `LABELS` object byte for byte, exits 0, derives its object
name from the source when no `-o` is given, and exits 2 naming `XC` when handed
`--cpu`.

Suite is **3319** Debug / **3316** Release, both green (from 3286 / 3283);
Dormann and Harte both pass; `scripts/BuildDemoDisk.ps1` reproduces its committed
disk image byte for byte. **Seventeen mutations over this slice, sixteen caught**
, one of them only after a vacuous sweep was closed, and the seventeenth is
recorded at the code as uncovered on purpose (see T051).

**TWO as65-VISIBLE CHANGES, BOTH IN T071's CHANGELOG.** They are not incidental
and were taken deliberately:

1. **A run that named no `--cpu` now reports the target that stood**: on stderr
   under `-v`, and in the listing header when a listing is produced. That is the
   reporting table's last row read literally, and it fires for as65 as well as
   merlin because the row is about the CPU axis rather than about a dialect. It
   never reaches stdout, so a piped listing is unaffected in the case the
   contract spends most of its words on.
2. **The usage line no longer advertises the removed bare-source form.** It still
   read `CassoCli <source> [flags]`, which T049a had already made a lie; it is
   now swept from the subcommand table, as is the "expected one of" list.

**THE REPORTED CPU NEEDED A FACT THE ASSEMBLER WAS NOT REPORTING**, and that is
the one addition here no task named. `AssemblyResult::extendedSetSelectedInSource`
says whether the SOURCE selected the wider instruction set. Without it the CLI
cannot supply a truthful `CpuReport` for a dialect that selects its CPU in
source, the directive may sit inside a conditional, so an invocation that
passed no flag cannot tell "the default stood" from "the source chose the wider
set", and wiring `DialectReporting` at all would have meant announcing a
processor the assembly never ran on. Additive, defaulted to the reading every
existing result had, set where the switch happens, and asserted in both
directions.

**`DoAs65`'s exit codes were left alone.** T078's mapping is used by `DoMerlin`;
`DoAs65` keeps its own arithmetic because its 2 also covers four distinct write
failures the mapping does not model, and rewriting it would be an as65 behavior
change bought for tidiness.

**EVERY MERLIN DIRECTIVE IN THE TABLE IS NOW EITHER CARRIED OUT OR REFUSED BY
NAME.** T036, T037, T039, T040, T041 and T042 all landed; see each task's own
line for what diverged. *(This paragraph previously named `WordHighFirst`
(`DDB`) as the one exception, silently ignored and owned by no task. It is now
implemented as T080; see the block at the top. It then named a second gap that
was not a null row but a MISSING one, `VAR` being absent from the directive
table entirely; that is T081, and it is now implemented too. Every Merlin
spelling has a row, and every row is either carried out or refused by name.)*

Suite is **3286** Debug / **3283** Release, both green (from 3246 / 3243);
Dormann and Harte both pass; all six vendor oracle objects still reproduce byte
for byte; `scripts/BuildDemoDisk.ps1` still produces its committed disk image
unchanged. **Twenty-nine mutations over this slice, twenty-nine caught**, two of
them only after a test was added, and both of those were vacuous assertions
rather than missing ones.

**No as65-visible behavior or message changed.** Checked rather than assumed:
as65's spelling table (`Directive.cpp`) claims none of the tokens whose handlers
were filled, so no as65 source can reach any of them, and the two new fields on
`AssemblerOptions` / `AssemblyResult` are additive and default to empty. Nothing
here belongs in T071's CHANGELOG entry.

**PHASE 5 IS COMPLETE.** T063–T068 all landed; see each task's own line for what
diverged. Dormann and Harte both pass; `scripts/BuildDemoDisk.ps1` reproduces its
committed disk image byte for byte, which is how the message changes below were
checked against a real caller rather than only against tests.

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
are also done**; see the note on T069 for why its hold expired. Suite is
**3243** Release / **3246** Debug, both green; Dormann and Harte both pass. In
Phase 4, the conflict-free core subset is done: the **exit-code mapping (T078)**
and the **dialect-and-CPU reporting decision (T053/T053a/T053c)**. **Phase 5 is
complete**: the subset boundary is built and enforced (T056–T062a, T067), and
diagnostics are positioned, dialect-native and attributed (T063–T066, T068); see
below.

## The subset boundary: built, enforced, and reachable only from core

**Six constructs were recognized and then silently ignored.** `REL`, `ENT`,
`EXT`, `TYP`, `SAV` and every occurrence of `XC` had null handler rows, and a
directive with a null pass-1 row is dropped without a word, so a relocatable
module assembled "successfully" into bytes nobody asked for. That is the exact
degraded-reads-as-healthy shape the coding standards name, sitting in the
feature that exists to avoid it.

**The boundary is one table and the refusals are composed from it**, so guarantee
6 holds by construction. `CassoCore/MerlinSubsetBoundary.{h,cpp}` holds the six
rows and generates the help text; every diagnostic and every help line is built
from the same fields.

**It took FOUR files, not the two T056 named, and the split is the contract's.**
Guarantee 1 forbids the shared engine being parameterized by dialect, so
`AssemblySession` must not name `MerlinSubsetBoundary`. The row shape, the
lookup and the wording therefore live in `CassoCore/SubsetBoundary.{h,cpp}` as
mechanism, and Merlin's rows reach the engine through a new profile accessor,
`DialectProfile::GetSubsetBoundary`, defaulting to an empty span, following the
`GetOriginSemantic` precedent exactly. A profile stating no boundary cannot reach
the refusal path at all, which is what keeps the driver free of any dialect's
name. `MerlinDialect::GetSubsetBoundary` hands the table through and is the whole
of T057's share of `MerlinDialect.cpp`.

**A FOURTH reason class was needed, and the spec says three.** spec.md's
subset-boundary preamble names "needs a linker, needs a CPU Casso does not
emulate, needs a capability another feature owns", but FR-029 forbids
describing the save-object directive as another feature's, so it cannot use the
third. `SubsetBoundaryReason::NeedsItsOwnDecision` is the fourth, and its
widening text says out loud that disk file access will not settle it. **The
preamble's "three reasons" should be corrected to four.**

**The contract's stated workaround is INCOMPLETE and the message says more than
it does.** `contracts/merlin-directives.md` says the export-only module
"assembles on its own once relocatable mode is removed and an origin supplied".
Following that alone leaves six `ENT` lines behind, and each is refused in its
own right, so the message names all three steps: remove `REL`, drop the `ENT`
declarations, supply an origin with `ORG`.

**The linkage is a property of the MODULE, and that is why refusals are
deferred.** One `EXT` anywhere removes the workaround from every refusal in the
file, including those above it in the source, so composing the message where the
construct was met would have to guess. Offenders are collected through pass 1
and reported from `ValidateAssemblyCompletion`, each with the file it was met in
restored, the deferred-diagnostic rule `m_currentSourceFile` documents.

**Crossing the boundary stops the assembly before pass 2.** The refusals ARE the
answer; letting pass 2 run buries them under the undefined symbols a linker would
have resolved. That is T061's "collect every offender across the whole pass
before failing" read literally, and it is what makes `PI.START.S` report five
refusals rather than five refusals and a wall of noise.

**A refusal is distinguishable STRUCTURALLY, not by its wording.**
`AssemblyError::kind` is a new additive field defaulting to the reading every
existing diagnostic had. A test asserting only that a message contains some
phrase is the bare-substring trap this feature has already been caught by once,
so the assertion is on the field and the negative half, that a genuine syntax
error does not claim it, is asserted too.

**Two vendor files, two messages, and the counts are the evidence.**
`PI.ADD.S` produces exactly 7 refusals (one `REL`, six `ENT`), all carrying the
fix; `PI.START.S` produces exactly 5 (one `REL`, three `EXT`, one `ENT`), none
carrying it and two denying it. Both are assembled with `SAVOBJ` answered 0 so
the object-file directive inside the conditional stays unassembled.

**Twelve mutations, twelve caught, none needing a second try.** Among them:
never claiming a refused line, claiming every occurrence (so the first `XC`
would be refused), forcing the linkage both ways, dropping the workaround
selection, recording a refusal as an ordinary error, letting pass 2 run anyway,
stopping at the first offender, dropping the widening from the help text,
dropping a row from the help text, refusing constructs inside a skipped
conditional, and the profile returning an empty boundary.

**`XC`'s first occurrence is now implemented (T040), and the refusal path was
right that nothing about it assumed otherwise.** The second and every later
occurrence is still refused, by the same table and the same count. One thing did
change: because the first occurrence now really does select the wider table, an
assembly given only ONE instruction table is told so, which meant the boundary
fixture had to supply a second one, since `NoRefusedConstructAlsoFailsAsAParseError`
requires the refusal to be the only thing said about a refused line. That is the
configuration the `XC` row describes anyway. The unsettled question about a reset
form is **untouched**: an operand is refused rather than interpreted, so nothing
here answers T025.

**Two validation scripts were stale and one could not run at all.** T049a
removed the unrecognized-first-argument fallback without updating
`scripts/RunDormannTest.ps1` or `scripts/BuildDemoDisk.ps1`, both of which pass a
source file as the first argument. Dormann failed inside its own error handling
rather than reporting the refusal, so the suite T074 requires was unrunnable on
this branch. Both now name `as65`; fixed in its own commit.

## Diagnostics: positioned, dialect-native, and attributed

**A COLUMN TRAVELS WITH THE FILE, NOT WITH THE MESSAGE.** `AssemblySession`
already had one ambient value stamped onto every diagnostic, the originating
file, with a long note on its own member saying exactly where it is correct and
where it is not. The column is the same value at the same six sites, so every
caveat carried over rather than being rediscovered. The three deferred carriers
capture their own column beside their own file: the boundary offense, the open
conditional, and the macro definition.

**The ambient column answers for the LINE; a field-subject diagnostic says so.**
A duplicated label points at the label and an operand that will not evaluate
points at the operand, through a second recorder rather than by moving the
ambient value and trusting the next caller to reset it. A column of 0 there means
the field was never written and the line's own column stands in, which can never
invent a position for a dialect that records none, since that dialect's line
column is 0 as well.

**as65's diagnostics are asserted to be unchanged, not assumed.** FR-021 makes
the position fields additive, and "additive" is a claim about what did NOT change,
which is otherwise untestable. as65 records no columns, so a sweep over its
diagnostics requires every one to report 0.

**THREE as65 MESSAGES CHANGE, AND THAT IS USER-VISIBLE.** The origin and
reserve-space diagnostics hard-coded `.org` and `.ds`, spellings that appear in
neither dialect's table and are simply wrong at a Merlin line, where the source
wrote `ORG`. They now quote `ParsedLine::directive`, which makes as65 read `.ORG`
and `.DS`. Nothing in the tree pinned the old text, `BuildDemoDisk.ps1` produces
the identical disk image, and the alternative was a dialect branch in shared
mechanism. `.align` and the struct diagnostics were deliberately left alone:
those tokens are as65-only and unreachable under Merlin.

**A WORD THE ACTIVE DIALECT CANNOT EXECUTE IS NOW ATTRIBUTED**, and the answer
comes from the registry rather than from a comparison. `FindForeignConstruct`
walks every dialect but the active one and reports whether the word is a
directive there or an alternate instruction spelling, two categories, because
they are not interchangeable to a reader: a directive is source in the wrong
assembler's language, and an alias names an instruction the machine really has
under another name. The category fragment carries its own article, since a caller
cannot choose between `a` and `an` for a word it did not write.

**The indented label gets the column rule, and the engine supplies the one fact
the profile cannot.** Whether the field AFTER the unknown word names something
the assembler could execute is a question about shared, unnamed instruction
tables; a profile reaching into them to compose a sentence is the seam leaking in
the direction the contract spends most of its words on. So the engine computes it
dialect-neutrally (active profile's aliases, active profile's directives, shared
opcode table) and the profile decides what it means.

**A MACRO PARAMETER WITH NO ARGUMENT IS NOW AN ERROR**, which is a behavior change
and not only a diagnostic. It was substituted with empty text, so a call
punctuated for another assembler, the argument separator is the dialect's,
arrived as one argument however many were meant and assembled a *different
program* without complaint. All six vendor oracles are unaffected, which is the
evidence that real Merlin source never relied on the old reading; had one relied
on it, the rule would have had to go rather than the oracle.

**One vacuous test, found by mutation rather than by reading.** The macro entry's
byte assertion originally used a body that failed a step later whether or not the
call was refused, so the mutation that reported the mismatch and then expanded
anyway went uncaught. The body is now a data directive and a bare shift, both of
which assemble cleanly under the empty substitution, so the unrefused reading
emits three bytes of a different program in silence, and the assertion
distinguishes them. **Twenty-three mutations over this slice, twenty-two caught.**

**One guard is deliberately uncovered and is recorded at the code**, in the
pattern this feature has used three times before. The operand column of an
explicit macro invocation written flush against its name keeps the attached and
spaced spellings agreeing, and nothing can observe it: the operand column is read
only by operand-subject diagnostics, and none can arise on a line whose opcode
field is a macro call. Mutated, not caught, kept.

## Reporting and exit codes: decided in core, and NOW WIRED

*The heading below said "with nothing wired to the CLI yet" and no longer does.
T052 and T053b landed, so `DoMerlin` returns what `AssemblerExitCode` computes
and prints what `DialectReporting` returns. The paragraph is kept as written
because it records why the split happened in that order.*

**T078 and T053/T053a/T053c were taken as a deliberately conflict-free slice.**
All four are new files: `CassoCore/AssemblerExitCode.{h,cpp}`,
`CassoCore/DialectReporting.{h,cpp}`, `UnitTest/AssemblerExitCodeTests.cpp` and
`UnitTest/DialectReportingTests.cpp`. Nothing under `CommandLineParser`,
`CommandLineOptions` or `CassoCli/CommandLine` was touched, because spec 020
holds unmerged work in exactly those files. The consequence is that **nothing
calls either of these yet**, T052 and T053b are the wiring, and until they land
the reporting and the exit-code vocabulary are reachable only from the tests.
That is the intended state, not an oversight.

**Provenance is a field on `AssemblerOptions`, not something derived.**
`DialectSelection` (`Stated` / `Defaulted`) lives in `CassoCore/AssemblerTypes.h`
beside `dialect` and defaults to `Defaulted`. Deriving it was not an option: AS65
is both a dialect a caller can state and the value a caller that stated nothing
ends up with, so the dialect alone cannot say which happened. The default is the
safe direction, a stated dialect that forgot to say so is merely over-reported,
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
composes the line. Core's assembler has no CPU-target vocabulary of its own, 
instruction sets arrive as unnamed `Microcode` tables, so the alternatives were
inventing a second CPU enumeration in core to serve one report, or depending on
`CommandLineOptions::CpuTarget`, which would couple the assembler layer to the
command-line parse struct in a file spec 020 is editing. Asking for the CPU to be
reported without naming it is a caller bug and is rejected as one.

**A contradiction in [contracts/cli.md](./contracts/cli.md), resolved in favor of
the reading that can fire.** The reporting table's last row says a CPU left at
the dialect's default is "reported wherever the dialect is". Read literally, 
only where the dialect itself is reported, the row is unreachable: via the
command line the dialect is now always stated, so it would be reported nowhere,
and the row's own stated purpose ("so 'no directive was seen' is not read as 'the
flag was ignored'") could never be served. The row two above it settles the
question: a CPU selected in source is reported under `-v` even though the dialect
was stated, which only makes sense if the two axes are decided independently. So
"wherever the dialect is" means the same SINKS (stderr under `-v`, the listing
header when a listing is produced, never stdout) and not "only when the dialect
is also reported". Both halves are tested separately, so the choice is visible
rather than buried in an implementation.

**Macros and variables landed as ONE commit, deliberately.** Merlin writes a
positional parameter and a reassignable symbol with the same character (`]1` is
an argument and `]COUNT` a symbol, and the digit is the entire distinction) so
the two share one lexing decision and there is no ordering in which either is
correct alone.

## `MAKE DUMP.S`: REACHED, all 589 bytes

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
| Trailing hexadecimal bytes after a string operand | **done**, settled against the object |
| `TR` / `EXP` / `AST` listing directives, and `NAME = expr` equates | **done** |
| Macro parameters `]1`..`]n` with `;`-separated arguments | **done** (T038) |
| A parameter substituted INTO a symbol name, `LDX #A]1-ADRTBL` | **done** (T038) |
| Macro-body labels unique per expansion: `NI`, `ND`, `LP` each recur | **done** (T038) |
| **`ORG` moves the PC and not the output cursor** | **done** (T035e), the spec amendment |
| A LABEL on an `ORG` line never binds, `HEREINT ORG INTRFACE` | **done** (T035f) |
| `ERR \expr`, the "does this fit below" form | **done** (T035g) |
| Bare `ORG` with no operand | **done** (T035e) |
| An operandless shift meaning accumulator mode, `LSR` | **done** (T035g) |

**A twelfth class the earlier census could not name, because it is not a
diagnostic.** `LDA #>HEREMAIN-1` assembled cleanly and produced the wrong byte:
in Merlin the selector after the immediate sigil picks a byte out of the WHOLE
expression, where the shared evaluator's `<` and `>` are prefix operators
binding to the term beside them. Both readings agree on the LOW byte of every
such pair, so half the evidence matches either way, it surfaced only as two
wrong bytes out of 589, at offsets 28 and 57. Fixed as a parse-time operand
rewrite in the profile.

**The census also under-counted the character-constant class.** `CMP #"N"`
reported as an expression error, which reads as one problem and is two: `"` is a
second character-constant spelling AND it means high ASCII, matching the
convention Merlin's string directives take from their delimiter.

**Two guards are deliberately uncovered and are recorded at the code**, in the
pattern this feature has used twice before. The implied-mode test in the
operandless-accumulator rule cannot be reached, no mnemonic in either table
carries an implied and an accumulator encoding, and `m_segmentOutputPos` cannot
be told from `m_segmentPC`, because segment directives are as65-only and as65's
two cursors never part. Both were mutated and neither was caught; both stay,
because each is what keeps the property true by construction.

**`LABELS.S` now assembles WHOLE-FILE to all 984 bytes of `LABELS`, at `$8000`,
through the real assembler.** Not through the encoder in isolation; the file is
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
2. **`Directive::StringData` had no handler in either pass**: its row was
   `{ nullptr, nullptr }` exactly like `ErrorIf`'s, so the 105 `DCI` lines
   emitted nothing. Pass 1 now sizes a string by *running the encoder and
   measuring* rather than by counting characters, so a mode carrying a length
   prefix cannot make the two passes disagree about where the next label binds.
3. **`ERR` got its behavior**, in **pass 2** so its expression may name a forward
   label, which is the point of such assertions. Pulled forward from the
   T036–T042 band deliberately, as planned.
4. **Merlin has NO operator precedence.** This was on nobody's list and is the
   sharpest find of the slice. `LABELS.S` ends with `ERR END-LABTBL-1/$700`,
   bounding its own table at seven pages. Under ordinary precedence the division
   binds first, the expression collapses to `END-LABTBL` = 983, and the assertion
   fires on a file the vendor shipped a working object for. Folded left to right
   it is `(END-LABTBL-1)/$700` = 0. That settles the last of the contract's
   unsettled questions, "Do Merlin's expression operators and precedence match
   the shared evaluator?", from bytes rather than from the manual. The answer is
   **no**, on binding. *(The clause that followed, "the operator set itself is
   unchallenged so far", was true of `LABELS.S` and false of the disk. `CLOCK.S`
   challenges it: `!` is exclusive-or and `.` is inclusive-or. See the `KBD`
   section above.)*

**`/` in expressions was already supported.** Measured before writing anything:
`TokType::Slash` and `TryApplyDiv` have been in the evaluator all along. The
state-of-play line naming it as a gap was reading a requirement as a hole.

**`ExpressionEvaluator.cpp` WAS modified, and legitimately.** SC-009 (T070) names
it as one of three files that adding a dialect must not touch, but that criterion
is evaluated against **T069's own commit**, not against `origin/master`, T070
says so itself. The change here is one dialect-neutral branch: when
`ExprContext::binding` is `LeftToRight`, every operator flattens to the loosest
level, so the recursion for the right operand can absorb nothing. The operator
set, the folds and the diagnostics are untouched, and AS65 is unaffected because
the field defaults to `ByPrecedence`. **This is not an SC-009 violation and must
not be recorded as one later.**

**One engine bug fixed on the way.** `ClassifyPrelude` recognized the origin
directive by comparing the canonical *spelling* against `".ORG"`. A dialect
spelling it without a dot parsed correctly, resolved to `Directive::Org`, and
then silently did nothing, output at the wrong address with no diagnostic. It
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
  Pinned by both-spellings tests plus a mutation check, pointing the comparison
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

`EXITM` in `CheckForExitm` is the same shape and is **not** converted; it was
outside this slice's brief. It is the last one.

**Debug was red before this slice and nobody had said so.** Three
`MerlinFixtureTests` drive the fixture decoder's asserting EHM rejections, which
`SetupForUnitTests` routes to `Assert::Fail`; they passed in Release, where the
assertions compile away. Fixed with `ExpectedEhmAssert`, production code
untouched. **Report both configurations, not just Release.**

## `KBD`: DONE, and it was never interactive input

**`KBD` is a DIRECTIVE, not a terminal read.** It binds the symbol in its label
field to an answer given to the assembly, and every artifact that described it as
"interactive keyboard input" or as a permanent barrier was wrong. The answer path
already existed: `AssemblerOptions::predefinedSymbols`, the map behind `-d`. It
had no implementation at all, zero matches for the spelling anywhere in
`CassoCore`, so what looked like a design problem was an unwritten table row and
a handler.

**No answer supplied is an ERROR**, naming the symbol and quoting the prompt. The
two easier outcomes are both silent failures of exactly the kind this feature
exists to avoid: blocking on a prompt hangs an unattended build, and defaulting
the answer assembles a *different program* cleanly, since the vendor sources gate
whole sections on these symbols.

**It DID get a `Directive` token**, and guarantee 2 admits it. The assembler
could not already require a value from outside the source and say which value was
missing, `-d` binds whatever it is handed and can say nothing about what a source
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
   constant already needed for the same reason, and only a DATA test caught the
   pass-2 half, for the fourth time in this feature.
4. **`?` inside a symbol**, in both the label rule and the identifier lexer.

**A fifth is an engine correction rather than a Merlin fact.** T035f bound a
label sharing a line with an origin to the OUTPUT CURSOR. That agrees with the
right answer everywhere the two cursors are in step, which is everywhere `MAKE
DUMP` looks, and disagrees on a **bare** origin closing a relocated section,
which `CLOCK.S` has. `IRQEND ORG` closes a section relocated to `$BFC8`, and
`LDY #IRQEND-IRQHAND-1` is `$12` in the shipped object where the cursor reading
gives `$30`. The rule is now the plainest one available and has no dialect input
at all: **a label binds to the program counter as its line was reached**, exactly
like a label on any other line. as65's answer changed with it, and its test was
rewritten rather than special-cased.

**`&` against a character literal turned out not to be a gap.** The emit-cursor
slice listed it beside `?` as unbuilt. Measured: `#"Q"&$9F` and `$9F&"N"` already
worked, `&` was always bitwise-and and the high-ASCII delimiter already landed.
What actually broke on those lines was the OPERAND SCANNER: a character constant
may hold a space (`LDA #" "`), and a whitespace-delimited scan kept `#"` and
handed the rest to the comment field.

**`PRINTFILER.S` identified the vendor's own build configuration.** Its two
answers are semantic and which pair produced the shipped 286 bytes is recorded
nowhere; all four were tried and exactly one matches, **formatting on,
monitoring off**. The test asserts the COUNT as well as the pair, because more
than one match would mean an answer reaches no byte.

**The corpus caught a macro bug every synthetic test had missed.** `KEYMAC.S`
closes a macro with `NI <<<`, the label the body branches to sits on the line
that *closes the definition*, which is precisely the line closing a body throws
away. Twelve hand-written macro tests were green before the vendor source was
tried. That is the third demonstration in this feature that synthetic tests and
the corpus cover different sets, and this time it was the corpus's turn.

**`BLT`/`BGE` are done, and are dialect-scoped DATA rather than a branch.** The
profile supplies a spelling table and `Parser::ParseLine` rewrites the mnemonic
on the way out, so nothing downstream ever sees the alternate name. The
alternative (teaching the opcode lookup, the size estimator, the branch-range
check and the encoder each about a second spelling) is a per-dialect special
case in four places in shared mechanism for two words, which is what
`contracts/dialect-profile.md` guarantee 3 forbids and what SC-009/T069 exists
to catch. as65 declares no aliases and is swept to prove it.

**The other three oracle objects did not come along with them** at the time, but
they have since: `PRINTFILER` and `CLOCK` also needed the keyboard-input
directive, and both use macros. The aliases unblocked those objects without
delivering any of them.

**SC-004 re-verified after the engine change.** `AssemblerTests`,
`RegressionTests`, `IntegrationTests` and `OutputFormatTests`, 180 tests
between them, are green in both configurations, and the split was mutated
eleven ways to check they would have noticed: freezing the output cursor alone
fails twelve of them.

**Blocked on someone else.** T049 (explicit `as65` selector + fallback removal)
is **held until spec 020's command-line work reaches `master`**; see
[docs/coordination.md](../../docs/coordination.md). Nothing else is blocked.

**Carrying NO known incompleteness in the handler table, down from six.** Every
new `Directive` token has a row in `AssemblySession`'s handler table, and a null
pass-1 row means *not implemented yet* rather than *does nothing*, such a line
is dropped without a word. `StringData`, `ErrorIf`, `HexData`, `Loop`/`LoopEnd`,
`DummySection`/`DummySectionEnd`, `CpuSelect`, `ObjectFile` and now
`WordHighFirst` are all filled, and `MacroDef`/`MacroEnd` act through the
collection state rather than through their rows. *(`WordHighFirst` was the last
one and is T080; the incompleteness that followed it was a directive with no
table row at all, which is T081 and is now implemented, `ParameterBinding` has
both rows, and its pass-2 one acts rather than emits, as the assembly-time
assertion's already does.)* **`KeyboardInput`'s rows are null for
the third reason**, the one `Org` already used: it acts entirely in the pass-1
prelude, before a label can bind, so a row would never be reached. **The five
refused-by-name tokens are null for a FOURTH reason**: the boundary claims those
lines in the prelude, so their rows are unreachable by construction. Four
meanings for one null is worth watching, and each is stated at its own rows.

**Two guards in the macro work are deliberately uncovered, and are recorded at
the code rather than left to be rediscovered.** The digit test that separates
`]1` from `]COUNT` cannot be reached by any test: a macro body is stored as raw
text and re-parsed only after substitution, so a parameter reference never
reaches the profile's rewriter on a line whose parse is used. The bracket-depth
clamp in `Parser::SplitOnSeparator` is the same shape, variable references are
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
reasoning.** `ASC "TEXT"8D`, digits after the closing delimiter, is a run of
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
- [x] T002a Create `scripts/ExtractDos33File.ps1`, catalog walk and file extraction from a flat DOS-order image, stripping the DOS BIN header. **Throwaway capture tooling, not a product feature**: it exists only because the Merlin disk happens to be flat DOS-order, and it does not duplicate `020-disk-file-access`'s `disk get`, which is tested C++ spanning every mountable format including WOZ. Delete it if 020's extraction lands first. *(Validated against the DOS 3.3 System Master: FID extracts at load `$0803`, CHAIN at `$0208` / 453 bytes with the stripped payload matching the raw sectors after the 4-byte header.)*
- [x] T002 [P] Create `UnitTest/MerlinCorpus/README.md` documenting the capture procedure end to end: source goes in by typing or pasting into Merlin's editor, and bytes come back out via `scripts/ExtractDos33File.ps1`. Record the Merlin-version-per-entry rule. *(Two premises here are obsolete and the README must be corrected, not just extended: the developer no longer supplies their own image, `UnitTest/Fixtures/Disks/Merlin-proDos2.23.dsk` is committed, and "the disk image is never committed" is now false. Capture is for **adding** a fixture; the five vendor oracles need none of it.)*
- [x] T002b [P] Create `scripts/FetchMerlin.ps1`, retrieve the Merlin Pro volumes from the archive item and verify each against a pinned SHA-256. Recorded after the fact: the script was written during capture and had no task, which is how the provenance chain came to be re-runnable without being planned. It stays useful now that the volumes are committed, because it is what makes "these bytes are the archive's bytes" checkable rather than asserted
- [x] T002c [P] Create `scripts/ExtractMerlinFixtures.ps1`, lift the vendor source and object files off the hash-pinned image into `UnitTest/Fixtures/Merlin/`. Also recorded after the fact. This is the **only** sanctioned way to add a Merlin fixture; nothing in the test suite runs it, and the fixtures it produces must never be edited afterward
- [x] T003 [P] Create `scripts/CaptureMerlinCorpus.ps1` skeleton with `-Entry` and `-MerlinImage` parameters and usage text, calling `ExtractDos33File.ps1` for the read-back half

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The dialect seam, diagnostic positions, and the switchable instruction set. Nothing story-specific can begin until these land.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Seam extraction: must change no behavior

- [x] T004 Create `CassoCore/Dialect.h` with the `DialectId` enum and register it in `CassoCore.vcxproj`. *(Ships with `As65` only. `Merlin` was initially listed and the registry sweep failed on its first run, correctly: an enumerator with no profile resolves to the wrong profile while looking like support that exists. The enumerator lands with the profile in T027.)*
- [x] T005 Create `CassoCore/DialectProfile.h` declaring the abstract seam (identity, CPU-selection source, and `ParseLine`) and register it in `CassoCore.vcxproj`. *(Built with **one** virtual, not the four originally sketched: extraction showed line parsing is the honest boundary and the other three are internal to a profile that needs them. Virtuals get added when a dialect proves it needs one; see [data-model.md](./data-model.md).)*
- [x] T006 Create `CassoCore/As65Dialect.h` / `CassoCore/As65Dialect.cpp` holding today's grammar moved verbatim from `Parser::ParseLine`, and register both in `CassoCore.vcxproj`. **The AS65 directive spelling table does not move**: `DirectiveTable` keeps its global table and `GetAllSpellings()` accessor, and the profile delegates to them. Moving it would change `UnitTest/DirectiveTokenTests.cpp:70`, which sweeps that accessor, and T010 forbids exactly that
- [x] T007 Create `CassoCore/DialectRegistry.h` / `CassoCore/DialectRegistry.cpp` with the name-to-profile table and a `GetAllDialects()` accessor matching the `DirectiveTable::GetAllSpellings` pattern, and register both in `CassoCore.vcxproj`
- [x] T008 Route `Parser::ParseLine` through the active profile in `CassoCore/Parser.cpp` and `CassoCore/Parser.h`, moving the file-scope `StripComments` helper into the profile
- [x] T009 Add `dialect` to `AssemblerOptions` in `CassoCore/AssemblerTypes.h`, defaulting to `DialectId::As65` so every existing caller is unaffected
- [x] T010 Verify the seam changed nothing, **before any new test file is added**: full suite green in `x64\Release` AND `git diff --stat origin/master -- UnitTest/` shows no *existing* test file modified. Adding new files is expected later and does not violate this gate; editing one that already existed does, and means behavior moved with the code, stop and find out what
- [x] T011 [P] Add `DialectRegistry` sweep tests to `UnitTest/DialectMechanismTests.cpp` asserting every `DialectId` enumerator resolves to a profile, and register the file in `UnitTest.vcxproj`. Runs **after** T010, since it adds a file under `UnitTest/`

### Diagnostic positions

- [x] T012 Add `file` (default empty) and `column` (default 0) to `AssemblyError` in `CassoCore/AssemblerTypes.h`
- [x] T013 Route `RecordError` and `RecordWarning` through the current `PendingLine`'s `sourceFile` in `CassoCore/AssemblySession.cpp`. **Populate the position where the error is CREATED, not where it is reported**; that is the whole difficulty. Extending the error record is trivial; include attribution is only correct if the originating file is captured at the point of failure, since by reporting time the only file in hand is the top-level input. The value must also survive the trip out of core to reach the reporting site in the executable
- [x] T014 Make `ReportAssemblyDiagnostics` in `CassoCli/CommandLine.cpp` print the error's own `file` when set, falling back to the input path when empty so AS65 diagnostics are byte-for-byte unchanged
- [x] T015 [P] Add tests to `UnitTest/MerlinDiagnosticTests.cpp` proving a diagnostic raised inside an included file names that file rather than the top-level input, and register the file in `UnitTest.vcxproj`

### Switchable instruction set

- [x] T016 Create `CassoCore/InstructionSetProvider.h` / `.cpp` holding both the 6502 and 65C02 `OpcodeTable`s with an active selection, and register both in `CassoCore.vcxproj`
- [x] T017 Change `AssemblySession`'s `const OpcodeTable & m_opcodeTable` to a re-seatable pointer in `CassoCore/AssemblySession.h`, keeping `Assembler`'s existing single-`Microcode` constructor working
- [x] T018 Record the active instruction table **per line** during pass 1 and replay it in pass 2 in `CassoCore/AssemblySession.cpp`; never recompute, because conditional assembly can move where the directive is reached
- [x] T019 Verify SC-004: full suite green, with `UnitTest/AssemblerTests.cpp`, `RegressionTests.cpp`, `IntegrationTests.cpp`, and `OutputFormatTests.cpp` confirming AS65 output bytes are unchanged

**Checkpoint**: Seam in place, diagnostics carry position, both instruction tables held. User story work can begin.

---

## Phase 3: User Story 1 - Assemble existing Merlin source unmodified (Priority: P1) 🎯 MVP

**Goal**: A developer points Casso at unmodified Merlin source and gets the bytes Merlin produces.

**Independent test**: `UnitTest/MerlinCorpusTests.cpp` assembles every corpus entry and compares byte-for-byte against bytes captured from real Merlin Pro. Nothing reads a file or invokes another assembler.

### Corpus first: these settle open questions the parser depends on

**⚠️ The five vendor oracles no longer need capturing.** They are committed under
`UnitTest/Fixtures/Merlin/` and read through `IFixtureProvider::OpenFixture`, the
project's only sanctioned path to fixture bytes, `UnitTest/Fixtures/README.md`
states the Constitution II contract: no `std::ifstream` of host paths from test
code, and anything outside `UnitTest/Fixtures/` is a violation. **No test may read
`DevDisks/`.** Capture tooling exists only to *add* a fixture, via
`scripts/ExtractMerlinFixtures.ps1` against the hash-pinned disk.

Fixture format: raw DOS 3.3 bytes; skip the 4-byte BIN header, take the length
from bytes 2–3, mask bit 7, translate `$8D` to newline, compare objects from
offset 4. Do **not** assert bit 7 is set; `DCI` clears it on its terminator.
Never edit a fixture to make a test pass.

That reorders this phase: the byte-comparison work (T045-series) is unblocked
**now** and no longer waits on capture, while only the settle-by-capture entries
(T022–T025f), which need source authored here, still want the editor.

- [x] T020 [US1] Define the `CorpusEntry` shape from [data-model.md](./data-model.md) and the comparison harness in `UnitTest/MerlinCorpusTests.cpp`, serving multi-source entries through `UnitTest/MockFileReader.h`, and register the file in `UnitTest.vcxproj`. *(Comparison logic and its ten self-tests are done. The entry source changes: bytes now come from `IFixtureProvider::OpenFixture`, not from generated literals; see T020e.)*
- [x] T020f [US1] Add a **type-T** read path to `UnitTest/MerlinCorpus/MerlinFixture.h` / `.cpp`. DOS 3.3 gives a text file **no header at all**, `T.SENDMSG` begins with the literal characters `SE`, and its first four bytes read as a header claiming 50382 bytes of a 149-byte file. Kept as a separate entry point rather than sniffed from the bytes, because guessing the file type is the kind of inference that succeeds on the sample and fails on the next file; the type-B length check turns a wrong choice into a loud failure instead of text quietly missing its first four characters. Arrived with the two vendor macro libraries (`T.PI.MACS`, `T.SENDMSG`) that T045f wants- [x] T020e [US1] Read corpus bytes through `IFixtureProvider::OpenFixture` (e.g. `OpenFixture("Merlin/LABELS.S")`) and add a fixture-decoding helper covering the DOS 3.3 BIN convention once rather than per entry: skip the 4-byte header, read the length from bytes 2–3, mask bit 7 for source text, translate `$8D` to newline, compare objects from offset 4. It must **not** assert bit 7 is set, `DCI` clears it on the terminating character, which is exactly the encoding this corpus exists to pin. *(Landed as `UnitTest/MerlinCorpus/MerlinFixture.h` / `.cpp` with seven tests. Two findings from the fixtures themselves. The high-bit prohibition turned out to have a **second and much earlier** reason than `DCI`: Merlin stores source as high-bit ASCII **except spaces, which are plain `$20`**, 81 of them in `LABELS.S` alone, so a decoder asserting bit 7 would fail on the first space of the first line, long before reaching any `DCI` terminator. And the declared length is **verified** against the payload rather than skipped past, since all 13 committed fixtures carry an exact match, making the strict form free; a truncated or sector-padded extraction otherwise decodes into plausible bytes, which is the failure this corpus exists to catch.)*
- [x] T020d [US1] Assert a **non-zero entry count** in the corpus sweep, and assert the count against the corpus floor once the floor is met. This is the half of the absent-corpus guard T020a could not land, since counting needs the entry table T020e introduces; it is not a duplicate of it. Lands with the first real entry rather than now, because asserting it against an empty corpus would leave a permanently red test in the suite, which masks other failures and is its own version of a signal nobody reads
  *(Done, and the floor is stated as an EQUALITY rather than a minimum: six shipped objects from five sources is a measured figure rather than a target, so a table that grew or shrank should be seen. `TheVendorCorpusMeetsItsFloor` asserts the non-zero count first and both figures after, and it runs before every other sweep in the class because each of them is a loop. **Divergence: the floor asserted is the VENDOR-ORACLE floor, not the spec's Corpus Floor.** The manual-and-disk floor of spec.md needs the hand-authored entries T045 and T045b add, and neither has been captured, so the count assertion here pins what the disk supplies and will need raising when they land. Mutated by deleting a table row: caught.)*
- [x] T020a [US1] Make the corpus harness **fail when the corpus is absent**, not pass. A loop over an empty entry table reports success while covering nothing, which is the same failure shape as a stale test assembly and as an integration test whose data cannot be reached, success reported, coverage absent. Make an entry with empty expected bytes an error rather than a trivially satisfied comparison, and make two empty vectors comparing equal an error too; that is the worst case, since a naive comparison calls it a match. *(Done, and **only** that half. The entry-**count** assertions this task originally also claimed cannot exist yet: there is no entry table to count until T020e supplies one, so they are T020d's and the checkbox here covers the empty-expectation guards alone.)*
- [x] T020b [US1] *(**Done.** `discriminates` is a column on `VendorOracleEntry` and `EveryDiscriminatingEntryFailsUnderAs65` sweeps it: every entry carrying the flag is assembled under AS65 and required NOT to reproduce the shipped object. The three ad-hoc AS65 counterpart tests it replaces are gone, because two lists of oracles drift and only one of them gets counted. **Divergence: the flag is `true` on every row today**, which is asserted rather than left implicit; every vendor source is full of Merlin constructs, and an entry added later with the flag clear is a claim about itself that should have to be made deliberately. The consequence is that the flag's FALSE branch is unexercised: no committed entry is built from shared constructs alone, so nothing distinguishes the conditional from an unconditional sweep. Closing that needs a hand-authored shared-construct entry with captured bytes, which is T045's. Mutated by running the sweep's AS65 arm under Merlin: caught.)* Add a `discriminates` flag to `CorpusEntry` and have the harness in `UnitTest/MerlinCorpusTests.cpp` assert every entry carrying it **fails under the AS65 profile** as well as matching under Merlin. This closes the second vacuity shape: labels, origin, literals, and the evaluator are shared, so an entry built from those alone is green whether the Merlin profile works or is never consulted. An entry that passes under both dialects while claiming a Merlin construct is a defect either way; it is not exercising what it claims, or the profile is not being consulted. Shared-construct entries leave the flag clear and stay legitimate engine regression cover
- [x] T020c [US1] Set `discriminates` on every settle-by-capture and Merlin-construct entry as it is captured (T022–T025f, T043–T045), so the classification is recorded with the entry rather than reconstructed later *(**Done for every entry that exists, which is the six vendor oracles.** The classification is a column filled in as the row is written, so it is recorded with the entry rather than reconstructed. **The settle-by-capture entries this task also names are not captured**, so nothing there is unclassified; there is nothing there. Ticked because the discipline is in place and the sweep enforces it; it is not evidence that T022–T025f or T043–T045 have been done.)*
- [x] T021 [US1] Implement `scripts/CaptureMerlinCorpus.ps1` to assemble one entry under real Merlin Pro in Casso and emit source, bytes, and Merlin version as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), reading bytes back with `scripts/ExtractDos33File.ps1`
  *(**Done, and the script's one check turned out not to work at all.** `-Verify` read the extracted type-B source as plain ascii. Merlin source is HIGH-BIT ascii and the extractor strips only the four-byte header, so every byte decoded to a question mark and a file of question marks compares unequal to everything; the check reported DIFFERS for a perfect round trip in exactly the words it used for a total garble. A comparison that cannot pass is not a loose guard, it is no guard at all, and it had been that way since the script was written. Masking bit 7 fixes it, and the canonical copy is now written decoded, since that copy is what gets committed and a corpus entry is a C++ string literal. **Divergence: the assemble-and-emit half is deliberately still manual.** Merlin is an interactive program with a menu, an editor, an update-source prompt and an object-name prompt, and each transition has to be READ before the next input is sent, a script that assumes a prompt appends its lines to the previous composite's buffer and captures one entry's bytes under another's name. What the script does own is the parts a human gets wrong: the work-copy refusal, the absence check, and the round trip.)*
- [x] T021c [US1] Make **delete-before-assemble** a required step of capture: delete the target object from within DOS before every assembly and confirm its absence with `scripts/CaptureMerlinCorpus.ps1 -ConfirmAbsent`. DOS 3.3 catalogs carry no timestamps, so there is no equivalent of the test suite's staleness guard; this is the only freshness check available. Without it, an assembly that errors before saving leaves the *previous* entry's object on the disk, and capturing it records one entry's bytes as another's expectation: self-consistent, plausible, wrong, and it will never fail, because the assembler faithfully reproduces the first entry's bytes from the first entry's constructs. Absence after assembly proves nothing wrote it; presence proves *this* assembly did
  *(**Done for every capture, and strengthened rather than merely followed.** Each object was given a name that had never been on the disk, which is strictly stronger than deleting the target: absence beforehand is guaranteed rather than checked, and presence afterwards still proves THIS assembly wrote it. `-ConfirmAbsent` was run before every one anyway, because the point is to make "did Merlin succeed?" something the extraction step asserts rather than something a human watches for. **It fired for real once**: the disk filled up mid-session, the object save failed behind a `DISK FULL` prompt the script never saw, and the extraction refused rather than handing back the previous entry's bytes. The delete route itself is also proven, Merlin's Quit drops to BASIC, where `DELETE <name>` works and `BRUN MERLIN` returns to the menu.)*
- [x] T021a [US1] Read the source back off the disk and **commit that copy**, not the text that was pasted. The disk copy is what Merlin assembled, so it is the only text guaranteed to correspond to the captured bytes, and the entry becomes self-consistent by construction. This matters because Merlin's editor may normalize whitespace or column positions on save (it is column-oriented over a high-bit, CR-terminated format) which would otherwise fail every entry's verification and invite loosening the comparison until it guarded nothing. Keep the comparison and keep it loud, but treat a mismatch as information about the editor rather than a failed capture (issue #110)
  *(**Done. Every committed source is the copy read back off the disk.** Nine round trips, all CLEAN, which is also how the editor question got settled; see T021d. The comparison was NOT loosened; it was fixed, because it could not pass. Two mismatches were reported and both were information rather than failure: a missing trailing newline in the host-side intended text, and one real finding, the editor uppercases symbol text, so `Mixed = $22` comes back as `MIXED = $22`.)*
- [x] T021d [US1] **Settle on entry one**: does Merlin's editor store pasted source byte-for-byte, or normalize it? Record the answer in `specs/019-assembler-dialects/research.md`. *(Partially settled: the editor demonstrably tabs fields to fixed display columns, so the screen is not what was typed. Whether STORED bytes are normalized is still open.)* **The "how do you exit Add mode" blocker is CLOSED and this note was stale.** [quickstart.md](./quickstart.md), section "Driving Merlin under emulation", records the answer: `RETURN` as the very first character of a line exits; `ESC`, a bare `RETURN` mid-line, and `Ctrl-C` are all appended as text, which is what made it look unexitable. Read the quickstart before estimating this task, the stale note here has already produced one wrong estimate. **The fixtures pivot demoted it further.** It was written when every entry came through the editor, so an editor that silently normalized would have invalidated the whole corpus. The five vendor oracles are committed and never passed through the editor, so this now gates only the first **entry authored here**, T022 onward, and nothing in the byte-comparison path.
  *(**Settled: the editor stores what is typed, byte for byte.** Two leading spaces come back as two `$A0`s, three spaces after a label come back as three, and the spaces inside a comment come back too. No column padding, no whitespace collapsing, no tab expansion, the display's tab stops are rendering and nothing else. Recorded in research.md, and confirmed nine times over rather than once. **One exception, and it is the one that costs something: symbol text is UPPERCASED on entry.** So symbol case sensitivity cannot be settled through the editor at all, the case is gone before the assembler sees the line, and answering it needs a source placed on the disk by a route this project does not have yet. That is the gap, and it is recorded in the capture procedure rather than left as an open question, because a capture that CANNOT discriminate looks exactly like one that discriminated and agreed.)*
- [x] T021f [US1] Add the disk **work-copy** discipline to `scripts/CaptureMerlinCorpus.ps1` so it refuses to operate on the pristine image: capture writes source, writes objects, and deletes targets, all on irreplaceable commercial software the developer supplied and this repository cannot regenerate. The procedure now mandates a copy; the tooling should enforce it rather than rely on remembering
  *(**Was already implemented and is now verified end to end.** The script hashes whatever image it is handed against the same pin `FetchMerlin.ps1` verifies and refuses when the bytes are the vendor's. Both arms were exercised for real during capture: the pristine image was refused by name, and the work copy was accepted with its hash printed at every step. The pristine image's hash is unchanged after the whole session.)*
- [x] T021e [US1] Record in `UnitTest/MerlinCorpus/README.md` the residual gap automation cannot close: if the paste garbled *and* Merlin assembled the garbled source, the entry is self-consistent and simply tests a construct nobody intended. The `discriminates` flag catches the worst version, a garble that destroys the Merlin construct stops the entry failing under AS65, but not one that merely changes an operand. Read the first few entries by eye; nothing downstream will report it. **This guard has a demonstrated near-miss, not a theoretical one**: the first version of `SendCassoKeys.ps1` corrupted every shifted character, and it surfaced only because the garbled text happened to be a BASIC syntax error. Had the first thing typed been valid either way, subtly wrong source would have been assembled faithfully and its bytes captured, self-consistent, wrong, and caught by none of the five automated axes
  *(**The paragraph this asks for was already in the document; what this adds is a SECOND gap, which is narrower and certain rather than conditional.** The editor uppercases symbol text, so no capture entered through it can discriminate symbol case at all, and a capture that cannot discriminate looks exactly like one that discriminated and agreed. Recorded beside the first. The operational lessons went in with it: the `.S` and `T.` name manglings, the disk filling up mid-session, the prompt that has to be READ before the next input, and the diagnostics that end the assembly so only the first bad line is ever reported.)*
- [x] T021b [US1] Batch constructs into a few **composite** source files rather than one file per construct: assemble once with the listing on, save the object, extract, and split by known offsets. A handful of composites covers the FR-007..FR-015 floor at a fraction of the typing
  *(**Done: eight typing sessions produced twenty entries.** The design is recorded as a table in the capture procedure. **The splitting rule needed stating and the task did not state it: split by a MARKER, never by counting bytes.** A four-byte `HEX DEADBEEF` between sections splits the object programmatically; counting offsets by hand is exactly where a self-consistent-and-wrong entry gets made. And splitting is only sound where the sections are POSITION INDEPENDENT, data directives with no labels and no branches assemble identically wherever they sit, so each segment stands alone, while a section holding a branch or a program-counter reference cannot be split out and stays one entry. The strings composite is the payoff: twenty-one typed lines, one assembly, eleven independently named expectations.)*
- [x] T022 [P] [US1] Capture irregular-spacing entries (extra spaces, tabs, and mixtures) as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle the field-based line model empirically
  *(**Done, and the answer is that Merlin COLLAPSES a whitespace run.** `  LDA #$42` with two leading spaces is an ordinary instruction, and `LBL   LDA #$43` with three spaces after the label is a label plus an instruction, neither pushes the mnemonic into a later field. The trap is that the EDITOR's display tabs one field per space, so the screen shows the mnemonic at a later stop while the bytes say otherwise; reading the answer off the screen gives the opposite result. Captured as the `line model` entry, which is also the corpus's only row with the discriminating flag CLEAR; see T020b, whose false branch was unexercised until now.)*
- [x] T023 [P] [US1] Capture mixed-case and long-symbol entries as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle symbol case sensitivity, length limit, and legal character set (research.md CHK008, CHK009)
  *(**Two of the three answered; the third cannot be answered this way and that is the finding.** LENGTH: symbols are significant to **13 characters** and a 14th is a hard `Bad label`, not truncation, measured with names of 13, 14, 15 and 16 characters, where only the 13 lands in the symbol table and every reference to the others then draws `Unknown label`. CHARACTER SET: `?` is legal, captured in the `symbols` entry, confirming from a source authored here what the vendor disk had only implied. **CASE: unanswerable through the editor.** It uppercases symbol text as it is typed, `Mixed = $22` is stored as `MIXED = $22`, so the case is destroyed before the assembler sees the line and no capture entered this way can discriminate. Recorded as a gap in the capture procedure rather than left looking settled; closing it needs a source placed on the disk by a route that does not exist yet.)*
- [x] T024 [P] [US1] Capture expression entries covering Merlin's operator set, precedence, and the current-program-counter form as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) (research.md CHK052)
  *(**Done, split into three entries so a failure names its construct.** `expression operators` covers left-to-right evaluation, the exclusive-or and inclusive-or spellings, bitwise and, the binary literal and the current-program-counter form both as a value and in a difference; two more cover the character constants and the negative literal beside both word orders. **Every byte was hand-derived from the manual BEFORE the capture and all sixteen agreed**, which is the cross-check T026 asks for, discharged against a whole entry rather than a sample. **It also found a defect**: ` DFB "A"` is `C1` under Merlin and `41` under Casso, because a quoted argument reaches the other dialect's string-literal path instead of being read as a high-ASCII character constant. That row is captured and pinned as pending; see T082.)*
- [x] T025 [P] [US1] Capture a `XC OFF` entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle whether Merlin accepts a reset form (spec Edge Cases)
  *(**Answered: there is NO reset form, so T026's conditional does not fire and the one-way state transition stands unamended.** The probe is discriminating rather than merely quiet, `XC` / `PHX` / `XC OFF` / `PHX`, where the second `PHX` is an instruction only the wider processor has. Merlin assembles both to `$DA` and reports no error, so the operand is accepted and changes nothing. **The divergence is in the other direction and is real**: Casso REFUSES an operand here, deliberately, on the grounds that the question was unsettled. It is settled now, and the refusal rejects source real Merlin assembles; see T083. No corpus entry is committed for it, because the entry cannot pass until that lands; the bytes and the reasoning are in research.md.)*
- [x] T025a [P] [US1] Capture the comment-field experiment as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file): one line whose fourth field begins with an ordinary word and would be a syntax error if parsed as anything but a comment. Acceptance confirms comment-by-position; an error proves `;` is required. Keep the entry either way, to pin the answer against regression
  *(**Answered: comment-by-position is confirmed.** ` LDA #$44 THIS IS A COMMENT` assembles clean with four ordinary words in the fourth field and no introducer at all. Kept as part of the `line model` entry either way, exactly as the task asks, so a later change that starts requiring the semicolon is a failure rather than a surprise. This is what T028 implemented and was waiting on.)*
- [x] T025b [P] [US1] Capture quoted-string entries with **leading, embedded, and trailing spaces inside the quotes** as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file). A whitespace-delimited operand scanner breaks on these, and the disk's own `PI.START.S` is full of them; the spaces are payload bytes, so getting this wrong both truncates the operand and silently changes emitted data
  *(**Done as the `quoted spaces` entry**: ` ASC " AB C "` produces `A0 C1 C2 A0 C3 A0`, leading, embedded and trailing spaces all emitted as payload. Captured alongside two more operand-scanner rows from the same composite: a string whose delimiter is `!` chosen because the text contains a quote, and a hexadecimal run after the closing delimiter where the trailing `00` stays `00` rather than going through the high-bit convention.)*
- [x] T025c [P] [US1] Capture a macro fall-through entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), modeled on the vendor library's `ADDX MAC` / `TXA` / `ADDA MAC` with one shared `<<<`, to settle whether an unterminated `MAC` falls into the next. Also settles that the unterminated-macro diagnostic must not fire on legitimate vendor source
  *(**Captured, and it does fall through**: `ADDX MAC` / `TXA` / `ADDA MAC` with one shared terminator, invoked as `ADDX $10` and `ADDA $20`, produces `8A 18 65 10` then `18 65 20`, the first macro's body runs on into the second's. The entry is **pending rather than in the corpus**, and for a reason unrelated to fall-through: the same composite carries the first-character conditional, which has no spelling in the Merlin directive table, so Casso reports the definition as unclosed. That is T084, and it is precisely the diagnostic this task warned must not fire on legitimate source, it fires on a construct the vendor's own macro library uses thirteen times.)*

  *(**CORRECTION, measured on the executable while T084 was implemented: FALL-THROUGH DOES NOT WORK, and the diagnosis above is wrong in both halves.** The two constructs were tested apart. The conditional half alone reports six expression errors and never mentions a definition, because the missing spelling resolved through the OTHER dialect's table rather than going unrecognized. The fall-through half alone (`ADDX MAC` / `TXA` / `ADDA MAC` / `CLC` / `ADC ]1` / one shared `<<<`, invoked as `ADDX $10`) is what reports `Unclosed macro definition`, before and after `IF` landed: the inner `MAC` line is swallowed into the outer body, so the expansion opens a definition nothing closes. So this task's capture is genuine and its IMPLICATION was never implemented. It needs an owner; see the block at the top.)*

  *(**RESOLVED by T086. Both halves of the original note are now corrected in place rather than left to be read past.** The capture was always right that Merlin falls through; what it recorded wrongly was that CASSO did, and it attributed the unclosed-definition diagnostic to the missing `IF` spelling. Neither was true, and both were stated in the same sentence, which is worth remembering as a shape, because a composite entry lets one construct's failure be blamed on the other without anyone noticing. The two halves were tested apart only after the row had been sitting in the pending table for a full slice. The composite now assembles whole and has moved into the corpus proper as `macro fall-through and first-character conditional`.)*
- [x] T025d [P] [US1] ~~Capture a macro-local label entry~~, **ANSWERED BY THE DISK; no capture needed or planned.** The premise was that the vendor library "carefully never creates" the case. The library does not, but the vendor **program** does: `MAKE DUMP.S` expands `INCD` twice and `STORE` three times, and each expansion redefines `NI` / `LP`, while `DECD` redefines `ND`. A shipped, working 589-byte object is therefore proof that Merlin makes macro-body labels unique per expansion, the second of the two outcomes this task was written to distinguish, settled from bytes rather than from an experiment. `KEYMAC.S` adds a detail no capture would have thought to try: the label may sit on the terminator line itself (`NI <<<`). Implemented in T038; the corresponding tests are synthetic because the *rule* is now known, so a capture would only re-confirm it
- [x] T025e [P] [US1] Capture a `>>>` macro-invocation entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file). The vendor library invokes macros by bare name only, so the disk can never report whether an explicit invocation prefix is also accepted, and a user's source may well contain one. First instance of the general rule that absence from the disk is not absence from the language
  *(**The construct is IMPLEMENTED and the capture is still open**; those are different things, and conflating them is how an unverified guess becomes a settled fact. Both spellings work, spaced and flush against the name, and both are covered by tests; the macro's name is taken as the first `;`-separated item of the operand. Evidence status: **UNVERIFIED**, marked as such at `s_kpszExplicitCallKeyword` in `CassoCore/MerlinDialect.cpp` and in the test's own comment. The tests prove self-consistency and nothing about real Merlin. What capture must still settle: that the prefix is accepted at all, and whether the name is separated from the arguments by the same character that separates the arguments from each other. `PMC`, its documented word synonym, is deliberately NOT implemented.)*

  *(**Captured, and all three of the unverified claims above are FALSE.** Measured against Merlin Pro 2.23, one form per assembly because the diagnostic ends the run: `>>>NOPS` flush against the name is **refused** (`Not macro`), `>>> NOPS` with the name in the operand field is accepted, `PMC NOPS` is accepted and behaves identically, `PMC ADDA;$30` with name and argument joined by the separator is **refused**, and `>>> MOV2 $10;$20` is accepted with both arguments bound. So Merlin takes the name from the OPERAND field and the arguments from the field AFTER it: the name is separated from the arguments by a SPACE, and only the arguments are separated from each other by the macro separator. The flush spelling the implementation accepts does not exist, the separator it uses between name and arguments is wrong, and `PMC`, left out on the grounds that implementing it would double an unverified surface, is a real spelling. The entry is captured and pinned as pending; the fix is T085.)*
- [x] T025f [P] [US1] Capture the vendor library's five-deep nested first-character conditional (`MOVD`) as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) as a stress entry. This is how Merlin macros dispatch on addressing mode, so any macro library of consequence exercises it
  *(**Captured, and it found the largest gap of this phase. Two corrections to the task's own framing first.** `MOVD` nests **three** deep, not five, counted in `T.MACRO LIBRARY` on the distribution disk. And the construct is not a conditional in the ordinary sense: it is `IF <char>=]n`, which compares the FIRST CHARACTER of a parameter against a literal character. **The spelling is absent from the Merlin directive table entirely.** It is not obscure, the vendor's own macro library uses it thirteen times, in `MOVD`, `LDHI`, `ADD`, `SUB` and `PRINT`, and it is how every Merlin macro of consequence dispatches on addressing mode. It went unnoticed because that library is not one of the two committed as fixtures and no committed source reaches it. A three-way dispatch modeled on `MOVD` was captured (`DISP (ZZ),Y` gives `EA`, `DISP #5` gives `E8`, `DISP QQQ` gives `C8`) and pinned as pending. Fix is T084.)*
- [x] T026 [US1] Cross-check a sample of captured entries against hand-derived expectations from the Merlin manual, and record the answers to all settle-by-capture items in `specs/019-assembler-dialects/research.md`. **If T025 shows a CPU-target reset form exists**, amend FR-015 and the `InstructionSetProvider` state transition in `data-model.md`, both currently describe a one-way `base → extended` change, and add the implementing task before T040 rather than discovering the conflict during it
  *(**Done, and the conditional does NOT fire: T025 shows there is no reset form, so FR-015 and the state transition in data-model.md are correct as written and were not amended.** The cross-check was run against whole entries rather than a sample, and it is stronger than the task asks for: every byte of the `expression operators`, `structure`, `symbols`, `macros`, `explicit macro call` and `inclusion` composites was hand-derived from the manual BEFORE the capture and the totals agreed exactly, 16, 14, 21, 10, 10 and 6 bytes. Agreement across six independent programs is what discharges "did the emulator run Merlin correctly on the day"; a disagreement anywhere would have been either a corpus error or an emulator bug. All settle-by-capture answers are recorded in research.md under "The settle-by-capture answers". **Four divergences were found and each has an implementing task rather than a silent amendment: T082 through T085.**)*

### The Merlin profile

- [x] T027 [US1] Create `CassoCore/MerlinDialect.h` / `.cpp` as a `DialectProfile` subclass, register it in `CassoCore/DialectRegistry.cpp` and `CassoCore.vcxproj`, and **add the `DialectId::Merlin` enumerator in the same change**. The enumerator is added *with* its profile, never ahead of it, a placeholder resolves to the wrong profile while looking like support that exists, and `DialectMechanismTests` fails until the registry answers for it
- [x] T028 [US1] Implement Merlin comment conventions in `CassoCore/MerlinDialect.cpp`, asterisk in column 1 for a whole-line comment; a semicolon **beginning the field after the operand** introduces a trailing comment. **Not "a semicolon anywhere"**: inside the operand field a semicolon is data, and the disk's own macro library depends on it (`ADD SUMSTR;DEFLEN;PL`). Whether the introducer is even required, or whether a fourth field is a comment regardless of what starts it, is one of the settle-by-capture questions, T025a answers it and this task implements the answer (FR-007)
- [x] T029 [US1] Implement field-based line segmentation in `CassoCore/MerlinDialect.cpp`, whitespace runs separate label, opcode, operand, and comment; tabs are ordinary whitespace with no tab-stop expansion; no field is required at a specific column. **The scanner must respect quoting**: whitespace ends the operand only outside a quoted string, or `ASC "HELLO WORLD"` splits into an operand and a bogus comment. **And a `;` inside the operand field is data**, not a comment; it is Merlin's macro-argument separator (FR-007, FR-008)
  *(T027-T029 landed as one commit, deliberately. The operand scanner cannot be written without the comment rule, and neither is correct without the delimiter rule, so splitting them would have shipped a knowingly-wrong quoting rule for one commit. `DialectId::Merlin` and its registry row land here too, per the enum's own rule that an enumerator arrives WITH its profile, safe because `merlin` is unreachable from the command line until US2. **Directive spellings are NOT included**: `directiveToken` stays `Directive::None` until T032/T033, so this is an incomplete profile, not a broken advertised feature.*

  *Two findings from the vendor sources changed the implementation. `;` in column 1 is a whole-line comment, 8 such lines across 3 files, and it is not a special case: with no label, column 1 IS the first field boundary, so the general rule already covers it. And the string delimiter is **any character**, taken from the source: `ASC !" ASC ""!` in `KEYMAC.S` chooses `!` precisely because its text contains quotes, so a `"`-only scanner ends the operand inside the data. 164 of 166 string lines use `"`; the 2 that do not are why this is a rule about delimiters.)*
- [x] T030 [US1] Implement label rules and the local-label prefix in `CassoCore/MerlinDialect.cpp`, scoping locals to the enclosing global label (FR-008)
  *(**Divergence, deliberate**: the prefix is declared in `MerlinDialect.h` (one character, `':'`) but the SCOPING is in `AssemblySession.cpp`, because it is stateful and profiles are stateless and shared. A definition binds under the global label joined to the local name, and every local REFERENCE inside an operand is rewritten to match. The reference half is not optional: the vendor sources write `LDA :TABLE+5,X`, so a dialect answering only "is this a local definition" leaves every use unresolvable.*

  *The separator is a **period** because `Parser::ValidateLabel` rejects one in a label while the expression tokenizer accepts one inside an identifier. That pair of facts is the whole design: no symbol a source can spell may contain a period, so a scoped name cannot collide with a global however either is written, and the scoped name still resolves through the ordinary expression path rather than a second lookup. A local is validated as the name it SPELLS and stored under the name it BINDS to, since validating the joined form would reject every one of them.*

  *A local before any global label is an error rather than a symbol in an unnamed scope. A colon inside string payload is left alone, `ASC ":::6::6:6:"` is on the vendor disk, and rewriting there would change emitted bytes rather than resolve a symbol.*

  *Still open, and NOT part of this: **`?` is legal in a Merlin label**. `CMD?`, `CORR?`, `ISY?` and `RNGOK?` are in the vendor sources, and `Parser::ValidateLabel` rejects all four today. The corpus therefore already answers half of research's "legal label character set" question, ahead of T023 capturing it, but accepting `?` also needs the expression tokenizer to lex it, and that is a change to `ExpressionEvaluator.cpp` with no oracle forcing it yet. Variable symbols (`]name`) are T031 and were not touched.)*
- [x] T031 [US1] Implement variable symbols in `CassoCore/MerlinDialect.cpp` with reassignment semantics (FR-011)
  *(**No oracle exists for any of this.** Not one variable symbol appears in the nine committed vendor sources, the sigil occurs there only as `]1`..`]3` inside macro bodies, so every test is self-consistency plus the documented rule, and must not be quoted as corpus evidence.*

  *A variable binds in a NAMESPACE OF ITS OWN, because the sigil is part of the name: `]COUNT` and `COUNT` are two symbols and either may exist without the other. The stored name cannot keep the sigil, since the shared expression tokenizer will not lex it, so the profile rewrites both definitions and references to a prefixed form carrying TWO periods. That count is the whole design: an ordinary label may hold none, `ValidateLabel` rejects the character, and a scoped local binds as one label joined to another, so it holds exactly one. Nothing a source can spell can therefore collide.*

  *Reassignment reuses the existing mutable symbol kind rather than inventing one, which gives the rule its shape for free: a variable may be redefined, an ordinary equate may not.*

  ***Divergence, deliberate at the time and now LIFTED: the form where a variable stands as a program-counter label was REFUSED.** The reasoning was sound (pass 2 holds one symbol table, so a repeated program-counter symbol would point every branch at the last copy) but the fix already existed a few lines above it. `RebindMutableConstant` gives a reassignable symbol its value again where pass 2 reaches its definition, and a reassignable LABEL is the same problem with the program counter for an expression. `CLOCK.S` forced it: eight `]LOOP` targets, each branch meaning the one above it. It also opens no local-label scope, which no oracle can discriminate and a synthetic test does. See the `KBD` section in the state of play.*

  *One shared-engine fix came with it, and it is a two-pass disagreement rather than a Merlin matter. A mutable constant is now re-evaluated in pass 2 where its definition is reached. Pass 1 walks the file in order and sees each assignment in turn, which is what sizes the lines between them; pass 2 read one table built after pass 1 finished, so **data emitted in pass 2 took the value the file assigned last** while an instruction on the line above took the right one. An immutable symbol cannot show the difference, which is why it went unnoticed. The instruction-shaped test does NOT discriminate it; that was confirmed by mutation, and a data-directive test was added because of it.)*
- [x] T032 [US1] Add the Merlin directive spelling table to `CassoCore/MerlinDialect.cpp`, reusing existing `Directive` tokens wherever the operation is identical
- [x] T033 [P] [US1] Add **all** new `Directive` tokens this feature introduces in `CassoCore/Directive.h` and `CassoCore/Directive.cpp`, with their pass-1/pass-2 rows in `CassoCore/AssemblySession.cpp`: reversed-order words, raw hexadecimal data, the loop construct and its terminator, the dummy section and its terminator, CPU selection, and the single encoded-string token. Adding them in one task keeps the exhaustiveness-checked `switch` compiling once rather than breaking at each of T035–T040 (research.md D2, FR-009)
  *(The exhaustiveness check is real and fired exactly once, as this task predicted, but it is a `static_assert` on a handler ROW TABLE in `AssemblySession.cpp`, not a switch: "s_kRows must have one row per Directive". All 17 new tokens got rows in the same edit. Their handlers are null, meaning **not implemented yet** rather than "does nothing"; they are unreachable while as65 is the only selectable dialect, and T034-T042 fill them. Emitting nothing for a `HEX` line would be precisely the silent wrong-bytes failure this feature exists to avoid.)*

  *(`Directive` stopped being total over as65's spelling table here, which broke `EveryToken_HasACanonicalSpelling` correctly. Merlin tokens must NOT gain an as65 spelling, FR-005 forbids admitting one dialect's constructs into another, so the test's claim was narrowed to as65's own tokens, keeping the RMB regression it was written for. The totality that survives is checked in `DialectMechanismTests`: every token is claimed by **at least one** dialect. A token claimed by none is unreachable, which is the bug the original sweep actually defended against.)*
- [x] T033a [US1] Resolve directive spellings that collide with an instruction mnemonic by the active dialect's rule in `CassoCore/MerlinDialect.cpp`, using the `DirectiveTable::FromAmbiguousSpelling` precedent so resolution never depends on which table is consulted first (spec Edge Cases) *(**Done, and it did NOT need a resolution table, which is the finding.** Not one Merlin spelling collides with an instruction mnemonic, measured rather than assumed, by asking every dialect in the registry about every mnemonic in BOTH instruction tables, `OpcodeTable::GetAllMnemonics` being added so the sweep asks about all of them rather than about the ones somebody listed. Disjointness IS the required property: when the two tables share no spelling, consulting either first gives the same answer, which is exactly what "resolution must not depend on which table is consulted first" asks for. The rule is stated at the resolution site in `MerlinDialect::ParseLine` and enforced by the sweep, so a spelling added later that would shadow an instruction fails before it can ship.*

  *`MerlinDirectiveTable::FromAmbiguousSpelling` was deliberately NOT added. With no member it is a lookup that can only ever answer None, which no mutation can catch and no reader can trust, dead machinery standing in for a guarantee. The precedent is followed in what it GUARANTEES rather than in its shape: a spelling that is genuinely both stays out of the main table and is resolved from the operand, and `DirectiveTable::FromAmbiguousSpelling` is where Merlin's first one goes when it arrives.*

  *One thing the sweep CANNOT see, and it is worth recording because it looks like a hole: as65's `RMB` is ambiguous with a mnemonic the opcode table does not answer to. The table carries `RMB0`..`RMB7`, and the bit form is normalized into one of those from the operand, so a bare `RMB` in the main table collides with nothing the sweep can ask about and silently turns every Rockwell RMB into storage. That is the case the ambiguous table exists for, and it is why both mechanisms are needed rather than one. Three mutations, three caught: a directive spelled like a base-set instruction, one spelled like an extended-set instruction, and the mnemonic enumeration returning nothing.*
- [x] T034 [P] [US1] Create `CassoCore/StringEncoding.h` / `.cpp` implementing high-bit, inverse, flashing, and terminator handling per [contracts/merlin-directives.md](./contracts/merlin-directives.md), and register both in `CassoCore.vcxproj`
- [x] T035 [US1] Wire the five Merlin string spellings to one `Directive` token carrying a `StringEncodingMode` in `CassoCore/MerlinDialect.cpp`, including delimiter-driven high-bit inference (FR-010)
  *(**First byte-identical result against vendor object code.** `LABELS.S`'s 105 DCI lines reproduce 983 of `LABELS`'s 984 bytes exactly, through the real parser and the real encoder. The 984th is the `$00` of `END BRK`, an instruction rather than string data, and `END` there is a LABEL in column 0, not the END directive, which the parser gets right only because directive lookup reads the mnemonic field.*

  *DCI **inverts** the last character's high bit rather than clearing it. The distinction is invisible to the corpus, and the sabotage proves it: an implementation that CLEARS instead of inverting **passes all 983 vendor bytes** and is caught only by the synthetic low-ASCII test. That is the sharpest demonstration so far that a corpus of real vendor source is not sufficient on its own; it can only test what the vendor happened to write, and every DCI on the disk is high-ASCII.*

  *Three of the six encodings have **no oracle**: `INV` appears once, in a linker demo shipping no object, and `FLS` and `STR` appear nowhere. They follow documentation rather than bytes and are marked UNVERIFIED at each line. Same for the apostrophe half of the delimiter rule, `"` and `!` both give high ASCII, which disproves any ASCII-ordering rule, but no `'`-delimited string exists in the corpus.*

  *Also lands `GetDefaultOrigin` on the seam: Merlin defaults to `$8000`, as `LABELS.S` proves by containing no origin directive while its object loads there. as65 keeps 0. A wrong default yields byte-perfect output at the wrong address, which reads as a far deeper problem than it is.)*
- [x] T035a [US1] Give `Directive::StringData` and `Directive::ErrorIf` their handler rows in `CassoCore/AssemblySession.cpp`. **Recorded after the fact, and pulled forward out of the T036–T042 band on purpose**: `LABELS.S` is 105 string lines and one `ERR`, so the first whole-file oracle cannot exist without both, and leaving them null would have meant claiming an end-to-end result from a hand-rolled loop in a test. `ERR` acts in **pass 2**, where every symbol is known, because the assertions people write bound a table by the distance between its own two ends and one of those ends is always a forward reference. Its pass-1 row is `IgnorePass1Directive` rather than null, a directive with no pass-1 handler is not marked as one, and an unmarked line never reaches pass-2 dispatch, so a pass-2-only directive silently does nothing. The remaining null rows are still T036–T042's
- [x] T035b [US1] Give `Directive::HexData` its handler rows in `CassoCore/AssemblySession.cpp`, and accept a hexadecimal run after a string operand's closing delimiter. **Recorded after the fact**, and pulled forward for the same reason T035a was: `MAKE DUMP.S` cannot be read at all without them. One encoder serves both passes, so the size pass 1 reserves and the bytes pass 2 writes cannot disagree. *(The trailing-run form was previously REFUSED because nothing pinned what the digits meant; `MAKE DUMP`'s object settles it. The bytes are hexadecimal, and, the half no reasoning would have produced, they are NOT put through the delimiter's high-bit convention, which `00` staying `00` proves. An odd digit count is refused rather than padded, since both plausible repairs change every byte after it. Comma separators are accepted and marked UNVERIFIED: no vendor line uses one, and refusing a documented form can only cost a user source that assembles elsewhere.)*
- [x] T035c [US1] Add Merlin equates and the listing directives to `CassoCore/MerlinDialect.cpp`. **Recorded after the fact.** An equate puts its sign in the OPCODE field with the name beside it in the label field, so it is a field-model fact rather than an expression one, a parser hunting for the sign inside the operand finds it in the wrong place and leaves the line looking like an instruction named for it. The name must NOT also bind to the program counter, or the equate is reported as a duplicate of the label its own line just defined. 128 uses across the vendor sources and no other equate spelling; `EQU` is accepted as language rather than as oracle. `TR`/`EXP`/`AST` reuse `Directive::OptNoop` rather than each bringing a token whose handler would do nothing
- [x] T035d [US1] Add dialect-scoped instruction aliases to `CassoCore/DialectProfile.h` and `CassoCore/MerlinDialect.cpp`, `BLT`/`BGE` for `BCC`/`BCS`, resolved once in `Parser::ParseLine`. **Recorded after the fact**, and deliberately DATA on the seam rather than a Merlin arm in the instruction machinery: the alternative touches the opcode lookup, the size estimator, the branch-range check and the encoder, which is a per-dialect special case in four places in shared mechanism for two words (`contracts/dialect-profile.md` guarantee 3, SC-009/T069). as65 declares none and is swept to prove it (FR-005)
- [x] T035e [US1] **Separate the emit cursor from the program counter** in `CassoCore/AssemblySession.h` / `.cpp`. `AssemblySession` gains an output cursor beside `m_pc`; `ReserveBytes` is the one place both advance, and only an origin directive can part them. Which it does is profile data, `DialectProfile::GetOriginSemantic`, following T051's `cpuSource` precedent rather than a Merlin arm in the driver. A bare origin resyncs the program counter to the cursor, and is refused as a missing operand where the two cannot differ. **This is the spec amendment guarantee 1 asks for**, recorded in [contracts/dialect-profile.md](./contracts/dialect-profile.md); the gap is in the ENGINE, which had no way to express "advance the PC without advancing output" in any dialect. Editing `AssemblySession.cpp` is **not** an SC-009 violation: T070 is judged against T069's own commit, exactly as the `ExpressionEvaluator` binding change was
- [x] T035f [US1] Bind a label sharing a line with an origin directive, in `CassoCore/AssemblySession.cpp`. It never bound, because the origin claims the line as a prelude before `RecordLabel` runs (dialect-neutral, as65 dropped it too. ~~**The value is the OUTPUT CURSOR**~~) **CORRECTED: the value is the PROGRAM COUNTER as the line was reached.** The cursor reading agrees everywhere the two cursors are in step, which is everywhere `MAKE DUMP` looks, and disagrees on a **bare** origin closing a relocated section. `CLOCK.S` has one: `IRQEND ORG`, with `LDY #IRQEND-IRQHAND-1` reading `$12` in the shipped object where the cursor reading gives `$30`. The corrected rule takes no dialect input at all (a label binds where its line was reached, exactly like a label on any other line) and as65's expectation was rewritten with it rather than special-cased
- [x] T035g [US1] The four remaining `MAKE DUMP` diagnostic classes, in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp`. `ERR \expr` and the immediate byte selectors are parse-time operand rewrites in the profile; the assembler can already compute both, so guarantee 2 admits no new token; the operandless accumulator form and the double-quoted high-ASCII character constant are profile data, the latter carried into the shared evaluator through `ExprContext` exactly as `binding` already was. **Divergence, deliberate at the time and now CLOSED**: `?` in a label and `&` against a character literal were left undone, since nothing forced either while `KEYMAC.S` could not be an oracle. Both are settled in T035h. `?` needed the label rule and the identifier lexer together; `&` needed **nothing**; it was already bitwise-and and the high-ASCII delimiter already landed, and what actually failed on those lines was the operand scanner breaking on a space inside a character constant. Naming it as an unbuilt construct was a wrong diagnosis, not a wrong decision
- [x] T035h [US1] The keyboard-input directive and the four expression facts the remaining three oracles need, in `CassoCore/Directive.h`, `CassoCore/MerlinDialect.h`/`.cpp`, `CassoCore/AssemblySession.h`/`.cpp`, `CassoCore/AssemblerTypes.h`, `CassoCore/ExpressionEvaluator.h`/`.cpp` and `CassoCore/Parser.h`/`.cpp`. **Recorded after the fact**, in the T035 band because that is where it executes. `KBD` binds a symbol from `AssemblerOptions::predefinedSymbols` and refuses by name when no answer was supplied; it gets a `Directive` token, because requiring a value from outside the source and saying which one is missing is an operation the assembler could not already perform. Beside it: Merlin's own spellings for exclusive-or and inclusive-or, unsigned 16-bit arithmetic, `?` inside a symbol, and a variable symbol standing as a repeated program-counter label. The three expression facts are carried through `ExprContext` as profile DATA, exactly as the operator binding and the high-ASCII delimiter already were, so the evaluator gains no dialect branch. **Divergence:** T031's refusal of the program-counter variable is lifted, and T035f's label-on-origin rule is corrected; see both
- [x] T036 [P] [US1] Implement the loop construct and its terminator in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-011) *(**No `MerlinDialect.cpp` change was needed**; both spellings were already in the profile's table from T032, so the whole of this landed as a collecting state and two handler rows in `AssemblySession`. The body is collected as **`PendingLine` records, not text**, so every copy carries the line number and file its original was written at and a diagnostic in the third iteration still points at the one line the author wrote; a mutation flattening the line number is caught by exactly that. **Nesting costs a counter rather than a stack**: an inner block is body text to the outer one and is expanded afresh inside every copy of it. **The count is settled in pass 1**, because that is when the block becomes lines; a forward reference is reported rather than silently expanded zero times, which is a property of what expansion IS rather than a limitation worth working around. **A new bound, `kMaxLoopIterations` (32768), is Casso's guard on the pending queue rather than a claim about the dialect**, and is stated as such at the code: expansion is lines pushed onto a deque, so a mistyped count would otherwise be answered with memory instead of a diagnostic. No vendor source uses the construct, so there is **no oracle** for any of it.)*
- [x] T037 [P] [US1] Implement dummy sections and their terminator in `CassoCore/AssemblySession.cpp`, assign addresses, emit no bytes (FR-012) *(**The emit-cursor split T035e built is what made this small.** A section advances the program counter and leaves the output cursor alone, which is the second thing besides a relocating origin that can part them, so `ReserveBytes` stays the single place they move together and gains one branch. `LineInfo::emitsNoBytes` is recorded per line for the reason `usedExtendedSet` is: pass 2 walks the record rather than the source and cannot otherwise see which section a line sat in. **Nesting is refused rather than supported**; there is one place to return to, and a second entry would overwrite it, so the first terminator would restore the second section's origin and place every byte after it somewhere nobody asked for, in silence. **Two of the first assertions written here were vacuous**, both for the same reason and both found by mutation: closing a section restores the output cursor, so a section that wrongly emitted writes exactly where the next real line is about to, and the real line covers it. Only a section LONGER than what follows shows it, and only the first origin shows a section that wrongly claimed to have started the output. No vendor source uses the construct, so there is **no oracle**.)*
- [x] T038 [US1] Implement Merlin macro definition, positional parameters, and invocation syntax in `CassoCore/MerlinDialect.cpp`, reusing the existing `kMaxMacroDepth` limit (FR-013)
  *(The definition shape (`MAC` in the opcode field with the name in the label, closed by the triple-angle token) arrived earlier with the terminator spelling fix. This completes the substance.*

  ***Positional substitution ignores identifier boundaries, and that is the requirement rather than an oversight.** The vendor library splices a parameter into the MIDDLE of a name in both directions: `LDX #A]1-ADRTBL` pastes the argument after a prefix, `LDX #]1END-]1-1` before a suffix. A whole-word rule, which is what the named-parameter path correctly uses, leaves both unresolvable. Both directions have their own test, and they earned it: a mutation adding a right-hand boundary check breaks only the suffix one.*

  ***Body labels are unique per expansion with no declaration, and a label an expansion produced no longer opens a local-label scope.** The second half is not a refinement of the first. `MAKE DUMP.S` calls macros defining `LP` and `ND` in the middle of routines whose locals belong to a global further up, so a macro label becoming the enclosing global strands every local after the call, which is exactly what the file's diagnostics said before the fix.*

  ***The terminator line may carry a label.*** `KEYMAC.S` writes `NI <<<`, so the body's own branch target sits on the line that closes the definition, the one line closing a body discards. Twelve synthetic macro tests were green before the vendor source found it.*

  ***Reuses `kMaxMacroDepth` unchanged**, as the task asks; nesting still costs queue entries rather than C++ stack, so a Merlin macro calling another simply re-enters the expander one level deeper. Argument separator and parameter sigil are profile DATA, not a Merlin branch in the expander.*

  ***Divergence, minor: the explicit invocation is spelled only with the prefix form.** `PMC` is documented as its word synonym; neither appears on the disk, so implementing both would double the unverified surface for no evidence. See the state-of-play note.*

  *This answers T025d from the disk rather than from capture; see that task.)*
- [x] T039 [US1] Map Merlin's file-inclusion directives to `Directive::Include` in `CassoCore/MerlinDialect.cpp`, resolving relative to the including source and reusing the existing `kMaxIncludeDepth` limit (FR-014) *(**The mapping was already there from T032; what was missing is that the operand is NOT the filename.** `MerlinDialect::ResolveIncludeName` prepends `T.` at parse time, the same shape `RewriteAddressCheck` and `RewriteByteSelector` already use, so the assembler resolves an ordinary name and never learns this dialect writes them short. Measured, not assumed: `PI.START.S` says `USE PI.MACS` and `PI.ADD.S` says `PUT SENDMSG` for files stored as `T.PI.MACS` and `T.SENDMSG`, which is why both are committed. **Applied unconditionally, including to an operand that already begins with the prefix**, softening it to "prefix unless it looks prefixed" would invent a second rule on a case the corpus contains nowhere. Tested end to end against BOTH committed libraries: `T.PI.MACS` is resolved, read, and its `STADR` macro expanded to the eight bytes it produces, and `T.SENDMSG` is resolved under its type-T on-disk name with its own label binding as proof the file was walked rather than merely requested. **Divergence: "relative to the including source" is left as the existing `baseDir` behavior and NOT changed.** DOS 3.3 is a flat catalog with no directories at all, so every Merlin include sits beside the top-level source and the existing resolution already satisfies it; deepening it to the including file's own directory is an as65-visible change to nested includes that nothing here requires. `kMaxIncludeDepth` is reused untouched.)*
- [x] T040 [US1] Implement the first occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp`, switching `InstructionSetProvider` to the extended table for the remainder of the assembly (FR-003, FR-015) *(**Landed in `AssemblySession.cpp`, not `MerlinDialect.cpp`**; the spelling was already profile data, and the switch is engine work the profile must not reach into. The second and later occurrences are untouched: the boundary still counts and refuses them, and a test asserts the first taking effect did not disturb that. **`HasExtended` is asked first, and an assembly given only one table is TOLD.** A provider with nothing to switch to answers with the table it already had, so saying nothing would leave the source told it had reached a wider processor while the assembler stayed narrow, the exact degraded-reads-as-healthy shape. This made `MerlinSubsetBoundaryTests`'s fixture supply a second table, which is the configuration that row describes anyway. **An operand is REFUSED, and this deliberately settles nothing about whether a reset form exists**; that is T025's, to be answered by capture. Refusing says only that Casso implements the plain form; ignoring the operand would answer the question silently and wrongly, with "no such form exists, and writing one selects the wider processor anyway". A test asserts the refused line did not select anything on the way past. No vendor source uses the directive, so there is **no oracle**.)*
- [x] T041 [US1] Implement the object-file directive as naming the output in `CassoCore/MerlinDialect.cpp`, with the command line taking precedence over it (FR-027) *(**Landed in `AssemblySession.cpp` and `AssemblerTypes.h`.** `AssemblerOptions::outputFileName` carries the caller's answer and `AssemblyResult::outputFileName` reports the one in effect; the precedence is settled ONCE, in `Initialize`, which seeds the result from the caller so the directive has only to not overwrite a name that is already there. Carried on the options struct rather than left to the command-line layer for the reason `dialect` is: every entry point that assembles source resolves the same rule, and one resolving it differently would be a difference nobody finds until a build writes the wrong file. **Nothing sets `outputFileName` yet**, exactly as T078 and T053 are reachable only from tests until T052 wires the CLI, and `CommandLineParser`/`CommandLineOptions`/`CassoCli` were deliberately not touched, since spec 020 holds unmerged work there. The directive **names** an output; it does not write one, and a test pins that it takes no space. A later directive replaces an earlier one, the way a later origin does.)*
- [x] T042 [US1] Add diagnostics for unterminated dummy sections, loops, and macros at end of file in `CassoCore/AssemblySession.cpp`, naming the construct and its opening line as the existing unclosed-conditional diagnostic does (research.md CHK041) *(**The macro third was already done** and is only newly covered for Merlin's own spelling; the loop and section halves landed with T036 and T037 respectively, since each construct's carrier has to capture its own opening line at the moment it opens. **Each names the construct in the spelling the SOURCE wrote**, captured from `ParsedLine::directive` at the opening line; the engine has no reverse token-to-spelling lookup and adding one to a profile would widen the seam to serve one message, so the terminator is described in words (`no matching terminator`) exactly as the conditional's already says `no matching endif`. **All three are DEFERRED diagnostics and capture their own file and column**, per the rule on `m_currentSourceFile`: by the time they are reported the ambient answer belongs to whatever was processed last. That was initially untested for the two new ones and the mutation went uncaught; both now have an include-file test, and the four mutations over file and column are caught. **Two orphan-terminator diagnostics were added beyond the task**, for a loop terminator and a section terminator with nothing open; each was previously a null row, and a null row is dropped without a word.)*

- [x] T080 [US1] Implement the reversed-order word directive in `CassoCore/AssemblySession.h`/`.cpp`, two bytes per value with the HIGH one first (FR-009). **Found, not planned.** The T036–T042 band was described as filling every remaining null handler row, but no task named `DDB`, and `Directive::WordHighFirst` sat with `{ nullptr, nullptr }` after all seven of them landed. A null pass-1 row drops the line without a word, so a Merlin source writing it was assembled to a program two bytes short at every following address, in silence, the exact failure shape this dialect's vocabulary exists to prevent, left in place by a band that read as complete. *(Pass 1 REUSES `HandlePass1Word`: both reserve two bytes per argument whatever order pass 2 writes them in, and a second sizing function is a copy that can drift from the one the neighboring row uses. Only the emitter differs. **There is NO oracle**, the directive appears in none of the nine committed vendor sources, so every test is the documented rule plus self-consistency and none of it may be quoted as corpus evidence. Every value in those tests is deliberately not a palindrome, since a palindrome satisfies both byte orders and proves only that two bytes came out. **Not visible to as65**: the token has no as65 spelling and must never acquire one, which `As65DoesNotAssembleDdb` pins. Three mutations, three caught: the row reverted to null, the bytes emitted low-first, and pass 1 reserving nothing.)*
- [x] T081 [US1] Implement the positional-parameter binding directive (`VAR`) in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp`. **Found by T046b's sweep, not planned, and NOT implemented.** `PI.ADD.S` line 123 writes `VAR MSGPNT;OUTPUT` and line 124 writes `PUT SENDMSG`: the directive binds `]1`..`]n` so an included fragment can be parameterized without a macro call, which is how the vendor gives one body to several call sites. The spelling is absent from the Merlin directive table entirely, so the line is rejected and the eight `]1`/`]2` references inside `T.SENDMSG` have nothing to resolve to, nine rejections of valid Merlin source with no boundary row behind them, which SC-003 defines as a defect rather than a limitation. It is pinned at exactly nine, per file and corpus-wide, so this task landing fails that assertion and forces the count to zero deliberately. **No positive oracle**: `PI.ADD.S` ships no object of its own and is refused at the boundary regardless, so the evidence this can produce is that the rejections stop, not that the bytes are right *(**Done, and the pinned count went 9 -> 0 by fixing the cause.** `Directive::ParameterBinding` is a new token, `VAR` a new row in the Merlin spelling table, and the parameters bind as REASSIGNABLE symbols under names the PROFILE supplies, `DialectProfile::GetPositionalParameterSymbol`, defaulting to empty, following the `GetLocalLabelPrefix` precedent. The engine never learns what the name looks like, which is the point: a reference is rewritten while the line is parsed and the directive has to produce the identical name, so one rule is spelled once. `QualifyVariableRefs` now rewrites the sigil followed by a DIGIT as well as by a name, inside a macro body the expander replaces a positional parameter textually long before parsing, so one arriving at the parser is a reference outside any expansion, which is exactly what this directive serves. The old comment saying the digit must never be rewritten is corrected at the code.*

  *Both passes bind, through one helper. Pass 1 so the lines below are SIZED against the values in force, a parameter naming a zero-page address makes the instruction beside it two bytes rather than three, and pass 2 so a reference resolves against the binding above it rather than the last one the file made. That second half is RebindMutableConstant's problem in a second shape, and it is NOT redundant with pass 1: where pass 1 can resolve the expression it caches the value on the line, so only a binding pass 1 cannot evaluate shows the difference. A test built from two forward-referenced labels is what discriminates it, and a mutation making the first pass-2 binding win is caught by that test alone.*

  ***An expression pass 1 cannot evaluate is not an error there**, and that is forced by the vendor: VAR MSGPNT;OUTPUT binds OUTPUT, a label defined BELOW the fragment the next line pulls in. Refusing a forward reference would reject the one shape the directive exists for. So each diagnostic belongs to exactly one pass (how the line is WRITTEN is settled in pass 1, whether its expressions resolve in pass 2) because the line is visited by both and anything said in both is said twice. Mutating either gate is caught.*

  *The binding holds a VALUE rather than the text of its expression, which is what Merlin documents VAR as doing and what lets the shared symbol table hold the result. The divergence it implies is recorded at the code: a reference pasted into a longer identifier, which textual substitution inside a macro body would splice, lexes here as one symbol instead. No vendor line does it, so the corpus cannot settle it.*

  *Nine mutations over this task, nine caught, one of them only after a test was added. Binding the parameters as IMMUTABLE was not caught at first, because this directive overwrites whatever is there and only the assignment form checks the kind; the test that discriminates it is a VAR followed by an ordinary ]1 = expr, which the immutable reading refuses as a redefinition. That is the mutable kind made observable rather than asserted about the symbol table.*

### Corpus completion

- [x] T043 [P] [US1] Capture one entry per string-encoding spelling as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), five entries, not one, because a high-bit or terminator error still looks plausible
  *(**Done, and it is eleven entries rather than five, from one twenty-one-line composite split on a four-byte marker.** All six spellings: the family is six, not five, since `REV` is in the sources and missing from most descriptions of the dialect. Three of them had **no oracle at all** before this: `INV` appears once on the disk in a file shipping no object, and `FLS` and `STR` appear nowhere, so they were following documentation. `EveryStringEncodingHasACapturedEntry` asserts each spelling is present, because a row quietly dropped would take the only evidence for that spelling with it and leave every other test green.*

  *(**The sharpest result is `DCI 'ABC'` giving `41 42 C3`.** That is the pair the vendor corpus provably cannot settle: every `DCI` on the disk is high-ASCII, so its terminator always ends with bit 7 clear and an implementation that CLEARS rather than INVERTS reproduces all 984 bytes of `LABELS` exactly, confirmed earlier by deliberately introducing that bug and watching the comparison stay green. Only a low-ASCII string tells them apart, and the disk holds none. It now exists. `ASC 'ABC'` giving `41 42 43` settles the apostrophe half of the delimiter rule for the same reason.)*
- [x] T044 [P] [US1] Capture at least one multi-file inclusion entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), served through `UnitTest/MockFileReader.h`
  *(**Done**, and the included file was written to the disk by Merlin's own text-file command rather than smuggled in, so both halves are ours. The test asserts the name REQUESTED alongside the bytes: the source says `USE MYMAC` and the file is `T.MYMAC`, so an assembler asking for the name as written would draw a missing-file diagnostic that looks nothing like the wrong include it actually is.*

  ***A divergence nobody had asked about: `PUT` and `USE` are NOT one operation.*** *The identical file included with `PUT MYMAC` **silently does nothing**, no diagnostic, and the macro is undefined at its first call. `USE MYMAC` reads it and the definitions take effect. Both resolve the same filename; what differs is what the included text may contribute. The directive table maps both spellings to one token and the comment beside them says the difference is "which filesystem convention the name follows, not what the assembler does". That is now known to be wrong. Not given a task of its own, because what a `PUT` file may contain is a further capture question rather than a settled rule, recorded in research.md so it is not rediscovered.)*
- [x] T045 [P] [US1] Capture one entry per remaining construct in FR-007 through FR-015 and FR-027 as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), completing the corpus floor
  *(**Done for every construct the editor can reach.** The `structure` entry covers the loop and its terminator, the dummy section and its terminator, storage reservation, the conditional with its else arm, and the assembly-time assertion, **no vendor source uses the loop or the dummy section at all**, so before this row neither had any oracle. The `symbols` entry covers local labels reused under two different globals, a variable symbol reassigned between two references, `?` inside a name, and a symbol at the longest length Merlin accepts. **Two constructs are deliberately NOT captured and cannot be**: the object-file directive names an output that capture would have to read back through a path this project does not have, and the save-object directive is outside the subset by design and refused by name. Both are covered by the boundary table and the negative corpus instead.)*
- [x] T045a-partial, **superseded: all five sources and all six objects are done.** See T045a.
- [x] T045a [P] [US1] Validate against the **five** real positive oracles on the Merlin Pro 2.23 disk (source and shipped object both present, absolute mode) not "~40 files". Measured: `LABELS.S`→`LABELS` (984 @ `$8000`, 105× `DCI` plus one `ERR`), `KEYMAC.S`→`KEYMAC` (674 @ `$9000`), `PRINTFILER.S`→`PRINTFILER` (286 @ `$02A0`), `MAKE DUMP.S`→`MAKE DUMP` (589 @ `$9000`), and `CLOCK.S`→**both** `CLOCK.24` and `CLOCK.12` (365 @ `$0240` each). Five sources, six objects. Vendor source and objects **are committed**, under `UnitTest/Fixtures/Merlin/`, and read through `IFixtureProvider::OpenFixture`, the earlier "used, not committed" instruction is superseded
- [x] T045d [P] [US1] Prioritize `CLOCK.S`: one source producing two different objects through `DO HOURS-12` / `ELSE` / `FIN`, so a single capture yields conditional-assembly coverage **and** two independent byte-identical checks. The highest-value single entry on the disk *(Done, and it was every bit of that: it forced the operator set, the arithmetic width, the program-counter variable label, and the correction to T035f's label-on-origin rule, none of which any other source could discriminate. **Divergence from the task's own framing:** the two objects are selected by `VERSION`, an answer supplied to the assembly, NOT by `DO HOURS-12` alone, that inner conditional gates only which `SAV` line is reached, and `SAV` is out of subset. `HOURS` is derived from `VERSION` by arithmetic and is what the emitted bytes actually depend on. A third test requires the two results to differ, or the pair would be two copies of one check.)*
- [x] T045e [P] [US1] *(**Done, and the exclusion is now ASSERTED rather than observed.** `NoLinkerDemoSourceIsUsedAsAPositiveOracle` requires no entry of the positive table to name a `PI.` source, and `TheLinkerDemoSourcesAreTheOnlyOnesThatReject` requires the converse across the committed-source list, only the linker demo may be expected to reject. Both were one careless row away from being lost, and the row would have looked entirely reasonable. The two refusal counts and the two distinct messages were already pinned by `RelocatableWorkaroundTests`; what this adds is that the files can never quietly become positive comparisons.)* Use `Merlin/PI.ADD.S` and `Merlin/PI.START.S` as **negative** subset-boundary specimens only, never positive comparisons, they ship no objects, and the APPLE PI group is the linker demo whose own header says "This is just a test source for the linker". `PI.ADD.S` is the export-only shape (`REL` + **6** `ENT`, no `EXT`); `PI.START.S` is the no-workaround shape (`REL` + 3× `EXT` + 1 `ENT`). Between them they exercise **both** refusal messages, which is why exactly these two are committed: `PI.MAIN.S` and `PI.DIV.S` also import and are redundant with `PI.START.S`, and `PI.LOOK.S` is redundant with `PI.ADD.S`
- [x] T045f [P] [US1] *(**Done for the two libraries that are committed, and the task's premise about the rest is wrong.** Only `T.PI.MACS` and `T.SENDMSG` were extracted; the other six names here were never committed, so this task can only have covered two files and says so now rather than reading as six missing ones. Both are served through `MockFileReader` under the names the disk stores while real vendor sources ask for them under the short names they write, and `BothMacroLibrariesAreRequestedByAVendorSource` asserts each is actually REQUESTED, serving a file nobody asks for looks exactly like inclusion coverage and is none. `TheServedLibrariesAreTheOnesTheSourcesAskFor` closes the other half: a request that missed would otherwise be invisible, because both including sources are refused at the boundary either way.* **Two corrections to the task's framing.** These are NOT standalone sources: `T.SENDMSG` is a macro BODY whose second line writes to a positional parameter, and assembling it alone produces seven expression errors and means nothing, measured, and it is why the sweep includes them by inclusion rather than by assembly. And `RWTS DEMO.S` is not committed either, so there is no parse/assemble case for it.)* Use the type-T macro libraries (`T.MACRO LIBRARY`, `T.SENDMSG`, `T.PRDEC`, `T.OUTPUT`, `T.FPMACROS`, `T.ROCKWELL MACROS`, `T.PI.MACS`, `PI.NAMES`) as the `PUT`/`USE` inclusion corpus. They are the only type-T files on the disk, every `.S` source is type B loading at `$0901`, so they also settle the DOS 3.3 text convention from real vendor files. `RWTS DEMO.S` ships **no** object, so it is a parse/assemble case only, never a comparison
- [x] T045c [P] [US1] *(**Done**, [research.md](./research.md), "Vendor-source validation". It records the pinned oracle and disk hash, the six-row result table with byte counts, load addresses and the answers each source needs, the four properties asserted beside the bytes and why bytes alone do not establish them, the negative specimens and their two refusal counts, the one open SC-003 gap with its measured count and its single cause, and the finding that the macro libraries are not standalone sources.)* Record the vendor-source validation *result*, that re-assembling a vendor file reproduces its shipped object byte-for-byte, as evidence in `research.md` rather than as a committed corpus entry, which is licensing-safe and is the part that actually carries information
- [x] T045b [P] [US1] Walk the **manual's directive list** and add a corpus entry for every construct the disk does not exercise. The disk demonstrates idiom but cannot report what the vocabulary holds that this vendor never used; absence from the disk is not evidence of absence from the language (spec Corpus Floor)
  *(**Done by walking the directive TABLE against the disk rather than the manual, which is the same walk from the side this project can check.** Every spelling with a zero or absent frequency count was chased: the reversed-order word directive, the loop and its terminator, the dummy section and its terminator, three of the six string encodings, the low-ASCII delimiter, the explicit macro invocation, and the CPU selector's operand form. Each now has a captured entry or a recorded answer. `DDB $1234` giving `12 34` is the whole of the evidence for that directive, it appears in none of the nine committed vendor sources, so every test for it was previously the documented rule plus self-consistency.*

  ***And the walk ran in the other direction too, which is what found the biggest gap.*** *Reading the disk's own macro library for constructs the TABLE does not hold, rather than only the table for constructs the disk does not use. That is how the first-character conditional surfaced: thirteen uses in `T.MACRO LIBRARY`, no spelling in the table, and no committed source that reaches it. The task's framing, walk the vocabulary looking for what the vendor never used, would not have found it, because the vendor used it constantly.)*
- [x] T046 [US1] Verify SC-001: every corpus entry assembles byte-identically via `UnitTest/MerlinCorpusTests.cpp` *(**Done for the corpus that exists.** `EveryVendorEntryAssemblesToItsShippedObjectByteForByte` sweeps the entry table: zero diagnostics, `CorpusVerdict::Match` against the shipped object, and the shipped LOAD ADDRESS, the last because a wrong default origin yields byte-perfect output in the wrong place. It replaces the six hand-written per-object methods, which nothing counted. **The claim is bounded by the table**: SC-001 is measured against spec.md's Corpus Floor, and the hand-authored half of that floor (T045, T045b) is not captured, so this verifies the disk's contribution to it and not the whole. Mutated with a wrong dialect default origin: caught, along with twelve others.)*
- [x] T046a [US1] *(**Done, and it is the assertion this slice most needed.** An entry holds a fixture PATH and never text, so it cannot carry a transcribed copy; it carries no copy at all. `EveryVendorEntryAssemblesTheFixtureBytesUnmodified` then re-derives the stored bytes straight from `IFixtureProvider::OpenFixture`, strips the header and reads the declared length in the TEST rather than through the decoder the sweep uses, and requires one character of assembled text per stored byte, in order, with only the documented masking and terminator translation between them. A re-indented line, a stripped trailing space, a deleted comment or an inserted origin each move or change a character; a transcription would have to be byte-identical to survive, at which point it is not a transcription. `EveryVendorFixtureIsTheSizeTheInventoryRecords` sits beside it for the case text-versus-bytes cannot see, a fixture edited and re-saved consistently, by pinning each stored size against the fixture inventory. **The discriminating mutation is the one that matters**: a source given one extra line it did not have assembles to byte-identical output, so SC-001, the load address and the AS65 sweep all stay green and this test alone goes red. Also mutated with a decoder that dropped whole-line comments and a truncated raw read: both caught.)* Verify SC-002 in `UnitTest/MerlinCorpusTests.cpp`: the five vendor sources assemble **exactly as committed**, with no edit to any line. The corpus already proves the bytes match; what this adds is the claim that the *input* was not touched to get there; assert each entry's source is the fixture bytes as `IFixtureProvider::OpenFixture` returns them, not a transcribed or tidied copy. Without it, SC-002 is only inspected, and "unmodified" is precisely the property a passing corpus can be made to fake
- [x] T046b [US1] *(**Done, and it found the one SC-003 defect in the corpus.** `EveryRejectionOfAVendorSourceMapsToABoundaryRow` assembles all seven committed vendor sources and splits their diagnostics: each boundary refusal is attributed by recomposing the row's own sentence and requiring EQUALITY in one of the two linkage wordings, and exactly one row must match. Equality rather than a substring, because this feature has been caught by a bare-substring assertion once and a spelling is three characters that match inside ordinary words. Every diagnostic that is NOT a refusal is counted as an unexplained rejection, exactly, per file and corpus-wide, so a new one fails by construction and fixing the known gap also fails and forces the number down.* **The defect: `PI.ADD.S` draws NINE rejections with no boundary row, and they are one cause.** Line 123 is `VAR MSGPNT;OUTPUT`, Merlin binding the positional parameters for the fragment its next line pulls in with `PUT SENDMSG`. `VAR` is absent from the directive table, so the line is rejected and the eight `]1`/`]2` references inside the included fragment have nothing to resolve to. See T081. *Two divergences. **The file is `UnitTest/MerlinCorpusTests.cpp`, not `MerlinSubsetTests.cpp`**, which does not exist, the boundary tests live in `MerlinSubsetBoundaryTests.cpp`, and this sweep needs the committed-source list and its per-source answers, which are the corpus's. And **the sweep is over seven sources, not nine**: the two type-T files are macro libraries reached by inclusion, not sources Casso can be pointed at. Four mutations, four caught: a refusal composed with a name the table does not supply, a refusal recorded as an ordinary error, a recognized directive dropped so a new unexplained rejection appears, and an include name losing its prefix.)* Verify SC-003 in `UnitTest/MerlinSubsetTests.cpp`: assemble every committed vendor source and assert that **each rejection maps to a row in `CassoCore/MerlinSubsetBoundary.cpp`**. SC-003 defines a rejection with no boundary row as a defect, so this is a sweep over rejections rather than a fixed list of expected errors, a new unexplained rejection fails the test by construction. Runs against `PI.ADD.S` and `PI.START.S` too, where rejections are the expected outcome and must still be table-backed
- [x] T047 [P] [US1] Add focused parser tests to `UnitTest/MerlinParserTests.cpp` and directive tests to `UnitTest/MerlinDirectiveTests.cpp`, registering both in `UnitTest.vcxproj`
  *(**Done. Both files already existed and were already registered, so no `UnitTest.vcxproj` change was needed**; the remaining work was the coverage, not the scaffolding. The directive half filled in as T036–T042 landed, exactly as the note below predicted, and gained the reversed-order word directive with T080.*

  *The PARSER half was the gap. Six line-model facts the vendor sources cost were covered end to end through the assembler and nowhere at the parser: the equate read from the fields and then CLEARING them so its name does not also bind at the program counter; `?` as a label character; a character constant holding a space, which is an operand-scanner fact rather than a string-directive one since the mnemonic there is an ordinary instruction; a macro terminator carrying the label its own body branches to; a variable symbol standing where a label would and binding as the reassignable kind; and each field recording the column it was written at, asserted with three DIFFERENT columns in one line, so a stamped constant cannot satisfy it. Two directive-table facts joined them: the two word directives asserted as a PAIR, since either alone is satisfied by a table mapping both spellings to whichever token is under test, and a sweep requiring no Merlin spelling to carry a leading dot, which is what makes a diagnostic quoting a dotted form describe a construct that cannot exist in a Merlin file.*

  *Four mutations, four caught: the operand column stamped from the mnemonic, the scanner stopping at a space inside a character constant, the reversed word directive dotted and folded onto the ordinary token, and the equate name left in the label field.)*

### What the capture found that the implementation gets wrong

**ALL FOUR ARE IMPLEMENTED. T082, T083, T084 and T085 all landed; see each task's
own line for what diverged.** Suite is **3391** Debug / **3388** Release, both
green (from 3375 / 3372); Dormann passes and `scripts/BuildDemoDisk.ps1` still
produces its committed disk image unchanged. **Fourteen mutations over this
slice, fourteen caught**, three of them only after a test was added, and all
three of those were properties nothing had been asserting rather than assertions
that were vacuous.

**NO NEW `Directive` TOKEN AND NO ENGINE PARAMETERIZATION.** The one place a
token looked necessary is T084, and it is not: the assembler can already
assemble a block when a value is non-zero, so the first-character conditional is
a second way of writing the CONDITION and folds while the line is parsed. The
engine edits this slice does make are three, and each one reads profile data
that already existed rather than learning a dialect's name, the byte emitter and
the pass-1 argument evaluator ask the evaluation context for its character-
constant delimiter, and the CPU-selection handler simply stops refusing an
operand. `MacroSyntax` got NARROWER, not wider: `callKeyword` is gone.

**A FIFTH DIVERGENCE WAS FOUND HERE AND IS FIXED IN T086.** A macro definition
with no terminator of its own does **not** fall into the next: `ADDX MAC` / `TXA`
/ `ADDA MAC` / … / one shared `<<<` should define both macros, the outer one
ending with the inner one's body, real Merlin produces `8A 18 65 10` for
`ADDX $10`, and Casso swallowed the inner `MAC` line into the outer body, so the
expansion opened a definition that never closed and reported `Unclosed macro
definition`. T025c recorded fall-through as WORKING and attributed that
diagnostic to the missing `IF` spelling; both halves of that are wrong, and the
diagnostic was measured on the executable both before and after `IF` landed.
Fixing it is an ENGINE change and was taken as a spec amendment; see T086 and
the block at the top. The first-character-conditional row has moved up into the
corpus.

The bytes for all four were **already committed**, in the pending table in
`UnitTest/MerlinCorpusTests.cpp` where a sweep asserts each still diverges, so
implementing any of them turns that test red and forces the row up into the
corpus proper, rather than leaving a note behind claiming a gap that is closed.
Two of the three rows have moved up. The third stays, with its recorded
divergence rewritten to name the cause that is actually left.
Nothing here needs a further capture session.

- [x] T082 [US1] Read a quoted argument to Merlin's byte directive as a **high-ASCII character constant** rather than as the other dialect's string literal. ` DFB "A"` is `C1` under real Merlin and `41` under Casso today: the directive takes an EXPRESSION, and the double-quote convention the string directives take from their delimiter applies to one character the same way. The mechanism already exists and is already profile data, `GetHighAsciiCharDelimiter`, and is already correct for instruction operands, which is why `CMP #"N"` in a vendor source reproduces its shipped bytes; what is wrong is that the byte directive's emitter never reaches the evaluator for a quoted argument. as65's own string behavior must NOT change, so the difference belongs on the seam rather than in the emitter. Pending row: *high-ASCII character constant in a byte directive*
  *(**Done, and it took TWO sites rather than the one the task names.** The emitter was the visible half; the other is `TryEvaluateDirectiveArgs`, which carries the same string branch and is what pass 1 sizes with, and what the WORD directive emits through. Leaving it would have made ` DFB "A"` produce `C1` and ` DA "A"` produce `41 00` in the same file under the same dialect, which is the shape of defect this feature exists to avoid. The predicate is one line in both places and reads off data that already existed: a quoted run is text unless the run's delimiter is the one the active dialect spells a character constant with. Taken from `ExprContext::highAsciiCharDelimiter` rather than from the profile directly, so the branch and the evaluator that is about to be handed the argument cannot disagree. as65 answers 0, which matches no argument, asserted, not assumed, by a test requiring its byte directive to still emit text. The captured row moved into the corpus. Three mutations, three caught, including the one that makes every quoted run an expression and breaks eleven as65 string tests.)*
- [x] T083 [US1] **Accept and ignore an operand on the CPU-selection directive.** T040 refused one deliberately, on the stated grounds that whether a reset form exists was unsettled and that ignoring the operand would answer the question silently and wrongly. It is settled: `XC OFF` draws no diagnostic from Merlin and selects nothing; the wider set stays selected, proven by a following instruction only the wider processor has still assembling. So there is no reset form, the one-way transition in `data-model.md` stands, and the refusal is now the thing that rejects valid source. The test asserting the operand is refused changes with it, and the reasoning at the code should be replaced rather than deleted: the refusal was right while the answer was unknown
  *(**Done exactly as written, and T040's stated grounds are recorded as having been right rather than deleted.** The evidence beats the reasoning because the reasoning was explicitly conditional on the question being open: T040 refused the operand in order not to ANSWER a question about the language, and the question is now answered from bytes. `AnOperandIsRefusedAndDoesNotSelectAnything` is replaced by two tests rather than one: that an operand changes nothing, and that ANY operand is ignored, without the second, an implementation that grew a table of accepted operands passes. **What did NOT change is the subset boundary**: `XC` / `PHX` / `XC OFF` / `PHX` is still refused, because a SECOND occurrence means the 65802/65816 and that refusal is about the count rather than the operand. So the captured probe still does not assemble whole, and correctly; no corpus entry was ever committed for it. Two mutations, two caught.)*
- [x] T084 [US1] Implement the **first-character conditional**, `IF <char>=]n`, which compares the first character of a parameter against a literal and is how a Merlin macro dispatches on addressing mode. The spelling is absent from the Merlin directive table entirely, so a macro using it never reaches its terminator and is reported as an unclosed definition. **This is the largest gap the capture found, and it is not exotic**: the distribution disk's own `T.MACRO LIBRARY` uses it thirteen times, in `MOVD`, `LDHI`, `ADD`, `SUB` and `PRINT`. It went unseen because that library is not one of the two committed as fixtures and no committed source reaches it. Consider committing it as a third fixture alongside the implementation, so the sweep that already asserts every rejection maps to a boundary row starts covering it. Pending row: *first-character conditional inside a macro*
  *(**Done, with NO new token and nothing shared changed, and three corrections to the task's own framing.**

  *First, the SYNTAX is wider than `IF <char>=]n` and the manual states the rule exactly: the assembler compares the FIRST and THIRD characters of the operand and never examines the one between them, so `IF (=]1` and `IF (,]1` are the same test and the library writes both. Implemented positionally for that reason. Second, the DIAGNOSTIC was not an unclosed definition. `IF` is absent from Merlin's table, but the conditional dispatcher falls back to the OTHER dialect's table for a bare mnemonic and as65 spells the token `IF` too, so every use was reported as an expression that would not evaluate. The unclosed-definition message came from macro fall-through, which is a separate and still-open gap; see the block above. Third, the count: eleven uses are legible in a raw scan of the disk image, and at least two more lines are visibly garbled by that scan, so thirteen is consistent but is not what was measured here.*

  ***No `Directive` token was added, and the guarantee it is checked against is guarantee 2 in `contracts/dialect-profile.md`: a token exists for an operation the assembler cannot already perform.** Assembling a block when a value is non-zero is one it can, so `IF` shares `Directive::If` with `DO` and the profile folds the operand to `1` or `0` while the line is parsed. Both compared characters are known at that moment, because the expander substitutes textually before the line is re-parsed. Same treatment `BLT`/`BGE` and the `ERR \` address check already get. Nothing in `AssemblySession`, the evaluator or the opcode tables was touched for this task.*

  *One thing the corpus structurally cannot settle, recorded at the code: the spelling that puts the PARAMETER first and the literal after it, the vendor's `PRINT` macro writes `IF ]1="`, cannot be distinguished here from an argument that happens to begin with the same character, because substitution has already erased which position held the reference. Every other use on the disk writes the literal first.*

  *(**SETTLED BY T090, and the answer is that Merlin cannot distinguish it either.** The test is purely positional there too, so `PRINT "X"` takes the quoted branch by accident, `"X"="` has `"` in positions one and three, and `PRINT "HI"` takes the hex branch and produces no object at all. The vendor macro is broken for any message longer than one character, and Casso's positional implementation is right. This was NOT structurally unsettleable; it needed the library to be assembled, which needed T086 and T088.)*

  ***The fixture question was raised rather than decided here; T088 decided it, the other way.** The reasoning below is kept because its first half is still right and its second half is the mistake worth remembering. `UnitTest/Fixtures/Merlin/README.md` covers the licensing half cleanly (same disk, same CC BY-NC-ND terms, same hash-pinned extraction script) and its INCLUSION rule is narrower than that: every committed file is either half of a source/object oracle pair, or a file a committed source actually requests (`T.PI.MACS` and `T.SENDMSG` are there because `USE`/`PUT` reach them, and a test asserts each is really requested). Where this went wrong is the next step: `T.MACRO LIBRARY` ships no object, and that was read as "has no oracle" when it only means "the vendor never built one". **An oracle can be generated**, and generating one makes the library an ordinary pair rather than a third category. See T088 and T089.*

  *Six mutations, six caught, one of them only after a test was added. That one is worth reading: renaming the table's `IF` row left all 3389 tests green, because the dispatcher's fallback to as65's table supplies the same token and the fold is keyed on the spelling. Every byte comparison passed while the dialect borrowed a spelling it must own, so the claim is now asserted structurally against `MerlinDirectiveTable::FromSpelling`. **That fallback is a pre-existing seam leak and is not fixed here**: a bare `IFDEF` under Merlin still resolves through as65's vocabulary.)*
- [x] T085 [US1] Correct **explicit macro invocation** in `CassoCore/MerlinDialect.cpp` on three counts, all of them currently marked UNVERIFIED at the code and all three now falsified. Merlin takes the macro NAME from the operand field and its ARGUMENTS from the field after it, so the name is separated from the arguments by a space and only the arguments are separated from each other by the macro separator, Casso takes the name up to the separator instead. The prefix written flush against the name is **refused** by Merlin and accepted by Casso. And `PMC`, left unimplemented on the grounds that adding it would double an unverified surface, is a real spelling that behaves identically. The comment claiming both spacings work must go with the change. Pending row: *explicit macro invocation*
  *(**Done, and the fix is a PARSE-TIME resolution rather than three corrections to the expander, which is what makes all three refusals fall out instead of needing to be written.** The prefix and the macro's name are two fields, so the profile promotes the name into the opcode field and the arguments field into the operand: `>>> MOV2 $10;$20` becomes the ordinary invocation `MOV2 $10;$20`, which already worked. Then `>>>NOPS` is one word matching no prefix, `PMC ADDA;$30` is one word naming no macro, and `PMC` differs from `>>>` in nothing but its spelling. Same treatment the branch aliases get: nothing downstream learns the source said something else.*

  ***A SEAM FIELD WAS REMOVED, and that is the one shared change here.*** `MacroSyntax::callKeyword` had exactly one user, and its contract, the expander normalizing the two fields back into one separator-joined operand, is precisely what CANNOT represent the case the real assembler refuses, since a name containing the separator is indistinguishable there from a name plus arguments. With Merlin resolving the prefix itself the field had no user at all, so it and the expander's explicit-invocation branch are gone. **This narrows the seam rather than widening it**, and follows T033a's precedent that an always-empty lookup is dead machinery standing in for a guarantee. No dialect's behavior changes but Merlin's.*

  ***Two tests encoded the old wrong belief and were changed rather than weakened.*** `TheExplicitPrefixMayBeWrittenFlushAgainstTheName` asserted the opposite of what Merlin does; it is replaced by `ThePrefixWrittenFlushAgainstTheNameIsRefused`, which also requires no bytes to have been emitted on the way past. `TheExplicitPrefixInvokesTheNamedMacro` and `AnExplicitInvocationOfAnUnknownMacroNamesIt` move from the `;` spelling to the space one. Four tests are added: the word form against the punctuation form's own bytes, the separator-joined name refused, a prefix naming no macro refused rather than dropped, and a macro whose NAME begins with the word form still being an ordinary invocation. The captured row moved into the corpus.*

  *One diagnostic is worse than it should be and is left alone as out of scope: ` >>>MOV $10;$11` reports an expression error about the operand rather than naming the word, because the operand reaches the evaluator before the unknown opcode is reported. ` >>>NOPS`, the form actually captured, names the word. Six mutations, six caught, two of them only after a test was added.)*

  *(Previously half done. `MerlinDirectiveTests.cpp` exists and is registered, covering the string family through both passes, `ERR` in **both** directions, local-label scoping, operator binding, the default origin, the keyboard-input directive in both the answered and unanswered directions, the two renamed operators, unsigned 16-bit division, `?` inside a symbol, and character constants holding a space, each with an AS65 counterpart where the construct is shared, since a test passing under both dialects is no evidence the profile was consulted. The remaining directives get theirs as T036–T042 land.*

  *Two of these could only be written as pairs. The vendor corpus contains only the SILENT case of `ERR`, because a source shipping an object necessarily assembled clean, so an `ERR` that never fires passes every oracle on the disk. Same for binding: an evaluator that happened to agree looks identical without the AS65 half.)*

### The fifth divergence, and the library that would have found it

**Both are user-decided and both are done.** The fall-through fix is a **spec
amendment**, see FR-032 and [contracts/dialect-profile.md](./contracts/dialect-profile.md)
under "Amendment: overlapping macro definitions", and the macro library is
committed as a **generated** oracle pair (FR-033), which is a third kind of
fixture rather than an exception to the inclusion rule.

- [x] T086 [US1] **Implement macro definition fall-through** in `CassoCore/AssemblySession.h`/`.cpp`: while any definition is open, a further opening line starts another beside it, every following line is appended to all of them, and one terminator closes them all. `ADDX $10` against the captured shape must emit `8A 18 65 10` and `ADDA $10` must emit `18 65 10`. **This is an engine change and therefore a spec amendment, not a task decided here**, amend `spec.md` and `contracts/dialect-profile.md` first, saying plainly that it was user-approved and why the guarantee is relaxed in the open rather than bypassed. Prefer the smallest honest change: mechanism every dialect gets, never `if (dialect == Merlin)` (FR-032)
  *(**Done as a stack, and the smallest honest shape turned out to be a NARROWING as well as an addition.** `AssemblySession` kept one definition in six scalar fields; it now keeps `std::vector<MacroDefinition>`, and `MacroDefinition` gained the one carrier it was missing (`openColumn`, beside the `lineNumber` and `sourceFile` it already had for exactly this) so the pending record and the finished definition are one type rather than two that could drift. `OpenMacroDefinition` is extracted so the prelude, which opens the first, and the collector, which opens every further one, cannot disagree about what opening a definition means.*

  ***Every dialect gets it and none opts in.*** *There is no profile field for fall-through and no dialect is named in the collector. The engine simply had no way to represent overlapping definitions at ALL, the collector had nowhere to put the second one, which is what makes this a missing capability rather than a Merlin quirk. `MacroTests.cpp` asserts the same shape in as65's `macro`/`endm` spelling beside the Merlin half in `MerlinDirectiveTests.cpp`, because a test in only one dialect leaves a dialect-shaped special case indistinguishable from the mechanism.*

  ***One spelling moved OUT of the engine, and the vendor library is what forced it.*** `IsMacroDefinitionStart` compared the operand against the literal `"MACRO"`, so ` PUT MACRO LIBRARY`, an inclusion naming a real file on the disk, was read as defining a macro called `PUT`, swallowed the rest of the file, and reported it unclosed. The keyword is now `MacroSyntax::defKeyword`, beside `endKeyword` and `localKeyword` which were already there for the same reason, and Merlin answers empty. *This narrows the engine's reach rather than widening the seam*, the direction T085 took `callKeyword`.

  *The unclosed diagnostic is now one error per open level, each at its own opening line, the treatment the conditional stack already had, and newly necessary because two definitions can now be open at once. **Five mutations, five caught.***
- [x] T087 [US1] Land the pending corpus row this unblocks and correct `T025c`'s note, whose two halves were both wrong *(Done. The composite assembles whole and is in `s_kCapturedCorpus` as `macro fall-through and first-character conditional`, at `$2200` with its ten captured bytes. T025c carries a second correction in place rather than a note pointing elsewhere.)*
- [x] T088 [US1] **Commit `T.MACRO LIBRARY`** through the hash-pinned `scripts/ExtractMerlinFixtures.ps1` chain, and update `UnitTest/Fixtures/Merlin/README.md`'s inclusion rule to describe what is actually true, without deleting the reasoning behind the old rule, which was sound for oracle fixtures *(**Done, and the old rule needed no exception after all.** The rule admits a file that is half of a source/object pair; what was too narrow was reading "ships no object" as "has no oracle". An oracle can be GENERATED, and generating one makes the library an ordinary pair. The README says so as a third kind of file, with the original reasoning kept intact above it. Extracted by adding one row to `$textFixtures` and re-running the script against the pinned disk, which verified and was not written to, 1615 bytes, SHA-256 `FA37CFB3…`, inventory row pasted from the script's own output. **No top-level notice was created**: constitution 1.9.0 says fixtures are not dependencies and that one `LICENSE` per directory covers every file in it, which this directory already has.)*
- [x] T089 [US1] **Generate the library's oracle**: author a source that includes it and invokes the macros using the first-character conditional, assemble it under real Merlin Pro 2.23, and pin the bytes (FR-033) *(**Done, 279 bytes, and Casso reproduces every one.** Full capture discipline: a work copy of the disk (the pristine image hashes unchanged afterwards), an object name that had never been on the disk, `-ConfirmAbsent` before assembling, and the source read BACK off the disk after saving; the round trip was CLEAN, so the committed text is the text Merlin assembled. `MerlinMacroLibraryOracleTests` has four tests: the bytes and load address, that the library is requested under the name the disk stores, that a comment after the filename is not part of it, and the vacuity guard that the same source fails under as65.*

  *The composite invokes `ADDX`/`ADDA`/`ADDY` (the fall-through chain, and the first seven bytes ARE that expansion), every branch of `MOVD`'s three-deep first-character nest, both branches of `LDHI`, `ADD` and `SUB`, and the eleven remaining macros. **Two facts about `PUT`/`USE` came out of it, neither settled by any vendor line:** a macro library must be reached with `USE` and not `PUT` (the manual is explicit that a `PUT` file may not hold macro definitions, and Merlin defines nothing while saying nothing) and **a space inside the operand is part of the filename**, with the comment introducer ending it. Both vendor inclusions name a file with no space and carry no trailing comment, so a scanner stopping at the first space reproduces every byte on the disk and asks for `T.MACRO`. Fixed in `MerlinDialect::ReadOperandField` via a new `TakesFileName`, profile-side, next to the delimited-text rule it mirrors.)*
- [x] T090 [US1] Pin what the library still does NOT assemble, if anything *(**One macro of the nineteen, and the finding is about the LIBRARY rather than about Casso.** `PRINT` writes its conditional with the parameter first, `IF ]1="`, and real Merlin's test is purely positional, comparing the first and third characters of the operand after substitution without ever learning which position held the reference. So `PRINT "X"` substitutes to `"X"="`, whose first and third are both `"`, takes the quoted branch by accident and assembles to six bytes; `PRINT "HI"` substitutes to `"HI"="`, whose first and third are `"` and `I`, takes the hex branch and produces **no object at all**. Both measured on the executable. **This answers the question T084 recorded as structurally unsettleable**, Merlin cannot distinguish the parameter-first spelling either, and Casso's positional implementation is right.*

  *What DIVERGES is unrelated to the conditional: Casso refuses the invocation because the branch that is not taken refers to `]2` and only one argument was supplied, where Merlin binds an unsupplied parameter to nothing. Captured as the pending row `first-character conditional with the parameter written first`, so implementing that behavior turns the pending sweep red and forces the row up. `STADR` is the other macro left uninvoked: it expands through `POKE` to `LDA ##ADDR`, a doubled immediate marker, and whether Merlin accepts that is not captured here.)*

### The directive-table fallback, closed

**Measured first, in [research.md](./research.md) under "The directive-table
fallback, measured rather than estimated", and fixed here against those numbers.**
The engine resolved a word the active profile had DECLINED through a second,
fixed table, so 55 spellings Merlin does not have still resolved and eight of
them steered conditional assembly. Nothing caught it because under AS65 the same
code is unreachable, that profile resolves every one of its own spellings
first, so the arm could only ever fire across dialects, and no test crossed
them. It is **not** an amendment: no engine capability is missing and no
guarantee is relaxed. Each site was already supposed to ask the profile, and
four of them asked a class name instead.

- [x] T091 [US1] Route every dialect-blind directive lookup in `CassoCore/AssemblySession.cpp` through the ACTIVE PROFILE, `IsConditionalLine`, `HandleConditionalDirective`, `GetStructMemberSize` and `CountExitmIfDepth` *(**Done, and the four became one.** `TokenForLine` is the single place a parsed line is turned into a token: the profile's own answer where the parser already resolved the word, and `m_dialect.GetDirectiveForSpelling` on the mnemonic where it did not. `IsConditionalLine` had to stop being `static` (it has no profile otherwise, and its only caller reads `m_dialect` two lines later) and `CountExitmIfDepth` now READS each expanded line with `m_dialect.ParseLine` rather than splitting a leading word off it, which additionally settles a labeled conditional that no word-splitting scan can see. **Case normalization lives in that one function** and is contract compliance rather than a fix: `GetDirectiveForSpelling` is specified to take an upper-cased spelling, AS65 stores its mnemonic upper-cased and Merlin stores it raw, and that mismatch was narrowing the old leak to the upper-case half. Normalizing WITHOUT changing the table would have widened it instead, which is why `AnAs65ConditionalIsRefusedInMerlinInEitherCase` is a separate test.)*
- [x] T092 [US1] Move the hard-coded AS65 spellings sitting beside those sites onto the seam: the macro exit keyword, the closer an early exit synthesizes, the repeat-`NOP` mnemonic, and the `END`/`STRUCT` pair *(**Done, three additions to the seam, all of them SPELLINGS, and no new `Directive` token.** Guarantee 2 admits a token only for an operation the assembler cannot already perform, and every one of these is an operation it performs today under another name. `MacroSyntax::exitKeyword` joins `endKeyword` and `localKeyword`, the last of that family, Merlin answers empty, since no vendor source on the disk writes an early exit and inventing one is the admission FR-005 forbids. `GetSpellingForDirective` is the inverse of `GetDirectiveForSpelling`, and exists for the one thing tokens cannot do: WRITE a line. The assembler synthesizes source in exactly one place, the closers an early exit owes its abandoned conditionals, and a fixed `ENDIF` there put a word Merlin does not have into a Merlin stream. `GetAmbiguousDirectiveForSpelling` carries `RMB`, kept separate from the ordinary accessor because resolving it is only sound where the context has already ruled the instruction out. `GetMultiNopMnemonic` names the repeat form; a Merlin source writing `NOP 3` was silently given three bytes. `END`/`STRUCT` needed nothing new, both resolve through the ordinary accessor now. **`DirectiveTable::FromStorageSpelling` is deleted**: its only caller was the dialect-blind one, and leaving a shared-table convenience in place invites the next site to reach for it. `DirectiveTable` is now referenced from `As65Dialect.cpp` and its own file and nowhere else, which is the engine holding no vocabulary stated as a grep.)*
- [x] T093 [US1] Cover the leak with tests that assemble ACROSS dialects, since that is the only direction it ever fired *(Done, `VocabularyIsolationTests` in `UnitTest/DialectMechanismTests.cpp`, six tests. The foreign vocabulary is **swept rather than sampled**: every AS65 spelling Merlin does not claim is assembled under Merlin and must be refused by name, with the seven genuinely shared words skipped by asking Merlin instead of by listing them. The eight that changed behavior get real blocks around an emitted byte, because a one-line specimen cannot tell a leaked conditional from an unopened block. The early-exit closers are proved through a **test-only profile that is Merlin plus the keyword Merlin lacks**, the only way to observe them in a dialect with no early exit of its own, and it fails in both directions at once: counted through a foreign table `DO` is not a conditional and nothing closes, spelled with a foreign word the closer is not an operation.)*
- [x] T094 [US1] Re-run the mutation that exposed the seam and prove it now goes red *(**Red, 9 of 3408.** Renaming Merlin's `IF` row to `IFX` once left the whole suite green, which is what showed the arm was unreachable; it now fails `AMacroDispatchesOnItsArgumentsFirstCharacter`, the vendor macro-library oracle and the captured-corpus sweep. **Eight mutations, eight caught**, one of them only after the test was corrected: the fixed exit keyword was NOT caught by a macro merely NAMED for it, because an early exit is only looked for inside an expansion; the specimen had to be a body line, which is exactly the line the fixed comparison read. The `END`/`STRUCT` mutation is reported CAUGHT-BY-CRASH rather than by an assertion; a struct that never closes swallows the rest of the file and trips a vector bound, which is a pre-existing robustness gap in that path and is not touched here.)*

**Checkpoint**: Unmodified Merlin source assembles to Merlin's bytes. This is the MVP.

---

## Phase 4: User Story 2 - Choose a dialect explicitly (Priority: P1)

**Goal**: A developer states the dialect and gets exactly that dialect's rules, strictly.

**Independent test**: One source file assembled under each dialect selection; constructs valid in one and invalid in the other are accepted and rejected accordingly.

**⚠️ Shared file warning**: `CassoCore/CommandLineParser.cpp` and `UnitTest/CommandLineTests.cpp` are shared with the concurrently developed spec 020. Everything **added** here is additive, two rows, one enumerator, one arm, one flag parser. Do not restructure the dispatcher; if that seems necessary, stop and raise it.

The **one** sanctioned exception is T049a, removing the fallback heuristic: a decision taken explicitly, not a restructuring reached for. It is on hold until 020's command-line work merges, so 019 never edits those files while 020 holds unmerged changes in them. It is the only place this feature edits shared behavior rather than extending it, and it is why the 020 session will need to rebase. Keep it confined to the fallback cases, a wider edit here is not covered by that decision.

- [x] T048 [US2] Add `Subcommand::Merlin` and a `dialect` field to `CommandLineOptions` in `CassoCore/CommandLineOptions.h` *(**Four fields, not one.** `dialectSelection` rides beside `dialect` because the reporting table keys off provenance and the CLI is the only thing that knows it; `hasCpuTarget`, because the default target and an explicitly requested one are the same value and the CPU report has to tell them apart; and `cpuFlagRefusal`, carrying the already-worded refusal, since the sentence names a directive only the profile knows and the printing edge must not be where it is composed. `Subcommand::Merlin` is appended rather than inserted, so no existing enumerator moves.)*
- [x] T049 [US2] Add one row `{ "merlin", CommandLineOptions::Subcommand::Merlin }` to `s_kSubcommands` and one arm in `Parse` in `CassoCore/CommandLineParser.cpp` *(Exactly that, plus `ApplyMerlinDefaults` beside the arm; the source path is auto-extended the way AS65 mode does it, and the OUTPUT name deliberately is not defaulted there: a name invented at parse time is the caller's answer, which outranks the source's, so every object-file directive would be dead. The dispatcher is not restructured; the bail condition gained one disjunct.)*
- [x] T049a [US2] **⛔ HOLD until the user confirms spec 020's command-line work has merged to master.** Sequencing measured rather than guessed: 020 has 384 lines in flight across `CommandLineOptions.h`, `CommandLineParser.cpp`/`.h`, and `CommandLineTests.cpp`; 019 has touched none of them. Whoever holds unmerged work in a file should not have to resolve around someone else's edit. The stronger reason is not conflict at all: 020 is adding `disk` to `s_kSubcommands`, and that table is exactly what decides which bare words reach the fallback, so the behavior being removed *changes shape* when `disk` lands, and doing this first means writing tests against an intermediate table that is about to move. **Then, in ONE commit**: add the row `{ "as65", CommandLineOptions::Subcommand::As65 }` to `s_kSubcommands`, remove the unrecognized-first-argument fallback from `Parse` (both in `CassoCore/CommandLineParser.cpp`), update `UnitTest/CommandLineTests.cpp`, and add the `CHANGELOG.md` breaking-changes entry.

  **One commit is a correctness requirement, not tidiness.** The table on master holds exactly one row, `{ "run", Subcommand::Run }`; there is no `as65` word, so `Subcommand::As65` is reachable *only* through the fallback being removed. Any commit that removes it without adding the row in the same change leaves AS65 unreachable, and a bisect lands on that broken midpoint.

  The error for an unrecognized first argument MUST name the replacement (`did you mean: CassoCli as65 <source>`) rather than print usage: the affected population is build scripts, which nobody re-reads until they fail, so a bare "unknown argument" turns a one-line fix into a bisect.

  **Deleting 020's `BareWordThatIsNotASubcommand_StaysAs65` is the intended outcome, not a regression.** That test was added as a tripwire, so that adding a table row could not erode the fallback incidentally and removal would have to be a deliberate act carrying its own CHANGELOG entry. This is that act. Say so in the commit message, or it reads later as someone deleting an inconvenient test.

  Reverses the earlier deferral recorded in [research.md](./research.md) D5 and in the CLI contract's guaranteed-unchanged list; both now say so.
- [x] T050 [US2] Add `ParseMerlinFlags` to `CassoCore/CommandLineParser.h` and `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](./contracts/cli.md) *(**Table-driven, and that is the whole of T053b's anti-drift claim.** The three flags are `DialectFlag` rows carrying an argument form and a description; the walk reads the rows and decides nothing itself, and `ApplyMerlinFlag` REPORTS an unrecognized letter rather than ignoring one, so a row added without an arm is a flag the help advertises and the parser drops, which the table sweep catches. **Two divergences from the contract's flag table.** `-l` takes an ATTACHED filename only, unlike the as65 form that also takes a separated one: Merlin source names its own object, so the bare word after a flag is far more likely to be the source, and swallowing it leaves an assembly with no input; pinned by its own test. And the output SHAPE is `Raw` for this dialect and is not a flag at all, Merlin's origin relocates rather than seeks, so the as65 default of an address-indexed image scatters one contiguous object across memory. There is deliberately no `-q`: a clean merlin run prints nothing, so there is nothing to silence.)*
- [x] T051 [US2] Refuse `--cpu` **when the active profile's `cpuSource` is in-source**, driven by profile data rather than by a merlin-specific branch, in `CassoCore/CommandLineParser.cpp`; the message names the directive supplied by the profile. A hard-coded merlin arm here would put a per-dialect branch in the shared mechanism, which is what `contracts/dialect-profile.md` guarantee 3 forbids and what SC-009 exists to catch (FR-026) *(`RefuseCpuFlagWhereSelectedInSource` looks the profile up from `options.dialect` and composes both halves of the sentence out of `GetName()` and `GetCpuDirectiveName()`. **BOTH grammars call it** (the as65 arm too, where it can never fire) because that is what makes the refusal a property of the mechanism rather than of one flag parser. The evidence is a registry sweep asserting each dialect answers as its own profile says, plus two mutations: flipping Merlin's profile to `CommandLine` makes it accept the flag, and flipping as65's to `InSource` makes **as65** refuse it. Deleting the call from the as65 arm is caught by nothing, and is recorded at the code as such; see the note there for why it stays.)*
- [x] T052 [US2] Set `AssemblerOptions::dialect` from `CommandLineOptions::dialect` in `CassoCli/CommandLine.cpp`, carrying **provenance** and not just the dialect. With the fallback gone the command line always states a dialect, but `AssemblerOptions` still defaults to AS65 for callers that set none, FR-006 makes the assembler reachable from entry points that are not the CLI, and the reporting table in [contracts/cli.md](./contracts/cli.md) keys off exactly that distinction: stated is reported nowhere, defaulted is reported under `-v` or in the listing header *(Done in `BuildAssemblerOptions`, along with the caller's output name. **`run` is the reachable "defaulted" case**: it assembles source and names no dialect, so the provenance pair is exercised from the command line in both directions rather than only from tests. The wiring proper is here too (`DoMerlin`, the exit-code mapping, and the reporting print) see the state-of-play section for the two as65-visible consequences.)*
- [x] T053 [US2] Create `CassoCore/DialectReporting.h` / `.cpp` deciding **what** dialect-and-CPU line to emit and **when**, per the reporting table in [contracts/cli.md](./contracts/cli.md), and register both in `CassoCore.vcxproj`. `CassoCli/CommandLine.cpp` only prints what it returns; never unconditionally on stdout, which carries the listing when no listing file is named. The decision lives in core so `UnitTest` can exercise it (FR-004, SC-005). *(Decision: the sink is an enum with a `StandardOutput` value that is never produced, so "never on stdout" becomes an assertion a sweep can make rather than a property only a reader can check. Provenance arrives as `AssemblerOptions::dialectSelection`; see the state-of-play note. Nothing calls this yet; T052/T053b are the wiring and they touch files spec 020 holds.)*
- [x] T053a [P] [US2] Report the **CPU target** alongside the dialect through the same path (including when it was left at the dialect's default, so "no directive was seen" is not misread as "the flag was ignored") in `CassoCore/DialectReporting.cpp` (SC-005). *(Decision: the CPU's NAME is supplied by the caller in `CpuReport` rather than derived. Core's assembler has no CPU-target vocabulary, instruction tables arrive unnamed, and the two alternatives were a second CPU enumeration in core or a dependency on `CommandLineOptions::CpuTarget`, which sits in a file 020 is editing. Second decision: the CPU's reporting is decided INDEPENDENTLY of the dialect's provenance; the contract's wording admits a reading under which this row can never fire, and the state-of-play note records why that reading was rejected.)*
- [x] T053c [US2] Verify SC-005 in `UnitTest/DialectReportingTests.cpp`: walk **every row** of the reporting table in [contracts/cli.md](./contracts/cli.md) and assert `DialectReporting` produces what that row says, including the two negative rows, that nothing is emitted when a selection was stated, and that nothing reaches stdout in any case. The negatives are the half worth having: an implementation that reports unconditionally satisfies "the developer can determine it" while breaking the piped-listing guarantee FR-004 spends most of its words on. *(Every row has a test, both negatives included, plus one for the `AssemblerOptions` default itself. Nine mutations of the reporting code were caught, among them "report the dialect unconditionally", "report the CPU unconditionally", "drop the dialect-default CPU row", both sinks pointed at stdout, and the dialect name hard-coded.)*
- [x] T053b [P] [US2] Register the `as65` and `merlin` subcommands and their flag tables in the tool's usage and help output via `CassoCli/CommandLine.h` and `CassoCli/CommandLine.cpp`, deriving the flag list from core so help cannot drift from the parser, with a test (FR-024, US2 acceptance 4) *(**A fifth file, `CassoCore/DialectHelp.{h,cpp}`, because generation in the executable is unreachable from `UnitTest`, the same reason T062 gives.** It sweeps the registry for the dialect list, `CommandLineParser::GetFlags` for the flag lines, the profile for the CPU sentence, and the profile's boundary rows for where support ends; the executable prints the string. The usage line and the "expected one of" list are likewise swept from the subcommand table in `CassoCli`. **The drift guarantee is uneven and deliberately so:** for merlin it holds by construction, because help reads the very table the parser walks; for as65 it does not, because that grammar is a hand-rolled walk over a historical command line and a table it does not consult would be a second description of it, so as65's flag block stays where it is, retitled from "Assembler flags" to "as65 flags", and only its `--cpu` lines move into the generated section. **`--cpu` is described by the mechanism rather than by either dialect**: the help line branches on the profile's answer, not on the dialect. Also needed a shared `SubsetBoundary::ComposeHelpText`: calling `MerlinSubsetBoundary::GetHelpText` from shared help mechanism would have named a dialect in it, so the per-row wording moved down and Merlin's accessor now supplies only its own heading.)*
- [x] T054 [P] [US2] Add merlin grammar tests to a **new** `UnitTest/MerlinCommandLineTests.cpp` rather than editing `UnitTest/CommandLineTests.cpp`, and register it in `UnitTest.vcxproj` *(31 tests in five classes: the grammar, dialect selection, the CPU flag, the generated help, and cross-dialect strictness. `CommandLineTests.cpp` is untouched. One vacuous sweep was found by mutation and closed, the help sweep iterated `GetFlags` with no emptiness guard, so pointing the flag table at another dialect left it passing over nothing.)*
- [x] T055 [US2] ~~Verify `UnitTest/CommandLineTests.cpp` passes with zero modifications~~, **the premise expired with T049a and this is not a regression.** Removing the fallback necessarily rewrote 30 of that file's 45 `ArgVector` cases to name the subcommand and added three tests, all in T049a's own commit, exactly as that task and the CLI contract require. What still holds, and is what this task now means: **this slice adds nothing to that file and changes nothing in it**, and every one of its tests passes, so spec 020's pinned behavior is intact apart from the one removal 020 itself was told to expect
- [x] T078 [US2] Create `CassoCore/AssemblerExitCode.h` / `.cpp` mapping an `AssemblyResult` to the shared vocabulary (0 clean, 1 succeeded with complaints, 2 no output) and register both in `CassoCore.vcxproj`; `CassoCli/CommandLine.cpp` returns what it computes. A subset-boundary refusal maps to 2 and is distinguished by its message, not by a distinct code. In core so the mapping is unit-testable rather than reachable only by running the exe (FR-030). *(Decision: tests live in a new `UnitTest/AssemblerExitCodeTests.cpp`, which the task did not name. `CassoCli/CommandLine.cpp` does NOT yet return what it computes; that edit belongs with T052's wiring and the file is shared with spec 020. The failure test reads `success` rather than the error list, since every recorded error clears the flag and warnings-as-errors clears it while filing the diagnostic as an error; the refusal case is pinned by asserting it earns the SAME code as a syntax error rather than by asserting each is 2.)*
- [x] T079 [P] [US2] Add a cross-dialect strictness test to `UnitTest/MerlinCommandLineTests.cpp`: a Merlin-only construct assembled under AS65 is rejected naming the construct and the active dialect, and an AS65-only construct under Merlin likewise. This is US2's independent test and FR-005's direct evidence, no other task exercises the accept/reject matrix *(**Four tests, not two.** A rejection pair alone is satisfied by an assembler that rejects the construct under EVERY dialect, which is breakage rather than strictness, so each construct is also assembled under the dialect that owns it and its bytes checked: `DO`/`FIN` under merlin, a structure declaration under as65. The dialect comes from a parsed command line rather than being named in the fixture, which is what makes this US2's test and not a second copy of the corpus attribution tests. `.STRUCT` is the as65-only construct because as65's macro keyword is not in its spelling table at all and so cannot be attributed; three mutations confirm the attribution is read from the registry rather than composed from a literal.)*

**Checkpoint**: Dialect selection is explicit, strict, and additive to the shared command-line surface.

---

## Phase 5: User Story 3 - Diagnostics that speak the developer's dialect (Priority: P2)

**Goal**: Failures name constructs in the selected dialect's vocabulary, at the right line and column, and subset-boundary refusals are unmistakable.

**Independent test**: A known error introduced per dialect produces a diagnostic naming the right construct at the right position; every boundary construct produces a named refusal rather than a parse error.

- [x] T056 [US3] Create `CassoCore/MerlinSubsetBoundary.h` / `.cpp` with one row per refused construct carrying spelling, reason class, explanation, and what widens it, exposed by a `GetAll`-style accessor; register both in `CassoCore.vcxproj` (FR-019). *(Four files, not two. The row shape, the lookup and the wording are MECHANISM and live in `CassoCore/SubsetBoundary.{h,cpp}`; only the rows and the help text are Merlin's. Guarantee 1 forbids `AssemblySession` naming a dialect's class, and it must compose the refusal, so the composer cannot sit in the Merlin file. Two columns beyond the four named: whether a construct is what makes the module depend on another, and the workaround pair T057a needs; both are data the composer reads, not behavior.)*
- [x] T057 [US3] Refuse relocatable-mode assembly and entry and external symbol declarations in `CassoCore/MerlinDialect.cpp` via the boundary table, naming each construct (FR-016). *(The profile's share is one accessor, `MerlinDialect::GetSubsetBoundary` handing the table through a new `DialectProfile` virtual that defaults to an empty span. The refusal itself is in the pass-1 prelude, which is where the directive-row comment already said it belonged: "consulted before dispatch, so these rows stay null by design". Claimed there so a refused construct never reaches content dispatch and fails as an unknown directive.)*
- [x] T057a [US3] Make the relocatable refusal **actionable** in `CassoCore/MerlinSubsetBoundary.cpp`: a module with relocatable mode and entry symbols but no external symbols exports without importing, so the message states it assembles on its own once relocatable mode is removed and an origin supplied. A module declaring any external symbol gets the other message (no workaround, resolving cross-module references needs the linker in issue #112) because offering the first fix there sends the developer down a path that cannot work. The vendor's own sample is the export-only case, so this is the likely first encounter (FR-031). *(The contract's stated fix is incomplete and the message says more than it does: removing `REL` and supplying an origin leaves the `ENT` lines, each refused in its own right, so the message names all three steps. `PI.ADD.S` gets 7 refusals all carrying the fix, `PI.START.S` gets 5 with none carrying it and two denying it, and a test asserts the two files' first refusals are not the same string.)*
- [x] T058 [US3] Refuse the second occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp` as selecting an unemulated CPU (FR-016). *(A `SubsetBoundaryTrigger` column, so "accepted once, refused thereafter" is a property of the row rather than a special case in the driver. The FIRST occurrence is deliberately untouched, still recognized, unclaimed and doing nothing, which is T040's to fill. An occurrence inside a false conditional is neither refused nor counted, since the source never crossed the boundary there.)*
- [x] T059 [P] [US3] Refuse the file-type directive in `CassoCore/MerlinDialect.cpp` as owned by `020-disk-file-access` (FR-028). *(The message says "Casso's disk file-access support", not the spec number: a spec identifier means nothing to whoever reads the diagnostic, and the comment rule bans spec references in code anyway.)*
- [x] T060 [P] [US3] Refuse the save-object directive in `CassoCore/MerlinDialect.cpp` as multi-output segmentation needing its own decision, the message must NOT describe it as waiting on 020 (FR-029). *(Goes further than not-describing: the widening text denies it out loud, "a decision about multi-output assembly, which disk file access will not settle", so the wrong reading is refuted rather than merely omitted, and a test asserts that clause is present. It needed a fourth reason class; see the note above on the spec's "three reasons".)*
- [x] T061 [US3] Make a boundary refusal distinguishable from a syntax error in `CassoCore/AssemblySession.cpp`, and collect every offender across the whole pass before failing rather than stopping at the first (FR-017, FR-018). *(Distinguishable STRUCTURALLY: `AssemblyError::kind`, a new additive field. A test asserting a phrase in the message would be the bare-substring trap this feature has already been caught by, so the assertion is on the field and the negative half is asserted too. "Before failing" is taken literally, pass 2 does not run once the boundary is crossed, so the refusals are not buried under the undefined symbols a linker would have resolved.)*
- [x] T062 [US3] Generate the subset-boundary help text **from the boundary table inside `CassoCore/MerlinSubsetBoundary.cpp`**, returning a string that `CassoCli/CommandLine.cpp` merely prints. Generation in the executable would be unreachable from `UnitTest`, so FR-019's "cannot disagree by construction" would gain no test, and Principle VI is non-negotiable (FR-019, FR-024). *(`MerlinSubsetBoundary::GetHelpText` returns the string. **Nothing prints it yet**: `CassoCli/CommandLine.cpp` is one of the files spec 020 holds unmerged work in, so the print is T053b's along with the rest of the help output. Same intended state as T053/T078, reachable only from the tests until the wiring lands.)*
- [x] T062a [P] [US3] Add a test to `UnitTest/MerlinSubsetBoundaryTests.cpp` asserting the generated help text names every row the accessor returns, so a row added to the table without help coverage fails the build rather than shipping. *(Built from the row FIELDS rather than from the generator, or a mutation would move both sides and the test would stay green. Spelling, explanation and widening must appear on the SAME line as the construct, so a help text holding every value in two unrelated columns cannot pass; a second test counts listed lines against the row count in both directions.)*
- [x] T063 [US3] Populate `column` on every Merlin diagnostic in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-021). *(**Five files, not the two the task named, and the column travels with the FILE rather than with the message.** `Parser.h`, `AssemblerTypes.h` and `AssemblySession.h` come along because the position has to be carried and the deferred carriers have to hold it. `ParsedLine` gains three field columns the profile records as it segments; `AssemblySession` stamps one onto every diagnostic at each of the six places it already stamped `m_currentSourceFile`, so every caveat documented on that member carries over unchanged, including the deferred one. Three deferred carriers therefore capture their own: the boundary offense, `ConditionalState::openColumn`, and `m_currentMacroColumn`. **Divergence:** the ambient column answers for the LINE (the opcode field, else the label), and a diagnostic whose subject is a particular field uses a second recorder, `RecordErrorAt`, rather than moving the ambient value, 8 sites, the label and operand ones. A column of 0 there means the field was never written and the line's own column stands in, which cannot invent a position for a dialect recording none. as65 records none and its diagnostics are swept to prove they still report 0.)*
- [x] T064 [US3] Describe constructs in the active dialect's vocabulary, and name which dialect defines a construct rejected as belonging to another, in `CassoCore/AssemblySession.cpp` (FR-020, FR-022). *(Two halves. **Vocabulary:** three shared-engine messages quoted as65 spellings at a Merlin line, the origin and reserve-space directives, both of which Merlin has, and the assertion directive. They now quote `ParsedLine::directive`, the active dialect's own canonical name. **Divergence, and it is user-visible:** as65's text changes from a hand-written lowercase `.org` / `.ds` to the spelling its table holds, `.ORG` / `.DS`. Nothing in the tree pinned either, and the alternative was a dialect branch in shared mechanism. `.align` and the struct diagnostics are deliberately untouched; those tokens are as65-only and unreachable under Merlin, so changing them would be churn with no reader. **Attribution, and it took six files beyond the one the task named** (`DialectRegistry.{h,cpp}`, `DialectProfile.h`, `As65Dialect.{h,cpp}` and `MerlinDialect.{h,cpp}`) because a dialect can only be named by asking the registry, and the registry can only ask a profile through the seam: `DialectRegistry::FindForeignConstruct` answers which OTHER dialect defines a rejected word, and whether as a directive or as an alternate instruction spelling; it needed one new profile accessor, `GetDirectiveForSpelling`, so a foreign table can be consulted without its parser running. The category fragment carries its own article, since a caller cannot choose between `a` and `an` for a word it did not write.)*
- [x] T065 [US3] Explain the column rule when a Merlin label is indented, rather than reporting an unknown symbol, in `CassoCore/MerlinDialect.cpp` (User Story 3 acceptance 1). *(A profile virtual, `ExplainUnknownOperation`, defaulting to empty so the engine keeps its own wording for a dialect with nothing to add. **The engine supplies the one fact the profile cannot get for itself**, whether the field after the unknown word names something the assembler could execute, because the instruction tables are shared and arrive unnamed, and a profile reaching into them to compose a sentence is the seam leaking. Both conditions are load-bearing and both were mutated: dropping the operand test makes an as65 directive under Merlin get the column rule instead of its attribution.)*
- [x] T066 [P] [US3] Add the hand-authored negative corpus class (boundary refusals and diagnostic expectations, kept distinct from captured entries) to `UnitTest/MerlinCorpusTests.cpp` as hand-authored entries, including an entry where a macro is invoked with another dialect's argument syntax and must be **rejected rather than partially expanded** (spec Edge Cases). *(Nine entries, each stating kind, line, column, a phrase it must carry and one it must not, the last being the assertion a bare substring cannot make, since `mustSay` alone is satisfied by a message that also reports the symptom. No two entries expect the same column, and that is asserted over the table rather than left as a comment. **The macro entry needed a behavior change, not just a test**: a positional parameter the body refers to with no argument to fill it was substituted with empty text, so a call punctuated for another assembler assembled a different program in silence. It is now refused, and all six vendor oracles are unaffected, which is the evidence that real Merlin source never relied on it. **The entry's first version was VACUOUS and mutation is what said so**: its body failed a step later whether or not the call was refused, so the mutation that reported the mismatch and expanded anyway went uncaught. The body is now a data directive and a bare shift, both of which assemble cleanly under the empty substitution.)*
- [x] T067 [P] [US3] Add `UnitTest/MerlinSubsetBoundaryTests.cpp` sweeping the boundary accessor and asserting every row produces the expected refusal, and register it in `UnitTest.vcxproj`. *(28 tests. The sweep builds each row's source FROM the row, so a row added to the table is exercised without anyone editing the file, and it asserts the message quotes the row's own construct, explanation and widening rather than merely being non-empty. Three guards sit upstream of it, because a sweep cannot see the failures that would make it sweep the wrong thing: the table is non-empty, no two rows share a token, and every construct `Directive.h` marks as recognized-only-to-be-refused has a row; that last one is the inverse direction, which a table sweep structurally cannot check. The same sweep under AS65 asserts nothing is refused, so the refusal is proved to come from the active profile rather than from the token being in the shared vocabulary.)*
- [x] T068 [US3] Verify SC-006 and SC-007: every dialect-specific diagnostic identifies the correct line and column, and every out-of-subset construct is named rather than failing as a parse error. *(`BoundaryDiagnosticQualityTests` in `UnitTest/MerlinSubsetBoundaryTests.cpp`, swept from the accessor so a row added later is covered without anyone editing a test. SC-006 varies the indent per row, so no two rows expect the same column and a cumulative construct puts its two occurrences in different places, an implementation reporting the first occurrence's position for the second fails there and nowhere else. SC-007 asserts the refusal EXISTS and is the only thing said about the line; the existence guard is not decoration, since a row producing nothing at all satisfies "no other error" with zero on both sides. **Measured on the way:** a construct in column 1 is a label, so the boundary can only be crossed from column 2 onward, the sweep started at 1 and produced no refusal for any row. The criteria are not carried by this class alone: the diagnostics that are NOT refusals have no table to sweep and are pinned entry by entry in the negative corpus, each with an exact line and column.)*

**Checkpoint**: Diagnostics are dialect-native, positioned, and boundary refusals are unmistakable.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [x] T069 Add a synthetic, test-only third dialect profile to `UnitTest/DialectMechanismTests.cpp` and prove it works end to end; this is what catches a mechanism secretly built for exactly two dialects (SC-009). ~~**Do not pull this forward.**~~ **That hold has EXPIRED and the task was pulled forward deliberately**, rather than being contradicted silently. Its reason was that against a seam shaped by AS65 alone the synthetic profile gets written to fit whatever seam exists; Merlin has since pressed on the seam with the field model, operand-internal semicolons, quoted operands, mnemonic aliases, macro syntax, variable symbols and now the origin semantic, so the seam it is written against is a real one. **The profile must declare the OPPOSITE origin semantic from AS65**, or it never exercises the axis the emit-cursor split added and passes while testing nothing, which is the exact trap this task's own warning describes
- [x] T070 Verify SC-009 against **T069's commit alone**, not against `origin/master`: `git show --stat HEAD -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp` must be empty. *(Run against T069's commit and empty: that commit changes exactly one file, `UnitTest/DialectMechanismTests.cpp`. The synthetic profile reaches the engine through `AssemblerOptions::dialectProfile`, an injection point added in the PRECEDING commit for the reason the registry cannot supply: a closed table cannot demonstrate that a dialect outside it would work. Same shape as `fileReader`.)* Diffing against master cannot work, T013, T018, T033, T036, T037, T042, T061, T063, and T064 all modify `AssemblySession.cpp` earlier in this same feature, so that diff is never empty and the criterion 023 gates on would go unverified. The claim is that *adding a dialect* touches none of the three, which is a property of the adding commit
- [x] T071a [P] Add a **breaking changes** entry to `CHANGELOG.md`, as its own heading rather than inside the feature announcement: `CassoCli input.a65 -o out.bin` no longer works and becomes `CassoCli as65 input.a65 -o out.bin`, and a bare `CassoCli as65` stops resolving `as65` as a source filename (both T049a). **Written as part of T049a's single commit, not afterward**; this entry is the deliberate-act record that 020's tripwire test was protecting. A reader scanning for what will break must not have to find it inside a paragraph about dialect support. State the replacement invocation literally, so the entry is copy-pasteable into a build script
- [x] T071 [P] Update `CHANGELOG.md` with the merlin subcommand, the dialect mechanism, and the corrected include-file diagnostic attribution *(**Done, and it owed SEVEN items rather than three.** The merlin subcommand and the dialect mechanism were already written; added here are the corrected include-file diagnostic attribution and the four as65-visible changes this feature accumulated one slice at a time: the pass-2 rebind of reassignable constants (`name = expr` is as65's reassignable form, so a file assigning a symbol twice and referring to it between the assignments now emits different and correct bytes), the three shared-engine messages that hard-coded `.org` / `.ds` and now quote the active dialect's canonical spelling so as65 reads `.ORG` / `.DS`, the run that names no `--cpu` reporting the target that stood, and the usage line swept of the removed bare-source form. T071a's breaking-changes entry was left exactly as written.*

  *`DDB` and `VAR` are deliberately absent. Neither has an as65 spelling and neither can, so no as65 source can reach them, and Merlin itself is NEW in this release, so its individual directives are described by the dialect entry and by [docs/merlin-subset.md](../../docs/merlin-subset.md) rather than each earning a changelog line.*
- [x] T072 [P] Update `README.md` with the new dialect, the updated test count, and the roadmap position relative to `023-ca65-dialect` *(**Done. The test count was READ, not remembered: 3352 Debug at the time of writing, so the README says 3350+ where it said 2900+.** The assembler bullet's attribution was WRONG and is corrected: as65 is Frank A. Vorstenbosch's, not Frank A. Kingswood's, the real v1.11 banner is the source. The same misattribution survives in one released `CHANGELOG.md` entry and four documents under `specs/002-as65-assembler-compat`, all left alone as historical record. A Merlin bullet is added naming the mechanism, and the roadmap position is stated there rather than in a roadmap section the README does not have: ca65 is next, gated on this mechanism rather than on more Merlin, absolute subset first because full compatibility needs a linker. The CLI bullet also stopped being true when the bare-source form went, "runs as an AS65-style assembler by default" is now "an assembler under a named dialect".*
- [x] T073 [P] Document the supported subset and where it ends in the repository docs, deriving the list from `CassoCore/MerlinSubsetBoundary.cpp` (SC-008) *(**Done: [docs/merlin-subset.md](../../docs/merlin-subset.md).** The boundary section is the GENERATED help text quoted verbatim, with `CassoCore/MerlinSubsetBoundary.cpp` named as the authority for it and for the refusals, restating six rows in prose is how a document and a tool end up describing two different sets of rules. The doc covers the larger half the boundary table says nothing about: the field-based line model, the directive vocabulary, symbols and expressions, macros, and every place the implementation is documentation-led rather than settled by vendor bytes, so a reader can tell the two apart.*

  *It also records a gap this task FOUND rather than documented around: the `merlin` subcommand has no `-d`, so a source containing `KBD` cannot be assembled from the command line, and the assembler's own diagnostic tells the user to pass exactly that flag, which the subcommand then rejects as unknown. Verified by running the executable. Not fixed here: the flag table lives in `CassoCore/CommandLineParser.cpp`, which spec 020 holds unmerged work in.*
- [x] T074 Run `scripts/RunDormannTest.ps1`, required for assembler changes *(**Passes.** `6502_functional_test.a65` assembles under `as65` to a 65536-byte flat binary with 4 warnings and no errors. The downloads were real payloads, not error pages: 148497 bytes of source and a 65536-byte reference. That check was made by hand, because **this branch's copy of the script does not carry `--fail` on its `curl.exe` calls**, the hardening commit `bcaf69a3` is the single commit master holds and this branch does not, so it arrives at merge rather than needing to be written here. The 52893 differing bytes against the reference are the documented pre-Jan-2020 `zps` shift and are informational.)*
- [x] T075 Run `scripts/RunHarteTests.ps1 -SkipGenerate`, required for assembler changes *(**Passes, against the REDUCED set**, which the runner states rather than leaves to be assumed: 153 6502 opcodes and 256 rockwell65c02 opcodes, **200 vectors each**, 81800 vectors, resolved from the checked-in `UnitTest/HarteVectors/`. No full-depth set exists on this machine (`%LOCALAPPDATA%\Casso\HarteTests` absent, `CASSO_HARTE_DIR` unset), so this run is NOT the 10000/opcode evidence and must not be read as it. Suite 3391/3391 Debug.)*
- [x] T076 Run `scripts/Build.ps1 -RunCodeAnalysis` and `scripts/CheckStyle.ps1`, and confirm x64 Debug and Release are both green *(**All three pass.** Code analysis: **0 warnings, 0 errors** across all eight projects, with `CodeAnalysisTreatWarningsAsErrors` on. CheckStyle: 84 files checked over the lines added between `origin/master` and `HEAD`, OK. Suite: **Debug 3391/3391** against a `UnitTest.dll` the analysis build had just relinked, **Release 3388/3388**, no `EhmAssert` in the Release run. The Release DLL is the one already on disk: an incremental Release build found nothing to do, and `RunTests.ps1`'s staleness guard, which compares the assembly against the newest tracked source, let the run proceed, so it is up to date rather than stale. `-AllowStale` was not used.)*
- [x] T092 [US1] Commit the vendor SOURCES as ordinary Windows text (seven-bit ASCII, CRLF, no BIN header) and read them without decoding *(**Done, and the reason is stronger than convenience.** The only decoder lived in `UnitTest/MerlinCorpus/MerlinFixture.cpp`, so a fixture was readable by the test project and by nothing else: `CassoCli merlin` takes text off the host filesystem and saw question marks. The corpus that measures the dialect could not be fed to the tool the dialect ships in. **Lossless for everything the assembler can see**, across all ten sources the only bytes below `$80` were spaces, and the parser already accepted CR, LF and CRLF. What went is the stored spelling of a space, `$A0` between fields against `$20` inside comment text, which looked like a free lexer and was true only of files authored on a Merlin disk; it is now absent rather than merely forbidden. **Objects were NOT touched**; they are the expectation, so transforming them transforms the answer. The type-B/type-T split in the loader went with the headers, and the two decode tests became one sweep over all ten sources. `.gitattributes` states the classification per half rather than leaving it to the NUL heuristic, and a test pins the CRLF so the attribute cannot be dropped silently.)*
- [x] T093 [US1] Make `scripts/ExtractMerlinFixtures.ps1` transcode as it extracts, take `-Verbatim`, and record both hashes *(**Done, and the transcode is verified rather than assumed**: run against the pre-conversion bytes it reproduces all ten committed files byte for byte, so a future `-Force` run from the hash-pinned disk regenerates what is committed instead of quietly reverting it. This is what replaced "every file is byte-identical to the disk" as the provenance claim; the chain is re-run rather than hashed in place, and `README.md` carries both figures per file. **The licensing reading is stated rather than implied**: the ND term forbids an adaptation, and reproducing a work in another format is not one. `-Verbatim` exists so that reading can be reversed without archaeology.)*
- [x] T094 Add `--dos-bin` and `--flat` to the merlin grammar, and an end-to-end script that reproduces every shipped object through the executable *(**Done, with no engine work**: `CassoCore/OutputFormats` already had all three writers and `CassoCli` already dispatched on the shape for any dialect, only the grammar refused to name them. Shapes are a TABLE for the reason the flag table is one, with sweeps in both directions so an accepted-but-undocumented shape and a documented-but-unmatched one both fail. **They are matched as whole words ahead of the letter loop, which a test pins**: `--flat` read a character at a time is `-f -l -a -t`, and the `l` arm generates a listing, so the failure would be an unasked-for output file rather than a warning. Mutating the early match away fails that test and the shape sweep. `scripts/RunMerlinOracles.ps1` reproduces all six objects through the exe and compares the WHOLE file including the header, which is stricter than the corpus tests. **as65 still has no `--flat`**, left deliberately, since widening a hand-rolled walk is not this feature's business.)*
- [x] T096 Fold the help to the reader's terminal instead of to a width chosen for them *(**Done: `CassoCore/UsageText`, nine tests.** Usage is now written one logical line per item and folded at print time, which is what let the run section stop being a paragraph the reader has to re-split by dialect: `--as65` and `--merlin` each carry the assembler options that come along with them, on their own row, since that is the only thing that differs between the two here. **The continuation column is FOUND rather than passed in**, from the last run of two or more spaces on the line; that gap is the gutter between a flag and its description, so a flag row continues under its description and a line with no gutter is prose that continues at its own indent. Passing the column instead would let a row and its continuation disagree about it, which is exactly the drift the hand-wrapped text had. A word wider than the column overhangs rather than being cut; a path broken across two rows cannot be told from a path with a break in it. Redirected output folds at 80; a file has no width to ask about and gets read in an editor. `docs/Assembler.md` lost its last two `--cpu` references, which the 1.18 removal left behind. **CassoCli gained a `Pch.h` of its own**, asking the console its width needs `<windows.h>`, and included after `CassoCore/Pch.h` it arrives to find Ehm has already defined `S_OK` and its five siblings portably, so `<winerror.h>` redefines every one of them and code analysis went from 0 warnings to 6. The new header pulls Windows in ahead of the core one, which is what `Casso`, `Dxui` and `CassoEmuCore` already do, and settles the platform for every translation unit in the project rather than for whichever file reached for a console API. Back to 0 warnings.)*
- [x] T097 Make `CassoCli/CommandLine.cpp` obey the free-function rule it had been ignoring *(**Done, 34 file-scope functions became one class.** The constitution says file-local helpers are class `static` members and that a free function needs a very convincing justification; this file had twenty-seven of them plus seven free entry points, and I had just added four more. `CommandLine.h` now declares a `CommandLine` class: eight public statics, which is the whole of what the executable can call, and the rest private, so the helpers are visibly the inside of one thing rather than thirty-four things at namespace scope. `AssembleResult` nests as a private struct, per the file-local-type rule for a plain-data struct used only as a parameter. `ParseCommandLine` is `CommandLine::Parse`, the old name repeated its scope. Behavior is unchanged: same 3457/3454, oracles byte for byte, code analysis 0 warnings. **112 file-scope static functions remain across 40 other files** and are NOT touched here.)*
- [x] T098 Split `CassoCli/CommandLine` into the things it was actually doing *(**Done, one 1,780-line file became eight classes.** T097 put the free functions on a class, which made the real problem visible: the class was named for the command line and contained `WriteSymbolFile`. It now parses and prints usage and nothing else. `SourceAssembler` turns a source into an `AssemblyResult` and reports its diagnostics, needed by all three subcommands, owned by none of them, and the reason `run` can assemble without acquiring an opinion about output files. `ArtifactWriter` turns a result into files; `run` never includes its header. `HostFile` is the two whole-file reads, kept apart from the assembler's own `FileReader`, which resolves an include against a base directory and is a different question. `AssemblerMode` is the template method both assembler subcommands were separately open-coding, with `As65Mode` and `MerlinMode` supplying the four things that differ: the instruction tables, the object's name, the progress printed along the way, and which extra artifacts exist. `RunMode` is `run`, and is deliberately NOT an `AssemblerMode`, those exist to produce files, this one produces a result and passes through an assembly on the way. **The exit codes stopped being hand-rolled per dialect**: `as65` computed `ok ? 0 : 2` then `warned ? 1 : 0` at the far end of the function, which is `AssemblerExitCode::FromResult` written out longhand, so both dialects now call the core mapping and a write failure is the only thing that overrides it. Verified unchanged end to end: 3457/3454, the six oracles byte for byte, Dormann, the demo disk reproducing its committed image, code analysis 0 warnings.)*
- [x] T099 Return an honest HRESULT from the modes, and name the hooks as command-object *(**Done, two corrections from review.** `AssemblerMode::Run` and `RunMode::Run` were stamping `S_OK` over every failure with `BAIL_OUT_IF` and carrying the truth in the exit code alone, which is a function lying to its caller about whether it worked. Both now return an HRESULT AND set the exit code, and the two are deliberately not derived from each other: a missing input is `ERROR_BAD_ARGUMENTS`, a source that will not assemble is `ERROR_INVALID_DATA`, a failed write carries whatever the writer said, and an assembly that merely warned is `S_OK` and exits 1, which is the case that shows why one cannot stand in for the other. An illegal opcode under `run` stays exit-code-only: the program faulted, the tool did not. The exit matrix was re-measured on the executable across all three subcommands and fifteen shapes and is unchanged. **`NarrowInstructionSet`/`WideInstructionSet` were nouns**; they are `SelectInstructionSet`/`SelectExtendedInstructionSet`, and the progress hooks are `ReportAssemblyStarting`/`Finished`/`Succeeded`, `OutputName` is `ResolveOutputName`.)*
- [x] T100 Hand the modes a provider, and fail the illegal-opcode run *(**Done, two more from review.** The two instruction-set hooks were two names for one decision, and names (`Narrow`/`Wide`, then `InstructionSet`/`ExtendedInstructionSet`) that described neither what the sets were nor anything a fourth architecture could reuse. They are one hook now, `CreateInstructionSetProvider`, returning core's own `InstructionSetProvider`; `Assembler` gained a constructor that takes one, so `SourceAssembler::Assemble` stopped unpacking the decision into arrays and branching on whether the second was null. The vocabulary is core's, base and extended, and each mode says which CPUs those are in its own words: AS65 takes the base from `-x` and has no directive, Merlin starts on the 6502 and lets `XC` reach the 65C02. **An illegal opcode under `run` is now a failed run**: `HRESULT_FROM_NT (STATUS_ILLEGAL_INSTRUCTION)`, exit 3, where the previous commit had called it the program's fault and returned `S_OK`. `E_INVALIDARG` stays out of all of it because the constitution reserves it for a coding error that asserts; a user who typed no filename is not one. The exit matrix was re-measured at nineteen shapes, now including `-x`, `XC`, and the illegal opcode.)*
- [x] T101 Say that both dialects choose between the same two CPUs, and let `run` ask the dialect *(**Done, and it found a bug.** The previous note called AS65 "one set, with nothing to switch to", which mistook the engine's shape for the dialect's: AS65 is the 6502 by default and the 65C02 under `-x`, exactly Merlin's two CPUs, chosen once on the command line instead of from the line `XC` appears on. `As65Mode::CreateInstructionSetProvider` now names both tables and the choice between them, and the hook's own comment says what differs is WHEN. **`run` had a second copy of the CPU decision** (`SourceAssembler::SelectInstructionSet`, one set, always) so `run --merlin` on a source with `XC` was refused: *"XC selects a wider instruction set, and this assembly was given only one instruction set to work from"*, exit 1, measured on the executable. The copy is deleted; `run` now asks `AssemblerMode::CreateFor (options.dialect)` for the same provider the subcommand would build, and the hook is public for that reason. **The HRESULTs settled on**: a missing input file is `E_INVALIDARG`, the user's call, since nothing asserts on that path and the constitution's concern was the assert, not the value; bad input data, whether a source that will not assemble or a binary with an illegal opcode, is `HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)`. `STATUS_ILLEGAL_INSTRUCTION` was rejected as a kernel-mode value in user-mode code.)*
- [x] T102 Refuse an argument the grammar does not know, and print the help that applies *(**Done.** An unknown flag used to be a warning printed from inside the parser, after which the assembly ran, exited 0, and wrote its output as though the flag had been honored, a typo in a build script was silent in every way that mattered. The parser now RECORDS the first unknown argument (`CommandLineOptions::unrecognizedFlag`, as typed, with the prefix the user used) and prints nothing, which is also what lets the UnitTest project see it; the edge refuses the invocation with exit 2, prints the FULL usage, and puts the line naming the argument last, since the bottom of the screen is what a reader sees. (A first cut printed only the offending mode's section; that split belongs to 020, which owns the per-mode help, so it was taken back out.) A first word that names no subcommand gets the same shape, with its "type this instead" line at the end. Five parser tests pin the field; suite 3462 Debug / 3459 Release. The oracles, Dormann and the demo disk all still go through, which is the check that no script in the repo relied on an unknown flag being ignored.)*
- [x] T077a Update `CLAUDE.md`'s spec inventory **at merge time**, not before: move 019 from "drafted but NOT started" to shipped, and strike the sequencing note that says 023 must wait on it, 023's gate is satisfied once this lands. Deliberately deferred rather than done during the feature, because `CLAUDE.md` names spec 020 as active and the concurrent session owns that block; editing it early would put the two specs in conflict on a shared file. Leave the active-spec pointer alone entirely; that is 020's to change *(**Done as the last commit before the merge.** 019 moved from the not-started list to the shipped list, 023's gate is recorded as satisfied, and the 020 active-spec block is untouched, `CLAUDE.md` had not changed on master since the merge base, so there was nothing to collide with.)*
- [x] T077 Revert `.specify/feature.json` to `specs/020-disk-file-access` before merging, so master does not thrash between two concurrent specs. This is the **only** mechanism for that file, plan.md states the same, and staging by explicit path throughout the feature is what keeps it from being committed accidentally in the meantime *(**Already true.** The committed file on this branch and on master both name 020; only the working copy ever said 019, and it was never staged. Discarded with `git checkout --`, so there is nothing to revert in history.)*

---

## Dependencies

**Phase order**: Setup → Foundational → US1 → US2 → US3 → Polish.

**Hard blocks**:

- T010 gates everything. If the seam changed behavior, no later phase's evidence is trustworthy.
- T016–T018 block T040; the CPU-selection directive has nothing to switch without the provider.
- T020 and T020e block T027 onward. The harness and the fixture read path are what make any parser claim checkable; without them the parser has no oracle.
- The **settle-by-capture** entries block only the tasks whose semantics they settle, not the phase: T025a → T028, T022 and T025b → T029, T023 → T030, T025c/T025d/T025e → T038, T025 → T040, T025f → the conditional handling in T038's neighborhood, T024 → the expression work T026 records. Each is a specific question with a specific dependent, and pretending the whole block gates the whole phase would idle work the committed fixtures already unblock.
- The five vendor oracles block nothing: they are committed. T045a and the rest of the T045-series can run as soon as T020e lands, which is why they no longer sit behind capture.
- T056 blocks T057–T062a. The boundary table is the single source those all derive from, including its generated help text.
- T033 blocks T035–T040. All the new `Directive` tokens land in one commit so the exhaustiveness-checked `switch` breaks once rather than at every directive task.
- T005 blocks T051. The `--cpu` refusal reads the profile's `cpuSource`, so the field must exist on the seam before the parser can consult it.
- T049a is a **single commit** and cannot be split. `s_kSubcommands` on master holds only `{ "run", Subcommand::Run }`, so `Subcommand::As65` is reachable solely through the fallback; adding the `as65` row, removing the fallback, and updating the tests in separate commits leaves a midpoint where AS65 is unreachable and a bisect lands on it. It is also gated on spec 020's command-line work merging first; see the task for why the `disk` row changes the shape of what is being removed.
- T069 depends on the whole mechanism, so it lands last despite being the criterion 023 gates on.
- T070 must be evaluated against T069's own commit. Diffing against `origin/master` cannot work, because earlier tasks in this feature legitimately modify `AssemblySession.cpp`.

**Story independence**: US2 and US3 both depend on US1's profile existing, so they are not parallel with it. They are independent of each other and can proceed in either order once US1 lands.

## Parallel Execution Examples

**Setup**; all three are independent files:

```
T001, T002, T003
```

**Corpus capture within US1**; each writes a distinct entry:

```
T022, T023, T024, T025      # the settle-by-capture entries
T043, T044, T045            # the corpus floor completion
```

**Independent implementation within US1**:

```
T033 (new Directive tokens)   +  T034 (StringEncoding)
T036 (loop construct)         +  T037 (dummy sections)
```

**Within US3**, distinct refusals and distinct test files:

```
T059, T060                  # file-type and save-object refusals
T066, T067                  # negative corpus and boundary sweep tests
```

**Polish**, documentation is independent of validation:

```
T071, T072, T073
```

## Implementation Strategy

**MVP is Phase 1 through Phase 3.** At that point unmodified Merlin source
assembles to Merlin's bytes, which the spec calls "the entire feature." US2 and
US3 are refinements on top, valuable, but a developer can already stop porting
their codebase.

**Commit per phase**, per the constitution's commit discipline. Do not accumulate
phases into one commit.

**Validate cheaply on the branch**: compile the touched project and run the
narrowest relevant tests per commit; Release runs the suite in roughly 2 minutes
against Debug's 15. Defer the full gate (both extended suites, code analysis,
and style) to the pre-merge check in Phase 6.

**Merge with `--no-ff`.** CONTRIBUTING forbids squashing.

**One deferral is deliberate and already recorded**: the "unrecognized first
argument falls back to AS65" heuristic stays. Issue #92 wants it gone, but it
breaks the documented `CassoCli input.a65 -o out.bin` form and needs its own
decision plus a CHANGELOG entry. The reasoning is on issue #92; do not remove it
as part of this feature.
