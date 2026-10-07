# Contract: Tape files

## Read

| Format | Accepted |
|---|---|
| WAV (RIFF/WAVE) | PCM 8, 16, 24, 32-bit; IEEE float 32, 64; `WAVE_FORMAT_EXTENSIBLE` wrapping any of these; 1 or 2 channels; 8-96 kHz |
| AIFF / AIFF-C | PCM 8, 16, 24, 32-bit; AIFF-C `NONE` and `sowt`; 1 or 2 channels; 8-96 kHz |
| MP3 | whatever Windows Media Foundation decodes |

The format is detected from the content (`RIFF`/`FORM`/MP3 frame sync or ID3),
not from the extension. Anything else fails with a message in the project's
error format:

    Error: unreadable tape
           Casso reads WAV, AIFF, and MP3 recordings. This file is not one
           of them, or its header is damaged.

## Write

- Only WAV tapes are written. Casso writes 16-bit PCM, mono, at the tape's
  existing sample rate (44,100 Hz for a new blank tape), with a canonical
  44-byte header.
- The write is atomic: temp file, then rename, through
  `IDiskFileIo::ReplaceAtomically`.
- Recorded signal: a square wave at 80% of full scale with edges at the
  recorded toggle times. The sample before the recording point is untouched.

## Other tools

A Casso-written WAV loads in c2t-compatible tools and other emulators that
read standard Apple II cassette WAVs. SC-002 scenario 2 checks this by hand.
