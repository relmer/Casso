# Quickstart: Validating the Uthernet II

## Automated

```powershell
scripts\FetchRoms.ps1 -Fixtures          # only if UnitTest\Fixtures lacks the ROMs
scripts\RunTests.ps1 -Build -Filter Uthernet
scripts\RunTests.ps1 -Build -Filter W5100
scripts\RunTests.ps1 -Build -Filter VirtualNetwork
scripts\RunTests.ps1 -Build -Scenario    # includes the real-Winsock loopback round trip
```

Result: no failures. A filtered run prints the filtered-run banner, so run the full suite before merging.

What they cover: [contracts/w5100-guest-interface.md](contracts/w5100-guest-interface.md) register by register, the bridge and the virtual network against `FakeHostNetwork` ([contracts/host-network-seam.md](contracts/host-network-seam.md)), configuration per [contracts/machine-config.md](contracts/machine-config.md), and three 6502 programs on a //e `TestMachine`.

## In Casso

1. Launch a fresh build, open Settings > Network on an Apple //e, turn on Uthernet II with slot 3, click OK. The machine restarts.
2. In the Monitor (`CALL -151`): `C0B4:80`, `C0B4:03`, then `C0B4` prints `C0B4- 03` (manual's card test). `C0C4` (slot 4) does not.
3. Note the MAC address on the Network page; it stays the same after closing and reopening Casso.
4. Turn on "Allow connections from other computers", click OK. The Windows Firewall prompt appears the first time a guest program listens.

## ADTPro (owner validation, SC-006)

Requires ADTPro's server and its Ethernet client disk, supplied by the owner; the agent does not download them.

1. Start the ADTPro server on the PC with Ethernet (UDP) selected.
2. In Casso, boot ADTPro's Ethernet client on the //e with the card enabled in slot 3.
3. In the client's configuration, set slot 3 and either DHCP or static `10.0.65.15`, and set the server address to `10.0.65.1` (the PC itself, through the virtual gateway).
4. Result: the client lists the server's directory, then sends a disk image to the server and receives one back, byte for byte.

## Performance (SC-005)

At Authentic speed the emulator is throttled to 1.023 MHz with or without an idle card. Unthrottled, measured 2026-10-14 in Release on a //e `TestMachine` running a `JMP *` loop for 50 million cycles per run, 16 runs alternating the order (card first, then card second): 3558.5 ms without the card, 3619.8 ms with it, a cost of about 1.7% of maximum emulation speed. Each run pair is order-sensitive by roughly 10%, so single-order comparisons are not meaningful.
