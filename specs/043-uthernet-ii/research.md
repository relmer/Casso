# Research: Uthernet II Ethernet Card

Sources, all read for facts only:

- WIZnet W5100 datasheet v1.1.8 (register map, reset values, command and status values, socket memory math, UDP/IPRAW/MACRAW receive headers).
- Uthernet II manual, draft (slot I/O, auto-increment wrap, probe, interrupt chapter, socket examples).
- a2retrosystems/uthernet2 wiki, CC BY 3.0 (memory buffer access, hardware detection, shared access, operation modes, virtual W5100).
- ADTPro and IP65 sources, read only to establish which W5100 mode their drivers select. No code from them, and no emulator source of any kind, informs the design.

## R1. Which mode ADTPro and IP65 use

**Decision**: Emulate hardware TCP/UDP sockets and MACRAW with a virtual network (spec clarification).

**Findings**:

- ADTPro's `prodos-ethernet` and `prodos-vedrive` build targets link `ip65.lib` with `a2combo.lib` or `a2uther2.lib`. ADTPro's `uther.asm` imports `ip65_init`, `dhcp_init` and `udp_send`; its traffic is UDP on port 6502, and a configuration flag (`PDHCP`) selects DHCP or a static address.
- IP65's `drivers/w5100.s` sets `Sn_MR = $44` (MACRAW, MAC filter) on socket 0, uses `$C0n4`, `$C0n5`, `$C0n7`, sets MR to `$03` (indirect, auto-increment), splits socket 0's buffers 4 KB/4 KB, and splits its own copies at the buffer end.
- ADTPro's `prodos-vedrive2` target (`vuiidrive`) uses a hardware UDP socket (socket 1, port 6502, `Sn_MR = $02`). It is absent from the `prodos` target's dependency list, so the default build does not produce it.
- The card's demo code (`demo/udp.s`, `demo/tcp.s`) uses hardware sockets with slot 3 (`$C0B4`).

**Alternatives considered**: hardware sockets only (shipped ADTPro does not run); UDP-only virtual network (runs ADTPro and little else); Npcap bridge (third-party driver, administrator rights).

## R2. Address pointer and auto-increment

**Decision**: On every IDM_DR access with AI set: if bit 14 of the pointer is set and its low 13 bits are all 1, clear the low 13 bits; otherwise add 1 (16-bit wrap). The internal location accessed is `pointer & $7FFF`.

**Rationale**: Reproduces every documented point (manual, page 12): `$5FFF` to `$4000`, `$7FFF` to `$6000`, `$8000` up advancing normally while accessing `$0000` up, `$FFFF` to `$E000`. The wiki confirms the wrap covers the whole 8 KB buffer, not a socket's share. `$DFFF` to `$C000` is the rule's extrapolation; no document covers it.

**Other facts**: The manual states that a hardware reset sets the pointer to 0 and a software reset does not alter it. Reading IDM_AR0/AR1 returns the pointer.

**IND clear**: Per the datasheet, unused address lines A14-A2 are pulled down in indirect mode, so with IND clear the four slot locations access internal addresses 0-3 directly. Emulated as such.

## R3. Receive buffer headers

**Decision**:

- UDP: 8 bytes, peer IP (4), peer port (2), payload length (2), big-endian. The length field excludes the header, per the datasheet's receive pseudocode, which reads `get_size = header[6..7]` bytes after the header and advances `Sn_RX_RD` by `get_size + header_size`. The manual's prose states that the field includes the header, but its own example (3-byte payload, `Sn_RX_RSR` of 11) is about RSR, not the field. The datasheet governs.
- MACRAW: 2 bytes, the frame length plus 2, per the manual (a 34-byte frame reports 36).
- IPRAW: out of scope.

## R4. Where host networking runs

**Decision**: On the emulation thread, with non-blocking host sockets, pumped from the card's per-instruction `Tick` once every 1,023 CPU cycles (about 1 ms of emulated time at 1.023 MHz) and only while at least one socket is open, the virtual network has flows, or a port forward is listening.

**Rationale**: No locks, no cross-thread queues, and the guest-visible state changes only at instruction boundaries, so tests are deterministic. A poll of a handful of non-blocking sockets per millisecond is negligible. With nothing open the per-instruction cost is one counter add and one compare, which satisfies SC-005.

**Pause and fast-forward**: Paused, no pump runs; the PC's socket buffers fill and TCP flow control pushes back on the peer. In fast-forward, pumps run more often in wall time.

**Alternatives considered**: a dedicated network thread (synchronization required on every register access; nondeterministic tests); Windows overlapped I/O or I/O completion ports (same thread problem, more code).

