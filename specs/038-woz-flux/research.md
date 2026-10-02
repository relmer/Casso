# Research: WOZ Flux Track Support

All decisions below are settled; none is left open for implementation.

## R1. Time base for flux playback

**Decision**: Keep flux time in exact integer units of 1/45 flux tick. Each LSS
clock adds 176 units; a transition at tick `t` falls due at `45 * t` units.

**Rationale**: The Apple II master clock is 14.31818 MHz = 315/22 MHz. The CPU
runs at 1/14 of that (45/44 MHz), and `Disk2NibbleEngine::Tick` runs the LSS
twice per CPU cycle (45/22 MHz). A flux tick is 125 ns (8 MHz), so one LSS clock
is 8 / (45/22) = 176/45 = 3.9111 ticks, and a nominal 8-clock cell is
1408/45 = 31.289 ticks. That is the 31.29 the user gave, and it explains why it
is not 32: the Disk II cell is four CPU cycles (3.911 µs), not 4 µs. The ratio is
exactly rational, so integer arithmetic has no drift over a revolution or over
hours of spinning. A 64-bit counter holds a revolution (about 1.6 million ticks,
times 45) with room to spare.

**Alternatives considered**: Floating-point accumulation drifts and costs more
per clock. 16.16 fixed point (256,319 per clock) still drifts by one part in
about 500,000. Converting the flux to bits at load erases the cell lengths
*Bandits* checks, which the spec rules out.

## R2. How a pulse reaches the sequencer

**Decision**: On a flux track, the engine hands the LSS a pulse on the clock
where the next transition's due time falls, whatever clock that is (0-7). On a
bit track nothing changes: the bit is still sampled on `kLssReadClock` (clock
4).

**Rationale**: The P6 ROM (`kSequencerRom16`) is the real sequencer, so it
already handles a pulse arriving on any clock. That is how real hardware tells
a 3.7 µs cell from a 4.1 µs one. AppleEm (web-a2e, commit 74d710b,
`fluxToPulses`) does the same. Leaving the bit path alone is what keeps FR-009
true by construction.

**Alternatives considered**: Running bit tracks on the time base too, by
expanding bits into transitions at 31.29 ticks, would make one code path. It
would also change the phase of every existing copy-protected bit disk relative
to the LSS, which puts SC-004 at risk for no gain.

## R3. Walking flux in memory

**Decision**: Keep each flux track's raw bytes exactly as they sit in TRKS
(`vector<Byte>`, about 38 KB per track on *Bandits*). Add a cursor: the byte
index of the next transition and its absolute tick. Decoding a 255 run adds 255
and keeps reading, so one transition costs one or a few byte reads. Precompute
the track's total tick count at load (one pass) for wrapping and for angle
conversion.

**Rationale**: This is the agreed decision: file size in memory, walked, not
expanded. One byte per transition means about 25,000 transitions per
revolution, against 410,000 LSS clocks. The per-clock cost is one add and one
compare.

**Alternatives considered**: Expanding to one bit per LSS step means 51 KB per
track, plus a second copy for writes. A precomputed absolute-tick array is
four times the file size and buys nothing a cursor does not.

## R4. Head angle across track changes

**Decision**: When the head moves between a bit track and a flux track, convert
through the fraction of a revolution: bit position / bit count becomes
tick / total ticks, and back again. Bit-to-bit moves keep today's
`m_bitPos %= newBits` rule exactly. Flux-to-flux moves use the fraction too.

**Rationale**: FR-004. *Bandits* steps between bit track 0, flux tracks
1.5-20.5 and bit tracks 21-34, so a mismatch here shows up as a lost sync on
the step. Its bit tracks have about 51,000 bits and its flux revolutions are
about 200.5 ms. Both span one revolution, so a fraction is the honest common
measure. Bit-to-bit stays as it is to keep FR-009.

## R5. Long gaps (MC3470 behavior) on flux tracks

**Decision**: Track how much time has passed since the last real transition.
Once it goes past the window that triggers weak bits on a bit track (four
nominal cells, 4 × 1408/45 ticks), inject `NextWeakBit()` pulses at nominal
cell cadence, using the same RNG and the same probability, until the next real
transition is due. A track with zero transitions is the limiting case and reads
as noise (edge case 1). A gap made of a 255 run decodes to one long gap (edge
case 3), because the cursor sums the run before it schedules anything.

**Rationale**: FR-005 asks for the same random-bit behavior as zero runs on a
bit track. Reusing the existing generator and threshold makes "the same" a
literal statement.

## R6. Writes to a flux track

