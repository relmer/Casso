#include "Pch.h"

#include "DxuiPainter.h"




// Shader source stays a file-scope static rather than a class member: it is
// bulk implementation detail read only by CreateShaders in this file, and a
// class member would have to be declared in the header -- putting HLSL into
// every translation unit that includes DxuiPainter.h.
//
// The array type is load-bearing. CreateShaders passes
// `sizeof (s_kVertexShaderSrc) - 1` as the source length; against a
// `const char *` that silently becomes sizeof(void*) - 1 == 7, compiling
// clean while handing D3DCompile a 7-character shader.
static constexpr char  s_kVertexShaderSrc[] =
    "struct VSIn  { float2 pos : POSITION; float4 col : COLOR; };\n"
    "struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
    "VSOut main (VSIn input)\n"
    "{\n"
    "    VSOut output;\n"
    "    output.pos = float4 (input.pos, 0.0f, 1.0f);\n"
    "    output.col = input.col;\n"
    "    return output;\n"
    "}\n";


static constexpr char  s_kPixelShaderSrc[] =
    "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
    "float4 main (PSIn input) : SV_TARGET\n"
    "{\n"
    "    return input.col;\n"
    "}\n";

#pragma comment(lib, "d3dcompiler.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiPainter::~DxuiPainter()
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



    DXUI_ASSERT_UI_THREAD();

    CBRAEx (pDevice,  E_INVALIDARG);
    CBRAEx (pContext, E_INVALIDARG);

    m_device  = pDevice;
    m_context = pContext;

    hr = CreateShaders();
    CHRA (hr);

    hr = CreatePipelineState();
    CHRA (hr);

    hr = EnsureVertexBuffer (kInitialVertexCapacity);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::Shutdown()
{
    DXUI_ASSERT_UI_THREAD();

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

HRESULT DxuiPainter::OnDeviceLost()
{
    DXUI_ASSERT_UI_THREAD();

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
    DXUI_ASSERT_UI_THREAD();

    return Initialize (pDevice, pContext);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateShaders
//
//  Compiles the painter's vertex / pixel shader pair and its input layout.
//
//  The vertex format is 2D position plus color and nothing else -- no texture
//  coordinate, because this painter draws only solid geometry. Anything
//  textured goes through the 3D renderer, and anything glyph-shaped through
//  the text renderer, so the painter stays the cheapest of the three.
//
//  Shader source is embedded as string literals rather than as resources,
//  since both are a few lines and belong beside the vertex struct they must
//  agree with.
//
//  The names passed to D3DCompile are for DIAGNOSTICS only; they make a
//  compile error name which of the two shaders failed instead of reporting
//  against an anonymous blob.
//
//  The input layout is validated against the compiled vertex-shader blob, so a
//  mismatch between the struct and the shader signature fails at startup
//  rather than as garbage geometry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::CreateShaders()
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
                                      std::size (inputElements),
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
//  Builds the three fixed-function state objects the painter needs.
//
//  Premultiplied-alpha source-over is the compositing convention shared with
//  the text renderer and the 3D renderer, which is what lets all three draw
//  into one surface and layer predictably. Changing it here would silently
//  desynchronize chrome translucency from everything else.
//
//  Culling is off because 2D geometry has no meaningful winding order; a quad
//  emitted either way must paint.
//
//  Depth is disabled entirely -- test, write, and stencil. UI is painted in
//  back-to-front order by the panel tree, so a depth buffer would add cost and
//  could only ever reject something the tree intended to be on top.
//
//  The scissor is off by default; clipping is applied per draw when a widget
//  needs it, so the common unclipped case costs nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::CreatePipelineState()
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



    BAIL_OUT_IF ((m_vertexBuffer != nullptr) && (requiredVerts <= m_vertexBufferCapacity), S_OK);

    newCap = m_vertexBufferCapacity > 0 ? m_vertexBufferCapacity : kInitialVertexCapacity;

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



    DXUI_ASSERT_UI_THREAD();

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
    constexpr float   kByteToUnit            = 1.0f / 255.0f;



    Vertex  v;
    float   a = ((argbColor >> 24) & 0xFF) * kByteToUnit;
    float   r = ((argbColor >> 16) & 0xFF) * kByteToUnit;
    float   g = ((argbColor >>  8) & 0xFF) * kByteToUnit;
    float   b = ((argbColor      ) & 0xFF) * kByteToUnit;



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

    // Two triangles per quad: (tl, tr, bl) and (bl, tr, br). Append all six in
    // one insert so the vector grows/size-checks once rather than six times
    // (the six 24-byte copies are the same either way; Vertex is a POD).
    m_vertices.insert (m_vertices.end(), { tl, tr, bl, bl, tr, br });
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



    DXUI_ASSERT_UI_THREAD();

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



    DXUI_ASSERT_UI_THREAD();

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



    DXUI_ASSERT_UI_THREAD();

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



    DXUI_ASSERT_UI_THREAD();

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

        FillSpanAA (cxPx - half, cxPx + half, y0, y1 - y0, argbColor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillEllipseApprox
//
//  Axis-aligned ellipse via the same horizontal rect slicing as
//  FillCircleApprox, with the half-width scaled by rx/ry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillEllipseApprox (
    float     cxPx,
    float     cyPx,
    float     radiusXPx,
    float     radiusYPx,
    uint32_t  argbColor)
{
    int  slices = (int) (radiusYPx * 2.0f);



    DXUI_ASSERT_UI_THREAD();

    if (radiusXPx <= 0.0f || radiusYPx <= 0.0f) return;
    if (slices <  6)  slices = 6;
    if (slices > 32)  slices = 32;

    for (int i = 0; i < slices; i++)
    {
        float  y0   = cyPx - radiusYPx + (2.0f * radiusYPx * (float) i)       / (float) slices;
        float  y1   = cyPx - radiusYPx + (2.0f * radiusYPx * (float) (i + 1)) / (float) slices;
        float  ymid = (y0 + y1) * 0.5f;
        float  t    = (ymid - cyPx) / radiusYPx;
        float  sq   = 1.0f - t * t;
        float  half = (sq > 0.0f) ? radiusXPx * sqrtf (sq) : 0.0f;

        FillSpanAA (cxPx - half, cxPx + half, y0, y1 - y0, argbColor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillSpanAA
//
//  Fill one horizontal scanline span with coverage anti-aliasing on the
//  fractional left/right edges: the interior whole-pixel columns get full
//  color, and the boundary pixel columns get the color at partial alpha =
//  fractional coverage. A plain FillRect hard-snaps its edges to pixel
//  centers, so a stack of them approximating an oblique edge stair-steps;
//  feathering the end columns turns that staircase into a smooth ramp.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillSpanAA (float x0, float x1, float y, float h, uint32_t argbColor)
{
    uint32_t  baseA    = (argbColor >> 24) & 0xFFu;
    float     xl       = 0.0f;
    float     xr       = 0.0f;
    float     leftCov  = 0.0f;
    float     rightCov = 0.0f;
    auto      withA = [&](float cov) -> uint32_t
    {
        uint32_t  a = 0;

        cov = (cov < 0.0f) ? 0.0f : (cov > 1.0f) ? 1.0f : cov;
        a = (uint32_t) ((float) baseA * cov + 0.5f);
        return (argbColor & 0x00FFFFFFu) | (a << 24);
    };



    xl = floorf (x0);
    xr = floorf (x1);

    // Degenerate spans (zero or negative width / height) draw nothing.
    if (x1 > x0 && h > 0.0f)
    {
        if (xl == xr)
        {
            // Span lives inside ONE pixel column: its whole width is the
            // coverage, and there are no interior or far-edge columns.
            FillRect (xl, y, 1.0f, h, withA (x1 - x0));
        }
        else
        {
            if (xr > xl + 1.0f)                                 // interior full columns
                FillRect (xl + 1.0f, y, xr - (xl + 1.0f), h, argbColor);

            leftCov = (xl + 1.0f) - x0;                         // left edge coverage
            if (leftCov > 0.004f)
                FillRect (xl, y, 1.0f, h, withA (leftCov));

            rightCov = x1 - xr;                                 // right edge coverage
            if (rightCov > 0.004f)
                FillRect (xr, y, 1.0f, h, withA (rightCov));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillConvexQuad
//
//  Convex quad (points in order) via horizontal scanline slices: for each
//  slice the left/right span is interpolated along the two edges the slice
//  crosses. Good to ~1px, matching the circle approximation's fidelity.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillConvexQuad (
    float x0, float y0, float x1, float y1,
    float x2, float y2, float x3, float y3,
    uint32_t argbColor)
{
    float  px[4]  = { x0, x1, x2, x3 };
    float  py[4]  = { y0, y1, y2, y3 };
    float  minY   = py[0], maxY = py[0];
    int    slices = 0;



    DXUI_ASSERT_UI_THREAD();

    for (int i = 1; i < 4; i++)
    {
        if (py[i] < minY) minY = py[i];
        if (py[i] > maxY) maxY = py[i];
    }

    slices = (int) (maxY - minY);
    if (slices < 1)   slices = 1;
    if (slices > 96)  slices = 96;

    for (int i = 0; i < slices; i++)
    {
        float  sy0  = minY + (maxY - minY) * (float) i       / (float) slices;
        float  sy1  = minY + (maxY - minY) * (float) (i + 1) / (float) slices;
        float  ymid = (sy0 + sy1) * 0.5f;
        float  lo   = 0.0f;
        float  hi   = 0.0f;
        bool   any  = false;

        // Intersect the scanline with each edge; track the min/max x.
        for (int e = 0; e < 4; e++)
        {
            float  ax = px[e],           ay = py[e];
            float  bx = px[(e + 1) & 3], by = py[(e + 1) & 3];

            if ((ay <= ymid && by >= ymid) || (by <= ymid && ay >= ymid))
            {
                float  dy = by - ay;
                float  x  = (fabsf (dy) < 0.0001f) ? ax
                                                   : ax + (bx - ax) * (ymid - ay) / dy;
                if (!any)        { lo = hi = x; any = true; }
                else if (x < lo) { lo = x; }
                else if (x > hi) { hi = x; }
            }
        }

        if (any && hi > lo)
        {
            FillSpanAA (lo, hi, sy0, sy1 - sy0, argbColor);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawLineApprox
//
//  Line segment as small steps of filled rects along the major axis --
//  glyph-stroke quality (grip ribs, knurl ticks), not general vector art.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::DrawLineApprox (
    float x0, float y0, float x1, float y1,
    float thicknessPx, uint32_t argbColor)
{
    float  dx    = x1 - x0;
    float  dy    = y1 - y0;
    float  len   = sqrtf (dx * dx + dy * dy);
    float  half  = thicknessPx * 0.5f;
    int    steps = 0;



    DXUI_ASSERT_UI_THREAD();

    if (len < 0.5f)
    {
        FillRect (x0 - half, y0 - half, thicknessPx, thicknessPx, argbColor);
        return;
    }

    steps = (int) len + 1;
    if (steps > 96) steps = 96;

    for (int i = 0; i <= steps; i++)
    {
        float  t = (float) i / (float) steps;
        FillRect (x0 + dx * t - half, y0 + dy * t - half,
                  thicknessPx, thicknessPx, argbColor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  End
//
//  Flushes everything accumulated since Begin as ONE draw call.
//
//  Batching is the painter's whole reason for existing. Every FillRect and
//  OutlineRect between Begin and End only appends triangles to a CPU-side
//  vector; a chrome frame that draws a hundred rectangles costs one upload and
//  one Draw rather than a hundred of each.
//
//  The full pipeline state is set here, every frame. Nothing is saved or
//  restored, and that is the shared convention -- the text renderer and the 3D
//  renderer do the same -- so all three can interleave freely without any of
//  them assuming what state it inherits.
//
//  m_betweenBeginEnd is cleared BEFORE the early-out, so an empty batch or a
//  null target still closes the Begin/End pair rather than leaving the painter
//  believing it is mid-batch.
//
//  WRITE_DISCARD tells the driver the previous vertex contents are dead, so it
//  hands back a fresh buffer instead of stalling until the GPU has finished
//  reading last frame's.
//
//  The vertex list is cleared only on the path that actually drew it, so a
//  failed flush keeps the geometry rather than silently discarding a frame's
//  worth of work.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPainter::End (ID3D11RenderTargetView * pRtv)
{
    HRESULT                     hr               = S_OK;
    D3D11_MAPPED_SUBRESOURCE    mapped           = {};
    UINT                        stride           = sizeof (Vertex);
    UINT                        offset           = 0;
    float                       blendFactor[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT              vp               = {};
    ID3D11RenderTargetView    * rtvs[1]          = { pRtv };
    bool                        hasNothingToDraw = false;



    DXUI_ASSERT_UI_THREAD();

    m_betweenBeginEnd = false;
    hasNothingToDraw  = m_vertices.empty() || (pRtv == nullptr);

    BAIL_OUT_IF (hasNothingToDraw, S_OK);

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


