# Implementation Plan: Serial Connectivity

**Branch**: `047-serial-connectivity` | **Date**: 2026-10-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/047-serial-connectivity/spec.md`

## Summary

Connect each 6551 (the Super Serial Card's and the //c's two) to a host COM
port, to TCP (out or listening, with an optional Telnet layer), or to a
Hayes-compatible modem that dials over TCP. Every attachment is polled on the
emulation thread through `Acia6551::Tick`, as the Uthernet II pumps its
sockets, and everything it brings in (bytes with their error flags, line
levels, connections opening and closing) is journaled where the chip takes it.
Behind live the attachments are not polled and send nothing; a resume from the
past closes the connection and drops DCD, as `UthernetCard` does. Settings
gain the three attachments and their options for the Super Serial Card and a
new page for the //c's ports. Design decisions are in [research.md](research.md).

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145

**Primary Dependencies**: Windows SDK (Winsock 2 through the existing
`IHostNetwork` seam, the Win32 communications API through a new
`IHostSerialPorts` seam); no third-party code

**Storage**: the machine JSON's port entries (contracts/modem-and-config.md)

**Testing**: Microsoft C++ Unit Test Framework. `FakeHostNetwork` (extended
with host name lookup) and a new `FakeHostSerialPorts` replace the PC's
network stack and serial ports. The scenario suite is run as it is.

**Target Platform**: Windows 10/11, x64 and ARM64 (`GetCommPorts` is
available from Windows 10 1803)

**Project Type**: desktop application, core static library plus a thin exe

**Performance Goals**: an idle attachment costs about 125 non-blocking host
calls a second (research R1); no per-instruction cost beyond a subtract and a
compare

**Constraints**: replays bit-exact against live (SC-005); nothing is sent to the
PC in a replay; no blocking call on the emulation thread (lookups run on a
worker)

**Scale/Scope**: three attachments, a modem command interpreter, a Telnet
codec, two host seams, settings for four port positions

## Constitution Check

| Principle | Status |
|---|---|
| I. Code Quality | New code follows EHM, alignment, banner and spacing rules; CheckStyle tree sweep before handoff |
| II. Testing Discipline | Every attachment, the modem, the codec, the journal entries, the settings model and the page are driven by unit tests through fakes; no real socket or port |
| III. UX Consistency | Sentence-case labels; errors in the two-line form; infobars as the Super Serial Card already uses |
| IV. Performance | Polled at an interval, never per instruction; idle cost measured by a test that counts host calls |
| V. Simplicity | One polling model for every attachment; the modem reuses the TCP link |
| VI. Thin Executable | All new code in `CassoEmuCore`; the Win32 seams sit in `CassoEmuCore/Seams` beside `Win32HostSocket` |

No violations, before or after Phase 1.

## Project Structure

### Documentation (this feature)

```text
specs/047-serial-connectivity/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── attachment-interface.md
│   ├── host-seams.md
│   └── modem-and-config.md
└── tasks.md
```

### Source Code

```text
CassoEmuCore/
├── Devices/
│   ├── Acia6551.{h,cpp}             receive errors, host-input methods, polling, state v3
│   ├── IAciaEndpoint.h              OnFormatChanged, IsHostLink, Poll, DropConnection
│   ├── SerialByteQueue.{h,cpp}      entries with an error byte
│   └── Serial/
│       ├── SerialLineFormat.{h,cpp}     the guest's format, from the registers
│       ├── SerialLinkReports.{h,cpp}    problems for the infobar
│       ├── TelnetCodec.{h,cpp}
│       ├── TcpSerialLink.{h,cpp}        connect / listen / send / receive over IHostNetwork
│       ├── HayesModem.{h,cpp}           the command interpreter, no I/O
│       ├── AciaComPortEndpoint.{h,cpp}
│       ├── AciaTcpEndpoint.{h,cpp}
│       └── AciaModemEndpoint.{h,cpp}
├── Seams/
│   ├── IHostSerialPort.h, Win32HostSerialPorts.{h,cpp}
│   ├── IHostNetwork.h               LookUpHost
│   └── Win32HostNetwork.cpp, Win32HostNameLookup.{h,cpp}
├── Debugger/Reverse/InputJournal.h  AciaLines, AciaConnection
├── Debugger/IoEventLog.{h,cpp}      SerialConnection, error text
├── Core/MachineConfig.{h,cpp}       attachment options
├── Shell/MachineBuilder.{h,cpp}     one attachment factory for every 6551
├── Shell/MachineHost.{h,cpp}        DropHostConnections, ApplyDeviceInput, reports
├── Shell/Components/ShellSerialCard.{h,cpp}  the link infobar
└── Ui/Settings/
    ├── SerialAttachmentView.{h,cpp} drop-down and options, shared
    ├── SerialCardPage.{h,cpp}
    ├── SerialPortsPage.{h,cpp}      the //c's two ports
    └── SettingsPanelState.{h,cpp}   staging, RequiresReset

UnitTest/
├── NetworkTests/FakeHostNetwork.*   name lookup
├── SerialTests/                     FakeHostSerialPorts, codec, modem, link, endpoints, replay
└── UiTests/                          page and staging tests
```

**Structure Decision**: new attachment code gathers under
`CassoEmuCore/Devices/Serial/`, since it is several classes with one purpose;
the 6551 and the existing endpoints stay where they are.

## Phases

1. **Foundation**: the error byte through the receive path, the host-input
   methods and their journal entries, polling, the replay rules, state v3,
   the serial log entries, `SerialLineFormat`, `SerialLinkReports`, the
   attachment config and the builder's single factory. Everything after
   depends on this.
2. **TCP (US3)**: name lookup, `TelnetCodec`, `TcpSerialLink`,
   `AciaTcpEndpoint`.
3. **Modem (US1)**: `HayesModem`, `AciaModemEndpoint` over `TcpSerialLink`.
4. **COM port (US2)**: the seam, the fake, `AciaComPortEndpoint`, the Win32
   implementation.
5. **History (US4)**: drop on resume, replay tests across all three, the
   idle-cost test.
6. **Settings (US5)**: staging, `SerialAttachmentView`, both pages, the
   infobar.
7. **Polish**: docs, CHANGELOG, README, full gates, a build for the owner.

## Complexity Tracking

None.
