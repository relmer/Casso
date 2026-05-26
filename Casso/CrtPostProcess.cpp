#include "Pch.h"

#include "CrtPostProcess.h"

#include "Config/CrtPresets.h"
#include "Shaders/ShaderResourceIds.h"

#pragma comment(lib, "d3dcompiler.lib")




////////////////////////////////////////////////////////////////////////////////
//
//  Embedded shader plumbing. Pixel shader source lives in Casso/Shaders/CRT
//  and is embedded as RCDATA so the .hlsl files are the single source of truth.
//
//  Shared vertex shader: emits a fullscreen triangle from the indexed quad
//  upload in CrtPostProcess::Initialize. Same input layout as the existing
//  D3DRenderer.cpp emulator-blit VS.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr UINT          s_kMaxBoundPsSrvSlots = 2;

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


    struct CrtVertex
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
//  MakeCrtParams
//
//  Pure logic. Produces a `CrtParams` constant-buffer payload by
//  resolving the precedence chain:
//
//    1. User override (prefsCrt.userOverride == true) -> prefsCrt verbatim
//    2. Theme variant override (themeDefaults != nullptr) -> theme values
//    3. Monitor-type preset (CrtPresets::ForMode(modeIndex)) -> preset values
//    4. (Implicit) struct defaults of GlobalUserPrefs::Crt -- fallback
//       if everything above is somehow unset
//
//  Gamma and persistence are NEW in the per-monitor schema and have
//  no theme-default equivalent yet; they fall through user override ->
//  monitor preset directly.
//
////////////////////////////////////////////////////////////////////////////////

