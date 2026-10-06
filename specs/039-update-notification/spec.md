# Feature Specification: Update notification and self-update

**Feature Branch**: `039-update-notification`

**Created**: 2026-10-06

**Status**: Draft

**Input**: User description: "Update notification and self-update for Casso. Casso checks for a new release two ways: a throttled background check at startup (at most once a day, can be turned off in Settings) and a manual "Check for updates" command. When a newer release exists, an indicator appears in the title bar; clicking it opens a dialog with the new version, its release notes, and actions: update now, skip this version (stays quiet until a newer release), or remind me later. "Update now" downloads and installs the new build, then restarts Casso, with recovery if a download or install fails. Casso is distributed from more than one source: GitHub Releases today and winget soon. The update path must respect how this copy was installed: a winget install updates through winget, and a copy from the GitHub release zip updates from GitHub Releases. The version check must never block or slow emulation, and a network failure is silent for the startup check and reported for the manual check."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Learn that a new release exists (Priority: P1)

A user runs Casso on a normal day. A newer release has been published since
their copy was built. Without doing anything, they see a small indicator in
the title bar telling them an update is available. Emulation is never
interrupted, and no dialog appears unless they ask for it.

**Why this priority**: This is the problem as stated: today, people have no
way to know a new release is out.

**Independent Test**: Run a build whose version is older than the latest
published release; the title-bar indicator appears within a few seconds of
startup while a program keeps running at full speed.

**Acceptance Scenarios**:

1. **Given** automatic checks are on and no check has run in the last 24 hours, **When** Casso starts and a newer release is published, **Then** the update indicator appears in the title bar.
2. **Given** a check ran less than 24 hours ago, **When** Casso starts, **Then** no network request is made, and the indicator shows the result of the last check if that result is still newer than the running build.
3. **Given** the running build is the latest release, **When** the check finishes, **Then** no indicator appears.
4. **Given** the machine is offline, **When** the startup check fails, **Then** nothing is shown and Casso runs normally.

---

### User Story 2 - Read about the release and decide (Priority: P1)

The user clicks the title-bar indicator. A dialog shows the running version,
the new version, its release date and its release notes. The dialog offers
**Update now** and **Skip this version**. Closing the dialog leaves the
indicator in place.

**Why this priority**: The indicator is useless without a place to read what
changed and act on it.

**Independent Test**: With the indicator showing, click it and use each of
the three actions in turn.

**Acceptance Scenarios**:

1. **Given** the indicator is showing, **When** the user clicks it, **Then** the dialog opens with both versions and the release notes.
2. **Given** the dialog is open, **When** the user chooses **Skip this version**, **Then** the indicator disappears and does not return for that version, at startup or later in the session.
3. **Given** version N is skipped, **When** version N+1 is published, **Then** the indicator returns for N+1.
4. **Given** the dialog is open, **When** the user closes the dialog, **Then** the indicator stays for this session, and the next automatic check that is due shows it again.

---

### User Story 3 - Update in place (Priority: P2)

From the dialog, the user chooses **Update now**. Casso gets the new build
through the channel this copy was installed from, then restarts on the new
version. If anything fails, the user keeps a working copy of the old version
and sees what went wrong.

**Why this priority**: Telling users a release exists delivers most of the
value. Installing it for them removes the remaining friction, but it carries
most of the risk.

**Independent Test**: For each install type, start an old build, choose
**Update now**, and confirm Casso restarts on the new version. Then simulate a
failed download and a failed install, and confirm the old version still runs.

**Acceptance Scenarios**:

1. **Given** a copy unpacked from the release zip, **When** the user chooses **Update now**, **Then** Casso downloads the matching zip for its architecture, verifies it, replaces its own files, and restarts on the new version.
2. **Given** a copy installed from the MSIX package, whether by hand from the release page or by winget, **When** the user chooses **Update now**, **Then** Casso downloads the new MSIX bundle from the release, installs it as an in-place upgrade of the installed package, and restarts on the new version with its settings intact.
3. **Given** a copy running from a source checkout, **When** a newer release exists, **Then** the dialog shows the release notes and tells the user to pull and rebuild, and offers no **Update now**.
4. **Given** an emulated disk has unsaved writes, **When** the user chooses **Update now**, **Then** Casso flushes them or asks first, exactly as it does on a normal exit, before it restarts.
5. **Given** the download fails, is incomplete, or does not verify, **When** the update runs, **Then** no file of the installed copy is changed, and the dialog reports the failure and offers a link to the release page.
6. **Given** the install step fails partway, **When** the update runs, **Then** the previous version is restored and still launches.
7. **Given** the folder of the zip copy cannot be written to, **When** the user chooses **Update now**, **Then** Casso explains this and offers the release page instead.

---

### User Story 4 - Check on demand and control the automatic check (Priority: P2)

The user chooses **Check for updates** from the menu. Casso checks right away,
ignoring the daily limit and any skipped version, and always reports the
result. In Settings, the user can turn the automatic check off.

**Why this priority**: Users who disable automatic checks, or who heard about
a release elsewhere, need a direct way to check.

**Independent Test**: Run the manual check while online with an older build,
with a current build, and while offline; then turn automatic checks off and
confirm startup makes no network request.

**Acceptance Scenarios**:

1. **Given** a newer release, **When** the user runs **Check for updates**, **Then** the dialog opens directly, even if that version was skipped.
2. **Given** the running build is current, **When** the user runs **Check for updates**, **Then** Casso says it is up to date.
3. **Given** no network, **When** the user runs **Check for updates**, **Then** Casso reports that the check failed and why, in plain words.
4. **Given** automatic checks are off, **When** Casso starts, **Then** no network request is made for updates.

---

