# Assets/Sounds/DiskII

Drive-audio sample assets for the Disk II 5.25" floppy drive
(spec `005-disk-ii-audio`).

## Files (PascalCase WAV, decoded at startup via `IMFSourceReader`)

| File              | Role                                | Length (target)  |
|-------------------|-------------------------------------|------------------|
| `MotorLoop.wav`   | Continuous DC-motor / spindle hum   | 1 – 2 s loop     |
| `HeadStep.wav`    | Single quarter-track head click     | 80 – 150 ms      |
| `HeadStop.wav`    | Track-0 / max-track travel-stop bump | 100 – 200 ms    |
| `DoorOpen.wav`    | Disk-eject lever / door open snap   | 150 – 400 ms     |
| `DoorClose.wav`   | Disk-insert / door close ratchet    | 200 – 500 ms     |

All files are decoded and resampled once at startup via
`IMFSourceReader` to mono float32 at the WASAPI device's native rate
(typically 44.1 kHz or 48 kHz). Native sample rate of the source
files does not matter — Media Foundation handles the conversion.

## Graceful degradation (FR-009)

This directory may be empty or missing entirely. Each file is loaded
on a best-effort basis: missing or malformed assets are logged once
and silently muted. The emulator never refuses to launch, and never
shows a modal dialog, over a missing drive-audio asset.

## Licensing (NFR-004 / NFR-005)

Real-hardware recordings of an actual Disk II drive are preferred when
permissively licensed (CC0, CC-BY, MIT). Procedural synthesis baked
into the binary is an acceptable fallback.

**Do NOT commit** the OpenEmulator / libemulation Disk II samples to
the public repo: they are GPL-3 and would virally relicense the
emulator. They may be used locally during development.
