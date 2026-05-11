# UnitTest/Fixtures — Binary Test Fixture Inventory

**Phase F0 / Spec 004 (Apple //e Fidelity)** — staged in F0; populated for real
in later phases (D1, D2, US2). Audited per plan.md
§"Test fixtures (provenance + license posture)".

> **Test isolation contract (constitution §II)**: every test that needs to read
> bytes from a fixture MUST go through `IFixtureProvider`, which is the only
> sanctioned path. No `std::ifstream` of host paths from any test code.
> Anything outside `UnitTest/Fixtures/` is a violation.

## Inventory & provenance matrix

| Fixture                  | Size (bytes)   | Status (F0)  | Provenance                                       | License                             | Commit posture                                        |
|--------------------------|----------------|--------------|--------------------------------------------------|-------------------------------------|-------------------------------------------------------|
| `apple2e.rom`            | 16384          | Real (F0)    | Copied from `ROMs/apple2e.rom` (already in repo) | Apple //e ROM (existing repo policy) | Tracked binary, identical to ROMs/                    |
| `apple2e-video.rom`      | 4096           | Real (F0)    | Copied from `ROMs/apple2e-enhanced-video.rom`    | Apple //e ROM (existing repo policy) | Tracked binary, identical to ROMs/                    |
| `dos33.dsk`              | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic 143360-byte .dsk in memory at test-init time inside `Phase11IntegrationTests.cpp` (`BuildSyntheticDsk`). The on-disk fixture stays a zero-byte placeholder so the build sees the path; no third-party DOS 3.3 image is shipped. | Synthetic — original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `prodos.po`              | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic 143360-byte .po blob in memory at test-init time (`BuildSyntheticPo`). Same posture as `dos33.dsk`. | Synthetic — original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `sample.woz`             | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic v2 WOZ via `WozLoader::BuildSyntheticV2` (51200-bit standard track 0). Phase 10 `WozLoaderTests` and `DiskImageStoreTests` already exercised the loader against in-memory blobs. | Synthetic — original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `copyprotected.woz`      | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic CP-style v2 WOZ with a non-standard 50000-bit track 0 length so the variable-bit-count code path of the nibble engine is exercised end-to-end. No real CP boot-loader is emulated; FR-024 is met by demonstrating the engine handles variable-length tracks via the headless harness. | Synthetic — original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `golden/`                | dir            | Empty        | Golden hashes/framebuffers populated in V1       | n/a                                 | Tracked directory (`.gitkeep`)                         |

## Rules

- All fixtures here are accessed through `IFixtureProvider::OpenFixture()` with
  a path **relative to this directory**.
- `IFixtureProvider` rejects `..`, drive letters, and absolute roots
  (returns `E_INVALIDARG`).
- Real disk-image fixtures (DOS 3.3, ProDOS, WOZ) are intentionally zero-byte
  placeholders: Phase 11 (US2) synthesizes the disk bytes in memory at
  test-init time rather than shipping third-party software. The synthetic
  builders live alongside the tests that use them — `Phase11IntegrationTests.cpp`
  (`BuildSyntheticDsk` / `BuildSyntheticPo` / `BuildSyntheticWoz`) for the
  headless boot-ROM scenarios; `NibblizationTests.cpp` and `WozLoaderTests.cpp`
  for round-trip unit tests.
- Adding a new fixture: append a row to the matrix above with provenance,
  license, and size, then commit the file.

## See also

- `specs/004-apple-iie-fidelity/plan.md` §"Test fixtures (provenance + license posture)"
- `specs/004-apple-iie-fidelity/contracts/IFixtureProvider.md`
