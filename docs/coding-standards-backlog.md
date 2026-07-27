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
| `CS0002` | no anonymous namespaces | **53** |
| `CS0006` | no bare `goto Error` | **48** |
| `CS0009` | do not *produce* `S_FALSE` | **22** |

## Queue

### 1. `CS0002` — anonymous namespaces (53)

All test files are done. What remains is production code, where moving a
helper onto its implementing class means editing the header.

Rules settled: constants split by usage (single-use → function-local
`constexpr`, multi-use → private `static constexpr` member); helpers → class
statics; types → nested private types.

Heaviest: `UserConfigStore.cpp` (621-line block), `ThemePage.cpp` (395),
`AssetBootstrap.cpp` (209).

### 2. `CS0006` — bare `goto Error` (48)

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

### 3. `CS0009` — producing `S_FALSE` (22)

Each site needs the same question answered: what does the second outcome
*mean*? Then model it explicitly.

Precedents set:
- print path → `PrintOutcome { Delivered, Canceled }` out-param
- `TryExtractFirstHDropPath` → plain `bool`, because it had exactly two
  outcomes and the name was made to say so
- `PromptForDiskImage` → `S_OK`, because nothing ever tested the `S_FALSE`

Largest cluster: `DiskSettings.cpp` (10). `MigrateLegacyFiles`' "nothing to
migrate" wants a `LegacyMigration { Migrated, NothingToMigrate }` enum, and six
`MachineConfigUpgradeTests` assertions pin `S_FALSE` as contract, so that one
changes tests too.

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
