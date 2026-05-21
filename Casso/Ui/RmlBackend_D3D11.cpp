#include "Pch.h"

#include "RmlBackend_D3D11.h"

#include "../Config/IFileSystem.h"

#include <wincodec.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "windowscodecs.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  HLSL source — kept in sync with Shaders/Ui/rml_textured.hlsl and
//  Shaders/Ui/rml_untextured.hlsl. Compiled at init via D3DCompile;
//  the .hlsl files are the canonical source for offline tooling but
//  are intentionally duplicated inline here so the backend has no
//  runtime dependency on a separate Shaders/ directory.
//
////////////////////////////////////////////////////////////////////////////////

static const char kTexturedHlsl[] =
"cbuffer Constants : register (b0)                                          \n"
"{                                                                          \n"
"    row_major float4x4 g_mvp;                                              \n"
"    float4             g_translation_pad;                                  \n"
"};                                                                         \n"
"                                                                           \n"
"struct VSIn  { float2 pos:POSITION; float4 color:COLOR; float2 uv:TEXCOORD0; }; \n"
"struct PSIn  { float4 sv_pos:SV_POSITION; float4 color:COLOR; float2 uv:TEXCOORD0; }; \n"
"                                                                           \n"
"Texture2D    g_texture : register (t0);                                    \n"
"SamplerState g_sampler : register (s0);                                    \n"
"                                                                           \n"
"PSIn VSMain (VSIn input)                                                   \n"
"{                                                                          \n"
"    PSIn output;                                                           \n"
"    float2 p = input.pos + g_translation_pad.xy;                           \n"
"    output.sv_pos = mul (float4 (p, 0.0f, 1.0f), g_mvp);                   \n"
"    output.color  = input.color;                                           \n"
"    output.uv     = input.uv;                                              \n"
"    return output;                                                         \n"
"}                                                                          \n"
"                                                                           \n"
"float4 PSMain (PSIn input) : SV_TARGET                                     \n"
"{                                                                          \n"
"    float4 t = g_texture.Sample (g_sampler, input.uv);                     \n"
"    return t * input.color;                                                \n"
"}                                                                          \n";


static const char kUntexturedHlsl[] =
"cbuffer Constants : register (b0)                                          \n"
"{                                                                          \n"
"    row_major float4x4 g_mvp;                                              \n"
"    float4             g_translation_pad;                                  \n"
"};                                                                         \n"
"                                                                           \n"
"struct VSIn  { float2 pos:POSITION; float4 color:COLOR; float2 uv:TEXCOORD0; }; \n"
"struct PSIn  { float4 sv_pos:SV_POSITION; float4 color:COLOR; };           \n"
"                                                                           \n"
"PSIn VSMain (VSIn input)                                                   \n"
"{                                                                          \n"
"    PSIn output;                                                           \n"
"    float2 p = input.pos + g_translation_pad.xy;                           \n"
"    output.sv_pos = mul (float4 (p, 0.0f, 1.0f), g_mvp);                   \n"
"    output.color  = input.color;                                           \n"
"    return output;                                                         \n"
"}                                                                          \n"
"                                                                           \n"
"float4 PSMain (PSIn input) : SV_TARGET                                     \n"
"{                                                                          \n"
"    return input.color;                                                    \n"
"}                                                                          \n";


// Layout matches Rml::Vertex (position : float2, colour : Colourb-Premultiplied
// 4xUNORM, tex_coord : float2). Offsets are derived from offsetof() in
// CompileGeometry to defend against future RmlUi-side struct changes.
static const D3D11_INPUT_ELEMENT_DESC kInputLayout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
      static_cast<UINT> (offsetof (Rml::Vertex, position)),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0,
      static_cast<UINT> (offsetof (Rml::Vertex, colour)),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
      static_cast<UINT> (offsetof (Rml::Vertex, tex_coord)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
};


struct ConstantBufferData
{
    float mvp[16];     // 64 bytes, row-major
    float trans[4];    // xy = translation, zw = pad — 16 bytes
};

static_assert (sizeof (ConstantBufferData) == 80,
               "ConstantBufferData must stay 80 bytes (must be a multiple of 16 for D3D11)");





////////////////////////////////////////////////////////////////////////////////
//
//  RmlBackend_D3D11
//
////////////////////////////////////////////////////////////////////////////////

RmlBackend_D3D11::RmlBackend_D3D11()
{
}



