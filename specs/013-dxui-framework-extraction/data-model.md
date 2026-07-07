# Phase 1 — Data Model: Dxui Type Relationships & Ownership

Dxui has no persistent storage. The "data model" here is the runtime object graph: which type owns which, who points to whom, what lives across what lifetime, and how the tree is walked.

## Ownership graph

> **Renamed / reshaped 2026-07**: the top-level owner sketched here as
> `DxuiHostWindow` is now **`DxuiHwndSource`** (WPF `HwndSource` — the HWND /
> swap-chain / pump backend). Consumers no longer instantiate it directly; they
> derive from **`DxuiWindow : DxuiPanel`** (WPF `Window : ContentControl`), which
> owns a `DxuiHwndSource` privately and installs *itself* as the source's
> non-owning content root and `IDxuiHostClient`. See `plan.md` §Architecture.

```text
DxuiWindow : DxuiPanel                          (consumer-facing top-level element)
├── (IS its own content root — children Add<T>'d / Create<T>'d directly onto it)
├── implements IDxuiHostClient (private)         (translates unowned WM_* → Dxui events)
└── unique_ptr<DxuiHwndSource> m_source          (the OS-window backend, below)

DxuiHwndSource                                  (owns the OS window)
├── HWND m_hwnd                                 (Win32-owned; destroyed via DestroyWindow)
├── ComPtr<ID3D11Device> m_device               (canonical device, shared with popups)
├── ComPtr<IDXGISwapChain1> m_swapChain
├── unique_ptr<DxuiPainter> m_painter           (implements IDxuiPainter)
├── unique_ptr<DxuiTextRenderer> m_textRenderer (implements IDxuiTextRenderer)
├── unique_ptr<DxuiPanel> m_root                (owned root; shadowed by SetContentRootRef)
├── DxuiPanel * m_contentRootRef                (non-owning; the DxuiWindow itself)
├── DxuiFocusManager m_focusManager             (attached to the active root)
├── IDxuiHostClient * m_client                  (non-owning; the DxuiWindow itself)
├── vector<unique_ptr<DxuiPopupHost>> m_popupPool   (pool of 3, grow on demand)
├── bool m_composited                             (FR-130: composited-transparent per-pixel-alpha surface)
└── function<void(ID3D11RenderTargetView*,int,int)> m_afterPaintHook  (FR-130: consumer composites after tree paint, before Present)

DxuiPanel (and every IDxuiControl)              (tree node)
├── IDxuiControl * m_parent                     (raw back-pointer; non-owning)
├── vector<unique_ptr<IDxuiControl>> m_children (owns children)
├── unique_ptr<IDxuiLayout> m_layout             (panel owns; per-instance layout state)
├── RECT m_boundsDip                            (assigned by parent's layout)
├── bool m_visible, m_enabled, m_focusable
├── int m_tabIndex                              (override; -1 = use geometry order)
└── (per-widget state)

DxuiPopupHost                                   (separate HWND, shared device)
├── HWND m_hwnd                                 (WS_POPUP | WS_EX_NOACTIVATE)
├── ID3D11Device * m_device                     (non-owning; borrowed from host)
├── ComPtr<IDXGISwapChain1> m_swapChain         (own present surface)
├── unique_ptr<DxuiPanel> m_content             (popup body)
├── HWND m_ownerHwnd                            (back-pointer for owner-chain)
├── DxuiPopupHost * m_parentPopup               (nullable; cascading submenus)
└── promise<int> m_closeResult                  (satisfied on UI thread)

DxuiDialogManager                               (PENDING deletion — see note below)
└── vector<unique_ptr<DxuiDialog>> m_stack      (modal stack; topmost is active)

DxuiDialog : DxuiPanel                          (PENDING: to become DxuiDialog : DxuiWindow)
├── DxuiCaptionBar m_caption                    (Add<T>'d child)
├── DxuiPanel m_content                         (Add<T>'d child)
└── optional DxuiPanel m_buttonRow              (Add<T>'d child if buttons present)
```

