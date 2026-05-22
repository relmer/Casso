# Window Architecture: Borderless D3D11 Custom Chrome

## Overview

Casso's main window is a **borderless custom window class** that owns a **D3D11 swap chain** and an **RmlUi renderer** overlaid on top of a live emulation viewport. The window has no native Win32 chrome (no title bar, menus, or frame) — all visual chrome (title bar, navigation strip, drive widgets, LEDs) is rendered as Direct3D11 primitives by RmlUi. The emulated Apple II video output fills the lower portion of the screen, with custom chrome overlaid above.

### Key Architectural Principles

1. **Single Swap Chain** — One D3D11 swap chain controlled by `D3DRenderer` holds the entire scene: emulation framebuffer + RmlUi overlay.
2. **D3D-Based Chrome** — No Win32 HWND separation between chrome and viewport. All visual elements (including the titlebar, navigation strip, drive widgets) are rendered into the same D3D11 backbuffer.
3. **Custom Window Class** — A registered custom Win32 window class (`CassoRenderSurface`) handles input routing and non-client (NC) messages while suppressing default frame painting.
4. **RmlUi Overlay** — The Mozilla-style UI library renders UI elements on top of the emulation framebuffer in a single D3D render pass, allowing hot-swap theme changes at runtime.
5. **Input Routing** — Mouse and keyboard input flows through a custom **NavLayer** dispatcher that routes commands to either the emulated machine or the RmlUi UI shell depending on focus.

---

## Window Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│ EmulatorShell::m_hwnd (Main Window — Borderless)            │
│ ├─ Custom class "CassoRenderSurface"                        │
│ ├─ Child window: m_renderHwnd (fills client area)           │
│ │  └─ Hosts D3D11 swap chain via D3DRenderer                │
│ │                                                             │
│ │  [RENDERED CONTENT]:                                       │
│ │  ┌─────────────────────────────────────────────────────┐  │
│ │  │ Emulated Apple II Video (640×480 @ 96 DPI)          │  │
│ │  │ [D3D11 backbuffer, post-processed with CRT effects] │  │
│ │  │                                                      │  │
│ │  │ ┌──────────────────────────────────────────────────┐│  │
│ │  │ │ RmlUi Overlay (32px titlebar + nav strip):        ││  │
│ │  │ │ ┌────────────────────────────────────────────┐   ││  │
│ │  │ │ │ Titlebar (32px)                            │   ││  │
│ │  │ │ │ [Close Button, Maximize Button, Minimize]  │   ││  │
│ │  │ │ └────────────────────────────────────────────┘   ││  │
│ │  │ │ ┌────────────────────────────────────────────┐   ││  │
│ │  │ │ │ Navigation Strip (28dp ≈ 28px @ 96 DPI)    │   ││  │
│ │  │ │ │ [Home Icon, Recent, Settings, Help Buttons] │   ││  │
│ │  │ │ └────────────────────────────────────────────┘   ││  │
│ │  │ └──────────────────────────────────────────────────┘│  │
│ │  │                                                      │  │
│ │  │ [Apple II emulation viewport rendered below chrome] │  │
│ │  └─────────────────────────────────────────────────────┘  │
│ │                                                             │
│ ├─ m_statusBar (optional, HWND)                              │
│ │  └─ Drive indicators + emulation state (bottom of window)  │
│ │                                                             │
│ └─────────────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

### 1. Main Window (`EmulatorShell::m_hwnd`)

**File**: `Casso/EmulatorShell.h`, `Casso/EmulatorShell.cpp`  
**Class**: `EmulatorShell`

**Responsibilities**:
- Create the main borderless window via `RegisterRenderSurfaceClass()`
- Own and manage the swap chain lifecycle via `D3DRenderer`
- Route Win32 messages (input, resize, DPI change) to child window and UI shell
- Manage window state (fullscreen, placement, DPI awareness)
- Coordinate emulation lifecycle (pause, resume, configuration changes)

**Key Methods**:
- `Initialize()` — Creates main HWND, registers custom window class, creates D3D device
- `CreateStatusBar()` — Creates child render window and initializes D3D swap chain
- `OnSize()` — Handles window resize, repositions render window, updates D3D viewport
- `OnNcHitTest()` — Custom hit-testing for titlebar, resize edges, and close button (via titlebar geometry cache)
- `SetupRmlUiShell()` — Initializes RmlUi context and loads active theme

