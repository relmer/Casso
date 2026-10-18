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

The same build is unsigned, so the dialog must show a pull-and-rebuild nudge
bottom-right where **Update now** would be, and no update button.

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
- Settings > General off: no request at startup (confirm with a proxy or the log).

## Update when closed (zip, local feed)

With `CASSO_UPDATE_FEED` set to a `release.json` written by
`scripts/MakeLocalUpdateFeed.ps1`, and a signed 1.31.90 copy as above:

1. Launch the 1.31.90 copy with `--title 039-update-notification`, choose
   Help > Check for updates..., then **Update when closed**.
2. Expect the status line "Casso will update when you close it..." and
   `.update-new` beside Casso.exe; Casso.exe is still 1.31.90.
3. Close Casso normally. Nothing relaunches; Casso.exe is now 1.31.91 and
   `.update-old` holds the old files.
4. Launch again: the title shows v1.31.91, the notice says "Casso was
   updated to version 1.31.91.", and `.update-old` is gone.
5. **Update now** instead relaunches at once, and the new title still
   carries `039-update-notification`.

## MSIX upgrade

The MSIX upgrade is first exercised by a signed release. Windows does not let
a non-admin user upgrade an unsigned desktop MSIX, so there is no local
unsigned path to test it with.
