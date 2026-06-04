#include "Pch.h"

#include "DxuiPainter.h"

#pragma comment(lib, "d3dcompiler.lib")





namespace
{
    constexpr size_t  s_kInitialVertexCapacity = 1024;
    constexpr float   s_kByteToUnit            = 1.0f / 255.0f;


    static const char s_kVertexShaderSrc[] =
        "struct VSIn  { float2 pos : POSITION; float4 col : COLOR; };\n"
        "struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
        "VSOut main (VSIn input)\n"
        "{\n"
        "    VSOut output;\n"
        "    output.pos = float4 (input.pos, 0.0f, 1.0f);\n"
        "    output.col = input.col;\n"
        "    return output;\n"
        "}\n";


    static const char s_kPixelShaderSrc[] =
        "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
        "float4 main (PSIn input) : SV_TARGET\n"
        "{\n"
        "    return input.col;\n"
        "}\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiPainter::~DxuiPainter ()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::Initialize (
    ID3D11Device         * pDevice,
    ID3D11DeviceContext  * pContext)
{
    HRESULT  hr = S_OK;



    CBRAEx (pDevice,  E_INVALIDARG);
    CBRAEx (pContext, E_INVALIDARG);

    m_device  = pDevice;
    m_context = pContext;

    hr = CreateShaders();
    CHRA (hr);

    hr = CreatePipelineState();
    CHRA (hr);

    hr = EnsureVertexBuffer (s_kInitialVertexCapacity);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::Shutdown ()
{
    m_vertices.clear();
    m_vertexBuffer.Reset();
    m_depthState.Reset();
    m_rasterState.Reset();
    m_blendState.Reset();
    m_layout.Reset();
    m_ps.Reset();
    m_vs.Reset();
    m_vertexBufferCapacity = 0;
    m_betweenBeginEnd      = false;
    m_device  = nullptr;
    m_context = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceLost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::OnDeviceLost ()
{
    Shutdown();
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceRestored
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::OnDeviceRestored (
    ID3D11Device         * pDevice,
    ID3D11DeviceContext  * pContext)
{
    return Initialize (pDevice, pContext);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateShaders
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::CreateShaders ()
{
    HRESULT             hr     = S_OK;
    ComPtr<ID3DBlob>    vsBlob;
    ComPtr<ID3DBlob>    psBlob;
    ComPtr<ID3DBlob>    errors;

    D3D11_INPUT_ELEMENT_DESC  inputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };



    hr = D3DCompile (s_kVertexShaderSrc,
                     sizeof (s_kVertexShaderSrc) - 1,
                     "DxuiPainter.vs",
                     nullptr,
                     nullptr,
                     "main",
                     "vs_4_0",
                     0,
                     0,
                     &vsBlob,
                     &errors);
    CHRA (hr);

    hr = D3DCompile (s_kPixelShaderSrc,
                     sizeof (s_kPixelShaderSrc) - 1,
                     "DxuiPainter.ps",
                     nullptr,
                     nullptr,
                     "main",
                     "ps_4_0",
                     0,
                     0,
                     &psBlob,
                     &errors);
    CHRA (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(),
                                       nullptr,
                                       &m_vs);
    CHRA (hr);

    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer(),
                                      psBlob->GetBufferSize(),
                                      nullptr,
                                      &m_ps);
    CHRA (hr);

    hr = m_device->CreateInputLayout (inputElements,
                                      ARRAYSIZE (inputElements),
                                      vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(),
                                      &m_layout);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreatePipelineState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::CreatePipelineState ()
{
    HRESULT                   hr      = S_OK;
    D3D11_BLEND_DESC          blend   = {};
    D3D11_RASTERIZER_DESC     raster  = {};
    D3D11_DEPTH_STENCIL_DESC  depth   = {};



    // Premultiplied-alpha source-over compositing.
    blend.RenderTarget[0].BlendEnable           = TRUE;
    blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState (&blend, &m_blendState);
    CHRA (hr);

    raster.FillMode        = D3D11_FILL_SOLID;
    raster.CullMode        = D3D11_CULL_NONE;
    raster.ScissorEnable   = FALSE;
    raster.DepthClipEnable = TRUE;

    hr = m_device->CreateRasterizerState (&raster, &m_rasterState);
    CHRA (hr);

    depth.DepthEnable    = FALSE;
    depth.StencilEnable  = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

    hr = m_device->CreateDepthStencilState (&depth, &m_depthState);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureVertexBuffer
//
//  Grows the dynamic vertex buffer when the next batch exceeds current
//  capacity. New capacity rounds up to the next power-of-two so we don't
//  thrash the allocator on incremental growth.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::EnsureVertexBuffer (size_t requiredVerts)
{
    HRESULT            hr       = S_OK;
    D3D11_BUFFER_DESC  desc     = {};
    size_t             newCap   = 0;



    if ((m_vertexBuffer != nullptr) && (requiredVerts <= m_vertexBufferCapacity))
    {
        return S_OK;
    }

    newCap = m_vertexBufferCapacity > 0 ? m_vertexBufferCapacity : s_kInitialVertexCapacity;

    while (newCap < requiredVerts)
    {
        newCap *= 2;
    }

    m_vertexBuffer.Reset();

    desc.ByteWidth      = (UINT) (newCap * sizeof (Vertex));
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer (&desc, nullptr, &m_vertexBuffer);
    CHRA (hr);

    m_vertexBufferCapacity = newCap;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Begin
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::Begin (int viewportWidthPx, int viewportHeightPx)
{
    HRESULT  hr = S_OK;



    CBRA (m_device);
    CBRA (m_context);

    m_viewportWidthPx  = viewportWidthPx;
    m_viewportHeightPx = viewportHeightPx;
    m_vertices.clear();
    m_betweenBeginEnd  = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeVertex
//
////////////////////////////////////////////////////////////////////////////////

DxuiPainter::Vertex DxuiPainter::MakeVertex (uint32_t argbColor, float alphaMultiplier)
{
    Vertex  v;
    float   a = ((argbColor >> 24) & 0xFF) * s_kByteToUnit;
    float   r = ((argbColor >> 16) & 0xFF) * s_kByteToUnit;
    float   g = ((argbColor >>  8) & 0xFF) * s_kByteToUnit;
    float   b = ((argbColor      ) & 0xFF) * s_kByteToUnit;



    a *= (alphaMultiplier < 0.0f) ? 0.0f : (alphaMultiplier > 1.0f) ? 1.0f : alphaMultiplier;

    // Premultiply RGB by alpha so the source-over blend renders correctly.
    v.x = 0.0f;
    v.y = 0.0f;
    v.r = r * a;
    v.g = g * a;
    v.b = b * a;
    v.a = a;

    return v;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NdcFromPixel
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::NdcFromPixel (float xPx, float yPx, float & outX, float & outY) const
{
    if ((m_viewportWidthPx <= 0) || (m_viewportHeightPx <= 0))
    {
        outX = 0.0f;
        outY = 0.0f;
        return;
    }

    outX = (xPx / (float) m_viewportWidthPx)  * 2.0f - 1.0f;
    outY = 1.0f - (yPx / (float) m_viewportHeightPx) * 2.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushQuad
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::PushQuad (
    float          xPx,
    float          yPx,
    float          widthPx,
    float          heightPx,
    const Vertex & topLeft,
    const Vertex & topRight,
    const Vertex & bottomLeft,
    const Vertex & bottomRight)
{
    Vertex  tl = topLeft;
    Vertex  tr = topRight;
    Vertex  bl = bottomLeft;
    Vertex  br = bottomRight;



    NdcFromPixel (xPx,           yPx,            tl.x, tl.y);
    NdcFromPixel (xPx + widthPx, yPx,            tr.x, tr.y);
    NdcFromPixel (xPx,           yPx + heightPx, bl.x, bl.y);
    NdcFromPixel (xPx + widthPx, yPx + heightPx, br.x, br.y);

    // Two triangles per quad: (tl, tr, bl) and (bl, tr, br).
    m_vertices.push_back (tl);
    m_vertices.push_back (tr);
    m_vertices.push_back (bl);
    m_vertices.push_back (bl);
    m_vertices.push_back (tr);
    m_vertices.push_back (br);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillRect (
    float     xPx,
    float     yPx,
    float     widthPx,
    float     heightPx,
    uint32_t  argbColor)
{
    Vertex  v = MakeVertex (argbColor, m_globalAlpha);



    PushQuad (xPx, yPx, widthPx, heightPx, v, v, v, v);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillGradientRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillGradientRect (
    float     xPx,
    float     yPx,
    float     widthPx,
    float     heightPx,
    uint32_t  argbTop,
    uint32_t  argbBottom)
{
    Vertex  top    = MakeVertex (argbTop,    m_globalAlpha);
    Vertex  bottom = MakeVertex (argbBottom, m_globalAlpha);



    PushQuad (xPx, yPx, widthPx, heightPx, top, top, bottom, bottom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OutlineRect
//
//  Draws four thin filled rects on the inside of the requested rect.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::OutlineRect (
    float     xPx,
    float     yPx,
    float     widthPx,
    float     heightPx,
    float     thicknessPx,
    uint32_t  argbColor)
{
    float  t = (thicknessPx > 0.0f) ? thicknessPx : 1.0f;



    FillRect (xPx,                    yPx,                    widthPx,  t,                  argbColor);
    FillRect (xPx,                    yPx + heightPx - t,     widthPx,  t,                  argbColor);
    FillRect (xPx,                    yPx + t,                t,        heightPx - 2.0f * t, argbColor);
    FillRect (xPx + widthPx - t,      yPx + t,                t,        heightPx - 2.0f * t, argbColor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillCircleApprox
//
//  Approximates a filled circle using horizontal slices. Inexpensive
//  and visually adequate for small UI indicators (LED dots, radio
//  buttons, toggle thumbs). Slice count scales gently with radius.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillCircleApprox (
    float     cxPx,
    float     cyPx,
    float     radiusPx,
    uint32_t  argbColor)
{
    int  slices = (int) (radiusPx * 2.0f);



    if (radiusPx <= 0.0f) return;
    if (slices  <  8)     slices = 8;
    if (slices  > 32)     slices = 32;

    for (int i = 0; i < slices; i++)
    {
        float  y0   = cyPx - radiusPx + (2.0f * radiusPx * (float) i)       / (float) slices;
        float  y1   = cyPx - radiusPx + (2.0f * radiusPx * (float) (i + 1)) / (float) slices;
        float  ymid = (y0 + y1) * 0.5f;
        float  dy   = ymid - cyPx;
        float  sq   = radiusPx * radiusPx - dy * dy;
        float  half = (sq > 0.0f) ? sqrtf (sq) : 0.0f;

        FillRect (cxPx - half, y0, 2.0f * half, y1 - y0, argbColor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  End
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::End (ID3D11RenderTargetView * pRtv)
{
    HRESULT                    hr           = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped       = {};
    UINT                       stride       = sizeof (Vertex);
    UINT                       offset       = 0;
    float                      blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT             vp           = {};
    ID3D11RenderTargetView   * rtvs[1]      = { pRtv };



    m_betweenBeginEnd = false;

    if (m_vertices.empty() || (pRtv == nullptr))
    {
        return S_OK;
    }

    hr = EnsureVertexBuffer (m_vertices.size());
    CHRA (hr);

    hr = m_context->Map (m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHRA (hr);

    memcpy (mapped.pData, m_vertices.data(), m_vertices.size() * sizeof (Vertex));
    m_context->Unmap (m_vertexBuffer.Get(), 0);

    vp.Width    = (float) m_viewportWidthPx;
    vp.Height   = (float) m_viewportHeightPx;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    m_context->OMSetRenderTargets (1, rtvs, nullptr);
    m_context->OMSetBlendState        (m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState (m_depthState.Get(), 0);
    m_context->RSSetState             (m_rasterState.Get());

    m_context->IASetInputLayout       (m_layout.Get());
    m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers     (0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    m_context->VSSetShader (m_vs.Get(), nullptr, 0);
    m_context->PSSetShader (m_ps.Get(), nullptr, 0);

    m_context->Draw ((UINT) m_vertices.size(), 0);

    m_vertices.clear();

Error:
    return hr;
}
