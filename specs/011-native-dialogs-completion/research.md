# Phase 0 Research: Native DX Dialogs Completion

All Technical Context entries are concrete (no `NEEDS CLARIFICATION` markers
left after spec clarification). This document records the design decisions
made by inspecting the existing spec-007 infrastructure.

## Decision 1 — Extract modal-overlay plumbing from `SettingsWindow` into `DialogPrimitive`

- **Decision**: Factor `SettingsWindow`'s modal-overlay show / route-input /
  dismiss / DPI-relayout / theme-repaint machinery into a new
  `Casso/Ui/Dialog/DialogPrimitive` and re-express `SettingsWindow` in
  terms of it (with `SettingsPanel` remaining the body content).
- **Rationale**: This is the only piece of DX chrome that already implements
  a modal overlay correctly under all three themes and four DPI scales.
  Duplicating it for each new dialog would create three copies of the
  trickiest code in the chrome.
- **Alternatives considered**:
  - *Copy-paste the SettingsWindow scaffolding into AssetBootstrap*:
    rejected — three copies of the modal/overlay logic to drift apart.
  - *Leave SettingsWindow alone, build DialogPrimitive as a parallel
    implementation*: acceptable fallback if the extraction surfaces
    unexpected coupling, but `SettingsWindow` would then carry dead
    weight forever. Try the extraction first; fall back only if the
    refactor cost exceeds ~one work day.

## Decision 2 — JSON schema location for the disk MRU

- **Decision**: Add a top-level `recentDisks` string array to the
  `GlobalUserPrefs` JSON document, capped at 16 entries,
  most-recent-first. Load and save plumbing mirrors the pattern of the
  recent `fix(prefs): preserve machines section in GlobalUserPrefs::Save`
  change — read into a typed field, write back from that typed field,
  preserve unknown keys untouched on round-trip.
- **Rationale**: `GlobalUserPrefs` already owns cross-machine user state
  and survives schema additions cleanly. A per-machine config is the
  wrong home — the MRU follows the user, not the machine.
- **Alternatives considered**:
  - *Per-machine MRU*: rejected — users reuse the same disk images
    across multiple emulated machines, and the spec scenarios assume a
    single user-wide list.
  - *Separate `recent_disks.json` sidecar file*: rejected — adds a
    second I/O surface for no benefit; the existing prefs file is
    already well-tested for round-trip.

## Decision 3 — MRU pruning policy on the UI thread

- **Decision**: `Prune` uses `std::filesystem::exists` on the UI thread.
  Network paths that block longer than a cheap stat are treated as
  "still exists" (no removal). Persisted prefs are pruned only when
  `exists` returned a definitive `false`.
- **Rationale**: Spec edge case explicitly accepts this — "leave
  pruning to the next launch where stat succeeds quickly." UI must not
  hang waiting on a flaky network share.
- **Alternatives considered**:
  - *Threadpool stat-then-marshal-back*: over-engineered for 16 entries
    and dragged into the boot-disk-picker render path. Revisit only if
    a real complaint surfaces.
  - *Always prune, blocking*: rejected — hangs the UI thread on slow
    network paths.

## Decision 4 — Hyperlink rendering inside dialog body text

- **Decision**: Extend `DxUiPainter` with a minimal `DrawTextRunsWithLinks`
  primitive that takes a sequence of `{ text, isLink }` runs and reports
  per-link bounding rects to the caller. Hover styling and click
  dispatch live in the dialog primitive, not in the painter. URL launch
  uses `ShellExecuteW` via the EHM-notifying variant for the no-default-
  browser edge case.
- **Rationale**: Keeps `DxUiPainter` doing one thing (rendering text
  runs into known rects); keeps interactivity in the primitive layer
  where input routing already lives. The bounded extension is what the
  spec assumes (Assumption 3 in `spec.md`).
- **Alternatives considered**:
  - *Full rich-text engine*: massively over-scoped. We need exactly
    one link in the About dialog body; we do not need an HTML subset.
  - *Render the URL as a button*: violates the About dialog's
    information-density and looks wrong for `https://github.com/relmer/Casso`
    appearing inline in a paragraph.

## Decision 5 — Asset download progress within the unified startup dialog

- **Decision**: The startup-download dialog reuses today's existing
  download progress reporting (whatever `AssetBootstrap` currently
  uses to surface progress to TaskDialog) and re-targets the
  notifications at the new themed dialog. Aggregate "N of M assets
  downloaded" plus current-asset percent is sufficient — no need for
  parallel multi-bar UI.
- **Rationale**: Keeps the unified-dialog scope to "consolidate three
  modals into one decision point with progress feedback" without
  rebuilding the download engine.
- **Alternatives considered**:
  - *Parallel per-asset progress bars*: rejected — the bottleneck is
    sequential network bandwidth, not UI parallelism.

## Decision 6 — Disk II Debug Dialog incremental conversion strategy

- **Decision**: Stand up the new `DiskIIDebugPanel` alongside the
  existing `DiskIIDebugDialog`. Both bind to the same headless
  `DiskIIDebugDialogState`. Port one control family at a time in
  order of risk (labels → checkboxes → radio buttons → text inputs
  with validation → buttons → ListView → column-header context menu
  → tooltips). Keep the Win32 version building behind a compile-time
  switch until parity is verified, then delete it.
- **Rationale**: This is the heaviest single conversion in the spec
  (FR-010 / SC-010). An incremental, parity-checked approach lets us
  bisect regressions and ship interim builds. The `DiskIIDebugDialogState`
  separation that already exists makes parallel implementations cheap.
- **Alternatives considered**:
  - *Big-bang rewrite*: rejected — the dialog has eight distinct
    control families and a real ListView; a single PR replacing all of
    it would be unreviewable and unbisectable.
  - *Drop column-header context menu / tooltips from the conversion*:
    rejected — FR-010 calls out both as required for parity.

## Decision 7 — Validation-suite gating

- **Decision**: This feature does **not** require Dormann or Harte
  runs. No CPU, assembler, or binary-output code is touched.
- **Rationale**: Both validation suites guard CassoCore / CassoEmuCore
  behavior; this feature touches only the `Casso` Win32 GUI project's
  UI layer plus a JSON-schema addition in `GlobalUserPrefs`.
- **Re-evaluation trigger**: If implementation discovers a need to
  touch any code under `CassoCore/` or `CassoEmuCore/Core/`, re-run
  the gate and run both suites before commit.
