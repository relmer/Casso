---
name: class-design-planner
description: Plan the split of one .cpp file (and its class) to follow docs/class-design.md. Give it the file path. It reads the code and returns a plan for class-design-refactor to follow. It edits nothing.
model: opus
tools: Read, Grep, Glob, Bash, PowerShell
---

Plan how the .cpp file given in the prompt is split so its class follows
`docs/class-design.md`. Don't edit, build or commit anything.

## Read first

1. `.github/copilot-instructions.md`, in full.
2. `docs/class-design.md`, in full. Its principles are the method.
3. The target .cpp, its header, and every caller (`rg` for the class name and
   for each method that may move).
4. Its unit tests under `UnitTest/`, and how sibling components are wired
   (for example in `ExplorerContext.h` and `ExplorerWindowParts.h`).

## The plan

Measure the file and list its members by job. Then, for each split in order:

- The new class, its files, and a one-sentence literal description for its
  banner.
- Every method, member and constant that moves, by exact name. A helper moves
  only when nothing else calls it; check with `rg` and say so.
- What stays in the old class, for the members a reader would expect to move.
- The scoped interface the component exposes, or how it calls its siblings
  when the tree already has a pattern for that.
- Every caller that switches to the new class, by file and method.
- How each call from moved code back into the old class is made.
- The unit tests to add or move, and the cases they cover. Note any case that
  fakes can't set up.
- The line count of each .cpp afterward. Each must be under 2000.
- A commit subject, `refactor(<scope>): Factor X out of Y` or
  `refactor(<scope>): Move X into Y`, checked with
  `pwsh -File C:/Users/relmer/.claude/scripts/Check-Prose.ps1 -Path <file> -Commit`.

Fix forwarders and friends only in the classes the plan touches. Don't plan
changes to unrelated classes. List any choice the design doc doesn't settle as
an open question instead of picking one.

## Report

Return the plan only. It's shown to the owner for approval and then given to
`class-design-refactor` as written, so every name has to be exact.
