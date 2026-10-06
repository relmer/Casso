# Tasks: Update notification and self-update

**Input**: design documents in `specs/039-update-notification/`

**Tests**: required by the constitution (Principle II); every core unit gets
unit tests over mocks. The whole spec ships in one merge; phase order is build
order only.

## Phase 1: Setup

- [X] T001 Create `CassoEmuCore/Update/`, `CassoEmuCore/Net/` and `UnitTest/UpdateTests/` and add them as filters in `CassoEmuCore/CassoEmuCore.vcxproj(.filters)` and `UnitTest/UnitTest.vcxproj(.filters)`
- [X] T002 Add `wintrust.h`, `softpub.h`, `appmodel.h`, `bcrypt.h` (if absent) and `windows.management.deployment.h` to `CassoEmuCore/Pch.h` and `UnitTest/Pch.h`; link `wintrust.lib`, `crypt32.lib`, `runtimeobject.lib` in `Casso` and `UnitTest`

## Phase 2: Foundational

- [X] T003 Define `IHttpClient` (GET to memory with optional extra headers, progress and cancel; result carries HTTP status) and its null implementation in `CassoEmuCore/Net/IHttpClient.h`
- [X] T004 Move the body of `AssetBootstrap::DownloadHttp` into `WinHttpClient` in `CassoEmuCore/Net/WinHttpClient.h/.cpp`, with User-Agent `Casso/<VERSION_STRING>`; make `AssetBootstrap` call it with no behavior change in `CassoEmuCore/AssetBootstrap.cpp`
- [X] T005 [P] `ReleaseVersion` (parse `v1.30.0`/`1.30.0`, compare, `IsMinorLine`) in `CassoEmuCore/Update/ReleaseVersion.h/.cpp` with tests in `UnitTest/UpdateTests/ReleaseVersionTests.cpp`
- [X] T006 [P] Add `autoUpdateCheck` (default true), `lastUpdateCheckUtc` (int64, default 0), `latestKnownVersion` and `skippedVersion` (default "") to `CassoEmuCore/Config/GlobalUserPrefs.h/.cpp` (field, known-key list, save, load) with round-trip tests in the existing prefs test file

## Phase 3: User story 1 - learn that a new release exists (P1)

**Independent test**: an older build shows the title-bar indicator within 10 s while emulation runs.

- [X] T007 [P] [US1] `ReleaseInfo` parser for the fields in `contracts/github-release-api.md`, rejecting prereleases, with `FindZipAsset (arch)` and `FindBundleAsset()`, in `CassoEmuCore/Update/ReleaseInfo.h/.cpp`; tests incl. missing tag, prerelease, missing digest, no assets in `UnitTest/UpdateTests/ReleaseInfoTests.cpp`
- [X] T008 [P] [US1] `UpdateSchedule`: due when on and (never checked, >= 24 h, or clock moved backward); indicator decision from latest known vs running vs skipped; manual check ignores both, in `CassoEmuCore/Update/UpdateSchedule.h/.cpp`; tests in `UnitTest/UpdateTests/UpdateScheduleTests.cpp`
- [X] T009 [US1] `UpdateService` check on a worker thread: single-instance mutex for the automatic check, fetch via `IHttpClient`, map failures (Network, RateLimited, BadData), update state, post `WM_APP_UPDATE_RESULT` through an injected poster; cancel and join on shutdown, in `CassoEmuCore/Update/UpdateService.h/.cpp`; tests with a mock client in `UnitTest/UpdateTests/UpdateServiceTests.cpp`
- [X] T010 [US1] Caption indicator: icon button left of the minimize button, hidden by default, tooltip "Update available: Casso <version>", in `CassoEmuCore/Shell/Layout/ChromeBandLayout.h/.cpp` and the caption panel; hit testing in `CassoEmuCore/Shell/Window/EmulatorWindow.cpp`
- [X] T011 [US1] Shell wiring: start the check after the first frame, handle `WM_APP_UPDATE_RESULT` (free payload), save prefs, show/hide the indicator, stop the service in `~EmulatorShell`, in `CassoEmuCore/Shell/EmulatorShell*.cpp`

## Phase 4: User story 2 - read about the release and decide (P1)

**Independent test**: clicking the indicator shows both versions, date and notes; Skip hides it for good; closing keeps it.

- [X] T012 [P] [US2] `ReleaseNotesExtractor`: CHANGELOG sections in (running, new] and README highlights for minor lines in that range, newest first, per `contracts/release-notes-headings.md`, in `CassoEmuCore/Update/ReleaseNotesExtractor.h/.cpp`; tests incl. Unreleased ignored, skipped versions, no match in `UnitTest/UpdateTests/ReleaseNotesExtractorTests.cpp`
- [X] T013 [P] [US2] `ReleaseNotesFormatter`: markdown subset to `FormattedLine` runs; unknown syntax kept as text, in `CassoEmuCore/Update/ReleaseNotesFormatter.h/.cpp`; tests in `UnitTest/UpdateTests/ReleaseNotesFormatterTests.cpp`
- [X] T014 [US2] `UpdateService::FetchNotes` from raw.githubusercontent.com at the release tag, cached per session, in `CassoEmuCore/Update/UpdateService.cpp`
- [X] T015 [US2] `UpdateDialog` per `contracts/update-ui.md` (header, scrolling formatted body, buttons by install type, developer text, release page link) in `CassoEmuCore/Ui/Dialogs/UpdateDialog.h/.cpp`; button-set selection as a pure function with tests
- [X] T016 [US2] Skip this version: persist `skippedVersion`, hide indicator; close leaves it, in `CassoEmuCore/Shell/EmulatorShell*.cpp`