## R5. The host network seam

**Decision**: `CassoEmuCore/Seams/IHostNetwork.h` with `IHostNetwork` (create TCP or UDP socket, query the PC's DNS server) and `IHostSocket` (bind, listen, accept, connect, send, receive, sendto, recvfrom, shutdown, close, readiness poll, all non-blocking). `Win32HostNetwork` implements it with Winsock (`WSAPoll` with a zero timeout for readiness; `GetNetworkParams` for the DNS server) and is linked through `#pragma comment (lib, ...)`, following `WasapiAudio.cpp`. Unit tests use `FakeHostNetwork`, which lets a test act as the remote peer.

**Rationale**: FR-020. A socket-level seam is narrow enough to fake byte for byte and wide enough for both the hardware-socket bridge and the virtual network's NAT, which share it.

## R6. Virtual network addressing

**Decision**: Subnet `10.0.65.0/24`. Gateway `10.0.65.1`, DNS `10.0.65.3`, guest `10.0.65.15`, mask `255.255.255.0`, lease 86,400 s. Gateway MAC `02:CA:55:0A:00:01` (locally administered). ARP requests for every IPv4 address except the guest's own get a reply with the gateway MAC, so traffic to a destination inside the virtual subnet's numbering (a LAN host numbered the same way) goes to the NAT.

**Gateway to loopback (FR-024h)**: TCP and UDP to `10.0.65.1` go to `127.0.0.1` on the PC, so the Casso Explorer link and an ADTPro server on the same PC work with no LAN.

**Rationale**: A fixed plan makes guest configurations portable between PCs. `10.0.65.x` avoids the common home ranges (`192.168.0/1.x`, `10.0.0.x`).

## R7. DNS

**Decision**: Relay. A UDP datagram to `10.0.65.3:53` is sent to the PC's first IPv4 DNS server from `IHostNetwork::GetDnsServer`, and the response returns to the guest with source `10.0.65.3:53`. With no IPv4 DNS server, queries get no response and the guest's resolver times out.

**Rationale**: No DNS parsing, no blocking `getaddrinfo` on the emulation thread. Spec FR-024f reflects this.

## R8. UDP NAT

**Decision**: One host UDP socket per guest source port, bound to the same port number when free so replies sent to the original source port (ADTPro's server replies to port 6502) arrive at the guest, and to an ephemeral port otherwise. Entries expire after 120 s of emulated time with no traffic. Inbound datagrams follow FR-019a.

## R9. TCP NAT

**Decision**: A per-connection state machine keyed by guest port, peer address and peer port, with no congestion control because the virtual wire is lossless:

- Guest SYN: start a non-blocking host connect; reply SYN-ACK on success with MSS `min(guest MSS, 1460)` and window 4,096, RST on failure.
- Guest data: accept only the next in-sequence bytes, queue them for the host up to 4,096 bytes, ACK what was queued; anything else gets a duplicate ACK.
- Host data: send to the guest in segments of at most the MSS, within the guest's advertised window; retain unacknowledged bytes and resend the oldest unacknowledged segment after 1 s of emulated time without an ACK.
- FIN from the guest: host `shutdown (SD_SEND)` once queued data is sent. Host EOF: FIN to the guest. RST either way: close both sides.
- Port forwards: an accepted host connection opens a NAT entry and sends SYN to `10.0.65.15:guestPort`, from the peer's IPv4 address (from `10.0.65.1` when the peer is on loopback).

**Rationale**: IP65 retransmits its own segments, and no segment is dropped on the virtual wire except when socket 0's RX buffer has no room, in which case frames wait in the network's queue instead of dropping. A retransmit timer covers guest stacks that discard segments themselves.

## R10. Hardware socket mapping

**Decision**:

| W5100 operation | Host action | Guest-visible result |
|---|---|---|
| TCP OPEN | none | Sn_SR = SOCK_INIT |
| CONNECT | non-blocking connect to Sn_DIPR:Sn_DPORT (`10.0.65.1` included as loopback) | SOCK_SYNSENT, then SOCK_ESTABLISHED + CON, or SOCK_CLOSED + TIMEOUT |
| LISTEN | bind Sn_PORT (loopback, or any address with LAN access) and listen | SOCK_LISTEN; first accepted connection produces SOCK_ESTABLISHED + CON and fills Sn_DIPR/Sn_DPORT; the listener then closes |
| SEND | queue Sn_TX_RD..Sn_TX_WR, write as the host accepts it | Sn_TX_FSR rises as the host accepts bytes; SEND_OK once the host has accepted all of them |
| received bytes | receive up to the RX buffer's free space | RECV, Sn_RX_RSR grows |
| peer FIN | | SOCK_CLOSE_WAIT + DISCON |
| DISCON | shutdown, then close | SOCK_CLOSED + DISCON |
| CLOSE | close | SOCK_CLOSED |
| UDP OPEN | bind any address : Sn_PORT (ephemeral if 0 or taken) | SOCK_UDP |
| UDP SEND | sendto Sn_DIPR:Sn_DPORT; `255.255.255.255` with SO_BROADCAST; port 67 to the virtual DHCP server | SEND_OK |

Bind failure for LISTEN (port in use): SOCK_CLOSED + TIMEOUT (spec edge case). SEND_MAC and SEND_KEEP: SEND_MAC as SEND; SEND_KEEP sets nothing and changes nothing on an established socket.

## R11. Reset and interrupts

**Decision**: `PowerCycle` performs the hardware reset (all registers, pointer to 0, close all host sockets). `SoftReset` (Ctrl-Reset) changes nothing in the chip, per the spec assumption, but drops the card's record of an asserted IRQ, since `MachineHost::SoftReset` clears the interrupt controller after the bus; the next `Tick` re-asserts while `IR & IMR` is non-zero. MR bit 7 runs the software reset, which does not alter the pointer.

## R12. Configuration

**Decision**: Device token `uthernet2`. Each shipped ][, ][+, //e and //e Enhanced definition gains `{ "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false }`, and each takes its next `$cassoMachineVersion`, with the previous hashes added to the prior-hash table. The card's settings are a `network` object on that slot entry:

```json
"network": { "mac": "02:xx:xx:xx:xx:xx", "lanAccess": false,
             "portForwards": [ { "protocol": "tcp", "hostPort": 6502, "guestPort": 6502 } ] }
```

Loading merges the shipped slot entries with the user's by slot number, so a user file saved before this feature gets the card's shipped entry, turned off, with no migration.

**Rationale**: A migration would rewrite every existing user configuration that has a `slots` array, including ones with nothing else to migrate, for a card that ships disabled. The settings page is the only place an entry is needed.

## R13. Settings UI

**Decision**: The card follows the same model as the other slot cards and the Super Serial Card (042): the machine configuration places it in slot 3 and the Machine tab's device list turns it on or off. A new Settings page, "Network", present only while the card is on (as the Storage page follows the disk controller), has a MAC address text field (validated as six hex pairs, unicast), a "Allow connections from other computers" checkbox (LAN access), and a port forward list with add and remove. Turning the card on when it has no MAC generates one from the OS random source. All edits follow the sheet's Apply/Cancel model. Turning the card on or off takes effect at the next machine build, which `RequiresReset` triggers; LAN access and port forwards apply to the running card through the CPU thread's command queue (`IDM_NETWORK_CONFIGURE`): open TCP connections stay open, UDP flows close, and the forwards reopen. The MAC address has no effect inside the chip, so a change to it is saved and nothing more.

## R14. Windows Firewall

**Facts**: Binding a TCP listener or UDP socket on a non-loopback address can raise the Windows Firewall prompt the first time. Loopback-only listeners do not. UDP sockets must bind the wildcard address to send to the LAN, so FR-019a filters inbound datagrams in Casso instead of by binding loopback. The quickstart includes a warning about the prompt.

## R15. Testing approach

**Decision**:

- Unit (UnitTest, `FakeHostNetwork`): W5100 register file and reset values; pointer and wrap; MR reset; RMSR/TMSR allocation; each socket command and status transition; TX/RX pointer math across wraps; UDP and MACRAW headers; IRQ; hardware-socket bridge; virtual network frame handling (ARP, DHCP, ICMP, UDP NAT, TCP NAT, DNS relay, port forwards, FR-019a filtering); config parse, defaults and upgrade injection; settings state.
- Guest (UnitTest, `TestMachine("Apple2e")` with the card in slot 3 and `FakeHostNetwork`): 6502 programs assembled by `Assembler` at test time: hardware UDP round trip, hardware TCP round trip larger than the socket's buffer share, and a MACRAW program that runs DHCP and a TCP exchange through the virtual network.
- Scenario (ScenarioTests, real `Win32HostNetwork`, loopback only): a hardware TCP and UDP round trip against a peer socket in the test, proving the Winsock implementation.
- Owner validation: shipped ADTPro against an ADTPro server on the same PC (SC-006). The agent does not download ADTPro (no external binaries).