> **Dialog model — PENDING (2026-07):** the target is a **no-`DxuiDialogManager`**
> design where a dialog is just a `DxuiWindow` shown via a new `ShowDialog()`
> (modal message loop). `DxuiDialog` + `DxuiDialogManager` are slated for deletion
> and all dialog consumers (StartupDownloadDialog, ROM picker, simple dialogs)
> migrate onto the `DxuiWindow` path. Not yet implemented — the entries above
> describe the still-live legacy shape.

## Lifetimes

- **`DxuiWindow` / its `DxuiHwndSource` backend**: created at app startup (or on-demand for secondary windows), destroyed at shutdown / close. Everything below the window shares its lifetime unless explicitly removed.
- **`IDxuiControl` instances**: owned by their parent `DxuiPanel` via `unique_ptr`. Lifetime = from `Add<T>` to `Remove` / `Clear` / parent destruction. **No detach-and-reuse in v1** (FR-011 clarification Q3).
- **`DxuiPopupHost`**: long-lived (pooled). Pool entries are constructed lazily, persist for the host window's lifetime, and recycle their `m_content`, `m_swapChain`, and `m_hwnd` per `Show` call. Initial pool size 3; grows on demand (FR-055).
- **`DxuiDialog`**: owned by `DxuiDialogManager::m_stack`; destroyed when popped (close result delivered, owner re-enabled, focus restored).
- **`std::future<int>` returned by `DxuiPopupHost::Show` / `DxuiDialogManager::Show`**: shared state set on the UI thread inside Dxui's message handling (FR-083). Background-thread `await` is permitted; what is forbidden is touching Dxui APIs from the background thread *after* the future resolves.

## Mutation operations

| Operation | Method | Returns | Side effects |
|-----------|--------|---------|--------------|
| Add child | `DxuiPanel::Add<T>(args…)` | `T &` to the constructed child | Sets `m_parent`; triggers layout recalc on next pump; broadcasts theme + DPI to the new child. |
| Create child (observer) | `DxuiPanel::CreateChild<T>(args…)` | `T *` observer pointer (owning stays with the panel) | MFC/`CreateWindow`-style factory: constructs `T`, parents it, returns a raw pointer. `<T>` is the type-safe analog of `CreateWindow`'s class arg; ctor args are the widget's defining property (e.g. label text). **Geometry is NOT passed** — bounds come from the layout pass; DPI rides the scaler. Callers keep the pointer only for controls they mutate later. |
| Remove child | `DxuiPanel::Remove(IDxuiControl *)` | `bool` (true if found and removed) | Destroys child (`unique_ptr` drop); recalc layout. |
| Clear all children | `DxuiPanel::Clear()` | `void` | Destroys all children; recalc layout. |
| Toggle visibility | `IDxuiControl::SetVisible(bool)` | `void` | Parent recalcs layout (Collapsed mode — hidden = 0 space, FR-011); skipped in paint, input, focus walks. |
| Show popup | `DxuiPopupHost::Show(ShowParams)` | `std::future<int>` | Acquires pool slot; reparents content; classifies placement (`MonitorFromRect` flip if offscreen); shows HWND; pushes focus scope. |
| Show dialog | `DxuiDialogManager::Show(std::unique_ptr<DxuiDialog>, ShowParams)` | `std::future<int>` | Disables current top HWND (or owner if stack empty); sets new dialog's owner HWND; pushes onto stack. `ShowParams::modalScrim` defaults false. |

## Validation rules (from spec FRs)

