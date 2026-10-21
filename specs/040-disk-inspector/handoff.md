# 040 Disk Inspector: Handoff

State as of 2026-10-21. The `040-disk-inspector` branch is clean and pushed;
the last code commit is `c7e0ae465`.

## Setting up on another machine

1. Pull `040-disk-inspector`.
2. `.specify/feature.json` is untracked, and each checkout has its own. Before
   any `/speckit-*` command, run:

   ```powershell
   $env:SPECIFY_FEATURE_DIRECTORY = 'specs/040-disk-inspector'
   ```

## Waiting on the owner

- **Find hit tooltips, by mouse.** Hover over the yellow find marks on the
  platter, the track strip, the sector buttons, the Nibbles tab, hit bytes in
  Sector data and file map cells. Each tooltip should end with a line such as
  `Find hit for "D5 AA 96"` beside a yellow swatch. Posted mouse input does not
  open Casso's tooltips, so these were checked by unit tests only, not on
  screen. The Sector data tooltip, the sector button count and the file map
  line have no tests of their own.
- **The earlier tooltip fix, by mouse.** Posted input cannot open it either.
- **Menu bar: yes or no.** The mockup in the session has File, Edit, View, Go, Tools,
  Window and Help over a slimmer toolbar. Nothing is built.

## Open work

- `tasks.md` has no tasks for the recent spec additions (FR-038 shimmer and
  marquee changes, FR-056 find bar, hit marks, hit tooltips and Legend entry).
  Run `/speckit-converge` to append them.
- T160: save the zoom.
- T161: adopt 033's status bar once 033 merges.
- Phase 18 (T153 to T159) and the phases after it.

## Known gaps

- File map grid hit marks are not verified on screen; capture clicks landed on
  the file list instead of the grid.
- A hit in sector data is placed on the strip and platter through the nibbles
  of its bytes. The placement is exact for 6-and-2 sectors and approximate for
  5-and-3, whose bytes are not in order.
- `UnitTest/Fixtures/dos33.dsk` is an empty file.
- Selecting a find hit zooms the platter to the hit, by design. On a short hit
  the zoom goes as high as several hundred times.
