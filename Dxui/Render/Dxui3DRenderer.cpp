#include "Pch.h"

#include "Render/Dxui3DRenderer.h"




// Row-vector convention (clip = v * M) with a row_major cbuffer matrix, so
// the CPU-side float[16] goes into the constant buffer untransposed.
static const char s_kVertexShaderSrc[] =
    "cbuffer Mvp : register(b0) { row_major float4x4 mvp; };\n"
    "struct VSIn  { float3 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };\n"
    "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };\n"
    "VSOut main (VSIn input)\n"
    "{\n"
    "    VSOut output;\n"
    "    output.pos = mul (float4 (input.pos, 1.0f), mvp);\n"
    "    output.uv  = input.uv;\n"
    "    output.col = input.col;\n"
    "    return output;\n"
    "}\n";


static const char s_kPixelShaderSrc[] =
    "Texture2D    tex  : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };\n"
    "float4 main (PSIn input) : SV_TARGET\n"
    "{\n"
    "    return tex.Sample (samp, input.uv) * input.col;\n"
    "}\n";





////////////////////////////////////////////////////////////////////////////////
//
//  ~Dxui3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

Dxui3DRenderer::~Dxui3DRenderer()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Brings up the 3D renderer against a borrowed device and context: shaders,
//  pipeline state, and the 1x1 white texture.
//
//  That white texture is what lets ONE shader pair serve both textured and
//  untextured geometry. Untextured draws sample it, so the sample is always
//  opaque white and the vertex tint alone becomes the surface color -- no
//  second shader, no branch in the pixel shader, and no state swap between
//  the two cases.
//
//  It is created IMMUTABLE, since a single white pixel never changes and the
//  driver is free to place it wherever it likes.
//
//  A failure anywhere calls Shutdown before returning, so a half-built
//  renderer is never left for a caller to discover -- the object is either
//  fully usable or fully empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::Initialize (ID3D11Device * device, ID3D11DeviceContext * context)
{
    HRESULT   hr = S_OK;



    CBREx (device != nullptr && context != nullptr, E_INVALIDARG);

    m_device  = device;
    m_context = context;

    hr = CreateShaders();
    CHR (hr);

    hr = CreatePipelineState();
    CHR (hr);

    // 1x1 opaque white: untextured geometry samples it so the vertex tint IS
    // the surface color, and the one shader pair covers both cases.
    {
        uint32_t                 white   = 0xFFFFFFFFu;
        D3D11_TEXTURE2D_DESC     desc    = {};
        D3D11_SUBRESOURCE_DATA   initial = {};

        desc.Width            = 1;
        desc.Height           = 1;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        initial.pSysMem     = &white;
        initial.SysMemPitch = sizeof (white);

        hr = m_device->CreateTexture2D (&desc, &initial, m_whiteTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateShaderResourceView (m_whiteTex.Get(), nullptr, m_whiteSrv.GetAddressOf());
        CHR (hr);
    }

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
//  Releases every GPU resource and returns the renderer to its pre-Initialize
//  state.
//
//  The cached SIZES are cleared alongside the resources, and that is the part
//  that matters. This object is reused across a device reset, and the growth
//  checks in UpdateContentTexture and the depth-buffer path compare against
//  those cached dimensions -- leaving them set would let a later frame decide
//  a texture it no longer owns is already big enough.
//
//  Device and context are borrowed, not owned, so they are dropped rather than
//  released.
//
//  Safe to call on a partially built renderer, which is what lets Initialize
//  use it as its own failure path.
//
////////////////////////////////////////////////////////////////////////////////

void Dxui3DRenderer::Shutdown()
{
    m_vs.Reset();
    m_ps.Reset();
    m_layout.Reset();
    m_vertexBuffer.Reset();
    m_mvpBuffer.Reset();
    m_blendState.Reset();
    m_rasterState.Reset();
    m_depthState.Reset();
    m_depthStateTest.Reset();
    m_sampler.Reset();
    m_contentTex.Reset();
    m_contentSrv.Reset();
    m_whiteTex.Reset();
    m_whiteSrv.Reset();
    m_depthTex.Reset();
    m_depthDsv.Reset();

    m_vertexBufferCapacity = 0;
    m_contentWidth         = 0;
    m_contentHeight        = 0;
    m_depthWidth           = 0;
    m_depthHeight          = 0;
    m_device               = nullptr;
    m_context              = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateShaders
//
//  Compiles the single vertex / pixel shader pair and builds the input layout
//  that matches it.
//
//  One pair covers everything this renderer draws. The vertex format carries
//  position, texture coordinate, and per-vertex color, and the pixel shader
//  simply multiplies the sample by the color -- which is why untextured
//  geometry can sample the 1x1 white texture and get its vertex tint back
//  unchanged, with no second shader to maintain.
//
//  Shader source is embedded as string literals rather than as resources,
//  because these two are small and belong with the vertex struct they have to
//  agree with.
//
//  The input layout is validated against the compiled VERTEX SHADER BLOB, so a
//  mismatch between the struct and the shader's expected signature fails here
//  at startup rather than as garbage geometry at draw time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::CreateShaders()
{
    HRESULT            hr      = S_OK;
    ComPtr<ID3DBlob>   vsBlob;
    ComPtr<ID3DBlob>   psBlob;
    ComPtr<ID3DBlob>   errors;



    D3D11_INPUT_ELEMENT_DESC   layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = D3DCompile (s_kVertexShaderSrc, sizeof (s_kVertexShaderSrc) - 1,
                     nullptr, nullptr, nullptr, "main", "vs_4_0",
                     0, 0, vsBlob.GetAddressOf(), errors.GetAddressOf());
    CHR (hr);

    hr = D3DCompile (s_kPixelShaderSrc, sizeof (s_kPixelShaderSrc) - 1,
                     nullptr, nullptr, nullptr, "main", "ps_4_0",
                     0, 0, psBlob.GetAddressOf(), errors.ReleaseAndGetAddressOf());
    CHR (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                       nullptr, m_vs.GetAddressOf());
    CHR (hr);

    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                      nullptr, m_ps.GetAddressOf());
    CHR (hr);

    hr = m_device->CreateInputLayout (layout, ARRAYSIZE (layout),
                                      vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                      m_layout.GetAddressOf());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreatePipelineState
//
//  Builds the fixed-function state objects: blend, rasterizer, the two depth
//  modes, the sampler, and the MVP constant buffer.
//
//  Blending is PREMULTIPLIED source-over, chosen to match DxuiPainter exactly.
//  3D scene pixels layer into the same surface as the panel-tree paint, and a
//  different blend equation would make a translucent scene composite
//  differently from every other translucent thing in the UI.
//
//  Culling is off. The paper curl's far side is legitimately visible from
//  behind, so back-face culling would punch holes in it, and these meshes are
//  far too small for culling to be worth anything.
//
//  Two depth modes exist because there are two kinds of caller. Hand-built
//  batches arrive already ordered back to front and want depth OFF, painter's-
//  algorithm style; loaded meshes arrive in arbitrary triangle order and need
//  a real LESS test with depth writes. Neither mode works for the other case,
//  which is why both are created up front and chosen per draw.
//
//  The sampler clamps so a UV that reaches past an edge repeats the edge
//  texel instead of wrapping content in from the opposite side.
//
//  The MVP buffer is DYNAMIC: it is rewritten per draw, since each batch
//  carries its own transform.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::CreatePipelineState()
{
    HRESULT   hr = S_OK;



    // Premultiplied source-over -- identical compositing to DxuiPainter, so
    // scene pixels layer with the panel-tree paint without surprises.
    {
        D3D11_BLEND_DESC   blend = {};

        blend.RenderTarget[0].BlendEnable           = TRUE;
        blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState (&blend, m_blendState.GetAddressOf());
        CHR (hr);
    }

    // No cull: the paper curl's far side is legitimately visible from behind,
    // and the meshes are far too small for culling to matter.
    {
        D3D11_RASTERIZER_DESC   raster = {};

        raster.FillMode        = D3D11_FILL_SOLID;
        raster.CullMode        = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;

        hr = m_device->CreateRasterizerState (&raster, m_rasterState.GetAddressOf());
        CHR (hr);
    }

    // Two depth modes: off for hand-ordered painter's-algorithm batches, and
    // LESS test+write for loaded meshes whose triangles arrive unordered.
    {
        D3D11_DEPTH_STENCIL_DESC   depth = {};

        depth.DepthEnable = FALSE;

        hr = m_device->CreateDepthStencilState (&depth, m_depthState.GetAddressOf());
        CHR (hr);

        depth.DepthEnable    = TRUE;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth.DepthFunc      = D3D11_COMPARISON_LESS;

        hr = m_device->CreateDepthStencilState (&depth, m_depthStateTest.GetAddressOf());
        CHR (hr);
    }

    {
        D3D11_SAMPLER_DESC   samp = {};

        samp.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samp.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.MaxLOD         = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState (&samp, m_sampler.GetAddressOf());
        CHR (hr);
    }

    {
        D3D11_BUFFER_DESC   cb = {};

        cb.ByteWidth      = 16 * sizeof (float);
        cb.Usage          = D3D11_USAGE_DYNAMIC;
        cb.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer (&cb, nullptr, m_mvpBuffer.GetAddressOf());
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureVertexBuffer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::EnsureVertexBuffer (size_t requiredVerts)
{
    HRESULT             hr   = S_OK;
    D3D11_BUFFER_DESC   desc = {};



    BAIL_OUT_IF (requiredVerts <= m_vertexBufferCapacity && m_vertexBuffer != nullptr, S_OK);

    m_vertexBuffer.Reset();

    desc.ByteWidth      = (UINT) (requiredVerts * sizeof (Vertex));
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer (&desc, nullptr, m_vertexBuffer.GetAddressOf());
    CHR (hr);

    m_vertexBufferCapacity = requiredVerts;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateContentTexture
//
//  Uploads a BGRA image for textured geometry to sample, recreating the
//  texture only when the dimensions change.
//
//  The size test is what makes this cheap to call every frame: a page preview
//  that keeps its dimensions re-uses the same texture and pays only the
//  upload, while a resize gets a fresh one.
//
//  DYNAMIC usage with WRITE_DISCARD is the right pairing for per-frame
//  replacement -- discard tells the driver the previous contents are dead, so
//  it can hand back a fresh buffer instead of stalling until the GPU finishes
//  reading the old one.
//
//  Rows are copied ONE AT A TIME against the mapped RowPitch rather than as a
//  single memcpy. The driver's pitch is its own choice and is routinely larger
//  than width * 4 for alignment, so a flat copy would skew the image
//  progressively down the texture.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::UpdateContentTexture (const uint32_t * bgra, int width, int height)
{
    HRESULT                    hr     = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped = {};



    CBREx (m_device != nullptr, E_UNEXPECTED);
    CBREx (bgra != nullptr && width > 0 && height > 0, E_INVALIDARG);

    if (width != m_contentWidth || height != m_contentHeight || m_contentTex == nullptr)
    {
        D3D11_TEXTURE2D_DESC   desc = {};

        m_contentTex.Reset();
        m_contentSrv.Reset();

        desc.Width            = (UINT) width;
        desc.Height           = (UINT) height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DYNAMIC;
        desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateTexture2D (&desc, nullptr, m_contentTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateShaderResourceView (m_contentTex.Get(), nullptr, m_contentSrv.GetAddressOf());
        CHR (hr);

        m_contentWidth  = width;
        m_contentHeight = height;
    }

    hr = m_context->Map (m_contentTex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHR (hr);

    for (int y = 0; y < height; y++)
    {
        memcpy ((uint8_t *) mapped.pData + (size_t) y * mapped.RowPitch,
                bgra + (size_t) y * width,
                (size_t) width * sizeof (uint32_t));
    }

    m_context->Unmap (m_contentTex.Get(), 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDepthPass
//
//  Sizes (or re-sizes) the depth buffer to the render target that is bound
//  RIGHT NOW, and clears it for this frame's depth-tested draws. Queried
//  rather than passed in so the caller (a before-present hook) needs no
//  knowledge of the host's back-buffer plumbing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::BeginDepthPass()
{
    HRESULT                         hr   = S_OK;
    ComPtr<ID3D11RenderTargetView>  rtv;
    ComPtr<ID3D11Resource>          res;
    ComPtr<ID3D11Texture2D>         tex;
    D3D11_TEXTURE2D_DESC            desc = {};



    CBREx (m_device != nullptr, E_UNEXPECTED);

    m_context->OMGetRenderTargets (1, rtv.GetAddressOf(), nullptr);
    CBREx (rtv != nullptr, E_UNEXPECTED);

    rtv->GetResource (res.GetAddressOf());
    hr = res.As (&tex);
    CHR (hr);

    tex->GetDesc (&desc);

    if ((int) desc.Width != m_depthWidth || (int) desc.Height != m_depthHeight ||
        m_depthDsv == nullptr)
    {
        D3D11_TEXTURE2D_DESC   depthDesc = {};

        m_depthTex.Reset();
        m_depthDsv.Reset();

        depthDesc.Width            = desc.Width;
        depthDesc.Height           = desc.Height;
        depthDesc.MipLevels        = 1;
        depthDesc.ArraySize        = 1;
        depthDesc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage            = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

        hr = m_device->CreateTexture2D (&depthDesc, nullptr, m_depthTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateDepthStencilView (m_depthTex.Get(), nullptr, m_depthDsv.GetAddressOf());
        CHR (hr);

        m_depthWidth  = (int) desc.Width;
        m_depthHeight = (int) desc.Height;
    }

    m_context->ClearDepthStencilView (m_depthDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawTriangles
//
//  Draws one triangle-list batch with the given transform, texture choice, and
//  depth mode.
//
//  The FULL pipeline state is set on every draw, mirroring what
//  DxuiPainter::End does. That is deliberate and is what lets 3D batches
//  interleave freely with the painter and the text renderer: nobody has to
//  save or restore anything, because every one of them assumes nothing about
//  the state it inherits. The cost is a handful of redundant sets per draw
//  against a handful of draws per frame.
//
//  Textured and untextured differ only in WHICH SRV is bound -- the content
//  texture or the 1x1 white one -- so there is no shader or state swap between
//  them.
//
//  Depth-tested draws must re-bind the render target together with our depth
//  view, because the host bound the RTV with no DSV at all. The current RTV is
//  QUERIED rather than passed in, so callers running from a before-present
//  hook need no knowledge of the host's back-buffer plumbing. Depth-off draws
//  leave the bindings untouched.
//
//  The SRV is unbound after the draw. Nothing today binds the content texture
//  as a render target, but a later frame that did would otherwise trip a
//  read/write hazard warning, and the unbind is nearly free.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::DrawTriangles (const Vertex   * verts,
                                       size_t           vertexCount,
                                       const float      mvp[16],
                                       bool             textured,
                                       const D3D11_VIEWPORT & viewportPx,
                                       bool             depthTest)
{
    HRESULT                     hr             = S_OK;
    D3D11_MAPPED_SUBRESOURCE    mapped         = {};
    UINT                        stride         = sizeof (Vertex);
    UINT                        offset         = 0;
    float                       blendFactor[4] = {};
    ID3D11ShaderResourceView  * srv            = nullptr;
    bool                        useDepth       = depthTest && m_depthDsv != nullptr;

    CBREx (m_device != nullptr, E_UNEXPECTED);
    CBREx (verts != nullptr && vertexCount > 0 && (vertexCount % 3) == 0, E_INVALIDARG);

    srv = (textured && m_contentSrv != nullptr) ? m_contentSrv.Get() : m_whiteSrv.Get();

    hr = EnsureVertexBuffer (vertexCount);
    CHR (hr);

    hr = m_context->Map (m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHR (hr);
    memcpy (mapped.pData, verts, vertexCount * sizeof (Vertex));
    m_context->Unmap (m_vertexBuffer.Get(), 0);

    hr = m_context->Map (m_mvpBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHR (hr);
    memcpy (mapped.pData, mvp, 16 * sizeof (float));
    m_context->Unmap (m_mvpBuffer.Get(), 0);

    // Depth-tested draws re-bind the current RTV together with our DSV (the
    // host bound it without one); depth-off draws leave the bindings alone.
    if (useDepth)
    {
        ComPtr<ID3D11RenderTargetView>  rtv;
        ID3D11RenderTargetView       *  rawRtv = nullptr;

        m_context->OMGetRenderTargets (1, rtv.GetAddressOf(), nullptr);
        CBREx (rtv != nullptr, E_UNEXPECTED);

        rawRtv = rtv.Get();
        m_context->OMSetRenderTargets (1, &rawRtv, m_depthDsv.Get());
    }

    // Full state set every draw (mirrors DxuiPainter::End): interleaving with
    // the painter and text renderer needs no save/restore etiquette.
    m_context->RSSetViewports         (1, &viewportPx);
    m_context->OMSetBlendState        (m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState (useDepth ? m_depthStateTest.Get() : m_depthState.Get(), 0);
    m_context->RSSetState             (m_rasterState.Get());

    m_context->IASetInputLayout       (m_layout.Get());
    m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers     (0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    m_context->VSSetShader            (m_vs.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers   (0, 1, m_mvpBuffer.GetAddressOf());
    m_context->PSSetShader            (m_ps.Get(), nullptr, 0);
    m_context->PSSetShaderResources   (0, 1, &srv);
    m_context->PSSetSamplers          (0, 1, m_sampler.GetAddressOf());

    m_context->Draw ((UINT) vertexCount, 0);

    // Unbind the SRV so a later frame binding this texture as a render target
    // (not done today, but cheap insurance) never hits a hazard warning.
    {
        ID3D11ShaderResourceView *  nullSrv = nullptr;
        m_context->PSSetShaderResources (0, 1, &nullSrv);
    }

Error:
    return hr;
}
