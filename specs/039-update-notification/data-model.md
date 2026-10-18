# Data model: Update notification and self-update

## ReleaseVersion

| Field | Type | Rule |
|---|---|---|
| major, minor, patch | int | Parsed from `v1.30.0` or `1.30.0`; anything else fails to parse. |

Total order on (major, minor, patch). `IsMinorLine (other)` compares
(major, minor) only, used for README highlights.

## ReleaseAsset

| Field | Type |
|---|---|
| name | string |
| sizeBytes | uint64 |
| downloadUrl | wstring (host + path split at use) |
| sha256 | 32 bytes, empty when the API gave no digest |

## ReleaseInfo

| Field | Type | Rule |
|---|---|---|
| version | ReleaseVersion | From `tag_name`. |
| tag | string | Used to fetch notes. |
| publishedDate | string (YYYY-MM-DD) | From `published_at`. |
| pageUrl | wstring | `html_url`. |
| isPrerelease | bool | True → rejected. |
| assets | vector<ReleaseAsset> | |

`FindZipAsset (arch)` → `Casso-<ver>-<x64|ARM64>.zip`;
`FindBundleAsset()` → `Casso-<ver>.msixbundle`.

## InstallType

`Unknown` (before the worker classifies), `Msix`, `Zip`, `Developer`.

## UpdateState (persisted in GlobalUserPrefs)

| JSON key | Field | Default |
|---|---|---|
| `autoUpdateCheck` | bool | true |
| `lastUpdateCheckUtc` | int64 seconds | 0 (never) |
| `latestKnownVersion` | string | "" |
| `skippedVersion` | string | "" |

## ReleaseNotes

| Field | Type |
|---|---|
| highlights | vector<NotesSection> (README, newest first) |
| changes | vector<NotesSection> (CHANGELOG, newest first) |

`NotesSection`: version, heading text, markdown body.

## FormattedLine (dialog rendering)

`kind` (Heading, Bullet, Paragraph, Blank), `indentLevel`, `runs`
(text, bold, code, linkUrl).

## Update flow states

```text
Idle -> Checking -> {UpToDate, Available(release), Failed(reason)}
Available -> Downloading -> Verifying -> Installing -> Restarting
Downloading|Verifying -> Failed (nothing changed)
Installing(zip) -> RollingBack -> Failed (old copy restored)
Any -> Canceled (shutdown during download; nothing changed)
```

`UpdateFailure` reasons: Network, RateLimited, BadData, NoAsset,
DigestMismatch, NotOfficial, FolderNotWritable, OtherInstanceRunning,
InstallFailed, RestoreFailed.
