#include "Pch.h"

#include "CrtPostProcess.h"

#include "Config/CrtPresets.h"

#pragma comment(lib, "d3dcompiler.lib")




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
    constexpr UINT  s_kMaxBoundPsSrvSlots = 2;

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
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float4 c = tex.Sample (sam, i.uv);\n"
        "    c.rgb = saturate (((c.rgb - 0.5) * g_contrast + 0.5) * g_brightness);\n"
        "    return c;\n"
        "}\n";

    // See Shaders/CRT/scanlines.hlsl
    constexpr const char *  kPsScanlines =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "static const float kNativeScanlines = 192.0;\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float4 c       = tex.Sample (sam, i.uv);\n"
        "    float  linePos = i.uv.y * kNativeScanlines;\n"
        "    float  gap     = sin (linePos * 3.14159265);\n"
        "    float  bright  = gap * gap;\n"
        "    float  darken  = lerp (1.0 - g_scanlineIntensity, 1.0, bright);\n"
        "    c.rgb *= darken;\n"
        "    return c;\n"
        "}\n";

    // See Shaders/CRT/bloom_h.hlsl
    constexpr const char *  kPsBloomH =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
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
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
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
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
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
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
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

    // Persistence pass. Mixes current frame (t0) with the previous
    // frame's post-bloom output (t1) using max() instead of lerp() so
    // bright pixels snap on immediately but only fade gradually --
    // matches real phosphor physics (electrons hit -> phosphor lights
    // up instantly, then decays slowly). persistence==0 disables (pass-
    // through); persistence==0.8 makes amber-style afterglow visible.
    constexpr const char *  kPsPersistence =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
        "Texture2D    texCurrent : register(t0);\n"
        "Texture2D    texPrev    : register(t1);\n"
        "SamplerState sam        : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float3 cur  = texCurrent.Sample (sam, i.uv).rgb;\n"
        "    float3 prev = texPrev.Sample    (sam, i.uv).rgb;\n"
        // The carry-over RT is 8-bit UNORM, so a pure prev*decay
        // multiply quantizes back to the same byte once it gets
        // small enough (1/255 * 0.8 rounds to 1/255 -- ghost forever).
        // Subtracting ~1.5/255 after the multiply guarantees the
        // value crosses to zero in finite time even at high decay.
        "    float3 decayed = max (prev * saturate (g_persistence) - (1.5 / 255.0), 0.0);\n"
        "    return float4 (max (cur, decayed), 1.0);\n"
        "}\n";

    // Final gamma pass. Applies pow(rgb, 1/gamma) so content authored
    // for a ~1.8 CRT looks right on a 2.2 sRGB display. Trivial cost;
    // single ALU op per channel.
    constexpr const char *  kPsGamma =
        "cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };\n"
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    float4 c = tex.Sample (sam, i.uv);\n"
        "    float  invGamma = 1.0 / max (g_gamma, 0.1);\n"
        "    c.rgb = pow (saturate (c.rgb), invGamma);\n"
        "    return c;\n"
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

HRESULT CrtPostProcess::CompilePixelShader (const char * src, ID3D11PixelShader ** out)
{
    HRESULT             hr     = S_OK;
    ComPtr<ID3DBlob>    blob;
    ComPtr<ID3DBlob>    errors;



    CBRAEx (out, E_INVALIDARG);
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


    CBRAEx (device, E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

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
    hr = CompilePixelShader (kPsBrightness,  &m_psBrightness);   CHRA (hr);
    hr = CompilePixelShader (kPsScanlines,   &m_psScanlines);    CHRA (hr);
    hr = CompilePixelShader (kPsBloomH,      &m_psBloomH);       CHRA (hr);
    hr = CompilePixelShader (kPsBloomV,      &m_psBloomV);       CHRA (hr);
    hr = CompilePixelShader (kPsBloomComp,   &m_psBloomComp);    CHRA (hr);
    hr = CompilePixelShader (kPsColorBleed,  &m_psColorBleed);   CHRA (hr);
    hr = CompilePixelShader (kPsPersistence, &m_psPersistence);  CHRA (hr);
    hr = CompilePixelShader (kPsGamma,       &m_psGamma);        CHRA (hr);
    hr = CompilePixelShader (kPsCopy,        &m_psCopy);         CHRA (hr);

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



    CBRAEx (srcSrv, E_INVALIDARG);
    CBRAEx (dstRtv, E_INVALIDARG);
    CBRA (m_device);
    CBRA (m_context);

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

    // Pass 7: persistence. Current frame (ppMain[1]) mixed with the
    // PREVIOUS frame's post-persistence result (m_persistenceTex) via
    // max(current, prev*decay). Output lands in ppMain[0] AND we
    // capture a copy into m_persistenceTex for next frame. First frame
    // after EnsureSize has no valid prior, so we just pass through
    // (the persistence cbuffer field stays 0 until primed).
    if (params.persistence > 0.0f && m_persistencePrimed)
    {
        DrawFullscreen (m_ppMainRtv[0].Get(),
                        m_ppMainSrv[1].Get(),
                        m_persistenceSrv.Get(),
                        m_psPersistence.Get(),
                        backBufferW, backBufferH,
                        nullptr);
    }
    else
    {
        // Pass-through: copy ppMain[1] -> ppMain[0] so subsequent
        // passes can keep their input/output convention.
        DrawFullscreen (m_ppMainRtv[0].Get(),
                        m_ppMainSrv[1].Get(), nullptr,
                        m_psCopy.Get(),
                        backBufferW, backBufferH,
                        nullptr);
    }

    // Capture the persistence-pass result into the carry-over RT
    // before the destructive gamma pass writes to ppMain[1]. After
    // this CopyResource the next frame can sample the previous final
    // frame's post-color result.
    if (m_persistenceTex && m_ppMainTex[0])
    {
        m_context->CopyResource (m_persistenceTex.Get(), m_ppMainTex[0].Get());
        m_persistencePrimed = true;
    }

    // Pass 8: gamma, ppMain[0] -> ppMain[1].
    DrawFullscreen (m_ppMainRtv[1].Get(),
                    m_ppMainSrv[0].Get(), nullptr,
                    m_psGamma.Get(),
                    backBufferW, backBufferH,
                    nullptr);

    // Pass 9: final copy, ppMain[1] -> back buffer.
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
