# Task: every sound Casso makes is amplitude-modulated at ~41 Hz by render filler

Start a fresh session on relmer-desktop. Fetch first and branch from current
`origin/master`. Read `CLAUDE.md` and `.github/copilot-instructions.md` first.

This one is measured, not suspected, and it affects ALL audio -- speaker, disk,
Mockingboard tones and speech alike. It was found while chasing what sounded
like a defect in the SSI-263 synthesis; it is not in the synthesis at all.

## The measurement

`Apple2/Demos/mockingboard-test.dsk` plays ONE sustained AY square wave and
nothing else, so its envelope should be perfectly flat. Envelope modulation in
the 15-90 Hz band:

| where | modulation |
|---|---|
| a pure 1 kHz tone from Windows, same speakers, same capture rig | 0.00% |
| Casso's generated mix (`CASSO_AUDIO_DUMP`, producer side) | 0.04% |
| Casso's frames as handed to the endpoint (`CASSO_AUDIO_DUMP_DEVICE`) | 1.76% at 41.4 Hz |
| Casso at the speakers (WASAPI loopback) | 4.28% at 41.3 Hz |

The rig is clean, the device is clean, our mix is clean. The artifact appears
across the pending queue.

Reproduce both taps in one run:

    $env:CASSO_AUDIO_DUMP        = "mix.f32"
    $env:CASSO_AUDIO_DUMP_DEVICE = "dev.f32"
    .\x64\Release\Casso.exe --machine Apple2e --disk1 Apple2\Demos\mockingboard-test.dsk

Both are headerless float32 stereo at the device mix rate, **48000 Hz on this
hardware, not 44100**.

## The mechanism

`WasapiAudio::DrainFrames` fills any shortfall between what the queue holds and
what `RenderPump` asked for with filler: it holds the last frame and decays it
at 0.995 per sample, then blends back over a 96-frame resume ramp. That fade
and re-ramp is the beating.

It is not rare. In a 10 second run, **28672 of 531968 frames sent to the device
were filler -- 5.4% of the stream**.

The reason is a persistent production deficit, not jitter.
`EmulatorShell.cpp` (~line 8913) computes the sample count from EMULATED
cycles:

    exactSamples = sliceActual / cyclesPerSample + m_sampleRemainder;

so audio is produced in proportion to how fast the emulator actually runs. It
runs about 5% behind real time, the device consumes at exactly real time, and
the queue therefore drains on a regular cadence forever.

## What NOT to do

Do not tune the filler threshold. `RenderPump`'s floor was changed so filler is
written only when nothing is pending AND the device is near dry; it measured
identically (4.48% vs 4.28%, i.e. unchanged), and that change was reverted
rather than kept. No queue depth can cover a producer that is permanently
slower than the consumer -- a deeper buffer only drains more slowly.

## What to do

Decide where the 5% goes, then fix it there. Three candidates, in the order I
would try them:

1. **Find the pacing error.** The emulator targets 1x. Measure what it actually
   delivers: instrument cycles executed per wall-clock second. If frame pacing
   sleeps slightly long, or a slice is being dropped, this is a scheduling fix
   and the deficit disappears. `kAppleCpuClock` is 1020484 in MachineConfig.h;
   confirm the audio path and the emulation path agree on it.
2. **Close the loop.** Let the emulator's target rate be nudged by the queue
   depth -- speed up marginally when the queue is short, slow when it is long.
   This is how emulators usually sync audio and video, and it also fixes the
   converse case where a fast host overruns.
3. **Resample.** Correct in principle and the most work; only worth it if 1 and
   2 cannot hold the rate.

Whatever the fix, keep filler as a genuine last resort and consider counting
filler frames into a diagnostic so a regression is visible rather than audible.

## Validation

The tone test above must show the endpoint-side dump within noise of the
producer-side dump -- under about 0.1% in 15-90 Hz -- with filler frames near
zero. Then re-run `Apple2/Demos/mockingboard-speech-demo.dsk`, whose held sung
vowels are where a listener first noticed this.

`scripts/RunTests.ps1 -Build` and `scripts/CheckStyle.ps1 -Mode Tree`.
