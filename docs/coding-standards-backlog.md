# Coding-standards backlog

Work queue for bringing the tree in line with `.github/copilot-instructions.md`.
Ordered: finish each item before starting the next.

`scripts/CheckStyle.ps1 -Mode Tree` reports current counts. The pre-push gate
(`.githooks/pre-push`) is diff-scoped, so the backlog below never blocks a push —
it only stops *new* violations.

## Enforced today

| ID | Rule | Backlog |
|----|------|---------|
| `CS0001` | no space before empty parens in `.cpp` | 0 |
| `CS0003` | American spelling | 0 |
| `CS0004` | angle-bracket includes outside `Pch.h` | 0 |
| `CS0005` | `Pch.h` first include in every `.cpp` | 0 |
| `CS0007` | space after a C-style cast | 0 |
| `CS0008` | no Claude attribution in commit messages | 0 |
| `CS0010` | no `-Ex` macro passing its family's default hr | 0 |
| `CS0009` | do not *produce* `S_FALSE` | 0 |
| `CS0002` | no anonymous namespaces | 0 |
| `CS0006` | no bare `goto Error` | 0 |
| `CS0011` | no call of any kind in an EHM condition | 0 |
| `CS0012` | `Ehm.h` comes from `Pch.h`, never directly | 0 |
| `CS0013` | no dash-decorated section comments | 0 |
| `CS0014` | a `.cpp` function definition has an 80-slash banner | 0 |
| `CS0015` | a top-level banner is preceded by exactly 5 blank lines | 0 |
| `CS0016` | a declaration block is followed by exactly 3 blank lines | 0 |

`CS0011` now covers `SUCCEEDED` / `FAILED` / `IGNORE_RETURN_VALUE` as well as
the `CHR`/`CBR`/`CWR`/`CPR` families — 246 sites, production and tests, all
converted. Two notes on how it got there:

**The test exemption was wrong and got withdrawn.** 196 of the 246 were
`Assert::IsTrue (SUCCEEDED (Foo()))`, which looked like the sanctioned test
idiom, so the first cut of the rule skipped `UnitTest/`. Then the fix turned
out to be better than the exemption: `AssertSucceeded (Foo())` reads the same
and **prints the HRESULT**, which `Assert::IsTrue` never could — it only sees
a bool, so a failing fixture path reported "Expected: 1, Actual: 0" instead of
`0x80070002`. Verified by injecting `E_ACCESSDENIED` and reading the message.
`UnitTest/HResultAssert.h`, reachable from the test `Pch`.

**The scanner ignores comments now.** It had flagged the doc comment on
`JsonValue::HasString`, which quotes the pattern it exists to replace. A
checker that fails a file for *describing* the rule is one people switch off.

## Structural rules — ARMED, backlog zero

The three formatting rules a per-line regex cannot see are now enforced by
default. `-NoStructural` opts out.

| ID | Rule | Was | Now |
|----|------|----:|----:|
| `CS0014` | a `.cpp` function definition has an 80-slash banner | 236 | 0 |
| `CS0015` | a top-level banner is preceded by exactly 5 blank lines | 1,055 | 0 |
| `CS0016` | a declaration block is followed by exactly 3 blank lines | 594 | 0 |

**How the diff scoping works.** The analysis is whole-file; the *reporting* is
scoped. Each finding carries the line range it is about, and in `Diff` mode it
is reported only when the diff added a line inside that range — so editing the
body of an old function stays silent while adding a function does not. Ranges
rather than single lines, because the evidence spans lines: a wrong banner gap
is as much about the blank run as the banner.

The switch shipped **off** for one commit, deliberately: 1,882 tree-wide
scoped down to 260 on this branch, but those 260 sat on lines the branch itself
had added, and scoping cannot help when the branch is what introduced them.
Arming it then would have blocked every push instead of only new violations —
the failure that made the first cut of CS0011 unusable. Working the backlog to
zero first is what made arming it safe.

**The fixers are transcriptions of `Test-Structure`, not reimplementations.**
Re-deriving "what is a top-level banner" or "what is a declaration block" in a
separate script lets the two drift, and a fixer that disagrees with its gate
either edits lines the gate does not care about or leaves ones it does.

Three mechanical details that a future sweep of this kind needs:

* **Apply bottom-up, CS0016 before CS0015.** Every edit shifts the line numbers
  below it, and CS0016 anchors on function bodies whose offsets CS0015 moves.
* **Preserve line endings per file.** Twelve files in the tree are LF. Writing
  with `Environment.NewLine` would have turned a 24-line diff into a whole-file
  rewrite and buried the real change.
* **Prove the blast radius, do not assert it.** The blank-line sweep touched
  401 files; `git diff --ignore-blank-lines --ignore-all-space` came back empty
  over the whole thing, which is what makes "blank lines only" a fact. For the
  banner sweep the equivalent check was that all 78 removed comment lines
  reappear inside the banners that absorbed them.

**CS0014 absorbs existing prose rather than stacking on top of it.** 78 of the
228 functions already had a `//` comment above the signature; that comment *is*
the function's documentation, so it moves into the banner body instead of being
stranded above a new banner that repeats the name.

Two conventions are read off each file rather than assumed, because the tree is
not uniform: banner width (mostly 80 slashes, a few 72) and whether banners name
`Class::Method` or just `Method`. Both vary per file and both are majority-voted
from the banners already there.

