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
- Header: "Casso <new> (released <date>) is available. Sadly, you're still
  using <running>—<judgement>." The em dash abuts both sides. The judgement is
  one short, good-natured remark picked at random per dialog from a list of
  about a dozen ("how gauche", "how quaint", "positively vintage", ...).
- Body (scrolls): README highlights, then CHANGELOG sections newest first.
- Buttons: **Skip this version** bottom-left; the primary button bottom-right.
  - Msix / Zip with an asset: **Update to <version>** (default), for example
    "Update to 1.30.0".
  - Developer: **Update to <version>** shown disabled, with the text "This is
    a developer build. Pull and rebuild to update." near it, and a release page
    link. No default button, so Enter does not skip.
  - No asset, or folder not writable: **Open release page** (default).
- Closing the dialog leaves the indicator.
- During an update: progress text, and the primary button becomes **Cancel**
  until the install step begins.

## Settings

- Checkbox "Check for updates automatically", default on.

## Command line

- `--updated` (internal): marks a post-update launch.
- `--cleanup-old <pid>` (internal): wait for that process, delete `.update-old`.