**Decision**: While the drive is writing on a flux track, buffer the written
bits along with the tick position where the burst started. A burst is a run of
LSS write clocks; it ends when write mode turns off, the head steps, the motor
stops, or the disk is ejected or flushed. When it ends, re-encode once: copy the
original flux up to the burst's start tick, emit the burst's 1 bits as
transitions spaced by whole cells of 1408/45 ticks (rounded to integer ticks
with the fraction carried so it does not drift), then resume the original
stream at the burst's end tick. The gap that straddles each splice point is
split so the track's total tick count is unchanged. A burst that crosses the
end of the revolution wraps. The track is marked dirty.

"Nominal" here means the controller's own cell, 4 CPU cycles (31.289 ticks),
because that is the timing a real Disk II writes at. A track written and read
back in Casso then reads exactly as written.

**Flush mid-burst**: `DiskImageStore` flushes on its own path, so it cannot
end a burst the engine is holding. While a burst is open, the engine registers
itself on the `DiskImage` as its `IPendingWriteOwner`, and clears that when the
burst is committed. `DiskImage::Flush`, `DiskImage::Eject` and
`DiskImageStore::FlushEntry` call `DiskImage::CommitPendingWrite()` first, so
every flush path commits the burst without the store knowing about drives.
Every flush already runs on the CPU thread, which owns the burst. (A callback
from the store to the controller was tried first and dropped: the store
outlives the machine that would have supplied it.) A test flushes in the
middle of a burst and checks that the partial write is saved.

**Rationale**: FR-006 and US2. Re-encoding once per burst, about one sector,
costs a single 38 KB pass, so no per-bit splicing is needed. Reads never happen
during a burst, so there is nothing to overlay.

**Alternatives considered**: Converting a written flux track to a bit track,
as AppleEm does, was rejected in the spec's decisions. A permanent write overlay
consulted on every read puts a branch on the hot path forever.

## R7. Saving (WozLoader::Serialize)

**Decision**: Serialize keeps its current layout and adds four things:

1. A flux slot's TRKS record gets the flux byte count in Bit Count, and its
   bytes are copied verbatim. An unwritten flux track is therefore byte-for-byte
   unchanged (SC-003). Its block number can move, which is the same treatment
   bit tracks get today.
2. A FLUX chunk is rebuilt from the quarter-track map, the same way TMAP is,
   keeping TRKS indices. TMAP gets 0xFF wherever FLUX claims the quarter track.
   The chunk starts on the first block boundary after TRKS, matching the
   layout *Bandits* uses (FLUX at block 1624, offset 0, followed by META).
   Stale FLUX pass-through chunks are dropped.
3. INFO bytes 46-47 (FLUX block) and 48-49 (largest flux track, in blocks) are
   written. The INFO version is raised to 3 when a FLUX chunk is written and
   left alone otherwise. A bit-only image keeps bytes 46-49 as zero.
4. A loaded image that was never written is not rewritten at all. Flush only
   runs on a dirty image, so SC-003 holds at file level, not only at chunk
   level.

**Rationale**: The WOZ 2.1 reference: "A FLUX chunk always occupies its own
block", Bit Count "is actually a byte count" for flux tracks, and INFO version 3
or higher with both flux fields non-zero.

## R8. Reading the FLUX chunk

**Decision**: Find FLUX by the chunk walk, as TMAP is found today. It is
honored whatever INFO's version and flux fields say (clarification 3); INFO is
not consulted on read. Only a FLUX chunk shorter than 160 bytes refuses the
mount as `MalformedWoz` (FR-008). FLUX takes precedence over TMAP for the same quarter
track. A flux track's byte count must fit inside its block count × 512, its
blocks must lie inside the file, and its last byte must not be 255. A track
that breaks any of these is a damaged track (R9), not a refused mount.

**Rationale**: The reference requires INFO version 3 or higher with both flux
fields non-zero. A file that has a usable FLUX chunk but disagrees with INFO
can only boot if the chunk is used, and the chunk walk finds it without INFO.
Writes still set INFO correctly (R7), so Casso never produces such a file.

## R9. Damaged tracks: read-only mount and report

**Decision**: `ParseV2Track` (and its flux counterpart) report damage to the
loader instead of failing the load. The loader records the TRKS index and the
quarter tracks that map to it in a new damaged-track list on `DiskImage`, leaves
the slot unformatted, and keeps going. A non-empty list makes
`IsWriteProtected()` true, exactly as `m_sourceCrcMismatch` does, and makes
`DiskImageStore::AssessSalvage` offer salvage. `EmulatorShell::ReportDamagedMount`
states which tracks are damaged, as whole and half track numbers. When the image
has flux tracks, the salvage text also says the copy keeps sector data but not
flux timing or copy protection. A track index that is in range but whose record
is all zero stays "unformatted", as it is today; that is not damage. Structural
failures above track level (missing INFO, TMAP or TRKS, a truncated chunk, a
TRKS record table cut short) still refuse the mount.

