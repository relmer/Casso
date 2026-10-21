# 041-disk-integrity handoff

## Where the work is

- Branch `041-disk-integrity` is pushed to origin at `e24a6432a`.
- Phase 2 is done except two tasks:
  - T016, the Debug app run, deferred until after Phase 3.
  - T033, the next `035-debugger` merge.
- Next: T033, then Phase 3 from T034.
- `tasks.md` has the full task state, and `audit.md` and `research.md` have the design.

## The expected Debug assertion

A Debug Casso asserts in `DiskImageStore::GetImage` at startup. The window
thread reads the disk store every frame, and only the CPU thread may access
the store while it runs. Phase 3 moves those reads onto the published drive
status, which ends the assertion. It is not a regression. T016 waits for
Phase 3 for this reason.

## Rules from the last session

These were saved as memory on the first machine, which the second machine
does not have:

- For a serious defect, such as disk corruption, the commit subject may state
  the problem instead of the fix, for example `fix(disk): On the
  //c, writes to the IWM mode register while the disk motor was running (even
  if only spinning down) caused 0 bits to be written at the head's current
  position`.
- A new menu id goes above the highest id in every origin branch's
  `resource.h` and `.rc` files, not just this branch's. `IDM_DISK_SALVAGE_WRITE`
  is 40192 for this reason; 40081 and 40090 are taken on other branches.
