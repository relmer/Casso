---

description: "Task list for the Uthernet II Ethernet card"
---

# Tasks: Uthernet II Ethernet Card

**Input**: Design documents from `specs/043-uthernet-ii/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Requested by the feature description: unit tests on register and socket behavior against the datasheet with a fake socket seam, and a //e `TestMachine` test running a 6502 program assembled with Casso's assembler. Each test task comes before the code it covers and fails first.

**Organization**: Grouped by user story. Phase order is build order only; the feature merges whole.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 install the card, US2 hardware sockets, US3 MACRAW and ADTPro, US4 card settings

---

## Phase 1: Setup

- [X] T001 Add `#include <winsock2.h>`, `<ws2tcpip.h>` and `<iphlpapi.h>` after `<windows.h>` in CassoEmuCore/Pch.h and confirm the solution builds x64 Debug
- [X] T002 [P] Create folders CassoEmuCore/Devices/Network/ and UnitTest/NetworkTests/, and add their filters to CassoEmuCore/CassoEmuCore.vcxproj(.filters) and UnitTest/UnitTest.vcxproj(.filters) as files are added

---

## Phase 2: Foundational

**Purpose**: The host network seam, its fake, and the bare W5100 chip. Every story depends on these.

- [X] T003 Define `HostEndpoint`, `HostSocketKind`, `HostSocketReadiness`, `IHostSocket` and `IHostNetwork` per contracts/host-network-seam.md in CassoEmuCore/Seams/IHostNetwork.h
- [X] T004 [P] Implement `FakeHostNetwork` and `FakeHostSocket` (test-side peer: complete or fail connects, offer inbound connections, push bytes and datagrams, set EOF, capture sent bytes, cap `Send`) in UnitTest/NetworkTests/FakeHostNetwork.h/.cpp
- [X] T005 [P] Implement `Win32HostSocket` (non-blocking via `FIONBIO`, `WSAPoll` with zero timeout, `WSAEWOULDBLOCK` reported through out-parameters, `WSAEADDRINUSE` as `HRESULT_FROM_WIN32`) in CassoEmuCore/Seams/Win32HostSocket.h/.cpp
- [X] T006 Implement `Win32HostNetwork` (`WSAStartup` 2.2 in the constructor, `WSACleanup` in the destructor, `GetNetworkParams` for the first IPv4 DNS server, `E_NOT_SET` when none, `#pragma comment (lib, "ws2_32.lib")` and `"iphlpapi.lib"`) in CassoEmuCore/Seams/Win32HostNetwork.h/.cpp
- [X] T007 [P] Add W5100 address, bit and value constants from contracts/w5100-guest-interface.md in CassoEmuCore/Devices/Network/W5100Registers.h
- [X] T008 [P] Define `IW5100Backend` (`OnCommand (int socket, Byte command)`, `OnSoftwareReset()`) and `NullW5100Backend` in CassoEmuCore/Devices/Network/IW5100Backend.h
- [X] T009 Write W5100 register tests: every common and socket register's reset value (MR `$00`, RTR `$07D0`, RCR `$08`, RMSR/TMSR `$55`, PTIMER `$28`, Sn_DHAR `$FF` x6, Sn_TTL `$80`, Sn_TX_FSR = buffer size), reserved locations read `$00` and ignore writes, read-only registers ignore writes, MR bit 7 software reset does not alter the pointer, IND clear maps `$C0n4`-`$C0n7` to internal 0-3, in UnitTest/NetworkTests/W5100RegisterTests.cpp
- [X] T010 Write W5100 buffer tests: pointer high/low read-back, AI advance, wrap `$5FFF`->`$4000`, `$7FFF`->`$6000`, `$FFFF`->`$E000`, `$8000` accessing `$0000`, RMSR/TMSR allocation including `$06`, `$0A`, `$AA` (sockets 2-3 without buffers) and `$FF`, Sn_TX_FSR and Sn_RX_RSR math across the socket buffer end, in UnitTest/NetworkTests/W5100BufferTests.cpp
- [X] T011 Implement `W5100` per data-model.md (internal memory, pointer and R2 wrap, register side-effect table, `HardwareReset`, `SoftwareReset`, buffer placement from RMSR/TMSR, Sn_IR and IR clear-on-write-1, IR socket bits tracking Sn_IR != 0, `IsInterruptPending` = `IR & IMR`, backend-facing API) in CassoEmuCore/Devices/Network/W5100.h/.cpp until T009 and T010 pass

