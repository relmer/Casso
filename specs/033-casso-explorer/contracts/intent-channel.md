# Contract: Intent channel additions and process launch

**Feature**: 033-cassque | **Date**: 2026-09-10

Extends `CassoEmuCore/Seams/Win32IntentChannel` and adds a process seam.

## Wire format

Transport stays `WM_COPYDATA` via `SendMessageTimeoutW`, 2000 ms timeout,
4096-byte cap, sender HWND in `wParam`.

| dwData | Byte 0 | Rest | Sent by |
|---|---|---|---|
| `CassoDiskImageIntent` | `ReloadInPlace` or `Restart` | UTF-8 absolute path | Casso Explorer, CLI |
| `CassoDiskImageIntent` | `InsertDisk` | drive byte 1 or 2, then UTF-8 absolute path | Casso Explorer |
| `CassoDiskImageIntent` | `DescribeMachine` | none | Casso Explorer |
| `CassoIntentReply` | `MachineDescription` | drive count byte, UTF-8 display name | Casso |
| `CassoIntentReply` | `InsertRefused` | UTF-8 reason | Casso |
| `CassoIntentReply` | `InsertDone` | none | Casso |
| `CassoIntentReply` | `ReloadDone` | none | Casso |
| `CassoIntentReply` | `ReloadConflict` | UTF-8 description of the two kept versions | Casso |
| `CassoIntentReply` | `ReloadRefused` | UTF-8 reason | Casso |

Reload replies are sent only when the intent arrived with a sender window,
so the CLI's fire-and-forget behavior is unchanged.

`ExternalChangeIntent` gains `InsertDisk` and `DescribeMachine`. Decode
refuses an unknown intent byte, as today, so an older Casso ignores the new
ones and Casso Explorer treats a missing reply within the timeout as "no
description" and falls back to the default machine's layout.

## Emulator side

```cpp
//  EmulatorShell::OnCopyData in Shell/Window/EmulatorWindow.cpp, extended
case InsertDisk:       m_machine.GetDiskStore().RequestInsert (drive, path, replyTo);   // eject flushes first
case DescribeMachine:  ReplyMachineDescription (replyTo);
case ReloadInPlace:    NoteExternalChange (path, intent, replyTo);                     // reply when replyTo is set
```

Rules: insert goes through the same path as the drive widget, so eject
flushes a dirty image before the swap and a failed flush or a mid-write
drive yields `InsertRefused` with the store's reason text. A reload
answers `ReloadDone`, `ReloadConflict` or `ReloadRefused` from the
external-change policy's decision. Replies are posted to `replyTo` from
the UI thread after the store answers. Every mount that arrives by insert
or by an OLE drop from any source calls `KnownFolderStore::Append`, so
Casso is the one writer for hand-offs.

## Casso Explorer side

```cpp
class CassoTargeting
{
public:
    static CassoTarget  Choose (HWND owner, bool ownerAlive, std::span<const HWND> running, const MachineConfig & defaultMachine);
};
```

Rules: owner wins when `ownerAlive`, which the caller supplies from
`IsWindow`; else the most recently active `CassoWindow`; else Launch with
`driveCount` from `AttachedDiskIiDriveCount` of the default machine. Pure
and tested with fake window lists.

## Window classes and command line

- Casso: class `CassoWindow` (existing). Launches Casso Explorer with
  `--owner <decimal hwnd>`.
- Casso Explorer: class `CassoExplorerWindow`. On launch with `--owner`, enumerates
  `CassoExplorerWindow` for a window whose stored owner equals the argument and,
  if found, fronts it and exits. Standalone launches never dedupe.
- Casso Explorer accepts `--title <prefix>` with Casso's semantics.

## Process seam

```cpp
struct IProcessLauncher
{
    virtual HRESULT Launch (const std::wstring & exePath, const std::wstring & arguments) = 0;
};
```

`Win32ProcessLauncher` beside it; `FakeProcessLauncher` in tests records
calls. Casso launches `CassoExplorer.exe` from its own module directory; Casso Explorer
launches `Casso.exe` the same way with `--disk1 "<path>"`. Paths are always
quoted.
