#include "Pch.h"

#include "SettingsWindowRenderer.h"
#include "SettingsPanel.h"
#include "../Chrome/TitleBar.h"
#include "../../Shaders/ShaderResourceIds.h"

#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3dcompiler.lib")




namespace
{
    constexpr UINT          s_kSwapBufferCount       = 2;
    constexpr UINT          s_kMaxBoundPsSrvSlots    = 2;
    constexpr UINT          s_kFullscreenVertexCount = 4;
    constexpr UINT          s_kFullscreenIndexCount  = 6;
    constexpr UINT          s_kTexCoordOffsetBytes   = sizeof (float) * 2;
    constexpr float         s_kGaussianRadiusPx      = 8.0f;
    constexpr float         s_kDimFactor             = 0.25f;
    // Feather the focused-control sharp pop-out by this many pixels
    // beyond the control's row rect so the boundary against the
    // blurred backdrop reads as a soft halo, not a harsh edge.
    constexpr float         s_kFocusFeatherPx        = 24.0f;
    constexpr uint32_t      s_kClearArgb             = 0xFF1A2230;

    constexpr const char *  s_kpszVertexShaderSrc =
        "struct VSInput  { float2 pos : POSITION; float2 uv : TEXCOORD; };\n"
        "struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "VSOutput main (VSInput i)\n"
        "{\n"
        "    VSOutput o;\n"
        "    o.pos = float4 (i.pos, 0.0f, 1.0f);\n"
        "    o.uv  = i.uv;\n"
        "    return o;\n"
        "}\n";


    struct ShaderSource
    {
        const void * pData  = nullptr;
        size_t       cbData = 0;
    };


    struct SettingsVertex
    {
        float x;
        float y;
        float u;
        float v;
    };





////////////////////////////////////////////////////////////////////////////////
//
//  LoadShaderSource
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LoadShaderSource (int resourceId, ShaderSource * outSource)
{
    HRESULT    hr        = S_OK;
    HINSTANCE  hInstance = nullptr;
    HRSRC      hRes      = nullptr;
    HGLOBAL    hMem      = nullptr;
    DWORD      cbData    = 0;
    void     * pData     = nullptr;



    CBRAEx (outSource, E_INVALIDARG);

    outSource->pData  = nullptr;
    outSource->cbData = 0;

    hInstance = GetModuleHandleW (nullptr);
    CBRA (hInstance);

    hRes = FindResourceW (hInstance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
    CWRA (hRes);

    cbData = SizeofResource (hInstance, hRes);
    CBRA (cbData > 0);

    hMem = LoadResource (hInstance, hRes);
    CWRA (hMem);

    pData = LockResource (hMem);
    CWRA (pData);

    outSource->pData  = pData;
    outSource->cbData = static_cast<size_t> (cbData);

Error:
    return hr;
}
}




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

    hr = CreatePostProcessResources();
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
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags            = 0;

    hr = DCompositionCreateDevice (dxgiDevice.Get(), IID_PPV_ARGS (&m_dcompDevice));
    CHRA (hr);

    hr = m_dcompDevice->CreateTargetForHwnd (m_hwnd, TRUE, &m_dcompTarget);
    CHRA (hr);

    hr = m_dcompDevice->CreateVisual (&m_dcompVisual);
    CHRA (hr);

    hr = dxgiFactory->CreateSwapChainForComposition (m_device,
                                                     &desc,
                                                     nullptr,
                                                     &m_swapChain);
    CHRA (hr);

    hr = m_dcompVisual->SetContent (m_swapChain.Get());
    CHRA (hr);

    hr = m_dcompTarget->SetRoot (m_dcompVisual.Get());
    CHRA (hr);

    hr = m_dcompDevice->Commit();
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
    ReleaseTransparencyResources();

    m_psCompose.Reset();
    m_psGaussianV.Reset();
    m_psGaussianH.Reset();
    m_blendOpaque.Reset();
    m_sampler.Reset();
    m_composeConstantBuffer.Reset();
    m_blurConstantBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_vs.Reset();