**One measurement correction worth keeping.** An earlier estimate put CS0015 at
~440. It is 1,055. The first scanner counted every banner twice — a banner is
two rows of slashes and the closing row always has zero blanks before it — and
"fixing" that by halving was wrong too; the real check is to count only the
*opening* row, which is the one followed by a `//` line.

The overwhelmingly common wrong value was **4**, not 3 or 6 (872 of 1,055). The
rule says 5 and the standards' own example shows 5, so this was uniform drift
rather than anyone misreading the rule.

Verified by mutation: deleting one blank line from `TrackSectorPredicate.cpp`
makes the gate name the file, line, actual count and required count, and
restoring it returns the tree to green.

## Test counts differ by config, on purpose

Debug **2809**, Release **2807**. Both must report `Test Run Successful` — a
count on its own proves nothing, which is the whole point of this section.

| Only in | Tests | Why |
|---|---|---|
| Debug | 4 × `DxuiPopupHostPoolTests` | assert on `PopupHits()`/`PopupMisses()`, which are `#ifdef _DEBUG` |
| Debug | `CycleEmulation_SkippedInDebug` | sentinel |
| Release | `CycleEmulation_MeetsBudget`, `CycleEmulation_StableRunToRun` | perf assertions are meaningless unoptimized |
| Release | `PoolInstrumentation_SkippedInRelease` | sentinel |

Each side names what it is not covering, so the delta reads from the run output
instead of needing a bisect. That is deliberate: Release once aborted at 1368 of
2804 and the low count read as "fewer tests in this config" rather than "the
host crashed" — see the `ExpectedEhmAssert` entry below.

The counts are *not* evened up by stubbing the missing tests to pass. A green
`FirstAcquire_SeedsPoolToInitialSize3` in Release would claim pool coverage that
does not exist, and the alternative — making `m_popupHits` unconditional — is
changing production code to serve a metric. The pooling logic is not itself
conditional, so Debug already proves it.

## Queue

### 1. `CS0002` — anonymous namespaces — DONE

Placement settled, and it held for all 53: constants split by usage
(single-use → function-local `constexpr`, multi-use → private
`static constexpr` member); helpers → class statics; types → nested private
types. Where several `TEST_CLASS`es in one file shared a helper, no single
class owned it, so it became a file-scope `static` — the same "inventing a
class would be worse than the rule it satisfies" call as `82a96037`.

**The types were the real hazard, not the helpers.** Most file-local
functions were already `static`, which is all the internal linkage they
need. A bare `struct` or `class` in a `.cpp` has external linkage and there
is nothing you can write in the `.cpp` to change that — `static` does not
apply to types. The sweep turned up four genuine duplicate-name pairs living
one `#include` apart:

| Name | Declared in |
|---|---|
| `ShaderSource` + `LoadShaderSource` | `CrtPostProcess.cpp`, `SettingsCompositor.cpp` |
| `FindKey` | `MachineConfigUpgrade.cpp`, `SettingsPanelState.cpp`, `UserConfigStore.cpp` |
| `kpszVersionKey` | `MachineConfigUpgrade.cpp`, `SettingsPanelState.cpp`, `UserConfigStore.cpp` |
| `MakeRect` | five UI/test files |

Identical layouts today, so nothing was broken — but that is luck, not
design, and the linker reports none of it.

One conversion cost something worth recording: `StartupDownloadDialog.h` was
deliberately kept to forward declarations, and nesting its three panel
classes forced `DxuiPanel.h` / `DxuiDialogWindow.h` into it. Three files
include that header and two already depended on Dxui, so it was worth
paying; a file with a wider include fan-out might not be.

**The 3-line exception.** A constant whose declaration spans 3 or more lines
stays a file-scope `static constexpr` with its `s_k` prefix, even when only one
function reads it. See `copilot-instructions.md` for the full placement order.

The threshold is measured rather than asserted. Of 1,466 constant declarations
in the tree:

| Span | Count |
|---|---|
| 1 line | 1,411 (96%) |
| 2 lines | 8 |
| 3+ lines | 47 |

The distribution is bimodal, and every member of the 3+ group is a table or
payload — `s_kUndocumentedOpcodes` (82 lines), `s_kEntries` (30),
`s_kRomCatalog` (22), `s_kInkPalette` (19), `s_kReservedNops` (17),
`kWriteTranslate` (11), the two HLSL shader sources. None is a tuning
parameter. Below the line you have a value the logic hinges on and it belongs
next to that logic; at or above it you have data the function merely consults.

Heaviest: `UserConfigStore.cpp` (621-line block), `ThemePage.cpp` (395),
`AssetBootstrap.cpp` (209).

### 2. `CS0006` — bare `goto Error` — DONE

All 16 remaining are in `AssemblySession.cpp`, spread over 11 functions —
`HandleIncludeDirective` (3), `BuildListingEntry` / `DetectMacroDefinition` /
`ExpandMacro` (2 each), and one apiece in seven more.

**`ProcessPass1Line` is done** (was 13 of these). Earlier notes here said all
29 were in that one function; they were not — it held 13 and the rest were
scattered through the same file.

