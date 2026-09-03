# Task: Casso.exe silently ignores unrecognized command-line arguments

Start a fresh session on relmer-desktop. Fetch first and branch from current
`origin/master` (it moved today; do not trust a local master, and do not trust
any remembered SHA — as of this writing origin/master is `6fb4a833`).

Read `CLAUDE.md` and `.github/copilot-instructions.md` first for conventions
(EHM macros, single exit, style gate, test expectations).

## The defect

`CommandLineParser::ParseEmulator` in `CassoCore/CommandLineParser.cpp` matches
only known flags; anything unmatched falls through the if/else chain and is
dropped. This is deliberate and documented in `CassoCore/CommandLineOptions.h`
(~line 300):

> "An argument the table does not know is SKIPPED rather than refused: Casso.exe
> is a GUI program that Windows may launch with a shell-supplied argument, and
> refusing to start over one nobody asked about is worse than skipping it."

The reasoning is sound but over-applied: a stray shell argument and a user typo
are treated identically.

## Observed consequences

1. `Casso.exe --machine Apple2e path\to\disk.dsk` silently ignores the path and
   boots whatever disk was mounted last. This cost real debugging time: audio
   captured from what appeared to be a speech-demo disk had no speech in it, and
   the near-conclusion was that Mockingboard speech had regressed on master. A
   screenshot showed a ProDOS boot screen from a stale mount. The positional form
   worked before 1.20's table-driven command-line rewrite, so this is also a
   silent backward-compatibility break.
2. A typo'd or misspaced flag (`--dsik1 foo.dsk`, `--disk 1 foo.dsk`) does
   nothing, with no feedback.
3. `Casso.exe --help` launches the emulator instead of showing usage.
   `CassoCore/CommandLineHelp.cpp` already generates help text with the reader's
   own flag prefix; the GUI never displays it.

## Suggested direction (evaluate, do not assume)

- Distinguish flag-shaped arguments (leading `-` or `/`) from bare positional
  paths. An unknown flag is a mistake worth reporting; a bare path is not.
- Consider treating a lone positional image path as `--disk1`. That restores
  pre-1.20 behavior AND is what Windows passes for file associations and
  drag-onto-exe, so it likely makes double-clicking a `.dsk` work. Verify whether
  file-association launch is a supported scenario before committing to it.
- Unknown flags: themed usage dialog (Main.cpp already owns startup dialogs),
  then exit without starting the emulator.
- `--help`, `-h`, `-?`, `/?` should show that dialog and exit successfully.
- Preserve the original concern: find what Windows actually passes GUI apps
  unbidden (e.g. `-Embedding` for COM/DDE activation) and keep tolerating those
  specific cases rather than reverting to blanket tolerance.
- `ParseEmulator` returns `EmulatorOptions` with no error channel, so add one
  (unknown-args list or status field) that `Casso/Main.cpp:ParseCommandLine`
  acts on.

## Two carries from the relmer-desktop CRT session (it declined the task but
## reviewed the brief)

**Main.cpp is more crowded than the above suggests.** Around lines 236-253 there
is a boot-disk pre-flight that constructs a `UserConfigStore`, reads the
remembered disk path, and clears it when the file no longer exists. If you adopt
positional-path-means-disk1, that pre-flight is the code deciding whether a
remembered disk is used at all, so the interaction is real. Read it before
changing argument handling.

**There is a data-loss bug in that exact path — coordinate, do not land on top
of it.** `UserConfigStore::BuildCombinedJson` merges only the `machines` section
from the existing file (:1300-1331) and emits `global` from its `prefs`
PARAMETER at :1339 (`root.emplace_back (kpszGlobalKey, prefs.ToJson())`). What
`m_prefs` decides is which object gets passed: `SaveDelta` (:1204-1213) calls
`SaveCombinedJson (*m_prefs, fs)` when `m_prefs` is non-null, and
`SaveCombinedJson (fallbackPrefs, fs)` when it is null, where `fallbackPrefs` is
a **default-constructed `GlobalUserPrefs`** declared at :1197. So the failure is
not "stale prefs" — it is a default-constructed struct substituted for the real
prefs and then serialized over them. The same `fallbackPrefs` pattern appears
three times: `SaveDelta` (:1197), `Reset` (:1234), and `Load`'s migrate branch
(:1165).

`m_prefs` is assigned only at :957 in `LoadAll`, whose sole caller is
`EmulatorShell.cpp:1135`. The store constructed at `Main.cpp:239` never calls
it. So when the remembered disk is missing, `Main.cpp:249` ->
`DiskSettings::WriteSavedDiskPath` -> `SaveDelta` (DiskSettings.cpp:340) -> the
null branch -> every global preference is overwritten with defaults. Deleting or
moving the disk image you last used is enough to trigger it.

Caveats from the session that traced it: this rests on reading the code, not on
an observed wipe — nobody has run it. And two other un-loaded stores
(`Main.cpp:324`, `AssetBootstrap.cpp:1671`) were confirmed to be constructed
without `LoadAll` but were NOT checked for whether they reach a save on any
path. Only the Main.cpp:239 path was traced end to end.

If your work makes that pre-flight run more often, or on paths it did not run on
before, you make an existing wipe easier to hit.

## Testing

Keep the logic in CassoCore where it is testable — the code comment notes the
old hand-rolled loop in Main.cpp "could not be reached by a test." Cover:
unknown flag reported, positional path maps to disk1 (if adopted), help flags
recognized, known shell-supplied args still tolerated, and every existing valid
invocation unchanged.

Run `scripts/RunTests.ps1 -Build` and `scripts/CheckStyle.ps1`. Verify by
launching the built exe with a bad flag and with a positional disk path,
confirming what actually boots rather than trusting the parse result.
