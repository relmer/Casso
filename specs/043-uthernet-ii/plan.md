# Implementation Plan: Uthernet II Ethernet Card

**Branch**: `043-uthernet-ii` | **Date**: 2026-10-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/043-uthernet-ii/spec.md`

## Summary

Add an Uthernet II slot card for the Apple ][, ][+, //e and enhanced //e: a W5100 chip model driven through `$C0n4`-`$C0n7`, a bridge that maps its hardware TCP and UDP sockets onto non-blocking host sockets, and a virtual network (ARP, DHCP, ICMP echo, DNS relay, UDP and TCP NAT, port forwards) behind socket 0's MACRAW mode so IP65 software, ADTPro included, runs. Host networking sits behind a new `IHostNetwork` seam, faked in unit tests. A Settings > Network page configures the card. Design decisions are in [research.md](research.md).

## Technical Context

**Language/Version**: C++ (MSVC v145, `/std:c++latest` as the solution sets it)

**Primary Dependencies**: Winsock 2 (`ws2_32`), IP Helper (`iphlpapi`) for the DNS server address; Dxui for the settings page. No third-party libraries.

**Storage**: Machine configuration JSON (shipped definitions, user deltas through `UserConfigStore`).

**Testing**: Microsoft CppUnitTest in `UnitTest` (fake host network, `TestMachine`) and `ScenarioTests` (real Winsock on loopback).

**Target Platform**: Windows 11, x64 and ARM64 builds.

**Project Type**: Desktop emulator; all logic in `CassoEmuCore`.

**Performance Goals**: Idle card costs under 1% of emulation speed (SC-005); a pump runs every 1,023 CPU cycles only while networking is active.

**Constraints**: No blocking calls on the emulation thread (FR-021); no real network access from unit tests (FR-020); clean-room sources only (FR-029).

**Scale/Scope**: Four W5100 sockets; virtual network flows bounded by guest software (IP65 uses a handful).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How |
|---|---|---|
| I. Code Quality | Pass | EHM in every fallible function; one class per file pair; file-local constants per the placement rules; no anonymous namespaces. |
| II. Testing Discipline | Pass | Every class has unit tests; Winsock only behind `IHostNetwork`, faked in UnitTest; real sockets only in ScenarioTests on loopback. |
| III. UX Consistency | Pass | Settings page follows the sheet's Apply/Cancel model, sentence case, fixed error format for MAC errors. |
| IV. Performance | Pass | Pump gated on activity and on a cycle counter; no per-access allocation. |
| V. Simplicity | Pass | The virtual network covers only what IP65 uses (no congestion control, no IP fragmentation, no IPv6); functions stay short by factoring per-protocol handlers into their own classes. |
| VI. Thin Executable | Pass | Everything, `Win32HostNetwork` and the settings page included, lives in `CassoEmuCore`; nothing is added to an executable project. |

Post-design re-check: Pass. No violations to track.

## Project Structure

### Documentation (this feature)

```text
specs/043-uthernet-ii/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── w5100-guest-interface.md
│   ├── host-network-seam.md
│   └── machine-config.md
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Pch.h                                  # + winsock2.h, ws2tcpip.h, iphlpapi.h after windows.h
├── Seams/
│   ├── IHostNetwork.h                     # IHostNetwork, IHostSocket, HostEndpoint
│   └── Win32HostNetwork.h/.cpp            # Winsock implementation, Win32HostSocket in its own pair
├── Devices/Network/
│   ├── IW5100Backend.h                    # backend interface + null backend
│   ├── W5100.h/.cpp                       # chip: registers, pointer, buffers, commands
│   ├── W5100Registers.h                   # address and value constants
│   ├── W5100SocketBridge.h/.cpp           # hardware sockets -> host sockets, MACRAW -> VirtualNetwork
│   ├── VirtualNetwork.h/.cpp              # frame routing, ARP, ICMP
│   ├── PacketCodec.h/.cpp                 # header parse/build, checksums
│   ├── DhcpServer.h/.cpp
│   ├── UdpNat.h/.cpp
│   ├── TcpNat.h/.cpp
│   ├── TcpNatConnection.h/.cpp
│   └── PortForwarder.h/.cpp
├── Config/
│   ├── MacAddress.h/.cpp
│   └── NetworkCardSettings.h/.cpp         # settings struct, JSON read/write, validation
├── Core/
│   ├── MachineConfig.h/.cpp               # SlotConfig::network, loader
│   ├── MachineConfigUpgrade.h/.cpp        # inject the disabled card entry
│   └── ComponentRegistry.cpp              # register "uthernet2"
├── Machines/Apple2/Common/
│   └── UthernetCard.h/.cpp                # MemoryDevice, Tick, IRQ
├── Shell/
│   ├── MachineBuilder.h/.cpp              # services.hostNetwork; wire card, IRQ, settings
│   ├── MachineHost.cpp                    # tick the card in StepOne
│   ├── MachineRefs.h                      # uthernet
│   └── EmulatorShell / components         # own Win32HostNetwork, report port-forward errors
└── Ui/Settings/
    ├── SettingsPanelState.h/.cpp          # card settings in the snapshot
    ├── NetworkPage.h/.cpp                 # Settings > Network
    └── SettingsSheet.cpp                  # add the page

