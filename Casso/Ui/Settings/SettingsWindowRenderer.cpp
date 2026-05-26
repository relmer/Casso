#include "Pch.h"

#include "SettingsWindowRenderer.h"
#include "SettingsPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr UINT      s_kSwapBufferCount = 2;
static constexpr uint32_t  s_kClearArgb       = 0xFF1A2230;





////////////////////////////////////////////////////////////////////////////////
//
//  ~SettingsWindowRenderer
//
////////////////////////////////////////////////////////////////////////////////

SettingsWindowRenderer::~SettingsWindowRenderer()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::Initialize (
    HWND                   hwnd,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    int                    widthPx,
    int                    heightPx,
    UINT                   dpi)
{
    HRESULT                hr          = S_OK;
    ComPtr<IDXGIDevice>    dxgiDevice;
    ComPtr<IDXGIAdapter>   dxgiAdapter;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1  desc        = {};



    CBRAEx (hwnd, E_INVALIDARG);
    CBRAEx (device, E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

    m_hwnd     = hwnd;
    m_device   = device;
    m_context  = context;
    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_scaler.SetDpi (dpi);

    hr = m_painter.Initialize (m_device, m_context);
    CHRA (hr);

    hr = m_text.Initialize (m_device);
    CHRA (hr);

    hr = m_device->QueryInterface (IID_PPV_ARGS (&dxgiDevice));
    CHRA (hr);

    hr = dxgiDevice->GetAdapter (&dxgiAdapter);
    CHRA (hr);

    hr = dxgiAdapter->GetParent (IID_PPV_ARGS (&dxgiFactory));
    CHRA (hr);

    desc.Width            = static_cast<UINT> (m_widthPx);
    desc.Height           = static_cast<UINT> (m_heightPx);
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo           = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = s_kSwapBufferCount;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags            = 0;

    hr = dxgiFactory->CreateSwapChainForHwnd (m_device,
                                              m_hwnd,
                                              &desc,
                                              nullptr,
                                              nullptr,
                                              &m_swapChain);
    CHRA (hr);

    hr = dxgiFactory->MakeWindowAssociation (m_hwnd, DXGI_MWA_NO_ALT_ENTER);
    CHRA (hr);

    hr = CreateBackBufferTarget();
    CHRA (hr);

    m_initialized = true;

Error:
    if (FAILED (hr))
    {
        Shutdown();
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindowRenderer::Shutdown()
{
    m_text.Shutdown();
    m_painter.Shutdown();

    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets (0, nullptr, nullptr);
    }

    m_rtv.Reset();
    m_swapChain.Reset();
    m_context     = nullptr;
    m_device      = nullptr;
    m_hwnd        = nullptr;
    m_widthPx     = 0;
    m_heightPx    = 0;
    m_initialized = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferTarget
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::CreateBackBufferTarget()
{
    HRESULT                  hr = S_OK;
    ComPtr<ID3D11Texture2D>  backBuffer;



    CBRA (m_device);
    CBRA (m_swapChain);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

    hr = BindTextTarget();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BindTextTarget
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::BindTextTarget()
{
    HRESULT                  hr = S_OK;
    ComPtr<ID3D11Texture2D>  backBuffer;
    ComPtr<IDXGISurface>     surface;



    CBRA (m_swapChain);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = backBuffer.As (&surface);
    CHRA (hr);

    hr = m_text.BindBackBuffer (surface.Get(), m_scaler.Dpi(), m_scaler.Dpi());
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Resize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::Resize (int widthPx, int heightPx, UINT dpi)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_initialized || m_swapChain == nullptr, S_OK);
    BAIL_OUT_IF (widthPx <= 0 || heightPx <= 0, S_OK);

    m_widthPx  = widthPx;
    m_heightPx = heightPx;
    m_scaler.SetDpi (dpi);

    m_text.UnbindBackBuffer();
    m_context->OMSetRenderTargets (0, nullptr, nullptr);
    m_rtv.Reset();

    hr = m_swapChain->ResizeBuffers (s_kSwapBufferCount,
                                     static_cast<UINT> (m_widthPx),
                                     static_cast<UINT> (m_heightPx),
                                     DXGI_FORMAT_UNKNOWN,
                                     0);
    CHRA (hr);

    hr = CreateBackBufferTarget();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::Render (SettingsPanel & panel)
{
    HRESULT          hr            = S_OK;
    D3D11_VIEWPORT   viewport      = {};
    float            clearColor[4] = { 0.10196079f, 0.13333334f, 0.18823530f, 1.0f };



    BAIL_OUT_IF (!m_initialized || m_context == nullptr || m_swapChain == nullptr || m_rtv == nullptr, S_OK);

    viewport.Width    = static_cast<float> (m_widthPx);
    viewport.Height   = static_cast<float> (m_heightPx);
    viewport.MaxDepth = 1.0f;

    m_context->RSSetViewports (1, &viewport);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    panel.Layout (m_widthPx, m_heightPx, m_scaler);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDraw();
    CHRA (hr);

    panel.Paint (m_painter, m_text);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:
    (void) s_kClearArgb;
    return hr;
}




