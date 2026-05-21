# Phase 0 — Research: Full UI Overhaul

All NEEDS CLARIFICATION items in the planning Technical Context have been
resolved into explicit decisions below. Items still requiring user
confirmation are listed at the end of `plan.md` under *Open Technical
Questions*; they are not blockers for design completion but are blockers for
implementation of the specific task that depends on each one.

---

## R1 — UI framework choice (resolves the "Primary Dependencies" unknown)

**Decision**: Use **RmlUi** (https://github.com/mikke89/RmlUi), MIT-licensed,
vendored in-tree under `External/RmlUi/`, built as a static-library project
(`RmlUi.vcxproj`) added to `Casso.sln`. No vcpkg, no NuGet, no submodule
fetch at build time.

**Rationale**:
- HTML/CSS-like authoring model (`.rml` + `.rcss`) maps directly to the
  theming requirement (FR-029 .. FR-037). Hot-reload of stylesheets is a
  first-class supported scenario.
- Pluggable render backend — Casso supplies a `Rml::RenderInterface`
  implementation that draws through the existing `D3DRenderer` device, so
  there is exactly one swap chain and one device.
- Custom element support is exactly what drive widgets and LED indicators
  need.
- Focus management, tab order, IME, clipboard, and animation are built in,
  satisfying FR-044 keyboard nav without re-inventing focus logic.
- MIT license — compatible with Casso's MIT license without any attribution
  burden on end users (we still attribute in `External/RmlUi/README.casso.md`
  and the About box).

**Alternatives considered**:
- **Dear ImGui** — immediate-mode; not theme-friendly (CSS-style separation
  of skin from logic is foreign to it); custom-skinning the title bar and
  drive widgets to look skeuomorphic would be a hostile use of the library.
- **Ultralight** — feature-perfect (real Blink under the hood) but: (a)
  proprietary commercial license for closed-source / paid use; (b) huge
  binary footprint; (c) GPU-pipeline assumes its own ANGLE-backed driver,
  hard to share our D3D device.
- **Servo embedding** / **CEF** — both enormous; CEF in particular is a
  ~150 MB redistributable; absurd overhead for an emulator UI.
- **Hand-rolled D3D11 primitives + custom focus/tab/layout** — prior plan;
  explicitly rejected by the binding directive in the planning input.

---

## R2 — RmlUi D3D11 render backend (resolves P0-T4)

**Decision**: Write **our own** `RmlBackend_D3D11` from scratch
(`Casso/Ui/RmlBackend_D3D11.{h,cpp}`) implementing `Rml::RenderInterface`
against the shared `ID3D11Device` / `ID3D11DeviceContext` owned by
`D3DRenderer`. Estimated ~600 LOC including the two HLSL shaders.

**Rationale**:
- Fits Casso's EHM / single-exit / top-of-scope-vars style; upstream RmlUi
  backends use C++ exceptions and modern C++ idioms that would be jarring
  beside the rest of `Casso/`.
- Shares the existing device (no second `D3D11CreateDevice` call, no extra
  swap chain).
- Total surface is small: render geometry, scissor, load/release textures,
  optional `SetTransform` for 3D. RmlUi provides reference implementations
  of each in its `Backends/` directory we can read but not copy.
- Lets us hook device-lost recovery into the existing `D3DRenderer` path
  rather than running a parallel recovery state machine.

**Alternatives considered**:
- Fork upstream `RmlUi_Renderer_DX11` — workable but requires patching it
  every time we re-vendor; ongoing maintenance debt.

---

## R3 — Windows 11 borderless window recipe (resolves P0-T5)

**Decision**: Borderless window via
- `WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX`
- `WM_NCCALCSIZE` returning a zeroed-out non-client rect (extending the
  client area into the title-bar region).
- `WM_NCHITTEST` returning `HTCAPTION`, `HTMINBUTTON`, `HTMAXBUTTON`,
  `HTCLOSE`, and the 8 resize-edge codes based on RmlUi element rects
  computed for the title-bar document.
- `DwmExtendFrameIntoClientArea` with margins of `(1, 1, 1, 1)` so DWM
  still composes the drop-shadow / Mica border.
- `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)`
  for Win11 rounded corners.
- Optional Mica via
  `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE, DWMSBT_MAINWINDOW)`.

**Rationale**:
- This is the now-canonical Win11 recipe (used by Terminal, VS, Edge). It
  gives Snap Layouts on the maximize button automatically when the
  `HTMAXBUTTON` hit-test code is returned correctly.
- All APIs used are documented Win11 public DWM APIs — no undocumented
  hacks, no DLL probes.

**Alternatives considered**:
- Pure `WS_POPUP` with no `WS_THICKFRAME` — disables Aero Snap on the
  resize edges. Rejected.
- DPI virtualization tricks for ARM64 — unnecessary; v145 + Win11 ARM64
  handles per-monitor v2 DPI correctly.

---

## R4 — CRT shader source selection (resolves P0-T3)

**Decision**: Initial port set, all MIT or public domain:

| Effect | Source | License | Notes |
|--------|--------|---------|-------|
| Scanlines | `crt-pi-vertical.glsl` (Davide Berra) | MIT | Vertical scanlines parameterized by intensity; port to HLSL. |
| Bloom | libretro `bloom.glsl` (hunterk) | PD | Separable Gaussian; cheap. |
| Color bleed | libretro `ntsc-adaptive` chroma-only pass | MIT | Lateral chroma spread approximating NTSC phosphor. |
| Composite | original | n/a (Casso) | Glue pass. |

**Rationale**:
- All three are well-tested, visually credible, and minimal LOC.
- All MIT/PD — no GPL contamination.
- The libretro shader collection has been ported to D3D many times; no
  unsolved math problems.

**Alternatives considered**:
- **CRT-Royale** — best visual quality available but **GPL**. Excluded.
- **CRT-Lottes** — MIT, but heavy; would not hit the 1 ms frame budget on
  mid-range GPUs at 4× upscale. Keep as a future opt-in if performance
  budget allows.
- Hand-rolled effects — explicitly rejected by binding directive.

**Attribution mechanics**: Each `.hlsl` file carries a top-of-file comment
block: original author, upstream URL, original license SPDX identifier,
SHA of the upstream file we ported from, and the date of the port. The
aggregate appears in `Casso/Shaders/CRT/LICENSES.md` and the About box.

---

## R5 — Drive widget element model (resolves P6-T1)

**Decision**:
- **LED indicators**: RCSS-only. Three CSS classes (`.led--idle`,
  `.led--present`, `.led--active`); glow rendered via `box-shadow` and a
  radial-gradient background; state toggled from C++ by adding/removing
  classes on the parent drive `<div>`.
- **Drive widget body**: Custom `Rml::Element` subclass
  (`DriveWidgetElement`) — required for drag-drop hit handling, animation
  state machine (door open/close, spinning disk), and the eject-button
  child hit region.

**Rationale**:
- LEDs have no behavior of their own — they are pure visual reflections of
  drive state. RCSS-only minimizes code.
- The drive widget has real behavior (drag-drop, click-to-browse, eject)
  that doesn't map cleanly onto generic RML elements; a custom element
  centralizes the behavior in one C++ class.

**Alternatives considered**:
- LEDs as custom elements — overkill; no behavior to host.
- Drive widget as plain `<div>` with JS-style event handlers in RCSS — RmlUi
  does support an embedded scripting layer but we don't want to introduce
  yet another execution environment.

---

## R6 — Per-machine vs. global user preferences split (resolves a schema unknown)

**Decision**: Two distinct on-disk files.
- `Machines/<Name>/<Name>_user.json` — settings that are intrinsically
  about that machine: speed mode, video color mode, write protect, floppy
  sound, hardware component enable/disable, drive image last-mounted
  paths (per FR-047 — auto-remounted on machine load).
- `GlobalUserPrefs.json` — single file at the asset base directory.
  Settings that are about the application or the user, not a specific
  machine: active theme name, CRT brightness, CRT effect enable + params,
  window bounds, last-selected machine.

**Rationale**:
- FR-034 (theme persistence) and FR-040 (CRT params persistence) explicitly
  say "global, not machine-specific."
- Keeps `_user.json` files small, copyable, and shareable between users
  who want to swap machine configs without dragging UI preferences along.
- One global file is operationally simpler than scattering globals into
  the registry or into one arbitrarily-chosen machine's `_user.json`.

**Alternatives considered**:
- Single combined file — rejected per FR-034/040.
- Registry for globals — rejected per FR-016 (registry is being retired).

---

## R7 — Constitution amendment requirement (resolves P0-T1)

**Decision**: Amend `.specify/memory/constitution.md` to v1.5.0 **before**
any third-party source lands in the repo. Replace the absolute
"Windows SDK + STL only" clause with an explicit allowlist mechanism, and
list RmlUi + the in-tree CRT shader ports as the first two approved entries.
See `plan.md` § *Constitution Check* for the exact wording.

**Rationale**:
- The constitution is binding (§ Governance: "supersedes all ad-hoc
  practices"). Silently violating it would set a corrosive precedent and
  invalidate the gate-check in this very plan.
- The amendment is narrow: it does not invite arbitrary dependencies; it
  imposes specific criteria (MIT/BSD/Apache/PD only, source-vendored,
  buildable in-solution, explicitly listed).

**Alternatives considered**:
- Treat the existing clause as advisory — rejected; the constitution
  itself rejects this.
- Per-feature exception note instead of a constitution edit — rejected;
  the constitution's own amendment process is the documented mechanism.

---

## R8 — Font choices (resolves P0-T6, pending user confirmation)

**Decision (default; user may substitute)**:
- Skeuomorphic theme: **Inter** (SIL OFL 1.1) for UI text.
- Dark Modern theme: **Inter** (SIL OFL 1.1) for UI text.
- Retro Terminal theme: **VT323** (SIL OFL 1.1) for chrome text; falls
  back to a system monospace.

**Rationale**: All three fonts are SIL OFL, redistributable, widely tested,
and look right for their themed contexts. Files live under each theme's
`assets/fonts/` directory and are loaded by RmlUi's font interface.

---

## R9 — Cross-thread state for drive widgets

**Decision**: Mirror the existing audio-system pattern. The CPU thread
sets `atomic<bool> motorOn` and `atomic<bool> diskActive` on each
`DriveWidgetState`; the UI thread reads these once per frame in
`UiShell::Render` and toggles RCSS classes accordingly. No locks; no
queues for the visual reflection (only for the disk-insert/eject commands,
which already use the existing command queue).

**Rationale**: Already proven in the audio path; lock-free; bounded latency
of one frame which is well below human perception threshold.

---

## R10 — Test isolation for RmlUi-dependent code

**Decision**: Unit tests cover the **logic** layer only:
- `UserConfigStore`, `GlobalUserPrefs` — full coverage via `IFileSystem` mock.
- `ThemeLoader` — parses `theme.json` from in-memory strings; validates
  schema; returns structured errors. RmlUi is not invoked in tests.
- `SettingsPanelState` — pure value object; full coverage.
- `MachineConfigUpgrade` — extended for the rename + capabilityFlag default.
- Title-bar hit-test math — pure function tested with synthetic rects.
- `RmlBackend_D3D11` — exercised only via a *smoke* test that constructs
  the class with a mock device interface and verifies it doesn't allocate
  on construction. No real GPU work in tests (per constitution §II).

**Rationale**: Matches constitution §II (no real I/O, no real system APIs).
RmlUi's own behavior is the responsibility of RmlUi's test suite, not ours.