### Edge Cases

- The running build is newer than the latest release (a developer or prerelease build): no indicator, and the manual check reports that the build is up to date.
- A release exists but has no asset for this architecture or install type: the indicator still appears, and **Update now** is replaced by a link to the release page.
- A copy installed by winget is updated from the release's MSIX directly. Afterward winget sees the same package at the new version, so a later `winget upgrade` has nothing to do and does not conflict.
- The user skipped several releases: the dialog shows the changelog entries for every version between the running one and the new one, newest first.
- A copy built locally and then moved outside the source checkout: it is still recognized as a developer build and is never updated in place.
- A zip copy under a protected folder such as Program Files: covered by the unwritable-folder case, so Casso offers the release page.
- The release service limits the request rate or returns malformed data: treated as a network failure.
- Two Casso windows are open at once: only one runs the automatic check, and an update closes or waits for the other instance before replacing files.
- The user starts an update and then closes Casso during the download: the download is abandoned and nothing is changed.
- The clock jumps backward: the daily limit still allows a check within 24 hours of real time.
- A prerelease is published: it is never offered.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST determine the latest published, non-prerelease version and compare it with its own version.
- **FR-002**: When automatic checks are on, Casso MUST check at startup, at most once every 24 hours.
- **FR-003**: Settings MUST offer an option to turn automatic checks on or off; it MUST default to on.
- **FR-004**: Casso MUST provide a **Check for updates** command that ignores the daily limit and any skipped version.
- **FR-005**: No part of a check, a download, or an install MUST block the UI or slow emulation.
- **FR-006**: When a newer, unskipped version is known, Casso MUST show an indicator in the title bar.
- **FR-007**: Clicking the indicator MUST open a dialog that shows the running version, the new version and its release date.
- **FR-007a**: The dialog MUST show the CHANGELOG entry for each version newer than the running one, up to and including the new version, newest first.
- **FR-007b**: When the README has a release highlight for a major or minor version in that range, the dialog MUST show that highlight above the changelog entries. It MUST NOT show the rest of the README.
- **FR-007c**: Both MUST be read from the released version's own README and CHANGELOG, not from the running copy, and MUST be shown as formatted text.
- **FR-008**: The dialog MUST offer **Update now** and **Skip this version**. Closing the dialog MUST leave the indicator in place.
- **FR-009**: **Skip this version** MUST suppress the indicator for that version only, and MUST persist across restarts.
- **FR-010**: Casso MUST detect how this copy is running: as an installed MSIX package (by hand or by winget, which installs the same package), as an unpacked release zip, or as a developer build.
- **FR-011**: **Update now** MUST apply the update by the method for that install type, and MUST restart Casso on the new version when it is done: for MSIX, an in-place upgrade of the installed package from the release's MSIX bundle; for a zip copy, replacing its files from the release zip for its architecture.
- **FR-011a**: A developer build MUST NOT offer **Update now**. The dialog MUST tell the user to pull and rebuild instead.
- **FR-011b**: A copy MUST count as an official release only when its executable has a valid signature from Casso's publisher. Any other copy, unsigned or signed by anyone else, MUST count as a developer build, wherever it runs from.
- **FR-011c**: The release build MUST fail rather than publish an unsigned release, so that every official release has the signature that FR-011b checks.
- **FR-012**: A zip update MUST verify the downloaded package before it changes any installed file, and MUST restore the previous version if the replacement fails.
- **FR-013**: Before the restart, Casso MUST apply the same handling of unsaved emulated-disk writes and settings that a normal exit applies.
- **FR-014**: A failed automatic check MUST be silent. A failed manual check or a failed update MUST be reported in plain words, with a link to the release page.
- **FR-015**: When an update cannot be installed automatically (no matching asset, unwritable folder, or an unsupported install type), Casso MUST offer the release page instead.
- **FR-016**: The time of the last check, the latest version seen, and the skipped version MUST persist across restarts.
- **FR-017**: Casso MUST send no data other than what fetching release information requires, and it MUST NOT identify the user.

### Key Entities

- **Release**: a published version, with its version number, release date, release notes, prerelease flag, and download assets for each architecture and package type.
- **Install type**: how this copy is running: MSIX package, release zip, or developer build. It decides how an update is applied, or whether one is offered at all.
- **Update state**: the time of the last check, the latest version seen, the skipped version, and whether automatic checks are on.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Within 10 seconds of startup, an out-of-date copy with automatic checks on shows the indicator, given a working network.
- **SC-002**: During a check, a download or an install, emulation speed stays within normal run-to-run variation, and the UI never stops responding.
- **SC-003**: For both the MSIX and the zip install types, **Update now** brings Casso to the new version and restarts it without the user visiting a web page or running a command.
- **SC-004**: In every simulated failure (interrupted download, corrupt package, failed file replacement, failed package install), the previously installed version still launches.
- **SC-005**: A skipped version never shows the indicator again, across at least three restarts.
- **SC-006**: With automatic checks off, a startup makes no update-related network request.

## Assumptions

- GitHub Releases is the record of truth for what has been published, and the source of every update download.
- The winget package is the same MSIX that the release page offers. Windows records no difference between the two installs, and upgrading either one from the release's MSIX leaves the same state that `winget upgrade` would, so both use one path. This also avoids the delay before winget receives a new release.
- Installing a higher-versioned MSIX with the same package identity is an in-place upgrade that keeps the app's data, much like a major upgrade of an MSI.
- Prereleases are never offered. An opt-in prerelease channel is out of scope.
- The release zip and MSIX assets keep their current file names, so the right asset for an architecture and install type can be found by name.
- The README release-highlight headings and the CHANGELOG version headings keep their current formats, so the section for a version can be found by its heading.