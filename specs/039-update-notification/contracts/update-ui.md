# Contract: update UI

## Title-bar indicator

- An icon button left of the minimize button, visible only when a newer,
  unskipped release is known (or found by a manual check).
- Tooltip: "Update available: Casso <version>".
- Click opens the update dialog.

## Help menu

- "Check for updates..." between the key map item and "About Casso...".
- Up to date → message box "Casso is up to date" with the running version.
- Failure → message box, label line plus a sentence of cause, and a link to
  the release page.

## Update dialog

- Title: "Casso update".
- Header: "Casso <new> is available. You have <running>." and the release date.
- Body (scrolls): README highlights, then CHANGELOG sections newest first.
- Buttons by install type:
  - Msix / Zip with an asset: **Update now** (default), **Skip this version**.
  - Developer: text "This is a developer build. Pull and rebuild to update.",
    **Skip this version**, and a release page link.
  - No asset, or folder not writable: **Open release page**, **Skip this version**.
- Closing the dialog leaves the indicator.
- During an update: progress text and a **Cancel** button replace the actions
  until the install step begins.

## Settings

- Checkbox "Check for updates automatically", default on.

## Command line

- `--updated` (internal): marks a post-update launch.
- `--cleanup-old <pid>` (internal): wait for that process, delete `.update-old`.