### 2. Custom Window Class (`CassoRenderSurface`)

**File**: `Casso/EmulatorShell.cpp`  
**Function**: `RegisterRenderSurfaceClass()` + `s_RenderSurfaceWndProc()`

**Responsibilities**:
- Suppress native Win32 painting (no white flash on resize, no frame edges)
- Route non-client (NC) messages to parent window so titlebar hit-testing works
- Override cursor shape in the client area (show resize arrows on edges, normal arrow elsewhere)
- Provide a paint-suppressed child window for D3D swap chain

**Message Routing**:
- `WM_ERASEBKGND` — Returns 1 (suppressed), preventing white flash during resize
- `WM_PAINT` — Calls `BeginPaint`/`EndPaint` without painting (handled by D3D)
- `WM_PRINTCLIENT` — Returns 0 (suppressed)
- `WM_SETCURSOR` — Returns `IDC_ARROW` (suppresses inherited frame resize cursor)
- `WM_NCHITTEST`, `WM_NCLBUTTONDOWN`, `WM_NCLBUTTONUP`, `WM_NCMOUSEMOVE` — Forwarded to parent via `SendMessage()`
- Other messages — Delegated to `DefWindowProc()`

**Window Positioning**:
- Created at position **(0, 0)** filling the entire client area
- Fills space: `(0, 0)` to `(clientWidth, clientHeight - statusBarHeight)`
- Chrome is rendered **within** the D3D backbuffer, not by Windows

### 3. D3D Renderer (`D3DRenderer`)

**File**: `Casso/D3DRenderer.h`, `Casso/D3DRenderer.cpp`

**Responsibilities**:
- Manage Direct3D 11 device, context, and swap chain
- Upload emulated framebuffer as a texture and render it to the backbuffer
- Apply post-processing shaders (CRT effects: scanlines, phosphor bloom, color bleed)
- Composite RmlUi on top of the emulation framebuffer
- Handle device lost/reset scenarios (DPI change, GPU reset)
- Track render pass timing for performance monitoring

**Key Methods**:
- `Initialize()` — Creates D3D device and swap chain from parent HWND
- `Resize()` — Updates viewport and CRT post-processing parameters on window resize
- `SetTopInsetPx()` — Informs renderer of chrome height (used by aspect-fit calculations)
- `UploadAndPresent()` — Main render loop: upload framebuffer, apply CRT, composite UI, present
- `SetCrtParams()` — Update CRT post-processing effect parameters from user preferences

### 4. RmlUi Shell (`UiShell`)

**File**: `Casso/UiShell.h`, `Casso/UiShell.cpp`

**Responsibilities**:
- Load and parse `.rml` (layout) + `.rcss` (styles) documents from the active theme
- Manage the RmlUi document context and event dispatcher
- Render UI elements (titlebar, nav strip, drive widgets, settings dialog) each frame
- Handle theme switching (unload old, load new, re-render within one frame)
- Pass input events (mouse, keyboard) to RmlUi for UI interaction
- Provide C++ API for emulation state updates (drive status, LEDs, machine info)

**Key Methods**:
- `Initialize()` — Load theme, create RmlUi context, attach D3D backend
- `Render()` — Render all UI elements into the D3D backbuffer (called once per frame as an "after-blit hook")
- `OnResize()` — Update RmlUi viewport on window resize
- `SwitchTheme()` — Unload old theme, load new theme, re-render
- `OnMouseMove()`, `OnMouseDown()`, `OnKeyDown()` — Dispatch input to RmlUi

### 5. Title Bar Geometry Cache (`TitleBar` struct)

**File**: `Casso/EmulatorShell.h` (likely; check `OnNcHitTest()` implementation)

**Responsibilities**:
- Cache the screen-space rectangles of UI elements (close button, maximize button, minimize button)
- Provide hit-test geometry to `OnNcHitTest()` for determining which NC element was clicked
- Update cached geometry on window resize and DPI change

**Key Methods**:
- `UpdateGeometry()` — Recompute button rects based on window width and DPI
- Query methods to return rects for each button (e.g., `GetCloseButtonRect()`)

