# Quickstart: validating serial connectivity

## Automated

```powershell
scripts\RunTests.ps1 -Build -Filter Serial      # the inner loop
scripts\RunTests.ps1 -Build                     # the full unit suite
scripts\RunTests.ps1 -Build -Scenario           # the scenario suite
scripts\CheckStyle.ps1 -Mode Tree
```

The unit tests use `FakeHostNetwork` and `FakeHostSerialPorts`; nothing opens
a real socket or COM port.

## By hand, with the owner's hardware

Each run launches `Casso.exe --title 047-serial-connectivity`.

1. **Modem to a BBS (SC-001).** Settings > Serial card: Modem, Telnet on.
   Boot a terminal program (ProTERM, or the card's terminal mode: `IN#2`,
   `PR#2`, then Ctrl-A T). Type
   `ATDT bbs.example.org:23` (any Telnet BBS). The result is `CONNECT`
   and the BBS's screens. After `+++` with a second's pause either side, the
   result is `OK`; `ATH` then returns `OK`, and a remote hang-up returns
   `NO CARRIER`.
2. **Incoming call.** Modem listen port 6400. From the PC:
   `telnet localhost 6400`. `RING` appears on the Apple; `ATA` returns `CONNECT`.
3. **TCP.** Settings: TCP, listen, port 1977. From the PC:
   `Test-NetConnection` or `telnet localhost 1977`. Characters typed on either
   side appear on the other; disconnecting turns DCD off in the debugger's
   Serial log.
4. **COM port (SC-002, SC-003).** Two USB serial adapters joined by a null
   modem cable. Attach the Apple to one; open the other in a PC terminal at the
   same rate. Check data both ways at 300 through 19,200, then set the PC to a
   different rate and look for framing errors in the Serial log. Unplug the
   adapter: an infobar, and DCD off.
5. **History (SC-005).** During any session above, open the debugger, step
   back and seek forward through the Serial log; the remote end receives
   nothing. Resume from a point in the past: the connection stays up, and
   bytes from the remote end arrive again. Load a state file: `NO CARRIER`,
   and the connection closes on the PC side.
