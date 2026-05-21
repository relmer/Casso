# Contract: `D3DUiContext`

**Location**: `Casso/Ui/D3DUiContext.{h,cpp}`.
**Purpose**: shared D3D rendering primitives used by every chrome widget and
the Settings panel. One per `D3DRenderer`. Owns DirectWrite text layout,
Direct2D-on-D3D11 vector drawing, a sprite/atlas batcher, and a hit-test stack.

## Lifecycle

```cpp
HRESULT Initialize     (ID3D11Device * device, IDXGISwapChain * swapChain);
HRESULT OnDeviceLost   ();                          // releases all GPU-side objects
HRESULT OnDeviceRestored (ID3D11Device * device, IDXGISwapChain * swapChain);
void    Shutdown       ();
```

Tied into `D3DRenderer::Initialize` / `Resize` / device-lost recovery so
chrome resources are recreated on the same lifecycle as the existing
framebuffer surface (spec edge case "device lost").

## Per-frame protocol

```cpp
void    BeginFrame    ();                       // resets hit-test stack, sprite batcher
void    EndFrame      ();                       // flushes batches, draws to back buffer
```

Called once per frame between the existing framebuffer present and the swap
chain `Present`, so chrome composites over the emulated framebuffer with no
extra HWND.

## Drawing primitives

```cpp
void    DrawSpriteNineSlice (TextureRef tex, RECT dstPx, Insets borderPx);
void    DrawSpriteTiled     (TextureRef tex, RECT dstPx);
void    DrawSolidRect       (RECT dstPx, ColorRgba color);
void    DrawText            (wstring_view text, RECT dstPx, FontHandle font,
                             ColorRgba color, TextAlign align);
void    DrawFocusRing       (RECT dstPx, ColorRgba color);   // FR-044
void    DrawGlow            (POINT centerPx, GlowParams params);  // for LEDs
```

Color and texture inputs come from the active `ThemeData` (caller resolves the
palette/texture tokens themselves; `D3DUiContext` is theme-agnostic to keep it
single-purpose).

## Hit-test stack

A simple push/pop stack used to map screen coordinates to widget IDs and to
serve `WM_NCHITTEST` for the borderless window (FR-028).

```cpp
void    PushHitRect    (RECT rectPx, WidgetId id, HitTestKind kind);
WidgetId HitTest       (POINT pointPx) const;
LRESULT NcHitTest      (POINT pointScreen) const;        // returns HTCAPTION/HTCLIENT/HTLEFT/...
```

`HitTestKind` enum covers `Client`, `Caption`, `MinButton`, `MaxButton`,
`CloseButton`, `ResizeLeft`, `ResizeRight`, `ResizeTop`, `ResizeBottom`, plus
four corners. `WidgetId` is a plain `uint64_t` opaque handle owned by callers.

## Focus management (FR-044)

```cpp
void    RegisterFocusable (WidgetId id, RECT rectPx, FocusOrder order);
void    AdvanceFocus      (FocusDirection dir);    // Tab / Shift-Tab
WidgetId CurrentFocus     () const;
void    ActivateFocused   ();                       // Enter / Space
```

The Settings panel and nav layer push focusable entries each frame they're
visible; focus survives a frame as long as the same WidgetId is re-registered.

## Text layout cache

DirectWrite `IDWriteTextLayout` objects are cached by `(text, font, maxWidth)`
key with an LRU eviction policy (capacity = 256 entries) to meet the SC-005
≤ 1 ms budget. Cache lives entirely inside `D3DUiContext`; callers see only
`DrawText`.

## Theme decoupling

`D3DUiContext` does **not** know about `ThemeManager`. It exposes primitives;
chrome widgets read tokens from the active `ThemeData` and pass concrete
colors/textures/rects in. This keeps the rendering layer testable without
themes and avoids a circular dep.

## Test coverage

Pure-function elements that are unit-tested without a D3D device:
- 9-slice geometry computation (input rect + insets → 9 sub-rects).
- Hit-test stack `HitTest` and `NcHitTest` logic (table of rects → WidgetId / HT code).
- LRU eviction policy of text layout cache (mocked entries, count-only).
- Focus traversal order (Tab / Shift-Tab over a registered list).

GPU-side calls (`DrawSprite*`, `DrawText`, `DrawGlow`) are smoke-tested in the
manual validation flow in `quickstart.md`.
