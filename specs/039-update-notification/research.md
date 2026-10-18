# Research: Update notification and self-update

## R-1 Release source

- **Decision**: `GET https://api.github.com/repos/relmer/Casso/releases/latest`.
  It returns only the newest published release that is neither a draft nor a
  prerelease, so "never offer prereleases" holds by construction. The parser
  still rejects `"prerelease": true` defensively.
- **Rationale**: one unauthenticated request; 60/hour per IP is far above one a
  day. Fields used: `tag_name`, `published_at`, `html_url`, `prerelease`,
  `assets[].name`, `.size`, `.browser_download_url`, `.digest` (`sha256:<hex>`).
- **Alternatives**: the Atom feed (no assets, no digest); a version file in the
  repo (would need a second release step).
- Headers: `User-Agent: Casso/<VERSION_STRING>` (replacing the fixed
  `Casso/1.0`), `Accept: application/vnd.github+json`. Nothing else is sent
  (FR-017). HTTP 403/429, a non-200 status, or JSON that fails to parse count as
  a network failure.

## R-2 Release notes

- **Decision**: fetch `https://raw.githubusercontent.com/relmer/Casso/<tag>/CHANGELOG.md`
  and `.../README.md` only when the dialog first opens for that release (not
  on the background check), cached in memory for the session.
- CHANGELOG section: starts at a line matching `^## \[(\d+\.\d+\.\d+)\]`, ends
  at the next `^## ` line. Included when running < version <= new. Order:
  as found, then sorted newest first.
- README highlight: starts at `^### \[[^\]]*·\s*(\d+)\.(\d+)\]`, ends at the
  next `^#{1,3} ` line. Included when (major, minor) is greater than the
  running build's (major, minor) and at most the new one's. The source is
  CP1252 and the files are UTF-8, so the middle dot is matched as UTF-8
  `C2 B7` bytes, written as an escaped literal.
- No match at all for the new version is a parse failure for that document:
  the dialog shows the release page link in its place rather than an empty
  body.

## R-3 Official build identification

- **Decision**: `WinVerifyTrust` with `WINTRUST_ACTION_GENERIC_VERIFY_V2`,
  `WTD_REVOKE_NONE`, `WTD_CACHE_ONLY_URL_RETRIEVAL`, `WTD_UI_NONE`, then read
  the signer certificate subject with `WTHelperProvDataFromStateData` /
  `CertNameToStr (CERT_X500_NAME_STR)` and compare against
  `CN=Robert Elmer, O=Robert Elmer, L=Redmond, S=Washington, C=US`, the
  `Publisher` in `Installer/Package.appxmanifest`.
- The constant lives in one core header. A unit test reads nothing from disk,
  so the agreement with the manifest is checked by a CI step (it already
  compares the bundle's signer subject to the manifest Publisher; the plan adds
  the core constant to that comparison).
- Runs only on the check worker thread (decision from spec owner). The UI thread
  starts with install type "unknown" and never shows **Update to <version>** until the
  worker has classified the copy.

## R-4 Install type

- `GetCurrentPackageFullName` returns success → **MSIX** (signature check
  skipped; Windows verified the package at install).
- Otherwise the module's signature passes R-3 → **Zip**.
- Otherwise → **Developer**.
- Behind `IInstallEnvironment` (package name query, module path, verifier) so
  every branch is a unit test.

## R-5 MSIX upgrade

- **Decision**: `PackageManager.AddPackageByUriAsync` via WRL on the ABI
  headers (`windows.management.deployment.h`), with
  `DeploymentOptions::ForceTargetApplicationShutdown`, after calling
  `RegisterApplicationRestart (L"--updated", 0)`. Windows shuts the running
  package down and restarts it on the new version. Settings live in
  `%LOCALAPPDATA%`, untouched by the upgrade.
- Before calling, Casso downloads the bundle itself (progress, cancel, digest
  check) and passes a `file://` URI, so a failed download changes nothing.
- Unsaved disk writes: the shell runs the normal exit flush
  (`FlushAllForShutdown`, `FlushDeferredGlobalPrefs`) before the deploy call,
  because the forced shutdown does not run the destructor path.
- **Alternatives**: `winget upgrade` (rejected by the owner: delay, extra
  dependency); `ms-appinstaller:` URI (user interaction).

## R-6 Zip extraction

- **Decision**: clean-room `Inflate` (RFC 1951) and `ZipArchive` (APPNOTE
  central directory; methods 0 and 8; CRC-32 checked per entry; rejects
  encryption, zip64 beyond need, and any entry path that is absolute or
  contains `..`).
- **Rationale**: no zip code exists in the tree; Windows has no public inflate
  API; spawning `tar.exe` or the shell's zip folder is a second untestable
  path. Inflate is about 300 lines and fully unit-testable against vectors
  built at test time from stored blocks plus fixed and dynamic Huffman blocks
  captured as byte arrays.
- **Alternatives**: vendoring miniz (MIT; adds third-party code the tree has
  avoided); `tar.exe` (process launch, not mockable).

## R-7 Zip verification and swap

1. Download the zip into memory, check `size` and the asset `digest` SHA-256
   (BCrypt, already used by `FileMatchesSha256`). Missing digest → fail.
2. Parse the archive and inflate every entry in memory (CRC per entry).
3. Write the payload to a staging folder `<install>\.update-new\`; verify the
   staged `Casso.exe` with R-3 (must be official) and that its version
   resource equals the release version.
4. Probe writability first (create and delete `<install>\.update-probe`).
   Failure → offer the release page (FR-015), nothing changed.
5. Swap: for each target file, rename existing to `<install>\.update-old\...`,
   move the staged one in. The running exe can be renamed but not overwritten,
   which this ordering respects.
6. Any failure → move every `.update-old` file back, delete what was moved in,
   report. Success → launch the new `Casso.exe --updated --cleanup-old <pid>`
   and close normally (which flushes disks and prefs). The new process waits
   for the old PID to exit, then deletes `.update-old`.
- Other running instances: before step 5, a named mutex count
  (`Local\Casso.Instance`, held by every instance) shows whether another is
  running; if so the dialog asks the user to close it and stops.
- All steps go through `IUpdateFileSystem`, so the restore paths are tested by
  failing the Nth rename.

## R-8 Schedule

- State: `lastUpdateCheckUtc` (seconds since epoch), `latestKnownVersion`,
  `latestKnownTag`, `skippedVersion`, `autoUpdateCheck`.
- Due when automatic is on and (no previous check, now - last >= 24 h, or
  now < last, which handles a backward clock jump).
- Only one instance runs the automatic check: `Local\Casso.UpdateCheck` mutex,
  taken with a zero timeout. An instance that does not get it re-reads the
  update fields from the prefs file every 5 s for up to 2 minutes, adopts
  the record once its check time is newer than at launch, and decides the
  indicator as the not-due path does; when the wait runs out, the record on
  disk decides it.
- Not due: the indicator shows from `latestKnownVersion` when it is newer than
  the running build and not skipped. Release notes are fetched on dialog open.

## R-9 Threading

- One `std::thread` per check or update, owned by `UpdateService`, with an
  `std::atomic<bool>` cancel flag that the shell sets on shutdown and joins.
  Results go back as `WM_APP_UPDATE_RESULT` with a heap payload freed by the
  receiver (pattern from `EmulatorShellDisks.cpp`).

## R-10 CI

- Replace "Warn that the release is unsigned" with a step that fails
  (`throw`) when `AZURE_CLIENT_ID` is empty and a release would be published.
- Add the core publisher constant to the bundle subject comparison.
