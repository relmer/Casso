#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"


class TitleBar;
struct ChromeTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsWindowRenderer
//
////////////////////////////////////////////////////////////////////////////////

class SettingsWindowRenderer
{
public:
    SettingsWindowRenderer  () = default;
    ~SettingsWindowRenderer();

    HRESULT Initialize (HWND                   hwnd,
                        ID3D11Device         * device,
                        ID3D11DeviceContext  * context,
                        int                    widthPx,
                        int                    heightPx,
                        UINT                   dpi);
    void    Shutdown   ();
    HRESULT Resize     (int widthPx, int heightPx, UINT dpi);
    HRESULT Render     (class SettingsPanel & panel);
    void    SetChrome  (TitleBar * titleBar, const ChromeTheme * theme);

    bool    IsInitialized() const { return m_initialized; }

private:
    HRESULT CreateBackBufferTarget();
    HRESULT BindTextTarget         ();

    HWND                              m_hwnd     = nullptr;
    ID3D11Device                    * m_device   = nullptr;
    ID3D11DeviceContext             * m_context  = nullptr;
    ComPtr<IDXGISwapChain1>           m_swapChain;
    ComPtr<ID3D11RenderTargetView>    m_rtv;
    DxUiPainter                       m_painter;
    DwriteTextRenderer                m_text;
    DpiScaler                         m_scaler;
    TitleBar                        * m_titleBar = nullptr;
    const ChromeTheme                * m_theme    = nullptr;
    int                               m_widthPx  = 0;
    int                               m_heightPx = 0;
    bool                              m_initialized = false;
};