RmlBackend_D3D11::~RmlBackend_D3D11()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::Initialize (
    ID3D11Device         * pDevice,
    ID3D11DeviceContext  * pContext,
    UINT                   viewportWidthPx,
    UINT                   viewportHeightPx,
    IFileSystem          * pFs)
{
    HRESULT hr = S_OK;

    CPRAEx (pDevice,  E_INVALIDARG);
    CPRAEx (pContext, E_INVALIDARG);

    m_device     = pDevice;
    m_context    = pContext;
    m_fs         = pFs;
    m_viewportW  = (viewportWidthPx  > 0) ? viewportWidthPx  : 1;
    m_viewportH  = (viewportHeightPx > 0) ? viewportHeightPx : 1;

    hr = InitializeShaders();
    CHRA (hr);

    hr = InitializePipelineStates();
    CHRA (hr);

    hr = InitializeConstantBuffer();
    CHRA (hr);

    hr = InitializeWhiteTexture();
    CHRA (hr);

    hr = RebuildProjectionMatrix();
    CHRA (hr);

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

void RmlBackend_D3D11::Shutdown()
{
    m_geometries.clear();
    m_textures.clear();

    m_whiteSrv.Reset();
    m_constantBuffer.Reset();
    m_sampler.Reset();
    m_depthStencilState.Reset();
    m_rasterizerState.Reset();
    m_blendState.Reset();
    m_inputLayout.Reset();
    m_psUntextured.Reset();
    m_vsUntextured.Reset();
    m_psTextured.Reset();
    m_vsTextured.Reset();

    m_device  = nullptr;
    m_context = nullptr;
    m_fs      = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompileHlsl
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::CompileHlsl (
    const char         * src,
    size_t               srcLen,
    const char         * entry,
    const char         * profile,
    ComPtr<ID3DBlob>   & outBlob)
{
    HRESULT             hr     = S_OK;
    ComPtr<ID3DBlob>    errors;

    hr = D3DCompile (src,
                     srcLen,
                     "RmlBackend_D3D11.hlsl",
                     nullptr,
                     nullptr,
                     entry,
                     profile,
                     0,
                     0,
                     &outBlob,
                     &errors);

    if (FAILED (hr) && errors != nullptr)
    {
        OutputDebugStringA (static_cast<const char *> (errors->GetBufferPointer()));
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeShaders
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::InitializeShaders()
{
    HRESULT             hr     = S_OK;
    ComPtr<ID3DBlob>    vsBlob;
    ComPtr<ID3DBlob>    psBlob;

    // Textured
    hr = CompileHlsl (kTexturedHlsl, sizeof (kTexturedHlsl) - 1, "VSMain", "vs_4_0", vsBlob);
    CHRA (hr);

    hr = CompileHlsl (kTexturedHlsl, sizeof (kTexturedHlsl) - 1, "PSMain", "ps_4_0", psBlob);
    CHRA (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(),
                                       nullptr,
                                       &m_vsTextured);
    CHRA (hr);

    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer(),
                                      psBlob->GetBufferSize(),
                                      nullptr,
                                      &m_psTextured);
    CHRA (hr);

    hr = m_device->CreateInputLayout (kInputLayout,
                                      static_cast<UINT> (std::size (kInputLayout)),
                                      vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(),
                                      &m_inputLayout);
    CHRA (hr);

    // Untextured
    vsBlob.Reset();
    psBlob.Reset();

    hr = CompileHlsl (kUntexturedHlsl, sizeof (kUntexturedHlsl) - 1, "VSMain", "vs_4_0", vsBlob);
    CHRA (hr);

    hr = CompileHlsl (kUntexturedHlsl, sizeof (kUntexturedHlsl) - 1, "PSMain", "ps_4_0", psBlob);
    CHRA (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(),
                                       nullptr,
                                       &m_vsUntextured);
    CHRA (hr);

    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer(),
                                      psBlob->GetBufferSize(),
                                      nullptr,
                                      &m_psUntextured);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializePipelineStates
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::InitializePipelineStates()
{
    HRESULT                  hr  = S_OK;
    D3D11_BLEND_DESC         bd  = {};
    D3D11_RASTERIZER_DESC    rd  = {};
    D3D11_DEPTH_STENCIL_DESC dd  = {};
    D3D11_SAMPLER_DESC       sd  = {};

    // Premultiplied-alpha source-over blend
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState (&bd, &m_blendState);
    CHRA (hr);

    // Rasterizer: cull none, scissor always on (toggled via dimensions).
    rd.FillMode              = D3D11_FILL_SOLID;
    rd.CullMode              = D3D11_CULL_NONE;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable       = TRUE;
    rd.ScissorEnable         = TRUE;
    rd.MultisampleEnable     = FALSE;
    rd.AntialiasedLineEnable = FALSE;

    hr = m_device->CreateRasterizerState (&rd, &m_rasterizerState);
    CHRA (hr);

    // Depth/stencil: disabled
    dd.DepthEnable    = FALSE;
    dd.StencilEnable  = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

    hr = m_device->CreateDepthStencilState (&dd, &m_depthStencilState);
    CHRA (hr);

    // Sampler: bilinear, clamp.
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState (&sd, &m_sampler);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeConstantBuffer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::InitializeConstantBuffer()
{
    HRESULT            hr  = S_OK;
    D3D11_BUFFER_DESC  bd  = {};

    bd.ByteWidth      = sizeof (ConstantBufferData);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer (&bd, nullptr, &m_constantBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeWhiteTexture
//
//  RmlUi's clip-mask path and a few decorators bind the "no texture"
//  handle (0) to RenderGeometry. We catch that case at draw time and
//  bind the untextured pixel shader, so this 1x1 white texture is
//  only ever used as a defensive fallback if a caller hands us a
//  non-zero TextureHandle that has since been released.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::InitializeWhiteTexture()
{
    HRESULT                  hr        = S_OK;
    D3D11_TEXTURE2D_DESC     td        = {};
    D3D11_SUBRESOURCE_DATA   data      = {};
    ComPtr<ID3D11Texture2D>  tex;
    UINT                     whitePix  = 0xFFFFFFFFu;  // premultiplied RGBA white

    td.Width            = 1;
    td.Height           = 1;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    data.pSysMem     = &whitePix;
    data.SysMemPitch = sizeof (UINT);

    hr = m_device->CreateTexture2D (&td, &data, &tex);
    CHRA (hr);

    hr = m_device->CreateShaderResourceView (tex.Get(), nullptr, &m_whiteSrv);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Resize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::Resize (UINT widthPx, UINT heightPx)
{
    HRESULT hr = S_OK;

    if (widthPx == 0)  { widthPx  = 1; }
    if (heightPx == 0) { heightPx = 1; }

    m_viewportW = widthPx;
    m_viewportH = heightPx;

    hr = RebuildProjectionMatrix();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildProjectionMatrix
//
//  Top-left origin, pixels, row-major. Matches RmlUi's coordinate
//  expectations exactly. The matrix maps (x, y) in pixels to clip
//  space ([-1,1] x [-1,1]) with y flipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::RebuildProjectionMatrix()
{
    const float w = static_cast<float> (m_viewportW);
    const float h = static_cast<float> (m_viewportH);

    // Row-major orthographic projection: pixel (0,0) → clip (-1, +1).
    m_projection = Rml::Matrix4f::FromRows (
        Rml::Vector4f ( 2.0f / w,  0.0f,      0.0f, -1.0f),
        Rml::Vector4f ( 0.0f,     -2.0f / h,  0.0f,  1.0f),
        Rml::Vector4f ( 0.0f,      0.0f,      1.0f,  0.0f),
        Rml::Vector4f ( 0.0f,      0.0f,      0.0f,  1.0f));

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateConstantBuffer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::UpdateConstantBuffer (Rml::Vector2f translation)
{
    HRESULT                  hr      = S_OK;
    D3D11_MAPPED_SUBRESOURCE mapped  = {};
    ConstantBufferData       cb      = {};

    Rml::Matrix4f finalMatrix = m_projection;

    if (m_hasTransform)
    {
        finalMatrix = m_projection * m_userTransform;
    }

    // Walk row-by-row, agnostic of the underlying storage mode.
    for (int r = 0; r < 4; ++r)
    {
        auto row = finalMatrix.GetRow (r);
        for (int c = 0; c < 4; ++c)
        {
            cb.mvp[r * 4 + c] = row[c];
        }
    }

    cb.trans[0] = translation.x;
    cb.trans[1] = translation.y;
    cb.trans[2] = 0.0f;
    cb.trans[3] = 0.0f;

    hr = m_context->Map (m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHRA (hr);

    memcpy (mapped.pData, &cb, sizeof (cb));
    m_context->Unmap (m_constantBuffer.Get(), 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginFrame
//
//  Bind the pipeline state RmlUi expects. We do NOT snapshot the
//  prior state because D3DRenderer rebinds everything from scratch
//  in UploadAndPresent before its next draw — leaving our state
//  bound at swap-chain Present time is harmless.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::BeginFrame()
{
    if (m_context == nullptr)
    {
        return E_FAIL;
    }

    m_inFrame = true;

    const FLOAT          blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT       vp             = {};

    vp.Width    = static_cast<float> (m_viewportW);
    vp.Height   = static_cast<float> (m_viewportH);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    m_context->IASetInputLayout       (m_inputLayout.Get());
    m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->OMSetBlendState        (m_blendState.Get(), blendFactor, 0xFFFFFFFFu);
    m_context->OMSetDepthStencilState (m_depthStencilState.Get(), 0);
    m_context->RSSetState             (m_rasterizerState.Get());
    m_context->VSSetConstantBuffers   (0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetSamplers          (0, 1, m_sampler.GetAddressOf());

    // Default to "scissor = full viewport" so any geometry drawn
    // before RmlUi calls SetScissorRegion is not arbitrarily clipped.
    D3D11_RECT full = { 0, 0, static_cast<LONG> (m_viewportW), static_cast<LONG> (m_viewportH) };
    m_context->RSSetScissorRects (1, &full);
    m_scissorEnabled  = false;
    m_scissorRect     = full;

    // Reset accumulated transform so each frame starts in identity.
    m_hasTransform = false;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::EndFrame()
{
    m_inFrame = false;
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompileGeometry
//
////////////////////////////////////////////////////////////////////////////////

Rml::CompiledGeometryHandle RmlBackend_D3D11::CompileGeometry (
    Rml::Span<const Rml::Vertex>  vertices,
    Rml::Span<const int>          indices)
{
    if (m_device == nullptr || vertices.empty() || indices.empty())
    {
        return 0;
    }

    HRESULT                 hr        = S_OK;
    D3D11_BUFFER_DESC       vbd       = {};
    D3D11_BUFFER_DESC       ibd       = {};
    D3D11_SUBRESOURCE_DATA  vData     = {};
    D3D11_SUBRESOURCE_DATA  iData     = {};
    auto                    geometry  = std::make_unique<CompiledGeometry> ();

    vbd.ByteWidth = static_cast<UINT> (vertices.size() * sizeof (Rml::Vertex));
    vbd.Usage     = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    vData.pSysMem = vertices.data();

    hr = m_device->CreateBuffer (&vbd, &vData, &geometry->vb);

    if (FAILED (hr))
    {
        return 0;
    }

    ibd.ByteWidth = static_cast<UINT> (indices.size() * sizeof (int));
    ibd.Usage     = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    iData.pSysMem = indices.data();

    hr = m_device->CreateBuffer (&ibd, &iData, &geometry->ib);

    if (FAILED (hr))
    {
        return 0;
    }

    geometry->indexCount = static_cast<UINT> (indices.size());

    uintptr_t handle = reinterpret_cast<uintptr_t> (geometry.get());

    m_geometries.emplace (handle, std::move (geometry));

    return static_cast<Rml::CompiledGeometryHandle> (handle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderGeometry
//
////////////////////////////////////////////////////////////////////////////////

void RmlBackend_D3D11::RenderGeometry (
    Rml::CompiledGeometryHandle  geometry,
    Rml::Vector2f                translation,
    Rml::TextureHandle           texture)
{
    if (m_context == nullptr)
    {
        return;
    }

    auto it = m_geometries.find (static_cast<uintptr_t> (geometry));

    if (it == m_geometries.end())
    {
        return;
    }

    const CompiledGeometry & geo = *it->second;

    UpdateConstantBuffer (translation);

    UINT stride = sizeof (Rml::Vertex);
    UINT offset = 0;

    m_context->IASetVertexBuffers (0, 1, geo.vb.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer   (geo.ib.Get(), DXGI_FORMAT_R32_UINT, 0);

    if (texture != 0)
    {
        ID3D11ShaderResourceView * srv = reinterpret_cast<ID3D11ShaderResourceView *> (texture);

        m_context->VSSetShader          (m_vsTextured.Get(), nullptr, 0);
        m_context->PSSetShader          (m_psTextured.Get(), nullptr, 0);
        m_context->PSSetShaderResources (0, 1, &srv);
    }
    else
    {
        m_context->VSSetShader (m_vsUntextured.Get(), nullptr, 0);
        m_context->PSSetShader (m_psUntextured.Get(), nullptr, 0);
    }

    m_context->DrawIndexed (geo.indexCount, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGeometry
//
////////////////////////////////////////////////////////////////////////////////

void RmlBackend_D3D11::ReleaseGeometry (Rml::CompiledGeometryHandle geometry)
{
    m_geometries.erase (static_cast<uintptr_t> (geometry));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnableScissorRegion / SetScissorRegion
//
////////////////////////////////////////////////////////////////////////////////

void RmlBackend_D3D11::EnableScissorRegion (bool enable)
{
    m_scissorEnabled = enable;

    if (m_context == nullptr)
    {
        return;
    }

    D3D11_RECT rect = enable
        ? m_scissorRect
        : D3D11_RECT { 0, 0,
                       static_cast<LONG> (m_viewportW),
                       static_cast<LONG> (m_viewportH) };

    m_context->RSSetScissorRects (1, &rect);

    ++m_scissorCallCount;
    m_lastScissorEnabled = enable;
    m_lastScissorRect    = rect;
}



void RmlBackend_D3D11::SetScissorRegion (Rml::Rectanglei region)
{
    m_scissorRect = D3D11_RECT
    {
        region.Left   (),
        region.Top    (),
        region.Right  (),
        region.Bottom()
    };

    if (m_context == nullptr)
    {
        return;
    }

    m_context->RSSetScissorRects (1, &m_scissorRect);

    ++m_scissorCallCount;
    m_lastScissorEnabled = m_scissorEnabled;
    m_lastScissorRect    = m_scissorRect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTransform
//
////////////////////////////////////////////////////////////////////////////////

void RmlBackend_D3D11::SetTransform (const Rml::Matrix4f * transform)
{
    if (transform == nullptr)
    {
        m_hasTransform = false;
        m_userTransform = Rml::Matrix4f::Identity();
    }
    else
    {
        m_hasTransform  = true;
        m_userTransform = *transform;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadImageBytesViaWic
//
//  Decode a byte buffer (PNG/JPG/BMP/etc.) into 32-bit premultiplied
//  RGBA via the Windows Imaging Component. Requires COM to be
//  initialized on the calling thread (the emulator main thread does
//  this in Main.cpp).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlBackend_D3D11::LoadImageBytesViaWic (
    const std::vector<Byte>  & bytes,
    std::vector<Byte>        & outRgbaPremultiplied,
    UINT                     & outWidth,
    UINT                     & outHeight)
{
    HRESULT                            hr        = S_OK;
    ComPtr<IWICImagingFactory>         factory;
    ComPtr<IWICStream>                 stream;
    ComPtr<IWICBitmapDecoder>          decoder;
    ComPtr<IWICBitmapFrameDecode>      frame;
    ComPtr<IWICFormatConverter>        converter;
    size_t                             stride    = 0;
    size_t                             total     = 0;

    outRgbaPremultiplied.clear();
    outWidth  = 0;
    outHeight = 0;

    CBREx (!bytes.empty(), E_INVALIDARG);

    hr = CoCreateInstance (CLSID_WICImagingFactory,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&factory));
    CHRA (hr);

    hr = factory->CreateStream (&stream);
    CHR (hr);

    // The buffer pointed to by `bytes` must outlive the stream; we
    // copy out via CopyPixels below before returning so this is safe.
    hr = stream->InitializeFromMemory (const_cast<BYTE *> (bytes.data()),
                                       static_cast<DWORD> (bytes.size()));
    CHR (hr);

    hr = factory->CreateDecoderFromStream (stream.Get(),
                                           nullptr,
                                           WICDecodeMetadataCacheOnDemand,
                                           &decoder);
    CHR (hr);

    hr = decoder->GetFrame (0, &frame);
    CHR (hr);

    hr = frame->GetSize (&outWidth, &outHeight);
    CHR (hr);

    hr = factory->CreateFormatConverter (&converter);
    CHR (hr);

    // 32bppPRGBA is RGBA8888 with premultiplied alpha — exactly what
    // RmlUi's RenderInterface contract requires of GenerateTexture
    // sources.
    hr = converter->Initialize (frame.Get(),
                                GUID_WICPixelFormat32bppPRGBA,
                                WICBitmapDitherTypeNone,
                                nullptr,
                                0.0,
                                WICBitmapPaletteTypeMedianCut);
    CHR (hr);

    stride = static_cast<size_t> (outWidth) * 4;
    total  = stride * outHeight;

    outRgbaPremultiplied.resize (total);

    hr = converter->CopyPixels (nullptr,
                                static_cast<UINT> (stride),
                                static_cast<UINT> (total),
                                outRgbaPremultiplied.data());
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        outRgbaPremultiplied.clear();
        outWidth  = 0;
        outHeight = 0;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterTexture
//
////////////////////////////////////////////////////////////////////////////////

Rml::TextureHandle RmlBackend_D3D11::RegisterTexture (ComPtr<ID3D11ShaderResourceView> srv)
{
    if (srv == nullptr)
    {
        return 0;
    }

    uintptr_t handle = reinterpret_cast<uintptr_t> (srv.Get());

    m_textures.emplace (handle, std::move (srv));

    return static_cast<Rml::TextureHandle> (handle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadTexture
//
////////////////////////////////////////////////////////////////////////////////

Rml::TextureHandle RmlBackend_D3D11::LoadTexture (
    Rml::Vector2i        & texture_dimensions,
    const Rml::String    & source)
{
    texture_dimensions = { 0, 0 };

    if (m_device == nullptr || m_fs == nullptr)
    {
        return 0;
    }

    // RmlUi gives us UTF-8 paths. Convert to wide for IFileSystem.
    int wlen = MultiByteToWideChar (CP_UTF8, 0, source.c_str(), -1, nullptr, 0);

    if (wlen <= 0)
    {
        return 0;
    }

    std::wstring wpath;
    wpath.resize (static_cast<size_t> (wlen - 1));
    MultiByteToWideChar (CP_UTF8, 0, source.c_str(), -1, wpath.data(), wlen);

    std::string raw;

    if (FAILED (m_fs->ReadAllText (wpath, raw)))
    {
        return 0;
    }

    std::vector<Byte> bytes (raw.begin(), raw.end());
    std::vector<Byte> rgba;
    UINT              w = 0;
    UINT              h = 0;

    if (FAILED (LoadImageBytesViaWic (bytes, rgba, w, h)))
    {
        return 0;
    }

    // Re-use the GenerateTexture path now that we have pixels.
    Rml::Span<const Rml::byte> span (
        reinterpret_cast<const Rml::byte *> (rgba.data()),
        rgba.size());

    Rml::TextureHandle h2 = GenerateTexture (
        span,
        Rml::Vector2i (static_cast<int> (w), static_cast<int> (h)));

    if (h2 != 0)
    {
        texture_dimensions = { static_cast<int> (w), static_cast<int> (h) };
    }

    return h2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GenerateTexture
//
////////////////////////////////////////////////////////////////////////////////

Rml::TextureHandle RmlBackend_D3D11::GenerateTexture (
    Rml::Span<const Rml::byte>  source,
    Rml::Vector2i               source_dimensions)
{
    if (m_device == nullptr || source.empty() ||
        source_dimensions.x <= 0 || source_dimensions.y <= 0)
    {
        return 0;
    }

    HRESULT                  hr       = S_OK;
    D3D11_TEXTURE2D_DESC     td       = {};
    D3D11_SUBRESOURCE_DATA   data     = {};
    ComPtr<ID3D11Texture2D>  tex;
    ComPtr<ID3D11ShaderResourceView>  srv;

    td.Width            = static_cast<UINT> (source_dimensions.x);
    td.Height           = static_cast<UINT> (source_dimensions.y);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    data.pSysMem     = source.data();
    data.SysMemPitch = static_cast<UINT> (source_dimensions.x) * 4;

    hr = m_device->CreateTexture2D (&td, &data, &tex);

    if (FAILED (hr))
    {
        return 0;
    }

    hr = m_device->CreateShaderResourceView (tex.Get(), nullptr, &srv);

    if (FAILED (hr))
    {
        return 0;
    }

    return RegisterTexture (std::move (srv));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseTexture
//
////////////////////////////////////////////////////////////////////////////////

void RmlBackend_D3D11::ReleaseTexture (Rml::TextureHandle texture)
{
    if (texture == 0)
    {
        return;
    }

    m_textures.erase (static_cast<uintptr_t> (texture));
}
