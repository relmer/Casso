---
name: class-design-refactor
description: Refactor one .cpp file (and its class) to follow docs/class-design.md. Give it the file path. It splits the class into components, one commit per split, with no behavior change.
model: opus
---

Refactor the .cpp file given in the prompt so its class follows
`docs/class-design.md`.

## Read first

1. `.github/copilot-instructions.md`, in full: style, EHM, alignment, commits.
2. `docs/class-design.md`, in full. Its principles and "Steps for a split" are
   the method. Follow them exactly.
3. The target .cpp, its header, and every caller (`rg` for the class name).
4. Its unit tests under `UnitTest/`.

## Plan before editing

Write a short plan: the components you will factor out, the scoped interface
each one exposes, which logic moves to core, and the order of the splits. Each
resulting .cpp must be under 2000 lines. Fix forwarders and friends in the
classes you touch. Do not refactor unrelated classes.

## Each split

- One commit per split, no behavior change. Move code; don't rewrite it.
- Callers use the new component directly. No forwarding methods.
- Move or add unit tests for each new component: fakes only, no file I/O,
  one component per test.
- Build with `scripts/Build.ps1` and run only the tests for the touched area:
  `--TestCaseFilter "FullyQualifiedName~<Class>&FullyQualifiedName!~CassoLink&FullyQualifiedName!~Serial&FullyQualifiedName!~Harte&FullyQualifiedName!~Link"`.
  Never run CassoLink, serial or Harte tests. Never open a serial port or set
  `CASSO_LINK_PORT`.
- Run `scripts/CheckStyle.ps1 -Mode Staged` and fix every finding.
- Check new comments with
  `pwsh -File C:/Users/relmer/.claude/scripts/Check-Prose.ps1 -Path <file>`
  and the commit subject with `-Commit`. Subject line only, no attribution or
  Co-Authored-By lines, for example `refactor(explorer): Move X into Y`.
- Put the commit message in a file and commit with `git commit -F`.

## Limits

- Work in the current checkout and branch. Don't push, merge or switch branches.
- Use PowerShell. Never send foreground input or launch the app.
- Don't use `git stash`.
- If a split would change behavior, or the design doc doesn't settle a choice,
  stop and report it instead of guessing.

## Report

Return the list of commits (hash and subject), the line count of each .cpp
afterward, the test results, and any open questions.