CrtParams MakeCrtParams (
    const GlobalUserPrefs::Crt  & prefsCrt,
    size_t                        modeIndex,
    const ThemeCrtDefaults      * themeDefaults,
    float                         outputW,
    float                         outputH)
{
    CrtParams                    params;
    const GlobalUserPrefs::Crt & preset = CrtPresets::ForMode (modeIndex);

    // Default everything from the monitor-type preset; layered sources
    // overwrite into this struct in lowest-priority-first order so the
    // highest-priority winner is the final value.
    float  brightness  = preset.brightness;
    float  contrast    = preset.contrast;
    float  gamma       = preset.gamma;
    float  persistence = preset.persistence;
    bool   scanEn      = preset.scanlinesEnabled;
    float  scanInt     = preset.scanlinesIntensity;
    bool   bloomEn     = preset.bloomEnabled;
    float  bloomR      = preset.bloomRadius;
    float  bloomS      = preset.bloomStrength;
    bool   bleedEn     = preset.colorBleedEnabled;
    float  bleedW      = preset.colorBleedWidth;



    // Theme variant overrides land on top of the monitor preset for
    // every field the theme cares about. Theme doesn't yet carry
    // gamma / persistence so those keep preset values.
    if (!prefsCrt.userOverride && themeDefaults != nullptr)
    {
        brightness = themeDefaults->brightness;
        contrast   = themeDefaults->contrast;
        scanEn     = themeDefaults->scanlinesEnabled;
        scanInt    = themeDefaults->scanlinesIntensity;
        bloomEn    = themeDefaults->bloomEnabled;
        bloomR     = themeDefaults->bloomRadius;
        bloomS     = themeDefaults->bloomStrength;
        bleedEn    = themeDefaults->colorBleedEnabled;
        bleedW     = themeDefaults->colorBleedWidth;
    }

    // User overrides win outright. Once flipped, prefs values land
    // verbatim regardless of theme or preset.
    if (prefsCrt.userOverride)
    {
        brightness  = prefsCrt.brightness;
        contrast    = prefsCrt.contrast;
        gamma       = prefsCrt.gamma;
        persistence = prefsCrt.persistence;
        scanEn      = prefsCrt.scanlinesEnabled;
        scanInt     = prefsCrt.scanlinesIntensity;
        bloomEn     = prefsCrt.bloomEnabled;
        bloomR      = prefsCrt.bloomRadius;
        bloomS      = prefsCrt.bloomStrength;
        bleedEn     = prefsCrt.colorBleedEnabled;
        bleedW      = prefsCrt.colorBleedWidth;
    }

    params.brightness        = brightness;
    params.contrast          = contrast;
    params.gamma             = gamma;
    params.persistence       = persistence;
    params.scanlineIntensity = scanEn  ? scanInt : 0.0f;
    params.bloomRadius       = bloomEn ? bloomR  : 0.0f;
    params.bloomStrength     = bloomEn ? bloomS  : 0.0f;
    params.colorBleedWidth   = bleedEn ? bleedW  : 0.0f;
    params.outputW           = (outputW > 0.0f) ? outputW : 1.0f;
    params.outputH           = (outputH > 0.0f) ? outputH : 1.0f;

    return params;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeLetterboxRect
//
//  Returns the largest centered 4:3 sub-rectangle that fits inside the
//  back buffer.
//
////////////////////////////////////////////////////////////////////////////////

RECT ComputeLetterboxRect (int backBufferW, int backBufferH)
{
    RECT  contentRect = { 0, 0, backBufferW, backBufferH };



    return ComputeLetterboxRectInRect (contentRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeLetterboxRectInRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ComputeLetterboxRectInRect (const RECT & contentRect)
{
    // Backward-compatible alias: fixed 4:3 aspect for callers that
    // do not yet pass the desired aspect through.
    return ComputeAspectFitRectInRect (contentRect, 4, 3);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeAspectFitRectInRect
//
//  Returns the largest centered sub-rectangle of `contentRect` whose
//  width:height equals `aspectW:aspectH`. Pillarbox or letterbox bars
//  fill the remainder. Integer arithmetic keeps the bars pixel-aligned.
//
////////////////////////////////////////////////////////////////////////////////

RECT ComputeAspectFitRectInRect (const RECT & contentRect, int aspectW, int aspectH)
{
    RECT  r        = {};
    int   contentW = contentRect.right  - contentRect.left;
    int   contentH = contentRect.bottom - contentRect.top;



    if (contentW <= 0 || contentH <= 0 || aspectW <= 0 || aspectH <= 0)
    {
        return r;
    }

    int  wForH = (contentH * aspectW) / aspectH;
    if (wForH <= contentW)
    {
        int  barX = (contentW - wForH) / 2;

        r.left   = contentRect.left + barX;
        r.top    = contentRect.top;
        r.right  = r.left + wForH;
        r.bottom = contentRect.bottom;
    }
    else
    {
        int  hForW = (contentW * aspectH) / aspectW;
        int  barY  = (contentH - hForW) / 2;

        r.left   = contentRect.left;
        r.top    = contentRect.top + barY;
        r.right  = contentRect.right;
        r.bottom = r.top + hForW;
    }

    return r;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPostProcess
//
////////////////////////////////////////////////////////////////////////////////

CrtPostProcess::CrtPostProcess()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~CrtPostProcess
//
////////////////////////////////////////////////////////////////////////////////

CrtPostProcess::~CrtPostProcess()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompilePixelShader
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPostProcess::CompilePixelShader (
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
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPostProcess::Initialize (
    ID3D11Device         * device,
    ID3D11DeviceContext  * context)
{
    HRESULT                hr       = S_OK;
    ComPtr<ID3DBlob>       vsBlob;
    ComPtr<ID3DBlob>       errors;
    D3D11_BUFFER_DESC      bd       = {};
    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SAMPLER_DESC     sd       = {};
    D3D11_BLEND_DESC       bld      = {};

    CrtVertex vertices[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f },
    };

    UINT16 indices[] = { 0, 1, 2, 2, 1, 3 };

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };


    CBRAEx (device, E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

    m_device  = device;
    m_context = context;

    // VS (shared).
    hr = D3DCompile (s_kpszVertexShaderSrc, strlen (s_kpszVertexShaderSrc),
                     "CrtPostProcess.hlsl", nullptr, nullptr, "main",
                     "vs_4_0", 0, 0, &vsBlob, &errors);
    CHRA (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(),
                                       nullptr, &m_vs);
    CHRA (hr);

    hr = m_device->CreateInputLayout (layout, 2,
                                      vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(),
                                      &m_inputLayout);
    CHRA (hr);

    // Pixel shaders.
    hr = CompilePixelShader (IDR_HLSL_BRIGHTNESS,      "brightness.hlsl",       &m_psBrightness);   CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_SCANLINES,       "scanlines.hlsl",        &m_psScanlines);    CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_BLOOM_H,         "bloom_h.hlsl",          &m_psBloomH);       CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_BLOOM_V,         "bloom_v.hlsl",          &m_psBloomV);       CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_BLOOM_COMPOSITE, "bloom_composite.hlsl",  &m_psBloomComp);    CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_COLOR_BLEED,     "color_bleed.hlsl",      &m_psColorBleed);   CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_PERSISTENCE,     "persistence.hlsl",      &m_psPersistence);  CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_GAMMA,           "gamma.hlsl",            &m_psGamma);        CHRA (hr);
    hr = CompilePixelShader (IDR_HLSL_COPY,            "copy.hlsl",             &m_psCopy);         CHRA (hr);

    // Quad geometry.
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

    // Constant buffer (CrtParams). Dynamic so we can update every frame
    // from the UI thread without recreating.
    bd = {};
    bd.ByteWidth      = sizeof (CrtParams);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer (&bd, nullptr, &m_constantBuffer);
    CHRA (hr);

    // Bilinear sampler (clamped).
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = m_device->CreateSamplerState (&sd, &m_sampler);
    CHRA (hr);

    // Opaque blend (the chain is internally opaque; the back buffer composite
    // will see this overwrite the cleared-black bars).
    bld.RenderTarget[0].BlendEnable           = FALSE;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_device->CreateBlendState (&bld, &m_blendOpaque);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPostProcess::EnsureSize (int width, int height)
{
    HRESULT              hr  = S_OK;
    D3D11_TEXTURE2D_DESC td  = {};
    int                  i   = 0;


    BAIL_OUT_IF (width <= 0 || height <= 0, E_INVALIDARG);

    if (width == m_width && height == m_height && m_ppMainTex[0])
    {
        return S_OK;
    }

    // Unbind everything from the pipeline before releasing the old
    // resources. D3D11 retains internal references to bound RTVs and
    // SRVs; if we just Reset the ComPtrs while they're still bound,
    // the GPU keeps the old (now full-window-sized) textures alive
    // until the next pipeline rebind. During rapid window resize that
    // stacks up VRAM and command-buffer pressure and is a common
    // trigger for DXGI_ERROR_DRIVER_INTERNAL_ERROR. Unbinding here
    // ensures the Reset()s below actually free the GPU memory.
    if (m_context != nullptr)
    {
        ID3D11ShaderResourceView *  nullSrvs[s_kMaxBoundPsSrvSlots] = {};

        m_context->OMSetRenderTargets   (0, nullptr, nullptr);
        m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);
    }

    for (i = 0; i < 2; ++i)
    {
        m_ppMainSrv[i].Reset();
        m_ppMainRtv[i].Reset();
        m_ppMainTex[i].Reset();
        m_ppBloomSrv[i].Reset();
        m_ppBloomRtv[i].Reset();
        m_ppBloomTex[i].Reset();
    }

    td.Width            = static_cast<UINT> (width);
    td.Height           = static_cast<UINT> (height);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    for (i = 0; i < 2; ++i)
    {
        hr = m_device->CreateTexture2D (&td, nullptr, &m_ppMainTex[i]);
        CHRA (hr);
        hr = m_device->CreateRenderTargetView (m_ppMainTex[i].Get(), nullptr, &m_ppMainRtv[i]);
        CHRA (hr);
        hr = m_device->CreateShaderResourceView (m_ppMainTex[i].Get(), nullptr, &m_ppMainSrv[i]);
        CHRA (hr);

        hr = m_device->CreateTexture2D (&td, nullptr, &m_ppBloomTex[i]);
        CHRA (hr);
        hr = m_device->CreateRenderTargetView (m_ppBloomTex[i].Get(), nullptr, &m_ppBloomRtv[i]);
        CHRA (hr);
        hr = m_device->CreateShaderResourceView (m_ppBloomTex[i].Get(), nullptr, &m_ppBloomSrv[i]);
        CHRA (hr);
    }

    // Persistence carry-over RT. One texture, not ping-pong; the
    // previous frame's final color (pre-gamma) lives here from one
    // Process() call to the next.
    hr = m_device->CreateTexture2D (&td, nullptr, &m_persistenceTex);
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (m_persistenceTex.Get(), nullptr, &m_persistenceRtv);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_persistenceTex.Get(), nullptr, &m_persistenceSrv);
    CHRA (hr);
    m_persistencePrimed = false;

    m_width  = width;
    m_height = height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UploadConstants
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPostProcess::UploadConstants (const CrtParams & params)
{
    HRESULT                   hr     = S_OK;
    D3D11_MAPPED_SUBRESOURCE  mapped = {};



    hr = m_context->Map (m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHRA (hr);

    memcpy (mapped.pData, &params, sizeof (params));

    m_context->Unmap (m_constantBuffer.Get(), 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawFullscreen
//
//  Binds rt as the sole output, srv0/srv1 as inputs (srv1 may be null),
//  the supplied pixel shader, sets a full-RT viewport unless `subViewport`
//  is non-null, clears the RT to opaque black (so letterbox bars stay
//  black no matter which pass writes them), and issues one DrawIndexed.
//
////////////////////////////////////////////////////////////////////////////////

void CrtPostProcess::DrawFullscreen (
    ID3D11RenderTargetView   * rt,
    ID3D11ShaderResourceView * srv0,
    ID3D11ShaderResourceView * srv1,
    ID3D11PixelShader        * ps,
    int                        viewportW,
    int                        viewportH,
    const RECT               * subViewport)
{
    UINT                       stride        = sizeof (CrtVertex);
    UINT                       offset        = 0;
    float                      clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    D3D11_VIEWPORT             vp            = {};
    ID3D11ShaderResourceView * srvs[s_kMaxBoundPsSrvSlots]     = { srv0, srv1 };
    ID3D11ShaderResourceView * nullSrvs[s_kMaxBoundPsSrvSlots] = {};
    ID3D11Buffer             * cbs[1]        = { m_constantBuffer.Get() };
    float                      blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };


    // Unbind RTs first so a previously-bound RT isn't simultaneously bound
    // as an SRV when we set inputs below.
    m_context->OMSetRenderTargets (1, &rt, nullptr);
    m_context->OMSetBlendState    (m_blendOpaque.Get(), blendFactor, 0xFFFFFFFF);
    m_context->ClearRenderTargetView (rt, clearColor);

    if (subViewport != nullptr)
    {
        vp.TopLeftX = (float) subViewport->left;
        vp.TopLeftY = (float) subViewport->top;
        vp.Width    = (float) (subViewport->right  - subViewport->left);
        vp.Height   = (float) (subViewport->bottom - subViewport->top);
    }
    else
    {
        vp.Width    = (float) viewportW;
        vp.Height   = (float) viewportH;
    }
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

    m_context->DrawIndexed (6, 0, 0);

    // Detach SRVs so the just-written RT can be bound as an input on the
    // next pass without a D3D11 hazard warning.
    m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Process
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPostProcess::Process (
    ID3D11ShaderResourceView  * srcSrv,
    ID3D11RenderTargetView    * dstRtv,
    const CrtParams           & params,
    const RECT                & viewportRect,
    int                         backBufferW,
    int                         backBufferH)
{
    HRESULT  hr   = S_OK;
    int      cur  = 0;



    CBRAEx (srcSrv, E_INVALIDARG);
    CBRAEx (dstRtv, E_INVALIDARG);
    CBRA (m_device);
    CBRA (m_context);

    hr = EnsureSize (backBufferW, backBufferH);
    CHRA (hr);

    hr = UploadConstants (params);
    CHRA (hr);

    // Brightness pass: srcEmulator -> ppMain[0], in the letterboxed
    // viewport. The pre-pass clear renders the bars black. Always
    // runs (it also applies contrast and is the only pass that knows
    // the viewport rect).
    DrawFullscreen (m_ppMainRtv[0].Get(),
                    srcSrv, nullptr,
                    m_psBrightness.Get(),
                    backBufferW, backBufferH,
                    &viewportRect);
    cur = 0;

    // Bloom: 3-pass blur + composite. Skipped wholesale when strength
    // or radius is zero (idle GPU win on color preset which has bloom
    // off, and gives the user a true off when they toggle it).
    if (params.bloomStrength > 0.0f && params.bloomRadius > 0.0f)
    {
        DrawFullscreen (m_ppBloomRtv[0].Get(),
                        m_ppMainSrv[cur].Get(), nullptr,
                        m_psBloomH.Get(),
                        backBufferW, backBufferH,
                        nullptr);
        DrawFullscreen (m_ppBloomRtv[1].Get(),
                        m_ppBloomSrv[0].Get(), nullptr,
                        m_psBloomV.Get(),
                        backBufferW, backBufferH,
                        nullptr);
        DrawFullscreen (m_ppMainRtv[1 - cur].Get(),
                        m_ppMainSrv[cur].Get(),
                        m_ppBloomSrv[1].Get(),
                        m_psBloomComp.Get(),
                        backBufferW, backBufferH,
                        nullptr);
        cur = 1 - cur;
    }

    // Color bleed: 1 pass. Skipped when width is zero.
    if (params.colorBleedWidth > 0.0f)
    {
        DrawFullscreen (m_ppMainRtv[1 - cur].Get(),
                        m_ppMainSrv[cur].Get(), nullptr,
                        m_psColorBleed.Get(),
                        backBufferW, backBufferH,
                        nullptr);
        cur = 1 - cur;
    }

    // Persistence MUST run BEFORE scanlines: max() only ever
    // brightens, so it would erase scanline darkening every frame
    // if it ran after. When persistence > 0 but not yet primed
    // (first frame after EnsureSize), skip the shader and just
    // capture the current state so the next frame has a prior.
    if (params.persistence > 0.0f)
    {
        if (m_persistencePrimed)
        {
            DrawFullscreen (m_ppMainRtv[1 - cur].Get(),
                            m_ppMainSrv[cur].Get(),
                            m_persistenceSrv.Get(),
                            m_psPersistence.Get(),
                            backBufferW, backBufferH,
                            nullptr);
            cur = 1 - cur;
        }
        if (m_persistenceTex && m_ppMainTex[cur])
        {
            m_context->CopyResource (m_persistenceTex.Get(), m_ppMainTex[cur].Get());
            m_persistencePrimed = true;
        }
    }

    // Scanlines: 1 pass. Skipped when intensity is zero (scanlines
    // toggle off in UI sets intensity=0 in MakeCrtParams).
    if (params.scanlineIntensity > 0.0f)
    {
        DrawFullscreen (m_ppMainRtv[1 - cur].Get(),
                        m_ppMainSrv[cur].Get(), nullptr,
                        m_psScanlines.Get(),
                        backBufferW, backBufferH,
                        nullptr);
        cur = 1 - cur;
    }

    // Gamma. Skip when within 1% of 1.0 (no-op pow). Tiny epsilon
    // since UI slider steps are 0.1.
    if (params.gamma < 0.99f || params.gamma > 1.01f)
    {
        DrawFullscreen (m_ppMainRtv[1 - cur].Get(),
                        m_ppMainSrv[cur].Get(), nullptr,
                        m_psGamma.Get(),
                        backBufferW, backBufferH,
                        nullptr);
        cur = 1 - cur;
    }

    // Final copy to back buffer.
    DrawFullscreen (dstRtv,
                    m_ppMainSrv[cur].Get(), nullptr,
                    m_psCopy.Get(),
                    backBufferW, backBufferH,
                    nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void CrtPostProcess::Shutdown()
{
    int  i = 0;



    for (i = 0; i < 2; ++i)
    {
        m_ppMainSrv[i].Reset();
        m_ppMainRtv[i].Reset();
        m_ppMainTex[i].Reset();
        m_ppBloomSrv[i].Reset();
        m_ppBloomRtv[i].Reset();
        m_ppBloomTex[i].Reset();
    }
    m_persistenceSrv.Reset();
    m_persistenceRtv.Reset();
    m_persistenceTex.Reset();
    m_persistencePrimed = false;

    m_blendOpaque.Reset();
    m_sampler.Reset();
    m_constantBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_vs.Reset();

    m_psBrightness.Reset();
    m_psScanlines.Reset();
    m_psBloomH.Reset();
    m_psBloomV.Reset();
    m_psBloomComp.Reset();
    m_psColorBleed.Reset();
    m_psPersistence.Reset();
    m_psGamma.Reset();
    m_psCopy.Reset();

    m_device  = nullptr;
    m_context = nullptr;
    m_width   = 0;
    m_height  = 0;
}
