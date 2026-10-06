# Implementation Plan: Update notification and self-update

**Branch**: `039-update-notification` | **Date**: 2026-10-06 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/039-update-notification/spec.md`

## Summary

Casso learns about new releases from the GitHub Releases API, shows a title-bar
indicator when a newer, unskipped release exists, and opens a dialog with the
release's own CHANGELOG entries (and README highlights for major or minor
versions in the range) read from the release tag. **Update to <version>** installs the
release by the method for this copy's install type: an in-place MSIX upgrade
from the release's `.msixbundle`, or a verified replace-and-restore of the
release zip's files. A developer build, meaning any executable without a valid
Authenticode signature from Casso's publisher, is never updated in place.

Every decision lives in `CassoEmuCore/Update/` behind data-in/data-out
functions and interface seams for HTTP, file system, signature verification,
package deployment and process control, so the unit tests reach all of it. The
exe gains nothing. CI changes so that a release without a signing credential
fails instead of shipping unsigned.

## Technical Context

**Language/Version**: C++23, MSVC v145 (VS 2026)

**Primary Dependencies**: Win32 only. WinHTTP (existing, via `AssetBootstrap`),
WinTrust/Crypt32 (`WinVerifyTrust`, `CryptQueryObject`), appmodel
(`GetCurrentPackageFullName`), Windows Runtime ABI headers through WRL for
`Windows.Management.Deployment.PackageManager`, the existing core JSON parser,
and a clean-room inflate (RFC 1951) plus zip central-directory reader (PKWARE
APPNOTE). No third-party code.

**Storage**: `GlobalUserPrefs` JSON (four new fields, see data-model.md).
Downloaded packages go to `%LOCALAPPDATA%\Casso\Updates\`.

**Testing**: Microsoft Native CppUnitTest in `UnitTest`, with mock seams; no
real network, file system, registry or process in unit tests.

**Target Platform**: Windows 10 1809+ / 11, x64 and ARM64.

**Project Type**: Desktop application (Win32 + Direct3D/Dxui).

**Performance Goals**: zero work on the UI or CPU thread beyond posting and
painting; startup unchanged (no signature or network work before first frame).

**Constraints**: no online revocation check; no data sent beyond the GET
requests; prereleases never offered; failure of the automatic check is silent.

**Scale/Scope**: about 12 new core classes, one new dialog, one caption widget,
one Settings checkbox, one Help command, one CI step change.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How |
|---|---|---|
| I. Code quality | Pass | EHM throughout; style gate on every commit. |
| II. Testing discipline | Pass | Every parser, comparer, planner and the replace-and-restore sequence has unit tests over mocks; failure paths are tested, including "succeeded while doing nothing" cases (empty asset list, empty changelog range). |
| III. UX consistency | Pass | Sentence case labels; dialog built from the existing Dxui dialog infrastructure; indicator styled like caption buttons. |
| IV. Performance | Pass | Check, signature verification, download and extraction run on a worker thread; results posted with `WM_APP_*`. |
| V. Simplicity | Pass | One HTTP path (extracted from `AssetBootstrap`), one prefs store, no new project. The inflate is the one sizeable addition and is justified in research.md R-6. |
| VI. Thin exe | Pass | No change to `Casso/`. Shell wiring goes in `CassoEmuCore/Shell`, which the tests link. |

Post-design re-check: unchanged, all pass. No violations to track.

## Project Structure

### Documentation (this feature)

```text
specs/039-update-notification/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── github-release-api.md
│   ├── release-notes-headings.md
│   └── update-ui.md
└── tasks.md
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Net/
│   ├── IHttpClient.h              # seam: GET to memory, with cancel + progress
│   └── WinHttpClient.h/.cpp       # body moved out of AssetBootstrap::DownloadHttp
├── Update/
│   ├── ReleaseVersion.h/.cpp      # parse "v1.30.0" / "1.30.0", compare
│   ├── ReleaseInfo.h/.cpp         # parse GitHub "latest release" JSON
│   ├── ReleaseNotesExtractor.h/.cpp  # CHANGELOG + README section slicing
│   ├── ReleaseNotesFormatter.h/.cpp  # markdown subset -> styled lines
│   ├── UpdateSchedule.h/.cpp      # 24 h throttle, skip, indicator decision
│   ├── InstallTypeDetector.h/.cpp # MSIX / zip / developer
│   ├── ISignatureVerifier.h       # seam
│   ├── AuthenticodeVerifier.h/.cpp
│   ├── ZipArchive.h/.cpp          # central directory + stored/deflate
│   ├── Inflate.h/.cpp             # RFC 1951 decoder
│   ├── ZipUpdateInstaller.h/.cpp  # verify, stage, swap, restore
│   ├── IPackageDeployer.h         # seam
│   ├── MsixPackageDeployer.h/.cpp # WRL PackageManager AddPackageAsync
│   ├── IUpdateFileSystem.h        # seam for the swap steps
│   └── UpdateService.h/.cpp       # orchestration on the worker thread
├── Ui/Dialogs/
│   └── UpdateDialog.h/.cpp        # details dialog (custom body)
├── Ui/Chrome/EmulatorCommands.cpp # IDM_HELP_CHECK_UPDATES
├── Ui/Settings/<page>.cpp         # "Check for updates automatically"
├── Shell/Layout/ChromeBandLayout.* # indicator slot left of the caption buttons
└── Config/GlobalUserPrefs.*       # update state fields

UnitTest/UpdateTests/
├── ReleaseVersionTests.cpp
├── ReleaseInfoTests.cpp
├── ReleaseNotesExtractorTests.cpp
├── ReleaseNotesFormatterTests.cpp
├── UpdateScheduleTests.cpp
├── InstallTypeDetectorTests.cpp
├── InflateTests.cpp
├── ZipArchiveTests.cpp
├── ZipUpdateInstallerTests.cpp
└── UpdateServiceTests.cpp

.github/workflows/ci.yml           # unsigned release fails
```

**Structure Decision**: all new logic in `CassoEmuCore`, in a new `Update/`
folder plus a `Net/` seam that `AssetBootstrap` also adopts. Tests in a new
`UnitTest/UpdateTests/` folder.

## Complexity Tracking

None.
