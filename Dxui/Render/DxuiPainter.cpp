#include "Pch.h"

#include "DxuiPainter.h"




// The shaders are COMPILED AT BUILD TIME by fxc (see Dxui.vcxproj) and
// arrive here as bytecode. They used to be C++ string literals compiled
// by D3DCompile during CreateShaders, which pulled d3dcompiler_47.dll
// into a launch that now needs it nowhere.
#include "Painter.vs.h"
#include "Painter.ps.h"





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
//  Creates the painter's vertex / pixel shader pair and its input layout.
//
//  The vertex format is 2D position, color, and a shape description (a local
//  position, four shape parameters, and a kind) -- no texture coordinate,
//  because this painter draws only solid geometry. Curved and diagonal edges
//  get their coverage from a signed distance in the pixel shader; a solid
//  quad ignores the shape fields. Anything textured goes through the 3D
//  renderer, and anything glyph-shaped through the text renderer, so the
//  painter stays the cheapest of the three.
//
//  Nothing is compiled here. The pair arrives as bytecode from fxc, which
//  reports a shader error against its own source file at build time rather
//  than against an anonymous blob at launch.
//
//  The input layout is validated against the vertex shader's bytecode, so a
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
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT,          0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };



    hr = m_device->CreateVertexShader (g_PainterVs,
                                       sizeof (g_PainterVs),
                                       nullptr,
                                       &m_vs);
    CHRA (hr);

    hr = m_device->CreatePixelShader (g_PainterPs,
                                      sizeof (g_PainterPs),
                                      nullptr,
                                      &m_ps);
    CHRA (hr);

    hr = m_device->CreateInputLayout (inputElements,
                                      std::size (inputElements),
                                      g_PainterVs, sizeof (g_PainterVs),
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
    float   x0 = xPx + m_originXPx;
    float   y0 = yPx + m_originYPx;



    // The origin is applied HERE and only here: every primitive, spans and
    // arcs included, reaches the vertex buffer through this function.
    NdcFromPixel (x0,            y0,            tl.x, tl.y);
    NdcFromPixel (x0 + widthPx,  y0,            tr.x, tr.y);
    NdcFromPixel (x0,            y0 + heightPx, bl.x, bl.y);
    NdcFromPixel (x0 + widthPx,  y0 + heightPx, br.x, br.y);

    // Two triangles per quad: (tl, tr, bl) and (bl, tr, br). Append all six in
    // one insert so the vector grows/size-checks once rather than six times
    // (the six 24-byte copies are the same either way; Vertex is a POD).
    m_vertices.insert (m_vertices.end(), { tl, tr, bl, bl, tr, br });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushShapeQuad
//
//  Emits one quad whose visible edge is a signed-distance shape evaluated in
//  the pixel shader. The four corners share the color, kind and parameters;
//  only the local position differs, and because it is an affine function of
//  screen position, interpolating it gives the exact pixel-center position in
//  the shape's frame. Nothing here depends on the origin, which PushQuad
//  applies to the screen position alone.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::PushShapeQuad (
    float      xPx,
    float      yPx,
    float      widthPx,
    float      heightPx,
    float      localXPx,
    float      localYPx,
    ShapeKind  kind,
    float      shape0,
    float      shape1,
    float      shape2,
    float      shape3,
    uint32_t   argbColor)
{
    Vertex  tl = MakeVertex (argbColor, m_globalAlpha);
    Vertex  tr;
    Vertex  bl;
    Vertex  br;



    tl.shape0 = shape0;
    tl.shape1 = shape1;
    tl.shape2 = shape2;
    tl.shape3 = shape3;
    tl.kind   = (float) kind;
    tl.localX = localXPx;
    tl.localY = localYPx;

    tr = tl;
    bl = tl;
    br = tl;

    tr.localX += widthPx;
    bl.localY += heightPx;
    br.localX += widthPx;
    br.localY += heightPx;

    PushQuad (xPx, yPx, widthPx, heightPx, tl, tr, bl, br);
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
//  FillRoundedRect
//
//  A solid rounded rect as one quad, its corners cut by the rounded-box
//  distance function in the pixel shader. The quad is grown by the AA fringe
//  so the outer half of each edge's coverage ramp has pixels to land on; on a
//  straight run whose edge sits on a pixel boundary those fringe pixels get
//  zero coverage, so the sides stay as crisp as a FillRect.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillRoundedRect (
    float     xPx,
    float     yPx,
    float     widthPx,
    float     heightPx,
    float     radiusPx,
    uint32_t  argbColor)
{
    float  r     = radiusPx;
    float  half  = 0.0f;
    float  halfW = widthPx  * 0.5f;
    float  halfH = heightPx * 0.5f;



    DXUI_ASSERT_UI_THREAD();

    if (widthPx <= 0.0f || heightPx <= 0.0f)
    {
        return;
    }

    // Past half the shorter side a radius makes a pill, not a rounder rect.
    half = (widthPx < heightPx ? widthPx : heightPx) * 0.5f;
    r    = (r > half) ? half : ((r < 0.0f) ? 0.0f : r);

    // No radius is a plain fill, and stays on the solid path.
    if (r <= 0.0f)
    {
        FillRect (xPx, yPx, widthPx, heightPx, argbColor);
        return;
    }

    PushShapeQuad (xPx - kShapeFringePx,
                   yPx - kShapeFringePx,
                   widthPx  + 2.0f * kShapeFringePx,
                   heightPx + 2.0f * kShapeFringePx,
                   -halfW - kShapeFringePx,
                   -halfH - kShapeFringePx,
                   ShapeKind::RoundedBox,
                   halfW,
                   halfH,
                   r,
                   0.0f,
                   argbColor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OutlineRoundedRect
//
//  A rounded ring: the region between the requested rounded rect and the same
//  one inset by the stroke thickness, with the inner radius shrunk by the
//  same amount so the stroke keeps its width around the corners.
//
//  One quad, cut by the ring distance function in the pixel shader, rather
//  than four straight edges plus four arcs: the difference-of-two-shapes
//  formulation has no seams to get wrong -- a corner arc butted against an
//  edge segment shows a notch wherever the two disagree by a fraction of a
//  pixel, and at a 1.5px stroke that fraction is most of the stroke. The
//  hollow interior costs fragment work that produces zero coverage; at the
//  sizes focus rings and buttons are drawn, that is cheaper than the several
//  hundred scanline quads this replaced.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::OutlineRoundedRect (
    float     xPx,
    float     yPx,
    float     widthPx,
    float     heightPx,
    float     radiusPx,
    float     thicknessPx,
    uint32_t  argbColor)
{
    float  t     = (thicknessPx > 0.0f) ? thicknessPx : 1.0f;
    float  r     = radiusPx;
    float  half  = 0.0f;
    float  halfW = widthPx  * 0.5f;
    float  halfH = heightPx * 0.5f;



    DXUI_ASSERT_UI_THREAD();

    if (widthPx <= 0.0f || heightPx <= 0.0f)
    {
        return;
    }

    //  A radius past half the shorter side is not a rounder rectangle, it is
    //  a differently wrong one -- clamp to the pill.
    half = (widthPx < heightPx ? widthPx : heightPx) * 0.5f;
    r    = (r > half) ? half : ((r < 0.0f) ? 0.0f : r);

    PushShapeQuad (xPx - kShapeFringePx,
                   yPx - kShapeFringePx,
                   widthPx  + 2.0f * kShapeFringePx,
                   heightPx + 2.0f * kShapeFringePx,
                   -halfW - kShapeFringePx,
                   -halfH - kShapeFringePx,
                   ShapeKind::RoundedRing,
                   halfW,
                   halfH,
                   r,
                   t,
                   argbColor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillCircle
//
//  A filled circle as one quad. A rounded box whose half-size equals its
//  corner radius is exactly a circle, and its distance function is the
//  exact Euclidean one, so the circle needs no kind of its own.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillCircle (
    float     cxPx,
    float     cyPx,
    float     radiusPx,
    uint32_t  argbColor)
{
    float  extent = radiusPx + kShapeFringePx;



    DXUI_ASSERT_UI_THREAD();

    if (radiusPx <= 0.0f)
    {
        return;
    }

    PushShapeQuad (cxPx - extent,
                   cyPx - extent,
                   2.0f * extent,
                   2.0f * extent,
                   -extent,
                   -extent,
                   ShapeKind::RoundedBox,
                   radiusPx,
                   radiusPx,
                   radiusPx,
                   0.0f,
                   argbColor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillEllipse
//
//  Axis-aligned ellipse as one quad, cut by the ellipse distance function in
//  the pixel shader.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillEllipse (
    float     cxPx,
    float     cyPx,
    float     radiusXPx,
    float     radiusYPx,
    uint32_t  argbColor)
{
    float  extentX = radiusXPx + kShapeFringePx;
    float  extentY = radiusYPx + kShapeFringePx;



    DXUI_ASSERT_UI_THREAD();

    if (radiusXPx <= 0.0f || radiusYPx <= 0.0f)
    {
        return;
    }

    PushShapeQuad (cxPx - extentX,
                   cyPx - extentY,
                   2.0f * extentX,
                   2.0f * extentY,
                   -extentX,
                   -extentY,
                   ShapeKind::Ellipse,
                   radiusXPx,
                   radiusYPx,
                   0.0f,
                   0.0f,
                   argbColor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeEdgePlanes
//
//  For each edge of a convex quad, the line equation nx * x + ny * y + offset
//  whose value is the signed distance to that edge, positive outside. The
//  quad may wind either way; the centroid, which is inside any convex quad,
//  picks the sign. An edge too short to have a direction -- a triangle passed
//  as a quad with one point repeated -- gets a plane that is far inside
//  everywhere, so it never limits the shape.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::MakeEdgePlanes (
    const float  px[4],
    const float  py[4],
    float        nx[4],
    float        ny[4],
    float        offset[4])
{
    constexpr float  kMinEdgePx     = 1.0e-4f;
    constexpr float  kFarInsidePx   = -1.0e6f;
    constexpr int    kCorners       = 4;



    float  cx = (px[0] + px[1] + px[2] + px[3]) * 0.25f;
    float  cy = (py[0] + py[1] + py[2] + py[3]) * 0.25f;



    for (int e = 0; e < kCorners; e++)
    {
        int    next = (e + 1) % kCorners;
        float  dx   = px[next] - px[e];
        float  dy   = py[next] - py[e];
        float  len  = sqrtf (dx * dx + dy * dy);

        if (len < kMinEdgePx)
        {
            nx[e]     = 0.0f;
            ny[e]     = 0.0f;
            offset[e] = kFarInsidePx;
            continue;
        }

        // The left-hand normal of the edge direction, unit length.
        nx[e]     = -dy / len;
        ny[e]     =  dx / len;
        offset[e] = -(nx[e] * px[e] + ny[e] * py[e]);

        // Flip so the centroid measures negative, i.e. inside.
        if (nx[e] * cx + ny[e] * cy + offset[e] > 0.0f)
        {
            nx[e]     = -nx[e];
            ny[e]     = -ny[e];
            offset[e] = -offset[e];
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillConvexQuad
//
//  Convex quad (points in order, either winding) as one quad covering its
//  bounding box. Each corner of that box carries its signed distance to the
//  four edge lines; the pixel shader takes the largest, which is the distance
//  to the quad's boundary everywhere except just outside a corner, where it
//  runs slightly short and leaves a sharp tip a touch bolder. Diagonal edges
//  come out smooth in both directions rather than stepped scanline by
//  scanline.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::FillConvexQuad (
    float x0, float y0, float x1, float y1,
    float x2, float y2, float x3, float y3,
    uint32_t argbColor)
{
    Vertex  corner[4];
    float   px[4]      = { x0, x1, x2, x3 };
    float   py[4]      = { y0, y1, y2, y3 };
    float   nx[4]      = {};
    float   ny[4]      = {};
    float   offset[4]  = {};
    float   left       = fminf (fminf (x0, x1), fminf (x2, x3)) - kShapeFringePx;
    float   top        = fminf (fminf (y0, y1), fminf (y2, y3)) - kShapeFringePx;
    float   right      = fmaxf (fmaxf (x0, x1), fmaxf (x2, x3)) + kShapeFringePx;
    float   bottom     = fmaxf (fmaxf (y0, y1), fmaxf (y2, y3)) + kShapeFringePx;
    float   cornerX[4] = { left, right, left,   right  };
    float   cornerY[4] = { top,  top,   bottom, bottom };



    DXUI_ASSERT_UI_THREAD();

    MakeEdgePlanes (px, py, nx, ny, offset);

    // Corners in PushQuad's order: top-left, top-right, bottom-left, bottom-right.
    for (int i = 0; i < 4; i++)
    {
        corner[i]       = MakeVertex (argbColor, m_globalAlpha);
        corner[i].kind  = (float) ShapeKind::ConvexQuad;
        corner[i].edge0 = nx[0] * cornerX[i] + ny[0] * cornerY[i] + offset[0];
        corner[i].edge1 = nx[1] * cornerX[i] + ny[1] * cornerY[i] + offset[1];
        corner[i].edge2 = nx[2] * cornerX[i] + ny[2] * cornerY[i] + offset[2];
        corner[i].edge3 = nx[3] * cornerX[i] + ny[3] * cornerY[i] + offset[3];
    }

    PushQuad (left, top, right - left, bottom - top, corner[0], corner[1], corner[2], corner[3]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawLine
//
//  Line segment as a capsule: the segment thickened by half the thickness on
//  every side, with round caps. One quad covering the capsule's bounding box,
//  cut by the capsule distance function in the pixel shader, so a diagonal
//  comes out as a smooth stroke rather than a staircase of stamped squares.
//  A horizontal or vertical line on pixel centers with a whole-pixel
//  thickness still lands fully covered along its length.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPainter::DrawLine (
    float x0, float y0, float x1, float y1,
    float thicknessPx, uint32_t argbColor)
{
    float  half   = thicknessPx * 0.5f;
    float  extent = half + kShapeFringePx;
    float  left   = ((x0 < x1) ? x0 : x1) - extent;
    float  top    = ((y0 < y1) ? y0 : y1) - extent;
    float  right  = ((x0 > x1) ? x0 : x1) + extent;
    float  bottom = ((y0 > y1) ? y0 : y1) + extent;



    DXUI_ASSERT_UI_THREAD();

    if (thicknessPx <= 0.0f)
    {
        return;
    }

    PushShapeQuad (left,
                   top,
                   right  - left,
                   bottom - top,
                   left - x0,
                   top  - y0,
                   ShapeKind::Capsule,
                   x1 - x0,
                   y1 - y0,
                   half,
                   0.0f,
                   argbColor);
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


