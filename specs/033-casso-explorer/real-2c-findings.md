# CassoLink on a real //c: findings

The owner's Apple //c, linked over its port 2 to COM3 with ADTPro's null-modem
cable, 2026-10-18 to 2026-10-20. Each finding lists the symptom on the real
machine, the evidence, the cause, the fix, and why the fault did not occur on
Casso's emulated //c. That last part is the input for a review of the emulator once the
real //c works without faults; entries marked *unverified* are hypotheses to
confirm against the emulator's code in that review.

The emulator's //c disk path, for reference: `Disk2Controller` with
`SetIwmMode (true)` adds the IWM's mode and status registers, and every read
and write goes through `Disk2NibbleEngine`, a model of the Disk II's logic
state sequencer. A track has 51,200 bit cells (`kNominalRevolutionCells`), so
the drive turns at exactly 300 rpm.

## Disk writes

### 1. Absolute stores leave the IWM's disk unwritten

This entry first had the cause reversed, as "indexed stores put a false byte on
the IWM", with `3b869585d` moving the player to absolute stores. That was
wrong; the record of the reversal follows.

- **Symptom:** sector writes failed to verify, and formats left the track
  as it was (item 4).
- **Evidence, 2026-10-20,** each a sector write of a known pattern to T28 S0
  on a disk freshly formatted by DOS's `INIT`:
  - Absolute stores (`sta $C0ED`, the committed `3b869585d`): the sector
    read back the old data; verify error `$40`.
  - Indexed stores (`sta $C08D,x`) with the same cycle counts: the disk
    changed, but the data field did not decode (`$30`), and the next read
    found no address field (`$20`).
  - DOS's RWTS, called from inside CassoLink: written and read back exactly.
  - Indexed stores with every spacing a whole number of bit cells (item 21):
    written and read back exactly; a format then read all 16 sectors.
- **Cause:** documented in Sather, *Understanding the Apple IIe*, in the
  chapter on the disk controller, for the Disk II card: `STA $C08F,X` makes
  a double access to `$C0EF`. The first access starts the logic state
  sequencer's write loop, and the second stores the data. The controller
  accepts data only on the clock pulse after the one that started the
  sequencer, and on every fourth clock pulse after that. Sather warns
  against replacing RWTS's `STA $C08F,X` (at `$B83F` in DOS 3.3) with
  `STA $C0EF`: the absolute store starts the software loop one clock out of
  step with the sequencer, and the controller does not accept the write
  data. `STA $C0EF,X` with X = 0, or `STA (zp),Y` with no page crossing, also
  works.
- **The //c's 65C02 makes the double access too:** the //c Technical
  Reference (2nd edition) lists the NCR 65C02's one change to the extra
  cycle of indexed addressing: across a page boundary, it reads the last
  instruction byte instead of an invalid address. `$C08D + $60` crosses no
  page, so the first access is a read of `$C0ED`, as on the 6502.
- **The ROM note:** the //c ROM listing's comment on a false byte from
  indexed stores is about the IWM's mode register, written with the motor
  off, where the byte on the bus during the first access is taken. It does
  not apply to data.
- **Why sector writes seemed to verify after `3b869585d`:** most likely the
  sectors already had the pattern from an earlier build with indexed stores,
  so the verify compared old data. Every verify since uses a disk freshly
  formatted by DOS.
- **Fix:** `8305fa908`. Indexed stores throughout the player, with item 21's
  timing.
- **Why the emulator missed it** *(unverified)*: the emulated controller loads
  the data register from any store to `Q6H` in write mode, absolute or
  indexed, so both work there. Per Sather this is a Disk II rule, not an IWM
  one, so the gap is in `Disk2NibbleEngine` for every machine.

### 2. The end of a written stream was lost

- **Symptom:** the last sector of a write, or its epilogue, did not read back.
- **Cause:** the //c's IWM buffers one byte: a stored byte waits until the
  byte before it has been shifted out. Turning write mode off right after the
  last store discards the byte in the buffer.
- **Fix:** `3b869585d`. `pdone` writes two self-sync bytes after the last
  nibble, 40 cycles apart, and waits past both before read mode.
- **Why the emulator missed it** *(unverified)*: the LSS model has no buffer
  register. A stored byte goes straight to the shift register, so nothing is
  pending when write mode turns off.

### 3. The drive turns at 293.1 rpm

- **Symptom:** a full-track write's end overwrote the track's first sectors.
- **Evidence:** a revolution measured at 204.68 ms (+/-0.3 ms), timed by
  repeated passes of sector 0's address field.
- **Cause:** a stream laid out for 300 rpm is longer than a slow drive's
  revolution, so the stream overlaps its own start.