## Phase 5: User story 3 - update in place (P2)

**Independent test**: each install type updates and restarts; failures leave the old version working.

- [X] T017 [P] [US3] `ISignatureVerifier` + `AuthenticodeVerifier` (`WTD_REVOKE_NONE`, cache-only retrieval, subject compare against the publisher constant `CN=Robert Elmer, O=Robert Elmer, L=Redmond, S=Washington, C=US`) in `CassoEmuCore/Update/`
- [X] T018 [P] [US3] `InstallTypeDetector` (package name → Msix; official signature → Zip; else Developer) over `IInstallEnvironment`, in `CassoEmuCore/Update/InstallTypeDetector.h/.cpp`; tests in `UnitTest/UpdateTests/InstallTypeDetectorTests.cpp`
- [X] T019 [P] [US3] Clean-room `Inflate` (RFC 1951: stored, fixed, dynamic) in `CassoEmuCore/Update/Inflate.h/.cpp`; tests with known vectors and corrupt input in `UnitTest/UpdateTests/InflateTests.cpp`
- [X] T020 [US3] `ZipArchive` (central directory, methods 0/8, CRC-32, rejects encryption, absolute paths and `..`) in `CassoEmuCore/Update/ZipArchive.h/.cpp`; tests in `UnitTest/UpdateTests/ZipArchiveTests.cpp`
- [X] T021 [US3] `IUpdateFileSystem` + Win32 implementation, and `ZipUpdateInstaller` (digest check, extract, stage, verify staged exe signature and version, writability probe, swap via `.update-old`, restore on any failure, cleanup) in `CassoEmuCore/Update/`; tests that fail the Nth step and confirm restore in `UnitTest/UpdateTests/ZipUpdateInstallerTests.cpp`
- [X] T022 [US3] `IPackageDeployer` + `MsixPackageDeployer` (WRL `AddPackageByUriAsync`, `ForceTargetApplicationShutdown`, after `RegisterApplicationRestart`) in `CassoEmuCore/Update/`
- [X] T023 [US3] `UpdateService::Apply`: download asset with progress/cancel, digest check, other-instance check (`Local\Casso.Instance`), dispatch by install type, flush disks and prefs before the MSIX deploy, relaunch with `--updated --cleanup-old <pid>` after a zip swap, in `CassoEmuCore/Update/UpdateService.cpp`; tests with mocks
- [X] T024 [US3] Parse `--updated` and `--cleanup-old <pid>` (hidden) in `CassoCore/CommandLineParser.cpp` / `CommandLineOptions.h`, and perform the cleanup at startup in the shell
- [X] T025 [US3] Dialog progress, cancel, and failure reporting with the release page link; unwritable folder or missing asset shows **Open release page**, in `CassoEmuCore/Ui/Dialogs/UpdateDialog.cpp`

## Phase 6: User story 4 - check on demand and control the automatic check (P2)

**Independent test**: manual check online old/current/offline; automatic off makes no request.

- [X] T026 [US4] `IDM_HELP_CHECK_UPDATES` in `CassoEmuCore/resource.h` inside the Help routing range (adjust `WindowCommandManager.cpp:436`), menu entry "Check for updates..." in `CassoEmuCore/Ui/Chrome/EmulatorCommands.cpp`, handler in `CassoEmuCore/Shell/WindowCommandManager.cpp`; routing test in `UnitTest/UiTests/ChromeCommandRoutingTests.cpp`
- [X] T027 [US4] Manual result reporting: up to date, failure with cause, or dialog directly, in the shell
- [X] T028 [US4] Settings checkbox "Check for updates automatically" bound to `autoUpdateCheck` in a Settings page and `CassoEmuCore/Ui/Settings/SettingsSheet.cpp`

## Phase 7: Polish

- [X] T029 Replace "Warn that the release is unsigned" with a failing step and add the core publisher constant to the subject comparison in `.github/workflows/ci.yml`
- [ ] T030 Full suite Debug + Release, `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`, `scripts/CheckStyle.ps1 -Mode Tree`
- [ ] T031 On-screen validation per `quickstart.md`: old-versioned unsigned build and signed build, screenshots of indicator and dialog
- [ ] T032 CHANGELOG and README entries, drafted for owner approval after testing

## Dependencies

Phase 1 → Phase 2 → US1 → US2 → US3; US4 needs US1 and US2 only. Inside
phases, [P] tasks touch different files and can run in parallel.
