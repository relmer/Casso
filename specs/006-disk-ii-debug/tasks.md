---
description: "Disk II Debug Window — actionable, dependency-ordered tasks"
---

# Tasks: Disk II Debug Window

**Input**: Design documents from `/specs/006-disk-ii-debug/`
**Prerequisites**: spec.md, research.md, plan.md
**Constitution**: `.specify/memory/constitution.md`

**Tests**: Required throughout. Every production-code task that has
observable behavior has a paired test task. Tests use a mock
`IDiskIIEventSink`, in-memory `DiskIIEventRing` instances, and
in-memory nibble streams — no host filesystem reads, no audio device,
no real `HWND`, no real timer, per constitution §II and NFR-002.

**Organization**: Phases follow `plan.md` §"Phasing" — interface first,
producer side, consumer side, Win32 wiring, polish. Each phase ends with
an explicit `[GATE]` task that re-asserts pre-existing tests still green
and that the no-sink controller path is byte-identical to the pre-feature
baseline (FR-007, FR-020, SC-007).

## Format: `[ID] [P?] [GATE?] Description`

- **[P]** — parallel-safe within its phase (different files, no shared-edit conflict)
- **[GATE]** — phase-completion gate task; not parallel
- Every task cites the FR(s) it satisfies and the file(s) it touches
- Every production-code task is paired with (or precedes) a test task

## Constitution rules — apply to every code task

1. Every `.cpp` includes `"Pch.h"` as the **first** include; no angle-bracket
   includes anywhere except inside `Pch.h`; project headers use quoted includes
2. Function comments live in `.cpp` (80-`/` delimiters), not in the header
3. Function/operator spacing: `func()`, `func (a, b)`, `if (...)`, `for (...)`
4. All locals at top of scope, column-aligned (type / `*`-`&` column / name / `=` / value)
5. No non-trivial calls inside macro arguments — store result first
6. ≤ 50 lines per function (100 absolute); ≤ 2 indent levels beyond EHM (3 max)
7. Exactly 5 blank lines between top-level constructs; exactly 3 between
   top-of-function declarations and first statement
8. EHM pattern (`HRESULT hr = S_OK; … Error: … return hr;`) on every fallible
   function; never bare `goto Error`
9. No magic numbers (except 0/1/-1/nullptr/sizeof) — see `plan.md` Constitution
   table for the named-constants list
10. File names: PascalCase, no underscores (`DiskIIEventRing.cpp`, not
    `disk_ii_event_ring.cpp`)
11. C-style cast spacing: `(float) value` with one space before the value
12. Hungarian for file-scope statics (`g_pszWindowClass`, `s_kColumnWidths`)
13. American English throughout

---

## Phase 0: Contracts & Declarations

**Purpose**: Stand up interfaces and types. No behavior yet.

- [ ] T001 [P] Create `CassoEmuCore/Devices/IDiskIIEventSink.h` declaring the
      abstract event-sink interface with exactly the 12 methods listed in
      FR-006 (four-event motor lifecycle: `OnMotorCommandOn`,
      `OnMotorEngaged`, `OnMotorCommandOff`, `OnMotorDisengaged`; plus
      `OnHeadStep`, `OnHeadBump`, `OnAddressMark`, `OnDataMarkRead`,
      `OnDataMarkWrite`, `OnDriveSelect`, `OnDiskInserted`,
      `OnDiskEjected`). All `void`, all infallible. Header-only; no STL
      includes beyond what is in `Pch.h`. Leave room in the doc-comment
      for the future `OnMotorAtSpeed` (A-009). (FR-006, A-009)
- [ ] T002 [P] Create `CassoEmuCore/Devices/DiskIIEvent.h` declaring the
      `DiskIIEvent` POD struct: `EventCategory category` (enum class
      `{Controller, Audio}` per FR-023), event-type enum covering BOTH
      controller events (`MotorCommandOn`, `MotorEngaged`,
      `MotorCommandOff`, `MotorDisengaged`, `HeadStep`, `HeadBump`,
      `AddrMark`, `DataRead`, `DataWrite`, `DriveSelect`, `DiskInserted`,
      `DiskEjected`, `EventsLost`) and audio outcomes (`AudioStarted`,
      `AudioRestarted`, `AudioContinued`, `AudioSilent`,
      `AudioLoopStarted`, `AudioLoopStopped`), `uint64_t` cycle counter
      snapshot, and a payload union covering the per-type integer payload
      (qt prev/new for step, qt for bump, track/sector/volume for addr
      mark, sector/byte-count for data mark, drive number for
      select/insert/eject, count for lost, `{SoundKind, drive,
      SilentReason?}` for audio outcomes). Total size ≤ 24 bytes; verify
      with a `static_assert`. (FR-006, FR-021, FR-023, NFR-003)
- [ ] T002a [P] Create `CassoEmuCore/Audio/IDriveAudioEventSink.h`
      declaring the abstract audio-outcome sink interface per FR-021
      with exactly six `void`/infallible methods (`OnAudioStarted`,
      `OnAudioRestarted`, `OnAudioContinued`, `OnAudioSilent`,
      `OnAudioLoopStarted`, `OnAudioLoopStopped`). In the same header
      (or sibling `SoundKind.h` / `SilentReason.h`), declare:
      - `enum class SoundKind { MotorLoop, HeadStep, HeadStop,
        DoorOpen, DoorClose };`
      - `enum class SilentReason { DriveAudioDisabled, BufferMissing,
        NoSourceRegistered, ColdBootSuppression };`
      (FR-021, FR-025)
