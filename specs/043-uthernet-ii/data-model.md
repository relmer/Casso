# Data Model: Uthernet II Ethernet Card

All types live in `CassoEmuCore` unless marked otherwise. One class per `.h`/`.cpp` pair.

## UthernetCard (`Machines/Apple2/Common/UthernetCard`)

`MemoryDevice` on `$C0n4`-`$C0n7`, `n = slot + 8`.

| Field | Meaning |
|---|---|
| `m_slot` | 1-7 |
| `m_chip` | `W5100` |
| `m_bridge` | `W5100SocketBridge`, the chip's backend |
| `m_network` | `VirtualNetwork`, used by the bridge for MACRAW and DHCP |
| `m_cyclesSincePump` | cycles accumulated toward the next pump |
| `m_irqSource`, `m_irqAsserted` | interrupt controller registration and last driven level |

Behavior: `Read`/`Write` forward the offset (0-3) to `W5100::ReadIndirect`/`WriteIndirect`. `Tick (cycles)` adds cycles, and when the total is at least `kPumpCycles` (1,023) and `m_bridge.IsActive()`, calls `m_bridge.Pump (cpuCycleCount)`; it then drives the IRQ line from `m_chip.IsInterruptPending()`. `PowerCycle` calls `m_chip.HardwareReset()` and `m_bridge.CloseAll()`. `SoftReset` clears `m_irqAsserted` only.

## W5100 (`Devices/Network/W5100`)

The chip, with no knowledge of hosts.

| Field | Meaning |
|---|---|
| `m_mem[0x8000]` | internal address space: common registers, socket registers, TX and RX memory |
| `m_pointer` | 16-bit indirect address pointer |
| `m_backend` | `IW5100Backend *`, receives socket commands |
| per socket: `rxWrite`, `txBase/txSize`, `rxBase/rxSize` | RX write position (not guest-visible), buffer placement derived from TMSR/RMSR |

State transitions:

- `WriteIndirect (0, v)`: bit 7 set runs `SoftwareReset()` (all registers to reset values, sockets closed through the backend, MR = `$00`, pointer not altered).
- `WriteIndirect (1|2, v)`: sets the pointer's high or low byte.
- `ReadIndirect/WriteIndirect (3, ...)`: accesses `m_mem[pointer & $7FFF]` through the register side-effect table, then advances the pointer per research R2 when AI is set.
- Writes to `Sn_CR` call `m_backend->OnCommand (n, cmd)`, after which `Sn_CR` reads `$00`.
- Writes to `Sn_IR` clear the written bits; writes to IR clear CONFLICT/UNREACH/PPPoE bits written with 1.
- Reads of `Sn_TX_FSR` and `Sn_RX_RSR` are computed from the pointers.

Backend-facing API: `GetSocketMode`, `SetSocketStatus`, `RaiseSocketInterrupt`, `GetTxPending (n)`, `CopyTx (n, out)`, `CommitTx (n)`, `GetRxFree (n)`, `TryAppendRx (n, bytes)`, `GetDestination (n)`, `SetDestination (n, ip, port)`, `GetSourcePort (n)`, `SetUnreachable (ip, port)`.

Validation: commands invalid for the socket's mode or state are accepted (CR reads `$00`) and change nothing, as on the chip.

## IW5100Backend (`Devices/Network/IW5100Backend.h`)

`OnCommand (int socket, Byte command)` and `OnSoftwareReset()`. A null implementation sits in the same header for tests of the bare chip.

## W5100SocketBridge (`Devices/Network/W5100SocketBridge`)

Implements `IW5100Backend`. Per socket:

| Field | Meaning |
|---|---|
| `host` | `std::unique_ptr<IHostSocket>`, null when no host socket |
| `listener` | host listener during SOCK_LISTEN |
| `phase` | Idle, Connecting, Established, PeerClosed |
| `txQueue` | bytes from SEND not yet accepted by the host |
| `udpPeers` | addresses this UDP socket has sent to (FR-019a filter) |

`Pump (now)` polls every host socket once: completes connects and accepts, writes `txQueue`, reads into the RX buffer up to `GetRxFree`, detects EOF and errors. Socket 0 in MACRAW sends each SEND's bytes to `VirtualNetwork::OnGuestFrame` and moves `VirtualNetwork::TryTakeFrame` output into RX with the 2-byte header. UDP to port 67 goes to `DhcpServer::Process`.

