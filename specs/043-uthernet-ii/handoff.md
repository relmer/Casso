# 043 Uthernet II: handoff

The branch `043-uthernet-ii` is complete and pushed, with master merged in
after 042. It lands on master once the owner confirms 033 is ready, per the
owner's plan: 043 stays off master until then, so master is free for a hotfix.

## State

- All tasks in [tasks.md](tasks.md) are done, including the on-screen check
  (T056) and the CHANGELOG and README entries (T057).
- ADTPro 2.1.0 over Ethernet (SC-006) and Telnet65 against a public BBS both
  pass; see [quickstart.md](quickstart.md).
- 042 (Super Serial Card) is on master at `8543234b7`, and master is merged
  into this branch at `360fb54bc`. That merge did all of the second-merger
  work this handoff used to list; the commit message covers it. In short: both
  cards in the slot list (2 and 3), a second version raise on the four machine
  configs with the stamps to match, four more prior hashes in `AssetBootstrap`
  for the configs as 042 shipped them, both `UserConfigStore` save fixes,
  `StartTabSparkle` on the Network tab, and `TestMachine` carrying both
  constructor sets.

## What the gate still owes

The merge was validated with the Debug suite (6741 tests), the Release suite
(6740) and `scripts\CheckStyle.ps1 -Mode Tree` over 1976 files. Three gates
were skipped at the owner's request and must pass before this branch lands on
master:

- `scripts\RunTests.ps1 -Build -Configuration Release -Scenario` (it fetches
  the network disks with `scripts\FetchNetworkDisks.ps1`)
- `scripts\RunTests.ps1 -Build -Sanitize -Configuration Release -Scenario`
- `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`

Then `git add -A` and `scripts\CheckStyle.ps1 -Mode Tree` again, since the
sweep is the only thing that catches an orphaned `////` banner.

**`prodos-users.dsk` could not be fetched from `www.apple.asimov.net` on
2026-10-21.** Eight attempts, every one "unexpected EOF or 0 bytes from the
transport stream", while `dos33-master.dsk` and `kermit-65.dsk` came down
fine. The copy under `Disks/Apple/` was taken from
`mirrors.apple2.org.za/ftp.apple.asimov.net`, which is the host the Kermit
entry in `scripts/FetchStockDisks.ps1` already reads, and verified against
that script's pinned SHA1. A fresh checkout that cannot fetch it will not run
the scenario suite at all.

## Menu id blocks

Branches allocate `IDM_*` ids in separate blocks: 040 from 40194, 043 from
40300 (`IDM_NETWORK_CONFIGURE`), 042 from 40400.

## Later

- A slot chooser for every optional card on the Machine tab, as its own spec
  after 043 is on master. A task suggestion for it exists in the desktop app.
- [relmer/Casso#177](https://github.com/relmer/Casso/issues/177): check
  per-cycle bus activity in the Harte CPU tests, which would have caught the
  store-read bug fixed on this branch.
