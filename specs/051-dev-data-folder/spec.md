# Feature Specification: Separate data folder for developer builds

**Feature Branch**: `051-dev-data-folder`

**Created**: 2026-11-18

**Status**: Draft

**Input**: Every Casso build on a machine, master and every worktree, reads and writes the same `%LOCALAPPDATA%\Casso` folder. A newer build's machine configs break an older build, and test launches of branch builds overwrite the owner's `UserPrefs.json`. Move developer builds to a folder in their checkout. Shipped copies use the usual folder and start correctly over files a newer release wrote.

## Background

Observed 2026-11-18 while testing the `041-disk-integrity` branch, which is older than master:

1. `--machine Apple2e` failed with "Liron.rom not found". Master builds had rewritten the //e and //e Enhanced configs with a Liron card in slot 5. The older build skips a config with a higher `$cassoMachineVersion` and boots it as is, and its ROM check reads only its embedded config, so the missing ROM surfaces when the card is built.
2. `--machine Apple2cPlus` failed with "No embedded config for machine 'Apple2cPlus'". The machine list includes every subfolder of `Machines\`, so the older build lists the //c Plus folder master created.
3. Every test launch changed the owner's `UserPrefs.json`: last machine, scene view, desk floppies, window placement, the disk MRU and `disk1Path`. A disk under the exe's folder is stored relative to the exe, so a path written by a worktree build points somewhere else for master.

`CASSO_PROFILE_DIR` already redirects the folder, but only when a session sets it. An older branch may predate it, and a handful of call sites read `LOCALAPPDATA` directly and bypass it.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A developer switches branches (Priority: P1)

The owner builds master, then a feature branch in a worktree, then an older branch, and launches each. Each checkout has separate machine configs, prefs, ROMs, themes and disks, so no build changes the files another build reads.

**Why this priority**: This is the failure that cost the 2026-11-18 session, and every developer launch hits it.

**Independent Test**: Build two checkouts at different commits, launch each, change settings in each, and confirm neither checkout's data folder changes when the other build runs, and that `%LOCALAPPDATA%\Casso` doesn't change at all.

**Acceptance Scenarios**:

1. **Given** an unsigned build inside a clone, **When** it starts, **Then** it reads and writes only `<repo root>\CassoData\`.
2. **Given** an unsigned build inside a worktree, whose `.git` is a file, **When** it starts, **Then** it uses `<worktree root>\CassoData\`.
3. **Given** Debug x64 and Release ARM64 builds from one checkout, **When** each starts, **Then** both use the same `CassoData\` folder.
4. **Given** a master build has written //e configs with a Liron card, **When** an older branch's build starts in that branch's checkout, **Then** it boots the //e config from that checkout.
5. **Given** a checkout whose `CassoData\` doesn't exist yet, **When** its build starts, **Then** it starts from defaults and offers the usual ROM download.

---

### User Story 2 - An agent makes a throwaway test launch (Priority: P1)

An agent launches a branch build to check a change. The owner's installed copy and its prefs are untouched afterward, with no backup and restore step.

**Why this priority**: Test sessions back up and restore `UserPrefs.json` manually today, and a missed restore wipes out the owner's settings.

**Independent Test**: Record the hashes of every file under `%LOCALAPPDATA%\Casso`, launch a developer build, change settings and mount disks, exit, and compare.

**Acceptance Scenarios**:

1. **Given** an agent launches a developer build, **When** it exits, **Then** every file under `%LOCALAPPDATA%\Casso` matches what was there before.
2. **Given** `CASSO_PROFILE_DIR` is set, **When** any build starts, official or not, **Then** it uses that folder for everything.
3. **Given** a developer build crashes, **When** it writes a dump, **Then** the dump goes under its data folder.

---

### User Story 3 - A shipped user installs an older release (Priority: P2)

A user who ran a newer release installs an older one through winget, the MSIX or a zip. The older release uses the same `%LOCALAPPDATA%\Casso` folder and starts every machine it supports.

**Why this priority**: Shipped users have one folder and can't be separated from newer files, so the build itself has to tolerate them. It's less common than the developer case.

**Independent Test**: Start from a data folder a newer build wrote, with a higher-version //e config and a machine folder the older build has no config for, and launch the older build.

**Acceptance Scenarios**:

1. **Given** the //e config on disk has a higher `$cassoMachineVersion` than the build's embedded copy, **When** the build boots the //e, **Then** it uses its embedded config for that session and the file on disk is not written.
2. **Given** `Machines\` contains a machine the build has no embedded config for, **When** the machine list is displayed, **Then** that machine is absent from it.
3. **Given** `--machine` specifies a machine the build has no embedded config for, **When** it starts, **Then** it reports an unknown machine as it does for any other unknown name.
4. **Given** the older build ran, **When** the newer release starts again, **Then** its configs are as it left them.

---

### User Story 4 - Machine config upgrades keep hand edits (Priority: P3)

A user edits a machine config manually and later installs a release with a newer version of that config. The edited file is renamed to a `.user-backup-<timestamp>` file before the new default replaces it.

**Why this priority**: Today a config with a lower version is overwritten with no backup, which destroys an edit. It's rare because Settings saves changes to `UserPrefs.json`, not to the machine file.

**Independent Test**: Write a machine config with an older version and content that doesn't match any shipped default, launch, and confirm a backup exists with the original bytes.

**Acceptance Scenarios**:

1. **Given** a config with a lower version whose content matches a known shipped default, **When** the build starts, **Then** it's replaced with no backup.
2. **Given** a config with a lower version whose content doesn't match any known shipped default, **When** the build starts, **Then** it's renamed to a backup and then replaced.

### Edge Cases

- An unsigned exe copied outside any repo, for example to a USB stick, uses `CassoData\` beside the exe.
- An exe in a folder with no write access: the build reports that the data folder can't be created, the same way it reports a `%LOCALAPPDATA%` failure today.
- `.git` exists as both a folder in a parent and a file in a nearer folder: the nearest one is used, so a worktree nested under the primary checkout uses the worktree's root.
- `CassoData\` in a repo is listed in `.gitignore`, so its contents are absent from `git status`.
- A locally built MSIX has a package identity and counts as official. This is accepted.
- A signed exe run from inside a repo is official and uses `%LOCALAPPDATA%\Casso`; the signature determines this, not the location.
- The signature check fails to run, for example a crypto API error: the build counts as a developer build, so the shipped folder is not written.
- Two developer builds from one checkout run at once: they share that checkout's folder, as all builds share one folder today.

## Requirements *(mandatory)*

### Functional Requirements

**Choosing the data folder**

- **FR-001**: Casso MUST choose its data folder once per process, before reading any prefs or config, and use that folder for the rest of the process.
- **FR-002**: When `CASSO_PROFILE_DIR` is set, the data folder MUST be that folder, for every build.
- **FR-003**: Otherwise, a build that counts as official MUST use `%LOCALAPPDATA%\Casso`. A build counts as official when it runs with a package identity, or when its exe's Authenticode signer matches the official publisher. These are the rules the updater uses today.
- **FR-004**: Otherwise, the build MUST look for a repo root by checking the exe's folder and then each parent folder in turn for a `.git` entry, which may be a folder or a file. The first folder that has one is the repo root, and the data folder MUST be `<repo root>\CassoData\`.
- **FR-005**: When no repo root is found, the data folder MUST be `<exe folder>\CassoData\`.
- **FR-006**: The layout inside the data folder MUST be identical in every case: `Machines\`, `Themes\`, `Disks\`, `UserPrefs.json` and the rest at the same relative paths.
- **FR-007**: A developer build MUST NOT read or write anything under `%LOCALAPPDATA%\Casso` unless `CASSO_PROFILE_DIR` points there. This includes ROMs, stock disks, themes, links, logs, thumbnails, crash dumps and updates.
- **FR-008**: A developer build whose data folder is new MUST start from defaults, with no copy from `%LOCALAPPDATA%\Casso`.
- **FR-009**: When the signature check fails to run, the build MUST count as a developer build.
- **FR-010**: The repo's `.gitignore` MUST list `CassoData/`.

**Call sites**

- **FR-011**: Every file Casso reads or writes at runtime MUST resolve through the chosen data folder. This includes crash dumps, the thumbnail cache, the update staging folder and the storage card broker, which read `LOCALAPPDATA` directly today.
- **FR-012**: The scenario tests that look for `SSC.rom` and the stock disks MUST find them through the same rules as the build under test, and fail with a message that includes the folder searched when they are missing.

**Older builds over newer files**

- **FR-013**: When a machine config on disk has a higher `$cassoMachineVersion` than the build's embedded copy, the build MUST boot that machine from its embedded config and MUST NOT write the file.
- **FR-014**: The machine list MUST include only machines the build has an embedded config for.
- **FR-015**: `--machine` with a machine the build has no embedded config for MUST fail with the unknown-machine error.

**Config upgrades**

- **FR-016**: A machine config with a lower version whose content doesn't match any known shipped default MUST be renamed to a `.user-backup-<timestamp>` file before it's replaced.
- **FR-017**: A machine config with a lower version whose content matches a known shipped default MUST be replaced without a backup.

**Testability and cost**

- **FR-018**: The folder MUST be chosen by core code that the unit tests can drive with a fake package identity, a fake signature result and a fake file system, covering every row of the folder rules.
- **FR-019**: The startup cost of the signature check MUST be measured on Debug and Release builds, and the result added to this spec's validation notes.

**Documentation**

- **FR-020**: The header comment of the machine config upgrade decision MUST match what the code does for a missing stamp or a stamp of zero.
- **FR-021**: The comment in `.github/workflows/ci.yml` stating that a release goes out unsigned without the signing secrets MUST be corrected; the release job fails an unsigned release.
- **FR-022**: `CLAUDE.md`, `docs/testing.md` and the launch guidance in `.github/copilot-instructions.md` MUST document where a developer build's data lives, and that test launches don't require a prefs backup.

### Key Entities

- **Data folder**: The single folder with every file Casso reads or writes at runtime for one process. Chosen once at startup.
- **Install type**: MSIX, zip or developer, determined from package identity and the exe's signer. MSIX and zip count as official.
- **Repo root**: The nearest folder at or above the exe's folder that has a `.git` entry.
- **Embedded machine config**: The config compiled into the build for each machine it supports, with its `$cassoMachineVersion`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After a developer build is launched, used and closed, every file under `%LOCALAPPDATA%\Casso` is byte-identical to before.
- **SC-002**: With master and an older branch built in separate checkouts, each starts every machine it supports, in either order, with no ROM or config error.
- **SC-003**: An official build started over a folder a newer release wrote starts every machine it supports, and the newer release's files are byte-identical afterward.
- **SC-004**: The unit tests cover every row of the folder rules and each older-build case, and each test fails when the rule it covers is broken.
- **SC-005**: The signature check adds less than 50 ms to startup on a Release build; the measured value is in the validation notes.

## Assumptions

- The owner's installed, signed copy and its `%LOCALAPPDATA%\Casso` folder are untouched by this change; the owner's existing prefs stay there.
- The first launch of each existing checkout after this change starts from defaults and downloads ROMs again, or gets them from `scripts/FetchRoms.ps1`. This is accepted.
- `CassoExplorerPrefs.json` moves with the data folder like every other file. Its contents are out of scope.
- Builds from different checkouts don't find each other through the data folder, and nothing requires them to.
- A locally built MSIX counting as official is accepted.
- No MVP: every requirement ships together.
