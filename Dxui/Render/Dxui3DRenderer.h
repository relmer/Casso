#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dxui3DRenderer
//
//  A deliberately scoped 3D path for Dxui's D3D11 pipeline: one MVP constant
//  buffer, one textured+tinted shader pair, one dynamic vertex buffer, and a
//  single dynamic content texture (plus a built-in 1x1 white for untextured
//  geometry). Consumers submit world-space triangles with UVs and a per-vertex
//  tint (which doubles as baked lighting); the renderer transforms them by the
//  caller's matrix and composites premultiplied source-over into whatever
//  render target is currently bound -- the same compositing contract as
//  DxuiPainter, so a scene drawn from a window's before-present hook layers
//  correctly under the panel-tree paint.
//
//  This is intentionally NOT an engine: no depth buffer (submit back-to-front,
//  painter's algorithm), no scene graph, no materials. It exists to render
//  small procedural set-pieces -- first the printer panel's ImageWriter +
//  curled-fanfold-paper scene (FR-032, research R-017) -- and is the primitive
//  the drive widgets can adopt when they move to true 3D.
//
//  All state is set on every Draw, mirroring DxuiPainter::End, so interleaving
//  with the painter / text renderer requires no state save/restore etiquette.
//
////////////////////////////////////////////////////////////////////////////////

class Dxui3DRenderer
{
public:
    struct Vertex
    {
        float  x, y, z;      // world-space position
        float  u, v;         // content-texture coordinates (ignored when tinted-only)
        float  r, g, b, a;   // tint * baked light, multiplied with the texture
    };

    Dxui3DRenderer  () = default;
    ~Dxui3DRenderer ();

    // Non-owning device/context, same lifetime contract as DxuiPainter.
    HRESULT  Initialize (ID3D11Device * device, ID3D11DeviceContext * context);
    void     Shutdown   ();

    bool     IsInitialized () const { return m_device != nullptr; }

    // Upload premultiplied-BGRA pixels into the content texture (recreated on
    // size change, Map/WRITE_DISCARD otherwise). Triangles drawn with
    // `textured == true` sample it; call again only when the content changes.
    HRESULT  UpdateContentTexture (const uint32_t * bgra, int width, int height);

    // Adopt an externally-owned SRV as the content texture instead of the
    // CPU-upload path -- the desk scene hands over the CRT chain's finished
    // output so a GPU-resident image never round-trips through system memory.
    // While set (non-null, non-owning, caller-guaranteed lifetime across the
    // frame), textured draws sample it in preference to the uploaded texture;
    // pass null to fall back.
    void     SetContentSrv        (ID3D11ShaderResourceView * srv) { m_externalSrv = srv; }

    // The CPU-uploaded texture's view, for callers that want to hand it back
    // as an explicit content SRV rather than relying on the fallback.
    ID3D11ShaderResourceView *  ContentSrv () const { return m_contentSrv.Get(); }

    // Prepare (and clear) a depth buffer matching the currently bound render
    // target, for draws submitted with `depthTest == true`. Call once at the
    // start of a scene frame, AFTER the caller's render target is bound --
    // the depth texture is sized by querying it. Loaded meshes need real
    // depth testing (their triangles arrive in arbitrary order); the
    // hand-built painter's-algorithm batches keep passing false.
    HRESULT  BeginDepthPass ();

    // Multisampled scene pass. Between these two calls the scene draws into
    // an offscreen MSAA target instead of the caller's render target; End
    // resolves it and composites the result back.
    //
    // The detour exists because a flip-model swap chain CANNOT be
    // multisampled -- DXGI requires SampleDesc.Count == 1 on it -- so the
    // only way to antialias is to render somewhere else and resolve.
    //
    // The offscreen target starts fully transparent and the scene layers into
    // it exactly as it would have layered onto the destination: this
    // renderer blends premultiplied source-over, and premultiplied
    // compositing is associative, so compositing the finished layer over the
    // destination gives the same pixels as drawing straight onto it. No copy
    // of the destination is needed going in.
    //
    // Call Begin AFTER binding the destination -- it sizes the offscreen
    // target by querying it, the same contract BeginDepthPass keeps. When the
    // device will not multisample at this count Begin quietly does nothing
    // and the scene draws straight to the destination; End then has nothing
    // to composite and is equally quiet, so callers pair them unconditionally.
    HRESULT  BeginMultisampledScene ();
    HRESULT  EndMultisampledScene   ();

    // Crop every subsequent draw to `rect` (target pixels); null clears it.
    // For keeping a scene inside a sub-rect that something else may cover --
    // shrinking the viewport instead would re-map the projection and squash
    // the scene rather than crop it.
    void     SetScissor (const RECT * rect);

    // Antialiasing for the scene pass, in SAMPLES: 1 disables the offscreen
    // detour entirely, 2 and 4 arm it. What it costs is very much a property
    // of the machine -- on an integrated GPU the whole scene is rasterized
    // into a target this many times over, out of the same memory bandwidth
    // the CPU is using -- so the user owns the number, not this file.
    // Unsupported counts fall back to drawing unantialiased, exactly as a
    // device that will not multisample at all does.
    void     SetSceneSampleCount (UINT samples);
    UINT     SceneSampleCount    () const { return m_sampleCount; }

