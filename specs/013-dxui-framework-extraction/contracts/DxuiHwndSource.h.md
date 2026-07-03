# Contract: `DxuiHwndSource` (+ `DxuiWindow`)

> **Renamed 2026-07 (commit `5c9ac3f`)**: the type originally sketched here as
> `DxuiHostWindow` is now **`DxuiHwndSource`**, mirroring WPF's `HwndSource` — the
> HWND / swap-chain / message-pump backend. It is no longer the consumer-facing
> top-level type. Consumers derive from the new **`DxuiWindow : DxuiPanel`**
> (WPF's `Window : ContentControl`), which owns a `DxuiHwndSource` privately and
> hides the HWND, `WPARAM`/`LPARAM`, and `IDxuiHostClient` entirely. See the
> "Window element" section below and `plan.md` §Architecture.

## `DxuiWindow` — top-level window element (consumer-facing)

```cpp
// Dxui/Window/DxuiWindow.h
#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Window/DxuiHwndSource.h"
#include "Window/IDxuiHostClient.h"


//
//  Mirrors WPF `Window : ContentControl`: a DxuiWindow IS a DxuiPanel (its own
//  content root — children are added to it directly via Create<T> / Add<T>) AND
//  it owns the single OS window (HWND + swap chain + caption + paint pump)
//  through an internal DxuiHwndSource backend that stays hidden from the
//  consumer. A subclass overrides OnCreate() to populate children and never
//  touches an HWND, a WPARAM, or IDxuiHostClient.
//
class DxuiWindow : public DxuiPanel, private IDxuiHostClient
{
public:
    struct CreateParams
    {
        std::wstring        title;
        HINSTANCE           hInstance                = nullptr;
        HWND                ownerHwnd                = nullptr;
        SIZE                initialSizeDip           = { 1024, 768 };
        SIZE                minSizeDip               = { 0, 0 };
        bool                resizable                = true;
        bool                insetContentBelowCaption = false;
        DxuiCaptionStyle    captionStyle             = DxuiCaptionStyle::Standard;
        LPCWSTR             classNameOverride        = nullptr;
        HICON               appIconBig               = nullptr;
        HICON               appIconSmall             = nullptr;
    };

    DxuiWindow  () = default;
    ~DxuiWindow () override;

    HRESULT  Create      (const CreateParams & params);   // conjures HWND + backend, installs
                                                           //   this panel as content root, calls OnCreate()
    void     Show        ();
    void     Hide        ();
    void     Close       ();
    void     Invalidate  ();                               // request a repaint (backend WM_PAINT pump)

    bool     IsCreated   () const;
    HWND     Hwnd        () const;
    void     SetTheme    (const IDxuiTheme * theme);
    void     SetTitle    (const std::wstring & title);
    int      CaptionHeightPx () const;
    UINT     Dpi         () const;

    DxuiHwndSource    *  PopupHost    () const;            // popup backend for owned menus / tooltips
    IDxuiTextRenderer *  TextRenderer () const;

protected:
    virtual void  OnCreate        () {}                    // populate children here (fires once)
    virtual void  OnWindowClose   () { Hide(); }           // caption close box / WM_CLOSE
    virtual void  OnWindowDestroy () {}                    // WM_DESTROY
    void          DestroyBackend  ();

private:
    // Translates the Win32 messages the backend does not own end-to-end
    // (mouse / keyboard / cursor / close / min-max) into DxuiMouseEvent /
    // DxuiKeyEvent dispatch to its own tree via the private IDxuiHostClient
    // overrides. Manages capture / focus / cursor / min-max / close.
    DxuiMessageResult  OnLButtonDown (WPARAM, LPARAM) override;
    DxuiMessageResult  OnLButtonUp   (WPARAM, LPARAM) override;
    DxuiMessageResult  OnRButtonDown (WPARAM, LPARAM) override;
    DxuiMessageResult  OnMouseMove   (WPARAM, LPARAM) override;
    DxuiMessageResult  OnMouseWheel  (WPARAM, LPARAM, bool horizontal) override;
    DxuiMessageResult  OnKeyDown     (WPARAM vk, LPARAM) override;
    DxuiMessageResult  OnChar        (WPARAM ch, LPARAM) override;
    DxuiMessageResult  OnSetCursor   (WORD hitTest) override;
    DxuiMessageResult  OnGetMinMax   (MINMAXINFO *) override;
    DxuiMessageResult  OnClose       () override;
    void               OnDestroy     () override;

    std::unique_ptr<DxuiHwndSource>  m_source;
    SIZE                             m_minSizeDip = { 0, 0 };
};
```

**Planned (PENDING):** a modal `DxuiDialog : DxuiWindow` shown via a new
`ShowDialog()` (modal loop) is intended to replace the legacy
`DxuiDialog`/`DxuiDialogManager` pair, which will be deleted once all dialog
consumers migrate. Not yet implemented.

## `DxuiHwndSource` — HWND / swap-chain / pump backend (framework-internal)

```cpp
// Dxui/Window/DxuiHwndSource.h
#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"


class DxuiPopupHost;
class IDxuiHostClient;
class DxuiCaptionBar;


enum class DxuiHwndSourceBackdrop { None, Mica };
enum class DxuiCaptionStyle       { None, Standard, CloseOnly };


class DxuiHwndSource
{
public:
    struct CreateParams
    {
        std::wstring             title;
        HINSTANCE                hInstance        = nullptr;
        HWND                     ownerHwnd        = nullptr;
        bool                     borderless       = true;
        bool                     resizable        = true;
        bool                     roundedCorners   = true;
        bool                     darkMode         = true;
        DxuiHwndSourceBackdrop   backdrop         = DxuiHwndSourceBackdrop::Mica;
        float                    resizeBorderDip  = 6.0f;
        SIZE                     initialSizeDip   = { 1024, 768 };
        LPCWSTR                  classNameOverride        = nullptr;   // e.g. "CassoWindow" for Spy++ / tooling
        bool                     useInitialWindowRectPx   = false;     // restore saved placement verbatim
        RECT                     initialWindowRectPx      = {};
        HICON                    appIconBig               = nullptr;
        HICON                    appIconSmall             = nullptr;
        bool                     createSwapChain          = true;      // false: consumer owns its own pipeline
        DxuiCaptionStyle         captionStyle             = DxuiCaptionStyle::None;   // host-owned caption
        bool                     insetRootBelowCaption    = false;
    };

    DxuiHwndSource  ();
    ~DxuiHwndSource ();

    HRESULT  Create   (const CreateParams & params);          // full-ownership: register class, CreateWindowEx
    void     Destroy  ();

    // Adopt mode — wrap a caller-owned HWND. No CreateWindow/DestroyWindow,
    // no swap chain, no WNDPROC install. Caller forwards messages through
    // HandleMessage(); host classifies NC hits (+ optional SetHitTestDelegate)
    // and propagates DPI / theme to its internal panel tree. Pass nullptr for
    // existingHwnd to drive the classifier headlessly in tests.
    static HRESULT  CreateInAdoptMode (HWND existingHwnd,
                                       const CreateParams & params,
                                       std::unique_ptr<DxuiHwndSource> & outHost);
    void  SetHitTestDelegate (std::function<LRESULT (POINT ptScreen)> delegate);
    bool  HandleMessage      (UINT msg, WPARAM wp, LPARAM lp, LRESULT & outResult);

    HWND          Hwnd    () const;
    DxuiPanel  &  Root    ();
    const DxuiDpiScaler &  Scaler () const;
    void          SetTheme (const IDxuiTheme * theme);

    // Host-owned caption (SetWindowText model — active when captionStyle != None).
    void          SetTitle        (const std::wstring & title);
    void          SetCaptionIcon  (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx);
    int           CaptionHeightPx () const;

    // Content root install. SetContentPanel takes ownership; SetContentRootRef
    // installs a NON-owning root — used by DxuiWindow, which IS its own content
    // root (derives from DxuiPanel) while owning this host as its backend.
    void          SetContentPanel   (std::unique_ptr<DxuiPanel> panel);
    void          SetContentRootRef (DxuiPanel * root);

    // Optional client that receives the Win32 messages the host does not own
    // end-to-end. DxuiWindow installs ITSELF (via private IDxuiHostClient).
    void          SetClient       (IDxuiHostClient * client);

    bool          SetTimer  (UINT_PTR timerId, UINT intervalMs);
    bool          KillTimer (UINT_PTR timerId);

    // Popup pool (FR-055) + shared device accessors elided here for brevity —
    // see Dxui/Window/DxuiHwndSource.h.
};
```

## Messages handled (FR-050, FR-052)

| Message | Behaviour |
|---------|-----------|
| `WM_NCCALCSIZE` | When `borderless`, claim NC as client (zero NC margins) to draw custom chrome. |
| `WM_NCHITTEST` | Eight resize edges first; in adopt mode consult the optional hit-test delegate; then front-to-back tree walk calling `IDxuiControl::ClassifyHit`; translate `DxuiHitTestKind` to `HT*`. `MaxButton` → `HTMAXBUTTON` (Win11 snap-layouts popover). |
| `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE` | Translate to input for chrome controls (system buttons); **suppress `DefWindowProc`** to prevent Win32-drawn button overlays. |
| `WM_DPICHANGED` | Re-compute DPI scale; relayout tree; repaint. |
| `WM_DPICHANGED_BEFOREPARENT` | Forward to every active `DxuiPopupHost` in the pool. |
| `WM_SETTINGCHANGE` / `WM_THEMECHANGED` / `WM_DWMCOLORIZATIONCOLORCHANGED` | Notify theme owner; if accepted, broadcast `OnThemeChanged` through the tree. |
| `WM_SIZE` | Resize swap chain; relayout root; fire `DxuiViewport::OnBoundsChanged` for subscribed viewports. |
| `WM_PAINT` | Run paint pass through the (owned or ref'd) content root. |

## Contract notes

- Single `ID3D11Device` per source. `DxuiPopupHost` borrows it (non-owning) so popup swap chains share the device.
- `DxuiWindow` installs itself as the source's **non-owning content root** (`SetContentRootRef`) and as its `IDxuiHostClient`, so the backend paints / lays out / routes the DxuiWindow's own child tree with no extra root object.
- All public methods are UI-thread-only (FR-083). Debug builds assert the thread ID matches the thread that called `Create`.
- `Destroy` / `DestroyBackend` are idempotent and safe to call from the destructor.