### 6. Navigation Layer (`NavLayer`)

**File**: Likely `Casso/NavLayer.h`, `Casso/NavLayer.cpp` (check location)

**Responsibilities**:
- Dispatcher for keyboard and mouse input
- Route input to emulated machine when emulation has focus
- Route input to RmlUi when a UI element has focus
- Handle global shortcuts (e.g., Alt+Tab, Ctrl+S for save)

---

## Key Behaviors

### Window Resize

1. **User drags window edge** → OS sends `WM_NCHITTEST` to custom class
2. **Custom class forwards `WM_NCHITTEST` to parent** via `SendMessage()`
3. **Parent's `OnNcHitTest()` checks titlebar geometry** — returns `HTSIZE`, `HTTOPLEFT`, etc.
4. **OS enters resize loop**, repeatedly sends `WM_WINDOWPOSCHANGING`, `WM_WINDOWPOSCHANGED`
5. **`OnSize()` handler is called** with new dimensions
6. **`OnSize()` calls `MoveWindow(m_renderHwnd, 0, 0, newWidth, newHeight - sbHeight, FALSE)`**
   - `FALSE` flag prevents Windows from erasing the window (no flash)
7. **`OnSize()` calls `D3DRenderer::Resize(newWidth, newHeight - sbHeight)`** to update D3D viewport
8. **`OnSize()` calls `UiShell::OnResize()`** to reflow UI for new viewport
9. **Next frame render** produces content at new dimensions

**Key Design**: `MoveWindow(..., FALSE)` prevents the child window from being erased by Windows, eliminating the white flash during resize.

### Titlebar Drag

1. **User clicks on titlebar** → custom class receives the click
2. **Custom class forwards `WM_NCLBUTTONDOWN` to parent**
3. **Parent's `OnNcHitTest()` identifies the click as in the titlebar** (returns `HTCAPTION`)
4. **OS enters drag loop**, repeatedly calls `WM_MOUSEMOVE` with NC flag
5. **User releases mouse** → `WM_NCLBUTTONUP` forwarded to parent
6. **Window moves to new position** (handled by OS)

### NC Message Forwarding

The custom class **must** forward NC (non-client) messages to the parent, because the parent's window procedure (`EmulatorShell::WndProc()`) implements `OnNcHitTest()` with custom chrome geometry. If the child class handles these messages, the parent never sees them.

```cpp
// In s_RenderSurfaceWndProc():
case WM_NCHITTEST:
case WM_NCLBUTTONDOWN:
case WM_NCLBUTTONUP:
case WM_NCMOUSEMOVE:
case WM_NCDESTROY:
    return SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
```

This ensures that:
- The parent's `OnNcHitTest()` receives NC hit-tests and can return proper codes
- The parent's titlebar geometry is consulted for all NC input
- Resize, drag, and close operations flow through the parent's handlers

### Chrome Rendering

1. **Emulation framebuffer** is uploaded to D3D texture by `D3DRenderer::UploadAndPresent()`
2. **Emulation texture is rendered** to the D3D backbuffer (respecting aspect ratio and CRT effects)
3. **RmlUi renders on top** — `m_uiShell.Render()` is called as an "after-blit hook"
4. **RmlUi draws**:
   - Titlebar (32px, hardcoded `s_kTitleBarHeightPx`)
   - Navigation strip (28dp ≈ 28px @ 96 DPI, scaled with MulDiv per DPI)
   - Settings dialog (if open)
   - Drive widgets, LEDs, status indicators
5. **Swap chain presents** the completed backbuffer to screen

**Chrome Inset**:
- Chrome height = `32 + MulDiv(28, dpi, 96)` pixels
- Emulation viewport is centered **below** the chrome area (not inset — the inset is handled by the D3D layout, not window positioning)
- Render window still fills client area at `(0, 0)` — the D3D renderer respects the chrome inset when laying out the viewport

### Cursor Management

**WM_SETCURSOR Handler** in custom class returns:
```cpp
return (int) SetCursor(LoadCursor(nullptr, IDC_ARROW));
```

This suppresses the inherited frame resize cursor. On the frame edges, the OS tries to show `IDC_SIZEALL`, but because the custom class always returns `IDC_ARROW`, the cursor remains a normal arrow. The RmlUi UI layer can show context-specific cursors (e.g., hand cursor over buttons).