- **Fix:** `2db6b37c2`. A 352-byte first gap and 10-byte gaps between
  sectors, so the overlap falls in the first gap. A gap that adapts to the
  drive (a byte shorter after each failed pass, as in DOS's INIT) was built
  and dropped: with 4-byte gaps every fourth sector failed, because the verify
  read fell behind the disk.
- **Why the emulator missed it:** the emulated drive turns at exactly 300 rpm.
  Real drives vary by a few percent, so a configurable drive speed in the
  emulator would cover this.

### 4. Writes stopped changing the disk (fixed)

- **Symptom:** a format and a sector write both return success, or a verify
  error, while the track is left as it was.
- **Evidence, 2026-10-20:**
  - A format that wrote volume 253, and another that wrote 252, left every
    address field on track 28 at volume 254.
  - A sector write of a known pattern to T28 S0 read back the old zero data.
    A raw capture after the address field contained no trace of the write.
  - The same result with the committed program from `3b869585d`, whose sector
    writes verified earlier.
  - The format stream built in the //c's memory, sent to the PC byte for byte,
    matches the stream computed on the PC exactly.
  - The write-protect sense reads "not protected" throughout.
- **Physical cause ruled out, 2026-10-20:** on the same disk in the same
  drive, DOS 3.3's `INIT HELLO` succeeded and the disk then booted without
  an error. Before that `INIT`, `CATALOG` worked but booting ended in
  `I/O ERROR` with the head chattering: DOS loaded and its catalog on track
  17 read, but HELLO's sectors, just past the catalog track and inside the
  range CassoLink's experiments had overwritten, did not. So CassoLink's
  writes changed this disk at some point, and then stopped changing it.
- **Cause:** items 1 and 21. `3b869585d`'s absolute stores wrote nothing;
  before it, indexed stores with partial-cell spacing wrote nibbles that did
  not read. The damage on the disk dates from those earlier builds.
- **Fix:** `8305fa908`.
- **Why the emulator missed it:** see items 1 and 21.

### 5. Damage attributed to the format was on the old track

- **Symptom:** about one sector per track failed to read after a format, always the
  same sectors (on T28: S2, S5, S6, S9, S10, S12, S14).
- **Evidence:** raw captures had the damage at the same byte of the same
  sectors after shifting the whole stream by 96 bytes and after delaying the
  write's start by 1, 51, 101 and 151 ms. Item 4 is the reason: none of those
  formats wrote the track.
- **Cause:** a stale track, from DOS's INIT and earlier experiments, read
  back after formats that wrote nothing. A raw capture after one more
  format had a short run of self-sync bytes inside two otherwise intact data
  fields: a format with indexed stores and partial-cell timing (item 21)
  wrote a few bytes at a random place, then nothing that read. Each such
  format damaged one more sector, which is why the failing set grew.
- **Fix:** see item 6, so that a format that writes nothing is reported as an
  error.
- **Why the emulator missed it:** not an emulator gap. The emulated drive
  always writes, so nothing exposed the missing verify.

### 6. A format does not read back what it wrote

- **Symptom:** a format reports success when nothing was written.
- **Cause:** `doformat` writes the stream and returns without reading the
  track. Write track reads back and compares; format does not.
- **Fix:** `doformat` reads all 16 sectors back after writing (`readall`,
  shared with Write track's verify) and ends with `ERRNOTFND` and the first
  sector that does not read.
- **Why the emulator missed it:** see item 5.

### 21. Stores partway through a bit cell

- **Symptom:** with indexed stores, a sector write changed the disk but the
  data field did not decode. A raw capture had the last self-sync bytes out
  of frame (`9F F3 FD`) and the data prologue one bit off (`AB AA B7` for
  `D5 AA AD`). The same sector written by RWTS read `FF FF FF FF FF D5 AA AD`.
- **Cause:** the player's spacing from one store to the next was 32 cycles
  between data bytes and 40 from a data byte to a sync byte, but 43 between
  sync bytes, 35 from a sync byte to a data byte, and 3 more at a page
  crossing. A bit cell is 4 cycles. A store at 43 or 35 cycles lands partway
  through a cell, and on the IWM the bits after it are shifted. In RWTS,
  each spacing is a multiple of 4.
- **Fix:** `8305fa908`. 32 cycles between data bytes, 40 to and between sync
  bytes, 32 from a sync byte to a data byte, and 8 more at a page crossing:
  two whole cells, which put two zero bits between nibbles.
- **Why the emulator missed it** *(unverified)*: the emulated logic state
  sequencer loads a byte at the store wherever it falls. Sather documents
  that the Disk II controller accepts data only on every fourth clock pulse
  after the write loop starts (item 1), so this timing fails on a real Disk II
  card too; the gap is in `Disk2NibbleEngine` for every machine.

## Bootstrap and link

### 7. Typed bootstrap characters were dropped

- **Symptom:** the loader typed in over `IN#2` arrived with missing
  characters; the monitor listing had the last byte at `$3F78` short of the end.
- **Cause:** with DOS's input hook active, the //c takes longer per character
  than the 9,600-baud line delivers them, and there is no flow control.
- **Fix:** `3b869585d`. `LinkBootstrap::Send` sends one character at a time
  with a 3,000 us pause (`kCharPauseUs`).
- **Why the emulator missed it** *(unverified)*: the emulated port 2 either
  delivers each byte only when the guest reads the previous one, or runs the
  guest fast enough that its input loop always reads each byte in time. A real 6551 has
  a single-byte receive register and overruns.

### 8. The cable's control lines glitch

- **Symptom:** the host reported the //c disconnected and reconnected many times a second.
- **Evidence:** CTS, DSR and DCD drop for about 1 ms many times a second on
  ADTPro's cable.
- **Fix:** `IsPeerGone` requires DSR off for 500 ms.
- **Why the emulator missed it:** the emulated serial link has no electrical
  noise. A test that injects control-line glitches would cover it.

### 9. The shim sends Ready once, then waits

- **Symptom:** a PC that connects while the //c sits at the shim gets no
  reply and times out.
- **Cause:** the shim sends one Ready and then waits for a packet with no
  timeout. A PC that connects after that Ready waits for a Ready that never
  comes.
- **Workaround:** send any packet first (an Idle): the shim fails to match it
  and sends Ready again.
- **Fix:** pending. Confirm that Casso Explorer's connect sends first, or have
  the shim repeat Ready while it waits.
- **Why the emulator missed it** *(unverified)*: in emulated runs the host is
  usually listening before the shim starts, so the single Ready arrives.

### 10. The program's speed after a reload

- **Symptom:** after a program update the shim reloaded at a different speed
  from the link's.
- **Fix:** `db39947d5`. The program's `doload` makes the shim reload at the
  link's current speed.
- **Why the emulator missed it** *(unverified)*: an emulated link at a fixed
  speed hides any speed mismatch between the two ends.

### 11. Drive 2 reported present with nothing attached

- **Symptom:** drive 2 listed as present with no external drive.
- **Evidence:** with no drive 2, its read latch reads a constant `$80`.
- **Fix:** `db39947d5`, `e122a0d53`. The probe counts changes in the latch.
- **Why the emulator missed it** *(unverified)*: the emulator's absent drive
  returns a latch value that the old probe already treated as no drive.

### 12. The drive motor stayed on after the PC stopped

- **Symptom:** the drive motor ran on after the PC went quiet.
- **Fix:** `f648e1aed`. The motor stops after a second with no reply from the
  PC.
- **Why the emulator missed it:** an emulated motor left on has no visible or
  audible cost unless the drive sound is enabled.

### 20. Leaving CassoLink breaks BASIC

- **Symptom:** after Control-Reset out of CassoLink, `LIST` printed a screen
  of `< = > ? [ \ ] ^ _ | ~`, and `NEW` beeped with an error message in the
  same characters. The `]` prompt and the typed command looked normal.
- **Cause:** every character BASIC printed was ORed with `$1C`. Applesoft
  ORs each character it prints with `FLASHBIT` at `$F3`; CassoLink stores an
  address field's track at `$F3` (`atrk`), and the last operation was on
  track 28, `$1C`. CassoLink also uses `$06`-`$23` and `$E0`-`$FF`, much of
  which Applesoft and the monitor use, and does not restore it. Its program
  at `$0800` also overlaps Applesoft's program area, so `LIST` decodes
  machine code as BASIC.
- **Workaround:** boot again (`PR#6`), or `POKE 243,0` for readable text.
- **Fix:** pending, after item 4. One cleanup routine for both ways out:
  - **Clean exit:** an Exit command from the PC, and Escape on the Apple.
  - **Control-Reset:** on entry, save the reset vector at `$3F2`/`$3F3`,
    point it at the cleanup routine and set the power-up byte at `$3F4` to
    match. Control-OpenApple-Reset reboots the //c as a last resort.
  - **The routine:** stop the motor and turn off DTR; restore the zero page
    saved on entry; clear Applesoft's program area as `NEW` does; set the
    input and output hooks to the keyboard and screen and have DOS connect
    its hooks again; put the original reset vector back; return to DOS's
    warm start. When DOS is missing or damaged, boot from the disk instead.
  - Move the program's variables to zero page locations that Applesoft, DOS
    and the monitor do not use, to reduce damage from a reset in the middle
    of a disk operation.
  - **Test:** exit both ways on the //c, then `LIST`, `NEW` and `CATALOG`.
- **Why the emulator missed it:** not an emulator gap. Emulated sessions end
  by resetting the machine or closing Casso, never by returning to BASIC.

### 22. The shim's program load can stop and not recover

- **Symptom:** the shim's screen stopped at `LOADING PROGRAM 61%`, with the
  PC's host waiting for the page's ACK. An Idle from the PC afterwards did
  not get a Ready back, and the //c was reset and bootstrapped again. An
  earlier load the same day (version 30) also timed out.
- **Evidence, 2026-10-20:** COM3 had no traffic from the //c after the
  host stopped. The host waits up to `kOperationTimeoutMs` (30 s) for each
  page's ACK and sends a page up to 6 times.
- **Cause:** most likely a byte lost on the line (see the logged load
  below). Contributing: the shim waits for each byte of a page with no
  timeout, so after a page cut short it waits for good, and
  it sends Ready again only after an envelope it cannot use.
- **Logged load, 2026-10-20:** a successful load after a fresh bootstrap had
  15 pages, each ACKed within about 85 ms. In reply to a 5-byte Idle, the stuck
  shim sent NAK: it was waiting for the last few bytes of a page, so
  bytes had been lost. Not an overrun: the shim's per-byte loop takes about
  35 of the 89 cycles a byte allows, and interrupts are off (`sei` at
  `$3D00`). Most likely a byte lost on the line: noise on a start bit can
  cost the 6551 its framing with bytes sent back to back.
- **Fix:** `getbto` in the shim, a one-second limit on each byte of a page;
  a timeout sends NAK, the host sends the page again, and the shim takes it
  from its envelope. Shim version 3. Not yet tested on the //c.
- **Why the emulator missed it:** no byte is ever dropped on the emulated
  serial link. The noisy test stream under "Follow-up ideas" would cover it.

### 23. The Apple's "no reply" timer stops at 255 and the PC stays connected

- **Symptom:** the program's status screen counted the seconds since the
  PC's last reply up to 255 and stopped there, and it went on showing the PC
  as connected.
- **Cause:** the count is one byte (`silent`), and the connected state
  (`pcflag`) is set by the first reply and never cleared.
- **Fix:** the host ends its session after 3 seconds with no packet
  (`CassoLinkSession::kSilenceMs`), so after `PCGONE` (5) seconds with no
  reply the program treats the PC as gone: `pcflag` and `silent` go back to
  0, the PC's line reads `PC: WAITING`, and no silence is counted until the
  next reply, which starts a new connection. Program version 42.
- **Why the emulator missed it:** emulated sessions are short; nothing runs
  for four minutes with the PC silent.

### 31. The label of an operation stayed after the PC stopped

- **Symptom:** the status row read `STATUS: WAITING FOR A COMMAND` with
  `READING THE CATALOG OF D2` on the label row under it, long after the
  catalog read.
- **Cause:** the program clears the label row only on an Idle reply to its
  Ready. A session that ends right after an operation (a closed
  Casso Explorer, a Disconnect, or a probe that stops) never sends that
  Idle, so the label stays until the next connection's first operation.
- **Fix:** when the PC is gone (finding 23), the label row is cleared too.
  It is not cleared on the first second with no reply: a slow PC between a
  whole-disk copy's requests would clear the label of a running operation, and
  the label is sent once per operation. `APcThatStopsAnswering_ClearsItsLabel_AndIsWaitedForAgain`
  sends a label, runs the machine for seven seconds with no host, and checks
  the label row and the PC's line.
- **Why the emulator missed it:** the scenario tests check the screen while
  the host runs, and Casso Explorer's emulated links stay connected.

### 32. A disk with no catalog was reported as unreadable

- **Symptom:** both drives listed with no volume, and opening either one,
  or its preview, read "This drive could not be read."
- **Evidence:** a session probe on the //c returned `0x800703ED`
  (`ERROR_UNRECOGNIZED_VOLUME`) for both catalogs, with no message. A dump
  of tracks 0 and 17 read every sector of both drives with good checksums;
  the data is the pseudo-random image of the round-trip tests (finding 26),
  so neither disk has a catalog left.
- **Cause:** the session attaches no message to an unrecognized volume, and
  the browser's fallback text for any failure with no message is "could not
  be read."
- **Fix:** with `CassoExplorerBrowser::FormatLinkError`, an unrecognized
  volume has the same sentence as an image with no file system: "The disk in
  this drive can't be previewed because it doesn't have a recognized file
  system." For other failures with no message, the text is "This drive could not be read."
- **Why the emulator missed it:** the emulated drive with the same result
  (drive 1, the CassoLink boot disk) has no catalog either, so its bare
  label looked right, and its message was never compared with the disk.

### 24. A stray `)` under the history

- **Symptom:** after a disconnect and reconnect from Casso Explorer, the
  program's screen had two history lines (`12:17 PC CONNECTED`, `12:16 PC
  CONNECTED`), a blank row, then `)` in column 0 of the row after. After a
  fresh bootstrap, with one history line (`14:01 PC CONNECTED`), the `)` was
  at the same place, row 14, under two blank rows.
- **Cause:** Set speed's status row ran past column 39. The status is
  `STATUS: SETTING SPEED D1, T0, S5 (0%)`, with the drive, track, sector and
  percentage of the last disk operation, and the sector and the percentage's
  bounds (`sector`, `plo`, `phi`) were never set at the program's start, so
  the first Set speed's status contained the values BASIC or the monitor had
  left in those zero page bytes. The text is 33 characters plus its digits; at 41, the
  closing `)` lands in column 40 of row 5 (`$0680`), which is `$06A8`,
  column 0 of row 13, the history's third line. The next log line scrolls
  the history down a row and moves the `)` to row 14. On a reconnect, after
  the drive probe's last read (drive 2, track 17, a two-digit sector, 100%),
  the text is exactly 41 characters: `STATUS: SETTING SPEED D2, T17, S12
  (100%)`.
- **Fix:** the status of Set speed is the operation alone; the program sets
  `sector`, `plo` and `phi` to 0 at its start; and `putch` draws nothing at a
  column of 40 or more, so no text is drawn on the row below it. Program
  version 41. `SetSpeed_StatusStaysOnItsRow` sets the sector and the
  percentage to 200 before the connection and checks every frame's history
  rows: the old program puts `%)` at row 13.
- **Why the emulator missed it:** an Emulated link starts at the link's
  speed, so Casso Explorer never sends it Set speed; the scenario tests that
  do send it started with the sector and percentage short enough to fit.

### 25. A lost request hung the host for good

- **Symptom:** in a whole-disk copy to drive 2, the PC stopped with no
  error for more than 10 minutes. The //c was idle in its main loop, sending
  Ready every second.
- **Cause:** `CassoLinkHost::ReadReply` skipped every Ready, on the grounds
  that one can be sent before the operation. After a request lost on the
  line, the program sends Ready every second, and each one restarted the
  host's wait.
- **Fix:** pending commit. `ReadReply` skips one Ready (the program's wait
  for a reply can run out as the request arrives) and returns
  `kLostRequest` on a second; Read sectors, Write sectors, Write track,
  Format track and Time drive send the request again on that, up to
  `kMaxResends` times (item 15).
- **Why the emulator missed it:** no request is lost on the emulated link.

### 26. Results on both drives, 2026-10-20

The owner's Disk II on the //c's external port as drive 2, a blank disk in
it, and a DOS disk in drive 1.

- **Shim timeout (item 22):** page 2 of a load, sent 10 bytes short, was
  met with NAK within 2.5 s; sent whole, with ACK, and the load
  finished. Shim version 3.
- **Drive 2 detected:** the program's drive map is `$03`.
- **Drive 2, blank:** a read of T17 S0 ended in `ERRNOTFND`, as it should.
- **Drive 2, format:** all 35 tracks formatted and read back (item 6's
  verify).
- **Drive 1, round trip:** a 140K pseudo-random image written by Write
  track, every track verified, and read back with no sector different;
  read a second time with no error.
- **Drive 2, round trip (open):** the same with another image: tracks 9 and
  11 failed their verify, and on reading back tracks 3 and 9 failed, 32
  sectors in all. Zeros (the format) wrote correctly on every track, random
  data did not on a few, and track 3 verified and later did not read.
- **Drive speeds, by Time drive (item 27):** drive 1 at 293.16 rpm (204.667
  to 204.670 ms over three runs of 20 revolutions), gap 27; drive 2 at
  298.37 rpm (201.08 to 201.11 ms), gap 21. Drive 2 is 0.5% slow, inside the
  usual setting, so its failures are not its speed.
- **Drive 2 with its own gap:** written again with gap 21, tracks 3 and 8
  failed their verify and track 8 did not read back: 16 sectors. The failing
  tracks move from run to run. Next: the same disk written by DOS's RWTS,
  to separate CassoLink's write loop from the drive and its disk.

### 29. A page crossing put three zero bits in a row on the disk

- **Symptom:** drive 2 (the Disk II) lost a few tracks to random data, and
  the failing tracks moved from run to run; drive 1 never did.
- **Evidence, 2026-10-20:** every logical sector of drive 2 written by DOS's
  own RWTS, called from a diagnostic build: 12 write errors, every one
  logical sector 8 (physical 14), on tracks 3, 4, 5, 7, 8, 14, 17, 24, 27,
  28, 30 and 32; all other sectors read back exactly. The tracks had last
  been laid out by CassoLink's Write track with a gap of 21, where sector
  14's address field runs from stream byte 5878 to 5891 and the page at
  5888 begins inside its checksum.
- **Cause:** the player took 8 more cycles where the stream crossed a page,
  two more bit cells with no flux change. After a nibble ending in a 0 bit,
  that is three zero bits in a row, which disks written by Apple's routines
  never have outside self-sync bytes; on a weaker drive the read amplifier
  can turn the long gap into a false 1 bit. Drive 1 read them; drive 2, and
  RWTS's stricter address check, did not always.
- **Fix:** `69a25eb2e`. The stream ends with a mark, `PEND`, instead of a
  page count, and every path from a store back to the next load is 21
  cycles, the page-stepping one too, so a crossing adds no time.
- **Result:** drive 2 written with gap 21 by the new player: 34 of 35
  tracks written and verified. Track 3 ended in `ERRLINK` (item 30), not a
  disk error.
- **Why the emulator missed it:** the emulated drive reads any run of zero
  bits back exactly; a real read amplifier does not.

### 30. One track's sectors did not arrive over the line

- **Symptom:** in the write of drive 2 above, Write track for track 3 ended
  with `ERRLINK`: one sector's data failed its CRC, or did not arrive, 8
  times in a row.
- **Evidence:** with three pseudo-random images written to drive 2 and every
  packet timed, two wrote whole and one ended in a link error on track 30,
  sector 14. From its first NAK, a NAK followed about every 22 ms, the time
  one data packet takes on the line, and each came within 2 ms of the
  host's resend.
- **Cause:** a damaged packet is found partway through, with the rest of it
  on the line. The NAK went out at once, the host sent the packet
  again behind that rest, and the program read the rest as the new
  packet's start, failed again, and so on until all 8 attempts had failed. The
  3-second timeouts this morning made it rare; the tenth-of-a-second ones
  (finding 34) made it common.
- **Fix:** before each NAK, `drain` drops the bytes arriving until a
  tenth of a second passes with none. Program version 46. The image that
  failed then wrote whole in 62.3 s with 5 NAKs, and the other two with 8
  and 9.
- **Why the emulator missed it:** no byte is lost on the emulated line,
  so no packet is ever damaged.
### 27. Each drive's speed is timed, and the format fits it

- **Before:** fixed gaps of 10 after each sector and a 352-byte first gap,
  which fit a drive up to about 4% fast and any slower one (item 3).
- **Now** (`ee1763358` and the commits before it): the program's Time drive
  command sends a marker each time sector 0's address field passes; the PC
  times 10 revolutions (`CassoLinkHost::TimeDrive`), logs the speed ("D1
  turns at 293.2 rpm, 2.3% slow", with a note past 1.5%), and sends
  `CassoLinkHost::ComputeFormatGap`'s gap with each Format and Write track:
  as long as fits 16 sectors in the revolution with about a 1% margin, 4 to
  40. The program builds the first gap to fill the rest of 26 pages. A drive
  is timed once a connection, on track 0, which is formatted first with the
  program's gap when it has no sector 0.
- **Limit:** the PC times by its own clock, so only Serial and Wi-Fi modem
  links are timed; an emulated Apple does not always run in real time, so
  its milliseconds are not its cycles, and Emulated and TCP links use the
  program's gap. Timing on the Apple in CPU cycles would serve every link,
  and the emulator's per-drive speed setting (Follow-up ideas) would test
  it.

### 28. Leaving CassoLink, and a disconnected link

- **Leaving (item 20's fix):** the shim saves the zero page at `$3C00` and
  the reset vector on its first start; Control-Reset, Escape or the PC's
  Exit run `exitlink`, which stops the drive, turns DTR off, puts both back,
  empties BASIC's program, sets the keyboard and screen and ends at DOS's
  warm start, or the monitor with no DOS. Shim version 4. On the //c,
  2026-10-20: after Escape, the BASIC prompt with an empty program; `CATALOG`
  worked, and no stray characters were drawn.
- **Disconnect:** Casso Explorer's list started a stopped link on every
  refresh, so Disconnect was followed at once by a new connection. A
  disconnected link now stays stopped, with "... is disconnected." and a
  Connect button in the list and Connect on its menu. A serial link whose
  Apple is not replying has a Bootstrap the Apple button in the list.
- **Disconnect's status text:** the status bar read "The Apple stopped
  answering: the line failed with 0x8007006D" after a Disconnect, because
  `CassoLinkSession::Stop` closes the line and the session's read then
  fails like a dropped one. `CassoLinkSession::m_stopping` is set first, and
  a session stopped on purpose reports "Disconnected"
  (`Stop_IsReportedAsDisconnected`).
- **The Connect button:** the list's button drew but did not respond to a
  click, as Casso Explorer routes mouse input to each control explicitly
  and the button had no route. It is given every move and the presses
  inside it. Verified with clicks on the emulated and the serial link.

### 34. A 10 KB file took 31 seconds to write

- **Symptom:** a 10 KB binary written to drive 1 took 31.0 s; SC-028's limit is
  5 s. Read back from a new session, the same file took 3.2 s.
- **Cause, in three parts, from a timed trace of every packet:**
  - Write sectors handled one sector at a time: the program requested its
    data with an Acknowledge, wrote the sector when it next passed the
    head, read it back a revolution later, and only then requested the
    next. Each sector cost more than a revolution, about 200 ms, and DOS
    3.3's skew made the order worse, as each next physical sector is two
    behind the last.
  - Before each write, `CassoLinkSession::CheckCatalog` dropped the drive's
    cache and listed the drive again: track 17, then each file's
    track/sector list and first sector, one request a track, about 7 s.
  - A byte lost on the line cost the 3 s timeout before the NAK; one in
    about forty data packets is lost on the owner's line.
- **Fix:**
  - Write sectors receives and encodes every sector of the request first,
    each into ENCBUF and its write stream into its own two pages of
    `WSLOTS` (`$5000`-`$6FFF`); then writes them in one pass, each as its
    address field passes, as a track read does; then reads the track back
    once and compares each sector with its encoding (`verifysecs`,
    `cmpslot`).
  - `CheckCatalog` reads only the catalog's sectors again
    (`LinkedDisk::GetCatalogSectors`); when they read as they are in
    memory, the cache is used as it is. While CassoLink runs the Apple runs nothing else,
    so only another disk in the drive changes them.
  - The bytes of a packet after its first, and the bytes of a damaged
    packet, wait about a tenth of a second (`getbtq`): a loss costs a
    tenth of a second.
  - The motor runs for about two seconds after an operation, so the next
    one does not wait for it to come up to speed.
  - Program version 44.
- **Result:** a new 10 KB file in 6.3 s, read back equal; 8.6 s when it
  replaces a file, as the volume code then reads each other file's
  track/sector list once. The rest is the disk: a sector costs about 62 ms
  (23 ms on the line, then its CRC, encoding and stream), and each track's
  request about 0.55 s of revolutions to write it and read it back. Under
  5 s takes dropping the read-back, or receiving on interrupts while
  encoding, which the //c's interrupt path is too slow for at 115,200 baud.
- **Why the emulator missed it:** the scenario tests checked what was
  written, not how long it took; in-process, the line costs nothing.
  `WriteSectors_AWholeTrackTakesAFewRevolutions` now times a whole track:
  about 16 revolutions of emulated time, against 38 for the old loop.

### 33. Write track's status ended on sector 16

- **Symptom:** while a whole-disk write ran, the status read `S16`, a
  sector that does not exist.
- **Cause:** `verifytrack` and `buildformat` count `sector` from 0 to 16,
  and the status is drawn after the loop. On the //c the status stays on
  the screen while the next track's data comes over the line.
- **Fix:** both loops step `sector` back to 15, the last sector handled.
  Program version 43. `WriteTrack_WritesEverySector_AndItsStatusEndsOnTheLast`
  checks the sector after a Write track, and that the track reads back as
  written; with the old program the sector is 16.
- **Why the emulator missed it:** in-process, the next request arrives at
  once and the status is redrawn before a frame is captured; and no
  scenario test ran Write track.

## Link errors

The cable's noise so far has been on the control lines (item 8). No data
CRC failure has been seen, but none is counted (item 18), so that is not
proof there were none. The link's handling of data errors, from the code:

- Every packet's envelope has a check byte, and its data a CRC-16/XMODEM.
- Sector data to the Apple (Write sectors, Write track): the Apple checks
  each sector's CRC and sends NAK for another copy, up to 8 times, then
  reports `ERRLINK`.
- Program load: the shim sends ACK or NAK per page; the host resends a
  page up to `kMaxResends` (5) times.
- ADTPro image transfers: the host NAKs a bad block and accepts up to 5
  resends.
- A dropped line: the host disconnects after a time with no packet, or after
  DSR is off for 500 ms, and connects again.

### 14. Sector data from the Apple is never resent

- **Cause:** in `CassoLinkHost::ReadSectors`, a Sector data packet that fails
  its CRC fails the whole request; the host sends no NAK. A whole-disk read
  reads failing tracks again sector by sector, which may cover a link error;
  whether it treats a link error as a disk error is not yet checked.
- **Fix:** pending. NAK a bad sector; the Apple sends it again from its
  buffer.

### 15. A damaged request is lost without a resend

- **Cause:** the Apple drops a request whose envelope or CRC fails. The host
  waits for the reply until the operation timeout and fails the request. A
  damaged Status reply also fails the request.
- **Fix:** pending. Send the request again when no reply arrives. Every
  operation can be repeated safely: a sector written twice with the same
  data, or a track formatted twice, ends the same.

### 16. The typed bootstrap has no checksum

- **Cause:** item 7's pacing stops dropped characters, but a character
  damaged on the line goes unnoticed.
- **Fix:** pending.

### 17. The 6551's error bits are not checked (to verify)

- Whether the Apple's receive routine reads the 6551's overrun, framing and
  parity bits is not yet checked.

### 18. Link errors are not counted

- **Cause:** CRC failures, resends and overruns are not counted, so a noisy
  cable is invisible until a transfer fails.
- **Fix:** pending. Count them per session; display the counts in Casso Explorer's
  link status and in the Apple's history.

## Tooling

### 19. A program that fails to assemble breaks the link silently

- **Symptom:** every request timed out, including the first read.
- **Cause:** the host assembles `Program.s` when it loads the program. An
  out-of-range branch in a diagnostic build failed assembly, and the load returned
  no error to the caller.
- **Fix:** the load reports the first assembly error, or that the program
  does not fit below the shim, through the host's activity sink
  (`CassoLinkHost::DescribeAssemblyFailure`), and drops the partial image so
  the next load assembles again.

## What ADTPro does

A survey of ADTPro's source (the owner's clone, last commit 2026-05-28),
read for strategies only; no code is taken from it. References are to its
tree.

**Disk writes.** Sector and block writes go through Apple's routines: the
ProDOS client calls the MLI's WRITE_BLOCK (`src/client/prodos/rw.asm`), the
DOS client calls RWTS. ADTPro's only nibble-level write is its formatter,
taken from Hyper-FORMAT (`src/client/diskii.asm` 40-91,
`src/client/prodos/format.asm`).

- The formatter stores with **indexed** addresses (`$C08F,X`, `$C08D,X`,
  `$C08C,X`), with 32 cycles per data byte and 40 per sync. The //c's slot 6
  passes the same Disk II signature test and takes the same loop; there is
  no IWM path. **This contradicts item 1's explanation**: indexed stores work
  on the //c for ADTPro's formatter and for Apple's RWTS. Item 1's fix may
  have worked for another reason, such as the rebalanced cycle counts.
  Reopen item 1 once writes take effect on the disk again (item 4).
- A write ends with the last bytes of the buffer being gap filler, and read
  mode follows within about 10 cycles of the last store. A buffered byte
  dropped there is filler. This is consistent with item 2.
- Track layout is fixed: 206 syncs in gap 1, 6 in gap 2, 16 in gap 3,
  volume 254. The whole write is about 207,700 cycles, longer than a
  revolution at 300 rpm, so its end lands in gap 1. No speed measurement, no
  gap adaptation (compare item 3).
- No read-back verify after a format or a write (compare items 4 to 6).
  The DOS client retries a failed RWTS call a user-chosen number of times.
- Write-protect is sensed the same way as ours.
- Drive presence comes from ProDOS's device list, with no probe of its own
  (compare item 11). The motor is started early, during the serial transfer of
  the data to write, which removes the spin-up wait.

**Bootstrap** (compare items 7 and 16).

- Characters of a line go out in one burst at the carriage return; the
  pause is after each line: 250 ms by default (15 to 1,000 ms), and at least
  500 ms for the first and last few lines.
- The typed text is a monitor listing that returns the cursor to the top
  row at a fixed interval of lines, so the screen never scrolls; scrolling slows the
  monitor's echo.
- No checksum. The typed stage is a small loader that requests the rest in
  binary and sends the request again after each timeout of about 2 s.

**Protocol errors** (compare items 14, 15 and 18).

- CRC-16 with polynomial `$1021` on the data, an EOR check byte on each
  envelope.
- Apple to PC: the host sends ACK or NAK, with up to 3 resends and a wait
  of 500 ms times the resend number before each NAK.
- PC to Apple: the Apple's ACK has the next block's number, and the host
  treats a wrong number as a NAK; up to 3 resends with a growing wait. The host
  drops a resent block it already has, which covers an ACK that does not arrive. The Apple's
  side retries without limit.
- A corrupted request from the Apple is dropped by the host; the Apple's
  wait for a reply times out and the operation is abandoned.

**6551 and line state** (compare items 8 and 17).

- Polled, interrupts off, command `$0B`. The 6551's parity, framing and
  overrun bits are never read; the CRC is relied on.
- Before each menu the Apple reads and discards input until about 0.13 s
  of silence, which clears a line left in a bad state.
- The PC side has no flow control unless the user picks the ImageWriter
  cable option, and no CTS, DSR or DCD monitoring, no disconnect detection
  and no reconnect. A dead Apple appears only as long timeouts.
- The Apple always sends first; only the bootstrap loader repeats its
  request until a reply arrives (compare item 9).

**Ideas from it for us:**

- Write data sectors the way Apple's routines time them, and recheck our
  write loop against RWTS's timing rather than against item 1's theory.
- Write throwaway sync bytes after the last nibble of every write (item 2).
- Pace the bootstrap per line rather than per character, and return the
  cursor to the top so the screen does not scroll, which may let it run faster than item 7's pacing.
- Put the block number in every ACK and check it on both ends.
- Drain the line until quiet before a new session.
- Start the motor during the transfer of the data to write.

## Follow-up ideas

For the spec, after the real //c works; not part of the current fixes.

- **Measure each drive's speed on the Apple.** Count CPU cycles per
  revolution between two passes of one address field, on each connect and
  disk change. Size a format's gaps to fit the measured revolution, and
  report rpm and cycles per revolution to Casso Explorer, with a warning
  outside about +/-1.5% of 300 rpm. On a blank disk, write one address field first and time that.
- **A per-drive speed setting in the emulator**, about 285 to 315 rpm, so
  item 3 and the speed measurement can be tested without hardware.
- **A noisy test stream**: bit flips, dropped bytes and control-line glitches
  injected over `InProcessByteStream`, for items 8 and 14 to 18.
- **Other hardware:** the owner's Disk II on the //c's external port, as
  drive 2, is a second mechanism on the same IWM; a //e with a Disk II card
  would test the Disk II controller path.

## Probe method

The probes are throwaway `TEST_METHOD`s in
`UnitTest/CassoLink/CassoLinkClientTests.cpp`, never committed. Each opens
COM3 through `Win32ByteStreamFactory` and drives `CassoLinkHost` directly,
with Casso Explorer closed. Diagnostic program builds take over
`doformat`'s volume byte: 2 captures 512 nibbles after a sector's address
field, 4 formats and captures 32 pages, 5 sends the built stream, 6 formats
with a longer first gap, 7 captures without writing.
