# CassoEmuCore/Audio

Drive-audio subsystem (spec `005-disk-ii-audio`):

- `IDriveAudioSink` — event interface fired by drive controllers
  (motor / head / disk-insert / disk-eject).
- `IDriveAudioSource` — per-drive audio source: consumes sink events,
  produces mono PCM, exposes per-drive stereo pan coefficients.
- `DriveAudioMixer` — registers sources, mixes them per-frame into a
  stereo float-PCM buffer for `WasapiAudio`.
- `DiskIIAudioSource` — concrete `IDriveAudioSource` for the Disk II
  drive (motor loop, head step/bump one-shots, door open/close
  one-shots, step-vs-seek discrimination).
- `AudioGenerator` — pre-existing //e speaker waveform generator
  (unchanged by this feature).

Future drive types (`AppleIIcInternalDriveAudioSource`,
`ProFileAudioSource`, …) live alongside `DiskIIAudioSource` without
modifying the mixer or sink interface (FR-016).