`IsActive()`: true while any socket is open or the virtual network has flows or listeners.

## VirtualNetwork (`Devices/Network/VirtualNetwork`)

Routes frames between socket 0 and the NAT.

| Field | Meaning |
|---|---|
| `m_outbound` | queue of frames for the guest |
| `m_guestMac` | source MAC of the guest's most recent frame |
| `m_dhcp` | `DhcpServer` |
| `m_udp` | `UdpNat` |
| `m_tcp` | `TcpNat` |
| `m_forwards` | `PortForwarder` |
| `m_lanAccess` | FR-019a mode |

Dispatch: ARP to `ArpReply`; IPv4 ICMP echo to the gateway to an echo reply; UDP to `10.0.65.3:53` to `UdpNat` aimed at the PC's DNS server; UDP to port 67 to `DhcpServer`; other UDP to `UdpNat`; TCP to `TcpNat`. Frames with bad checksums are dropped.

## PacketCodec (`Devices/Network/PacketCodec`)

Static helpers: parse and build Ethernet, ARP, IPv4, ICMP, UDP and TCP headers; Internet checksum. No state.

## DhcpServer (`Devices/Network/DhcpServer`)

`Process (request bytes) -> reply bytes`. DISCOVER to OFFER, REQUEST to ACK, both with the R6 addresses, mask, router, DNS and lease time. Other message types get no reply.

## UdpNat (`Devices/Network/UdpNat`)

Map of guest source port to `UdpFlow { host socket, lastActivityCycle }`. Outbound: create or reuse the flow, `sendto` the destination (gateway mapped to loopback, DNS address mapped to the PC's DNS server). Inbound: each received datagram becomes a guest frame from its source (loopback mapped to the gateway, DNS server mapped to the DNS address). Expiry after 120 s of emulated time.

## TcpNat and TcpNatConnection (`Devices/Network/TcpNat`, `TcpNatConnection`)

`TcpNat` contains connections keyed by (guest port, peer IP, peer port). `TcpNatConnection` fields: `state` (SynReceived, Connecting, Established, GuestFinSent, HostFinSent, Closed), `guestNext` (next sequence number from the guest), `ourNext`, `ourUnacked`, `guestWindow`, `mss`, `toHost` (queued guest data, cap 4,096), `toGuest` (unacknowledged and unsent host data), `lastSendCycle`, `host` socket. Transitions per research R9.

## PortForwarder (`Devices/Network/PortForwarder`)

One host listener (TCP) or bound socket (UDP) per configured `PortForward`, bound per FR-019a. Accepted TCP connections go to `TcpNat::AcceptInbound`; UDP datagrams to `UdpNat::DeliverInbound`. Bind failures are collected as errors for the shell to report (FR-027b).

## NetworkCardSettings (`Config/NetworkCardSettings`)

| Field | Type | Default | Validation |
|---|---|---|---|
| `mac` | `MacAddress` | generated on first enable | six octets, multicast bit clear |
| `lanAccess` | bool | false | |
| `portForwards` | vector of `PortForward` | empty | protocol tcp or udp; ports 1-65535; no duplicate (protocol, hostPort) |

Stored as the `network` object on the card's slot entry and loaded into `SlotConfig::network`.

## MacAddress (`Config/MacAddress`)

Six bytes. `TryParse ("02:CA:55:12:34:56")` accepts colon- or hyphen-separated hex pairs; `Format`; `IsUnicast`; `MakeRandomLocal (random bytes)` sets the locally administered bit and clears the multicast bit.

## IHostNetwork and IHostSocket (`Seams/IHostNetwork.h`)

See [contracts/host-network-seam.md](contracts/host-network-seam.md). `Win32HostNetwork` (`Seams/Win32HostNetwork`) in the product; `FakeHostNetwork` (UnitTest) in tests.

## Settings state

`SettingsPanelState` gains the card's `NetworkCardSettings` in its `original` and `current` snapshots plus `SetNetworkCardSlot`, `SetNetworkCardMac`, `SetNetworkLanAccess`, `AddPortForward`, `RemovePortForward`. `BuildJson` writes them to the slot entry's `network` object. `NetworkPage` (`Ui/Settings/NetworkPage`) binds Dxui widgets to these setters.
