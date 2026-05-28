#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Button.h"
#include "DialogDefinition.h"
#include "DialogLayout.h"


struct ChromeTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  DialogPrimitiveRenderer
//
//  Direct3D 11 renderer for the themed dialog primitive. Owns a
//  swap chain (non-DComp, CreateSwapChainForHwnd), render target
//  view, DxUiPainter for geometry, and DwriteTextRenderer for
//  text. Renders the dialog chrome (title bar, background, icon,
//  body text, hyperlinks, buttons) on each Render() call. No blur,
//  no transparency compositing.
//
////////////////////////////////////////////////////////////////////////////////

class DialogPrimitiveRenderer
{
public:
    DialogPrimitiveRenderer  () = default;
    ~DialogPrimitiveRenderer ();

    HRESULT Initialize    (HWND                   hwnd,
                           ID3D11Device         * device,
                           ID3D11DeviceContext  * context,
                           int                    widthPx,
                           int                    heightPx,
                           UINT                   dpi);
    void    Shutdown      ();
    HRESULT Resize        (int widthPx, int heightPx, UINT dpi);
    HRESULT Render        (const DialogDefinition   & def,
                           const DialogLayoutResult & layout,
                           const ChromeTheme        & theme,
                           int                        titleHeightPx,
                           std::vector<Button>      & buttons);
    HRESULT MeasureText   (const wchar_t * text,
                           float           fontSizePx,
                           float         & outWidthPx);
    bool    IsInitialized () const { return m_initialized; }
    UINT    Dpi           () const { return m_scaler.Dpi(); }

private:
    HRESULT CreateBackBufferTarget ();
    HRESULT BindTextTarget         ();
    void    PaintBackground        (const ChromeTheme        & theme,
                                    int                        titleHeightPx);
    void    PaintTitle             (const DialogDefinition   & def,
                                    const ChromeTheme        & theme,
                                    int                        titleHeightPx);
    void    PaintIcon              (const DialogDefinition   & def,
                                    const DialogLayoutResult & layout,
                                    int                        titleHeightPx);
    void    PaintBody              (const DialogDefinition   & def,
                                    const DialogLayoutResult & layout,
                                    const ChromeTheme        & theme,
                                    int                        titleHeightPx);
    void    PaintCustomBody        (const DialogDefinition   & def,
                                    const DialogLayoutResult & layout,
                                    const ChromeTheme        & theme,
                                    int                        titleHeightPx);
    void    PaintButtons           (std::vector<Button>      & buttons,
                                    const ChromeTheme        & theme);

    HWND                              m_hwnd        = nullptr;
    ID3D11Device                    * m_device      = nullptr;   // non-owning
    ID3D11DeviceContext             * m_context     = nullptr;   // non-owning
    ComPtr<IDXGISwapChain1>           m_swapChain;
    ComPtr<ID3D11RenderTargetView>    m_rtv;
    DxUiPainter                       m_painter;
    DwriteTextRenderer                m_text;
    DpiScaler                         m_scaler;
    int                               m_widthPx     = 0;
    int                               m_heightPx    = 0;
    bool                              m_initialized = false;
};