    m_text.Shutdown();
    m_painter.Shutdown();

    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets (0, nullptr, nullptr);
    }

    m_rtv.Reset();
    m_dcompVisual.Reset();
    m_dcompTarget.Reset();
    m_dcompDevice.Reset();
    m_swapChain.Reset();
    m_context     = nullptr;
    m_device      = nullptr;
    m_hwnd        = nullptr;
    m_titleBar    = nullptr;
    m_theme       = nullptr;
    m_widthPx     = 0;
    m_heightPx    = 0;
    m_initialized = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetChrome
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindowRenderer::SetChrome (TitleBar * titleBar, const CassoTheme * theme)
{
    m_titleBar = titleBar;
    m_theme    = theme;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetTransparencyState
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindowRenderer::SetTransparencyState (bool active, RECT emuRectClient, RECT focusRectClient)
{
    m_transparencyActive = active;
    m_emuRectClient      = active ? emuRectClient   : RECT {};
    m_focusRectClient    = active ? focusRectClient : RECT {};
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
//  CompilePixelShader
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::CompilePixelShader (
    int                  resourceId,
    const char         * sourceName,
    ID3D11PixelShader ** out)
{
    HRESULT             hr     = S_OK;
    ShaderSource        source = {};
    ComPtr<ID3DBlob>    blob;
    ComPtr<ID3DBlob>    errors;



    CBRAEx (sourceName, E_INVALIDARG);
    CBRAEx (out,        E_INVALIDARG);

    *out = nullptr;

    hr = LoadShaderSource (resourceId, &source);
    CHRA (hr);

    hr = D3DCompile (source.pData,
                     source.cbData,
                     sourceName,
                     nullptr,
                     nullptr,
                     "main",
                     "ps_4_0",
                     0,
                     0,
                     &blob,
                     &errors);
    CHRA (hr);

    hr = m_device->CreatePixelShader (blob->GetBufferPointer(),
                                      blob->GetBufferSize(),
                                      nullptr,
                                      out);
    CHRA (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CreatePostProcessResources
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::CreatePostProcessResources()
{
    HRESULT                hr       = S_OK;
    ComPtr<ID3DBlob>       vsBlob;
    ComPtr<ID3DBlob>       errors;
    D3D11_BUFFER_DESC      bd       = {};
    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SAMPLER_DESC     sd       = {};
    D3D11_BLEND_DESC       bld      = {};

    SettingsVertex vertices[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f },
    };

    UINT16 indices[] = { 0, 1, 2, 2, 1, 3 };

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                     D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, s_kTexCoordOffsetBytes, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };



    hr = D3DCompile (s_kpszVertexShaderSrc,
                     strlen (s_kpszVertexShaderSrc),
                     "SettingsWindowRenderer.hlsl",
                     nullptr,
                     nullptr,
                     "main",
                     "vs_4_0",
                     0,
                     0,
                     &vsBlob,
                     &errors);
    CHRA (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(),
                                       nullptr,
                                       &m_vs);
    CHRA (hr);

    hr = m_device->CreateInputLayout (layout,
                                      ARRAYSIZE (layout),
                                      vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(),
                                      &m_inputLayout);
    CHRA (hr);

    hr = CompilePixelShader (IDR_HLSL_SETTINGS_GAUSSIAN_H, "gaussian_h.hlsl",       &m_psGaussianH);  CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_SETTINGS_GAUSSIAN_V, "gaussian_v.hlsl",       &m_psGaussianV);  CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_SETTINGS_COMPOSE,    "settings_compose.hlsl", &m_psCompose);    CHRA (hr);

    bd.ByteWidth = sizeof (vertices);
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    initData.pSysMem = vertices;
    hr = m_device->CreateBuffer (&bd, &initData, &m_vertexBuffer);
    CHRA (hr);

    bd = {};
    bd.ByteWidth = sizeof (indices);
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData = {};
    initData.pSysMem = indices;
    hr = m_device->CreateBuffer (&bd, &initData, &m_indexBuffer);
    CHRA (hr);

    bd = {};
    bd.ByteWidth      = sizeof (SettingsBlurParams);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer (&bd, nullptr, &m_blurConstantBuffer);
    CHRA (hr);

    bd.ByteWidth = sizeof (SettingsComposeParams);
    hr = m_device->CreateBuffer (&bd, nullptr, &m_composeConstantBuffer);
    CHRA (hr);

    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = m_device->CreateSamplerState (&sd, &m_sampler);
    CHRA (hr);

    bld.RenderTarget[0].BlendEnable           = FALSE;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_device->CreateBlendState (&bld, &m_blendOpaque);
    CHRA (hr);

Error:
    (void) s_kFullscreenVertexCount;
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseTransparencyResources
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindowRenderer::ReleaseTransparencyResources()
{
    ID3D11ShaderResourceView * nullSrvs[s_kMaxBoundPsSrvSlots] = {};



    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets   (0, nullptr, nullptr);
        m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);
    }

    m_blurSrv.Reset();
    m_blurRtv.Reset();
    m_blurTex.Reset();
    m_blurHSrv.Reset();
    m_blurHRtv.Reset();
    m_blurHTex.Reset();
    m_fullSrv.Reset();
    m_fullRtv.Reset();
    m_fullTex.Reset();
    m_transparencyWidthPx  = 0;
    m_transparencyHeightPx = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EnsureTransparencyResources
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::EnsureTransparencyResources()
{
    HRESULT            hr = S_OK;
    D3D11_TEXTURE2D_DESC td = {};



    BAIL_OUT_IF (m_widthPx <= 0 || m_heightPx <= 0, E_INVALIDARG);
    BAIL_OUT_IF (m_transparencyWidthPx == m_widthPx &&
                 m_transparencyHeightPx == m_heightPx &&
                 m_fullTex != nullptr, S_OK);

    ReleaseTransparencyResources();

    td.Width            = static_cast<UINT> (m_widthPx);
    td.Height           = static_cast<UINT> (m_heightPx);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&td, nullptr, &m_fullTex);
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (m_fullTex.Get(), nullptr, &m_fullRtv);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_fullTex.Get(), nullptr, &m_fullSrv);
    CHRA (hr);

    hr = m_device->CreateTexture2D (&td, nullptr, &m_blurHTex);
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (m_blurHTex.Get(), nullptr, &m_blurHRtv);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_blurHTex.Get(), nullptr, &m_blurHSrv);
    CHRA (hr);

    hr = m_device->CreateTexture2D (&td, nullptr, &m_blurTex);
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (m_blurTex.Get(), nullptr, &m_blurRtv);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_blurTex.Get(), nullptr, &m_blurSrv);
    CHRA (hr);

    m_transparencyWidthPx  = m_widthPx;
    m_transparencyHeightPx = m_heightPx;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  UploadBlurParams
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::UploadBlurParams (const SettingsBlurParams & params)
{
    HRESULT                   hr     = S_OK;
    D3D11_MAPPED_SUBRESOURCE  mapped = {};



    hr = m_context->Map (m_blurConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHRA (hr);

    memcpy (mapped.pData, &params, sizeof (params));
    m_context->Unmap (m_blurConstantBuffer.Get(), 0);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  UploadComposeParams
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::UploadComposeParams (const SettingsComposeParams & params)
{
    HRESULT                   hr     = S_OK;
    D3D11_MAPPED_SUBRESOURCE  mapped = {};



    hr = m_context->Map (m_composeConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHRA (hr);

    memcpy (mapped.pData, &params, sizeof (params));
    m_context->Unmap (m_composeConstantBuffer.Get(), 0);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DrawFullscreen
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindowRenderer::DrawFullscreen (
    ID3D11RenderTargetView   * rt,
    ID3D11ShaderResourceView * srv0,
    ID3D11ShaderResourceView * srv1,
    ID3D11PixelShader        * ps,
    ID3D11Buffer             * constantBuffer)
{
    UINT                       stride         = sizeof (SettingsVertex);
    UINT                       offset         = 0;
    float                      clearColor[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float                      blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT             vp             = {};
    ID3D11ShaderResourceView * srvs[s_kMaxBoundPsSrvSlots]     = { srv0, srv1 };
    ID3D11ShaderResourceView * nullSrvs[s_kMaxBoundPsSrvSlots] = {};
    ID3D11Buffer             * cbs[1]         = { constantBuffer };



    m_context->OMSetRenderTargets      (1, &rt, nullptr);
    m_context->OMSetBlendState         (m_blendOpaque.Get(), blendFactor, 0xFFFFFFFF);
    m_context->ClearRenderTargetView   (rt, clearColor);

    vp.Width    = (float) m_widthPx;
    vp.Height   = (float) m_heightPx;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    m_context->IASetVertexBuffers     (0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer       (m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetInputLayout       (m_inputLayout.Get());
    m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->VSSetShader            (m_vs.Get(), nullptr, 0);
    m_context->PSSetShader            (ps,         nullptr, 0);
    m_context->PSSetSamplers          (0, 1, m_sampler.GetAddressOf());
    m_context->PSSetShaderResources   (0, s_kMaxBoundPsSrvSlots, srvs);
    m_context->PSSetConstantBuffers   (0, 1, cbs);

    m_context->DrawIndexed (s_kFullscreenIndexCount, 0, 0);

    m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);
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
    ReleaseTransparencyResources();

    hr = m_swapChain->ResizeBuffers (s_kSwapBufferCount,
                                     static_cast<UINT> (m_widthPx),
                                     static_cast<UINT> (m_heightPx),
                                     DXGI_FORMAT_UNKNOWN,
                                     0);
    CHRA (hr);

    hr = CreateBackBufferTarget();
    CHRA (hr);

    if (m_dcompDevice != nullptr)
    {
        hr = m_dcompDevice->Commit();
        CHRA (hr);
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  RenderDirect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::RenderDirect (
    SettingsPanel        & panel,
    const D3D11_VIEWPORT & viewport,
    const CassoTheme    & theme)
{
    HRESULT           hr            = S_OK;
    ChromeVisualState visual        = {};
    float             clearColor[4] = { 0.10196079f, 0.13333334f, 0.18823530f, 1.0f };



    if (!m_text.IsTargetBound())
    {
        hr = BindTextTarget();
        CHRA (hr);
    }

    m_context->RSSetViewports (1, &viewport);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDraw();
    CHRA (hr);

    visual.dpi = m_scaler.Dpi();
    if (m_titleBar != nullptr)
    {
        m_titleBar->Paint (m_painter, m_text, theme);
    }

    panel.Paint (m_painter, m_text);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

Error:
    return hr;
}




HRESULT SettingsWindowRenderer::RenderModalOverlay (
    SettingsPanel        & panel,
    const D3D11_VIEWPORT & viewport)
{
    HRESULT  hr = S_OK;



    if (!m_text.IsTargetBound())
    {
        hr = BindTextTarget();
        CHRA (hr);
    }

    m_context->RSSetViewports (1, &viewport);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDraw();
    CHRA (hr);

    panel.PaintModalOverlay (m_painter, m_text);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderPanelToTexture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::RenderPanelToTexture (SettingsPanel & panel, const CassoTheme & theme)
{
    HRESULT                  hr            = S_OK;
    ChromeVisualState        visual        = {};
    ComPtr<IDXGISurface>     surface;
    float                    clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };



    hr = m_fullTex.As (&surface);
    CHRA (hr);

    hr = m_text.BindBackBuffer (surface.Get(), m_scaler.Dpi(), m_scaler.Dpi());
    CHRA (hr);

    m_context->OMSetRenderTargets (1, m_fullRtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_fullRtv.Get(), clearColor);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDraw();
    CHRA (hr);

    visual.dpi = m_scaler.Dpi();
    if (m_titleBar != nullptr)
    {
        m_titleBar->Paint (m_painter, m_text, theme);
    }

    panel.Paint (m_painter, m_text);

    hr = m_painter.End (m_fullRtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

Error:
    m_text.UnbindBackBuffer();
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  RenderTransparency
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindowRenderer::RenderTransparency (
    SettingsPanel        & panel,
    const D3D11_VIEWPORT & viewport,
    const CassoTheme    & theme)
{
    HRESULT               hr             = S_OK;
    SettingsBlurParams    blurParams     = {};
    SettingsComposeParams composeParams  = {};
    float                 clearColor[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };



    hr = EnsureTransparencyResources();
    CHRA (hr);

    hr = RenderPanelToTexture (panel, theme);
    CHRA (hr);

    hr = BindTextTarget();
    CHRA (hr);

    blurParams.radiusPx = s_kGaussianRadiusPx;
    blurParams.outputW  = (float) m_widthPx;
    blurParams.outputH  = (float) m_heightPx;
    hr = UploadBlurParams (blurParams);
    CHRA (hr);

    DrawFullscreen (m_blurHRtv.Get(),
                    m_fullSrv.Get(), nullptr,
                    m_psGaussianH.Get(),
                    m_blurConstantBuffer.Get());
    DrawFullscreen (m_blurRtv.Get(),
                    m_blurHSrv.Get(), nullptr,
                    m_psGaussianV.Get(),
                    m_blurConstantBuffer.Get());

    composeParams.emuRectClient[0]   = (float) m_emuRectClient.left;
    composeParams.emuRectClient[1]   = (float) m_emuRectClient.top;
    composeParams.emuRectClient[2]   = (float) m_emuRectClient.right;
    composeParams.emuRectClient[3]   = (float) m_emuRectClient.bottom;
    composeParams.focusRectClient[0] = (float) m_focusRectClient.left;
    composeParams.focusRectClient[1] = (float) m_focusRectClient.top;
    composeParams.focusRectClient[2] = (float) m_focusRectClient.right;
    composeParams.focusRectClient[3] = (float) m_focusRectClient.bottom;
    composeParams.outputW            = (float) m_widthPx;
    composeParams.outputH            = (float) m_heightPx;
    composeParams.dimFactor          = s_kDimFactor;
    composeParams.featherPx          = s_kFocusFeatherPx;
    hr = UploadComposeParams (composeParams);
    CHRA (hr);

    m_context->RSSetViewports (1, &viewport);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    DrawFullscreen (m_rtv.Get(),
                    m_fullSrv.Get(), m_blurSrv.Get(),
                    m_psCompose.Get(),
                    m_composeConstantBuffer.Get());

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
    HRESULT           hr       = S_OK;
    D3D11_VIEWPORT    viewport = {};
    CassoTheme       theme    = (m_theme != nullptr) ? *m_theme : CassoTheme::Skeuomorphic();



    BAIL_OUT_IF (!m_initialized || m_context == nullptr || m_swapChain == nullptr || m_rtv == nullptr, S_OK);

    viewport.Width    = static_cast<float> (m_widthPx);
    viewport.Height   = static_cast<float> (m_heightPx);
    viewport.MaxDepth = 1.0f;

    panel.Layout (m_widthPx, m_heightPx, m_scaler, (m_titleBar != nullptr) ? m_titleBar->GetTitleHeight() : 0);
    panel.PreparePreviewFrame();

    if (m_transparencyActive)
    {
        hr = RenderTransparency (panel, viewport, theme);
        CHRA (hr);
    }
    else
    {
        hr = RenderDirect (panel, viewport, theme);
        CHRA (hr);
    }

    // Modal overlays (the color picker) paint in a fresh pass on top of
    // the finished (possibly blurred) panel so they stay crisp with no
    // page content bleeding through.
    if (panel.HasModalOverlay())
    {
        hr = RenderModalOverlay (panel, viewport);
        CHRA (hr);
    }

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

    if (m_dcompDevice != nullptr)
    {
        hr = m_dcompDevice->Commit();
        CHRA (hr);
    }

Error:
    (void) s_kClearArgb;
    return hr;
}
