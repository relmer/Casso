#pragma once

#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPainter
//
//  Direct3D 11 geometry painter for the native UI runtime. Owns a
//  vertex / pixel shader pair plus a dynamic vertex buffer; consumers
//  submit colored quads (solid fills, gradients, outlined rects) and
//  the painter batches them into a single draw on `End()`. The painter
//  composites premultiplied-alpha source-over onto whatever render
//  target is bound by the caller.
//
//  Lifetime: `Initialize` allocates GPU resources from the device the
//  caller passes in (typically `D3DRenderer::GetDevice()`).
//  `OnDeviceLost` releases everything; `OnDeviceRestored` rebuilds
//  against the post-restore device. The painter holds non-owning
//  pointers to the device + context for the duration of one
//  Initialize -> Shutdown cycle.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPainter : public IDxuiPainter
{
public:
    DxuiPainter  () = default;
    ~DxuiPainter ();

    HRESULT Initialize       (ID3D11Device         * pDevice,
                              ID3D11DeviceContext  * pContext);
    void    Shutdown         ();

    HRESULT OnDeviceLost     ();
    HRESULT OnDeviceRestored (ID3D11Device         * pDevice,
                              ID3D11DeviceContext  * pContext);

    HRESULT Begin            (int viewportWidthPx,
                              int viewportHeightPx);

    void    FillRect         (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              uint32_t argbColor) override;

    void    FillGradientRect (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              uint32_t argbTop,
                              uint32_t argbBottom) override;

    void    OutlineRect      (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              float thicknessPx,
                              uint32_t argbColor) override;

    void    OutlineRoundedRect (float xPx,
                                float yPx,
                                float widthPx,
                                float heightPx,
                                float radiusPx,
                                float thicknessPx,
                                uint32_t argbColor) override;

    void    FillRoundedRect  (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              float radiusPx,
                              uint32_t argbColor) override;

    // Filled circle with analytic edge coverage. Used for round
    // indicators (LEDs, radio dots, toggle thumbs).
    void    FillCircle       (float cxPx,
                              float cyPx,
                              float radiusPx,
                              uint32_t argbColor) override;

    void    FillConvexQuad    (float x0, float y0, float x1, float y1,
                               float x2, float y2, float x3, float y3,
                               uint32_t argbColor) override;
    void    FillEllipse       (float cxPx, float cyPx,
                               float radiusXPx, float radiusYPx,
                               uint32_t argbColor) override;
    void    DrawLine          (float x0, float y0, float x1, float y1,
                               float thicknessPx, uint32_t argbColor) override;

    HRESULT End            (ID3D11RenderTargetView * pRtv);

    // Global alpha multiplier applied to every vertex's alpha channel.
    // Used by the Settings panel's live-preview state machine to fade
    // the whole UI without touching individual paint call sites. 1.0
    // is opaque (default), 0.0 is fully transparent.
    void    SetGlobalAlpha (float alpha)            override { m_globalAlpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha; }
    void    SetOrigin      (float xPx, float yPx)   override { m_originXPx = xPx; m_originYPx = yPx; }
    float   GetGlobalAlpha () const                 override { return m_globalAlpha; }

    int     GetPendingVertexCount () const { return (int) m_vertices.size(); }

private:
    static constexpr size_t  kInitialVertexCapacity = 1024;


    static constexpr float   kShapeFringePx         = 1.0f;


    // Which signed-distance function the pixel shader evaluates for a quad.
    // Solid means no shape: the quad's own edges are the edges, at full
    // coverage, which is how every axis-aligned rect stays crisp. The values
    // must match the kKind constants in Painter.ps.hlsl.
    enum class ShapeKind
    {
        Solid       = 0,
        RoundedBox  = 1,
        RoundedRing = 2,
        Ellipse     = 3,
        Capsule     = 4,
        ConvexQuad  = 5,
    };


    // Position (NDC), premultiplied color, then the shape: the vertex's
    // position in pixels relative to the shape's own origin, the shape's
    // parameters, and its kind. A convex quad instead carries the vertex's
    // signed distance to each of its four edges, which interpolates exactly
    // because distance to a line is affine in position. The shape fields are
    // zero for a solid quad.
    struct Vertex
    {
        float  x      = 0.0f;
        float  y      = 0.0f;
        float  r      = 0.0f;
        float  g      = 0.0f;
        float  b      = 0.0f;
        float  a      = 0.0f;
        float  localX = 0.0f;
        float  localY = 0.0f;
        float  shape0 = 0.0f;
        float  shape1 = 0.0f;
        float  shape2 = 0.0f;
        float  shape3 = 0.0f;
        float  kind   = 0.0f;
        float  edge0  = 0.0f;
        float  edge1  = 0.0f;
        float  edge2  = 0.0f;
        float  edge3  = 0.0f;
    };


    HRESULT CreateShaders    ();
    HRESULT CreatePipelineState ();
    HRESULT EnsureVertexBuffer  (size_t requiredVerts);
    void    PushQuad         (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              const Vertex & topLeft,
                              const Vertex & topRight,
                              const Vertex & bottomLeft,
                              const Vertex & bottomRight);
    // A quad whose edges come from a signed-distance shape rather than from
    // the quad itself. The quad is the shape's bounds grown by the AA fringe;
    // (localX, localY) is the quad's top-left relative to the shape's origin.
    void    PushShapeQuad    (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              float localXPx,
                              float localYPx,
                              ShapeKind kind,
                              float shape0,
                              float shape1,
                              float shape2,
                              float shape3,
                              uint32_t argbColor);
    void    NdcFromPixel    (float xPx, float yPx, float & outX, float & outY) const;

    static Vertex MakeVertex     (uint32_t argbColor, float alphaMultiplier = 1.0f);
    static void   MakeEdgePlanes (const float px[4],
                                  const float py[4],
                                  float       nx[4],
                                  float       ny[4],
                                  float       offset[4]);


    ID3D11Device                    * m_device  = nullptr;   // non-owning
    ID3D11DeviceContext             * m_context = nullptr;   // non-owning

    ComPtr<ID3D11VertexShader>        m_vs;
    ComPtr<ID3D11PixelShader>         m_ps;
    ComPtr<ID3D11InputLayout>         m_layout;
    ComPtr<ID3D11Buffer>              m_vertexBuffer;
    ComPtr<ID3D11BlendState>          m_blendState;
    ComPtr<ID3D11RasterizerState>     m_rasterState;
    ComPtr<ID3D11DepthStencilState>   m_depthState;

    size_t                            m_vertexBufferCapacity = 0;
    int                               m_viewportWidthPx      = 0;
    int                               m_viewportHeightPx     = 0;
    bool                              m_betweenBeginEnd      = false;
    float                             m_globalAlpha          = 1.0f;
    float                             m_originXPx            = 0.0f;
    float                             m_originYPx            = 0.0f;

    std::vector<Vertex>               m_vertices;
};