    static constexpr UINT  kDefaultSceneSampleCount = 4;

    // Transform `verts` by row-major `mvp` (row-vector convention: clip = v * M)
    // and draw as a triangle list into the currently bound render target,
    // restricted to `viewportPx`. `textured` selects the content texture;
    // untextured geometry samples opaque white, so the tint IS the color.
    // `depthTest` binds the BeginDepthPass buffer (test + write, LESS).
    HRESULT  DrawTriangles (const Vertex   * verts,
                            size_t           vertexCount,
                            const float      mvp[16],
                            bool             textured,
                            const D3D11_VIEWPORT & viewportPx,
                            bool             depthTest  = false,
                            bool             depthWrite = true);

    // A vertex array parked on the GPU between frames -- one per array, held
    // by whoever owns the geometry. See DrawStatic.
    struct StaticMesh
    {
        ComPtr<ID3D11Buffer>  buffer;
        size_t                vertexCount = 0;
        uint32_t              revision    = 0;
    };

    // DrawTriangles for geometry that does not change every frame, which in
    // this scene is nearly all of it. `revision` is the caller's own counter,
    // bumped whenever it rebuilds the array; the vertices are re-uploaded only
    // when it moves, so a still frame costs no copying at all. Passing a
    // revision that never changes for an array that does is the one way to get
    // this wrong -- it draws stale geometry, silently.
    HRESULT  DrawStatic (StaticMesh     & mesh,
                         const Vertex   * verts,
                         size_t           vertexCount,
                         uint32_t         revision,
                         const float      mvp[16],
                         bool             textured,
                         const D3D11_VIEWPORT & viewportPx,
                         bool             depthTest  = false,
                         bool             depthWrite = true);

private:
    // The tail both draw paths share, once the vertices are wherever they
    // live: transform, full state set, Draw.
    HRESULT  IssueDraw (ID3D11Buffer             * vertexBuffer,
                        size_t                     vertexCount,
                        const float                mvp[16],
                        ID3D11ShaderResourceView * srv,
                        const D3D11_VIEWPORT     & viewportPx,
                        bool                       useDepth,
                        ID3D11DepthStencilState  * depthState);

    HRESULT  CreateShaders      ();
    HRESULT  CreatePipelineState ();
    HRESULT  EnsureVertexBuffer (size_t requiredVerts);

    ID3D11Device        *             m_device  = nullptr;   // non-owning
    ID3D11DeviceContext *             m_context = nullptr;   // non-owning

    ComPtr<ID3D11VertexShader>        m_vs;
    ComPtr<ID3D11PixelShader>         m_ps;
    ComPtr<ID3D11InputLayout>         m_layout;
    ComPtr<ID3D11Buffer>              m_vertexBuffer;
    ComPtr<ID3D11Buffer>              m_mvpBuffer;
    ComPtr<ID3D11BlendState>          m_blendState;
    ComPtr<ID3D11RasterizerState>     m_rasterState;
    ComPtr<ID3D11RasterizerState>     m_rasterStateScissor;
    RECT                              m_scissor    = {};
    bool                              m_hasScissor = false;
    ComPtr<ID3D11DepthStencilState>   m_depthState;       // depth off (painter's algorithm)
    ComPtr<ID3D11DepthStencilState>   m_depthStateTest;   // LESS test + write (meshes)
    ComPtr<ID3D11DepthStencilState>   m_depthStateReadOnly;   // LESS test, no write (light)
    ComPtr<ID3D11SamplerState>        m_sampler;

    // The multisampled scene detour: an MSAA color target the scene draws
    // into, and the single-sample texture it resolves to before being
    // composited back over the caller's target.
    ComPtr<ID3D11Texture2D>           m_msaaTex;
    ComPtr<ID3D11RenderTargetView>    m_msaaRtv;
    ComPtr<ID3D11Texture2D>           m_resolveTex;
    ComPtr<ID3D11ShaderResourceView>  m_resolveSrv;
    ComPtr<ID3D11RenderTargetView>    m_savedRtv;        // the caller's, while detoured
    int                               m_msaaWidth   = 0;
    int                               m_msaaHeight  = 0;
    bool                              m_inMsaaScene = false;
    UINT                              m_sampleCount = kDefaultSceneSampleCount;

    ComPtr<ID3D11Texture2D>         m_depthTex;
    ComPtr<ID3D11DepthStencilView>  m_depthDsv;
    UINT                            m_depthSamples = 0;
    int                             m_depthWidth   = 0;
    int                             m_depthHeight  = 0;

    ComPtr<ID3D11Texture2D>           m_contentTex;
    ComPtr<ID3D11ShaderResourceView>  m_contentSrv;
    ID3D11ShaderResourceView *        m_externalSrv = nullptr;   // non-owning
    ComPtr<ID3D11Texture2D>           m_whiteTex;
    ComPtr<ID3D11ShaderResourceView>  m_whiteSrv;

    size_t                            m_vertexBufferCapacity = 0;
    int                               m_contentWidth         = 0;
    int                               m_contentHeight        = 0;
};