The fix was not a state machine. Pass 1 is a **chain of responsibility**: the
stages run in order and the first to claim the line wins. It resisted a switch
because the order is not derived from any one mode value — three stages are
modal short-circuits (collecting a struct body, collecting a macro body,
skipping an inactive conditional) but the rest are a priority list, and two
deliberately run *while* skipping so `.ENDIF` / `.ELSE` and macro-definition
starts are still seen.

What removed the gotos was noticing that all 13 exits did the same two things:
`m_lineInfos.push_back (info); goto Error;`. Hoisting that push into a wrapper
left each stage needing only an exit, which is plain `BAIL_OUT_IF`. The
invariant — record the line exactly once, and never on a failure path — now
lives in one place instead of being retyped 13 times.

Verified byte-for-byte, not just by the suite: a throwaway harness assembled
all 15 in-repo `.a65` sources before and after and compared an FNV-1a-64
digest of the emitted bytes plus the diagnostic shape. All 15 identical. Worth
the detour on an assembler — the unit suite is good (253 assembler tests plus
Dormann, which assembles *and* executes) but a reordering bug hides in exactly
the corner no test covers.

Shapes that did convert, and what each turned into:

| Shape | Conversion |
|---|---|
| jump to the label on the next line | delete it — it was already a fall-through |
| set `hr`, exit | `BAIL_OUT_IF (cond, hr)` |
| set state + `hr`, exit | keep the `if` for the state, follow with `BAIL_OUT_IF` |
| set `hr` + a message, exit | `CBRFEx (cond, hr, msg = ...)` |
| `else` branch of a dispatch | hoist to a `CBRAEx` guard, then a ternary |
| `default:` in a switch | assign `hr` and `break`, then `CHR (hr)` after |
| the same mapping at N exits | do it once at the `Error:` label |

That last row is the one worth reaching for. `PrintToWindowsPrinter` repeated
an identical "was this a user cancel?" block at four spool calls; moving it to
the label left each call site as a single `CHRF` and gave the rule one owner.

**A condition that calls a function must be hoisted to a local first** — the
macro hides the call otherwise. `PrintDlgW`, `IsLoaded`, and two
`atomic::load`s each needed a named local before the guard could read them.

One near-miss worth recording: `SavePrintoutAs` and `PrintToWindowsPrinter`
preset `outOutcome = Delivered` and correct it to `Canceled` at each exit,
which looks backwards — until you notice the caller tests
`outcome == Canceled` *before* `SUCCEEDED (hr)`. Flipping the default so
"canceled" is the safe fallback would route genuine failures down the
cancel branch and suppress the error dialog. The optimistic default is load-
bearing; check the consumer before inverting one.

### 2b. Assembler: tokenize, then drive from tables

Not a style rule — an architecture item that subsumes several of them. The
directive layer is where `AssemblySession.cpp` degenerated: 29 directives
dispatched by *name* across five `else if` chains (`HandlePass1Directives` 16
branches, `EmitDirectiveBytes` 8, `HandleConditionalDirective` 5,
`HandleSegmentSwitch` / `IsSegmentDirective` 2 each), with no single place
listing the vocabulary. Adding a directive means editing three chains and
hoping.

**Why this ordering matters beyond tidiness.** Casso intends more 6502
assembler dialects and more CPUs. The CPU seam already exists — `OpcodeTable`
is injected, so a new instruction set is tractable today. The *dialect* seam
does not: a second syntax currently means editing ~84 scattered string
compares. Tokenizing moves a dialect's whole vocabulary into one table and
leaves the semantics shared.

```
DirectiveTable   spelling -> token      <- the DIALECT varies here
DirectiveInfo    token    -> { pass1, pass2, flags }   (array, indexed by token)
OpcodeTable      mnemonic -> encoding   <- the CPU varies here (already exists)
AssemblySession  shared
```

Stages, each verifiable byte-for-byte (see below):

1. **DONE** — `enum class Directive` + `DirectiveTable` (spelling -> token),
   covering dotted canonical names and as65's bare synonyms. Replaced a
   35-line if/else chain in `Parser.cpp`; its literal string compares went
   32 -> 9.
2. **DONE** — `ParsedLine::directiveToken`, set on both parser paths.
   Unknown dotted spellings resolve to `Directive::None`, which is what
   pass-1 dispatch already treats as unhandled.
3. **NEXT** — `HandlePass1Directives` -> array indexed by token. All 16 arms
   take `(current, info)` plus `this`, so the row is uniform:
   `HRESULT (AssemblySession::*)(const PendingLine &, LineInfo &)`. Four
   handlers already exist (`HandlePass1DataDirectives`,
   `HandleIncludeDirective`, `StartStructDefinition`, `HandleCmapDirective` —
   the last needs `current` added for signature uniformity); the other twelve
   become small named members, which is what removes the calls-inside-EHM-
   macros rather than a separate cleanup pass. A `static_assert` that the
   array length equals the enum count makes a missing row a build error.
4. `EmitDirectiveBytes` -> the pass-2 column of the same row. Today pass 1 and
   pass 2 each carry their own copy of "which directives exist" and can
   disagree.
5. `HandleConditionalDirective` -> switch on the token; its bare mnemonic
   forms (`IF` / `ELSE` / `ENDIF`) join the spelling table.
6. Put the spelling table behind a dialect interface.