Resources/Machines/{Apple2,Apple2Plus,Apple2e,Apple2eEnhanced}/*.json   # slot 3 entry, version 13
(AssetBootstrap prior-hash table)                                      # version-12 hashes

UnitTest/
├── NetworkTests/
│   ├── FakeHostNetwork.h/.cpp
│   ├── W5100RegisterTests.cpp
│   ├── W5100BufferTests.cpp
│   ├── W5100SocketBridgeTests.cpp
│   ├── PacketCodecTests.cpp
│   ├── DhcpServerTests.cpp
│   ├── UdpNatTests.cpp
│   ├── TcpNatTests.cpp
│   ├── VirtualNetworkTests.cpp
│   ├── PortForwarderTests.cpp
│   ├── MacAddressTests.cpp
│   └── NetworkCardSettingsTests.cpp
├── EmuTests/
│   ├── TestMachine.h/.cpp                 # optional config/services customization
│   ├── UthernetCardTests.cpp              # bus decode, probe, IRQ, resets
│   └── UthernetGuestTests.cpp             # 6502 programs via Assembler
└── UiTests/NetworkSettingsTests.cpp

ScenarioTests/
└── UthernetLoopbackTests.cpp              # real Win32HostNetwork on 127.0.0.1
```

**Structure Decision**: The chip, bridge and virtual network go under a new `CassoEmuCore/Devices/Network/` folder beside `Disk`, `Printer` and `Tape`; the existing `CassoEmuCore/Net/` contains the update checker's HTTP client and is unrelated. The card class sits with the other slot cards in `Machines/Apple2/Common/`.

## Build Order

1. Seam and fake: `IHostNetwork`, `FakeHostNetwork`, `Win32HostNetwork`, Pch and link changes.
2. Chip: `W5100` with a null backend; register, pointer, wrap, buffer and reset tests.
3. Card on the bus: `UthernetCard`, registry, machine definitions, builder and host wiring, IRQ; probe test on a `TestMachine`.
4. Hardware sockets: `W5100SocketBridge` for TCP and UDP; bridge tests; guest UDP and TCP programs.
5. Virtual network: `PacketCodec`, `DhcpServer`, `UdpNat`, `TcpNat`, `VirtualNetwork`, `PortForwarder`; tests for each; MACRAW guest program.
6. Configuration: `MacAddress`, `NetworkCardSettings`, `SlotConfig::network`, loader, upgrade injection, version 13 and prior hashes.
7. Settings UI: `SettingsPanelState` additions, `NetworkPage`, sheet registration, port-forward error reporting.
8. Scenario test on real Winsock; CHANGELOG and README; full gate.

The order is build order only; the feature merges whole.

## Complexity Tracking

No constitution violations.
