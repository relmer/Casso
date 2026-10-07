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
- Opener, above the header in a larger bold face: one excited line picked at
  random per dialog ("Ooh ooh, new toys, new toys!!", "ZOMG! Fresh Casso
  available!!", "I love it when a plan comes together.", "I love the smell of
  fresh Casso in the morning!", "This just in...", "Huzzah!", and a few more).
- Header: "Casso <new> (released <date>) is available. Sadly, you're still
  using <running>" and one remark, picked at random per dialog:
  - a short judgement after an abutting em dash: "...still using
    <running>—how gauche." (about a dozen: "how quaint", "positively
    vintage", ...), or
  - when the running build's date is known (from its heading in the new
    tag's CHANGELOG), an age remark instead: the sentence ends "...still using
    <running>." and the remark follows on its own line with the day count,
    for example "Wait, this can't be right—you haven't updated Casso in 47
    days? Srsly?" ("1 day" for one day). A date in the future gives no age.
  Without a known age only the short judgements are drawn. The remark is
  added once the notes arrive; until then the header ends at "<running>.".
- Body (scrolls): README highlights, then CHANGELOG sections newest first.
  Images fit the body width (or their percent of it), are never scaled past
  their own size at the current DPI, keep their aspect, and have their
  caption centered beneath in a smaller muted face. Until an image loads, a
  box of placeholder height shows its alt text; a failed image keeps it.
- Buttons: **Skip this version** bottom-left; the primary bottom-right.
  - Msix / Zip with an asset: **Update to <version>** (default), for example
    "Update to 1.30.0".
  - Developer: no update button. In its place, bottom-right, a nudge picked
    at random: "Psst... you should probably pull and rebuild." or one of a
    few others in the same voice. A release page link shows above. No
    default button, so Enter does not skip.
  - No asset, or folder not writable: **Open release page** (default).
- The status rows (progress, failure) take space only while a status shows.
- Closing the dialog leaves the indicator.
- During an update: progress text, and the primary button becomes **Cancel**
  until the install step begins.

## Settings

- Checkbox "Check for updates automatically", default on.

## Command line

- `--updated` (internal): marks a post-update launch.
- `--cleanup-old <pid>` (internal): wait for that process, delete `.update-old`.