- **FR-010**: `IDxuiControl::Layout(const RECT & bounds, const DxuiDpiScaler & scaler)` assigns the control bounds; `DxuiPanel::Layout` delegates to `m_layout->Arrange(bounds, scaler, children_view())` and must produce monotonically non-decreasing child `top` for stack-layout containers — guarantees reading-order tab traversal is well-defined.
- **FR-011**: `SetVisible(false)` triggers parent layout recalc; control is skipped in paint, input, focus walks; siblings fill the freed space.
- **FR-022 / FR-082**: Public layout API takes DIPs only; `Dip` suffix on identifiers. Per-paint scaling via `DxuiDpiScaler`.
- **FR-030**: `DxuiViewport::OnBoundsChanged` fires only when bounds actually change (not on every layout pass with equal bounds).
- **FR-031**: Focus tree skips controls where any of `Visible() == false`, `Enabled() == false`, `Focusable() == false`, or `TabIndex() == IDxuiControl::kTabIndexExcluded`; `kTabIndexGeometry` uses geometry order. `RowEpsilonDip()` defaults to `IDxuiTheme::BodyLineHeightDip()` with `SetRowEpsilonDip(float)` as a test seam.
- **FR-034**: When `DxuiViewport::SetConsumesInput(true)` and viewport has focus, `OnKey` returns `true` (consumed) for everything except exactly the unmodified reserved chords Tab / Shift+Tab / Esc / Alt-alone / F10; modifier combinations such as Ctrl+Tab and Apple ][ CTRL-C/CTRL-G forward to the sink.
- **FR-072**: Dialog stack invariant — exactly one HWND in the stack is enabled at a time (the topmost); on close the previous top is re-enabled.
- **FR-080**: All public string parameters use `std::wstring` (typically `const std::wstring &` inputs; by-value only for ownership transfer).
- **FR-083**: `DXUI_ASSERT_UI_THREAD()` is invoked at every public-method entry on `DxuiPanel`, `DxuiFocusManager`, concrete `DxuiPainter`, concrete `DxuiTextRenderer`, `DxuiPopupHost`, `DxuiDialogManager`, `DxuiWindow`, and its `DxuiHwndSource` backend; debug builds assert the thread ID on entry.

## State transitions

### `DxuiPopupHost`

```
Idle ──Show()──▶ Active ──(dismiss policy fires)──▶ Closing
                  │                                    │
                  └─owner WM_ACTIVATEAPP(inactive)─────┘
Closing ──promise.set_value(result)──▶ Idle (returns to pool)
```

### `DxuiDialogManager`

```
EmptyStack ──Show(d1)──▶ Stack=[d1]               (owner disabled)
Stack=[d1] ──Show(d2)──▶ Stack=[d1,d2]            (d1 disabled, d2 owns d1)
Stack=[d1,d2] ──d2.close──▶ Stack=[d1]             (d2 destroyed, d1 re-enabled)
Stack=[d1] ──d1.close──▶ EmptyStack                (owner re-enabled)
```

### `IDxuiControl` focus

```
Unfocused ──CanFocus()=true, focus reason──▶ Focused (OnFocusChanged(true))
Focused ──blur trigger──▶ Unfocused (OnFocusChanged(false))
```

Focus *scopes* layer on top: opening a popup pushes the current focused control + scope root; closing pops and restores. Nested popups nest scopes.

## Dialog model (reshaped 2026-07 — FR-126 / FR-129)

> `DxuiDialog` / `DxuiDialogManager` and the modal-dialog **stack** sketched above are **removed**.
> Nesting / owner-chain behaviour now lives inside the `DxuiWindow` modal message loop.

```text
DxuiWindow (as a dialog)                         (a dialog IS a window)
├── ShowModalDialog(defaultId) -> int            (pumps its own loop until EndDialog(id))
├── ShowModelessDialog(defaultId)                (non-blocking; live-preview dialogs)
└── EndDialog(id)

DxuiDialogWindow : DxuiWindow                     (shared content + button-row shape)

DxuiPropertySheet : DxuiDialogWindow              (FR-129 — generic paged sheet)
├── tab strip over pages + [OK][Cancel][Apply] row
├── AddPage(DxuiPropertyPage*, tabLabel)
├── ApplyAllDirtyPages()                          (OK/Apply; false if an OnApply blocked)
└── RefreshApplyEnabled()                         (Apply enabled iff >= 1 page dirty)

DxuiPropertyPage : DxuiPanel
├── bool m_dirty                                  (MarkDirty(bool) fires on-dirty-changed)
├── OnApply() -> bool                             (false blocks OK/Apply, re-selects this page)
└── (page snapshots its own baseline for Cancel — no built-in undo)
```

*Casso `SettingsPanel` has NOT adopted `DxuiPropertySheet` (still a hand-rolled `[OK][Cancel]` sheet, no Apply). FR-131 adds a staged machine/ROM restart notice on the Machine + ROM/Hardware pages; FR-132 adds a Theme "Apply now" button.*

## Cross-references to contracts

The concrete C++ shape of each public type lives in `contracts/`. This document describes relationships; the contracts document signatures.
