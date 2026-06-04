#pragma once





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

class DxuiPainter
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
                              uint32_t argbColor);

    void    FillGradientRect (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              uint32_t argbTop,
                              uint32_t argbBottom);

    void    OutlineRect      (float xPx,
                              float yPx,
                              float widthPx,
                              float heightPx,
                              float thicknessPx,
                              uint32_t argbColor);

    // Approximate filled circle using horizontal slices. Cheap and
    // looks good enough at typical UI sizes (radii 4-12px). Used for
    // round indicators (LEDs, radio dots, toggle thumbs).
    void    FillCircleApprox (float cxPx,
                              float cyPx,
                              float radiusPx,
                              uint32_t argbColor);

    HRESULT End              (ID3D11RenderTargetView * pRtv);

    // Global alpha multiplier applied to every vertex's alpha channel.
    // Used by the Settings panel's live-preview state machine to fade
    // the whole UI without touching individual paint call sites. 1.0
    // is opaque (default), 0.0 is fully transparent.
    void    SetGlobalAlpha   (float alpha)            { m_globalAlpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha; }
    float   GlobalAlpha      () const                 { return m_globalAlpha; }

    int     PendingVertexCount () const { return (int) m_vertices.size(); }

private:
    struct Vertex
    {
        float  x;
        float  y;
        float  r;
        float  g;
        float  b;
        float  a;
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
    void    NdcFromPixel     (float xPx, float yPx, float & outX, float & outY) const;

    static Vertex MakeVertex (uint32_t argbColor, float alphaMultiplier = 1.0f);


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

    std::vector<Vertex>               m_vertices;
};
