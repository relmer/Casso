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
| `CS0002` | no anonymous namespaces | **32** |
| `CS0006` | no bare `goto Error` | **46** |

## Queue

### 1. `CS0002` — anonymous namespaces (32)

All test files are done. What remains is production code, where moving a
helper onto its implementing class means editing the header.

Rules settled: constants split by usage (single-use → function-local
`constexpr`, multi-use → private `static constexpr` member); helpers → class
statics; types → nested private types.

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

### 2. `CS0006` — bare `goto Error` (46)

The mechanical shapes are converted. What remains needs restructuring:

- **29 in `AssemblySession::ProcessPass1Line`.** These are early *exits*
  ("handled this line, record it and leave"), not error checks, and each runs
  `m_lineInfos.push_back (info)` on the way out. `BAIL_OUT_IF` has no action
  slot, so a mechanical conversion would read worse than the `if` it replaces.
  The function wants a state machine or switch first — the EHM question mostly
  dissolves once it does. Well covered for a rewrite: 253 assembler unit tests,
  15 in-repo `.a65` sources, plus Dormann (which assembles *and* executes).
- **~19 others** are `else`-branch or action-then-exit shapes, each needing a
  local decision.

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

### 4. Multiple return statements

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
