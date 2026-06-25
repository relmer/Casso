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
    SettingsWindowRenderer() = default;
    ~SettingsWindowRenderer();

    HRESULT Initialize           (HWND                   hwnd,
                                  ID3D11Device         * device,
                                  ID3D11DeviceContext  * context,
                                  int                    widthPx,
                                  int                    heightPx,
                                  UINT                   dpi);
    void    Shutdown();
    HRESULT Resize               (int widthPx, int heightPx, UINT dpi);
    HRESULT Render               (class SettingsPanel & panel);
    void    SetChrome            (TitleBar * titleBar, const ChromeTheme * theme);
    void    SetTransparencyState (bool active, RECT emuRectClient, RECT focusRectClient);

    bool    IsInitialized() const { return m_initialized; }

private:
    struct SettingsBlurParams
    {
        float  radiusPx = 0.0f;
        float  outputW  = 0.0f;
        float  outputH  = 0.0f;
        float  _pad     = 0.0f;
    };


    struct SettingsComposeParams
    {
        float  emuRectClient[4]   = {};
        float  focusRectClient[4] = {};
        float  outputW            = 0.0f;
        float  outputH            = 0.0f;
        float  dimFactor          = 0.0f;
        float  featherPx          = 0.0f;
    };


    HRESULT CreateBackBufferTarget();
    HRESULT BindTextTarget();
    HRESULT CompilePixelShader           (int                  resourceId,
                                           const char         * sourceName,
                                           ID3D11PixelShader ** out);
    HRESULT CreatePostProcessResources();
    HRESULT EnsureTransparencyResources();
    void    ReleaseTransparencyResources();
    HRESULT UploadBlurParams             (const SettingsBlurParams & params);
    HRESULT UploadComposeParams          (const SettingsComposeParams & params);
    void    DrawFullscreen               (ID3D11RenderTargetView   * rt,
                                           ID3D11ShaderResourceView * srv0,
                                           ID3D11ShaderResourceView * srv1,
                                           ID3D11PixelShader        * ps,
                                           ID3D11Buffer             * constantBuffer);
    HRESULT RenderDirect                 (class SettingsPanel & panel,
                                           const D3D11_VIEWPORT     & viewport,
                                           const ChromeTheme        & theme);
    HRESULT RenderPanelToTexture         (class SettingsPanel & panel,
                                           const ChromeTheme        & theme);
    HRESULT RenderTransparency           (class SettingsPanel & panel,
                                           const D3D11_VIEWPORT     & viewport,
                                           const ChromeTheme        & theme);
    HRESULT RenderModalOverlay           (class SettingsPanel & panel,
                                           const D3D11_VIEWPORT     & viewport);

    HWND                              m_hwnd     = nullptr;
    ID3D11Device                    * m_device   = nullptr;
    ID3D11DeviceContext             * m_context  = nullptr;
    ComPtr<IDXGISwapChain1>           m_swapChain;
    ComPtr<IDCompositionDevice>        m_dcompDevice;
    ComPtr<IDCompositionTarget>        m_dcompTarget;
    ComPtr<IDCompositionVisual>        m_dcompVisual;
    ComPtr<ID3D11RenderTargetView>    m_rtv;
    DxUiPainter                       m_painter;
    DwriteTextRenderer                m_text;
    DpiScaler                         m_scaler;
    TitleBar                        * m_titleBar = nullptr;
    const ChromeTheme                * m_theme    = nullptr;
    int                               m_widthPx  = 0;
    int                               m_heightPx = 0;
    bool                              m_initialized = false;

    ComPtr<ID3D11VertexShader>        m_vs;
    ComPtr<ID3D11InputLayout>         m_inputLayout;
    ComPtr<ID3D11Buffer>              m_vertexBuffer;
    ComPtr<ID3D11Buffer>              m_indexBuffer;
    ComPtr<ID3D11Buffer>              m_blurConstantBuffer;
    ComPtr<ID3D11Buffer>              m_composeConstantBuffer;
    ComPtr<ID3D11SamplerState>        m_sampler;
    ComPtr<ID3D11BlendState>          m_blendOpaque;
    ComPtr<ID3D11PixelShader>         m_psGaussianH;
    ComPtr<ID3D11PixelShader>         m_psGaussianV;
    ComPtr<ID3D11PixelShader>         m_psCompose;

    ComPtr<ID3D11Texture2D>           m_fullTex;
    ComPtr<ID3D11RenderTargetView>    m_fullRtv;
    ComPtr<ID3D11ShaderResourceView>  m_fullSrv;
    ComPtr<ID3D11Texture2D>           m_blurHTex;
    ComPtr<ID3D11RenderTargetView>    m_blurHRtv;
    ComPtr<ID3D11ShaderResourceView>  m_blurHSrv;
    ComPtr<ID3D11Texture2D>           m_blurTex;
    ComPtr<ID3D11RenderTargetView>    m_blurRtv;
    ComPtr<ID3D11ShaderResourceView>  m_blurSrv;
    int                               m_transparencyWidthPx  = 0;
    int                               m_transparencyHeightPx = 0;

    bool                              m_transparencyActive   = false;
    RECT                              m_emuRectClient        = {};
    RECT                              m_focusRectClient      = {};
};
