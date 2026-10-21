# Handoff: 042 Super Serial Card

2026-10-21. 042 is ready to merge into master. Everything is committed and
pushed on `042-super-serial-card`; master had nothing the branch lacked when
this was written.

## State

- All tasks in `tasks.md` are done. The firmware validation is in
  `validation.md`.
- The pre-merge gate passed after master was merged in (`007d8e396`): code
  analysis rebuild, Debug and Release x64 unit and scenario suites, ARM64
  Debug and Release builds, the AddressSanitizer scenario run, and the
  CheckStyle tree sweep. Since then only `CHANGELOG.md` and `README.md`
  changed.
- `CHANGELOG.md` and `README.md` have the owner's approved text.
- One validation item is open by design: ADTPro with its host. It arrives with
  033's serial attachments (below) and does not block the merge.

## Shipping plan (the owner's)

042 and 043 merge into master, then 033. They stay off master until 033 is
ready, so master is free for a hotfix in the meantime.

1. If master has moved, merge it into `042-super-serial-card` and run the
   pre-merge gate again (`.github/copilot-instructions.md`, including the
   AddressSanitizer scenario run).
2. Merge 042 into master.
3. Tell the 043 session that 042 is on master.
4. 043 merges 042 in, does the second-merge work below, and lands.
5. 033 lands after both.

## What 043 does as the second merge

Agreed with the 043 session:

- Bump the machine config versions again and add 042's prior hashes to
  `AssetBootstrap`.
- Keep both `UserConfigStore` save fixes: 042's switches, and 043's slot and
  network settings.
- Call `StartTabSparkle (m_networkPageIndex)` in
  `UpdateNetworkTabVisibility` when the Network tab is added, as 042 does for
  the Serial card tab.

043 reported its own full gate passing; it was reminded that master now
requires the AddressSanitizer scenario run.

## What 033 builds on 042

033's FR-095, its Phase 23 after 042 and 043 merge: TCP and COM-port
attachments for an emulated serial port, the Super Serial Card's or the
//c's. The 033 session agreed to check before changing 042's files.

- An attachment implements `IAciaEndpoint`, as the loopback and file
  attachments in `CassoEmuCore/Devices/AciaEndpoints.*` do.
- For TCP, assert CTS, DSR and DCD while a client is connected.
- Bytes from a socket or COM port arrive on a host thread; pass them to the
  emulation thread rather than calling `EnqueueReceivedBytes` or
  `SetModemInputs` from the host thread.
- The Serial card page takes its connector choices from
  `SerialCardPage::kAttachmentDevices`. The //c has no Serial card page; its
  ports have loopback plugs attached in `MachineBuilder`, so the //c
  requires a separate place to choose.
- FR-097: the port is open only while the link is started.

With the TCP attachment, the ADTPro client on an emulated //e can connect to
Casso Explorer's link, which closes the open validation item.

## Not in the repo

The scratch tools used for validation (a UI driver, a headless scenario
harness, rendered manual pages, captures) were on the original machine only.
Their results are in `validation.md`.
