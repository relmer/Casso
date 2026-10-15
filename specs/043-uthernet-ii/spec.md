# Feature Specification: Uthernet II Ethernet Card

**Feature Branch**: `043-uthernet-ii`

**Created**: 2026-10-14

**Status**: Implemented on branch `043-uthernet-ii`; not merged

**Input**: Emulate the Uthernet II Ethernet card (WIZnet W5100) for the Apple ][+ and //e. The card sits in a slot and is driven in the W5100's indirect bus mode through four registers at `$C0n4`-`$C0n7`. Emulate the common registers, the four hardware sockets and the 8 KB TX and RX buffers. Hardware TCP and UDP sockets map onto Windows sockets on the PC, behind a seam unit tests can fake. Card settings, machine config and settings UI to put the card in a slot. The original Uthernet (CS8900A) is out of scope. Clean-room: the WIZnet W5100 datasheet and the card's published documentation only. Motivation: spec 033 (Casso Explorer) adds an ADTPro-compatible Apple II link, which on a ][+ or //e can run over Ethernet with an Uthernet II; the owner has no card.

## Context

### The card

The Uthernet II (a2RetroSystems) has a WIZnet W5100 and no firmware ROM. Only two of the chip's address lines are wired to the bus, so software drives it in the W5100's indirect bus mode through four slot I/O locations, where `n` is the slot number plus 8:

| Location | Register | Purpose |
|---|---|---|
| `$C0n4` | MR | Mode: software reset, ping block, PPPoE, address auto-increment, indirect mode |
| `$C0n5` | IDM_AR0 | Internal address, high byte |
| `$C0n6` | IDM_AR1 | Internal address, low byte |
| `$C0n7` | IDM_DR | Data at the internal address |

The chip's internal 32 KB address space has the common registers at `$0000`-`$002F`, the four socket register blocks at `$0400`-`$07FF`, the TX buffers at `$4000`-`$5FFF` and the RX buffers at `$6000`-`$7FFF`. RMSR and TMSR split each 8 KB buffer among the four sockets, 1, 2, 4 or 8 KB apiece, allocated from socket 0 upward.

With auto-increment on, every data access advances the address. The card manual documents a wrap the datasheet omits: past `$5FFF` the address returns to `$4000`, past `$7FFF` to `$6000`, and past `$FFFF` to `$E000`. An address at `$8000` or above accesses the location 32 KB lower. The wrap covers the whole TX or RX buffer, not one socket's share, so driver code splits its copies at a socket's boundary itself. IP65 and the datasheet's own procedure both do.

Software finds the card by behavior, since neither the card nor the chip has ID bytes: write `$80` to `$C0n4` and test for `$00`, then write `$03` and test for `$03`. A second published probe reads RTR, whose reset value is `$07D0`.

### Which W5100 mode ADTPro uses

How much of a network Casso has to supply depends on the mode, so the mode was researched before the spec was written.

- **ADTPro's shipped Ethernet client and its virtual Ethernet drive (VEDRIVE) link the IP65 stack.** IP65's W5100 driver opens socket 0 in MACRAW mode (`Sn_MR = $44`, MACRAW with the MAC filter) and exchanges whole Ethernet frames. ARP, IPv4, UDP and DHCP all run in 6502 code. ADTPro's traffic is UDP on port 6502; DHCP is optional in its configuration.
- **ADTPro's source also contains an Uthernet II-only virtual drive (`vuiidrive`)** that drives a hardware UDP socket directly (socket 1, port 6502, `Sn_MR = $02`). Its build target is not part of ADTPro's default build, so the shipped disk does not include it.
- **Hardware TCP and UDP sockets alone therefore do not run shipped ADTPro.** Running it requires MACRAW, and MACRAW requires a component on the far side of the emulated wire that processes Ethernet frames: either a small virtual network inside Casso, or a packet-capture driver such as Npcap that puts frames on the real LAN.
- Other Uthernet II software is split: IP65 and Contiki applications use MACRAW; the card's demo code, ADTPro's `vuiidrive` and newer IP65 programs such as those in the emailler suite use the hardware sockets.

### Hardware sockets on a PC

A hardware TCP or UDP socket is close to a BSD socket already, so each one maps onto a Windows socket: OPEN and CONNECT become a host connect, LISTEN a host listen, SEND a host send, and data the host receives lands in the socket's RX buffer with the datasheet's headers. The guest's IP, gateway and subnet registers have no effect on where host sockets go; the PC's own network stack routes them.

The W5100 has no DHCP client. DHCP is guest software sending UDP broadcasts from port 68 to port 67. Windows' own DHCP client uses port 68 on the PC.

## Clarifications

### Session 2026-10-14

- Q: How much MACRAW support should Casso provide? -> A: A virtual network inside Casso for socket 0's MACRAW frames: ARP, a DHCP server, ICMP echo, DNS, and UDP and TCP relayed onto host sockets (NAT). UDP alone would run shipped ADTPro and little else; most IP65 and Contiki programs (telnet, wget, IRC, the web browser) use TCP. Npcap bridging was ruled out because it requires a third-party driver and administrator rights (FR-024 to FR-024f).
- Q: Which machines can connect to an Apple II program that listens for connections? -> A: This PC only by default; a card setting opens listening ports to the LAN, with the Windows Firewall prompt that implies (FR-019a, FR-027a).
- Q: Can connections started from the PC or the LAN get through to a server on the Apple II behind the virtual network's NAT? -> A: Yes, through a list of port forwards in the card settings, each a protocol (TCP or UDP), a host port and a guest port; whether other machines can use them depends on the LAN access setting (FR-024g, FR-027b).
- Q: What does the card's MAC address setting control? -> A: Nothing inside the chip. It is the card's label value, displayed in the card settings for the user to enter in guest software; SHAR (the chip's MAC address register, `$0009`-`$000E`) resets to `$00` as the datasheet specifies, and guest software writes the MAC into it (FR-027).
- Q: What happens to DHCP requests a guest program sends through a hardware UDP socket? -> A: Casso intercepts UDP sent to port 67 from a hardware socket and replies from the virtual network's DHCP server, with the same configuration a MACRAW program receives. Windows' own DHCP client uses port 68, so relaying to the real LAN is not viable (FR-023).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Install the card (Priority: P1)

A user opens the settings for an Apple ][, ][+, //e or enhanced //e, turns on the Uthernet II, picks its slot and restarts the machine. Apple II software that probes for the card finds it in that slot.

**Why this priority**: Nothing else in the feature is reachable until the card is on the bus.

**Independent Test**: Enable the card in slot 3 of a //e, boot to the Monitor, store `$80` then `$03` at `$C0B4` and read `$C0B4` back as `$03`.

**Acceptance Scenarios**:

1. **Given** a //e with the card disabled, **When** the user enables it in slot 3 and applies, **Then** `$C0B4`-`$C0B7` respond as the W5100's indirect registers and the setting persists across restarts.
2. **Given** the card enabled, **When** the user disables it, **Then** the slot's I/O locations have empty-slot behavior again.
3. **Given** an Apple //c, **When** the user opens its hardware settings, **Then** the card is not offered, since the //c has no slots.

---

### User Story 2 - Talk to the PC through hardware sockets (Priority: P1)

An Apple II program opens a W5100 TCP or UDP socket, connects to or listens on an address, and exchanges bytes with a program on the PC or elsewhere on the network, the way it would on a real card on a real LAN.

**Why this priority**: This is the chip's primary mode and the transport the scenario test and the 033 link use.

**Independent Test**: A small 6502 program, assembled with Casso's assembler, opens a UDP socket to a port a test peer is bound to, sends a datagram, and receives the peer's reply in its RX buffer with the 8-byte UDP header.

**Acceptance Scenarios**:

1. **Given** a TCP socket in SOCK_INIT with a destination IP and port, **When** the guest issues CONNECT and a peer accepts, **Then** Sn_SR reads SOCK_ESTABLISHED and Sn_IR has CON set.
2. **Given** an established TCP socket, **When** the guest writes bytes to its TX buffer, advances Sn_TX_WR and issues SEND, **Then** the peer receives exactly those bytes, Sn_TX_RD catches up to Sn_TX_WR, Sn_TX_FSR returns to the buffer size and Sn_IR has SEND_OK set.
3. **Given** an established TCP socket, **When** the peer sends bytes, **Then** Sn_RX_RSR grows by that count, the bytes appear at Sn_RX_RD in the socket's RX buffer, and Sn_IR has RECV set.
4. **Given** a UDP socket in SOCK_UDP, **When** a datagram arrives, **Then** the RX buffer receives an 8-byte header of peer IP, peer port and data length followed by the data.
5. **Given** a TCP socket, **When** the guest issues LISTEN and a peer connects to that port, **Then** Sn_SR moves to SOCK_ESTABLISHED and Sn_DIPR and Sn_DPORT contain the peer's address.
6. **Given** a connected TCP socket, **When** the peer closes, **Then** Sn_SR moves to SOCK_CLOSE_WAIT and Sn_IR has DISCON set; **When** the guest issues DISCON or CLOSE, **Then** Sn_SR reads SOCK_CLOSED.
7. **Given** a CONNECT to an address with no listener, **When** the host connect fails, **Then** Sn_IR has TIMEOUT set and Sn_SR reads SOCK_CLOSED.

---

### User Story 3 - Run ADTPro's Ethernet client (Priority: P1)

A user boots ADTPro's Ethernet client on an emulated //e with the card, points it at an ADTPro server on the PC, and transfers a disk.

**Why this priority**: This is the reason the feature exists. ADTPro's shipped client uses MACRAW, and so do IP65's and Contiki's other programs.

**Independent Test**: Frames the test builds pass through a 6502 frame pump in MACRAW mode on socket 0: a DHCP exchange for the guest address, an ARP request for the gateway, and a TCP exchange with a test peer, all through the virtual network, with no real network access.

**Acceptance Scenarios**:

1. **Given** ADTPro's Ethernet client configured for slot 3, **When** it starts, **Then** its card probe and initialization succeed and its main menu appears.
2. **Given** an ADTPro server on the PC, **When** the client sends a directory request, **Then** the server's reply arrives and the client lists the server's files.

---

### User Story 4 - Card settings (Priority: P2)

A user sets the card's MAC address, whether the LAN can connect to the guest, and any port forwards, and those values are in effect the next time the machine starts.

**Why this priority**: Guest network software is configured with a MAC address during setup, which requires a stable value to type in, and a guest server is unusable from outside without a port forward.

**Independent Test**: Change the MAC address and add a TCP port forward in settings, restart, and confirm the MAC value persists and a connection to the forwarded host port arrives at the guest.

**Acceptance Scenarios**:

1. **Given** a fresh install, **When** the user first enables the card, **Then** it has a MAC address in the locally administered range that stays the same across restarts.
2. **Given** a MAC address the user typed, **When** it is malformed or a multicast address, **Then** an error in the fixed error format appears in the settings sheet and the old value remains in effect.

---

### Edge Cases

- A guest accesses `$C0n0`-`$C0n3`: the card does not decode them, so they have empty-slot behavior.
- A guest writes MR with IND clear: with only two address lines wired, `$C0n4`-`$C0n7` access internal addresses `$0000`-`$0003` directly (MR, GAR0, GAR1, GAR2).
- A software reset (MR bit 7) reinitializes every register to its reset value and closes every socket, and MR reads `$00` afterward; only a machine power cycle resets the address pointer to `$0000`.
- RMSR or TMSR allocate more than 8 KB: sockets past the 8 KB mark get no buffer, as the datasheet specifies, and commands on them do not touch another socket's memory.
- Sn_TX_WR is advanced past Sn_TX_FSR's free space: SEND transmits at most one buffer's worth; the excess never overruns into another socket.
- A guest reads a 16-bit register's two bytes in either order: values are consistent when read high byte first, as the datasheet requires.
- Sn_IR bits are cleared by writing 1; IR's socket bits clear when that socket's Sn_IR becomes `$00`; CONFLICT, UNREACH and PPPoE clear by writing 1 to them.
- The peer sends more than the RX buffer's free space: data stays queued on the host side and is delivered as the guest frees space with RECV, never dropped for TCP.
- A UDP datagram larger than the RX buffer's free space: it waits until it fits, and a datagram larger than the whole buffer is discarded.
- A guest LISTENs on a port the PC is already using: the socket closes with TIMEOUT rather than appearing to listen.
- The guest opens MACRAW on socket 1-3, or PPPoE anywhere: the socket stays SOCK_CLOSED, since MACRAW and PPPoE exist only on socket 0 and PPPoE is not emulated.
- IPRAW sockets: no host mapping exists without raw-socket privileges, so after OPEN the socket reads SOCK_CLOSED.
- The machine is reset with Ctrl-Reset: the card does not see the Apple II RESET line (an assumption below), so its sockets stay open; a power cycle closes them.
- The emulator is paused or running in fast-forward: host connections stay open while paused, and received data queues on the host side.
- The PC has no network: hardware socket operations to remote addresses fail with TIMEOUT; loopback works.

## Requirements *(mandatory)*

### Functional Requirements

#### Card and bus

- **FR-001**: The Uthernet II MUST be available on the Apple ][, the ][+, the //e and the enhanced //e, in any slot from 1 to 7 not already occupied, and MUST NOT be offered on the //c.
- **FR-002**: The card MUST respond at `$C0n4`-`$C0n7` as MR, IDM_AR0, IDM_AR1 and IDM_DR; `$C0n0`-`$C0n3` and the slot's `$Cn00` ROM space MUST remain undecoded.
- **FR-003**: Reads of IDM_AR0 and IDM_AR1 MUST return the current address pointer; reads of MR MUST return the mode byte.
- **FR-004**: With MR's AI bit set, each IDM_DR read or write MUST advance the address pointer by one, except that advancing from an address whose bit 14 is set and whose low 13 bits are all 1 MUST return to the start of that 8 KB region (`$5FFF` to `$4000`, `$7FFF` to `$6000`, `$FFFF` to `$E000`).
- **FR-005**: An address pointer at `$8000` or above MUST access the internal location 32 KB lower.
- **FR-006**: Writing MR with bit 7 set MUST reset every common and socket register to its datasheet reset value, and close every socket, releasing its host resources; MR MUST read `$00` afterward and the address pointer MUST NOT change.
- **FR-007**: The card MUST assert the Apple II IRQ line while any bit of IR is set whose IMR bit is also set, and release it when none is.

#### Common registers

- **FR-008**: The common registers MUST be readable and writable at their datasheet addresses with their datasheet reset values: MR `$00`, GAR, SUBR, SHAR and SIPR all `$00`, IR `$00`, IMR `$00`, RTR `$07D0`, RCR `$08`, RMSR `$55`, TMSR `$55`, PATR `$0000`, PTIMER `$28`, PMAGIC `$00`, UIPR and UPORT `$00`.
- **FR-009**: RMSR and TMSR MUST allocate each 8 KB buffer to sockets 0 through 3 in order, at 1, 2, 4 or 8 KB per socket, with a socket past the 8 KB mark receiving no buffer.
- **FR-010**: IR's socket bits MUST track whether each socket's Sn_IR is non-zero; CONFLICT, UNREACH and PPPoE MUST clear when written with 1. UNREACH, UIPR and UPORT MUST be set when the host reports a UDP destination unreachable, where the host reports one.
- **FR-011**: Reserved and unimplemented locations below `$4000` MUST read as `$00` and ignore writes.

#### Sockets

- **FR-012**: Each of the four sockets MUST implement Sn_MR, Sn_CR, Sn_IR, Sn_SR, Sn_PORT, Sn_DHAR (reset `$FF`), Sn_DIPR, Sn_DPORT, Sn_MSSR, Sn_PROTO, Sn_TOS, Sn_TTL (reset `$80`), Sn_TX_FSR, Sn_TX_RD, Sn_TX_WR, Sn_RX_RSR and Sn_RX_RD at their datasheet offsets.
- **FR-013**: Sn_CR MUST accept OPEN, LISTEN, CONNECT, DISCON, CLOSE, SEND, SEND_MAC, SEND_KEEP and RECV, and MUST read `$00` once the command has been accepted.
- **FR-014**: Sn_SR MUST follow the datasheet's status values and transitions for TCP (CLOSED, INIT, LISTEN, ESTABLISHED, CLOSE_WAIT and the transitional states) and UDP (CLOSED, UDP).
- **FR-015**: SEND MUST transmit the bytes between Sn_TX_RD and Sn_TX_WR, reading the socket's TX buffer modulo its size, then set Sn_TX_RD to Sn_TX_WR, restore Sn_TX_FSR and set SEND_OK.
- **FR-016**: Received TCP data MUST be written to the socket's RX buffer at its write position modulo the buffer size; Sn_RX_RSR MUST contain the unread count; RECV MUST accept the new Sn_RX_RD and recompute Sn_RX_RSR.
- **FR-017**: Received UDP datagrams MUST be preceded in the RX buffer by an 8-byte header: peer IP (4 bytes), peer port (2 bytes), data length (2 bytes), big-endian.
- **FR-018**: Sn_IR's CON, DISCON, RECV, TIMEOUT and SEND_OK bits MUST be set on the events the datasheet specifies and clear when written with 1.
- **FR-019**: TCP and UDP sockets MUST map onto host sockets: CONNECT to the destination in Sn_DIPR and Sn_DPORT, LISTEN on Sn_PORT, UDP bound to Sn_PORT and sending to Sn_DIPR and Sn_DPORT.
- **FR-019a**: Unless the card's LAN access setting is on, incoming TCP connections (a hardware LISTEN, or a virtual-network inbound port) MUST be accepted only from this PC, and incoming UDP datagrams MUST be delivered only if they come from this PC or from an address and port the socket has sent to. Outgoing traffic to any address MUST work regardless of the setting.
- **FR-020**: All host socket access MUST go through an interface that unit tests replace with a fake, so no unit test opens a real socket.
- **FR-021**: Host socket work MUST NOT block the emulation thread; a slow connect or a full host send buffer MUST appear to the guest as a socket state or a free-size value, never as a stalled CPU.
- **FR-022**: Guest IP, gateway and subnet register values MUST NOT affect where hardware TCP and UDP sockets connect.
- **FR-023**: UDP that a hardware socket sends to port 67 MUST be delivered to the virtual network's DHCP server instead of the PC's network, and its replies MUST arrive in that socket's RX buffer as UDP datagrams from the DHCP server's address and port 67. Other guest UDP broadcasts MUST be sent as broadcasts on the PC.

#### MACRAW

- **FR-024**: OPEN in MACRAW mode on socket 0 MUST connect it to a virtual network inside Casso; OPEN in MACRAW on sockets 1-3 MUST result in SOCK_CLOSED. Frames the guest SENDs MUST be delivered to the virtual network, and frames the virtual network produces MUST appear in socket 0's RX buffer preceded by the datasheet's 2-byte length header. With Sn_MR's MAC filter bit set, only frames addressed to SHAR or to broadcast MUST be delivered.
- **FR-024a**: The virtual network MUST present a gateway with its own IPv4 and MAC address, and MUST send an ARP reply to each guest ARP request for the gateway, the DNS address or any off-subnet destination, in the next frame the guest receives.
- **FR-024b**: The virtual network MUST include a DHCP server that offers the guest a fixed address, subnet mask, gateway and DNS server, so IP65's DHCP client succeeds.
- **FR-024c**: ICMP echo requests addressed to the gateway MUST produce ICMP echo replies.
- **FR-024d**: UDP datagrams from the guest MUST be relayed through host UDP sockets to their destination, and replies MUST be returned to the guest's source port as UDP frames from the original destination's address and port.
- **FR-024e**: TCP connections the guest opens MUST be relayed through host TCP connections: the guest's SYN produces a host connect, the connect result produces a SYN-ACK or a reset, data flows in both directions within the guest's advertised window and its MSS, and FIN and RST map onto host shutdown and close.
- **FR-024f**: DNS queries the guest sends to the virtual DNS address MUST be relayed to the PC's configured IPv4 DNS server, and the server's response MUST be returned to the guest as if it came from the virtual DNS address.
- **FR-024g**: Each configured port forward MUST accept connections (TCP) or datagrams (UDP) on its host port and relay them to the guest's DHCP-assigned address at its guest port, subject to FR-019a.
- **FR-024h**: UDP and TCP traffic the guest sends to the gateway's address MUST be relayed to the PC's loopback address, so programs on the same PC (an ADTPro server, the Casso Explorer link) are reachable without a LAN.

#### Settings and configuration

- **FR-025**: The card's presence, slot and settings MUST be stored in the machine configuration and survive restarts; an existing configuration from before this feature MUST load with the card absent.
- **FR-026**: The settings UI MUST let the user enable or disable the card and choose its slot among the free ones, following the settings sheet's Apply/Cancel model.
- **FR-027**: The card MUST have a user-editable MAC address, defaulting to a randomly generated, locally administered unicast address generated once and stored. It is the card's label value for entry into guest software; it MUST NOT be written into SHAR, which resets to `$00`.
- **FR-027a**: The card MUST have a LAN access setting, off by default, that controls FR-019a.
- **FR-027b**: The card settings MUST include an editable list of port forwards (protocol, host port, guest port), empty by default; a host port already in use MUST produce a visible error when the machine starts, not a silent failure.
- **FR-028**: An invalid MAC address MUST produce an error in the fixed error format, and the stored value MUST stay in effect.

#### Clean room

- **FR-029**: The emulation MUST be derived only from the WIZnet W5100 datasheet and the Uthernet II's published documentation (its manual and the a2retrosystems/uthernet2 wiki), never from another emulator's source.

### Key Entities

- **Uthernet II card**: a slot device with a slot number, a MAC address setting and a W5100.
- **W5100**: the mode byte, the address pointer, 32 KB of internal space (common registers, socket registers, TX and RX buffers), the interrupt state, and four sockets.
- **Socket**: mode, command, status, interrupt bits, source and destination address and port, TX and RX pointers into its buffer share, and an optional host connection.
- **Virtual network**: the gateway and DNS addresses, the DHCP lease, and a NAT table of relayed UDP flows and TCP connections, each linking a guest address and port to a host socket.
- **Port forward**: a protocol (TCP or UDP), a host port and a guest port, stored with the card's settings.
- **Host socket seam**: the PC-side operations a socket requires (open, bind, connect, listen, accept, send, receive, close, poll for readiness), implemented with Windows sockets in the product and faked in tests.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The card manual's own probe (write `$80`, read `$00`, write `$03`, read `$03`) and the RTR probe both find the card in its configured slot and in no other slot.
- **SC-002**: A 6502 program assembled with Casso's assembler opens a hardware UDP socket on an emulated //e and completes a round trip of at least 1 KB with a test peer, byte for byte.
- **SC-003**: SC-002 applies equally to a hardware TCP socket, in both directions, across a transfer larger than the socket's buffer share, proving wrap at the buffer boundary.
- **SC-004**: Every register's reset value and every documented command transition in the datasheet has a unit test, run against a fake host socket layer with no real network access.
- **SC-005**: With the card idle, emulation speed at 1x stays within 1% of the same machine without the card.
- **SC-006**: Shipped ADTPro's Ethernet client transfers a disk image to and from an ADTPro server on the same PC, through the virtual network.
- **SC-007**: On an emulated //e, frames built by the test and sent through a 6502 MACRAW frame pump on socket 0 obtain the guest address by DHCP, resolve the gateway by ARP, and complete a TCP round trip of at least 1 KB with a test peer through the virtual network, byte for byte.

## Assumptions

- The original Apple ][ is included alongside the requested ][+ and //e: its slots are identical, and the card manual reports its probe tested on an Integer BASIC Apple II.
- The card ships disabled; when first enabled its default slot is 3, the first slot in the card manual's probe order and the slot in the card's demo code.
- The card does not connect to the Apple II RESET line, so Ctrl-Reset has no effect on the W5100 and only a power cycle or MR's reset bit reinitializes it. The manual's instruction to reset the chip with `$80` at program start supports this.
- The W5100's interrupt output is wired to the Apple II IRQ line, per the interrupt chapter of the card manual.
- Retransmission timing (RTR, RCR) has no PC equivalent; host socket failures map to TIMEOUT whatever those registers contain, and the registers read back what was written.
- IPRAW, PPPoE, IGMP multicast and the "virtual W5100" DNS-offload extension published on the card's wiki are out of scope. The extension also contradicts the real chip's PTIMER reset value, which software reads to distinguish a real chip from the extension.
- The virtual network uses one fixed private IPv4 subnet, chosen in planning, so the guest's DHCP address, gateway and DNS addresses are the same on every PC.
- The original Uthernet (CS8900A) and the Apple IIgs are out of scope.
- Clean-room sources: WIZnet W5100 datasheet v1.1.8; the Uthernet II manual (draft); the a2retrosystems/uthernet2 wiki (CC BY 3.0). ADTPro and IP65 sources were read only to determine which W5100 mode their drivers select.