**Checkpoint**: The chip passes its datasheet tests with a null backend.

---

## Phase 3: User Story 1 - Install the card (Priority: P1)

**Goal**: The card is on the bus in a chosen slot of a ][, ][+, //e or //e Enhanced, found by the manual's probe.

**Independent Test**: Card in slot 3 of a //e `TestMachine`; `$80` then `$03` stored at `$C0B4` reads back `$03`; `$C0C4` does not.

### Tests for User Story 1

- [X] T012 [P] [US1] Write `UthernetCard` tests: decode of `$C0n4`-`$C0n7` only, empty-slot behavior at `$C0n0`-`$C0n3`, manual probe and RTR probe (SC-001), IRQ asserted while `IR & IMR` and released when cleared, Ctrl-Reset does not alter registers, and the card re-asserts a pending IRQ on the next tick, power cycle resets the pointer to 0, pump not called while idle, in UnitTest/EmuTests/UthernetCardTests.cpp
- [X] T013 [P] [US1] Write registry and machine-definition tests: `"uthernet2"` registered; the ][, ][+, //e and //e Enhanced definitions contain the disabled slot-3 entry; the //c definition does not, in UnitTest/EmuTests/MachineConfigTests.cpp and UnitTest/EmuTests/BackwardsCompatTests.cpp
- [ ] T014 [P] [US1] Write settings state tests for enabling the card on a machine whose user `slots` array has no `uthernet2` entry: the entry is added at slot 3, or the lowest free slot when 3 is taken; no entry when all seven are taken; not offered without a `slots` key, in UnitTest/UiTests/NetworkSettingsTests.cpp

### Implementation for User Story 1

- [X] T015 [US1] Implement `UthernetCard` (`MemoryDevice` on `$C0n4`-`$C0n7`, `Create` factory, `Tick` with `kPumpCycles` 1,023, IRQ registration and level tracking, `PowerCycle`, `SoftReset`, `GetSlot`) in CassoEmuCore/Machines/Apple2/Common/UthernetCard.h/.cpp
- [X] T016 [US1] Register `"uthernet2"` in CassoEmuCore/Core/ComponentRegistry.cpp and add the caption "Uthernet II (Ethernet)" to `s_kDeviceDisplayNames` in CassoEmuCore/Ui/Settings/SettingsPanelState.cpp
- [X] T017 [US1] Add `uthernet` to CassoEmuCore/Shell/MachineRefs.h; in CassoEmuCore/Shell/MachineBuilder.cpp cache the card, attach the interrupt controller and pass `services.hostNetwork` (new `IHostNetwork *` in `MachineBuildServices`, CassoEmuCore/Shell/MachineBuilder.h); tick the card in `MachineHost::StepOne` in CassoEmuCore/Shell/MachineHost.cpp
- [X] T018 [US1] Own a `Win32HostNetwork` in the shell and bind it into `MachineBuildServices` where the other services are bound (CassoEmuCore/Shell/EmulatorShell.cpp or its components)
- [X] T019 [US1] Add `{ "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false }` and the next version stamp to Resources/Machines/Apple2/Apple2.json, Apple2Plus/Apple2Plus.json, Apple2e/Apple2e.json and Apple2eEnhanced/Apple2eEnhanced.json; add each one's previous normalized SHA-256 to the prior-hash table in CassoEmuCore/AssetBootstrap.cpp
- [ ] T020 [US1] Add the card's slot entry, when absent, on enable in `SettingsPanelState` (slot 3 or the lowest free slot) in CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp
- [X] T021 [US1] Add an optional config/services customization hook to `TestMachine` (constructor overload taking `std::function<void (MachineConfig &, MachineBuildServices &)>`) in UnitTest/EmuTests/TestMachine.h/.cpp
- [ ] T022 [US1] Add `SetHardwareSlot (index, slot)` with free-slot validation (slots 1-7, not occupied by another enabled entry) to CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp, with tests in UnitTest/UiTests/NetworkSettingsTests.cpp

**Checkpoint**: The probe finds the card in a //e `TestMachine`; old user configs gain a disabled entry.

---

## Phase 4: User Story 2 - Hardware sockets (Priority: P1)

**Goal**: W5100 TCP and UDP sockets exchange bytes with host peers.

**Independent Test**: A 6502 program on a //e `TestMachine` completes UDP and TCP round trips with `FakeHostNetwork` as the peer.

### Tests for User Story 2

- [X] T023 [P] [US2] Write bridge TCP tests against `FakeHostNetwork`: OPEN to SOCK_INIT; CONNECT to SYNSENT then ESTABLISHED + CON, or CLOSED + TIMEOUT on refusal; `10.0.65.1` connects to `127.0.0.1`; SEND of bytes spanning the buffer end, partial host sends, SEND_OK and Sn_TX_FSR recovery; received bytes limited to RX free space, RECV and Sn_RX_RSR; peer EOF to CLOSE_WAIT + DISCON; DISCON and CLOSE to CLOSED; LISTEN bound to loopback (any address with LAN access), accept to ESTABLISHED with Sn_DIPR/Sn_DPORT from the peer, port in use to CLOSED + TIMEOUT; software reset closes host sockets, in UnitTest/NetworkTests/W5100SocketBridgeTests.cpp
- [X] T024 [P] [US2] Write bridge UDP tests: OPEN binds Sn_PORT (ephemeral when 0 or taken) to SOCK_UDP; SEND to Sn_DIPR:Sn_DPORT; broadcast enables SO_BROADCAST; datagrams land with the 8-byte header (payload length excludes the header); a datagram waits for RX space and one larger than the buffer is discarded; FR-019a filter delivers loopback and sent-to peers only unless LAN access; IPRAW, PPPoE and MACRAW on sockets 1-3 stay CLOSED, in UnitTest/NetworkTests/W5100SocketBridgeTests.cpp
- [ ] T025 [P] [US2] Write guest tests on a //e `TestMachine` with the card in slot 3 and `FakeHostNetwork`: 6502 programs assembled by `Assembler` at test time open a hardware UDP socket for a 1 KB round trip (SC-002) and a hardware TCP socket for a round trip larger than the socket's 2 KB share in both directions (SC-003), checked byte for byte, in UnitTest/EmuTests/UthernetGuestTests.cpp

### Implementation for User Story 2

- [X] T026 [US2] Implement `W5100SocketBridge` TCP mapping per research R10 (non-blocking connect, listen/accept, TX queue, RX fill, EOF, DISCON/CLOSE, SEND_MAC as SEND, SEND_KEEP no-op) in CassoEmuCore/Devices/Network/W5100SocketBridge.h/.cpp
- [X] T027 [US2] Implement the bridge's UDP mapping (bind, sendto, broadcast, recvfrom into the 8-byte header, FR-019a filter with per-socket sent-to set, port 67 routed to the DHCP server hook) in CassoEmuCore/Devices/Network/W5100SocketBridge.cpp
- [X] T028 [US2] Wire the bridge as the chip's backend inside `UthernetCard` and implement `IsActive` and `Pump` gating in CassoEmuCore/Machines/Apple2/Common/UthernetCard.cpp

**Checkpoint**: SC-002 and SC-003 pass against the fake.

---

## Phase 5: User Story 3 - MACRAW and ADTPro (Priority: P1)

**Goal**: Socket 0 in MACRAW connects to a virtual network with ARP, DHCP, ICMP echo, DNS relay, UDP and TCP NAT and port forwards.

**Independent Test**: A MACRAW 6502 program obtains `10.0.65.15` by DHCP, resolves the gateway by ARP, and completes UDP and TCP exchanges with fake peers.

### Tests for User Story 3

- [ ] T029 [P] [US3] Write `PacketCodec` tests: Internet checksum vectors (RFC 1071 example), IPv4/UDP/TCP pseudo-header checksums, Ethernet/ARP/IPv4/ICMP/UDP/TCP build-then-parse round trips, rejection of truncated and bad-checksum frames, in UnitTest/NetworkTests/PacketCodecTests.cpp
- [ ] T030 [P] [US3] Write `DhcpServer` tests: DISCOVER to OFFER and REQUEST to ACK with yiaddr `10.0.65.15`, mask `255.255.255.0`, router `10.0.65.1`, DNS `10.0.65.3`, lease 86,400, server identifier `10.0.65.1`; transaction id and client MAC echoed; other message types and malformed packets get no reply, in UnitTest/NetworkTests/DhcpServerTests.cpp
- [ ] T031 [P] [US3] Write `UdpNat` tests: flow per guest source port bound to the same host port when free; gateway destination to loopback; DNS address relayed to `GetDnsServer` and replies rewritten to `10.0.65.3:53`; no DNS server means no reply; inbound datagrams become guest frames, with a loopback source rewritten to the gateway; FR-019a filter; expiry after 120 s of emulated cycles, in UnitTest/NetworkTests/UdpNatTests.cpp
- [ ] T032 [P] [US3] Write `TcpNat` tests: SYN to host connect, SYN-ACK with MSS `min(guest, 1460)` and window 4,096, RST on refusal; in-sequence guest data queued (cap 4,096) and ACKed, out-of-sequence data gets a duplicate ACK; host data segmented by MSS within the guest window; resend of the oldest unacked segment after 1 s of emulated cycles; FIN and RST in both directions; inbound connections from port forwards send SYN to the guest, in UnitTest/NetworkTests/TcpNatTests.cpp
- [ ] T033 [P] [US3] Write `VirtualNetwork` tests: ARP replies for every address but the guest's; ICMP echo to the gateway; MAC filter on socket 0 with Sn_MR bit 6; frames queue when socket 0's RX buffer is full and none are dropped; the 2-byte MACRAW header includes itself, in UnitTest/NetworkTests/VirtualNetworkTests.cpp
- [ ] T034 [P] [US3] Write `PortForwarder` tests: one listener or bound socket per forward, loopback unless LAN access, bind conflicts reported as errors, accepted connections and datagrams go to the NAT, in UnitTest/NetworkTests/PortForwarderTests.cpp
- [ ] T035 [P] [US3] Write a MACRAW guest test on a //e `TestMachine`: a 6502 program assembled at test time opens socket 0 with `Sn_MR = $44`, runs DISCOVER/REQUEST, ARPs for `10.0.65.1`, and completes a 1 KB TCP round trip with a fake peer through the gateway (SC-007), in UnitTest/EmuTests/UthernetGuestTests.cpp

### Implementation for User Story 3

- [ ] T036 [P] [US3] Implement `PacketCodec` in CassoEmuCore/Devices/Network/PacketCodec.h/.cpp
- [ ] T037 [P] [US3] Implement `DhcpServer` in CassoEmuCore/Devices/Network/DhcpServer.h/.cpp
- [ ] T038 [US3] Implement `UdpNat` in CassoEmuCore/Devices/Network/UdpNat.h/.cpp
- [ ] T039 [US3] Implement `TcpNatConnection` and `TcpNat` in CassoEmuCore/Devices/Network/TcpNatConnection.h/.cpp and TcpNat.h/.cpp
- [ ] T040 [US3] Implement `PortForwarder` in CassoEmuCore/Devices/Network/PortForwarder.h/.cpp
- [ ] T041 [US3] Implement `VirtualNetwork` (dispatch, ARP, ICMP echo, outbound queue, guest MAC) in CassoEmuCore/Devices/Network/VirtualNetwork.h/.cpp
- [ ] T042 [US3] Connect socket 0 MACRAW in the bridge to `VirtualNetwork` (SEND to `OnGuestFrame`, queued frames into RX with the 2-byte header, MF filter) and hardware UDP to port 67 to `DhcpServer`, in CassoEmuCore/Devices/Network/W5100SocketBridge.cpp and CassoEmuCore/Machines/Apple2/Common/UthernetCard.cpp

**Checkpoint**: SC-007 passes against the fake.

---

## Phase 6: User Story 4 - Card settings (Priority: P2)

**Goal**: MAC address, LAN access and port forwards on a Settings > Network page, persisted on the card's slot entry.

**Independent Test**: Change the MAC and add a TCP forward, apply, rebuild the machine: the MAC persists and a connection to the forwarded host port arrives at the guest.

### Tests for User Story 4

- [ ] T043 [P] [US4] Write `MacAddress` tests: parse colon and hyphen forms, fail on wrong length, non-hex and multicast addresses, format, `MakeRandomLocal` sets the locally administered bit and clears the multicast bit, in UnitTest/NetworkTests/MacAddressTests.cpp
- [ ] T044 [P] [US4] Write `NetworkCardSettings` tests: JSON read with all keys, missing keys default (no MAC, `lanAccess` false, no forwards), invalid MAC/protocol/port replaced by defaults with a warning, duplicate (protocol, hostPort) invalid, write round trip per contracts/machine-config.md, in UnitTest/NetworkTests/NetworkCardSettingsTests.cpp
- [ ] T045 [P] [US4] Write settings state tests: enabling the card without a MAC generates one; MAC edit with invalid text produces the fixed-format error and the old value remains; LAN access and forward add/remove mark the state dirty and require a reset; Apply writes the `network` object on the slot entry; Cancel restores, in UnitTest/UiTests/NetworkSettingsTests.cpp

### Implementation for User Story 4

- [ ] T046 [P] [US4] Implement `MacAddress` in CassoEmuCore/Config/MacAddress.h/.cpp
- [ ] T047 [US4] Implement `NetworkCardSettings` and `PortForward` (JSON read/write, validation) in CassoEmuCore/Config/NetworkCardSettings.h/.cpp
- [ ] T048 [US4] Add `NetworkCardSettings network` to `SlotConfig` and parse it in the loader in CassoEmuCore/Core/MachineConfig.h/.cpp; pass it to the card in CassoEmuCore/Shell/MachineBuilder.cpp (LAN access to bridge and NAT, forwards to `PortForwarder`)
- [ ] T049 [US4] Report port-forward bind failures at machine start through the shell's infobar path (FR-027b) from CassoEmuCore/Shell/MachineBuilder.cpp and the shell component that displays build warnings
- [ ] T050 [US4] Add card settings to the `SettingsPanelState` snapshot with `SetNetworkCardMac`, `SetNetworkLanAccess`, `AddPortForward`, `RemovePortForward`, MAC generation on enable, and `BuildJson` output, in CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp
- [ ] T051 [US4] Implement `NetworkPage` (enable toggle, slot combo of free slots, MAC text input with error label, "Allow connections from other computers" checkbox, port forward list view with protocol combo, host and guest port inputs, Add and Remove buttons; sentence case) in CassoEmuCore/Ui/Settings/NetworkPage.h/.cpp
- [ ] T052 [US4] Register the Network page in the settings sheet, present for machines with slots, in CassoEmuCore/Ui/Settings/SettingsSheet.h/.cpp

**Checkpoint**: Settings round-trip through the user config and are passed to the card at the next build.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T053 [P] Write a real-Winsock loopback test: `Win32HostNetwork` TCP and UDP round trips through the bridge against a peer socket in the test, `127.0.0.1` only, in ScenarioTests/UthernetLoopbackTests.cpp
- [ ] T054 [P] Measure SC-005: idle card overhead on a //e `TestMachine` (cycles per wall second with and without the card) and record the result in specs/043-uthernet-ii/quickstart.md
- [ ] T055 Verify each new test fails without its code (stub or revert the covered function, confirm red, restore and touch the file)
- [ ] T056 Validate the Network page on screen: launch the build in the background with `--title`, open Settings > Network, capture it, check layout and sentence case
- [ ] T057 [P] Add a CHANGELOG.md `[Unreleased]` entry and a README.md feature and test-count update, after the owner has tested
- [ ] T058 Run the merge gate: `scripts/RunTests.ps1 -Build` (Debug and Release), `scripts/RunTests.ps1 -Build -Scenario`, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, `git add -A` then `scripts/CheckStyle.ps1 -Mode Tree`

---

## Dependencies & Execution Order

- Setup (T001-T002) before everything.
- Foundational (T003-T011) blocks all stories. T004, T005, T007 and T008 run in parallel after T003.
- US1 (T012-T022) after Foundational. US2 depends on the card on the bus (T015-T017). US3 depends on the bridge (T026-T028). US4's config plumbing (T048) depends on the card (T015) and the forwarder (T040).
- Within each story, tests come first and fail before the implementation.
- Polish after all stories.

## Parallel Examples

- Foundational: T004 `FakeHostNetwork`, T005 `Win32HostSocket`, T007 `W5100Registers.h`, T008 `IW5100Backend.h`.
- US3 tests: T029-T034 touch separate files and run in parallel; T036 and T037 likewise.
- US4: T043, T044, T045 and T046 in parallel.

## Implementation Strategy

The feature merges to master whole, after every phase is done and the merge gate (T058) passes; no partial merge. Within the branch, finish each checkpoint before starting the next, commit at each checkpoint, and push the feature branch.

## Notes

- Clean-room: code from the datasheet, the card manual and the card wiki only (FR-029).
- No unit test may open a real socket; only ScenarioTests uses `Win32HostNetwork`.
- `.specify/feature.json` stays out of every commit.
