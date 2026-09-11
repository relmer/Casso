# Contract: Intent channel additions and process launch

**Feature**: 033-cassque | **Date**: 2026-09-10

Extends `CassoEmuCore/Seams/Win32IntentChannel` and adds a process seam.

## Wire format

Transport stays `WM_COPYDATA` via `SendMessageTimeoutW`, 2000 ms timeout,
4096-byte cap, sender HWND in `wParam`.

| dwData | Byte 0 | Rest | Sent by |
|---|---|---|---|
| `CassoDiskImageIntent` | `ReloadInPlace` or `Restart` | UTF-8 absolute path | Cassque, CLI |
| `CassoDiskImageIntent` | `InsertDisk` | drive byte 1 or 2, then UTF-8 absolute path | Cassque |
| `CassoDiskImageIntent` | `DescribeMachine` | none | Cassque |
| `CassoIntentReply` | `MachineDescription` | drive count byte, UTF-8 display name | Casso |
| `CassoIntentReply` | `InsertRefused` | UTF-8 reason | Casso |
| `CassoIntentReply` | `InsertDone` | none | Casso |

`ExternalChangeIntent` gains `InsertDisk` and `DescribeMachine`. Decode
refuses an unknown intent byte, as today, so an older Casso ignores the new
ones and Cassque treats a missing reply within the timeout as "no
description" and falls back to the default machine's layout.

## Emulator side

```cpp
//  EmulatorShell::OnCopyData, extended
case InsertDisk:       m_machine.GetDiskStore().RequestInsert (drive, path, replyTo);   // eject flushes first
case DescribeMachine:  ReplyMachineDescription (replyTo);
```

Rules: insert goes through the same path as the drive widget, so eject
flushes a dirty image before the swap and a failed flush or a mid-write
drive yields `InsertRefused` with the store's reason text. The reply is
posted to `replyTo` from the UI thread after the store answers.

## Cassque side

```cpp
class CassoTargeting
{
public:
    static CassoTarget  Choose (HWND owner, std::span<const HWND> running, const MachineConfig & defaultMachine);
};
```

Rules: owner alive wins; else the most recently active `CassoWindow`;
else Launch with `driveCount` from `AttachedDiskIiDriveCount` of the
default machine. Pure and tested with fake window lists.

## Window classes and command line

- Casso: class `CassoWindow` (existing). Launches Cassque with
  `--owner <decimal hwnd>`.
- Cassque: class `CassqueWindow`. On launch with `--owner`, enumerates
  `CassqueWindow` for a window whose stored owner equals the argument and,
  if found, fronts it and exits. Standalone launches never dedupe.
- Cassque accepts `--title <prefix>` with Casso's semantics.

## Process seam

```cpp
struct IProcessLauncher
{
    virtual HRESULT Launch (const std::wstring & exePath, const std::wstring & arguments) = 0;
};
```

`Win32ProcessLauncher` beside it; `FakeProcessLauncher` in tests records
calls. Casso launches `Cassque.exe` from its own module directory; Cassque
launches `Casso.exe` the same way with `--disk1 "<path>"`. Paths are always
quoted.