7. **Mnemonics get a token too.** Same argument as directives, and the
   objection I first raised against it was wrong. I claimed a `Mnemonic` enum
   would have to be "the union across every CPU, with holes per CPU" — but the
   *code that acts on the comparison* is already that union
   (`IsBitOpMnemonic` naming Rockwell ops, the `JMP` size cases). Paying
   it in string literals instead of enum values is strictly worse: a
   misspelled literal silently never matches, a misspelled enum will not
   compile. The `Directive` enum has the same shape and nobody minds — it
   lists all 27, and a dialect's spelling table populates a subset.

   The mnemonic compares that motivated this are now gone, but they went two
   different ways, which is the useful part. `IsBranchMnemonic` and both
   `EstimateErrorRecoverySize` cases became opcode-table questions, because what
   they wanted was a *CPU* fact the table already held — no enum needed.
   `IsBitOpMnemonic` and the `nop <count>` guard stayed as literals, because
   they are *dialect* facts the CPU table cannot answer: the table holds
   `RMB0..RMB7`, and the bare `RMB` spelling exists only because as65 writes
   the bit as an operand. So a `Mnemonic` enum is worth less than it looked —
   ask which seam a compare belongs to first; several turn out to be table
   lookups already available.

   The real constraint is different and bigger: mnemonic names originate as
   `const char * instructionName` inside `Microcode`, populated by string
   literal across `Group00/01/10/Cmos/Misc.h` (64 distinct mnemonics), and
   `Microcode` is shared with the **emulator's** CPU — `Cpu65C02`,
   `Cpu65C02Table`, `Cpu.cpp` and their tests all consume it. So adding the
   token is a change to the CPU instruction definitions, not just to the
   assembler.

   Do it additively, exactly as `ParsedLine::directiveToken` was done: add
   `Mnemonic mnemonic` to `Microcode` alongside the existing name, key
   `OpcodeTable` on the enum, resolve it once in the parser via a
   spelling table (which is also where the bare Rockwell forms belong — see
   below), and convert consumers one at a time. Per-CPU variance needs
   nothing new: a CPU simply has no `Microcode` row for a mnemonic it lacks,
   which is what `IsMnemonic()` already reports.

**9. `ResolveAddressingMode` — DONE.** Filed as wanting a 2-D syntax x
mnemonic-class matrix. It did not need one. Every arm had the same shape:
try a short list of candidate modes in priority order, take the first the
mnemonic carries, else a per-syntax fallback. That is one row per
`OperandSyntax` — 15 lines for the whole policy — plus a 9-line loop. The
21 returns collapsed to 1 as a side effect, not as multi-return work.

Two things the switch had hidden. `IsBranchMnemonic` was only a separate
concept because it predated the opcode table answering it; it is exactly the
ungated `Relative` candidate. And every arm built an `OpcodeEntry` it never
read, because `Lookup` was standing in for `HasMode`.

**10. `ExpandMacro` — nothing to do; the queue entry was stale.** It is already
50 lines of straight EHM. The mess had moved into `SubstituteMacroParams`,
`CheckForExitm` and `CountExitmIfDepth`, and it was the same duplication as
everywhere else rather than a scope problem:

* `CountExitmIfDepth` wrote out `IF/.IF/IFDEF/.IFDEF/IFNDEF/.IFNDEF` and
  `ENDIF/.ENDIF` by hand — the **third** copy of the vocabulary, after the
  parser and `ParseStructMember`. Now `DirectiveTable::FromSpelling` plus a
  test of which tokens open a block and which closes one.
* Eight hand-rolled `toupper` loops across the file, plus two copies of
  "trim, cut at `;`, trim" and three of "first word of the line". Now
  `ToUpperCase` / `StripCommentAndTrim` / `GetLeadingWord`. One `toupper`
  remains, inside `ToUpperCase`.
* `GetUpperOperand` was a duplicate of `ToUpperCase` that I added without
  noticing the existing one. Deleted; it had a single caller.

`EXITM` and `LOCAL` stay string compares on purpose. `DirectiveTable` feeds
`Parser::ParseLine`, so adding them would tokenize those words on every line
in the file rather than only inside a macro body being expanded.

**Coverage this exposed.** The only `exitm` test had no `IF` around it, so
`CountExitmIfDepth` always counted zero and the loop never ran — the function
was called but the part that does the work was untested. Two tests added, and
both confirmed to fail against a mutation that drops the upper-casing (the
realistic bug, since the table is upper-case) while the pre-existing test
still passed.

**Not solved by any of this:** nothing outstanding in `AssemblySession.cpp`
except the multiple-return sweep, which stays parked.

**8. `ParseStructMember` — DONE.** Filed above as wanting "its own small
type-size table", which turned out to be half right and half backwards. The
~80-line `if` was naming `DS/DSB/RMB`, `DB/BYT/BYTE/FCB`, `DW/WORD/FCW/FDB`,
`DD` — a **second copy of the spelling vocabulary**, one that a dialect adding
a synonym would never have reached, so struct members would silently have
stopped recognizing it. Only the *widths* are local knowledge. So the spellings
went to `DirectiveTable` and the table here holds four rows keyed by token,
with `kSizeFromOperand` marking the one that reads its count from the operand.
94 lines became a 15-line orchestrator plus `GetStructMemberSize` and
`RecordStructMember`.

