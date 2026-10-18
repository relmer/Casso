# Validation: Apple's firmware (T032)

2026-10-18, on a //e, with Apple's Super Serial Card firmware 341-0065-A
(2,048 bytes, SHA-1 `6DAB633470C6BC4CB3E81D09FDA46597CAF8EE57`) from the
mirror in the asset catalog. Its $Cn05, $Cn07, $Cn0B and $Cn0C bytes read
$38, $18, $01 and $31, the Pascal 1.1 serial card ID.

## Automated with the real firmware

`ScenarioTests/GuestVisibleSuperSerialCardTests.cpp` boots the DOS 3.3 master
on a //e whose slot 2 entry turns the card on with this firmware, sets its
switches and attaches its connector, so `MachineBuilder` loads and wires them
as it does for Casso. Run with `scripts\RunTests.ps1 -Build -Scenario -Filter
GuestVisibleSuperSerialCardTests`.

| Case | Quickstart step | Result |
|---|---|---|
| `PR#2`, `LIST`, `PR#0` in printer mode, 40 characters with the screen on, file out | 2, 3 | Pass: the listing is in the file and on the screen |
| The same at the printer settings the card ships with: 80 characters, screen off, 1/4 second after each carriage return | 3 | Pass: the listing is in the file, after the delays, and not on the screen |
| `IN#2` in communications mode with the loopback plug, then `PRINT 6*7` | 4 | Pass: the typed line comes back through the plug and prints 42 |
| `IN#2` in communications mode at 300 baud, file in: two program lines and `IN#0` | none | Pass: the program is entered from the file and runs |

## Findings

- While it sends a character, the firmware reads the keyboard and discards a
  key pressed there ($CC34: `LDA $C000`, `BPL`, `STA $C010`), so Applesoft
  never receives a key typed within about a millisecond of the previous one,
  as on the card. The scenario cases type with a pause after each key.
- At 9,600 baud with no handshake, characters of a file sent into `IN#2` are
  dropped while Applesoft stores each line: one arrives every 1,070 cycles,
  and the 6551's single receive register overruns. At 300 baud every
  character arrives. On the real card too, a program sent into BASIC requires
  a slow line or a handshake.

## Not yet checked

- Steps 1 and 2 through the UI: turning the card on under Settings > Machine
  with the firmware missing, accepting the download, and setting the switches
  and attachment on the Serial card page.
- Steps 5 and 6: the missing-firmware infobar after `SSC.rom` is removed, and
  the //c's paced serial printing.
- The ][+: the scenario helpers boot only the //e.
