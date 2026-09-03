# Task: reads in the Mockingboard speech range return the voice chip, not the VIA

Start a fresh session on relmer-desktop. Fetch first and branch from current
`origin/master`.

Read `CLAUDE.md` and `.github/copilot-instructions.md` first for conventions
(EHM macros, single exit, style gate, test expectations).

## The defect

`CassoEmuCore/Devices/Mockingboard/MockingboardCard.cpp`, in the read path:

    if (IsInstalledSpeech (offset))
    {
        // The VIA still sees the access -- its side effects are real on the
        // board -- but the chip drives D7 with its request status, the only
        // line it drives on a read.
        return static_cast<Byte> ((m_via[index].ReadRegister (reg) & 0x7F) |
                                  m_speech->ReadRegister (static_cast<Byte> (offset)));
    }

The comment's premise is wrong. On a Mockingboard the SSI-263 drives **nothing**
on a read. The first 6522 is selected by !A7 with RS3:0 = A3:0, so it answers
across the whole of `$Cn00-$Cn7F`, and `$Cn40` **is** that VIA's port B. A read
there returns IRB. Substituting the chip's request bit into D7 is a behavior
real hardware does not have.

Reading A/R back is a **Phasor native-mode** facility. Two independent
references agree:

- AppleWin gates its `SSI263::Read` behind `m_isPhasorCard && m_phasorMode ==
  PH_Phasor`, with the comment "NB. Mockingboard: SSI263.bit7 not readable".
- mb-audit (validated against physical MB-C and Phasor boards) documents
  `REQ_NEW_DATA = 1<<7` as "Phasor mode only, when reading any register".
- MAME's `a2bus_ayboard_device::read_cnxx` likewise returns only the VIA.

The write side is already right: writes land in both chips, which is why a
phoneme byte to `$Cn40` is also an ORB write and a rate byte to `$Cn42` is also
a DDRB write. Casso models that correctly. Only the read is wrong.

## Why this matters, with a worked example

It hid a real bug for the entire life of `Apple2/Demos/mockingboard-speech-demo.a65`.
That demo paced spoken phonemes by polling D7 at `$C440`. On Casso it worked.
On AppleWin it stopped on the second sound of the first word, repeating it
forever. The reason is exactly this divergence: on real hardware the poll reads
back the demo's own writes.

    intro rate byte $8A -> DDRB, so PB7 is an output
      phoneme $AC (HF, DR=%10) -> ORB bit 7 = 1 -> poll exits at once
      phoneme $0A (EH, DR=%00) -> ORB bit 7 = 0 -> `bpl` spins forever

Streams whose rate byte leaves PB7 an input ($54, $61, $42) read back 1 and
never wait at all. The demo has since been changed to pace on counted cycles
and no longer reads the register, so it is no longer a reproducer -- but any
period title that polls this way will behave differently on Casso than on
hardware, and the difference is invisible from inside Casso.

## A second, related question to settle

`MockingboardCard::SyncSpeechRequest` drives A/R onto **VIA #1's** CA1:

    m_via[0].SetCa1 (!m_speech->IsRequesting());

On real hardware the `$Cn40` chip's A/R is understood to run to the **second**
6522's CA1, latching IFR bit 1 at `$Cn8D` -- which is precisely what makes the
handshake usable, since the `$Cn4x` write aliasing lands on the *first* VIA and
would otherwise keep clobbering the register you are polling. If that is right,
Casso puts the flag at `$C40D` where hardware puts it at `$C48D`, and
IRQ-driven period speech drivers will not work.

This one is **not confirmed** -- verify it against mb-audit and a schematic
before changing anything. Do not take the paragraph above as established.

## What to do

1. Make reads in the speech range return the VIA alone for the Mockingboard
   variants. Keep the chip's readback only where a Phasor in native mode would
   expose it -- if Casso has no Phasor, then there is no path that should
   return it at all, and `Ssi263::ReadRegister` may become dead.
2. Settle the CA1 wiring question above and fix or document it.
3. Add a test. `UnitTest/EmuTests/` has Mockingboard coverage; assert that after
   writing a phoneme byte to `$Cn40` and a rate byte with PB7 as an output to
   `$Cn42`, a read of `$Cn40` returns the phoneme byte's bit 7 and not the
   chip's request state. That test is the whole bug in four lines.
4. Check whether anything in the tree depends on the current readback.
   `Apple2/Demos/mockingboard-speech-test.a65` may poll the same way; if it
   does, it is broken on hardware for the same reason and wants the same
   cycle-paced treatment the demo got.

## Validation

`scripts/RunTests.ps1 -Build` and `scripts/CheckStyle.ps1 -Mode Tree`.

For an end-to-end check, boot `Apple2\Demos\mockingboard-speech-demo.dsk` from
branch `028-speech-demo` and confirm it still runs start to finish -- it no
longer reads the register, so this fix must not disturb it.

Note when analyzing a `CASSO_AUDIO_DUMP` file: headerless raw float32 stereo at
the WASAPI device mix rate, **48000 Hz** on this hardware, not 44100.