`RMB` was the wrinkle: it is deliberately *not* in `s_kSpellings`, because
`rmb <count>` is storage while `rmb <bit>,<zp>` is the Rockwell instruction,
and a flat name->token table cannot say that. It now sits in its own
`s_kAmbiguousSpellings` beside the main one, reached by
`FromAmbiguousSpelling` (only the ambiguous forms, for a caller that already
missed in `FromSpelling`) and `FromStorageSpelling` (both, for a caller whose
context rules the instruction out — inside a `.STRUCT` body nothing is
ambiguous). That deletes the last `== "RMB"` literal from `Parser.cpp`.

Note the shape of the first draft: routing the parser through
`FromStorageSpelling` was correct but made every non-directive line rescan all
68 rows to re-derive a miss the caller already knew about. Splitting the
one-row ambiguous table out fixed it. A shared helper is not free just because
it is shared — check what the hot caller already knows.

Coverage was the usual story: only `ds`/`db`/`dw` were tested, so nine of the
twelve spellings had none. `StructMemberSpellingTests` now sweeps all of them,
plus case-insensitivity, an unknown type word, and — guarding the direction
this change could have broken — that `rmb 3,$20` outside a struct is still the
Rockwell instruction.

**Caveat on table-driving everything:** a table earns its place when the rows
are uniform. Where handlers need genuinely different arguments you get a
struct of mostly-null function pointers, which reads worse than the switch it
replaced. The directive rows are uniform; check each new one before assuming.

**How to verify a stage.** The unit suite (253 assembler tests plus Dormann,
which assembles *and* executes) is good but cannot see a reordering bug in an
untested corner. Every stage above was checked by assembling all 15 in-repo
`.a65` sources and comparing an FNV-1a-64 digest of the emitted bytes *and*
the diagnostic shape against a pre-change baseline. The harness is a
throwaway `TEST_CLASS` in `UnitTest/` — write it, capture the baseline from
the previous commit, apply the change, compare, delete it. It is deliberately
not kept: `casso-rocks` is already assembled and booted by `BootDiskTests`, so
a permanent version would add no coverage.

**A green digest is necessary, not sufficient**, and the failure mode is
specific: the 15 sources are all *valid* programs, so no digest run touches an
error-recovery path, and none of them spells `.DS` as `rmb <count>`. Both gaps
have now produced a real bug or a false confidence claim. Ask what the change
touches before trusting the digest, and where it is a path the corpus cannot
reach, add a unit test *and check that it fails against the old code* — the
first draft of `IndirectErrorRecoverySizingTests` passed on both versions and
covered nothing, which was only visible once the assertions were re-aimed at
the path that actually changed.

**A finding worth keeping from that change**, in two parts — the second one is
a mistake I shipped and had to back out.

*What the function is.* `EstimateInstructionSize` was named and commented as
though it sized forward references. It does not: its single caller is inside
the `else if (!info.hasError)` arm, *after* `RecordError`, so it runs only when
`ResolveAddressingMode` named a mode the opcode table does not carry. It is
error recovery — how far to skip an instruction that cannot be encoded so the
following labels stay close enough for the remaining diagnostics to be useful.
A forward reference the table *can* encode is sized from the `OpcodeEntry` the
caller already has. Renamed to `EstimateErrorRecoverySize`.

*Which table question to ask.* First attempt replaced `mnemonic == "JMP"` with
"does the mnemonic carry the CMOS mode this syntax suggests"
(`AbsoluteXIndirect` / `ZeroPageIndirect`), justified as agreeing with the mode
the resolver had just picked. That was wrong, and being table-driven did not
make it right. When nothing matches, `ResolveAddressingMode` returns a
*default* the mnemonic does not possess — `ZeroPageXIndirect` for
`JMP (foo,X)` on NMOS — so the estimate became consistent with a fiction and
advanced 2 bytes for an instruction with no 2-byte encoding on any 6502. The
crude string compare had been encoding a real fact all along: jumps are 3
bytes, other parenthesized forms are 2.

The correct question is `HasMode (mnemonic, JumpAbsolute)`, which is exactly
`{JMP, JSR}` on both instruction sets — table-driven, no literal, restores the
original widths, and fixes `JSR` besides, which the old compare sized at 2
despite JSR being 3 bytes. Pinned by `IndirectErrorRecoverySizingTests`.

**The transferable lesson:** replacing a literal with a table lookup is only an
improvement if the lookup asks the question the literal was answering. Ask what
fact the constant encoded before deciding what to look up — "derived from a
table" is not self-justifying.

### 2c. `CS0011` — calls in EHM conditions — DONE, and the rule got wider

**Now absolute: an EHM condition may not contain a call of any kind**, including
`.empty()` / `.size()` / `.good()`. 88 sites across 33 files converted; the gate
holds it at 0.

The earlier version of this entry (kept below, because the reasoning was sound
and the conclusion still wrong) gated only the `CHR` family and left `CBR`/`CWR`
to human judgment, on the grounds that a pattern cannot tell "does work" from
"asks a question". True — but the consequence is that the rule was **never
applied**. It sat in the standards text and drifted to 88 sites without anyone
noticing, which is a worse outcome than a rule that occasionally asks for a
pointless local.

Measuring it also corrected two of my own numbers. I first reported 51 sites
from a regex that only matched a call as the *first* token, missing `!AtEnd()`
and `a && b.Foo()` — a 42% undercount. And ~81 of the 88 turned out to be pure
state queries rather than the operation-hiding cases the rule was written for,
which is worth knowing before quoting effort.

