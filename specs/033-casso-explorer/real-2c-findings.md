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

### 1. Indexed stores put a false byte on the IWM

- **Symptom:** sector writes failed to verify; the drive did not take the
  intended byte.
- **Evidence:** writes verified after the change below and not before it.
- **Cause:** `sta $C08D,x` takes five cycles, and on its fourth the 65C02
  accesses the I/O page once before the real store. The IWM latches a byte on
  that access, a cycle early. The //c ROM listing notes the same effect for
  the IWM's mode register.
- **Fix:** `3b869585d`. The write player stores to slot 6's absolute addresses
  (`W6Q6L` `$C0EC`, `W6Q6H` `$C0ED`, `W6Q7H` `$C0EF`), with its cycle counts
  rebalanced.
- **Why the emulator missed it** *(unverified)*: the emulated LSS takes the
  data bus value at the store's write cycle and either ignores the extra
  access or treats it as harmless. A real Disk II card tolerates it too; only
  the IWM does not.

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

### 4. Writes stopped reaching the disk (open)

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
- **Cause:** not yet known. Since the committed program also fails, a physical
  cause (the disk, its notch, its seating, or the drive's write circuit) is
  the leading suspect. A DOS `SAVE` on the same disk separates a physical
  cause from a program fault.
- **Why the emulator missed it:** to be determined with the cause.

### 5. Damage attributed to the format was on the old track

- **Symptom:** about one sector per track failed to read after a format, always the
  same sectors (on T28: S2, S5, S6, S9, S10, S12, S14).
- **Evidence:** raw captures had the damage at the same byte of the same
  sectors after shifting the whole stream by 96 bytes and after delaying the
  write's start by 1, 51, 101 and 151 ms. Item 4 is the reason: none of those
  formats wrote the track.
- **Cause:** a stale track, from DOS's INIT and earlier experiments, read
  back after formats that did nothing.
- **Fix:** see item 6, so that a format that writes nothing is reported as an
  error.
- **Why the emulator missed it:** not an emulator gap. The emulated drive
  always writes, so nothing exposed the missing verify.

### 6. A format does not read back what it wrote

- **Symptom:** a format reports success when nothing was written.
- **Cause:** `doformat` writes the stream and returns without reading the
  track. Write track reads back and compares; format does not.
- **Fix:** pending. Read the track back after a format and report the first
  sector that does not read.
- **Why the emulator missed it:** see item 5.

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

## Tooling

### 13. A program that fails to assemble breaks the link silently

- **Symptom:** every request timed out, including the first read.
- **Cause:** the host assembles `Program.s` when it loads the program. An
  out-of-range branch in a diagnostic build failed assembly, and the load returned
  no error to the caller.
- **Fix:** pending. Report the assembly error from the load.

## Probe method

The probes are throwaway `TEST_METHOD`s in
`UnitTest/CassoLink/CassoLinkClientTests.cpp`, never committed. Each opens
COM3 through `Win32ByteStreamFactory` and drives `CassoLinkHost` directly,
with Casso Explorer closed. Diagnostic program builds take over
`doformat`'s volume byte: 2 captures 512 nibbles after a sector's address
field, 4 formats and captures 32 pages, 5 sends the built stream, 6 formats
with a longer first gap, 7 captures without writing.