This covers WOZ 1 bit tracks too: a v1 record past the end of TRKS, which today
refuses at cpp:651, becomes a damaged track.

**Rationale**: Clarification 2. The checksum-damaged path from 1.17.0 already
has every piece (read-only enforcement, the report on insert, salvage, and a
flush refusal), so damaged tracks become a second reason to enter the same
path, not a new path.

## R10. Hot-path cost

**Decision**: Cache the resolved slot and its kind in the engine on
`SetCurrentTrack` and `SetDiskImage`. Today `StepLss` calls
`ResolveQuarterTrack` on every one of the 410,000 clocks a revolution takes.
Dispatch once per clock on a cached `bool m_isFluxSlot`. Measure with a new
`Disk2NibbleEngine` microbenchmark that ticks a bit track and a flux track for
the same number of cycles, Release only. In the unit suite it prints both
times and asserts only a loose sanity bound (flux ≤ 1.5 × bit), because a 2%
timing assertion is flaky on CI runners and unpinned machines. SC-005's 2%
budget is checked by hand, pinned to one CCD (memory note
`reference-pin-perf-runs-to-one-ccd`), at maximum speed on the real machine.

**Rationale**: FR-011 and SC-005. Caching removes more cost than the flux
branch adds, so the bit path should get slightly faster, not slower.

## R11. Sector-level access to flux tracks (salvage, `disk` command, Explorer)

**Decision**: Add a `FluxBitView`, a decode of one flux track into bits at the
controller's cell (1408/45 ticks), plus a parallel array giving the tick at
which each bit starts. Each transition becomes
`round (gap / cell) - 1` zero bits followed by a 1. The gap is measured from
the previous transition, so the cell grid follows the track's own drift, the
way a data separator does. Gaps longer than the weak-bit window decode as
zeros, never as random bits, so a sector read is deterministic. This view is
used only for sector work. Playback never uses it (R1-R2).

`NibblizationLayer`'s reads (`ReadNibbleAt`, `DecodeTracks`, `Denibblize`,
`SalvageSectors`) take their bits from a track source. On a bit slot that is
`DiskImage`'s packed bits, as today. On a flux slot it is a `FluxBitView`
built for the call. Nothing else in the decoder changes, so bit-track results
are unchanged.

**Sector writes** split by track kind:
- **Bit tracks** keep `RenibblizeTracks` exactly as it is, regenerating each
  touched track (FR-009).
- **Flux tracks** never regenerate. For each changed sector, the writer finds
  that sector's data field in the `FluxBitView`, encodes the new data field
  (prolog, 6-and-2 data, checksum, epilog) with the same helper
  `AppendDataField` uses, and calls `FluxTrack::SpliceWrite` at the field's
  start tick. That is the same splice and the same 31.29-tick cell the drive
  write uses (R6). The address fields, the gaps and every other sector keep
  their recorded flux.

A sector whose data field cannot be found on a flux track fails the write with
the existing "sector not found" error, rather than regenerating the track.

**Rationale**: FR-012 and FR-013. Without this, salvage would zero every flux
track, and the sector tools would see flux disks as blank (analysis findings
C1 and C2). Splicing only the data field keeps a protected flux track as
intact as a sector write can.

**Alternatives considered**: Regenerating the touched flux track as fresh
standard flux, which mirrors the bit path, throws away the timing on 15 sectors
the write never touched. Refusing writes leaves a feature half done.

**Note**: The bit path still regenerates whole tracks on a sector write, which
is coarser than the flux path. Changing that is out of scope here. It is worth
a follow-up, since the same data-field splice would work on bits.

## R12. Test images without *Bandits*

**Decision**: Build synthetic flux WOZ images in the tests. `WozLoader` gets a
`BuildSyntheticV21` helper next to `BuildSyntheticV2`, and a test-side converter
turns a bit track into flux with chosen cell lengths per stretch: 3.7 µs = 29.6
ticks and 4.1 µs = 32.8 ticks, with the fraction carried. The scenario suite
gets a flux copy of the DOS 3.3 System Master, made by that converter with
alternating fast and slow stretches on every track. These run 3% either side of
nominal, a range a stock DOS RWTS is known to tolerate. The 3.7/4.1 µs extremes
are checked at engine level (SC-002), not by asking DOS to read them. It boots that copy, writes
a file to it, saves, reloads, boots again and catalogs. *Bandits* is used by
hand only, as clarification 1 decided, and no merged test refers to it.

**Rationale**: The scenario suite is required because this changes the drive
and the loader, and DOS reading a flux disk is the guest-visible check. A DOS
disk at off-nominal cell lengths exercises the same timing tolerance the
protection relies on, without the protected disk.
