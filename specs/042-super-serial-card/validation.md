# Validation: Apple's firmware (T032)

2026-10-18, on a //e and a ][+, with Apple's Super Serial Card firmware 341-0065-A
(2,048 bytes, SHA-1 `6DAB633470C6BC4CB3E81D09FDA46597CAF8EE57`) from the
mirror in the asset catalog. Its $Cn05, $Cn07, $Cn0B and $Cn0C bytes read
$38, $18, $01 and $31, the Pascal 1.1 serial card ID.

## Automated with the real firmware

`ScenarioTests/GuestVisibleSuperSerialCardTests.cpp` boots the DOS 3.3 master
on a //e or ][+ whose slot 2 entry turns the card on with this firmware, sets its
switches and attaches its connector, so `MachineBuilder` loads and wires them
as it does for Casso. Run with `scripts\RunTests.ps1 -Build -Scenario -Filter
GuestVisibleSuperSerialCardTests`.

| Case | Quickstart step | Result |
|---|---|---|
| `PR#2`, `LIST`, `PR#0` in printer mode, 40 characters with the screen on, file out; //e and ][+ | 2, 3 | Pass: the listing is in the file and on the screen |
| The same at the printer settings the card ships with: 80 characters, screen off, 1/4 second after each carriage return | 3 | Pass: the listing is in the file, after the delays, and not on the screen |
| `IN#2` in communications mode with the loopback plug, then `PRINT 6*7`; //e and ][+ | 4 | Pass: the typed line comes back through the plug and prints 42 |
| `IN#2` in communications mode at 300 baud, file in: two program lines and `IN#0` | none | Pass: the program is entered from the file and runs |
| The firmware's commands from Applesoft: Ctrl+I in printer mode and Ctrl+A in communications mode, then `6B`, `1D` and `3P`; //e and ][+ | none | Pass: 300 baud, 7 data bits and even parity are written to the 6551 |

## Findings

- While it sends a character, the firmware reads the keyboard and discards a
  key pressed there ($CC34: `LDA $C000`, `BPL`, `STA $C010`), so Applesoft
  never receives a key typed within about a millisecond of the previous one,
  as on the card. The scenario cases type with a pause after each key.
- At 9,600 baud with no handshake, characters of a file sent into `IN#2` are
  dropped while Applesoft stores each line: one arrives every 1,070 cycles,
  and the 6551's single receive register overruns. At 300 baud every
  character arrives. On the real card too, a program sent into BASIC requires
  a slow line or a handshake. The //c's own ports, run by Apple's //c
  firmware, drop characters the same way: its modem port received the
  program intact at its default 300 baud and garbled at 9,600, and its printer
  port at 9,600 dropped more. This was checked once; no test for it is committed.
- On an overrun, the newest character replaces the unread one in the 6551's
  data register. In WDC's W65C51N datasheet, an overrun is flagged when the
  next byte is transferred to the receive data register before the previous
  one is read.
