# Contract: `DxuiHostWindow`

```cpp
// Dxui/Win32/DxuiHostWindow.h
#pragma once

#include "Pch.h"
#include "DxuiPanel.h"
#include "DxuiFocusManager.h"
#include "DxuiPainter.h"
#include "DxuiTextRenderer.h"


class DxuiPopupHost;
class DxuiDialogManager;
class IDxuiTheme;


class DxuiHostWindow
{
public:
    struct CreateParams
    {
        std::wstring   title;
        HINSTANCE      hInstance      = nullptr;
        HWND           ownerHwnd      = nullptr;     // nullptr for top-level
        bool           borderless     = true;        // claim NC, custom chrome
        bool           resizable      = true;
        bool           roundedCorners = true;        // Win11; ignored on Win10
        bool           darkMode       = true;
        bool           micaBackdrop   = true;        // Win11; ignored on Win10
        float          resizeBorderDip = 6.0f;
        SIZE           initialSizeDip = {1024, 768};
    };


    DxuiHostWindow ();
    ~DxuiHostWindow();

    HRESULT Create            (const CreateParams & params);
    HRESULT Destroy           ();

    HWND          Hwnd        () const { return m_hwnd; }
    DxuiPanel &   Root        () { return *m_root; }
    void          SetTheme    (const IDxuiTheme * theme);  // non-owning

    // Pump integration
    LRESULT WndProc           (UINT msg, WPARAM wp, LPARAM lp);

    // Popup pool (FR-055)
    DxuiPopupHost * AcquirePopup ();
    void            ReleasePopup (DxuiPopupHost * popup);

    DxuiDialogManager & Dialogs () { return *m_dialogs; }

#ifdef _DEBUG
    size_t PopupHits   () const;
    size_t PopupMisses () const;
#endif

    // Test seam (debug builds only) — exposes ClassifyHit without a real WM_NCHITTEST.
#ifdef _DEBUG
    DxuiHitTestKind ClassifyHitForTest (POINT clientDip) const;
#endif

private:
    HWND                                       m_hwnd        = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device>       m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>    m_swapChain;
    std::unique_ptr<DxuiPainter>               m_painter;
    std::unique_ptr<DxuiTextRenderer>          m_textRenderer;
    std::unique_ptr<DxuiPanel>                 m_root;
    DxuiFocusManager                           m_focusManager;
    std::unique_ptr<DxuiDialogManager>         m_dialogs;
    std::vector<std::unique_ptr<DxuiPopupHost>> m_popupPool;
#ifdef _DEBUG
    size_t                                      m_popupHits   = 0;
    size_t                                      m_popupMisses = 0;
#endif
    const IDxuiTheme *                         m_theme       = nullptr;
};
```

## Messages handled (FR-050, FR-052)

| Message | Behaviour |
|---------|-----------|
| `WM_NCCALCSIZE` | When `borderless`, claim NC as client (zero NC margins) to draw custom chrome. |
| `WM_NCHITTEST` | Walk eight resize edges first; then front-to-back tree walk calling `IDxuiControl::ClassifyHit`; translate `DxuiHitTestKind` to `HT*` codes. `MaxButton` → `HTMAXBUTTON` (enables Win11 snap-layouts popover). |
| `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE` | Translate to `DxuiMouseEvent` for chrome controls (system buttons); **suppress `DefWindowProc`** to prevent Win32-drawn button overlays. |
| `WM_DPICHANGED` | Re-compute DPI scale; relayout tree; repaint. |
| `WM_DPICHANGED_BEFOREPARENT` | Forward to every active `DxuiPopupHost` in the pool so popups straddling monitor boundaries re-DPI correctly. |
| `WM_SETTINGCHANGE` / `WM_THEMECHANGED` / `WM_DWMCOLORIZATIONCOLORCHANGED` | Notify theme owner; if accepted, broadcast `OnThemeChanged` through the tree. |
| `WM_SIZE` | Resize swap chain; relayout root; fire `DxuiViewport::OnBoundsChanged` for subscribed viewports. |
| `WM_PAINT` | Run paint pass through the panel tree. |

## Contract notes

- Single `ID3D11Device` per host window. `DxuiPopupHost` borrows it (non-owning) so popup swap chains share the device.
- Popup pool initial size 3, grows on demand (FR-055). Debug builds expose `PopupHits()` / `PopupMisses()` for reuse verification. `AcquirePopup` / `ReleasePopup` are the lifecycle hooks; widget code calls `Acquire` from `DxuiDropdown::Open` etc. and `Release` from the popup's close callback.
- All public methods are UI-thread-only (FR-083). Debug builds assert the thread ID matches the thread that called `Create`.
- `WndProc` is a member function; the class registration uses a static thunk that stores the `this` pointer in `GWLP_USERDATA` (same pattern as Casso's existing `Window`).
- `Destroy` is idempotent and safe to call from the destructor.
