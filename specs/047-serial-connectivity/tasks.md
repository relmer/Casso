# Tasks: Serial Connectivity

**Input**: Design documents from `specs/047-serial-connectivity/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Required. Each task writes its tests first, confirms they fail
against a stub, then implements. Tests use `FakeHostNetwork` and
`FakeHostSerialPorts`; no unit test opens a real socket or COM port.

**Organization**: by user story. US1 (modem) runs over US3's TCP link, so US3
comes first although both are P1.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an
  incomplete task)

---

## Phase 1: Setup

- [X] T001 Create `CassoEmuCore/Devices/Serial/` and `UnitTest/SerialTests/` and add their filters to `CassoEmuCore/CassoEmuCore.vcxproj(.filters)` and `UnitTest/UnitTest.vcxproj(.filters)`; fetch fixture ROMs with `scripts/FetchRoms.ps1 -Fixtures` if `RunTests.ps1` reports them missing. *Done: neither project has a filters file; ROMs fetched.*

---

## Phase 2: Foundational (blocks every story)

- [X] T002 [P] Add `SerialLineFormat` (baud, wordBits 5-8, parity None/Odd/Even/Mark/Space, stopBits One/OneAndHalf/Two) and `SerialLineFormat::FromRegisters (control, command, cpuClock)` in `CassoEmuCore/Devices/Serial/SerialLineFormat.{h,cpp}`; tests in `UnitTest/SerialTests/SerialLineFormatTests.cpp` cover every baud code (code 0 = 115,200), each word length, each parity setting and the 1.5 stop bit case. *Done: the table moved out of `Acia6551.cpp`, and the 6551 now times 8 bits with parity and SBN set as one stop bit, as the //c TRM specifies.*
- [X] T003 Add an error byte to each `SerialByteQueue` entry (`Push (bytes, count)` sets errors to 0; add `Push (value, errors)` and `TryPop (value, errors)`; `CopyTo` returns both) in `CassoEmuCore/Devices/SerialByteQueue.{h,cpp}`; extend `UnitTest/EmuTests/SerialByteQueueTests.cpp`. *Done: the single-entry push is `PushEntry`, since a literal 0 converts to a null pointer.*
- [X] T004 Land receive errors in `Acia6551::LandReceivedByte`: parity and framing bits set from the entry's errors and cleared by a good byte, host overrun sets the overrun bit, errors in the serial log's `SerialReceived` entry (`IoEvent::reg`); save state version 3 with the error byte per waiting entry, loading versions 1-2 with no errors, in `CassoEmuCore/Devices/Acia6551.{h,cpp}`; tests in `UnitTest/EmuTests/Acia6551Tests.cpp`
- [X] T005 Add `InputKind::AciaLines` and `InputKind::AciaConnection` in `CassoEmuCore/Debugger/Reverse/InputJournal.h`, their text in the input log `CassoEmuCore/Shell/DebugEventLogs.cpp`, and route both kinds to the chip in `MachineHost::ApplyDeviceInput` (`CassoEmuCore/Shell/MachineHost.cpp`). *Done: the input log lists them as `HostSerialLines` and `HostSerialLink` rows.*
- [X] T006 Add the host-link half of `IAciaEndpoint` (`OnFormatChanged`, `IsHostLink`, `Poll (cycles)`, `DropConnection`) in `CassoEmuCore/Devices/IAciaEndpoint.h`, and to `Acia6551`: `ReceiveFromHost (value, errors)` (journals `AciaByte` with errors in `data`), `SetHostLines (cts, dsr, dcd)` (journals `AciaLines` when they differ, applied in this tick through the pending inputs), `NoteHostConnection (isOpen, peer)` (journals `AciaConnection` and logs `SerialConnection`), `ApplyHostLinesNow`, `ReceiveFromHostNow`, `GetElapsedCycles`, `GetLineFormat`; polling from `Tick` at `kActivePollCycles` 1,024 after a busy poll and `kIdlePollCycles` 8,192 after a quiet one, skipped while `HostInputGate::IsHeld`; `OnFormatChanged` on attach and on a format change; `ApplyInput` for all three kinds, a replayed line change applied at the next `Tick` (research R3); tests with a scripted host-link endpoint in `UnitTest/EmuTests/AciaHostLinkTests.cpp`: journaling with errors, line change timing live versus replay, no poll behind live, poll intervals, mute. *Done: behind live the gate is tested again every active interval, not every tick. A mutation applying journaled lines at once fails the timing test.*
- [X] T007 [P] Add `IoEventKind::SerialConnection` and the text for it and for receive errors in `CassoEmuCore/Debugger/IoEventLog.{h,cpp}` and the decoder `CassoEmuCore/Debugger/Events/IoEventDecoder.cpp`; tests in `UnitTest/DebuggerTests/IoEventLogTests.cpp`
- [X] T008 [P] Add `SerialLinkReports` (thread-safe list of `{ portTitle, text }` with a generation; an empty text clears the port) in `CassoEmuCore/Devices/Serial/SerialLinkReports.{h,cpp}`, owned by `MachineHost`; tests in `UnitTest/SerialTests/SerialLinkReportsTests.cpp`
- [X] T009 Add the attachment options to `PortConfig` (`comPort`, `host`, `tcpPort` default 1977, `listen` false, `telnet` (false for TCP, true for the modem when absent), `modemListenPort` 0; JSON keys `comPort`, `host`, `port`, `listen`, `telnet`, `listenPort`; a port is 1-65535, 0 only for `listenPort`) and the device strings `serial-com-port`, `serial-tcp`, `serial-modem` in `CassoEmuCore/Core/MachineConfig.{h,cpp}`; parse tests in `UnitTest/EmuTests/MachineConfigTests.cpp`. *Done: the options are `PortConfig::serial`; writing them back is the settings staging's job (T028).*
- [X] T010 Factor `MachineBuilder::AttachSerialEndpoint` into one factory for any 6551 and port (`CreateSerialAttachment (const PortConfig &, Acia6551 &, portTitle, problem&)`) used by the Super Serial Card and both //c ports, with the //c's empty device as the loopback plug (research R9); add `IHostSerialPorts * hostSerialPorts` to `MachineBuildServices`; offline attachments when a seam is null; in `CassoEmuCore/Shell/MachineBuilder.{h,cpp}`; tests in `UnitTest/EmuTests/SuperSerialCardMachineTests.cpp` and a new `UnitTest/SerialTests/SerialAttachmentBuildTests.cpp` (each device string on each port position). *Done: `AttachSerialPort` and `CreateSerialAttachment`; the //c's problems go to `SerialLinkReports` and the card's to its attachment problem. The machine switch now clears the old attachments before the build (part of T032).*
- [X] T011 Call each serial chip's endpoint `DropConnection` from `MachineHost::DropHostConnections` in `CassoEmuCore/Shell/MachineHost.cpp`; test in `UnitTest/EmuTests/ReverseSerialTests.cpp`

**Checkpoint**: commit and push. The serial, reverse and Uthernet suites pass.

---

## Phase 3: User Story 3 - Connect over TCP (P1)

**Goal**: a port attached to TCP, out to a host or listening, with an optional
Telnet layer. **Independent test**: listen, connect a fake client, exchange
bytes including $FF both ways, close from each side.

- [X] T012 [P] [US3] Add `IHostNameLookup` and `IHostNetwork::LookUpHost` in `CassoEmuCore/Seams/IHostNetwork.h`; Win32 lookup on a worker thread in `CassoEmuCore/Seams/Win32HostNameLookup.{h,cpp}` and `Win32HostNetwork.cpp`; a table-driven lookup in `UnitTest/NetworkTests/FakeHostNetwork.{h,cpp}` (pending, resolved, failed). *Done: the Win32 lookup is an asynchronous `GetAddrInfoExW`, not a worker thread, so it can be canceled. `FakeHostNameLookup` is a separate pair, compiled into both test projects.*
- [X] T013 [P] [US3] Add `TelnetCodec` per research R7 in `CassoEmuCore/Devices/Serial/TelnetCodec.{h,cpp}`; tests in `UnitTest/SerialTests/TelnetCodecTests.cpp`: $FF doubling both ways, each negotiation reply, no reply loop, SB dropped, CR NUL both ways, a sequence split across reads
- [X] T014 [US3] Add `TcpSerialLink` over `IHostNetwork` in `CassoEmuCore/Devices/Serial/TcpSerialLink.{h,cpp}`: connect (literal or looked-up host, connect, failure states), listen (one connection; a second client closed at once; bind failure as port in use), send queue with partial sends, receive up to a limit, Telnet through `TelnetCodec`, close; tests in `UnitTest/SerialTests/TcpSerialLinkTests.cpp`. *Done.*
- [X] T015 [US3] Add `AciaTcpEndpoint` in `CassoEmuCore/Devices/Serial/AciaTcpEndpoint.{h,cpp}`: DCD and DSR on only while connected (FR-007), each change through `SetHostLines`, `NoteHostConnection` on open and close, bytes through `ReceiveFromHost` with backpressure, sent bytes queued, offline host / unresolved name / port in use reported to `SerialLinkReports` with DCD off (FR-022), `DropConnection`; tests in `UnitTest/SerialTests/AciaTcpEndpointTests.cpp` on a real `Acia6551` and `FakeHostNetwork`, including FR-009 pacing at the guest's baud rate
- [X] T016 [US3] Wire `serial-tcp` into the builder's factory (T010) and a guest-level test in `UnitTest/SerialTests/SerialGuestTests.cpp`: a 6502 program on a Super Serial Card machine receives and echoes over a fake connection

**Checkpoint**: commit and push.

---

## Phase 4: User Story 1 - Call a BBS (P1)

**Goal**: a Hayes-compatible modem over `TcpSerialLink`. **Independent test**:
dial a fake server, exchange text, escape, hang up; every result code and DCD.

- [X] T017 [US1] Add `HayesModem`, the command interpreter with no I/O, per contracts/modem-and-config.md and research R8, in `CassoEmuCore/Devices/Serial/HayesModem.{h,cpp}`: input from the guest with the cycle it arrived at, output toward the guest, requests to dial / answer / hang up, events from the link (connected, failed, closed, incoming), and `Tick (cycle)` for the escape guard, rings and the S7 timeout; tests in `UnitTest/SerialTests/HayesModemTests.cpp` for every command and result, word and number forms, echo and quiet, S-register range errors, the 40-character buffer, `A/`, the escape in each timing (FR-014), S2 > 127 and S12 = 0 (FR-015), guard time in emulated cycles at two clock speeds (FR-016), and the SC-004 sweep: 1,000 bursts with `+++` at varying positions, none leaving data mode
- [X] T018 [US1] Add `AciaModemEndpoint` in `CassoEmuCore/Devices/Serial/AciaModemEndpoint.{h,cpp}`: `HayesModem` plus a `TcpSerialLink` for dialing and one listening on `modemListenPort` (FR-012), DCD by `&C` (FR-013), DTR drop by `&D`, an incoming call while dialing closed at once, an unresolved name as `NO CARRIER`, `DropConnection` sending `NO CARRIER` with DCD off (FR-019); tests in `UnitTest/SerialTests/AciaModemEndpointTests.cpp` covering spec US1 scenarios 1-6
- [X] T019 [US1] Wire `serial-modem` into the builder's factory and a guest-level test in `UnitTest/SerialTests/SerialGuestTests.cpp`: a 6502 program dials, reads `CONNECT`, exchanges text and hangs up (SC-001)

**Checkpoint**: commit and push.

---

## Phase 5: User Story 2 - Use a real serial device (P1)

**Goal**: a port attached to a PC COM port. **Independent test**: through
`FakeHostSerialPorts`, the settings, data both ways, each line, a framing error.

- [X] T020 [P] [US2] Add `IHostSerialPorts` / `IHostSerialPort` per contracts/host-seams.md in `CassoEmuCore/Seams/IHostSerialPort.h` and `FakeHostSerialPorts` in `UnitTest/SerialTests/FakeHostSerialPorts.{h,cpp}` (ports that can be present, in use or unplugged; scripted bytes, errors and lines; the configuration, control lines, break and writes it was sent, for assertions)
- [X] T021 [US2] Add `AciaComPortEndpoint` in `CassoEmuCore/Devices/Serial/AciaComPortEndpoint.{h,cpp}`: format applied on change (FR-002), nearest rate with one report per substituted rate (research R5), DTR/RTS (FR-003) and break, lines to the chip (FR-004), errors with the last waiting byte and break as $00 with a framing error (FR-005), unplug reported with DCD off and a reopen every 2 seconds of emulated time, port in use and absent reported (FR-022); tests in `UnitTest/SerialTests/AciaComPortEndpointTests.cpp`, including every 6551 rate up to 19,200 passing data both ways (SC-002) and a mismatch producing framing errors in the status register (SC-003)
- [X] T022 [US2] Add `Win32HostSerialPorts` per contracts/host-seams.md in `CassoEmuCore/Seams/Win32HostSerialPorts.{h,cpp}`, created by the shell beside `Win32HostNetwork` and passed in `MachineBuildServices` (`CassoEmuCore/Shell/EmulatorShell*.cpp`)
- [X] T023 [US2] Wire `serial-com-port` into the builder's factory, with a test in `UnitTest/SerialTests/SerialAttachmentBuildTests.cpp`

**Checkpoint**: commit and push.

---

## Phase 6: User Story 4 - Rewind through a serial session (P2)

**Goal**: replays match live and send nothing; a resume drops the line.
**Independent test**: record a session, seek back and forth, compare.

- [X] T024 [US4] Replay tests in `UnitTest/EmuTests/ReverseSerialLinkTests.cpp` for each of TCP, modem and COM: a session replayed by seeking matches live at every cycle (status, received bytes, serial log) and the fake host receives nothing (FR-017, FR-018, SC-005)
- [X] T025 [US4] Resume tests in the same file: a connection open at a past point is closed on resume, DCD reads off, the modem returns `NO CARRIER` (FR-019); the COM port's lines are read again on the next poll
- [X] T026 [US4] Serial log tests in `UnitTest/DebuggerTests/IoEventLogTests.cpp` or `ReverseSerialTests.cpp`: each attachment's traffic, line changes, errors and connections appear live and in a replay (FR-020)
- [X] T027 [US4] Idle-cost test in `UnitTest/SerialTests/SerialIdleCostTests.cpp`: a TCP and a modem attachment, idle for one emulated second, make at most 130 host calls (SC-006)

**Checkpoint**: commit and push.

---

## Phase 7: User Story 5 - Choose a port's attachment (P2)

**Goal**: every port's attachment and options in Settings, saved, applied by
rebuild. **Independent test**: set each attachment, apply, rebuild, read back.

- [X] T028 [US5] Stage the attachment options for the Super Serial Card and the //c's `serial1` / `serial2` in `CassoEmuCore/Ui/Settings/SettingsPanelState.{h,cpp}`: read, write to JSON, `RequiresReset` on any attachment or option change on either machine (FR-021, FR-023); tests in `UnitTest/UiTests/SettingsPanelStateTests.cpp` (or the existing staging test file)
- [X] T029 [US5] Add `SerialAttachmentView` in `CassoEmuCore/Ui/Settings/SerialAttachmentView.{h,cpp}`: drop-down (Nothing on the card only, Loopback plug, Output file, Input file, COM port, TCP, Modem) and the chosen attachment's options (COM port list from `IHostSerialPorts::EnumeratePorts` when opened; host and port, or listen and port; Telnet; modem listen port), with field validation in the two-line error form; tests in `UnitTest/UiTests/SerialAttachmentViewTests.cpp`
- [X] T030 [US5] Use `SerialAttachmentView` in `CassoEmuCore/Ui/Settings/SerialCardPage.{h,cpp}`; update `UnitTest/UiTests/SerialCardPageTests.cpp`
- [X] T031 [US5] Add `SerialPortsPage` for the //c (port 1 and port 2, each a `SerialAttachmentView`), present only on a //c, in `CassoEmuCore/Ui/Settings/SerialPortsPage.{h,cpp}` and the sheet `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`; tests in `UnitTest/UiTests/SerialPortsPageTests.cpp`
- [X] T032 [US5] Close the old machine's attachments before the new machine's open on a rebuild (FR-023), in the machine switch path in `CassoEmuCore/Shell/`; test with two builds sharing one fake port and one listen port
- [X] T033 [US5] Display `SerialLinkReports` in a warning infobar in `CassoEmuCore/Shell/Components/ShellSerialCard.{h,cpp}` (newest report, cleared on recovery); tests in `UnitTest/UiTests/SerialCardNoticeTests.cpp`

**Checkpoint**: commit and push.

---

## Phase 8: Polish

- [X] T034 [P] Document the attachments, their options, Telnet, the modem command set and the history rules in `docs/` (the serial or Super Serial Card doc, created if none) and the Serial log entries in `docs/Debugger.md`
- [X] T035 [P] `CHANGELOG.md` and `README.md` entries (feature list, test counts)
- [ ] T036 Full gates: `scripts/RunTests.ps1 -Build` (Debug), the scenario suite, `scripts/CheckStyle.ps1 -Mode Tree`, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`
- [ ] T037 Build Release for the owner and list the steps for a COM port, a TCP connection and the modem (quickstart.md)

---

## Dependencies

- Phase 2 blocks everything.
- US3 (Phase 3) before US1 (Phase 4): the modem uses `TcpSerialLink`.
- US2 (Phase 5) depends only on Phase 2.
- US4 (Phase 6) follows US1, US2 and US3.
- US5 (Phase 7) follows the device strings (T009) and the seams (T020).

## Parallel opportunities

- T002, T007, T008 in Phase 2.
- T012 and T013 in Phase 3; T020 with any of Phase 3 or 4.
- T034 and T035.

## Implementation strategy

The spec ships whole: no story is released alone. Work proceeds phase by
phase, with a commit and push at each checkpoint, and the full gates run once
before the owner's build.
