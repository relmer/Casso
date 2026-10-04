# Quickstart: Validating cassette tape support

## Prerequisites

```powershell
scripts\FetchRoms.ps1 -Fixtures
scripts\Build.ps1
```

## Automated

```powershell
scripts\RunTests.ps1 -Filter Tape        # edit-test loop
scripts\RunTests.ps1 -Build              # full suite before merge
scripts\Build.ps1 -RunCodeAnalysis
scripts\CheckStyle.ps1 -Mode Tree
```

Expected: every tape test passes, including the real-ROM load matrix (SC-001),
the round trip (SC-002), the robustness matrix (SC-003), and the governor
start and stop cases (SC-005). The tape tests must report a non-zero case
count; a filtered run is not the suite.

## Manual

Launch per the project rules (background, `--title <worktree name>`).

1. **Load (US1, SC-006)**: on a ][+, insert an Internet Archive Apple II
   cassette (WAV, then the same tape as MP3). In the Monitor, type the range
   from the tape's notes followed by `R`, or `LOAD` at the prompt, then press
   play. It loads and runs.
2. **Fast loading (US2, SC-004)**: with "Fast tape loading" on, time a 16 KB
   load. It must take under 10 s of host time; audio plays as real-time slices at true pitch and the speed
   menu still shows the user's setting. Turn the checkbox off and repeat: the
   load takes its real duration and the tape is audible.
3. **Stop conditions (SC-005)**: stop the tape mid-load; speed drops to
   normal at once. On a //e with no tape, run a program that polls the
   buttons; speed stays normal.
4. **Record (US3)**: create a new blank tape, arm record, press play, type
   `800.9FFW`, then press stop. Reset, rewind, type `800.9FFR`, and press play:
   memory matches. Open the WAV in another Apple II tape tool and confirm it
   decodes the same.
5. **Controls and persistence (US4, FR-016)**: insert, play part way, stop,
   check that the position holds, rewind to 0, restart Casso, and check that
   the tape comes back at 0. Switch to the //c and check that there is no tape
   deck.