Shape of the fix: hoist to a local **named for what is tested** (`isOpen`,
`hasBytes`, `rawSize`, `mergedRootType`), declared at function top per the
declarations rule, with the **comparison left inside the macro** — hoist the
value, not the predicate. A reused generic `ok` was rejected: a name that never
says what it holds defeats the purpose, which is that the bail point names the
condition.

**Not a line rule.** The condition ends at the first *top-level* comma, so the
check tracks paren depth and skips string/char literals — `CBRF (Peek() == ',',
SetError (...))` has a comma inside a literal that a line pattern gets wrong.
Implemented as `Test-EhmConditionCalls`, following the `Test-PchFirst`
precedent for non-regex checks. Verified by reintroducing a violation and
confirming the gate names the file, line, macro and call.

Exempt: `BCRYPT_SUCCESS` / `NT_SUCCESS`, which are function-like macros
expanding to a comparison (4 sites). `SUCCEEDED`/`FAILED` are deliberately *not*
on that list — inside a `CBR` they are wrong anyway, because testing an HRESULT
means the macro should be `CHR`. There are currently zero of those.

Action arguments stay exempt by construction; `CBRF`'s action is normally a call.

#### Superseded: the original half-gated entry

The gate covers the `CHR` family only, and is at zero: `CHR` tests an
`HRESULT`, so an argument that is a call always means "this did work and
returned a code", and the operation that failed is exactly what the macro
hides. All 28 instances (all in `AssemblySession.cpp`) are converted.

**The `CBR`/`CWR` half is deliberately not gated**, and that is a finding
rather than a shortcut. Attempting it flagged 52 sites, and inspection showed
the pattern cannot make the distinction that matters:

| Site | Verdict |
|---|---|
| `CBR (OpenClipboard (hwnd))` | hides an operation — should be hoisted |
| `CBR (EmptyClipboard())` | same |
| `CBR (!tok.empty())` | pure query, reads fine inline |
| `CBR (out.good())` | pure query |
| `CBRAEx (raw.size() == kFoo, E_INVALIDARG)` | query inside a comparison |

A regex cannot tell "does work" from "asks a question", and flagging all of
them produced far more noise than signal — which is exactly how a check gets
switched off. The rule stays in the standards text for review to apply.

Worth noting the gate caught this itself: adding the broad version blocked a
push on 15 sites in files this branch had merely *moved*, which is the
baseline problem any new rule has on a dirty tree, and the signal that the
rule was wrong rather than the code.

### 2d. Files that mention `HRESULT` but use no EHM at all — DONE (2 of 8 were real)

Worked through all eight. **Two were genuine and are fixed; the other six are
correct as they stand**, and the entry below that called the concern
"real" over-generalized from a three-file sample.

**The test is "calls a failable API", not "mentions `HRESULT`".** That is what
the standards actually say, and it is what the grep could not see.

*Fixed — `ThemeLoader.cpp`.* The real one: 549 lines, 31 returns, 17
`if (FAILED (...))` blocks. `ParseMetadata` was a validation chain that
hand-rolled EHM at every step, each ending
`return FAILED (hr) ? hr : E_INVALIDARG`. Now `CHRF` / `CBRFEx` with one exit
apiece. The two-ways-to-fail sites fold the "succeeded but unusable" case into
`hr` first, so the choice of code is stated once instead of at fourteen sites.
The three optional getters took the documented non-`HRESULT` shape — vestigial
`hr`, normal result returned at `Error:`.

*Fixed — `DxuiDwm.cpp`.* **Four** sites, not the one this entry named. The file
only surfaced in a `HRESULT` grep because one of the four had a comment saying
"ignore HRESULT"; the other three ignored results just as silently with a
`(void)` cast and no comment at all. All four now use `IGNORE_RETURN_VALUE`, so
the decision is greppable rather than merely readable.

**Correct as they stand (6).** A function that returns `HRESULT`, cannot fail,
and calls nothing failable is *participating* in EHM — it honors the contract
so callers can `CHR` it. That is the point of the pattern, not a violation of
it. `Cpu6502.cpp` (`Reset`/`Step` call nothing failable), `MemoryBus::Validate`
(documented hook, always `S_OK`), `DriveWidgetController::LoadDocument` (stub),
`CpuFactory::Create` (single exit already), `PathResolver::GetLocalAppDataDir`
(three fallback strategies — failure falls through to the next, so there is
nothing to bail to and EHM would be actively wrong), and
`StartupDownloadDialog::WorkerThreadProc` (`void`, on a worker thread, maps the
result into a status enum — handling, not propagating).

**This entry was wrong about `CpuFactory.cpp`**, which it said "hand-rolls
`hr = E_INVALIDARG; return hr;` where `CBRAEx` is the sanctioned form". Twice
wrong: `config.cpu` comes from user-editable machine JSON, so an unknown CPU is
a *user* error and `CBRAEx` asserts — which collides with the rule that assert
and notify are mutually exclusive. And `Cpu65C02Tests.cpp:342` deliberately
calls `CpuFactory::Create ("z80", ...)` expecting failure, unwrapped by
`ExpectedEhmAssert`, so adding the assert would break a passing test.

