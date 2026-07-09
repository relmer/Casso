#pragma once

#include "Window/DxuiWindow.h"
#include "Widgets/DxuiButton.h"

#include "PrinterPaperView.h"


struct CassoTheme;
class  PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel
//
//  A DxuiWindow (like the Disk II / Input debug panels) that shows the emulated
//  printer's output and doubles as print preview (FR-020): the rendered strip
//  is blitted into a PrinterPaperView, with a toolbar to finish (deliver to the
//  configured destination), copy, discard, or refresh the snapshot.
//
//  The strip snapshot is supplied by the shell (which quiesces the drain worker
//  to read the raster race-free) via SetStrip; toolbar actions fan back out
//  through the ActionFn callbacks, which the shell routes to the existing
//  delivery commands.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterPanel : public DxuiWindow
{
public:
    using ActionFn = std::function<void ()>;

    PrinterPanel  () = default;
    ~PrinterPanel () override = default;

    HRESULT  Create (HINSTANCE              hInstance,
                     HWND                   hwndOwner,
                     ID3D11Device         * device,
                     ID3D11DeviceContext  * context,
                     const CassoTheme     * theme);

    bool     IsOpen () const { return IsCreated (); }
    HRESULT  RenderFrame ();
    void     SetTheme (const CassoTheme * theme);

    // Render a snapshot of the current strip into the paper view (empty clears).
    void     SetStrip (const PrintRaster & raster);

    void     SetOnFinish  (ActionFn fn) { m_onFinish  = std::move (fn); }
    void     SetOnCopy    (ActionFn fn) { m_onCopy    = std::move (fn); }
    void     SetOnDiscard (ActionFn fn) { m_onDiscard = std::move (fn); }
    void     SetOnRefresh (ActionFn fn) { m_onRefresh = std::move (fn); }

    void     Layout (const RECT          & boundsDip,
                     const DxuiDpiScaler & scaler) override;

    void     Paint  (IDxuiPainter        & painter,
                     IDxuiTextRenderer   & text,
                     const IDxuiTheme    & theme) override;

    // Escape closes (hides) the preview -- the expected keyboard dismiss for a
    // preview window; other keys fall through to the base child dispatch.
    bool     OnKey  (const DxuiKeyEvent  & ev) override;

protected:
    void     OnCreate () override;

private:
    const CassoTheme  * m_theme   = nullptr;

    PrinterPaperView  * m_paper   = nullptr;
    DxuiButton        * m_finish  = nullptr;
    DxuiButton        * m_copy    = nullptr;
    DxuiButton        * m_discard = nullptr;
    DxuiButton        * m_refresh = nullptr;

    ActionFn            m_onFinish;
    ActionFn            m_onCopy;
    ActionFn            m_onDiscard;
    ActionFn            m_onRefresh;
};
