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
| `CS0011` | no call as a `CHR` argument | 0 |
| `CS0012` | `Ehm.h` comes from `Pch.h`, never directly | 0 |

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
   (`IsBitOpMnemonic` naming Rockwell ops, the `JMP`/`JSR` size cases). Paying
   it in string literals instead of enum values is strictly worse: a
   misspelled literal silently never matches, a misspelled enum will not
   compile. The `Directive` enum has the same shape and nobody minds — it
   lists all 27, and a dialect's spelling table populates a subset.

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

**Not solved by this**, so do not expect them to fall out: `ParseStructMember`
(101 lines, ~80-line `if`) is a field-declaration parser wanting its own small
type-size table; `ResolveAddressingMode` (140 lines, 21 returns) is a
*matrix* — syntax x mnemonic class -> mode — so it wants a 2-D lookup, a
different table than the directive one; `ExpandMacro`'s scope is a
decomposition problem.

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

### 2c. `CS0011` — calls as EHM macro arguments — gated at 0, half by judgment

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

### 2d. Files that mention `HRESULT` but use no EHM at all (8) — NOT SCHEDULED

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

### 4. Multiple return statements — SCHEDULED LAST (largest scope)

Not yet gated. **535 of 2,466 functions** have more than one `return`; **45 of
those return `HRESULT`** and so violate the written rule outright ("Functions
returning `HRESULT` MUST have exactly one exit point").

Gate the 45 first — unambiguous. The other 490 need a decision, because pure
lookup functions are clearer with multiple returns: `QwertyToDvorak` is a
256-entry mapping with 71 `return`s, and forcing single-exit would mean
assigning to a variable in every case. (That one is really an array, not a
switch.)

Needs function-level analysis — brace matching over lexed source, not a line
regex. `scripts/CheckStyle.ps1` is line-based today.

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