- In printer mode the firmware echoes output to the screen only at the
  40-character line width. Apple's 1985 manual (page 15) adds "or up to 80
  characters if you have an 80-column card installed", but ROM 341-0065-A
  has no such case: its initialization looks up the width from SW2-3 and
  SW2-4 in the table at $C9A6 (41, 72, 80, 132) and clears the echo bit (bit
  7 of $0738+slot) for every width but the first ($C85E `BEQ`, $C861 `AND
  #$7F`), and it never reads the //e's 80-column status. On a //e with the
  80-column display on (PR#3), with and without DOS 3.3, a listing at 80
  characters went to the connector and not to the screen. Settings labels
  only the 40-character choice "echoed to the screen".

## Real programs

Driven headlessly through a scratch scenario harness that types keys, injects
bytes on the line and saves frames as images; not committed. All on the
Enhanced //e unless stated.

| Program | Setup | Result |
|---|---|---|
| Kermit-65 3.87 (ProDOS) | `KERMIT.INSTALL` driver 1, "Super Serial Card or Apple IIc"; `KERMIT.SETUP` at 9,600 baud; loopback plug | Pass on the //e with SW2-6 on: `CONNECT`, and a typed line comes back through the plug. Its driver is interrupt-driven, so with SW2-6 off it stalls with the receive interrupt pending, as on the card. On the Enhanced //e, Kermit breaks to the monitor at $C404 after reading its command file, and on the //c it echoes NUL characters at its prompt, with or without the card; both have a 65C02 |
| ProTERM 2.2 | Apple Super Serial Card in slot 2, Apple Modem 1200; modem replies injected | Pass: the init strings are sent, the `0` reply is accepted, and on-line a typed line is sent and an injected line is displayed. With the loopback plug and no reply, it reports that it cannot initialize the modem |
| ProTERM 3.1 | Apple Super Serial Card in slot 2, Apple Modem 1200 and the null modem driver | Pass: `ATX1` is sent at 1,200 baud, the injected `OK` is accepted, and with the null modem driver at 9,600 baud an injected line is displayed. The images are ProDOS order despite the `.DSK` name |
| ADTPro 2.1.0 client | Serial connection | Pass as far as it can go without a host: it finds "SSC SLOT 2" by itself at 115,200 baud (baud code 0), sends its `DIR` request and reports a host timeout. File transfers start working once spec 033 adds the host connection |
| Apple Pascal 1.2 | Filer: transfer `CONSOLE:` to `REMOUT:` | Pass: the typed line goes out through the firmware's Pascal entry points, followed by a line feed and the rest of the 512-byte block as NULs |
| The Print Shop Color | Setup: ImageWriter, Apple Super Serial in slot 2; printer mode at 9,600 baud, file out | Pass: the printer test's ImageWriter stream (701 bytes) and a full sign (about 134 KB) both render correctly through Casso's ImageWriter engine |

**A file transfer between two emulated //e's.** Two machines in one test, each with the card in slot 2, joined by a null-modem cable endpoint: each side's transmitted bytes go to the other side's receive line, RTS loops back to the same side's CTS, and DTR goes to the other side's DSR and DCD. Kermit-65 on one sent `KERMIT.HELP` at 9,600 baud to Kermit-65 on the other, whose disk did not have it. The received file, saved as `KERMIT.HELP.00`, matches the original byte for byte (1,100 bytes). With RTS crossed to the other side's CTS instead, the transfer stalls: Kermit turns RTS off when it is not sending.

ProTERM times its delays with the //e's vertical-blank flag at $C019. A
`TestMachine` has no video timing unless the test attaches one, so $C019 reads
0 and ProTERM waits indefinitely; with one attached, both versions run. No
committed test depends on it: the unit and scenario suites have the same
results with a video timing attached to every test machine.

## Through the UI

Driven in Casso itself on a //e: one instance launched without activation,
posted clicks and keys, PrintWindow captures, with `UserPrefs.json`, the
machine's config and autosave, and `SSC.rom` backed up and restored.

| Quickstart step | Result |
|---|---|
| 1. Turn the card on under Settings > Machine with the firmware missing | Pass: the checkbox stays off, the download is offered, Yes downloads `SSC.rom` (the same SHA-1), turns the card on and opens the Serial card tab |
| 2. Printer mode, 40 characters with the screen on, file out with a path | Pass: the page's settings and the SW1 and SW2 readout follow each change; the path is chosen in the Save As dialog |
| 3. `PR#2`, `LIST`, `PR#0` after OK (reboot) | Pass after the fix below: the listing is in the chosen file |
| 5. The infobar after `SSC.rom` is removed | Pass: it reports the missing firmware with Download and Dismiss; Download installs it and offers Restart, which rebuilds the machine and clears it |

Two bugs found and fixed (`fix(ssc): Save the Super Serial Card's switches
and connector when Settings applies`):

- On OK, the sheet re-reads the machine's prefs from disk, and the refresh
  replaced every card's staged connector with the one on disk, which
  discarded the connector chosen on the Serial card page. A connector changed
  in the sheet is now left as staged.
- For a slot, the saved difference and the merge on load included only
  `enabled` and `ports`, so the switches were never saved. Both now include
  `switches` too.

The download prompt's text overruns its 500 by 230 dialog: it starts under
the title bar and runs into the buttons.
## Not yet checked

- Step 4 through the UI; the scenario tests cover it through the same build
  path.
- Step 6: the //c's paced serial printing.
- The ][: GuestSession recognizes only the ] prompt, and the ][ boots to
  Integer BASIC.
- ADTPro with its host, which arrives with the host connection in spec 033.
