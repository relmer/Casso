# Contract: damaged-track report on insert

Shown by `EmulatorShell::ReportDamagedMount` when an image mounts with damaged
tracks, a checksum mismatch, or both. It follows the existing dialog ("Disk
image is damaged") and Casso's error message format: a short label, then
complete sentences.

Content rules (the exact wording is approved by the owner during
implementation):

- It says the disk is mounted read-only.
- It lists the damaged tracks by track number, using .25/.5/.75 for quarter
  tracks (for example "tracks 3, 7.5 and 12"). More than eight are listed as
  a count plus the first eight.
- When the checksum also fails, both facts appear in one report, not two
  dialogs.
- When the image has flux tracks, the salvage offer says the salvaged copy
  keeps the readable sectors but not flux timing or copy protection.
- The salvage button and flow are the existing ones.

The text is built by a core formatter that `UnitTest` can reach, not inline in
the shell (Principle VI). The tests check the track list, the count cutoff and
the flux sentence.
