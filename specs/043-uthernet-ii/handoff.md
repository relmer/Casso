# 043 Uthernet II: handoff

The branch `043-uthernet-ii` is complete, tested and pushed. It merges into
master after 042, and only once 033 is ready, per the owner's plan: 042 and 043
stay off master until then, so master is free for a hotfix.

## State

- All tasks in [tasks.md](tasks.md) are done, including the on-screen check
  (T056) and the CHANGELOG and README entries (T057).
- ADTPro 2.1.0 over Ethernet (SC-006) and Telnet65 against a public BBS both
  pass; see [quickstart.md](quickstart.md).
- Last full gate, after merging master at `16f4e890e`: Debug 6644, Release
  6643, scenario 41, AddressSanitizer scenario 41, code analysis, tree style.

## Merge order and the second-merger work

042 (Super Serial Card) lands first; 043 lands right after it and does the
second-merger work. The 042 session sends a message when 042 is on master. Then:

1. Merge master, which then includes 042, into 043.
2. Raise the version of the four machine configs again (`Resources/Machines/`
   Apple2, Apple2Plus, Apple2e, Apple2eEnhanced). Match the stamps in
   `CassoEmuCore/EmbeddedMachineConfigs.h`, and leave 042's prior hashes in
   `CassoEmuCore/AssetBootstrap.cpp` beside 043's.
3. In `CassoEmuCore/Config/UserConfigStore.cpp`, preserve both save fixes: 042's
   switches and connector, and 043's network settings and moved cards. Run
   042's `Diff_And_Merge_Keep_A_Serial_Cards_Switches_And_Connector` and 043's
   `NetworkSettingsSurviveSaveAndLoad` and `MovedCardSurvivesSaveAndLoad`.
4. In `SettingsSheet::UpdateNetworkTabVisibility`, call
   `StartTabSparkle (m_networkPageIndex)` when the tab is added, as 042's
   `UpdateSerialTabVisibility` does for its tab.
5. Run the gate:
   - `scripts\RunTests.ps1 -Build` for Debug and for Release
   - `scripts\RunTests.ps1 -Build -Configuration Release -Scenario` (it
     fetches the network disks with `scripts\FetchNetworkDisks.ps1`)
   - `scripts\RunTests.ps1 -Build -Sanitize -Configuration Release -Scenario`
   - `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`
   - `git add -A`, then `scripts\CheckStyle.ps1 -Mode Tree`
6. Merge into master when the owner confirms 033 is ready.

## Menu id blocks

Branches allocate `IDM_*` ids in separate blocks: 040 from 40194, 043 from
40300 (`IDM_NETWORK_CONFIGURE`), 042 from 40400.

## Later

- A slot chooser for every optional card on the Machine tab, as its own spec
  after 042 and 043 are on master. A task suggestion for it exists in the
  desktop app.
- [relmer/Casso#177](https://github.com/relmer/Casso/issues/177): check
  per-cycle bus activity in the Harte CPU tests, which would have caught the
  store-read bug fixed on this branch.
