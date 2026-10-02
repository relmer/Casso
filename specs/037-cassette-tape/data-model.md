# Data Model: Apple II Cassette Tape Support

**Feature**: 037-cassette-tape | **Date**: 2026-10-02

## TapeImage

An inserted recording, decoded once at insert time.

| Field | Type | Notes |
|---|---|---|
| path | `std::string` | source file |
| format | `TapeFormat` enum: Wav, Aiff, Mp3 | from the content, not the extension |
| sampleRate | `uint32_t` | 8,000-96,000 Hz |
| pcm | `std::vector<int16_t>` | mono; kept for audible playback and so a recording can be spliced in |
| transitions | `std::vector<double>` | ascending, fractional sample indices of each level flip |
| initialLevel | `bool` | level before the first transition |
| isWritable | `bool` | WAV and not read-only; gates record |

**Validation**: an unreadable or unsupported file fails insertion with
`ERROR_INVALID_DATA` and a user-facing message, and the deck stays as it was. A
zero-length WAV (a blank tape) is valid: no samples and no transitions.

**Derived**: length in seconds is `pcm.size() / sampleRate`, and the level at
sample `s` is `initialLevel` flipped once for each transition at or before `s`.

## TapeDeck

The transport. The shell owns it; it outlives machine rebuilds.

| Field | Type | Thread |
|---|---|---|
| image | `std::unique_ptr<TapeImage>` | UI thread inserts or ejects, through a posted command |
| state | `TapeTransport` enum: Empty, Stopped, Playing, Recording | CPU thread; mirrored to an atomic for the UI |
| positionSample | `double` | CPU thread; atomic mirror for the UI |
| playStartCycle | `uint64_t` | CPU thread; bus cycle when play began |
| playStartSample | `double` | CPU thread |
| cursor | `size_t` | CPU thread; index into `transitions` |
| recordArmed | `bool` | the record button is latched |
| lastAccessCycle | `uint64_t` | CPU thread; last $C060 read or $C020 toggle while moving |

**State transitions**

```text
Empty --insert--> Stopped
Stopped --play--> Playing           (recordArmed && isWritable --> Recording)
Playing/Recording --stop--> Stopped (Recording: TapeRecorder commits the capture)
Playing --end of tape--> Stopped    (position clamps at the length)
Recording --past end--> continues   (the tape is extended on commit)
Stopped --rewind--> Stopped, position 0
any --eject--> Empty                (Recording: commit first)
Playing/Recording --reset or machine change--> Stopped (stays inserted)
```

Rewinding while playing stops first. Record cannot be armed on a protected tape.

## RecordingCapture

| Field | Type | Notes |
|---|---|---|
| startSample | `double` | tape position when recording began |
| startCycle | `uint64_t` | bus cycle when recording began |
| toggleCycles | `std::vector<uint64_t>` | absolute bus cycle of each $C020 access |
| initialLevel | `bool` | output flip-flop state at start |

On commit, each toggle maps to sample `startSample + (cycle - startCycle) *
sampleRate / cpuClockHz`. The square wave replaces `pcm` from `startSample` on,
extending it if needed. The WAV is rewritten atomically and the transitions are
re-decoded.

## CassettePort (bus device)

| Field | Type | Notes |
|---|---|---|
| range | $C020-$C02F | the //e forwards to it from `Apple2eKeyboard`; the ][/][+ game port forwards $C060/$C068 |
| outputLevel | `bool` | flip-flop toggled by any $C020 access |
| pBusCycle | `const uint64_t *` | from `GetBusCyclePtr()` |
| pDeck | `ITapeDeckPort *` | narrow interface onto `TapeDeck`: `ReadInputLevel (cycle)`, `OnOutputToggle (cycle)` |

Cassette in: `ReadInputLevel` returns the tape level at that cycle, or 0 when
the deck is not playing. Real hardware reads a quiet line when no tape plays,
so 0 is the defined idle value.

## TapeTurboGovernor (pure)

Input: `{ preferenceOn, transport, lastAccessCycle, nowCycle, cpuClockHz }`.
Output: `bool overrideMaximum`.

Rule: on when `preferenceOn && (transport == Playing || transport == Recording)
&& nowCycle - lastAccessCycle <= cpuClockHz / 10`. The window is 100 ms.

## FastTapeLoading preference

`fastTapeLoading`: bool, default `true`, stored in the UI prefs with
`floppySoundEnabled`. Per machine, `tapePath`: string, stored with
`disk1Path`. In global prefs, `recentTapes`: MRU list.
