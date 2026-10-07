# Contract: update UI

## Title-bar indicator

- Left of the minimize button, visible only when a newer, unskipped release
  is known (or found by a manual check): a download arrow and a short line of
  text in the accent color, right-justified against the caption buttons, one
  line picked at random per session from about ten ("Shiny new Casso!",
  "Psst—update me", "Casso 1.30.0 is out", ...; some give the version).
  Hover and press fills match the caption buttons.
- When the caption is too narrow for the text beside a title of at least
  80 DIPs, the indicator drops to the arrow alone; the title gives way first.
- Shimmer: 2 s after it appears and then every 8 s (start to start), a sweep
  of 3.58 s. A lead pass of four glints crosses the text in 1 s; when it is
  half way across, a soft diagonal band (30% of the text's width, smoothstep
  edges) follows over 2 s with four more glints of its own. Each glint fades
  in over 0.5 s, holds 80 ms and fades out over 0.5 s, starting as its pass
  reaches its x. Every sweep scatters both passes' glints afresh at random
  spots, each set inside the text, sorted and spaced apart; a band glint that
  would be lit beside a lit lead glint on the same edge moves to the other
  edge, or the band set is drawn again. Hovering starts a sweep at once
  unless one is running, and the next periodic sweep comes a full period
  later. Frames are requested only during a sweep; between sweeps nothing
  repaints. With the system's "Show animations" off the text is static.
- Tooltip: "Update available: Casso <version>", after the usual dwell, placed
  clear of the pointer: against the indicator grown by the pointer's image
  above and below its hot spot, plus 4 DIPs, so it never sits under the arrow.
- Click opens the update dialog.

## Help menu

- "Check for updates..." between the key map item and "About Casso...".
- Up to date → message box "Casso is up to date" with the running version.
- Failure → message box, label line plus a sentence of cause, and a link to
  the release page.

## Update dialog

- Title: "Casso update".
- Resizable, with maximize and close on the caption; double-clicking the
  caption maximizes and restores. Default size 600 x 560, minimum 480 x 420
  (opener, wrapped header, a few lines of notes, the link and the button row
  still fit). On a resize the header and notes rewrap, images rescale to the
  new width (never past their own size), the notes keep the same fraction of
  the way down, and the buttons stay pinned bottom-left and bottom-right.
- A resize gripper, six muted dots in a 45-degree triangle, marks the
  bottom-right corner and resizes from there; it is hidden while maximized.
- Opener, above the header in a larger bold face: one excited line picked at
  random per dialog ("Ooh ooh, new toys, new toys!!", "ZOMG! Fresh Casso
  available!!", "I love it when a plan comes together.", "I love the smell of
  fresh Casso in the morning!", "This just in...", "Huzzah!", the Firefly lines ("Curse your sudden but inevitable update!",
  ...), and a few more).
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
- Notes tabs: a tab strip over the body with "What's new" (README highlights)
  and "Changelog" (CHANGELOG sections newest first). What's new is selected
  when it has content. Without README highlights for the range there is no
  strip at all, only the changelog: a lone tab would be a control with nothing
  to choose. The loading and could-not-load notices also stand alone. Each tab
  scrolls on its own and keeps its position when the other is shown. The
  body starts 8 DIPs below the strip, or below the header when there is none.
- Header: as many lines as the versions sentence and any age remark wrap to,
  measured when painted; it grows when the remark arrives.
- Body (scrolls): the selected tab's notes.
  Images fit the body width (or their percent of it), are never scaled past
  their own size at the current DPI, keep their aspect, and have their
  caption centered beneath in a smaller muted face. Until an image loads, a
  box of placeholder height shows its alt text; a failed image keeps it.
- Buttons: **Skip this version** bottom-left; the primary bottom-right.
  - Msix / Zip with an asset: **Update when closed** and **Update now**
    (default), side by side, no version in either label.
    - Update now: download, check, install, and restart Casso. The restart
      repeats --title, --trace and --no-image-watch from the original
      command line and nothing else (the machine, disks, tape and seed are
      already in the preferences, or are for one run only).
    - Update when closed: download and check now (a zip copy is also
      extracted and its Casso.exe checked, so errors show in the dialog).
      The dialog then says "Casso will update when you close it. Update now
      restarts it on the new version.", the indicator reads "Updates when
      you close Casso" with no shimmer, and Update now applies it at once.
      A zip copy swaps its files in after the normal exit flush and does not
      relaunch; an MSIX copy is registered by Windows once Casso exits. The
      next launch on the new version says "Casso was updated to version X."
      and removes .update-old. A swap that fails at exit puts the old files
      back and is reported at the next launch; if Casso ends without closing
      normally, nothing is swapped and the staged files are removed at the
      next launch.
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
