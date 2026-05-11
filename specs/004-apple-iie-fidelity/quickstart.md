# Quickstart: Apple //e Fidelity

## Build

```powershell
# From repo root
.\scripts\Build.ps1                    # Debug + Release, x64
.\scripts\Build.ps1 -Configuration Release
.\scripts\Build.ps1 -RunCodeAnalysis   # required pre-commit per constitution
```

VS Code: `Ctrl+Shift+B` → `Build + Test Debug` or `Build + Test Release`.

## Run the //e (GUI)

```powershell
.\x64\Release\Casso.exe --machine apple2e
.\x64\Release\Casso.exe --machine apple2e --disk1 path\to\disk.dsk
.\x64\Release\Casso.exe --machine apple2e --disk1 path\to\image.woz
```

Soft reset: GUI menu (or Ctrl+Reset). Power cycle: GUI menu — re-seeds RAM
via `Prng` so previously-deterministic state diverges intentionally.

## Run unit + integration tests

```powershell
.\scripts\RunTests.ps1                          # all tests
.\scripts\RunTests.ps1 -Filter EmuTests         # //e suite only
.\scripts\RunTests.ps1 -Configuration Release   # required for PerformanceTests
```

The //e integration suite is fully headless. It will not open a window, will
not open an audio device, and will not access any host file outside
`UnitTest/Fixtures/`.

## Validation suites for significant CPU/assembler changes

```powershell
.\scripts\RunDormannTest.ps1
.\scripts\RunHarteTests.ps1 -SkipGenerate
```

Required when CPU strategy refactor (Phase 1) or any opcode-touching change
is committed.

## Authoring a new headless test

```cpp
// UnitTest/EmuTests/MyNewTest.cpp
#include "Pch.h"
#include "HeadlessHost.h"
#include "MachineFactory.h"

TEST_METHOD (BootsToBasicPrompt)
{
    HRESULT          hr        = S_OK;
    HeadlessHost     host;
    MachinePtr       machine;
    bool             reached   = false;

    hr = MachineFactory::Create ("apple2e", host, machine);
    CHRA (hr);

    hr = machine->PowerCycle ();
    CHRA (hr);

    hr = host.RunUntil ([&]() {
        return ScrapeText (*machine).Contains ("APPLE //E");
    }, /*maxCycles*/ 5'000'000, reached);
    CHRA (hr);

Error:
    Assert::IsTrue (reached);
    return hr;
}
```

Test isolation rules (constitution §II, NON-NEGOTIABLE):
- Use `HeadlessHost` — never construct a real `Casso/HostShell`.
- Use `IFixtureProvider` for any fixture file — never call `CreateFileW`,
  `fopen`, `std::ifstream` on a host path directly.
- Pin the PRNG seed to `0xCA550001` (the harness default).
- Re-running any test must produce byte-identical scraped text and identical
  framebuffer hashes.

## Adding a fixture

1. Drop the binary into `UnitTest/Fixtures/`.
2. Add a row to `UnitTest/Fixtures/README.md` documenting source + license.
3. Reference it from tests via `IFixtureProvider::OpenFixture("name.ext")`.

## Performance

The Phase 15 perf budget (FR-042 / SC-007) is enforced by
`UnitTest/EmuTests/PerformanceTests.cpp`. It runs **only in Release**
builds — Debug builds compile a sentinel test that reports
"skipped" because the unoptimized debug build's measurements are
not actionable.

### Run the perf check

```powershell
.\scripts\RunTests.ps1 -Configuration Release -Filter PerformanceTests
```

### Sampling protocol

1. Build a //e via `HeadlessHost::BuildAppleIIe` with the pinned PRNG
   seed `0xCA550001` so RAM init is deterministic across runs.
2. Cold-boot to the Applesoft idle prompt (5,000,000 cycle budget).
3. Warm up 100,000 additional cycles so caches and branch predictors
   stabilize before the timed window.
4. Snapshot `QueryPerformanceCounter`, run **exactly 1,000,000**
   emulated cycles (driven by `EmulatorCore::RunCycles`, which pumps
   the `EmuCpu` and accumulates cycle counts via
   `EmuCpu::AddCycles`), snapshot QPC again, convert to milliseconds.
5. Compare against the named ceiling
   `kPerformanceCeilingMs = 97.75 ms`.

### Threshold derivation

| Quantity                                     | Value        |
|----------------------------------------------|--------------|
| Real //e cycle frequency                     | 1.023 MHz    |
| Wall-clock cost of 1M cycles on real //e     | 977.5 ms     |
| 1% of one host core (production target)      | 9.775 ms     |
| **Test ceiling = 10× the production target** | **97.75 ms** |

The 10× headroom guards against measurement noise on busy CI hosts
while still flunking any regression that materially erodes the
production-throttled budget. The threshold lives in a single named
`kPerformanceCeilingMs` constant at the top of the test file, the
only knob you should turn.

### Stability gate

`CycleEmulation_StableRunToRun` runs the protocol 5 times in
succession. Every run must individually meet the 97.75 ms ceiling,
**and** the worst run must lie within 30% of the median. The 30%
tolerance was chosen empirically: dev-class hosts typically show
median ≈ 20–30 ms with worst-case run-to-run drift well under 30%,
so a breach signals a real regression rather than jitter.

### Host-class assumption

The budget assumes a "developer-class" host: a modern desktop or
laptop x86-64 / ARM64 CPU released within the last ~5 years. The
ceiling is independent of architecture (1M emulated cycles, ≤
97.75 ms wall-time): if the test fails on ARM64 but passes on x64
(or vice-versa) that's a real regression in the per-architecture
codegen, not flake.

### CI gating

Both perf tests are part of the standard `Build + Test Release`
target — failure fails CI. They are skipped in Debug runs (the
sentinel test `CycleEmulation_SkippedInDebug` logs the reason and
passes), so the default `Build + Test Debug` target stays fast.

### What to do if the test flakes on slower hardware

1. **Confirm it's the host, not the emulator** — re-run the test
   suite alone (no other build/test/IDE workload). If it then
   passes, it was host contention.
2. **Re-derive the threshold** — `kPerformanceCeilingMs` is the
   only knob; recompute as
   `target_fraction × (1,000,000 / 1.023 MHz × 1000)`. The test
   source comment documents the math.
3. **Bump headroom, not budget** — if a slower CI tier needs more
   slack, raise the ceiling to e.g. 150 ms (≈ 6× headroom). Do
   **not** change the per-cycle target; it's the shipping budget.
4. **Increase sample count** — if jitter, not slowness, is the
   issue, bump `kStabilityRunCount` and use median-of-medians
   rather than raw worst.

All four levers live in a single named-constant block at the top
of `PerformanceTests.cpp` for surgical tuning.
