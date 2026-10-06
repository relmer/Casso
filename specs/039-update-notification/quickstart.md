# Quickstart: validating update notification

## Unit tests

```powershell
scripts/RunTests.ps1 -Build -Filter Update
scripts/RunTests.ps1 -Build
```

## Old-versioned build (indicator path)

1. Temporarily set `VERSION_MINOR` in `CassoCore/Version.h` below the latest
   release, build x64 Debug. Do not commit.
2. Clear `lastUpdateCheckUtc` in the prefs JSON.
3. Launch with `--title 039-update-notification`. Within 10 s the indicator
   appears. Click it: the dialog lists CHANGELOG entries for every version
   newer than the build, newest first, with README highlights above.

## Unsigned local build

The same build is unsigned, so the dialog must show the developer-build text
and no **Update now**.

## Signed build

Sign a copy of the old-versioned build with a test certificate whose subject
matches the publisher constant only for the duration of the check (or use a
released build with its version resource unchanged and a newer release
present). Expect **Update now** for the zip install type. Do not run the update
against a real install folder without a backup.

## Failure checks

- Offline: the startup check shows nothing; **Check for updates** reports the
  failure.
- Read-only folder: **Update now** is replaced by **Open release page**.
- Settings off: no request at startup (confirm with a proxy or the log).
