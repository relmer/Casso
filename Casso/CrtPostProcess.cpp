#include "Pch.h"

#include "CrtPostProcess.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Inline shader source. Kept in sync with the canonical .hlsl files under
//  Casso/Shaders/CRT/. The .hlsl files are the source of truth for offline
//  tooling + the GPL-guard script (scripts/CheckShaderLicenses.ps1); the
//  copies here exist so the renderer has no runtime dependency on a
//  separate Shaders/ directory.
//
//  Shared vertex shader: emits a fullscreen triangle from the indexed quad
//  upload in CrtPostProcess::Initialize. Same input layout as the existing
//  D3DRenderer.cpp emulator-blit VS.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char *  kVertexShaderSrc =
        "struct VSInput  { float2 pos : POSITION; float2 uv : TEXCOORD; };\n"
        "struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "VSOutput main (VSInput i)\n"
        "{\n"
        "    VSOutput o;\n"
        "    o.pos = float4 (i.pos, 0.0f, 1.0f);\n"
        "    o.uv  = i.uv;\n"
        "    return o;\n"
        "}\n";

    // See Shaders/CRT/brightness.hlsl
    constexpr const char *  kPsBrightness =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_pad; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float4 c = tex.Sample (sam, i.uv);\n"
        "    c.rgb *= g_brightness;\n"
        "    return c;\n"
        "}\n";

    // See Shaders/CRT/scanlines.hlsl
    constexpr const char *  kPsScanlines =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_pad; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float4 c    = tex.Sample (sam, i.uv);\n"
        "    float  row  = i.pos.y;\n"
        "    float  wave = 0.5 + 0.5 * cos (row * 3.14159265);\n"
        "    float  d    = 1.0 - g_scanlineIntensity * wave * 0.6;\n"
        "    c.rgb *= d;\n"
        "    return c;\n"
        "}\n";

    // See Shaders/CRT/bloom_h.hlsl
    constexpr const char *  kPsBloomH =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_pad; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "static const float W[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float  tx   = 1.0 / max (g_outputW, 1.0);\n"
        "    float  step = tx * max (g_bloomRadius, 0.001);\n"
        "    float3 acc  = tex.Sample (sam, i.uv).rgb * W[0];\n"
        "    [unroll] for (int k = 1; k < 5; ++k)\n"
        "    {\n"
        "        float2 off = float2 (step * (float) k, 0.0);\n"
        "        acc += tex.Sample (sam, i.uv + off).rgb * W[k];\n"
        "        acc += tex.Sample (sam, i.uv - off).rgb * W[k];\n"
        "    }\n"
        "    return float4 (acc, 1.0);\n"
        "}\n";

    // See Shaders/CRT/bloom_v.hlsl
    constexpr const char *  kPsBloomV =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_pad; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "static const float W[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float  ty   = 1.0 / max (g_outputH, 1.0);\n"
        "    float  step = ty * max (g_bloomRadius, 0.001);\n"
        "    float3 acc  = tex.Sample (sam, i.uv).rgb * W[0];\n"
        "    [unroll] for (int k = 1; k < 5; ++k)\n"
        "    {\n"
        "        float2 off = float2 (0.0, step * (float) k);\n"
        "        acc += tex.Sample (sam, i.uv + off).rgb * W[k];\n"
        "        acc += tex.Sample (sam, i.uv - off).rgb * W[k];\n"
        "    }\n"
        "    return float4 (acc, 1.0);\n"
        "}\n";

    // See Shaders/CRT/bloom_composite.hlsl
    constexpr const char *  kPsBloomComp =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_pad; };\n"
        "Texture2D    texSrc   : register(t0);\n"
        "Texture2D    texBloom : register(t1);\n"
        "SamplerState sam      : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float3 b = texSrc.Sample   (sam, i.uv).rgb;\n"
        "    float3 g = texBloom.Sample (sam, i.uv).rgb;\n"
        "    return float4 (b + g * g_bloomStrength, 1.0);\n"
        "}\n";

    // See Shaders/CRT/color_bleed.hlsl
    constexpr const char *  kPsColorBleed =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_pad; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float3 ToYCbCr (float3 c) { float y = 0.299*c.r + 0.587*c.g + 0.114*c.b; float cb = -0.168736*c.r - 0.331264*c.g + 0.5*c.b; float cr = 0.5*c.r - 0.418688*c.g - 0.081312*c.b; return float3 (y, cb, cr); }\n"
        "float3 ToRgb (float3 c) { float r = c.x + 1.402*c.z; float g = c.x - 0.344136*c.y - 0.714136*c.z; float b = c.x + 1.772*c.y; return float3 (r, g, b); }\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float  tx     = 1.0 / max (g_outputW, 1.0);\n"
        "    float  radius = max (g_colorBleedWidth, 0.0);\n"
        "    float3 centerYcbcr = ToYCbCr (tex.Sample (sam, i.uv).rgb);\n"
        "    float2 chromaAcc   = centerYcbcr.yz;\n"
        "    float  weightSum   = 1.0;\n"
        "    int    iRadius     = (int) ceil (radius);\n"
        "    [unroll(8)] for (int k = 1; k <= 8; ++k)\n"
        "    {\n"
        "        if (k > iRadius) break;\n"
        "        float  w = (radius - (float) (k - 1)) / max (radius, 0.0001);\n"
        "        if (w < 0.0) w = 0.0;\n"
        "        float2 off = float2 (tx * (float) k, 0.0);\n"
        "        float3 p = ToYCbCr (tex.Sample (sam, i.uv + off).rgb);\n"
        "        float3 m = ToYCbCr (tex.Sample (sam, i.uv - off).rgb);\n"
        "        chromaAcc += p.yz * w + m.yz * w;\n"
        "        weightSum += 2.0 * w;\n"
        "    }\n"
        "    float3 outY = float3 (centerYcbcr.x, chromaAcc / weightSum);\n"
        "    return float4 (ToRgb (outY), 1.0);\n"
        "}\n";

    // See Shaders/CRT/copy.hlsl
    constexpr const char *  kPsCopy =
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    return tex.Sample (sam, i.uv);\n"
        "}\n";


    struct CrtVertex
    {
        float x;
        float y;
        float u;
        float v;
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeCrtParams
//
//  Pure logic. Produces a `CrtParams` constant buffer
//  payload from the user's prefs + (optionally) the active theme's
//  `crtDefaults`.
//
//  Resolution rule:
//      * If `prefsCrt.userOverride == true` -> use prefs values verbatim.
//      * Otherwise, if `themeDefaults != nullptr` -> use theme values.
//      * Otherwise -> use the in-struct defaults of `GlobalUserPrefs::Crt`.
//
////////////////////////////////////////////////////////////////////////////////

CrtParams MakeCrtParams (
    const GlobalUserPrefs::Crt  & prefsCrt,
    const ThemeCrtDefaults      * themeDefaults,
    float                         outputW,
    float                         outputH)
{
    CrtParams  params;
    float      brightness   = prefsCrt.brightness;
    bool       scanEn       = prefsCrt.scanlinesEnabled;
    float      scanInt      = prefsCrt.scanlinesIntensity;
    bool       bloomEn      = prefsCrt.bloomEnabled;
    float      bloomR       = prefsCrt.bloomRadius;
    float      bloomS       = prefsCrt.bloomStrength;
    bool       bleedEn      = prefsCrt.colorBleedEnabled;
    float      bleedW       = prefsCrt.colorBleedWidth;


    // Resolution rule: theme overrides apply only when the user hasn't
    // committed any prefs of their own. The moment FromJson parses a
    // "crt" object on disk we flip userOverride=true and prefs win.
    if (!prefsCrt.userOverride && themeDefaults != nullptr)
    {
        brightness = themeDefaults->brightness;
        scanEn     = themeDefaults->scanlinesEnabled;
        scanInt    = themeDefaults->scanlinesIntensity;
        bloomEn    = themeDefaults->bloomEnabled;
        bloomR     = themeDefaults->bloomRadius;
        bloomS     = themeDefaults->bloomStrength;
        bleedEn    = themeDefaults->colorBleedEnabled;
        bleedW     = themeDefaults->colorBleedWidth;
    }

    params.brightness        = brightness;
    params.scanlineIntensity = scanEn  ? scanInt : 0.0f;
    params.bloomRadius       = bloomEn ? bloomR  : 0.0f;
    params.bloomStrength     = bloomEn ? bloomS  : 0.0f;
    params.colorBleedWidth   = bleedEn ? bleedW  : 0.0f;
    params.outputW           = (outputW > 0.0f) ? outputW : 1.0f;
    params.outputH           = (outputH > 0.0f) ? outputH : 1.0f;
    params.pad               = 0.0f;

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
    RECT  r = {};


    if (backBufferW <= 0 || backBufferH <= 0)
    {
        return r;
    }

    // Target 4:3 aspect. If window is wider, pillarbox; if narrower, letterbox.
    // Integer arithmetic to keep the bars pixel-aligned.
    int  w43 = (backBufferH * 4) / 3;
    if (w43 <= backBufferW)
    {
        // Pillarbox.
        int barX = (backBufferW - w43) / 2;
        r.left   = barX;
        r.top    = 0;
        r.right  = barX + w43;
        r.bottom = backBufferH;
    }
    else
    {
        // Letterbox.
        int h34 = (backBufferW * 3) / 4;
        int barY = (backBufferH - h34) / 2;
        r.left   = 0;
        r.top    = barY;
        r.right  = backBufferW;
        r.bottom = barY + h34;
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

HRESULT CrtPostProcess::CompilePixelShader (const char * src, ID3D11PixelShader ** out)
{
    HRESULT             hr     = S_OK;
    ComPtr<ID3DBlob>    blob;
    ComPtr<ID3DBlob>    errors;


    CPRA (out);
    *out = nullptr;

    hr = D3DCompile (src,
                     strlen (src),
                     "CrtPostProcess.hlsl",
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


    CPRA (device);
    CPRA (context);

    m_device  = device;
    m_context = context;

    // VS (shared).
    hr = D3DCompile (kVertexShaderSrc, strlen (kVertexShaderSrc),
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
    hr = CompilePixelShader (kPsBrightness, &m_psBrightness);  CHRA (hr);
    hr = CompilePixelShader (kPsScanlines,  &m_psScanlines);   CHRA (hr);
    hr = CompilePixelShader (kPsBloomH,     &m_psBloomH);      CHRA (hr);
    hr = CompilePixelShader (kPsBloomV,     &m_psBloomV);      CHRA (hr);
    hr = CompilePixelShader (kPsBloomComp,  &m_psBloomComp);   CHRA (hr);
    hr = CompilePixelShader (kPsColorBleed, &m_psColorBleed);  CHRA (hr);
    hr = CompilePixelShader (kPsCopy,       &m_psCopy);        CHRA (hr);

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
    ID3D11ShaderResourceView * srvs[2]       = { srv0, srv1 };
    ID3D11ShaderResourceView * nullSrvs[2]   = { nullptr, nullptr };
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
    m_context->PSSetShaderResources   (0, 2, srvs);
    m_context->PSSetConstantBuffers   (0, 1, cbs);

    m_context->DrawIndexed (6, 0, 0);

    // Detach SRVs so the just-written RT can be bound as an input on the
    // next pass without a D3D11 hazard warning.
    m_context->PSSetShaderResources (0, 2, nullSrvs);
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


    CPRA (srcSrv);
    CPRA (dstRtv);
    CPRA (m_device);
    CPRA (m_context);

    hr = EnsureSize (backBufferW, backBufferH);
    CHRA (hr);

    hr = UploadConstants (params);
    CHRA (hr);

    // Pass 1: brightness, srcEmulator -> ppMain[0], in the letterboxed
    // viewport. The pre-pass clear renders the bars black.
    DrawFullscreen (m_ppMainRtv[0].Get(),
                    srcSrv, nullptr,
                    m_psBrightness.Get(),
                    backBufferW, backBufferH,
                    &viewportRect);

    // Pass 2: scanlines, ppMain[0] -> ppMain[1].
    DrawFullscreen (m_ppMainRtv[1].Get(),
                    m_ppMainSrv[0].Get(), nullptr,
                    m_psScanlines.Get(),
                    backBufferW, backBufferH,
                    nullptr);

    // Pass 3: bloom horizontal, ppMain[1] -> ppBloom[0].
    DrawFullscreen (m_ppBloomRtv[0].Get(),
                    m_ppMainSrv[1].Get(), nullptr,
                    m_psBloomH.Get(),
                    backBufferW, backBufferH,
                    nullptr);

    // Pass 4: bloom vertical, ppBloom[0] -> ppBloom[1].
    DrawFullscreen (m_ppBloomRtv[1].Get(),
                    m_ppBloomSrv[0].Get(), nullptr,
                    m_psBloomV.Get(),
                    backBufferW, backBufferH,
                    nullptr);

    // Pass 5: bloom composite, (ppMain[1] + ppBloom[1]) -> ppMain[0].
    DrawFullscreen (m_ppMainRtv[0].Get(),
                    m_ppMainSrv[1].Get(),
                    m_ppBloomSrv[1].Get(),
                    m_psBloomComp.Get(),
                    backBufferW, backBufferH,
                    nullptr);

    // Pass 6: color bleed, ppMain[0] -> ppMain[1].
    DrawFullscreen (m_ppMainRtv[1].Get(),
                    m_ppMainSrv[0].Get(), nullptr,
                    m_psColorBleed.Get(),
                    backBufferW, backBufferH,
                    nullptr);

    // Pass 7: final copy, ppMain[1] -> back buffer.
    DrawFullscreen (dstRtv,
                    m_ppMainSrv[1].Get(), nullptr,
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
    m_psCopy.Reset();

    m_device  = nullptr;
    m_context = nullptr;
    m_width   = 0;
    m_height  = 0;
}