**The first pass at `ThemeLoader` stopped too early**, and the reasoning is
worth keeping because it is a tempting mistake. I converted the *error* paths
and left 26 optional reads written as
`if (SUCCEEDED (crtObj->GetNumber ("brightness", d)))`, on the grounds that
CS0011 governs EHM conditions and this is a plain `if`. True of the gate,
false of the rule: the standards say the ban applies to **all** macros, and
`SUCCEEDED (...)` is a macro with a call — an out-param call at that — inside
it. "The gate does not flag it" is not the same as "the rule allows it".

The fix was not 26 hoisted `hr` locals. Four presence helpers — `HasBool`,
`HasNumber`, `HasString`, `HasObject` — turn each site into an ordinary call
in an ordinary `if`, which no rule objects to, and `HasObject` folds in the
`&& ptr != nullptr` that all nine object sites repeated. They are file-scope
statics because `ThemeLoader` and `LoadedTheme` both use them, so no single
class owns them — the same call as the shared-test-helper exception above.

**Worth noting the gate cannot see this class of violation at all.** CS0011
scans EHM macros only. Extending it to `SUCCEEDED`/`FAILED` would be easy and
is probably right; it is not done, so for now this one is review-only.

**Coverage note.** The `ThemeLoader` tests asserted `FAILED (hr)` and the
structured `ThemeLoadResult`, but never the returned `HRESULT` — exactly the
logic the rewrite touched. Three tests added pinning `E_NOTIMPL` for a
too-new schema and `E_INVALIDARG` for missing/empty required fields, and all
three were confirmed to pass **against the pre-change code** as well, which is
what makes them evidence of equivalence rather than of the new shape.

#### Superseded: the original survey

Posited as "any .cpp not using EHM is non-compliant". Measured: 303 of 414
`.cpp` files use no EHM, but that number is not the finding — most have no
`HRESULT` anywhere (pure `void`/value code with no failure path), and 60 of
the 68 that do are unit tests, where `Assert::` is the correct idiom.

The sharp version of the rule is **a file that mentions `HRESULT` and uses no
EHM macro**, which is 8 production files:

| File | Lines |
|---|---:|
| `Casso/Ui/Dialogs/StartupDownloadDialog.cpp` | 710 |
| `Casso/Ui/ThemeLoader.cpp` | 549 |
| `CassoEmuCore/Core/MemoryBus.cpp` | 374 |
| `CassoEmuCore/Core/PathResolver.cpp` | 276 |
| `CassoCore/Cpu6502.cpp` | 267 |
| `Dxui/Theme/DxuiDwm.cpp` | 234 |
| `Casso/Ui/DriveWidgetController.cpp` | 161 |
| `CassoEmuCore/Core/CpuFactory.cpp` | 44 |

(`CassoCore/Ehm.cpp` also matches and is exempt — it implements the macros.)

Spot-checking three confirms the concern is real, and `DxuiDwm.cpp` shows why
it matters: its only `HRESULT` mention is the comment *"Best-effort: ignore
HRESULT"*. It ignores results **by prose rather than by
`IGNORE_RETURN_VALUE`**, so a deliberate decision is invisible to grep and
indistinguishable from an oversight. `CpuFactory.cpp` hand-rolls
`hr = E_INVALIDARG; return hr;` where `CBRAEx` is the sanctioned form.
`DriveWidgetController::LoadDocument` returns `HRESULT` with no EHM at all.

**Deliberately not scheduled.** The owner's call: the other rules will surface
these files as they are worked. Item 4 (multiple returns) covers the
`CpuFactory.cpp` shape directly -- `hr = E_INVALIDARG; return hr;` is a
multi-return -- and CS0006 catches hand-rolled control flow.

One gap to be aware of rather than to act on: nothing planned catches
`DxuiDwm.cpp`'s pattern, where a result is ignored by *comment* instead of by
`IGNORE_RETURN_VALUE`. There is no line to flag -- the tell is the absence of
a macro. If that ever matters, the check is file-level (matches `HRESULT`,
matches no EHM), which needs a second pass mode since every rule in
`CheckStyle.ps1` today is per-line. The list above is the starting set.

### 3. `CS0009` — producing `S_FALSE` — DONE

Every site asked the same question: what does the second outcome *mean*? Three
answers covered all 22, and they are the precedents for anything new:

- **"the thing isn't there."** Report `HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)`
  or `ERROR_NOT_FOUND`. Callers that treat absence as normal test for that exact
  code, which is what makes the change worth doing: a read or parse failure can
  no longer be silently mistaken for "nothing saved yet."
  (`DiskSettings::LoadMachineDefaultJson`, `GlobalUserPrefs::Load`,
  `PrintJobStore::Load`, `ThemeManager::Activate`)
- **"the user said no."** A `bool &` out-param, because a dismissed dialog is
  not a result code at all. (`RunStartupDownloader`'s `outUserExited`, threaded
  through `LoadMachineConfig` to `wWinMain` and to `DoMachineSelect`;
  `PromptBootDiskMru` already worked this way and set the pattern.)
- **"nothing needed doing."** A `bool &` out-param alongside the real output.
  (`MigrateUserConfig`'s `outChanged`, `MigrateLegacyFiles`' `outFoundLegacy`)

