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

## Performance check

```powershell
.\scripts\RunTests.ps1 -Configuration Release -Filter PerformanceTests
```

Asserts unthrottled emulation throughput exceeds 10 MHz emulated /
host-second on the dev machine — ~10x the real //e clock — which gives
sufficient headroom to land within the production target of ~1% of one
host core when throttled.