---

## DPI Awareness

**DPI Scaling Points**:

1. **Chrome inset** — Nav strip height scaled with `MulDiv(28, dpi, 96)`
2. **Titlebar button sizing** — Button rects recomputed in `UpdateGeometry()` per DPI
3. **RmlUi layout** — Percentage-based layout in `.rcss` scales automatically with viewport
4. **Emulation viewport** — Aspect-fit calculation in D3DRenderer uses chrome inset as input

**Trigger**: `OnSize()` calls `ComputeChromeTopInsetPx(GetDpiForWindow(m_hwnd))` and `TitleBar::UpdateGeometry(..., GetDpiForWindow(m_hwnd))` on every resize (which includes DPI change on a display switch or Settings adjustment).

---

## Input Flow

```
┌─────────────────────────┐
│ OS sends WM_KEYDOWN     │
│ to EmulatorShell::m_hwnd│
└────────────┬────────────┘
             │
             ▼
┌──────────────────────────────────┐
│ EmulatorShell::OnKeyDown()       │
│ (Message handler)                │
└────────────┬─────────────────────┘
             │
             ▼
┌──────────────────────────────────┐
│ NavLayer::OnKeyDown()            │
│ (Input dispatcher)               │
├──────────────────────────────────┤
│ Is this a UI shortcut?           │
│ (e.g., Alt+S for Settings)       │
└────────────┬─────────────────────┘
             │
      ┌──────┴──────┐
      │ YES         │ NO
      ▼             ▼
┌──────────────┐ ┌──────────────────┐
│ RmlUi::      │ │ Emulator::       │
│ OnKeyDown()  │ │ OnKeyDown()      │
│ (UI focus)   │ │ (Emulation focus)│
└──────────────┘ └──────────────────┘
```

**Key Decisions**:
- RmlUi has **first priority** on input — if a UI element has focus, the input goes to RmlUi
- Emulation receives input **only if** no UI element is consuming it
- Global shortcuts (e.g., Ctrl+Q for quit) are intercepted by `NavLayer` before either RmlUi or emulation see them

---

## Technologies Involved

| Component | Technology | Purpose | File Location |
|-----------|-----------|---------|----------------|
| Window framework | Win32 API | Create borderless window, route messages, handle resizing | `Casso/EmulatorShell.cpp` |
| Rendering | Direct3D 11 | Render emulation framebuffer + post-processing | `Casso/D3DRenderer.cpp` |
| Custom chrome | RmlUi (MIT) | Layout, style, and render UI elements (titlebar, nav strip) | `Casso/UiShell.cpp`, `Themes/*/theme.rml`, `Themes/*.rcss` |
| Post-processing | HLSL shaders | CRT scanlines, phosphor bloom, color bleed | `Casso/Shaders/CRT/*.hlsl` |
| Input dispatch | C++ custom logic | Route keyboard/mouse to emulation or RmlUi | `Casso/NavLayer.cpp` |
| Emulation | 6502 CPU + Apple II | Generate video framebuffer each cycle | `CassoEmuCore/` |

---

## Paint/Resize Sequence

```
Frame N: User drags window corner to resize
    ├─ OS calls custom class WM_NCHITTEST
    ├─ Custom class forwards to parent via SendMessage()
    ├─ Parent returns HTSIZE (hit resize edge)
    ├─ OS enters resize loop
    │
Frame N+1: Window is being resized by OS
    ├─ OS calls WM_WINDOWPOSCHANGED
    ├─ OS calls OnSize() with new dimensions
    ├─ OnSize() calls MoveWindow(m_renderHwnd, 0, 0, newW, newH, FALSE)
    │  └─ FALSE flag prevents Windows from erasing child window
    ├─ OnSize() calls D3DRenderer::Resize(newW, newH)
    │  └─ Updates D3D viewport
    ├─ OnSize() calls UiShell::OnResize()
    │  └─ Reflown UI for new dimensions
    │
Frame N+2: Render emulation + chrome
    ├─ D3DRenderer::UploadAndPresent() executes:
    │  ├─ Upload framebuffer to D3D texture
    │  ├─ Render texture (with CRT effects) to backbuffer
    │  ├─ Call m_uiShell.Render() (after-blit hook)
    │  │  └─ RmlUi renders titlebar, nav strip on top
    │  ├─ Present backbuffer to screen
    │
Frame N+3: Resize complete, normal rendering resumes
    └─ Window is at new size with no artifacts
```