Three real defects fell out of the sweep, all of the same shape — a failure
flattened into "nothing to do": a discarded config parse error, `DiskSettings`
reporting parse failures as "nothing saved", and `ApplyAndPersistTheme`
persisting a theme name that never activated.

### 4. Multiple return statements — HRESULT half DONE, rest outstanding

**All 38 `HRESULT`-returning multi-return functions are converted; the scan is
at zero.** 611 of the tree's functions still have more than one `return`; the
573 non-`HRESULT` ones are unscheduled and need the decision below.

Measured with a brace-matching scanner over lightly-lexed source (string/char
literals and comments blanked with offsets preserved), not a line regex —
`CheckStyle.ps1` cannot do this per-line. Lambda bodies are attributed to the
lambda, not the enclosing function: a `return` inside a callback is not the
enclosing function's exit, and counting it produces false positives on every
file that passes a lambda.

**Two counts in the old entry were wrong, and the second one mattered.** It
said 535 functions / 45 `HRESULT`. The real numbers are 611 and 38 — but the
38 is only right *after* a fix: the first filter matched `\bHRESULT\b` on the
return type, which silently skips every COM interface method, because
`IFACEMETHODIMP` and `STDMETHODIMP` expand to `HRESULT STDMETHODCALLTYPE` and
never spell `HRESULT` in the source. That hid 4 functions including a 5-return
`MakeDocument`. Any future return-type filter needs the same widening.

Four shapes covered all 38:

| Shape | Conversion |
|---|---|
| `if (nothing to do) return S_OK;` | `BAIL_OUT_IF (cond, S_OK)` — 16 sites, the dominant one |
| `if (bad arg) return E_INVALIDARG;` | `CBREx (cond, E_INVALIDARG)` |
| `if (FAILED (hr)) { return hr; }` | `CHR (hr)` |
| cache hit → `return S_OK` | `if (*outX == nullptr) { create }`, or `BAIL_OUT_IF` when the create path is the function tail |

`BAIL_OUT_IF (cond, S_OK)` is the one worth knowing: it is documented in
`Ehm.h` as "early-out guard, NOT an error check", takes the opposite polarity
to `CBR`, and is exactly the "nothing to do" return that most of these were.

**A condition that calls anything still has to be hoisted first** (CS0011), so
`m_vertices.empty()`, `IsCreated()`, `file.good()` and `configPath.empty()`
each became a named local before the guard could read it.

**The trap in this conversion is C2362.** Replacing `return` with a bail turns
a straight-line exit into a `goto Error`, and a goto may not jump across the
initialization of a variable still in scope at the label. Three functions hit
it: `MakePage` and `MakeDocument` (a `std::lock_guard` between the guards and
the end) and `PrinterAudioSource::LoadSounds` (a lambda). Fixes, in order of
preference: put the guarded region in a nested block so the label sits outside
it, or — where the declaration has no side effect, as with a lambda — move the
guard *below* the declaration. Bailing out of a nested block still runs the
destructors, so the lock releases either way.

**Two behavior-preservation calls worth recording.**
`DxuiRenderTarget::EnsureComposeTarget` had a bad-argument `return E_FAIL` that
deliberately skipped its `Error:` cleanup, so converting it to `CBR` would have
started releasing the cached target on a zero-size call (routine when the
window is minimized). The cleanup is now gated on `recreated`, which is set
alongside the teardown and so already means "past the guards".
`WindowCommandManager::HrFromSpoolResult` is a pure mapper with a success
fast-path, so it just became an `if (ret <= 0)` around the chain — no EHM,
because there is nothing to bail from.

**Verification.** Debug 2813 / Release 2811, both `Test Run Successful`, counts
unchanged. `AssemblySession::EmitInstructionBytes` also got the byte-
differential treatment (throwaway `TEST_CLASS`, FNV-1a-64 over emitted bytes
plus diagnostic shape, all 15 in-repo `.a65` sources, baseline from `HEAD`
with only that file reverted): **identical on all 15**. Its change was
`if (ZeroPageRelative) { ...; return hr; }` followed by an unconditional block
→ `if / else`, which is equivalent by inspection and now confirmed.

**Still to decide: the 573 non-`HRESULT` cases.** Pure lookup functions are
clearer with multiple returns — `QwertyToDvorak` is a 256-entry mapping with 71
`return`s, and single-exit would mean assigning to a variable in every case.
(That one is really an array, not a switch.) The rule as written is about
manual flow control that EHM should own; a `switch` that maps input to output
is not that. Needs a decision on where the line falls before any of it is
converted.

### 5. `bool` return naming (tail)

A function may return `bool` only when its name makes `true`/`false` obvious:
`IsXxx`, `HasXxx`, `TryXxx`, `CanXxx`, and similar.

Survey of bool-returning declarations in headers:

| Prefix | Count |
|---|---|
| *(other)* | **309** |
| `Is` | 129 |
| `Has` | 18 |
| `Try` | 11 |
| `Should` | 4 |
| `Matches` / `Wants` / `Needs` | 6 |
| `Was` | 1 |

Cheaply checkable — a declaration-line regex, no body analysis. Two things to
settle first: the approved prefix list, and how many of the 309 are genuine
versus artifacts of a crude regex (operators, `operator bool`, lambdas).

**Applies now, not just at cleanup time:** anything converted to `bool` during
items 1-4 must be named accordingly.
