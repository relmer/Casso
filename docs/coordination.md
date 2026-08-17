# Coordination — concurrent spec sessions

State that belongs to no single spec, and would otherwise live only in one
session's conversation. Written down because it survives neither compaction nor
a move to another machine.

Keep this current or delete it. A stale coordination note is worse than none —
it is read by exactly the people who have no other way to check it.

## Active work

| Spec | Branch | Focus |
|---|---|---|
| 019 | `019-assembler-dialects` | Dialect mechanism + Merlin. Seeded by GH #92. |
| 020 | `020-disk-file-access` | Disk file read/write, `disk` subcommand, boot config. |

Both branch from `master` and integrate through it. **Nothing flows directly
between the two branches** — no cherry-picks, no cross-branch merges. Shared
artifacts are landed on `master` and picked up by a normal merge. That is why
the command-line parser was moved into `CassoCore` before either started.

## The conflict surface is three files, and only one is source

Measured, not assumed — re-measured after 020 landed its CLI edge, which added
the third. Across their whole diffs the branches overlap on:

    CassoCore/CassoCore.vcxproj
    UnitTest/UnitTest.vcxproj
    CassoCli/CommandLine.cpp

The two project files are additive — each session adds its own `<ClCompile>` /
`<ClInclude>` rows.

**Resolve by keeping both sides. Never take one side.** Accepting one drops the
other session's files from the build, and the failure is quiet: the `.cpp` still
sits on disk so nothing looks missing, the suite still compiles, and it still
passes — with fewer tests in it. Both sessions report exact test counts (020's
is at the top of its `tasks.md`), so the check after merging is that the count
matches the sum of both sides, not merely that the run is green.

`CassoCli/CommandLine.cpp` **merges cleanly as things stand**, because the two
sides are in different regions of it: 019 replaced the diagnostic-printing
loops with `DiagnosticFormatter`, 020 added lines to `PrintUsage`. The
one-line collision predicted below arrives only when 019 registers `as65` in
that same usage text. Same keep-both rule when it does.

Nothing else is shared. 019 touches none of `CommandLineOptions.h`,
`CommandLineParser.h/.cpp`, `UnitTest/CommandLineTests.cpp` or
`CassoCli/CassoCli.vcxproj`, all of which 020 has edited — which is the
measurement behind the sequencing rule in the next section.

## Sequencing: the `as65` fallback removal (019 T049)

**019 holds T049 until 020's command-line work reaches `master`.**

Two reasons, in order of weight:

1. 020 has ~384 lines in flight across `CommandLineParser.cpp`,
   `CommandLineParser.h`, `CommandLineOptions.h` and `CommandLineTests.cpp`.
   019 has touched none of them. Whoever holds unmerged work in a file should
   not be the one made to resolve around someone else's edit.
2. 020 is adding `disk` to `s_kSubcommands`, and that table is what decides
   which bare words reach the fallback. Removing the fallback first means
   writing tests against a table that is about to change shape.

When it does happen it is **one commit**, not three. `s_kSubcommands` currently
holds only `{ "run", … }`; the `As65` enum value is reachable *only* through the
fallback. So any commit that removes the fallback without adding an explicit
`as65` row in the same change leaves AS65 unreachable and breaks the tree at
that point in history.

`UnitTest/CommandLineTests.cpp` contains `BareWordThatIsNotASubcommand_StaysAs65`,
placed by 020 as a deliberate tripwire so that removing the fallback has to be a
decision rather than an accident. Deleting it is the intended outcome. Say so in
the commit message, or it reads later as someone removing an inconvenient test.

Both sessions also touch `PrintUsage` by one line each — 020 registering `disk`,
019 registering `as65`. Same keep-both rule.

## Extending the fixtures

`UnitTest/Fixtures/Merlin/` and `UnitTest/Fixtures/Disks/` live on `master` and
are consumed by both sessions, so adding to either is a shared-surface change.

- Pull `master` first. Both sessions have been behind at least once.
- Re-derive rather than hand-place: `scripts/ExtractMerlinFixtures.ps1` for the
  Merlin corpus, `scripts/FetchMerlin.ps1` for the disks. Both refuse to run
  against an image whose SHA-256 does not match the pin.
- Update the directory's own `README.md` inventory **and** the matrix row in
  `UnitTest/Fixtures/README.md`.
- Land it on `master`, not on a feature branch.

Both directories are CC BY-NC-ND 3.0 and carry a `LICENSE` covering the whole
directory. Per constitution 1.9.0 a sidecar `LICENSE` per directory is the
entire obligation for a fixture — no per-file accounting, and adding one is
never an amendment. Material whose license forbids modification must be
read-only to its tests.

## Two machines

Sessions on this project may run on different physical machines. **Only git
crosses.** Not the working tree, not `%LOCALAPPDATA%`, not Claude Code memory
files, and not per-clone git config such as `core.hooksPath` or
`.git/info/exclude` — which is why `/DevDisks/` is ignored in `.gitignore`
rather than locally.

Practical consequence: before reporting that you are blocked on something from
`master`, pull. A file described as "landed" is only landed once it is pushed,
and a session on the other machine cannot see your working tree at all.

## Where knowledge goes

Prefer an existing artifact over a new document; a second description of the
same thing is a second thing to go stale.

| Kind | Home |
|---|---|
| Operational how-to (driving Merlin, manual procedures) | that spec's `quickstart.md` |
| Corpus and on-disk format findings | the fixture `README.md` on `master` |
| Decisions and their rationale | `plan.md`, `contracts/` |
| Settled and open questions | `tasks.md` |
| Rules every session needs | `.github/copilot-instructions.md` |
| Cross-session state | this file |

Claude Code memory is the least portable form of any of these — it is keyed per
directory and per machine, so a worktree gets its own and none of it survives a
move. Anything worth keeping belongs in the repo as well.
