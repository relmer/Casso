# UnitTest/Fixtures: Binary Test Fixture Inventory

**Phase F0 / Spec 004 (Apple //e Fidelity)**, staged in F0; populated for real
in later phases (D1, D2, US2). Audited per plan.md
§"Test fixtures (provenance + license posture)".

> **Test isolation contract (constitution §II)**: every test that needs to read
> bytes from a fixture MUST go through `IFixtureProvider`, which is the only
> sanctioned path. No `std::ifstream` of host paths from any test code.
> Anything outside `UnitTest/Fixtures/` is a violation.

## Inventory & provenance matrix

| Fixture                  | Size (bytes)   | Status (F0)  | Provenance                                       | License                             | Commit posture                                        |
|--------------------------|----------------|--------------|--------------------------------------------------|-------------------------------------|-------------------------------------------------------|
| `Apple2e.rom`            | 16384          | Real (F0)    | Copied from `ROMs/Apple2e.rom` (already in repo) | Apple //e ROM (existing repo policy) | Tracked binary, identical to ROMs/                    |
| `Apple2e_Video.rom`      | 4096           | Real (F0)    | Copied from `ROMs/Apple2e_Video.rom`    | Apple //e ROM (existing repo policy) | Tracked binary, identical to ROMs/                    |
| `dos33.dsk`              | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic 143360-byte .dsk in memory at test-init time inside `Phase11IntegrationTests.cpp` (`BuildSyntheticDsk`). The on-disk fixture stays a zero-byte placeholder so the build sees the path; no third-party DOS 3.3 image is shipped. | Synthetic, original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `prodos.po`              | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic 143360-byte .po blob in memory at test-init time (`BuildSyntheticPo`). Same posture as `dos33.dsk`. | Synthetic, original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `sample.woz`             | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic v2 WOZ via `WozLoader::BuildSyntheticV2` (51200-bit standard track 0). Phase 10 `WozLoaderTests` and `DiskImageStoreTests` already exercised the loader against in-memory blobs. | Synthetic, original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `copyprotected.woz`      | 0 (placeholder)| Placeholder (Phase 11) | Phase 11 builds a synthetic CP-style v2 WOZ with a non-standard 50000-bit track 0 length so the variable-bit-count code path of the nibble engine is exercised end-to-end. No real CP boot-loader is emulated; FR-024 is met by demonstrating the engine handles variable-length tracks via the headless harness. | Synthetic, original to this repo | Zero-byte placeholder; real bytes synthesized at test-init |
| `golden/`                | dir            | Empty        | Golden hashes/framebuffers populated in V1       | n/a                                 | Tracked directory (`.gitkeep`)                         |
| `Merlin/`                | 34908 (15 files) | Real       | Vendor source and shipped object code extracted verbatim from the Merlin Pro 2.23 disk (archive.org item `MerlinProMacroAssembler`), re-derivable via `scripts/ExtractMerlinFixtures.ps1` from a hash-pinned image. Byte-identical oracles for the Merlin dialect, two subset-boundary specimens, and the two type-T macro libraries those specimens include. Note that three of the five oracles need `KBD` values supplied non-interactively, see `Merlin/README.md`. | **CC BY-NC-ND 3.0**: Glen Bredon / Roger Wagner Publishing, 1984. Not MIT. See `Merlin/README.md`. | Tracked binaries, unmodified and must stay so |
| `Disks/`                 | 438930 (6 files) | Real       | Three unmodified Apple II volumes from the same archive item: one DOS 3.3, two ProDOS (`/MERLIN`, `/APPLESOFT`): plus the vendor catalog listing for each. All three images are in DOS sector order, so filesystem and ordering must be determined independently. `Disks/README.md` records the on-disk format findings measured against them: the inline-header-versus-`aux_type` asymmetry between the two filesystems, the backspace-drawn catalog headings, the high-bit ProDOS text, and the two shapes these volumes cannot reach. Obtained and hash-verified by `scripts/FetchMerlin.ps1`. | **CC BY-NC-ND 3.0**: Glen Bredon / Roger Wagner Publishing, 1984–85. Not MIT, and **contains runnable software**. See `Disks/README.md`. | Tracked binaries, read-only, unmodified and must stay so |

## Rules

- All fixtures here are accessed through `IFixtureProvider::OpenFixture()` with
  a path **relative to this directory**.
- `IFixtureProvider` rejects `..`, drive letters, and absolute roots
  (returns `E_INVALIDARG`).
- Real disk-image fixtures (DOS 3.3, ProDOS, WOZ) are intentionally zero-byte
  placeholders: Phase 11 (US2) synthesizes the disk bytes in memory at
  test-init time rather than shipping third-party software. The synthetic
  builders live alongside the tests that use them, `Phase11IntegrationTests.cpp`
  (`BuildSyntheticDsk` / `BuildSyntheticPo` / `BuildSyntheticWoz`) for the
  headless boot-ROM scenarios; `NibblizationTests.cpp` and `WozLoaderTests.cpp`
  for round-trip unit tests.
- `Merlin/` and `Disks/` are deliberate exceptions to that posture, not a drift
  from it. Neither job can be done synthetically. An assembler oracle is only
  worth anything because a third party produced the expected bytes, bytes we
  generate ourselves would prove Casso agrees with Casso. A filesystem reader
  validated against images this repository wrote agrees with its own
  understanding of the format by construction, including where that
  understanding is wrong. The exceptions are narrow and paid for: the material
  is redistributable under an explicit license, verbatim, with attribution, and
  its provenance is re-derivable from a hash-pinned source. Do not read this as
  permission to ship third-party software as a fixture generally; read it as
  the bar such a fixture has to clear. Where a specific structural shape needs
  constructing on purpose, synthetic is still correct.
- Real-volume fixtures are **read-only**. A test that writes to one destroys
  evidence for every later run and breaches the no-derivatives term of their
  license. Copy the bytes and mutate the copy; `IFixtureProvider` opens
  read-only and never writes back.
- Fixtures are not uniformly MIT. `Merlin/` is CC BY-NC-ND 3.0, which carries a
  non-commercial restriction the rest of the repository does not. Check a
  directory's own README before copying its contents anywhere.
- Adding a new fixture: append a row to the matrix above with provenance,
  license, and size, then commit the file.
- **Non-permissively licensed fixtures need a sidecar `LICENSE` file, and that
  is the whole obligation.** One `LICENSE` per directory covers every file in
  it, naming the license, the attribution it requires, and where the material
  came from. Group files that share a license into their own subdirectory
  rather than annotating them individually; the directory is the unit. No
  per-file license accounting is required, and adding a fixture is never a
  constitution amendment.
- Fixtures are not dependencies. The **Approved Third-Party Dependencies**
  allowlist in the constitution governs material that *ships* (compiled into,
  linked into, or distributed alongside a released binary) where a license
  grant has to be established in detail. Fixtures reach no end user, so a
  non-permissive license is acceptable here where it would not be there.
  Permissive and repo-original fixtures need nothing at all.

## See also

- `specs/004-apple-iie-fidelity/plan.md` §"Test fixtures (provenance + license posture)"
- `specs/004-apple-iie-fidelity/contracts/IFixtureProvider.md`