- [ ] T003 [P] Create `Casso/DiskIIEventDisplay.h` declaring the UI-thread
      display struct: pre-formatted wall string
      (`std::array<wchar_t, 16>` for `HH:MM:SS.mmm`), pre-formatted
      uptime string (`std::array<wchar_t, 12>` for `MM:SS.mmm`),
      pre-formatted cycle string (`std::array<wchar_t, 24>`),
      `EventCategory` + event-type enum (mirroring `DiskIIEvent`'s),
      `int drive` (for the FR-014 Drive filter, -1 if N/A),
      `int track` and `int sector` (for FR-014a/b filters, INT_MIN
      sentinel if N/A), pre-formatted detail string (`std::wstring`
      or `std::array<wchar_t, 64>`). Total size ≤ 96 bytes target;
      verify with `static_assert` if using fixed buffers. (FR-005,
      FR-014, NFR-003)
- [ ] T004 [P] Create `CassoEmuCore/Devices/DiskIIEventRing.h` per plan.md
      §"SPSC Ring Buffer" — class declaration only, with `kCapacity = 4096`
      named constant, `TryPush`, `TryPop`, `Drain (DiskIIEvent*, uint32_t)`,
      `ApproxSize`. Two `alignas(64)` `std::atomic<uint32_t>` index members.
      Header-only; no `.cpp` yet. (FR-009, NFR-003)
- [ ] T005 [P] Create `CassoEmuCore/Devices/DiskIIAddressMarkWatcher.h`
      declaring the class with `ObserveNibble (uint8_t nibble)` and
      `SetEventSink (IDiskIIEventSink*)`. Member fields: address-mark and
      data-mark state enums, accumulator fields (vol/trk/sec/chk halves),
      cached most-recent sector number, 3-nibble rolling window for
      epilogue peek-ahead, data-mark body counter. Named constants for the
      prologue/epilogue bytes and `kDataNibbleCount = 342`. (FR-008)
- [ ] T006 Add `CassoEmuCore/Devices/{IDiskIIEventSink,DiskIIEvent,DiskIIEventRing,DiskIIAddressMarkWatcher}.h`,
      `CassoEmuCore/Audio/IDriveAudioEventSink.h`, and
      `Casso/DiskIIEventDisplay.h` to their respective vcxproj files
      under the header item-group. Build must succeed (declarations only,
      no `.cpp` yet).
- [ ] T007 [GATE] Run `scripts\Build.ps1` and the full unit-test suite;
      verify no regressions from header-only additions.

---

## Phase 1: SPSC Ring Buffer

**Purpose**: Implement and exhaustively test the SPSC ring.

- [ ] T010 Create `CassoEmuCore/Devices/DiskIIEventRing.cpp`. Implement
      `TryPush`, `TryPop`, `Drain`, `ApproxSize` per `research.md` §4.2.
      Use `std::memory_order_relaxed` on same-side index, acquire/release
      on cross-side. (FR-009)
- [ ] T011 [P] Create `UnitTest/Devices/DiskIIEventRingTests.cpp`. Single-
      threaded tests: empty pop returns false; push fills to capacity; one-
      past-capacity push returns false without corrupting state; drain
      empties; wrap-around (push 4096, pop 2048, push 2048, drain 4096)
      preserves FIFO order. (FR-009)
- [ ] T012 [P] In `DiskIIEventRingTests.cpp`, add a two-threaded stress
      test: one producer pushing 100,000 monotonically-increasing payloads,
      one consumer draining; assert no torn reads, no out-of-order pops,
      pushes that returned `false` produce a gap in the consumer's
      observed sequence that matches the producer's count of failed
      pushes. (FR-009, FR-010)
- [ ] T013 [GATE] Run the new ring tests under both Debug and Release
      configurations (Release is required because relaxed-ordering bugs
      are typically masked by Debug's lock-step memory model on x86).

---

## Phase 2: Address-Mark / Data-Mark Watcher

**Purpose**: Implement and test the passive nibble-stream watcher.

- [ ] T020 Create `CassoEmuCore/Devices/DiskIIAddressMarkWatcher.cpp`.
      Implement `ObserveNibble`, `StepAddrMarkState`, `StepDataMarkState`,
      and the 4-and-4 decoder helper. Verify the address-mark checksum
      (`chk == vol XOR trk XOR sec`); on mismatch, silently reset to
      `S_ADDR_IDLE`. Fire `OnAddressMark` only on checksum success. For
      data marks, fire `OnDataMarkRead (cachedSector, 256)` on epilogue
      detection within the expected window. (FR-008)
- [ ] T021 [P] Create `UnitTest/Devices/DiskIIAddressMarkWatcherTests.cpp`.
      Tests:
      - Stock DOS 3.3 address-mark stream (D5 AA 96 + 4-and-4 fields with
        good checksum + DE AA EB) fires exactly one `OnAddressMark` with
        decoded (vol, trk, sec).
      - Same stream with corrupted checksum fires zero `OnAddressMark`.
      - Random-nibble torture stream (1 MB of `rand() & 0xFF`) fires
        zero `OnAddressMark` (false-positive guard).
      - Data-mark stream (D5 AA AD + 342 nibbles + DE AA EB) fires exactly
        one `OnDataMarkRead`.
      - Data-mark without preceding address mark still fires
        `OnDataMarkRead` (sector field falls back to cached `S?` indicator,
        formatted at the UI layer).
      - Interleaved address + data marks (the normal sector cadence) fire
        in expected order with `OnDataMarkRead` using the most recently
        decoded sector number.
      (FR-008)
- [ ] T022 [GATE] Run watcher tests; verify all pass.

---

## Phase 2a: Track/Sector Filter Parser

**Purpose**: Implement and test the FR-014a/b filter-expression parser
used by the dialog's Track and Sector inputs.

- [ ] T025 [P] Create `Casso/TrackSectorPredicate.h` declaring a class
      with `static TrackSectorPredicate Parse (std::wstring_view expr,
      bool rawQt)` and `bool Matches (int value) const` and
      `bool MatchesQuarterTrack (int qt) const`. Internal representation:
      `std::vector<Range>` where `Range = {int lo, int hi, bool isQt}`,
      plus `std::vector<RejectedSpan>` where
      `RejectedSpan = {int beginUtf16, int endUtf16}` recording the
      half-open UTF-16 code-unit byte range of every token the parser
      rejected within the original `expr` (used by the dialog's
      FR-014e squiggle layer; ignored by `Matches`). Expose
      `const std::vector<RejectedSpan>& RejectedSpans () const noexcept`.
      An empty parse result MUST match all values. (FR-014a, FR-014b,
      FR-014e)
- [ ] T026 Create `Casso/TrackSectorPredicate.cpp`. Implement `Parse`:
      tokenize on commas, then for each token detect range (`a-b`) vs.
      single value vs. decimal quarter-track (`17.25`, `17.5`,
      `17.75`). Convert decimal-qt to integer-qt via `(whole * 4) +
      quarter_index`. Honor the `rawQt` flag (FR-014a — when set, bare
      integers are interpreted as quarter-tracks rather than whole
      tracks). Tolerate surrounding whitespace; silently skip
      malformed tokens (e.g., `abc`) — they MUST NOT throw or block
      other valid tokens in the same expression — but record their
      `{begin, end}` UTF-16 offsets within `expr` as a `RejectedSpan`
      (the span MUST cover only the token text itself, not the
      surrounding whitespace or comma separators, so the dialog can
      squiggle just the token). NO max-value
      validation: a token of `999` parses as `{999, 999, false}` and
      simply fails to match in practice (FR-014a NO max-value rule).
      (FR-014a, FR-014b, FR-014e)
- [ ] T027 [P] Create `UnitTest/Casso/TrackSectorPredicateTests.cpp`.
      Tests:
      - Empty expression matches every value (returns true for all
        inputs) and `RejectedSpans()` is empty.
      - `17` matches 17 only.
      - `17.5` matches quarter-track 70 only (`MatchesQuarterTrack`).
      - `0-2` matches 0, 1, 2 (and not 3 or -1).
      - `0,17,34` matches each listed value and nothing else.
      - Mixed `0-2,17,30-34` works as the union.
      - `rawQt=true` with input `68` matches quarter-track 68
        (whole-track 17); same input with `rawQt=false` matches
        whole-track 68 (which is out of standard range but the
        parser MUST NOT reject it — FR-014a's "no max-value
        validation" rule).
      - `abc` parses to an empty predicate (matches everything per
        FR-014a's "malformed token = no-match for that token" rule);
        `1, abc, 3` parses to `{1, 3}` matching 1 and 3.
      - Whitespace tolerated: `  0 - 2 , 17  ` parses correctly.
      - `999` parses successfully and matches only 999 (no max-value
        rejection — FR-014a, FR-014b).
      - **Rejected-span recording (FR-014e)**: for input
        `0-2, abc, 17`, `RejectedSpans()` returns exactly one span
        whose `[begin, end)` UTF-16 offsets cover the substring
        `abc` (NOT the surrounding spaces or commas). For input
        `xx, 5, yy` returns exactly two spans, one covering `xx`
        and one covering `yy`. For a fully valid input
        (`0-2, 17, 30-34`) the rejected-span vector is empty. For
        an all-junk input (`abc, def`) returns two spans covering
        `abc` and `def` and the predicate matches nothing valid
        (but does not match everything — it matches nothing
        because there are no ranges; the "empty = match all" rule
        applies only when the input is empty/whitespace, not when
        every token was rejected).
      (FR-014a, FR-014b, FR-014e)
- [ ] T028 [GATE] Run parser tests.

---

## Phase 3: Controller Integration

**Purpose**: Wire `IDiskIIEventSink*` + watcher into `DiskIIController`
without changing observable behavior.

- [ ] T030 In `CassoEmuCore/Devices/DiskIIController.h`, add
      `IDiskIIEventSink* m_eventSink = nullptr;`, `DiskIIAddressMarkWatcher
      m_addrMarkWatcher;`, and `void SetEventSink (IDiskIIEventSink* sink)
      noexcept;`. (FR-007)
- [ ] T031 In `CassoEmuCore/Devices/DiskIIController.cpp`, implement
      `SetEventSink` (assign pointer; propagate to `m_addrMarkWatcher`).
      Add guarded fire sites at:
      - `HandleSwitch` case `0x9`: ALWAYS fire `OnMotorCommandOn`
        (every strobe, including no-op re-strobes on an already-engaged
        motor). Additionally, on false→true edge of `m_motorOn`, fire
        `OnMotorEngaged`. (FR-006: four-event motor lifecycle)
      - `HandleSwitch` case `0x8`: ALWAYS fire `OnMotorCommandOff`
        (every strobe; arms the spindown timer when engaged).
      - `Tick` spindown branch for `OnMotorDisengaged` (only on true→false
        transition after the spindown counter expires).
      - `HandlePhase` after `m_quarterTrack += qtDelta`, branch on
        clamp-vs-no-clamp to fire `OnHeadStep (prevQt, newQt)` or
        `OnHeadBump (atQt)` (mutually exclusive).
      - `HandleSwitch` cases `0xA` / `0xB` for `OnDriveSelect (newDrive)`.
      - `MountImage` for `OnDiskInserted (drive)`.
      - `EjectImage` for `OnDiskEjected (drive)`.
      Every fire site guarded by `if (m_eventSink != nullptr) { ... }`.
      (FR-006, FR-007)
- [ ] T032 In `DiskIIController::NibbleReadByte`, call
      `m_addrMarkWatcher.ObserveNibble (returnedNibble)` after the nibble
      is computed but before returning. The watcher's invocation must
      not modify the returned value. (FR-008)
- [ ] T033 [P] Create `UnitTest/Devices/DiskIIControllerEventTests.cpp`
      with a recording mock sink that captures every fired event into a
      `std::vector<DiskIIEvent>`. Tests:
      - Cold-boot fixture sequence (DOS 3.3 in-memory image, ~5000
        CPU-cycle slice) fires events in the expected order: drive select,
        `MotorCommandOn` → `MotorEngaged`, head steps, head bump(s),
        address marks, data reads.
      - Re-strobing `$C0E9` on an already-engaged motor fires
        `OnMotorCommandOn` again but does NOT re-fire `OnMotorEngaged`.
      - `$C0E8` always fires `OnMotorCommandOff`; `OnMotorDisengaged`
        fires only after the spindown timer expires.
      - `HandlePhase` with `qtDelta == 0` fires zero events.
      - `HandlePhase` that would step past `qt 0` fires `OnHeadBump`,
        not `OnHeadStep`, exactly once.
      - `MountImage` / `EjectImage` fire one event each.
      - No-sink build (attach a sink, immediately revoke, then run the
        same fixture) produces byte-identical nibble stream and head
        positions vs. the original baseline `DiskIIControllerTests`.
      (FR-006, FR-007, FR-020, SC-007)
- [ ] T034 [P] Add a regression assertion to
      `DiskIIControllerEventTests.cpp`: with a recording sink attached
      to a stub that records but does not consume from the ring,
      `DiskIIControllerTests`'s existing cold-boot read fixture produces
      the same nibble byte sequence as without the sink. (FR-008, SC-007)
- [ ] T035 [GATE] Run the full unit-test suite. Existing
      `DiskIIControllerTests` MUST pass byte-identically; new
      `DiskIIControllerEventTests` MUST pass.

---

## Phase 3a: DiskIIAudioSource Integration

**Purpose**: Wire `IDriveAudioEventSink*` into `DiskIIAudioSource`,
migrate its existing motor listeners to the new four-event motor
lifecycle, and fire audio outcomes from every audio decision site
(including the cold-boot insert suppression).

- [ ] T036 In `CassoEmuCore/Audio/DiskIIAudioSource.h`, add
      `IDriveAudioEventSink* m_audioEventSink = nullptr;` and
      `void SetAudioEventSink (IDriveAudioEventSink* sink) noexcept;`.
      (FR-022)
- [ ] T037 In `CassoEmuCore/Audio/DiskIIAudioSource.cpp`:
      - Migrate existing `OnMotorStart` / `OnMotorStop` overrides to
        `OnMotorEngaged` / `OnMotorDisengaged` (the audio-relevant
        edges per FR-006). The other two new motor events
        (`OnMotorCommandOn` / `OnMotorCommandOff`) are NOT audio-
        relevant and MUST NOT be overridden.
      - At each audio decision moment inside the existing hooks
        (`OnMotorEngaged` → `OnAudioLoopStarted(MotorLoop, drive)` or
        `OnAudioSilent(MotorLoop, drive, …)`; `OnMotorDisengaged` →
        `OnAudioLoopStopped(MotorLoop, drive)`; `OnHeadStep` →
        Started / Restarted / Continued / Silent for HeadStep;
        `OnHeadBump` → Started / Restarted / Silent for HeadStop;
        `OnDiskInserted` → Started / Silent for DoorClose;
        `OnDiskEjected` → Started / Silent for DoorOpen), fire exactly
        one of the six `IDriveAudioEventSink` methods describing the
        outcome. Each fire site guarded by `if (m_audioEventSink !=
        nullptr) { ... }`. With no sink attached, audio behavior MUST
        be 100% byte-identical to the pre-feature `DiskIIAudioSource`.
      (FR-022)
- [ ] T038 In the cold-boot insert suppression path inside
      `DiskIIAudioSource::OnDiskInserted` (per spec-005 FR-013), when
      the insert sound is suppressed because the mount happened before
      the first emulator slice ran, fire `OnAudioSilent(SoundKind::DoorClose,
      drive, SilentReason::ColdBootSuppression)` so the suppression
      decision is visible in the debug log. The `OnDiskInserted`
      controller event still fires normally — the suppression is a
      pure audio decision. (FR-025)
- [ ] T039 [P] Create `UnitTest/Audio/DiskIIAudioSourceEventSinkTests.cpp`
      with a recording mock `IDriveAudioEventSink`. Tests (one per
      outcome path, satisfying SC-011 / SC-012 / SC-013 / SC-014):
      - First `OnHeadStep` after a long quiet period produces one
        `OnAudioStarted(HeadStep, drive)` and no other outcome. (SC-011)
      - Two `OnHeadStep` invocations within the duration of one
        HeadStep sample produce `OnAudioStarted` followed by
        `OnAudioRestarted`. (SC-012)
      - `OnHeadStep` during the spec-005 FR-005 seek-mode window
        produces `OnAudioContinued`, not Started/Restarted. (SC-013)
      - With drive-audio toggle off (mock `Options::IsDriveAudioEnabled
        == false`), `OnHeadStep` produces `OnAudioSilent(HeadStep,
        drive, DriveAudioDisabled)`. (SC-014)
      - With a missing sample buffer (mock returns nullptr for the
        HeadStep buffer), `OnHeadStep` produces `OnAudioSilent(...,
        BufferMissing)`. (SC-014)
      - With no source registered for the active drive (the audio
        subsystem reports no `DiskIIAudioSource` for that drive),
        the equivalent dispatch site produces `OnAudioSilent(...,
        NoSourceRegistered)`. (SC-014)
      - Cold-boot `OnDiskInserted` produces `OnAudioSilent(DoorClose,
        drive, ColdBootSuppression)`. (SC-014, FR-025)
      - With no audio sink attached, all the above scenarios produce
        byte-identical audio output (verified by `DriveAudioMixerTests`'
        existing fixtures still passing).
      (FR-022, FR-025, SC-011, SC-012, SC-013, SC-014)
- [ ] T039a [GATE] Run audio-sink tests; full existing
      `DriveAudioMixerTests` MUST still pass byte-identically.

---

## Phase 4: UI-Thread Projection Helper

**Purpose**: Implement the non-Win32 projection helper that drains the
ring into the deque, formats per FR-005, applies the rolling cap, and
inserts the lossage marker. Pure-data-structure logic.

- [ ] T040 [P] Create `Casso/DebugDialogProjection.h` declaring a
      class with `DrainAndProject (DiskIIEventRing&, std::deque<DiskIIEventDisplay>&,
      uint32_t droppedCount, std::chrono::steady_clock::time_point uptimeAnchor)`,
      plus a `FormatEvent (const DiskIIEvent&, std::chrono::steady_clock::time_point uptimeAnchor, DiskIIEventDisplay&)`
      helper, plus named constants `kDisplayDequeCap = 100000`. (FR-004a,
      FR-005, FR-010, FR-012)
- [ ] T041 Create `Casso/DebugDialogProjection.cpp`. Implement
      `FormatEvent` per the FR-005 table:
      - **Wall** column: `HH:MM:SS.mmm` from
        `std::chrono::system_clock::now()` snapshot at format time.
      - **Uptime** column: `MM:SS.mmm` from `(steady_clock::now() -
        uptimeAnchor)` where `uptimeAnchor` is the shell-owned anchor
        reseeded on `SoftReset` / `PowerCycle` per FR-004a.
      - **Cycle** column: decimal with thousands separators.
      - **Event** column: enum-to-fixed-string table covering BOTH
        controller events (`MOTOR COMMAND ON`, `MOTOR ENGAGED`,
        `MOTOR COMMAND OFF`, `MOTOR DISENGAGED`, `HEAD STEP`, etc.)
        AND audio outcomes (`AUDIO STARTED`, `AUDIO RESTARTED`,
        `AUDIO CONTINUED`, `AUDIO SILENT`, `AUDIO LOOP STARTED`,
        `AUDIO LOOP STOPPED`). The `EventCategory` tag selects the
        sub-table.
      - **Detail** column: per-event-type per FR-005 (controller details
        unchanged from prior draft; audio details formatted as
        `kind=<SoundKind> drive=<N>` or, for Silent,
        `kind=<SoundKind> drive=<N> reason=<SilentReason>`).
      Implement `DrainAndProject` per plan.md §"Data Flow Per Event"
      step 5: if `droppedCount > 0`, push a synthetic `EventsLost`
      display entry before draining; drain via `ring.Drain`; push each
      formatted entry; if the deque exceeds `kDisplayDequeCap`,
      `pop_front` until restored. (FR-004, FR-004a, FR-005, FR-010,
      FR-012, FR-021, FR-023, FR-025)
- [ ] T042 [P] Create `UnitTest/Casso/DebugDialogProjectionTests.cpp`
      (or under `UnitTest/Devices/` if the test project organization
      requires it). Tests:
      - `FormatEvent` produces the exact strings called out by FR-005 for
        every event type (table-driven test) including:
        - All four motor lifecycle events (`MOTOR COMMAND ON`,
          `MOTOR ENGAGED`, `MOTOR COMMAND OFF`, `MOTOR DISENGAGED`).
        - All six audio outcomes with sensible `kind=` / `drive=` /
          `reason=` formatting.
        - `AUDIO SILENT kind=DoorClose drive=1 reason=ColdBootSuppression`
          specifically (FR-025).
      - Wall column produces a `HH:MM:SS.mmm`-shape string matching
        regex `^\d\d:\d\d:\d\d\.\d\d\d$`.
      - Uptime column resets to `00:00.???` when the test passes a
        freshly-reseeded `uptimeAnchor` (simulates SoftReset per
        FR-004a).
      - `DrainAndProject` with an empty ring and zero droppedCount leaves
        the deque unchanged.
      - `DrainAndProject` with N events in the ring appends N entries to
        the deque in FIFO order.
      - `DrainAndProject` with droppedCount > 0 inserts exactly one
        `EventsLost` entry at the position where the loss occurred (test
        fixture: pre-populate deque with 2 entries, set droppedCount = 7,
        push 3 events into ring, drain; expect deque order: orig1, orig2,
        [7 events lost], drained1, drained2, drained3).
      - Rolling-cap enforcement: pre-populate deque to `kDisplayDequeCap`,
        drain 1 event, assert deque size still equals the cap and the
        front entry advanced by one.
      (FR-004, FR-004a, FR-005, FR-010, FR-012, FR-025)
- [ ] T043 [GATE] Run projection tests; verify all pass.

---

## Phase 5: Dialog Skeleton (Win32 wiring, no event data yet)

**Purpose**: Create the modeless dialog window, ListView in virtual mode,
filter checkboxes, buttons. Render an empty list.

- [ ] T050 Create `Casso/DiskIIDebugDialog.h` declaring the class with
      `Create (HWND parentHwnd)`, `Show()`, `Hide()`, `Destroy()`,
      `AttachSinks (DiskIIController&, DiskIIAudioSource*)`,
      `DetachSinks (DiskIIController&, DiskIIAudioSource*)`,
      and overrides for BOTH `IDiskIIEventSink` and `IDriveAudioEventSink`
      (CPU-thread side: pack a tagged `DiskIIEvent`, push to ring,
      bump dropped counter on full). Member fields:
      `HWND m_hwnd`, `HWND m_listView`,
      event-type checkbox handles (one per FR-014 category),
      `HWND m_driveRadio[3]` (All/Drive 1/Drive 2),
      `HWND m_trackEdit`, `HWND m_sectorEdit`, `HWND m_trackRawQtCheck`,
      `HWND m_audioMasterCheck`, `HWND m_audioSubCheck[4]`
      (Started/Restarted/Continued/Silent),
      `HWND m_pauseButton`, `HWND m_clearButton`,
      `DiskIIEventRing m_ring`,
      `std::atomic<uint32_t> m_droppedSinceLastDrain`,
      `std::deque<DiskIIEventDisplay> m_deque`,
      `std::vector<uint32_t> m_filteredIndices`,
      `DebugDialogProjection m_projection`,
      `FilterState m_filter`, `bool m_paused`,
      `std::array<LogicalColumn, 5> m_columns`,
      `UINT_PTR m_timerId`. (FR-001..FR-004, FR-009, FR-011, FR-014,
      FR-021, FR-023, FR-024, FR-026, FR-027)
- [ ] T051 Create `Casso/DiskIIDebugDialog.cpp`. Implement `Create`
      (programmatic `CreateWindowEx` for an overlapped, sizeable window
      with the standard frame; size and position from named constants).
      In `WM_CREATE` / `WM_INITDIALOG`-equivalent: create the ListView
      (`LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS`). Seed
      `m_columns[0..4]` with id / headerText / `defaultWidth` (from
      named constants `kColWallWidth = 110`, `kColUptimeWidth = 90`,
      `kColCycleWidth = 110`, `kColEventWidth = 110`,
      `kColDetailWidth = 360`) / `savedWidth = defaultWidth` /
      `visible = true` / `autoSizedYet = false`. Call `RebuildListViewColumns()`
      (see Phase 10a / T106) to populate the LV's column set from the
      logical model; this also runs the FR-027 first-time auto-size
      pass on each visible column. (FR-026, FR-027)
      Create the FR-014 filter controls above the ListView:
      - Event-type checkboxes (Motor, HeadStep, HeadBump, AddrMark,
        Read, Write, Door, DriveSelect, Audio-master), all initially
        checked.
      - Drive radio group (All / Drive 1 / Drive 2), default = All.
      - Track and Sector text inputs (initially empty = match all),
        created as **RichEdit 4.1 controls** (class `RICHEDIT50W`,
        named constant `kFilterRichEditClass`; load
        `kFilterRichEditModule` = `L"msftedit.dll"` once via
        `LoadLibraryW` in `Create` BEFORE the first
        `CreateWindowExW` call for these inputs, so the class is
        registered), NOT standard `EDIT` controls. A read-only
        `STATIC` label MUST be placed directly beneath each input
        (initially empty text) to host the FR-014e
        `Ignored: <token>, <token>` summary. A "raw qt" checkbox
        sits alongside the Track input.
      - Audio sub-checkboxes (Started, Restarted, Continued, Silent),
        all initially checked, visually disabled when Audio-master is
        unchecked (FR-014c).
      - Pause and Clear buttons.
      (FR-001, FR-003, FR-004, FR-014, FR-014a, FR-014b, FR-014c,
      FR-014e, FR-026, FR-027)
- [ ] T052 Implement window-class registration (lazy, on first `Create`)
      using a file-scope `static const wchar_t g_pszDebugWndClass[] =
      L"CassoDiskIIDebugWindow";`. Hungarian per constitution. Register
      with `LoadCursor (NULL, IDC_ARROW)` and a default brush. (FR-001)
- [ ] T053 Implement `Show` / `Hide` / `Destroy` and a basic
      `WM_CLOSE` handler that hides (does not destroy). `WM_DESTROY`
      kills the timer and tears down state. (FR-001)
- [ ] T054 [P] Create `UnitTest/Devices/DiskIIDebugDialogTests.cpp`
      covering the non-Win32 portions: dialog's `FilterState` defaults
      to "all event-type checkboxes set, Drive=All, empty Track/Sector
      predicates, Audio master checked, all four audio sub-toggles
      checked" (FR-014, FR-014c), `m_paused` defaults to false,
      `m_columns` is seeded with five `LogicalColumn` entries in id
      order, each with `visible = true`, `autoSizedYet = false`, and
      `defaultWidth == savedWidth` from the five named constants
      (FR-026). No `HWND` is created.
      (FR-014, FR-014c, FR-015, FR-026)
- [ ] T055 [GATE] Build the shell, manually open the dialog from a debug
      menu stub, verify the empty window renders with 5 columns
      (Wall, Uptime, Cycle, Event, Detail), the FR-014 filter controls
      (event-type checkboxes, Drive radio, Track/Sector text inputs,
      Audio sub-checkboxes), 2 buttons, and a resizeable frame.
      (Manual gate; automated UI test is out of scope per NFR-002.)

---

## Phase 6: Drain Timer + Auto-Tail + GETDISPINFO

**Purpose**: Wire the 30 Hz drain and the ListView row-fetch path.

- [ ] T060 In `DiskIIDebugDialog.cpp`, implement `WM_TIMER` handler per
      plan.md §"Auto-Tail Scroll Algorithm". Capture `wasAtTail` BEFORE
      invoking projection. Call `m_projection.DrainAndProject(...)`. If
      deque size changed, call `ListView_SetItemCountEx` with
      `LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL`. If `wasAtTail`, call
      `ListView_EnsureVisible` on the new last index. (FR-011, FR-013)
- [ ] T061 Start the timer (`SetTimer (m_hwnd, m_timerId, kDrainTimerMs,
      nullptr)` with `kDrainTimerMs = 33`) in `WM_CREATE` and kill it in
      `WM_DESTROY`. Skip drain (early-return from `WM_TIMER`) when the
      window is hidden or minimized (`IsWindowVisible` returns false) or
      `m_paused` is true. (FR-011, FR-015)
- [ ] T062 Implement `LVN_GETDISPINFO` handler. Translate the `iItem`
      index via `m_filteredIndices[iItem]` (Phase 7 populates this
      vector; until then, seed it with the identity mapping
      `[0, 1, ... deque.size()-1]`). For each requested column, the
      ListView's `iSubItem` is the **visible-subset ordinal** — map it
      back to the `LogicalColumn::id` via the active visible-subset
      list (kept by `RebuildListViewColumns`, e.g., a small
      `std::array<int, 5> m_visibleOrdinalToLogicalId` rebuilt every
      time the column set changes). Use the resolved logical id to
      pick the right field of `m_deque[deqIdx]`. Hidden columns are
      simply absent from the LV, so the dispatcher never sees their
      ordinal. (FR-003, FR-005, FR-026, FR-027)
- [ ] T063 [P] Add a `DiskIIDebugDialogTests.cpp` headless test for the
      auto-tail decision function: extract the `wasAtTail` computation
      into a free function `bool ComputeWasAtTail (int topIndex, int
      countPerPage, int totalCount)` testable without an HWND. Verify
      truth table: at tail, scrolled-up-by-1, scrolled-up-by-N, empty
      list, partial-last-row visibility. (FR-013)
- [ ] T064 [GATE] Build, manually open the dialog before a DOS 3.3 cold
      boot, observe the event stream tail-following. Verify scrolling
      up freezes the view; verify scrolling back to the bottom re-enables
      auto-tail on the next event.

---

## Phase 7: Filter Projection

**Purpose**: Wire the FR-014 filter controls to the displayed projection.

- [ ] T070 In `DiskIIDebugDialog.cpp`, handle `WM_COMMAND` for each
      event-type checkbox, each Drive radio button, the Audio-master,
      each Audio sub-checkbox, and `EN_CHANGE` for the Track/Sector
      text inputs (and the "raw qt" checkbox). On any change, update
      `m_filter` (re-parsing the Track and Sector inputs via
      `TrackSectorPredicate::Parse` from Phase 2a) and rebuild
      `m_filteredIndices` from `m_deque` by running `MatchesFilter` on
      every entry. After each Track / Sector re-parse, call the
      FR-014e helpers (T074): `ApplyRejectedTokenSquiggles` on the
      RichEdit + `SetIgnoredTokensLabel` on the static beneath it,
      using the predicate's `RejectedSpans()`. When the Audio-master
      is unchecked, visually disable
      (grey out) the four sub-checkboxes via `EnableWindow(..., FALSE)`
      but preserve their checked state for restoration when Audio-master
      is re-checked (FR-014c). (FR-014, FR-014a, FR-014b, FR-014c,
      FR-014e)
- [ ] T071 Update the `LVN_GETDISPINFO` handler so the virtual row count
      reported by `ListView_SetItemCountEx` equals
      `m_filteredIndices.size()` (not `m_deque.size()`). On drain, after
      `DrainAndProject` appends new deque entries, the incremental tail
      of `m_filteredIndices` MUST be computed (run `MatchesFilter` only
      on the newly-appended entries, not the entire deque) and appended
      to the vector. (FR-014)
- [ ] T072 [P] Add `DebugDialogProjectionTests.cpp` cases that exercise
      the full FR-014 filter composition:
      - With a fixed deque of one event of each event-type, every
        single-checkbox-on / others-off configuration shows only the
        corresponding events.
      - All-checked shows everything; all-unchecked shows nothing;
        re-checking after un-checking shows the events again in
        chronological order (regression for the "filtering is a
        projection, not a drop" rule).
      - Drive=Drive 1 filter shows only events whose `drive` field is
        1; Drive=All shows both.
      - Track filter `0` shows only events with `track == 0`; Track
        filter `0-2,17` shows events with track in {0,1,2,17}; empty
        Track shows everything.
      - Sector filter `0-15` shows standard sector events; sector
        filter `999` matches no events but DOES NOT throw (FR-014b
        "no max-value validation").
      - Combined `Drive=1 + Track=0 + Read-only + Audio-master-off`
        shows only `DATA READ` rows from drive 1 reading track 0
        (SC-016).
      - Audio-master unchecked hides ALL six audio outcomes
        regardless of sub-toggle state; Audio-master checked with
        only `Silent` sub-toggle on shows `AUDIO SILENT` rows and
        hides the other three audio outcome rows (loop events still
        show — they are not gated by sub-toggles per FR-014c).
      (FR-014, FR-014a, FR-014b, FR-014c, SC-016)
- [ ] T073 [GATE] Manual test: during a DOS 3.3 boot, toggle each
      filter control and verify the displayed events change
      appropriately and restore on re-check.

### Phase 7a: Track/Sector Squiggle Helper (FR-014e)

- [ ] T074 [P] Create `Casso/RichEditSquiggle.h` declaring a free
      helper `void ApplyRejectedTokenSquiggles (HWND hRichEdit,
      const std::vector<TrackSectorPredicate::RejectedSpan>& spans)`
      and `void SetIgnoredTokensLabel (HWND hStatic,
      std::wstring_view originalExpr,
      const std::vector<TrackSectorPredicate::RejectedSpan>& spans)`.
      The first helper saves the RichEdit selection
      (`EM_EXGETSEL`), selects `{0, -1}` and applies a default
      `CHARFORMAT2W` that clears `CFM_UNDERLINETYPE |
      CFM_UNDERLINECOLOR`, then for each span selects that
      range and applies a `CHARFORMAT2W` with
      `dwMask = CFM_UNDERLINETYPE | CFM_UNDERLINECOLOR`,
      `bUnderlineType = CFU_UNDERLINEWAVE`,
      `bUnderlineColor = kFilterSquiggleUnderlineColor`
      (= `CFU_UNDERLINECOLOR_RED`), then restores the saved
      selection (`EM_EXSETSEL`). The second helper builds the
      `Ignored: tok1, tok2` UTF-16 string from `originalExpr` +
      `spans` and calls `SetWindowTextW (hStatic, ...)` (empty
      string when `spans` is empty). (FR-014e)
- [ ] T075 [P] Create `UnitTest/Casso/RichEditSquiggleTests.cpp`.
      Because the helpers touch real Win32 controls, factor the
      label-building logic into a pure free function
      `std::wstring BuildIgnoredTokensLabel (std::wstring_view expr,
      const std::vector<TrackSectorPredicate::RejectedSpan>& spans)`
      that the test exercises directly:
      - Empty `spans` → returns empty string.
      - One span over `abc` in `0-2, abc, 17` → returns
        `L"Ignored: abc"`.
      - Two spans over `xx` and `yy` in `xx, 5, yy` → returns
        `L"Ignored: xx, yy"`.
      - Spans MUST be slicing into `expr` via the recorded UTF-16
        offsets — no re-tokenization, no whitespace inclusion.
      The Win32-touching squiggle apply is validated manually
      under T073's GATE plus the new manual check below.
      (FR-014e)
- [ ] T076 [GATE] Manual: type `0-2, abc, 17` into the Track
      RichEdit, wait 250 ms (debounce flush), verify (a) `abc`
      gains a red wavy underline; the rest of the text is
      undecorated, (b) the label beneath the input reads
      `Ignored: abc`, (c) the filter still applies as
      `{0, 1, 2, 17}` (the squiggle does not block filtering),
      (d) fixing the typo (`abc` → `5`) and waiting 250 ms
      removes the squiggle and clears the label. Repeat for the
      Sector input. (FR-014e)

---

## Phase 8: Pause / Resume / Clear

**Purpose**: Wire the two action buttons.

- [ ] T080 Handle `WM_COMMAND` for the Pause button: toggle `m_paused`,
      update the button label between "Pause" and "Resume". When paused,
      `WM_TIMER` early-returns; events continue to push into the ring;
      ring overflow increments `m_droppedSinceLastDrain` per FR-010.
      (FR-015)
- [ ] T081 Handle `WM_COMMAND` for the Clear button: clear `m_deque`,
      call `ListView_SetItemCountEx (m_listView, 0, ...)`. Do NOT
      flush the ring — `m_ring` and `m_droppedSinceLastDrain` remain
      untouched, so in-flight events drain into the now-empty deque on
      the next timer tick. (FR-016)
- [ ] T082 [P] Add `DebugDialogProjectionTests.cpp` cases:
      - Pause-induced overflow: push 8192 events to a 4096-capacity ring
        with the consumer not draining (simulating pause), then drain
        once; assert `m_droppedSinceLastDrain` was 4096 (or near it) and
        the resulting deque has exactly one `[N events lost]` entry with
        N matching the dropped count.
      - Clear-with-in-flight: drain 50 events, push 100 more (don't
        drain), call Clear (drop deque + count), drain once; assert the
        deque now contains the 100 in-flight events with no marker.
      (FR-010, FR-015, FR-016, SC-005, SC-006)
- [ ] T083 [GATE] Manual: pause during a heavy seek burst, leave paused
      long enough for the ring to overflow, resume, verify exactly one
      `[N events lost]` row appears at the correct position.

---

## Phase 9: Clipboard Copy

**Purpose**: Ctrl+C support per FR-019.

- [ ] T090 Implement a `WM_KEYDOWN` / accelerator handler on the ListView
      for Ctrl+C: enumerate selected items (`ListView_GetNextItem` loop
      with `LVNI_SELECTED`), format each as
      `"<wall>\t<uptime>\t<cycle>\t<event>\t<detail>\r\n"` (UTF-16,
      CRLF terminator) in VISIBLE-COLUMN order — hidden columns (width
      0 per FR-026) MUST be omitted, including their leading tab —
      concatenate into a `std::wstring`, open clipboard, set
      `CF_UNICODETEXT`. (FR-019, FR-026)
- [ ] T091 [P] Add a headless test that exercises the formatting helper:
      given a vector of `DiskIIEventDisplay` entries (the "selected"
      set), the helper returns the expected tab-separated UTF-16
      string. (FR-019)
- [ ] T092 [GATE] Manual: select 3 rows, Ctrl+C, paste into Notepad,
      verify tab-separated text with CRLF line endings.

---

## Phase 10: Menu / Accelerator / Shell Wiring

**Purpose**: Make the dialog actually reachable from the UI.

- [ ] T100 [P] In `Casso/resource.h`, define `IDM_VIEW_DISKII_DEBUG` with
      the next available menu-item id.
- [ ] T101 In `Casso/Casso.rc`:
      - Under the existing `View` popup menu: add a new menu item
        `MENUITEM "Disk II Debug...\tCtrl+Shift+D", IDM_VIEW_DISKII_DEBUG`.
      - In the `ACCELERATORS` table: add
        `"D", IDM_VIEW_DISKII_DEBUG, VIRTKEY, CONTROL, SHIFT`.
      (FR-001, FR-002)
- [ ] T102 In `Casso/MenuSystem.cpp`, route `WM_COMMAND` for
      `IDM_VIEW_DISKII_DEBUG` to a new `EmulatorShell::OpenDiskIIDebugDialog()`.
- [ ] T103 In `Casso/EmulatorShell.{h,cpp}`, add
      `std::unique_ptr<DiskIIDebugDialog> m_diskIIDebugDialog;`,
      `std::chrono::steady_clock::time_point m_uptimeAnchor;`
      (initialized in constructor), `void ResetUptimeAnchor();`
      (assigns `steady_clock::now()` to `m_uptimeAnchor`), and
      `void OpenDiskIIDebugDialog();`. On first call to
      `OpenDiskIIDebugDialog`, lazy-create the dialog, attach as sink
      on the first `DiskIIController` instance in the active machine
      config (controller #0 per FR-017) via `SetEventSink`, AND attach
      as audio sink on that controller's associated
      `DiskIIAudioSource` via `SetAudioEventSink` (FR-024), then
      `Show`. On subsequent calls: `Show` + `SetForegroundWindow`.
      On dialog close: revoke BOTH sinks (`SetEventSink(nullptr)`
      then `SetAudioEventSink(nullptr)`) BEFORE the dialog tears down
      its ring. (FR-001, FR-017, FR-018, FR-024)
- [ ] T103a In `CassoEmuCore/Machine/MachineShell.cpp` (or wherever
      `SoftReset` and `PowerCycle` live), call `shell.ResetUptimeAnchor()`
      from each handler so the Uptime column zeroes on every //e reset
      and power-cycle (FR-004a). The shell-side helper is a single
      assignment to `m_uptimeAnchor`. No threading concern: both
      handlers run on the UI thread, and the anchor is read on the UI
      thread during the dialog's drain.
- [ ] T103b [P] Add `DiskIIDebugDialogTests.cpp` headless test for the
      Uptime anchor: instantiate a fake-shell harness exposing
      `ResetUptimeAnchor`, capture the anchor, sleep ~50 ms, format an
      event, assert the Uptime string is `00:00.0??` (not the
      pre-reset value). (FR-004a, SC-015)
- [ ] T104 If `MachineConfig` reports more than one Disk II controller,
      append " (controller #0 only)" to the dialog title. (FR-017)
- [ ] T105 [GATE] Build, run the shell, verify the menu item appears
      with the keyboard accelerator hint, Ctrl+Shift+D opens the window,
      menu item also opens the window, closing then reopening reuses
      the dialog instance.

---

## Phase 10a: Column Show/Hide (FR-026 / FR-027)

**Purpose**: Right-click the column header to toggle individual column
visibility via a logical column model. Hidden columns are absent from
the ListView entirely; show/hide rebuilds the LV's column set from
the logical model. In-session only — closing the dialog returns all
columns to their default state (NFR-006).

- [ ] T106 In `DiskIIDebugDialog.cpp`, implement `RebuildListViewColumns()`
      per `plan.md` §"Column Show/Hide". Iterate `m_columns`, skip
      `!visible`, delete every existing LV column via a
      `while (ListView_DeleteColumn (m_lv, 0))` loop, then
      `ListView_InsertColumn` each visible `LogicalColumn` at the
      next virtual ordinal using `savedWidth` for `lvc.cx`. For any
      column whose `autoSizedYet == false`, immediately call
      `ListView_SetColumnWidth (..., LVSCW_AUTOSIZE_USEHEADER)` and
      capture the result back into `savedWidth`, then set
      `autoSizedYet = true` (FR-027). Maintain
      `m_visibleOrdinalToLogicalId` (see T062). (FR-026, FR-027)
- [ ] T107 Install a header subclass (or handle the `NM_RCLICK`
      notification on the ListView's header HWND, or `WM_CONTEXTMENU`
      while the header has focus). Build a popup menu via
      `CreatePopupMenu` + `AppendMenu` with five `MFT_STRING |
      MFS_CHECKED`-or-`MFS_UNCHECKED` items: **Wall**, **Uptime**,
      **Cycle**, **Event**, **Detail**. The checked state of each
      item MUST reflect `m_columns[id].visible`. Display with
      `TrackPopupMenu` at the cursor location. On selection, call
      `ToggleColumn(id)`: capture current widths back into the model
      first (so any user-drag that happened since the last rebuild is
      preserved), flip the `visible` bit, then call
      `RebuildListViewColumns()`. Hiding all five columns is allowed —
      the user sees a blank ListView and can re-show via the same
      menu. (FR-026)
- [ ] T107b Implement `CaptureCurrentWidthsIntoModel()` and wire it
      to the `HDN_ENDTRACK` notification from the header subcontrol
      so user-dragged width changes are written back into the
      corresponding `LogicalColumn::savedWidth`. This makes drag-then-
      hide-then-show preserve the user's chosen width. (FR-026)
- [ ] T108 [P] Add `DiskIIDebugDialogColumnTests.cpp` headless tests
      for the logical column model and rebuild planner. Extract the
      "which visible LV columns should I have, and at what widths,
      given this logical model" computation into a pure helper
      `std::vector<VisibleColumnSpec> PlanVisibleColumns
      (const std::array<LogicalColumn, 5> & model)`. Tests:
      - All five visible, none auto-sized yet → returns five specs,
        widths = default constants (auto-size happens via a real LV
        and is exercised separately).
      - One hidden (e.g., Cycle) → returns four specs in id order,
        skipping the hidden one.
      - All hidden → returns empty vector.
      - Mixed `savedWidth` values (user dragged Detail to 500) →
        returned spec for Detail uses 500, not the default.
      - `autoSizedYet == true` for a re-shown column → spec preserves
        `savedWidth` rather than re-flagging for auto-size.
      Also verify `ToggleColumn`'s state-machine semantics (flips the
      bit and calls `RebuildListViewColumns` exactly once per toggle).
      (FR-026, FR-027, NFR-006)
- [ ] T109 [GATE] Manual: right-click the header, verify the popup
      menu lists all five columns with correct checkmarks, toggling
      each fully removes/restores the column (no zero-width sliver),
      dragging a column to a custom width then hiding-and-showing it
      preserves the custom width, hiding all five leaves a usable
      (empty) ListView, closing and reopening the dialog restores all
      columns to defaults with first-show auto-sizing re-running (per
      NFR-006).

---

## Phase 11: Polish

- [ ] T110 Update `CHANGELOG.md` with a "Disk II Debug Window" entry
      under the next-release section (motor/head/address-mark/data-mark
      live event log; Ctrl+Shift+D; modeless; auto-tailing; filterable;
      pausable).
- [ ] T111 Update `README.md` "What's New" / "Debugging" sections to
      mention the debug window and its accelerator. Include one-sentence
      "open the window *before* the operation you want to investigate"
      guidance (FR-018 / Risks table).
- [ ] T112 Manual A/B test the four bundled WOZ fixtures
      (`Apple2/Demos/AppleStellarInvaders.woz`, `Choplifter.woz`,
      `HardHatMack.woz`, `Karateka.woz`) — verify each surfaces a
      reasonable event stream (motor on, head steps, address marks
      with their actual volume numbers, data reads). Note which
      titles surface non-standard volume numbers or half-track reads,
      and document those observations in the PR description as evidence
      for SC-008.
- [ ] T113 Final constitution sweep:
      - `rg -n '#include' CassoEmuCore/Devices/ Casso/` to verify
        every new `.cpp` includes `"Pch.h"` first.
      - `rg -n '\w \(\)' CassoEmuCore/Devices/ Casso/` to verify
        empty-arg call sites use `func()` (no space) and non-empty use
        `func (args)` (with space).
      - Verify variable blocks at function tops are column-aligned per
        the constitution's type/pointer/name/= columns.
      - Verify exactly 5 blank lines between top-level constructs and
        3 blank lines after function-top variable blocks.
- [ ] T114 [GATE] Full build + full test suite + manual smoke test on
      both x64 and ARM64 (constitution requirement).

---

## Dependency notes

- T030 (controller field additions) blocks T031, T032, T033.
- T010 (ring impl) blocks T011, T012, and any later phase that uses the ring.
- T020 (watcher impl) blocks T021 and T032 (which invokes `ObserveNibble`).
- T026 (TrackSectorPredicate impl) blocks T027 and Phase 7 (T070, T074).
- T074 (RichEditSquiggle helper) blocks T070's Track/Sector handlers
  invoking the helper; T074 itself depends on T026 (it references
  `TrackSectorPredicate::RejectedSpan`).
- T036–T038 (audio source integration) block T039 and Phase 10's
  `SetAudioEventSink` wiring in T103.
- T041 (projection impl) blocks T042 and Phase 6 (T060).
- T050/T051 (dialog scaffold) block T060–T091, T106–T109.
- T100–T103 (shell wiring) is the last gate before manual end-to-end
  testing; do not start before Phases 5–9 are gate-clean.
- T103a (Uptime anchor reseed on SoftReset / PowerCycle) MUST land
  before SC-015 manual validation in T112.

## Out-of-scope reminders (do NOT add to tasks.md scope)

- Multi-controller debug window (deferred per spec Out of Scope item 1).
- CPU debugger / breakpoints / memory dump (deferred to #51/#59).
- Persisting filter / pause / column-width / column-visibility state
  across launches (Out of Scope item 3; NFR-006).
- Save-log-to-file button beyond Ctrl+C (Out of Scope item 4).
- Per-row color coding (Out of Scope item 5).
- Decoding 6-and-2 data nibbles into byte values (Out of Scope item 6).
- Volume filter widget (Out of Scope item 11; volume remains visible
  inside the ADDR MARK detail string).
- `OnMotorAtSpeed` / `READ_DURING_SPINUP` events (A-009 forward-compat
  note; lands with issue #67).
