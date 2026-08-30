# 018 desk scene: handoff

Written on relmer-SP8, for a fresh session on relmer-desktop. Branch
`desk-scene-models`, cut as **1.21.0**, "The one where the skeuomorphic theme
goes to 11".

## Read this first

**The branch may not have reached origin yet.** If `git log origin/desk-scene-models -1`
shows `78c1ac01`, the ~500 commits below it are still only on SP8 and nothing
here applies. Ask before doing anything else.

## What this session is for

Two jobs, in order.

### 1. Run the style gate

SP8 cannot: `CheckStyle.ps1` is diff-scoped against origin, the merge from
master put ~650 files in that scope, and it runs past every timeout available
there. This machine is faster.

```powershell
.\scripts\CheckStyle.ps1 -Mode Diff -Against 78c1ac01 -Revision HEAD
```

It should pass. The whole-tree audit was run on SP8 and is clean, and the tree
is a superset of the diff scope:

```powershell
.\scripts\CheckStyle.ps1 -Mode Tree     # 1185 files, 0 violations
```

If it fails, the violations are real. `scripts/FixDeclAlign.ps1 -Apply` repairs
CS0019; CS0016 is a blank-line count after a declaration run, exactly 3.

### 2. Merge to master

Only after the gate passes. The merge gate is otherwise green, all verified on
SP8 at `35418e12`:

| check | result |
|---|---|
| Debug x64 | build clean, suite 4366/4366 |
| Release x64 | build clean 0/0, suite 4363/4363 |
| ARM64 Release | build clean 0/0 (build-only, no device) |
| Code Analysis | clean rebuild, 0 warnings 0 errors |
| CheckStyle tree | 1185 files, 0 violations |
| CHANGELOG | 1.21.0 section written |
| README | What's New section with five captures |
| Version.h | 1.21.0 |

Release runs three fewer tests than Debug. That is the Debug-only assert tests,
not a gap.

## What is in the release

The skeuomorphic theme became a 3D desk scene: four CAD-built devices (Monitor
II, Monitor //c, Disk II, Disk IIc) at true dimensions, per-pixel lighting,
cast and contact shadows, the picture mapped onto spherical-sag glass with
input inverse-projected back through the curvature. Bezel tilt, scene rotation
by drag or compass, phosphor as a property of the monitor, occluded hit
testing, and an idle-render fix that took idle GPU from ~47% to 0.

`CHANGELOG.md` has the entries. The README's "The skeuomorphic theme goes to
11 (1.21)" section has the prose and pictures.

## After the merge

**Movable drives** is the next feature, and the last unstarted piece of 018.
Groundwork already in place:

- `DeskSceneLayout::MakeDeviceWorld` builds each device's world matrix;
  `driveTx[0]` / `driveTx[1]` set the side-by-side placement.
- `DeskSceneHitTester` already resolves per-drive rays with occlusion.
- `EmulatorShell`'s bezel-tilt and compass drags are the pattern to model a
  drive drag on.
- `GlobalUserPrefs::monitorTilt` shows how to persist a per-device scene
  property.

Also open on 018: Disk II realism tweaks, //e monitor (A2M2010) refinement.

**Parked:** GH #131, the steady-state GPU cost that scales with window area
rather than picture area. Its first step is a measurement, not a design. It
overlaps #100, so plan them together rather than building invalidation twice.

## Things that will bite

- **Two machines, and only git crosses.** No files, no local git config.
  Check `$env:COMPUTERNAME` before theorising about the filesystem.
- **Builds and checks now run at BelowNormal** by default (`scripts/HostLoad.ps1`).
  `-NormalPriority` opts out. MSBuild node reuse is off while lowering, because
  reused workers predate the build and never inherit priority.
- **`RunTests.ps1`'s staleness guard false-positives** on Casso-only edits: it
  compares against the newest source anywhere, and UnitTest.dll does not
  compile the shell. Prove the changed files are not in `UnitTest.vcxproj`
  before reaching for `-AllowStale`.
- **`InspectScene.ps1` is flaky.** `Process.MainWindowHandle` intermittently
  reports 0 when the window plainly exists; enumerate for the `CassoWindow`
  class instead. Worth fixing.
- **Casso holds its own exe.** A build fails with LNK1168 while it is running.
  Kill only instances from this worktree, never all of them.

## Style, learned the hard way this session

- CHANGELOG entries are TERSE. One or two lines, the user-visible effect, stop.
  No mechanism, no numbers. "Reduce GPU use when idle." is a complete entry.
- Dashes mostly should not be there. Rewrite around them. When one is used it
  abuts the text, never spaced.
- American spelling everywhere, including commit messages. The gate enforces it.
- No Claude trailers in commits.
