#pragma once

#include "Pch.h"

#include "Widgets/DxuiButton.h"
#include "DialogDefinition.h"
#include "DialogLayout.h"


struct ChromeTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  DialogPrimitiveRenderer
//
//  Direct3D 11 renderer for the themed dialog primitive. Owns a
//  swap chain (non-DComp, CreateSwapChainForHwnd), render target
//  view, DxuiPainter for geometry, and DxuiTextRenderer for
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
                           std::vector<DxuiButton>      & buttons,
                           size_t                     focusedHyperlinkRunIdx,
                           size_t                     hoveredHyperlinkRunIdx,
                           bool                       closeHovered,
                           bool                       closePressed);
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
                                    int                        titleHeightPx,
                                    bool                       closeHovered,
                                    bool                       closePressed);
    void    PaintIcon              (const DialogDefinition   & def,
                                    const DialogLayoutResult & layout,
                                    int                        titleHeightPx);
    void    PaintBody              (const DialogDefinition   & def,
                                    const DialogLayoutResult & layout,
                                    const ChromeTheme        & theme,
                                    int                        titleHeightPx,
                                    size_t                     focusedHyperlinkRunIdx,
                                    size_t                     hoveredHyperlinkRunIdx);
    void    PaintCustomBody        (const DialogDefinition   & def,
                                    const DialogLayoutResult & layout,
                                    const ChromeTheme        & theme,
                                    int                        titleHeightPx);
    void    PaintButtons           (std::vector<DxuiButton>      & buttons,
                                    const ChromeTheme        & theme);
    void    EnsureAppIconLoaded    (int                        iconResourceId,
                                    int                        sizePx);

    HWND                              m_hwnd        = nullptr;
    ID3D11Device                    * m_device      = nullptr;   // non-owning
    ID3D11DeviceContext             * m_context     = nullptr;   // non-owning
    ComPtr<IDXGISwapChain1>           m_swapChain;
    ComPtr<ID3D11RenderTargetView>    m_rtv;
    DxuiPainter                       m_painter;
    DxuiTextRenderer                m_text;
    DxuiDpiScaler                         m_scaler;
    std::vector<uint32_t>             m_appIconPixels;
    int                               m_appIconW       = 0;
    int                               m_appIconH       = 0;
    int                               m_appIconResId   = 0;
    int                               m_widthPx     = 0;
    int                               m_heightPx    = 0;
    bool                              m_initialized = false;
};
