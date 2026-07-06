#include "Pch.h"

#include "SettingsCompositor.h"
#include "../../Shaders/ShaderResourceIds.h"

#pragma comment(lib, "d3dcompiler.lib")




namespace
{
    constexpr UINT          s_kMaxBoundPsSrvSlots    = 2;
    constexpr UINT          s_kFullscreenIndexCount  = 6;
    constexpr UINT          s_kTexCoordOffsetBytes   = sizeof (float) * 2;
    constexpr float         s_kGaussianRadiusPx      = 8.0f;
    constexpr float         s_kDimFactor             = 0.25f;
    // Feather the focused-control sharp pop-out by this many pixels beyond the
    // control's row rect so the boundary against the blurred backdrop reads as a
    // soft halo, not a harsh edge.
    constexpr float         s_kFocusFeatherPx        = 24.0f;

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
//  ~SettingsCompositor
//
////////////////////////////////////////////////////////////////////////////////

SettingsCompositor::~SettingsCompositor ()
{
    Shutdown();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsCompositor::Initialize (ID3D11Device * device, ID3D11DeviceContext * context)
{
    HRESULT  hr = S_OK;


    CBRAEx (device,  E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

    if (m_initialized)
    {
        return S_OK;
    }

    m_device  = device;
    m_context = context;

    hr = CreateResources();
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

void SettingsCompositor::Shutdown ()
{
    ReleaseBlurTextures();

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

    m_device      = nullptr;
    m_context     = nullptr;
    m_initialized = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetTransparencyState
//
////////////////////////////////////////////////////////////////////////////////

void SettingsCompositor::SetTransparencyState (bool active, RECT emuRectClient, RECT focusRectClient)
{
    m_transparencyActive = active;
    m_emuRectClient      = emuRectClient;
    m_focusRectClient    = focusRectClient;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CompilePixelShader
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsCompositor::CompilePixelShader (int resourceId, const char * sourceName, ID3D11PixelShader ** out)
{
    HRESULT           hr     = S_OK;
    ShaderSource      source = {};
    ComPtr<ID3DBlob>  blob;
    ComPtr<ID3DBlob>  errors;


    CBRAEx (sourceName, E_INVALIDARG);
    CBRAEx (out,        E_INVALIDARG);

    *out = nullptr;

    hr = LoadShaderSource (resourceId, &source);
    CHRA (hr);

    hr = D3DCompile (source.pData, source.cbData, sourceName,
                     nullptr, nullptr, "main", "ps_4_0", 0, 0, &blob, &errors);
    CHRA (hr);

    hr = m_device->CreatePixelShader (blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, out);
    CHRA (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CreateResources
//
//  Fullscreen quad + input layout + shaders + constant buffers + sampler +
//  opaque blend. Sized-independent (the blur textures are lazily created in
//  EnsureBlurTextures once the back-buffer size is known).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsCompositor::CreateResources ()
{
    HRESULT                 hr       = S_OK;
    ComPtr<ID3DBlob>        vsBlob;
    ComPtr<ID3DBlob>        errors;
    D3D11_BUFFER_DESC       bd       = {};
    D3D11_SUBRESOURCE_DATA  initData = {};
    D3D11_SAMPLER_DESC      sd       = {};
    D3D11_BLEND_DESC        bld      = {};

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
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                      D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, s_kTexCoordOffsetBytes, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };


    hr = D3DCompile (s_kpszVertexShaderSrc, strlen (s_kpszVertexShaderSrc), "SettingsCompositor.hlsl",
                     nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errors);
    CHRA (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    CHRA (hr);

    hr = m_device->CreateInputLayout (layout, ARRAYSIZE (layout),
                                      vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);
    CHRA (hr);

    hr = CompilePixelShader (IDR_HLSL_SETTINGS_GAUSSIAN_H, "gaussian_h.hlsl",       &m_psGaussianH);  CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_SETTINGS_GAUSSIAN_V, "gaussian_v.hlsl",       &m_psGaussianV);  CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_SETTINGS_COMPOSE,    "settings_compose.hlsl", &m_psCompose);    CHRA (hr);

    bd.ByteWidth     = sizeof (vertices);
    bd.Usage         = D3D11_USAGE_DEFAULT;
    bd.BindFlags     = D3D11_BIND_VERTEX_BUFFER;
    initData.pSysMem = vertices;
    hr = m_device->CreateBuffer (&bd, &initData, &m_vertexBuffer);
    CHRA (hr);

    bd = {};
    bd.ByteWidth     = sizeof (indices);
    bd.Usage         = D3D11_USAGE_DEFAULT;
    bd.BindFlags     = D3D11_BIND_INDEX_BUFFER;
    initData         = {};
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
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseBlurTextures
//
////////////////////////////////////////////////////////////////////////////////

void SettingsCompositor::ReleaseBlurTextures ()
{
    ID3D11ShaderResourceView * nullSrvs[s_kMaxBoundPsSrvSlots] = {};


    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets   (0, nullptr, nullptr);
        m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);
    }

    m_blurVSrv.Reset();
    m_blurVRtv.Reset();
    m_blurVTex.Reset();
    m_blurHSrv.Reset();
    m_blurHRtv.Reset();
    m_blurHTex.Reset();
    m_blurWidthPx  = 0;
    m_blurHeightPx = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EnsureBlurTextures
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsCompositor::EnsureBlurTextures (int widthPx, int heightPx)
{
    HRESULT               hr = S_OK;
    D3D11_TEXTURE2D_DESC  td = {};


    BAIL_OUT_IF (widthPx <= 0 || heightPx <= 0, E_INVALIDARG);
    BAIL_OUT_IF (m_blurWidthPx == widthPx && m_blurHeightPx == heightPx && m_blurHTex != nullptr, S_OK);

    ReleaseBlurTextures();

    td.Width            = static_cast<UINT> (widthPx);
    td.Height           = static_cast<UINT> (heightPx);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&td, nullptr, &m_blurHTex);
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (m_blurHTex.Get(), nullptr, &m_blurHRtv);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_blurHTex.Get(), nullptr, &m_blurHSrv);
    CHRA (hr);

    hr = m_device->CreateTexture2D (&td, nullptr, &m_blurVTex);
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (m_blurVTex.Get(), nullptr, &m_blurVRtv);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_blurVTex.Get(), nullptr, &m_blurVSrv);
    CHRA (hr);

    m_blurWidthPx  = widthPx;
    m_blurHeightPx = heightPx;

Error:
    if (FAILED (hr))
    {
        ReleaseBlurTextures();
    }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  UploadBlurParams
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsCompositor::UploadBlurParams (const SettingsBlurParams & params)
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

HRESULT SettingsCompositor::UploadComposeParams (const SettingsComposeParams & params)
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
//  Clears `rt` to transparent then draws the fullscreen quad with `ps`, sampling
//  srv0 (t0) + srv1 (t1). Unbinds the SRVs afterwards so the same textures can be
//  reused as render targets on the next pass without a read/write hazard.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsCompositor::DrawFullscreen (
    ID3D11RenderTargetView   * rt,
    ID3D11ShaderResourceView * srv0,
    ID3D11ShaderResourceView * srv1,
    ID3D11PixelShader        * ps,
    ID3D11Buffer             * constantBuffer,
    int                        widthPx,
    int                        heightPx)
{
    UINT                       stride         = sizeof (SettingsVertex);
    UINT                       offset         = 0;
    float                      clearColor[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float                      blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT             vp             = {};
    ID3D11ShaderResourceView * srvs[s_kMaxBoundPsSrvSlots]     = { srv0, srv1 };
    ID3D11ShaderResourceView * nullSrvs[s_kMaxBoundPsSrvSlots] = {};
    ID3D11Buffer             * cbs[1]         = { constantBuffer };


    m_context->OMSetRenderTargets    (1, &rt, nullptr);
    m_context->OMSetBlendState       (m_blendOpaque.Get(), blendFactor, 0xFFFFFFFF);
    m_context->ClearRenderTargetView (rt, clearColor);

    vp.Width    = (float) widthPx;
    vp.Height   = (float) heightPx;
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
//  Compose
//
//  DxuiRenderTarget compose hook. `contentSrv` is the panel already rendered to
//  the base's offscreen content texture; `backBufferRtv` is the window's back
//  buffer (which this hook owns and Presents afterwards).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsCompositor::Compose (
    ID3D11ShaderResourceView * contentSrv,
    ID3D11RenderTargetView   * backBufferRtv,
    int                        widthPx,
    int                        heightPx)
{
    SettingsComposeParams  composeParams = {};


    if (!m_initialized || contentSrv == nullptr || backBufferRtv == nullptr || widthPx <= 0 || heightPx <= 0)
    {
        return;
    }
    if (FAILED (EnsureBlurTextures (widthPx, heightPx)))
    {
        return;
    }

    composeParams.outputW = (float) widthPx;
    composeParams.outputH = (float) heightPx;

    if (m_transparencyActive)
    {
        SettingsBlurParams  blurParams = {};

        blurParams.radiusPx = s_kGaussianRadiusPx;
        blurParams.outputW  = (float) widthPx;
        blurParams.outputH  = (float) heightPx;
        (void) UploadBlurParams (blurParams);

        // Separable Gaussian: horizontal (content -> blurH), then vertical
        // (blurH -> blurV).
        DrawFullscreen (m_blurHRtv.Get(), contentSrv,        nullptr, m_psGaussianH.Get(), m_blurConstantBuffer.Get(), widthPx, heightPx);
        DrawFullscreen (m_blurVRtv.Get(), m_blurHSrv.Get(),  nullptr, m_psGaussianV.Get(), m_blurConstantBuffer.Get(), widthPx, heightPx);

        composeParams.emuRectClient[0]   = (float) m_emuRectClient.left;
        composeParams.emuRectClient[1]   = (float) m_emuRectClient.top;
        composeParams.emuRectClient[2]   = (float) m_emuRectClient.right;
        composeParams.emuRectClient[3]   = (float) m_emuRectClient.bottom;
        composeParams.focusRectClient[0] = (float) m_focusRectClient.left;
        composeParams.focusRectClient[1] = (float) m_focusRectClient.top;
        composeParams.focusRectClient[2] = (float) m_focusRectClient.right;
        composeParams.focusRectClient[3] = (float) m_focusRectClient.bottom;
        composeParams.dimFactor          = s_kDimFactor;
        composeParams.featherPx          = s_kFocusFeatherPx;
        (void) UploadComposeParams (composeParams);

        DrawFullscreen (backBufferRtv, contentSrv, m_blurVSrv.Get(), m_psCompose.Get(), m_composeConstantBuffer.Get(), widthPx, heightPx);
    }
    else
    {
        // Inactive: focus rect covers the whole frame so FocusWeight == 1
        // everywhere -> the compose outputs the sharp content opaque (no blur,
        // no emulator hole). Bind content as both sources so texBlur is valid
        // even though the lerp ignores it.
        composeParams.focusRectClient[0] = 0.0f;
        composeParams.focusRectClient[1] = 0.0f;
        composeParams.focusRectClient[2] = (float) widthPx;
        composeParams.focusRectClient[3] = (float) heightPx;
        composeParams.dimFactor          = 1.0f;
        composeParams.featherPx          = 0.0f;
        (void) UploadComposeParams (composeParams);

        DrawFullscreen (backBufferRtv, contentSrv, contentSrv, m_psCompose.Get(), m_composeConstantBuffer.Get(), widthPx, heightPx);
    }
}