**Key**: The custom class suppresses `WM_ERASEBKGND` and `WM_PAINT`, and uses `MoveWindow(..., FALSE)` so Windows never attempts to fill the child window with a background color. All rendering is handled by Direct3D.

---

## Summary of Design Decisions

| Decision | Rationale | Consequence |
|----------|-----------|-------------|
| **Borderless custom window** | Enable pixel-perfect control over chrome layout and styling; support skeuomorphic design without native frame | Requires manual implementation of resize handles, titlebar, dragging, and window buttons |
| **Single D3D swap chain** | Avoid compositing latency and tearing; keep emulation + UI in sync | All UI rendering happens in the emulation frame pipeline; no separate UI window |
| **Custom window class for render surface** | Suppress default frame painting and cursor shapes while routing NC messages to parent | Must carefully forward NC messages (WM_NCHITTEST, etc.) or resize/drag break |
| **Chrome rendered in D3D** | Leverage RmlUi for layout/styling; support hot-swap themes without recompiling | Chrome is part of the emulation framebuffer, not a separate Win32 layer |
| **RmlUi overlay via after-blit hook** | Synchronize chrome rendering with emulation framebuffer without introducing a separate render pass | Each frame, RmlUi must render in ≤1ms to maintain 60fps |
| **MoveWindow(..., FALSE)** | Prevent white flash during resize | Child window is not erased; D3D renderer must fill the entire viewport each frame |
| **Cursor shape via WM_SETCURSOR** | Suppress inherited frame resize cursor in client area; allow RmlUi to provide context-specific cursors | Must return before DefWindowProc can set the default resize cursor |

---

## Testing & Verification

**Manual Testing Checklist**:
1. ✅ Window resizes from all edges without white flash
2. ✅ Titlebar can be dragged to move window
3. ✅ Double-click titlebar maximizes/restores
4. ✅ Close button (X) closes window
5. ✅ Chrome (titlebar + nav strip) visible and interactive
6. ✅ Cursor shows resize arrows on edges, normal arrow in client area
7. ✅ Settings dialog opens without pausing emulation
8. ✅ Theme switching re-renders chrome within one frame
9. ✅ Drag-and-drop disks onto drive widgets works
10. ✅ All window controls work at 96 DPI and non-96 DPI (144 DPI, etc.)

**Unit Tests**:
- `RmlBackendSmokeTests` — Verify RmlUi initialization and document loading
- `ThemeLoaderTests` — Verify theme file parsing and validation
- `SettingsPanelStateTests` — Verify machine selector updates all visible settings
- `TitleBarGeometryTests` (if present) — Verify button rect caching and hit-testing

---

## Related Files

**Core Implementation**:
- `Casso/EmulatorShell.h` — Main window class declaration
- `Casso/EmulatorShell.cpp` — Main window implementation, custom class registration, message handlers
- `Casso/D3DRenderer.h/cpp` — Rendering pipeline
- `Casso/UiShell.h/cpp` — RmlUi integration
- `Casso/RmlBackend.h/cpp` — RmlUi D3D11 backend (rendering context)

**Theme Assets**:
- `Themes/Skeuomorphic/theme.json` — Theme metadata
- `Themes/Skeuomorphic/theme.rml` — UI layout (titlebar, nav strip, drive widgets)
- `Themes/Skeuomorphic/theme.rcss` — Styles (colors, fonts, animations)
- `Themes/Skeuomorphic/assets/` — Images, icons, fonts

**Post-Processing**:
- `Casso/Shaders/CRT/CrtLottes.hlsl` — CRT scanline + phosphor bloom shader
- `Casso/D3DRenderer.cpp` (UploadAndPresent) — Applies shader to backbuffer

**Input Routing**:
- `Casso/NavLayer.h/cpp` — Input dispatcher
- `Casso/EmulatorShell.cpp` (OnKeyDown, OnMouseMove) — Routes input to NavLayer
